import pandas as pd
import matplotlib.pyplot as plt
import sys
import numpy as np
from matplotlib.ticker import FuncFormatter
import os
plt.style.use('default')
plt.rcParams.update({
    'font.size': 10,
    'axes.titlesize': 12,
    'axes.labelsize': 11,
    'xtick.labelsize': 9,
    'ytick.labelsize': 9,
    'legend.fontsize': 9,
    'figure.titlesize': 14,
    'lines.linewidth': 2,
    'lines.markersize': 6,
    'axes.grid': True,
    'grid.alpha': 0.3, 
})


figsize = (4.4, 2.8)


def format_sci_notation(val):
    if val == 'None' or pd.isnull(val):
        return 'None'
    try:
        val = float(val)
        if val == 0:
            return '0'
        exponent = int(np.floor(np.log10(abs(val))))
        coeff = val / (10 ** exponent)
        # Format with 2 significant digits
        return f"$\\sim$${coeff:.2f}$$\\times$$10^{{{exponent}}}$"
    except Exception:
        return str(val)

def formatter(x, pos):
            import math
            if x == 0:
                return '0'
            # Calculate the exponent
            exponent = int(math.floor(math.log10(abs(x))))
            # Format as 10^exponent
            return f'$10^{exponent}$'




# Trim trailing zeros and decimal point for time and memory
def trim_float_str(s):
    return s.rstrip('0').rstrip('.') if '.' in s else s

gray_colors = ['0.85', '0.5', '0.35', '0.15', '0.0']
border_colors = ['0.75',  '0.5', '0.35', '0.15', '0.0']

hatches =  ['']*len(gray_colors) + ['///']*len(gray_colors) +  ['+']*len(gray_colors) +  ['xxx']*len(gray_colors) +  ['ooo']*len(gray_colors) +  ['...']*len(gray_colors) +  ['***']*len(gray_colors)

#hatches =['','']
def plot_comparison(merged, use_log=True, output_folder="results", file_name ="comparison_bar_chart.png", parallel_df={}, properties=[3,5,10,20, 30]):
    # parallel_DF is a dictionary {n_workers: df}
    benchmarks = merged['model'].tolist()
    labels = []
    prefix_map = {'S': 'Small', 'M': 'Medium', 'L': 'Large', 'XL': 'Extra Large'}
    for model in benchmarks:
        parts = model.split('/')[-1].split('_')
        prefix = parts[0].upper()
        prev = parts[1].split('.')[0]
        name = prefix_map.get(prefix, prefix)
        num = prev.split('n')

        if len(num) == 1:
            snum = int(num[0])
            #print(snum, name)
            if snum == 1:
                label = name
            else:
                label = f"{name} ({snum})"
        else:
            label = f"{name} ({num[0]}, {num[1]} Workers)"
        labels.append(label)


    x = np.arange(len(benchmarks))
    n_bars = 2 + len(properties) + len(parallel_df) if parallel_df else 2  # Uppaal, RTWBS, and all parallel
    bar_width = 0.2 / n_bars

    fig, axes = plt.subplots(2, 1, figsize=(12, 10))
    gray_colors = ['0.85', '0.6', '0.35', '0.15', '0.0']  # light to dark gray
    hatches = ['/', '\\', '|', '-', '+', 'x', 'o', 'O', '.', '*']


    # Prepare all time data
    time_data = [merged['Time(ms)'], merged['check_time_ms']]
    time_labels = ['Uppaal', 'RTWBS']
    if parallel_df is not None:
        for n_workers,pdf in parallel_df.items():
            pdf = pdf.rename(columns={'model_name': 'model'}).set_index('model')
            time_data.append([pdf.loc[m, 'check_time_ms'] if m in pdf.index else 0 for m in merged['model']])
            time_labels.append(f'RTWBS ({n_workers})')
    #add to time _date merged['Time(ms)']*properties
    for prop in properties:
        time_data.append(merged['Time(ms)']*prop)
        time_labels.append(f'Uppaal {prop} Prop.')
    
    # Plot time bars
    for i, (y, label) in enumerate(zip(time_data, time_labels)):
        axes[0].bar(
        x + (i - n_bars/2)*bar_width + bar_width/2, y, bar_width,
        color=gray_colors[i % len(gray_colors)],
        label=label,
        hatch=hatches[i % len(hatches)],
        edgecolor='black'
    )
    handles, labels_ = axes[0].get_legend_handles_labels()
    axes[0].legend(handles, labels_, loc='best', frameon=True)
    if use_log:
        axes[0].set_yscale('log')
    # Prepare all memory data
    mem_data = [merged['Memory(KB)'], merged['memory_usage_kb']]
    mem_labels = ['Uppaal', 'RTWBS']
    if parallel_df is not None:
        for n_workers, pdf in parallel_df.items():
            pdf = pdf.rename(columns={'model_name': 'model'}).set_index('model')
            mem_data.append([pdf.loc[m, 'memory_usage_kb'] if m in pdf.index else 0 for m in merged['model']])
            mem_labels.append(f'RTWBS ({n_workers})')

    # Plot memory bars
    for i, (y, label) in enumerate(zip(mem_data, mem_labels)):
        axes[1].bar(
        x + (i - n_bars/2)*bar_width + bar_width/2, y, bar_width,
        color=gray_colors[i % len(gray_colors)],
        label=label,
        hatch=hatches[i % len(hatches)],
        edgecolor='black'
    )
    handles, labels_ = axes[1].get_legend_handles_labels()
    axes[1].legend(handles, labels_, loc='best', frameon=True)
    if use_log:
        axes[1].set_yscale('log')
    plt.tight_layout()
    plt.savefig(f'{output_folder}/{file_name}')
    print(f"Bar chart saved: {output_folder}/{file_name}")
    plt.show()

