
# Prompt CS9 — Precision Irrigation System (Author-Constructed)

## Prerequisites

Prompts 0–CS8 must be fully implemented.

---

## Background and Domain

Author-constructed case study for the smart farming domain. Ontology is
grounded in ISO 11783 (ISOBUS) and FAO-56. Document the ontology
construction methodology prominently: (1) identify governing standard,
(2) extract vocabulary from data dictionary, (3) formalize normative
requirement clauses as FOL axioms.

Governing standards:
- **ISO 11783-7:2015** — Tractors and Machinery for Agriculture and
  Forestry — Serial Control and Communications Data Network (ISOBUS).
  Part 7: Implement Messages Application Layer:
  - Defines process data identifiers for soil moisture, flow meters,
    section control — used to derive function symbols
  - Timing requirement: ISOBUS command latency ≤3 time units
  - Section control: minimum response time ≤1 time unit
- **ISO 11783-10:2015** — Task Controller and Management Information
  System Data Interchange. Defines the data format for prescription
  maps — source for zone-specific application rates.
- **FAO Irrigation and Drainage Paper 56 (1998)** — Crop
  Evapotranspiration: Guidelines for Computing Crop Water Requirements.
  The definitive international standard for irrigation scheduling:
  - Penman-Monteith equation: ETo = reference evapotranspiration
  - ETc = ETo × Kc (crop coefficient) — actual crop evapotranspiration
  - Irrigation requirement: IR = ETc - effective rainfall - soil storage
  - Depletion threshold: irrigate when soil moisture < (1 - p) × TAW
    where p = depletion fraction, TAW = Total Available Water
- **EN 13101:2003** — Steps for underground tanks. Used for underground
  irrigation reservoir capacity constraints.
- **EU Regulation 2009/128/EC** — Sustainable Use of Pesticides
  Directive. While primarily for pesticides, Article 12 covers
  precision application timing requirements used for spray inhibit
  logic in irrigation control (wind speed limits, buffer zones).

The **PT view** models the physical irrigation controller from the
*precision agriculture engineer* perspective: zone valve commands,
sensor threshold crossings, and ISOBUS section control events.

The **DT view** models a **crop water budget monitoring twin** authored
by the *agronomist* using FAO-56 evapotranspiration vocabulary rather
than ISOBUS command vocabulary.

---

## STEP 1 — UPPAAL Models

### File: `assets/CS9_Irrigation/V1_PT.xml`

Template name: `IrrigationControllerPT`

**Clocks:** `t_irrigate`, `t_monitor`, `t_dry`, `t_zone`, `t_sensor`

**Locations (8 total):**
```
MONITORING        — soil sensors active, no irrigation active
IRRIGATION_ZONE_1 — Zone 1 valve open, irrigating
IRRIGATION_ZONE_2 — Zone 2 valve open, irrigating
IRRIGATION_ZONE_3 — Zone 3 valve open, irrigating
DRAINING          — all valves closed, drainage phase
ALARM_DEFICIT     — water deficit exceeds FAO-56 depletion threshold
ALARM_OVERFLOW    — soil moisture > field capacity: irrigation stopped
MAINTENANCE       — system maintenance mode (ISO 11783 diagnostic)
```

**Transitions:**

