# Prompt CS5 — Autonomous Grasping / Space Debris Removal (NASA/FRET)

## Prerequisites

Prompts 0–CS4 must be fully implemented. Study all completed case studies.
This is the most important case study for RQ2 because the original paper
(Oakes et al., MODELS 2024) already implements runtime monitors generated
from FRET requirements — giving a peer-reviewed baseline comparison.

---

## Background and Domain

This case study is derived from Oakes et al. [MODELS 2024, "Towards
Ontological Service-Driven Engineering of Digital Twins"] and the
NASA/FRET autonomous grasping use case for spent rocket stage capture.
The physical system captures tumbling space debris using a robotic arm
with attitude control. The scenario is drawn from ESA's e.Deorbit and
JAXA's Commercial Removal of Debris from Orbit (CRD2) programmes.

Governing standards:
- **ECSS-E-ST-40C (2009)** — Space Engineering: Software. The primary
  ESA software engineering standard for space systems:
  - Section 5.2.4: Real-time performance and timing requirements
  - Section 5.3.3: Safety-critical function identification and verification
  - Section 6.4: Verification and validation of timing properties
- **ECSS-E-ST-10-06C (2017)** — Technical Requirements Specification:
  - Timing margins for autonomous proximity operations: ±5%
  - Abort condition response time: ≤3 time units
- **ECSS-Q-ST-80C (2017)** — Software Product Assurance:
  - Traceability from safety requirements to verification evidence
- **ISO 9283:1998** — Manipulating Industrial Robots: Performance Criteria.
  Adapted for space manipulators:
  - Positioning accuracy: ±2mm at end-effector (here: alignment_error ≤ 2°)
  - Repeatability requirements for grasp operations
- **CCSDS 727.0-B-5 (2017)** — CCSDS File Delivery Protocol (CFDP).
  Defines the telemetry latency bounds used in DT communication modelling.
  Maximum one-way light time for LEO: 0.067 seconds (modelled as 4 time units).
- **NASA-STD-8719.13C (2013)** — Software Safety Standard:
  - Requirement 4.2.3: Critical command verification timeout ≤ 3 time units
  - Requirement 4.4.1: Abort sequence must be reachable from any active state

The **PT view** models the physical grasping mechanism from the *GNC
(Guidance, Navigation and Control) engineer* perspective: approach
manoeuvres, alignment verification, contact confirmation, grasp
actuation, and abort sequences are the observable events.

The **DT view** models a **mission execution monitoring twin** authored
by the *mission operations team* at the ground station. It receives
CFDP telemetry with bounded latency and tracks mission phases using
percentage-completion metrics and force/torque telemetry streams rather
than discrete GNC state transitions.

The key result for RQ2: the published Oakes et al. runtime monitors
check state-based properties (force threshold at SECURED state, alignment
error at CONTACT state). They **cannot** detect the timing violation
misalignment introduced in Variant A because it requires comparing the
DT event time against the PT clock — a cross-model timing property that
is outside the scope of any state-based runtime monitor.

---

## STEP 1 — UPPAAL Models

### File: `assets/CS5_Grasping/V1_PT.xml`

Template name: `GraspingPT`

**Clocks:** `t_approach`, `t_align`, `t_contact`, `t_grasp`, `t_abort`

**Locations (9 total):**
```
STANDBY         — system armed, awaiting approach initiation
APPROACH        — GNC active, closing distance to target
ALIGNMENT       — rotational alignment manoeuvre with tumbling target
CONTACT         — end-effector in contact, force sensors active
GRASPING        — actuators energised, grasp operation in progress
SECURED         — target captured, grasp force confirmed sufficient
RELEASE         — deliberate release sequence (mission end or abort recovery)
ABORT           — emergency abort: immediate retract
FAULT           — system fault: GNC or actuator failure
```

**Transitions and timing constraints:**

```
STANDBY -> APPROACH
  trigger: approach_start!
  guard:   t_approach >= 0
  reset:   t_approach := 0

APPROACH -> ALIGNMENT
  trigger: approach_complete!
  guard:   t_approach >= 10 && t_approach <= 60
           [ECSS-E-ST-10-06C: approach phase 10–60 time units]
  invariant on APPROACH: t_approach <= 60
  reset:   t_align := 0

ALIGNMENT -> CONTACT
  trigger: alignment_achieved!
  guard:   t_align >= 5 && t_align <= 40
           [ISO 9283: alignment must be achieved within 40 units
            once the approach phase completes]
  invariant on ALIGNMENT: t_align <= 40
  reset:   t_contact := 0

CONTACT -> GRASPING
  trigger: grasp_initiated!
  guard:   t_contact >= 1 && t_contact <= 10
           [ECSS-E-ST-40C Sec.5.2.4: contact confirmation within 10 units]
  invariant on CONTACT: t_contact <= 10
  reset:   t_grasp := 0

GRASPING -> SECURED
  trigger: grasp_secured!
  guard:   t_grasp >= 5 && t_grasp <= 20
           [NASA-STD-8719.13C: grasp confirmation within 20 units]
  invariant on GRASPING: t_grasp <= 20
  reset:   t_grasp := 0

SECURED -> RELEASE
  trigger: release_initiated!
  guard:   t_grasp >= 5

RELEASE -> STANDBY
  trigger: release_complete!
  guard:   t_grasp >= 3

ANY -> ABORT
  trigger: abort_command!
  guard:   t_abort <= 3
           [NASA-STD-8719.13C Req 4.2.3: abort reachable within 3 units]
  (reachable from APPROACH, ALIGNMENT, CONTACT, GRASPING)
  reset:   t_abort := 0

ABORT -> STANDBY
  trigger: abort_complete!
  guard:   t_abort >= 5 && t_abort <= 30
  invariant on ABORT: t_abort <= 30

ANY -> FAULT
  trigger: fault_detected!
  guard:   t_abort <= 2
  (reachable from ALIGNMENT, CONTACT, GRASPING)
  reset:   t_abort := 0

FAULT -> ABORT
  trigger: fault_abort!
  guard:   t_abort <= 3
  invariant on FAULT: t_abort <= 3
```

### File: `assets/CS5_Grasping/V2_DT.xml`

Template name: `GraspingMissionDT`

**Label mapping intent:**
```
PT label              DT label
approach_start!    --> mission_phase_start!
approach_complete! --> proximity_ops_confirmed!
alignment_achieved!--> attitude_sync_telemetry!
grasp_initiated!   --> capture_sequence_start!
grasp_secured!     --> capture_confirmed_telemetry!
release_initiated! --> release_sequence_start!
release_complete!  --> release_confirmed!
abort_command!     --> abort_uplink!
abort_complete!    --> safe_config_confirmed!
fault_detected!    --> anomaly_detected_telemetry!
fault_abort!       --> emergency_abort_command!
```

**Clocks:** `t_telem`, `t_capture`, `t_force`, `t_anomaly`

**Locations (8 total):**
```
DT_IDLE
DT_PROXIMITY_OPS
DT_ATTITUDE_SYNC
DT_CAPTURE_SEQUENCE
DT_CAPTURE_CONFIRMED
DT_RELEASE_OPS
DT_ABORT_SEQUENCE
DT_ANOMALY
```

Timing: DT bounds = PT bounds + 4 units (CFDP LEO telemetry latency
per CCSDS 727.0-B-5, modelled as relaxed timing in weak bisimulation).

---

## STEP 2 — Ontology

### File: `assets/CS5_Grasping/domain.ont`

```
; Ontology: Autonomous Space Debris Grasping System
; Standard references:
;   ECSS-E-ST-40C:2009   — Space Engineering: Software
;   ECSS-E-ST-10-06C:2017— Technical Requirements Specification
;   ECSS-Q-ST-80C:2017   — Software Product Assurance
;   ISO 9283:1998        — Manipulating Industrial Robots: Performance Criteria
;   CCSDS 727.0-B-5:2017 — CFDP (telemetry latency bounds)
;   NASA-STD-8719.13C    — Software Safety Standard
;
; Fragment: Quantifier-free Linear Real Arithmetic (QF_LRA)

; === SORTS ===
sort Angle         ; degrees — rotational alignment error
sort Force         ; Newtons — grasp/contact force
sort Distance      ; metres — approach range and position error
sort Torque        ; Nm — reaction torque from target rotation
sort AngularRate   ; deg/s — target tumble rate
sort SignalDelay   ; seconds — CFDP telemetry one-way delay

; === FUNCTIONS ===
; Alignment and proximity
fun alignment_error       : Angle     ; rotational error to target frame (degrees)
fun max_alignment_error   : Angle     ; ISO 9283 / ECSS tolerance: 2 degrees
fun approach_distance     : Distance  ; range to target centre of mass (m)
fun contact_threshold     : Distance  ; distance at which contact is declared (m)
fun position_error        : Distance  ; translational error at end-effector (m)
fun max_position_error    : Distance  ; ISO 9283 positioning accuracy: 0.002 m

; Force and torque
fun grasp_force           : Force     ; current end-effector grasp force (N)
fun min_grasp_force       : Force     ; minimum for secure capture (N)
fun max_grasp_force       : Force     ; structural limit (N)
fun contact_force         : Force     ; force at first contact (N)
fun target_torque         : Torque    ; reaction torque from tumbling target (Nm)
fun max_target_torque     : Torque    ; maximum handleable tumble torque (Nm)

; Target dynamics
fun target_tumble_rate    : AngularRate ; target angular velocity (deg/s)
fun max_tumble_rate       : AngularRate ; maximum tumble rate for safe approach (deg/s)

; Communication
fun telem_delay           : SignalDelay ; current one-way CFDP delay (s)
fun max_telem_delay       : SignalDelay ; CCSDS 727.0-B-5 LEO bound: 4 time units

; Mission completion
fun grasp_cycles_total    : Distance  ; reusing sort for count — total grasp attempts
fun mission_phase_progress: Angle     ; reusing sort — 0-100% mission progress

; === RELATIONS ===
rel aligned_to_target     :           ; alignment_error <= max_alignment_error
rel contact_made          :           ; approach_distance <= contact_threshold
rel grasp_secure          :           ; grasp_force >= min_grasp_force
rel abort_required        :           ; target_tumble_rate > max_tumble_rate
rel telem_nominal         :           ; telem_delay <= max_telem_delay

; === AXIOMS ===

; --- ISO 9283 / ECSS tolerance parameters ---
axiom max_align_val       : max_alignment_error = 2
axiom contact_thresh_val  : contact_threshold = 1
axiom max_pos_error_val   : max_position_error = 1

; --- Force parameters (NASA-STD-8719.13C structural margins) ---
axiom min_grasp_val       : min_grasp_force = 50
axiom max_grasp_val       : max_grasp_force = 300
axiom grasp_ordering      : min_grasp_force < max_grasp_force

; --- Non-negativity ---
axiom nn_align            : alignment_error >= 0
axiom nn_dist             : approach_distance >= 0
axiom nn_grasp            : grasp_force >= 0
axiom nn_contact          : contact_force >= 0
axiom nn_torque           : target_torque >= 0
axiom nn_tumble           : target_tumble_rate >= 0
axiom nn_delay            : telem_delay >= 0

; --- Target dynamics limits (ECSS-E-ST-40C Sec.5.3.3 proximity ops) ---
axiom max_tumble_val      : max_tumble_rate = 3
axiom max_torque_val      : max_target_torque = 10

; --- CFDP telemetry latency (CCSDS 727.0-B-5 LEO) ---
axiom max_delay_val       : max_telem_delay = 4
axiom delay_bound         : telem_delay <= max_telem_delay

; --- Contact force must not exceed structural limit ---
axiom contact_force_bound : contact_force <= max_grasp_force

; --- Grasp security condition ---
axiom grasp_security      : grasp_force <= max_grasp_force
axiom grasp_lower_bound   : grasp_force >= 0

; --- Alignment must be within tolerance before contact ---
axiom align_approach_bound: alignment_error <= max_alignment_error + 10
; +10 degrees tolerance during approach phase (tightens to 2 at CONTACT)
```

---

## STEP 3 — Interpretations

### File: `assets/CS5_Grasping/pt.interp`

```
; PT Interpretation: GraspingPT
; GNC engineer perspective. Standards: ECSS-E-ST-40C, NASA-STD-8719.13C

; --- Location interpretations ---
STANDBY     : (and (= grasp_force 0) (> approach_distance contact_threshold))
APPROACH    : (and (> approach_distance contact_threshold) (<= target_tumble_rate max_tumble_rate))
ALIGNMENT   : (and (<= approach_distance 5) (<= alignment_error max_alignment_error + 1))
CONTACT     : (and (<= approach_distance contact_threshold) (<= alignment_error max_alignment_error))
GRASPING    : (and (<= approach_distance contact_threshold) (> grasp_force 0) (<= grasp_force max_grasp_force))
SECURED     : (and (>= grasp_force min_grasp_force) (<= grasp_force max_grasp_force))
RELEASE     : (and (>= grasp_force 0) (<= approach_distance contact_threshold))
ABORT       : (= grasp_force 0)
FAULT       : (and (>= alignment_error max_alignment_error) (> target_tumble_rate max_tumble_rate))

; --- Event interpretations ---
approach_start!      : (and (> approach_distance contact_threshold) (<= target_tumble_rate max_tumble_rate))
approach_complete!   : (and (<= approach_distance 5) (<= target_tumble_rate max_tumble_rate))
alignment_achieved!  : (and (<= alignment_error max_alignment_error) (<= approach_distance contact_threshold + 1))
grasp_initiated!     : (and (<= alignment_error max_alignment_error) (<= approach_distance contact_threshold))
grasp_secured!       : (and (>= grasp_force min_grasp_force) (<= grasp_force max_grasp_force))
release_initiated!   : (>= grasp_force min_grasp_force)
release_complete!    : (= grasp_force 0)
abort_command!       : (>= target_tumble_rate max_tumble_rate - 1)
abort_complete!      : (and (= grasp_force 0) (> approach_distance contact_threshold))
fault_detected!      : (> target_torque max_target_torque)
fault_abort!         : (and (> target_torque max_target_torque) (= grasp_force 0))
```

### File: `assets/CS5_Grasping/dt.interp`

```
; DT Interpretation: GraspingMissionDT
; Mission operations perspective. Uses telemetry-derived quantities.

; --- Location interpretations ---
DT_IDLE               : (and (= grasp_force 0) (> approach_distance contact_threshold))
DT_PROXIMITY_OPS      : (and (> approach_distance contact_threshold) (<= target_tumble_rate max_tumble_rate))
DT_ATTITUDE_SYNC      : (and (<= approach_distance 5) (<= alignment_error max_alignment_error + 1))
DT_CAPTURE_SEQUENCE   : (and (<= approach_distance contact_threshold) (> grasp_force 0))
DT_CAPTURE_CONFIRMED  : (and (>= grasp_force min_grasp_force) (<= grasp_force max_grasp_force))
DT_RELEASE_OPS        : (>= grasp_force 0)
DT_ABORT_SEQUENCE     : (= grasp_force 0)
DT_ANOMALY            : (> target_torque max_target_torque)

; --- Event interpretations ---
mission_phase_start!        : (and (> approach_distance contact_threshold) (<= target_tumble_rate max_tumble_rate))
proximity_ops_confirmed!    : (and (<= approach_distance 5) (<= target_tumble_rate max_tumble_rate))
attitude_sync_telemetry!    : (and (<= alignment_error max_alignment_error) (<= approach_distance contact_threshold + 1))
capture_sequence_start!     : (and (<= alignment_error max_alignment_error) (<= approach_distance contact_threshold))
capture_confirmed_telemetry!: (and (>= grasp_force min_grasp_force) (<= grasp_force max_grasp_force))
release_sequence_start!     : (>= grasp_force min_grasp_force)
release_confirmed!          : (= grasp_force 0)
abort_uplink!               : (>= target_tumble_rate max_tumble_rate - 1)
safe_config_confirmed!      : (and (= grasp_force 0) (> approach_distance contact_threshold))
anomaly_detected_telemetry! : (> target_torque max_target_torque)
emergency_abort_command!    : (and (> target_torque max_target_torque) (= grasp_force 0))
```

---

## STEP 4 — Misalignment Variants

### Variant A — Timing Violation (Primary RQ2 result)

### File: `assets/CS5_Grasping/V2_DT_timing_violation.xml`

Copy `V2_DT.xml`. Change the `DT_ATTITUDE_SYNC -> DT_CAPTURE_SEQUENCE`
transition so that `capture_sequence_start!` can fire up to
`t_telem <= 48` instead of the correct bound `<= 44` (40 + 4 latency).
The 4 extra time units mean the DT can confirm capture initiation
**after** the ECSS-E-ST-10-06C alignment window has closed.

This misalignment is the central RQ2 result for this case study:
- Syntactic bisimulation: FAILS (for wrong reason — label mismatch)
- Published Oakes et al. Monitor 2 (force threshold at SECURED): CANNOT DETECT
  (it only checks grasp_force at the SECURED state, not alignment timing)
- Published Oakes et al. Monitor 1 (alignment within window): CANNOT DETECT
  (it checks alignment_error value, not the timing of the transition)
- SemAlign delay Condition IV: DETECTS — the DT's timing zone for
  `capture_sequence_start!` does not match the PT's timing zone for
  `grasp_initiated!` under the 4-unit latency tolerance

### Variant B — Threshold Drift: Grasp Force Gap

### File: `assets/CS5_Grasping/V2_DT_thresh_drift.xml` + `dt_thresh_drift.interp`

Change `capture_confirmed_telemetry!` to fire at `grasp_force >= 40`
instead of `>= min_grasp_force (= 50)`. The 10N gap means the DT
reports successful capture when the grasp is insufficiently firm —
target could be re-released during debris disposal manoeuvre.

### Published Monitor Baseline

Implement the two Oakes et al. monitors in:
`src/baselines/grasping_monitors.cpp`
`include/rtwbs/baselines/grasping_monitors.h`

```
Monitor 1 (from Oakes et al.): "ECSS_AlignmentWindow"
  Checks: alignment_error <= max_alignment_error at CONTACT state
  property: (<= alignment_error max_alignment_error)
  label:    grasp_initiated! / capture_sequence_start!
  type:     safety

Monitor 2 (from Oakes et al.): "ECSS_GraspForce"
  Checks: grasp_force >= min_grasp_force at SECURED state
  property: (>= grasp_force min_grasp_force)
  label:    grasp_secured! / capture_confirmed_telemetry!
  type:     safety
```

These monitors are sourced from Oakes et al. (MODELS 2024) — cite them
explicitly in the benchmark output CSV and in the paper as a peer-reviewed
baseline, not a self-constructed one.

### Variant C — Ontology Evolution (RQ1)

### File: `assets/CS5_Grasping/domain_v2.ont`

**Condition I:** Add `sort Mass` (kg), `fun target_mass : Mass`,
`fun max_capturable_mass : Mass` (ESA e.Deorbit programme specification:
maximum capturable debris mass 3,000 kg).

**Condition II:** Tighten `max_alignment_error` from 2 to 1.5 degrees
(ISO 9283 tighter variant for high-value target capture).

**Condition III:** Update `alignment_achieved!` and `attitude_sync_telemetry!`
interpretations accordingly.

---

## STEP 5 — Benchmark Runner

### File: `benchmark/run_CS5.cpp`

Follow the structure of `benchmark/run_CS1.cpp`. Extend Run 4 to use the
published Oakes et al. monitors as the runtime baseline (not generic monitors).
In the CSV output, add a column `monitor_source` with value `"Oakes_MODELS2024"`
for these runs to make the peer-reviewed provenance explicit. This is the
column that directly supports the RQ2 claim in the paper.