// Web RC page

#if WEB_RC_ENABLED

const char webRCIndexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>ARIA Remote</title>
<style>
/*======== Responsive variables ========*/
:root{
  --js-size:clamp(150px,min(50vw,62vh),420px);
  --knob-size:calc(var(--js-size)*0.25);
  --pad:clamp(6px,1.5vmin,15px);
  --gap:clamp(6px,1.5vmin,15px);
}
/*======== Common ========*/
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent;touch-action:none;user-select:none}
body{font-family:'Roboto Mono',Arial,"Microsoft YaHei",sans-serif;background:#3c3c3c;color:#fff;overflow:hidden;height:100vh;height:100dvh;width:100vw}
.container{width:100%;height:100%;display:flex;flex-direction:column;padding:var(--pad);gap:var(--gap);max-width:1200px;margin:0 auto;overflow:hidden}

/*======== Top status bar ========*/
.header {
  text-align: center;
  padding: 6px 10px;
  background: rgba(30,30,30,.9);
  border-radius: 12px;
  border: 2px solid rgba(150,150,150,.3);
  box-shadow: 0 4px 16px rgba(0,0,0,.4);
  backdrop-filter: blur(10px)
}

.header h1 {
  font-size: 1.1rem;
  color: #ffffff;
  margin-bottom: 4px;
}

.status-bar {
  display: flex;
  justify-content: center;
  align-items: center;
  gap: clamp(4px,1.2vmin,10px);
  flex-wrap: nowrap;
  overflow: hidden;
  margin-top: 4px;
}

.status-item {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 2px 5px;
  background: rgba(0,0,0,.3);
  border-radius: 6px;
  border: 1px solid rgba(255,255,255,.1);
  font-size: clamp(0.58rem,1.8vmin,0.7rem);
  white-space: nowrap;
  flex-shrink: 1;
  min-width: 0;
}

.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}

