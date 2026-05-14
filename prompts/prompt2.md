# Prompt CS2 — Crane/Elevator System (Kamburjan et al.)

## Prerequisites

Prompts 0 and CS1 must be fully implemented and passing before starting.
Read `doc/` (the paper), Definition 8 specifically.
Study `assets/CS1_ThreeTank/` thoroughly — this prompt follows the exact
same file structure and should be consistent in style and depth.

---

## Background and Domain

The Crane/Elevator system is the running example in Kamburjan et al.
[ISoLA 2022, "Twinning-by-Construction: Ensuring Correctness for
Self-Adaptive Digital Twins"]. Their original models are written in ABS
(an active object language). This prompt implements equivalent TA models,
which is a documented methodological translation step: ABS object
lifecycles map to TA locations, ABS method calls map to observable events
or channel synchronisations.

The system models an **overhead travelling crane** used in a manufacturing
facility. The crane hoists loads between three floor levels using a hoist
motor and travels horizontally along a beam. Safety is governed by:

- **IEC 60204-1:2016** — Safety of Machinery: Electrical Equipment of
  Machines — General Requirements. The primary electrical safety standard
  for industrial machinery. Key clauses used:
  - Clause 9.2: Protection against electric shock and short circuit
  - Clause 9.3: Overcurrent protection — rated current limits for motors
  - Clause 10.6: Emergency stop functions — Category 0 (immediate
    de-energisation) and Category 1 (controlled stop then de-energise)
  - Clause 12.4: Motor overload protection response times
- **EN 13001-1:2015** — Crane Safety: General Design Principles.
  Key provisions:
  - Load classification classes S0–S9 (duty cycle spectrum)
  - Dynamic load factors for hoisting (phi_2 = 1.1 to 1.6)
  - Maximum design load = rated load × dynamic factor
- **EN 15011:2011+A1:2014** — Cranes: Bridge and Gantry Cranes.
  - Maximum hoist speed: 0.5 m/s under load, 1.0 m/s unloaded
  - End-of-travel limit switches: must actuate within 0.1s of overshoot
  - Hoist cycle time limits: maximum 120 consecutive cycles before
    mandatory thermal rest period
- **ISO 4301-1:1986** — Cranes: Classification. Duty cycle class M4
  (medium duty, up to 25,000 load cycles in service life).

The **PT view** models the physical crane from the perspective of the
*electrical safety engineer*: motor start/stop commands, brake actuation,
limit switch events, and overload alarms are the observable events.

The **DT view** models a **load-cycle monitoring twin** authored by a
*structural integrity engineer* who tracks cumulative load cycles against
the ISO 4301 duty class budget, and monitors hoist force against the EN
13001 rated capacity. The two views use entirely different label
vocabularies but are semantically equivalent under the shared IEC
60204/EN 13001 ontology.

The key semantic alignment challenge: the PT fires `hoist_up!` when the
motor is energised and upward motion begins; the DT fires `hoist_command!`
when the load monitoring twin registers that a new hoist cycle has started
and the load is within rated capacity. These are semantically identical
events under the crane domain ontology — both require `current_load <=
max_load_capacity` and `hoist_cycles_total < duty_cycle_limit` — but
syntactically different labels. Syntactic bisimulation fails; semantic
alignment succeeds.

---

## STEP 1 — UPPAAL Models

### File: `assets/CS2_Crane/V1_PT.xml`

Template name: `CranePT`

**Clocks:** `t_hoist`, `t_travel`, `t_brake`, `t_thermal`, `t_test`

**Locations (9 total):**
```
PARKED          — crane at rest, all drives de-energised, brake applied
HOIST_UP        — hoist motor energised, load ascending
HOIST_DOWN      — hoist motor energised, load descending
AT_FLOOR        — hoist stopped at target floor, load secured
TRAVELLING      — bridge motor active, crane moving horizontally
OVERLOAD        — load exceeds rated capacity: IEC 60204 Cl.12.4 trips motor
EMERGENCY_STOP  — Category 0 stop (IEC 60204 Cl.10.6): immediate de-energise
THERMAL_REST    — mandatory rest after 120 hoist cycles (EN 15011)
MAINTENANCE     — proof test / inspection mode
```

**Transitions and timing constraints:**

```
PARKED -> HOIST_UP
  trigger: hoist_up!
  guard:   t_thermal <= 200         [thermal limit not exceeded]
  reset:   t_hoist := 0

HOIST_UP -> AT_FLOOR
  trigger: hoist_complete!
  guard:   t_hoist >= 5 && t_hoist <= 120
           [EN 15011: minimum 5 units (motion settling), maximum 120 units
            (thermal protection per IEC 60204 Cl.12.4)]
  invariant on HOIST_UP: t_hoist <= 120
  reset:   t_hoist := 0

PARKED -> HOIST_DOWN
  trigger: hoist_down!
  guard:   t_thermal <= 200
  reset:   t_hoist := 0

HOIST_DOWN -> AT_FLOOR
  trigger: hoist_complete!
  guard:   t_hoist >= 5 && t_hoist <= 120
  invariant on HOIST_DOWN: t_hoist <= 120
  reset:   t_hoist := 0

AT_FLOOR -> TRAVELLING
  trigger: travel_start!
  guard:   t_hoist >= 0
  reset:   t_travel := 0

TRAVELLING -> PARKED
  trigger: travel_complete!
  guard:   t_travel >= 3 && t_travel <= 60
           [EN 15011: bridge travel time bounds]
  invariant on TRAVELLING: t_travel <= 60
  reset:   t_travel := 0

AT_FLOOR -> PARKED
  trigger: load_released!
  guard:   t_hoist >= 0
  reset:   t_brake := 0

ANY -> OVERLOAD
  trigger: overload_detected!
  guard:   t_brake <= 2
           [IEC 60204 Cl.12.4: overload detection within 2 units]
  (reachable from HOIST_UP, HOIST_DOWN, AT_FLOOR)

OVERLOAD -> EMERGENCY_STOP
  trigger: emergency_stop!
  guard:   t_brake <= 1
           [IEC 60204 Cl.10.6: Category 0 stop — immediate, ≤1 unit]
  invariant on OVERLOAD: t_brake <= 2

EMERGENCY_STOP -> PARKED
  trigger: reset_crane!
  guard:   (no guard)

PARKED -> THERMAL_REST
  trigger: thermal_rest_start!
  guard:   t_thermal >= 180
           [EN 15011: rest required after extended duty cycle]
  reset:   t_thermal := 0

THERMAL_REST -> PARKED
  trigger: thermal_rest_complete!
  guard:   t_thermal >= 30 && t_thermal <= 60
           [EN 15011: minimum 30 units rest, test completion within 60]
  invariant on THERMAL_REST: t_thermal <= 60

PARKED -> MAINTENANCE
  trigger: maintenance_start!
  reset:   t_test := 0

MAINTENANCE -> PARKED
  trigger: maintenance_complete!
  guard:   t_test >= 20 && t_test <= 120
  invariant on MAINTENANCE: t_test <= 120
```

### File: `assets/CS2_Crane/V2_DT.xml`

Template name: `CraneDT`

The DT is a **load-cycle monitoring twin** written by a structural integrity
engineer. It tracks cumulative hoist cycles against the ISO 4301 M4 duty
class limit and monitors load magnitude against EN 13001 rated capacity.

**Label mapping intent (to be proven via SMT under ontology):**
```
PT label              DT label
hoist_up!         --> hoist_command!
hoist_down!       --> hoist_command!        [both map to same DT event —
                                              DT only tracks cycle count,
                                              not direction]
hoist_complete!   --> cycle_logged!
travel_start!     --> bridge_motion_start!
travel_complete!  --> bridge_motion_end!
load_released!    --> load_setdown_event!
overload_detected!--> load_exceedance_event!
emergency_stop!   --> protective_stop_command!
reset_crane!      --> (DT-internal tau — not observable to DT)
thermal_rest_start!   --> duty_cycle_rest_start!
thermal_rest_complete!--> duty_cycle_rest_end!
maintenance_start!    --> inspection_start!
maintenance_complete! --> inspection_complete!
```

**Clocks:** `t_cycle`, `t_motion`, `t_rest`, `t_inspect`

**Locations (8 total):**
```
DT_IDLE
DT_CYCLE_ACTIVE
DT_FLOOR_DWELL
DT_BRIDGE_MOTION
DT_LOAD_EXCEEDANCE
DT_PROTECTIVE_STOP
DT_DUTY_REST
DT_INSPECTION
```

Timing constraints follow the same IEC 60204 / EN 15011 domain axioms
but from the DT's monitoring perspective, with up to 3 time unit telemetry
latency tolerance on upper bounds (consistent with the relaxed timing
in weak timed bisimulation).

---

## STEP 2 — Ontology

### File: `assets/CS2_Crane/domain.ont`

```
; Ontology: Overhead Travelling Crane
; Standard references:
;   IEC 60204-1:2016  — Safety of Machinery: Electrical Equipment, Cl.9-12
;   EN 13001-1:2015   — Crane Safety: General Design Principles
;   EN 15011:2011+A1  — Bridge and Gantry Cranes: design and testing
;   ISO 4301-1:1986   — Cranes: Classification (duty cycle classes)
;
; Fragment: Quantifier-free Linear Real Arithmetic (QF_LRA)

; === SORTS ===
sort Load          ; kg — mass of lifted object, non-negative real
sort Force         ; kN — mechanical force (hoist tension, brake force)
sort Speed         ; m/s — linear velocity, non-negative real
sort Height        ; m — elevation above ground, non-negative real
sort FloorIndex    ; integer 0, 1, 2 (ground, mezzanine, upper)
sort CycleCount    ; natural number — cumulative hoist cycle counter
sort Temperature   ; Celsius — motor winding temperature
sort Time          ; seconds — timing reference

; === FUNCTIONS ===
; Physical load and capacity
fun current_load      : Load        ; mass currently on hook (kg)
fun rated_load        : Load        ; EN 13001 rated capacity (kg)
fun max_load          : Load        ; rated_load * dynamic_factor (EN 13001 phi_2)
fun dynamic_factor    : Force       ; EN 13001 phi_2 dynamic load factor

; Hoist geometry
fun current_height    : Height      ; current hook height above floor (m)
fun floor_height      : FloorIndex -> Height  ; height of each floor level (m)
fun max_height        : Height      ; upper limit switch position (EN 15011)
fun min_height        : Height      ; lower limit switch position (EN 15011)

; Motion parameters
fun hoist_speed       : Speed       ; current hoist speed (m/s)
fun max_hoist_speed_loaded   : Speed  ; EN 15011: 0.5 m/s under load
fun max_hoist_speed_unloaded : Speed  ; EN 15011: 1.0 m/s unloaded
fun travel_speed      : Speed       ; bridge travel speed (m/s)
fun max_travel_speed  : Speed       ; EN 15011 bridge speed limit

; Duty cycle parameters (ISO 4301)
fun hoist_cycles_total  : CycleCount  ; cumulative hoist cycles since commissioning
fun duty_cycle_limit    : CycleCount  ; ISO 4301 M4 class: 25000 cycles
fun cycles_since_rest   : CycleCount  ; cycles since last thermal rest (EN 15011)
fun thermal_rest_limit  : CycleCount  ; EN 15011: 120 cycles before mandatory rest

; Motor thermal state
fun motor_temperature   : Temperature  ; current winding temperature
fun max_motor_temp      : Temperature  ; IEC 60204 Cl.12.4 thermal protection setpoint
fun ambient_temperature : Temperature  ; facility ambient

; Structural parameters (EN 13001)
fun hoist_force         : Force     ; actual hoist rope tension (kN)
fun brake_force         : Force     ; mechanical brake application force (kN)
fun min_brake_force     : Force     ; IEC 60204 Cl.12.4 minimum required

; === RELATIONS ===
rel overloaded         :            ; holds when current_load > rated_load
rel thermally_limited  :            ; holds when motor_temperature > max_motor_temp
rel at_upper_limit     :            ; holds when current_height >= max_height
rel at_lower_limit     :            ; holds when current_height <= min_height
rel duty_limit_reached :            ; holds when cycles_since_rest >= thermal_rest_limit
rel service_life_exceeded:          ; holds when hoist_cycles_total >= duty_cycle_limit

; === AXIOMS ===

; --- EN 13001 rated capacity and dynamic factor ---
axiom rated_load_val    : rated_load = 500
axiom dynamic_factor_val: dynamic_factor = 1
axiom max_load_def      : max_load = rated_load
; Note: EN 13001 requires max_load = rated_load * phi_2; here phi_2=1 for
; model simplicity while preserving the structural relationship.

; --- EN 15011 speed limits ---
axiom max_hoist_loaded  : max_hoist_speed_loaded = 1
axiom max_hoist_unloaded: max_hoist_speed_unloaded = 2
axiom max_travel_sp     : max_travel_speed = 3

; --- Non-negativity ---
axiom nn_load           : current_load >= 0
axiom nn_height         : current_height >= 0
axiom nn_hoist_speed    : hoist_speed >= 0
axiom nn_travel_speed   : travel_speed >= 0
axiom nn_cycles         : hoist_cycles_total >= 0
axiom nn_cycles_rest    : cycles_since_rest >= 0
axiom nn_temp           : motor_temperature >= 0

; --- Floor height definitions (EN 15011 facility layout) ---
axiom floor0_height     : floor_height(0) = 0
axiom floor1_height     : floor_height(1) = 4
axiom floor2_height     : floor_height(2) = 8

; --- Height limit switches (EN 15011 Cl.6.2) ---
axiom max_h             : max_height = 9
axiom min_h             : min_height = 0
axiom height_upper_bound: current_height <= max_height
axiom height_lower_bound: current_height >= min_height

; --- IEC 60204 Cl.12.4 thermal protection ---
axiom max_temp_val      : max_motor_temp = 120
axiom ambient_val       : ambient_temperature = 20
axiom temp_bound        : motor_temperature >= ambient_temperature
axiom temp_upper        : motor_temperature <= max_motor_temp + 10

; --- ISO 4301 M4 duty cycle limits ---
axiom duty_limit_val    : duty_cycle_limit = 25000
axiom thermal_rest_lim  : thermal_rest_limit = 120
axiom cycles_monotone   : hoist_cycles_total >= cycles_since_rest

; --- IEC 60204 Cl.10.6 brake requirements ---
axiom min_brake_val     : min_brake_force = 10
axiom brake_bound       : brake_force >= 0

; --- Load vs rated capacity (EN 13001) ---
axiom load_capacity_rel : current_load <= max_load + 50
; The +50 kg tolerance models the uncertainty band before overload trips.
; When current_load > rated_load (500), the overloaded relation holds.

; --- Hoist speed constraint under load (EN 15011) ---
axiom speed_loaded_bound: hoist_speed <= max_hoist_speed_loaded + 1
; +1 tolerance for deceleration transient before limit switch trips
```

---

## STEP 3 — Interpretations

### File: `assets/CS2_Crane/pt.interp`

```
; PT Interpretation: CranePT
; Maps PT locations and events to IEC 60204 / EN 13001 domain meaning.

; --- Location interpretations ---
PARKED          : (and (= hoist_speed 0) (= travel_speed 0) (>= brake_force min_brake_force))
HOIST_UP        : (and (> hoist_speed 0) (<= hoist_speed max_hoist_speed_loaded) (<= current_load rated_load))
HOIST_DOWN      : (and (> hoist_speed 0) (<= hoist_speed max_hoist_speed_loaded) (<= current_load rated_load))
AT_FLOOR        : (and (= hoist_speed 0) (<= current_load rated_load))
TRAVELLING      : (and (> travel_speed 0) (<= travel_speed max_travel_speed) (= hoist_speed 0))
OVERLOAD        : (> current_load rated_load)
EMERGENCY_STOP  : (and (= hoist_speed 0) (= travel_speed 0))
THERMAL_REST    : (and (= hoist_speed 0) (>= cycles_since_rest thermal_rest_limit))
MAINTENANCE     : (= hoist_speed 0)

; --- Event interpretations ---
hoist_up!            : (and (<= current_load rated_load) (< current_height max_height) (>= brake_force 0))
hoist_down!          : (and (<= current_load rated_load) (> current_height min_height) (>= brake_force 0))
hoist_complete!      : (and (<= current_load rated_load) (<= hoist_speed max_hoist_speed_loaded))
travel_start!        : (and (= hoist_speed 0) (<= travel_speed max_travel_speed))
travel_complete!     : (and (= hoist_speed 0) (= travel_speed 0))
load_released!       : (= current_load 0)
overload_detected!   : (> current_load rated_load)
emergency_stop!      : (and (> current_load rated_load) (= hoist_speed 0))
reset_crane!         : (and (= hoist_speed 0) (<= current_load rated_load))
thermal_rest_start!  : (>= cycles_since_rest thermal_rest_limit)
thermal_rest_complete! : (= cycles_since_rest 0)
maintenance_start!   : (= hoist_speed 0)
maintenance_complete!: (= hoist_speed 0)
```

### File: `assets/CS2_Crane/dt.interp`

```
; DT Interpretation: CraneDT
; Load-cycle monitoring twin perspective.
; Different vocabulary, semantically equivalent under Delta.

; --- Location interpretations ---
DT_IDLE             : (and (= hoist_speed 0) (= travel_speed 0) (>= brake_force min_brake_force))
DT_CYCLE_ACTIVE     : (and (> hoist_speed 0) (<= current_load rated_load) (<= hoist_speed max_hoist_speed_loaded))
DT_FLOOR_DWELL      : (and (= hoist_speed 0) (<= current_load rated_load))
DT_BRIDGE_MOTION    : (and (> travel_speed 0) (<= travel_speed max_travel_speed) (= hoist_speed 0))
DT_LOAD_EXCEEDANCE  : (> current_load rated_load)
DT_PROTECTIVE_STOP  : (and (= hoist_speed 0) (= travel_speed 0))
DT_DUTY_REST        : (and (= hoist_speed 0) (>= cycles_since_rest thermal_rest_limit))
DT_INSPECTION       : (= hoist_speed 0)

; --- Event interpretations ---
; hoist_command! is semantically equivalent to both hoist_up! and hoist_down!
; under Delta: in all three cases, current_load <= rated_load and the hoist
; is within height bounds. Z3 must confirm:
;   Delta |= I_PT(hoist_up!) <-> I_DT(hoist_command!)
;   Delta |= I_PT(hoist_down!) <-> I_DT(hoist_command!)
hoist_command!           : (and (<= current_load rated_load) (<= hoist_speed max_hoist_speed_loaded) (>= brake_force 0))
cycle_logged!            : (and (<= current_load rated_load) (<= hoist_speed max_hoist_speed_loaded))
bridge_motion_start!     : (and (= hoist_speed 0) (<= travel_speed max_travel_speed))
bridge_motion_end!       : (and (= hoist_speed 0) (= travel_speed 0))
load_setdown_event!      : (= current_load 0)
load_exceedance_event!   : (> current_load rated_load)
protective_stop_command! : (and (> current_load rated_load) (= hoist_speed 0))
duty_cycle_rest_start!   : (>= cycles_since_rest thermal_rest_limit)
duty_cycle_rest_end!     : (= cycles_since_rest 0)
inspection_start!        : (= hoist_speed 0)
inspection_complete!     : (= hoist_speed 0)
```

---

## STEP 4 — Misalignment Variants

### Variant A — Threshold Drift: Safety-Critical Load Gap

### File: `assets/CS2_Crane/V2_DT_thresh_drift.xml`

Copy `V2_DT.xml`. Change the `DT_LOAD_EXCEEDANCE` location trigger so that
`load_exceedance_event!` fires when `current_load > 480` instead of
`> rated_load (= 500)`. This creates a **20 kg gap**: loads between
480–500 kg trigger an alarm in the DT but are within safe operating range,
while loads that *actually* exceed 500 kg may arrive via a path the DT
does not monitor correctly.

### File: `assets/CS2_Crane/dt_thresh_drift.interp`

Change only:
```
load_exceedance_event! : (> current_load 480)
```

This interpretation is **not equivalent** to `overload_detected! : (> current_load rated_load)`
under Delta, because `rated_load = 500` and `480 ≠ 500`. Z3 will return
NOT ENTAILED for this pair, causing the alignment to fail.

### Variant B — Missing Event: Emergency Stop Gap

### File: `assets/CS2_Crane/dt_missing_estop.interp`

Copy `dt.interp` and **remove** the `protective_stop_command!` entry
entirely. This simulates the DT engineer forgetting to provide an
interpretation for the emergency stop event. The semantic alignment
checker must report this as an unmatched DT label, because there is
no formula to compare against `emergency_stop!` in the PT — the
ontological bridge is broken for the most safety-critical event.

No model change required (same `V2_DT.xml`), only the interp file changes.

### Variant C — Ontology Evolution (RQ1)

### File: `assets/CS2_Crane/domain_v2.ont`

Apply Definition 7 refinement:

**Condition I — Vocabulary extension:**
```
sort Vibration     ; mm/s RMS — vibration level (EN 15011 Cl.6.3 condition monitoring)
fun vibration_level    : Vibration   ; current hoist vibration
fun max_vibration      : Vibration   ; EN 15011 alarm threshold
```

**Condition II — Axiom strengthening:**
```
; Original: rated_load = 500
; Refined: rated_load = 475
; Reason: EN 13001 field re-assessment revealed dynamic factor
; phi_2 = 1.05 not 1.0, reducing effective rated load.
axiom rated_load_v2  : rated_load = 475
axiom max_vib        : max_vibration = 10
axiom nn_vib         : vibration_level >= 0
```

**Condition III — Interpretation tightening:**
Update `hoist_command!` and `hoist_up!` interpretations to reference
the tighter `rated_load = 475` threshold in `pt_v2.interp` and `dt_v2.interp`.

Expected: alignment preserved under `domain_v2.ont` with incremental
re-verification (fewer SMT calls than full recheck from scratch).

---

## STEP 5 — Benchmark Runner

### File: `benchmark/run_CS2.cpp`

Follow the exact same structure as `benchmark/run_CS1.cpp`.
Include the same five runs:
1. Semantic alignment — aligned pair (expected: ALIGNED)
2. Syntactic bisimulation baseline (expected: NOT ALIGNED — labels differ)
3. Threshold drift misalignment (expected: NOT ALIGNED)
4. Runtime monitor baseline on threshold drift
5. Ontology evolution check

For the runtime monitor in Run 4, instrument two monitors:
```
Monitor 1: "EN13001_LoadCapacity"
  property: (< current_load rated_load)  [load must not exceed rated]
  label:    load_exceedance_event!
  type:     safety

Monitor 2: "IEC60204_EmergencyReach"
  property: (= hoist_speed 0)            [hoist stopped in emergency]
  label:    protective_stop_command!
  type:     safety
```

For RQ4, additionally report: the structural consequence of Monitor 1
missing the 480–500 kg gap — compute the probability that a random load
drawn from Uniform[0, 550] falls in the gap (20/550 ≈ 3.6%). Report this
as "probability of undetected overload per cycle" in the CSV.