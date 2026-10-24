"""Build a version-locked experimental Wave module copy; never edits the input.

Only for the observed 75/90Hz regression experiment. Changes the initialized
gMaxPosePredictionTime float from .050 to .060 seconds, retaining error checks.
This is not a signed Android-installable replacement for HTC's system package.
"""
import argparse
import copy
import hashlib
import json
import struct
import zipfile
from pathlib import Path

PROFILES = {
    '19fade83706d9427346644aa47ddb01a85a5e5bfec1eb8af75062ffd803dd667': {
        'version':'6.2.0-u09', 'runtime_sha256':'7abd01d50ac948b38a41a55cef5a461b018ed29e1856d8198aa5deeb6db29d18',
        'file_offset':0xA329A4, 'vaddr':0xA349A4},
    '5047eed07a6234f24a8a7a4c806d544b98deb6f8980ec7348443b10004f419d6': {
        'version':'6.2.0-u25', 'runtime_sha256':'b4e0e369b53e1ca016a33ddacc2af31400b134f05372532b0b02153c70c3a72b',
        'file_offset':0xA31F44, 'vaddr':0xA33F44},
}
MEMBER = 'lib/arm64-v8a/libwvr_runtime.so'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    module_sha = sha(args.module.read_bytes())
    if module_sha not in PROFILES:
        raise ValueError('Unknown module SHA-256; refusing version-dependent binary patch')
    profile = PROFILES[module_sha]
    args.output_dir.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(args.module) as original:
        runtime = original.read(MEMBER)
        assert sha(runtime) == profile['runtime_sha256']
        offset = profile['file_offset']
        before, after = struct.pack('<f', 0.05), struct.pack('<f', 0.06)
        assert runtime[offset:offset+4] == before
        patched = runtime[:offset] + after + runtime[offset+4:]
        assert runtime[:offset] == patched[:offset] and runtime[offset+4:] == patched[offset+4:]
        original_path = args.output_dir/'libwvr_runtime.original.so'
        candidate_path = args.output_dir/'libwvr_runtime.cap60.so'
        original_path.write_bytes(runtime)
        candidate_path.write_bytes(patched)
        output_module = args.output_dir/'client-module.cap60.apk'
        with zipfile.ZipFile(output_module, 'x') as output:
            output.comment = original.comment
            for info in original.infolist():
                output.writestr(copy.copy(info), patched if info.filename == MEMBER else original.read(info))
        with zipfile.ZipFile(output_module) as output:
            assert output.testzip() is None
            assert original.namelist() == output.namelist()
            changed_members = [name for name in original.namelist() if original.read(name) != output.read(name)]
            assert changed_members == [MEMBER]
    changed_offsets = [i for i,(a,b) in enumerate(zip(runtime,patched)) if a != b]
    manifest = {
        'status':'experimental offline-validated candidate; not live validated or deployed',
        'version':profile['version'], 'original_module_path':str(args.module.resolve()),
        'original_module_sha256':module_sha, 'original_runtime_sha256':sha(runtime),
        'candidate_runtime_sha256':sha(patched), 'candidate_module_sha256':sha(output_module.read_bytes()),
        'modified_member':MEMBER, 'cap_file_offset':hex(offset), 'cap_virtual_address':hex(profile['vaddr']),
        'before_bytes':before.hex(), 'after_bytes':after.hex(),
        'changed_byte_offsets':[hex(o) for o in changed_offsets],
        'old_seconds':struct.unpack('<f',before)[0], 'new_seconds':struct.unpack('<f',after)[0],
        'zip_crc_verification':'passed', 'other_member_contents_identical':True,
        'limitations':['Only arm64 runtime changed; other ABIs remain original.',
                       'Any APK signing metadata is no longer a valid signature for the modified archive.',
                       'This is an app-private module candidate, not an installable provider update.',
                       'Mutable runtime settings can override the initializer.',
                       'The cap is shared with pose-prediction callers; tracking accuracy above50ms needs headset validation.',
                       '60ms accommodates the tested75Hz path but is not a universal refresh-aware fix.'],
        'rollback':'Use the byte-identical original module, clear only the diagnostic app module cache, and restart the diagnostic app.'}
    (args.output_dir/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(manifest,indent=2))


if __name__ == '__main__':
    main()
