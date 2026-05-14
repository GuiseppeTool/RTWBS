# Prompt CS8 — Lift Plus Cruise eVTOL (NASA/FRET + LMCPS)

## Prerequisites

Prompts 0–CS7 must be fully implemented. Study all completed case studies.

---

## Background and Domain

The Lift Plus Cruise (LPC) vehicle is an electric Vertical Take-Off and
Landing (eVTOL) aircraft with separate lift rotors and a pusher propeller
for cruise. It is derived from the NASA/FRET + LMCPS challenge problems
and models the flight mode transition safety requirements. The system is
timely: eVTOL is the fastest-growing sector in aviation certification
(Joby Aviation, Archer, Wisk all certificating under FAA AC 21-7A).

Governing standards:
- **ASTM F3269-21** — Standard Practice for Methods to Safely Bound
  Flight Behavior of Unmanned Aircraft Systems. The primary well-clear
  and separation assurance standard. Key provisions:
  - Section 6.3: Modified tau well-clear criterion (τ_mod ≥ 35 seconds)
  - Section 7.2: Collision avoidance manoeuvre initiation bounds
- **FAA AC 21-7A (2022)** — Airworthiness Certification of eVTOL Aircraft.
  The specific eVTOL certification basis:
  - Powered-lift category: hybrid lift/cruise requires dedicated mode
    transition verification
  - Battery state of charge minimum for safe landing: ≥10% SOC
- **RTCA DO-365 (2020)** — Minimum Operational Performance Standards
  for Detect and Avoid Systems (UAS):
  - Section 3.4.2: Transition phase timing ≤25 seconds (modelled as ≤25 units)
  - Self-separation alerting: manoeuvre initiation within 35 units of
    well-clear violation
- **SAE AS6968 (2021)** — EVTOL Powertrain Requirements:
  - Motor current limits for lift and cruise motors
  - Battery derating below 20% SOC
  - Thermal management during hover-to-cruise transitions
- **EUROCAE ED-269 (2020)** — MOPS for UAS Detect and Avoid.
  European equivalent of DO-365, used for EASA certification.
- **IEC 61508-3:2010** — Functional Safety of Electrical/Electronic/
  Programmable Electronic Safety-related Systems: SIL-3 for the
  mode transition controller (catastrophic failure = loss of aircraft).

The **PT view** models the flight control computer from the *eVTOL
avionics engineer* perspective: mode transition commands, rotor
engagement/disengagement events, and fault/well-clear alerts are
the observable events.

The **DT view** models a **flight envelope and separation monitoring twin**
authored by the *airspace integration team*. It tracks well-clear
metrics (τ_mod, horizontal/vertical separation) and battery state
of charge rather than individual rotor/motor commands.

---

## STEP 1 — UPPAAL Models

### File: `assets/CS8_eVTOL/V1_PT.xml`

Template name: `LiftCruisePT`

**Clocks:** `t_climb`, `t_trans`, `t_cruise`, `t_battery`, `t_fault`

**Locations (10 total):**
```
GROUND            — vehicle grounded, all rotors stopped
VERTICAL_CLIMB    — lift rotors active, ascending vertically
TRANSITION_TO_CRUISE — lift rotors active + pusher spooling up
CRUISE            — pusher active, lift rotors slowed/stopped
TRANSITION_TO_HOVER — pusher decelerating, lift rotors spooling up
HOVER             — sustained hover at altitude (lift rotors only)
VERTICAL_DESCENT  — controlled descent on lift rotors
LANDING           — final approach and touchdown
BATTERY_CRITICAL  — SOC below safe landing reserve: forced descent
ROTOR_FAULT       — lift or cruise motor fault: emergency landing
```

**Transitions:**

