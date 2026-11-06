"""Build each frozen source tree and assemble the requested model images."""
from pathlib import Path
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile

WORK = Path.cwd()
SOURCE = WORK / 'source'
INPUTS = WORK / 'private-inputs'
LOGS = WORK / 'logs'
OUTPUT = WORK / 'output'
IMAGE = SOURCE / 'target/linux/mediatek/image'
KDIR = SOURCE / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic'
VERSION = os.environ['BUILD_VERSION']
BRANCH = os.environ['BRANCH']
MODELS = os.environ['MODELS'].split(',')
LOGS.mkdir(exist_ok=True)
OUTPUT.mkdir(exist_ok=True)
os.environ['FORCE_UNSAFE_CONFIGURE'] = '1'
os.environ['CCACHE_BASEDIR'] = str(SOURCE)
os.environ['MWRT_REFERENCE'] = str(WORK / 'MWRT-reference.bin')

def run(name, args, cwd=SOURCE):
    print(name + ': started', flush=True)
    with (LOGS / (name + '.log')).open('w') as log:
        process = subprocess.Popen([str(a) for a in args], cwd=cwd, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, errors='replace')
        for line in process.stdout:
            log.write(line)
            if re.search(r'Entering directory|ERROR:|Error [0-9]|error:|Downloading|^time:', line):
                print(line.rstrip(), flush=True)
        code = process.wait()
    print(name + ': exit ' + str(code), flush=True)
    if code:
        lines = (LOGS / (name + '.log')).read_text(errors='replace').splitlines()
        print('\n'.join(lines[-70:]), flush=True)
        raise subprocess.CalledProcessError(code, args)

def unpack(name):
    parts = sorted(INPUTS.glob(name + '.tar.gz*'))
    producer = subprocess.Popen(['cat', *map(str, parts)], stdout=subprocess.PIPE)
    try:
        subprocess.run(['tar', '-xz', '--no-same-owner', '-C', str(WORK)], stdin=producer.stdout, check=True)
    finally:
        producer.stdout.close()
    if producer.wait():
        raise RuntimeError('Failed to read private input archive ' + name)

def prepare():
    unpack('factory')
    unpack(BRANCH)
    if BRANCH != 'main':
        (SOURCE / 'dl').mkdir(exist_ok=True)
        shutil.copy2(INPUTS / 'Xray-core-26.10.10.tar.gz', SOURCE / 'dl')
    if BRANCH == 'main':
        unpack('main-binaries')
        if (WORK / 'MWRT-reference.bin').is_file():
            os.environ['MWRT_REFERENCE'] = str(WORK / 'MWRT-reference.bin')
    patch = INPUTS / (BRANCH + '.patch')
    if patch.stat().st_size:
        # Keep private service sources separate from the public source history.
        run('private-overlay', ['git', 'apply', '--whitespace=nowarn', patch])
        (LOGS / 'private-overlay.log').unlink()
    shutil.rmtree(INPUTS)
    # The original local entrypoints remain unchanged; only this cloud workspace
    # receives the scheduled build version.
    for relative in ('assemble-c5800.py', 'build-c5800.py', 'assemble-nbcpe.py', 'build-nbcpe.py',
                     'nradio-c8-688/assemble-c8.py', 'nradio-c8-688/build-c8.py'):
        path = IMAGE / relative
        if path.is_file():
            text = path.read_text()
            text = re.sub(r"(?m)^((?:VERSION|version) = ['\"])(V?)3\.1\.40-\d+(['\"])",
                          lambda m: m[1] + m[2] + VERSION + m[3], text)
            path.write_text(text)
    config = SOURCE / '.config'
    text = re.sub(r'(?m)^CONFIG_VERSION_NUMBER=.*$', 'CONFIG_VERSION_NUMBER="' + VERSION + '"', config.read_text())
    text = re.sub(r'(?m)^(?:# CONFIG_CCACHE is not set|CONFIG_CCACHE=.*)$', 'CONFIG_CCACHE=y', text)
    config.write_text(text)
    os.environ['FORCE_UNSAFE_CONFIGURE'] = '1'
    os.environ['CCACHE_BASEDIR'] = str(SOURCE)
    run('feeds-index', ['./scripts/feeds', 'update', '-i', '-a'])
    run('feeds-install', ['./scripts/feeds', 'install', '-a'])
    run('config', ['make', 'defconfig'])

