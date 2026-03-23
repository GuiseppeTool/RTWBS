#!/usr/bin/env python3
"""
simulation.py — Discrete-event simulation of DA_abstract and DA_refined
for the Sugar Beet Monitoring Fleet case study (EMSOFT).

Produces:
  - trace_comparison.pdf   (Plot 1: event trace timeline)
  - energy_comparison.pdf  (Plot 2: energy consumption)

All timing values are formal clock values from the Timed Automata models.
"""

import matplotlib
matplotlib.use("Agg")  # non-interactive backend for PDF output
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
import os

# ──────────────────────────────────────────────────────────────────────
# 1.  TIMING PARAMETERS  (consistent with DA_abstract.xml / DA_refined.xml)
# ──────────────────────────────────────────────────────────────────────
T_DETECT      = 0      # anomaly detection at t = 0 ms (by convention)
T_SND_WAYPOINT = 10    # FC sends waypoint to drone at t = 10 ms
T_DUTY_CYCLE  = 40     # duty-cycle period in DA_refined (ms)
T_PROC        = 5      # processing duration (ms), guard x >= 5
T_FC_PROCESS  = 435    # FC pipeline: anomaly classification + treatment
                        # planning + safety validation against digital twin
DEADLINE      = 500    # contractual treatment-command deadline (ms)
SIM_WINDOW    = 550    # simulation window (ms)

# ──────────────────────────────────────────────────────────────────────
# 2.  SIMULATION — compute event timestamps
# ──────────────────────────────────────────────────────────────────────

class DroneSimulation:
    """Formal trace simulation for one DA variant."""

    def __init__(self, name, duty_cycle_delay=0):
        self.name = name
        self.duty_delay = duty_cycle_delay
        self.events = []       # list of (time_ms, label, detail)

    def run(self):
        t = T_DETECT
        self.events.append((t, "anomaly_detected", "FC detects anomaly"))

        # FC sends waypoint
        t_snd_wp = T_SND_WAYPOINT
        self.events.append((t_snd_wp, "snd_waypoint", "FC → DA"))

        # DA receives waypoint (same physical arrival time in both systems)
        t_rcv_wp = t_snd_wp  # message arrives at t = 10 ms in both
        self.events.append((t_rcv_wp, "rcv_waypoint_arrival", "DA buffer"))

        # DA processes waypoint (immediate vs. duty-cycled)
        t_process_start = t_rcv_wp + self.duty_delay
        self.events.append((t_process_start, "rcv_waypoint", "DA processes"))

        # Processing completes
        t_process_end = t_process_start + T_PROC
        self.events.append((t_process_end, "processing_done", "DA internal"))

        # DA sends anomaly report
        t_snd_report = t_process_end
        self.events.append((t_snd_report, "snd_report", "DA → FC"))

        # FC receives report and issues treatment command
        t_treatment = t_snd_report + T_FC_PROCESS
        self.events.append((t_treatment, "snd_treatment", "FC → GR"))

        self.t_snd_report  = t_snd_report
        self.t_treatment   = t_treatment
        return self


# Run both simulations
sim_abs = DroneSimulation("DA_abstract", duty_cycle_delay=0).run()
sim_ref = DroneSimulation("DA_refined",  duty_cycle_delay=T_DUTY_CYCLE).run()

print("=== DA_abstract event trace ===")
for t, lbl, det in sim_abs.events:
    print(f"  t={t:>4d} ms  {lbl:<25s}  {det}")

print(f"\n=== DA_refined event trace ===")
for t, lbl, det in sim_ref.events:
    print(f"  t={t:>4d} ms  {lbl:<25s}  {det}")

print(f"\n--- Summary ---")
print(f"  DA_abstract  snd_report @ {sim_abs.t_snd_report} ms,  "
      f"snd_treatment @ {sim_abs.t_treatment} ms  "
      f"({'OK' if sim_abs.t_treatment <= DEADLINE else 'FAIL'})")
print(f"  DA_refined   snd_report @ {sim_ref.t_snd_report} ms,  "
      f"snd_treatment @ {sim_ref.t_treatment} ms  "
      f"({'OK' if sim_ref.t_treatment <= DEADLINE else 'FAIL'})")

# ──────────────────────────────────────────────────────────────────────
# 3.  ENERGY MODEL  (Cortex-M4 class drone MCU, CPU power only)
#     Radio power is identical in both systems and not modelled.
# ──────────────────────────────────────────────────────────────────────
# Power states based on STM32L4 @ 3.3 V:
#   Abstract idle = Run @ 16 MHz + radio RX always armed:
#     CPU ~9 mA + radio RX ~2 mA ≈ 11 mA → ~36 mW
#   Refined sleep = Stop 1 (RTC) + radio quiescent + crystal + RAM:
#     CPU ~1.2 mA + radio standby ~3 mA + TCXO ~2.5 mA ≈ 6.7 mA → ~22 mW
#   Active = Run @ 80 MHz + radio TX/RX burst:
#     CPU ~12 mA → ~40 mW  (same for both)
POWER_IDLE_MW    = 36.0   # Abstract baseline: CPU run + radio RX always armed
POWER_SLEEP_MW   = 22.0   # Refined baseline: Stop 1 + radio/TCXO quiescent
POWER_ACTIVE_MW  = 40.0   # Both: CPU active — processing data or duty-cycle check
DUTY_WAKE_MS     =  8.0   # Duration of each duty-cycle wake window (20% of 40 ms)
TX_BURST_MS      =  2.0   # Duration of report TX burst