```
GROUND -> VERTICAL_CLIMB
  trigger: takeoff_initiated!
  guard:   t_battery <= 180       [battery not depleted at takeoff]
  reset:   t_climb := 0, t_battery := 0

VERTICAL_CLIMB -> TRANSITION_TO_CRUISE
  trigger: transition_cruise_start!
  guard:   t_climb >= 15 && t_climb <= 60
           [FAA AC 21-7A: minimum climb altitude before transition]
  invariant: t_climb <= 60, reset: t_trans := 0

TRANSITION_TO_CRUISE -> CRUISE
  trigger: cruise_established!
  guard:   t_trans >= 10 && t_trans <= 25
           [DO-365 Sec.3.4.2: transition ≤25 time units]
  invariant: t_trans <= 25, reset: t_cruise := 0

CRUISE -> TRANSITION_TO_HOVER
  trigger: transition_hover_start!
  guard:   t_cruise >= 30, reset: t_trans := 0

TRANSITION_TO_HOVER -> HOVER
  trigger: hover_stable!
  guard:   t_trans >= 10 && t_trans <= 25
  invariant: t_trans <= 25, reset: t_climb := 0

HOVER -> VERTICAL_DESCENT
  trigger: descent_initiated!
  guard:   t_climb >= 5, reset: t_climb := 0

VERTICAL_DESCENT -> LANDING
  trigger: landing_initiated!
  guard:   t_climb >= 5 && t_climb <= 60
  invariant: t_climb <= 60, reset: t_climb := 0

LANDING -> GROUND
  trigger: touchdown!
  guard:   t_climb >= 3

ANY -> BATTERY_CRITICAL
  trigger: battery_critical!
  guard:   t_battery >= 180
           [SAE AS6968: forced descent when t_battery >= 180]
  (from CRUISE, HOVER, TRANSITION_TO_CRUISE)
  reset:   t_fault := 0

BATTERY_CRITICAL -> VERTICAL_DESCENT
  trigger: emergency_descent!
  guard:   t_fault <= 5
           [FAA AC 21-7A: must initiate emergency descent within 5 units]
  invariant: t_fault <= 5

ANY -> ROTOR_FAULT
  trigger: rotor_fault!
  guard:   t_fault <= 2
           [IEC 61508-3 SIL-3: fault response ≤2 units]
  (from VERTICAL_CLIMB, TRANSITION_TO_CRUISE, HOVER)
  reset:   t_fault := 0

ROTOR_FAULT -> LANDING
  trigger: emergency_landing!
  guard:   t_fault <= 5
  invariant: t_fault <= 5
```

### File: `assets/CS8_eVTOL/V2_DT.xml`

Template name: `FlightEnvelopeDT`

**Label mapping intent (airspace integration / well-clear vocabulary):**
```
PT label                   DT label
takeoff_initiated!      --> vehicle_departure_event!
transition_cruise_start!--> propulsion_mode_change!
cruise_established!     --> cruise_mode_confirmed!
transition_hover_start! --> hover_mode_initiation!
hover_stable!           --> hover_confirmed_telemetry!
descent_initiated!      --> descent_sequence_start!
landing_initiated!      --> final_approach_event!
touchdown!              --> ground_contact_event!
battery_critical!       --> low_energy_alert!
emergency_descent!      --> forced_descent_command!
rotor_fault!            --> propulsion_fault_event!
emergency_landing!      --> emergency_landing_command!
```

**Clocks:** `t_dt_trans`, `t_dt_cruise`, `t_soc`, `t_wc`

**Locations (9 total):**
```
DT_GROUND, DT_VERTICAL_OPS, DT_TRANSITION_CRUISE,
DT_CRUISE_OPS, DT_TRANSITION_HOVER, DT_HOVER_OPS,
DT_DESCENT_OPS, DT_LOW_ENERGY, DT_FAULT_OPS
```

Timing: DT bounds = PT bounds + 3 units (DO-365 telemetry latency budget).

---

## STEP 2 — Ontology

### File: `assets/CS8_eVTOL/domain.ont`

