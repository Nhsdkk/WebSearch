const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const html = fs.readFileSync(process.argv[2], 'utf8');
const script = html.split('<script>')[1].split('</script>')[0];
assert.match(html.split('id="search-form"')[1].split('</form>')[0], /id="word-forms"/);

class Element {
  constructor() {
    Object.assign(this, {children:[], listeners:{}, attributes:{}, hidden:false, disabled:false, checked:false,
      value:'', textContent:'', files:[], classList:{toggle(){}}});
  }
  append(...items) { this.children.push(...items); }
  replaceChildren(...items) { this.children = items; }
  addEventListener(event, callback) { this.listeners[event] = callback; }
  querySelectorAll() { return []; }
  setAttribute(key, value) { this.attributes[key] = value; }
  removeAttribute(key) { delete this.attributes[key]; if (key === 'href') delete this.href; }
  get valueAsNumber() { return Number(this.value); }
}
const event = {preventDefault(){}};
const tick = () => new Promise((done) => setImmediate(done));
const reply = (body, status=200) => ({ok:status < 400, status, json:async()=>body});
const deferred = () => {
  let resolve;
  const promise = new Promise((done) => {resolve=done;});
  return {promise, resolve};
};
const stats = {index_name:'new.bin', documents:3, terms:4, stem_terms:3, tokens:7, index_bytes:500,
  text_bytes:100, average_term_characters:4, average_token_characters:3, load_ms:1,
  total_seconds:1, ms_per_document:1, ms_per_kib:1, documents_per_second:1, kib_per_second:1};
const model = {c:12,q:0,s:1,log10_rmse:0};
const distribution = {available:true,index_name:'new.bin',terms:3,tokens:22,zipf:model,mandelbrot:model,
  points:[{rank:1,frequency:12,zipf:12,mandelbrot:12},{rank:2,frequency:6,zipf:6,mandelbrot:6},
          {rank:3,frequency:4,zipf:4,mandelbrot:4}]};
function harness(search, fetcher) {
  const elements = new Map();
  const el = (id) => {
    if (!elements.has(id)) elements.set(id,new Element());
    return elements.get(id);
  };
  el('word-forms').checked = true;
  el('benchmark-kind').value = 'search';
  el('benchmark-mode').value = 'stem';
  const location = {search};
  const change = (_state,_unused,url) => {location.search = new URL(url,'http://localhost').search;};
  const context = vm.createContext({URL, URLSearchParams, AbortController, location,
    history:{pushState:change,replaceState:change}, window:{addEventListener(){}},
    document:{getElementById:el,createElement:()=>new Element(),createElementNS:()=>new Element()},
    fetch:async(path,options={}) => {
      const custom = fetcher?.(path,options);
      if (custom !== undefined) return custom;
      if (path === '/api/stats') return reply(stats);
      if (path === '/api/benchmark' && options.method !== 'POST') {
        return reply({queries:['машина'],repetitions:10,max_repetitions:100});
      }
      if (path === '/api/benchmark/tokenizer' && options.method !== 'POST') {
        return reply({corpus:'demo.tsv',repetitions:3,max_repetitions:10});
      }
      if (path === '/api/benchmark/quality' && options.method !== 'POST') {
        return reply({available:true,index_name:'new.bin',query_count:16,repetitions:3,max_repetitions:100});
      }
      if (path === '/api/analysis/zipf') return reply(distribution);
      throw Error(path);
    }});
  vm.runInContext(script,context);
  return {el,location,context};
}

