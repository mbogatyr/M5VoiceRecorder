#pragma once

#include <Arduino.h>

// The page served at http://192.168.4.1 in Wi-Fi mode. Self-contained (the
// access point has no internet): no external fonts, icons or scripts.
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
:root{--bg:#f5f5f4;--card:#fff;--line:rgba(0,0,0,.1);--text:#1c1c1e;--muted:#6e6e73;--track:#e8e8ea;--fill:#8e8e93;--red:#d70015;--redbg:#fde8e8;--btn:rgba(0,0,0,.04)}
@media (prefers-color-scheme:dark){:root{--bg:#1c1c1e;--card:#2c2c2e;--line:rgba(255,255,255,.12);--text:#f2f2f7;--muted:#98989f;--track:#3a3a3c;--fill:#98989f;--red:#ff453a;--redbg:#3b1f1f;--btn:rgba(255,255,255,.06)}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
main{max-width:640px;margin:0 auto;padding:28px 16px 40px}
.head{display:flex;align-items:center;gap:12px}
.logo{width:40px;height:40px;border-radius:50%;background:var(--redbg);color:var(--red);display:flex;align-items:center;justify-content:center;flex:none}
h1{margin:0;font-size:20px;font-weight:600}
.sub{margin:0;font-size:13px;color:var(--muted)}
.bar{height:6px;background:var(--track);border-radius:3px;margin:18px 0 20px;overflow:hidden}
.bar div{height:100%;background:var(--fill);border-radius:3px;width:0}
.list{background:var(--card);border:1px solid var(--line);border-radius:12px;overflow:hidden}
.row{display:flex;align-items:center;gap:12px;padding:12px 14px}
.row+.row{border-top:1px solid var(--line)}
.meta{flex:1;min-width:0}
.name{margin:0;font-weight:600;font-size:15px}
.info{margin:0;font-size:13px;color:var(--muted);white-space:nowrap}
button,a.btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;height:34px;padding:0 12px;border-radius:8px;border:1px solid var(--line);background:var(--btn);color:var(--text);font:inherit;font-size:14px;text-decoration:none;cursor:pointer}
button:hover,a.btn:hover{border-color:var(--muted)}
.round{width:36px;height:36px;padding:0;border-radius:50%;flex:none}
.del{width:34px;padding:0;color:var(--red)}
.empty{padding:28px 16px;text-align:center;color:var(--muted)}
.note{font-size:12px;color:var(--muted);margin:14px 2px 0}
svg{width:18px;height:18px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
@media (max-width:480px){a.btn{width:34px;padding:0}a.btn span{display:none}}
</style>
</head>
<body>
<main>
<div class="head">
  <div class="logo"><svg viewBox="0 0 24 24"><rect x="9" y="3" width="6" height="11" rx="3"/><path d="M5 11a7 7 0 0 0 14 0M12 18v3"/></svg></div>
  <div><h1>Voice Recorder</h1><p class="sub" id="sub">Loading</p></div>
</div>
<div class="bar"><div id="used"></div></div>
<div class="list" id="list"></div>
<p class="note">IMA ADPCM WAV, 8 kHz mono. Play decodes in the browser; the downloaded file opens in VLC, QuickTime or Windows Media Player.</p>
</main>
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
const player=new Audio();
let playing=null,playBtn=null;

async function load(){
  try{
    const d=await (await fetch('/api/recordings',{cache:'no-store'})).json();
    const n=d.recordings.length;
    $('#sub').textContent=`${n} recording${n==1?'':'s'} · ${mb(d.free)} of ${mb(d.total)} free · about ${Math.floor(d.secondsLeft/60)} min left`;
    $('#used').style.width=(d.total?Math.round((d.total-d.free)/d.total*100):0)+'%';
    const list=$('#list');
    list.innerHTML='';
    if(!n){list.innerHTML='<div class="empty">No recordings yet. Press the blue button on the recorder to make one.</div>';return}
    for(const r of d.recordings){
      const row=document.createElement('div');
      row.className='row';
      row.innerHTML=`<button class="round" aria-label="Play">${ICON.play}</button>
        <div class="meta"><p class="name">${r.name}</p><p class="info">${dur(r.seconds)} · ${mb(r.bytes)}</p></div>
        <a class="btn" href="/rec/${r.name}" download aria-label="Download">${ICON.down}<span>Download</span></a>
        <button class="del" aria-label="Delete">${ICON.trash}</button>`;
      const [play,del]=row.querySelectorAll('button');
      play.onclick=()=>toggle(r.name,play);
      del.onclick=()=>remove(r.name);
      list.appendChild(row);
    }
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
  load();
}

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

document.addEventListener('visibilitychange',()=>{if(!document.hidden)load()});
load();
</script>
</body>
</html>
)HTML";
