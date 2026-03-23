# Generate multiple UPPAAL model copies while sweeping Delta from 0..n in steps of m.
# Uses direct text-based file manipulation to avoid pyuppaal/lxml injecting a
# <!DOCTYPE> declaration that confuses the utap XML reader.

import os
import re
import shutil


_DELTA_ASSIGN_RE = re.compile(r"(\bDelta\b\s*=\s*)(-?\d+)")


def make_delta_copy(src_path: str, dst_path: str, delta_value: int) -> None:
    """
    Copy src_path to dst_path, replacing the first 'Delta = <number>'
    assignment in the file with 'Delta = <delta_value>'.

    Writes the file as plain text (no XML re-serialisation) so that no
    <!DOCTYPE> header is injected, which would otherwise break the utap
    XML reader with an '$unexpected $end' error.
    """
    with open(src_path, "r", encoding="utf-8") as f:
        content = f.read()

    if not _DELTA_ASSIGN_RE.search(content):
        raise ValueError(
            f"Could not find a 'Delta = <number>' assignment in '{src_path}'.\n"
            "Hint: make sure the UPPAAL global declaration contains something like:\n"
            "  int Delta = 0;\n"
            "or:\n"
            "  const int Delta = 0;\n"
        )

    new_content = _DELTA_ASSIGN_RE.sub(rf"\g<1>{delta_value}", content, count=1)

    with open(dst_path, "w", encoding="utf-8") as f:
        f.write(new_content)


if __name__ == "__main__":
    model_path = "assets/SprayingUseCase/V2_nob.xml"

    # Get n and m from user input
    n = int(input("Enter the value of n (max Delta): "))
    m = int(input("Enter the value of m (step): "))
    if m <= 0:
        raise ValueError("m must be a positive integer.")

    # Create output directory next to this script
    output_dir = os.path.join(os.path.dirname(__file__), f"analysis_{n}_{m}")
    os.makedirs(output_dir, exist_ok=True)

    # Save the original model as V1.xml (byte-for-byte copy, no re-serialisation)
    shutil.copy2(model_path, os.path.join(output_dir, "V1.xml"))

    # Create swept copies: Delta = 0, m, 2m, ... up to <= n
    for delta in range(0, n + 1, m):
        out_path = os.path.join(output_dir, f"V2_nob_{delta}.xml")
        make_delta_copy(model_path, out_path, delta)

    print(f"Done. Wrote models to: {output_dir}")