"""Build an isolated negative-test APK: final API with the pre-VRS runtime.

Never installs or changes a production application. Uses the diagnostic key.
The resulting APK is intentionally NOT accepted by verify_apk.py.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from build import AAR, BUILD, MODULE, RUNTIME, member, original, replace


def main():
    report = json.loads((HERE.parent / 'VERIFICATION.json').read_text())
    source = HERE / 'app/build/outputs/apk/minified/app-minified.apk'
    apk = source.read_bytes()
    module = member(apk, MODULE)
    assert hashlib.sha256(module).hexdigest() == report['module_sha256']
    runtime = member(member(original(AAR), MODULE), RUNTIME)
    runtime_hash = hashlib.sha256(runtime).hexdigest()
    assert runtime_hash == report['native_patch']['baseline_sha256']
    assert runtime_hash != report['runtime_sha256']
    output = BUILD / 'unsupported-fixture'
    output.mkdir(parents=True, exist_ok=True)
    unsigned = output / 'unsigned.apk'
    aligned = output / 'aligned.apk'
    signed = output / 'unsupported-runtime.apk'
    unsigned.write_bytes(replace(apk, {MODULE: replace(module, {RUNTIME: runtime})}))
    sdk = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk/build-tools/36.0.0'
    java = Path(os.environ.get('JAVA_HOME', 'C:/Program Files/Android/Android Studio/jbr')) / 'bin/java.exe'
    subprocess.run([str(sdk / 'zipalign.exe'), '-f', '4', str(unsigned), str(aligned)], check=True)
    signer = [str(java), '-jar', str(sdk / 'lib/apksigner.jar')]
    subprocess.run(signer + ['sign', '--ks', str(HERE / '.local/vrs-diagnostic-debug.keystore'),
                            '--ks-key-alias', 'vrs-diagnostic', '--ks-pass', 'pass:android',
                            '--key-pass', 'pass:android', '--out', str(signed), str(aligned)], check=True)
    subprocess.run(signer + ['verify', str(signed)], check=True)
    print(json.dumps({'apk': str(signed), 'apk_sha256': hashlib.sha256(signed.read_bytes()).hexdigest(),
                      'loaded_runtime_sha256': runtime_hash, 'api_expected_runtime_sha256': report['runtime_sha256'],
                      'expected_effective_mode': 'Unsupported'}, indent=2))


if __name__ == '__main__':
    main()
