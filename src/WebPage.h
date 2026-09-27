#pragma once

#include <Arduino.h>

// The page served at http://192.168.4.1 in Wi-Fi mode. Self-contained (the
// access point has no internet): no external fonts, icons or scripts.
//
// Top: the recorder card (Record, or the live timer, waveform and Stop).
// Below: the recordings, greyed out while recording, and Delete all under
// the end of the list, well away from Record. The page polls /api/status
// every 400 ms while recording and every 2 s otherwise.
//
// Browsers cannot play IMA ADPCM WAV, so Play fetches the file, decodes it
// in JavaScript into 16-bit PCM and plays that. Download hands over the
// original file, which VLC, Windows and macOS play as is.
static const char kWebPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Voice Recorder</title>
<style>
/* Always dark, like the stick's own screen (the user's choice). */
:root{color-scheme:dark;--bg:#1c1c1e;--card:#2c2c2e;--line:rgba(255,255,255,.12);--text:#f2f2f7;--muted:#98989f;--track:#3a3a3c;--fill:#98989f;--red:#ff453a;--redbg:#3b1f1f;--btn:rgba(255,255,255,.06);--rec:#ff453a;--hl:#3a3320}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
main{max-width:640px;margin:0 auto;padding:28px 16px 40px}
.head{display:flex;align-items:center;gap:12px}
.logo{width:40px;height:40px;border-radius:50%;background:var(--redbg);color:var(--red);display:flex;align-items:center;justify-content:center;flex:none}
h1{margin:0;font-size:20px;font-weight:600}
.sub{margin:0;font-size:13px;color:var(--muted)}
.bar{height:6px;background:var(--track);border-radius:3px;margin:18px 0 20px;overflow:hidden}
.bar div{height:100%;background:var(--fill);border-radius:3px;width:0}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px 16px;margin-bottom:10px}
.card.live{border-color:var(--rec)}
.cardrow{display:flex;align-items:center;gap:14px}
.grow{flex:1;min-width:0}
.title{margin:0;font-weight:600;font-size:15px}
.timer{margin:2px 0 0;font-size:32px;font-weight:600;line-height:1.1;font-variant-numeric:tabular-nums}
.info{margin:0;font-size:13px;color:var(--muted);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.card .info{white-space:normal}
.dot{color:var(--rec);animation:pulse 1s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.25}}
.wave{display:flex;align-items:center;gap:2px;height:40px;margin-top:12px}
.wave i{flex:1;min-height:2px;border-radius:1px;background:var(--rec)}
button,a.btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;height:34px;padding:0 12px;border-radius:8px;border:1px solid var(--line);background:var(--btn);color:var(--text);font:inherit;font-size:14px;text-decoration:none;cursor:pointer;white-space:nowrap}
button:hover,a.btn:hover{border-color:var(--muted)}
button:disabled{cursor:default;opacity:.45}
.pill{height:40px;padding:0 20px;border-radius:20px;font-weight:600;flex:none}
.record{background:var(--rec);border-color:var(--rec);color:#fff}
.record:hover{border-color:var(--rec);filter:brightness(1.08)}
.record .ico{width:11px;height:11px;border-radius:50%;background:#fff}
.stop .ico{width:11px;height:11px;border-radius:2px;background:var(--text)}
.saved{margin:0 2px 14px;font-size:13px;color:var(--muted)}
.saved:empty{margin:0}
.saved b{color:var(--text);font-weight:600}
.label{margin:8px 2px 6px;font-size:13px;font-weight:600;color:var(--muted)}
.list{background:var(--card);border:1px solid var(--line);border-radius:12px;overflow:hidden;transition:opacity .2s}
.list.paused{opacity:.45;pointer-events:none}
.row{display:flex;align-items:center;gap:12px;padding:12px 14px;transition:background 1.5s}
.row+.row{border-top:1px solid var(--line)}
.row.new{background:var(--hl)}
.name{margin:0;font-weight:600;font-size:15px}
.round{width:36px;height:36px;padding:0;border-radius:50%;flex:none}
.del{width:34px;padding:0;color:var(--red)}
.empty{padding:28px 16px;text-align:center;color:var(--muted)}
.note{font-size:12px;color:var(--muted);margin:8px 2px 0}
.foot{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-top:12px}
.foot .note{margin:0}
.dall{color:var(--red);border-color:var(--red);background:transparent;font-size:13px;height:32px}
.modal{position:fixed;inset:0;background:rgba(0,0,0,.45);display:none;align-items:center;justify-content:center;padding:16px}
.modal.open{display:flex}
.dialog{background:var(--card);border-radius:14px;padding:20px 22px;max-width:360px;width:100%;box-shadow:0 10px 40px rgba(0,0,0,.3)}
.dialog h2{margin:0 0 8px;font-size:17px}
.dialog p{margin:0 0 18px;font-size:14px;color:var(--muted)}
.actions{display:flex;justify-content:flex-end;gap:8px}
.danger{background:var(--rec);border-color:var(--rec);color:#fff;font-weight:600}
svg{width:18px;height:18px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
@media (max-width:480px){a.btn{width:34px;padding:0}a.btn span{display:none}.timer{font-size:28px}}
</style>
</head>
<body>
<main>
<div class="head">
  <div class="logo"><svg viewBox="0 0 24 24"><rect x="9" y="3" width="6" height="11" rx="3"/><path d="M5 11a7 7 0 0 0 14 0M12 18v3"/></svg></div>
  <div><h1>Voice Recorder</h1><p class="sub" id="sub">Loading</p></div>
</div>
<div class="bar"><div id="used"></div></div>
<div class="card" id="card"></div>
<p class="saved" id="saved"></p>
<p class="label">Recordings</p>
<div class="list" id="list"></div>
<p class="note" id="paused" hidden>Play, download and delete are paused while recording, so the recording has no gaps.</p>
<div class="foot">
  <p class="note">IMA ADPCM WAV, 8 kHz mono. Downloads open in VLC, QuickTime or Windows Media Player.</p>
  <button class="dall" id="dall" hidden></button>
</div>
</main>
<div class="modal" id="modal" role="dialog" aria-modal="true" aria-labelledby="mtitle">
  <div class="dialog">
    <h2 id="mtitle"></h2>
    <p id="mtext"></p>
    <div class="actions"><button id="mcancel">Cancel</button><button class="danger" id="mok">Delete all</button></div>
  </div>
</div>
<script>
const ICON={
 play:'<svg viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>',
 pause:'<svg viewBox="0 0 24 24"><path d="M8 5v14M16 5v14"/></svg>',
 down:'<svg viewBox="0 0 24 24"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg>',
 trash:'<svg viewBox="0 0 24 24"><path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3"/></svg>'};
const $=s=>document.querySelector(s);
const pad=n=>String(n).padStart(2,'0');
const dur=s=>s>=3600?`${Math.floor(s/3600)}:${pad(Math.floor(s/60)%60)}:${pad(s%60)}`:`${pad(Math.floor(s/60))}:${pad(s%60)}`;
const mb=b=>(b/1e6).toFixed(b<1e7?2:1)+' MB';
const plural=(n,w)=>`${n} ${w}${n==1?'':'s'}`;
const player=new Audio();
let playing=null,playBtn=null,status=null,recs=[],timer=null,cardState='';

// ---- recorder card ----
function renderCard(){
  const card=$('#card');
  const rec=status&&status.state=='recording';
  const key=!status?'':rec?'rec':status.canRecord?'ready':'full';
  if(key!=cardState){
    cardState=key;
    card.classList.toggle('live',rec);
    if(key=='rec'){
      card.innerHTML=`<div class="cardrow"><div class="grow">
        <p class="info"><span class="dot">&#9679;</span> Recording &middot; <span id="rname"></span></p>
        <p class="timer" id="rtime">00:00</p><p class="info" id="rinfo"></p></div>
        <button class="pill stop" id="stop"><span class="ico"></span>Stop</button></div>
        <div class="wave" id="wave">${'<i></i>'.repeat(54)}</div>`;
      $('#stop').onclick=stop;
    }else if(key=='ready'||key=='full'){
      const full=key=='full';
      card.innerHTML=`<div class="cardrow"><div class="grow">
        <p class="title">${full?'Memory full':'Ready to record'}</p>
        <p class="info">${full?'Delete recordings to record again.':'The recording is saved on the stick. The stick shows it too.'}</p></div>
        <button class="pill record" id="record" ${full?'disabled':''}><span class="ico"></span>Record</button></div>`;
      $('#record').onclick=record;
    }else{
      card.innerHTML='<p class="info">Connecting to the recorder</p>';
    }
  }
  if(rec){
    $('#rname').textContent=status.name;
    $('#rtime').textContent=dur(status.seconds);
    $('#rinfo').textContent=`${mb(status.bytes)} · ${dur(status.secondsLeft)} left`;
    const bars=$('#wave').children,lv=status.levels||'';
    for(let i=0;i<bars.length;i++){
      const v=parseInt(lv.substr(i*2,2),16)||0;
      bars[i].style.height=Math.max(2,Math.round(v/255*40))+'px';
      bars[i].style.opacity=(.35+.65*i/(bars.length-1)).toFixed(2);
    }
  }
  const paused=rec;
  $('#list').classList.toggle('paused',paused);
  $('#paused').hidden=!paused;
  $('#dall').disabled=paused;
}

async function post(url){
  const r=await fetch(url,{method:'POST'});
  if(!r.ok)throw new Error(await r.text());
  return r.json();
}

async function record(){
  $('#record').disabled=true;
  try{setStatus(await post('/api/record'))}
  catch(e){cardState='';renderCard();$('#saved').textContent=e.message}
}

async function stop(){
  $('#stop').disabled=true;
  try{setStatus(await post('/api/stop'))}catch(e){$('#saved').textContent=e.message}
}

function setStatus(next){
  const was=status&&status.state=='recording';
  status=next;
  if(status.state=='recording'){
    if(!was){player.pause();setIcon(playBtn,'play');playing=null;$('#saved').textContent=''}
  }else if(was){
    const s=$('#saved');
    s.innerHTML=`Saved <b></b> · ${dur(status.seconds)} · ${mb(status.bytes)}${status.stoppedFull?' · memory full':''}`;
    s.querySelector('b').textContent=status.name;
    load(status.name);
  }
  renderCard();
  schedule();
}

function schedule(){
  clearTimeout(timer);
  timer=setTimeout(poll,status&&status.state=='recording'?400:2000);
}

async function poll(){
  try{
    setStatus(await (await fetch('/api/status',{cache:'no-store'})).json());
  }catch(e){
    $('#sub').textContent='The recorder does not answer. Is Wi-Fi mode still on?';
    schedule();
  }
}

// ---- recordings ----
async function load(highlight){
  try{
    const d=await (await fetch('/api/recordings',{cache:'no-store'})).json();
    recs=d.recordings;
    const n=recs.length;
    $('#sub').textContent=`${plural(n,'recording')} · ${mb(d.free)} of ${mb(d.total)} free · about ${Math.floor(d.secondsLeft/60)} min left`;
    $('#used').style.width=(d.total?Math.round((d.total-d.free)/d.total*100):0)+'%';
    const list=$('#list');
    list.innerHTML='';
    if(!n){list.innerHTML='<div class="empty">No recordings yet. Press Record to make one.</div>'}
    for(const r of recs){
      const row=document.createElement('div');
      row.className='row'+(r.name==highlight?' new':'');
      row.innerHTML=`<button class="round" aria-label="Play">${ICON.play}</button>
        <div class="grow"><p class="name">${r.name}</p><p class="info">${dur(r.seconds)} · ${mb(r.bytes)}</p></div>
        <a class="btn" href="/rec/${r.name}" download aria-label="Download">${ICON.down}<span>Download</span></a>
        <button class="del" aria-label="Delete">${ICON.trash}</button>`;
      const [play,del]=row.querySelectorAll('button');
      play.onclick=()=>toggle(r.name,play);
      del.onclick=()=>remove(r.name);
      list.appendChild(row);
      if(r.name==highlight)setTimeout(()=>row.classList.remove('new'),2500);
    }
    const all=$('#dall');
    all.hidden=!n;
    all.innerHTML=`${ICON.trash}Delete all ${plural(n,'recording')}`;
  }catch(e){$('#sub').textContent='The recorder does not answer. Is Wi-Fi mode still on?'}
}

function setIcon(btn,name){if(btn){btn.innerHTML=ICON[name];btn.setAttribute('aria-label',name=='play'?'Play':'Pause')}}

async function toggle(name,btn){
  if(playing===name){
    if(player.paused){player.play();setIcon(btn,'pause')}else{player.pause();setIcon(btn,'play')}
    return;
  }
  player.pause();setIcon(playBtn,'play');
  playing=name;playBtn=btn;setIcon(btn,'pause');
  const buf=await (await fetch('/rec/'+name)).arrayBuffer();
  if(playing!==name)return;
  if(player.src)URL.revokeObjectURL(player.src);
  player.src=URL.createObjectURL(toPcmWav(buf));
  player.play();
}
player.onended=()=>{setIcon(playBtn,'play');playing=null};

async function remove(name){
  if(!confirm(`Delete ${name}? This cannot be undone.`))return;
  if(playing===name){player.pause();playing=null}
  await fetch('/rec/'+name,{method:'DELETE'});
  load();poll();
}

// ---- Delete all ----
function openDeleteAll(){
  const n=recs.length,bytes=recs.reduce((a,r)=>a+r.bytes,0);
  const minutes=Math.floor(bytes/256*505/8000/60);
  $('#mtitle').textContent=`Delete all ${plural(n,'recording')}?`;
  $('#mtext').textContent=`This frees ${mb(bytes)}${minutes?` and gives back about ${minutes} min of recording time`:''}. Download anything you want to keep first. This can't be undone.`;
  $('#modal').classList.add('open');
  $('#mcancel').focus();
}
function closeDeleteAll(){$('#modal').classList.remove('open')}
$('#dall').onclick=openDeleteAll;
$('#mcancel').onclick=closeDeleteAll;
$('#modal').onclick=e=>{if(e.target.id=='modal')closeDeleteAll()};
document.addEventListener('keydown',e=>{if(e.key=='Escape')closeDeleteAll()});
$('#mok').onclick=async()=>{
  closeDeleteAll();
  player.pause();playing=null;
  const r=await fetch('/api/recordings',{method:'DELETE'});
  $('#saved').textContent=r.ok?'All recordings deleted.':await r.text();
  load();poll();
};

const STEP=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767];
const IDX=[-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8];

// IMA ADPCM WAV (mono) -> 16-bit PCM WAV blob.
function toPcmWav(buf){
  const v=new DataView(buf);
  let rate=8000,align=256,spb=505,data=0,size=0;
  for(let p=12;p+8<=v.byteLength;){
    const id=String.fromCharCode(v.getUint8(p),v.getUint8(p+1),v.getUint8(p+2),v.getUint8(p+3));
    const len=v.getUint32(p+4,true);
    if(id=='fmt '){rate=v.getUint32(p+12,true);align=v.getUint16(p+20,true);spb=v.getUint16(p+26,true)}
    if(id=='data'){data=p+8;size=Math.min(len,v.byteLength-data);break}
    p+=8+len+(len&1);
  }
  const blocks=Math.floor(size/align);
  const pcm=new Int16Array(blocks*spb);
  let o=0;
  for(let b=0;b<blocks;b++){
    let q=data+b*align,pred=v.getInt16(q,true),idx=v.getUint8(q+2);
    pcm[o++]=pred;q+=4;
    for(let i=1;i<spb;i+=2){
      const byte=v.getUint8(q++);
      for(const code of [byte&15,byte>>4]){
        const step=STEP[idx];
        let d=step>>3;
        if(code&4)d+=step;if(code&2)d+=step>>1;if(code&1)d+=step>>2;
        pred+=code&8?-d:d;
        pred=pred<-32768?-32768:pred>32767?32767:pred;
        idx+=IDX[code];idx=idx<0?0:idx>88?88:idx;
        pcm[o++]=pred;
      }
    }
  }
  const out=new DataView(new ArrayBuffer(44+pcm.length*2));
  const str=(p,s)=>{for(let i=0;i<4;i++)out.setUint8(p+i,s.charCodeAt(i))};
  str(0,'RIFF');out.setUint32(4,36+pcm.length*2,true);str(8,'WAVE');
  str(12,'fmt ');out.setUint32(16,16,true);out.setUint16(20,1,true);out.setUint16(22,1,true);
  out.setUint32(24,rate,true);out.setUint32(28,rate*2,true);out.setUint16(32,2,true);out.setUint16(34,16,true);
  str(36,'data');out.setUint32(40,pcm.length*2,true);
  for(let i=0;i<pcm.length;i++)out.setInt16(44+i*2,pcm[i],true);
  return new Blob([out],{type:'audio/wav'});
}

document.addEventListener('visibilitychange',()=>{if(!document.hidden){load();poll()}});
renderCard();load();poll();
</script>
</body>
</html>
)HTML";
