#include "WifiWebServer.h"
#include "InversionConfig.h"

// =====================================================================
//         HTML + CSS + JS интерфейс пульта (поворот на 90 градусов)
// =====================================================================
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#121418">
<title>Brick Box V1</title>
<style>
  *{box-sizing:border-box;user-select:none;-webkit-user-select:none;touch-action:none;}
  html,body{margin:0;padding:0;width:100vw;height:100vh;overflow:hidden;background:#121418;color:#fff;font-family:system-ui,sans-serif;}
  #app{
    position:fixed;
    top:0; left:0;
    width:100vw; height:100vh;
    display:flex;
    flex-direction:column;
    transform-origin:top left;
  }
  @media (orientation: portrait) {
    #app{
      width:100vh;
      height:100vw;
      transform:rotate(90deg) translateY(-100%);
    }
  }
  header{display:flex;justify-content:space-between;align-items:center;padding:8px 16px;background:#1b1f26;border-bottom:1px solid #2c323d;font-size:14px;}
  .status{display:flex;align-items:center;gap:8px;font-weight:600;}
  .hdr-center{display:flex;align-items:center;gap:12px;}
  .dot{width:10px;height:10px;border-radius:50%;background:#e74c3c;}
  .dot.ok{background:#2ecc71;box-shadow:0 0 8px #2ecc71;}
  .btn-top{background:#2c323d;border:none;color:#fff;padding:6px 12px;border-radius:6px;font-size:13px;}
  .btn-top:active{background:#3d4554;}
  .main{flex:1;display:flex;justify-content:space-between;align-items:center;padding:8px 32px 4px 18px;gap:10px;}
  .stick-box{position:relative;width:148px;height:200px;background:#1b1f26;border:2px solid #2f3642;border-radius:24px;display:flex;align-items:center;justify-content:center;flex-shrink:0;}
  .stick-box.horiz{width:195px;height:150px;margin-right:10px;}
  .knob{position:absolute;width:64px;height:64px;border-radius:50%;background:radial-gradient(circle at 30% 30%,#4da3ff,#1d6ed8);box-shadow:0 4px 12px rgba(0,0,0,0.5);pointer-events:none;will-change:transform;}
  .knob.steer{background:radial-gradient(circle at 30% 30%,#ff9f43,#ee5253);}
  .knob.m5mode{background:radial-gradient(circle at 30% 30%,#a55eea,#8854d0);}
  .label{position:absolute;bottom:6px;font-size:11px;color:#8892a0;pointer-events:none;}
  .center-panel{display:flex;flex-direction:column;gap:6px;align-items:center;}
  .row{display:flex;gap:8px;}
  .act-btn{width:64px;height:42px;border-radius:10px;border:1px solid #384150;background:#232832;color:#fff;font-weight:700;font-size:13px;}
  .act-btn:active,.act-btn.active{background:#3498db;border-color:#5dade2;}
  .act-btn.dimmed{opacity:0.35;}
  .horn-btn{width:136px;height:36px;border-radius:10px;border:1px solid #d68910;background:#b9770e;color:#fff;font-weight:700;font-size:13px;}
  .horn-btn:active{background:#f39c12;}
  .stop-btn{width:136px;height:38px;border-radius:10px;border:none;background:#c0392b;color:#fff;font-weight:700;font-size:13px;}
  footer{text-align:center;font-size:10px;color:#57606f;padding:2px 0 4px 0;letter-spacing:0.5px;}
  #cfg{display:none;position:absolute;inset:44px 0 0 0;background:rgba(18,20,24,0.97);padding:14px;overflow-y:auto;z-index:10;touch-action:auto;}
  .cfg-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;max-width:460px;margin:0 auto;touch-action:auto;}
  .chk{display:flex;align-items:center;gap:10px;background:#1b1f26;padding:10px 12px;border-radius:10px;font-size:13px;touch-action:auto;}
  .chk.wide{grid-column:span 2;background:#222731;border:1px solid #343d4d;}
  input[type=checkbox]{width:20px;height:20px;touch-action:auto;}
</style>
</head>
<body>
<div id="app">
  <header>
    <div class="status"><span class="dot" id="dot"></span><span id="st">Подключение...</span></div>
    <div class="hdr-center">
      <div id="vals" style="font-family:monospace;color:#9aa4b2;">T:0 S:0</div>
      <button class="btn-top" onclick="toggleCfg()">⚙ Настройки</button>
    </div>
    <button class="btn-top" onclick="toggleFS()" title="На весь экран">⛶</button>
  </header>

  <div class="main">
    <div class="stick-box" id="stickT">
      <div class="knob" id="knobT"></div>
      <div class="label">ГАЗ / НАЗАД</div>
    </div>

    <div class="center-panel">
      <div class="row">
        <button class="act-btn" data-cmd="A100" data-rel="A0">M3 +</button>
        <button class="act-btn" data-cmd="B100" data-rel="B0">M4 +</button>
      </div>
      <div class="row">
        <button class="act-btn" data-cmd="A-100" data-rel="A0">M3 -</button>
        <button class="act-btn" data-cmd="B-100" data-rel="B0">M4 -</button>
      </div>
      <div class="row">
        <button class="act-btn m5-btn" data-cmd="P-100" data-rel="P0">↺ M5</button>
        <button class="act-btn m5-btn" data-cmd="P100" data-rel="P0">M5 ↻</button>
      </div>
      <button class="horn-btn" onclick="send('H')">🔊 БИП-БИП</button>
      <button class="stop-btn" onclick="send('R')">СТОП (R)</button>
    </div>

    <div class="stick-box horiz" id="stickS">
      <div class="knob steer" id="knobS"></div>
      <div class="label" id="lblSteer">РУЛЬ (SERVO)</div>
    </div>
  </div>

  <footer>Brick Box V1 • ESP32-S3 5CH + 2 Servo RC Controller</footer>

  <div id="cfg">
    <div class="cfg-grid">
      <label class="chk wide"><input type="checkbox" id="chkSwap" onchange="toggleStickMode(this.checked)">Правый стик → M5 (вместо Серво)</label>
      <label class="chk"><input type="checkbox" onchange="send('I1'+(this.checked?1:0))">Инверсия M1</label>
      <label class="chk"><input type="checkbox" onchange="send('I2'+(this.checked?1:0))">Инверсия M2</label>
      <label class="chk"><input type="checkbox" onchange="send('I3'+(this.checked?1:0))">Инверсия M3</label>
      <label class="chk"><input type="checkbox" onchange="send('I4'+(this.checked?1:0))">Инверсия M4</label>
      <label class="chk"><input type="checkbox" onchange="send('I5'+(this.checked?1:0))">Инверсия M5</label>
      <label class="chk"><input type="checkbox" onchange="send('Ig'+(this.checked?1:0))">Инверсия Серво 1</label>
      <label class="chk"><input type="checkbox" onchange="send('IG'+(this.checked?1:0))">Инверсия Серво 2</label>
      <label class="chk"><input type="checkbox" onchange="send('D'+(this.checked?1:0))">Режим ALT (4x4)</label>
    </div>
  </div>
</div>

<script>
let ws, tVal=0, sVal=0, lastSentT=0, lastSentS=0, lastSendTime=0;
const dot=document.getElementById('dot'), st=document.getElementById('st'), vals=document.getElementById('vals');

function isPortrait(){
  return window.innerHeight > window.innerWidth;
}

function toggleFS(){
  const el = document.documentElement;
  if(!document.fullscreenElement && !document.webkitFullscreenElement){
    if(el.requestFullscreen) el.requestFullscreen().catch(()=>{});
    else if(el.webkitRequestFullscreen) el.webkitRequestFullscreen();
  } else {
    if(document.exitFullscreen) document.exitFullscreen().catch(()=>{});
    else if(document.webkitExitFullscreen) document.webkitExitFullscreen();
  }
}

function toggleStickMode(isM5){
  send('Is' + (isM5 ? '1' : '0'));
  const lbl = document.getElementById('lblSteer');
  const knob = document.getElementById('knobS');
  lbl.textContent = isM5 ? 'M5 (ПОВОРОТ)' : 'РУЛЬ (SERVO)';
  if(isM5) knob.classList.add('m5mode');
  else     knob.classList.remove('m5mode');
  document.querySelectorAll('.m5-btn').forEach(b=>{
    if(isM5) b.classList.add('dimmed');
    else     b.classList.remove('dimmed');
  });
}

function connect(){
  ws = new WebSocket('ws://'+location.hostname+':81/');
  ws.onopen = ()=>{ dot.classList.add('ok'); st.textContent='Brick Box V1'; };
  ws.onclose = ()=>{ dot.classList.remove('ok'); st.textContent='Переподключение...'; setTimeout(connect,1000); };
}
connect();

function send(msg){
  if(ws && ws.readyState===1 && ws.bufferedAmount < 64){
    ws.send(msg);
    lastSendTime = performance.now();
  }
}

function syncJoy(force){
  const now = performance.now();
  if(!force && tVal===lastSentT && sVal===lastSentS) return;
  if(!force && (now - lastSendTime) < 35) return;
  lastSentT = tVal;
  lastSentS = sVal;
  send('J'+tVal+','+sVal);
  vals.textContent = 'T:'+tVal+' S:'+sVal;
}

setInterval(()=>{
  if(performance.now() - lastSendTime > 350){
    send('J'+tVal+','+sVal);
  }
}, 400);

function bindStick(boxId, knobId, axis){
  const box=document.getElementById(boxId), knob=document.getElementById(knobId);
  let touchId=null;
  const maxD = 58;

  function update(clientX, clientY){
    const r = box.getBoundingClientRect();
    const cx = r.left + r.width/2, cy = r.top + r.height/2;
    let rawX = clientX - cx, rawY = clientY - cy;
    let dx = rawX, dy = rawY;
    if(isPortrait()){
      dx = rawY;
      dy = -rawX;
    }
    if(axis==='y'){
      dy = Math.max(-maxD, Math.min(maxD, dy));
      knob.style.transform = `translate3d(0px, ${dy}px, 0)`;
      tVal = Math.round((-dy / maxD) * 100);
    } else {
      dx = Math.max(-maxD, Math.min(maxD, dx));
      knob.style.transform = `translate3d(${dx}px, 0px, 0)`;
      sVal = Math.round((dx / maxD) * 100);
    }
    syncJoy(false);
  }

  function reset(){
    touchId = null;
    knob.style.transform = 'translate3d(0px, 0px, 0)';
    if(axis==='y') tVal = 0;
    else           sVal = 0;
    syncJoy(true);
  }

  box.addEventListener('touchstart', e=>{
    e.preventDefault();
    if(touchId===null){
      const t=e.changedTouches[0];
      touchId=t.identifier;
      update(t.clientX, t.clientY);
    }
  });
  box.addEventListener('touchmove', e=>{
    e.preventDefault();
    for(let t of e.changedTouches) if(t.identifier===touchId) update(t.clientX, t.clientY);
  });
  box.addEventListener('touchend', e=>{
    for(let t of e.changedTouches) if(t.identifier===touchId) reset();
  });
  box.addEventListener('touchcancel', reset);
}

bindStick('stickT','knobT','y');
bindStick('stickS','knobS','x');

document.querySelectorAll('.act-btn').forEach(b=>{
  const press = e=>{ e.preventDefault(); b.classList.add('active'); send(b.dataset.cmd); };
  const rel   = e=>{ e.preventDefault(); b.classList.remove('active'); send(b.dataset.rel); };
  b.addEventListener('touchstart', press);
  b.addEventListener('touchend', rel);
  b.addEventListener('mousedown', press);
  b.addEventListener('mouseup', rel);
});

function toggleCfg(){
  const c=document.getElementById('cfg');
  c.style.display = (c.style.display==='block') ? 'none' : 'block';
}
</script>
</body>
</html>
)rawliteral";

// =====================================================================
//                    Инициализация Wi-Fi + WebServer + WS
// =====================================================================

// Поднимает точку доступа Wi-Fi, настраивает перехват Captive Portal (DNS и системные URL),
// запускает HTTP-сервер на 80 порту и WebSocket-сервер на 81 порту.
void WifiWebServer::begin(const char* ssid, const char* password) {
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.softAP(ssid, password);
    delay(100);

    IPAddress apIP = WiFi.softAPIP();
    dns_.start(53, "*", apIP);

    // Главная страница пульта
    http_.on("/", [this]() {
        http_.send_P(200, "text/html", INDEX_HTML);
    });

    // Обработчики системных проверок подключения к интернету (Android / iOS / Windows),
    // чтобы смартфон не ругался на отсутствие сети и не разрывал Wi-Fi соединение.
    http_.on("/generate_204", [this]() {
        http_.send(204, "text/plain", "");
    });
    http_.on("/gen_204", [this]() {
        http_.send(204, "text/plain", "");
    });
    http_.on("/hotspot-detect.html", [this]() {
        http_.send_P(200, "text/html", INDEX_HTML);
    });
    http_.on("/connecttest.txt", [this]() {
        http_.send(200, "text/plain", "Microsoft Connect Test");
    });

    // Перенаправление любых других адресов на главную страницу пульта
    http_.onNotFound([this]() {
        http_.sendHeader("Location", "http://192.168.4.1/", true);
        http_.send(302, "text/plain", "");
    });
    http_.begin();

    // Запуск WebSocket для обмена командами в реальном времени
    ws_.begin();
    ws_.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        this->onWsEvent(num, type, payload, length);
    });
}

// Основной цикл обслуживания сетевых клиентов (вызывается на каждой итерации loopPhone)
void WifiWebServer::loop() {
    ws_.loop();
    if (clientCount_ == 0) {
        dns_.processNextRequest();
    }
    http_.handleClient();
}

// Обработчик событий WebSocket: отслеживает число подключённых устройств,
// сбрасывает управление при обрыве связи и передаёт входящие строки в парсер handleLine.
void WifiWebServer::onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    (void)num;
    switch (type) {
        case WStype_CONNECTED:
            clientCount_++;
            state.lastUpdate = millis();
            break;

        case WStype_DISCONNECTED:
            if (clientCount_ > 0) clientCount_--;
            if (clientCount_ == 0) {
                state.throttle = state.steer = state.aux1 = state.aux2 = state.spotTurn = 0;
                state.horn = false;
            }
            break;

        case WStype_TEXT: {
            String msg;
            msg.reserve(length);
            for (size_t i = 0; i < length; ++i) {
                msg += (char)payload[i];
            }
            handleLine(msg);
            break;
        }
        default:
            break;
    }
}

// Парсер протокола управления с телефона:
// J<t>,<s> — координаты стиков; A — M3; B — M4; P — M5; D — режим 4x4;
// I<id><0/1> — инверсии и свап; H — сигнал клаксона; R — экстренный стоп.
void WifiWebServer::handleLine(const String& line) {
    if (line.isEmpty()) return;
    state.lastUpdate = millis();

    char c = line[0];

    auto clamp100 = [](long x) -> int8_t {
        if (x >  100) x =  100;
        if (x < -100) x = -100;
        return (int8_t)x;
    };

    if (c == 'J') {
        int comma = line.indexOf(',');
        if (comma > 1) {
            long t = line.substring(1, comma).toInt();
            long s = line.substring(comma + 1).toInt();
            state.throttle = clamp100(t);
            state.steer    = clamp100(s);
        }
        return;
    }

    long v = (line.length() > 1) ? line.substring(1).toInt() : 0;

    switch (c) {
        case 'T': state.throttle = clamp100(v); break;
        case 'S': state.steer    = clamp100(v); break;
        case 'A': state.aux1     = clamp100(v); break;
        case 'B': state.aux2     = clamp100(v); break;
        case 'P': state.spotTurn = clamp100(v); break;
        case 'D': state.driveAlt = (v != 0);    break;
        case 'H': state.horn     = true;        break;

        case 'I': {
            if (line.length() >= 3) {
                char what = line[1];
                bool on   = (line[2] == '1');
                switch (what) {
                    case '1': state.invM1   = on; break;
                    case '2': state.invM2   = on; break;
                    case '3': state.invM3   = on; break;
                    case '4': state.invM4   = on; break;
                    case '5': state.invM5   = on; break;
                    case 'g': state.geek1   = on; break;
                    case 'G': state.geek2   = on; break;
                    case 's': state.swapped = on; break;
                }
            }
            break;
        }

        case 'R':
            state.throttle = state.steer = state.aux1 = state.aux2 = state.spotTurn = 0;
            state.horn = false;
            break;
    }
}