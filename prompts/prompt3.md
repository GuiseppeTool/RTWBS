# Prompt CS3 — Autonomous Inspection Rover (NASA/FRET)

## Prerequisites

Prompts 0, CS1, and CS2 must be fully implemented and passing before starting.
Read `doc/` (the paper) fully. Study `assets/CS1_ThreeTank/` and
`assets/CS2_Crane/` as the reference for expected file structure, ontology
depth, and interpretation style.

---

## Background and Domain

This case study is derived from the NASA FRET (Formal Requirements
Elicitation Tool) inspection rover use case, documented in Giannakopoulou
et al. [NFM 2020, "Formal Requirements Elicitation with FRET"] and used
as a running evaluation case in the FRET ecosystem. The rover performs
autonomous inspection of a facility or planetary surface, visiting a
sequence of waypoints under temporal requirements formalized in FRET's
structured natural language and compiled to temporal logic.

The system is governed by the following standards and references:

- **NASA NPR 7150.2D (2022)** — NASA Software Engineering Requirements.
  The primary NASA standard for software assurance:
  - Requirement 3.1.2: Software shall perform its intended function within
    the allocated performance requirements
  - Requirement 3.6.1: Safety-critical software shall be identified and
    managed under enhanced development assurance
  - Requirement 7.1.1: Timing and sequencing constraints shall be documented
    and verifiable
- **NASA-STD-8739.8A (2017)** — Software Assurance Standard:
  - Section 3.3: Verification of timing properties for safety-critical
    functions
- **ECSS-E-ST-10-06C (2017)** — Space Engineering: Technical Requirements
  Specification — provides the formal requirements format for space-grade
  autonomous systems, including timing windows for autonomous operations
- **ISO/IEC 25010:2011** — Systems and Software Quality Requirements,
  specifically reliability and fault-tolerance sub-characteristics used
  to specify the fault detection and recovery requirements
- **RTCA DO-178C (2011)** — applied by analogy for timing assurance:
  mission segment completion time must be verifiable against allocated
  budgets (even though DO-178C strictly applies to airborne systems,
  NASA guidance NASA-HDBK-2203 endorses its application to safety-critical
  rover software under Development Assurance Level B)

The **PT view** models the physical rover from the perspective of the
*flight software engineer*: navigation commands, waypoint acknowledgements,
sensor activation events, and fault detection triggers are the observable
events. Timing constraints are hard deadlines from the mission operations
timeline.

The **DT view** models a **ground-station mission monitoring twin** authored
by the *mission operations team*. The ground station receives telemetry
with a bounded latency (up to 4 time units modelled in the DT's relaxed
timing) and tracks mission progress as a percentage-complete metric rather
than discrete waypoint indices. The DT fires events when telemetry
thresholds are crossed, using a vocabulary drawn from mission operations
documentation rather than flight software specifications.

The key semantic alignment challenge: the PT fires `waypoint_reached!`
when the rover's onboard navigation confirms arrival at a waypoint
(position error < tolerance); the DT fires `telemetry_position_update!`
when the ground station's telemetry stream shows the rover's reported
position matches the next waypoint coordinate within the monitoring
threshold. These are semantically identical — both require
`mission_progress` to have advanced and `distance_to_waypoint` to be
below the acceptance radius — but use entirely different labels authored
by different teams. Syntactic bisimulation fails; semantic alignment succeeds.

---

## STEP 1 — UPPAAL Models

### File: `assets/CS3_Rover/V1_PT.xml`

Template name: `RoverPT`

**Clocks:** `t_mission`, `t_segment`, `t_inspect`, `t_battery`, `t_fault`

**Locations (10 total):**
```
STANDBY         — rover powered but awaiting mission start command
NAVIGATING      — autonomous navigation to next waypoint active
AT_WAYPOINT     — rover arrived at waypoint, navigation paused
INSPECTING      — sensor suite active, data collection in progress
RETURNING       — return-to-base navigation active (battery or mission end)
CHARGING        — battery charging at base station
SAFE_MODE       — fault-induced: non-critical fault, reduced operations
FAULT_CRITICAL  — critical fault: immediate mission abort required
MISSION_COMPLETE— all waypoints inspected, mission ended nominally
COMMS_LOST      — communication timeout (ECSS-E-ST-10-06C Cl.5.2.3)
```

