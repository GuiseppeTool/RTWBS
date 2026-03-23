import os
import sys

# Ensure we can import from scripts/ regardless of cwd
_script_dir = os.path.dirname(os.path.abspath(__file__))
if _script_dir not in sys.path:
    sys.path.insert(0, _script_dir)

from BenchmarkGenerator import BenchmarkGenerator, TemplateParams


def generate_eval_benchmarks():
    """
    Create evaluation benchmarks sized by approximate per-automaton zone counts.

    Note: Zone counts depend on exploration and constraints; these parameters are
    chosen heuristically to land near the targets when explored per-automaton.
    """

    out_dir = "assets/syn_eval"

    # Heuristic parameter presets (tuned for smaller, medium, larger zone graphs)
    small = TemplateParams(
        name="EvalS",
        num_states=10,
        num_clocks=2,
        num_int_vars=1,
        branching=3,
        guard_density=0.2,
        invariant_density=0.4,
        reset_density=0.6,
        assign_density=0.4,
        send_events=1,
        recv_events=1,
        env_ratio=0.5,
    )

    medium = TemplateParams(
       name="EvalM",
        num_states=12,
        num_clocks=2,
        num_int_vars=1,
        branching=3,
        guard_density=0.15,
        invariant_density=0.4,
        reset_density=0.5,
        assign_density=0.4,
        send_events=3,
        recv_events=3,
        env_ratio=0.1,
    )

    large = TemplateParams(
       name="EvalL",
        num_states=15,
        num_clocks=2,
        num_int_vars=1,
        branching=3,
        guard_density=0.15,
        invariant_density=0.4,
        reset_density=0.5,
        assign_density=0.4,
        send_events=5,
        recv_events=5,
        env_ratio=0.1,
    )
    xlarge = TemplateParams(
       name="EvalXL",
        num_states=17,
        num_clocks=2,
        num_int_vars=1,
        branching=3,
        guard_density=0.15,
        invariant_density=0.4,
        reset_density=0.6,
        assign_density=0.4,
        send_events=7,
        recv_events=7,
        env_ratio=0.1,
    )


    xxlarge = TemplateParams(
       name="EvalXXL",
        num_states=20,
        num_clocks=2,
        num_int_vars=1,
        branching=3,
        guard_density=0.4,
        invariant_density=0.4,
        reset_density=0.64,
        assign_density=0.4,
        send_events=9,
        recv_events=9,
        env_ratio=0.1,
    )

 

   

    # Generate three single-file NTAs with 5 templates each
    configs = [
        ("s", small),
        ("m", medium),
        ("l", large),
        ("xl", xlarge),
        ("xxl", xxlarge),
    ]

    saved = []
    base_seed = 42
    size_idx = {"s": 0, "m": 1, "l": 2, "xl": 3}
    for aut_count in [1,3,5,7]:
        for filename, params in configs:
            # Deterministic per-config seed so each (size, count) pair is
            # independent.  This guarantees monotonic zone growth when
            # only the component count changes.
            #cfg_seed = base_seed #+ size_idx.get(filename, 99) * 100 + aut_count
            gen = BenchmarkGenerator(seed=base_seed)
            templates = gen.generate_templates(count=aut_count, base_name=params.name + "_", params=params)
            nta = gen.build_nta(templates, system_name=filename)
            saved.append(gen.save_xml(nta, out_dir=out_dir, filename=f"{filename}_{aut_count}.xml"))

    print("Saved evaluation benchmarks:")
    for p in saved:
        print(f"  {p}")


def test():
    # Keep the previous demo generation for convenience
    gen = BenchmarkGenerator(seed=42)
    base = TemplateParams(
        name="Stress",
        num_states=12,
        num_clocks=4,
        num_int_vars=3,
        branching=4,
        guard_density=0.8,
        invariant_density=0.6,
        reset_density=0.7,
        assign_density=0.6,
        send_events=5,
        recv_events=5,
        env_ratio=0.1,
    )
    templates = gen.generate_templates(count=4, base_name="Stress_", params=base)
    nta = gen.build_nta(templates, system_name="StressSystem")
    single_path = gen.save_xml(nta, out_dir="assets/demo", filename="demo.xml")
    print("Saved single-file demo:")
    print(f"  {single_path}")


if __name__ == "__main__":
    # Default to producing evaluation-sized benchmarks
    generate_eval_benchmarks()
