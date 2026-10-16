#!/usr/bin/env python3
"""Make a macOS app self-contained, then verify and ad-hoc sign its dylib closure."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

app = Path(sys.argv[1]).resolve()
executable = app / 'Contents/MacOS/openchess'
frameworks = app / 'Contents/Frameworks'
frameworks.mkdir(exist_ok=True)
resources = app / 'Contents/Resources'
source = Path(sys.argv[2]).resolve()
for name in ('LICENSE', 'ATTRIBUTIONS.md'):
    shutil.copy2(source / name, resources / name)

def output(*args):
    return subprocess.check_output(args, text=True, timeout=30).strip()

def dependencies(path):
    values = [line.strip().split(' (', 1)[0] for line in output('otool', '-L', str(path)).splitlines()[1:]]
    return values[1:] if path.suffix == '.dylib' else values

def rpaths(path):
    lines = output('otool', '-l', str(path)).splitlines()
    return [lines[i + 2].strip().split('path ', 1)[1].split(' (', 1)[0]
            for i, line in enumerate(lines) if line.strip() == 'cmd LC_RPATH']

def expand(value, original):
    return value.replace('@loader_path', str(original.parent)).replace('@executable_path', str(executable.parent))

def resolve(dep, original):
    if dep.startswith('@rpath/'):
        for root in rpaths(original) + rpaths(executable):
            candidate = Path(expand(root, original)) / dep[len('@rpath/'):]
            if candidate.exists():
                return candidate.resolve()
        raise SystemExit('Unresolved dependency: ' + dep)
    candidate = Path(expand(dep, original))
    if not candidate.exists():
        raise SystemExit('Missing dependency: ' + dep)
    return candidate.resolve()

# Inspect originals before rewriting: their own rpaths resolve transitive deps.
queue = [(executable, executable)]
seen = set()
names = {}
packages = set()
# sdl2-compat dlopens SDL3 rather than declaring it in the Mach-O imports.
if any('sdl2-compat' in dep for dep in dependencies(executable)):
    dynamic = (Path(output('brew', '--prefix', 'sdl3')) / 'lib/libSDL3.dylib').resolve()
    destination = frameworks / dynamic.name
    names[dynamic.name] = dynamic
    shutil.copy2(dynamic, destination)
    destination.chmod(0o755)
    alias = frameworks / 'libSDL3.dylib'
    if alias.name != dynamic.name:
        alias.symlink_to(dynamic.name)
    queue.append((dynamic, destination))
while queue:
    original, target = queue.pop()
    if original in seen:
        continue
    seen.add(original)
    parts = original.parts
    if 'Cellar' in parts:
        index = parts.index('Cellar')
        packages.add(Path(*parts[:index + 3]))
    for dep in dependencies(original):
        if dep.startswith(('/System/Library/', '/usr/lib/')):
            continue
        resolved = resolve(dep, original)
        if resolved == original:  # dylib install ID, not a dependency
            continue
        name = resolved.name
        if name in names and names[name] != resolved:
            raise SystemExit('Conflicting library name: ' + name)
        destination = frameworks / name
        if name not in names:
            names[name] = resolved
            shutil.copy2(resolved, destination)
            destination.chmod(0o755)
            queue.append((resolved, destination))
            parts = resolved.parts
            if 'Cellar' in parts:
                index = parts.index('Cellar')
                packages.add(Path(*parts[:index + 3]))
        replacement = ('@executable_path/../Frameworks/' if target == executable else '@loader_path/') + name
        subprocess.run(['install_name_tool', '-change', dep, replacement, str(target)], check=True, capture_output=True)
    if target != executable:
        subprocess.run(['install_name_tool', '-id', '@rpath/' + target.name, str(target)], check=True, capture_output=True)

licenses = resources / 'licenses'
for package in packages:
    destination = licenses / package.parent.name
    candidates = [p for p in package.glob('*') if p.is_file() and any(word in p.name.upper() for word in ('LICENSE', 'COPYING', 'NOTICE'))]
    shared = package / 'share/licenses'
    if shared.is_dir():
        shutil.copytree(shared, destination / 'share', dirs_exist_ok=True)
    for candidate in candidates:
        destination.mkdir(parents=True, exist_ok=True)
        shutil.copy2(candidate, destination / candidate.name)
(resources / 'DEPENDENCIES.txt').write_text('\n'.join(sorted(names)) + '\n', encoding='utf-8')
for target in [*frameworks.glob('*.dylib'), executable]:
    for dep in dependencies(target):
        if dep.startswith(('/System/Library/', '/usr/lib/')):
            continue
        if dep == '@rpath/' + target.name:
            continue
        if not dep.startswith(('@loader_path/', '@executable_path/')) or not Path(expand(dep, target)).exists():
            raise SystemExit('Non-portable dependency remains: ' + dep)
# Copied artwork may carry Finder/resource-fork metadata; release copies must not.
subprocess.run(['xattr', '-cr', str(app)], check=True)
for target in frameworks.glob('*.dylib'):
    subprocess.run(['codesign', '--force', '--sign', '-', str(target)], check=True, capture_output=True)
# Sign outside synced folders: File Provider may recreate FinderInfo immediately
# after xattr cleanup. Copy only the resulting signed bytes/seal back afterward.
with tempfile.TemporaryDirectory(prefix='openchess-sign-') as directory:
    signing_app = Path(directory) / 'OpenChess.app'
    subprocess.run(['ditto', '--norsrc', '--noextattr', str(app), str(signing_app)], check=True)
    subprocess.run(['codesign', '--force', '--sign', '-', str(signing_app)], check=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(signing_app)], check=True)
    shutil.copyfile(signing_app / 'Contents/MacOS/openchess', executable)
    seal = app / 'Contents/_CodeSignature'
    if seal.exists():
        shutil.rmtree(seal)
    shutil.copytree(signing_app / 'Contents/_CodeSignature', seal)
version = (source / 'VERSION').read_text().strip()
actual = output(str(executable), '--version')
if actual != 'OpenChess ' + version:
    raise SystemExit('Packaged executable version mismatch: ' + actual)
print(f'Verified macOS {version}: {len(names)} bundled libraries, ad-hoc signature valid.')
