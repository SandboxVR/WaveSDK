"""Verify the diagnostic APK packages the exact locally verified Wave artifact."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    here = Path(__file__).resolve().parent
    patch = here.parent
    root = patch.parents[1]
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--apk', type=Path, default=here / 'app/build/outputs/apk/debug/app-debug.apk')
    args = parser.parse_args()
    report = json.loads((patch / 'VERIFICATION.json').read_text())
    aar_path = root / 'repo/com/htc/vr/wvr_client/6.2.0/wvr_client-6.2.0.aar'
    aar_data = aar_path.read_bytes()
    assert digest(aar_data) == report['aar_sha256'], 'SDK artifact is not the verified build'
    apk_data = args.apk.read_bytes()
    module_path = 'assets/wavevr/client-module.apk'
    helper_path = 'lib/arm64-v8a/libssvr_wave_compositor.so'
    with zipfile.ZipFile(io.BytesIO(aar_data)) as aar, zipfile.ZipFile(io.BytesIO(apk_data)) as apk:
        module_data = apk.read(module_path)
        assert module_data == aar.read(module_path), 'Packaged client module differs from SDK'
        assert apk.read(helper_path) == aar.read('jni/arm64-v8a/libssvr_wave_compositor.so'), 'Packaged JNI helper differs from SDK'
        dex = b''.join(apk.read(n) for n in apk.namelist() if n.startswith('classes') and n.endswith('.dex'))
        assert b'Lcom/sandboxvr/wave/CompositorSettings;' in dex, 'Missing private Java API'
        assert report['runtime_sha256'].encode() in dex, 'Missing expected runtime identity'
        assert report['aar_sha256'].encode() in dex, 'Missing diagnostic SDK identity'
    with zipfile.ZipFile(io.BytesIO(module_data)) as module:
        runtime_hash = digest(module.read('lib/arm64-v8a/libwvr_runtime.so'))
        assert runtime_hash == report['runtime_sha256'], 'Packaged runtime differs from verified SDK'
        assert digest(module.read('classes.dex')) == report['preserved_module_dex_sha256'], 'Lifecycle-fixed module DEX differs'
    print(json.dumps({'apk_sha256': digest(apk_data), 'aar_sha256': digest(aar_data),
                      'module_sha256': digest(module_data), 'runtime_sha256': runtime_hash,
                      'result': 'PASS: packaged runtime, module DEX, private API and JNI helper match verified SDK'}, indent=2))


if __name__ == '__main__':
    main()
