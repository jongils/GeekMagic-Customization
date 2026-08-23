#pragma once
#include <pgmspace.h>

static const char CRAB_COLOR_PAGE[] PROGMEM = R"RAW(<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>게 색상 설정</title>
<style>
*{box-sizing:border-box}
body{margin:0;padding:20px;font-family:sans-serif;background:#111;color:#ddd;max-width:420px}
h1{font-size:1.3em;margin:0 0 14px;color:#fff}
.card{background:#1c1c1e;border-radius:12px;padding:16px;margin-bottom:12px}
.ct{font-size:.72em;color:#777;text-transform:uppercase;letter-spacing:.08em;margin-bottom:12px}
.tgl{display:flex;align-items:center;gap:12px}
.tgl input{width:44px;height:24px;accent-color:#0af;cursor:pointer}
.tgl span{font-size:.95em}
.row{display:flex;gap:10px;align-items:flex-end}
.col{flex:1}
label{display:block;font-size:.82em;color:#aaa;margin-bottom:5px}
.cp{display:flex;align-items:center;gap:8px}
input[type=color]{width:44px;height:36px;border:none;border-radius:6px;cursor:pointer;background:none;padding:0}
.hex{flex:1;padding:7px 8px;background:#2c2c2e;color:#fff;border:1px solid #3a3a3c;border-radius:7px;font-size:.9em;font-family:monospace}
.srow{margin-bottom:14px}
input[type=range]{width:100%;accent-color:#0af}
.sv{font-size:.8em;color:#0af;text-align:right;font-weight:bold;margin-top:2px}
button{display:block;width:100%;padding:13px;background:#0af;color:#000;font-size:1em;font-weight:700;border:none;border-radius:10px;cursor:pointer;margin-top:4px}
button:active{background:#08c}
#msg{text-align:center;font-size:.85em;margin-top:10px;min-height:1.2em;color:#4f4}
.preview{display:flex;align-items:center;gap:14px;margin-bottom:16px}
.crab-icon{width:56px;height:40px;border-radius:8px;flex-shrink:0}
.temp-info{font-size:.9em;color:#bbb;line-height:1.6}
.temp-info b{color:#fff;font-size:1.1em}
.gradient-bar{height:12px;border-radius:6px;margin-top:6px;margin-bottom:2px}
.bar-labels{display:flex;justify-content:space-between;font-size:.72em;color:#777}
</style>
</head>
<body>
<h1>&#127998; 게 아이콘 온도 색상</h1>
<div class="preview">
<canvas class="crab-icon" id="cv" width="56" height="40"></canvas>
<div class="temp-info">CPU 온도: <b id="tdis">–</b>°C<br>현재 색상: <span id="chex" style="font-family:monospace">#------</span></div>
</div>
<div class="card">
<div class="ct">활성화</div>
<label class="tgl">
<input type="checkbox" id="en">
<span>온도에 따라 색상 변경</span>
</label>
</div>
<div class="card">
<div class="ct">온도 범위 (°C)</div>
<div class="row">
<div class="col">
<label>최저 (냉색)</label>
<input type="range" id="mn" min="20" max="70" oninput="clamp();u('mnv',this.value+'°C')">
<div class="sv" id="mnv"></div>
</div>
<div class="col">
<label>기준 (기본색)</label>
<input type="range" id="md" min="20" max="70" oninput="clamp();u('mdv',this.value+'°C')">
<div class="sv" id="mdv"></div>
</div>
<div class="col">
<label>최고 (열색)</label>
<input type="range" id="mx" min="20" max="70" oninput="clamp();u('mxv',this.value+'°C')">
<div class="sv" id="mxv"></div>
</div>
</div>
<div class="gradient-bar" id="gbar"></div>
<div class="bar-labels"><span id="lb0"></span><span id="lb1"></span><span id="lb2"></span></div>
</div>
<div class="card">
<div class="ct">색상 설정</div>
<div class="row">
<div class="col">
<label>냉색 (최저)</label>
<div class="cp">
<input type="color" id="cc" oninput="oncp('cc','ch')">
<input class="hex" id="ch" maxlength="7" oninput="onhex('ch','cc')" placeholder="#RRGGBB">
</div>
</div>
<div class="col">
<label>기본색 (기준)</label>
<div class="cp">
<input type="color" id="bc" oninput="oncp('bc','bh')">
<input class="hex" id="bh" maxlength="7" oninput="onhex('bh','bc')" placeholder="#RRGGBB">
</div>
</div>
<div class="col">
<label>열색 (최고)</label>
<div class="cp">
<input type="color" id="hc" oninput="oncp('hc','hh')">
<input class="hex" id="hh" maxlength="7" oninput="onhex('hh','hc')" placeholder="#RRGGBB">
</div>
</div>
</div>
</div>
<button onclick="save()">저장</button>
<div id="msg"></div>
<script>
function u(id,v){document.getElementById(id).textContent=v}
function oncp(cpId,hexId){document.getElementById(hexId).value=document.getElementById(cpId).value;updateGrad();}
function onhex(hexId,cpId){var v=document.getElementById(hexId).value;if(/^#[0-9a-fA-F]{6}$/.test(v)){document.getElementById(cpId).value=v;updateGrad();}}
function clamp(){var mn=+document.getElementById('mn').value,md=+document.getElementById('md').value,mx=+document.getElementById('mx').value;if(md<=mn){document.getElementById('md').value=mn+1;md=mn+1;}if(mx<=md){document.getElementById('mx').value=md+1;mx=md+1;}u('mnv',mn+'°C');u('mdv',md+'°C');u('mxv',mx+'°C');updateGrad();}
function rgb565to888(c){var r=((c>>11)&0x1F)<<3,g=((c>>5)&0x3F)<<2,b=(c&0x1F)<<3;return '#'+('0'+r.toString(16)).slice(-2)+('0'+g.toString(16)).slice(-2)+('0'+b.toString(16)).slice(-2);}
function rgb888to565(h){var r=parseInt(h.slice(1,3),16)>>3,g=parseInt(h.slice(3,5),16)>>2,b=parseInt(h.slice(5,7),16)>>3;return(r<<11)|(g<<5)|b;}
function hex2rgb(h){return[parseInt(h.slice(1,3),16),parseInt(h.slice(3,5),16),parseInt(h.slice(5,7),16)];}
function lerp3(a,b,t){return Math.round(a+(b-a)*t);}
function lerpColor(c1,c2,t){var a=hex2rgb(c1),b=hex2rgb(c2);return 'rgb('+lerp3(a[0],b[0],t)+','+lerp3(a[1],b[1],t)+','+lerp3(a[2],b[2],t)+')';}
function curColors(){return{cold:document.getElementById('cc').value,base:document.getElementById('bc').value,hot:document.getElementById('hc').value};}
function updateGrad(){var c=curColors();document.getElementById('gbar').style.background='linear-gradient(to right,'+c.cold+','+c.base+','+c.hot+')';document.getElementById('lb0').textContent=document.getElementById('mn').value+'°C';document.getElementById('lb1').textContent=document.getElementById('md').value+'°C';document.getElementById('lb2').textContent=document.getElementById('mx').value+'°C';}
function drawCrab(col){var cv=document.getElementById('cv'),ctx=cv.getContext('2d'),cx=28,y=22;ctx.clearRect(0,0,56,40);ctx.fillStyle=col;ctx.fillRect(cx-12,y-8,24,16);ctx.fillRect(cx-16,y,4,4);ctx.fillRect(cx+12,y,4,4);ctx.fillStyle='#000';ctx.fillRect(cx-8,y-4,2,4);ctx.fillRect(cx+6,y-4,2,4);ctx.fillStyle=col;ctx.fillRect(cx-10,y+8,2,4);ctx.fillRect(cx-6,y+8,2,4);ctx.fillRect(cx+4,y+8,2,4);ctx.fillRect(cx+8,y+8,2,4);}
function lerpColorTemp(temp,minT,midT,maxT,cold,base,hot){if(temp<=minT)return cold;if(temp>=maxT)return hot;if(temp<midT){var t=(temp-minT)/(midT-minT);return lerpColor(cold,base,t);}var t=(temp-midT)/(maxT-midT);return lerpColor(base,hot,t);}
var _lastTemp=50;
function updatePreview(temp){var c=curColors(),mn=+document.getElementById('mn').value,md=+document.getElementById('md').value,mx=+document.getElementById('mx').value;var col=document.getElementById('en').checked?lerpColorTemp(temp,mn,md,mx,c.cold,c.base,c.hot):c.base;drawCrab(col);document.getElementById('chex').textContent=col.replace('rgb(','').replace(')','');document.getElementById('tdis').textContent=temp.toFixed(1);}
function load(){
  fetch('/crab/status').then(r=>r.json()).then(d=>{
    document.getElementById('en').checked=d.enabled;
    document.getElementById('mn').value=d.minTemp;u('mnv',d.minTemp+'°C');
    document.getElementById('md').value=d.midTemp;u('mdv',d.midTemp+'°C');
    document.getElementById('mx').value=d.maxTemp;u('mxv',d.maxTemp+'°C');
    var cold=rgb565to888(d.coldColor),base=rgb565to888(d.baseColor),hot=rgb565to888(d.hotColor);
    document.getElementById('cc').value=cold;document.getElementById('ch').value=cold;
    document.getElementById('bc').value=base;document.getElementById('bh').value=base;
    document.getElementById('hc').value=hot;document.getElementById('hh').value=hot;
    if(d.tempValid){_lastTemp=d.temp;document.getElementById('tdis').textContent=d.temp.toFixed(1);}
    else{document.getElementById('tdis').textContent='–';}
    updateGrad();if(d.tempValid)updatePreview(d.temp);else{drawCrab(document.getElementById('bc').value);}
  });
}
function save(){
  var p={enabled:document.getElementById('en').checked,minTemp:+document.getElementById('mn').value,midTemp:+document.getElementById('md').value,maxTemp:+document.getElementById('mx').value,coldColor:rgb888to565(document.getElementById('cc').value),baseColor:rgb888to565(document.getElementById('bc').value),hotColor:rgb888to565(document.getElementById('hc').value)};
  fetch('/crab/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(p)}).then(r=>r.json()).then(d=>{document.getElementById('msg').textContent=d.ok?'✓ 저장 완료':'✗ 실패';load();});
}
setInterval(function(){fetch('/crab/status').then(r=>r.json()).then(d=>{if(d.tempValid){_lastTemp=d.temp;document.getElementById('tdis').textContent=d.temp.toFixed(1);updatePreview(d.temp);}});},5000);
load();
</script>
</body>
</html>)RAW";
