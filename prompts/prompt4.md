# Prompt CS4 — Aircraft Engine Controller (NASA/FRET + Lockheed Martin CPS)

## Prerequisites

Prompts 0, CS1, CS2, CS3 must be fully implemented and passing.
Study all completed case studies in `assets/` before starting.
This is the most complex case study — it has the largest state space
and includes a compositional verification experiment for RQ3.

---

## Background and Domain

This case study is derived from two sources: the NASA/FRET aircraft engine
software controller use case and the Lockheed Martin Cyber-Physical Systems
(LMCPS) challenge problems. The system models a dual-spool turbofan engine
controller managing fuel flow, thrust scheduling, and health monitoring.

Governing standards:
- **DO-178C (2011)** — Software Considerations in Airborne Systems and
  Equipment Certification. Primary avionics software standard.
  - Section 6.3.1: Verification of timing and sequencing requirements
  - Section 6.4.2: Structural coverage for DAL-B software
  - Table A-3: Independence requirements for safety-critical functions
- **ARP4754A (2010)** — Guidelines for Development of Civil Aircraft and
  Systems. System-level safety:
  - Section 5.3: Functional Hazard Assessment (FHA) — engine flame-out
    and overheat classified as Hazardous (Class III)
  - Section 7.1: Derived safety requirements from FHA
- **SAE AS50881F (2020)** — Wiring Aerospace Vehicle. Electrical system
  safety bounds for engine control wiring (used for voltage/current
  limits in the ontology).
- **FAA AC 33.28-3 (2017)** — Aircraft Engine Control System
  Installation and Test. Key provisions:
  - Maximum EGT (Exhaust Gas Temperature) operational limit: 950°C
  - N1 fan speed limits: 100% = 3,500 RPM, redline = 104%
  - Fuel flow rate: maximum 3,000 lb/hr at takeoff, minimum 200 lb/hr at idle
  - Engine start time: idle must be reached within 45 seconds
  - Flame-out response: relight attempt must initiate within 3 seconds
- **MIL-STD-882E (2012)** — System Safety. Risk matrix for
  catastrophic/critical/marginal/negligible severity classifications.
  Used to define the safety property hierarchy in the ontology.

The **PT view** is the engine controller from the *avionics software
engineer* perspective: fuel metering commands, thrust lever angle events,
engine mode transitions, and FADEC (Full Authority Digital Engine Control)
alarm outputs are the observable events.

The **DT view** is an **engine health monitoring twin** authored by the
*propulsion health management team*. It tracks engine degradation indices
(EGT margin, N1 speed margin, fuel flow deviation) against fleet-average
baselines. The DT uses prognostics-oriented vocabulary completely
different from the FADEC command vocabulary of the PT.

This case study also serves the **compositional verification** experiment
in RQ3. The engine controller naturally decomposes into two subsystems:
- **Fuel Management Subsystem (FMS):** start, idle, fuel metering, shutdown
- **Thrust Management Subsystem (TMS):** takeoff, climb, cruise, descent, landing

---

## STEP 1 — UPPAAL Models

### File: `assets/CS4_Engine/V1_PT.xml`

Template name: `EngineControllerPT`

**Clocks:** `t_start`, `t_phase`, `t_temp`, `t_fuel`, `t_fault`

**Locations (12 total):**
```
ENGINE_OFF        — FADEC powered, engine not running
STARTING          — ignition sequence active, engine accelerating
IDLE              — engine at ground idle (N1 ~30%), fuel flow minimum
TAKEOFF           — maximum rated thrust, N1 ~100%
CLIMB             — climb thrust rating, N1 ~94%
CRUISE            — cruise thrust, N1 ~88%, EGT nominal
DESCENT           — flight idle, fuel flow reduced
APPROACH          — approach thrust, N1 ~78%
LANDING           — reverse thrust / ground idle post-touchdown
OVERHEAT          — EGT > max_egt: thermal protection active
FLAME_OUT         — engine flame-out detected: relight sequence
EMERGENCY_SHUTDOWN— catastrophic fault: immediate fuel cutoff
```

**Transitions and timing constraints:**

