"""Run the P2 host suites and record the exact tested sources and optional JARs."""
import json
import re
import subprocess
from pathlib import Path
from build import HERE, ROOT, BUILD, GROUPS, source_snapshot, source_sha, sha


def main():
    candidate = json.loads((BUILD/'candidate-verification.json').read_text())
    inputs = source_snapshot(list(GROUPS))
    assert set(candidate['groups']) == set(GROUPS)
    assert candidate['source_sha256'] == inputs, 'Rebuild candidate before testing'
    test_files = {Path(__file__)}
    for area in ('notify', 'camera', 'optional'):
        test_files.update((HERE/area/'tests').rglob('*.java'))
        test_files.update((HERE/area).glob('*.ps1'))
    test_hashes = {p.relative_to(ROOT).as_posix():source_sha(p) for p in sorted(test_files)}
    optional = {name:BUILD/'optional'/name/'patched.jar' for name in ('ime','permission')}
    jars = {name:sha(path.read_bytes()) for name,path in optional.items()}
    results = {}
    for area in ('notify','camera','optional'):
        command = ['powershell.exe','-NoProfile','-File',str(HERE/area/'run-tests.ps1')]
        if area == 'optional':
            command += ['-PatchedImeJar',str(optional['ime']),'-PatchedPermissionJar',str(optional['permission']),
                        '-UtilsJar',str(BUILD/'original/utils.jar')]
        run = subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=120)
        output = run.stdout + run.stderr
        print(output,end='')
        assert run.returncode == 0, f'{area} suite failed'
        counts = re.findall(r'(\d+)[^\r\n]*assertions',output)
        assert len(counts) == 1, f'Missing/ambiguous {area} assertion count'
        results[area] = {'status':'passed','assertions':int(counts[0]),'output':output}
    assert inputs == source_snapshot(list(GROUPS)), 'Sources changed during tests'
    for rel,digest in test_hashes.items(): assert source_sha(ROOT/rel) == digest
    for name,path in optional.items(): assert sha(path.read_bytes()) == jars[name]
    report = {'status':'passed','source_sha256':inputs,'test_source_sha256':test_hashes,
              'optional_jar_sha256':jars,'suites':results,
              'scope':'notify/camera donors with host Android/native stand-ins; optional suites execute actual grafted JARs with Android/service stand-ins'}
    (HERE/'HOST_TESTS.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__ == '__main__': main()
