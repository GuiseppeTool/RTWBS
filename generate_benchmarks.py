try:
    from BenchmarkGenerator import BenchmarkGenerator, TemplateParams
except ModuleNotFoundError:
    import os
    import sys
    sys.path.append(os.path.dirname(os.path.dirname(__file__)))
    from BenchmarkGenerator import BenchmarkGenerator, TemplateParams


def generate_eval_benchmarks():
    """
    Create evaluation benchmarks sized by approximate per-automaton zone counts.
    Targets (sum across 5 automata):
      - Small  ~ 5 x ~100  ≈ 500
      - Medium ~ 5 x ~1000 ≈ 5,000
      - Large  ~ 5 x ~10,000 ≈ 50,000

    Note: Zone counts depend on exploration and constraints; these parameters are
    chosen heuristically to land near the targets when explored per-automaton.
    """
    gen = BenchmarkGenerator(seed=42)

    out_dir = "assets/eval"

    # Heuristic parameter presets (tuned for smaller, medium, larger zone graphs)
    small = TemplateParams(
        name="EvalS",
        num_states=7,
        num_clocks=2,
        num_int_vars=1,
        branching=3,
        guard_density=0.2,
        invariant_density=0.4,
        reset_density=0.6,
        assign_density=0.4,
        sync_density=0.5,
    )

    medium = TemplateParams(
       name="EvalM",
        num_states=15,
        num_clocks=2,
        num_int_vars=1,
        branching=3,
        guard_density=0.15,
        invariant_density=0.4,
        reset_density=0.5,
        assign_density=0.4,
        sync_density=0.5,
    )

    large = TemplateParams(
       name="EvalL",
        num_states=25,
        num_clocks=2,
        num_int_vars=1,
        branching=3,
        guard_density=0.2,
        invariant_density=0.4,
        reset_density=0.63,
        assign_density=0.4,
        sync_density=0.5,
    )
    xlarge = TemplateParams(
       name="EvalXL",
        num_states=36,
        num_clocks=2,
        num_int_vars=1,
        branching=3,
        guard_density=0.15,
        invariant_density=0.4,
        reset_density=0.6,
        assign_density=0.4,
        sync_density=0.5,
    )


    xxlarge = TemplateParams(
       name="EvalXXL",
        num_states=17,
        num_clocks=3,
        num_int_vars=1,
        branching=3,
        guard_density=0.4,
        invariant_density=0.4,
        reset_density=0.64,
        assign_density=0.4,
        sync_density=0.5,
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
    for aut_count in [5,3,1]:
        for filename, params in configs:
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
        sync_density=0.7,
    )
    templates = gen.generate_templates(count=4, base_name="Stress_", params=base)
    nta = gen.build_nta(templates, system_name="StressSystem")
    single_path = gen.save_xml(nta, out_dir="assets/demo", filename="demo.xml")
    print("Saved single-file demo:")
    print(f"  {single_path}")


if __name__ == "__main__":
    # Default to producing evaluation-sized benchmarks
    generate_eval_benchmarks()