def energy_profile_abstract(t_array, sim):
    """DA_abstract: interrupt-driven idle + active bursts during events."""
    power = np.full_like(t_array, POWER_IDLE_MW, dtype=float)
    # Active during processing (rcv_waypoint → snd_report + TX)
    t_proc_start = next(t for t, l, _ in sim.events if l == "rcv_waypoint")
    t_proc_end   = next(t for t, l, _ in sim.events if l == "snd_report")
    mask = (t_array >= t_proc_start) & (t_array < t_proc_end + TX_BURST_MS)
    power[mask] = POWER_ACTIVE_MW
    return power


def energy_profile_refined(t_array, sim):
    """DA_refined: deep sleep + periodic duty-cycle wakes + active burst."""
    power = np.full_like(t_array, POWER_SLEEP_MW, dtype=float)
    # Periodic duty-cycle wake checks
    t_proc_start = next(t for t, l, _ in sim.events if l == "rcv_waypoint")
    t_proc_end   = next(t for t, l, _ in sim.events if l == "snd_report")
    for t_wake in np.arange(0, SIM_WINDOW, T_DUTY_CYCLE):
        # Skip if overlapping with the processing burst
        if t_wake >= t_proc_start and t_wake < t_proc_end + TX_BURST_MS:
            continue
        mask = (t_array >= t_wake) & (t_array < t_wake + DUTY_WAKE_MS)
        power[mask] = POWER_ACTIVE_MW
    # Active during processing (same computation as abstract)
    mask = (t_array >= t_proc_start) & (t_array < t_proc_end + TX_BURST_MS)
    power[mask] = POWER_ACTIVE_MW
    return power


dt = 0.1  # time resolution (ms)
t = np.arange(0, SIM_WINDOW, dt)

power_abs = energy_profile_abstract(t, sim_abs)
power_ref = energy_profile_refined(t, sim_ref)

energy_abs_mJ = np.trapezoid(power_abs, t) / 1000.0  # mW·ms → mJ
energy_ref_mJ = np.trapezoid(power_ref, t) / 1000.0
energy_saving_pct = (1.0 - energy_ref_mJ / energy_abs_mJ) * 100.0

# ── Per-state energy breakdown ──
def energy_breakdown(label, t_arr, power_arr):
    """Print how much time/energy is spent in each power state."""
    unique_levels = sorted(set(power_arr))
    print(f"\n  [{label}] per-state breakdown:")
    total_mj = 0.0
    for pw in unique_levels:
        mask = power_arr == pw
        ms_in_state = np.sum(mask) * dt
        mj = pw * ms_in_state / 1000.0
        total_mj += mj
        print(f"    {pw:6.1f} mW : {ms_in_state:7.1f} ms → {mj/3600:.6f} mWh")
    print(f"    {'TOTAL':>8s} : {np.sum(np.ones_like(t_arr[power_arr>-1])) * dt:7.1f} ms → {total_mj/3600:.6f} mWh")

energy_breakdown("DA_abstract", t, power_abs)
energy_breakdown("DA_refined",  t, power_ref)

print(f"\n--- Energy ---")
print(f"  DA_abstract: {energy_abs_mJ/3600:.5f} mWh")
print(f"  DA_refined:  {energy_ref_mJ/3600:.5f} mWh")
print(f"  Saving:      {energy_saving_pct:.1f}%")

# ──────────────────────────────────────────────────────────────────────
# 4.  PLOT STYLING  (IEEE-appropriate: serif, clean, no chartjunk)
# ──────────────────────────────────────────────────────────────────────
plt.rcParams.update({
    "font.family":        "serif",
    "font.size":          8.5,
    "axes.linewidth":     0.5,
    "xtick.major.width":  0.4,
    "ytick.major.width":  0.4,
    "xtick.direction":    "in",
    "ytick.direction":    "in",
    "lines.linewidth":    0.8,
    "text.usetex":        False,
    "figure.dpi":         300,
    "axes.spines.top":    False,
    "axes.spines.right":  False,
})

COLOR_ABS   =  "gray" #gray  ##"#1F77B4"   # muted blue
COLOR_REF   =  "black" ##"#D95F02"   # muted orange
COLOR_DEAD  = "#B03030"   # dark red
COLOR_GRID  = "#D0D0D0"

OUT_DIR = os.path.dirname(os.path.abspath(__file__))


# ──────────────────────────────────────────────────────────────────────
# 5.  PLOT 1 — Event Trace Comparison  (broken axis: two regions)
# ──────────────────────────────────────────────────────────────────────
from matplotlib.lines import Line2D

fig1, (ax_l, ax_r) = plt.subplots(
    1, 2, sharey=True, figsize=(7.0, 2.4),
    gridspec_kw={"width_ratios": [1.0, 1.0], "wspace": 0.10},
)

lane_abs = 1.0
lane_ref = 0.0
BAR_H = 0.14  # half-height of processing bars

# Panel limits — left: early events; right: deadline region
ax_l.set_xlim(-12, 75)
ax_r.set_xlim(425, 518)

for ax in (ax_l, ax_r):
    ax.set_ylim(-0.70, 1.85)
    ax.tick_params(axis="x", labelsize=7)

# Thin lane baseline
for ax in (ax_l, ax_r):
    for y in (lane_abs, lane_ref):
        ax.axhline(y, color=COLOR_GRID, linewidth=0.3, zorder=0)

# Break marks — hide inner spines, draw diagonal slashes
ax_l.spines["right"].set_visible(False)
ax_r.spines["left"].set_visible(False)
ax_r.tick_params(left=False)

brk = 0.015
for ax, side in [(ax_l, "right"), (ax_r, "left")]:
    kw = dict(transform=ax.transAxes, color="0.45", clip_on=False, lw=0.6)
    xc = 1.0 if side == "right" else 0.0
    ax.plot((xc - brk, xc + brk), (-brk, +brk), **kw)
    ax.plot((xc - brk, xc + brk), (1 - brk, 1 + brk), **kw)