```
ENGINE_OFF -> STARTING
  trigger: engine_start!
  guard:   t_start >= 0
  reset:   t_start := 0, t_temp := 0

STARTING -> IDLE
  trigger: idle_reached!
  guard:   t_start >= 10 && t_start <= 45
           [FAA AC 33.28-3: idle must be reached within 45 seconds]
  invariant on STARTING: t_start <= 45
  reset:   t_phase := 0

IDLE -> TAKEOFF
  trigger: takeoff_thrust!
  guard:   t_phase >= 3
  reset:   t_phase := 0, t_fuel := 0

TAKEOFF -> CLIMB
  trigger: climb_thrust!
  guard:   t_phase >= 10 && t_phase <= 120
           [ARP4754A: minimum takeoff phase before thrust reduction]
  invariant on TAKEOFF: t_phase <= 120
  reset:   t_phase := 0

CLIMB -> CRUISE
  trigger: cruise_thrust!
  guard:   t_phase >= 30
  reset:   t_phase := 0

CRUISE -> DESCENT
  trigger: descent_initiated!
  guard:   t_phase >= 60
  reset:   t_phase := 0

DESCENT -> APPROACH
  trigger: approach_thrust!
  guard:   t_phase >= 20 && t_phase <= 300
  invariant on DESCENT: t_phase <= 300
  reset:   t_phase := 0

APPROACH -> LANDING
  trigger: landing_thrust!
  guard:   t_phase >= 5 && t_phase <= 60
  invariant on APPROACH: t_phase <= 60
  reset:   t_phase := 0

LANDING -> IDLE
  trigger: ground_idle!
  guard:   t_phase >= 5
  reset:   t_phase := 0

IDLE -> ENGINE_OFF
  trigger: shutdown_normal!
  guard:   t_phase >= 5

ANY -> OVERHEAT
  trigger: overheat_detected!
  guard:   t_temp <= 3
           [DO-178C Sec.6.3.1: thermal event response within 3 time units]
  (reachable from TAKEOFF, CLIMB, CRUISE)
  reset:   t_fault := 0

OVERHEAT -> EMERGENCY_SHUTDOWN
  trigger: emergency_shutdown!
  guard:   t_fault <= 5
           [ARP4754A Sec.5.3: Hazardous event response ≤5 time units]
  invariant on OVERHEAT: t_fault <= 5

ANY -> FLAME_OUT
  trigger: flameout_detected!
  guard:   t_temp <= 2
  (reachable from CRUISE, DESCENT, APPROACH)
  reset:   t_fault := 0

FLAME_OUT -> STARTING
  trigger: relight_initiated!
  guard:   t_fault >= 1 && t_fault <= 3
           [FAA AC 33.28-3: relight must initiate within 3 seconds of flame-out]
  invariant on FLAME_OUT: t_fault <= 3

EMERGENCY_SHUTDOWN -> ENGINE_OFF
  trigger: system_reset!
  guard:   (no guard)
```

### File: `assets/CS4_Engine/V2_DT.xml`

Template name: `EngineHealthDT`

The DT is the **engine health monitoring twin** from the propulsion health
management team's perspective. It tracks EGT margin, N1 margin, and fuel
flow deviation from fleet baseline.

**Label mapping intent:**
```
PT label                DT label
engine_start!        --> engine_spool_telemetry!
idle_reached!        --> idle_confirmation_event!
takeoff_thrust!      --> takeoff_power_mode!
climb_thrust!        --> climb_power_mode!
cruise_thrust!       --> cruise_power_mode!
descent_initiated!   --> descent_power_mode!
approach_thrust!     --> approach_power_mode!
landing_thrust!      --> landing_power_mode!
ground_idle!         --> ground_ops_mode!
shutdown_normal!     --> shutdown_telemetry!
overheat_detected!   --> thermal_exceedance_event!
emergency_shutdown!  --> protective_shutdown_command!
flameout_detected!   --> combustion_loss_event!
relight_initiated!   --> relight_command_telemetry!
system_reset!        --> (DT-internal tau)
```

**Clocks:** `t_spool`, `t_health`, `t_egt`, `t_n1`

