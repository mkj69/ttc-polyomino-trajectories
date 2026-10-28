#!/usr/bin/env python3
"""Fail closed on recognizable credentials, archived authorities and size limits.

Only counts and paths are emitted. Matched text is never printed. Source-env
secrets, if present, are compared in memory. This scanner never reads key files.
"""
import argparse
import collections
import gzip
import hashlib
import json
from pathlib import Path
import re
import sys
from redact import ROOT, URL, TOKEN, BEARER, QUOTED_SECRET, SHELL_SECRET, PRIVATE_HOST, DNS, known_values, dump_json

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT)
    args=parser.parse_args();root=args.root.resolve()
    secrets,hosts=known_values();counts=collections.Counter();failures=[];total=0;largest=0;files=0;jsonl_rows=0;seen={}
    # Do not report literal fixture values; fixtures are temporary and not tracked.
    for p in sorted(root.rglob('*')):
        if '.git' in p.relative_to(root).parts or '__pycache__' in p.parts or not p.is_file():continue
        rel=str(p.relative_to(root));size=p.stat().st_size;total+=size;largest=max(largest,size);files+=1
        if size>=50_000_000:failures.append({'path':rel,'rule':'file_size_50MB'})
        if p.is_symlink():failures.append({'path':rel,'rule':'symbolic_link'});continue
        raw=p.read_bytes()
        if raw.startswith(b'\x7fELF'):failures.append({'path':rel,'rule':'ELF_binary'});continue
        archived=rel.startswith(('jobs/','docs/archive/','docs/data/','docs/harbor/snapshots/')) or rel=='docs/harbor/snapshot.json.gz'
        cache_key=(hashlib.sha256(raw).digest(),archived,p.suffix)
        if cache_key in seen:
            failures.extend({'path':rel,'rule':rule} for rule in seen[cache_key])
            continue
        start_failures=len(failures)
        if p.suffix=='.gz':
            try:raw=gzip.decompress(raw)
            except (OSError,EOFError):failures.append({'path':rel,'rule':'invalid_gzip'});continue
        try:s=raw.decode('utf-8')
        except UnicodeError:
            # Only vendored font/favicon binaries are permitted.
            if not rel.startswith('docs/harbor/') or p.suffix not in {'.ttf','.ico','.woff','.woff2'}:failures.append({'path':rel,'rule':'unexpected_nontext'})
            continue
        checks={'exact_env_secret':any(v in s for v in secrets)}
        # Source-code patterns in scripts are not archived credentials. Inspect
        # actual credential assignments in data only, with placeholder exclusions.
        if archived:
            checks['known_env_gateway']=any(v in s for v in hosts)
        for line in s.splitlines():
            low=line.lower()
            if any(k in low for k in ('sk-','ghp_','gho_','ghu_','ghs_','ghr_','github_pat_','akia','eyj')) and TOKEN.search(line):checks['credential_token']=True
            if 'bearer' in low and BEARER.search(line):checks['bearer_value']=True
            if archived:
                if any(k in low for k in ('http','wss','grpc','ftp')) and URL.search(line):checks['unredacted_url_authority']=True
                if any(k in low for k in ('.internal','.local','.lan')) and PRIVATE_HOST.search(line):checks['private_hostname']=True
                if any(k in low for k in ('.com','.org','.net','.edu','.io','.ai','.dev','.cloud')) and DNS.search(line):checks['bare_dns_candidate']=True
                if any(k in low for k in ('key','token','password','authorization','secret')):
                    if any(m.group(2).rstrip('\\') not in {'','<redacted>'} for m in QUOTED_SECRET.finditer(line)):checks['quoted_credential']=True
                    if any(m.group(2)!='<redacted>' for m in SHELL_SECRET.finditer(line)):checks['shell_credential']=True
        for rule,bad in checks.items():
            if bad:failures.append({'path':rel,'rule':rule});counts[rule]+=int(bad)
        # Syntax integrity matters: redaction must not damage structured data.
        name=p.name.removesuffix('.gz')
        if name.endswith(('.json','.jsonl','.otel')):
            try:
                if name.endswith(('.jsonl','.otel')):
                    for line in s.splitlines():
                        if line.strip():json.loads(line);jsonl_rows+=1
                else:json.loads(s)
            except (json.JSONDecodeError,ValueError):failures.append({'path':rel,'rule':'invalid_json'})
        seen[cache_key]=[f['rule'] for f in failures[start_failures:]]
    if total>=1_500_000_000:failures.append({'path':'.','rule':'repository_size_1.5GB'})
    result={'passed':not failures,'scanned_files':files,'unique_payloads_checked':len(seen),'stored_bytes_excluding_git':total,'largest_file_bytes':largest,'parsed_jsonl_rows_unique_payloads':jsonl_rows,'failed_rule_counts':dict(collections.Counter(x['rule'] for x in failures)),'failures':failures,'limitations':'Pattern and process-env scan, including decompressed data; identical bytes with the same archive policy are scanned once. No credential files accessed. Not a proof that arbitrary unrecognized secrets or encoded credentials do not exist.'}
    print(json.dumps({k:v for k,v in result.items() if k not in {'failures','limitations'}}))
    for failure in failures[:25]:print('FAIL',failure['rule'],failure['path'])
    if root==ROOT:dump_json(ROOT/'metadata/scan-result.json',result)
    return 1 if failures else 0

if __name__=='__main__':sys.exit(main())
