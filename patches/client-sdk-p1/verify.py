"""Independently verify the published AARs, constrained member changes and Maven hashes."""
import copy
import json
import hashlib
import io
import zipfile
from build import HERE, ROOT, CLIENT, UTILS, MODULE, ALGS, original, sha, read_zip, changed_members, source_snapshot

def main():
    report=json.loads((HERE/'VERIFICATION.json').read_text())
    assert report['applied']
    assert report['source_sha256']==source_snapshot(['dispatch','compositor'])
    for rel in (CLIENT,UTILS):
        before,after=original(rel),(ROOT/rel).read_bytes()
        record=report['artifacts'][rel.as_posix()]
        assert sha(before)==record['before_sha256']
        assert sha(after)==record['after_sha256']
        assert len(after)==record['bytes']
        assert changed_members(before,after)==([MODULE] if rel==CLIENT else ['classes.jar'])
        meta_rel=rel.with_suffix('.module')
        expected=copy.deepcopy(json.loads(original(meta_rel)))
        for variant in expected['variants']:
            for f in variant.get('files',[]):
                if f['name']==rel.name:
                    f.update(size=len(after),**{a:hashlib.new(a,after).hexdigest() for a in ALGS})
        assert json.loads((ROOT/meta_rel).read_bytes())==expected
        assert b'\r' not in (ROOT/meta_rel).read_bytes()
        for p in (ROOT/rel,ROOT/meta_rel):
            for a in ALGS:
                assert p.with_name(p.name+'.'+a).read_text().strip()==hashlib.new(a,p.read_bytes()).hexdigest()
    old_module=read_zip(original(CLIENT),MODULE)
    new_module=read_zip((ROOT/CLIENT).read_bytes(),MODULE)
    assert changed_members(old_module,new_module)==['classes.dex']
    dex=read_zip(new_module,'classes.dex')
    assert sha(dex)==report['patched_dex_sha256']
    assert sha(read_zip(new_module,'lib/arm64-v8a/libwvr_runtime.so'))=='7d6bcf757430196b73eb44e217dd4d1e18c9b4873f910fdd008cf1fe4fd9c72a'
    old_jar=read_zip(original(UTILS),'classes.jar')
    new_jar=read_zip((ROOT/UTILS).read_bytes(),'classes.jar')
    with zipfile.ZipFile(io.BytesIO(old_jar)) as old,zipfile.ZipFile(io.BytesIO(new_jar)) as new:
        prefix='com/htc/vr/sdk/util/CustomizeHandler'
        assert set(new.namelist())-set(old.namelist())=={prefix+'$ShutdownRunnable.class'}
        assert set(old.namelist())<=set(new.namelist())
        assert {n for n in old.namelist() if old.read(n)!=new.read(n)}=={prefix+'.class',prefix+'$SyncRunnable.class'}
        assert new.testzip() is None
    smoke=json.loads((HERE/'ANDROID_SMOKE.json').read_text())
    assert smoke['status']=='passed' and smoke['dex_sha256']==sha(dex)
    print('PASS published SDK: constrained archive changes, preserved native cap60, Maven metadata and all checksums, matching headset ART smoke')

if __name__=='__main__': main()