**Transitions and timing constraints:**

```
STANDBY -> NAVIGATING
  trigger: mission_start!
  guard:   t_battery >= 0         [battery check: modelled via t_battery clock]
  reset:   t_mission := 0, t_segment := 0

NAVIGATING -> AT_WAYPOINT
  trigger: waypoint_reached!
  guard:   t_segment >= 5 && t_segment <= 120
           [NASA NPR 7150.2 Req 7.1.1: mission segment must complete
            within allocated time window. Min 5 units for travel settling.]
  invariant on NAVIGATING: t_segment <= 120
  reset:   t_segment := 0, t_inspect := 0

AT_WAYPOINT -> INSPECTING
  trigger: inspection_start!
  guard:   t_inspect >= 0
  reset:   t_inspect := 0

INSPECTING -> NAVIGATING
  trigger: inspection_complete!
  guard:   t_inspect >= 10 && t_inspect <= 60
           [ECSS-E-ST-10-06C: inspection window 10–60 time units per waypoint]
  invariant on INSPECTING: t_inspect <= 60
  reset:   t_segment := 0

INSPECTING -> MISSION_COMPLETE
  trigger: mission_end!
  guard:   t_mission >= 0         [all waypoints done — can occur from last inspection]

NAVIGATING -> RETURNING
  trigger: return_initiated!
  guard:   t_battery >= 150       [NASA NPR 7150.2: mandatory return when
                                   battery budget < 25% — modelled as t_battery >= 150]
  reset:   t_segment := 0

RETURNING -> CHARGING
  trigger: base_reached!
  guard:   t_segment >= 5 && t_segment <= 90
  invariant on RETURNING: t_segment <= 90
  reset:   t_battery := 0

CHARGING -> STANDBY
  trigger: charge_complete!
  guard:   t_battery >= 60        [minimum charge time for full replenishment]
  invariant on CHARGING: t_battery <= 200

ANY -> SAFE_MODE
  trigger: fault_noncritical!
  guard:   t_fault <= 5
           [NASA-STD-8739.8A Sec.3.3: non-critical fault response ≤5 time units]
  (reachable from NAVIGATING, INSPECTING, RETURNING)

SAFE_MODE -> NAVIGATING
  trigger: fault_cleared!
  guard:   t_fault >= 3 && t_fault <= 30
  invariant on SAFE_MODE: t_fault <= 30
  reset:   t_fault := 0

ANY -> FAULT_CRITICAL
  trigger: fault_critical!
  guard:   t_fault <= 3
           [NASA NPR 7150.2 Req 3.6.1: critical fault response ≤3 time units]
  (reachable from any active mission state)

FAULT_CRITICAL -> RETURNING
  trigger: emergency_return!
  guard:   t_fault <= 5
  invariant on FAULT_CRITICAL: t_fault <= 5

NAVIGATING -> COMMS_LOST
  trigger: comms_timeout!
  guard:   t_segment >= 60
           [ECSS-E-ST-10-06C Cl.5.2.3: comms loss declared after 60 units]

COMMS_LOST -> NAVIGATING
  trigger: comms_restored!
  guard:   t_fault >= 1
```

### File: `assets/CS3_Rover/V2_DT.xml`

Template name: `RoverDT`

The DT is a **ground-station mission monitoring twin**. It receives
telemetry from the rover and tracks mission progress as a percentage
metric rather than discrete waypoint events. It was authored
independently by the mission operations team using their own vocabulary.

**Label mapping intent (to be proven via SMT):**
```
PT label               DT label
mission_start!      --> mission_uplink_command!
waypoint_reached!   --> telemetry_position_update!
inspection_start!   --> sensor_activation_telemetry!
inspection_complete!--> data_downlink_event!
return_initiated!   --> return_command_uplinked!
base_reached!       --> base_contact_confirmed!
charge_complete!    --> power_nominal_telemetry!
fault_noncritical!  --> anomaly_flag_minor!
fault_cleared!      --> nominal_ops_restored!
fault_critical!     --> anomaly_flag_critical!
emergency_return!   --> abort_sequence_commanded!
comms_timeout!      --> telemetry_gap_detected!
comms_restored!     --> telemetry_resumed!
mission_end!        --> mission_complete_telemetry!
```

