#!/usr/bin/env python3
"""Count multiply instructions in a compiler's assembly listing.

The question this answers is whether a compiler folds UMul64's four 32-bit
partial products down to one native multiply when the inputs are 32-bit, which
is what decides whether PhiloxEngine needs a hand-written specialisation.
Ten Philox rounds times two lanes means a fully specialised kernel emits two
multiplies per round; anything much above that is unfolded partial products.
"""
import re
import sys

# GNU: "\timulq ...". MSVC listings: "\timul\trax, rcx".
MUL = re.compile(r"^\s*(imul|mul)[a-z]*\s", re.M | re.I)


def main() -> int:
    path, label = sys.argv[1], sys.argv[2]
    text = open(path, errors="replace").read()
    n = len(MUL.findall(text))
    print(f"{label}: {n} multiply instructions")
    return 0


if __name__ == "__main__":
    sys.exit(main())
