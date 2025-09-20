import os
import random
import string
from dataclasses import dataclass
from typing import List, Tuple, Optional, Dict
import xml.etree.ElementTree as ET


@dataclass
class TemplateParams:
    name: str
    num_states: int = 8
    num_clocks: int = 3
    num_int_vars: int = 2
    branching: int = 3
    guard_density: float = 0.7
    invariant_density: float = 0.5
    reset_density: float = 0.6
    assign_density: float = 0.5
    sync_density: float = 0.6


class BenchmarkGenerator:
    """
    Synthetic UPPAAL model generator for stress-testing: builds parametric
    timed-automata templates and assembles complete NTA XML models.
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
        # Ensure at least some channels exist when sync is requested
        self._ensure_channels(max(4, int(count * (params.sync_density or 0)) + 2))
        for i in range(count):
            p = TemplateParams(
                name=f"{base_name}{i}",
                num_states=params.num_states,
                num_clocks=params.num_clocks,
                num_int_vars=params.num_int_vars,
                branching=params.branching,
                guard_density=params.guard_density,
                invariant_density=params.invariant_density,
                reset_density=params.reset_density,
                assign_density=params.assign_density,
                sync_density=params.sync_density,
            )
            templates.append(self._generate_template(p))
        return templates

    def build_nta(self, templates: List[ET.Element], system_name: str = "System") -> ET.Element:
        nta = ET.Element("nta")
        # Global declarations: channels shared across templates
        decl = ET.SubElement(nta, "declaration")
        decl.text = self._emit_global_declarations()
        # Templates
        for t in templates:
            nta.append(t)
        # System instantiation
        sys = ET.SubElement(nta, "system")
        proc_lines = []
        for t in templates:
            tname = t.findtext("name") or "T" + self._rand_ident(4)
            inst = f"{tname}_i = {tname}();"
            proc_lines.append(inst)
        sys_body = "\n".join(proc_lines)
        sys_body += "\n\nsystem " + ", ".join((t.findtext("name") + "_i") for t in templates) + ";"
        sys.text = sys_body
        # Queries stub (optional, empty)
        #ET.SubElement(nta, "queries")
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
                num_clocks=max(1, int(self._vary(params.num_clocks, 0.3))),
                num_int_vars=max(0, int(self._vary(params.num_int_vars, 0.3))),
                branching=max(2, int(self._vary(params.branching, 0.3))),
                guard_density=self._clip01(self._vary(params.guard_density, 0.15)),
                invariant_density=self._clip01(self._vary(params.invariant_density, 0.15)),
                reset_density=self._clip01(self._vary(params.reset_density, 0.15)),
                assign_density=self._clip01(self._vary(params.assign_density, 0.15)),
                sync_density=self._clip01(self._vary(params.sync_density, 0.15)),
            )
            templates = self.generate_templates(templates_per_file, base_name=f"T{fidx}_", params=varied)
            nta = self.build_nta(templates, system_name=f"System_{fidx}")
            #out_dir = os.path.join(out_root, f"suite_{fidx}")
            # Write suite info before saving
            self._write_benchmark_info(out_dir=out_root,
                                       suite_index=fidx,
                                       templates_per_file=templates_per_file,
                                       params_used=varied,
                                       channel_count=len(self._channel_pool))
            paths.append(self.save_xml(nta, out_root, f"bench_{fidx}.xml"))
        return paths

    # -------------------- template generation --------------------
    def _generate_template(self, p: TemplateParams) -> ET.Element:
        template = ET.Element("template")
        name_el = ET.SubElement(template, "name")
        name_el.text = p.name
        # Local declarations: clocks and ints
        ldecl = ET.SubElement(template, "declaration")
        clks = ", ".join(f"x{i}" for i in range(p.num_clocks))
        ints = ", ".join(f"v{i}" for i in range(p.num_int_vars))
        parts = []
        if p.num_clocks > 0:
            parts.append(f"clock {clks};")
        if p.num_int_vars > 0:
            parts.append("int " + ", ".join(f"{v} = 0" for v in (f"v{i}" for i in range(p.num_int_vars))) + ";")
        ldecl.text = "\n".join(parts)

        # Locations
        loc_ids = [f"{p.name}_L{i}" for i in range(p.num_states)]
        for i, lid in enumerate(loc_ids):
            loc = ET.SubElement(template, "location", {"id": lid, "x": str(100 + 160 * (i % 6)), "y": str(100 + 120 * (i // 6))})
            nm = ET.SubElement(loc, "name")
            nm.text = f"L{i}"
            # Optional invariants
            if p.num_clocks > 0 and self._bernoulli(p.invariant_density):
                inv = ET.SubElement(loc, "label", {"kind": "invariant"})
                inv.text = self._rand_invariant(p.num_clocks)
        # Initial location
        ET.SubElement(template, "init", {"ref": loc_ids[0]})

        # Edges
        for i, src in enumerate(loc_ids):
            for _ in range(p.branching):
                tgt = self._pick_target(loc_ids, src)
                edge = ET.SubElement(template, "transition")
                source = ET.SubElement(edge, "source", {"ref": src})
                target = ET.SubElement(edge, "target", {"ref": tgt})
                
                # Guard
                if p.num_clocks > 0 and self._bernoulli(p.guard_density):
                    g = ET.SubElement(edge, "label", {"kind": "guard"})
                    g.text = self._rand_guard(p.num_clocks)
                # Synchronisation
                if self._bernoulli(p.sync_density) and self._channel_pool:
                    sync = ET.SubElement(edge, "label", {"kind": "synchronisation"})
                    sync.text = self._pick_sync()
                # Assignments: resets and int updates
                assigns: List[str] = []
                if p.num_clocks > 0 and self._bernoulli(p.reset_density):
                    assigns.extend(self._rand_resets(p.num_clocks))
                if p.num_int_vars > 0 and self._bernoulli(p.assign_density):
                    assigns.extend(self._rand_int_assigns(p.num_int_vars))
                if assigns:
                    a = ET.SubElement(edge, "label", {"kind": "assignment"})
                    a.text = ", ".join(assigns)
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
        if not self._channel_pool:
            return ""
        return "chan " + ", ".join(self._channel_pool) + ";\n"

    def _pick_target(self, ids: List[str], src: str) -> str:
        if len(ids) == 1:
            return src
        tgt = src
        while tgt == src:
            tgt = self.rng.choice(ids)
        return tgt

    def _bernoulli(self, p: float) -> bool:
        return self.rng.random() < p

    def _rand_ident(self, n: int) -> str:
        return "".join(self.rng.choice(string.ascii_letters) for _ in range(n))

    def _rand_invariant(self, num_clocks: int) -> str:
        parts = [self._rand_clock_cmp(num_clocks)]
        if self._bernoulli(0.6):
            parts.append(self._rand_clock_diff_cmp(num_clocks))
        return " && ".join(parts)

    def _rand_guard(self, num_clocks: int) -> str:
        parts = [self._rand_clock_cmp(num_clocks)]
        if self._bernoulli(0.7):
            parts.append(self._rand_clock_cmp(num_clocks))
        if self._bernoulli(0.5):
            parts.append(self._rand_clock_diff_cmp(num_clocks))
        return " && ".join(parts)

    def _rand_clock_cmp(self, num_clocks: int) -> str:
        i = self.rng.randrange(num_clocks)
        op = self.rng.choice(["<=", "<", ">=", ">"])
        c = self.rng.randint(0, 30)
        return f"x{i} {op} {c}"

    def _rand_clock_diff_cmp(self, num_clocks: int) -> str:
        if num_clocks < 2:
            return self._rand_clock_cmp(num_clocks)
        i, j = self.rng.sample(range(num_clocks), 2)
        op = self.rng.choice(["<=", "<", ">=", ">"])
        c = self.rng.randint(-10, 20)
        return f"x{i} - x{j} {op} {c}"

    def _rand_resets(self, num_clocks: int) -> List[str]:
        k = max(1, int(self.rng.random() * min(3, num_clocks)))
        idxs = self.rng.sample(range(num_clocks), k)
        return [f"x{i} := 0" for i in idxs]

    def _rand_int_assigns(self, num_ints: int) -> List[str]:
        # Use constant assignments (v := k) to maximise parser compatibility
        k = max(1, int(self.rng.random() * min(3, num_ints)))
        idxs = self.rng.sample(range(num_ints), k)
        assigns = []
        for i in idxs:
            v = f"v{i}"
            const = self.rng.randint(0, 5)
            assigns.append(f"{v} := {const}")
        return assigns

    def _pick_sync(self) -> str:
        cname = self.rng.choice(self._channel_pool)
        bal = self._channel_balance.get(cname, 0)
        dir_sym = "!" if bal <= 0 else "?"
        self._channel_balance[cname] = bal + (1 if dir_sym == "!" else -1)
        return f"{cname}{dir_sym}"

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

    # -------------------- suite documentation --------------------
    def _write_benchmark_info(self,
                              out_dir: str,
                              suite_index: int,
                              templates_per_file: int,
                              params_used: TemplateParams,
                              channel_count: int) -> None:
        os.makedirs(out_dir, exist_ok=True)
        path = os.path.join(out_dir, "benchmark_info.txt")
        lines = []
        lines.append(f"Suite index: {suite_index}")
        lines.append(f"Seed: {self._seed}")
        lines.append(f"Templates in file: {templates_per_file}")
        lines.append("")
        lines.append("Parameters used:")
        lines.append(f"  base name           : {params_used.name}")
        lines.append(f"  num_states (locs)   : {params_used.num_states}")
        lines.append(f"  num_clocks          : {params_used.num_clocks}")
        lines.append(f"  num_int_vars        : {params_used.num_int_vars}")
        lines.append(f"  branching (per loc) : {params_used.branching}")
        lines.append(f"  guard_density       : {params_used.guard_density:.2f}")
        lines.append(f"  invariant_density   : {params_used.invariant_density:.2f}")
        lines.append(f"  reset_density       : {params_used.reset_density:.2f}")
        lines.append(f"  assign_density      : {params_used.assign_density:.2f}")
        lines.append(f"  sync_density        : {params_used.sync_density:.2f}")
        lines.append(f"  channels (global)   : {channel_count}")
        lines.append("")
        lines.append("Parameter explanations:")
        lines.append("  base name           : Prefix used to name each template in the model.")
        lines.append("  num_states (locs)   : Number of locations per template.")
        lines.append("  num_clocks          : Number of local clocks declared in the template.")
        lines.append("  num_int_vars        : Number of local integer variables (initialised to 0).")
        lines.append("  branching (per loc) : Outgoing edges created per location (controls graph fan-out).")
        lines.append("  guard_density       : Probability an edge gets a guard (clock and clock-difference constraints).")
        lines.append("  invariant_density   : Probability a location gets an invariant (clock and differences).")
        lines.append("  reset_density       : Probability an edge resets one or more clocks (x := 0).")
        lines.append("  assign_density      : Probability an edge assigns integer constants (v := k).")
        lines.append("  sync_density        : Probability an edge carries a channel synchronisation (c! or c?).")
        lines.append("  channels (global)   : Number of global channels declared and reused across templates.")
        lines.append("")
        lines.append("Semantics:")
        lines.append("  Guards: conjunctions of x_i op c and x_i - x_j op c with op in {<=,<,>=,>} and integer c.")
        lines.append("  Invariants: same constraint language as guards, attached to locations.")
        lines.append("  Resets: clock resets use the syntax x := 0.")
        lines.append("  Int assignments: constant updates v := k with small non-negative k.")
        lines.append("  Synchronisations: c! (send) and c? (receive), balanced per channel.")
        lines.append("")
        lines.append("How the models are generated:")
        lines.append("  Each template is built with the requested number of locations.")
        lines.append("  From each location, a fixed number of outgoing edges (branching) is created,")
        lines.append("  targeting random locations (self-loops allowed) to form a sparse graph.")
        lines.append("  For each edge/location, optional features are added independently using the")
        lines.append("  given densities: guards (on clocks and clock differences), invariants,")
        lines.append("  clock resets (x := 0), integer constant assignments (v := k), and channel")
        lines.append("  synchronisations c!/c?. Channels are chosen from a small global pool and")
        lines.append("  send/receive directions are balanced across templates to avoid deadlocks.")
        lines.append("  A global random seed controls all choices to make suites reproducible.")
        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines))
