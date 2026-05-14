import re

# -------- FILES --------
pt_file = "assets/CS6_Pump/V1_PT.xml"
dt_file = "assets/CS6_Pump/V2_DT.xml"

# -------- PT CHANGES --------
pt_changes = {
    "t_infuse <= 30": "t_infuse <= 5",
    "t_infuse >= 5 && t_infuse <= 30": "t_infuse >= 1 && t_infuse <= 5",
    "t_infuse >= 10": "t_infuse >= 1",

    "t_bolus <= 10": "t_bolus <= 2",
    "t_bolus >= 1 && t_bolus <= 10": "t_bolus >= 1 && t_bolus <= 2",

    "t_pause <= 31": "t_pause <= 5",
    "t_pause <= 120": "t_pause <= 6",
    "t_pause >= 30": "t_pause >= 4",

    "t_kvo >= 1": "t_kvo >= 2",

    "t_alarm <= 30": "t_alarm <= 5",
    "t_alarm <= 5": "t_alarm <= 5",
    "t_alarm >= 1 && t_alarm <= 30": "t_alarm >= 5 && t_alarm <= 15",
    "t_alarm <= 1": "t_alarm <= 3"
}

# -------- DT CHANGES --------
dt_changes = {
    "t_ice_telem <= 18": "t_ice_telem <= 10",
    "t_ice_telem >= 2 && t_ice_telem <= 18": "t_ice_telem >= 1 && t_ice_telem <= 10",
    "t_ice_telem >= 8": "t_ice_telem >= 5",

    "t_safety <= 12": "t_safety <= 8",
    "t_safety >= 2 && t_safety <= 12": "t_safety >= 3 && t_safety <= 8",

    "t_dose_track <= 18": "t_dose_track <= 10",
    "t_dose_track <= 15": "t_dose_track <= 9",
    "t_dose_track >= 12": "t_dose_track >= 6",

    "t_kvo_dt >= 4": "t_kvo_dt >= 4",

    "t_safety <= 8": "t_safety <= 6",
    "t_safety >= 4 && t_safety <= 18": "t_safety >= 4 && t_safety <= 10"
}

# -------- APPLY CHANGES FUNCTION --------
def apply_changes(file_path, changes):
    with open(file_path, "r") as f:
        content = f.read()

    for old, new in changes.items():
        content = content.replace(old, new)

    with open(file_path, "w") as f:
        f.write(content)

    print(f"{file_path} updated.")


# -------- RUN --------
apply_changes(pt_file, pt_changes)
apply_changes(dt_file, dt_changes)

print("PT and DT timing values updated.")