# FC pipeline annotation above the break
fig1.text(0.50, 0.92,
          f"\u2190  FC decision pipeline ({T_FC_PROCESS} ms)  \u2192",
          ha="center", va="bottom", fontsize=7, color="0.30",
          style="italic")

# ==== LEFT PANEL: early events ====

# --- Anomaly detected at t=0 (shared) ---
for y, m, col in [(lane_abs, "o", COLOR_ABS), (lane_ref, "s", COLOR_REF)]:
    ax_l.plot(0, y, m, color=col, markersize=4, zorder=3)
ax_l.text(0, lane_abs + 0.25, "anomaly\ndetected", fontsize=5.5,
          ha="center", va="bottom", color="0.25")

# --- Waypoint arrives at t=10 (same in both: core RWTBS point) ---
ax_l.axvline(T_SND_WAYPOINT, color="0.60", linewidth=0.5, linestyle=":",
             ymin=0.18, ymax=0.82, zorder=1)
ax_l.plot(T_SND_WAYPOINT, (lane_abs + lane_ref) / 2, "d",
          color="0.40", markersize=4, zorder=4)
ax_l.text(T_SND_WAYPOINT, 1.60, "snd_waypoint @10\n(identical)", fontsize=5.5,
          ha="center", va="bottom", color="0.35")

# --- Processing bars (activity intervals) ---
# Abstract: rcv_waypoint @10 → snd_report @15 (immediate)
t_abs_rcv = next(t_ for t_, l, _ in sim_abs.events if l == "rcv_waypoint")
t_abs_rpt = next(t_ for t_, l, _ in sim_abs.events if l == "snd_report")
ax_l.fill_between([t_abs_rcv, t_abs_rpt],
                   lane_abs - BAR_H, lane_abs + BAR_H,
                   color=COLOR_ABS, alpha=0.45, zorder=2)
ax_l.plot(t_abs_rpt, lane_abs, ">", color=COLOR_ABS, markersize=4, zorder=3)
ax_l.text((t_abs_rcv + t_abs_rpt) / 2, lane_abs + BAR_H + 0.06,
          f"rcv\u2192rpt\n({t_abs_rpt - t_abs_rcv} ms)",
          fontsize=5, ha="center", va="bottom", color=COLOR_ABS)

# Refined: rcv_waypoint @50 → snd_report @55 (after duty-cycle delay)
t_ref_rcv = next(t_ for t_, l, _ in sim_ref.events if l == "rcv_waypoint")
t_ref_rpt = next(t_ for t_, l, _ in sim_ref.events if l == "snd_report")
ax_l.fill_between([t_ref_rcv, t_ref_rpt],
                   lane_ref - BAR_H, lane_ref + BAR_H,
                   color=COLOR_REF, alpha=0.45, zorder=2)
ax_l.plot(t_ref_rpt, lane_ref, ">", color=COLOR_REF, markersize=4, zorder=3)
ax_l.text((t_ref_rcv + t_ref_rpt) / 2, lane_ref - BAR_H - 0.06,
          f"rcv\u2192rpt\n({t_ref_rpt - t_ref_rcv} ms)",
          fontsize=5, ha="center", va="top", color=COLOR_REF)

# --- Duty-cycle delay bracket (the central visual) ---
brace_y = (lane_abs + lane_ref) / 2 - 0.06
ax_l.annotate("", xy=(t_ref_rcv, brace_y), xytext=(t_abs_rcv, brace_y),
              arrowprops=dict(arrowstyle="<->", color="0.25", lw=0.8))
ax_l.text((t_abs_rcv + t_ref_rcv) / 2, brace_y - 0.06,
          f"\u0394 = {t_ref_rcv - t_abs_rcv} ms  (duty-cycle delay)",
          ha="center", va="top", fontsize=6, color="0.15",
          fontweight="bold")

# ==== RIGHT PANEL: deadline region ====

# Deadline line
ax_r.axvline(DEADLINE, color=COLOR_DEAD, linestyle="--", linewidth=1.0,
             zorder=1)
ax_r.text(DEADLINE + 1.5, 1.60, f"deadline\n{DEADLINE} ms", fontsize=6.5,
          color=COLOR_DEAD, ha="left", va="top", style="italic")

# Treatment events
t_abs_treat = sim_abs.t_treatment
t_ref_treat = sim_ref.t_treatment
margin_abs = DEADLINE - t_abs_treat
margin_ref = DEADLINE - t_ref_treat

# Abstract treatment marker + label
ax_r.plot(t_abs_treat, lane_abs, "o", color=COLOR_ABS, markersize=5, zorder=3)
ax_r.text(t_abs_treat - 2, lane_abs + 0.22,
          f"snd_treatment\n@{t_abs_treat} ms",
          fontsize=5.5, ha="right", va="bottom", color=COLOR_ABS)

# Refined treatment marker + label
ax_r.plot(t_ref_treat, lane_ref, "s", color=COLOR_REF, markersize=5, zorder=3)
ax_r.text(t_ref_treat - 2, lane_ref - 0.22,
          f"snd_treatment\n@{t_ref_treat} ms",
          fontsize=5.5, ha="right", va="top", color=COLOR_REF)

# Margin brackets (abstract)
ax_r.annotate("", xy=(DEADLINE, lane_abs + 0.12),
              xytext=(t_abs_treat, lane_abs + 0.12),
              arrowprops=dict(arrowstyle="<->", color=COLOR_ABS, lw=0.5))
