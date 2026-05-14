# Prompt CS6 — GPCA Infusion Pump

## Prerequisites

Prompts 0–CS5 must be fully implemented. Study all completed case studies.

---

## Background and Domain

The GPCA (Generic Patient Controlled Analgesia) infusion pump is a
classic safety-critical benchmark extensively used in the formal methods
community, originally developed by Jiang et al. and adopted by the
FDA for infusion pump software evaluation. The PT models the physical
pump dispensing medication; the DT is a dosage monitoring and safety
enforcement twin.

Governing standards:
- **IEC 60601-1:2005+AMD1:2012** — Medical Electrical Equipment:
  General Safety Requirements. Primary medical device electrical standard:
  - Clause 14: Programmable Electrical Medical Systems (PEMS) requirements
  - Clause 14.3: Software lifecycle requirements
  - Clause 14.11: Risk management for PEMS
- **IEC 62443-3-3:2013** — Industrial Automation and Control Systems
  Security: System Security Requirements. Applied to infusion pump
  cybersecurity (FDA guidance since 2022 requires IEC 62443 compliance):
  - Security Requirement SR 2.12: Non-repudiation of dosage events
  - Security Requirement SR 3.3: Security monitoring
- **FDA Guidance: Infusion Pump Software Safety Research (2010)**:
  - Maximum VTBI (Volume To Be Infused) safety limit: hard coded
  - Occlusion alarm response time: ≤5 minutes (modelled as ≤5 time units)
  - Over-infusion alarm: must fire when cumulative > prescribed + tolerance
- **IEC 62133-2:2017** — Secondary lithium cells: battery safety for
  medical devices (used for battery-related timing constraints).
- **ASTM F2761-09** — Medical Devices Incorporating Software — Integrated
  Clinical Environment (ICE) Architecture Standard:
  - Defines the interface between pump and clinical monitoring systems
  - Used to specify the DT's monitoring interface label vocabulary

The **PT view** models the physical pump from the *pump firmware engineer*
perspective: infusion commands, bolus delivery confirmations, alarm outputs,
and KVO (Keep Vein Open) mode transitions are the observable events.

The **DT view** models a **clinical dosage monitoring twin** authored by
a *clinical informatics engineer* using ASTM F2761 ICE vocabulary.
The DT tracks cumulative dose against prescription limits and integrates
with the clinical monitoring system.

---

## STEP 1 — UPPAAL Models

### File: `assets/CS6_Pump/V1_PT.xml`

Template name: `InfusionPumpPT`

**Clocks:** `t_infuse`, `t_pause`, `t_kvo`, `t_bolus`, `t_alarm`

**Locations (8 total):**
```
IDLE              — pump powered, no active infusion
PRIMING           — line priming sequence before infusion
INFUSING          — primary infusion rate active
BOLUS_DELIVERY    — patient-controlled bolus in progress
PAUSED            — infusion suspended (patient or clinician)
KVO_MODE          — Keep Vein Open: minimum rate, T_pause > 30 units
ALARM_OCCLUSION   — downstream occlusion detected
ALARM_OVERDOSE    — cumulative dose exceeds prescribed + tolerance
```

**Transitions (key constraints from FDA Guidance and IEC 60601-1):**

```
IDLE -> PRIMING
  trigger: prime_start!
  guard:   t_infuse >= 0, reset: t_infuse := 0

PRIMING -> INFUSING
  trigger: infusion_start!
  guard:   t_infuse >= 5 && t_infuse <= 30, invariant: t_infuse <= 30
  reset:   t_infuse := 0, t_bolus := 0

INFUSING -> BOLUS_DELIVERY
  trigger: bolus_request!
  guard:   t_bolus >= 0, reset: t_bolus := 0

BOLUS_DELIVERY -> INFUSING
  trigger: bolus_complete!
  guard:   t_bolus >= 1 && t_bolus <= 10
           [FDA: bolus duration 1–10 time units, max 5ml per event]
  invariant: t_bolus <= 10, reset: t_infuse := 0

INFUSING -> PAUSED
  trigger: pause_infusion!
  reset:   t_pause := 0

PAUSED -> INFUSING
  trigger: resume_infusion!
  guard:   t_pause <= 120, reset: t_pause := 0

PAUSED -> KVO_MODE
  trigger: kvo_start!
  guard:   t_pause >= 30
           [FDA: KVO activated after 30 units pause without resume]
  invariant on PAUSED: t_pause <= 31 (epsilon transition to KVO)
  reset:   t_kvo := 0

KVO_MODE -> INFUSING
  trigger: resume_from_kvo!
  guard:   t_kvo >= 1, reset: t_kvo := 0

INFUSING -> ALARM_OCCLUSION
  trigger: occlusion_alarm!
  guard:   t_alarm <= 5
           [IEC 60601-1 Cl.14.11: occlusion alarm ≤5 time units]
  reset:   t_alarm := 0

ALARM_OCCLUSION -> INFUSING
  trigger: occlusion_cleared!
  guard:   t_alarm >= 1 && t_alarm <= 30, invariant: t_alarm <= 30

INFUSING -> ALARM_OVERDOSE
  trigger: overdose_alarm!
  guard:   t_alarm <= 1
           [FDA: immediate — cumulative dose exceeded prescribed + tolerance]
  reset:   t_alarm := 0

ALARM_OVERDOSE -> IDLE
  trigger: alarm_acknowledged!
  guard:   (no guard)

INFUSING -> IDLE
  trigger: infusion_complete!
  guard:   t_infuse >= 10
```

