/* Original adapter, inspired by the read-only snapshot design at
 * https://realtimeagents.org/harbor/. Native Harbor assets remain Apache-2.0. */
const harborBase = '/ttc-polyomino-trajectories/harbor/';
const restore = sessionStorage.getItem('harbor-return');
if (restore?.startsWith(harborBase)) {
  sessionStorage.removeItem('harbor-return');
  history.replaceState(null, '', restore);
}
window.harborReady = (async () => {
  if (!('serviceWorker' in navigator)) throw Error('A current browser and HTTPS (or localhost) are required.');
  await navigator.serviceWorker.register(harborBase + 'snapshot-worker.js', {scope: harborBase});
  await navigator.serviceWorker.ready;
  if (!navigator.serviceWorker.controller) {
    await new Promise(resolve => navigator.serviceWorker.addEventListener('controllerchange', resolve, {once:true}));
  }
})();
window.harborReady.catch(error => {
  document.body.replaceChildren();
  const p = document.createElement('p'); p.textContent = `Viewer could not start: ${error.message}`;
  const a = document.createElement('a'); a.href='../';a.textContent='Browse the study overview';
  document.body.append(p,a);
});
function annotateSnapshot() {
  for (const node of document.querySelectorAll('a,button')) {
    if (/^(New run|Sign in|Upload|Delete|Summarize|Generate analysis|Open editor|Compare|Compare jobs|Compare selected)$/.test(node.textContent.trim()) || node.getAttribute('href')?.endsWith('/run')) node.dataset.snapshotWrite='';
  }
  if (document.body && !document.getElementById('snapshot-note')) {
    const note=document.createElement('div');note.id='snapshot-note';
    const a=document.createElement('a');a.href='/ttc-polyomino-trajectories/';a.textContent='← Study overview';
    note.append(a,document.createTextNode(' · Read-only archive · Agent labels identify a timestamp-merged Copilot projection. Shared activity does not establish knowledge transfer.'));
    document.body.prepend(note);
  }
}
new MutationObserver(annotateSnapshot).observe(document.documentElement,{childList:true,subtree:true});
document.addEventListener('DOMContentLoaded',annotateSnapshot);