ax_r.text((t_abs_treat + DEADLINE) / 2, lane_abs + 0.17,
          f"{margin_abs} ms", ha="center", fontsize=5.5, color=COLOR_ABS,
          fontweight="bold")

# Margin brackets (refined)
ax_r.annotate("", xy=(DEADLINE, lane_ref - 0.12),
              xytext=(t_ref_treat, lane_ref - 0.12),
              arrowprops=dict(arrowstyle="<->", color=COLOR_REF, lw=0.5))
ax_r.text((t_ref_treat + DEADLINE) / 2, lane_ref - 0.17,
          f"{margin_ref} ms", ha="center", fontsize=5.5, color=COLOR_REF,
          fontweight="bold")

# Y axis labels
ax_l.set_yticks([lane_ref, lane_abs])
ax_l.set_yticklabels(["$DA_{ref}$  ", "$DA_{abs}$  "], fontsize=8)
ax_l.set_xlabel("Time (ms)", fontsize=8.5)
ax_r.set_xlabel("Time (ms)", fontsize=8.5)

# Legend
legend_els = [
    Line2D([0], [0], marker="o", color="w", markerfacecolor=COLOR_ABS,
           markersize=4.5, label="$DA_{abs}$ (deployed)"),
    Line2D([0], [0], marker="s", color="w", markerfacecolor=COLOR_REF,
           markersize=4.5, label="$DA_{ref}$ (duty-cycled)"),
]
ax_r.legend(handles=legend_els, fontsize=6, loc="lower left",
            framealpha=0.9, edgecolor="0.80", handletextpad=0.3)

fig1.suptitle("Event Trace Comparison: Abstract vs. Duty-Cycled Drone Agent",
              fontsize=9, y=0.99)

fig1.subplots_adjust(wspace=0.10, left=0.07, right=0.97, top=0.88, bottom=0.16)
path1 = os.path.join(OUT_DIR, "trace_comparison.pdf")
fig1.savefig(path1, bbox_inches="tight")
print(f"\nSaved {path1}")
plt.close(fig1)


# ──────────────────────────────────────────────────────────────────────
# 6.  PLOT 2 — Energy Consumption Over Time
# ──────────────────────────────────────────────────────────────────────

fig2, ax2 = plt.subplots(figsize=(7.0, 2.8))

# Shaded area + step line for each system
ax2.fill_between(t, power_abs, alpha=0.15, color=COLOR_ABS, step="post")
ax2.step(t, power_abs, where="post", color=COLOR_ABS, linewidth=0.7,
         label=f"$DA_{{abs}}$ (polling idle): {energy_abs_mJ/3600:.5f} mWh")

ax2.fill_between(t, power_ref, alpha=0.15, color=COLOR_REF, step="post")
ax2.step(t, power_ref, where="post", color=COLOR_REF, linewidth=0.7,
         label=f"$DA_{{ref}}$ (duty-cycled): {energy_ref_mJ/3600:.5f} mWh")

# Power-level reference lines
for pw, lbl, va_adj in [
    (POWER_SLEEP_MW,  f"sleep ({POWER_SLEEP_MW:.0f} mW)",  "center"),
    (POWER_IDLE_MW,   f"idle ({POWER_IDLE_MW:.0f} mW)",    "top"),
    (POWER_ACTIVE_MW, f"active ({POWER_ACTIVE_MW:.0f} mW)", "bottom"),
]:
    ax2.axhline(pw, color="0.80", linewidth=0.3, linestyle=":", zorder=0)
    ax2.text(SIM_WINDOW + 3, pw, lbl, fontsize=5.5, color="0.45",
             va=va_adj, ha="left")

# Event markers (vertical ticks) for cross-reference with Plot 1
# Stagger text heights to avoid collisions on the compressed time axis
event_markers = [
    (0,                      "anomaly",       0),
    (T_SND_WAYPOINT,         "wp",            1),
    (sim_abs.t_snd_report,   "rpt$_{a}$",    0),
    (sim_ref.t_snd_report,   "rpt$_{r}$",    1),
    (sim_abs.t_treatment,    "treat$_{a}$",   0),
    (sim_ref.t_treatment,    "treat$_{r}$",   1),
]
y_hi = POWER_ACTIVE_MW + 8.0
y_lo = POWER_ACTIVE_MW + 3.5
for t_ev, lbl, tier in event_markers:
    ax2.axvline(t_ev, color="0.70", linewidth=0.3, linestyle="-", zorder=0)
    y_txt = y_hi if tier == 0 else y_lo
    ax2.text(t_ev, y_txt, lbl, fontsize=5, ha="center",
             color="0.40", va="bottom")

ax2.set_xlabel("Time (ms)", fontsize=8.5)
ax2.set_ylabel("CPU power (mW)", fontsize=8.5)
ax2.set_xlim(0, SIM_WINDOW)
ax2.set_ylim(0, POWER_ACTIVE_MW + 16)
ax2.set_yticks([0, POWER_SLEEP_MW, POWER_IDLE_MW, POWER_ACTIVE_MW])
ax2.tick_params(axis="both", labelsize=7.5)
ax2.legend(fontsize=7, loc="center left",
           framealpha=0.85, edgecolor="0.80")
ax2.set_title(
    f"MCU Power Consumption  \u2014  {energy_saving_pct:.0f}% energy reduction "
    f"with duty-cycled processing",
    fontsize=9, pad=6)

fig2.subplots_adjust(left=0.08, right=0.88, top=0.88, bottom=0.14)
path2 = os.path.join(OUT_DIR, "energy_comparison.pdf")
fig2.savefig(path2, bbox_inches="tight")
print(f"Saved {path2}")
plt.close(fig2)