```
; Ontology: Lift Plus Cruise eVTOL Vehicle
; Standards: ASTM F3269-21, FAA AC 21-7A, RTCA DO-365:2020,
;            SAE AS6968:2021, EUROCAE ED-269:2020, IEC 61508-3:2010
; Fragment: QF_LRA

; === SORTS ===
sort Altitude     ; feet — pressure altitude
sort Speed        ; knots — airspeed
sort Distance     ; feet — horizontal separation
sort Energy       ; percent — battery state of charge (SOC)
sort Current      ; Amperes — motor current draw
sort TauMod       ; seconds — ASTM F3269 modified tau metric

; === FUNCTIONS ===
fun altitude          : Altitude   ; current altitude (ft AGL)
fun airspeed          : Speed      ; indicated airspeed (kt)
fun vertical_speed    : Speed      ; rate of climb/descent (ft/min ÷ 100, scaled)
fun horizontal_sep    : Distance   ; horizontal distance to nearest traffic (ft)
fun vertical_sep      : Distance   ; vertical separation to nearest traffic (ft)
fun tau_mod           : TauMod     ; ASTM F3269 modified tau (seconds)
fun well_clear_h      : Distance   ; ASTM F3269-21 horizontal well-clear threshold (ft)
fun well_clear_v      : Distance   ; ASTM F3269-21 vertical well-clear threshold (ft)
fun tau_mod_threshold : TauMod     ; ASTM F3269-21 Section 6.3: 35 seconds
fun battery_soc       : Energy     ; current battery state of charge (%)
fun min_soc_landing   : Energy     ; FAA AC 21-7A: minimum SOC for safe landing (%)
fun min_soc_operation : Energy     ; SAE AS6968: minimum SOC for powered ops (%)
fun lift_motor_current: Current    ; aggregate lift rotor current (A)
fun cruise_motor_current: Current  ; pusher motor current (A)
fun max_motor_current : Current    ; SAE AS6968 rated current limit (A)
fun transition_time   : TauMod     ; actual transition duration (time units)
fun max_transition_time: TauMod    ; DO-365 Sec.3.4.2 limit: 25 time units

; === RELATIONS ===
rel well_clear          :  ; horizontal_sep > well_clear_h AND tau_mod >= tau_mod_threshold
rel battery_safe        :  ; battery_soc >= min_soc_operation
rel transition_complete_rel: ; transition_time <= max_transition_time
rel motor_current_safe  :  ; lift_motor_current <= max_motor_current

; === AXIOMS ===
axiom wc_h_val          : well_clear_h = 4000
axiom wc_v_val          : well_clear_v = 450
axiom tau_mod_thresh    : tau_mod_threshold = 35
axiom min_soc_land      : min_soc_landing = 10
axiom min_soc_ops       : min_soc_operation = 20
axiom max_current_val   : max_motor_current = 200
axiom max_trans_time    : max_transition_time = 25
axiom nn_altitude       : altitude >= 0
axiom nn_airspeed       : airspeed >= 0
axiom nn_h_sep          : horizontal_sep >= 0
axiom nn_v_sep          : vertical_sep >= 0
axiom nn_tau            : tau_mod >= 0
axiom soc_lower         : battery_soc >= 0
axiom soc_upper         : battery_soc <= 100
axiom nn_current_lift   : lift_motor_current >= 0
axiom nn_current_cruise : cruise_motor_current >= 0
axiom current_lift_bound: lift_motor_current <= max_motor_current + 20
axiom soc_ops_ordering  : min_soc_operation > min_soc_landing
axiom soc_ops_val       : min_soc_operation = 20
```

---

## STEP 3 — Interpretations

### File: `assets/CS8_eVTOL/pt.interp`

```
; eVTOL avionics engineer perspective
GROUND              : (and (= airspeed 0) (>= battery_soc min_soc_landing))
VERTICAL_CLIMB      : (and (> vertical_speed 0) (>= battery_soc min_soc_operation) (<= lift_motor_current max_motor_current))
TRANSITION_TO_CRUISE: (and (> airspeed 0) (>= battery_soc min_soc_operation) (<= lift_motor_current max_motor_current))
CRUISE              : (and (>= airspeed 50) (>= battery_soc min_soc_operation) (<= cruise_motor_current max_motor_current))
TRANSITION_TO_HOVER : (and (> airspeed 0) (>= battery_soc min_soc_operation))
HOVER               : (and (= airspeed 0) (>= battery_soc min_soc_operation) (<= lift_motor_current max_motor_current))
VERTICAL_DESCENT    : (and (< vertical_speed 0) (>= battery_soc min_soc_landing))
LANDING             : (and (< vertical_speed 0) (>= battery_soc min_soc_landing) (<= altitude 500))
BATTERY_CRITICAL    : (< battery_soc min_soc_operation)
ROTOR_FAULT         : (> lift_motor_current max_motor_current)

takeoff_initiated!        : (and (= altitude 0) (>= battery_soc min_soc_operation))
transition_cruise_start!  : (and (>= altitude 500) (>= battery_soc min_soc_operation))
cruise_established!       : (and (>= airspeed 50) (<= transition_time max_transition_time) (>= battery_soc min_soc_operation))
transition_hover_start!   : (and (>= battery_soc min_soc_operation) (>= altitude 500))
hover_stable!             : (and (<= airspeed 5) (<= transition_time max_transition_time) (>= battery_soc min_soc_operation))
descent_initiated!        : (>= battery_soc min_soc_landing)
landing_initiated!        : (and (<= altitude 500) (>= battery_soc min_soc_landing))
touchdown!                : (= altitude 0)
battery_critical!         : (< battery_soc min_soc_operation)
emergency_descent!        : (< battery_soc min_soc_operation)
rotor_fault!              : (> lift_motor_current max_motor_current)
emergency_landing!        : (> lift_motor_current max_motor_current)
```

