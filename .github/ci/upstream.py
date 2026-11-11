"""Merge current upstream userspace into the maintained MYOS build inputs."""
from pathlib import Path
import argparse
import json
import os
import re
import subprocess
import sys

CORE_URL = 'https://github.com/padavanonly/immortalwrt-mt798x-6.6.git'
CORE_REF = 'openwrt-24.10-6.6'
CORE_BASE = '4c657c93546c86cd9f9d83cd5931b5056e652dbb'
# The six devices retain the kernel and hardware inputs of their MYOS branch.
PRESERVE = ('.github', 'README.md', 'README-OPEN-SOURCE.md', 'AGENTS.md',
            'target/linux', 'include/kernel-6.6', 'package/kernel', 'package/mtk/drivers')
LOCAL_CONFLICTS = (
    'package/base-files/files/bin/config_generate',
    'package/mtk/applications/luci-app-mtwifi-cfg/root/etc/uci-defaults/luci-app-mtwifi-cfg.default',
    'package/mtk/applications/luci-app-mtwifi-cfg/root/usr/share/luci-app-mtwifi-cfg/wireless-mtk.js',
    'package/network/utils/iwinfo/src/iwinfo_mtk.c',
    'package/mtk/applications/switch/files/switch_ctl.sh',
)
KEEP_LOCAL = {
    'feeds/packages': ('net/cgi-io/Makefile',),
}
TAKE_UPSTREAM = {
    'feeds/packages': ('net/v2ray-geodata/Makefile',),
    'feeds/nas': ('network/services/ddnsto/Makefile',),
}
QUICKSTART = 'luci/luci-app-quickstart/luasrc/controller/quickstart.lua'
ISTORE_ENHANCE = 'luci/luci-app-istoreenhance/Makefile'
OPENCLASH = 'applications/luci-app-openclash'


