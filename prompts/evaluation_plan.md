

## Evaluation Plan

### Overview

The evaluation has four research questions, 10 case studies across four domains and three independent external sources, a tool implementation (SemAlign), and two baseline comparisons. Every theorem in the paper has a corresponding RQ.

---

### Benchmark Suite

| # | System | Source | Domain Standard |
|---|---|---|---|
| CS1 | Three-Tank | Gil et al. | IEC 61511 |
| CS2 | Crane/Elevator | Kamburjan et al. | IEC 60204 |
| CS3 | Inspection Rover | NASA/FRET | NASA NPR 7150.2 |
| CS4 | Aircraft Engine | NASA/FRET + LMCPS | DO-178C |
| CS5 | Autonomous Grasping | NASA/FRET | ECSS-E-ST-40C |
| CS6 | Infusion Pump | NASA/FRET | IEC 62443 |
| CS7 | Autopilot FSM | LMCPS | DO-178C/ARP4754A |
| CS8 | Lift Plus Cruise eVTOL | NASA/FRET + LMCPS | ASTM F3269 |
| CS9 | Precision Irrigation | Author-constructed | ISO 11783 / FAO-56 |
| CS10 | Pesticide Spraying Drone | Author-constructed | EPPO / EU 2009/128 |

Three independent external sources (DT literature, NASA/FRET, Lockheed Martin), four domains (industrial process, aerospace/avionics, space/robotics, smart farming), all ontologies derived from named industrial standards. CS9 and CS10 are author-constructed but standard-grounded; documented as such in threats to validity.

---

### Tool: SemAlign

Implemented in C++17 on top of the existing RTWBS codebase. Takes as input two UPPAAL TA files (PT view, DT view), an ontology file (.ont DSL), and two interpretation files (.interp). Outputs: alignment verdict, counterexample trace if not aligned, alignment relation size, SMT call count, and wall-clock time per phase. Z3 C++ API for SMT entailment checks. Submitted to ICSE Artifact Evaluation track.

Two modes:
- **Semantic mode:** full Algorithm 1 with ontological label matching
- **Syntactic mode:** degenerate bisimulation with label equality (RQ2 baseline)

---

### RQ1 — Does Alignment Survive Ontology Evolution?

**Validates:** Theorem 1 (conservative refinement) and Corollaries

**Case studies:** CS1, CS4, CS9, CS10

**What we do:** For each case study, start with the base ontology $\Phi$ and establish alignment. Then apply three types of refinement in sequence:
- **Type A — Vocabulary extension** (Condition I only): add new sort and function, interpretations unchanged. Expected: alignment preserved for free by Corollary 1, zero incremental SMT calls.
- **Type B — Axiom strengthening** (Conditions I+II): tighten a numerical bound in the axioms, interpretations unchanged. Expected: alignment preserved, incremental recheck only recomputes entailments involving changed axioms.
- **Type C — Interpretation tightening** (all three conditions): update an event interpretation to reflect tighter domain knowledge. Expected: alignment may require incremental recheck of affected label pairs only, per Corollary 2.

**What we report:**

| Case Study | Refinement Type | Preserved | Full Check Time (ms) | Incremental Time (ms) | Speedup | SMT Calls (incremental) |
|---|---|---|---|---|---|---|
| CS1 | Vocab extension | ✓ | — | — | — | 0 |
| CS1 | Axiom strengthening | ✓/✗ | X | Y | X/Y | Z |
| ... | ... | ... | ... | ... | ... | ... |

Also report one case where refinement correctly *breaks* alignment — showing the framework detects invalidation, not just preservation.

**Key metric:** incremental recheck time as a fraction of full re-verification time. If this is consistently <10%, it demonstrates that iterative development does not restart verification from scratch.

---

### RQ2 — What Misalignment Classes Do Existing Approaches Miss?

**Validates:** Theorems 2, 3, 4

**Case studies:** CS1, CS2, CS5, CS9, CS10

**What we do:** For each case study, inject misalignments covering five distinct classes:

| Class | Description | Example |
|---|---|---|
| Threshold drift | Numerical safety bound differs between PT and DT interpretations | DT uses wrong EGT limit |
| Label renaming | DT label has no ontological mapping | Emergency stop event orphaned |
| Missing event | DT has no interpretation for a safety-critical PT event | Flame-out unhandled |
| Timing violation | DT event timing window violates domain constraint | Telemetry latency exceeds spec |
| Ontology gap | DT introduces precondition not in shared ontology | Well-clear condition added unilaterally |

Run three checkers on each misaligned variant:
1. **SemAlign (semantic):** full Algorithm 1
2. **SemAlign (syntactic):** degenerate bisimulation, no SMT
3. **Runtime monitor baseline:** for CS5, the published Oakes et al. MODELS 2024 monitors; for CS1/CS2, a simple trace-based monitor checking the violated property

