"""Reproduce the private compositor VRS extension on the exact lifecycle-fixed SDK."""
import argparse
import copy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BUILD = HERE / '.build'
BASE = '023dc61446973ca6c7a968ef824262fb74ee4ba9'
AAR = Path('repo/com/htc/vr/wvr_client/6.2.0/wvr_client-6.2.0.aar')
MODULE = 'assets/wavevr/client-module.apk'
RUNTIME = 'lib/arm64-v8a/libwvr_runtime.so'
ALGS = ('md5', 'sha1', 'sha256', 'sha512')

def sha(data): return hashlib.sha256(data).hexdigest()
def original(rel): return subprocess.check_output(['git','show',BASE+':'+rel.as_posix()],cwd=ROOT)
def member(data,name):
    with zipfile.ZipFile(io.BytesIO(data)) as z: return z.read(name)
def replace(data,updates):
    out=io.BytesIO()
    with zipfile.ZipFile(io.BytesIO(data)) as src, zipfile.ZipFile(out,'w') as dst:
        dst.comment=src.comment
        for info in src.infolist(): dst.writestr(copy.copy(info),updates.get(info.filename,src.read(info)))
        for name,value in updates.items():
            if name not in src.namelist():
                info=zipfile.ZipInfo(name,(1980,1,1,0,0,0)); info.compress_type=zipfile.ZIP_DEFLATED
                dst.writestr(info,value)
    return out.getvalue()
def changed(before,after):
    with zipfile.ZipFile(io.BytesIO(before)) as a,zipfile.ZipFile(io.BytesIO(after)) as b:
        assert set(a.namelist()) <= set(b.namelist())
        assert b.testzip() is None
        return sorted(n for n in b.namelist() if n not in a.namelist() or a.read(n)!=b.read(n))
def write(path,data): path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
def js(data): return (json.dumps(data,indent=2)+'\n').encode()
def run(args): subprocess.run(list(map(str,args)),cwd=ROOT,check=True)
def source_hashes():
    files=[HERE/'build.py',HERE/'patch_runtime.py',HERE/'native_hook.S',HERE/'extension/consumer-rules.pro']
    files+=list((HERE/'extension').rglob('*.java'))+list((HERE/'extension').rglob('*.cpp'))+list((HERE/'extension').rglob('*.h'))
    return {p.relative_to(ROOT).as_posix():sha(p.read_bytes().replace(b'\r\n',b'\n')) for p in sorted(files) if '.build' not in p.parts}

def main():
    ap=argparse.ArgumentParser(__doc__)
    ap.add_argument('--apply',action='store_true')
    ap.add_argument('--android-sdk',type=Path,default=Path(os.environ['LOCALAPPDATA'])/'Android/Sdk')
    ap.add_argument('--java-home',type=Path,default=Path('C:/Program Files/Android/Android Studio/jbr'))
    args=ap.parse_args()
    sdk=args.android_sdk; ndk=sdk/'ndk/25.1.8937393'
    jdk=args.java_home/'bin'; android=sdk/'platforms/android-36.1/android.jar'
    llvm=ndk/'toolchains/llvm/prebuilt/windows-x86_64/bin'
    before=original(AAR)
    assert sha(before)=='a934a9f3108cd3ca1e66d8d9f3b4c5b4cb9e538222a0e93b0ddad5b98c3771e2'
    oldmodule=member(before,MODULE); oldruntime=member(oldmodule,RUNTIME)
    write(BUILD/'original.so',oldruntime)
    inputs=source_hashes()
    spec=importlib.util.spec_from_file_location('vrs_patch',HERE/'patch_runtime.py')
    patch=importlib.util.module_from_spec(spec); spec.loader.exec_module(patch)
    runtime,native_report=patch.patch_runtime(oldruntime,ndk,BUILD/'native-patch')
    write(BUILD/'libwvr_runtime.so',runtime)
    identity=BUILD/'generated/com/sandboxvr/wave/PatchedRuntimeIdentity.java'
    write(identity,('package com.sandboxvr.wave;\nfinal class PatchedRuntimeIdentity { static final String SHA256 = "'+sha(runtime)+'"; }\n').encode())
    classes=BUILD/'extension-classes'; classes.mkdir(parents=True,exist_ok=True)
    assert classes.resolve().is_relative_to(BUILD.resolve())
    for stale in classes.rglob('*.class'): stale.unlink()
    java_sources=list((HERE/'extension/src').rglob('*.java'))
    run([jdk/'javac.exe','--release','8','-encoding','UTF-8','-cp',android,'-d',classes,*java_sources,identity])
    additions={p.relative_to(classes).as_posix():p.read_bytes() for p in sorted(classes.rglob('*.class'))}
    jar=replace(member(before,'classes.jar'),additions)
    native=BUILD/'libssvr_wave_compositor.so'
    cpp=list((HERE/'extension/native').glob('*.cpp'))
    run([llvm/'clang++.exe','--target=aarch64-linux-android29','--sysroot='+str(ndk/'toolchains/llvm/prebuilt/windows-x86_64/sysroot'),
         '-shared','-fPIC','-O2','-std=c++17','-fvisibility=hidden','-Wl,--build-id=sha1','-Wl,-z,relro,-z,now',
         '-static-libstdc++',*cpp,'-ldl','-llog','-o',native])
    module=replace(oldmodule,{RUNTIME:runtime})
    after=replace(before,{MODULE:module,'classes.jar':jar,'jni/arm64-v8a/libssvr_wave_compositor.so':native.read_bytes(),
                          'proguard.txt':(HERE/'extension/consumer-rules.pro').read_bytes().replace(b'\r\n',b'\n')})
    assert changed(oldmodule,module)==[RUNTIME]
    assert changed(before,after)==sorted([MODULE,'classes.jar','jni/arm64-v8a/libssvr_wave_compositor.so','proguard.txt'])
    assert inputs==source_hashes(),'Sources changed during build'
    updates={AAR:after}
    meta=AAR.with_suffix('.module'); document=json.loads(original(meta))
    for variant in document['variants']:
        for f in variant.get('files',[]):
            if f['name']==AAR.name: f.update(size=len(after),**{a:hashlib.new(a,after).hexdigest() for a in ALGS})
    updates[meta]=js(document)
    for rel in (AAR,meta):
        for alg in ALGS: updates[Path(str(rel)+'.'+alg)]=hashlib.new(alg,updates[rel]).hexdigest().encode()
    report={'base_commit':BASE,'applied':args.apply,'source_sha256':inputs,'runtime_sha256':sha(runtime),'native_patch':native_report,
            'aar_sha256':sha(after),'module_sha256':sha(module),'extension_sha256':sha(native.read_bytes()),
            'preserved_module_dex_sha256':sha(member(module,'classes.dex')),
            'published_sha256':{rel.as_posix():sha(data) for rel,data in updates.items()}}
    for rel,data in updates.items(): write(BUILD/'candidate'/rel,data)
    write(BUILD/'VERIFICATION.json',js(report))
    if args.apply:
        previous=json.loads((HERE/'VERIFICATION.json').read_text()) if (HERE/'VERIFICATION.json').exists() else {}
        for rel,data in updates.items():
            current=(ROOT/rel).read_bytes()
            assert sha(current) in {sha(original(rel)),sha(data),previous.get('published_sha256',{}).get(rel.as_posix())},f'Unrelated edit: {rel}'
        for rel,data in updates.items(): write(ROOT/rel,data)
        write(HERE/'VERIFICATION.json',js(report))
    print(json.dumps({k:report[k] for k in ('applied','runtime_sha256','aar_sha256','module_sha256')},indent=2))

if __name__=='__main__': main()