def generate_latex_table(merged, zones_path, output_folder,file_name="results_table.tex", parallel_df={}):
    zones_df = pd.read_csv(zones_path)
    zone_map = {}
    for _, row in zones_df.iterrows():
        zone_map[row['name'].strip()] = {
            'system_size': row['system_size'],
            'comp': row['comp']
        }
    latex = r"""\begin{table}[t]
\centering
\caption{Comparison of Verification Time and Memory Footprint of RTWBS vs. UPPAAL.The best time and memory values for each configuration are highlighted in bold.}
\label{tab:evaluation}
\resizebox{\columnwidth}{!}{
\begin{tabular}{lcccccc}
\toprule
\multirow{2}{*}{\textbf{Configuration}} & \textbf{System} &\multicolumn{3}{c}{\textbf{Time (s)}} & \multicolumn{2}{c}{\textbf{Memory (KB)}} \\
\cmidrule(lr){3-5} \cmidrule(lr){6-7}
 & \textbf{Size}&\textbf{RTWBS}&\textbf{RTWBS (Paral.)} & \textbf{UPPAAL} & \textbf{RTWBS} & \textbf{UPPAAL}\\
\midrule
"""
    prefix_map = {'S': 'Small', 'M': 'Medium', 'L': 'Large', 'XL': 'Extra Large'}
    timed_out_threshold = 10 * 3600  # 10 hours in seconds
    memory_threshold = 81216
    has_timed_out = False
    for _, row in merged.iterrows():
        model = row['model'].split('/')[-1].replace('.xml', '')
        parts = model.split('_')
        prefix = parts[0].upper()
        num = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 1
        name = prefix_map.get(prefix, prefix)
        if num == 1:
            config = name
        else:
            config = f"{name} ({num} Comp)"
        
        zone_info = zone_map.get(model, {'system_size': 'None', 'comp': 'None'})
        tot_states = zone_info['system_size'] if pd.notnull(zone_info['system_size']) else 'None'
        tot_states = format_sci_notation(tot_states)



        #components = zone_info['comp'] if pd.notnull(zone_info['comp']) else 'None'
        uppaal_time_val = row.get('Time(ms)', None)
        if pd.notnull(uppaal_time_val) and float(uppaal_time_val) >= timed_out_threshold*1000:
            #uppaal_time = f">${timed_out_threshold}^*$"
            uppaal_time = timed_out_threshold*1000
            has_timed_out = True
        elif pd.notnull(uppaal_time_val):
            uppaal_time = trim_float_str(f"{uppaal_time_val/1000:.3f}")
        else:
            uppaal_time = "None"

        rwtbs_time = trim_float_str(f"{row.get('check_time_ms', 'None')/1000:.3f}") if pd.notnull(row.get('check_time_ms', None)) else "None"
        uppaal_mem = trim_float_str(f"{row.get('Memory(KB)', 'None'):.3f}") if pd.notnull(row.get('Memory(KB)', None)) else None
        rwtbs_mem = trim_float_str(f"{row.get('memory_usage_kb', 'None'):.3f}") if pd.notnull(row.get('memory_usage_kb', None)) else None

        
        

        n_workers, pdf = list(parallel_df.items())[0] if parallel_df else (None, None)
        rwtbs_time_par = "-"
        if n_workers and pdf is not None:
            #print(row['model'], pdf["model"])
            if row['model'] in pdf["model"].values:
                rwtbs_time_val = pdf.loc[pdf["model"] == row['model'], 'check_time_ms'].values[0]
                if pd.notnull(rwtbs_time_val) and float(rwtbs_time_val) >= timed_out_threshold*1000:
                    #rwtbs_time_par = f">${timed_out_threshold}^*$"
                    rwtbs_time_par = timed_out_threshold
                    #has_timed_out = True
                elif pd.notnull(rwtbs_time_val):
                    rwtbs_time_par = trim_float_str(f"{rwtbs_time_val/1000:.3f}")
                else:
                    rwtbs_time_par = "None"
            else:
                rwtbs_time_par = "-"


        
        #text_uppaal_time = f"\\textbf{{{uppaal_time}}}" if uppaal_time < rwtbs_time else uppaal_time
        #text_rwtbs_time = f"\\textbf{{{rwtbs_time}}}" if rwtbs_time <= uppaal_time   else rwtbs_time
        times_to_compare = []
        if rwtbs_time not in [None, "-", "None"]:
            times_to_compare.append(float(rwtbs_time))
        if rwtbs_time_par not in [None, "-", "None"]:
            times_to_compare.append(float(rwtbs_time_par))
        if uppaal_time not in [None, "-", "None"]:
            times_to_compare.append(float(uppaal_time))

        if times_to_compare:
            min_time = min(times_to_compare)
        else:
            min_time = None
        
        is_uppaal_min = min_time is not None and float(uppaal_time) == min_time
        #text_uppaal_time = f"\\textbf{{{uppaal_time}}}" if is_uppaal_min else uppaal_time
        text_rwtbs_time = f"\\textbf{{{rwtbs_time}}}" if min_time is not None and float(rwtbs_time) == min_time else rwtbs_time
        text_rwtbs_time_par = f"\\textbf{{{rwtbs_time_par}}}" if min_time is not None and rwtbs_time_par not in [None, "-", "None"] and float(rwtbs_time_par) == min_time else rwtbs_time_par

        #rwtbs_time_par = f">${timed_out_threshold}^*$"
        if (is_uppaal_min):
            if has_timed_out:
                text_uppaal_time = f"{{>$\\mathbf{{{timed_out_threshold}}}^*$}}"
            else:
                text_uppaal_time = f"\\textbf{{{uppaal_time}}}"
        else:
            if has_timed_out:
                text_uppaal_time = f">${timed_out_threshold}^*$"
            else:
                text_uppaal_time = uppaal_time


        text_rwtbs_mem = f"\\textbf{{{rwtbs_mem}}}" if pd.notnull(rwtbs_mem) and (not pd.notnull(uppaal_mem) or (pd.notnull(uppaal_mem) and float(rwtbs_mem) <= float(uppaal_mem))) else rwtbs_mem
        if not has_timed_out:
            text_uppaal_mem = f"\\textbf{{{uppaal_mem}}}" if pd.notnull(rwtbs_mem) and pd.notnull(uppaal_mem) and float(uppaal_mem) < float(rwtbs_mem) else uppaal_mem
        else:
            text_uppaal_mem = f">${memory_threshold}^*$"


        latex += rf"{config} & {tot_states} &{text_rwtbs_time}&{text_rwtbs_time_par} & {text_uppaal_time} & {text_rwtbs_mem} & {text_uppaal_mem}" + r"\\"+ "\n"
    
    # Add parallel configurations if any, for the uppaal, simply "-", no
    if parallel_df is not None and len(parallel_df) > 0 and False:
        #
        for n_workers, pdf in parallel_df.items():
            latex += r"\midrule" + "\n"
            latex += r"\multicolumn{6}{c}{\textbf{Parallel Configurations}}\\"+ "\n" #+ f" ({n_workers}"+ r"Workers)}} \\"
            
            latex += r"\midrule" + "\n"
            for _, row in pdf.iterrows():
                model_name = row['model'].split('/')[-1].replace('.xml', '')
                parts = model_name.split('_')
                prefix = parts[0].upper()
                num = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 1
                name = prefix_map.get(prefix, prefix)
                if num == 1:
                    config = f"{name}"
                else:
                    config = f"{name} ({num} Comp)"
                zone_info = zone_map.get(model_name, {'system_size': 'None', 'comp': 'None'})
                tot_states = zone_info['system_size'] if pd.notnull(zone_info['system_size']) else 'None'
                tot_states = format_sci_notation(tot_states)
                #components = zone_info['comp'] if pd.notnull(zone_info['comp']) else 'None'
                # check against the single uppaal_time from merged
                uppaal_time = merged.loc[merged['model'] == row['model'], 'Time(ms)'].values
                if len(uppaal_time) == 0 or pd.isnull(uppaal_time[0]):
                    uppaal_time = "-"
                rwtbs_time_val = row.get('check_time_ms', None)
                if pd.notnull(rwtbs_time_val) and float(rwtbs_time_val) >= timed_out_threshold*1000:
                    rwtbs_time = f">${timed_out_threshold}^*$"
                    has_timed_out = True
                elif pd.notnull(rwtbs_time_val):
                    rwtbs_time = trim_float_str(f"{rwtbs_time_val/1000:.3f}")
                else:
                    rwtbs_time = "None"
                rwtbs_mem = trim_float_str(f"{row.get('memory_usage_kb', 'None'):.3f}") if pd.notnull(row.get('memory_usage_kb', None)) else None
                uppaal_mem = "-"

                text_rwtbs_time = f"\\textbf{{{rwtbs_time}}}" if uppaal_time == "-" or (pd.notnull(rwtbs_time) and (uppaal_time == "-" or float(rwtbs_time) <= float(uppaal_time))) else rwtbs_time
                text_uppaal_time = uppaal_time

                text_rwtbs_mem = f"\\textbf{{{rwtbs_mem}}}" if pd.notnull(rwtbs_mem) and (uppaal_mem == "-" or not pd.notnull(uppaal_mem) or (pd.notnull(uppaal_mem) and float(rwtbs_mem) <= float(uppaal_mem))) else rwtbs_mem
                text_uppaal_mem = uppaal_mem    
                latex += rf"{config} & {tot_states}& {text_rwtbs_time} & - & {text_rwtbs_mem} & -" + r"\\"+ "\n"
    
    
    latex += r"""\bottomrule
\end{tabular}
}
"""
    if has_timed_out:
        latex += r"""
    *UPPAAL verification exceeded """ +f"""{int(timed_out_threshold/3600)}""" + r""" hour"""+ ("s"if int(timed_out_threshold/3600) > 1 else "")+ r""" and was terminated.
"""

    latex += r"""\end{table}"""
    with open(f"{output_folder}/{file_name}", "w") as f:
        f.write(latex)
    print(f"LaTeX table saved: {output_folder}/{file_name}")



