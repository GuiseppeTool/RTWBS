# Prompt CS7 — Autopilot FSM (Lockheed Martin CPS Challenge)

## Prerequisites

Prompts 0–CS6 must be fully implemented. This is the largest and most
complex model, the primary RQ4 case study, and includes the full
compositional verification experiment for RQ3.

---

## Background and Domain

Derived from the Lockheed Martin Cyber-Physical Systems (LMCPS) challenge
problems, specifically the Autopilot Finite State Machine benchmark.
The autopilot commands a safety manoeuvre in response to hazardous
flight conditions. State space: STANDBY → NOMINAL → TRANSITION →
MANEUVER, with emergency paths.

Governing standards:
- **DO-178C (2011)** — Software Considerations in Airborne Systems.
  DAL-A (most critical) for the hazard response function:
  - Section 6.3.2: Formal methods acceptable for DAL-A verification
  - Section 6.4.4.2: Modified condition / decision coverage required
- **ARP4754A (2010)** — Aircraft and Systems Development:
  - Section 5.5: Safety Assessment — hazard response classified
    Development Assurance Level A (catastrophic failure condition)
  - Section 7.2: Requirements capture for DAL-A functions
- **DO-254 (2000)** — Design Assurance Guidance for Airborne Electronic
  Hardware. Applied to the FADEC hardware executing the autopilot:
  - Section 6.1: Requirements capture completeness
- **RTCA DO-330 (2011)** — Software Tool Qualification Considerations.
  Tool qualification for the model checker used to verify autopilot SW.
- **FAA Advisory Circular AC 25.1309-1A** — System Design and Analysis:
  - Catastrophic failure condition: probability < 10^-9 per flight hour
  - Hazardous failure condition: probability < 10^-7 per flight hour
  - The safety manoeuvre initiation timing falls under Hazardous class.
- **EUROCAE ED-12C / RTCA DO-178C alignment** — European equivalent,
  applicable for EASA certification alongside FAA.
- **MIL-STD-1797B (2004)** — Flying Qualities of Piloted Aircraft:
  - Level 1 flying qualities must be maintained during manoeuvre execution
  - Maximum allowable pitch/roll rate during safety manoeuvre

The **PT view** models the flight control software from the *avionics
software safety engineer* perspective: hazard detection triggers,
mode transitions, manoeuvre commands, and recovery completions are
the observable events. All timing constraints are hard deadlines.

The **DT view** models a **flight safety monitoring twin** authored by
the *flight test and safety analysis team*. It monitors flight envelope
parameters (pitch, roll, altitude, airspeed) against DO-178C-verified
safety bounds, using the flight performance vocabulary of ARP4754A
rather than the autopilot command vocabulary.

Compositional decomposition for RQ3:
- **Normal Operations Subsystem (NOS):** STANDBY, NOMINAL, TRANSITION states
- **Manoeuvre Execution Subsystem (MES):** MANEUVER_* states
- **Emergency Handling Subsystem (EHS):** HAZARD, EMERGENCY, FAULT, RECOVERY states

---

## STEP 1 — UPPAAL Models

### File: `assets/CS7_Autopilot/V1_PT.xml`

Template name: `AutopilotPT`

**Clocks:** `t_active`, `t_maneuver`, `t_recovery`, `t_hazard`, `t_trans`

**Locations (12 total):**
```
STANDBY           — autopilot armed, not engaged
NOMINAL           — autopilot engaged, normal flight envelope
TRANSITION_UP     — thrust increasing, transitioning to higher power
TRANSITION_DOWN   — thrust decreasing, transitioning to lower power
MANEUVER_CLIMB    — safety climb manoeuvre active
MANEUVER_DESCEND  — safety descent manoeuvre active
MANEUVER_LEFT     — safety left turn manoeuvre active
MANEUVER_RIGHT    — safety right turn manoeuvre active
RECOVERY          — post-manoeuvre recovery to nominal
HAZARD_DETECTED   — hazard flag raised, selecting manoeuvre
EMERGENCY_OVERRIDE— pilot override: immediate manual takeover
SYSTEM_FAULT      — autopilot hardware/software fault detected
```

**Transitions:**

