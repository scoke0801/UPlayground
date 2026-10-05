"""Disk transaction for generated UE packages. Roll back only after editor exit."""
import hashlib
import json
import shutil
from pathlib import Path

EXTENSIONS = ('.uasset', '.uexp', '.ubulk', '.uptnl')


def sha256(file):
    return hashlib.sha256(Path(file).read_bytes()).hexdigest()


def write_json(file, value):
    file = Path(file)
    file.parent.mkdir(parents=True, exist_ok=True)
    temporary = file.with_suffix(file.suffix+'.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False), encoding='utf-8')
    temporary.replace(file)


def inside(root, relative):
    root = Path(root).resolve()
    target = (root / relative).resolve()
    if not target.is_relative_to(root) or target == root:
        raise ValueError('Path outside transaction root: '+str(relative))
    return target


def package_file(root, package, extension):
    if not package.startswith('/Game/') or '.' in package or '\\' in package:
        raise ValueError('Not a project package: '+package)
    return inside(Path(root)/'Content', package[len('/Game/'):]+extension)


class Transaction:
    def __init__(self, root, directory):
        self.root = Path(root).resolve()
        self.directory = Path(directory).resolve()
        self.file = self.directory/'transaction.json'
        self.data = dict(schema_version=1, root=str(self.root), status='PREPARING', files=[], packages=[])

    def prepare(self, packages, source):
        if self.file.exists():
            raise ValueError('Transaction already exists')
        self.directory.mkdir(parents=True, exist_ok=True)
        for package in sorted(set(packages)):
            for extension in EXTENSIONS:
                file = package_file(self.root, package, extension)
                relative = str(file.relative_to(self.root)).replace('\\', '/')
                backup = inside(self.directory/'backup', relative)
                row = dict(relative=relative, existed=file.exists(), before=None)
                if file.exists():
                    backup.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(file, backup)
                    row['before'] = sha256(file)
                    assert sha256(backup) == row['before']
                self.data['files'].append(row)
        shutil.copy2(source, self.directory/'polish-source.json')
        self.data.update(status='PREPARED', packages=sorted(set(packages)), source_sha256=sha256(source))
        self.flush()

    def flush(self):
        write_json(self.file, self.data)

    def mark_written(self, package):
        if package not in self.data['packages']:
            raise ValueError('Unplanned package write: '+package)
        self.data['status'] = 'APPLYING'
        self.flush()


def restore(root, directory):
    """Exact pre-run files, including removal of newly created package sidecars.

    Caller must have waited for its editor process to exit. Validate the complete
    manifest and backups BEFORE replacing anything; never infer paths from globbing.
    """
    root, directory = Path(root).resolve(), Path(directory).resolve()
    manifest = directory/'transaction.json'
    data = json.loads(manifest.read_text(encoding='utf-8'))
    if data['schema_version'] != 1 or Path(data['root']).resolve() != root:
        raise ValueError('Transaction belongs to another project')
    if data['status'] == 'RESTORED':
        return data
    allowed = {package_file(root, package, ext) for package in data['packages'] for ext in EXTENSIONS}
    resolved = []
    for row in data['files']:
        target = inside(root, row['relative'])
        backup = inside(directory/'backup', row['relative'])
        if target not in allowed or target in [t for t, _, _ in resolved]:
            raise ValueError('Invalid or duplicate transaction target')
        if row['existed'] and (not backup.is_file() or sha256(backup) != row['before']):
            raise ValueError('Missing/corrupt backup: '+str(backup))
        resolved.append((target, backup, row))
    if {target for target, _, _ in resolved} != allowed:
        raise ValueError('Incomplete transaction manifest')
    for target, backup, row in resolved:
        if row['existed']:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(backup, target)
            assert sha256(target) == row['before']
        elif target.exists():
            target.unlink()
    data['status'] = 'RESTORED'
    write_json(manifest, data)
    return data
