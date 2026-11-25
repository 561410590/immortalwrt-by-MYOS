import datetime
import json
import os
from pathlib import Path
from zoneinfo import ZoneInfo

groups = {'main': ['C8-688', 'C5800-688', 'NBCPE-688'],
          'C2000MAX': ['C2000MAX'], 'C2000-Ultra': ['C2000-Ultra'], 'N5000': ['N5000']}
matrix = []
requested = {model.strip() for model in os.environ['MODEL'].split(',')}
settings = json.loads(Path('.github/ci/inputs.json').read_text())
sources = {**settings['source_snapshots'], 'main': os.environ['GITHUB_SHA']}
for branch, models in groups.items():
    selected = [m for m in models if 'all' in requested or m in requested]
    if selected:
        matrix.extend({'branch': branch, 'sha': sources[branch], 'model': model} for model in selected)
stamp = datetime.datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d%H%M')
with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
    stream.write('matrix=' + json.dumps({'include': matrix}) + '\n')
    stream.write('stamp=' + stamp + '\n')
    stream.write('inputs_ref=' + settings['commit'] + '\n')
print(json.dumps(matrix))
