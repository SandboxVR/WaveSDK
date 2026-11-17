"""Execute the exact donor JAR methods, selectively grafted into Android host fixtures."""
import json
import os
import re
import subprocess
from build import HERE, ROOT, BUILD, jar_classes, json_bytes, run, sha, source_snapshot, write
from verify import verify_candidate

def main():
    candidate = verify_candidate()
    tool = candidate['toolchain']
    test_sources = sorted((HERE / 'tests').rglob('*.java'))
    extra_sources = [HERE / 'compile-stubs/com/htc/vr/R.java', HERE / 'compile-stubs/com/htc/vr/sdk/overlay/VROverlayError.java']
    hashes = {p.relative_to(ROOT).as_posix(): sha(p.read_bytes().replace(b'\r\n', b'\n'))
              for p in [HERE / 'run_tests.py', HERE / 'verify.py', *test_sources, *extra_sources]}
    out = BUILD / 'test-classes'
    out.mkdir(exist_ok=True)
    for stale in out.rglob('*.class'): stale.unlink()
    run([tool['javac'], '--release', '8', '-d', out, *test_sources, *extra_sources])
    fixture = BUILD / 'test-fixture.jar'
    patched = BUILD / 'test-patched.jar'
    jar_classes(out, fixture)
    assert sha((BUILD / 'donor.jar').read_bytes()) == candidate['donor_jar_sha256']
    run([tool['java'], '-cp', tool['tool_cp'], 'Graft', 'jar', HERE / 'graft-manifest.json', fixture, patched, BUILD / 'donor.jar'])
    result = subprocess.run([tool['java'], '-ea', '-cp', str(patched), 'GazeRegression'], cwd=ROOT,
                            capture_output=True, text=True, timeout=60)
    print(result.stdout + result.stderr, end='')
    assert result.returncode == 0, 'Gaze regression failed'
    counts = re.findall(r'(\d+) assertions', result.stdout)
    assert len(counts) == 1
    report = {'status': 'passed', 'assertions': int(counts[0]), 'source_sha256': source_snapshot(),
              'test_source_sha256': hashes, 'candidate_sha256': candidate['after_sha256'],
              'donor_jar_sha256': candidate['donor_jar_sha256'],
              'scope': 'Exact donor JAR methods used for DEX, selectively grafted into host Android/render stand-ins; no headset runtime validation',
              'output': result.stdout}
    assert report['source_sha256'] == candidate['source_sha256']
    for rel, digest in hashes.items(): assert sha((ROOT / rel).read_bytes().replace(b'\r\n', b'\n')) == digest
    write(HERE / 'HOST_TESTS.json', json_bytes(report))

if __name__ == '__main__': main()
