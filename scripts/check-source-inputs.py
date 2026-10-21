#!/usr/bin/env python3
"""Read-only check of the external inputs recorded for this source snapshot."""

import argparse
import hashlib
import json
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1]


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', required=True,
                        choices=('c8-688', 'c5800-688', 'nbcpe-688', 'c2000-max', 'c2000-ultra'))
    args = parser.parse_args()
    manifest = json.loads((SOURCE / 'BINARY_INPUTS.json').read_text())
    if args.model not in manifest['models']:
        parser.error('This model belongs to a different source tree')
    missing, different, checked, optional_missing = [], [], [], []
    for item in manifest['inputs']:
        if args.model not in item['models']:
            continue
        path = SOURCE / item['path']
        present = path.is_dir() if item.get('kind') == 'directory' else path.is_file()
        if not present:
            (optional_missing if item.get('optional') else missing).append(item['path'])
            continue
        if item.get('sha256') and digest(path) != item['sha256']:
            different.append(item['path'])
            continue
        checked.append(item['path'])
    print(json.dumps({'model': args.model, 'checked': checked, 'missing': missing,
                      'optional_missing': optional_missing,
                      'different': different, 'ready': not missing and not different}, indent=2))
    return 1 if missing or different else 0


if __name__ == '__main__':
    raise SystemExit(main())
