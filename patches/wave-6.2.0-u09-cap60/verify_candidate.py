"""Compare untouched code loaded from original and patched ELF files."""
import argparse
import json
from pathlib import Path
from replay_prediction import Replay, NOW_NS

parser = argparse.ArgumentParser()
parser.add_argument('--original', type=Path, required=True)
parser.add_argument('--candidate', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
original = Replay(args.original)
candidate = Replay(args.candidate, original_reference=args.original)
results = []
max_model_error_ns = 0
for hz in (75,90):
    for tws in (False,True):
        unchanged, restored, missing = 0,0,0
        for index in range(1000):
            old = original.run(hz,index/1000,tws,cap_ms=None)
            new = candidate.run(hz,index/1000,tws,cap_ms=None)
            if old['timestamp_ns']:
                assert old == new
                unchanged += 1
            else:
                assert new['timestamp_ns'] and not new['warnings']
                restored += 1
            missing += not new['timestamp_ns']
            expected_ns = NOW_NS + round(((4 if tws else 3)-index/1000)/hz*1e9)
            error_ns = abs(new['timestamp_ns']-expected_ns)
            assert error_ns <= 20, (hz,tws,index,error_ns)
            max_model_error_ns = max(max_model_error_ns,error_ns)
        results.append(dict(hz=hz,tws=tws,unchanged_valid=unchanged,restored=restored,candidate_missing=missing))
for milliseconds in (60,70,100):
    rejected = candidate.run(75,.5,False,cap_ms=None,override_ms=milliseconds)
    assert rejected['timestamp_ns'] == 0 and rejected['warnings'] == 1
output = dict(status='passed',original_sha256=original.sha,candidate_sha256=candidate.sha,
              cap_source='ELF file initializer; no in-memory cap overrides in these tests',
              expected_change_only_verified=True,results=results,
              max_model_error_ns=max_model_error_ns,asserted_model_error_bound_ns=20,
              refresh_control='Original ELF flag1 retained; virtual GetRefreshRate returns the chosen test frequency',
              file_candidate_out_of_range_rejection='60/70/100ms rejected as expected',
              limitations='OS/timing callbacks and object state are constructed as documented in replay_prediction.py; not a headset test.')
args.output.write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
print(json.dumps(output,indent=2))
