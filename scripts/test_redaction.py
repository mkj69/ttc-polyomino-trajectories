#!/usr/bin/env python3
"""Security regression checks; synthetic fixtures only, no external data."""
import gzip
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from redact import Redactor, TOKEN, ROOT, normalize_records

class RedactionTests(unittest.TestCase):
    def test_nested_json_and_shell(self):
        token='s'+'k-'+'A'*40
        content={'api_key':token,'command':f'export OPENAI_API_KEY="{token}"; curl -H "Authorization: Bearer {token}" https://private.example.invalid/v1/chat'}
        nested={'content':json.dumps(content)}
        clean=Redactor([token],['private.example.invalid'])(json.dumps(nested))
        decoded=json.loads(json.loads(clean)['content'])
        self.assertNotIn(token,clean)
        self.assertNotIn('private.example.invalid',clean)
        self.assertEqual(decoded['api_key'],'<redacted>')

    def test_unknown_credentials(self):
        value='s'+'k-'+'B'*42
        clean=Redactor()(json.dumps({'api_key':value,'content':'Bearer '+'C'*35}))
        self.assertNotIn(value,clean)
        self.assertNotIn('C'*35,clean)
        json.loads(clean)

    def test_urls_and_bare_hosts(self):
        clean=Redactor(hosts=['compute.example.invalid:1234'])('https://user:pass@compute.example.invalid:1234/v1 compute.example.invalid:1234')
        self.assertNotIn('compute.example.invalid',clean)
        self.assertIn('https://<gateway>/v1',clean)

    def test_opaque_blob(self):
        clean=json.loads(Redactor()(json.dumps({'reasoningOpaque':'X'*20000})))
        self.assertEqual(clean['reasoningOpaque'],'<omitted-opaque>')

    def test_cpp_intact(self):
        code='#include <bits/stdc++.h>\nint main(){double score=0.812; std::string s="solution.cpp";}\n'
        self.assertEqual(Redactor()(code),code)

    def test_truncated_source_record_preserved(self):
        source='{"type":"assistant.message","data":{"content":"truncated'
        invalid=[]
        result=normalize_records(source,source,'sample.jsonl',invalid)
        row=json.loads(result)
        self.assertEqual(row['type'],'archive.unparsed_record')
        self.assertEqual(row['data']['sanitized_fragment'],source)
        self.assertEqual(len(invalid),1)

    def test_valid_json_damage_rejected(self):
        with self.assertRaises(ValueError):
            normalize_records('{"type":"ok"}\n','{"type":"bad\n','sample.jsonl',[])

    def test_scan_fails_on_gzipped_secret(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'jobs').mkdir()
            (root/'jobs/events.jsonl.gz').write_bytes(gzip.compress(json.dumps({'message':'s'+'k-'+'D'*45}).encode()))
            result=subprocess.run([sys.executable,str(ROOT/'scripts/scan.py'),'--root',str(root)],capture_output=True,text=True)
            self.assertEqual(result.returncode,1)
            self.assertIn('credential_token',result.stdout)
            self.assertNotIn('D'*45,result.stdout)

    def test_scan_fails_on_unredacted_authority(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'jobs').mkdir()
            (root/'jobs/test.json').write_text(json.dumps({'base_url':'https://synthetic.example.invalid/v1'}))
            result=subprocess.run([sys.executable,str(ROOT/'scripts/scan.py'),'--root',str(root)],capture_output=True,text=True)
            self.assertEqual(result.returncode,1)
            self.assertIn('unredacted_url_authority',result.stdout)
            self.assertNotIn('synthetic.example.invalid',result.stdout)

if __name__=='__main__':unittest.main()