**Clocks:** `t_telem`, `t_downlink`, `t_power`, `t_anomaly`

**Locations (9 total):**
```
DT_STANDBY
DT_NAV_MONITORING
DT_WAYPOINT_DWELL
DT_SENSOR_ACTIVE
DT_RETURN_MONITORING
DT_RECHARGE_MONITORING
DT_MINOR_ANOMALY
DT_CRITICAL_ANOMALY
DT_MISSION_COMPLETE
```

Telemetry latency tolerance: DT upper bounds are PT upper bounds + 4 time
units (bounded telemetry latency, modelled as relaxed timing in weak
timed bisimulation, consistent with the existing RTWBS relaxed receiver
semantics in the codebase — see `TA_CONFIG.receiver_suffix`).

---

## STEP 2 — Ontology

### File: `assets/CS3_Rover/domain.ont`

```
; Ontology: Autonomous Inspection Rover — Mission Domain
; Standard references:
;   NASA NPR 7150.2D:2022 — NASA Software Engineering Requirements
;   NASA-STD-8739.8A:2017 — Software Assurance Standard
;   ECSS-E-ST-10-06C:2017 — Technical Requirements Specification
;   ISO/IEC 25010:2011    — Systems and Software Quality Requirements
;   NASA-HDBK-2203        — NASA Software Engineering Handbook (DO-178C guidance)
;
; Fragment: Quantifier-free Linear Real Arithmetic (QF_LRA)

; === SORTS ===
sort Distance      ; metres — spatial distance, non-negative real
sort Energy        ; Wh — electrical energy, non-negative real
sort Percentage    ; 0-100 real — mission completion percentage
sort WaypointIndex ; integer 0..N — discrete waypoint identifier
sort DataVolume    ; MB — inspection data collected, non-negative
sort SignalStrength ; dBm — communication link quality

; === FUNCTIONS ===
; Navigation and position
fun distance_to_waypoint  : Distance     ; Euclidean distance to next waypoint (m)
fun waypoint_accept_radius: Distance     ; acceptance radius for waypoint arrival (m)
fun distance_to_base      : Distance     ; distance from current position to base (m)
fun waypoints_completed   : WaypointIndex; count of waypoints successfully inspected
fun total_waypoints       : WaypointIndex; total mission waypoints
fun mission_progress      : Percentage   ; (waypoints_completed / total_waypoints) * 100

; Energy and power budget
fun battery_level         : Energy       ; current battery charge (Wh)
fun battery_capacity      : Energy       ; rated battery capacity (Wh)
fun battery_reserve       : Energy       ; minimum reserve before mandatory return (Wh)
fun energy_to_base        : Energy       ; energy required to return to base (Wh)

; Data collection
fun data_collected        : DataVolume   ; total inspection data acquired (MB)
fun data_required_per_wp  : DataVolume   ; minimum data per waypoint (MB)

; Communication
fun signal_strength       : SignalStrength; current comms link quality (dBm)
fun min_signal_threshold  : SignalStrength; minimum for reliable comms (dBm)
fun comms_latency         : Distance     ; round-trip comms delay equivalent

; Timing parameters (NASA NPR 7150.2 Req 7.1.1)
fun max_segment_time      : Distance     ; maximum time per navigation segment
fun inspection_time_min   : Distance     ; minimum inspection duration per waypoint
fun inspection_time_max   : Distance     ; maximum inspection duration per waypoint
fun fault_response_critical: Distance    ; max response time for critical fault (NASA Req 3.6.1)
fun fault_response_noncritical: Distance ; max response time for non-critical fault

; === RELATIONS ===
rel waypoint_reached       :             ; distance_to_waypoint <= waypoint_accept_radius
rel battery_critical       :             ; battery_level <= battery_reserve + energy_to_base
rel mission_complete       :             ; waypoints_completed = total_waypoints
rel comms_nominal          :             ; signal_strength >= min_signal_threshold
rel data_sufficient        : WaypointIndex ; data_collected >= data_required_per_wp

; === AXIOMS ===

; --- Mission parameters (ECSS-E-ST-10-06C operational specification) ---
axiom total_wp              : total_waypoints = 5
axiom accept_radius         : waypoint_accept_radius = 2
axiom data_per_wp           : data_required_per_wp = 10

; --- Energy budget (NASA NPR 7150.2 Req 3.1.2 performance allocation) ---
axiom bat_capacity          : battery_capacity = 100
axiom bat_reserve           : battery_reserve = 25
axiom nn_battery            : battery_level >= 0
axiom bat_upper             : battery_level <= battery_capacity
axiom energy_to_base_val    : energy_to_base = 20
axiom reserve_ordering      : battery_reserve > energy_to_base

; --- Mission progress definition ---
axiom progress_lower        : mission_progress >= 0
axiom progress_upper        : mission_progress <= 100
axiom progress_monotone     : waypoints_completed >= 0
axiom wp_upper_bound        : waypoints_completed <= total_waypoints

; --- Timing requirements (NASA NPR 7150.2 Req 7.1.1) ---
axiom max_seg_time          : max_segment_time = 120
axiom inspect_min           : inspection_time_min = 10
axiom inspect_max           : inspection_time_max = 60
axiom fault_crit_resp       : fault_response_critical = 3
axiom fault_noncrit_resp    : fault_response_noncritical = 5

; --- Communication parameters (ECSS-E-ST-10-06C Cl.5.2.3) ---
axiom min_signal_val        : min_signal_threshold = -85
axiom comms_latency_bound   : comms_latency >= 0

; --- Distance non-negativity ---
axiom nn_dist_wp            : distance_to_waypoint >= 0
axiom nn_dist_base          : distance_to_base >= 0

; --- Data collection non-negativity ---
axiom nn_data               : data_collected >= 0

; --- Waypoint reached definition ---
; waypoint_reached holds iff distance_to_waypoint <= waypoint_accept_radius
axiom wp_reached_def        : distance_to_waypoint <= waypoint_accept_radius + 100
; (upper bound relaxed to keep QF_LRA fragment — exact definition
;  is expressed via relation semantics in the SMT encoding)

; --- Battery criticality definition ---
; battery_critical holds iff battery_level <= battery_reserve + energy_to_base = 45
axiom bat_critical_thresh   : battery_reserve + energy_to_base = 45

; --- Signal ordering ---
axiom signal_upper          : signal_strength <= 0
; (dBm is non-positive; 0 dBm is maximum)
```