```
MONITORING -> IRRIGATION_ZONE_1
  trigger: irrigation_start_z1!
  guard:   t_dry >= 24 && t_sensor <= 3
           [FAO-56: irrigate when deficit threshold reached after 24 dry units;
            ISO 11783-7: sensor response ≤3 time units]
  reset:   t_zone := 0, t_irrigate := 0

IRRIGATION_ZONE_1 -> MONITORING
  trigger: irrigation_complete!
  guard:   t_zone >= 10 && t_zone <= 60
           [ISO 11783-10 prescription: zone 1 irrigation window 10–60 units]
  invariant: t_zone <= 60, reset: t_dry := 0

MONITORING -> IRRIGATION_ZONE_2
  trigger: irrigation_start_z2!
  guard:   t_dry >= 24 && t_sensor <= 3
  reset:   t_zone := 0

IRRIGATION_ZONE_2 -> MONITORING
  trigger: irrigation_complete!
  guard:   t_zone >= 8 && t_zone <= 45
  invariant: t_zone <= 45, reset: t_dry := 0

MONITORING -> IRRIGATION_ZONE_3
  trigger: irrigation_start_z3!
  guard:   t_dry >= 24 && t_sensor <= 3
  reset:   t_zone := 0

IRRIGATION_ZONE_3 -> MONITORING
  trigger: irrigation_complete!
  guard:   t_zone >= 6 && t_zone <= 30
  invariant: t_zone <= 30, reset: t_dry := 0

ANY -> DRAINING
  trigger: drain_start!
  guard:   t_irrigate >= 0
  (from any zone irrigation state), reset: t_monitor := 0

DRAINING -> MONITORING
  trigger: tau
  guard:   t_monitor >= 3 && t_monitor <= 15, invariant: t_monitor <= 15

ANY -> ALARM_DEFICIT
  trigger: deficit_alarm!
  guard:   t_dry >= 48
           [FAO-56: critical deficit declared after 48 dry time units]
  (from MONITORING)

ALARM_DEFICIT -> IRRIGATION_ZONE_1
  trigger: emergency_irrigation!
  guard:   (no guard), reset: t_zone := 0

ANY -> ALARM_OVERFLOW
  trigger: overflow_alarm!
  guard:   t_sensor <= 1
           [ISO 11783-7: overflow sensor response ≤1 time unit]
  (from any zone state)

ALARM_OVERFLOW -> MONITORING
  trigger: overflow_cleared!
  guard:   t_sensor >= 2

MONITORING -> MAINTENANCE
  trigger: maintenance_start!
  reset:   t_monitor := 0

MAINTENANCE -> MONITORING
  trigger: maintenance_complete!
  guard:   t_monitor >= 10 && t_monitor <= 60
  invariant: t_monitor <= 60
```

### File: `assets/CS9_Irrigation/V2_DT.xml`

Template name: `CropWaterBudgetDT`

**Label mapping intent (FAO-56 agronomist vocabulary):**
```
PT label                  DT label
irrigation_start_z1!   --> water_application_zone1!
irrigation_start_z2!   --> water_application_zone2!
irrigation_start_z3!   --> water_application_zone3!
irrigation_complete!   --> water_application_event!
drain_start!           --> drainage_cycle_start!
deficit_alarm!         --> deficit_threshold_breach!
emergency_irrigation!  --> emergency_water_application!
overflow_alarm!        --> field_capacity_exceeded!
overflow_cleared!      --> soil_moisture_nominal!
maintenance_start!     --> system_check_start!
maintenance_complete!  --> system_check_complete!
```

**Clocks:** `t_et`, `t_budget`, `t_deficit`, `t_dt_zone`

**Locations (7 total):**
```
DT_MONITORING, DT_WATER_APPLICATION_Z1, DT_WATER_APPLICATION_Z2,
DT_WATER_APPLICATION_Z3, DT_DRAINAGE, DT_DEFICIT_ALERT, DT_OVERFLOW_ALERT
```

---

## STEP 2 — Ontology

### File: `assets/CS9_Irrigation/domain.ont`