# ──────────────────────────────────────────────────────────────────────
# 7.  SUMMARY  (for results_table.tex values)
# ──────────────────────────────────────────────────────────────────────
print("\n====== VALUES FOR results_table.tex ======")
print(f"  DA_abstract treatment time : {sim_abs.t_treatment} ms")
print(f"  DA_refined  treatment time : {sim_ref.t_treatment} ms")
print(f"  DA_abstract energy         : {energy_abs_mJ/3600:.5f} mWh")
print(f"  DA_refined  energy         : {energy_ref_mJ/3600:.5f} mWh")
print(f"  Energy saving              : {energy_saving_pct:.1f}%")

# ──────────────────────────────────────────────────────────────────────
# 8.  SENSITIVITY ANALYSIS — waypoint send-frequency sweep
#
#     Key question: for which waypoint frequency f does RWTBS still hold?
#
#     Condition 4 end-to-end constraint (Definition 3):
#       δR + δR_next ≤ δA + δA_next
#     Concretely: the refined system must issue treatment for cycle i
#     before the abstract system would issue treatment for cycle i+1.
#       treatment_ref[i] ≤ treatment_abs[i+1]
#     ↔ latency_ref[i] ≤ L_ABS + T_interval
#     ↔ T_interval ≥ L_REF_BASE − L_ABS = T_DUTY_CYCLE = 40 ms
#     ↔ f ≤ 1000 / T_DUTY_CYCLE = 25 Hz  (theoretical threshold)
#
#     The multi-cycle simulation also captures queueing: at f > 25 Hz
#     a new waypoint arrives before the drone finishes the previous duty-
#     cycle window, causing latencies to grow and deadlines to be violated.
# ──────────────────────────────────────────────────────────────────────

N_CYCLES        = 20    # cycles per frequency point (enough for steady state)
WARMUP_CYCLES   = 5     # discard first N cycles from steady-state metrics

# The "case study operating point" is the scenario from Plot 1 / Plot 2.
# Plot 1 is a single-shot event trace; we choose 1 Hz as the representative
# operating frequency (one waypoint per second during a survey pass —
# a typical agricultural drone mission cadence).
CASE_STUDY_FREQ = 1.0   # Hz

# Steady-state latency reference values (no queueing)
L_ABS      = T_SND_WAYPOINT + T_PROC + T_FC_PROCESS             # 450 ms
L_REF_BASE = T_SND_WAYPOINT + T_DUTY_CYCLE + T_PROC + T_FC_PROCESS  # 490 ms

# Frequency range: 0.5 Hz → 60 Hz
# T_interval spans 1667 ms (very safe) → 16.7 ms (well into failure region)
# We need f_crit ≈ 25 Hz comfortably inside this window.
freq_hz = np.logspace(np.log10(0.5), np.log10(60.0), 60)


def simulate_frequency_sweep(f_hz_array, n_cycles=N_CYCLES, warmup=WARMUP_CYCLES):
    """
    For each frequency f (Hz) simulate n_cycles waypoint cycles for both
    DA_abstract (immediate) and DA_refined (worst-case duty-cycle + queueing).

    Returns a list of per-frequency result dicts.
    """
    results = []

    for f in f_hz_array:
        T_int = 1000.0 / f  # inter-message interval (ms)

        # ── Abstract system: immediate processing, no queueing ──────────
        lat_abs_cycles = []
        for i in range(n_cycles):
            t_wp     = i * T_int + T_SND_WAYPOINT
            t_treat  = t_wp + T_PROC + T_FC_PROCESS
            lat_abs_cycles.append(t_treat - i * T_int)  # always L_ABS

        # ── Refined system: worst-case duty-cycle delay + queueing ──────
        lat_ref_cycles   = []
        viol_cycles      = []
        cond4_ok_cycles  = []
        prev_done        = 0.0   # time when drone finished previous cycle

        for i in range(n_cycles):
            cycle_start  = i * T_int
            t_wp         = cycle_start + T_SND_WAYPOINT

            # Drone starts processing after both: waypoint arrived AND
            # previous cycle finished.  Then waits one full duty-cycle
            # period (worst-case).
            t_avail      = max(t_wp, prev_done)
            t_proc_start = t_avail + T_DUTY_CYCLE
            t_proc_end   = t_proc_start + T_PROC
            t_treat      = t_proc_end + T_FC_PROCESS
            prev_done    = t_proc_end

            latency = t_treat - cycle_start
            lat_ref_cycles.append(latency)
            viol_cycles.append(latency > DEADLINE)

            # Condition 4 (Definition 3, formal check — per isolated cycle):
            #   treatment_ref[i] ≤ treatment_abs[i+1]
            #   ↔ L_REF_BASE ≤ L_ABS + T_interval
            # This is the same for every cycle (depends only on f, not on
            # queueing state), so we evaluate it on the non-queued reference
            # latency L_REF_BASE.  The queued latency is tracked separately
            # via deadline_viol to capture practical system-level effects.
            cond4_ok_cycles.append(L_REF_BASE <= L_ABS + T_int)

        # ── Steady-state metrics (skip warm-up cycles) ──────────────────
        lat_abs_ss  = float(np.mean(lat_abs_cycles[warmup:]))
        lat_ref_ss  = float(np.mean(lat_ref_cycles[warmup:]))
        viol_rate   = float(np.mean(viol_cycles[warmup:]))
        cond4_holds = bool(np.all(cond4_ok_cycles[warmup:]))
        cond4_rate  = float(np.mean(cond4_ok_cycles[warmup:]))

        # ── Energy per cycle (numerical, matching Plot 2 power model) ────
        # Abstract: idle baseline + one active burst
        e_abs = (POWER_IDLE_MW  * T_int
                 + (POWER_ACTIVE_MW - POWER_IDLE_MW) * (T_PROC + TX_BURST_MS)
                 ) / 1000.0  # mW·ms → mJ
        # Refined: sleep baseline + periodic duty-cycle wakes + one burst
        n_wakes = T_int / T_DUTY_CYCLE   # continuous approximation
        e_ref = (POWER_SLEEP_MW * T_int
                 + (POWER_ACTIVE_MW - POWER_SLEEP_MW)
                 * (n_wakes * DUTY_WAKE_MS + T_PROC + TX_BURST_MS)
                 ) / 1000.0  # mJ

        results.append(dict(
            f          = f,
            T_int      = T_int,
            lat_abs    = lat_abs_ss,
            lat_ref    = lat_ref_ss,
            margin_abs = DEADLINE - lat_abs_ss,
            margin_ref = DEADLINE - lat_ref_ss,
            viol_rate  = viol_rate,
            cond4_holds= cond4_holds,
            cond4_rate = cond4_rate,
            e_abs      = e_abs,
            e_ref      = e_ref,
        ))

    return results


