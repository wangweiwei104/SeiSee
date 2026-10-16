"""Regenerate golden values from the user-selected fk_spectrum_fixed.py.

Usage: python tests/generate_fk_reference.py /path/to/fk_spectrum_fixed.py
Requires the reference script's NumPy, Matplotlib, segyio and tqdm dependencies.
The application itself does not depend on Python.
"""
import hashlib
import importlib.util
import json
import sys
from pathlib import Path
import numpy as np

source = Path(sys.argv[1]).resolve()
spec = importlib.util.spec_from_file_location("fk_reference", source)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
cases = []
for nx, nt, pre in [(7, 37, False), (7, 37, True), (6, 32, True), (17, 31, True), (8, 40, False)]:
    x = np.arange(nx, dtype=np.float64)[:, None]
    t = np.arange(nt, dtype=np.float64)[None, :]
    data = (np.cos(2*np.pi*(3*t/nt-2*x/nx)) + .3*np.sin(.37*t+.19*x) + .025*x + .03*t).astype(np.float32)
    amplitude, f, k = mod.compute_fk_spectrum(data, .002, 12.5, pre)
    positive = f >= 0
    cases.append(dict(nx=nx, nt=nt, preprocess=pre, dt=.002, dx=12.5,
                      input=data.ravel().tolist(), amplitude=amplitude[positive].ravel().tolist(),
                      frequency=f[positive].tolist(), wavenumber=k.tolist()))
output = dict(reference=source.name, sha256=hashlib.sha256(source.read_bytes()).hexdigest(), cases=cases)
Path(__file__).with_name("fk_reference.json").write_text(json.dumps(output, separators=(",", ":")), encoding="utf-8")
print("Generated 5 reference cases from", source.name)
