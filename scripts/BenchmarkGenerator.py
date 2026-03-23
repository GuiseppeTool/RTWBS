import os
import random
import string
from dataclasses import dataclass
from typing import Dict, List, Optional, Set, Tuple
import xml.etree.ElementTree as ET


@dataclass
class TemplateParams:
    name: str
    num_states: int = 8
    num_clocks: int = 2          # fixed: x0 for operation durations, x1 for delays
    num_int_vars: int = 2
    branching: int = 3
    guard_density: float = 0.7
    invariant_density: float = 0.5
    reset_density: float = 0.6
    assign_density: float = 0.5
    send_events: int = 3
    recv_events: int = 4
    env_ratio: float = 0.5      # fraction of communications with environmental monitoring


class BenchmarkGenerator:
    """
    Generator for smart farming system models representing distributed
    agricultural automation components with temporal constraints.

    Key design principles from the smart farming architecture:

      1. **Urgent internal actions**: A global ``urgent chan tau_backbone`` represents
         instantaneous internal processing within farming equipment (e.g., sensor
         readings, actuator responses) that don't consume time.
      2. **Deterministic communication**: Every communication channel has exactly one
         sender and one receiver across all components, ensuring predictable
         coordination between farming units (sprayers, sensors, irrigation systems).
      3. **Dual timing clocks**: Each component uses exactly 2 clocks.
         ``x0`` tracks operation durations with upper bounds (e.g., max spray time),
         ``x1`` tracks delays and guard conditions (e.g., minimum wait before action).
      4. **Sequential environment**: The Env template models cyclical environmental
         conditions (soil monitoring, weather patterns) that interact with components
         in a predictable sequence.
      5. **Simple timing constraints**: Only simple bounds (``x <= c`` or
         ``x >= c``) are used to represent real-world timing requirements like
         "wait at least 5 minutes" or "complete within 30 seconds".
      6. Every operational state has at least one outgoing transition with a lower-bound
         guard ``x1 >= c, c > 0`` ensuring forward progress in field operations.
      7. The component graph is strongly connected via a backbone cycle
         representing the main operational loop, with additional branching for
         exceptional conditions and coordinated actions.
      8. Clocks tracking operation durations (``x0``) are reset when entering
         states with time limits to ensure feasibility.
      9. Backbone (cycle) edges carry ``tau_backbone!`` urgent sync, representing
         internal state changes that complete instantaneously (e.g., control decisions,
         mode switches) in the agricultural automation system.
    """

    def __init__(self, seed: Optional[int] = None):
        self.rng = random.Random(seed)
        self._seed = seed
        self._channel_pool: List[str] = []
        self._channel_balance: Dict[str, int] = {}

    # -------------------- public API --------------------
    def generate_templates(self, count: int, base_name: str = "T",
                           params: Optional[TemplateParams] = None) -> List[ET.Element]:
        if params is None:
            params = TemplateParams(name=base_name)
        templates = []

        # Force 2 clocks as per dual-timing architecture
        params.num_clocks = 2

        # Split send/recv events into inter-component and env portions.
        # env_ratio determines what fraction involves environmental monitoring.
        n_env_sends = max(1, round(params.send_events * params.env_ratio))
        n_env_recvs = max(1, round(params.recv_events * params.env_ratio))
        n_inter_sends = params.send_events - n_env_sends
        n_inter_recvs = params.recv_events - n_env_recvs

        # For deterministic 1:1 sync, inter-component channel count must
        # match between adjacent templates.  Use the minimum.
        n_inter = min(n_inter_sends, n_inter_recvs) if count > 1 else 0

        # --------------- per-template unique channel assignment ---------------
        # Each channel gets exactly one sender and one receiver (deterministic).
        all_env_channels: List[Tuple[str, str]] = []   # (channel, direction_for_env)
        per_template_send: List[List[str]] = []
        per_template_recv: List[List[str]] = []

        ch_idx = 0
        for i in range(count):
            send_labels_i: List[str] = []
            recv_labels_i: List[str] = []

            # Env-send channels: component i sends data, Env monitoring receives
            for _k in range(n_env_sends):
                ch = f"ce{ch_idx}"
                ch_idx += 1
                send_labels_i.append(f"{ch}!")
                all_env_channels.append((ch, "?"))  # Env is receiver

            # Env-recv channels: Env broadcasts commands, component i receives
            for _k in range(n_env_recvs):
                ch = f"ce{ch_idx}"
                ch_idx += 1
                recv_labels_i.append(f"{ch}?")
                all_env_channels.append((ch, "!"))  # Env is sender

            # Inter-component: component i sends to component (i+1)%count
            if n_inter > 0:
                nxt = (i + 1) % count
                for k in range(n_inter):
                    ch = f"ci{i}t{nxt}_{k}"
                    send_labels_i.append(f"{ch}!")

            # Inter-component: component i receives from component (i-1)%count
            if n_inter > 0:
                prev = (i - 1) % count
                for k in range(n_inter):
                    ch = f"ci{prev}t{i}_{k}"
                    recv_labels_i.append(f"{ch}?")

            per_template_send.append(send_labels_i)
            per_template_recv.append(recv_labels_i)

        # Collect all channels for global declaration
        all_ch_names: List[str] = []
        for ch, _ in all_env_channels:
            all_ch_names.append(ch)
        if n_inter > 0:
            for i in range(count):
                nxt = (i + 1) % count
                for k in range(n_inter):
                    all_ch_names.append(f"ci{i}t{nxt}_{k}")
        self._channel_pool = list(dict.fromkeys(all_ch_names))

        # Store env channel info for _build_env_template (sequential cycle)
        self._env_channels_ordered = all_env_channels

        for i in range(count):
            p = TemplateParams(
                name=f"{base_name}{i}",
                num_states=params.num_states,
                num_clocks=2,       # forced: x0 invariants, x1 guards
                num_int_vars=params.num_int_vars,
                branching=params.branching,
                guard_density=params.guard_density,
                invariant_density=params.invariant_density,
                reset_density=params.reset_density,
                assign_density=params.assign_density,
                send_events=params.send_events,
                recv_events=params.recv_events,
                env_ratio=params.env_ratio,
            )
            templates.append(self._generate_template(
                p,
                send_labels=per_template_send[i],
                recv_labels=per_template_recv[i]))
        return templates

    def build_nta(self, templates: List[ET.Element], system_name: str = "System") -> ET.Element:
        nta = ET.Element("nta")
        # Global declarations: channels shared across templates
        decl = ET.SubElement(nta, "declaration")
        decl.text = self._emit_global_declarations()
        # Templates
        all_templates = list(templates)
        # Always add an Env template that matches the env-destined channels.
        env = self._build_env_template()
        if env is not None:
            all_templates.append(env)
        for t in all_templates:
            nta.append(t)
        # System instantiation
        sys = ET.SubElement(nta, "system")
        proc_lines = []
        for t in all_templates:
            tname = t.findtext("name") or "T" + self._rand_ident(4)
            inst = f"{tname}_i = {tname}();"
            proc_lines.append(inst)
        sys_body = "\n".join(proc_lines)
        sys_body += "\n\nsystem " + ", ".join((t.findtext("name") + "_i") for t in all_templates) + ";"
        sys.text = sys_body
        # Queries stub (optional, empty)
        queries = ET.SubElement(nta, "queries")
        query = ET.SubElement(queries, "query")
        ET.SubElement(query, "formula").text = "A[] true"
        ET.SubElement(query, "comment").text = "No queries defined."



        
        return nta

    def save_xml(self, nta: ET.Element, out_dir: str, filename: str) -> str:
        os.makedirs(out_dir, exist_ok=True)
        path = os.path.join(out_dir, filename)
        tree = ET.ElementTree(nta)
        self._indent_xml(nta)
        tree.write(path, encoding="utf-8", xml_declaration=True)
        return path

    def create_benchmark(self,
                         out_root: str,
                         files: int = 5,
                         templates_per_file: int = 3,
                         base_params: Optional[TemplateParams] = None) -> List[str]:
        paths: List[str] = []
        for fidx in range(files):
            params = base_params or TemplateParams(name="T")
            varied = TemplateParams(
                name=params.name,
                num_states=max(4, int(self._vary(params.num_states, 0.2))),
                num_clocks=2,       # always 2 (dual clocks)
                num_int_vars=max(0, int(self._vary(params.num_int_vars, 0.3))),
                branching=max(2, int(self._vary(params.branching, 0.3))),
                guard_density=self._clip01(self._vary(params.guard_density, 0.15)),
                invariant_density=self._clip01(self._vary(params.invariant_density, 0.15)),
                reset_density=self._clip01(self._vary(params.reset_density, 0.15)),
                assign_density=self._clip01(self._vary(params.assign_density, 0.15)),
                send_events=max(0, int(self._vary(params.send_events, 0.15))),
                recv_events=max(0, int(self._vary(params.recv_events, 0.15))),
                env_ratio=params.env_ratio,
            )
            templates = self.generate_templates(templates_per_file, base_name=f"T{fidx}_", params=varied)
            nta = self.build_nta(templates, system_name=f"System_{fidx}")
            #out_dir = os.path.join(out_root, f"config_{fidx}")
            # Write system configuration info before saving
            self._write_benchmark_info(out_dir=out_root,
                                       suite_index=fidx,
                                       templates_per_file=templates_per_file,
                                       params_used=varied,
                                       channel_count=len(self._channel_pool))
            paths.append(self.save_xml(nta, out_root, f"bench_{fidx}.xml"))
        return paths

    # -------------------- environmental monitoring component --------------------
    def _build_env_template(self) -> Optional[ET.Element]:
        """Create the environmental conditions template.

        The Env template models cyclical environmental patterns and external
        conditions in the smart farming system (e.g., soil moisture checks,
        weather monitoring, irrigation scheduling). It cycles through a series
        of measurement and response states::

            E0 --sense0?--> E1 --actuate1!--> E2 --sense2?--> ... --actuateN--> E0

        Each transition represents either receiving sensor data from field components
        or broadcasting actuation commands, ensuring coordinated interaction with
        the farming equipment.

        Additionally, every state has a ``tau_backbone?`` self-loop to acknowledge
        internal state transitions in component templates, allowing equipment to
        make instant control decisions while the environment waits.
        """
        env_channels = getattr(self, '_env_channels_ordered', [])
        if not env_channels:
            return None

        env = ET.Element("template")
        name_el = ET.SubElement(env, "name")
        name_el.text = "Env"
        decl = ET.SubElement(env, "declaration")
        decl.text = ""

        n = len(env_channels)
        loc_ids = [f"Env_L{i}" for i in range(n)]

        for i, lid in enumerate(loc_ids):
            loc_el = ET.SubElement(env, "location", {
                "id": lid,
                "x": str(100 + 140 * (i % 8)),
                "y": str(100 + 120 * (i // 8)),
            })
            n_el = ET.SubElement(loc_el, "name")
            n_el.text = f"E{i}"

        ET.SubElement(env, "init", {"ref": loc_ids[0]})

        # Cycle: E_i --ch[direction]--> E_{(i+1)%n}
        for i, (ch, direction) in enumerate(env_channels):
            edge = ET.SubElement(env, "transition")
            ET.SubElement(edge, "source", {"ref": loc_ids[i]})
            ET.SubElement(edge, "target", {"ref": loc_ids[(i + 1) % n]})
            s_el = ET.SubElement(edge, "label", {"kind": "synchronisation"})
            s_el.text = f"{ch}{direction}"

        # tau_backbone? self-loops on every Env location so that component
        # backbone edges (tau_backbone!) always have a matching receiver.
        for lid in loc_ids:
            edge = ET.SubElement(env, "transition")
            ET.SubElement(edge, "source", {"ref": lid})
            ET.SubElement(edge, "target", {"ref": lid})
            s_el = ET.SubElement(edge, "label", {"kind": "synchronisation"})
            s_el.text = "tau_backbone?"

        return env

    # -------------------- farming component generation --------------------
    def _generate_template(self, p: TemplateParams,
                           send_labels: Optional[List[str]] = None,
                           recv_labels: Optional[List[str]] = None) -> ET.Element:
        template = ET.Element("template")
        name_el = ET.SubElement(template, "name")
        name_el.text = p.name

        # ---- local declarations ----------------------------------------
        # Exactly 2 clocks: x0 (operation durations only), x1 (delays only)
        ldecl = ET.SubElement(template, "declaration")
        parts: List[str] = ["clock x0, x1;"]
        if p.num_int_vars > 0:
            parts.append(
                "int " + ", ".join(f"v{i} = 0" for i in range(p.num_int_vars)) + ";"
            )
        ldecl.text = "\n".join(parts)

        # ---- constant range for this component --------------------------
        max_const = self.rng.randint(10, 30)

        # ---- operational modes with duration limits (only x0 <= c) --------------------
        loc_ids = [f"{p.name}_L{i}" for i in range(p.num_states)]
        loc_inv: Dict[int, int] = {}                     # loc_idx -> bound
        for i in range(1, p.num_states):                  # skip initial mode
            if self._bernoulli(p.invariant_density):
                bound = self.rng.randint(max(3, max_const // 3), max_const)
                loc_inv[i] = bound

        for i, lid in enumerate(loc_ids):
            loc_el = ET.SubElement(template, "location", {
                "id": lid,
                "x": str(100 + 160 * (i % 6)),
                "y": str(100 + 120 * (i // 6)),
            })
            n_el = ET.SubElement(loc_el, "name")
            n_el.text = f"L{i}"
            if i in loc_inv:
                inv_el = ET.SubElement(loc_el, "label", {"kind": "invariant"})
                inv_el.text = f"x0 <= {loc_inv[i]}"

        ET.SubElement(template, "init", {"ref": loc_ids[0]})

        # ---- transitions -------------------------------------------------------
        # Phase 1: backbone cycle (tau_backbone urgent sync) for main operational loop
        edges: List[Tuple[int, int, bool]] = []
        for i in range(p.num_states):
            edges.append((i, (i + 1) % p.num_states, True))
        # Phase 2: branching transitions for exceptions and coordinated actions
        for i in range(p.num_states):
            for _ in range(max(0, p.branching - 1)):
                tgt = self.rng.randrange(p.num_states)
                while tgt == i and p.num_states > 1:
                    tgt = self.rng.randrange(p.num_states)
                edges.append((i, tgt, False))

        # ---- pre-assign sync labels to non-backbone edges ----------------
        branching_indices = [i for i, (_, _, bb) in enumerate(edges) if not bb]
        self.rng.shuffle(branching_indices)
        n_branch = len(branching_indices)

        planned_sends = send_labels or []
        planned_recvs = recv_labels or []
        send_count = min(len(planned_sends), n_branch)
        send_edges = set(branching_indices[:send_count])
        recv_avail = branching_indices[send_count:]
        recv_count = min(len(planned_recvs), len(recv_avail))
        recv_edges = set(recv_avail[:recv_count])

        sync_labels: Dict[int, str] = {}
        si = 0
        for ei in sorted(send_edges):
            sync_labels[ei] = planned_sends[si]
            si += 1
        ri = 0
        for ei in sorted(recv_edges):
            sync_labels[ei] = planned_recvs[ri]
            ri += 1

        # Track time-progress coverage per location
        tp_done: Set[int] = set()

        for edge_idx, (src, tgt, is_backbone) in enumerate(edges):
            edge_el = ET.SubElement(template, "transition")
            ET.SubElement(edge_el, "source", {"ref": loc_ids[src]})
            ET.SubElement(edge_el, "target", {"ref": loc_ids[tgt]})

            # ---- guard (simple bounds on x1, no clock differences) -------
            # Timed automaton semantics forbid clock guards on urgent edges, so backbone
            # edges (which carry tau_backbone!) must never have guards.
            guard_parts: List[str] = []
            if not is_backbone:
                needs_tp = src not in tp_done
                want_guard = needs_tp or self._bernoulli(p.guard_density)
            else:
                needs_tp = False
                want_guard = False

            if want_guard:
                # Guards use x1 exclusively (dual-timing architecture)
                if src in loc_inv:
                    ub = loc_inv[src]
                else:
                    ub = max_const

                lo = self.rng.randint(1 if needs_tp else 0,
                                     max(1, ub // 3))
                hi = self.rng.randint(max(lo, 1), ub)

                guard_parts.append(f"x1 >= {lo}")
                # Upper bound present most of the time
                if self._bernoulli(0.65) or src in loc_inv:
                    guard_parts.append(f"x1 <= {hi}")

                tp_done.add(src)

                # NOTE: clock-difference constraints (xi - xj <= d) are
                # intentionally omitted — they significantly complicate
                # zone-based state-space analysis and are not needed
                # for representing real-world timing in farming operations.

            if guard_parts:
                g_el = ET.SubElement(edge_el, "label", {"kind": "guard"})
                g_el.text = " && ".join(guard_parts)

            # ---- synchronisation -----------------------------------------
            if is_backbone:
                # Urgent backbone sync represents instantaneous internal processing.
                # Env monitoring has matching tau_backbone? self-loops.
                s_el = ET.SubElement(edge_el, "label", {"kind": "synchronisation"})
                s_el.text = "tau_backbone!"
            elif edge_idx in sync_labels:
                s_el = ET.SubElement(edge_el, "label", {"kind": "synchronisation"})
                s_el.text = sync_labels[edge_idx]

            # ---- resets & assignments ------------------------------------
            assigns: List[str] = []
            # Strategic: always reset x0 (operation duration clock) when entering
            # a mode with a time limit
            if tgt in loc_inv:
                assigns.append("x0 := 0")

            # Reset of x1 (delay clock) when starting new timing cycle
            if self._bernoulli(p.reset_density):
                assigns.append("x1 := 0")

            # Integer variable updates
            if p.num_int_vars > 0 and self._bernoulli(p.assign_density):
                assigns.extend(self._rand_int_assigns(p.num_int_vars))

            if assigns:
                unique = list(dict.fromkeys(assigns))
                a_el = ET.SubElement(edge_el, "label", {"kind": "assignment"})
                a_el.text = ", ".join(unique)

        return template

    # -------------------- helpers --------------------
    def _ensure_channels(self, k: int):
        if len(self._channel_pool) >= k:
            return
        needed = k - len(self._channel_pool)
        for _ in range(needed):
            cname = f"c{len(self._channel_pool)}"
            self._channel_pool.append(cname)
            self._channel_balance[cname] = 0

    def _emit_global_declarations(self) -> str:
        lines: List[str] = ["urgent chan tau_backbone;"]
        if self._channel_pool:
            lines.append("chan " + ", ".join(self._channel_pool) + ";")
        return "\n".join(lines) + "\n"

    def _bernoulli(self, p: float) -> bool:
        return self.rng.random() < p

    def _rand_ident(self, n: int) -> str:
        return "".join(self.rng.choice(string.ascii_letters) for _ in range(n))

    def _rand_int_assigns(self, num_ints: int) -> List[str]:
        k = max(1, int(self.rng.random() * min(3, num_ints)))
        idxs = self.rng.sample(range(num_ints), k)
        assigns = []
        for i in idxs:
            v = f"v{i}"
            const = self.rng.randint(0, 5)
            assigns.append(f"{v} := {const}")
        return assigns

    def _indent_xml(self, elem: ET.Element, level: int = 0):
        i = "\n" + level * "  "
        if len(elem):
            if not elem.text or not elem.text.strip():
                elem.text = i + "  "
            for e in elem:
                self._indent_xml(e, level + 1)
            if not elem.tail or not elem.tail.strip():
                elem.tail = i
        else:
            if level and (not elem.tail or not elem.tail.strip()):
                elem.tail = i

    def _clip01(self, x: float) -> float:
        return max(0.0, min(1.0, x))

    def _vary(self, val: float, rel: float) -> float:
        span = abs(val) * rel
        return val + self.rng.uniform(-span, span)

    # -------------------- system configuration documentation --------------------
    def _write_benchmark_info(self,
                              out_dir: str,
                              suite_index: int,
                              templates_per_file: int,
                              params_used: TemplateParams,
                              channel_count: int) -> None:
        os.makedirs(out_dir, exist_ok=True)
        path = os.path.join(out_dir, "benchmark_info.txt")
        lines = []
        lines.append(f"Configuration index: {suite_index}")
        lines.append(f"Seed: {self._seed}")
        lines.append(f"Templates in file: {templates_per_file}")
        lines.append("")
        lines.append("Parameters used:")
        lines.append(f"  base name           : {params_used.name}")
        lines.append(f"  num_states (locs)   : {params_used.num_states}")
        lines.append(f"  num_clocks          : {params_used.num_clocks}  (fixed: x0 invariants, x1 guards)")
        lines.append(f"  num_int_vars        : {params_used.num_int_vars}")
        lines.append(f"  branching (per loc) : {params_used.branching}")
        lines.append(f"  guard_density       : {params_used.guard_density:.2f}")
        lines.append(f"  invariant_density   : {params_used.invariant_density:.2f}")
        lines.append(f"  reset_density       : {params_used.reset_density:.2f}")
        lines.append(f"  assign_density      : {params_used.assign_density:.2f}")
        lines.append(f"  send_events         : {params_used.send_events}")
        lines.append(f"  recv_events         : {params_used.recv_events}")
        lines.append(f"  env_ratio           : {params_used.env_ratio:.2f}")
        lines.append(f"  channels (global)   : {channel_count}")
        lines.append("")
        lines.append("Parameter explanations:")
        lines.append("  base name           : Prefix used to name each component in the farming system.")
        lines.append("  num_states (locs)   : Number of operational modes per component.")
        lines.append("  num_clocks          : Always 2 (dual timing: x0 for operation durations, x1 for delays).")
        lines.append("  num_int_vars        : Number of local state variables (e.g., counters, modes, initialised to 0).")
        lines.append("  branching (per loc) : Decision points per mode (alternative actions, error handling paths).")
        lines.append("  guard_density       : Fraction of transitions with timing requirements (simple bounds on x1).")
        lines.append("  invariant_density   : Fraction of modes with time limits for operations (x0 <= c).")
        lines.append("  reset_density       : Fraction of transitions resetting timing for new operations; x0 is always")
        lines.append("                        reset when entering a mode with time limits.")
        lines.append("  assign_density      : Fraction of transitions updating state variables (v := k).")
        lines.append("  send_events         : Number of transitions per component sending commands/data (c!) to others.")
        lines.append("  recv_events         : Number of transitions per component receiving signals/data (c?) from others.")
        lines.append("  env_ratio           : Fraction of communications involving environmental monitoring (Env component).")
        lines.append("  channels (global)   : Number of communication channels connecting system components.")
        lines.append("")
        lines.append("Smart farming system design principles:")
        lines.append("  1. Urgent internal actions: 'urgent chan tau_backbone' for instantaneous internal")
        lines.append("     processing (sensor readings, control decisions) that don't consume time.")
        lines.append("  2. Deterministic communication: every channel has exactly 1 sender and 1 receiver,")
        lines.append("     ensuring predictable coordination between farming equipment units.")
        lines.append("  3. Dual timing clocks: each component uses exactly 2 clocks (x0 for operation")
        lines.append("     duration limits, x1 for minimum delays and guard conditions).")
        lines.append("  4. Sequential environment: Env models cyclical monitoring patterns")
        lines.append("     (soil→irrigation→spray→harvest...) in a deterministic sequence.")
        lines.append("  5. Simple timing constraints: only simple bounds (x<=c, x>=c) representing")
        lines.append("     real-world timing like 'wait at least 5 min' or 'complete within 30 sec'.")
        lines.append("")
        lines.append("Temporal semantics for agricultural operations:")
        lines.append("  Invariants: upper bounds x0 <= c for max operation durations (operational modes).")
        lines.append("  Guards: range constraints on x1 (a <= x1 <= b) for timing windows and delays.")
        lines.append("  Resets: x0 := 0 when starting timed operations in modes with duration limits;")
        lines.append("          x1 := 0 based on operational requirements for new timing cycles.")
        lines.append("  State updates: variable assignments v := k representing mode changes and counters.")
        lines.append("  Synchronisations: deterministic 1:1 channels (c!, c?) for component coordination.")
        lines.append("  Backbone: main operational cycle edges carry tau_backbone! (urgent/instantaneous).")
        lines.append("")
        lines.append("System architecture:")
        lines.append("  Each component has a main operational cycle (L0->L1->...->Ln-1->L0)")
        lines.append("  representing its normal operation sequence with instantaneous internal")
        lines.append("  transitions (tau_backbone!) for mode changes that don't consume time.")
        lines.append("  Additional branching represents exceptional conditions and coordinated actions")
        lines.append("  via communication channels with other components.")
        lines.append("  Guards specify timing windows (a <= x1 <= b) with at least one")
        lines.append("  time-progress transition (x1 >= c, c > 0) per operational mode.")
        lines.append("  Duration-limited modes use strict upper bounds (x0 <= c) with x0 reset")
        lines.append("  on entry transitions. Each channel provides deterministic point-to-point")
        lines.append("  communication between components (one sender, one receiver).")
        lines.append("  The Env component follows a sequential monitoring cycle.")
        lines.append("  A fixed seed ensures reproducible system configurations for analysis.")
        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines))
