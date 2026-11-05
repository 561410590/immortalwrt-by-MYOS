#!/usr/bin/env python3
"""Install the same upstream WebUI 3.0.2 assets embedded by C5800."""
import argparse
import json
import shutil
from pathlib import Path


def install(frontend, www):
    target = www / '5700'
    target.mkdir(parents=True, exist_ok=True)
    files = 0
    for source in frontend.rglob('*'):
        if not source.is_file():
            continue
        destination = target / source.relative_to(frontend)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        destination.chmod(0o644)
        files += 1
    index = target / 'index.html'
    html = index.read_text(encoding='utf-8')
    html = html.replace('<head>', '<head>\n<script src="/5700/nbcpe-webui-bootstrap.js?v=3.0.2-nbcpe1"></script>', 1)
    index.write_text(html, encoding='utf-8')
    shutil.copyfile(Path(__file__).with_name('nbcpe-webui-bootstrap.js'), target / 'nbcpe-webui-bootstrap.js')
    (target / 'nbcpe-webui-bootstrap.js').chmod(0o644)
    shutil.copyfile(Path(__file__).with_name('mt5700-webui-upstream.json'), target / 'mt5700-webui-upstream.json')
    (target / 'mt5700-webui-upstream.json').chmod(0o644)
    # The upstream adapter reads the browser hostname stored by the bootstrap.
    # Keep NBCPE's existing official AT queue / WebSocket service on port 8766.
    (target / 'config.json').write_text(json.dumps({
        'at': {'host': '', 'port': 8766}, 'status': 'false'
    }) + '\n', encoding='utf-8')
    for directory in target.rglob('*'):
        if directory.is_dir():
            directory.chmod(0o755)
    target.chmod(0o755)
    print(json.dumps({'upstream_version': '3.0.2', 'frontend': str(frontend), 'files': files + 2, 'port': 8766}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frontend', type=Path, required=True)
    parser.add_argument('--www', type=Path, required=True)
    arguments = parser.parse_args()
    install(arguments.frontend, arguments.www)
