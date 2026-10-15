"""Materialize the tested 24-bit rescaler fast path outside pinned OKGF."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil


def prepare(source: Path, destination: Path) -> Path:
    original = source / "src/rescale.c"
    if not original.is_file():
        raise RuntimeError("pinned OKGF rescaler is unavailable")
    stamp = destination / "e2e5-rescale-inputs.json"
    inputs = {
        "rescale_sha256": hashlib.sha256(original.read_bytes()).hexdigest(),
        "override_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }
    expected = json.dumps(inputs, sort_keys=True, indent=2) + "\n"
    if stamp.is_file() and stamp.read_text(encoding="utf-8") == expected:
        return destination
    shutil.copytree(source, destination, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns(".git"))
    target = destination / "src/rescale.c"
    text = target.read_text(encoding="utf-8")
    old_rounding = '''#ifdef OKGF_PORTABLE_MATH
    int host_rounding = 0;
#else
    int host_rounding = FLT_EVAL_METHOD == 0 && fegetround() == FE_TONEAREST;
#endif'''
    new_rounding = '''    int host_rounding = FLT_EVAL_METHOD == 0 && fegetround() == FE_TONEAREST;'''
    old_mode = '''        rows[i].arithmetic = host_rounding && (precision == 24 || precision == 53) ? precision : 64;'''
    new_mode = '''#ifdef OKGF_PORTABLE_MATH
        // ARM64 has IEEE binary32 arithmetic. At game precision, a product
        // and sum round at each operation just as in the 24-bit x87 mode.
        // Other precisions retain the exact software accumulator.
        rows[i].arithmetic = host_rounding && precision == 24 ? 24 : 64;
#else
        rows[i].arithmetic = host_rounding && (precision == 24 || precision == 53) ? precision : 64;
#endif'''
    old_guard = '''#ifndef OKGF_NATIVE_MATH
            /* Host float/double operations reproduce 24-/53-bit x87 rounding when all inputs,'''
    new_guard = '''#if !defined(OKGF_NATIVE_MATH) && !defined(OKGF_PORTABLE_MATH)
            /* Host float/double operations reproduce 24-/53-bit x87 rounding when all inputs,'''
    for before, after in ((old_rounding, new_rounding), (old_mode, new_mode),
                          (old_guard, new_guard)):
        if text.count(before) != 1:
            raise RuntimeError("pinned OKGF rescale boundary changed")
        text = text.replace(before, after, 1)
    target.write_text(text, encoding="utf-8")
    stamp.write_text(expected, encoding="utf-8")
    return destination
