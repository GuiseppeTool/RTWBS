#!/usr/bin/env python3
"""
Benchmark Results Analysis for SEAMS Publication
===============================================

This script analyzes the latest benchmark results from the TimedAutomata verification tool,
generating publication-quality graphs and statistics suitable for academic conferences
like SEAMS (Software Engineering for Adaptive and Self-Managing Systems).

Author: Generated for TimedAutomata project
Date: September 2025
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
import glob
from datetime import datetime
import matplotlib.patches as mpatches
from matplotlib.ticker import FuncFormatter
from scipy import stats
import warnings
warnings.filterwarnings('ignore')

# Publication-quality style settings for SEAMS
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams.update({
    'font.size': 12,
    'axes.titlesize': 14,
    'axes.labelsize': 12,
    'xtick.labelsize': 10,
    'ytick.labelsize': 10,
    'legend.fontsize': 10,
    'figure.titlesize': 16,
    'figure.dpi': 300,
    'savefig.dpi': 300,
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0.1
})

# Color palette for publication
colors = {
    'primary': '#2E86AB',
    'secondary': '#A23B72', 
    'accent': '#F18F01',
    'success': '#C73E1D',
    'neutral': '#6C757D',
    'light': '#E9ECEF'
}

class BenchmarkAnalyzer:
    def __init__(self, results_dir="results"):
        self.results_dir = Path(results_dir)
        self.benchmark_data = None
        self.comparison_data = None
        self.prefix = "ASTRail_"
        self.output_dir = Path(f"{self.prefix}analysis_output")
        self.output_dir.mkdir(exist_ok=True)
        
    def load_latest_results(self):
        """Load the most recent benchmark and comparison results."""
        # Find the latest benchmark results file
        benchmark_files = glob.glob(str(self.results_dir / f"{self.prefix}benchmark_results_*.csv"))
        comparison_files = glob.glob(str(self.results_dir / f"{self.prefix}comparison_results_*.csv"))
        
        if not benchmark_files:
            raise FileNotFoundError("No benchmark results found in results directory")
            
        latest_benchmark = max(benchmark_files, key=lambda x: Path(x).stat().st_mtime)
        latest_comparison = max(comparison_files, key=lambda x: Path(x).stat().st_mtime) if comparison_files else None
        
        print(f"Loading benchmark data from: {latest_benchmark}")
        self.benchmark_data = pd.read_csv(latest_benchmark)
        
        if latest_comparison:
            print(f"Loading comparison data from: {latest_comparison}")
            self.comparison_data = pd.read_csv(latest_comparison)
        
        # Clean and preprocess data
        self._preprocess_data()
        
    def _preprocess_data(self):
        """Clean and preprocess the loaded data."""
        # Remove TOTAL row if present
        self.benchmark_data = self.benchmark_data[self.benchmark_data['model_name'] != 'TOTAL']
        
        # Extract benchmark suite information
        self.benchmark_data['benchmark_suite'] = self.benchmark_data['model_name'].apply(
            lambda x: x.split('/')[0] if '/' in x else 'Other'
        )
        
        # Extract model name without path
        self.benchmark_data['model_short_name'] = self.benchmark_data['model_name'].apply(
            lambda x: Path(x).stem
        )
        
        # Convert memory to MB for better readability
        self.benchmark_data['memory_usage_mb'] = self.benchmark_data['memory_usage_kb'] / 1024
        
        # Sort by states for consistent ordering
        self.benchmark_data = self.benchmark_data.sort_values('refined_states')
        
    def generate_state_space_analysis(self):
        """Generate state space analysis graphs."""
        fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('State Space Analysis of Timed Automata Models', fontsize=16, fontweight='bold')
        
        # 1. States vs Memory Usage
        scatter = ax1.scatter(self.benchmark_data['refined_states'], 
                            self.benchmark_data['memory_usage_mb'],
                            c=self.benchmark_data.index, 
                            cmap='viridis', 
                            alpha=0.7, s=60)
        ax1.set_xlabel('Number of Refined States')
        ax1.set_ylabel('Memory Usage (MB)')
        ax1.set_title('Memory Consumption vs State Space Size')
        ax1.grid(True, alpha=0.3)
        
        # Add trend line
        z = np.polyfit(self.benchmark_data['refined_states'], self.benchmark_data['memory_usage_mb'], 1)
        p = np.poly1d(z)
        ax1.plot(self.benchmark_data['refined_states'], p(self.benchmark_data['refined_states']), 
                "r--", alpha=0.8, linewidth=2)
        
        # 2. Distribution of refined vs abstract states
        ax2.scatter(self.benchmark_data['refined_states'], 
                   self.benchmark_data['abstract_states'],
                   alpha=0.7, color=colors['primary'], s=60)
        ax2.plot([0, max(self.benchmark_data['refined_states'])], 
                [0, max(self.benchmark_data['refined_states'])], 
                'r--', alpha=0.5, label='Perfect Abstraction')
        ax2.set_xlabel('Refined States')
        ax2.set_ylabel('Abstract States')
        ax2.set_title('State Abstraction Effectiveness')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # 3. Benchmark suite comparison
        suite_stats = self.benchmark_data.groupby('benchmark_suite').agg({
            'refined_states': 'mean',
            'memory_usage_mb': 'mean',
            'simulation_pairs': 'mean'
        }).reset_index()
        
        bars = ax3.bar(suite_stats['benchmark_suite'], suite_stats['refined_states'], 
                      color=[colors['primary'], colors['secondary'], colors['accent']][:len(suite_stats)])
        ax3.set_xlabel('Benchmark Suite')
        ax3.set_ylabel('Average Refined States')
        ax3.set_title('State Space Size by Benchmark Suite')
        ax3.tick_params(axis='x', rotation=45)
        
        # Add value labels on bars
        for bar in bars:
            height = bar.get_height()
            ax3.text(bar.get_x() + bar.get_width()/2., height,
                    f'{int(height)}', ha='center', va='bottom')
        
        # 4. Simulation pairs vs states
        ax4.scatter(self.benchmark_data['refined_states'], 
                   self.benchmark_data['simulation_pairs'],
                   alpha=0.7, color=colors['accent'], s=60)
        ax4.set_xlabel('Refined States')
        ax4.set_ylabel('Simulation Pairs')
        ax4.set_title('Simulation Complexity vs State Space')
        ax4.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'state_space_analysis.png')
        plt.show()
        
    def generate_performance_analysis(self):
        """Generate performance analysis graphs."""
        fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Performance Analysis of Verification Tool', fontsize=16, fontweight='bold')
        
        # 1. Memory efficiency scatter plot
        efficiency = self.benchmark_data['refined_states'] / self.benchmark_data['memory_usage_mb']
        scatter = ax1.scatter(self.benchmark_data['refined_states'], efficiency,
                            c=self.benchmark_data['benchmark_suite'].astype('category').cat.codes,
                            cmap='Set1', alpha=0.7, s=60)
        ax1.set_xlabel('Refined States')
        ax1.set_ylabel('States per MB (Efficiency)')
        ax1.set_title('Memory Efficiency Analysis')
        ax1.grid(True, alpha=0.3)
        
        # Add legend for benchmark suites
        suites = self.benchmark_data['benchmark_suite'].unique()
        handles = [plt.scatter([], [], c=plt.cm.Set1(i), alpha=0.7, s=60) 
                  for i in range(len(suites))]
        ax1.legend(handles, suites, title='Benchmark Suite', loc='best')
        
        # 2. Memory usage distribution
        ax2.hist(self.benchmark_data['memory_usage_mb'], bins=15, 
                alpha=0.7, color=colors['primary'], edgecolor='black')
        ax2.axvline(self.benchmark_data['memory_usage_mb'].mean(), 
                   color='red', linestyle='--', linewidth=2, label='Mean')
        ax2.axvline(self.benchmark_data['memory_usage_mb'].median(), 
                   color='orange', linestyle='--', linewidth=2, label='Median')
        ax2.set_xlabel('Memory Usage (MB)')
        ax2.set_ylabel('Number of Models')
        ax2.set_title('Distribution of Memory Usage')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # 3. State space scaling
        model_sizes = pd.cut(self.benchmark_data['refined_states'], 
                           bins=[0, 200, 400, 600, float('inf')],
                           labels=['Small (≤200)', 'Medium (201-400)', 
                                  'Large (401-600)', 'Very Large (>600)'])
        
        size_stats = self.benchmark_data.groupby(model_sizes).agg({
            'memory_usage_mb': ['mean', 'std'],
            'simulation_pairs': ['mean', 'std']
        })
        
        x_pos = np.arange(len(size_stats))
        bars = ax3.bar(x_pos, size_stats[('memory_usage_mb', 'mean')],
                      yerr=size_stats[('memory_usage_mb', 'std')],
                      capsize=5, color=colors['secondary'], alpha=0.8)
        ax3.set_xlabel('Model Size Category')
        ax3.set_ylabel('Average Memory Usage (MB)')
        ax3.set_title('Memory Usage by Model Size')
        ax3.set_xticks(x_pos)
        ax3.set_xticklabels(size_stats.index)
        ax3.grid(True, alpha=0.3)
        
        # 4. Correlation matrix
        corr_data = self.benchmark_data[['refined_states', 'abstract_states', 
                                       'simulation_pairs', 'memory_usage_mb']].corr()
        im = ax4.imshow(corr_data, cmap='RdBu', vmin=-1, vmax=1)
        ax4.set_xticks(range(len(corr_data.columns)))
        ax4.set_yticks(range(len(corr_data.columns)))
        ax4.set_xticklabels(['Refined\nStates', 'Abstract\nStates', 
                           'Simulation\nPairs', 'Memory\n(MB)'])
        ax4.set_yticklabels(['Refined\nStates', 'Abstract\nStates', 
                           'Simulation\nPairs', 'Memory\n(MB)'])
        ax4.set_title('Feature Correlation Matrix')
        
        # Add correlation values
        for i in range(len(corr_data.columns)):
            for j in range(len(corr_data.columns)):
                ax4.text(j, i, f'{corr_data.iloc[i, j]:.2f}',
                        ha='center', va='center', 
                        color='white' if abs(corr_data.iloc[i, j]) > 0.5 else 'black')
        
        plt.colorbar(im, ax=ax4, shrink=0.8)
        plt.tight_layout()
        plt.savefig(self.output_dir / 'performance_analysis.png')
        plt.show()
        
    def generate_benchmark_comparison(self):
        """Generate detailed benchmark suite comparison."""
        if len(self.benchmark_data['benchmark_suite'].unique()) < 2:
            print("Skipping benchmark comparison - only one suite found")
            return
            
        fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Benchmark Suite Comparison Analysis', fontsize=16, fontweight='bold')
        
        # 1. Box plot of states by suite
        self.benchmark_data.boxplot(column='refined_states', by='benchmark_suite', ax=ax1)
        ax1.set_title('State Space Distribution by Suite')
        ax1.set_xlabel('Benchmark Suite')
        ax1.set_ylabel('Refined States')
        ax1.grid(True, alpha=0.3)
        
        # 2. Box plot of memory by suite
        self.benchmark_data.boxplot(column='memory_usage_mb', by='benchmark_suite', ax=ax2)
        ax2.set_title('Memory Usage Distribution by Suite')
        ax2.set_xlabel('Benchmark Suite')
        ax2.set_ylabel('Memory Usage (MB)')
        ax2.grid(True, alpha=0.3)
        
        # 3. Radar chart for multi-dimensional comparison
        suites = self.benchmark_data['benchmark_suite'].unique()
        metrics = ['refined_states', 'abstract_states', 'simulation_pairs', 'memory_usage_mb']
        
        # Normalize data for radar chart
        norm_data = self.benchmark_data.groupby('benchmark_suite')[metrics].mean()
        for col in metrics:
            norm_data[col] = (norm_data[col] - norm_data[col].min()) / (norm_data[col].max() - norm_data[col].min())
        
        angles = np.linspace(0, 2*np.pi, len(metrics), endpoint=False).tolist()
        angles += angles[:1]  # Complete the circle
        
        ax3.set_theta_offset(np.pi / 2)
        ax3.set_theta_direction(-1)
        ax3.set_thetagrids(np.degrees(angles[:-1]), 
                          ['Refined\nStates', 'Abstract\nStates', 'Simulation\nPairs', 'Memory\nUsage'])
        
        for i, suite in enumerate(suites):
            values = norm_data.loc[suite].tolist()
            values += values[:1]  # Complete the circle
            ax3.plot(angles, values, 'o-', linewidth=2, 
                    label=suite, color=plt.cm.Set1(i))
            ax3.fill(angles, values, alpha=0.25, color=plt.cm.Set1(i))
        
        ax3.set_ylim(0, 1)
        ax3.set_title('Multi-dimensional Suite Comparison')
        ax3.legend(loc='upper right', bbox_to_anchor=(1.3, 1.0))
        ax3.grid(True)
        
        # 4. Statistical comparison
        suite_summary = self.benchmark_data.groupby('benchmark_suite').agg({
            'refined_states': ['count', 'mean', 'std'],
            'memory_usage_mb': ['mean', 'std'],
            'simulation_pairs': ['mean', 'std']
        }).round(2)
        
        # Create table
        ax4.axis('tight')
        ax4.axis('off')
        table_data = []
        for suite in suites:
            row = [
                suite,
                int(suite_summary.loc[suite, ('refined_states', 'count')]),
                f"{suite_summary.loc[suite, ('refined_states', 'mean')]:.0f}±{suite_summary.loc[suite, ('refined_states', 'std')]:.0f}",
                f"{suite_summary.loc[suite, ('memory_usage_mb', 'mean')]:.2f}±{suite_summary.loc[suite, ('memory_usage_mb', 'std')]:.2f}",
                f"{suite_summary.loc[suite, ('simulation_pairs', 'mean')]:.0f}±{suite_summary.loc[suite, ('simulation_pairs', 'std')]:.0f}"
            ]
            table_data.append(row)
        
        table = ax4.table(cellText=table_data,
                         colLabels=['Suite', 'Models', 'States (μ±σ)', 'Memory MB (μ±σ)', 'Sim Pairs (μ±σ)'],
                         cellLoc='center',
                         loc='center')
        table.auto_set_font_size(False)
        table.set_fontsize(10)
        table.scale(1, 1.5)
        ax4.set_title('Statistical Summary by Suite')
        
        plt.tight_layout()
        
        plt.savefig(self.output_dir / 'benchmark_comparison.png')
        plt.show()
        
    def generate_scalability_analysis(self):
        """Generate scalability analysis for publication."""
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
        fig.suptitle('Scalability Analysis for SEAMS Publication', fontsize=16, fontweight='bold')
        
        # 1. Memory scaling with logarithmic fit
        x = self.benchmark_data['refined_states']
        y = self.benchmark_data['memory_usage_mb']
        
        # Fit different scaling models
        linear_fit = np.polyfit(x, y, 1)
        quad_fit = np.polyfit(x, y, 2)
        
        x_smooth = np.linspace(x.min(), x.max(), 100)
        linear_pred = np.polyval(linear_fit, x_smooth)
        quad_pred = np.polyval(quad_fit, x_smooth)
        
        ax1.scatter(x, y, alpha=0.7, color=colors['primary'], s=60, label='Observed')
        ax1.plot(x_smooth, linear_pred, '--', color=colors['secondary'], 
                linewidth=2, label=f'Linear (R² = {stats.pearsonr(x, y)[0]**2:.3f})')
        ax1.plot(x_smooth, quad_pred, '-', color=colors['accent'], 
                linewidth=2, label='Quadratic')
        
        ax1.set_xlabel('State Space Size')
        ax1.set_ylabel('Memory Usage (MB)')
        ax1.set_title('Memory Scaling Behavior')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # 2. Efficiency metrics over model size
        efficiency_states = x / y  # States per MB
        efficiency_pairs = self.benchmark_data['simulation_pairs'] / y  # Pairs per MB
        
        ax2.scatter(x, efficiency_states, alpha=0.7, color=colors['primary'], 
                   s=60, label='States/MB')
        ax2.scatter(x, efficiency_pairs, alpha=0.7, color=colors['secondary'], 
                   s=60, label='Sim Pairs/MB')
        
        ax2.set_xlabel('State Space Size')
        ax2.set_ylabel('Efficiency (per MB)')
        ax2.set_title('Tool Efficiency vs Model Size')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'scalability_analysis.png')
        plt.show()
        
    def generate_summary_statistics(self):
        """Generate comprehensive summary statistics."""
        print("\n" + "="*60)
        print("BENCHMARK ANALYSIS SUMMARY")
        print("="*60)
        
        total_models = len(self.benchmark_data)
        total_states = self.benchmark_data['refined_states'].sum()
        total_memory = self.benchmark_data['memory_usage_mb'].sum()
        total_pairs = self.benchmark_data['simulation_pairs'].sum()
        
        print(f"Total Models Analyzed: {total_models}")
        print(f"Total States Processed: {total_states:,}")
        print(f"Total Memory Usage: {total_memory:.2f} MB")
        print(f"Total Simulation Pairs: {total_pairs:,}")
        print()
        
        print("PERFORMANCE STATISTICS:")
        print("-" * 30)
        print(f"Average States per Model: {self.benchmark_data['refined_states'].mean():.1f} ± {self.benchmark_data['refined_states'].std():.1f}")
        print(f"Average Memory per Model: {self.benchmark_data['memory_usage_mb'].mean():.2f} ± {self.benchmark_data['memory_usage_mb'].std():.2f} MB")
        print(f"Memory Efficiency: {(total_states/total_memory):.1f} states/MB")
        print()
        
        print("BENCHMARK SUITE BREAKDOWN:")
        print("-" * 30)
        suite_stats = self.benchmark_data.groupby('benchmark_suite').agg({
            'model_name': 'count',
            'refined_states': ['mean', 'max'],
            'memory_usage_mb': ['mean', 'max']
        }).round(2)
        
        for suite in self.benchmark_data['benchmark_suite'].unique():
            count = suite_stats.loc[suite, ('model_name', 'count')]
            avg_states = suite_stats.loc[suite, ('refined_states', 'mean')]
            max_states = suite_stats.loc[suite, ('refined_states', 'max')]
            avg_mem = suite_stats.loc[suite, ('memory_usage_mb', 'mean')]
            print(f"{suite}: {count} models, avg {avg_states:.0f} states (max {max_states:.0f}), avg {avg_mem:.2f} MB")
        
        # Save statistics to file
        with open(self.output_dir / 'summary_statistics.txt', 'w') as f:
            f.write(f"Benchmark Analysis Summary - Generated on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write("="*80 + "\n\n")
            f.write(f"Total Models: {total_models}\n")
            f.write(f"Total States: {total_states:,}\n")
            f.write(f"Total Memory: {total_memory:.2f} MB\n")
            f.write(f"Average Memory per Model: {self.benchmark_data['memory_usage_mb'].mean():.2f} MB\n")
            f.write(f"Memory Efficiency: {(total_states/total_memory):.1f} states/MB\n\n")
            
            f.write("Detailed Statistics by Suite:\n")
            f.write("-" * 40 + "\n")
            for suite in self.benchmark_data['benchmark_suite'].unique():
                subset = self.benchmark_data[self.benchmark_data['benchmark_suite'] == suite]
                f.write(f"\n{suite}:\n")
                f.write(f"  Models: {len(subset)}\n")
                f.write(f"  States: {subset['refined_states'].mean():.1f} ± {subset['refined_states'].std():.1f}\n")
                f.write(f"  Memory: {subset['memory_usage_mb'].mean():.2f} ± {subset['memory_usage_mb'].std():.2f} MB\n")
        
def main():
    """Main analysis function."""
    print("Starting Benchmark Analysis for SEAMS Publication...")
    print("-" * 50)
    
    analyzer = BenchmarkAnalyzer()
    
    try:
        # Load the latest results
        analyzer.load_latest_results()
        
        # Generate all analysis graphs
        print("\nGenerating state space analysis...")
        analyzer.generate_state_space_analysis()
        
        print("\nGenerating performance analysis...")
        analyzer.generate_performance_analysis()
        
        print("\nGenerating benchmark comparison...")
        analyzer.generate_benchmark_comparison()
        
        print("\nGenerating scalability analysis...")
        analyzer.generate_scalability_analysis()
        
        # Generate summary statistics
        analyzer.generate_summary_statistics()
        
        print(f"\n" + "="*60)
        print("ANALYSIS COMPLETE!")
        print(f"All graphs and statistics saved to: {analyzer.output_dir}")
        print("Files generated:")
        print("  • summary_statistics.txt")
        print("="*60)
        
    except Exception as e:
        print(f"Error during analysis: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