sweep = simulate_frequency_sweep(freq_hz)

# ── Find critical frequency threshold from simulation ───────────────────
# First index where Condition 4 is violated in steady state
crit_idx = next((i for i, r in enumerate(sweep) if not r["cond4_holds"]), None)
if crit_idx is not None and crit_idx > 0:
    # Interpolate between last-valid and first-invalid points for precision
    f_lo, f_hi = sweep[crit_idx - 1]["f"], sweep[crit_idx]["f"]
    f_crit = (f_lo + f_hi) / 2.0
    T_crit = 1000.0 / f_crit
else:
    # Theoretical: T_interval = L_REF_BASE - L_ABS = T_DUTY_CYCLE
    f_crit = 1000.0 / (L_REF_BASE - L_ABS)
    T_crit = L_REF_BASE - L_ABS

# Practical deadline violation onset: first frequency where any queued cycle
# exceeds DEADLINE (i.e., viol_rate > 0 in steady state).
viol_crit_idx = next((i for i, r in enumerate(sweep) if r["viol_rate"] > 0), None)
if viol_crit_idx is not None and viol_crit_idx > 0:
    f_viol = (sweep[viol_crit_idx - 1]["f"] + sweep[viol_crit_idx]["f"]) / 2.0
else:
    f_viol = None

# Extract plotting arrays
freqs       = np.array([r["f"]          for r in sweep])
T_ints      = np.array([r["T_int"]      for r in sweep])
lat_abs_arr = np.array([r["lat_abs"]    for r in sweep])
lat_ref_arr = np.array([r["lat_ref"]    for r in sweep])
margin_abs_arr = np.array([r["margin_abs"] for r in sweep])
margin_ref_arr = np.array([r["margin_ref"] for r in sweep])
viol_rate_arr  = np.array([r["viol_rate"]  for r in sweep])
cond4_arr      = np.array([r["cond4_holds"] for r in sweep], dtype=bool)
e_abs_arr   = np.array([r["e_abs"]      for r in sweep])
e_ref_arr   = np.array([r["e_ref"]      for r in sweep])
saving_pct_arr = (1.0 - e_ref_arr / e_abs_arr) * 100.0

# Case-study energy saving (at CASE_STUDY_FREQ = 1 Hz)
cs_idx = int(np.argmin(np.abs(freqs - CASE_STUDY_FREQ)))
cs_saving = saving_pct_arr[cs_idx]


# ──────────────────────────────────────────────────────────────────────────────
# Print sensitivity-analysis summary (for use in paper text)
# ──────────────────────────────────────────────────────────────────────────────
print("\n" + "=" * 60)
print("SENSITIVITY ANALYSIS — SUMMARY")
print("=" * 60)
print(f"  Frequency range simulated  : {freqs[0]:.2f} – {freqs[-1]:.2f} Hz")
print(f"  RWTBS holds for f ≤         : {f_crit:.1f} Hz  "
      f"(T_interval ≥ {T_crit:.1f} ms)")
print(f"  RWTBS fails for f >         : {f_crit:.1f} Hz  "
      f"(T_interval < {T_crit:.1f} ms)")
print(f"  Theoretical threshold       : {1000.0/(L_REF_BASE-L_ABS):.1f} Hz  "
      f"(= 1000 / (L_ref − L_abs) = 1000 / {L_REF_BASE-L_ABS:.0f})")
if f_viol is not None:
    T_queue_onset = T_DUTY_CYCLE + T_PROC   # queueing when T_interval < this
    print(f"  Practical deadline onset    : {f_viol:.1f} Hz  "
          f"(T_interval < {T_queue_onset} ms = T_duty+T_proc)")
print(f"  Case study operating point  : {CASE_STUDY_FREQ:.1f} Hz  "
      f"(T_interval = {1000/CASE_STUDY_FREQ:.0f} ms)")
print(f"  Energy saving @ case study  : {cs_saving:.1f}%")
print(f"  Max energy saving (0.5 Hz)  : {saving_pct_arr[0]:.1f}%")
print(f"  Energy saving at threshold  : {saving_pct_arr[np.argmin(np.abs(freqs-f_crit))]:.1f}%")
n_valid = int(np.sum(cond4_arr))
n_total = len(cond4_arr)
print(f"  Valid frequency points      : {n_valid}/{n_total} "
      f"({100*n_valid/n_total:.0f}% of sweep)")
print("=" * 60)


# ──────────────────────────────────────────────────────────────────────
# 9.  PLOT 3 — RWTBS Validity Region  (rwtbs_sensitivity.pdf)
# ──────────────────────────────────────────────────────────────────────

fig3, ax3 = plt.subplots(figsize=(7.0, 3.2))

