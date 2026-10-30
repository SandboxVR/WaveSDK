"""Verify applied VRS artifacts and preservation of the previously patched SDK."""
import io
import json
import zipfile
import hashlib
from build import HERE,ROOT,BASE,AAR,MODULE,RUNTIME,ALGS,original,member,changed,sha,source_hashes

def main():
    report=json.loads((HERE/'VERIFICATION.json').read_text())
    assert report['base_commit']==BASE and report['applied']
    assert report['source_sha256']==source_hashes(),'Patch source differs from built artifact'
    for path,digest in report['published_sha256'].items(): assert sha((ROOT/path).read_bytes())==digest,path
    before=original(AAR); after=(ROOT/AAR).read_bytes()
    assert sha(after)==report['aar_sha256']
    assert changed(before,after)==sorted([MODULE,'classes.jar','jni/arm64-v8a/libssvr_wave_compositor.so','proguard.txt'])
    assert member(after,'proguard.txt')==(HERE/'extension/consumer-rules.pro').read_bytes().replace(b'\r\n',b'\n')
    bm=member(before,MODULE); am=member(after,MODULE)
    assert changed(bm,am)==[RUNTIME]
    assert sha(am)==report['module_sha256']
    assert sha(member(am,'classes.dex'))==report['preserved_module_dex_sha256']==sha(member(bm,'classes.dex'))
    runtime=member(am,RUNTIME)
    assert sha(runtime)==report['runtime_sha256']
    native=report['native_patch']
    baseline=member(bm,RUNTIME)
    restored=bytearray(runtime[:len(baseline)])
    for edit in native['changes']:
        offset=int(edit['file_offset'],16)
        assert runtime[offset:offset+edit['size']]==bytes.fromhex(edit['after_hex'])
        restored[offset:offset+edit['size']]=bytes.fromhex(edit['before_hex'])
    assert bytes(restored)==baseline and sha(baseline)==native['baseline_sha256']
    rx=int(native['rx_file_offset'],16)
    assert sha(runtime[rx:rx+native['rx_size']])==native['rx_sha256']
    # Prediction cap is data, independent of the constructor/draw VRS hooks.
    assert runtime[0xa329a4:0xa329a8]==member(bm,RUNTIME)[0xa329a4:0xa329a8]==bytes.fromhex('8fc2753d')
    with zipfile.ZipFile(io.BytesIO(member(before,'classes.jar'))) as old, zipfile.ZipFile(io.BytesIO(member(after,'classes.jar'))) as new:
        assert set(old.namelist()) <= set(new.namelist())
        for n in old.namelist(): assert old.read(n)==new.read(n),n
        additions=set(new.namelist())-set(old.namelist())
        assert additions and all(n.startswith('com/sandboxvr/wave/') and n.endswith('.class') for n in additions)
    meta=AAR.with_suffix('.module')
    expected=json.loads(original(meta))
    for variant in expected['variants']:
        for f in variant.get('files',[]):
            if f['name']==AAR.name: f.update(size=len(after),**{a:hashlib.new(a,after).hexdigest() for a in ALGS})
    assert json.loads((ROOT/meta).read_bytes())==expected
    for rel in (AAR,meta):
        for alg in ALGS: assert (ROOT/(str(rel)+'.'+alg)).read_text()==hashlib.new(alg,(ROOT/rel).read_bytes()).hexdigest()
    print('PASS: VRS artifact identity, extension allowlist, original Java lifecycle fixes, prediction cap, Maven metadata/checksums')

if __name__=='__main__': main()