class Merger:
    def __init__(self, source, git_dir=None, cached=False):
        self.source = source
        self.cached = cached
        self.command = ['git', '--git-dir=' + str(git_dir)] if git_dir else ['git', '-C', str(source)]
        self.environment = dict(os.environ, GIT_AUTHOR_NAME='MYOS', GIT_COMMITTER_NAME='MYOS',
            GIT_AUTHOR_EMAIL='67544413+561410590@users.noreply.github.com',
            GIT_COMMITTER_EMAIL='67544413+561410590@users.noreply.github.com')
        self.retained = []
        self.conflicts = []

    def git(self, *args, data=None, check=True):
        result = subprocess.run(self.command + list(args), input=data, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, env=self.environment)
        if check and result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))
        return result

    def text(self, *args):
        return self.git(*args).stdout.decode().strip()

    def fetch(self, name, url, ref, base):
        if self.cached:
            return self.text('rev-parse', 'refs/remotes/nightly/' + name)
        print('Fetching ' + name + ' ' + ref, flush=True)
        self.git('fetch', '--no-tags', '--depth=1', url,
                 '+' + ref + ':refs/remotes/nightly/' + name)
        if self.git('cat-file', '-e', base + '^{commit}', check=False).returncode:
            self.git('fetch', '--no-tags', '--depth=1', url, base)
        return self.text('rev-parse', 'refs/remotes/nightly/' + name)

    def entries(self, tree):
        result = {}
        for row in self.git('ls-tree', '-z', tree).stdout.split(b'\0'):
            if row:
                header, name = row.split(b'\t', 1)
                result[name.decode()] = header
        return result

    def entry(self, tree, path):
        row = self.git('ls-tree', tree, '--', path).stdout.strip()
        return row.split(b'\t', 1)[0] if row else None

    def replace(self, tree, path, entry):
        first, _, rest = path.partition('/')
        entries = self.entries(tree)
        if rest:
            old = entries.get(first)
            subtree = old.split()[2].decode() if old else self.text('mktree')
            new = self.replace(subtree, rest, entry)
            entries[first] = b'040000 tree ' + new.encode()
        elif entry is None:
            entries.pop(first, None)
        else:
            entries[first] = entry
        data = b''.join(value + b'\t' + name.encode() + b'\0' for name, value in entries.items())
        return self.git('mktree', '-z', data=data).stdout.decode().strip()

    def commit(self, tree):
        return self.text('commit-tree', tree, '-m', 'MYOS upstream merge input')

    def file(self, tree, path):
        return self.git('show', tree + ':' + path).stdout.decode()

    def write_file(self, tree, path, content, mode=b'100644'):
        blob = self.git('hash-object', '-w', '--stdin', data=content.encode()).stdout.strip()
        return self.replace(tree, path, mode + b' blob ' + blob)

    def prepare_feed(self, name, base, local, incoming):
        if name == 'luci':
            path = OPENCLASH + '/Makefile'
            versions = [
                tuple(map(int, re.search(r'^PKG_VERSION:=(\d+(?:\.\d+)+)', self.file(tree, path), re.M)[1].split('.')))
                for tree in (local, incoming)
            ]
            if versions[0] > versions[1]:
                entry = self.entry(local, OPENCLASH)
                self.retained.append('feeds/luci:' + OPENCLASH)
                return self.replace(base, OPENCLASH, entry), self.replace(incoming, OPENCLASH, entry)
        if name == 'nas_luci':
            index = re.compile(r'^function index\(\)\n.*?^end\n', re.M | re.S)
            local_index = index.search(self.file(local, QUICKSTART))[0]
            # MYOS registers the menu even before the daemon starts.
            base = self.write_file(base, QUICKSTART,
                index.sub(lambda _: local_index, self.file(base, QUICKSTART)))
            incoming = self.write_file(incoming, QUICKSTART,
                index.sub(lambda _: local_index, self.file(incoming, QUICKSTART)))
            self.retained.append('feeds/nas_luci:' + QUICKSTART + ' index()')
        return base, incoming

    def merge(self, base, ours, theirs, label, local_conflicts=()):
        result = self.git('merge-tree', '--write-tree', '--name-only',
            '--merge-base=' + self.commit(base), self.commit(ours), self.commit(theirs), check=False)
        lines = result.stdout.decode(errors='replace').splitlines()
        if result.returncode not in (0, 1) or not lines:
            raise RuntimeError(result.stderr.decode(errors='replace'))
        tree = lines[0]
        conflicts = []
        if result.returncode:
            for line in lines[1:]:
                if not line:
                    break
                conflicts.append(line)
        unresolved = []
        for path in conflicts:
            if path in local_conflicts or path in KEEP_LOCAL.get(label, ()):
                tree = self.replace(tree, path, self.entry(ours, path))
                self.retained.append(label + ':' + path)
            elif path in TAKE_UPSTREAM.get(label, ()):
                tree = self.replace(tree, path, self.entry(theirs, path))
            elif label == 'feeds/nas_luci' and path == ISTORE_ENHANCE:
                content = self.file(theirs, path)
                if '+lsblk' in self.file(ours, path) and '+lsblk' not in content:
                    content = re.sub(r'^(LUCI_DEPENDS:=.*)$', r'\1 +lsblk', content, flags=re.M)
                    self.retained.append(label + ':' + path + ' +lsblk')
                tree = self.write_file(tree, path, content)
            elif (self.entry(theirs, path) is None and self.entry(base, path)
                  and self.entry(ours, path)
                  and self.entry(base, path).split()[2] == self.entry(ours, path).split()[2]):
                # Preserve content changes; a local mode-only change need not revive
                # a script which upstream removed or replaced.
                tree = self.replace(tree, path, None)
            elif (path.endswith('mtk/applications/mtwifi-cfg/Makefile')
                  or label == 'feeds/istore' and path == 'luci/luci-lib-taskd/Makefile'):
                content = self.git('show', tree + ':' + path).stdout.decode()
                content = re.sub(r'^<<<<<<<[^\n]*\n(PKG_RELEASE:=[^\n]*\n)=======\nPKG_RELEASE:=[^\n]*\n>>>>>>>[^\n]*\n',
                                 r'\1', content, flags=re.M)
                if '<<<<<<<' in content:
                    unresolved.append(path)
                else:
                    tree = self.write_file(tree, path, content)
                    self.retained.append(label + ':' + path + ' PKG_RELEASE')
            elif label == 'feeds/packages' and path == 'lang/python/python-ubus/Makefile':
                content = self.git('show', tree + ':' + path).stdout.decode()
                content = re.sub(r'^<<<<<<<[^\n]*\n(PKG_BUILD_DEPENDS:=python3/host\n\n)=======\n>>>>>>>[^\n]*\n',
                                 r'\1', content, flags=re.M)
                if '<<<<<<<' in content:
                    unresolved.append(path)
                else:
                    blob = self.git('hash-object', '-w', '--stdin', data=content.encode()).stdout.strip()
                    tree = self.replace(tree, path, b'100644 blob ' + blob)
            else:
                unresolved.append(path)
        if unresolved:
            self.conflicts.append({'component': label, 'tree': tree, 'paths': unresolved})
            print(label + ' merge conflicts (tree ' + tree + '):\n' + '\n'.join(unresolved), flush=True)
        return tree

    def run(self, reference):
        original = self.text('rev-parse', reference + '^{tree}')
        snapshot = json.loads((self.source / 'SOURCE_SNAPSHOT.json').read_text())
        core = self.fetch('core', CORE_URL, CORE_REF, CORE_BASE)
        base = self.text('rev-parse', CORE_BASE + '^{tree}')
        incoming = self.text('rev-parse', core + '^{tree}')
        for path in PRESERVE:
            entry = self.entry(original, path)
            base = self.replace(base, path, entry)
            incoming = self.replace(incoming, path, entry)
        tree = self.merge(base, original, incoming, 'core', LOCAL_CONFLICTS)
        sources = {'core': {'repository': CORE_URL, 'branch': CORE_REF, 'base': CORE_BASE, 'commit': core}}
        # Ultra/N5000 also consume their bundled, locally modified MAX package feed.
        if snapshot['publication_branch'] in ('C2000-Ultra', 'N5000'):
            local = self.text('rev-parse', tree + ':feeds/base')
            old = self.text('rev-parse', base + ':package')
            new = self.text('rev-parse', incoming + ':package')
            for path in ('kernel', 'mtk/drivers'):
                entry = self.entry(local, path)
                old = self.replace(old, path, entry)
                new = self.replace(new, path, entry)
            merged = self.merge(old, local, new, 'feeds/base',
                tuple(p.removeprefix('package/') for p in LOCAL_CONFLICTS))
            tree = self.replace(tree, 'feeds/base', b'040000 tree ' + merged.encode())
        for line in (self.source / 'feeds.conf.default').read_text().splitlines():
            if not line.startswith('src-git '):
                continue
            _, name, address = line.split()[:3]
            url, _, branch = address.partition(';')
            branch = branch or 'HEAD'
            old = snapshot['feed_origins'][name]
            latest = self.fetch(name, url, branch, old)
            local = self.text('rev-parse', tree + ':feeds/' + name)
            old_tree, new_tree = self.prepare_feed(name, self.text('rev-parse', old + '^{tree}'),
                local, self.text('rev-parse', latest + '^{tree}'))
            merged = self.merge(old_tree, local, new_tree, 'feeds/' + name)
            tree = self.replace(tree, 'feeds/' + name, b'040000 tree ' + merged.encode())
            sources[name] = {'repository': url, 'branch': branch, 'base': old, 'commit': latest}
        kernel = '6.6' + re.search(r'^LINUX_VERSION-6\.6\s*=\s*(\.\d+)',
            self.file(tree, 'include/kernel-6.6'), re.M)[1]
        return tree, {'kernel': kernel, 'myos': self.text('rev-parse', reference),
                      'merged_tree': tree, 'upstream': sources, 'retained_local_conflicts': self.retained,
                      'conflicts': self.conflicts}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--git-dir', type=Path)
    parser.add_argument('--ref', default='HEAD')
    parser.add_argument('--preview', action='store_true')
    parser.add_argument('--cached', action='store_true')
    args = parser.parse_args()
    merger = Merger(args.source, args.git_dir, args.cached)
    tree, record = merger.run(args.ref)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=2) + '\n')
    if record['conflicts']:
        print(json.dumps(record['conflicts'], indent=2), flush=True)
        return 1
    if not args.preview:
        merger.git('read-tree', '--reset', '-u', tree)
    print(json.dumps(record, indent=2), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
