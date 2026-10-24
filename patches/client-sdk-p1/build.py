"""Rebuild the two P1 fixes from pinned SDK artifacts; --apply updates Maven files.

Requires Python 3.10+, JDK 17+, Android build-tools 36.1.0 and android-36.1.
Only pinned Maven build dependencies are downloaded (SHA-256 checked).
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BUILD = HERE / '.build'
BASE = '647da312ad377108e4187a3643bffaba7662b05d'
CLIENT = Path('repo/com/htc/vr/wvr_client/6.2.0/wvr_client-6.2.0.aar')
UTILS = Path('repo/com/htc/vr/wvr.common.utils/6.2.0/wvr.common.utils-6.2.0.aar')
MODULE = 'assets/wavevr/client-module.apk'
ALGS = ('md5', 'sha1', 'sha256', 'sha512')
DEPS = {
    'dexlib2-2.5.2.jar': ('org/smali/dexlib2/2.5.2/dexlib2-2.5.2.jar', '5a5c8982d8bd7d6e3bb1a0713049e3c78b719ec32b20f6b619885cec30a0dd61'),
    'guava-27.1-android.jar': ('com/google/guava/guava/27.1-android/guava-27.1-android.jar', '686404f2d1d4d221911f96bd627ff60dac2226a5dfa6fb8ba517073eb97ec0ef'),
    'gson-2.10.1.jar': ('com/google/code/gson/gson/2.10.1/gson-2.10.1.jar', '4241c14a7727c34feea6507ec801318a3d4a90f070e4525681079fb94ee4c593'),
    'asm-9.2.jar': ('org/ow2/asm/asm/9.2/asm-9.2.jar', 'b9d4fe4d71938df38839f0eca42aaaa64cf8b313d678da036f0cb3ca199b47f5'),
    'asm-tree-9.2.jar': ('org/ow2/asm/asm-tree/9.2/asm-tree-9.2.jar', 'aabf9bd23091a4ebfc109c1f3ee7cf3e4b89f6ba2d3f51c5243f16b3cffae011'),
}

def sha(data): return hashlib.sha256(data).hexdigest()
def run(args): subprocess.run([str(x) for x in args], check=True, cwd=ROOT)
def original(path): return subprocess.check_output(['git', 'show', BASE + ':' + path.as_posix()], cwd=ROOT)
def read_zip(data, name):
    with zipfile.ZipFile(io.BytesIO(data)) as z: return z.read(name)
def replace_zip(data, replacements):
    out = io.BytesIO()
    with zipfile.ZipFile(io.BytesIO(data)) as source, zipfile.ZipFile(out, 'w') as dest:
        assert set(replacements) <= set(source.namelist())
        dest.comment = source.comment
        for entry in source.infolist():
            dest.writestr(copy.copy(entry), replacements.get(entry.filename, source.read(entry)))
    return out.getvalue()
def changed_members(before, after):
    with zipfile.ZipFile(io.BytesIO(before)) as a, zipfile.ZipFile(io.BytesIO(after)) as b:
        assert a.testzip() is None and b.testzip() is None
        assert a.namelist() == b.namelist()
        return [n for n in a.namelist() if a.read(n) != b.read(n)]
def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
def json_bytes(data): return (json.dumps(data, indent=2) + '\n').encode()
def source_snapshot(groups):
    files = {HERE/'build.py', HERE/'tools/Graft.java'}
    for group in groups:
        folder = HERE/group
        files.add(folder/('graft-manifest.json' if group == 'dispatch' else 'patch-spec.json'))
        for part in ('src','compile-stubs','donor','tests/stubs/com','tests/stubs/androidx'):
            files.update((folder/part).rglob('*.java'))
    return {p.relative_to(HERE).as_posix():sha(p.read_bytes()) for p in sorted(files)}

def main():
    ap = argparse.ArgumentParser(__doc__)
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--dispatch-only', action='store_true', help='Build utility JAR for focused host regressions; do not publish')
    ap.add_argument('--java-home', default=os.environ.get('JAVA_HOME', 'C:/Program Files/Android/Android Studio/jbr'))
    ap.add_argument('--android-sdk', default=os.environ.get('ANDROID_HOME', str(Path.home() / 'AppData/Local/Android/Sdk')))
    args = ap.parse_args()
    assert not (args.apply and args.dispatch_only)
    groups = ['dispatch'] if args.dispatch_only else ['dispatch', 'compositor']
    inputs = source_snapshot(groups)
    java_home, sdk = Path(args.java_home), Path(args.android_sdk)
    suffix = '.exe' if os.name == 'nt' else ''
    java, javac = (java_home / 'bin' / (n + suffix) for n in ('java', 'javac'))
    android = sdk / 'platforms/android-36.1/android.jar'
    d8 = sdk / 'build-tools/36.1.0/lib/d8.jar'
    for p in (java, javac, android, d8): assert p.is_file(), p
    jars = []
    for name, (url, digest) in DEPS.items():
        p = BUILD / 'deps' / name
        p.parent.mkdir(parents=True, exist_ok=True)
        if not p.exists(): urllib.request.urlretrieve('https://repo.maven.apache.org/maven2/' + url, p)
        assert sha(p.read_bytes()) == digest, f'Dependency checksum mismatch: {name}'
        jars.append(p)
    cp = os.pathsep.join(map(str, jars))
    run([javac, '-cp', cp, '-d', BUILD/'tools', HERE/'tools/Graft.java'])
    tool_cp = os.pathsep.join([str(BUILD/'tools'), cp])
    classes = []
    donor_dex = []
    for group in groups:
        folder = HERE/group
        manifest = json.loads((folder/('graft-manifest.json' if group == 'dispatch' else 'patch-spec.json')).read_text())
        classes.extend(manifest['classes'])
        src = sorted((folder/'src').rglob('*.java')) + sorted((folder/'compile-stubs').rglob('*.java'))
        if group == 'compositor':
            src += sorted((folder/'donor').rglob('*.java'))
            src += sorted((folder/'tests/stubs/com').rglob('*.java'))
            src += sorted((folder/'tests/stubs/androidx').rglob('*.java'))
        output = BUILD/group/'classes'
        output.mkdir(parents=True, exist_ok=True)
        # Remove only generated class files inside this build directory, avoiding stale donors.
        assert output.resolve().is_relative_to(BUILD.resolve())
        for stale in output.rglob('*.class'): stale.unlink()
        run([javac, '--release', '8' if group == 'dispatch' else '11', '-cp', android, '-d', output, *src])
        donor_jar = BUILD/group/'donor.jar'
        with zipfile.ZipFile(donor_jar, 'w', zipfile.ZIP_DEFLATED) as z:
            # Include compile stubs for D8 resolution/nest desugaring only; Graft's manifest
            # is the sole allowlist for copying anything into the shipped artifacts.
            for p in sorted(output.rglob('*.class')):
                e = zipfile.ZipInfo(p.relative_to(output).as_posix(), (1980,1,1,0,0,0))
                e.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(e, p.read_bytes())
        dest = BUILD/group/'dex'
        dest.mkdir(parents=True, exist_ok=True)
        run([java, '-cp', d8, 'com.android.tools.r8.D8', '--release', '--min-api', '29', '--lib', android, '--output', dest, donor_jar])
        donor_dex.append(dest/'classes.dex')
    manifest_path = BUILD/'graft-manifest.json'
    write(manifest_path, json_bytes({'classes':classes}))
    client_before, utils_before = original(CLIENT), original(UTILS)
    assert sha(client_before) == 'efe06b38dc84827d7b46f44d5a45dd535e7d1754257c739cc3bea5de9ef20b41'
    module_before = read_zip(client_before, MODULE)
    dex_before = read_zip(module_before, 'classes.dex')
    assert sha(dex_before) == '5dc75aaf96a0d77b153a22936d66358aa0aa17e27e6bf3d13433f37136c22475'
    write(BUILD/'original.dex', dex_before)
    write(BUILD/'utils-original.jar', read_zip(utils_before,'classes.jar'))
    run([java,'-cp',tool_cp,'Graft','jar',manifest_path,BUILD/'utils-original.jar',BUILD/'utils-patched.jar',BUILD/'dispatch/donor.jar'])
    if args.dispatch_only:
        assert source_snapshot(groups) == inputs, 'Source changed during compilation; rebuild'
        print('Dispatch JAR ready:', BUILD/'utils-patched.jar')
        return
    run([java,'-Xmx2g','-Dandroid.jar='+str(android),'-cp',tool_cp,'Graft','dex',manifest_path,BUILD/'original.dex',BUILD/'patched.dex',*donor_dex])
    module_after = replace_zip(module_before, {'classes.dex':(BUILD/'patched.dex').read_bytes()})
    client_after = replace_zip(client_before, {MODULE:module_after})
    utils_after = replace_zip(utils_before, {'classes.jar':(BUILD/'utils-patched.jar').read_bytes()})
    assert changed_members(client_before,client_after) == [MODULE]
    assert changed_members(module_before,module_after) == ['classes.dex']
    assert changed_members(utils_before,utils_after) == ['classes.jar']
    runtime = read_zip(module_after,'lib/arm64-v8a/libwvr_runtime.so')
    assert sha(runtime) == '7d6bcf757430196b73eb44e217dd4d1e18c9b4873f910fdd008cf1fe4fd9c72a'
    records = {}
    updates = {}
    for rel, before, after in ((CLIENT,client_before,client_after),(UTILS,utils_before,utils_after)):
        updates[rel] = after
        metadata_rel = rel.with_suffix('.module')
        metadata = json.loads(original(metadata_rel))
        found = 0
        for variant in metadata['variants']:
            for entry in variant.get('files',[]):
                if entry['name'] == rel.name:
                    entry.update(size=len(after),**{a:hashlib.new(a,after).hexdigest() for a in ALGS})
                    found += 1
        assert found > 0
        updates[metadata_rel] = json_bytes(metadata)
        for p in (rel, metadata_rel):
            for alg in ALGS: updates[Path(str(p)+'.'+alg)] = hashlib.new(alg,updates[p]).hexdigest().encode()
        records[rel.as_posix()] = {'before_sha256':sha(before),'after_sha256':sha(after),'bytes':len(after)}
    report = {'base_commit':BASE,'applied':args.apply,'artifacts':records,
        'source_sha256':inputs,
        'patched_dex_sha256':sha((BUILD/'patched.dex').read_bytes()),'native_runtime_sha256':sha(runtime),
        'native_libraries_unchanged':True,'only_client_module_member_changed':'classes.dex',
        'only_utils_member_changed':'classes.jar','dex_structural_roundtrip_verified':True,
        'maven_checksums':list(ALGS)}
    assert source_snapshot(groups) == inputs, 'Source changed during compilation; rebuild'
    # Candidate files always stay available for review/tests; no app/device installation.
    for rel, data in updates.items(): write(BUILD/'candidate'/rel, data)
    if args.apply:
        prior = json.loads((HERE/'VERIFICATION.json').read_text()) if (HERE/'VERIFICATION.json').exists() else {}
        for rel in (CLIENT,UTILS):
            current = sha((ROOT/rel).read_bytes())
            allowed = {records[rel.as_posix()]['before_sha256'],records[rel.as_posix()]['after_sha256']}
            if prior.get('applied'): allowed.add(prior['artifacts'][rel.as_posix()]['after_sha256'])
            assert current in allowed, f'Refusing to overwrite unrelated artifact changes: {rel}'
        for rel,data in updates.items(): write(ROOT/rel,data)
        write(HERE/'VERIFICATION.json',json_bytes(report))
    write(BUILD/'candidate-verification.json',json_bytes(report))
    print(json.dumps(report,indent=2))

if __name__ == '__main__': main()