```
STANDBY -> NOMINAL
  trigger: activate!
  guard:   t_active >= 0, reset: t_active := 0

NOMINAL -> TRANSITION_UP
  trigger: thrust_up!
  guard:   t_active >= 5, reset: t_trans := 0

NOMINAL -> TRANSITION_DOWN
  trigger: thrust_down!
  guard:   t_active >= 5, reset: t_trans := 0

TRANSITION_UP -> NOMINAL
  trigger: transition_complete!
  guard:   t_trans >= 3 && t_trans <= 20
  invariant: t_trans <= 20, reset: t_active := 0

TRANSITION_DOWN -> NOMINAL
  trigger: transition_complete!
  guard:   t_trans >= 3 && t_trans <= 20
  invariant: t_trans <= 20, reset: t_active := 0

ANY -> HAZARD_DETECTED
  trigger: hazard_alert!
  guard:   t_hazard <= 2
           [ARP4754A Sec.5.5: Hazardous — response initiation ≤2 units]
  (from NOMINAL, TRANSITION_UP, TRANSITION_DOWN)
  reset:   t_hazard := 0

HAZARD_DETECTED -> MANEUVER_CLIMB
  trigger: maneuver_climb!
  guard:   t_hazard >= 0 && t_hazard <= 2
  invariant on HAZARD_DETECTED: t_hazard <= 2, reset: t_maneuver := 0

HAZARD_DETECTED -> MANEUVER_DESCEND
  trigger: maneuver_descend!
  guard:   t_hazard >= 0 && t_hazard <= 2
  reset:   t_maneuver := 0

HAZARD_DETECTED -> MANEUVER_LEFT
  trigger: maneuver_left!
  guard:   t_hazard >= 0 && t_hazard <= 2
  reset:   t_maneuver := 0

HAZARD_DETECTED -> MANEUVER_RIGHT
  trigger: maneuver_right!
  guard:   t_hazard >= 0 && t_hazard <= 2
  reset:   t_maneuver := 0

MANEUVER_CLIMB -> RECOVERY
  trigger: maneuver_complete!
  guard:   t_maneuver >= 5 && t_maneuver <= 30
           [DO-178C: manoeuvre must complete within 30 time units]
  invariant: t_maneuver <= 30, reset: t_recovery := 0

MANEUVER_DESCEND -> RECOVERY
  trigger: maneuver_complete!
  guard:   t_maneuver >= 5 && t_maneuver <= 30
  invariant: t_maneuver <= 30, reset: t_recovery := 0

MANEUVER_LEFT -> RECOVERY
  trigger: maneuver_complete!
  guard:   t_maneuver >= 5 && t_maneuver <= 30
  invariant: t_maneuver <= 30, reset: t_recovery := 0

MANEUVER_RIGHT -> RECOVERY
  trigger: maneuver_complete!
  guard:   t_maneuver >= 5 && t_maneuver <= 30
  invariant: t_maneuver <= 30, reset: t_recovery := 0

RECOVERY -> NOMINAL
  trigger: recovery_complete!
  guard:   t_recovery >= 10 && t_recovery <= 60
  invariant: t_recovery <= 60, reset: t_active := 0

ANY -> EMERGENCY_OVERRIDE
  trigger: emergency_override!
  guard:   t_hazard <= 1
           [DO-178C DAL-A: override reachable within 1 unit from any state]
  (from any active flight state)

EMERGENCY_OVERRIDE -> STANDBY
  trigger: manual_takeover!
  guard:   (no guard)

ANY -> SYSTEM_FAULT
  trigger: system_fault!
  guard:   t_active >= 200
           [DO-178C: fault declared if active > 200 without nominal confirmation]

SYSTEM_FAULT -> STANDBY
  trigger: fault_reset!
  guard:   (no guard)
```

### File: `assets/CS7_Autopilot/V2_DT.xml`

Template name: `FlightSafetyDT`

**Label mapping intent (flight performance vocabulary):**
```
PT label              DT label
activate!          --> ap_engage_telemetry!
hazard_alert!      --> flight_envelope_breach!
maneuver_climb!    --> safety_climb_initiated!
maneuver_descend!  --> safety_descent_initiated!
maneuver_left!     --> safety_turn_left_initiated!
maneuver_right!    --> safety_turn_right_initiated!
maneuver_complete! --> safety_maneuver_confirmed!
recovery_complete! --> nominal_envelope_restored!
emergency_override!--> pilot_authority_event!
thrust_up!         --> power_increase_event!
thrust_down!       --> power_decrease_event!
transition_complete!-> power_mode_stable!
system_fault!      --> ap_fault_telemetry!
fault_reset!       --> ap_reset_telemetry!
```

