#!/usr/bin/env python3
"""Validate source coverage, derived links/scores and ATIF (if Harbor installed)."""
import argparse
import collections
import gzip
import json
from pathlib import Path
import sys
from redact import ROOT, dump_json
from build_data import read_text, records, locate

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--harbor-site-packages',type=Path)
args=parser.parse_args()
if args.harbor_site_packages:sys.path.insert(0,str(args.harbor_site_packages))
from harbor.models.trajectories.trajectory import Trajectory

study=json.loads((ROOT/'docs/data/study.json').read_text());assert len(study['runs'])==33
out=[]
for r in study['runs']:
    trial=ROOT/'jobs'/r['id']/r['trial']
    original=json.loads((trial/'result.json').read_text())
    assert r['reward']==original['verifier_result']['rewards']['reward']
    native=json.loads(gzip.decompress((ROOT/'docs/harbor/snapshots'/f'{r["id"]}.json.gz').read_bytes()))
    trajectory=Trajectory.model_validate(native['/trajectory'])
    assert len(trajectory.steps)==r['steps']
    assert {s.extra['agent_id'] for s in trajectory.steps}=={'a'+str(i) for i in range(1,r['agents']+1)}
    expected=collections.Counter()
    for source in r['source_sessions']:
        assert (trial/source).is_file()
        assert (ROOT/'docs/archive'/r['id']/r['trial']/source).is_file()
        for _,e in records(trial/source):expected[e['type']]+=1
    assert dict(expected)==r['source_event_counts']
    expected_steps=sum(expected[k] for k in ['user.message','assistant.message','assistant.reasoning','session.error','model.call_failure'])
    # Missing assistant tool-call records produce explicit deterministic dispatch
    # steps; ATIF step count may exceed the semantic-message count by that amount.
    assert len(trajectory.steps)>=expected_steps
    projected=sum(1 for s in trajectory.steps if s.extra['event_type'] in {'user.message','assistant.message','assistant.reasoning','session.error','model.call_failure'})
    assert projected==expected_steps
    dispatches=expected['tool.execution_start']
    timeline=json.loads(gzip.decompress((ROOT/'docs/data/runs'/f'{r["id"]}.json.gz').read_bytes()))
    events=timeline['events'];assert len([e for e in events if e['k']!='submission'])==dispatches
    subs=[v for _,v in records(locate(trial/'agent/submissions.jsonl')) if v.get('status')=='done']
    assert len(subs)==r['submissions']==len(timeline['submissions'])
    assert [s['score'] for s in timeline['submissions']]==[s.get('score') for s in sorted(subs,key=lambda s:s['ts'])]
    assert all(1<=e['step']<=r['steps'] for e in events if 'step' in e)
    assert all(a[1]<b[1] for a,b in zip(r['curve'],r['curve'][1:]))
    assert (ROOT/'docs/harbor/jobs'/r['id']/'trials'/r['trial']/'index.html').is_file()
    out.append({'id':r['id'],'steps':r['steps'],'source_events':sum(expected.values()),'submissions':len(subs),'tool_dispatches':dispatches,'unpaired_tool_results':r['counts'].get('unpaired_tool_results',0)})
    print('Validated',r['id'],flush=True)
dump_json(ROOT/'metadata/data-validation.json',{'passed':True,'runs':out,'checks':'ATIF pydantic schema, sequential IDs and source-call references; exact semantic event coverage and source event type counts; all dispatches and completed submission scores; final rewards; archive download targets and deep-link pages.'})
print('All data validations passed:',len(out),'runs')
