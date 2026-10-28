'use strict';
const $=id=>document.getElementById(id), COLORS={a1:'#226b53',a2:'#bd7020',a3:'#5669ad'}, KIND={tool:'#c7cec5',shared_read:'#5669ad',shared_write:'#c68922',submission:'#226b53'};
let study,current,page=0,visible=[],plotted=[],requestId=0;const PAGE=20;
const fmt=v=>v==null?'—':Number(v).toFixed(3), minute=t=>Number(t).toFixed(1), esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
async function loadJSON(url){const r=await fetch(url);if(!r.ok)throw Error(`Unable to load archive data (${r.status}).`);let b=await r.arrayBuffer();const u=new Uint8Array(b);if(u[0]===31&&u[1]===139){if(!('DecompressionStream'in window))throw Error('Please use a current browser with gzip decompression support.');b=await new Response(new Blob([b]).stream().pipeThrough(new DecompressionStream('gzip'))).arrayBuffer()}return JSON.parse(new TextDecoder().decode(b))}
const trialURL=r=>`harbor/jobs/${encodeURIComponent(r.id)}/trials/${encodeURIComponent(r.trial)}/`;
function filters(){return study.runs.filter(r=>(!$('model').value||r.model===$('model').value)&&(!$('setup').value||r.setup===$('setup').value)&&(!$('hours').value||String(r.hours)===$('hours').value))}
function fillRuns(preferred){const runs=filters();$('run').replaceChildren(...runs.map(r=>new Option(`${r.model} · ${r.setup} · ${r.hours}h · r${r.round} · ${fmt(r.reward)}`,r.id)));$('run-count').textContent=`${runs.length} / 33 runs`;$('run').disabled=!runs.length;if(runs.length){$('run').value=runs.some(r=>r.id===preferred)?preferred:runs[0].id;selectRun()}else{$('run-view').hidden=true;$('loading').textContent='No runs match these filters.';$('loading').hidden=false}}
function renderSummary(){
  $('total-subs').textContent=study.runs.reduce((s,r)=>s+r.submissions,0).toLocaleString();
  $('summary').innerHTML=['Sonnet 4.6','gpt-6.1-sol'].map(model=>`<tr><td><b>${esc(model)}</b></td>${['solo','team3','iso3'].map(setup=>{const s=study.summary.find(x=>x.model===model&&x.setup===setup);return `<td>${fmt(s.mean)} <span class="muted">± ${fmt(s.std_sample)}</span></td>`}).join('')}<td>${fmt(study.provided_context[model].best_at_3_solo)}</td></tr>`).join('');
  $('long-results').textContent='12-hour Sonnet final rewards (one run each): '+study.runs.filter(r=>r.hours===12).sort((a,b)=>['solo','team3','iso3'].indexOf(a.setup)-['solo','team3','iso3'].indexOf(b.setup)).map(r=>`${r.setup} ${fmt(r.reward)}`).join(' · ')+'.';
  $('runs-table').innerHTML=study.runs.map(r=>`<tr><td>${esc(r.model)}</td><td><button data-run="${esc(r.id)}">${esc(r.setup)}</button></td><td>${r.hours}</td><td>r${r.round}</td><td><b>${fmt(r.reward)}</b></td><td>${fmt(r.best_submission)}</td><td>${r.submissions}</td><td><a href="${trialURL(r)}">Harbor ↗</a></td></tr>`).join('');
}
async function selectRun(){
  const r=study.runs.find(r=>r.id===$('run').value);if(!r)return;
  const request=++requestId;$('loading').hidden=false;$('loading').textContent='Loading run timeline…';$('error').hidden=true;$('run-view').hidden=true;
  try{const data=await loadJSON(`data/runs/${encodeURIComponent(r.id)}.json.gz`);if(request!==requestId)return;current=data;page=0;history.replaceState(null,'',`#run=${encodeURIComponent(r.id)}`);
    $('run-title').textContent=`${r.model} · ${r.setup} · ${r.hours} hours · replicate ${r.round}`;$('run-id').textContent=r.id;$('harbor-link').href=trialURL(r);
    $('run-metrics').innerHTML=[['Final reward',fmt(r.reward)],['Best submitted',fmt(r.best_submission)],['Submissions',r.submissions],['Tools observed',(r.counts.tool||0)+(r.counts.shared_read||0)+(r.counts.shared_write||0)],['Shared refs*',(r.counts.shared_read||0)+(r.counts.shared_write||0)]].map(([k,v])=>`<div class="metric"><strong>${v}</strong>${k}</div>`).join('');
    $('from').value=0;$('to').value=Math.ceil(r.minutes_observed);$('from').max=$('to').max=Math.ceil(r.minutes_observed);$('event-agent').value='';$('event-kind').value='';$('event-search').value='';
    $('downloads').innerHTML=r.source_sessions.map(s=>`<li><a download href="archive/${encodeURIComponent(r.id)}/${encodeURIComponent(r.trial)}/${s.split('/').map(encodeURIComponent).join('/')}">${esc(s)}</a></li>`).join('');
    $('snapshot-download').href=`harbor/snapshots/${encodeURIComponent(r.id)}.json.gz`;$('snapshot-download').setAttribute('download','');
    $('event-detail').textContent='Choose an event on the timeline or in the list below.';$('run-view').hidden=false;$('loading').hidden=true;renderCurve();renderEvents();drawTimeline();
  }catch(e){if(request!==requestId)return;$('loading').hidden=true;$('error').hidden=false;$('error').textContent=e.message}
}
function renderCurve(){
  const r=current.run,w=960,h=280,left=47,right=20,top=20,bottom=38,max=r.minutes_observed,x=t=>left+Math.max(0,t)/max*(w-left-right),y=s=>top+(1-s)*(h-top-bottom);
  let svg=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="Best submitted score over minutes. Final reward ${fmt(r.reward)}, best submission ${fmt(r.best_submission)}.">`;
  for(let s=0;s<=1.001;s+=.25)svg+=`<line x1="${left}" y1="${y(s)}" x2="${w-right}" y2="${y(s)}" stroke="#d5dcd1"/><text x="${left-8}" y="${y(s)+4}" text-anchor="end">${s.toFixed(2)}</text>`;
  for(let i=0;i<=6;i++){const t=max*i/6;svg+=`<text x="${x(t)}" y="${h-15}" text-anchor="middle">${Math.round(t)}m</text>`}
  function path(points){let d=`M ${x(0)} ${y(0)}`;for(const [t,s]of points)d+=` H ${x(t)} V ${y(s)}`;return d+` H ${x(max)}`}
  for(let a=1;a<=r.agents;a++){const agent='a'+a;let best=0,points=[];for(const s of current.submissions)if(s.a===agent&&s.score!=null&&s.score>best){best=s.score;points.push([s.t,best])}svg+=`<path d="${path(points)}" fill="none" stroke="${COLORS[agent]}" stroke-width="1.4" stroke-dasharray="4 4" opacity=".65"/>`}
  svg+=`<path d="${path(r.curve)}" fill="none" stroke="#182f27" stroke-width="2.5"/>`;
  for(const [t,s,a]of r.curve)svg+=`<circle cx="${x(t)}" cy="${y(s)}" r="3.5" fill="${COLORS[a]||COLORS.a1}"><title>${minute(t)} min · ${esc(a)} · ${fmt(s)}</title></circle>`;
  if(r.reward!=null){const px=x(max),py=y(r.reward);svg+=`<path d="M${px},${py-6} l6,6 -6,6 -6,-6Z" fill="#182f27"><title>Final reward ${fmt(r.reward)}</title></path>`}
  $('curve').innerHTML=svg+'</svg>';
}
function windowRange(){let lo=Number($('from').value)||0,hi=Number($('to').value)||current.run.minutes_observed;lo=Math.max(0,lo);hi=Math.max(lo+1,hi);return[lo,hi]}
function drawTimeline(){
  if(!current||$('run-view').hidden)return;const canvas=$('timeline'),width=canvas.parentElement.clientWidth,dpr=window.devicePixelRatio||1,height=current.run.agents===1?115:205;canvas.style.height=height+'px';canvas.width=width*dpr;canvas.height=height*dpr;const c=canvas.getContext('2d');c.scale(dpr,dpr);c.clearRect(0,0,width,height);
  const [lo,hi]=windowRange(),left=40,right=14,usable=width-left-right,x=t=>left+(t-lo)/(hi-lo)*usable;plotted=[];
  for(let a=1;a<=current.run.agents;a++){let y=30+(a-1)*55;c.strokeStyle='#d5dcd1';c.beginPath();c.moveTo(left,y);c.lineTo(width-right,y);c.stroke();c.fillStyle=COLORS['a'+a];c.font='12px system-ui';c.fillText('a'+a,4,y+4)}
  for(const e of current.events){if(e.t<lo||e.t>hi)continue;const y=30+(Number(e.a.slice(1))-1)*55,px=x(e.t);c.fillStyle=KIND[e.k]||KIND.tool;if(e.k==='submission'){c.beginPath();c.arc(px,y-8,4,0,2*Math.PI);c.fill()}else{c.globalAlpha=e.k==='tool'?.5:.9;c.fillRect(px,y+3,2,e.k==='tool'?10:20);c.globalAlpha=1}plotted.push({x:px,y:e.k==='submission'?y-8:y+10,e})}
  c.fillStyle='#586a60';c.font='10px system-ui';for(let i=0;i<=5;i++){let t=lo+(hi-lo)*i/5;c.textAlign=i===0?'left':i===5?'right':'center';c.fillText(Math.round(t)+'m',x(t),height-10)}
}
function renderEvents(){
  if(!current)return;const [lo,hi]=windowRange(),search=$('event-search').value.toLowerCase();visible=current.events.filter(e=>e.t>=lo&&e.t<=hi&&(!$('event-agent').value||e.a===$('event-agent').value)&&(!$('event-kind').value||e.k===$('event-kind').value)&&(!search||JSON.stringify(e).toLowerCase().includes(search)));
  const pages=Math.max(1,Math.ceil(visible.length/PAGE));page=Math.max(0,Math.min(page,pages-1));$('event-count').textContent=`(${visible.length.toLocaleString()})`;
  $('events').innerHTML=visible.slice(page*PAGE,(page+1)*PAGE).map((e,i)=>`<tr><td>${minute(e.t)}</td><td style="color:${COLORS[e.a]}">${esc(e.a)}</td><td>${esc(e.k.replace('_',' '))}</td><td>${e.k==='submission'?`<b>${fmt(e.score)}</b> · ${esc(e.path)}`:esc(e.label)}</td><td><button data-event="${page*PAGE+i}" aria-label="Inspect ${esc(e.a)} ${esc(e.k)} at ${minute(e.t)} minutes">Open</button></td></tr>`).join('')||'<tr><td colspan="5">No events match these filters.</td></tr>';
  $('page-info').textContent=`Page ${page+1} of ${pages}`;$('previous').disabled=page===0;$('next').disabled=page>=pages-1;
}
function detail(e){$('event-detail').replaceChildren();const text=document.createElement('div');text.textContent=`${minute(e.t)} minutes · ${e.a} · ${e.k.replace('_',' ')}${e.step?` · ATIF step ${e.step}`:''}\n\n`+(e.k==='submission'?`Score: ${fmt(e.score)}\nSolution: ${e.path}\nSubmission: ${e.uuid}\nTimestamp: ${e.timestamp}\nOriginal code characters: ${e.code_chars??'unknown'}`:`Tool: ${e.name}\n${e.label}${e.label?.length>=240?'\n[Preview limited to 240 characters; full command is in the trajectory.]':''}${e.logs?.length?'\nShared logs: '+e.logs.join(', '):''}`);const a=document.createElement('a');a.href=trialURL(current.run);a.textContent='Read full messages, tools, and results in Harbor ↗';$('event-detail').append(text,a)}
$('timeline').addEventListener('click',e=>{const r=e.target.getBoundingClientRect(),x=e.clientX-r.left,y=e.clientY-r.top;let best=null,dist=100;for(const p of plotted){const d=(p.x-x)**2+(p.y-y)**2;if(d<dist){dist=d;best=p.e}}if(best)detail(best)});
for(const id of ['model','setup','hours'])$(id).addEventListener('change',()=>fillRuns(current?.run.id));$('run').addEventListener('change',selectRun);
for(const id of ['event-agent','event-kind','event-search'])$(id).addEventListener(id==='event-search'?'input':'change',()=>{page=0;renderEvents()});
for(const id of ['from','to'])$(id).addEventListener('change',()=>{page=0;drawTimeline();renderEvents()});$('reset-window').addEventListener('click',()=>{$('from').value=0;$('to').value=Math.ceil(current.run.minutes_observed);page=0;drawTimeline();renderEvents()});
$('previous').addEventListener('click',()=>{page--;renderEvents()});$('next').addEventListener('click',()=>{page++;renderEvents()});
$('events').addEventListener('click',e=>{const b=e.target.closest('[data-event]');if(b)detail(visible[Number(b.dataset.event)])});
$('runs-table').addEventListener('click',e=>{const b=e.target.closest('[data-run]');if(!b)return;for(const id of ['model','setup','hours'])$(id).value='';fillRuns(b.dataset.run);$('explore').scrollIntoView({behavior:'smooth'})});
window.addEventListener('resize',drawTimeline);
loadJSON('data/study.json').then(data=>{study=data;renderSummary();const selected=location.hash.startsWith('#run=')?decodeURIComponent(location.hash.slice(5)):null;fillRuns(selected||study.runs.find(r=>r.hours===3&&r.model==='Sonnet 4.6'&&r.setup==='team3')?.id)}).catch(e=>{$('loading').hidden=true;$('error').hidden=false;$('error').textContent=e.message});