.status-dot.connected { background: #0f8; box-shadow: 0 0 8px #0f8; }
.status-dot.disconnected { background: #f33; box-shadow: 0 0 8px #f33; }
.status-dot.warning { background: #ff9; box-shadow: 0 0 8px #ff9; }

/*======== Content ========*/
.content{display:flex;flex:1;gap:var(--gap);overflow:hidden;min-height:0}
.joystick-container{flex:1;display:flex;flex-direction:column;justify-content:center;align-items:center;background:rgba(0,0,0,.4);border-radius:20px;padding:clamp(8px,1.5vmin,15px);border:2px solid rgba(255,255,255,0.15);box-shadow:inset 0 0 30px rgba(0,0,0,.5)}
.joystick-title{font-size:clamp(0.75rem,2.2vmin,1.1rem);color:#cccccc}
.joystick-wrapper{width:100%;height:var(--js-size);display:flex;justify-content:center;align-items:center;position:relative;margin-top:clamp(4px,2vh,20px);}
.joystick{width:var(--js-size);height:var(--js-size);background:radial-gradient(circle at 30% 30%,rgba(255,255,255,.1),rgba(0,0,0,.3));border-radius:50%;position:relative;border:2px solid rgba(255,255,255,0.2);box-shadow:inset 0 0 20px rgba(0,0,0,.5),0 8px 25px rgba(0,0,0,.5);overflow:hidden}
.joystick::before{content:'';position:absolute;top:50%;left:50%;width:2px;height:100%;background:linear-gradient(to bottom,transparent,rgba(255,255,255,.15),transparent);transform:translate(-50%,-50%)}
.joystick::after{content:'';position:absolute;top:50%;left:50%;width:100%;height:2px;background:linear-gradient(to right,transparent,rgba(255,255,255,.15),transparent);transform:translate(-50%,-50%)}
.joystick-knob{width:var(--knob-size);height:var(--knob-size);background:radial-gradient(circle at 30% 30%,#fff,#cccccc);border-radius:50%;position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);border:2px solid rgba(255,255,255,.7);box-shadow:0 4px 15px rgba(0,0,0,.5),inset 0 0 10px rgba(255,255,255,.5);cursor:move;transition:transform .1s ease-out;z-index:10}

/*======== Buttons ========*/
.buttons-container{flex:.55;display:flex;flex-direction:column;gap:10px;padding:12px;background:rgba(0,0,0,.4);border-radius:20px;border:2px solid rgba(150,150,150,.3);box-shadow:inset 0 0 20px rgba(0,0,0,.5)}
.buttons-grid{display:grid;grid-template-columns:repeat(6,1fr);grid-template-rows:repeat(2,1fr);gap:8px;flex:1}
.buttons-grid>button{grid-column:span 2}
.buttons-grid>button:nth-child(4),.buttons-grid>button:nth-child(5){grid-column:span 3}
.button{background:linear-gradient(145deg,#484848,#383838);border:none;border-radius:10px;color:#fff;font-size:.85rem;font-weight:bold;display:flex;flex-direction:column;justify-content:center;align-items:center;text-align:center;padding:10px 5px;cursor:pointer;transition:all .15s cubic-bezier(.4,0,.2,1);box-shadow:0 3px 10px rgba(0,0,0,.3),inset 0 1px 0 rgba(255,255,255,.1);position:relative}
.button:hover{background:linear-gradient(145deg,#565656,#464646);transform:translateY(-1px)}
.button.active{background:linear-gradient(145deg,#1a73e8,#0d47a1);box-shadow:0 0 15px rgba(26,115,232,.6),inset 0 1px 0 rgba(255,255,255,.2);transform:scale(.95)}
.button:active{transform:scale(.92)}
.button-icon{font-size:1.1rem;margin-bottom:4px}

/*======== Debug console ========*/
.console-panel{background:rgba(10,10,10,.95);border-radius:12px;border:1px solid rgba(100,100,100,.4);padding:10px;flex-shrink:0;max-height:min(200px,35vh);display:flex;flex-direction:column;gap:6px}
.console-output{flex:1;overflow-y:auto;font-family:'Courier New',monospace;font-size:0.7rem;color:#00ff88;min-height:80px;max-height:130px;word-break:break-all;-webkit-overflow-scrolling:touch}
.console-output div{padding:1px 0;border-bottom:1px solid rgba(255,255,255,.03)}

/*======== Animation ========*/
@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(67,97,238,.7)}70%{box-shadow:0 0 0 12px rgba(67,97,238,0)}100%{box-shadow:0 0 0 0 rgba(67,97,238,0)}}
.joystick.active{animation:pulse 1.5s infinite}
@keyframes fadeIn{from{opacity:0;transform:translateY(20px)}to{opacity:1;transform:translateY(0)}}
.container>*{animation:fadeIn .5s ease-out}

/*======== Portrait ========*/
@media (orientation:portrait){
  :root{--js-size:clamp(120px,40vw,240px);--knob-size:calc(var(--js-size)*0.25)}
  .content{
    display:grid;
    grid-template-columns:repeat(2,minmax(0,1fr));
    grid-template-rows:auto auto;
    height:auto;
    flex:none;
    overflow:visible;
  }
  .buttons-container{grid-column:1/3;grid-row:1}
  .content>.joystick-container:first-child{grid-column:1;grid-row:2;min-width:0;overflow:hidden}
  .content>.joystick-container:last-child{grid-column:2;grid-row:2;min-width:0;overflow:hidden}
  .header h1{font-size:clamp(0.85rem,3.5vw,1.1rem)}
  .status-bar{gap:5px;flex-wrap:wrap;justify-content:center}
  .status-item{font-size:clamp(0.6rem,2.5vw,0.7rem);padding:2px 5px}
}

/*======== Small landscape (height up to 420px) ========*/
@media (max-height:420px) and (orientation:landscape){
  :root{--js-size:clamp(120px,min(42vw,44vh),240px);--knob-size:calc(var(--js-size)*0.25)}
  .joystick-title{font-size:0.72rem}
  .header h1{font-size:0.82rem}
  .header{padding:3px 8px}
  .header h1{margin-bottom:2px}
  .status-bar{margin-top:2px;gap:4px}
  .status-item{font-size:0.58rem;padding:2px 4px}
  .button{font-size:0.68rem;padding:5px 3px}
  .button-icon{font-size:0.88rem;margin-bottom:2px}
}
</style>
</head>
<body>
<div class="container">
  <!-- top status bar -->
  <div class="header">
    <h1>ARIA Web Remote</h1>
    <div class="status-bar">
      <div class="status-item"><span class="status-dot" id="status-dot"></span><span id="connection-text">Connecting...</span></div>
      <div class="status-item" id="armed-status-item" style="background:rgba(255,51,51,0.15)"><span id="armed-status" style="color:#ff6666">Disarmed</span></div>
      <div class="status-item"><span>Flight mode</span><span id="flight-mode">STAB</span></div>
      <div class="status-item"><span>Battery</span><span id="battery">-</span></div>
      <div class="status-item"><span>Latency</span><span id="latency">-</span></div>
      <div class="status-item"><span>Packet loss</span><span id="packet-loss">0%</span></div>
      <div class="status-item"><span>Throttle:</span><span id="left-y">0</span><span>%</span></div>
      <div class="status-item"><span>Yaw:</span><span id="left-x">0</span></div>
      <div class="status-item"><span>Roll:</span><span id="right-x">0</span></div>
      <div class="status-item"><span>Pitch:</span><span id="right-y">0</span></div>
    </div>
  </div>

  <div class="content">
    <!-- left stick -->
    <div class="joystick-container">
      <div class="joystick-title">Left stick (Throttle/Yaw)</div>
      <div class="joystick-wrapper">
        <div class="joystick" id="joystick-left"><div class="joystick-knob" id="knob-left"></div></div>
      </div>
    </div>

    <!-- buttons -->
    <div class="buttons-container">
      <div class="buttons-grid" id="buttons-container"></div>
    </div>

    <!-- right stick -->
    <div class="joystick-container">
      <div class="joystick-title">Right stick (Pitch/Roll)</div>
      <div class="joystick-wrapper">
        <div class="joystick" id="joystick-right"><div class="joystick-knob" id="knob-right"></div></div>
      </div>
    </div>
  </div>

  <!-- debug console, hidden until the Debug button -->
  <div id="console-panel" class="console-panel" style="display:none">
    <div id="console-output" class="console-output"></div>
    <div style="display:flex;gap:6px;touch-action:auto">
      <input id="console-input" placeholder="Type a command (ps/imu/rc/arm/disarm/help)..."
        style="flex:1;background:rgba(0,0,0,.6);border:1px solid rgba(100,100,100,.5);border-radius:6px;color:#0f8;padding:4px 8px;font-size:0.7rem;font-family:'Courier New',monospace;touch-action:auto">
      <button onclick="sendConsoleCmd()" style="background:#1a73e8;border:none;border-radius:6px;color:#fff;padding:4px 10px;font-size:0.7rem;cursor:pointer;touch-action:auto">Send</button>
    </div>
  </div>
</div>

<script>
/*======================== Globals ========================*/
let lastAnimationTime = 0;
let connectionOk = false;
let packetStats = { sent: 0, lost: 0 };
let latencyHistory = new Array(10).fill(0);
let latencyIndex = 0;
const SEND_INTERVAL = 50;  // about 20 Hz stick check
const FORCE_SEND_INTERVAL = 200; // resend interval at rest (ms), keeps the flight controller's data fresh

const touches = new Map();
let leftStick  = {x:0, y:0, rawX:0, rawY:-100};
let rightStick = {x:0, y:0, rawX:0, rawY:0};

let lastSentValues = { throttle:0, roll:0, pitch:0, yaw:0 };
let lastForceSentTime = 0; // last forced resend (performance.now())
let currentValues  = { throttle:0, roll:0, pitch:0, yaw:0 };
const MIN_CHANGE_THRESHOLD = 0.5;

// fixed settings, matching the firmware's
const DEADZONE = 3;   // dead zone: handled in the firmware's stickDeadzone
const EXPO    = 40;   // expo 40%
let consecutiveFails = 0; // failures in a row,>=3 counts as disconnected
let currentFlightMode = 2; // current flight mode (as the firmware's: 2 is STAB)

let buttonStates     = new Array(16).fill(false);
let lastButtonStates = new Array(16).fill(false);

let consolePollingTimer = null;
let consoleLastTotal    = 0;   // console cursor: shown up to line N
let consoleFetchInFlight = false; // skip this one while the last fetch is still out
let consolePanelOpen = false;
const CONSOLE_BASE_POLL_MS = 500;
const CONSOLE_CATCHUP_POLL_MS = 80;
const CONSOLE_PAGE_LIMIT = 20;

/*======================== Buttons (2x3) ========================*/
const buttonConfigs = [
  {icon:"🔓",label:"Arm",   color:"#00ff88",desc:"Arm the motors"},
  {icon:"🔒",label:"Disarm",   color:"#ff3333",desc:"Disarm the motors"},
  {icon:"🛑",label:"STOP",   color:"#ff0055",desc:"Emergency stop"},
  {icon:"🔄",label:"Mode", color:"#00cfff",desc:"STAB, ACRO, ALTHOLD in turn"},
  {icon:"🖥",label:"Debug",   color:"#4a9eff",desc:"Debug console"}
];

/*======================== Start ========================*/
function init() {
  initButtons();
  initNetwork();
  initPointerEvents();
  requestAnimationFrame(animationLoop);
  requestAnimationFrame(initKnobPositions);
}

function initKnobPositions() {
  // put the knob where rawY says (left stick throttle starts at the bottom)
  const joystick = document.getElementById('joystick-left');
  const knob     = document.getElementById('knob-left');
  const rect     = joystick.getBoundingClientRect();
  if (rect.width === 0) { requestAnimationFrame(initKnobPositions); return; } // layout not ready: try again
  const radius = rect.width / 2 - 10;
  const dy = -leftStick.rawY / 100 * radius; // rawY=-100 is dy=radius (bottom)
  knob.style.transform = `translate(calc(-50% + 0px), calc(-50% + ${dy}px))`;
}

function initButtons() {
  const container = document.getElementById('buttons-container');
  container.innerHTML = '';
  buttonConfigs.forEach((cfg, idx) => {
    const btn = document.createElement('button');
    btn.className = 'button';
    btn.id = `btn-${idx}`;
    btn.title = cfg.desc;
    btn.innerHTML = `<div class="button-icon">${cfg.icon}</div><div>${cfg.label}</div>`;
    btn.style.border = `2px solid ${cfg.color}55`;
    btn.addEventListener('pointerdown', e => {
      e.preventDefault();
      handleButton(idx);
      if (navigator.vibrate) navigator.vibrate(20);
    });
    container.appendChild(btn);
  });
}

function initNetwork() {
  updateConnectionStatus(true);
  setInterval(updateNetworkStatus, 2000);
}

function initPointerEvents() {
  [{ id:'joystick-left', side:'left' }, { id:'joystick-right', side:'right' }].forEach(js => {
    const el = document.getElementById(js.id);
    el.addEventListener('pointerdown', e => { e.preventDefault(); handlePointerStart(e, js.side); el.setPointerCapture(e.pointerId); });
    el.addEventListener('pointermove', e => { e.preventDefault(); handlePointerMove(e, js.side); });
    el.addEventListener('pointerup',   e => { e.preventDefault(); handlePointerEnd(e, js.side); });
    el.addEventListener('pointercancel', e => { e.preventDefault(); handlePointerEnd(e, js.side); });
    el.addEventListener('touchstart', e => e.preventDefault());
    el.addEventListener('touchmove',  e => e.preventDefault());
  });
}

/*======================== Animation loop ========================*/
function animationLoop(timestamp) {
  if (timestamp - lastAnimationTime >= SEND_INTERVAL) {
    processJoystickInput();
    checkAndSendChanges();
    updateDisplayAll();
    lastAnimationTime = timestamp;
  }
  requestAnimationFrame(animationLoop);
}

/*======================== Stick curve ========================*/
// expo only: the firmware does the dead zone (as for SBUS)
function applyCurve(value) {
  const absVal = Math.abs(value);
  const expoFactor = EXPO / 100;
  let curved = absVal * (1 - expoFactor) + Math.pow(absVal, 3) * expoFactor;
  return curved * (value >= 0 ? 1 : -1);
}

/*======================== Stick data ========================*/
function processJoystickInput() {
  // send raw values: the firmware maps them once (as for SBUS and MAVLink)
  // throttle: rawY in [-100,+100], firmware processThrottle: (raw+100)/(2*RAW_MAX)*100 → 0~100%
  // attitude: to [-1,1], expo, back times 100; the firmware divides by RAW_MAX
  currentValues.throttle = leftStick.rawY;
  currentValues.yaw   = applyCurve(leftStick.rawX  / 100) * 100;  // right is clockwise, as MAVLink
  currentValues.pitch = applyCurve(rightStick.rawY / 100) * 100;  // up is forward, as MAVLink
  currentValues.roll  = applyCurve(rightStick.rawX / 100) * 100;  // right rolls right, as MAVLink
  leftStick.x  = currentValues.yaw;
  leftStick.y  = leftStick.rawY;
  rightStick.x = currentValues.roll;
  rightStick.y = currentValues.pitch;
}

function hasSignificantChange(nv) {
  return Math.abs(nv.throttle - lastSentValues.throttle) > MIN_CHANGE_THRESHOLD ||
         Math.abs(nv.roll     - lastSentValues.roll)     > MIN_CHANGE_THRESHOLD ||
         Math.abs(nv.pitch    - lastSentValues.pitch)    > MIN_CHANGE_THRESHOLD ||
         Math.abs(nv.yaw      - lastSentValues.yaw)      > MIN_CHANGE_THRESHOLD;
}

function checkAndSendChanges() {
  const now = performance.now();
  // send on any change, or after the resend interval to keep the link alive
  if (hasSignificantChange(currentValues) || (now - lastForceSentTime >= FORCE_SEND_INTERVAL)) {
    sendJoystickData();
    lastForceSentTime = now;
  }
  for (let i = 0; i < buttonStates.length; i++) {
    if (buttonStates[i] !== lastButtonStates[i]) {
      sendButtonData(i, buttonStates[i]);
      lastButtonStates[i] = buttonStates[i];
    }
  }
}

/*======================== Sending ========================*/
function sendJoystickData() {
  sendToESP('/web_rc', { t:1, th:Math.round(currentValues.throttle), r:Math.round(currentValues.roll),
    p:Math.round(currentValues.pitch), y:Math.round(currentValues.yaw), ts:performance.now() });
  lastSentValues = {...currentValues};
  packetStats.sent++;
}

function sendButtonData(buttonIndex, state) {
  sendToESP('/web_rc', { t:2, b:buttonIndex, s:state ? 1 : 0, ts:performance.now() });
}

/*======================== Display ========================*/
function updateDisplayAll() {
  document.getElementById('left-y').textContent  = Math.round((currentValues.throttle + 100) / 2);
  document.getElementById('left-x').textContent  = Math.round(leftStick.x);
  document.getElementById('right-x').textContent = Math.round(rightStick.x);
  document.getElementById('right-y').textContent = Math.round(rightStick.y);
}

/*======================== Pointer events ========================*/
function handlePointerStart(e, side) {
  touches.set(e.pointerId, side);
  document.getElementById(`joystick-${side}`).classList.add('active');
  updateJoystickPosition(side, e.clientX, e.clientY);
}

function handlePointerMove(e, side) {
  if (touches.get(e.pointerId) === side) updateJoystickPosition(side, e.clientX, e.clientY);
}

function handlePointerEnd(e, side) {
  if (touches.get(e.pointerId) !== side) return;
  touches.delete(e.pointerId);
  const knob     = document.getElementById(`knob-${side}`);
  const joystick = document.getElementById(`joystick-${side}`);
  joystick.classList.remove('active');
  // where it returns to: left Y to the bottom outside ALTHOLD, the rest to centre
  const targetRawY = (side === 'left' && currentFlightMode !== 3) ? -100 : 0;
  const radius = joystick.getBoundingClientRect().width / 2 - 10;
  const targetDy = -targetRawY / 100 * radius;
  knob.style.transition = 'transform 0.2s ease-out';
  knob.style.transform  = `translate(calc(-50% + 0px), calc(-50% + ${targetDy}px))`;
  setTimeout(() => { knob.style.transition = ''; }, 200);
  if (side === 'left')  leftStick  = {x:0, y:0, rawX:0, rawY: targetRawY};
  else                  rightStick = {x:0, y:0, rawX:0, rawY:0};
  processJoystickInput();
  sendJoystickData();
}

function updateJoystickPosition(side, clientX, clientY) {
  const joystick = document.getElementById(`joystick-${side}`);
  const knob     = document.getElementById(`knob-${side}`);
  const rect     = joystick.getBoundingClientRect();
  const cx = rect.width / 2, cy = rect.height / 2;
  const radius = cx - 10;
  let dx = (clientX - rect.left) - cx;
  let dy = (clientY - rect.top)  - cy;
  const dist = Math.sqrt(dx*dx + dy*dy);
  if (dist > radius) { dx = dx/dist*radius; dy = dy/dist*radius; }
  knob.style.transform = `translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px))`;
  if (side === 'left')  { leftStick.rawX  =  dx/radius*100; leftStick.rawY  = -dy/radius*100; }
  else                  { rightStick.rawX =  dx/radius*100; rightStick.rawY = -dy/radius*100; }
}

/*======================== Network ========================*/
function sendToESP(url, data) {
  const t0 = performance.now();
  fetch(url, { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(data) })
    .then(r => { if (!r.ok) throw new Error(); updateLatency(performance.now() - t0); return r.json(); })
    .then(resp => {
      consecutiveFails = 0;
      updateConnectionStatus(true);
      const names = ['RAW','ACRO','STAB','ALTHOLD','AUTO'];

      // mode change result
      if (resp.m !== undefined) {
        if (resp.m !== currentFlightMode) {
          if (!resp.warn) showToast('✅ Changed to:' + (names[resp.m] || 'unknown'));
          const leftTouched = [...touches.values()].includes('left');
          if (!leftTouched) resetLeftStick(resp.m === 3 ? 0 : -100);
        }
        currentFlightMode = resp.m;
        document.getElementById('flight-mode').textContent = names[resp.m] || 'STAB';
      }

      // arming state (every response shows it)
      if (resp.arm !== undefined) {
        const el   = document.getElementById('armed-status');
        const item = document.getElementById('armed-status-item');
        el.textContent = resp.arm ? 'Armed' : 'Disarmed';
        item.style.background = resp.arm ? 'rgba(0,255,136,0.15)' : 'rgba(255,51,51,0.15)';
        el.style.color = resp.arm ? '#00ff88' : '#ff6666';
      }

      // button release toast (rt=2, bs=0)
      if (resp.rt === 2 && resp.bs === 0) {
        if (!resp.warn) {
          if (resp.bi === 0)
            showToast(resp.arm ? '✅ Armed' : '❌ Arming failed');
          else if (resp.bi === 1)
            showToast(resp.arm ? '⚠️ Disarm failed' : '🔒 Disarmed');
          else if (resp.bi === 2)
            showToast('🛑 Motors stopped');
        }
        if (resp.warn) showToast('⚠️ ' + resp.warn);
      }
      // system warning from a heartbeat (low battery and so on), kept apart from button toasts
      if (resp.rt === 4 && resp.warn) showToast('⚠️ ' + resp.warn);
    })
    .catch(() => {
      if (++consecutiveFails >= 3) updateConnectionStatus(false);
    });
}

function updateLatency(latency) {
  latencyHistory[latencyIndex] = latency;
  latencyIndex = (latencyIndex + 1) % latencyHistory.length;
  const avg = latencyHistory.reduce((a,b)=>a+b) / latencyHistory.length;
  document.getElementById('latency').textContent = Math.round(avg) + 'ms';
  const dot = document.querySelector('.status-dot');
  dot.className = avg > 200 ? 'status-dot warning' : avg > 100 ? 'status-dot warning' : 'status-dot connected';
}

function updateNetworkStatus() {
  const lr = packetStats.sent > 0 ? (packetStats.lost/packetStats.sent*100).toFixed(1) : '0';
  document.getElementById('packet-loss').textContent = lr + '%';
  fetch('/web_rc/status').then(r=>r.json()).then(d => {
    document.getElementById('battery').textContent = (d.voltage !== undefined && d.voltage !== null && d.voltage > 0.5) ? parseFloat(d.voltage).toFixed(2) + 'V' : '-';
  }).catch(()=>{ document.getElementById('battery').textContent = '-'; });
}

function updateConnectionStatus(connected) {
  connectionOk = connected;
  const dot  = document.getElementById('status-dot');
  const text = document.getElementById('connection-text');
  if (connected) { dot.className='status-dot connected'; text.textContent='Connected'; text.style.color='#0f8'; }
  else           { dot.className='status-dot disconnected'; text.textContent='Disconnected'; text.style.color='#f33'; }
}
/*======================== Throttle reset ========================*/
function resetLeftStick(targetRawY) {
  const knob     = document.getElementById('knob-left');
  const joystick = document.getElementById('joystick-left');
  const rect     = joystick.getBoundingClientRect();
  if (rect.width === 0) return;
  const radius = rect.width / 2 - 10;
  const dy = -targetRawY / 100 * radius;
  knob.style.transition = 'transform 0.3s ease-out';
  knob.style.transform  = `translate(calc(-50% + 0px), calc(-50% + ${dy}px))`;
  setTimeout(() => { knob.style.transition = ''; }, 300);
  leftStick = {x:0, y:0, rawX:0, rawY: targetRawY};
  processJoystickInput();
  sendJoystickData();
}
/*======================== Button handling ========================*/
function handleButton(idx) {
  if (idx === 4) { toggleConsole(); return; }
  if (idx === 3) {
    // mode cycle: STAB(2) to ACRO(1) to ALTHOLD(3) to STAB
    // no toast on the press: only when resp.m confirms it
    let nextBit;
    if (currentFlightMode === 2)      nextBit = 7; // STAB→ACRO
    else if (currentFlightMode === 1) nextBit = 8; // ACRO→ALTHOLD
    else                              nextBit = 6; // others to STAB
    sendButtonData(nextBit, 1);
    setTimeout(() => sendButtonData(nextBit, 0), 100);
    if (navigator.vibrate) navigator.vibrate(30);
    return;
  }
  // arm, disarm, stop: press (state=1), release (state=0) 100 ms later
  // the response carries rt, bi, bs, which decide the toast
  if (idx === 0 || idx === 1 || idx === 2) {
    if (idx === 0)      showToast('🔓 Arming...');
    else if (idx === 1) showToast('🔒 Disarming...');
    else if (idx === 2) showToast('🛑 Sending emergency stop...');
    sendButtonData(idx, 1);
    setTimeout(() => sendButtonData(idx, 0), 100);
    if (navigator.vibrate) navigator.vibrate(30);
  }
}

/*======================== Toast ========================*/
function showToast(msg) {
  let t = document.getElementById('toast-msg');
  if (!t) {
    t = document.createElement('div');
    t.id = 'toast-msg';
    t.style.cssText = 'position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:rgba(0,0,0,.85);color:#fff;padding:12px 20px;border-radius:10px;font-size:14px;z-index:9999;pointer-events:none;text-align:center;max-width:80vw;border:1px solid rgba(255,255,255,.2);transition:opacity .3s';
    document.body.appendChild(t);
  }
  t.textContent = msg; t.style.opacity = '1';
  clearTimeout(t._timer);
  t._timer = setTimeout(() => { t.style.opacity = '0'; }, 2000);
}

/*======================== Debug console ========================*/
function toggleConsole() {
  const panel = document.getElementById('console-panel');
  const btn   = document.getElementById('btn-5');
  const open  = panel.style.display === 'none';
  consolePanelOpen = open;
  panel.style.display = open ? 'flex' : 'none';
  if (open) { panel.style.flexDirection = 'column'; }
  if (btn) { open ? btn.classList.add('active') : btn.classList.remove('active'); }
  if (open) {
    document.getElementById('console-output').innerHTML = '';
    consoleLastTotal = 0;
    fetch('/console/enable', {method:'POST'}).catch(()=>{});
    fetchConsoleLogs();
  } else {
    fetch('/console/disable', {method:'POST'}).catch(()=>{});
    clearTimeout(consolePollingTimer);
    consolePollingTimer = null;
  }
}

function scheduleConsolePoll(delayMs) {
  if (!consolePanelOpen) return;
  clearTimeout(consolePollingTimer);
  consolePollingTimer = setTimeout(fetchConsoleLogs, delayMs);
}

function fetchConsoleLogs() {
  if (!consolePanelOpen) return;
  if (consoleFetchInFlight) {
    scheduleConsolePoll(CONSOLE_CATCHUP_POLL_MS);
    return;
  }

  consoleFetchInFlight = true;
  fetch('/console?since=' + consoleLastTotal + '&limit=' + CONSOLE_PAGE_LIMIT).then(r=>r.json()).then(data => {
    const out = document.getElementById('console-output');
    if (data.lines && data.lines.length > 0) {
      const frag = document.createDocumentFragment();
      data.lines.forEach(l => {
        const div = document.createElement('div');
        div.textContent = l;
        frag.appendChild(div);
      });
      out.appendChild(frag);
      out.scrollTop = out.scrollHeight;
      // cap the DOM lines, so a long session can't eat memory
      while (out.children.length > 200) out.removeChild(out.firstChild);
    }

    if (typeof data.next === 'number') consoleLastTotal = data.next;
    else if (typeof data.total === 'number') consoleLastTotal = data.total;

    scheduleConsolePoll(data.has_more ? CONSOLE_CATCHUP_POLL_MS : CONSOLE_BASE_POLL_MS);
  }).catch(()=>{
    scheduleConsolePoll(CONSOLE_BASE_POLL_MS);
  }).finally(() => {
    consoleFetchInFlight = false;
  });
}

function sendConsoleCmd() {
  const input = document.getElementById('console-input');
  const cmd = input.value.trim();
  if (!cmd) return;
  input.value = '';
  fetch('/console/cmd', {method:'POST', headers:{'Content-Type':'text/plain'}, body:cmd})
    .then(r => r.json())
    .then(resp => {
      if (!resp.ok) {
        if (resp.e === 'queue full') showToast('⚠️ Command queue full, try again soon');
        return;
      }
      fetchConsoleLogs();
    }).catch(()=>{});
}

// Enter sends, up and down arrows for history
(function(){
  const cmdHistory = [];
  let historyIdx   = -1;
  document.getElementById('console-input').addEventListener('keydown', function(e) {
    if (e.key === 'Enter') {
      e.preventDefault();
      const v = this.value.trim();
      if (v) { cmdHistory.unshift(v); if (cmdHistory.length > 20) cmdHistory.pop(); }
      sendConsoleCmd();
      historyIdx = -1;
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (historyIdx < cmdHistory.length - 1) { historyIdx++; this.value = cmdHistory[historyIdx]; }
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (historyIdx > 0) { historyIdx--; this.value = cmdHistory[historyIdx]; }
      else { historyIdx = -1; this.value = ''; }
    }
  });
})();

/*======================== Events ========================*/
document.addEventListener('DOMContentLoaded', init);
document.addEventListener('contextmenu', e => e.preventDefault());

// heartbeat every 2000 ms, 3 failures in a row counts as disconnected
setInterval(() => {
  if (connectionOk) sendToESP('/web_rc/heartbeat', {t:4, ts:performance.now()});
}, 2000);
</script>
</body>
</html>
)rawliteral";

#endif
