#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <DHTesp.h>

Preferences preferences;
DNSServer dnsServer;
int storedW[4] = {0, 0, 0, 0};
int storedR = 0;
int storedG = 0;
int storedB = 0;
int saveInterval = 5000;
bool settingsChanged = false;
unsigned long lastChangeTime = 0;

// DHT Sensor
#define DHT_PIN 23
DHTesp dht;

// ---------------- OLED CONFIG ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- LED PINS ----------------
#define LED1 5
#define LED2 17
#define LED3 16
#define LED4 4

#define RED_PIN   2
#define GREEN_PIN 15
#define BLUE_PIN  21

const int freq = 5000;
const int resolution = 8;

// PWM channels
const int ledChannels[4] = {0,1,2,3};
const int redChannel   = 4;
const int greenChannel = 5;
const int blueChannel  = 6;

// ---------------- WIFI & WEB SERVER ----------------
const char* ssid = "ESP Lamp http://192.168.4.1";
const char* password = "password123";

WebServer server(80);
String customText = ""; 
unsigned long timeOffset = 0;
unsigned long lastTimeSync = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long nextFaceChange = 0;
int currentEyeL = 0;
int currentEyeR = 0;
int currentMouth = 0;
int currentMod = 0;

// Settings
int faceMode = 0; // 0: Auto, 1: Shuffle, 2: Manual
int manualFaceID = 0; // For Manual Mode
bool catHappy = true;
bool catGoofy = true;
bool catSleepy = true;

// --- Face Drawing Helpers ---
void drawEye(int x, int y, int type) {
  // 0: Normal, 1: Happy, 2: Wide, 3: Closed, 4: X, 5: L, 6: R, 7: Glare, 8: Heart, 9: Spiral, 10: Dollar, 11: Teary, 12: Glasses
  switch(type) {
    case 0: display.fillRect(x-6, y-6, 12, 12, SSD1306_WHITE); break; 
    case 1: // ^
      display.drawLine(x-8, y+4, x, y-4, SSD1306_WHITE);
      display.drawLine(x, y-4, x+8, y+4, SSD1306_WHITE);
      break;
    case 2: display.drawCircle(x, y, 8, SSD1306_WHITE); break; // O
    case 3: display.drawLine(x-8, y, x+8, y, SSD1306_WHITE); break; // -
    case 4: // X
      display.drawLine(x-6, y-6, x+6, y+6, SSD1306_WHITE);
      display.drawLine(x-6, y+6, x+6, y-6, SSD1306_WHITE);
      break;
    case 5: // Look Left
      display.drawCircle(x, y, 8, SSD1306_WHITE);
      display.fillCircle(x-4, y, 4, SSD1306_WHITE);
      break;
    case 6: // Look Right
      display.drawCircle(x, y, 8, SSD1306_WHITE);
      display.fillCircle(x+4, y, 4, SSD1306_WHITE);
      break;
    case 7: // Glare >
      display.drawLine(x-8, y-4, x+8, y+2, SSD1306_WHITE);
      display.drawLine(x-8, y+2, x+8, y+2, SSD1306_WHITE); 
      break;
    case 8: // Heart
      display.fillCircle(x-4, y-4, 4, SSD1306_WHITE);
      display.fillCircle(x+4, y-4, 4, SSD1306_WHITE);
      display.fillTriangle(x-8, y-2, x+8, y-2, x, y+8, SSD1306_WHITE);
      break;
    case 9: // Spiral (Simplified as concentric circles or just a target)
      display.drawCircle(x, y, 8, SSD1306_WHITE);
      display.drawCircle(x, y, 4, SSD1306_WHITE);
      display.drawPixel(x, y, SSD1306_WHITE);
      break;
    case 10: // Dollar
      display.setTextSize(2); display.setCursor(x-5, y-7); display.print("$");
      break;
    case 11: // Teary (T)
      display.drawLine(x-8, y-6, x+8, y-6, SSD1306_WHITE);
      display.drawLine(x, y-6, x, y+6, SSD1306_WHITE);
      break;
    case 12: // Glasses (Square)
      display.drawRect(x-10, y-6, 20, 12, SSD1306_WHITE);
      display.drawLine(x+10, y, x+15, y, SSD1306_WHITE); // Bridge/Arm
      break;
  }
}