**Clocks:** `t_ap`, `t_safety`, `t_env`, `t_rec`

**Locations (10 total):**
```
FSD_DISENGAGED, FSD_NOMINAL, FSD_POWER_TRANSITION,
FSD_SAFETY_MANEUVER, FSD_RECOVERY, FSD_ENVELOPE_BREACH,
FSD_PILOT_AUTHORITY, FSD_FAULT, FSD_CLIMB, FSD_DESCEND
```

Timing: DT bounds = PT bounds + 2 units telemetry latency.

---

### Compositional Subsystem Files

**`V1_PT_NOS.xml` / `V2_DT_NOS.xml`:** Normal Operations Subsystem —
STANDBY, NOMINAL, TRANSITION_UP, TRANSITION_DOWN. Events: activate!,
thrust_up!, thrust_down!, transition_complete!. Clocks: t_active, t_trans.

**`V1_PT_MES.xml` / `V2_DT_MES.xml`:** Manoeuvre Execution Subsystem —
HAZARD_DETECTED, MANEUVER_CLIMB, MANEUVER_DESCEND, MANEUVER_LEFT,
MANEUVER_RIGHT. Events: hazard_alert!, maneuver_*!, maneuver_complete!.
Clocks: t_hazard, t_maneuver.

**`V1_PT_EHS.xml` / `V2_DT_EHS.xml`:** Emergency Handling Subsystem —
RECOVERY, EMERGENCY_OVERRIDE, SYSTEM_FAULT. Events: recovery_complete!,
emergency_override!, manual_takeover!, system_fault!, fault_reset!.
Clocks: t_recovery, t_active.

Interpretation subsets for each subsystem follow the same pattern as CS4.

---

## STEP 2 — Ontology

### File: `assets/CS7_Autopilot/domain.ont`

```
; Ontology: Autopilot Flight Safety System
; Standards: DO-178C:2011, ARP4754A:2010, DO-254:2000,
;            FAA AC 25.1309-1A, MIL-STD-1797B:2004
; Fragment: QF_LRA

; === SORTS ===
sort Angle      ; degrees — pitch, roll, yaw angles
sort Altitude   ; feet — pressure altitude
sort Speed      ; knots — airspeed
sort AngularRate; deg/s — pitch/roll/yaw rates
sort LoadFactor ; g — normal acceleration
sort DALLevel   ; integer 1-5 — Development Assurance Level (1=A, 5=E)

; === FUNCTIONS ===
fun pitch_angle       : Angle       ; current pitch (positive = nose up)
fun roll_angle        : Angle       ; current roll (positive = right wing down)
fun yaw_angle         : Angle       ; current yaw from heading
fun altitude          : Altitude    ; current pressure altitude (ft)
fun airspeed          : Speed       ; current indicated airspeed (kt)
fun pitch_rate        : AngularRate ; pitch rate (deg/s)
fun roll_rate         : AngularRate ; roll rate (deg/s)
fun load_factor       : LoadFactor  ; current normal acceleration (g)

; Flight envelope limits (DO-178C verified — FAA AC 25.1309)
fun max_pitch_up      : Angle       ; maximum nose-up pitch: +30 degrees
fun max_pitch_down    : Angle       ; maximum nose-down pitch: -20 degrees
fun max_roll          : Angle       ; maximum bank angle: ±45 degrees
fun min_airspeed      : Speed       ; stall speed + margin: 120 kt (clean)
fun max_airspeed      : Speed       ; VMO (max operating): 340 kt
fun min_altitude      : Altitude    ; minimum safe altitude: 1000 ft AGL
fun max_pitch_rate    : AngularRate ; MIL-STD-1797B Level 1: 3 deg/s
fun max_roll_rate     : AngularRate ; MIL-STD-1797B Level 1: 7 deg/s
fun max_load_factor   : LoadFactor  ; structural limit: +2.5g / -1.0g

; Safety manoeuvre parameters
fun maneuver_altitude_gain : Altitude ; minimum altitude gained in climb manoeuvre
fun maneuver_turn_angle    : Angle    ; heading change in turn manoeuvre
fun hazard_response_time   : Angle    ; reusing sort — max response (2 time units)

; Health parameters
fun ap_health_index   : LoadFactor  ; reusing sort — autopilot health 0-1
fun system_dal        : DALLevel    ; current system DAL requirement

; === RELATIONS ===
rel in_flight_envelope  :  ; all params within certified bounds
rel maneuver_complete_rel: ; maneuver_altitude_gain achieved or turn complete
rel hazard_active       :  ; hazard condition persists

; === AXIOMS ===
axiom max_pitch_up_val  : max_pitch_up = 30
axiom max_pitch_down_val: max_pitch_down = 20
axiom max_roll_val      : max_roll = 45
axiom min_airspeed_val  : min_airspeed = 120
axiom max_airspeed_val  : max_airspeed = 340
axiom min_alt_val       : min_altitude = 1000
axiom max_pitch_rate_val: max_pitch_rate = 3
axiom max_roll_rate_val : max_roll_rate = 7
axiom max_load_val      : max_load_factor = 2
axiom min_load_val      : load_factor >= -1
axiom maneuver_alt_val  : maneuver_altitude_gain = 500
axiom maneuver_turn_val : maneuver_turn_angle = 30
axiom hazard_resp_val   : hazard_response_time = 2
axiom nn_altitude       : altitude >= 0
axiom nn_airspeed       : airspeed >= 0
axiom pitch_lower       : pitch_angle >= -90
axiom pitch_upper       : pitch_angle <= 90
axiom roll_lower        : roll_angle >= -180
axiom roll_upper        : roll_angle <= 180
axiom airspeed_upper    : airspeed <= max_airspeed + 20
axiom load_upper        : load_factor <= max_load_factor + 0
axiom health_lower      : ap_health_index >= 0
axiom health_upper      : ap_health_index <= 1
```