**Locations (10 total):**
```
HM_OFFLINE
HM_SPOOLING
HM_GROUND_OPS
HM_HIGH_POWER
HM_CRUISE_OPS
HM_LOW_POWER
HM_THERMAL_EVENT
HM_COMBUSTION_LOSS
HM_PROTECTIVE_STOP
HM_SHUTDOWN
```

Timing: DT bounds = PT bounds + 2 units telemetry latency tolerance.

---

### Compositional Subsystem Files

#### File: `assets/CS4_Engine/V1_PT_FMS.xml`

Template name: `FuelMgmtPT` — locations: ENGINE_OFF, STARTING, IDLE,
LANDING (ground ops), EMERGENCY_SHUTDOWN. Events: engine_start!,
idle_reached!, ground_idle!, shutdown_normal!, emergency_shutdown!,
system_reset!. Clocks: t_start, t_fault.

#### File: `assets/CS4_Engine/V2_DT_FMS.xml`

Template name: `FuelMgmtDT` — corresponding DT subsystem for fuel management.
Locations: HM_OFFLINE, HM_SPOOLING, HM_GROUND_OPS, HM_PROTECTIVE_STOP, HM_SHUTDOWN.

#### File: `assets/CS4_Engine/V1_PT_TMS.xml`

Template name: `ThrustMgmtPT` — locations: IDLE, TAKEOFF, CLIMB, CRUISE,
DESCENT, APPROACH, LANDING, OVERHEAT, FLAME_OUT. Events: takeoff_thrust!,
climb_thrust!, cruise_thrust!, descent_initiated!, approach_thrust!,
landing_thrust!, overheat_detected!, flameout_detected!, relight_initiated!.
Clocks: t_phase, t_temp, t_fuel, t_fault.

#### File: `assets/CS4_Engine/V2_DT_TMS.xml`

Template name: `ThrustMgmtDT` — corresponding DT subsystem for thrust management.
Locations: HM_GROUND_OPS, HM_HIGH_POWER, HM_CRUISE_OPS, HM_LOW_POWER,
HM_THERMAL_EVENT, HM_COMBUSTION_LOSS.

---

## STEP 2 — Ontology

### File: `assets/CS4_Engine/domain.ont`

