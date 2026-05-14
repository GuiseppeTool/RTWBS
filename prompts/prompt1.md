# Prompt CS1 — Three-Tank System (Gil et al.)

## Prerequisites

Prompt 0 must be fully implemented and compiling before starting this prompt.
Read `doc/` (the paper) carefully, especially Definition 8 and Example 3.
Read the completed `include/rtwbs/semantic/` headers to understand the API.
Study `benchmark/run_FMICS2021.cpp` and `example/rtwbs_equivalence_example.cpp`
for the expected benchmark runner pattern.

---

## Background and Domain

The Three-Tank System is a classical process control benchmark drawn from
Gil et al. [SoSyM 2025, "An Architecture for Coupled Digital Twins with
Semantic Lifting"]. It models a series-connected fluid system: Tank 1 is
fed by an external pump, fluid flows sequentially from Tank 1 → Tank 2 →
Tank 3, and Tank 3 drains to a reservoir. The system is governed by IEC
61511-1:2016 ("Functional Safety — Safety Instrumented Systems for the
Process Industry"), which specifies:

- **Clause 9.2**: Physical integrity requirements for process vessels
  (capacity constraints, level bounds, overflow prevention)
- **Clause 9.3**: Safety instrumented function requirements for SIL-2 rating
  (minimum safe level, alarm response times)
- **Clause 10.3**: Design requirements for final elements (actuators:
  pumps and valves — rated flow, actuation time constraints)
- **Clause 11.6**: Proof test and diagnostic coverage requirements
  (periodic testing must be completable within a defined time window)
- **Clause 12.1**: Systematic capability — demands documented operational
  modes and mode transition requirements

The **PT view** models the physical tank system from the perspective of
the field instrument engineer: pump actuation commands, valve open/close
commands, and level alarms are the observable events.

The **DT view** models a water-budget monitoring twin authored from a
*process safety engineer's* perspective. Instead of tracking individual
valve timing, the DT tracks cumulative volume flows and safety margins.
The PT and DT are authored by different teams using different vocabularies
but must be shown to agree on the domain-level meaning of their events
under the IEC 61511 ontology. This is the core challenge of semantic
alignment: the PT says `pump_on!` meaning "pump is energised and flow
commences"; the DT says `pump_activate_monitor!` meaning "inflow to
tank 1 exceeds the minimum rated flow threshold." These are semantically
the same event under the IEC 61511 domain axioms, but syntactically
different labels — syntactic bisimulation fails, semantic alignment succeeds.

---

## STEP 1 — UPPAAL Models

### File: `assets/CS1_ThreeTank/V1_PT.xml`

Create a valid UPPAAL 4.x XML file containing **one template** named `ThreeTankPT`.

The template models the physical three-tank system. Use the existing codebase
conventions: channel names follow the `!`/`?` suffix convention defined in
`TA_CONFIG` (sender suffix `!`). Internal transitions use label `tau`.

**Clocks:** `t_fill`, `t_flow12`, `t_flow23`, `t_drain`, `t_alarm`

**Locations (8 total):**
```
IDLE          — system at rest, all valves closed, pump off
FILLING       — pump active, filling tank 1
FLOWING_12    — valve 1-2 open, fluid transferring T1→T2
FLOWING_23    — valve 2-3 open, fluid transferring T2→T3
DRAINING      — outlet valve open, T3 draining to reservoir
PROOF_TEST    — IEC 61511 Cl.11.6 periodic proof test in progress
ALARM         — level critical: level(3) < min_safe_level
EMERGENCY_STOP — fault state, all actuators locked
```

**Transitions and timing constraints (all from IEC 61511):**

```
IDLE -> FILLING
  trigger: pump_on!
  guard:   level(1) < capacity(1)   [modelled as: t_fill >= 0, always enabled]
  reset:   t_fill := 0

FILLING -> IDLE
  trigger: pump_off!
  guard:   t_fill <= 120            [IEC 61511 Cl.10.3: pump max continuous run 120s]
  invariant on FILLING: t_fill <= 120

FILLING -> FLOWING_12
  trigger: valve12_open!
  guard:   t_fill >= 10             [minimum fill time before transfer]
  reset:   t_flow12 := 0

FLOWING_12 -> IDLE
  trigger: valve12_close!
  guard:   t_flow12 >= 5 && t_flow12 <= 60  [IEC 61511 Cl.10.3 rated transfer time]
  invariant on FLOWING_12: t_flow12 <= 60

FLOWING_12 -> FLOWING_23
  trigger: valve23_open!
  guard:   t_flow12 >= 15           [T2 must receive minimum fill before T2→T3 transfer]
  reset:   t_flow23 := 0

FLOWING_23 -> DRAINING
  trigger: drain!
  guard:   t_flow23 >= 10 && t_flow23 <= 45
  invariant on FLOWING_23: t_flow23 <= 45
  reset:   t_drain := 0

DRAINING -> IDLE
  trigger: tau                      [internal: drain completes, system returns to idle]
  guard:   t_drain >= 5 && t_drain <= 30
  invariant on DRAINING: t_drain <= 30

ANY -> ALARM
  trigger: level_critical!
  guard:   t_alarm >= 0             [immediate: safety function, IEC 61511 Cl.9.3]
  (modelled as transition from IDLE, FILLING, FLOWING_12, FLOWING_23, DRAINING)

ALARM -> EMERGENCY_STOP
  trigger: emergency_shutdown!
  guard:   t_alarm <= 5             [IEC 61511 Cl.9.3: SIL-2 demands response ≤5s]
  invariant on ALARM: t_alarm <= 5

IDLE -> PROOF_TEST
  trigger: proof_test_start!
  guard:   t_fill >= 0              [can only start from idle]
  reset:   t_fill := 0

PROOF_TEST -> IDLE
  trigger: proof_test_complete!
  guard:   t_fill >= 20 && t_fill <= 60
           [IEC 61511 Cl.11.6: test window 20-60 time units]
  invariant on PROOF_TEST: t_fill <= 60

EMERGENCY_STOP -> IDLE
  trigger: system_reset!
  guard:   (no guard)               [operator-initiated reset]
```

Make the XML well-formed and parseable by `System("assets/CS1_ThreeTank/V1_PT.xml")`.

### File: `assets/CS1_ThreeTank/V2_DT.xml`

Create a UPPAAL XML file with template `ThreeTankDT`. This models the DT
water-budget monitoring twin authored by a *process safety engineer*
(different from the field instrument engineer who wrote V1_PT.xml).

The DT engineer tracks:
- Cumulative inflow volume to Tank 1 (proxy: `t_inflow` clock)
- Safety margin relative to `min_safe_level` (proxy: `t_safety` clock)
- System-wide energy/volume budget (proxy: `t_budget` clock)

The DT uses **different label names** reflecting the monitoring perspective.
This is the core point: the PT and DT must be shown semantically equivalent
despite using different label vocabularies.

**Label mapping intent (not syntactic — proven via SMT under ontology):**
```
PT label              DT label
pump_on!         -->  pump_activate_monitor!
pump_off!        -->  pump_deactivate_monitor!
valve12_open!    -->  inflow_t2_start!
valve12_close!   -->  inflow_t2_stop!
valve23_open!    -->  inflow_t3_start!
valve23_close!   -->  inflow_t3_stop!
drain!           -->  outflow_start!
level_critical!  -->  safety_margin_breach!
proof_test_start!    -->  diagnostic_cycle_start!
proof_test_complete! -->  diagnostic_cycle_complete!
emergency_shutdown!  -->  protective_trip_command!
system_reset!    -->  (no DT equivalent — DT-internal only)
```

**Clocks:** `t_inflow`, `t_safety`, `t_budget`, `t_diag`

**Locations (8 total):**
```
MONITOR_IDLE
MONITOR_INFLOW_ACTIVE
MONITOR_TRANSFER_T1T2
MONITOR_TRANSFER_T2T3
MONITOR_OUTFLOW
MONITOR_DIAGNOSTIC
MONITOR_SAFETY_ALERT
MONITOR_TRIP
```

The DT timing constraints model the *expected* duration of the corresponding
physical operations from the DT's monitoring perspective. They may differ
slightly from the PT (the DT observes telemetry with up to 2 time unit
latency — modelled by relaxed upper bounds), but must satisfy the same
IEC 61511 domain axioms on volume and flow rate.

For example:
```
MONITOR_IDLE -> MONITOR_INFLOW_ACTIVE
  trigger: pump_activate_monitor!
  guard:   t_inflow >= 0
  reset:   t_inflow := 0

MONITOR_INFLOW_ACTIVE -> MONITOR_IDLE
  trigger: pump_deactivate_monitor!
  guard:   t_inflow <= 122          [120 + 2 units telemetry latency tolerance]
  invariant on MONITOR_INFLOW_ACTIVE: t_inflow <= 122

[... construct remaining transitions analogously ...]
```

`safety_margin_breach!` must be reachable from `MONITOR_INFLOW_ACTIVE`,
`MONITOR_TRANSFER_T1T2`, `MONITOR_TRANSFER_T2T3`, and `MONITOR_OUTFLOW`.

---

## STEP 2 — Ontology

### File: `assets/CS1_ThreeTank/domain.ont`

```
; Ontology: Three-Tank Process Control System
; Standard references:
;   IEC 61511-1:2016 — Functional Safety: SIS for Process Industry Sector
;   IEC 61511-2:2016 — Guidelines for Application
;   ISA-84.00.01-2004 — Application of Safety Instrumented Systems
;
; Fragment: Quantifier-free Linear Real Arithmetic (QF_LRA)
; All axioms are linear over Volume, FlowRate, Time domains.

; === SORTS ===
sort Volume      ; litres — non-negative real, physical water quantity
sort FlowRate    ; litres per second — non-negative real
sort TankIndex   ; discrete index 1, 2, or 3 (represented as integer)
sort Pressure    ; bar — process pressure, used in SIL safety functions
sort Time        ; seconds — for timing constraint reference

; === FUNCTIONS ===
; Physical state functions
fun level         : TankIndex -> Volume   ; current water level in tank i (litres)
fun capacity      : TankIndex -> Volume   ; maximum rated capacity (IEC 61511 Cl.9.2)
fun inflow_rate   : TankIndex -> FlowRate ; volumetric inflow rate to tank i (L/s)
fun outflow_rate  : TankIndex -> FlowRate ; volumetric outflow rate from tank i (L/s)

; Aggregate quantities
fun total_volume  : Volume    ; sum level(1)+level(2)+level(3) — conservation law
fun net_flow      : FlowRate  ; total_inflow - total_outflow (positive = filling)

; Safety thresholds (from IEC 61511 SIL-2 process hazard analysis)
fun min_safe_level     : Volume    ; minimum safe level (SIL-2 low-low setpoint)
fun high_high_level    : Volume    ; maximum safe level (SIL-2 high-high setpoint)
fun alarm_setpoint_low : Volume    ; low-level alarm setpoint (before SIL trip)

; Actuator parameters
fun pump_rated_flow    : FlowRate  ; rated pump flow (IEC 61511 Cl.10.3)
fun valve_rated_flow   : FlowRate  ; rated valve flow (IEC 61511 Cl.10.3)

; Safety integrity parameters
fun sil_response_time  : Time      ; maximum allowable response time for SIL-2
fun proof_test_interval: Time      ; IEC 61511 Cl.11.6 proof test interval

; === RELATIONS ===
rel tank_overfull    : TankIndex  ; holds when level(i) > capacity(i) — violation
rel tank_underfull   : TankIndex  ; holds when level(i) < min_safe_level — alarm
rel system_safe      :            ; 0-ary: holds iff all tanks within safe bounds
rel pump_running     :            ; 0-ary: pump is energised and delivering flow
rel valve12_open     :            ; 0-ary: inter-tank valve T1->T2 is open
rel valve23_open     :            ; 0-ary: inter-tank valve T2->T3 is open

; === AXIOMS ===

; --- Physical vessel capacities (IEC 61511 Cl.9.2, vessel design specification) ---
axiom cap_t1        : capacity(1) = 100
axiom cap_t2        : capacity(2) = 80
axiom cap_t3        : capacity(3) = 60

; --- Safety setpoints (IEC 61511-1 Cl.9.3.3, SIL-2 hazard analysis output) ---
axiom min_safe      : min_safe_level = 10
axiom high_high     : high_high_level = 90
axiom alarm_low     : alarm_setpoint_low = 15

; --- Ordering of setpoints (must hold for safety logic to be coherent) ---
axiom setpoint_order_1 : min_safe_level < alarm_setpoint_low
axiom setpoint_order_2 : alarm_setpoint_low < high_high_level
axiom setpoint_order_3 : high_high_level < capacity(1)

; --- Non-negativity laws (physical law) ---
axiom nn_level_1    : level(1) >= 0
axiom nn_level_2    : level(2) >= 0
axiom nn_level_3    : level(3) >= 0

; --- Capacity bounds (physical law + IEC 61511 Cl.9.2.1) ---
axiom cap_bound_1   : level(1) <= capacity(1)
axiom cap_bound_2   : level(2) <= capacity(2)
axiom cap_bound_3   : level(3) <= capacity(3)

; --- Conservation: total volume is additive (physical law) ---
axiom conservation  : total_volume = level(1) + level(2) + level(3)

; --- Total volume bounds (derived from above — useful for solver) ---
axiom total_min     : total_volume >= 0
axiom total_max     : total_volume <= 240

; --- Rated flow parameters (IEC 61511 Cl.10.3, actuator data sheets) ---
axiom pump_flow     : pump_rated_flow = 5
axiom valve_flow    : valve_rated_flow = 4

; --- Flow rate non-negativity (physical: no reverse flow) ---
axiom pos_inflow_1  : inflow_rate(1) >= 0
axiom pos_inflow_2  : inflow_rate(2) >= 0
axiom pos_inflow_3  : inflow_rate(3) >= 0
axiom pos_outflow_1 : outflow_rate(1) >= 0
axiom pos_outflow_2 : outflow_rate(2) >= 0
axiom pos_outflow_3 : outflow_rate(3) >= 0

; --- Flow rate upper bounds (physical: cannot exceed rated capacity) ---
axiom max_inflow_1  : inflow_rate(1) <= pump_rated_flow
axiom max_inflow_2  : inflow_rate(2) <= valve_rated_flow
axiom max_inflow_3  : inflow_rate(3) <= valve_rated_flow
axiom max_outflow_1 : outflow_rate(1) <= valve_rated_flow
axiom max_outflow_2 : outflow_rate(2) <= valve_rated_flow
axiom max_outflow_3 : outflow_rate(3) <= 3

; --- SIL-2 response time (IEC 61511-1 Cl.9.3 safety function specification) ---
axiom sil_resp      : sil_response_time = 5

; --- Proof test interval (IEC 61511-1 Cl.11.6 maintenance requirement) ---
axiom proof_test    : proof_test_interval = 60

; --- Net flow definition ---
axiom net_flow_def  : net_flow = inflow_rate(1) - outflow_rate(3)
```

---

## STEP 3 — Interpretations

### File: `assets/CS1_ThreeTank/pt.interp`

```
; PT Interpretation: ThreeTankPT
; Maps each PT location and event label to its IEC 61511 domain-level meaning.
; Standard: IEC 61511-1:2016

; --- Location interpretations (Condition I: state consistency) ---
; Each location formula captures what must be true about the physical world
; when the PT is in that location, under the IEC 61511 ontology.

IDLE             : (and (>= total_volume min_safe_level) (<= total_volume high_high_level))
FILLING          : (and (>= inflow_rate(1) 0) (<= level(1) capacity(1)) (>= level(1) 0))
FLOWING_12       : (and (>= inflow_rate(2) 0) (>= level(1) min_safe_level))
FLOWING_23       : (and (>= inflow_rate(3) 0) (>= level(2) min_safe_level))
DRAINING         : (and (>= outflow_rate(3) 0) (<= outflow_rate(3) 3))
PROOF_TEST       : (>= total_volume min_safe_level)
ALARM            : (< level(3) min_safe_level)
EMERGENCY_STOP   : (< level(3) min_safe_level)

; --- Event interpretations (Conditions II-III: transition matching) ---
; Each event formula captures the domain-level precondition / meaning
; of firing that event.

pump_on!             : (and (>= inflow_rate(1) 2) (<= level(1) capacity(1)))
pump_off!            : (<= inflow_rate(1) 0)
valve12_open!        : (and (>= level(1) 5) (>= outflow_rate(1) 2))
valve12_close!       : (<= outflow_rate(1) 0)
valve23_open!        : (and (>= level(2) 5) (>= outflow_rate(2) 2))
valve23_close!       : (<= outflow_rate(2) 0)
drain!               : (>= outflow_rate(3) 0)
level_critical!      : (< level(3) min_safe_level)
proof_test_start!    : (>= total_volume min_safe_level)
proof_test_complete! : (and (>= total_volume min_safe_level) (<= total_volume high_high_level))
emergency_shutdown!  : (< level(3) min_safe_level)
system_reset!        : (>= total_volume min_safe_level)
```

### File: `assets/CS1_ThreeTank/dt.interp`

```
; DT Interpretation: ThreeTankDT
; Maps each DT monitoring label to its IEC 61511 domain-level meaning.
; The DT uses a water-budget / process safety vocabulary distinct from
; the field instrument vocabulary used by the PT, but semantically
; equivalent under the shared IEC 61511 ontology.

; --- Location interpretations ---
MONITOR_IDLE           : (and (>= total_volume min_safe_level) (<= total_volume high_high_level))
MONITOR_INFLOW_ACTIVE  : (and (>= inflow_rate(1) 0) (<= level(1) capacity(1)))
MONITOR_TRANSFER_T1T2  : (and (>= inflow_rate(2) 0) (>= level(1) min_safe_level))
MONITOR_TRANSFER_T2T3  : (and (>= inflow_rate(3) 0) (>= level(2) min_safe_level))
MONITOR_OUTFLOW        : (and (>= outflow_rate(3) 0) (<= outflow_rate(3) 3))
MONITOR_DIAGNOSTIC     : (>= total_volume min_safe_level)
MONITOR_SAFETY_ALERT   : (< level(3) min_safe_level)
MONITOR_TRIP           : (< level(3) min_safe_level)

; --- Event interpretations ---
; These are semantically equivalent to their PT counterparts under Delta.
; Z3 will confirm: Delta |= I_PT(pump_on!) <-> I_DT(pump_activate_monitor!)
; etc. for each pair.

pump_activate_monitor!   : (and (>= inflow_rate(1) 2) (<= level(1) capacity(1)))
pump_deactivate_monitor! : (<= inflow_rate(1) 0)
inflow_t2_start!         : (and (>= level(1) 5) (>= outflow_rate(1) 2))
inflow_t2_stop!          : (<= outflow_rate(1) 0)
inflow_t3_start!         : (and (>= level(2) 5) (>= outflow_rate(2) 2))
inflow_t3_stop!          : (<= outflow_rate(2) 0)
outflow_start!           : (>= outflow_rate(3) 0)
safety_margin_breach!    : (< level(3) min_safe_level)
diagnostic_cycle_start!  : (>= total_volume min_safe_level)
diagnostic_cycle_complete! : (and (>= total_volume min_safe_level) (<= total_volume high_high_level))
protective_trip_command! : (< level(3) min_safe_level)
```

---

## STEP 4 — Misalignment Variants

### Variant A — Threshold Drift (RQ2 primary)

### File: `assets/CS1_ThreeTank/V2_DT_thresh_drift.xml`

Copy `V2_DT.xml` and change one thing only: the `MONITOR_SAFETY_ALERT`
location becomes reachable with a guard that triggers `safety_margin_breach!`
when `level(3) < 5` instead of the correct `< min_safe_level (= 10)`.
This is a **threshold drift misalignment**: the DT uses a tighter threshold
(5 instead of 10), so states where `5 <= level(3) < 10` are not flagged by
the DT despite being unsafe per IEC 61511.

### File: `assets/CS1_ThreeTank/dt_thresh_drift.interp`

Change only the `safety_margin_breach!` mapping:
```
safety_margin_breach! : (< level(3) 5)
```
All other mappings identical to `dt.interp`.

### Variant B — Label Orphan (RQ2 syntactic baseline)

### File: `assets/CS1_ThreeTank/V2_DT_orphan.xml`

Copy `V2_DT.xml` and rename `safety_margin_breach!` to `new_alert_event!`
without adding it to any interpretation file. This creates a DT label with
no ontological interpretation — the semantic alignment checker must flag this
as an unmatched label, whereas a syntactic bisimulation checker would also
fail but for the wrong reason (label mismatch, not semantic gap).

### Variant C — Ontology Evolution (RQ1)

### File: `assets/CS1_ThreeTank/domain_v2.ont`

Refine `domain.ont` by applying Definition 7 (all three conditions):

**Condition I — Vocabulary extension:**
Add a new sort and functions not in the original ontology:
```
sort Conductivity  ; mS/cm — water quality parameter (IEC 61511 environmental monitoring)
fun conductivity   : TankIndex -> Conductivity  ; water conductivity in tank i
fun max_conductivity : Conductivity             ; IEC 61511 process quality limit
```

**Condition II — Axiom strengthening:**
Tighten the minimum safe level (refined from field measurements):
```
; REPLACES: axiom min_safe : min_safe_level = 10
axiom min_safe_v2 : min_safe_level = 12
; This is a legitimate IEC 61511 refinement: field calibration revealed
; the original 10L setpoint was insufficiently conservative.
```

**Condition III — Interpretation tightening:**
The `level_critical!` / `safety_margin_breach!` interpretation becomes
tighter to reflect the updated setpoint:
Update both `pt_v2.interp` and `dt_v2.interp` accordingly.

Expected outcome: alignment established under `domain.ont` is preserved
under `domain_v2.ont` (Theorem 1). The checker re-verifies incrementally,
reporting fewer SMT calls than a full re-run from scratch.

---

## STEP 5 — Benchmark Runner

### File: `benchmark/run_CS1.cpp`

```cpp
#include <iostream>
#include <fstream>
#include "rtwbs.h"
#include "rtwbs/semantic/ontology.h"
#include "rtwbs/semantic/interpretation.h"
#include "rtwbs/semantic/alignment_checker.h"
#include "rtwbs/semantic/runtime_monitor.h"
#include "rtwbs/benchmarks/common.h"

// CS1: Three-Tank System (Gil et al., IEC 61511)
// Tests: RQ1 (ontology evolution), RQ2 (coverage vs syntactic + runtime monitor),
//        RQ3 (scalability), RQ4 (engineering cost)

static const std::string ASSET_DIR = "assets/CS1_ThreeTank/";
static const std::string RESULT_FILE = "results/CS1_results.csv";

int main(int argc, char* argv[]) {
    using namespace rtwbs;
    using namespace rtwbs::semantic;

    std::string results_folder = "results/";
    parse_arguments(argc, argv, &results_folder, nullptr, nullptr, nullptr, false, nullptr);

    std::ofstream csv(results_folder + "CS1_results.csv");
    AlignmentResult::write_csv_header(csv);

    OntologyParser ont_parser;
    InterpretationParser interp_parser;
    SemanticAlignmentChecker checker;

    // --- Load base ontology and interpretations ---
    auto ontology = ont_parser.parse_file(ASSET_DIR + "domain.ont");
    auto pt_interp = interp_parser.parse_file(ASSET_DIR + "pt.interp");
    auto dt_interp = interp_parser.parse_file(ASSET_DIR + "dt.interp");

    // --- Load models ---
    TimedAutomaton pt(ASSET_DIR + "V1_PT.xml");
    TimedAutomaton dt(ASSET_DIR + "V2_DT.xml");
    pt.construct_zone_graph();
    dt.construct_zone_graph();

    std::cout << "=== CS1: Three-Tank System ===" << std::endl;
    std::cout << "PT zones: " << pt.get_num_states()
              << " | DT zones: " << dt.get_num_states() << std::endl;

    // =========================================================
    // RUN 1: Semantic alignment — aligned pair
    // Expected: ALIGNED = true
    // =========================================================
    std::cout << "\n[RUN 1] Semantic alignment (aligned pair)..." << std::endl;
    auto result_aligned = checker.check(pt, dt, ontology, pt_interp, dt_interp);
    result_aligned.print();
    result_aligned.append_to_csv(csv, "CS1", "aligned", "semantic");
    assert(result_aligned.aligned && "CS1 aligned pair should be ALIGNED");

    // =========================================================
    // RUN 2: Syntactic bisimulation baseline — same pair
    // Expected: NOT aligned (labels differ syntactically)
    // This is the RQ2 key result: syntactic misses semantic equivalence
    // =========================================================
    std::cout << "\n[RUN 2] Syntactic bisimulation baseline (same pair)..." << std::endl;
    auto result_syntactic = checker.check_syntactic(pt, dt);
    result_syntactic.print();
    result_syntactic.append_to_csv(csv, "CS1", "aligned", "syntactic");
    assert(!result_syntactic.aligned
           && "CS1 syntactic baseline should FAIL — labels differ");

    // =========================================================
    // RUN 3: Semantic alignment — threshold drift misalignment
    // Expected: ALIGNED = false, counterexample involving safety_margin_breach!
    // =========================================================
    std::cout << "\n[RUN 3] Threshold drift misalignment..." << std::endl;
    TimedAutomaton dt_drift(ASSET_DIR + "V2_DT_thresh_drift.xml");
    dt_drift.construct_zone_graph();
    auto dt_drift_interp = interp_parser.parse_file(ASSET_DIR + "dt_thresh_drift.interp");
    auto result_drift = checker.check(pt, dt_drift, ontology, pt_interp, dt_drift_interp);
    result_drift.print();
    result_drift.append_to_csv(csv, "CS1", "thresh_drift", "semantic");
    assert(!result_drift.aligned && "CS1 threshold drift should be NOT ALIGNED");

    // =========================================================
    // RUN 4: Runtime monitor baseline on threshold drift
    // Simulates 1000 traces; records how many needed to detect misalignment.
    // This is the RQ2 / RQ4 comparison data.
    // =========================================================
    std::cout << "\n[RUN 4] Runtime monitor baseline (threshold drift)..." << std::endl;
    RuntimeMonitorBaseline monitor;
    monitor.add_monitor({
        "IEC61511_LowLow_Safety",
        "(< level(3) min_safe_level)",   // correct property from ontology
        "safety_margin_breach!",
        true  // safety property
    });
    monitor.add_monitor({
        "IEC61511_TotalVolume_Bound",
        "(>= total_volume 0)",
        "pump_activate_monitor!",
        true
    });
    auto monitor_results = monitor.run(pt, dt_drift, ontology,
                                       pt_interp, dt_drift_interp,
                                       1000, 500, 42);
    for (const auto& mr : monitor_results) {
        std::cout << "Monitor '" << mr.monitor_name << "': "
                  << (mr.violation_detected ? "DETECTED" : "NOT DETECTED")
                  << " | traces needed: " << mr.traces_simulated
                  << " | steps to detection: " << mr.steps_to_detection
                  << " | overhead/step: " << mr.overhead_per_step_us << " us" << std::endl;
    }
    // Append monitor results to CSV with special mode tag
    // (extend AlignmentResult or write a separate monitor CSV section)

    // =========================================================
    // RUN 5: Ontology evolution — RQ1
    // Verify alignment preserved under domain_v2.ont (Theorem 1)
    // =========================================================
    std::cout << "\n[RUN 5] Ontology evolution (RQ1)..." << std::endl;
    auto ontology_v2 = ont_parser.parse_file(ASSET_DIR + "domain_v2.ont");
    auto pt_interp_v2 = interp_parser.parse_file(ASSET_DIR + "pt_v2.interp");
    auto dt_interp_v2 = interp_parser.parse_file(ASSET_DIR + "dt_v2.interp");
    auto evo_result = checker.check_evolution(
        pt, dt, ontology_v2, pt_interp_v2, dt_interp_v2, result_aligned);
    std::cout << "Evolution preserved: " << (evo_result.preserved ? "YES" : "NO")
              << " | Incremental SMT calls: " << evo_result.incremental_smt_calls
              << " | Time: " << evo_result.recheck_time_ms << " ms" << std::endl;

    // =========================================================
    // RQ4 Engineering Cost Report
    // =========================================================
    std::cout << "\n[RQ4] Engineering cost summary:" << std::endl;
    std::cout << "  SemAlign one-time cost:      " << result_aligned.total_time_ms << " ms" << std::endl;
    std::cout << "  SemAlign SMT calls:          " << result_aligned.smt_calls_precompute
                                                      + result_aligned.smt_calls_seeding << std::endl;
    std::cout << "  Monitor properties needed:   " << monitor_results.size() << std::endl;
    std::cout << "  Monitor LOC estimate:        " << monitor.estimate_monitor_loc() << std::endl;
    std::cout << "  Monitor overhead/step:       "
              << monitor_results[0].overhead_per_step_us << " us" << std::endl;
    std::cout << "  Properties covered by K~:    "
              << result_aligned.label_equiv_pairs << std::endl;

    std::cout << "\nResults written to: " << results_folder + "CS1_results.csv" << std::endl;
    return 0;
}
```

Add `run_CS1` as a CMake target in `CMakeLists.txt` following the existing
pattern for `run_FMICS2021`.