#!/usr/bin/env python3
"""Public export. Never print matched values or read credential files.

All network authorities in archived text are anonymized, a conservative superset
of gateway redaction. Source files and symbolic links are never modified.
"""
import argparse
import collections
import gzip
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
RUN = re.compile(r'^poly(?:3h-(?:claude-sonnet-46|gpt-61-sol)-(?:solo|team3|iso3)(?:-r[2-5])?-.+|12h-claude-sonnet-46-(?:solo-1024|team3-1024|iso3-rerun))$')
URL = re.compile(r'(?i)(?:https?|wss?|grpc|ftp)\s*:(?:\\?/){2}([^\s/\\"\'<>]+)')
IP = re.compile(r'(?<![\w.])(?:\d{1,3}\.){3}\d{1,3}(?::\d{2,5})?(?![\w.])')
TOKEN = re.compile(r'(?i)\b(?:sk-[a-z0-9_-]{8,}|gh[pousr]_[a-z0-9_]{15,}|github_pat_[a-z0-9_]{15,}|AKIA[A-Z0-9]{16}|eyJ[a-z0-9_-]{8,}\.[a-z0-9_-]{8,}\.[a-z0-9_-]{8,})\b')
BEARER = re.compile(r'(?i)(\bBearer\s+)([a-z0-9_.~+/=-]{8,})')
# Match both JSON and JSON nested inside log strings (escaped quotes).
QUOTED_SECRET = re.compile(r'(?i)((?:api[_-]?key|access[_-]?token|auth[_-]?token|client[_-]?secret|password|authorization)[\\]*["\']?\s*[:=]\s*[\\]*["\'])([^"\'\r\n]*)(["\'])')
SHELL_SECRET = re.compile(r'(?i)((?:API_KEY|ACCESS_TOKEN|AUTH_TOKEN|CLIENT_SECRET|PASSWORD)\b\s*=\s*)([^\s;"\'\\]+)')
OPAQUE = re.compile(r'("reasoningOpaque"\s*:\s*")([^"\n]*)(")')
PRIVATE_HOST = re.compile(r'(?i)\b[a-z0-9][a-z0-9.-]*\.(?:internal|local|lan)\b')
DNS = re.compile(r'(?i)\b(?:[a-z0-9-]+\.)+(?:com|org|net|edu|io|ai|dev|cloud|internal|local|lan)\b')

def known_values():
    """Process env only: values never leave memory; no key files are accessed."""
    secrets, hosts = set(), set()
    for k, v in os.environ.items():
        if len(v) >= 8 and re.search(r'(?:API_KEY|ACCESS_TOKEN|AUTH_TOKEN|CLIENT_SECRET|PASSWORD)$', k):
            secrets.add(v)
        if re.search(r'(?:BASE_URL|ENDPOINT)$', k):
            hosts.update(m.group(1) for m in URL.finditer(v))
    return secrets, hosts

def text_bytes(p):
    if p.is_symlink():
        return None
    try:
        opener=gzip.open if p.suffix=='.gz' else open
        with opener(p,'rb') as f:head=f.read(8192)
        if head.startswith(b'\x7fELF') or b'\x00' in head or p.suffix in {'.pyc', '.pickle', '.pkl'}:
            return None
        data = gzip.open(p, 'rb').read() if p.suffix == '.gz' else p.read_bytes()
        return data.decode('utf-8')
    except (UnicodeError, OSError, EOFError):
        return None

def discover(paths):
    secrets, hosts = known_values()
    for index,p in enumerate(paths):
        if index % 400 == 0: print(f'Discovery {index}/{len(paths)} files',flush=True)
        s = text_bytes(p)
        if s is None: continue
        for line in s.splitlines():
            low=line.lower()
            if any(k in low for k in ('http','wss','grpc','ftp')):
                hosts.update(m.group(1) for m in URL.finditer(line) if '<gateway>' not in m.group(1))
            if any(k in low for k in ('key','token','password','authorization','secret')):
                for m in QUOTED_SECRET.finditer(line):
                    v = m.group(2).rstrip('\\')
                    if len(v) >= 8 and not any(x in v for x in ['${', '<redacted>', 'process.env', 'os.environ']):
                        secrets.add(v)
                secrets.update(m.group(2) for m in SHELL_SECRET.finditer(line) if len(m.group(2)) >= 8 and not m.group(2).startswith(('$', '<')))
    return secrets, hosts

