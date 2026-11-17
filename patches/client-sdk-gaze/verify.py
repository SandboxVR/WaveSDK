"""Verify exact gaze candidate, unchanged nested native members and Maven metadata."""
import hashlib
import json
from build import HERE, ROOT, BUILD, CLIENT, MODULE, ALGS, original, source_snapshot, sha, read_zip, changed_members

def verify_candidate(require_tests=False):
    report = json.loads((BUILD / 'candidate-verification.json').read_text())
    assert report['source_sha256'] == source_snapshot(), 'Rebuild after source changes'
    before = original(CLIENT)
    after = (BUILD / 'candidate' / CLIENT).read_bytes()
    assert sha(before) == report['before_sha256'] and sha(after) == report['after_sha256']
    assert changed_members(before, after) == [MODULE]
    assert changed_members(read_zip(before, MODULE), read_zip(after, MODULE)) == ['classes.dex']
    assert sha(read_zip(read_zip(after, MODULE), 'classes.dex')) == report['patched_dex_sha256']
    for rel, digest in report['published_file_sha256'].items():
        assert sha((BUILD / 'candidate' / rel).read_bytes()) == digest, rel
    for rel in (CLIENT, CLIENT.with_suffix('.module')):
        data = (BUILD / 'candidate' / rel).read_bytes()
        for alg in ALGS:
            assert (BUILD / 'candidate' / (str(rel) + '.' + alg)).read_text() == hashlib.new(alg, data).hexdigest()
    metadata = json.loads((BUILD / 'candidate' / CLIENT.with_suffix('.module')).read_text())
    for variant in metadata['variants']:
        for entry in variant.get('files', []):
            if entry['name'] == CLIENT.name:
                assert entry['size'] == len(after)
                for alg in ALGS: assert entry[alg] == hashlib.new(alg, after).hexdigest()
    if require_tests:
        tests = json.loads((HERE / 'HOST_TESTS.json').read_text())
        assert tests['status'] == 'passed'
        assert tests['source_sha256'] == source_snapshot()
        assert tests['candidate_sha256'] == sha(after)
        assert tests['donor_jar_sha256'] == report['donor_jar_sha256']
        assert sha((BUILD / 'donor.jar').read_bytes()) == report['donor_jar_sha256']
        for rel, digest in tests['test_source_sha256'].items():
            assert sha((ROOT / rel).read_bytes().replace(b'\r\n', b'\n')) == digest
    return report

if __name__ == '__main__':
    report = verify_candidate(require_tests=True)
    print('Verified candidate, host regressions, unchanged native bytes and Maven checksums:', report['after_sha256'])