void drawMouth(int x, int y, int type) {
  // 0: Smile, 1: Neutral, 2: Open, 3: Sad, 4: Cat, 5: Tongue, 6: Teeth, 7: Zigzag, 8: Kiss
  switch(type) {
    case 0: // U
      display.drawLine(x-8, y-3, x-4, y+3, SSD1306_WHITE);
      display.drawLine(x-4, y+3, x+4, y+3, SSD1306_WHITE);
      display.drawLine(x+4, y+3, x+8, y-3, SSD1306_WHITE);
      break;
    case 1: display.drawLine(x-6, y, x+6, y, SSD1306_WHITE); break; // -
    case 2: display.drawCircle(x, y, 5, SSD1306_WHITE); break; // O
    case 3: // n
      display.drawLine(x-8, y+3, x-4, y-3, SSD1306_WHITE);
      display.drawLine(x-4, y-3, x+4, y-3, SSD1306_WHITE);
      display.drawLine(x+4, y-3, x+8, y+3, SSD1306_WHITE);
      break;
    case 4: // w
      display.drawLine(x-8, y-2, x-4, y+2, SSD1306_WHITE);
      display.drawLine(x-4, y+2, x, y-1, SSD1306_WHITE);
      display.drawLine(x, y-1, x+4, y+2, SSD1306_WHITE);
      display.drawLine(x+4, y+2, x+8, y-2, SSD1306_WHITE);
      break;
    case 5: // Tongue (P)
      display.drawLine(x-6, y, x+6, y, SSD1306_WHITE);
      display.fillCircle(x+2, y+4, 3, SSD1306_WHITE);
      break;
    case 6: // Teeth (Grid)
      display.drawRect(x-8, y-3, 16, 6, SSD1306_WHITE);
      display.drawLine(x-8, y, x+8, y, SSD1306_WHITE);
      display.drawLine(x, y-3, x, y+3, SSD1306_WHITE);
      break;
    case 7: // Zigzag
      display.drawLine(x-8, y, x-4, y+3, SSD1306_WHITE);
      display.drawLine(x-4, y+3, x, y-3, SSD1306_WHITE);
      display.drawLine(x, y-3, x+4, y+3, SSD1306_WHITE);
      display.drawLine(x+4, y+3, x+8, y, SSD1306_WHITE);
      break;
    case 8: // Kiss (*)
      display.drawLine(x-3, y-3, x+3, y+3, SSD1306_WHITE);
      display.drawLine(x-3, y+3, x+3, y-3, SSD1306_WHITE);
      display.drawLine(x, y-4, x, y+4, SSD1306_WHITE);
      break;
  }
}

void drawModifier(int type) {
    // 1: Zzz, 2: Sweat, 3: Blush, 4: Music, 5: Bulb, 6: Anger
    if(type == 1) {
        display.setTextSize(1);
        display.setCursor(100, 10); display.print("z");
        display.setCursor(110, 5); display.print("Z");
    } else if(type == 2) {
        display.drawCircle(105, 20, 3, SSD1306_WHITE);
        display.drawLine(105, 17, 105, 12, SSD1306_WHITE);
    } else if(type == 3) {
        display.drawLine(30, 38, 50, 38, SSD1306_WHITE);
        display.drawLine(78, 38, 98, 38, SSD1306_WHITE);
    } else if(type == 4) { // Music
        display.drawCircle(105, 15, 2, SSD1306_WHITE);
        display.drawLine(105, 15, 105, 5, SSD1306_WHITE);
        display.drawLine(105, 5, 110, 5, SSD1306_WHITE);
    } else if(type == 5) { // Bulb
        display.drawCircle(105, 15, 4, SSD1306_WHITE);
        display.drawLine(105, 19, 105, 22, SSD1306_WHITE);
    } else if(type == 6) { // Anger
        display.drawLine(100, 10, 105, 15, SSD1306_WHITE);
        display.drawLine(105, 10, 100, 15, SSD1306_WHITE);
        display.drawLine(108, 8, 113, 13, SSD1306_WHITE);
        display.drawLine(113, 8, 108, 13, SSD1306_WHITE);
    }
}