(async()=>{
  const search = harness('?q=машина&page=1&mode=exact',(path)=>{
    if (!path.startsWith('/api/search?')) return;
    const params = new URL('http://localhost'+path).searchParams;
    return reply({query:params.get('q'),mode:params.get('mode'),page:1,total_pages:2,total:55,elapsed_ms:1,results:[]});
  });
  await tick();
  assert.equal(search.el('word-forms').checked,false);
  assert.equal(new URL(search.el('next').href,'http://localhost').searchParams.get('mode'),'exact');
  let prevented = false;
  search.el('next').onclick({button:0,ctrlKey:true,preventDefault(){prevented=true;}});
  assert.equal(prevented,false);
  search.el('word-forms').checked=true;
  search.el('word-forms').listeners.change();
  await tick();
  assert.equal(new URLSearchParams(search.location.search).get('mode'),'stem');
  assert.equal(new URLSearchParams(search.location.search).get('page'),'1');
  console.log('PASS modes persist in search, history and real pagination links');

  const benchmark = harness('?view=benchmark',(path,options)=>{
    if (options.method !== 'POST') return;
    if (path === '/api/benchmark/tokenizer') {
      assert.deepEqual(JSON.parse(options.body),{repetitions:3});
      const row={percentage:100,documents:3,text_bytes:1024,tokens:7,average_token_characters:3,
        average_token_bytes:4,tokenization_mean_ms:2,total_mean_ms:3,ms_per_kib:2,kib_per_second:500};
      return reply({corpus:'demo.tsv',repetitions:3,samples:[row,row,row]});
    }
    if (path === '/api/benchmark/quality') {
      const result={total:3,p_at20:0.1,pool_recall_at20:1,mean_ms:1};
      return reply({index_name:'new.bin',queries:[{query:'машина',intent:'Vehicles',exact:result,stem:result}],
        macro:{exact_p_at20:0.1,stem_p_at20:0.1}});
    }
  });
  await tick();
  benchmark.el('benchmark-kind').value='tokenizer';
  await benchmark.el('benchmark-kind').listeners.change();
  assert.equal(benchmark.el('benchmark-queries').required,false);
  await benchmark.el('benchmark-form').listeners.submit(event);
  assert.equal(benchmark.el('measurements').children.length,3);
  assert.equal(benchmark.el('measurement-columns').children[0].children.length,10);
  benchmark.el('benchmark-kind').value='quality';
  await benchmark.el('benchmark-kind').listeners.change();
  assert.match(benchmark.el('benchmark-source').textContent,/16/);
  await benchmark.el('benchmark-form').listeners.submit(event);
  assert.equal(benchmark.el('measurements').children.length,1);
  assert.match(benchmark.el('benchmark-status').textContent,/размеченному пулу/);
  console.log('PASS tokenizer and quality controls load settings and render distinct result tables');

  const oldStats=deferred(), oldSearch=deferred(), oldBenchmark=deferred();
  let fail=false;
  const upload=harness('?view=benchmark',(path,options)=>{
    if(path==='/api/stats')return oldStats.promise;
    if(path.startsWith('/api/search?'))return oldSearch.promise;
    if(path==='/api/benchmark'&&options.method==='POST')return oldBenchmark.promise;
    if(path.startsWith('/api/index/load?'))return fail?reply({error:'Invalid BIDX'},400):reply(stats);
  });
  await tick();
  upload.el('query').value='old';upload.el('search-form').listeners.submit(event);
  const running=upload.el('benchmark-form').listeners.submit(event);
  upload.el('index-file').files=[{name:'new.bin'}];
  await upload.el('index-form').listeners.submit(event);
  oldStats.resolve(reply({...stats,index_name:'old.bin'}));
  oldSearch.resolve(reply({results:[{title:'old',url:'https://old.test'}]}));
  oldBenchmark.resolve(reply({queries:[]}));
  await running;await tick();
  assert.equal(upload.el('stats').children.length,14);
  assert.equal(upload.el('results').children.length,0);
  assert.equal(upload.el('measurements').children.length,0);
  assert.match(upload.el('active-index').textContent,/new.bin/);
  assert.equal(upload.el('pagination').hidden,true);
  upload.el('results').append(new Element());fail=true;
  await upload.el('index-form').listeners.submit(event);
  assert.equal(upload.el('results').children.length,1);
  console.log('PASS replacement resets results and ignores old responses; failed uploads preserve state');

  const oldChart=deferred();let calls=0;
  const analysis=harness('?view=analysis',(path)=>{
    if(path==='/api/analysis/zipf'&&calls++===0)return oldChart.promise;
    if(path.startsWith('/api/index/load?'))return reply(stats);
  });
  await tick();analysis.el('index-file').files=[{name:'new.bin'}];
  await analysis.el('index-form').listeners.submit(event);await tick();
  oldChart.resolve(reply({...distribution,index_name:'old.bin'}));await tick();
  assert.match(analysis.el('analysis-status').textContent,/new.bin/);
  assert.equal(analysis.el('analysis-results').hidden,false);
  assert.ok(analysis.el('zipf-chart').children.length>10);
  assert.match(analysis.el('zipf-parameters').textContent,/RMSE/);
  console.log('PASS logarithmic chart refreshes with its snapshot and ignores a stale chart response');
})().catch((error)=>{console.error(error);process.exitCode=1;});
