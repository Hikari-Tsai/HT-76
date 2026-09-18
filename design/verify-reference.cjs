// Requires Playwright; set NODE_PATH to its installation directory.
const {chromium}=require('playwright');
const fs=require('fs');const path=require('path');const {pathToFileURL}=require('url');
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
 const page=await browser.newPage({viewport:{width:1400,height:850},deviceScaleFactor:1.5,reducedMotion:'reduce'});
 const errors=[],checks=[];page.on('pageerror',e=>errors.push(e.message));
 const check=(name,pass,details)=>{checks.push({name,pass,...(details?{details}:{})});if(!pass)throw Error(name)};
 try{
 await page.goto(pathToFileURL(path.join(__dirname,'index.html')).href);await page.waitForTimeout(100);
 for(const width of [1400,1088,800,400]){
  await page.setViewportSize({width,height:850});await page.waitForTimeout(80);
  for(const view of ['rack','dynamic']){
   await page.locator(`[data-view=${view}]`).click();
   const box=await page.locator('.fp-window').boundingBox(),ratio=box.width/box.height,target=view==='rack'?1753/457:1753/695;
   check(`${view} reference ratio at ${width}`,Math.abs(ratio-target)<.003,{ratio,target});
   const layout=await page.evaluate(()=>{const win=document.querySelector('.fp-window'),r=win.getBoundingClientRect();return {overflow:document.documentElement.scrollWidth>innerWidth,clipped:[...win.querySelectorAll('button,input,canvas')].filter(e=>e.getClientRects().length).filter(e=>{const b=e.getBoundingClientRect();return b.left<r.left-1||b.top<r.top-1||b.right>r.right+1||b.bottom>r.bottom+1}).map(e=>e.className||e.dataset.param)}});
   check(`${view} controls fit at ${width}`,!layout.overflow&&!layout.clipped.length,layout);
  }
 }
 await page.setViewportSize({width:1400,height:850});await page.locator('[data-view=rack]').click();
 const input=page.locator('.fp-rack [data-param=input]'),b=await input.boundingBox();
 await page.mouse.move(b.x+b.width/2,b.y+b.height/2);await page.mouse.down();await page.mouse.move(b.x+b.width/2,b.y+b.height/2-25);await page.mouse.up();
 const value=await input.inputValue();check('knob drag changes input',value!=='533');
 await page.locator('[data-view=dynamic]').click();check('input persists between views',value===await page.locator('.fp-deck [data-param=input]').inputValue());
 await page.locator('.fp-deck [data-param=input]').dblclick();check('double click resets input',await page.locator('.fp-deck [data-value=input]').textContent()==='+12.0');
 const attack=page.locator('.fp-deck [data-param=attack]');await attack.focus();await page.keyboard.press('ArrowRight');
 check('clockwise attack is faster',parseInt(await attack.getAttribute('aria-valuetext'),10)<200);await attack.dblclick();
 await page.locator('.fp-deck [data-ratio="8"]').click();await page.locator('.fp-deck [data-ratio=all]').click();await page.locator('.fp-deck [data-ratio=all]').click();
 check('ALL restores previous ratio',await page.locator('.fp-deck [data-ratio="8"]').getAttribute('aria-pressed')==='true');
 await page.locator('.fp-deck [data-ratio="4"]').click();
 const canvas=()=>page.locator('.fp-chart').evaluate(e=>e.toDataURL());const paused=await canvas();await page.waitForTimeout(80);check('reduced motion pauses level history',paused===await canvas());
 await page.locator('[data-action=play]').click();await page.waitForTimeout(150);check('play advances level history',paused!==await canvas());
 await page.locator('.fp-bypass-block [data-action=bypass]').click();await page.waitForTimeout(80);check('bypass drives GR to zero',await page.locator('[data-reduction-readout]').textContent()==='GR −0.0 dB');
 await page.locator('.fp-bypass-block [data-action=bypass]').click();
 await page.locator('[data-source]').selectOption('vocal');await page.waitForTimeout(120);check('demo source remains selectable',await page.locator('[data-source]').inputValue()==='vocal');
 // Reload to capture consistent initial states matching the delivered preview.
 await page.reload();await page.waitForTimeout(200);
 for(const view of ['rack','dynamic']){await page.locator(`[data-view=${view}]`).click();await page.waitForTimeout(150);await page.locator('#fet-panel-study').screenshot({path:path.join(__dirname,`previews/${view}-reference-v5.png`)})}
 check('no browser errors',errors.length===0,errors);
 }catch(e){checks.push({name:'verification error',pass:false,details:e.message});process.exitCode=1}
 fs.writeFileSync(path.join(__dirname,'previews/verification-v5.json'),JSON.stringify({date:'2026-09-18',checks},null,2));
 console.log(JSON.stringify(checks,null,2));await browser.close();
})();