---

## STEP 3 — Interpretations

### File: `assets/CS7_Autopilot/pt.interp`

```
; Avionics software safety engineer perspective
STANDBY           : (and (>= ap_health_index 0) (= pitch_rate 0))
NOMINAL           : (and (>= airspeed min_airspeed) (<= pitch_angle max_pitch_up) (<= roll_angle max_roll) (<= load_factor max_load_factor))
TRANSITION_UP     : (and (>= airspeed min_airspeed) (>= pitch_rate 0))
TRANSITION_DOWN   : (and (>= airspeed min_airspeed) (<= pitch_rate 0))
MANEUVER_CLIMB    : (and (>= airspeed min_airspeed) (> pitch_angle 0) (<= pitch_rate max_pitch_rate))
MANEUVER_DESCEND  : (and (>= airspeed min_airspeed) (< pitch_angle 0) (<= pitch_rate max_pitch_rate))
MANEUVER_LEFT     : (and (>= airspeed min_airspeed) (<= roll_angle 0) (<= roll_rate max_roll_rate))
MANEUVER_RIGHT    : (and (>= airspeed min_airspeed) (>= roll_angle 0) (<= roll_rate max_roll_rate))
RECOVERY          : (and (>= airspeed min_airspeed) (<= pitch_angle max_pitch_up) (<= roll_angle max_roll))
HAZARD_DETECTED   : (and (>= airspeed min_airspeed) (>= altitude min_altitude))
EMERGENCY_OVERRIDE: (>= ap_health_index 0)
SYSTEM_FAULT      : (< ap_health_index 0)

activate!              : (>= ap_health_index 0)
hazard_alert!          : (and (>= altitude min_altitude) (>= airspeed min_airspeed))
maneuver_climb!        : (and (>= altitude min_altitude) (>= airspeed min_airspeed))
maneuver_descend!      : (and (>= altitude min_altitude + 500) (>= airspeed min_airspeed))
maneuver_left!         : (and (>= airspeed min_airspeed) (<= roll_angle max_roll))
maneuver_right!        : (and (>= airspeed min_airspeed) (<= roll_angle max_roll))
maneuver_complete!     : (and (<= pitch_angle max_pitch_up) (<= roll_angle max_roll) (<= load_factor max_load_factor))
recovery_complete!     : (and (<= pitch_angle 5) (<= roll_angle 10) (>= airspeed min_airspeed))
emergency_override!    : (>= ap_health_index 0)
thrust_up!             : (and (>= airspeed min_airspeed) (<= airspeed max_airspeed))
thrust_down!           : (>= airspeed min_airspeed)
transition_complete!   : (and (>= airspeed min_airspeed) (<= load_factor max_load_factor))
system_fault!          : (< ap_health_index 0)
fault_reset!           : (>= ap_health_index 0)
```

### File: `assets/CS7_Autopilot/dt.interp`