# Shaded validity / violation regions
ax3.axvspan(freqs[0], f_crit,  alpha=0.10, color="#2CA02C", zorder=0,
            label="_nolegend_")
ax3.axvspan(f_crit,  freqs[-1], alpha=0.10, color="#D62728", zorder=0,
            label="_nolegend_")

# Deadline margin lines for both systems
ax3.plot(freqs, margin_abs_arr, color=COLOR_ABS, linewidth=1.1,
         label=f"$DA_{{abs}}$ deadline margin ({DEADLINE - L_ABS:.0f} ms, constant)")
ax3.plot(freqs, margin_ref_arr, color=COLOR_REF, linewidth=1.1,
         label=f"$DA_{{ref}}$ deadline margin")

# Zero-margin reference
ax3.axhline(0, color=COLOR_DEAD, linewidth=0.6, linestyle="--", zorder=1)
ax3.text(freqs[-1] * 1.01, 0, "deadline\nexceeded", fontsize=5.5,
         color=COLOR_DEAD, va="center", ha="left", style="italic",
         clip_on=False)

# Critical frequency vertical line
ax3.axvline(f_crit, color="0.30", linewidth=0.8, linestyle="-.", zorder=2)
ax3.text(f_crit * 1.04, ax3.get_ylim()[1] if False else 55,
         f"$f_{{crit}}$ = {f_crit:.1f} Hz\n($T$ = {T_crit:.0f} ms)\n[Condition 4]",
         fontsize=6.5, ha="left", va="top", color="0.25")

# Practical deadline-violation onset (queueing effect)
if f_viol is not None:
    ax3.axvline(f_viol, color=COLOR_DEAD, linewidth=0.7, linestyle=":",
                zorder=2, alpha=0.70)
    ax3.text(f_viol * 0.88, -18,
             f"deadline viol.\nonset ≈ {f_viol:.1f} Hz\n[queueing]",
             fontsize=5.5, ha="right", va="top",
             color=COLOR_DEAD, alpha=0.80)

# Case study operating point
ax3.axvline(CASE_STUDY_FREQ, color="0.55", linewidth=0.6, linestyle=":",
            zorder=2)
ax3.text(CASE_STUDY_FREQ * 1.06, -8,
         f"case study\noperating point\n({CASE_STUDY_FREQ:.0f} Hz)",
         fontsize=5.5, ha="left", va="top", color="0.40")

# Region text labels (inside shaded areas)
ax3.text(np.sqrt(freqs[0] * f_crit), 38,
         "RWTBS holds", fontsize=8, ha="center", va="center",
         color="#1a6b1a", fontweight="bold", alpha=0.75)
ax3.text(np.sqrt(f_crit * freqs[-1]), 15,
         "RWTBS violated", fontsize=8, ha="center", va="center",
         color="#8b1a1a", fontweight="bold", alpha=0.75)

# Engineering insight text box (inside the plot)
insight_f   = f_crit
insight_T   = T_crit
insight_txt = (f"RWTBS holds for $f \\leq {insight_f:.1f}$ Hz  "
               f"($T \\geq {insight_T:.0f}$ ms inter-message interval)")
ax3.text(0.50, 0.08, insight_txt,
         transform=ax3.transAxes,
         fontsize=7, ha="center", va="bottom",
         bbox=dict(boxstyle="round,pad=0.35", facecolor="white",
                   edgecolor="0.65", linewidth=0.5, alpha=0.90))

ax3.set_xscale("log")
ax3.set_xlim(freqs[0] * 0.85, freqs[-1] * 1.15)
ax3.set_ylim(-25, 65)
ax3.set_xlabel("Waypoint send frequency (Hz)", fontsize=8.5)
ax3.set_ylabel("Deadline margin (ms)", fontsize=8.5)
ax3.tick_params(axis="both", labelsize=7.5)

# Custom x-tick labels showing both Hz and ms
_xticks = [0.5, 1, 2, 5, 10, 25, 50]
ax3.set_xticks(_xticks)
ax3.set_xticklabels([str(v) for v in _xticks], fontsize=7)

# Secondary x-axis: inter-message interval
ax3_top = ax3.twiny()
ax3_top.set_xscale("log")
ax3_top.set_xlim(ax3.get_xlim())
ax3_top.set_xticks(_xticks)
ax3_top.set_xticklabels([f"{1000/v:.0f}" for v in _xticks], fontsize=6.5)
ax3_top.set_xlabel("Inter-message interval (ms)", fontsize=7.5, labelpad=3)
ax3_top.tick_params(axis="x", direction="in", width=0.4)

# Spine cleanup (twin axis re-enables right spine on original)
ax3.spines["top"].set_visible(False)
ax3.spines["right"].set_visible(False)

legend_els3 = [
    mpatches.Patch(color="#2CA02C", alpha=0.25, label="RWTBS holds"),
    mpatches.Patch(color="#D62728", alpha=0.25, label="RWTBS violated"),
    plt.Line2D([0], [0], color=COLOR_ABS, linewidth=1.1,
               label=f"$DA_{{abs}}$ deadline margin ({DEADLINE - L_ABS:.0f} ms, constant)"),
    plt.Line2D([0], [0], color=COLOR_REF, linewidth=1.1,
               label="$DA_{ref}$ deadline margin"),
]
ax3.legend(handles=legend_els3, fontsize=6, loc="upper right",
           framealpha=0.90, edgecolor="0.80", handlelength=1.4)

ax3.set_title(
    "RWTBS Validity as a Function of Waypoint Send Frequency",
    fontsize=9, pad=8)

