"""Resolve final P2 artifacts and exercise retained P1 dispatch in an isolated headset process."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import uuid
import zipfile
from build import HERE, BUILD, ROOT, P1, ARTIFACTS, GROUPS, MODULE, read_zip, sha, source_sha, jar_classes

def main():
    ap=argparse.ArgumentParser(__doc__)
    ap.add_argument('--serial',required=True)
    ap.add_argument('--java-home',default=os.environ.get('JAVA_HOME','C:/Program Files/Android/Android Studio/jbr'))
    ap.add_argument('--android-sdk',default=os.environ.get('ANDROID_HOME',str(Path.home()/'AppData/Local/Android/Sdk')))
    args=ap.parse_args()
    sdk,jdk=Path(args.android_sdk),Path(args.java_home)
    suffix='.exe' if os.name=='nt' else ''
    adb=[str(sdk/'platform-tools'/('adb'+suffix)),'-s',args.serial]
    def run(cmd,timeout=45):
        result=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=timeout)
        if result.returncode: raise RuntimeError(result.stdout+result.stderr)
        return result.stdout+result.stderr
    work=BUILD/'android-smoke'
    work.mkdir(parents=True,exist_ok=True)
    android=sdk/'platforms/android-36.1/android.jar'
    java,javac=(jdk/'bin'/(n+suffix) for n in ('java','javac'))
    d8=sdk/'build-tools/36.1.0/lib/d8.jar'
    utils=ROOT/'repo/com/htc/vr/wvr.common.utils/6.2.0/wvr.common.utils-6.2.0.aar'
    (work/'utils.jar').write_bytes(read_zip(utils.read_bytes(),'classes.jar'))
    compile_cp=os.pathsep.join(map(str,[android,work/'utils.jar']))
    run([javac,'--release','8','-cp',compile_cp,'-d',work/'classes',P1/'tests/AndroidSmoke.java',HERE/'tests/P2LinkSmoke.java'])
    jar_classes(work/'classes',work/'harness.jar')
    dex=work/'dex'
    dex.mkdir(exist_ok=True)
    optional=[]
    for name in ('wvr_ime_client','wvr_permission_client'):
        p=work/(name+'.jar')
        p.write_bytes(read_zip((BUILD/'candidate'/ARTIFACTS[name]).read_bytes(),'classes.jar'))
        optional.append(p)
    run([java,'-cp',d8,'com.android.tools.r8.D8','--release','--min-api','29','--lib',android,'--classpath',work/'utils.jar','--output',dex,work/'harness.jar',*optional])
    module=work/'client-module.apk'
    module.write_bytes(read_zip((BUILD/'candidate'/ARTIFACTS['wvr_client']).read_bytes(),MODULE))
    names=[]
    for group in GROUPS:
        for c in json.loads((HERE/group/'graft-manifest.json').read_text())['classes']:
            names.append(c['type'][1:-1].replace('/','.'))
    remote='/data/local/tmp/wavesdk-p2-'+uuid.uuid4().hex
    run(adb+['shell','mkdir',remote])
    try:
        run(adb+['push',module,remote+'/client-module.apk'])
        run(adb+['push',dex/'classes.dex',remote+'/smoke.dex'])
        command=adb+['shell','timeout','30','env','CLASSPATH='+remote+'/client-module.apk:'+remote+'/smoke.dex','app_process','-Xverify:all','/system/bin']
        p1out=run(command+['AndroidSmoke'])
        assert 'PASS Android ART smoke:' in p1out,p1out
        # adb shell joins arguments; quote class names to keep literal dollar signs intact.
        p2out=run(command+['P2LinkSmoke']+["'"+name+"'" for name in names])
        assert 'PASS P2 ART class/member resolution:' in p2out,p2out
        report={'serial':args.serial,'status':'passed','module_sha256':sha(module.read_bytes()),
                'dex_sha256':sha(read_zip(module.read_bytes(),'classes.dex')),'optional_jar_sha256':{p.name:sha(p.read_bytes()) for p in optional},
                'test_source_sha256':{p.relative_to(ROOT).as_posix():source_sha(p) for p in (Path(__file__),HERE/'tests/P2LinkSmoke.java',P1/'tests/AndroidSmoke.java')},
                'scope':'isolated shell app_process; P1 dispatch and P2 class/member linkage; no app replacement or optional feature activation',
                'output':p1out+p2out}
        (HERE/'ANDROID_SMOKE.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(p1out+p2out)
    finally:
        run(adb+['shell','rm','-f',remote+'/client-module.apk',remote+'/smoke.dex'])
        run(adb+['shell','rmdir',remote])

if __name__=='__main__': main()
