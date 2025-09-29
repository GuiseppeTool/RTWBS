import pandas as pd
import matplotlib.pyplot as plt
import sys
import numpy as np
from matplotlib.ticker import FuncFormatter
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


figsize = (4.4, 3)


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
\caption{Comparison of Verification Time and Memory Footprint of RTWBS vs. UPPAAL. The parallelization has been achieved using OpenMP with 4 workers. The best time and memory values for each configuration are highlighted in bold.}
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
    timed_out_threshold = 24 * 3600  # 24 hours in seconds
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
            uppaal_time = f">${timed_out_threshold}^*$"
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
                    rwtbs_time_par = f">${timed_out_threshold}^*$"
                    has_timed_out = True
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
        
        text_uppaal_time = f"\\textbf{{{uppaal_time}}}" if min_time is not None and float(uppaal_time) == min_time else uppaal_time
        text_rwtbs_time = f"\\textbf{{{rwtbs_time}}}" if min_time is not None and float(rwtbs_time) == min_time else rwtbs_time
        text_rwtbs_time_par = f"\\textbf{{{rwtbs_time_par}}}" if min_time is not None and rwtbs_time_par not in [None, "-", "None"] and float(rwtbs_time_par) == min_time else rwtbs_time_par


        text_rwtbs_mem = f"\\textbf{{{rwtbs_mem}}}" if pd.notnull(rwtbs_mem) and (not pd.notnull(uppaal_mem) or (pd.notnull(uppaal_mem) and float(rwtbs_mem) <= float(uppaal_mem))) else rwtbs_mem
        text_uppaal_mem = f"\\textbf{{{uppaal_mem}}}" if pd.notnull(rwtbs_mem) and pd.notnull(uppaal_mem) and float(uppaal_mem) < float(rwtbs_mem) else uppaal_mem


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
        latex += r"""\begin{flushleft}
    *UPPAAL verification exceeded """ +f"""{int(timed_out_threshold/3600)}""" + r""" hour"""+ ("s"if int(timed_out_threshold/3600) > 1 else "")+ r""" and was terminated.
\end{flushleft}
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
    plt.tick_params(axis='x', which='major', pad=1)
    plt.tick_params(axis='y', which='major', pad=0)
    #plt.gca().yaxis.set_major_formatter(FuncFormatter(formatter))
    plt.tight_layout()
    ax_time.legend(handles, labels_, loc='upper center', ncol=3, frameon=True)
    ax_time.legend(loc='upper center', ncol=3, bbox_to_anchor=(0.5, 1.17))
    fig_time.savefig(f'{output_folder}/comparison_time.png', dpi=300, bbox_inches='tight')
    fig_time.savefig(f'{output_folder}/comparison_time.pgf', dpi=300, bbox_inches='tight')
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
    handles, labels_ = ax_mem.get_legend_handles_labels()
    ax_mem.legend(handles, labels_, loc='best', frameon=True)
    fig_mem.tight_layout()
    if use_log:
        ax_mem.set_yscale('log')
    ax_mem.legend(handles, labels_, loc='upper center', ncol=3, frameon=True)
    ax_mem.legend(loc='upper center', ncol=3, bbox_to_anchor=(0.5, 1.2))
    print(f"Memory bar chart saved: {output_folder}/comparison_memory.png and .pgf")
    plt.tick_params(axis='x', which='major', pad=1)
    plt.tick_params(axis='y', which='major', pad=0)
    #plt.gca().yaxis.set_major_formatter(FuncFormatter(formatter))
    plt.tight_layout()
    
    fig_mem.savefig(f'{output_folder}/comparison_memory.png', dpi=600, bbox_inches='tight')
    fig_mem.savefig(f'{output_folder}/comparison_memory.pgf', dpi=600, bbox_inches='tight')
    plt.close(fig_mem)


if __name__ == "__main__":
    csv1_path = "results/uppaal/results_2025-09-29_09-24-35.csv"
    csv2_path = "results/rtwbs/syn_benchmark_results_20250929_005237.csv"
    csv3_path = "results/rtbws_openmp/syn_benchmark_results_20250929_020000.csv"
    n_workers = "Parallel"
    
    zones_path = "assets/system_size.csv"
    output_folder = "results"

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

    plot_comparison(merged, use_log=use_log, output_folder=output_folder, parallel_df=parallel_df)
    generate_latex_table(merged, zones_path, output_folder=output_folder, parallel_df=parallel_df)
    plot_comparison_two_figs(merged, use_log=use_log, output_folder=output_folder, parallel_df=parallel_df, properties=[])





    