fig3.subplots_adjust(left=0.09, right=0.93, top=0.82, bottom=0.14)
path3 = os.path.join(OUT_DIR, "rwtbs_sensitivity.pdf")
fig3.savefig(path3, bbox_inches="tight")
print(f"\nSaved {path3}")
plt.close(fig3)


# ──────────────────────────────────────────────────────────────────────
# 10.  PLOT 4 — Energy vs Frequency Trade-off  (energy_sensitivity.pdf)
# ──────────────────────────────────────────────────────────────────────

#fig4, ax4l = plt.subplots(figsize=(7.0, 3.0))
fig4, ax4l = plt.subplots(figsize=(3, 2))
ax4r = ax4l.twinx()

## Invalid-frequency shading (consistent with Plot 3)
#ax4l.axvspan(f_crit, freqs[-1], alpha=0.08, color="#D62728", zorder=0,
#             label="_nolegend_")

ax4l.axvspan(
    f_crit, freqs[-1],
    facecolor="none",        # remove solid red fill
    hatch="///",             # hatch pattern
    edgecolor="gray",        # hatch color
    linewidth=0.0,
    zorder=0,
    label="_nolegend_"
)

# Energy per cycle lines (left axis)
ax4l.plot(freqs, e_abs_arr / 3600, color=COLOR_ABS, linewidth=1.1,
          label=f"$DA_{{abs}}$ energy/cycle (mWh)")
ax4l.plot(freqs, e_ref_arr / 3600, color=COLOR_REF, linewidth=1.1,
          label=f"$DA_{{ref}}$ energy/cycle (mWh)")

# Energy saving % (right axis)
ax4r.plot(freqs, saving_pct_arr, color="0.35", linewidth=0.9,
          linestyle="--", label="Energy saving (%)")
ax4r.set_ylabel("Energy saving (%)", fontsize=8, color="0.35")
ax4r.tick_params(axis="y", labelsize=7, colors="0.35")
ax4r.set_ylim(0, saving_pct_arr.max() * 1.30)
ax4r.spines["right"].set_visible(True)

# Critical frequency line
ax4l.axvline(f_crit, color="0.30", linewidth=0.8, linestyle="-.", zorder=2)
ax4l.text(f_crit * 0.95, e_abs_arr.max() / 3600 * 0.39,
          f"$f_{{crit}}$ = {f_crit:.1f} Hz", fontsize=6.5,
          ha="right", va="top", color="0.25")

# Case study operating point
ax4l.axvline(CASE_STUDY_FREQ, color="0.55", linewidth=0.6, linestyle=":",
             zorder=2)

# Annotate case study saving
#cs_freq_val   = freqs[cs_idx]
#cs_e_abs_val  = e_abs_arr[cs_idx] / 3600
#cs_e_ref_val  = e_ref_arr[cs_idx] / 3600
#_cs_annot_y = cs_e_abs_val * 0.75
#ax4l.annotate(
#    f"case study\n({CASE_STUDY_FREQ:.0f} Hz,  {cs_saving:.1f}% saving)",
#    xy=(cs_freq_val, cs_e_abs_val),
#    xytext=(cs_freq_val * 2.5, _cs_annot_y),
#    fontsize=5.5, color="0.30", ha="left",
#    arrowprops=dict(arrowstyle="->", color="0.55",
#                    connectionstyle="arc3,rad=-0.2", lw=0.5),
#)

# Region labels
#ax4l.text(np.sqrt(f_crit * freqs[-1]),
#          e_abs_arr.max() / 3600 * 0.25,
#          "RWTBS violated\n(invalid region)",
#          fontsize=6.5, ha="center", va="center",
#          color="#8b1a1a", alpha=0.70, style="italic")

ax4l.set_xscale("log")
ax4l.set_xlim(freqs[0] * 0.85, freqs[-1] * 1.15)
ax4l.set_xlabel("Waypoint send frequency (Hz)", fontsize=8.5)
ax4l.set_ylabel("Energy per cycle (mWh)", fontsize=8)
ax4l.tick_params(axis="both", labelsize=7.5)
ax4l.set_xticks(_xticks)
ax4l.set_xticklabels([str(v) for v in _xticks], fontsize=7)
ax4l.spines["top"].set_visible(False)

# Combined legend
lines_left,  labs_l = ax4l.get_legend_handles_labels()
lines_right, labs_r = ax4r.get_legend_handles_labels()
legend_els4 = (
    [plt.Line2D([0], [0], color=COLOR_ABS,  linewidth=1.1,
                label=f"$DA_{{abs}}$ energy/cycle")]
    + [plt.Line2D([0], [0], color=COLOR_REF, linewidth=1.1,
                  label=f"$DA_{{ref}}$ energy/cycle")]
    + [plt.Line2D([0], [0], color="0.35", linewidth=0.9, linestyle="--",
                  label="Energy saving (%)")]
    + [mpatches.Patch(
    facecolor="none",
    edgecolor="gray",
    hatch="///////",
    label="RWTBS violated"
)]
)


ax4l.legend(
    handles=legend_els4,
    fontsize=6,
    loc="upper center",
    framealpha=0.90,
    edgecolor="0.80",
    handlelength=1,
    handletextpad=0.2,      # space between marker and text
    columnspacing=0.6,      # horizontal space between columns
    ncol=2
)

#ax4l.set_title(
#    "Energy per Cycle vs. Waypoint Frequency — "
#    f"Saving valid up to {f_crit:.1f} Hz",
#    fontsize=9, pad=6)

fig4.subplots_adjust(left=0.09, right=0.88, top=0.88, bottom=0.14)
path4 = os.path.join(OUT_DIR, "energy_sensitivity.pdf")
fig4.savefig(path4, bbox_inches="tight")
print(f"Saved {path4}")
plt.close(fig4)