def plot_comparison_two_figs(merged, use_log=True, output_folder="results", file_name="comparison_bar_chart.png", parallel_df={}, properties=[3,5,10,20, 30]):
    import numpy as np
    import matplotlib.pyplot as plt

    benchmarks = merged['model'].tolist()
    labels = []
    prefix_map = {'S': 'S', 'M': 'M', 'L': 'L', 'XL': 'XL'}
    for model in benchmarks:
        parts = model.split('/')[-1].split('_')
        prefix = parts[0].upper()
        prev = parts[1].split('.')[0]
        name = prefix_map.get(prefix, prefix)
        num = prev.split('n')
        if len(num) == 1:
            snum = int(num[0])
            if snum == 1:
                label = name
            else:
                label = f"{name}({snum})"
        else:
            label = f"{name}({num[0]}, {num[1]} Workers)"
        labels.append(label)

    x = np.arange(len(benchmarks))
    n_bars = 2 + len(parallel_df)
    bar_width = 0.8 / n_bars
    
    # --- Time Figure ---
    fig_time, ax_time = plt.subplots(figsize=figsize)
    time_data = [merged['Time(ms)'], merged['check_time_ms']]
    time_labels = ['Uppaal', 'RTWBS']
    for n_workers,pdf in parallel_df.items():
        pdf = pdf.rename(columns={'model_name': 'model'}).set_index('model')
        time_data.append([pdf.loc[m, 'check_time_ms'] if m in pdf.index else 0 for m in merged['model']])
        time_labels.append(f'RTWBS ({n_workers})')
    #add to time _date merged['Time(ms)'] multiplied by the properties
    for prop in properties:
        time_data.append(merged['Time(ms)']*prop)
        time_labels.append(f'Uppaal {prop} Prop.')

    for i, (y, label) in enumerate(zip(time_data, time_labels)):
        ax_time.bar(x + (i - n_bars/2)*bar_width + bar_width/2, y, bar_width,
                   color=gray_colors[i % len(gray_colors)],
                   label=label,
                   hatch=hatches[i % len(hatches)],
                   alpha=1,
                   edgecolor=border_colors[i % len(border_colors)])
        
    
    
    ax_time.set_xticks(x)
    ax_time.set_xticklabels(labels, rotation=45, ha='right')
    ax_time.set_ylabel('Time (ms)')
    handles, labels_ = ax_time.get_legend_handles_labels()
    ax_time.legend(handles, labels_, loc='best', frameon=True)
    if use_log:
        ax_time.set_yscale('log')
    fig_time.tight_layout()
    
    print(f"Time bar chart saved: {output_folder}/comparison_time.png and .pgf")
    plt.tick_params(axis='x', which='major', pad=-3)
    plt.tick_params(axis='y', which='major', pad=0)
    #plt.gca().yaxis.set_major_formatter(FuncFormatter(formatter))
    plt.tight_layout()
    ax_time.legend(handles, labels_, loc='upper center', ncol=3, frameon=True)
    ax_time.legend(loc='upper center', ncol=3, bbox_to_anchor=(0.5, 1.17))
    fig_time.savefig(f'{output_folder}/comparison_time.png', dpi=600, bbox_inches='tight')
    fig_time.savefig(f'{output_folder}/comparison_time.pgf', dpi=600, bbox_inches='tight')
    plt.close(fig_time)


    

    # --- Memory Figure ---
    fig_mem, ax_mem = plt.subplots(figsize=figsize)
    mem_data = [merged['Memory(KB)'], merged['memory_usage_kb']]
    mem_labels = ['Uppaal', 'RTWBS']
    for n_workers,pdf in parallel_df.items():
        pdf = pdf.rename(columns={'model_name': 'model'}).set_index('model')
        mem_data.append([pdf.loc[m, 'memory_usage_kb'] if m in pdf.index else 0 for m in merged['model']])
        mem_labels.append(f'RTWBS ({n_workers})')
    for i, (y, label) in enumerate(zip(mem_data, mem_labels)):
        ax_mem.bar(x + (i - n_bars/2)*bar_width + bar_width/2, y, bar_width,
                   color=gray_colors[i % len(gray_colors)],
                   label=label,
                   hatch=hatches[i % len(hatches)],
                   alpha=0.9)
    ax_mem.set_xticks(x)
    ax_mem.set_xticklabels(labels, rotation=45, ha='right')
    ax_mem.set_ylabel('Memory (KB)')
    #set ax_mem limit to 10^7
    handles, labels_ = ax_mem.get_legend_handles_labels()
    ax_mem.legend(handles, labels_, loc='best', frameon=True)
    fig_mem.tight_layout()
    if use_log:
        ax_mem.set_yscale('log')
    ax_mem.legend(handles, labels_, loc='upper center', ncol=3, frameon=True)
    ax_mem.legend(loc='upper center', ncol=3, bbox_to_anchor=(0.5, 1.2))
    print(f"Memory bar chart saved: {output_folder}/comparison_memory.png and .pgf")
    plt.tick_params(axis='x', which='major', pad=-3)
    plt.tick_params(axis='y', which='major', pad=0)
    #plt.gca().yaxis.set_major_formatter(FuncFormatter(formatter))
    plt.tight_layout()
    
    fig_mem.savefig(f'{output_folder}/comparison_memory.png', dpi=600, bbox_inches='tight')
    fig_mem.savefig(f'{output_folder}/comparison_memory.pgf', dpi=600, bbox_inches='tight')
    plt.close(fig_mem)



