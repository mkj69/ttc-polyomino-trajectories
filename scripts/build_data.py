#!/usr/bin/env python3
"""Derive labeled ATIF and overview timelines from sanitized Copilot events."""
import collections
from datetime import datetime, timezone
import gzip
import json
from pathlib import Path
import re
import statistics
from redact import ROOT, dump_json

def read_text(p):
    return gzip.open(p,'rt',encoding='utf-8').read() if p.suffix=='.gz' else p.read_text()

def records(p):
    opener=gzip.open if p.suffix=='.gz' else open
    with opener(p,'rt',encoding='utf-8') as f:
        for line_no,line in enumerate(f,1):
            try: yield line_no,json.loads(line)
            except json.JSONDecodeError: raise ValueError(f'Malformed sanitized JSONL: {p.relative_to(ROOT)}:{line_no}') from None

def stamp(s):
    return datetime.fromisoformat(s.replace('Z','+00:00')).replace(tzinfo=timezone.utc).timestamp() if s and not datetime.fromisoformat(s.replace('Z','+00:00')).tzinfo else datetime.fromisoformat(s.replace('Z','+00:00')).timestamp() if s else None

def locate(p):
    return p if p.exists() else p.with_name(p.name+'.gz')

def compressed_json(p,data):
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_bytes(gzip.compress(json.dumps(data,ensure_ascii=False,separators=(',',':')).encode(),compresslevel=6,mtime=0))

SHARED=re.compile(r'\b(?:findings|adoptions|disconfirmations|champion)\.log\b')
WRITE=re.compile(r'>>?|\b(?:tee|touch|truncate|write_text|write_bytes|writelines|rename)\b|\bsed\s+-i|\bopen\([^\n]*["\'][aw][+b]?["\']')