```
; Ontology: Aircraft Turbofan Engine Controller
; Standard references:
;   DO-178C:2011      — Software Considerations in Airborne Systems (DAL-B)
;   ARP4754A:2010     — Guidelines for Development of Civil Aircraft and Systems
;   FAA AC 33.28-3    — Aircraft Engine Control System Installation and Test
;   SAE AS50881F:2020 — Wiring Aerospace Vehicle (electrical bounds)
;   MIL-STD-882E:2012 — System Safety (severity classification)
;
; Fragment: Quantifier-free Linear Real Arithmetic (QF_LRA)

; === SORTS ===
sort Temperature   ; Celsius — gas path temperatures
sort Speed         ; percent_N1 — fan/core speed as % of rated
sort FuelFlow      ; lb_per_hr — fuel mass flow rate
sort Pressure      ; psia — absolute pressure (fan inlet, combustor)
sort ThrustLevel   ; percent — thrust as percentage of rated takeoff thrust
sort HealthIndex   ; 0.0 to 1.0 — degradation index (1.0=new, 0.0=failed)
sort Vibration     ; inches_per_sec — engine vibration level

; === FUNCTIONS ===
; Engine performance parameters
fun egt               : Temperature  ; Exhaust Gas Temperature (turbine exit)
fun max_egt           : Temperature  ; FAA AC 33.28-3 red-line: 950°C
fun takeoff_egt_limit : Temperature  ; 5-minute takeoff EGT limit: 935°C
fun n1_speed          : Speed        ; fan speed (% rated)
fun n2_speed          : Speed        ; core speed (% rated)
fun n1_redline        : Speed        ; FAA: 104% N1 (absolute maximum)
fun n1_takeoff_limit  : Speed        ; 100% N1 (rated takeoff)
fun fuel_flow         : FuelFlow     ; current fuel flow rate
fun max_fuel_flow     : FuelFlow     ; FAA AC 33.28-3: 3000 lb/hr at takeoff
fun min_fuel_flow     : FuelFlow     ; minimum at idle: 200 lb/hr
fun fan_inlet_pressure: Pressure     ; P2 — fan inlet total pressure

; Health monitoring parameters
fun egt_margin        : Temperature  ; max_egt - egt (positive = safe headroom)
fun n1_margin         : Speed        ; n1_redline - n1_speed
fun fuel_flow_deviation: FuelFlow    ; actual - baseline_fuel_flow (fleet average)
fun health_index_egt  : HealthIndex  ; EGT-based degradation index
fun health_index_n1   : HealthIndex  ; N1-based degradation index
fun vibration_level   : Vibration    ; broadband vibration (IPS)
fun max_vibration     : Vibration    ; ARP4754A alert threshold: 1.5 IPS

; Safety thresholds
fun emergency_egt_threshold : Temperature ; threshold for emergency shutdown
fun flameout_n1_threshold   : Speed       ; N1 below which flame-out is declared

; === RELATIONS ===
rel engine_running        :           ; n1_speed > flameout_n1_threshold
rel thermally_exceeded    :           ; egt > max_egt
rel n1_redlined           :           ; n1_speed > n1_redline
rel health_degraded       :           ; health_index_egt < 0.5 OR health_index_n1 < 0.5
rel fuel_flow_nominal     :           ; abs(fuel_flow_deviation) < 50

; === AXIOMS ===

; --- FAA AC 33.28-3 engine performance limits ---
axiom max_egt_val         : max_egt = 950
axiom takeoff_egt_lim     : takeoff_egt_limit = 935
axiom n1_redline_val      : n1_redline = 104
axiom n1_takeoff_val      : n1_takeoff_limit = 100
axiom max_fuel_val        : max_fuel_flow = 3000
axiom min_fuel_val        : min_fuel_flow = 200
axiom emergency_egt_val   : emergency_egt_threshold = 970

; --- Flame-out detection threshold (DO-178C verified requirement) ---
axiom flameout_n1_val     : flameout_n1_threshold = 20

; --- Non-negativity ---
axiom nn_egt              : egt >= 0
axiom nn_n1               : n1_speed >= 0
axiom nn_n2               : n2_speed >= 0
axiom nn_fuel             : fuel_flow >= 0
axiom nn_fan_press        : fan_inlet_pressure >= 0
axiom nn_vib              : vibration_level >= 0

; --- Operational bounds ---
axiom egt_upper           : egt <= emergency_egt_threshold + 20
axiom n1_upper            : n1_speed <= n1_redline + 2
axiom fuel_upper          : fuel_flow <= max_fuel_flow + 100
axiom vib_upper           : vibration_level <= max_vibration + 1

; --- EGT margin definition ---
axiom egt_margin_def      : egt_margin = max_egt - egt

; --- N1 margin definition ---
axiom n1_margin_def       : n1_margin = n1_redline - n1_speed

; --- Health index bounds (MIL-STD-882E degradation model) ---
axiom hi_egt_lower        : health_index_egt >= 0
axiom hi_egt_upper        : health_index_egt <= 1
axiom hi_n1_lower         : health_index_n1 >= 0
axiom hi_n1_upper         : health_index_n1 <= 1

; --- EGT-health relationship (ARP4754A FHA-derived) ---
; When EGT margin < 30°C, health is degraded
axiom egt_health_relation : egt_margin < 30 -> health_index_egt < 0.5

; --- Fuel flow at idle ---
axiom idle_fuel_lower     : fuel_flow >= min_fuel_flow - 10
; -10 tolerance for transient during deceleration

; --- ARP4754A vibration limit ---
axiom max_vib_val         : max_vibration = 1
```

---

## STEP 3 — Interpretations

### File: `assets/CS4_Engine/pt.interp`