### File: `assets/CS6_Pump/V2_DT.xml`

Template name: `DosageMonitorDT`

**Label mapping intent (ICE/ASTM F2761 vocabulary):**
```
PT label              DT label
prime_start!       --> ice_device_connect!
infusion_start!    --> ice_therapy_start!
bolus_request!     --> ice_bolus_request!
bolus_complete!    --> ice_bolus_confirmed!
pause_infusion!    --> ice_therapy_pause!
resume_infusion!   --> ice_therapy_resume!
kvo_start!         --> ice_kvo_mode_event!
resume_from_kvo!   --> ice_kvo_resume!
occlusion_alarm!   --> ice_alarm_occlusion!
occlusion_cleared! --> ice_alarm_cleared!
overdose_alarm!    --> ice_safety_limit_breach!
alarm_acknowledged!--> ice_alarm_ack!
infusion_complete! --> ice_therapy_complete!
```

**Clocks:** `t_ice_telem`, `t_dose_track`, `t_safety`, `t_kvo_dt`

**Locations (8 total):**
```
ICE_DISCONNECTED, ICE_PRIMING, ICE_THERAPY_ACTIVE, ICE_BOLUS_ACTIVE,
ICE_THERAPY_PAUSED, ICE_KVO_MODE, ICE_ALARM_OCCLUSION, ICE_ALARM_OVERDOSE
```

---

## STEP 2 — Ontology

### File: `assets/CS6_Pump/domain.ont`

```
; Ontology: GPCA Infusion Pump — Clinical Dosage Domain
; Standards: IEC 60601-1:2005+AMD1, IEC 62443-3-3:2013,
;            FDA Infusion Pump Guidance 2010, ASTM F2761-09
; Fragment: QF_LRA

; === SORTS ===
sort Volume      ; ml — medication volume, non-negative
sort Rate        ; ml_per_hr — infusion rate
sort DoseIndex   ; integer — bolus event counter
sort Pressure    ; mmHg — line pressure (occlusion detection)

; === FUNCTIONS ===
fun dose_delivered       : Volume   ; cumulative volume infused (ml)
fun prescribed_dose      : Volume   ; clinician-ordered total dose (ml)
fun dose_tolerance       : Volume   ; FDA allowable over-infusion tolerance (ml)
fun bolus_volume         : Volume   ; volume per bolus event (ml)
fun max_bolus_volume     : Volume   ; FDA hard limit per bolus: 5 ml
fun primary_rate         : Rate     ; current primary infusion rate (ml/hr)
fun max_primary_rate     : Rate     ; FDA safety limit: 300 ml/hr
fun kvo_rate             : Rate     ; KVO minimum rate: 1 ml/hr
fun bolus_count          : DoseIndex; total bolus events delivered
fun max_bolus_per_hour   : DoseIndex; PCA lockout: max 4 per hour
fun line_pressure        : Pressure ; downstream line pressure (mmHg)
fun occlusion_threshold  : Pressure ; pressure triggering occlusion alarm (mmHg)

; === RELATIONS ===
rel over_infused    :  ; dose_delivered > prescribed_dose + dose_tolerance
rel occluded        :  ; line_pressure > occlusion_threshold
rel bolus_locked_out:  ; bolus_count >= max_bolus_per_hour

; === AXIOMS ===
axiom prescribed_val    : prescribed_dose = 100
axiom tolerance_val     : dose_tolerance = 5
axiom max_bolus_vol     : max_bolus_volume = 5
axiom max_rate_val      : max_primary_rate = 300
axiom kvo_rate_val      : kvo_rate = 1
axiom max_bolus_hr      : max_bolus_per_hour = 4
axiom occlusion_thresh  : occlusion_threshold = 300
axiom nn_dose           : dose_delivered >= 0
axiom nn_rate           : primary_rate >= 0
axiom nn_bolus_vol      : bolus_volume >= 0
axiom nn_pressure       : line_pressure >= 0
axiom nn_bolus_count    : bolus_count >= 0
axiom bolus_vol_bound   : bolus_volume <= max_bolus_volume
axiom rate_bound        : primary_rate <= max_primary_rate
axiom dose_monotone     : dose_delivered >= 0
axiom kvo_rate_lower    : kvo_rate >= 0
axiom over_infuse_def   : prescribed_dose + dose_tolerance = 105
```

---

## STEP 3 — Interpretations

### File: `assets/CS6_Pump/pt.interp`

