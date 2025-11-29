#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

Preferences preferences;
int storedW[4] = {0, 0, 0, 0};
int storedR = 0;
int storedG = 0;
int storedB = 0;
int saveInterval = 5000;
bool settingsChanged = false;
unsigned long lastChangeTime = 0;

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
int currentEffect = 0; // 0: None, 1: Rainbow
unsigned long lastEffectUpdate = 0;

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
    });
};

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

function setEffect(mode) {
    fetch(`/setEffect?mode=${mode}`);
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
  json += "\"interval\":" + String(saveInterval/1000);
  json += "}";
  server.send(200, "application/json", json);
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
  currentEffect = 0; 
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

void handleSetEffect() {
  if (server.hasArg("mode")) {
    currentEffect = server.arg("mode").toInt();
  }
  server.send(200, "text/plain", "OK");
}

void handleAllOff() {
  currentEffect = 0;
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

    // ---------------- WIFI SETUP ----------------
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.on("/", handleRoot);
    server.on("/getState", handleGetState);
    server.on("/setW", handleSetW);
    server.on("/setRGB", handleSetRGB);
    server.on("/setText", handleSetText);
    server.on("/setEffect", handleSetEffect);
    server.on("/allOff", handleAllOff);
    server.on("/setAllW", handleSetAllW);
    server.on("/setSaveInterval", handleSetSaveInterval);
    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
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
        settingsChanged = false;
        Serial.println("Settings saved to flash");
    }

    // Rainbow Effect
    if (currentEffect == 1) {
        static int pos = 0;
        if (millis() - lastEffectUpdate > 20) {
            pos++;
            if(pos > 255) pos = 0;
            
            int r, g, b;
            if(pos < 85) {
                r = 255 - pos * 3;
                g = 0;
                b = pos * 3;
            } else if(pos < 170) {
                pos -= 85;
                r = 0;
                g = pos * 3;
                b = 255 - pos * 3;
            } else {
                pos -= 170;
                r = pos * 3;
                g = 255 - pos * 3;
                b = 0;
            }
            ledcWrite(redChannel, r);
            ledcWrite(greenChannel, g);
            ledcWrite(blueChannel, b);
            lastEffectUpdate = millis();
        }
    }
}