// HTML Page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Lamp</title>
  <style>
    :root {
      --bg-color: #121212;
      --card-bg: #1e1e1e;
      --text-main: #e0e0e0;
      --text-muted: #a0a0a0;
      --accent: #00d2ff;
      --accent-glow: rgba(0, 210, 255, 0.3);
    }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background: var(--bg-color); color: var(--text-main); margin: 0; padding: 20px; }
    .container { max-width: 600px; margin: 0 auto; }
    
    /* Header */
    header { text-align: center; margin-bottom: 30px; }
    h1 { margin: 0; font-weight: 300; letter-spacing: 1px; color: var(--accent); }
    
    /* Cards */
    .card { background: var(--card-bg); border-radius: 16px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 15px rgba(0,0,0,0.3); }
    h2 { margin-top: 0; font-size: 1.2rem; color: var(--text-muted); border-bottom: 1px solid #333; padding-bottom: 10px; margin-bottom: 20px; display: flex; align-items: center; justify-content: space-between; }
    
    /* Sliders */
    .control-group { margin-bottom: 15px; }
    .label-row { display: flex; justify-content: space-between; margin-bottom: 8px; font-size: 0.9rem; font-weight: 500; }
    input[type=range] { -webkit-appearance: none; width: 100%; background: transparent; }
    input[type=range]:focus { outline: none; }
    input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 8px; cursor: pointer; background: #333; border-radius: 4px; }
    input[type=range]::-webkit-slider-thumb { height: 24px; width: 24px; border-radius: 50%; background: var(--accent); cursor: pointer; -webkit-appearance: none; margin-top: -8px; box-shadow: 0 0 10px var(--accent-glow); transition: transform 0.1s; }
    input[type=range]::-webkit-slider-thumb:active { transform: scale(1.2); }
    
    /* Specific Slider Colors */
    .slider-white::-webkit-slider-thumb { background: #fff; box-shadow: 0 0 10px rgba(255,255,255,0.3); }
    .slider-rgb::-webkit-slider-thumb { background: linear-gradient(45deg, #f00, #0f0, #00f); }

    /* Grid for small sliders */
    .sub-channels { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-top: 20px; padding-top: 15px; border-top: 1px solid #333; }
    
    /* Buttons */
    .btn { border: none; border-radius: 12px; padding: 15px; font-size: 1rem; font-weight: 600; cursor: pointer; transition: transform 0.1s, opacity 0.2s; width: 100%; color: white; text-align: center; }
    .btn:active { transform: scale(0.98); }
    
    .btn-primary { background: var(--accent); color: #000; }
    .btn-picker { background: linear-gradient(135deg, #ff0000, #00ff00, #0000ff); text-shadow: 0 1px 3px rgba(0,0,0,0.8); }
    .btn-off { background: #2a2a2a; border: 2px solid #ff4444; color: #ff4444; }

    /* Text Input */
    textarea { width: 100%; background: #121212; border: 1px solid #333; color: white; padding: 15px; border-radius: 12px; font-family: inherit; font-size: 1rem; resize: vertical; box-sizing: border-box; margin-bottom: 10px; }
    textarea:focus { outline: none; border-color: var(--accent); }

    /* Modal */
    .modal { display: none; position: fixed; z-index: 100; left: 0; top: 0; width: 100%; height: 100%; background-color: rgba(0,0,0,0.85); backdrop-filter: blur(8px); align-items: center; justify-content: center; opacity: 0; transition: opacity 0.3s ease; }
    .modal.show { opacity: 1; }
    .modal-content { background: #252525; padding: 30px; border-radius: 24px; text-align: center; position: relative; box-shadow: 0 20px 60px rgba(0,0,0,0.6); border: 1px solid #333; max-width: 90%; }
    .close-modal { position: absolute; top: 15px; right: 20px; font-size: 30px; color: #aaa; cursor: pointer; transition: color 0.2s; }
    .close-modal:hover { color: white; }
    canvas { border-radius: 50%; cursor: crosshair; touch-action: none; box-shadow: 0 0 30px rgba(0,0,0,0.5); border: 4px solid #333; }
    #colorPreview { width: 60px; height: 60px; border-radius: 50%; margin: 20px auto 0; border: 3px solid #fff; box-shadow: 0 0 20px rgba(0,0,0,0.5); transition: background-color 0.1s; }
    
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>ESP32 LAMP</h1>
    </header>

    <!-- Brightness Card -->
    <div class="card">
      <h2>Brightness Control</h2>
      
      <div class="control-group">
        <div class="label-row"><span>Master White</span><span id="val-all">0%</span></div>
        <input type="range" min="0" max="255" value="0" class="slider-white" oninput="setAllW(this.value)">
      </div>

      <div class="control-group">
        <div class="label-row"><span>RGB Dimmer</span><span id="val-dim">100%</span></div>
        <input type="range" min="0" max="255" value="255" class="slider-rgb" oninput="setBright(this.value)">
      </div>

      <div class="sub-channels">
        <div class="control-group">
          <div class="label-row"><span>L1</span></div>
          <input type="range" id="s1" min="0" max="255" value="0" oninput="setLed(1, this.value)">
        </div>
        <div class="control-group">
          <div class="label-row"><span>L2</span></div>
          <input type="range" id="s2" min="0" max="255" value="0" oninput="setLed(2, this.value)">
        </div>
        <div class="control-group">
          <div class="label-row"><span>L3</span></div>
          <input type="range" id="s3" min="0" max="255" value="0" oninput="setLed(3, this.value)">
        </div>
        <div class="control-group">
          <div class="label-row"><span>L4</span></div>
          <input type="range" id="s4" min="0" max="255" value="0" oninput="setLed(4, this.value)">
        </div>
      </div>
    </div>

    <!-- Color Card -->
    <div class="card">
      <h2>Color Control</h2>
      <div style="display: flex; gap: 10px;">
        <button class="btn btn-picker" onclick="openColorPicker()" style="flex: 2;">COLOR PICKER</button>
        <button class="btn btn-off" onclick="allOff()" style="flex: 1;">OFF</button>
      </div>
    </div>

    <!-- Display Card -->
    <div class="card">
      <h2>Display Message</h2>
      <textarea id="dispText" rows="3" placeholder="Type your message here..."></textarea>
      <button class="btn btn-primary" onclick="sendText()">SEND TO SCREEN</button>
    </div>

    <!-- Environment Card -->
    <div class="card">
      <h2>Environment</h2>
      <div class="label-row"><span>Temperature</span><span id="val-temp">-- &deg;C</span></div>
      <div class="label-row"><span>Humidity</span><span id="val-hum">-- %</span></div>
    </div>

    <!-- Face Control Card -->
    <div class="card">
      <h2>Face Control</h2>
      <div class="control-group">
        <div class="label-row"><span>Mode</span></div>
        <select id="faceMode" onchange="setFaceMode(this.value)" style="width:100%; padding:10px; background:#333; color:white; border:none; border-radius:8px;">
            <option value="0">Smart Auto</option>
            <option value="1">Random Shuffle</option>
            <option value="2">Manual Face</option>
        </select>
      </div>
      
      <div id="manualFaceControl" class="control-group" style="display:none;">
        <div class="label-row"><span>Select Face</span></div>
        <select id="manualFaceID" onchange="setFaceConfig()" style="width:100%; padding:10px; background:#333; color:white; border:none; border-radius:8px;">
            <option value="0">Happy</option>
            <option value="1">Sleepy</option>
            <option value="2">Love</option>
            <option value="3">Cool</option>
            <option value="4">Goofy</option>
            <option value="5">Sad</option>
            <option value="6">Angry</option>
            <option value="7">Focused</option>
            <option value="8">Cat</option>
            <option value="9">Dizzy</option>
        </select>
      </div>

      <div id="shuffleControl" class="control-group" style="display:none;">
        <div class="label-row"><span>Allowed Categories</span></div>
        <div style="display:flex; gap:10px; flex-wrap:wrap;">
            <button id="btn-happy" class="btn" style="flex:1; background:#444;" onclick="toggleCat('happy')">Happy</button>
            <button id="btn-goofy" class="btn" style="flex:1; background:#444;" onclick="toggleCat('goofy')">Goofy</button>
            <button id="btn-sleepy" class="btn" style="flex:1; background:#444;" onclick="toggleCat('sleepy')">Sleepy</button>
        </div>
      </div>
    </div>

    <!-- Settings Card -->
    <div class="card">
      <h2>Settings</h2>
      <div class="control-group">
        <div class="label-row"><span>Auto-Save Interval</span><span id="val-interval">5s</span></div>
        <input type="range" min="1" max="60" value="5" oninput="setSaveInterval(this.value)">
      </div>
    </div>
  </div>

  <!-- Modal -->
  <div id="colorModal" class="modal">
    <div class="modal-content">
      <span class="close-modal" onclick="closeColorPicker()">&times;</span>
      <h2 style="justify-content: center; border: none;">Pick a Color</h2>
      <canvas id="colorWheel" width="280" height="280"></canvas>
      <div id="colorPreview"></div>
    </div>
  </div>

<script>
// --- Init ---
window.onload = function() {
    fetch('/getState')
    .then(response => response.json())
    .then(data => {
        document.getElementById('s1').value = data.w1;
        document.getElementById('s2').value = data.w2;
        document.getElementById('s3').value = data.w3;
        document.getElementById('s4').value = data.w4;
        currentR = data.r;
        currentG = data.g;
        currentB = data.b;
        document.getElementById('colorPreview').style.backgroundColor = `rgb(${currentR},${currentG},${currentB})`;
        brightness = 255;
        document.querySelector('.slider-rgb').value = 255;
        document.getElementById('val-dim').innerText = "100%";
        
        // Update Interval
        let intervalSec = data.interval;
        document.querySelector('input[oninput="setSaveInterval(this.value)"]').value = intervalSec;
        document.getElementById('val-interval').innerText = intervalSec + "s";
        
        // Update Face UI
        document.getElementById('faceMode').value = data.faceMode;
        document.getElementById('manualFaceID').value = data.manualFaceID;
        updateFaceUI(data.faceMode);
        
        updateCatBtn('happy', data.catHappy);
        updateCatBtn('goofy', data.catGoofy);
        updateCatBtn('sleepy', data.catSleepy);

        // Sync Time
        let now = new Date();
        let localTs = Math.floor(now.getTime()/1000 - (now.getTimezoneOffset()*60));
        fetch(`/setTime?time=${localTs}`);

        updateSensor();
    });
};

function updateFaceUI(mode) {
    document.getElementById('manualFaceControl').style.display = (mode == 2) ? 'block' : 'none';
    document.getElementById('shuffleControl').style.display = (mode == 1) ? 'block' : 'none';
}

function setFaceMode(val) {
    fetch(`/setFaceMode?mode=${val}`);
    updateFaceUI(val);
}

function setFaceConfig() {
    let id = document.getElementById('manualFaceID').value;
    fetch(`/setFaceConfig?id=${id}`);
}

function toggleCat(cat) {
    let btn = document.getElementById('btn-' + cat);
    let val = btn.dataset.state == '1' ? 0 : 1;
    updateCatBtn(cat, val);
    fetch(`/setFaceCat?cat=${cat}&val=${val}`);
}

function updateCatBtn(cat, val) {
    let btn = document.getElementById('btn-' + cat);
    btn.dataset.state = val;
    btn.style.background = val ? '#00d2ff' : '#444';
    btn.style.color = val ? '#000' : '#fff';
}

function updateSensor() {
    fetch('/getSensor')
    .then(response => response.json())
    .then(data => {
        document.getElementById('val-temp').innerText = data.temp + " \u00B0C";
        document.getElementById('val-hum').innerText = data.hum + " %";
    });
}
setInterval(updateSensor, 5000);

// --- Modal Logic ---
const modal = document.getElementById("colorModal");
function openColorPicker() { 
    modal.style.display = "flex";
    setTimeout(() => modal.classList.add('show'), 10);
}
function closeColorPicker() { 
    modal.classList.remove('show');
    setTimeout(() => modal.style.display = "none", 300);
}
window.onclick = function(event) { if (event.target == modal) { closeColorPicker(); } }

// --- Color Wheel Logic ---
const canvas = document.getElementById('colorWheel');
const ctx = canvas.getContext('2d');
const radius = canvas.width / 2;
const centerX = radius;
const centerY = radius;
let isDragging = false;

// Draw the wheel once
function drawWheel() {
  for(let angle=0; angle<360; angle++){
    let startAngle = (angle-2)*Math.PI/180;
    let endAngle = angle*Math.PI/180;
    ctx.beginPath();
    ctx.moveTo(centerX, centerY);
    ctx.arc(centerX, centerY, radius, startAngle, endAngle);
    ctx.closePath();
    ctx.fillStyle = `hsl(${angle}, 100%, 50%)`;
    ctx.fill();
  }
  // White gradient center
  let grd = ctx.createRadialGradient(centerX, centerY, 0, centerX, centerY, radius);
  grd.addColorStop(0, 'white');
  grd.addColorStop(1, 'transparent');
  ctx.fillStyle = grd;
  ctx.fill();
}
drawWheel();

// Handle Interaction
function handleColor(x, y) {
    let dx = x - centerX;
    let dy = y - centerY;
    let dist = Math.sqrt(dx*dx + dy*dy);
    if (dist > radius) return; // Outside circle

    // Get pixel color
    let pixel = ctx.getImageData(x, y, 1, 1).data;
    let r = pixel[0], g = pixel[1], b = pixel[2];
    
    // Update Preview
    document.getElementById('colorPreview').style.backgroundColor = `rgb(${r},${g},${b})`;
    
    setColorThrottled(r, g, b);
}

function getPos(e) {
    let rect = canvas.getBoundingClientRect();
    return {
        x: (e.clientX || e.touches[0].clientX) - rect.left,
        y: (e.clientY || e.touches[0].clientY) - rect.top
    };
}

canvas.addEventListener('mousedown', e => { isDragging = true; let p = getPos(e); handleColor(p.x, p.y); });
canvas.addEventListener('mousemove', e => { if(isDragging) { let p = getPos(e); handleColor(p.x, p.y); }});
canvas.addEventListener('mouseup', () => isDragging = false);
canvas.addEventListener('touchstart', e => { isDragging = true; e.preventDefault(); let p = getPos(e); handleColor(p.x, p.y); }, {passive: false});
canvas.addEventListener('touchmove', e => { if(isDragging) { e.preventDefault(); let p = getPos(e); handleColor(p.x, p.y); }}, {passive: false});
canvas.addEventListener('touchend', () => isDragging = false);

// --- API Calls ---
let lastCall = 0;
let currentR = 0, currentG = 0, currentB = 0;
let brightness = 255;

function updateRGB() {
    let r = Math.floor(currentR * (brightness/255));
    let g = Math.floor(currentG * (brightness/255));
    let b = Math.floor(currentB * (brightness/255));
    fetch(`/setRGB?r=${r}&g=${g}&b=${b}`);
}

function setBright(val) {
    brightness = val;
    document.getElementById('val-dim').innerText = Math.round((val/255)*100) + "%";
    updateRGB();
}

function setColor(r, g, b) {
    currentR = r; currentG = g; currentB = b;
    updateRGB();
}

function setColorThrottled(r, g, b) {
    let now = Date.now();
    if(now - lastCall < 50) return; // Limit to 20fps
    lastCall = now;
    setColor(r, g, b);
}

function setLed(id, val) {
    fetch(`/setW?id=${id}&val=${val}`);
}

function setAllW(val) {
    document.getElementById('s1').value = val;
    document.getElementById('s2').value = val;
    document.getElementById('s3').value = val;
    document.getElementById('s4').value = val;
    document.getElementById('val-all').innerText = Math.round((val/255)*100) + "%";
    fetch(`/setAllW?val=${val}`);
}

function allOff() {
    fetch("/allOff");
    document.querySelectorAll('input[type=range]').forEach(el => el.value = 0);
    document.getElementById('val-all').innerText = "0%";
    // Don't reset brightness slider visually, just the output? 
    // Usually "Off" means turn off lights, but maybe keep brightness setting?
    // For now, let's zero out the white sliders as before.
}

function sendText() {
    let txt = document.getElementById("dispText").value;
    fetch("/setText?txt=" + encodeURIComponent(txt));
}

function setSaveInterval(val) {
    document.getElementById('val-interval').innerText = val + "s";
    fetch(`/setSaveInterval?val=${val}`);
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleGetState() {
  String json = "{";
  json += "\"w1\":" + String(storedW[0]) + ",";
  json += "\"w2\":" + String(storedW[1]) + ",";
  json += "\"w3\":" + String(storedW[2]) + ",";
  json += "\"w4\":" + String(storedW[3]) + ",";
  json += "\"r\":" + String(storedR) + ",";
  json += "\"g\":" + String(storedG) + ",";
  json += "\"b\":" + String(storedB) + ",";
  json += "\"interval\":" + String(saveInterval/1000) + ",";
  json += "\"faceMode\":" + String(faceMode) + ",";
  json += "\"manualFaceID\":" + String(manualFaceID) + ",";
  json += "\"catHappy\":" + String(catHappy) + ",";
  json += "\"catGoofy\":" + String(catGoofy) + ",";
  json += "\"catSleepy\":" + String(catSleepy);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetFaceMode() {
    if (server.hasArg("mode")) {
        faceMode = server.arg("mode").toInt();
        settingsChanged = true;
        lastChangeTime = millis();
    }
    server.send(200, "text/plain", "OK");
}

void handleSetFaceConfig() {
    if (server.hasArg("id")) {
        manualFaceID = server.arg("id").toInt();
        settingsChanged = true;
        lastChangeTime = millis();
    }
    server.send(200, "text/plain", "OK");
}

void handleSetFaceCat() {
    if (server.hasArg("cat") && server.hasArg("val")) {
        String cat = server.arg("cat");
        int val = server.arg("val").toInt();
        if(cat == "happy") catHappy = val;
        else if(cat == "goofy") catGoofy = val;
        else if(cat == "sleepy") catSleepy = val;
        
        settingsChanged = true;
        lastChangeTime = millis();
    }
    server.send(200, "text/plain", "OK");
}

void handleSetSaveInterval() {
  if (server.hasArg("val")) {
    int val = server.arg("val").toInt();
    if(val > 0) {
        saveInterval = val * 1000;
        preferences.putInt("interval", saveInterval);
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSetW() {
  if (server.hasArg("id") && server.hasArg("val")) {
    int id = server.arg("id").toInt();
    int val = server.arg("val").toInt();
    if(id >= 1 && id <= 4) {
      ledcWrite(ledChannels[id-1], val);
      storedW[id-1] = val;
      settingsChanged = true;
      lastChangeTime = millis();
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSetRGB() {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    int r = server.arg("r").toInt();
    int g = server.arg("g").toInt();
    int b = server.arg("b").toInt();
    ledcWrite(redChannel, r);
    ledcWrite(greenChannel, g);
    ledcWrite(blueChannel, b);
    storedR = r; storedG = g; storedB = b;
    settingsChanged = true;
    lastChangeTime = millis();
  }
  server.send(200, "text/plain", "OK");
}

void handleSetText() {
  if (server.hasArg("txt")) {
    customText = server.arg("txt");
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(2);
    display.println(customText);
    display.display();
  }
  server.send(200, "text/plain", "OK");
}

void handleAllOff() {
  for(int i=0; i<4; i++) {
      ledcWrite(ledChannels[i], 0);
      storedW[i] = 0;
  }
  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 0);
  ledcWrite(blueChannel, 0);
  storedR = 0; storedG = 0; storedB = 0;
  
  settingsChanged = true;
  lastChangeTime = millis();
  
  server.send(200, "text/plain", "OK");
}

void handleSetAllW() {
  if (server.hasArg("val")) {
    int val = server.arg("val").toInt();
    for(int i=0; i<4; i++) {
        ledcWrite(ledChannels[i], val);
        storedW[i] = val;
    }
    settingsChanged = true;
    lastChangeTime = millis();
  }
  server.send(200, "text/plain", "OK");
}

void handleSetTime() {
  if (server.hasArg("time")) {
    timeOffset = server.arg("time").toInt();
    lastTimeSync = millis();
  }
  server.send(200, "text/plain", "OK");
}

void handleGetSensor() {
  float h = dht.getHumidity();
  float t = dht.getTemperature();
  
  if (isnan(h) || isnan(t)) {
    server.send(200, "application/json", "{\"temp\":\"--\",\"hum\":\"--\"}");
    return;
  }

  String json = "{";
  json += "\"temp\":\"" + String(t, 1) + "\",";
  json += "\"hum\":\"" + String(h, 1) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);

    // Preferences
    preferences.begin("lamp", false);
    storedW[0] = preferences.getInt("w1", 0);
    storedW[1] = preferences.getInt("w2", 0);
    storedW[2] = preferences.getInt("w3", 0);
    storedW[3] = preferences.getInt("w4", 0);
    storedR = preferences.getInt("r", 0);
    storedG = preferences.getInt("g", 0);
    storedB = preferences.getInt("b", 0);
    saveInterval = preferences.getInt("interval", 5000);
    faceMode = preferences.getInt("faceMode", 0);
    manualFaceID = preferences.getInt("manualFaceID", 0);
    catHappy = preferences.getBool("catHappy", true);
    catGoofy = preferences.getBool("catGoofy", true);
    catSleepy = preferences.getBool("catSleepy", true);

    // OLED
    Wire.begin(18,19);
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
        Serial.println("SSD1306 not found");
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0,0);
        display.println("Starting WiFi...");
        display.display();
    }

    // PWM - White LEDs
    ledcSetup(ledChannels[0], freq, resolution); ledcAttachPin(LED1, ledChannels[0]);
    ledcSetup(ledChannels[1], freq, resolution); ledcAttachPin(LED2, ledChannels[1]);
    ledcSetup(ledChannels[2], freq, resolution); ledcAttachPin(LED3, ledChannels[2]);
    ledcSetup(ledChannels[3], freq, resolution); ledcAttachPin(LED4, ledChannels[3]);

    // Apply stored White
    for(int i=0; i<4; i++) ledcWrite(ledChannels[i], storedW[i]);

    // PWM - RGB LEDs
    ledcSetup(redChannel, freq, resolution);   ledcAttachPin(RED_PIN, redChannel);
    ledcSetup(greenChannel, freq, resolution); ledcAttachPin(GREEN_PIN, greenChannel);
    ledcSetup(blueChannel, freq, resolution);  ledcAttachPin(BLUE_PIN, blueChannel);

    // Apply stored RGB
    ledcWrite(redChannel, storedR);
    ledcWrite(greenChannel, storedG);
    ledcWrite(blueChannel, storedB);

    // DHT Sensor
    dht.setup(DHT_PIN, DHTesp::DHT11);

    // ---------------- WIFI SETUP ----------------
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Captive Portal DNS
    dnsServer.start(53, "*", IP);

    server.on("/", handleRoot);
    server.on("/getState", handleGetState);
    server.on("/setW", handleSetW);
    server.on("/setRGB", handleSetRGB);
    server.on("/setText", handleSetText);
    server.on("/allOff", handleAllOff);
    server.on("/setAllW", handleSetAllW);
    server.on("/setSaveInterval", handleSetSaveInterval);
    server.on("/getSensor", handleGetSensor);
    server.on("/setTime", handleSetTime);
    server.on("/setFaceMode", handleSetFaceMode);
    server.on("/setFaceConfig", handleSetFaceConfig);
    server.on("/setFaceCat", handleSetFaceCat);
    
    // Redirect all other requests to root (Captive Portal)
    server.onNotFound([]() {
        server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
        server.send(302, "text/plain", "");
    });

    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    
    // Save settings if changed and stable for saveInterval
    if (settingsChanged && (millis() - lastChangeTime > saveInterval)) {
        preferences.putInt("w1", storedW[0]);
        preferences.putInt("w2", storedW[1]);
        preferences.putInt("w3", storedW[2]);
        preferences.putInt("w4", storedW[3]);
        preferences.putInt("r", storedR);
        preferences.putInt("g", storedG);
        preferences.putInt("b", storedB);
        preferences.putInt("faceMode", faceMode);
        preferences.putInt("manualFaceID", manualFaceID);
        preferences.putBool("catHappy", catHappy);
        preferences.putBool("catGoofy", catGoofy);
        preferences.putBool("catSleepy", catSleepy);
        settingsChanged = false;
        Serial.println("Settings saved to flash");
    }

    // Display Update (Faces)
    if (millis() - lastDisplayUpdate > 100) { // Update frame faster for responsiveness
        lastDisplayUpdate = millis();
        
        // Only show faces if no custom text
        if (customText == "") {
            float t = dht.getTemperature();
            float h = dht.getHumidity();
            unsigned long currentEpoch = timeOffset + (millis() - lastTimeSync)/1000;
            int hour = (currentEpoch % 86400L) / 3600;
            int minute = (currentEpoch % 3600) / 60;
            
            // Change Face Logic (Every 7 seconds)
            if(millis() > nextFaceChange) {
                nextFaceChange = millis() + 7000;
                
                if (faceMode == 2) { // Manual Mode
                    currentMod = 0;
                    switch(manualFaceID) {
                        case 0: currentEyeL=1; currentEyeR=1; currentMouth=0; break; // Happy
                        case 1: currentEyeL=3; currentEyeR=3; currentMouth=1; currentMod=1; break; // Sleepy
                        case 2: currentEyeL=8; currentEyeR=8; currentMouth=0; currentMod=3; break; // Love
                        case 3: currentEyeL=12; currentEyeR=12; currentMouth=0; break; // Cool
                        case 4: currentEyeL=9; currentEyeR=9; currentMouth=5; break; // Goofy
                        case 5: currentEyeL=11; currentEyeR=11; currentMouth=3; break; // Sad
                        case 6: currentEyeL=7; currentEyeR=7; currentMouth=1; currentMod=6; break; // Angry
                        case 7: currentEyeL=7; currentEyeR=7; currentMouth=1; break; // Focused
                        case 8: currentEyeL=2; currentEyeR=2; currentMouth=4; break; // Cat
                        case 9: currentEyeL=4; currentEyeR=4; currentMouth=2; currentMod=2; break; // Dizzy
                    }
                } 
                else if (faceMode == 1) { // Shuffle Mode
                    // Build list of allowed faces based on categories
                    // 0:Happy, 1:Sleepy, 2:Love, 3:Cool, 4:Goofy, 5:Sad, 6:Angry, 7:Focused, 8:Cat, 9:Dizzy
                    int allowed[10];
                    int count = 0;
                    
                    if(catHappy) { allowed[count++] = 0; allowed[count++] = 2; allowed[count++] = 3; allowed[count++] = 8; }
                    if(catSleepy) { allowed[count++] = 1; allowed[count++] = 5; }
                    if(catGoofy) { allowed[count++] = 4; allowed[count++] = 9; }
                    
                    // Always allow at least Happy if nothing selected
                    if(count == 0) { allowed[0] = 0; count = 1; }
                    
                    int pick = allowed[random(0, count)];
                    
                    // Apply the picked face (Same switch as manual)
                    currentMod = 0;
                    switch(pick) {
                        case 0: currentEyeL=1; currentEyeR=1; currentMouth=0; break;
                        case 1: currentEyeL=3; currentEyeR=3; currentMouth=1; currentMod=1; break;
                        case 2: currentEyeL=8; currentEyeR=8; currentMouth=0; currentMod=3; break;
                        case 3: currentEyeL=12; currentEyeR=12; currentMouth=0; break;
                        case 4: currentEyeL=9; currentEyeR=9; currentMouth=5; break;
                        case 5: currentEyeL=11; currentEyeR=11; currentMouth=3; break;
                        case 6: currentEyeL=7; currentEyeR=7; currentMouth=1; currentMod=6; break;
                        case 7: currentEyeL=7; currentEyeR=7; currentMouth=1; break;
                        case 8: currentEyeL=2; currentEyeR=2; currentMouth=4; break;
                        case 9: currentEyeL=4; currentEyeR=4; currentMouth=2; currentMod=2; break;
                    }
                }
                else { // Smart Auto Mode
                    // Default: Happy/Normal
                    int possibleEyes[] = {0, 1, 2, 5, 6}; // Normal, Happy, Wide, Look L, Look R
                    int possibleMouths[] = {0, 1, 2, 4}; // Smile, Neutral, Open, Cat
                    currentMod = 0;

                    // Logic
                    if (t > 28.0) { // Hot
                        currentEyeL = 4; currentEyeR = 4; // X X
                        currentMouth = 2; // Open
                        currentMod = 2; // Sweat
                    } else if (t < 18.0) { // Cold
                        currentEyeL = 7; currentEyeR = 7; // > <
                        currentMouth = 3; // Sad
                        currentMod = 0;
                    } else if (hour >= 4 && hour < 8) { // Sleepy Time (4AM - 8AM)
                        currentEyeL = 3; currentEyeR = 3; // - -
                        currentMouth = 1; // -
                        currentMod = 1; // Zzz
                    } else if (hour >= 0 && hour < 4) { // Night Owl / Gaming (12AM - 4AM)
                        // Randomly Focused or Happy
                        if(random(0, 2) == 0) {
                            currentEyeL = 7; currentEyeR = 7; // > < (Focused)
                            currentMouth = 1; // -
                        } else {
                            currentEyeL = 2; currentEyeR = 2; // O O (Wide Awake)
                            currentMouth = 0; // U
                        }
                        currentMod = 0;
                    } else { // Day Time
                        // Random Shuffle
                        currentEyeL = possibleEyes[random(0, 5)];
                        currentEyeR = currentEyeL; // Symmetric usually
                        
                        // Occasional Wink
                        if(random(0, 5) == 0) {
                            currentEyeR = 1; // ^
                            currentEyeL = 3; // -
                        }
                        
                        currentMouth = possibleMouths[random(0, 4)];
                        
                        // Occasional Blush
                        if(random(0, 4) == 0) currentMod = 3;
                    }
                }
            }

            display.clearDisplay();
            
            // Draw Face
            drawEye(40, 25, currentEyeL);
            drawEye(88, 25, currentEyeR);
            drawMouth(64, 45, currentMouth);
            drawModifier(currentMod);
            
            // Draw Info Bar (Time | Temp | Hum)
            display.setTextSize(1);
            
            // Time
            display.setCursor(4, 55);
            if(hour < 10) display.print("0");
            display.print(hour);
            display.print(":");
            if(minute < 10) display.print("0");
            display.print(minute);
            
            // Temp
            display.setCursor(50, 55);
            if(isnan(t)) display.print("--C");
            else {
                display.print((int)t);
                display.print("C");
            }

            // Humidity
            display.setCursor(90, 55);
            if(isnan(h)) display.print("--%");
            else {
                display.print((int)h);
                display.print("%");
            }
            
            display.display();
        }
    }
}