---

## STEP 3 — Interpretations

### File: `assets/CS3_Rover/pt.interp`

```
; PT Interpretation: RoverPT
; Flight software engineer perspective.
; Standard: NASA NPR 7150.2D

; --- Location interpretations ---
STANDBY          : (and (>= battery_level battery_reserve) (= waypoints_completed 0))
NAVIGATING       : (and (> distance_to_waypoint 0) (>= battery_level battery_reserve))
AT_WAYPOINT      : (<= distance_to_waypoint waypoint_accept_radius)
INSPECTING       : (and (<= distance_to_waypoint waypoint_accept_radius) (>= data_collected 0))
RETURNING        : (and (> distance_to_base 0) (< battery_level battery_capacity))
CHARGING         : (= distance_to_base 0)
SAFE_MODE        : (>= battery_level battery_reserve)
FAULT_CRITICAL   : (< battery_level battery_capacity)
MISSION_COMPLETE : (= waypoints_completed total_waypoints)
COMMS_LOST       : (< signal_strength min_signal_threshold)

; --- Event interpretations ---
mission_start!       : (and (>= battery_level battery_reserve) (= waypoints_completed 0))
waypoint_reached!    : (and (<= distance_to_waypoint waypoint_accept_radius) (>= battery_level battery_reserve))
inspection_start!    : (<= distance_to_waypoint waypoint_accept_radius)
inspection_complete! : (and (<= distance_to_waypoint waypoint_accept_radius) (>= data_collected data_required_per_wp))
return_initiated!    : (<= battery_level battery_reserve + energy_to_base)
base_reached!        : (= distance_to_base 0)
charge_complete!     : (>= battery_level battery_capacity - 5)
fault_noncritical!   : (and (>= battery_level battery_reserve) (< signal_strength min_signal_threshold + 10))
fault_cleared!       : (and (>= battery_level battery_reserve) (>= signal_strength min_signal_threshold))
fault_critical!      : (< battery_level battery_reserve)
emergency_return!    : (< battery_level battery_reserve)
comms_timeout!       : (< signal_strength min_signal_threshold)
comms_restored!      : (>= signal_strength min_signal_threshold)
mission_end!         : (= waypoints_completed total_waypoints)
```

