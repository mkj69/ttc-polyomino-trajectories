/* Static, read-only Harbor API. No remote API or write operation is used. */
const BASE='/ttc-polyomino-trajectories/harbor/';
self.addEventListener('install',()=>self.skipWaiting());
self.addEventListener('activate',e=>e.waitUntil(self.clients.claim()));
const response=(value,status=200)=>new Response(JSON.stringify(value),{status,headers:{'Content-Type':'application/json; charset=utf-8'}});
const unavailable=()=>response({detail:'Not included in this read-only snapshot'},404);
async function decode(r,json=true){
  if(!r.ok)throw Error('Snapshot data unavailable');
  let b=await r.arrayBuffer();let bytes=new Uint8Array(b);
  if(bytes[0]===31&&bytes[1]===139)b=await new Response(new Blob([b]).stream().pipeThrough(new DecompressionStream('gzip'))).arrayBuffer();
  const s=new TextDecoder().decode(b);return json?JSON.parse(s):s;
}
let snapshot;const trials=new Map();
async function load(){
  return snapshot??=(fetch(BASE+'snapshot.json.gz',{cache:'no-store'}).then(r=>decode(r)).catch(e=>{snapshot=null;throw e}));
}
function paginate(items,q){
  const fields={agent:'agent_name',provider:'model_provider',model:'model_name',task:'task_name',source:'source',reasoning_effort:'reasoning_effort'};
  const plural={agent:'agents',provider:'providers',model:'models',source:'sources'};
  const search=(q.get('q')||'').toLowerCase();
  items=items.filter(item=>{
    if(search&&!JSON.stringify(item).toLowerCase().includes(search))return false;
    for(const [key,field]of Object.entries(fields)){
      const values=q.getAll(key);if(values.length&&!values.some(v=>item[field]===v||item[plural[key]]?.includes(v)))return false;
    }
    for(const key of ['task_name','agent_name','source']){
      const v=q.get(key);if(v&&v!=='_'&&item[key]!==v)return false;
    }
    const model=q.get('model_name');if(model&&model!=='_'&&![item.model_name,`${item.model_provider}/${item.model_name}`].includes(model))return false;
    const dates=q.getAll('date');if(dates.length&&!dates.some(v=>item.started_at?.startsWith(v)))return false;
    return true;
  });
  const sort=q.get('sort_by');if(sort)items.sort((a,b)=>(typeof a[sort]==='number'?a[sort]-b[sort]:String(a[sort]??'').localeCompare(String(b[sort]??'')))*(q.get('sort_order')==='desc'?-1:1));
  const size=Math.max(1,Math.min(100,Number(q.get('page_size'))||100));const page=Math.max(1,Number(q.get('page'))||1);
  return {items:items.slice((page-1)*size,page*size),total:items.length,page,page_size:size,total_pages:Math.ceil(items.length/size)};
}
async function api(request,url){
  if(!['GET','HEAD'].includes(request.method))return response({detail:'Published snapshot is read-only'},405);
  const d=await load(),path=decodeURIComponent(url.pathname);
  if(path==='/api/pricing')return response({input_cost_per_token:null,output_cost_per_token:null,cache_read_input_token_cost:null});
  if(path in d.lists)return response(paginate([...d.lists[path]],url.searchParams));
  if(path in d.api)return response(d.api[path]);
  if(path in d.files){
    const f=d.files[path],target=new URL(f.url,self.location.href);
    // Native Harbor itself limits file previews to 1 MiB. Keep raw download
    // accessible from this message without loading giant event streams.
    if(f.bytes>1024*1024)return new Response(`This archived file is ${f.bytes.toLocaleString()} uncompressed bytes and exceeds the 1 MiB Harbor preview limit.\n\nDownload the complete sanitized file:\n${target.href}\n\nFor readable completed messages and tools, use the Trajectory tab.`,{headers:{'Content-Type':'text/plain; charset=utf-8'}});
    return new Response(await decode(await fetch(target),false),{headers:{'Content-Type':'text/plain; charset=utf-8'}});
  }
  const m=path.match(/^(\/api\/jobs\/[^/]+\/trials\/[^/]+)(.*)$/);
  if(!m||!d.trials[m[1]])return unavailable();
  const root=m[1],suffix=m[2];
  if(!trials.has(root)){
    trials.set(root,fetch(BASE+d.trials[root]).then(r=>decode(r)).catch(e=>{trials.delete(root);throw e}));
    if(trials.size>3)trials.delete(trials.keys().next().value);
  }
  const t=await trials.get(root);
  if(suffix==='/files/agent/trajectory.json')return new Response(JSON.stringify(t['/trajectory'],null,2),{headers:{'Content-Type':'text/plain; charset=utf-8'}});
  return suffix in t?response(t[suffix]):unavailable();
}
self.addEventListener('fetch',event=>{
  const u=new URL(event.request.url);if(u.origin!==self.location.origin)return;
  if(u.pathname.startsWith(BASE+'api/'))u.pathname=u.pathname.slice(BASE.length-1);
  if(u.pathname.startsWith('/api/'))event.respondWith(api(event.request,u).catch(()=>response({detail:'Unable to load static data. Reload this page.'},503)));
  else if(event.request.mode==='navigate'&&u.pathname.startsWith(BASE)&&!u.pathname.match(/\.(?:js|css|json|gz)$/)){
    event.respondWith(fetch(event.request).then(r=>r.ok?r:fetch(BASE+'index.html')));
  }
});