**What we report — Detection matrix:**

| Misalignment | CS | SemAlign | Syntactic Bisim | Runtime Monitor | Traces to Detect |
|---|---|---|---|---|---|
| Threshold drift | CS1 | ✓ | ✗ | ✓/✗ | N or never |
| Label renaming | CS2 | ✓ | ✗ | ✗ | never |
| Timing violation | CS3 | ✓ | ✗ | ✗ | never |
| ... | ... | ... | ... | ... | ... |

The timing violation row is the strongest result — it is structurally undetectable by state-based runtime monitors because it requires cross-model clock comparison. Only bisimulation-based alignment catches it via delay Condition IV.

For CS5 specifically, the runtime monitor baseline is peer-reviewed (Oakes et al.), so the comparison is not self-serving. Mark the monitor source in the table.

---

### RQ3 — How Does SemAlign Scale, and What Does Compositional Verification Buy?

**Validates:** Algorithm 1 termination and complexity; federated scalability claim

**Case studies:** All 10 for scalability; CS4 and CS7 for compositionality

**What we do:**

*Scalability:* Run SemAlign on all 10 case studies and record: PT zone states, DT zone states, total zone states, SMT calls for label equivalence precomputation, SMT calls for state seeding, fixpoint iterations, time per phase, total wall-clock time.

*Compositionality:* For CS4 (2 subsystems: FMS + TMS) and CS7 (3 subsystems: NOS + MES + EHS), run:
- Monolithic check: full PT vs full DT
- Compositional check: check each subsystem pair independently, then compose

**What we report:**

Scalability table (all 10 case studies) + scatter plot of total zone states vs. wall-clock time with trend line.

Compositionality table:

| Case Study | Subsystems | Monolithic Time (ms) | Compositional Time (ms) | Speedup |
|---|---|---|---|---|
| CS4 | 2 | X | Y | X/Y |
| CS7 | 3 | X | Y | X/Y |

**Key claim:** compositional verification time scales linearly with subsystem count rather than exponentially with total state space, directly supporting the federated Gaia-X claim.

---

### RQ4 — What Is the Engineering Cost of SemAlign vs. Runtime Monitoring?

**Validates:** Theorem 4 (runtime subsumption); practical adoption argument

**Case studies:** CS1, CS5, CS7

**What we do:** For each case study, measure and compare the total engineering cost of achieving equivalent consistency guarantees via (a) SemAlign and (b) runtime monitoring.

SemAlign cost:
- One-time ontology specification (already required for Gaia-X compliance — zero additive cost)
- One-time Algorithm 1 execution time (from RQ3)
- Incremental recheck time per evolution step (from RQ1)

Runtime monitoring cost:
- Number of properties that need to be individually instrumented to cover $\mathrm{dom}(\lambda)$
- Estimated monitor implementation: lines of code per property (calibrated from Oakes et al. CS5 monitors)
- Runtime overhead per execution step (measured from monitor simulation)
- Re-instrumentation cost per evolution step (estimated as full LOC per changed property)

**What we report:**

| | CS1 | CS5 | CS7 |
|---|---|---|---|
| $\|\mathrm{dom}(\lambda)\|$ (properties covered) | | | |
| Monitor LOC to cover same fragment | | | |
| Runtime overhead per step (μs) | | | |
| SemAlign one-time cost (ms) | | | |
| Amortization point (steps) | | | |
| Re-instrumentation per evolution step (LOC) | | | |
| SemAlign incremental recheck (ms) | | | |

**Key metric:** amortization point = SemAlign cost / overhead per step. After this many execution steps, SemAlign has paid for itself relative to monitoring overhead. For any non-trivial deployment lifetime this number is reached quickly.

**Key narrative:** the ontology is already required for Gaia-X compliance. The interpretations are already maintained informally in MBSE documentation. SemAlign's one-time cost is therefore the only additive engineering effort — against indefinite runtime monitoring overhead that compounds with every evolution step.

---

### Threats to Validity

**Internal validity:**
- CS9 and CS10 are author-constructed; mitigated by standard-derived ontologies with explicit standard citations
- Misalignment variants designed to cover distinct classes, not to favor SemAlign
- Runtime monitor simulations use fixed random seed 42 for reproducibility
- Monitor LOC estimates calibrated from published implementation (Oakes et al.)

**External validity:**
- 10 case studies across 4 domains and 3 independent external sources
- Ontologies derived from 10 distinct industrial standards
- CS5 runtime monitor baseline from peer-reviewed published work — not self-constructed
- Lockheed Martin and NASA benchmarks are the most widely cited CPS verification benchmarks

**Construct validity:**
- Alignment relation size and SMT call count are proxy metrics — reported alongside direct timing measurements
- Amortization point is an estimate based on measured overhead; actual deployment cost depends on system execution rate