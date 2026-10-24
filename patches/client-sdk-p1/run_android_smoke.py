"""Exercise a built candidate in a temporary shell ART process, never the installed VR app."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import uuid
import zipfile
from build import HERE, BUILD, CLIENT, MODULE, read_zip, sha

def main():
    p=argparse.ArgumentParser(__doc__)
    p.add_argument('--serial',required=True)
    p.add_argument('--java-home',default=os.environ.get('JAVA_HOME','C:/Program Files/Android/Android Studio/jbr'))
    p.add_argument('--android-sdk',default=os.environ.get('ANDROID_HOME',str(Path.home()/'AppData/Local/Android/Sdk')))
    args=p.parse_args()
    sdk,jdk=Path(args.android_sdk),Path(args.java_home)
    suffix='.exe' if os.name=='nt' else ''
    adb=[str(sdk/'platform-tools'/('adb'+suffix)),'-s',args.serial]
    def run(cmd,timeout=45):
        r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=timeout)
        if r.returncode: raise RuntimeError(f'{cmd[0]} exited {r.returncode}\n{r.stdout}\n{r.stderr}')
        return r.stdout+r.stderr
    work=BUILD/'android-smoke'
    work.mkdir(parents=True,exist_ok=True)
    android=sdk/'platforms/android-36.1/android.jar'
    cp=os.pathsep.join(map(str,[android,BUILD/'utils-patched.jar']))
    run([jdk/'bin'/('javac'+suffix),'--release','8','-cp',cp,'-d',work/'classes',HERE/'tests/AndroidSmoke.java'])
    harness=work/'harness.jar'
    with zipfile.ZipFile(harness,'w') as z:
        for f in (work/'classes').rglob('*.class'): z.write(f,f.relative_to(work/'classes').as_posix())
    dex=work/'dex'
    dex.mkdir(exist_ok=True)
    run([jdk/'bin'/('java'+suffix),'-cp',sdk/'build-tools/36.1.0/lib/d8.jar','com.android.tools.r8.D8','--release','--min-api','29','--lib',android,'--classpath',BUILD/'utils-patched.jar','--output',dex,harness])
    module=work/'client-module.apk'
    module.write_bytes(read_zip((BUILD/'candidate'/CLIENT).read_bytes(),MODULE))
    remote='/data/local/tmp/wavesdk-p1-'+uuid.uuid4().hex
    run(adb+['shell','mkdir',remote])
    try:
        run(adb+['push',module,remote+'/client-module.apk'])
        run(adb+['push',dex/'classes.dex',remote+'/smoke.dex'])
        # app_process registers Android framework JNI (MessageQueue/EGL), unlike bare dalvikvm.
        output=run(adb+['shell','timeout','30','env','CLASSPATH='+remote+'/client-module.apk:'+remote+'/smoke.dex','app_process','-Xverify:all','/system/bin','AndroidSmoke'])
        assert 'PASS Android ART smoke:' in output, output
        report={'serial':args.serial,'status':'passed','module_sha256':sha(module.read_bytes()),'dex_sha256':sha(read_zip(module.read_bytes(),'classes.dex')),'scope':'isolated shell ART; no app installation, compositor binding, or Wave JNI calls','output':output}
        (HERE/'ANDROID_SMOKE.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(output)
    finally:
        run(adb+['shell','rm','-f',remote+'/client-module.apk',remote+'/smoke.dex'])
        run(adb+['shell','rmdir',remote])

if __name__=='__main__': main()
