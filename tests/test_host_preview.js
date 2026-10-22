// Execute the actual generated browser handlers with deterministic deferred HTTP.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const listeners = {}, requests = [], canvases = [];
const img = {style:{}, src:'', focus(){}, setPointerCapture(){},
  getBoundingClientRect(){return {left:10, top:20, width:640, height:480}}};
const elements = {screen:img, state:{}, hint:{}, pause:{}, step:{}, reset:{}, shot:{}, record:{}, speed:{}};
const context = {console, Set, Map, Promise, URL,
  document:{querySelector(selector){return elements[selector.slice(1)]}, createElement(tag){
    if(tag==='canvas') {const canvas={getContext(){return {drawImage(...args){canvas.draw=args}}},toDataURL(){return 'data:mock'}};canvases.push(canvas);return canvas}
    return {click(){}};
  }},
  addEventListener(name,callback){listeners[name]=callback}, requestAnimationFrame(){},
  Audio:function(){this.play=()=>Promise.resolve()},
  fetch(path,options){return new Promise(resolve=>requests.push({path, body:JSON.parse(options.body), resolve}))},
  ...elements};
vm.createContext(context);
vm.runInContext(fs.readFileSync(process.argv[2], 'utf8'), context);
const flush = () => new Promise(resolve=>setImmediate(resolve));
const event = key => ({key,preventDefault(){}});
(async()=>{
  listeners.keydown(event('a'));
  listeners.keyup(event('a'));
  await flush();
  assert.equal(requests.length,1, 'only one POST may be in flight');
  assert.equal(requests[0].body.left,true, 'snapshot must be captured before keyup');
  requests[0].resolve({ok:true});await flush();
  assert.equal(requests.length,2);assert.equal(requests[1].body.left,false);
  requests[1].resolve({ok:true});await flush();
  img.onpointerdown({pointerId:7,clientX:330,clientY:260,preventDefault(){}});
  listeners.keydown(event('f'));
  listeners.blur();
  await flush();
  assert.equal(requests[2].body.pointers[0].x,160);
  assert.equal(requests[2].body.pointers[0].y,120);
  requests[2].resolve({ok:true});await flush();
  assert.equal(requests[3].body.fire,true);
  requests[3].resolve({ok:true});await flush();
  assert.equal(requests[4].body.fire,false);
  assert.equal(requests[4].body.pointers.length,0,'blur sends an actual release');
  requests[4].resolve({ok:true});await flush();
  elements.shot.onclick();
  assert.equal(canvases[0].width,320);assert.equal(canvases[0].height,240);
  assert.deepEqual(canvases[0].draw.slice(1),[0,0,320,240]);
  assert.equal(img.style.aspectRatio,'320/240');
})().catch(error=>{console.error(error);process.exitCode=1});