### File: `assets/CS3_Rover/dt.interp`

```
; DT Interpretation: RoverDT
; Mission operations / ground station perspective.
; Uses telemetry-derived quantities: mission_progress percentage,
; signal_strength from downlink, data_collected from downlink stream.
; Semantically equivalent to PT interpretations under Delta.

; --- Location interpretations ---
DT_STANDBY           : (and (>= battery_level battery_reserve) (= mission_progress 0))
DT_NAV_MONITORING    : (and (> distance_to_waypoint 0) (>= battery_level battery_reserve))
DT_WAYPOINT_DWELL    : (<= distance_to_waypoint waypoint_accept_radius)
DT_SENSOR_ACTIVE     : (and (<= distance_to_waypoint waypoint_accept_radius) (>= data_collected 0))
DT_RETURN_MONITORING : (and (> distance_to_base 0) (< battery_level battery_capacity))
DT_RECHARGE_MONITORING: (= distance_to_base 0)
DT_MINOR_ANOMALY     : (>= battery_level battery_reserve)
DT_CRITICAL_ANOMALY  : (< battery_level battery_capacity)
DT_MISSION_COMPLETE  : (= mission_progress 100)

; --- Event interpretations ---
; Z3 must confirm equivalence under Delta for each pair, e.g.:
;   Delta |= I_PT(waypoint_reached!) <-> I_DT(telemetry_position_update!)
; Both require distance_to_waypoint <= waypoint_accept_radius AND battery sufficient.

mission_uplink_command!      : (and (>= battery_level battery_reserve) (= mission_progress 0))
telemetry_position_update!   : (and (<= distance_to_waypoint waypoint_accept_radius) (>= battery_level battery_reserve))
sensor_activation_telemetry! : (<= distance_to_waypoint waypoint_accept_radius)
data_downlink_event!         : (and (<= distance_to_waypoint waypoint_accept_radius) (>= data_collected data_required_per_wp))
return_command_uplinked!     : (<= battery_level battery_reserve + energy_to_base)
base_contact_confirmed!      : (= distance_to_base 0)
power_nominal_telemetry!     : (>= battery_level battery_capacity - 5)
anomaly_flag_minor!          : (and (>= battery_level battery_reserve) (< signal_strength min_signal_threshold + 10))
nominal_ops_restored!        : (and (>= battery_level battery_reserve) (>= signal_strength min_signal_threshold))
anomaly_flag_critical!       : (< battery_level battery_reserve)
abort_sequence_commanded!    : (< battery_level battery_reserve)
telemetry_gap_detected!      : (< signal_strength min_signal_threshold)
telemetry_resumed!           : (>= signal_strength min_signal_threshold)
mission_complete_telemetry!  : (= mission_progress 100)
```

---

## STEP 4 — Misalignment Variants

### Variant A — Timing Violation: Telemetry Latency Exceeds Specification

### File: `assets/CS3_Rover/V2_DT_timing_violation.xml`