def plot_ratio_vs_uppaal(merged, output_folder="results", file_name="comparison_speedup.png", parallel_df={}):
    """
    Creates a ratio plot (speedup) of RTWBS and parallel RTWBS vs Uppaal baseline.
    Ratio = Uppaal time / Other time
    >1 means faster than Uppaal.
    """

    benchmarks = merged['model'].tolist()
    labels = []
    prefix_map = {'S': 'Small', 'M': 'Medium', 'L': 'Large', 'XL': 'Extra Large'}
    for model in benchmarks:
        parts = model.split('/')[-1].split('_')
        prefix = parts[0].upper()
        prev = parts[1].split('.')[0]
        name = prefix_map.get(prefix, prefix)
        num = prev.split('n')
        if len(num) == 1:
            snum = int(num[0])
            if snum == 1:
                label = name
            else:
                label = f"{name} ({snum})"
        else:
            label = f"{name} ({num[0]}, {num[1]} Workers)"
        labels.append(label)

    x = np.arange(len(benchmarks))
    n_bars = 1 + len(parallel_df)  # RTWBS + parallel versions
    bar_width = 0.8 / (n_bars + 1)

    # --- Ratio Figure ---
    fig, ax = plt.subplots(figsize=(6,4))

    uppaal_times = merged['Time(ms)'].replace(0, np.nan)  # avoid divide by zero
    ratios = []

    # RTWBS ratio
    rtwbs_ratio = uppaal_times / merged['check_time_ms'].replace(0, np.nan)
    ratios.append((rtwbs_ratio, "RTWBS"))

    

    # Parallel RTWBS ratios
    for n_workers, pdf in parallel_df.items():
        pdf = pdf.rename(columns={'model_name': 'model'}).set_index('model')
        par_times = pd.Series([pdf.loc[m, 'check_time_ms'] if m in pdf.index else np.nan for m in merged['model']])
        par_ratio = uppaal_times / par_times.replace(0, np.nan)
        ratios.append((par_ratio, f'RTWBS ({n_workers})'))

    #print(ratios)
    # Plot bars
    for i, (y, label) in enumerate(ratios):
        ax.bar(x + (i - len(ratios)/2)*bar_width + bar_width/2,
               y.fillna(0),  # replace NaN with 0 just for plotting
               bar_width,
               color=gray_colors[i % len(gray_colors)],
               label=label,
               hatch=hatches[i % len(hatches)],
               edgecolor='black')

    ax.axhline(1.0, color="red", linestyle="--", linewidth=1, label="Uppaal baseline")

    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=45, ha='right')
    ax.set_ylabel('Speedup (Uppaal / Tool)')

    # Compute y-limit safely
    valid_max = max([r.replace([np.inf, -np.inf], np.nan).max(skipna=True) for r,_ in ratios] + [1])
    if np.isnan(valid_max) or valid_max <= 0:
        valid_max = 2.0  # fallback if everything invalid
    ax.set_ylim(bottom=0, top=valid_max*1.2)

    handles, labels_ = ax.get_legend_handles_labels()
    ax.legend(handles, labels_, loc='upper center', ncol=3, bbox_to_anchor=(0.5, 1.15))
    ax.set_yscale('log')
    plt.tight_layout()
    fig.savefig(f"{output_folder}/{file_name}", dpi=300, bbox_inches='tight')
    fig.savefig(f"{output_folder}/{file_name.replace('.png','.pgf')}", dpi=300, bbox_inches='tight')
    plt.close(fig)

    print(f"Ratio (speedup) chart saved: {output_folder}/{file_name} and .pgf")


