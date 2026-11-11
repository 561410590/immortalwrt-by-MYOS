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
CONFIG_VERSION = re.search(r'^CONFIG_VERSION_NUMBER="[Vv]?(\d+\.\d+\.\d+)', (SOURCE / '.config').read_text(), re.M)[1]
VERSION = CONFIG_VERSION + '-' + os.environ['BUILD_STAMP']
KERNEL = json.loads((LOGS / 'upstream.json').read_text())['kernel']
BRANCH = os.environ['BRANCH']
MODEL = os.environ['MODEL']
MAKE_VERSION = VERSION if BRANCH == 'main' else 'V' + VERSION
MAIN_PROFILES = {'C8-688': 'nradio_c8-688', 'C5800-688': 'nradio_c5800-688',
                 'NBCPE-688': 'nradio_nbcpe-688'}
LOGS.mkdir(exist_ok=True)
OUTPUT.mkdir(exist_ok=True)
os.environ['FORCE_UNSAFE_CONFIGURE'] = '1'
os.environ['CCACHE_BASEDIR'] = str(SOURCE)
os.environ['MWRT_REFERENCE'] = str(WORK / 'MWRT-reference.bin')
os.environ['MYOS_BUILD_VERSION'] = VERSION

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
    if MODEL != 'C2000MAX':
        unpack('factory')
    unpack(BRANCH)
    if BRANCH != 'main':
        (SOURCE / 'dl').mkdir(exist_ok=True)
        shutil.copy2(INPUTS / 'Xray-core-26.10.10.tar.gz', SOURCE / 'dl')
    if BRANCH == 'main':
        unpack('main-binaries')
        unpack('mwrt')
        if (WORK / 'MWRT-reference.bin').is_file():
            os.environ['MWRT_REFERENCE'] = str(WORK / 'MWRT-reference.bin')
    patch = INPUTS / (BRANCH + '.patch')
    if patch.stat().st_size:
        # Keep private service sources separate from the public source history.
        run('private-overlay', ['git', 'apply', '--whitespace=nowarn', patch])
        (LOGS / 'private-overlay.log').unlink()
    shutil.rmtree(INPUTS)
    config = SOURCE / '.config'
    text = re.sub(r'(?m)^CONFIG_VERSION_NUMBER=.*$', 'CONFIG_VERSION_NUMBER="' + MAKE_VERSION + '"', config.read_text())
    text = re.sub(r'(?m)^(?:# CONFIG_CCACHE is not set|CONFIG_CCACHE=.*)$', 'CONFIG_CCACHE=y', text)
    if MODEL in MAIN_PROFILES:
        text = re.sub(r'(?m)^(?:# )?CONFIG_TARGET_(?:mediatek_filogic_DEVICE_\S+|PROFILE=\S+).*\n', '', text)
        text += f'CONFIG_TARGET_mediatek_filogic_DEVICE_{MAIN_PROFILES[MODEL]}=y\n'
        for package in ('luci-app-turboacc', 'luci-i18n-turboacc-zh-cn', 'luci-app-c5800-hnat'):
            symbol = 'CONFIG_PACKAGE_' + package
            text = re.sub(rf'(?m)^(?:# )?{symbol}(?:=.*| is not set)\n', '', text)
            wanted = (MODEL == 'C8-688') == (package != 'luci-app-c5800-hnat')
            text += symbol + '=y\n' if wanted else '# ' + symbol + ' is not set\n'
    config.write_text(text)
    os.environ['FORCE_UNSAFE_CONFIGURE'] = '1'
    os.environ['CCACHE_BASEDIR'] = str(SOURCE)
    run('feeds-index', ['./scripts/feeds', 'update', '-i', '-a'])
    run('feeds-install', ['./scripts/feeds', 'install', '-a'])
    run('config', ['make', 'defconfig'])

def assemble(model):
    filename = f'MYOS-{model}-{VERSION}-{KERNEL}-sysupgrade.bin'
    if model == 'C8-688':
        run(model + '-assemble', ['python3', IMAGE / 'nradio-c8-688/assemble-c8.py', '--source', SOURCE, '--pack'])
        kernel_image = KDIR / ('linux-' + KERNEL) / 'arch/arm64/boot/Image'
        run(model + '-pack', ['python3', IMAGE / 'nradio-c8-688/build-c8.py', '--source', SOURCE,
                             '--kernel-image', kernel_image])
        root = IMAGE / ('nradio-c8-688/root-c8-myos-' + VERSION)
        firmware = SOURCE / 'bin/targets/mediatek/filogic' / filename
    elif model == 'C5800-688':
        run(model + '-pack', ['python3', IMAGE / 'build-c5800.py', '--source', SOURCE])
        root = KDIR / 'root-c5800-myos'
        firmware = SOURCE / 'bin/targets/mediatek/filogic' / filename
    elif model == 'NBCPE-688':
        run(model + '-assemble', ['python3', IMAGE / 'assemble-nbcpe.py', '--source', SOURCE, '--pack'])
        run(model + '-pack', ['python3', IMAGE / 'build-nbcpe.py', '--source', SOURCE, '--build-only'])
        root = KDIR / 'root-nbcpe-myos'
        firmware = SOURCE / 'bin/targets/mediatek/filogic' / filename
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
    target = OUTPUT / filename
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
        run(stage, ['make', *([target] if target else []), '-j' + str(os.cpu_count()), 'V=s', 'VERSION_NUMBER=' + MAKE_VERSION])
    if stage not in ('assemble', 'all'):
        return 0
    record = assemble(MODEL)
    (OUTPUT / (MODEL + '.json')).write_text(json.dumps(record) + '\n')
    return 0

if __name__ == '__main__':
    sys.exit(main())