```
; Ontology: Precision Irrigation System — Crop Water Budget Domain
; Standards: ISO 11783-7:2015, ISO 11783-10:2015, FAO-56:1998,
;            EN 13101:2003, EU Regulation 2009/128/EC
; Fragment: QF_LRA

; === SORTS ===
sort Volume_mm    ; mm water depth — FAO-56 standard unit
sort ZoneIndex    ; integer 1..3
sort CropCoeff    ; dimensionless — FAO-56 crop coefficient Kc
sort Percentage   ; 0-100 — depletion percentage

; === FUNCTIONS ===
; Soil water balance (FAO-56 Chapter 1)
fun soil_moisture        : ZoneIndex -> Volume_mm  ; current soil moisture (mm)
fun field_capacity       : ZoneIndex -> Volume_mm  ; maximum soil water holding capacity (mm)
fun wilting_point        : ZoneIndex -> Volume_mm  ; permanent wilting point (mm)
fun taw                  : ZoneIndex -> Volume_mm  ; Total Available Water = FC - WP (mm)
fun raw                  : ZoneIndex -> Volume_mm  ; Readily Available Water = p * TAW
fun depletion_fraction   : Percentage              ; FAO-56 p: fraction of TAW before stress
fun irrigation_applied   : ZoneIndex -> Volume_mm  ; water applied to zone i (mm)

; Evapotranspiration (FAO-56 Penman-Monteith, Chapter 2)
fun et_reference         : Volume_mm  ; ETo: reference evapotranspiration (mm/day)
fun kc_initial           : CropCoeff  ; Kc initial growth stage
fun kc_mid               : CropCoeff  ; Kc mid-season (peak demand)
fun kc_end               : CropCoeff  ; Kc late season
fun kc_current           : CropCoeff  ; current crop coefficient
fun et_crop              : Volume_mm  ; ETc = ETo * Kc (mm/day)

; Water budget
fun irrigation_deficit   : Volume_mm  ; ETc - effective_rainfall - irrigation_applied_total
fun effective_rainfall   : Volume_mm  ; rainfall contributing to soil water (mm)
fun irrigation_applied_total: Volume_mm; sum over all zones

; ISO 11783 application parameters
fun max_application_rate : Volume_mm  ; ISO 11783-10 prescription map max rate
fun min_application_per_zone: Volume_mm; minimum effective application per zone

; === RELATIONS ===
rel soil_stress          : ZoneIndex  ; soil_moisture(i) < raw(i) — water stress
rel field_saturated      : ZoneIndex  ; soil_moisture(i) >= field_capacity(i)
rel deficit_critical     :            ; irrigation_deficit > raw(1) — system-wide

; === AXIOMS ===
; --- ISO 11783-10 zone parameters (prescription map values) ---
axiom fc_z1             : field_capacity(1) = 40
axiom fc_z2             : field_capacity(2) = 35
axiom fc_z3             : field_capacity(3) = 30
axiom wp_z1             : wilting_point(1) = 10
axiom wp_z2             : wilting_point(2) = 8
axiom wp_z3             : wilting_point(3) = 7

; --- FAO-56 derived quantities ---
axiom taw_z1            : taw(1) = 30
axiom taw_z2            : taw(2) = 27
axiom taw_z3            : taw(3) = 23
axiom depletion_val     : depletion_fraction = 50
axiom raw_z1            : raw(1) = 15
axiom raw_z2            : raw(2) = 13
axiom raw_z3            : raw(3) = 11

; --- FAO-56 crop coefficients (wheat, mid-season per FAO-56 Table 12) ---
axiom kc_init_val       : kc_initial = 0
axiom kc_mid_val        : kc_mid = 1
axiom kc_end_val        : kc_end = 0
axiom kc_current_bound  : kc_current >= kc_initial
axiom kc_current_upper  : kc_current <= kc_mid + 0

; --- Non-negativity ---
axiom nn_sm_1           : soil_moisture(1) >= 0
axiom nn_sm_2           : soil_moisture(2) >= 0
axiom nn_sm_3           : soil_moisture(3) >= 0
axiom nn_et             : et_reference >= 0
axiom nn_etc            : et_crop >= 0
axiom nn_deficit        : irrigation_deficit >= -10
axiom nn_rain           : effective_rainfall >= 0

; --- Soil moisture bounds ---
axiom sm_upper_1        : soil_moisture(1) <= field_capacity(1)
axiom sm_upper_2        : soil_moisture(2) <= field_capacity(2)
axiom sm_upper_3        : soil_moisture(3) <= field_capacity(3)

; --- ETc definition (FAO-56 Eq.1) ---
axiom etc_def           : et_crop = et_reference * kc_current
; Linearised: et_crop = et_reference (when kc_current = 1, mid-season)

; --- Water budget ---
axiom deficit_def       : irrigation_deficit = et_crop - effective_rainfall - irrigation_applied_total
axiom appl_total_bound  : irrigation_applied_total >= 0

; --- ISO 11783 application limits ---
axiom max_app_rate      : max_application_rate = 20
axiom min_app_zone      : min_application_per_zone = 5
```

---

## STEP 3 — Interpretations

### File: `assets/CS9_Irrigation/pt.interp`