def get_ratios(merged, output_folder="results", file_name="ratios.csv", parallel_df={}):
    rtwbs_ratio =  merged['check_time_ms'].replace(0, 1)
    uppaal_times = merged['Time(ms)'].replace(0, 1)  # avoid divide by zero
    ratios = {'UPPAAL_RTWBS': uppaal_times / rtwbs_ratio}
    for n_workers, pdf in parallel_df.items():
        pdf = pdf.rename(columns={'model_name': 'model'}).set_index('model')
        par_times = pd.Series([pdf.loc[m, 'check_time_ms'] if m in pdf.index else 1 for m in merged['model']])
        par_ratio = uppaal_times / par_times.replace(0, 1)
        ratios[f'UPPAAL_{n_workers}'] = par_ratio
        ratios[f"RTWBS_{n_workers}"] = rtwbs_ratio / par_times

    #save ratios
    ratio_df = pd.DataFrame(ratios)
    ratio_df.insert(0, 'model', merged['model'])
    ratio_df.to_csv(f"{output_folder}/{file_name}", index=False)
    print(f"Ratios saved: {output_folder}/{file_name}")



def plot_ratios(csv_path="results/ratios.csv", output_folder="results"):
    # Load ratios
    df = pd.read_csv(csv_path)

    # --- 1. Log-scale grouped bar chart ---
    methods = [c for c in df.columns if c != "model"]
    ax = df.set_index("model")[methods].plot(
        kind="bar", figsize=(12, 6), logy=True
    )
    ax.set_ylabel("Speedup ratio (log scale)")
    ax.set_title("Speedup ratios across models and methods")
    plt.xticks(rotation=45, ha="right")
    plt.tight_layout()
    plt.savefig(f"{output_folder}/ratios_bar.png", dpi=300)
    plt.close()

    # --- 2. Normalized scatter plot ---
    norm_df = df.copy()
    for col in methods:
        norm_df[col] = df[col] / df["UPPAAL_RTWBS"]

    plt.figure(figsize=(10, 6))
    for col in methods:
        if col == "UPPAAL_RTWBS":
            continue
        plt.scatter(norm_df["model"], norm_df[col], label=col)

    #plt.yscale("log")
    plt.xlabel("Model")
    plt.ylabel("Normalized speedup (vs UPPAAL_RTWBS)")
    plt.title("Normalized speedups relative to UPPAAL_RTWBS")
    plt.legend()
    plt.xticks(rotation=45, ha="right")
    plt.tight_layout()
    plt.savefig(f"{output_folder}/ratios_normalized.png", dpi=300)
    plt.close()

    print(f"Plots saved in {output_folder}/ratios_bar.png and ratios_normalized.png")