class Redactor:
    def __init__(self, secrets=(), hosts=()):
        self.exact = re.compile('|'.join(re.escape(v) for v in sorted(secrets, key=len, reverse=True))) if secrets else None
        self.hosts = re.compile('|'.join(re.escape(v) for v in sorted(hosts, key=len, reverse=True)), re.I) if hosts else None
        self.counts = collections.Counter()
        self.literal_hosts=sorted(set(hosts)|{h.lower() for h in hosts}|{h.upper() for h in hosts},key=len,reverse=True)
        self.literal_secrets=sorted(secrets,key=len,reverse=True)

    def sub(self, pat, repl, s, label):
        s, n = pat.subn(repl, s)
        self.counts[label] += n
        return s

    def __call__(self, s):
        for v in self.literal_secrets:
            self.counts['exact_credential']+=s.count(v);s=s.replace(v,'<redacted>')
        # Literal passes cover authorities even outside URL strings, including
        # spellings discovered in archived configuration. Preserve private URL
        # userinfo removal by running that transformation first below.
        out=[]
        for line in s.splitlines(keepends=True):out.append(self.line(line))
        s=''.join(out)
        for v in self.literal_hosts:
            self.counts['bare_authority']+=s.count(v);s=s.replace(v,'<gateway>')
        return s

    def line(self,s):
        low=s.lower()
        if any(k in low for k in ('key','token','password','authorization','secret')):
            s = self.sub(QUOTED_SECRET, lambda m: m.group(1)+'<redacted>'+re.search(r'\\*$',m.group(2)).group(0)+m.group(3), s, 'credential_assignment')
            s = self.sub(SHELL_SECRET, lambda m: m.group(1)+'<redacted>', s, 'shell_credential')
        if any(k in low for k in ('sk-','ghp_','gho_','ghu_','ghs_','ghr_','github_pat_','akia','eyj')):
            s = self.sub(TOKEN, '<redacted>', s, 'token_pattern')
        if 'bearer' in low:s = self.sub(BEARER, lambda m: m.group(1)+'<redacted>', s, 'bearer')
        # Preserve scheme/path and replace the whole authority, including userinfo.
        if any(k in low for k in ('http','wss','grpc','ftp')):
            s = self.sub(URL, lambda m: m.group(0)[:-len(m.group(1))]+'<gateway>', s, 'url_authority')
        if any(k in low for k in ('.internal','.local','.lan')):s = self.sub(PRIVATE_HOST, '<gateway>', s, 'private_hostname')
        if any(k in low for k in ('.com','.org','.net','.edu','.io','.ai','.dev','.cloud')):s = self.sub(DNS, '<gateway>', s, 'bare_dns_candidate')
        def ip(m):
            try: return '<gateway>' if ipaddress.ip_address(m.group(0).split(':')[0]).is_private else m.group(0)
            except ValueError: return m.group(0)
        if re.search(r'\d+\.\d+\.\d+\.\d+',s):s = self.sub(IP, ip, s, 'ip_candidates')
        if 'reasoningOpaque' in s:s = self.sub(OPAQUE, lambda m: m.group(1)+'<omitted-opaque>'+m.group(3), s, 'opaque_field')
        return s

def dump_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n')

def normalize_records(original, sanitized, relative, invalid):
    """Retain source-truncated fragments inside explicit, valid JSON wrappers."""
    source_lines=original.splitlines(keepends=True)
    clean_lines=sanitized.splitlines(keepends=True)
    assert len(source_lines)==len(clean_lines), 'Redaction changed line count'
    result=[]
    for n,(source,clean) in enumerate(zip(source_lines,clean_lines),1):
        if not source.strip():result.append(clean);continue
        try:json.loads(source)
        except json.JSONDecodeError as e:
            info={'path':str(relative),'line':n,'original_chars':len(source),'reason':e.msg}
            invalid.append(info)
            result.append(json.dumps({'type':'archive.unparsed_record','data':{'reason':e.msg,'source_line':n,'original_chars':len(source),'sanitized_fragment':clean.rstrip('\n')}},ensure_ascii=False)+'\n')
            continue
        try:json.loads(clean)
        except json.JSONDecodeError:
            raise ValueError(f'Redaction damaged valid source JSON: {relative}:{n}') from None
        result.append(clean)
    return ''.join(result)