Copy `V2_DT.xml`. Change the `DT_NAV_MONITORING` -> `DT_WAYPOINT_DWELL`
transition so that `telemetry_position_update!` can fire up to
`t_telem <= 130` instead of the correct bound of `<= 124` (120 + 4
latency tolerance). The 6 extra time units mean the DT can report waypoint
arrival **after** the NASA NPR 7150.2 segment deadline has elapsed. This is
a **timing violation** misalignment: the DT appears to confirm arrival
within the mission window, but the semantic alignment checker detects that
the DT's timing interpretation violates the domain axiom
`max_segment_time = 120`.

This misalignment is particularly important for RQ2 because:
1. Syntactic bisimulation **cannot detect it** — the labels are already
   different so syntactic bisim fails for the wrong reason
2. A state-based runtime monitor **cannot detect it** — it only checks
   property values at states, not relative timing between PT and DT events
3. Only bisimulation-based alignment (checking delay Condition IV) catches
   the timing gap

### File: `assets/CS3_Rover/V2_DT_thresh_drift.xml`

Copy `V2_DT.xml`. Change `anomaly_flag_critical!` to fire when
`battery_level < 30` instead of `< battery_reserve (= 25)`. This creates
a 5 Wh gap where the DT raises a false critical alarm while the battery
is actually still sufficient for a safe return. This erodes trust in the
DT and causes unnecessary mission aborts.

### File: `assets/CS3_Rover/dt_thresh_drift.interp`

```
anomaly_flag_critical! : (< battery_level 30)
```

### Variant C — Ontology Evolution (RQ1)

### File: `assets/CS3_Rover/domain_v2.ont`

**Condition I — Vocabulary extension:**
```
sort Radiation     ; mSv/h — radiation dose rate (relevant for nuclear facility inspection)
fun radiation_level   : Radiation
fun max_radiation      : Radiation    ; NASA-STD-8739.8A mission abort threshold
```

**Condition II — Axiom strengthening:**
```
; Original: battery_reserve = 25
; Refined: battery_reserve = 30
; Reason: field mission data showed energy_to_base underestimated by 5 Wh
; on rough terrain. NASA NPR 7150.2 risk re-assessment increases reserve margin.
axiom bat_reserve_v2   : battery_reserve = 30
axiom max_rad_val      : max_radiation = 2
axiom nn_radiation     : radiation_level >= 0
```

**Condition III — Interpretation tightening:**
Update `return_initiated!` / `return_command_uplinked!` interpretations
to reference `battery_reserve = 30` in `pt_v2.interp` and `dt_v2.interp`.

Expected outcome: alignment preserved under `domain_v2.ont`. The
incremental re-verification only needs to re-check SMT entailments that
involve `battery_reserve` — not the full label equivalence set.

---

## STEP 5 — Benchmark Runner

### File: `benchmark/run_CS3.cpp`

Follow the same structure as `benchmark/run_CS1.cpp`.

Include the same five runs (aligned, syntactic baseline, misalignment,
runtime monitor baseline, ontology evolution).

For the runtime monitor in Run 4, use the timing violation variant
(`V2_DT_timing_violation.xml`) and instrument two monitors:
```
Monitor 1: "NPR7150_SegmentDeadline"
  property: (< t_segment max_segment_time)  [segment must complete in time]
  label:    telemetry_position_update!
  type:     safety
  NOTE: This is a TIMING property — the monitor must check wall-clock
        trace timing, not just state values. Implement it by comparing
        the clock value at the DT event against the PT clock at the
        corresponding PT event. This requires cross-model clock comparison,
        which is only possible if the monitor has access to both PT and DT
        traces simultaneously — a significant engineering burden documented
        in the RQ4 cost report.

Monitor 2: "NPR7150_BatteryCritical"
  property: (>= battery_level battery_reserve)
  label:    return_command_uplinked!
  type:     safety
```

For the timing violation monitor (Monitor 1): document in the RQ4 cost
section that this monitor requires *cross-model synchronisation*, which
is an engineering overhead not required by SemAlign (which checks timing
via delay Condition IV in the bisimulation). Estimate the additional
implementation cost as 25 LOC beyond a simple state-based monitor, and
note that this type of property is outside the scope of the Munoz et al.
[MODELS 2024] monitor framework.