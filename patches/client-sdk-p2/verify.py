"""Verify final P2 artifacts, native/P1 preservation and Maven publication integrity."""
import copy
import hashlib
import io
import json
from pathlib import Path
import zipfile
from build import HERE, ROOT, P1, ARTIFACTS, GROUPS, MODULE, ALGS, original, sha, source_sha, read_zip, changed_members, source_snapshot

def main():
    report=json.loads((HERE/'VERIFICATION.json').read_text())
    assert report['applied'] and set(report['groups'])==set(GROUPS)
    assert report['source_sha256']==source_snapshot(report['groups'])
    for rel,digest in report['published_file_sha256'].items(): assert sha((ROOT/rel).read_bytes())==digest,rel
    for name,rel in ARTIFACTS.items():
        before,after=original(rel),(ROOT/rel).read_bytes()
        record=report['artifacts'][rel.as_posix()]
        assert sha(before)==record['before_sha256'] and sha(after)==record['after_sha256']
        assert len(after)==record['bytes']
        assert changed_members(before,after)==([MODULE] if name=='wvr_client' else ['classes.jar'])
        metarel=rel.with_suffix('.module')
        expected=copy.deepcopy(json.loads(original(metarel)))
        for variant in expected['variants']:
            for f in variant.get('files',[]):
                if f['name']==rel.name:
                    f.update(size=len(after),**{a:hashlib.new(a,after).hexdigest() for a in ALGS})
        assert json.loads((ROOT/metarel).read_bytes())==expected
        assert b'\r' not in (ROOT/metarel).read_bytes()
        for path in (ROOT/rel,ROOT/metarel):
            for alg in ALGS:
                assert Path(str(path)+'.'+alg).read_text().strip()==hashlib.new(alg,path.read_bytes()).hexdigest()
        if name!='wvr_client':
            group=next(g for g,(artifact,_) in GROUPS.items() if artifact==name)
            policies=json.loads((HERE/group/'graft-manifest.json').read_text())['classes']
            additions={p['type'][1:-1]+'.class' for p in policies if p['mode']=='add' and p.get('jar')}
            changes={p['type'][1:-1]+'.class' for p in policies if p['mode']!='add' and p.get('jar')}
            with zipfile.ZipFile(io.BytesIO(read_zip(before,'classes.jar'))) as old,zipfile.ZipFile(io.BytesIO(read_zip(after,'classes.jar'))) as new:
                assert set(new.namelist())==set(old.namelist())|additions
                assert {n for n in old.namelist() if old.read(n)!=new.read(n)}==changes
                assert new.testzip() is None
    old=read_zip(original(ARTIFACTS['wvr_client']),MODULE)
    new=read_zip((ROOT/ARTIFACTS['wvr_client']).read_bytes(),MODULE)
    assert changed_members(old,new)==['classes.dex']
    assert sha(read_zip(new,'classes.dex'))==report['patched_dex_sha256']
    assert sha(read_zip(new,'lib/arm64-v8a/libwvr_runtime.so'))=='7d6bcf757430196b73eb44e217dd4d1e18c9b4873f910fdd008cf1fe4fd9c72a'
    # Recorded device validation must describe these exact final binaries.
    smoke=json.loads((HERE/'ANDROID_SMOKE.json').read_text())
    assert smoke['status']=='passed' and smoke['module_sha256']==sha(new)
    assert smoke['dex_sha256']==report['patched_dex_sha256']
    for name in ('wvr_ime_client','wvr_permission_client'):
        assert smoke['optional_jar_sha256'][name+'.jar']==sha(read_zip((ROOT/ARTIFACTS[name]).read_bytes(),'classes.jar'))
    for rel,digest in smoke['test_source_sha256'].items(): assert source_sha(ROOT/rel)==digest,rel
    host=json.loads((HERE/'HOST_TESTS.json').read_text())
    assert host['status']=='passed' and host['source_sha256']==report['source_sha256']
    assert set(host['suites'])=={'notify','camera','optional'}
    assert all(s['status']=='passed' and s['assertions']>0 for s in host['suites'].values())
    for short,name in (('ime','wvr_ime_client'),('permission','wvr_permission_client')):
        assert host['optional_jar_sha256'][short]==sha(read_zip((ROOT/ARTIFACTS[name]).read_bytes(),'classes.jar'))
    for rel,digest in host['test_source_sha256'].items(): assert source_sha(ROOT/rel)==digest,rel
    # The standalone utility P1 fix and its publication metadata are unchanged.
    utils=Path('repo/com/htc/vr/wvr.common.utils/6.2.0/wvr.common.utils-6.2.0.aar')
    for rel in (utils,utils.with_suffix('.module')):
        assert (ROOT/rel).read_bytes()==original(rel)
        for alg in ALGS:
            sidecar=Path(str(rel)+'.'+alg)
            assert (ROOT/sidecar).read_bytes()==original(sidecar)
    print('PASS P2 publication: exact allowlisted changes, original P1 utility, all native libraries/cap60, module metadata and checksums')

if __name__=='__main__': main()
