"""Build P2 lifecycle fixes on the committed P1 SDK. --apply updates Maven artifacts."""
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
BUILD = HERE/'.build'
P1 = HERE.parent/'client-sdk-p1'
BASE = '210f74320180e9d58135deb4305094618d70eff1'
spec = importlib.util.spec_from_file_location('p1_build_helpers', P1/'build.py')
p1 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p1)
sha, read_zip, replace_zip, changed_members = p1.sha, p1.read_zip, p1.replace_zip, p1.changed_members
ALGS, MODULE = p1.ALGS, p1.MODULE
ARTIFACTS = {n:Path(f'repo/com/htc/vr/{n}/6.2.0/{n}-6.2.0.aar') for n in ('wvr_client','wvr_ime_client','wvr_permission_client')}
GROUPS = {'notify':('wvr_client',8), 'camera':('wvr_client',8),
          'optional/ime':('wvr_ime_client',8), 'optional/permission':('wvr_permission_client',8)}

def run(args): subprocess.run(list(map(str,args)),check=True,cwd=ROOT)
def original(rel): return subprocess.check_output(['git','show',BASE+':'+rel.as_posix()],cwd=ROOT)
def write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(data)
def json_bytes(data): return (json.dumps(data,indent=2)+'\n').encode()
def source_sha(path):
    # Git may check text out with CRLF on Windows; hash logical source consistently.
    return sha(path.read_bytes().replace(b'\r\n',b'\n'))
def sources(group):
    folder=HERE/group
    return sorted((folder/'src').rglob('*.java'))+sorted((folder/'compile-stubs').rglob('*.java'))
def source_snapshot(groups):
    files={HERE/'build.py',P1/'build.py',P1/'tools/Graft.java'}
    for group in groups:
        files.add(HERE/group/'graft-manifest.json')
        files.update(sources(group))
    return {p.relative_to(ROOT).as_posix():source_sha(p) for p in sorted(files)}
def jar_classes(folder,dest):
    with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(folder.rglob('*.class')):
            e=zipfile.ZipInfo(p.relative_to(folder).as_posix(),(1980,1,1,0,0,0))
            e.compress_type=zipfile.ZIP_DEFLATED
            z.writestr(e,p.read_bytes())

