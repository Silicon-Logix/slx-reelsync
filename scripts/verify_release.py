# SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
# SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
"""Verify the distributed file checksums, without network access."""
from pathlib import Path
import hashlib
ROOT=Path(__file__).resolve().parents[1]
checked=0
for line in (ROOT/'SHA256SUMS').read_text(encoding='utf-8').splitlines():
    digest,name=line.split('  ',1);p=(ROOT/name).resolve()
    if not p.is_relative_to(ROOT) or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:
        raise SystemExit('Verification failed: '+name)
    checked+=1
print('Verified',checked,'distributed files. Checksums are not a digital signature.')
