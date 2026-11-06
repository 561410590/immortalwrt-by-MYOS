"""Merge current upstream userspace into the maintained MYOS build inputs."""
from pathlib import Path
import argparse
import json
import os
import subprocess
import sys

CORE_URL = 'https://github.com/padavanonly/immortalwrt-mt798x-6.6.git'
CORE_REF = 'openwrt-24.10-6.6'
CORE_BASE = '4c657c93546c86cd9f9d83cd5931b5056e652dbb'
# The six devices retain their maintained 6.6.110 kernel and hardware inputs.
PRESERVE = ('.github', 'README.md', 'README-OPEN-SOURCE.md', 'AGENTS.md',
            'target/linux', 'include/kernel-6.6', 'package/kernel', 'package/mtk/drivers')
LOCAL_CONFLICTS = (
    'package/base-files/files/bin/config_generate',
    'package/mtk/applications/luci-app-mtwifi-cfg/root/etc/uci-defaults/luci-app-mtwifi-cfg.default',
    'package/mtk/applications/luci-app-mtwifi-cfg/root/usr/share/luci-app-mtwifi-cfg/wireless-mtk.js',
    'package/network/utils/iwinfo/src/iwinfo_mtk.c',
)


class Merger:
    def __init__(self, source, git_dir=None, cached=False):
        self.source = source
        self.cached = cached
        self.command = ['git', '--git-dir=' + str(git_dir)] if git_dir else ['git', '-C', str(source)]
        self.environment = dict(os.environ, GIT_AUTHOR_NAME='MYOS', GIT_COMMITTER_NAME='MYOS',
            GIT_AUTHOR_EMAIL='67544413+561410590@users.noreply.github.com',
            GIT_COMMITTER_EMAIL='67544413+561410590@users.noreply.github.com')
        self.retained = []

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
            if path in local_conflicts:
                tree = self.replace(tree, path, self.entry(ours, path))
                self.retained.append(label + ':' + path)
            else:
                unresolved.append(path)
        if unresolved:
            raise RuntimeError(label + ' merge conflicts:\n' + '\n'.join(unresolved))
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
            merged = self.merge(self.text('rev-parse', old + '^{tree}'), local,
                self.text('rev-parse', latest + '^{tree}'), 'feeds/' + name)
            tree = self.replace(tree, 'feeds/' + name, b'040000 tree ' + merged.encode())
            sources[name] = {'repository': url, 'branch': branch, 'base': old, 'commit': latest}
        return tree, {'kernel': '6.6.110', 'myos': self.text('rev-parse', reference),
                      'merged_tree': tree, 'upstream': sources, 'retained_local_conflicts': self.retained}


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
    if not args.preview:
        merger.git('read-tree', '--reset', '-u', tree)
    print(json.dumps(record, indent=2), flush=True)


if __name__ == '__main__':
    main()