def build_run(job):
    trial=next(job.glob('polyomino__*'))
    result=json.loads((trial/'result.json').read_text())
    t0=stamp(result['agent_execution']['started_at'])
    duration=12 if job.name.startswith('poly12h') else 3
    model='Sonnet 4.6' if 'claude-sonnet' in job.name else 'gpt-6.1-sol'
    setup=re.search(r'-(solo|team3|iso3)(?:-|$)',job.name).group(1)
    rnd=int((re.search(r'-r([2-5])-',job.name) or [None,1])[1])
    steps=[]; timeline=[]; calls={}; counts=collections.Counter(); event_types=collections.Counter(); sources=[]
    # One event stream per session attempt. Preserve original coordinates and
    # completed semantic events; discard redundant streaming deltas in ATIF only.
    for p in sorted((trial/'agent').glob('*/copilot-attempt-*.jsonl*')):
        if p.name.endswith('.parts.json'): continue
        agent_match=re.search(r'-a([123])$',p.parent.name)
        agent='a'+agent_match[1] if agent_match else 'a1'
        source=str(p.relative_to(trial));sources.append(source)
        for line_no,e in records(p):
            ty=e.get('type','');event_types[ty]+=1
            d=e.get('data') or {};ts=e.get('timestamp');time=stamp(ts)
            if time is None: continue
            extra={'agent_id':agent,'source_file':source,'source_line':line_no,'event_type':ty,'event_id':e.get('id')}
            label=f'[{agent} · {p.parent.name}/{p.name.removesuffix(".gz")}]'
            if ty in {'user.message','assistant.message','assistant.reasoning'}:
                content=d.get('content') or ''
                if not isinstance(content,str): content=json.dumps(content,ensure_ascii=False)
                step={'source':'user' if ty=='user.message' else 'agent','timestamp':ts,'message':label+'\n\n'+content,'extra':extra}
                if ty=='assistant.reasoning':
                    step['message']=label+' · completed reasoning';step['reasoning_content']=content
                if ty=='assistant.message' and d.get('reasoningText'):
                    step['reasoning_content']=d['reasoningText']
                if ty=='assistant.message':
                    for request in d.get('toolRequests') or []:
                        cid=request.get('toolCallId')
                        args=request.get('arguments') or {}
                        if not isinstance(args,dict):args={'patch':args}
                        call={'tool_call_id':f'{source}:{cid}','function_name':request.get('name','tool'),'arguments':args}
                        step.setdefault('tool_calls',[]).append(call)
                        calls[(source,cid)]=(step,call)
                steps.append((time,len(steps),step))
            elif ty=='tool.execution_start':
                cid=d.get('toolCallId');pair=calls.get((source,cid))
                args=d.get('arguments') or {};name=d.get('toolName','tool')
                if not pair:
                    step={'source':'agent','timestamp':ts,'message':label+' · tool dispatch','llm_call_count':0,'extra':extra}
                    call={'tool_call_id':f'{source}:{cid}','function_name':name,'arguments':args if isinstance(args,dict) else {'patch':args}}
                    step['tool_calls']=[call];calls[(source,cid)]=(step,call);steps.append((time,len(steps),step))
                else: step=pair[0]
                text=args.get('command','') if isinstance(args,dict) and 'command' in args else json.dumps(args,ensure_ascii=False)
                shared=sorted(set(SHARED.findall(text)))
                kind='tool'
                if shared:kind='shared_write' if name=='apply_patch' or WRITE.search(text) else 'shared_read'
                timeline.append({'t':round((time-t0)/60,4),'a':agent,'k':kind,'name':name,'label':str(text)[:240],'logs':shared,'_step':step})
                counts[kind]+=1
            elif ty=='tool.execution_complete':
                pair=calls.get((source,d.get('toolCallId')))
                if pair:
                    step,call=pair
                    value=d.get('result') or d.get('error') or {}
                    content=value.get('content') if isinstance(value,dict) else str(value)
                    if not content and isinstance(value,dict):content=value.get('detailedContent') or json.dumps(value,ensure_ascii=False)
                    if not isinstance(content,str):content=json.dumps(content,ensure_ascii=False)
                    if isinstance(value,dict) and value.get('detailedContent') and value['detailedContent']!=content:
                        detail=value['detailedContent']
                        content+='\n\n[Detailed tool result]\n'+(detail if isinstance(detail,str) else json.dumps(detail,ensure_ascii=False))
                    step.setdefault('observation',{'results':[]})['results'].append({'source_call_id':call['tool_call_id'],'content':content})
                else:counts['unpaired_tool_results']+=1
            elif ty in {'session.error','model.call_failure'}:
                step={'source':'system','timestamp':ts,'message':label+' · '+ty+'\n\n'+json.dumps(d,ensure_ascii=False),'extra':extra}
                steps.append((time,len(steps),step));counts['errors']+=1
    steps.sort(key=lambda x:(x[0],x[1]))
    assert steps, f'No completed semantic events found for {job.name}'
    for i,(_,_,s) in enumerate(steps,1):s['step_id']=i
    for e in timeline:e['step']=e.pop('_step')['step_id']
    submissions=[];starts={}
    for _,r in records(locate(trial/'agent/submissions.jsonl')):
        sid=r.get('submission_uuid')
        if r.get('status')=='started':starts[sid]=r
        if r.get('status')=='done':
            start=starts.get(sid,{})
            s={'t':round((stamp(r['ts'])-t0)/60,4),'a':r.get('agent_id') or start.get('agent_id') or 'a1','k':'submission','score':r.get('score'),'uuid':sid,'path':r.get('solution_path') or start.get('solution_path'),'code_chars':r.get('code_chars') or start.get('code_chars'),'timestamp':r['ts']}
            submissions.append(s);timeline.append(s)
    submissions.sort(key=lambda x:x['t']); timeline.sort(key=lambda x:x['t'])
    best=0.;curve=[]
    for s in submissions:
        if s['score'] is not None and s['score']>best:
            best=s['score'];curve.append([s['t'],best,s['a']])
    reward=((result.get('verifier_result') or {}).get('rewards') or {}).get('reward')
    info={'id':job.name,'trial':trial.name,'model':model,'setup':setup,'round':rnd,'hours':duration,'reward':reward,'best_submission':best,'submissions':len(submissions),'agents':1 if setup=='solo' else 3,'steps':len(steps),'started_at':result['agent_execution']['started_at'],'finished_at':result.get('finished_at'),'counts':dict(counts),'source_event_counts':dict(event_types),'source_sessions':sources,'curve':curve,'minutes_observed':round(max([s['t'] for s in timeline]+[duration*60]),3)}
    trajectory={'schema_version':'ATIF-v1.8','session_id':job.name,'agent':{'name':'Copilot CLI · chronological multi-session projection','version':'1.0.70','model_name':model},'steps':[s for _,_,s in steps], 'notes':'Derived from sanitized Copilot events; merged by event timestamp with explicit agent/session labels. Completed messages, reasoning, tools, results and errors retained; streaming deltas and telemetry omitted. This is a viewing projection, not the original agent transcript. Tool results are attached to their call; their later completion time remains in original events. No communication or causal attribution is implied.','extra':{'source_sessions':sources,'generator':'scripts/build_data.py','agent_id_in_step_extra':True}}
    # Harbor requires this uncompressed filename; it is excluded from archive
    # duplication and separately compressed by the snapshot exporter.
    dump_json(trial/'agent/trajectory.json',trajectory)
    compressed_json(ROOT/'docs/data/runs'/f'{job.name}.json.gz',{'run':info,'events':timeline,'submissions':submissions})
    print(f'{job.name}: {len(steps)} steps, {len(submissions)} submissions',flush=True)
    return info

def main():
    runs=[build_run(p) for p in sorted((ROOT/'jobs').iterdir()) if p.is_dir()]
    summary=[]
    for model in ['Sonnet 4.6','gpt-6.1-sol']:
        for setup in ['solo','team3','iso3']:
            selected=[r for r in runs if r['hours']==3 and r['model']==model and r['setup']==setup]
            scores=[r['reward'] for r in selected]
            summary.append({'model':model,'setup':setup,'n':len(scores),'mean':statistics.mean(scores),'std_sample':statistics.stdev(scores)})
    dump_json(ROOT/'docs/data/study.json',{'runs':runs,'summary':summary,'provided_context':{'Sonnet 4.6':{'solo':[.767,.051],'team3':[.812,.048],'iso3':[.812,.028],'best_at_3_solo':.819},'gpt-6.1-sol':{'solo':[.983,.005],'team3':[.983,.008],'iso3':[.982,.008],'best_at_3_solo':.986}},'method':'Final reward from trial/result.json. Standard deviation uses ddof=1. Best-submission curves use completed submissions and may differ from final graded reward. Times measured from agent_execution.started_at; naive source timestamps treated as UTC. Shared-log reads/writes are command-text classifications, not proof of knowledge transfer.'})
    dump_json(ROOT/'metadata/derived-manifest.json',{'runs':len(runs),'steps':sum(r['steps'] for r in runs),'submissions':sum(r['submissions'] for r in runs),'summary':summary})

if __name__=='__main__':main()