# Example call (after you run get_ratios and save ratios.csv):
# plot_ratios("results/ratios.csv", "results")


if __name__ == "__main__":
    csv1_path = "results/upp_bench/results_2025-09-29_09-24-40.csv"
    csv2_path = "results/rtwbs/syn_benchmark_results_20250929_005237.csv"
    csv3_path = "results/rtbws_openmp/syn_benchmark_results_20250929_020000.csv"
    n_workers = "Parallel"
    
    zones_path = "assets/system_size.csv"
    from get_sizes import get_sizes
    get_sizes()
    output_folder = "results"
    output_subfolder = "analysis"

    use_log = True
    if len(sys.argv) > 1 and sys.argv[1] == "-no_log":
        use_log = False

    df1 = pd.read_csv(csv1_path)
    df2 = pd.read_csv(csv2_path)
    df3 = pd.read_csv(csv3_path)

    df1.rename(columns={'Benchmark': 'model'}, inplace=True)
    df2.rename(columns={'model_name': 'model'}, inplace=True)
    df3.rename(columns={'model_name': 'model'}, inplace=True)

    #check if df1 has all the models from df2, in case it doesnt add a row with all Nones
    for model in df2['model']:
        if model == "TOTAL":
            continue
        if model not in df1['model'].values:
            new_row = {'model': model, 'Time(ms)': 0, 'Memory(KB)': 0}
            df1 = pd.concat([df1, pd.DataFrame([new_row])], ignore_index=True)

    merged = pd.merge(df1, df2, on='model', how='inner')

    def get_sort_key(model):
        parts = model.split('/')[-1].split('_')
        prefix = parts[0]
        num = int(parts[1].split('.')[0])
        prefix_order = {'s': 0, 'm': 1, 'l': 2, 'xl': 3}[prefix]
        return (num, prefix_order)

    merged['sort_key'] = merged['model'].apply(get_sort_key)
    merged = merged.sort_values('sort_key')

    #drop last row df3
    df3.drop(index=df3.index[-1], inplace=True)
    df3['sort_key'] = df3['model'].apply(get_sort_key)
    df3 = df3.sort_values('sort_key')

    parallel_df = {n_workers:df3}
    try:
        os.mkdir(f"{output_folder}/{output_subfolder}")
    except:
        print("Folder already exists, overwriting results...")
    output_folder = f"{output_folder}/{output_subfolder}"
    plot_comparison(merged, use_log=use_log, output_folder=output_folder, parallel_df=parallel_df)
    generate_latex_table(merged, zones_path, output_folder=output_folder, parallel_df=parallel_df)
    plot_comparison_two_figs(merged, use_log=use_log, output_folder=output_folder, parallel_df=parallel_df, properties=[])
    plot_ratio_vs_uppaal(merged, output_folder=output_folder, parallel_df=parallel_df)
    get_ratios(merged, output_folder=output_folder, parallel_df=parallel_df)
    plot_ratios(f"{output_folder}/ratios.csv", output_folder=output_folder)




    