### File: `assets/CS8_eVTOL/dt.interp`

```
; Airspace integration / well-clear monitoring perspective
DT_GROUND           : (and (= airspeed 0) (>= battery_soc min_soc_landing))
DT_VERTICAL_OPS     : (and (> vertical_speed 0) (>= battery_soc min_soc_operation) (<= lift_motor_current max_motor_current))
DT_TRANSITION_CRUISE: (and (> airspeed 0) (>= battery_soc min_soc_operation))
DT_CRUISE_OPS       : (and (>= airspeed 50) (>= battery_soc min_soc_operation) (<= cruise_motor_current max_motor_current))
DT_TRANSITION_HOVER : (and (> airspeed 0) (>= battery_soc min_soc_operation))
DT_HOVER_OPS        : (and (= airspeed 0) (>= battery_soc min_soc_operation) (<= lift_motor_current max_motor_current))
DT_DESCENT_OPS      : (and (< vertical_speed 0) (>= battery_soc min_soc_landing))
DT_LOW_ENERGY       : (< battery_soc min_soc_operation)
DT_FAULT_OPS        : (> lift_motor_current max_motor_current)

vehicle_departure_event!   : (and (= altitude 0) (>= battery_soc min_soc_operation))
propulsion_mode_change!    : (and (>= altitude 500) (>= battery_soc min_soc_operation))
cruise_mode_confirmed!     : (and (>= airspeed 50) (<= transition_time max_transition_time) (>= battery_soc min_soc_operation))
hover_mode_initiation!     : (and (>= battery_soc min_soc_operation) (>= altitude 500))
hover_confirmed_telemetry! : (and (<= airspeed 5) (<= transition_time max_transition_time) (>= battery_soc min_soc_operation))
descent_sequence_start!    : (>= battery_soc min_soc_landing)
final_approach_event!      : (and (<= altitude 500) (>= battery_soc min_soc_landing))
ground_contact_event!      : (= altitude 0)
low_energy_alert!          : (< battery_soc min_soc_operation)
forced_descent_command!    : (< battery_soc min_soc_operation)
propulsion_fault_event!    : (> lift_motor_current max_motor_current)
emergency_landing_command! : (> lift_motor_current max_motor_current)
```

---

## STEP 4 — Misalignment Variants

### Variant A — Ontology Gap: Well-Clear Misclassification
`dt_wc_gap.interp`: Change `propulsion_mode_change!` to additionally
require `tau_mod >= tau_mod_threshold (= 35)`. But the PT's
`transition_cruise_start!` has no such requirement — the DT introduces
a well-clear precondition the PT does not enforce. This is an **ontology
gap**: the DT adds a new precondition not in the shared domain, breaking
alignment in the DT-to-PT direction (Condition III).

### Variant B — Threshold Drift: SOC Safety Margin
`dt_thresh_drift.interp`: Change `low_energy_alert!` to fire at
`battery_soc < 25` instead of `< min_soc_operation (= 20)`. The DT
raises an unnecessary alert for 5% SOC headroom — but more critically,
causes the DT to report an emergency when the PT's BATTERY_CRITICAL
state has not yet been reached, breaking state consistency (Condition I).

### Variant C — Ontology Evolution (RQ1)
`domain_v2.ont`: Add `sort NOx` (g/kWh — emissions per SAE ARP5765A
eVTOL environmental standard). Add `fun nox_emission_rate : NOx`,
`axiom max_nox : nox_emission_rate <= 10`. Tighten `min_soc_operation`
from 20 to 22% (SAE AS6968 revised recommendation for battery cycling).

---

## STEP 5 — Benchmark Runner

### File: `benchmark/run_CS8.cpp`

Follow the structure of `benchmark/run_CS1.cpp`.

---

---