def repair_existing(source):
    """Integrity pass for already-exported files; compares source in memory only."""
    invalid=[];checked=0
    for p in sorted((ROOT/'jobs').rglob('*')):
        if not p.is_file() or not p.name.removesuffix('.gz').endswith(('.jsonl','.otel')):continue
        rel=p.relative_to(ROOT/'jobs');source_rel=Path(str(rel).removesuffix('.gz'))
        raw=(source/source_rel)
        if not raw.exists():raw=raw.with_name(raw.name+'.gz')
        original=text_bytes(raw);clean=text_bytes(p)
        if original is None or clean is None:continue
        repaired=normalize_records(original,clean,source_rel,invalid)
        if repaired!=clean:
            payload=repaired.encode()
            p.write_bytes(gzip.compress(payload,compresslevel=6,mtime=0) if p.suffix=='.gz' else payload)
        checked+=1
        if checked%30==0:print('Integrity-checked',checked,'record streams',flush=True)
    manifest=json.loads((ROOT/'metadata/export-manifest.json').read_text())
    manifest['source_invalid_records']=invalid
    manifest['stored_bytes']=sum(p.stat().st_size for p in (ROOT/'jobs').rglob('*') if p.is_file())
    dump_json(ROOT/'metadata/export-manifest.json',manifest)
    print('Source-invalid fragments wrapped:',len(invalid),'streams checked:',checked)

def export(source):
    jobs = sorted(p for p in source.iterdir() if p.is_dir() and RUN.fullmatch(p.name))
    assert len(jobs) == 33, f'Expected 33 selected runs, found {len(jobs)}'
    files = sorted(p for j in jobs for p in j.rglob('*') if p.is_file())
    secrets, hosts = discover(files)
    redactor = Redactor(secrets, hosts)
    manifest = {'runs': len(jobs), 'source_files': len(files), 'source_bytes': 0,
                'copied_files': 0, 'stored_bytes': 0, 'uncompressed_export_bytes': 0,
                'omitted': [], 'compression_threshold_bytes': 200_000,
                'policy': 'All URL authorities; known bare authorities; private hosts/IPs; credential assignments, tokens and bearer values; opaque reasoning blobs.'}
    for index, p in enumerate(files):
        rel = p.relative_to(source)
        manifest['source_bytes'] += p.stat().st_size
        s = text_bytes(p)
        if s is None:
            manifest['omitted'].append({'path': str(rel), 'reason': 'binary, non-UTF8, serialized object, or symbolic link', 'bytes': p.stat().st_size})
            continue
        clean=redactor(s)
        if p.name.removesuffix('.gz').endswith(('.jsonl','.otel')):
            clean=normalize_records(s,clean,rel,manifest.setdefault('source_invalid_records',[]))
        raw = clean.encode('utf-8')
        out = ROOT/'jobs'/rel
        if out.suffix == '.gz': out = out.with_suffix('')
        # Keep Harbor config/results readable. Large log/code files are gzipped.
        compressed = len(raw) > 200_000 and out.name not in {'config.json','result.json','lock.json'}
        if compressed:
            out = out.with_name(out.name+'.gz')
            raw_stored = gzip.compress(raw, compresslevel=6, mtime=0)
        else: raw_stored = raw
        if len(raw_stored) >= 49_000_000:
            # Lossless chunks with an explicit index; never truncate trajectories.
            chunks=[]
            for i, start in enumerate(range(0,len(raw),24_000_000)):
                part=out.with_name(out.name+f'.part{i:03d}.gz')
                part.parent.mkdir(parents=True,exist_ok=True)
                payload=gzip.compress(raw[start:start+24_000_000],mtime=0)
                part.write_bytes(payload); chunks.append(part.name)
                manifest['stored_bytes']+=len(payload)
            dump_json(out.with_name(out.name+'.parts.json'),{'encoding':'gzip-chunks','parts':chunks})
        else:
            out.parent.mkdir(parents=True,exist_ok=True)
            out.write_bytes(raw_stored)
            manifest['stored_bytes'] += len(raw_stored)
        manifest['copied_files'] += 1
        manifest['uncompressed_export_bytes'] += len(raw)
        if index % 400 == 0: print(f'Processed {index}/{len(files)} files', flush=True)
    manifest['redaction_counts'] = dict(redactor.counts)
    manifest['discovered_authority_count'] = len(hosts)
    manifest['discovered_credential_count'] = len(secrets)
    dump_json(ROOT/'metadata/export-manifest.json', manifest)
    print(json.dumps({k:v for k,v in manifest.items() if k not in {'omitted','policy','redaction_counts'}}))

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--repair-existing',action='store_true')
    args=parser.parse_args()
    repair_existing(args.source) if args.repair_existing else export(args.source)