```
; Flight test and safety analysis team perspective
FSD_DISENGAGED     : (and (>= ap_health_index 0) (= pitch_rate 0))
FSD_NOMINAL        : (and (>= airspeed min_airspeed) (<= pitch_angle max_pitch_up) (<= roll_angle max_roll) (<= load_factor max_load_factor))
FSD_POWER_TRANSITION: (and (>= airspeed min_airspeed) (<= load_factor max_load_factor))
FSD_SAFETY_MANEUVER: (and (>= airspeed min_airspeed) (<= load_factor max_load_factor))
FSD_RECOVERY       : (and (>= airspeed min_airspeed) (<= pitch_angle max_pitch_up) (<= roll_angle max_roll))
FSD_ENVELOPE_BREACH: (and (>= airspeed min_airspeed) (>= altitude min_altitude))
FSD_PILOT_AUTHORITY: (>= ap_health_index 0)
FSD_FAULT          : (< ap_health_index 0)
FSD_CLIMB          : (and (>= altitude min_altitude) (>= airspeed min_airspeed) (> pitch_angle 0))
FSD_DESCEND        : (and (>= altitude min_altitude + 500) (>= airspeed min_airspeed) (< pitch_angle 0))

ap_engage_telemetry!       : (>= ap_health_index 0)
flight_envelope_breach!    : (and (>= altitude min_altitude) (>= airspeed min_airspeed))
safety_climb_initiated!    : (and (>= altitude min_altitude) (>= airspeed min_airspeed))
safety_descent_initiated!  : (and (>= altitude min_altitude + 500) (>= airspeed min_airspeed))
safety_turn_left_initiated!: (and (>= airspeed min_airspeed) (<= roll_angle max_roll))
safety_turn_right_initiated!:(and (>= airspeed min_airspeed) (<= roll_angle max_roll))
safety_maneuver_confirmed! : (and (<= pitch_angle max_pitch_up) (<= roll_angle max_roll) (<= load_factor max_load_factor))
nominal_envelope_restored! : (and (<= pitch_angle 5) (<= roll_angle 10) (>= airspeed min_airspeed))
pilot_authority_event!     : (>= ap_health_index 0)
power_increase_event!      : (and (>= airspeed min_airspeed) (<= airspeed max_airspeed))
power_decrease_event!      : (>= airspeed min_airspeed)
power_mode_stable!         : (and (>= airspeed min_airspeed) (<= load_factor max_load_factor))
ap_fault_telemetry!        : (< ap_health_index 0)
ap_reset_telemetry!        : (>= ap_health_index 0)
```

---

## STEP 4 — Misalignment Variants

### Variant A — Threshold Drift: Flight Envelope Safety Gap
`dt_thresh_drift.interp`: Change `flight_envelope_breach!` to fire at
`pitch_angle > 35` instead of `> max_pitch_up (= 30)`. The 5° gap is
operationally critical: this is the envelope band where stall risk
increases exponentially (ARP4754A FHA consequence: Hazardous).

### Variant B — Timing Violation: Hazard Response Delay
`V2_DT_timing_violation.xml`: Change `flight_envelope_breach!` upper bound
to `t_ap <= 5` instead of `<= 4` (2 + 2 latency). The extra 1 time unit
violates the ARP4754A 2-unit hazard response requirement.

### Variant C — Missing Event: Emergency Override Gap
`dt_missing_override.interp`: Remove `pilot_authority_event!`. The DT
has no semantic mapping for the most critical override event.

### Variant D — Ontology Evolution (RQ1)
`domain_v2.ont`: Add `sort Probability` (per flight hour), function
`failure_prob_per_hour` with axiom `failure_prob_per_hour <= 1`.
Tighten `max_pitch_up` from 30 to 27 degrees (updated ARP4754A
re-assessment after fleet data review). Update interpretations.

---

## STEP 5 — Benchmark Runner

### File: `benchmark/run_CS7.cpp`

Follow `benchmark/run_CS1.cpp` structure. Include:
1. Five standard runs (aligned, syntactic, drift misalignment, runtime monitor, evolution)
2. Compositional RQ3 experiment (same pattern as CS4 — monolithic vs. NOS+MES+EHS)
3. Full RQ4 engineering cost report:
   - Enumerate all properties covered by the alignment relation (|dom(λ)|)
   - For each, estimate monitor LOC and traces needed
   - Output `results/CS7_cost_report.csv` with columns:
     `property, covered_by_alignment, monitor_loc_estimate, traces_to_verify, semalign_time_ms`