def assemble(model):
    if model == 'C8-688':
        run(model + '-assemble', ['python3', IMAGE / 'nradio-c8-688/assemble-c8.py', '--source', SOURCE, '--pack'])
        run(model + '-pack', ['python3', IMAGE / 'nradio-c8-688/build-c8.py', '--source', SOURCE])
        root = IMAGE / ('nradio-c8-688/root-c8-myos-' + VERSION)
        firmware = SOURCE / ('bin/targets/mediatek/filogic/MYOS-C8-688-' + VERSION + '-6.6.110-sysupgrade.bin')
    elif model == 'C5800-688':
        run(model + '-assemble', ['python3', IMAGE / 'assemble-c5800.py'])
        run(model + '-pack', ['python3', IMAGE / 'build-c5800.py', '--source', SOURCE])
        root = KDIR / 'root-c5800-myos'
        firmware = SOURCE / ('bin/targets/mediatek/filogic/MYOS-C5800-688-' + VERSION + '-6.6.110-sysupgrade.bin')
    elif model == 'NBCPE-688':
        run(model + '-assemble', ['python3', IMAGE / 'assemble-nbcpe.py', '--source', SOURCE, '--pack'])
        run(model + '-pack', ['python3', IMAGE / 'build-nbcpe.py', '--source', SOURCE, '--build-only'])
        root = KDIR / 'root-nbcpe-myos'
        firmware = SOURCE / ('bin/targets/mediatek/filogic/MYOS-NBCPE-688-' + VERSION + '-6.6.110-sysupgrade.bin')
    else:
        patterns = {'C2000MAX': ('*nradio_c2000-max*sysupgrade.bin', 'root-c2000max-MYOS'),
                    'C2000-Ultra': ('*nradio_c2000-ultra*sysupgrade.bin', 'root-c2000ultra-*-app'),
                    'N5000': ('*nradio_n5000*sysupgrade.bin', 'root-n5000-*-app')}
        pattern, root_pattern = patterns[model]
        firmware = max((SOURCE / 'bin/targets/mediatek/filogic').glob(pattern), key=lambda p: p.stat().st_mtime)
        root = max((p for p in KDIR.glob(root_pattern) if p.is_dir()), key=lambda p: p.stat().st_mtime)
    run(model + '-permissions', ['python3', Path(__file__).with_name('permissions.py'),
        '--source', SOURCE, '--root', root, '--firmware', firmware,
        '--output', LOGS / (model + '-permissions'), '--version', VERSION])
    target = OUTPUT / ('MYOS-' + model + '-' + VERSION + '-6.6.110-sysupgrade.bin')
    shutil.copy2(firmware, target)
    return {'model': model, 'version': VERSION, 'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'source': os.environ['SOURCE_COMMIT'], 'branch': BRANCH, 'asset': target.name,
            'upstream': json.loads((LOGS / 'upstream.json').read_text())}

def main():
    stage = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if stage in ('prepare', 'all'):
        prepare()
    if stage in ('download', 'all'):
        run('download', ['make', 'download', '-j8', 'V=s'])
    targets = {'tools': 'tools/install', 'toolchain': 'toolchain/install',
               'kernel': 'target/linux/compile', 'packages': 'package/compile', 'compile': ''}
    if stage in targets or stage == 'all':
        target = targets.get(stage, '')
        run(stage, ['make', *([target] if target else []), '-j' + str(os.cpu_count()), 'V=s', 'VERSION_NUMBER=' + VERSION])
    if stage not in ('assemble', 'all'):
        return 0
    failed = False
    for model in MODELS:
        try:
            record = assemble(model)
            (OUTPUT / (model + '.json')).write_text(json.dumps(record) + '\n')
        except Exception as error:
            failed = True
            print(model + ': ' + str(error), flush=True)
    return 1 if failed else 0

if __name__ == '__main__':
    sys.exit(main())