def main():
    ap=argparse.ArgumentParser(__doc__)
    ap.add_argument('--apply',action='store_true')
    ap.add_argument('--group',action='append',choices=GROUPS,help='Build selected groups only; cannot publish partial fixes')
    ap.add_argument('--java-home',default=os.environ.get('JAVA_HOME','C:/Program Files/Android/Android Studio/jbr'))
    ap.add_argument('--android-sdk',default=os.environ.get('ANDROID_HOME',str(Path.home()/'AppData/Local/Android/Sdk')))
    args=ap.parse_args()
    groups=args.group or list(GROUPS)
    assert not args.apply or set(groups)==set(GROUPS)
    inputs=source_snapshot(groups)
    jdk,sdk=Path(args.java_home),Path(args.android_sdk)
    suffix='.exe' if os.name=='nt' else ''
    java,javac=(jdk/'bin'/(n+suffix) for n in ('java','javac'))
    android=sdk/'platforms/android-36.1/android.jar'
    d8=sdk/'build-tools/36.1.0/lib/d8.jar'
    for p in (java,javac,android,d8): assert p.is_file(),p
    dependencies=[]
    for name,(url,digest) in p1.DEPS.items():
        path=BUILD/'deps'/name
        if not path.exists():
            cached=P1/'.build/deps'/name
            if cached.is_file(): write(path,cached.read_bytes())
            else:
                path.parent.mkdir(parents=True,exist_ok=True)
                urllib.request.urlretrieve('https://repo.maven.apache.org/maven2/'+url,path)
        assert sha(path.read_bytes())==digest,path
        dependencies.append(path)
    cp=os.pathsep.join(map(str,dependencies))
    run([javac,'-cp',cp,'-d',BUILD/'tools',P1/'tools/Graft.java'])
    tool_cp=os.pathsep.join([str(BUILD/'tools'),cp])
    selected={GROUPS[g][0] for g in groups}
    before={name:original(ARTIFACTS[name]) for name in selected}
    for name,data in before.items(): write(BUILD/'original'/f'{name}.jar',read_zip(data,'classes.jar'))
    utils=Path('repo/com/htc/vr/wvr.common.utils/6.2.0/wvr.common.utils-6.2.0.aar')
    write(BUILD/'original/utils.jar',read_zip(original(utils),'classes.jar'))
    classes=[]
    donor_dex=[]
    after={}
    manifest_records={}
    for group in groups:
        name,release=GROUPS[group]
        folder=HERE/group
        manifest=folder/'graft-manifest.json'
        policy=json.loads(manifest.read_text())
        manifest_records[group]=policy
        out=BUILD/group/'classes'
        out.mkdir(parents=True,exist_ok=True)
        assert out.resolve().is_relative_to(BUILD.resolve())
        for stale in out.rglob('*.class'): stale.unlink()
        compile_cp=os.pathsep.join(map(str,[android,BUILD/'original'/f'{name}.jar',BUILD/'original/utils.jar']))
        run([javac,'--release',str(release),'-encoding','UTF-8','-cp',compile_cp,'-d',out,*sources(group)])
        donor=BUILD/group/'donor.jar'
        jar_classes(out,donor)
        if name=='wvr_client':
            classes.extend(policy['classes'])
            dexdir=BUILD/group/'dex'
            dexdir.mkdir(exist_ok=True)
            run([java,'-cp',d8,'com.android.tools.r8.D8','--release','--min-api','29','--lib',android,'--output',dexdir,donor])
            donor_dex.append(dexdir/'classes.dex')
        else:
            patched=BUILD/group/'patched.jar'
            run([java,'-cp',tool_cp,'Graft','jar',manifest,BUILD/'original'/f'{name}.jar',patched,donor])
            after[name]=replace_zip(before[name],{'classes.jar':patched.read_bytes()})
            assert changed_members(before[name],after[name])==['classes.jar']
    if classes:
        oldmodule=read_zip(before['wvr_client'],MODULE)
        olddex=read_zip(oldmodule,'classes.dex')
        assert sha(olddex)=='2b37042b6dac0c5f75642ea916b23350b968cc7d214d49117065be8fd80bfba8'
        write(BUILD/'original.dex',olddex)
        manifest=BUILD/'runtime-graft-manifest.json'
        write(manifest,json_bytes({'classes':classes}))
        run([java,'-Xmx2g','-Dandroid.jar='+str(android),'-cp',tool_cp,'Graft','dex',manifest,BUILD/'original.dex',BUILD/'patched.dex',*donor_dex])
        module=replace_zip(oldmodule,{'classes.dex':(BUILD/'patched.dex').read_bytes()})
        after['wvr_client']=replace_zip(before['wvr_client'],{MODULE:module})
        assert changed_members(oldmodule,module)==['classes.dex']
        assert changed_members(before['wvr_client'],after['wvr_client'])==[MODULE]
        assert sha(read_zip(module,'lib/arm64-v8a/libwvr_runtime.so'))=='7d6bcf757430196b73eb44e217dd4d1e18c9b4873f910fdd008cf1fe4fd9c72a'
    updates={}
    records={}
    for name,data in after.items():
        rel=ARTIFACTS[name]
        updates[rel]=data
        metarel=rel.with_suffix('.module')
        metadata=json.loads(original(metarel))
        matched=0
        for variant in metadata['variants']:
            for entry in variant.get('files',[]):
                if entry['name']==rel.name:
                    entry.update(size=len(data),**{a:hashlib.new(a,data).hexdigest() for a in ALGS})
                    matched+=1
        assert matched>0
        updates[metarel]=json_bytes(metadata)
        for path in (rel,metarel):
            for alg in ALGS: updates[Path(str(path)+'.'+alg)]=hashlib.new(alg,updates[path]).hexdigest().encode()
        records[rel.as_posix()]={'before_sha256':sha(before[name]),'after_sha256':sha(data),'bytes':len(data)}
    assert inputs==source_snapshot(groups),'Sources changed during build; rebuild'
    report={'base_commit':BASE,'applied':args.apply,'groups':groups,'source_sha256':inputs,'artifacts':records,
            'source_hash_normalization':'CRLF to LF; artifact hashes use exact bytes',
            'preserved':'P1 methods/classes and all native library bytes; only allowlisted P2 Java code changes',
            'maven_checksums':list(ALGS)}
    if classes: report['patched_dex_sha256']=sha((BUILD/'patched.dex').read_bytes())
    for rel,data in updates.items(): write(BUILD/'candidate'/rel,data)
    write(BUILD/'candidate-verification.json',json_bytes(report))
    if args.apply:
        prior=json.loads((HERE/'VERIFICATION.json').read_text()) if (HERE/'VERIFICATION.json').exists() else {}
        for rel in updates:
            current=(ROOT/rel).read_bytes()
            # Check every published file, including metadata, against baseline or prior output.
            baseline=original(rel)
            priorhash=prior.get('published_file_sha256',{}).get(rel.as_posix())
            # Previously unpinned module JSON may have a CRLF Git checkout.
            # Accept only that exact baseline text; publish LF matching sidecars.
            baseline_text = rel.suffix=='.module' and current.replace(b'\r\n',b'\n')==baseline
            assert baseline_text or sha(current) in {sha(baseline),sha(updates[rel]),priorhash},f'Refusing unrelated changes: {rel}'
        for rel,data in updates.items(): write(ROOT/rel,data)
        report['published_file_sha256']={rel.as_posix():sha(data) for rel,data in updates.items()}
        write(HERE/'VERIFICATION.json',json_bytes(report))
    print(json.dumps({k:v for k,v in report.items() if k not in ('source_sha256','published_file_sha256')},indent=2))

if __name__=='__main__': main()
