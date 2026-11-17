"""Build a selective gaze lifecycle patch on SDK 3d654a9; --apply publishes a tested candidate."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BUILD = HERE / '.build'
BASE = '3d654a98a3855666a58068895d6bb7a02a41f562'
P1 = HERE.parent / 'client-sdk-p1'
spec = importlib.util.spec_from_file_location('p1_helpers', P1 / 'build.py')
p1 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p1)
sha, read_zip, replace_zip, changed_members = p1.sha, p1.read_zip, p1.replace_zip, p1.changed_members
CLIENT, MODULE, ALGS = p1.CLIENT, p1.MODULE, p1.ALGS

def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)

def json_bytes(data): return (json.dumps(data, indent=2) + '\n').encode()
def original(path): return subprocess.check_output(['git', 'show', BASE + ':' + path.as_posix()], cwd=ROOT)
def run(args): subprocess.run(list(map(str, args)), cwd=ROOT, check=True)
def sources(): return sorted((HERE / 'src').rglob('*.java')) + sorted((HERE / 'compile-stubs').rglob('*.java'))
def source_snapshot():
    files = [HERE / 'build.py', HERE / 'graft-manifest.json', P1 / 'build.py', P1 / 'tools/Graft.java', *sources()]
    return {p.relative_to(ROOT).as_posix(): sha(p.read_bytes().replace(b'\r\n', b'\n')) for p in sorted(files)}

def jar_classes(folder, target):
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as z:
        for path in sorted(folder.rglob('*.class')):
            entry = zipfile.ZipInfo(path.relative_to(folder).as_posix(), (1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(entry, path.read_bytes())

def parser():
    ap = argparse.ArgumentParser(__doc__)
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--java-home', default=os.environ.get('JAVA_HOME'))
    ap.add_argument('--android-sdk', default=os.environ.get('ANDROID_HOME'))
    ap.add_argument('--platform', default='android-36')
    ap.add_argument('--build-tools', default='36.1.0')
    return ap

def apply_candidate():
    from verify import verify_candidate
    report = verify_candidate(require_tests=True)
    for rel, digest in report['published_file_sha256'].items():
        path = Path(rel)
        current = (ROOT / path).read_bytes()
        baseline = original(path)
        is_baseline = current == baseline or (path.suffix == '.module' and current.replace(b'\r\n', b'\n') == baseline)
        assert is_baseline or sha(current) == digest, 'Refusing unrelated changes: ' + rel
    for rel in report['published_file_sha256']:
        write(ROOT / rel, (BUILD / 'candidate' / rel).read_bytes())
    report['applied'] = True
    write(HERE / 'VERIFICATION.json', json_bytes(report))
    print('Published tested candidate:', report['after_sha256'])

def main():
    args = parser().parse_args()
    if args.apply:
        apply_candidate()
        return
    assert args.java_home and args.android_sdk, 'Set JAVA_HOME/ANDROID_HOME or pass explicit paths'
    inputs = source_snapshot()
    jdk, sdk = Path(args.java_home), Path(args.android_sdk)
    suffix = '.exe' if os.name == 'nt' else ''
    java, javac = (jdk / 'bin' / (name + suffix) for name in ('java', 'javac'))
    android = sdk / 'platforms' / args.platform / 'android.jar'
    d8 = sdk / 'build-tools' / args.build_tools / 'lib/d8.jar'
    for path in (java, javac, android, d8): assert path.is_file(), path
    deps = []
    for name, (url, digest) in p1.DEPS.items():
        path = BUILD / 'deps' / name
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            urllib.request.urlretrieve('https://repo.maven.apache.org/maven2/' + url, path)
        assert sha(path.read_bytes()) == digest, path
        deps.append(path)
    cp = os.pathsep.join(map(str, deps))
    run([javac, '-cp', cp, '-d', BUILD / 'tools', P1 / 'tools/Graft.java'])
    tool_cp = os.pathsep.join([str(BUILD / 'tools'), cp])
    before = original(CLIENT)
    assert sha(before) == '1277ee23445cec0fa459061bf3c1f9005e054709bc4cfd624491027a5601d7c8'
    oldmodule = read_zip(before, MODULE)
    write(BUILD / 'original.dex', read_zip(oldmodule, 'classes.dex'))
    out = BUILD / 'classes'
    out.mkdir(parents=True, exist_ok=True)
    for path in out.rglob('*.class'): path.unlink()
    run([javac, '--release', '8', '-encoding', 'UTF-8', '-cp', android, '-d', out, *sources()])
    donor = BUILD / 'donor.jar'
    jar_classes(out, donor)
    dexdir = BUILD / 'dex'
    dexdir.mkdir(exist_ok=True)
    run([java, '-cp', d8, 'com.android.tools.r8.D8', '--release', '--min-api', '29', '--lib', android, '--output', dexdir, donor])
    run([java, '-Xmx2g', '-Dandroid.jar=' + str(android), '-cp', tool_cp, 'Graft', 'dex',
         HERE / 'graft-manifest.json', BUILD / 'original.dex', BUILD / 'patched.dex', dexdir / 'classes.dex'])
    module = replace_zip(oldmodule, {'classes.dex': (BUILD / 'patched.dex').read_bytes()})
    after = replace_zip(before, {MODULE: module})
    assert changed_members(before, after) == [MODULE]
    assert changed_members(oldmodule, module) == ['classes.dex']
    updates = {CLIENT: after}
    metarel = CLIENT.with_suffix('.module')
    metadata = json.loads(original(metarel))
    matches = 0
    for variant in metadata['variants']:
        for entry in variant.get('files', []):
            if entry['name'] == CLIENT.name:
                entry.update(size=len(after), **{a: hashlib.new(a, after).hexdigest() for a in ALGS})
                matches += 1
    assert matches
    updates[metarel] = json_bytes(metadata)
    for rel in (CLIENT, metarel):
        for alg in ALGS: updates[Path(str(rel) + '.' + alg)] = hashlib.new(alg, updates[rel]).hexdigest().encode()
    assert inputs == source_snapshot(), 'Source changed during build'
    for rel, data in updates.items(): write(BUILD / 'candidate' / rel, data)
    report = {
        'base_commit': BASE, 'applied': False, 'source_sha256': inputs,
        'before_sha256': sha(before), 'after_sha256': sha(after),
        'patched_dex_sha256': sha((BUILD / 'patched.dex').read_bytes()),
        'donor_jar_sha256': sha(donor.read_bytes()),
        'published_file_sha256': {rel.as_posix(): sha(data) for rel, data in updates.items()},
        'toolchain': {'java': str(java), 'javac': str(javac), 'android_jar': str(android), 'd8_jar': str(d8), 'tool_cp': tool_cp},
        'preserved': 'All native libraries, VRS/prediction patches, constructors, fields and all methods except three allowlisted gaze methods',
        'structural_check': 'Every DEX class canonically round-tripped; exactly one allowlisted class, three methods replaced',
    }
    write(BUILD / 'candidate-verification.json', json_bytes(report))
    print(json.dumps({k: v for k, v in report.items() if k not in ('source_sha256', 'published_file_sha256', 'toolchain')}, indent=2))

if __name__ == '__main__': main()