```
; PT Interpretation: EngineControllerPT
; FADEC / avionics software engineer perspective.
; Standards: DO-178C, ARP4754A, FAA AC 33.28-3

; --- Location interpretations ---
ENGINE_OFF          : (and (= n1_speed 0) (= fuel_flow 0))
STARTING            : (and (> n1_speed 0) (< n1_speed 30) (>= fuel_flow min_fuel_flow))
IDLE                : (and (>= n1_speed 25) (<= n1_speed 35) (>= fuel_flow min_fuel_flow) (<= fuel_flow 400))
TAKEOFF             : (and (>= n1_speed 95) (<= n1_speed n1_takeoff_limit) (<= egt takeoff_egt_limit))
CLIMB               : (and (>= n1_speed 88) (<= n1_speed 96) (<= egt max_egt))
CRUISE              : (and (>= n1_speed 82) (<= n1_speed 90) (<= egt max_egt - 50))
DESCENT             : (and (>= n1_speed 40) (<= n1_speed 60) (<= fuel_flow 1000))
APPROACH            : (and (>= n1_speed 70) (<= n1_speed 82) (<= egt max_egt))
LANDING             : (and (<= n1_speed 40) (<= fuel_flow 600))
OVERHEAT            : (> egt max_egt)
FLAME_OUT           : (< n1_speed flameout_n1_threshold)
EMERGENCY_SHUTDOWN  : (and (= fuel_flow 0) (= n1_speed 0))

; --- Event interpretations ---
engine_start!        : (and (= n1_speed 0) (>= fuel_flow min_fuel_flow))
idle_reached!        : (and (>= n1_speed 25) (<= n1_speed 35) (>= fuel_flow min_fuel_flow))
takeoff_thrust!      : (and (>= n1_speed 95) (<= fuel_flow max_fuel_flow) (<= egt takeoff_egt_limit))
climb_thrust!        : (and (>= n1_speed 88) (<= egt max_egt))
cruise_thrust!       : (and (>= n1_speed 82) (<= egt max_egt - 50))
descent_initiated!   : (and (<= n1_speed 60) (<= fuel_flow 1000))
approach_thrust!     : (and (>= n1_speed 70) (<= egt max_egt))
landing_thrust!      : (<= n1_speed 40)
ground_idle!         : (and (<= n1_speed 35) (<= fuel_flow 400))
shutdown_normal!     : (and (<= n1_speed 10) (<= fuel_flow min_fuel_flow))
overheat_detected!   : (> egt max_egt)
emergency_shutdown!  : (and (> egt emergency_egt_threshold) (= fuel_flow 0))
flameout_detected!   : (< n1_speed flameout_n1_threshold)
relight_initiated!   : (and (< n1_speed flameout_n1_threshold) (>= fuel_flow min_fuel_flow))
system_reset!        : (and (= n1_speed 0) (= fuel_flow 0))
```

### File: `assets/CS4_Engine/dt.interp`

```
; DT Interpretation: EngineHealthDT
; Propulsion health management team perspective.
; Uses health indices, margins, and degradation metrics.

; --- Location interpretations ---
HM_OFFLINE          : (and (= n1_speed 0) (= fuel_flow 0))
HM_SPOOLING         : (and (> n1_speed 0) (< n1_speed 30) (> health_index_n1 0))
HM_GROUND_OPS       : (and (>= n1_speed 25) (<= n1_speed 35) (>= health_index_egt 0))
HM_HIGH_POWER       : (and (>= n1_speed 88) (>= egt_margin 0) (>= health_index_egt 0))
HM_CRUISE_OPS       : (and (>= n1_speed 82) (<= n1_speed 92) (>= egt_margin 50))
HM_LOW_POWER        : (and (<= n1_speed 65) (<= fuel_flow 1000))
HM_THERMAL_EVENT    : (> egt max_egt)
HM_COMBUSTION_LOSS  : (< n1_speed flameout_n1_threshold)
HM_PROTECTIVE_STOP  : (and (= fuel_flow 0) (= n1_speed 0))
HM_SHUTDOWN         : (and (<= n1_speed 10) (<= fuel_flow min_fuel_flow))

; --- Event interpretations ---
engine_spool_telemetry!      : (and (= n1_speed 0) (>= fuel_flow min_fuel_flow))
idle_confirmation_event!     : (and (>= n1_speed 25) (<= n1_speed 35) (>= health_index_n1 0))
takeoff_power_mode!          : (and (>= n1_speed 95) (>= egt_margin 0) (<= egt takeoff_egt_limit))
climb_power_mode!            : (and (>= n1_speed 88) (>= egt_margin 0))
cruise_power_mode!           : (and (>= n1_speed 82) (>= egt_margin 50))
descent_power_mode!          : (and (<= n1_speed 60) (<= fuel_flow 1000))
approach_power_mode!         : (and (>= n1_speed 70) (>= egt_margin 0))
landing_power_mode!          : (<= n1_speed 40)
ground_ops_mode!             : (and (<= n1_speed 35) (>= health_index_egt 0))
shutdown_telemetry!          : (and (<= n1_speed 10) (<= fuel_flow min_fuel_flow))
thermal_exceedance_event!    : (> egt max_egt)
protective_shutdown_command! : (and (> egt emergency_egt_threshold) (= fuel_flow 0))
combustion_loss_event!       : (< n1_speed flameout_n1_threshold)
relight_command_telemetry!   : (and (< n1_speed flameout_n1_threshold) (>= fuel_flow min_fuel_flow))
```