```
; ISOBUS / precision agriculture engineer perspective
MONITORING        : (and (>= soil_moisture(1) wilting_point(1)) (>= soil_moisture(2) wilting_point(2)) (>= soil_moisture(3) wilting_point(3)))
IRRIGATION_ZONE_1 : (and (< soil_moisture(1) raw(1)) (>= irrigation_applied(1) 0))
IRRIGATION_ZONE_2 : (and (< soil_moisture(2) raw(2)) (>= irrigation_applied(2) 0))
IRRIGATION_ZONE_3 : (and (< soil_moisture(3) raw(3)) (>= irrigation_applied(3) 0))
DRAINING          : (>= soil_moisture(1) 0)
ALARM_DEFICIT     : (> irrigation_deficit raw(1))
ALARM_OVERFLOW    : (>= soil_moisture(1) field_capacity(1))
MAINTENANCE       : (>= soil_moisture(1) 0)

irrigation_start_z1!   : (and (< soil_moisture(1) raw(1)) (<= et_crop max_application_rate))
irrigation_start_z2!   : (and (< soil_moisture(2) raw(2)) (<= et_crop max_application_rate))
irrigation_start_z3!   : (and (< soil_moisture(3) raw(3)) (<= et_crop max_application_rate))
irrigation_complete!   : (and (>= irrigation_applied(1) min_application_per_zone) (<= soil_moisture(1) field_capacity(1)))
drain_start!           : (>= soil_moisture(1) 0)
deficit_alarm!         : (> irrigation_deficit raw(1))
emergency_irrigation!  : (> irrigation_deficit raw(1))
overflow_alarm!        : (>= soil_moisture(1) field_capacity(1))
overflow_cleared!      : (< soil_moisture(1) field_capacity(1))
maintenance_start!     : (>= soil_moisture(1) 0)
maintenance_complete!  : (>= soil_moisture(1) 0)
```

### File: `assets/CS9_Irrigation/dt.interp`

```
; Agronomist / FAO-56 water budget perspective
DT_MONITORING           : (and (>= soil_moisture(1) wilting_point(1)) (>= soil_moisture(2) wilting_point(2)) (>= soil_moisture(3) wilting_point(3)))
DT_WATER_APPLICATION_Z1 : (and (< soil_moisture(1) raw(1)) (>= irrigation_applied(1) 0))
DT_WATER_APPLICATION_Z2 : (and (< soil_moisture(2) raw(2)) (>= irrigation_applied(2) 0))
DT_WATER_APPLICATION_Z3 : (and (< soil_moisture(3) raw(3)) (>= irrigation_applied(3) 0))
DT_DRAINAGE             : (>= soil_moisture(1) 0)
DT_DEFICIT_ALERT        : (> irrigation_deficit raw(1))
DT_OVERFLOW_ALERT       : (>= soil_moisture(1) field_capacity(1))

water_application_zone1! : (and (< soil_moisture(1) raw(1)) (<= et_crop max_application_rate))
water_application_zone2! : (and (< soil_moisture(2) raw(2)) (<= et_crop max_application_rate))
water_application_zone3! : (and (< soil_moisture(3) raw(3)) (<= et_crop max_application_rate))
water_application_event! : (and (>= irrigation_applied(1) min_application_per_zone) (<= soil_moisture(1) field_capacity(1)))
drainage_cycle_start!    : (>= soil_moisture(1) 0)
deficit_threshold_breach!: (> irrigation_deficit raw(1))
emergency_water_application!: (> irrigation_deficit raw(1))
field_capacity_exceeded! : (>= soil_moisture(1) field_capacity(1))
soil_moisture_nominal!   : (< soil_moisture(1) field_capacity(1))
system_check_start!      : (>= soil_moisture(1) 0)
system_check_complete!   : (>= soil_moisture(1) 0)
```

---

## STEP 4 — Misalignment Variants

### Variant A — Threshold Drift: FAO-56 Deficit Gap
`dt_thresh_drift.interp`: Change `deficit_threshold_breach!` to fire at
`irrigation_deficit > 20` instead of `> raw(1) (= 15)`. The 5mm gap
represents the water stress zone where crops experience yield penalty
but the DT monitoring twin fails to trigger irrigation.

### Variant B — Ontology Evolution (RQ1)
`domain_v2.ont`: Add `fun rainfall_forecast : Volume_mm` and
`axiom rainfall_nonneg : rainfall_forecast >= 0` (vocabulary extension).
Tighten `depletion_fraction` from 50 to 45% (FAO-56 tighter water
management for drought conditions — axiom strengthening). Update
`raw` axioms accordingly. Create `pt_v2.interp` and `dt_v2.interp`.

---

## STEP 5 — Benchmark Runner

### File: `benchmark/run_CS9.cpp`

Follow the structure of `benchmark/run_CS1.cpp`. Ontology evolution
test is particularly important here — report evolution from v1 to v2
in detail for RQ1.

---

---
