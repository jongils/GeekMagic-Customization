#pragma once
#include <pgmspace.h>

static const char DIMMER_PAGE[] PROGMEM = R"RAW(<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>절전 설정</title>
<style>
*{box-sizing:border-box}
body{margin:0;padding:20px;font-family:sans-serif;background:#111;color:#ddd;max-width:400px}
h1{font-size:1.3em;margin:0 0 14px;color:#fff}
.card{background:#1c1c1e;border-radius:12px;padding:16px;margin-bottom:12px}
.ct{font-size:.72em;color:#777;text-transform:uppercase;letter-spacing:.08em;margin-bottom:12px}
.row{display:flex;gap:12px}
.col{flex:1}
label{display:block;font-size:.85em;color:#aaa;margin-bottom:6px}
select{width:100%;padding:8px;background:#2c2c2e;color:#fff;border:1px solid #3a3a3c;border-radius:8px;font-size:.95em}
.tgl{display:flex;align-items:center;gap:12px;cursor:pointer}
.tgl input{width:44px;height:24px;accent-color:#0af;cursor:pointer}
.tgl span{font-size:.95em}
.sr{margin-bottom:14px}
input[type=range]{width:100%;accent-color:#0af}
.sv{font-size:.8em;color:#0af;text-align:right;font-weight:bold;margin-top:2px}
button{display:block;width:100%;padding:14px;background:#0af;color:#000;font-size:1em;font-weight:700;border:none;border-radius:10px;cursor:pointer}
button:active{background:#08c}
#msg{text-align:center;font-size:.85em;margin-top:10px;min-height:1.2em;color:#4f4}
.badge{display:inline-block;padding:2px 10px;border-radius:6px;font-size:.8em;font-weight:bold;vertical-align:middle}
.bdim{background:#2a2a2a;color:#888}
.bnorm{background:#0af2;color:#0af}
#cur{font-size:.9em;margin-bottom:14px;color:#bbb}
</style>
</head>
<body>
<h1>&#128161; 절전 모드</h1>
<div id="cur">–</div>
<div class="card">
<div class="ct">활성화</div>
<label class="tgl">
<input type="checkbox" id="en">
<span>시간대 절전 모드 사용</span>
</label>
</div>
<div class="card">
<div class="ct">시간 설정</div>
<div class="row">
<div class="col">
<label>시작 (어두워짐)</label>
<select id="sh"></select>
</div>
<div class="col">
<label>종료 (밝아짐)</label>
<select id="eh"></select>
</div>
</div>
</div>
<div class="card">
<div class="ct">밝기</div>
<div class="sr">
<label>절전 밝기</label>
<input type="range" id="db" min="0" max="255" oninput="u('dv',this.value)">
<div class="sv" id="dv"></div>
</div>
<div class="sr">
<label>일반 밝기</label>
<input type="range" id="nb" min="0" max="255" oninput="u('nv',this.value)">
<div class="sv" id="nv"></div>
</div>
</div>
<button onclick="save()">저장</button>
<div id="msg"></div>
<script>
function u(id,v){document.getElementById(id).textContent=v}
function mksel(id,def){var o='';for(var i=0;i<24;i++)o+='<option value='+i+(i==def?' selected':'')+'>'+('0'+i).slice(-2)+':00</option>';document.getElementById(id).innerHTML=o}
function load(){
  fetch('/dimmer/status').then(r=>r.json()).then(d=>{
    document.getElementById('en').checked=d.enabled;
    mksel('sh',d.startHour);mksel('eh',d.endHour);
    document.getElementById('db').value=d.dimBrt;u('dv',d.dimBrt);
    document.getElementById('nb').value=d.normalBrt;u('nv',d.normalBrt);
    document.getElementById('cur').innerHTML='현재 <b>'+d.hour+'시</b> &nbsp;<span class="badge '+(d.active?'bdim':'bnorm')+'">'+(d.active?'절전 중':'정상')+'</span>';
  });
}
function save(){
  var p={enabled:document.getElementById('en').checked,startHour:+document.getElementById('sh').value,endHour:+document.getElementById('eh').value,dimBrt:+document.getElementById('db').value,normalBrt:+document.getElementById('nb').value};
  fetch('/dimmer/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(p)}).then(r=>r.json()).then(d=>{document.getElementById('msg').textContent=d.ok?'✓ 저장 완료':'✗ 실패';load();});
}
load();
</script>
</body>
</html>)RAW";
