import datetime
import json
import os
from pathlib import Path
import subprocess
from zoneinfo import ZoneInfo

groups = {'main': ['C8-688', 'C5800-688', 'NBCPE-688'],
          'C2000MAX': ['C2000MAX'], 'C2000-Ultra': ['C2000-Ultra'], 'N5000': ['N5000']}
matrix = []
for branch, models in groups.items():
    selected = [m for m in models if os.environ['MODEL'] in ('all', m)]
    if selected:
        sha = subprocess.check_output(['gh', 'api', 'repos/' + os.environ['GITHUB_REPOSITORY'] +
            '/commits/' + branch, '--jq', '.sha'], text=True).strip()
        matrix.append({'branch': branch, 'sha': sha, 'models': selected})
stamp = datetime.datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d%H%M')
settings = json.loads(Path('.github/ci/inputs.json').read_text())
with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
    stream.write('matrix=' + json.dumps({'include': matrix}) + '\n')
    stream.write('version=3.1.40-' + stamp + '\n')
    stream.write('inputs_ref=' + settings['commit'] + '\n')
print(json.dumps(matrix))
