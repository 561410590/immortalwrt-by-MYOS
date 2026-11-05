"""Replace only successful model assets in the rolling nightly release."""
from pathlib import Path
import json
import os
import subprocess
import tempfile

REPO = os.environ['GITHUB_REPOSITORY']
MODELS = ['C8-688', 'C5800-688', 'NBCPE-688', 'C2000MAX', 'C2000-Ultra', 'N5000']

def gh(*args, check=True):
    return subprocess.run(['gh', *args], check=check, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

def main():
    records = [json.loads(p.read_text()) for p in Path('output').glob('*.json')]
    if not records:
        print('No successful firmware to publish; previous nightly assets retained.')
        return
    result = gh('api', 'repos/' + REPO + '/releases/tags/nightly', check=False)
    if result.returncode:
        if '404' not in result.stderr:
            raise RuntimeError(result.stderr)
        gh('release', 'create', 'nightly', '--repo', REPO, '--prerelease', '--title', 'MYOS nightly',
           '--notes', 'MYOS nightly', '--target', os.environ['GITHUB_SHA'])
        result = gh('api', 'repos/' + REPO + '/releases/tags/nightly')
    release = json.loads(result.stdout)
    manifest = {}
    previous = next((a for a in release['assets'] if a['name'] == 'nightly.json'), None)
    if previous:
        manifest = json.loads(gh('api', 'repos/' + REPO + '/releases/assets/' + str(previous['id']),
                                 '-H', 'Accept: application/octet-stream').stdout)
    for record in records:
        model = record['model']
        asset = Path('output') / record['asset']
        gh('release', 'upload', 'nightly', str(asset), '--repo', REPO, '--clobber')
        manifest[model] = record
        # Upload completed before removing the previous successful model image.
        for old in release['assets']:
            if old['name'].startswith('MYOS-' + model + '-') and old['name'].endswith('.bin') and old['name'] != asset.name:
                gh('api', 'repos/' + REPO + '/releases/assets/' + str(old['id']), '--method', 'DELETE')
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / 'nightly.json'
        path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
        gh('release', 'upload', 'nightly', str(path), '--repo', REPO, '--clobber')
        body = Path(directory) / 'notes.md'
        lines = ['| 机型 | 版本 | 构建时间（北京时间） | 源码 |', '| --- | --- | --- | --- |']
        from datetime import datetime
        from zoneinfo import ZoneInfo
        for model in MODELS:
            item = manifest.get(model)
            if item:
                date = datetime.fromisoformat(item['date']).astimezone(ZoneInfo('Asia/Shanghai')).strftime('%Y-%m-%d %H:%M')
                commit = item['source']
                lines.append(f"| {model} | {item['version']} | {date} | [{commit[:8]}](https://github.com/{REPO}/commit/{commit}) |")
            else:
                lines.append(f'| {model} | 尚未生成 | — | — |')
        body.write_text('\n'.join(lines) + '\n')
        gh('release', 'edit', 'nightly', '--repo', REPO, '--prerelease', '--notes-file', str(body))
    print('Updated nightly: ' + ', '.join(record['model'] for record in records))

if __name__ == '__main__':
    main()