```
; Firmware engineer perspective
IDLE              : (= dose_delivered 0)
PRIMING           : (= dose_delivered 0)
INFUSING          : (and (> primary_rate 0) (<= dose_delivered prescribed_dose + dose_tolerance))
BOLUS_DELIVERY    : (and (> bolus_volume 0) (<= bolus_volume max_bolus_volume))
PAUSED            : (<= dose_delivered prescribed_dose + dose_tolerance)
KVO_MODE          : (and (= primary_rate kvo_rate) (<= dose_delivered prescribed_dose + dose_tolerance))
ALARM_OCCLUSION   : (> line_pressure occlusion_threshold)
ALARM_OVERDOSE    : (> dose_delivered prescribed_dose + dose_tolerance)

prime_start!         : (= dose_delivered 0)
infusion_start!      : (and (> primary_rate 0) (<= primary_rate max_primary_rate))
bolus_request!       : (and (< bolus_count max_bolus_per_hour) (<= dose_delivered prescribed_dose))
bolus_complete!      : (and (<= bolus_volume max_bolus_volume) (<= dose_delivered prescribed_dose + dose_tolerance))
pause_infusion!      : (<= dose_delivered prescribed_dose + dose_tolerance)
resume_infusion!     : (<= dose_delivered prescribed_dose + dose_tolerance)
kvo_start!           : (<= dose_delivered prescribed_dose + dose_tolerance)
resume_from_kvo!     : (<= dose_delivered prescribed_dose + dose_tolerance)
occlusion_alarm!     : (> line_pressure occlusion_threshold)
occlusion_cleared!   : (<= line_pressure occlusion_threshold)
overdose_alarm!      : (> dose_delivered prescribed_dose + dose_tolerance)
alarm_acknowledged!  : (> dose_delivered prescribed_dose + dose_tolerance)
infusion_complete!   : (>= dose_delivered prescribed_dose)
```

### File: `assets/CS6_Pump/dt.interp`

```
; Clinical informatics / ICE perspective
ICE_DISCONNECTED  : (= dose_delivered 0)
ICE_PRIMING       : (= dose_delivered 0)
ICE_THERAPY_ACTIVE: (and (> primary_rate 0) (<= dose_delivered prescribed_dose + dose_tolerance))
ICE_BOLUS_ACTIVE  : (and (> bolus_volume 0) (<= bolus_volume max_bolus_volume))
ICE_THERAPY_PAUSED: (<= dose_delivered prescribed_dose + dose_tolerance)
ICE_KVO_MODE      : (and (= primary_rate kvo_rate) (<= dose_delivered prescribed_dose + dose_tolerance))
ICE_ALARM_OCCLUSION: (> line_pressure occlusion_threshold)
ICE_ALARM_OVERDOSE: (> dose_delivered prescribed_dose + dose_tolerance)

ice_device_connect!     : (= dose_delivered 0)
ice_therapy_start!      : (and (> primary_rate 0) (<= primary_rate max_primary_rate))
ice_bolus_request!      : (and (< bolus_count max_bolus_per_hour) (<= dose_delivered prescribed_dose))
ice_bolus_confirmed!    : (and (<= bolus_volume max_bolus_volume) (<= dose_delivered prescribed_dose + dose_tolerance))
ice_therapy_pause!      : (<= dose_delivered prescribed_dose + dose_tolerance)
ice_therapy_resume!     : (<= dose_delivered prescribed_dose + dose_tolerance)
ice_kvo_mode_event!     : (<= dose_delivered prescribed_dose + dose_tolerance)
ice_kvo_resume!         : (<= dose_delivered prescribed_dose + dose_tolerance)
ice_alarm_occlusion!    : (> line_pressure occlusion_threshold)
ice_alarm_cleared!      : (<= line_pressure occlusion_threshold)
ice_safety_limit_breach!: (> dose_delivered prescribed_dose + dose_tolerance)
ice_alarm_ack!          : (> dose_delivered prescribed_dose + dose_tolerance)
ice_therapy_complete!   : (>= dose_delivered prescribed_dose)
```

---

## STEP 4 — Misalignment Variants

### Variant A — Missing Event: Early Warning Gap

Copy `dt.interp` as `dt_missing_warning.interp`. Remove `ice_safety_limit_breach!`
entirely. The DT has no semantic mapping for the overdose alarm — the most
critical safety event. SemAlign must report this as an unmatched DT label.

### Variant B — Threshold Drift: Dose Tolerance Gap

`dt_thresh_drift.interp`: Change `ice_safety_limit_breach!` to fire at
`dose_delivered > 110` instead of `> prescribed_dose + dose_tolerance (= 105)`.
The 5ml gap represents a patient receiving up to 10% excess dose before the
monitoring twin detects it — clinically significant for opioid infusions.

### Variant C — Ontology Evolution (RQ1)

`domain_v2.ont`: Add `fun drug_concentration : Volume` (mg/ml — drug
concentration for dual-channel pumps per IEC 60601-2-24:2012).
Tighten `dose_tolerance` from 5 to 3 ml (FDA tightened guidance 2023
for high-alert medications).

---

## STEP 5 — Benchmark Runner

### File: `benchmark/run_CS6.cpp`

Follow the structure of `benchmark/run_CS1.cpp`.