Interpretation files for subsystems (`pt_fms.interp`, `dt_fms.interp`,
`pt_tms.interp`, `dt_tms.interp`) should contain the subset of mappings
corresponding to each subsystem's event alphabet.

---

## STEP 4 — Misalignment Variants

### Variant A — Threshold Drift: EGT Safety Gap

### File: `assets/CS4_Engine/V2_DT_thresh_drift.xml` + `dt_thresh_drift.interp`

Change `thermal_exceedance_event!` to fire at `egt > 980` instead of
`> max_egt (= 950)`. The 30°C gap is operationally significant: this
is the EGT band where blade creep and thermal fatigue accelerate
exponentially (ARP4754A FHA consequence: undetected overheat can
progress to in-flight shutdown within 50 flight cycles).

### Variant B — Missing Event: Flame-out Response Gap

### File: `assets/CS4_Engine/dt_missing_flameout.interp`

Remove `combustion_loss_event!` from the DT interpretation entirely.
The DT has no semantic mapping for the flame-out event —
the most time-critical safety event (3-second relight window per
FAA AC 33.28-3). Semantic alignment must flag this immediately.

### Variant C — Ontology Evolution (RQ1)

### File: `assets/CS4_Engine/domain_v2.ont`

**Condition I:** Add `sort Emissions` (kg/hr CO2), `fun nox_emissions : Emissions`,
`fun co2_emissions : Emissions`, `fun max_nox : Emissions` (ICAO Annex 16
environmental standard — reflects regulatory evolution in engine certification).

**Condition II:** Tighten `takeoff_egt_limit` from 935 to 925 (fleet
data shows 10°C margin erosion after 3,000 cycles — ARP4754A
periodic safety re-assessment outcome).

**Condition III:** Update `takeoff_thrust!` and `takeoff_power_mode!`
interpretations to reference the tighter EGT limit.

---

## STEP 5 — Benchmark Runner

### File: `benchmark/run_CS4.cpp`

Follow the same structure as `benchmark/run_CS1.cpp` for the five standard
runs (aligned, syntactic, misalignment, runtime monitor, ontology evolution).

Additionally implement the **compositional verification experiment (RQ3)**:

```
// RQ3 Compositional Experiment
// Run 6: Monolithic alignment check on full PT vs full DT
auto result_monolithic = checker.check(pt_full, dt_full, ontology, pt_interp, dt_interp);

// Run 7: Compositional — verify FMS and TMS subsystems independently
auto result_fms = checker.check(pt_fms, dt_fms, ontology, pt_fms_interp, dt_fms_interp);
auto result_tms = checker.check(pt_tms, dt_tms, ontology, pt_tms_interp, dt_tms_interp);

// Report monolithic vs compositional timing
// Both should yield ALIGNED = true (compositional result must match monolithic)
// Key metric: compositional_time = result_fms.total_time_ms + result_tms.total_time_ms
// Compare against result_monolithic.total_time_ms
```

Report the speedup factor (monolithic / compositional) in the CSV.