"""Verify the saved SDK, nested binary patch, and Maven checksums. Standard library only."""
import copy
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[2]
BASE='0ed9b131d4e37369f0a2f8135caf5a58e9b3633a'
REL='repo/com/htc/vr/wvr_client/6.2.0/wvr_client-6.2.0.aar'
aar=ROOT/REL
sha=lambda b:hashlib.sha256(b).hexdigest()
original_bytes=subprocess.check_output(['git','show',BASE+':'+REL],cwd=ROOT)
candidate_bytes=aar.read_bytes()
assert sha(original_bytes)=='b14ced0d4c6d80e4ffff600100de3d5865aeea11095d884468381c81be04d796'
assert sha(candidate_bytes)=='efe06b38dc84827d7b46f44d5a45dd535e7d1754257c739cc3bea5de9ef20b41'
member='assets/wavevr/client-module.apk'
runtime_member='lib/arm64-v8a/libwvr_runtime.so'
with zipfile.ZipFile(io.BytesIO(original_bytes)) as original, zipfile.ZipFile(io.BytesIO(candidate_bytes)) as candidate:
    assert original.testzip() is None and candidate.testzip() is None
    assert original.namelist()==candidate.namelist()
    assert [n for n in original.namelist() if original.read(n)!=candidate.read(n)]==[member]
    original_module=original.read(member)
    candidate_module=candidate.read(member)
with zipfile.ZipFile(io.BytesIO(original_module)) as original, zipfile.ZipFile(io.BytesIO(candidate_module)) as candidate:
    assert original.testzip() is None and candidate.testzip() is None
    assert original.namelist()==candidate.namelist()
    assert [n for n in original.namelist() if original.read(n)!=candidate.read(n)]==[runtime_member]
    runtime=original.read(runtime_member)
    patched=candidate.read(runtime_member)
assert sha(runtime)=='7abd01d50ac948b38a41a55cef5a461b018ed29e1856d8198aa5deeb6db29d18'
assert sha(patched)=='7d6bcf757430196b73eb44e217dd4d1e18c9b4873f910fdd008cf1fe4fd9c72a'
expected=bytearray(runtime)
assert expected[0xa329a4:0xa329a8]==struct.pack('<f',.05)
expected[0xa329a4:0xa329a8]=struct.pack('<f',.06)
assert bytes(expected)==patched
algorithms=['md5','sha1','sha256','sha512']
metadata_path=aar.with_suffix('.module')
for path in [aar,metadata_path]:
    for alg in algorithms:
        assert Path(str(path)+'.'+alg).read_text().strip()==hashlib.new(alg,path.read_bytes()).hexdigest()
metadata=json.loads(metadata_path.read_text())
original_metadata=json.loads(subprocess.check_output(['git','show',BASE+':'+metadata_path.relative_to(ROOT).as_posix()],cwd=ROOT))
expected_metadata=copy.deepcopy(original_metadata)
for variant in expected_metadata['variants']:
    for entry in variant.get('files',[]):
        if entry['name']==aar.name:
            entry.update(size=len(candidate_bytes),**{a:hashlib.new(a,candidate_bytes).hexdigest() for a in algorithms})
assert metadata==expected_metadata
assert b'\r' not in metadata_path.read_bytes()
result={'status':'passed','sdk_aar_sha256':sha(candidate_bytes),'nested_module_sha256':sha(candidate_module),'runtime_sha256':sha(patched),'only_aar_member_changed':member,'only_module_member_changed':runtime_member,'runtime_change_only':'float initializer50ms ->60ms at0xa329a4','outer_java_and_native_api_unchanged':True,'other_abis_unchanged':True,'zip_crc_verified':True,'aar_and_gradle_metadata_checksums_verified':algorithms,'maven_metadata_only_size_and_hashes_changed':True,'base_commit':BASE}
(Path(__file__).parent/'VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result,indent=2))
