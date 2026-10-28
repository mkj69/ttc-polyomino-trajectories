/* Browser integration checks. Install Playwright outside the public repository.
 * PLAYWRIGHT_MODULE=/absolute/path/to/playwright/index.mjs node scripts/check_site.mjs URL
 * Screenshots and JSON results go to QA_OUTPUT (default ../figures).
 */
import fs from 'node:fs/promises';
import path from 'node:path';
const {chromium}=await import(process.env.PLAYWRIGHT_MODULE||'playwright');
const base=(process.argv[2]||'http://127.0.0.1:8766/ttc-polyomino-trajectories/').replace(/\/?$/,'/');
const output=path.resolve(process.env.QA_OUTPUT||'../figures');await fs.mkdir(output,{recursive:true});
const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
const results={base,checks:[],console_errors:[],failed_requests:[]};
function check(name,ok){results.checks.push({name,passed:!!ok});if(!ok)throw Error('Check failed: '+name)}
function observe(page){page.on('pageerror',error=>results.console_errors.push(error.message));page.on('requestfailed',r=>results.failed_requests.push({url:new URL(r.url()).pathname,error:r.failure()?.errorText}));}
try{
  const context=await browser.newContext({viewport:{width:1440,height:1000}}),page=await context.newPage();observe(page);
  await page.goto(base);await page.locator('#run-view:not([hidden])').waitFor({timeout:120000});
  const study=await page.evaluate(async()=>await(await fetch('data/study.json')).json());
  check('33 archived runs',study.runs.length===33);
  check('overview summary rows',await page.locator('#summary tr').count()===2);
  check('complete run index',await page.locator('#runs-table tr').count()===33);
  check('score curve exists',await page.locator('#curve svg path').count()>0);
  await page.locator('#events button').first().click();check('event detail interaction',(await page.locator('#event-detail').innerText()).includes('minutes'));
  await page.selectOption('#event-kind','submission');check('submission event filter',(await page.locator('#events').innerText()).includes('submission'));
  await page.locator('#reset-window').click();
  await page.screenshot({path:path.join(output,'overview-desktop.png'),fullPage:true});
  await page.selectOption('#model','gpt-6.1-sol');await page.selectOption('#setup','iso3');
  await page.locator('#run-view:not([hidden])').waitFor();check('overview model/protocol filters',(await page.locator('#run-title').innerText()).includes('gpt-6.1-sol · iso3'));
  // Fresh mobile context also tests that the overview needs no service worker.
  const mobile=await browser.newContext({viewport:{width:390,height:844},isMobile:true,deviceScaleFactor:1});
  const mp=await mobile.newPage();observe(mp);await mp.goto(base);await mp.locator('#run-view:not([hidden])').waitFor({timeout:120000});
  check('mobile viewport avoids page overflow',await mp.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+1));
  await mp.screenshot({path:path.join(output,'overview-mobile.png'),fullPage:true});await mobile.close();
  // Go straight to a trial in a clean context: no previous SW installation.
  const native=await browser.newContext({viewport:{width:1440,height:1000}}),hp=await native.newPage();observe(hp);
  const sample=study.runs.find(r=>r.model==='Sonnet 4.6'&&r.setup==='team3'&&r.hours===3);
  const trial=`harbor/jobs/${encodeURIComponent(sample.id)}/trials/${encodeURIComponent(sample.trial)}/`;
  const response=await hp.goto(base+trial);check('fresh Harbor deep link HTTP 200',response.status()===200);
  await hp.waitForFunction(()=>!!navigator.serviceWorker.controller,{timeout:60000});
  await hp.waitForFunction(()=>document.body.innerText.includes('Trajectory'),{timeout:120000});
  await hp.waitForTimeout(2000);check('native Harbor trajectory renders',!(await hp.locator('body').innerText()).includes('Unexpected Application Error'));
  const api=await hp.evaluate(async ({sample})=>{
    const root=`/api/jobs/${sample.id}/trials/${sample.trial}`;
    const get=async p=>{const r=await fetch(p);return{status:r.status,value:await r.json()}};
    const jobs=await get('/api/jobs?page_size=5&page=2');const trajectory=await get(root+'/trajectory');const file=await fetch(root+'/files/config.json');
    const denied=await fetch('/api/jobs/'+sample.id,{method:'DELETE'});
    return{jobsTotal:jobs.value.total,page:jobs.value.page,items:jobs.value.items.length,trajectorySteps:trajectory.value.steps.length,hasAgentLabels:trajectory.value.steps.some(s=>s.extra?.agent_id==='a2'),fileStatus:file.status,fileJSON:!!JSON.parse(await file.text()),writeStatus:denied.status};
  },{sample});
  check('snapshot API pagination',api.jobsTotal===33&&api.page===2&&api.items===5);
  check('snapshot labels all agents',api.trajectorySteps>0&&api.hasAgentLabels);
  check('snapshot file endpoint',api.fileStatus===200&&api.fileJSON);
  check('snapshot rejects writes',api.writeStatus===405);
  await hp.screenshot({path:path.join(output,'harbor-desktop.png'),fullPage:false});
  // Exercise all 33 per-run JSONs and native APIs from a browser-controlled page.
  const all=await hp.evaluate(async runs=>{
    const rows=[];
    for(const r of runs){
      const root=`/api/jobs/${r.id}/trials/${r.trial}`;
      const response=await fetch(root+'/trajectory');const t=await response.json();
      rows.push({id:r.id,status:response.status,steps:t?.steps?.length??0,expected:r.steps});
    }
    return rows;
  },study.runs);
  check('all 33 native trajectory APIs',all.every(r=>r.status===200&&r.steps===r.expected));results.api_runs=all;
  // Avoid carrying requests aborted by browser closure into failure reporting.
  results.failed_requests=results.failed_requests.filter(r=>!r.error?.includes('ERR_ABORTED'));
  check('no JavaScript runtime errors',results.console_errors.length===0);
  check('no failed browser network requests',results.failed_requests.length===0);
  results.passed=true;
}catch(error){results.passed=false;results.error=error.message;process.exitCode=1}
finally{await fs.writeFile(path.join(output,'browser-checks.json'),JSON.stringify(results,null,2)+'\n');console.log(JSON.stringify({passed:results.passed,checks:results.checks,error:results.error,console_errors:results.console_errors,failed_requests:results.failed_requests}));await browser.close()}
