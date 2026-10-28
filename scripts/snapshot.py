#!/usr/bin/env python3
"""Dump Harbor's read-only HTTP API and package its installed static viewer.

Run a loopback Harbor viewer against sanitized jobs first. Never connect this
script to a server containing unredacted data. No API write endpoint is invoked.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import urllib.request
import urllib.error
from redact import ROOT, dump_json, Redactor
from build_data import compressed_json, read_text

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url',default='http://127.0.0.1:8765')
    parser.add_argument('--static',type=Path,required=True)
    args=parser.parse_args()
    assert args.url.startswith(('http://127.0.0.1:','http://localhost:'))
    docs=ROOT/'docs';dest=docs/'harbor';dest.mkdir(parents=True,exist_ok=True)
    for p in args.static.rglob('*'):
        if not p.is_file():continue
        out=dest/p.relative_to(args.static);out.parent.mkdir(parents=True,exist_ok=True)
        if p.suffix in {'.js','.css','.html','.svg'}:
            s=p.read_text().replace('/assets/','/ttc-polyomino-trajectories/harbor/assets/').replace('/fonts/','/ttc-polyomino-trajectories/harbor/fonts/')
            if p.name=='index.html':
                s=s.replace('href="/favicon','href="/ttc-polyomino-trajectories/harbor/favicon')
                s=s.replace('"basename":"/"','"basename":"/ttc-polyomino-trajectories/harbor"')
                s=s.replace('<head>','<head><title>Harbor · TTC trajectories</title><script src="/ttc-polyomino-trajectories/harbor/snapshot-bootstrap.js"></script><link rel="stylesheet" href="/ttc-polyomino-trajectories/harbor/snapshot.css"/>')
                # Gate the native app behind service-worker activation.
                s=s.replace('async="">import ','async="">await window.harborReady; import ')
            out.write_text(s)
        else:shutil.copyfile(p,out)
    calls=0
    def get(path,optional=False):
        nonlocal calls
        calls+=1
        try:
            with urllib.request.urlopen(args.url+path,timeout=90) as r:
                value=json.load(r)
                return json.loads(Redactor()(json.dumps(value)))
        except urllib.error.HTTPError as e:
            if optional and e.code==404:return None
            raise RuntimeError(f'Snapshot GET failed ({e.code}): endpoint index {calls}') from None
    data={'api':{},'lists':{},'trials':{},'files':{},'version':1,'base':'/ttc-polyomino-trajectories/harbor/'}
    for endpoint in ['/api/config','/api/jobs/filters']:
        data['api'][endpoint]=get(endpoint)
    # A public static snapshot has no local path, login, write or editor backend.
    config=data['api']['/api/config'];config['folder']='jobs';config['jobs_dir']='jobs';config['read_only']=True
    data['api']['/api/auth/status']={'authenticated':False,'user':None,'enabled':False}
    alljobs=get('/api/jobs?page_size=100')['items'];data['lists']['/api/jobs']=alljobs
    study=json.loads((docs/'data/study.json').read_text())
    index={r['id']:r for r in study['runs']}
    sizes=[]
    for job in alljobs:
        name=job['name'];run=index[name];trial=run['trial'];base=f'/api/jobs/{name}'
        for suffix in ['', '/config','/tasks/filters','/chart-trials','/analysis','/upload']:
            data['api'][base+suffix]=get(base+suffix,optional=True)
        for suffix in ['/trials','/tasks']:
            response=get(base+suffix+'?page_size=100')
            data['lists'][base+suffix]=response['items']
        root=base+'/trials/'+trial
        snap={}
        for suffix in ['', '/trajectory','/navigation','/verifier-output','/agent-logs','/recording','/artifacts','/interaction','/files']:
            snap[suffix]=get(root+suffix,optional=True)
        trialdir=ROOT/'jobs'/name/trial
        # File bytes are already sanitized. Hardlinks avoid duplicate working-disk
        # blocks; Git also deduplicates identical blob contents. Pages uses docs/.
        for p in sorted(trialdir.rglob('*')):
            if not p.is_file() or p.name=='trajectory.json':continue
            rel=p.relative_to(ROOT/'jobs');out=docs/'archive'/rel
            out.parent.mkdir(parents=True,exist_ok=True)
            if out.exists():out.unlink()
            os.link(p,out)
            relative=str(p.relative_to(trialdir));plain=relative.removesuffix('.gz')
            # Preview cap matches Harbor's own 1 MiB text limit. Download remains
            # lossless and independently accessible via archive links.
            raw_size=len(gzip.open(p,'rb').read()) if p.suffix=='.gz' else p.stat().st_size
            desc={'url':'../archive/'+str(rel),'gzip':p.suffix=='.gz','bytes':raw_size}
            data['files'][root+'/files/'+relative]=desc
            data['files'][root+'/files/'+plain]=desc
        payload=dest/'snapshots'/f'{name}.json.gz';compressed_json(payload,snap)
        assert payload.stat().st_size<49_000_000
        sizes.append(payload.stat().st_size)
        data['trials'][root]='snapshots/'+payload.name
        generated=trialdir/'agent/trajectory.json'
        generated.with_suffix('.json.gz').write_bytes(gzip.compress(generated.read_bytes(),compresslevel=6,mtime=0))
        generated.unlink()
        # Pre-materialize routes, so GitHub Pages fresh deep links return 200 and
        # work even before a browser has installed the service worker.
        for route in [f'jobs/{name}',f'jobs/{name}/trials/{trial}']:
            p=dest/route/'index.html';p.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(dest/'index.html',p)
        print(f'Snapshotted {name}',flush=True)
    compressed_json(dest/'snapshot.json.gz',data)
    # 404 fallback handles React routes not explicitly materialized above.
    (docs/'404.html').write_text('<!doctype html><meta charset="utf-8"><title>Open TTC archive</title><p>Opening the trajectory viewer…</p><script>const base="/ttc-polyomino-trajectories/harbor/";const p=location.pathname;if(p.startsWith(base)){sessionStorage.setItem("harbor-return",p+location.search+location.hash);location.replace(base)}else{location.replace("/ttc-polyomino-trajectories/")}</script>')
    (docs/'.nojekyll').touch()
    dump_json(ROOT/'metadata/snapshot-manifest.json',{'harbor_version':'0.24.0','http_get_count':calls,'runs':len(alljobs),'trial_snapshot_bytes':sum(sizes),'largest_trial_snapshot_bytes':max(sizes),'indexed_file_endpoints':len(data['files']),'mount':'/ttc-polyomino-trajectories/harbor/','source_static':'Installed harbor/viewer/static','projection':'Completed Copilot semantic events in generated ATIF; no native trajectory was present.'})
    print('Snapshot complete:',calls,'GETs')

if __name__=='__main__':main()
