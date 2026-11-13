#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <LCD_I2C.h>

// ---------------- PIN CONFIGURATION ----------------

#define EC_DIGITAL D5
#define EC_ANALOG  A0

#define LCD_SDA D2
#define LCD_SCL D1

// ---------------- LCD CONFIGURATION ----------------

LCD_I2C lcd(0x27, 16, 2);

// ---------------- WIFI CONFIGURATION ----------------

const char* ssid = "IOTCLOUD";
const char* password = "";

ESP8266WebServer server(80);

bool wifiConnected = false;
bool detectionStarted = false;
bool sampleDetected = false;
bool authenticated = false;

// Preserve the most recent valid sensor reading for the chat assistant.
bool hasValidReading = false;
bool hasFinalResult = false;
int finalRawValue = 0;
float finalEC = 0;
float finalPH = 0;
float finalSalt = 0;
float finalConfidence = 0;
String finalProduct = "Milk";
String finalStatus = "No completed test";

String selectedProduct = "Milk";

// ---------------- SENSOR VARIABLES ----------------

int ecRawValue = 0;
int digitalValue = HIGH;

float conductivityIndex = 0;
float adulterationConfidence = 0;

String resultStatus = "Waiting";
String productName = "Milk";

// ---------------- CALIBRATION VALUES ----------------

// IMPORTANT:
// Replace these example ADC limits with experimentally
// measured reference values for your own sensor and samples.
//
// These are illustrative software thresholds, NOT
// established food safety or adulteration standards.

struct Calibration {
  String name;
  int minimum;
  int maximum;
};

Calibration products[] = {
  {"Milk",    300, 600},
  {"Oil",     200, 500},
  {"Spices",  250, 650},
  {"Pulses",  300, 700}
};

const int PRODUCT_COUNT = 4;

// ---------------- LCD TITLE SCROLL ----------------

void scrollTitle() {

  String title = " FOOD NUTRITION ANALYZER ";

  title = "                " + title + "                ";

  for (int i = 0; i <= title.length() - 16; i++) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(title.substring(i, i + 16));

    lcd.setCursor(0, 1);
    lcd.print("   NUTRIGUARD     ");

    delay(250);
  }
}

// ---------------- WIFI LCD DISPLAY ----------------

void connectWiFi() {

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi SSID:");

  lcd.setCursor(0, 1);
  lcd.print(ssid);

  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Password:");

  lcd.setCursor(0, 1);
  lcd.print(password);

  delay(2000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  int retry = 0;

  while (WiFi.status() != WL_CONNECTED && retry < 10) {

    lcd.setCursor(0, 1);
    lcd.print("Retry: ");
    lcd.print(retry + 1);
    lcd.print("/10     ");

    delay(1000);

    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {

    wifiConnected = true;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");

    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());

    delay(4000);

  } else {

    wifiConnected = false;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");

    lcd.setCursor(0, 1);
    lcd.print("Restart Required");

    delay(4000);
  }
}

float estimatedPH = 0;
float estimatedSalt = 0;
float estimatedEC = 0;

// ---------------- LOGIN PAGE ----------------

String loginPage() {

  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Food Analyzer Login</title>

<style>
body {
  background:#101827;
  font-family:Arial;
  color:white;
  text-align:center;
  padding-top:80px;
}

.box {
  background:#1e293b;
  padding:30px;
  margin:auto;
  width:85%;
  max-width:350px;
  border-radius:15px;
}

input {
  width:90%;
  padding:12px;
  margin:10px 0;
  border-radius:6px;
  border:0;
  box-sizing:border-box;
}

button {
  background:#06b6d4;
  color:white;
  padding:12px;
  width:95%;
  border:0;
  border-radius:6px;
  font-weight:bold;
}
</style>
</head>

<body>

<div class="box">

<h2>Food Nutrition Analyzer</h2>
<h3>Login</h3>

<form action="/login" method="POST">

<input name="username" placeholder="Username" required>

<input name="password" type="password"
placeholder="Password" required>

<button type="submit">LOGIN</button>

</form>

</div>

</body>
</html>
)rawliteral";

  return page;
}

// ---------------- DASHBOARD PAGE ----------------


String dashboardPage() {

  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>Food Quality Analyzer</title>

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>

* {
  box-sizing:border-box;
}

body {
  margin:0;
  padding:20px;
  background:#080d17;
  color:#e5e7eb;
  font-family:Arial,Helvetica,sans-serif;
  font-size:13px;
}

.main {
  max-width:1150px;
  margin:auto;
}

header {
  display:flex;
  justify-content:space-between;
  align-items:center;
  border-bottom:1px solid #263244;
  padding-bottom:15px;
  margin-bottom:18px;
}

h2 {
  margin:0;
  font-size:19px;
  color:white;
}

.subtitle {
  color:#8995a7;
  font-size:11px;
  margin-top:5px;
}

.user {
  background:#062a24;
  color:#10d69a;
  padding:8px 12px;
  border:1px solid #075b47;
  border-radius:20px;
  font-size:11px;
}

.logout {
  background:#273244;
  color:white;
  padding:8px 13px;
  border:0;
  border-radius:7px;
  cursor:pointer;
  margin-left:6px;
}

.section-title {
  color:#9ca3af;
  font-size:11px;
  font-weight:bold;
  margin:18px 0 10px;
}

.products {
  display:grid;
  grid-template-columns:repeat(4,1fr);
  gap:9px;
}

.product {
  background:#121a29;
  border:1px solid #293548;
  border-radius:9px;
  padding:16px 5px;
  text-align:center;
  cursor:pointer;
  color:#e5e7eb;
  transition:0.2s;
}

.product:hover {
  border-color:#10b981;
}

.product.active {
  background:#06251f;
  border:1px solid #10c981;
}

.emoji {
  width:35px;
  height:35px;
  margin:0 auto 9px;
  display:flex;
  align-items:center;
  justify-content:center;
  border-radius:9px;
  background:#193a35;
  color:#10d69a;
  font-size:19px;
  font-weight:bold;
}

.product .emoji {
  font-size:23px;
  margin-bottom:8px;
}

.product strong {
  font-size:12px;
}

.grid {
  display:grid;
  grid-template-columns:0.85fr 1.15fr;
  gap:15px;
  margin-top:16px;
}

.card {
  background:#101827;
  border:1px solid #293548;
  border-radius:10px;
  padding:15px;
  min-width:0;
}

.card h3 {
  font-size:13px;
  margin:0 0 16px;
  color:white;
}

.pin {
  display:flex;
  justify-content:space-between;
  background:#080e19;
  border:1px solid #293548;
  border-radius:7px;
  padding:11px;
  margin-bottom:12px;
  font-size:11px;
}

.pin-status {
  color:#f87171;
  background:#3a1820;
  padding:5px 8px;
  border-radius:5px;
  font-weight:bold;
}

.sensor-info {
  color:#9ca3af;
  font-size:11px;
  margin:12px 0 7px;
}

.sensor-value {
  float:right;
  color:white;
  font-weight:bold;
}

input[type=range] {
  width:100%;
  accent-color:#10c981;
}

.scale {
  display:flex;
  justify-content:space-between;
  color:#68758a;
  font-size:9px;
}

.start {
  width:100%;
  padding:13px;
  background:#10b981;
  color:#041b15;
  font-weight:bold;
  border:0;
  border-radius:8px;
  margin-top:16px;
  cursor:pointer;
}

.start:hover {
  background:#34d399;
}

.stop {
  width:100%;
  padding:10px;
  background:#273244;
  color:white;
  border:0;
  border-radius:8px;
  margin-top:8px;
  cursor:pointer;
}

.metrics {
  display:grid;
  grid-template-columns:repeat(3,1fr);
  gap:9px;
}

.metric {
  background:#080e19;
  border:1px solid #293548;
  padding:13px 5px;
  border-radius:7px;
  text-align:center;
}

.metric small {
  display:block;
  color:#9ca3af;
  font-size:10px;
  margin-bottom:8px;
}

.metric strong {
  font-size:17px;
  color:white;
}

.metric .unit {
  font-size:10px;
  color:#9ca3af;
}

.status {
  display:inline-block;
  padding:6px 11px;
  border-radius:20px;
  font-size:10px;
  background:#123b2c;
  color:#34d399;
  border:1px solid #176b48;
}

.status.danger {
  background:#451a22;
  color:#f87171;
  border-color:#9f3545;
}

.status.waiting {
  background:#283247;
  color:#cbd5e1;
  border-color:#475569;
}

.confidence {
  margin-top:17px;
}

.confidence-head {
  display:flex;
  justify-content:space-between;
  font-size:11px;
  margin-bottom:9px;
}

.bar {
  width:100%;
  height:8px;
  background:#293548;
  border-radius:10px;
  overflow:hidden;
}

.bar-fill {
  height:100%;
  width:0%;
  background:#10c981;
  transition:0.3s;
}

.warning {
  margin-top:14px;
  padding:13px;
  border-radius:8px;
  background:#251923;
  border:1px solid #65303a;
  color:#d1c2c7;
  font-size:11px;
  line-height:1.6;
}

.graph-card {
  margin-top:16px;
}

.graph-head {
  display:flex;
  justify-content:space-between;
  align-items:center;
  flex-wrap:wrap;
  gap:8px;
  margin-bottom:15px;
}

.graph-head h3 {
  margin:0;
}

.graph {
  position:relative;
  height:260px;
}

.footer {
  text-align:center;
  color:#64748b;
  font-size:10px;
  padding:20px;
}

/* AI-style question and answer panel */
.chat-card { margin-top:16px; }
.chat-log { background:#080e19; border:1px solid #293548; border-radius:8px; padding:12px; min-height:110px; max-height:260px; overflow-y:auto; }
.chat-message { padding:10px 12px; border-radius:8px; margin:7px 0; line-height:1.5; white-space:pre-wrap; overflow-wrap:anywhere; }
.chat-user { background:#123b2c; color:#d1fae5; margin-left:12%; }
.chat-bot { background:#1b2638; color:#e2e8f0; margin-right:8%; }
.chat-row { display:flex; gap:8px; margin-top:10px; }
.chat-row input { flex:1; min-width:0; background:#080e19; color:white; border:1px solid #334155; border-radius:7px; padding:12px; }
.chat-row button,.quick-question { background:#0e9f78; color:white; border:0; border-radius:7px; padding:10px 14px; cursor:pointer; }
.quick-questions { display:flex; flex-wrap:wrap; gap:7px; margin-top:10px; }
.quick-question { background:#273548; font-size:11px; }

@media(max-width:700px) {

  body {
    padding:10px;
  }

  header {
    align-items:flex-start;
    gap:10px;
  }

  h2 {
    font-size:15px;
  }

  .grid {
    grid-template-columns:1fr;
  }

  .products {
    grid-template-columns:repeat(2,1fr);
  }

  .metrics {
    grid-template-columns:repeat(3,1fr);
  }

  .metric strong {
    font-size:13px;
  }

  .graph {
    height:220px;
  }

}

</style>
</head>

<body>

<div class="main">

<header>

<div>
<h2>Real-Time Food Quality & Adulteration Analyzer</h2>
<div class="subtitle">
NodeMCU Sensor Processing & Parameter Estimation
</div>
</div>

<div>
<span class="user">User: admin</span>
<button class="logout" onclick="location.href='/logout'">
Logout
</button>
</div>

</header>

<div class="section-title">
SELECT SAMPLE TESTING CATEGORY
</div>

<div class="products">

<div class="product active" onclick="selectProduct('Milk',this)">
  <div class="emoji">M</div>
  <strong>Milk</strong>
</div>

<div class="product" onclick="selectProduct('Oil',this)">
  <div class="emoji">O</div>
  <strong>Oil</strong>
</div>

<div class="product" onclick="selectProduct('Spices',this)">
  <div class="emoji">S</div>
  <strong>Spices</strong>
</div>

<div class="product" onclick="selectProduct('Pulses',this)">
  <div class="emoji">P</div>
  <strong>Pulses</strong>
</div>

</div>

<div class="grid">

<!-- SENSOR CONTROL PANEL -->

<div class="card">

<h3>Sensor Simulation Controls</h3>

<div class="pin">

<div>
<strong>DO Digital Interrupt Pin (D5)</strong>
<div class="subtitle">
LOW signal triggers sensing process
</div>
</div>

<span class="pin-status" id="digitalStatus">
HIGH (Inactive)
</span>

</div>

<div class="sensor-info">
AO Analog Sensor Input (A0)
<span class="sensor-value" id="analogValue">
0 ADC
</span>
</div>

<input type="range"
id="analogSlider"
min="0"
max="1023"
value="0"
disabled>

<div class="scale">
<span>0 ADC</span>
<span>512 ADC</span>
<span>1023 ADC</span>
</div>

<div style="margin-top:16px;font-size:11px;color:#9ca3af">
Selected Product:
<strong style="color:#10d69a" id="selectedProduct">
Milk
</strong>
</div>

<button class="start" onclick="startDetection()">
Start Adulteration Detection Process
</button>

<button class="stop" onclick="stopDetection()">
STOP DETECTION
</button>

<div class="subtitle" style="margin-top:12px" id="processMessage">
System ready. Select a food sample to begin.
</div>

</div>

<!-- RESULT PANEL -->

<div class="card">

<div style="display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:17px">

<h3 style="margin:0">Detection Analysis & Result</h3>

<span class="status waiting" id="resultBadge">
WAITING
</span>

</div>

<div class="metrics">

<div class="metric">
<small>Acidity Level</small>
<strong id="phValue">N/A</strong>
<div class="unit">pH</div>
</div>

<div class="metric">
<small>Salt / Salinity</small>
<strong id="saltValue">N/A</strong>
<div class="unit">ppt</div>
</div>

<div class="metric">
<small>Conductivity</small>
<strong id="ecValue">0</strong>
<div class="unit">µS/cm</div>
</div>

</div>

<div class="confidence">

<div class="confidence-head">
<strong>Detection Confidence Estimate</strong>
<strong id="confidenceText">0%</strong>
</div>

<div class="bar">
<div class="bar-fill" id="confidenceBar"></div>
</div>

</div>

<div class="warning" id="warningMessage">
● Waiting for sample. Start the detection process to obtain sensor readings.
</div>

</div>

</div>

<!-- SENSOR QUESTION ASSISTANT -->
<div class="card chat-card">
  <h3>Food Analyzer Chat Assistant</h3>
  <div class="subtitle" style="margin-bottom:12px">Ask questions about the current reading or the last completed test.</div>
  <div class="chat-log" id="chatLog" aria-live="polite">
    <div class="chat-message chat-bot">Hello! I can show the latest test result, conductivity, pH, salt estimate, ADC value and confidence estimate. Start a test and place a sample, or ask about the last completed test.</div>
  </div>
  <div class="quick-questions">
    <button class="quick-question" onclick="askQuick('What is the final result?')">Final result</button>
    <button class="quick-question" onclick="askQuick('Show all sensor values')">Show all values</button>
    <button class="quick-question" onclick="askQuick('What is the pH value?')">pH value</button>
    <button class="quick-question" onclick="askQuick('Was adulteration detected?')">Adulteration status</button>
  </div>
  <div class="chat-row">
    <input id="chatQuestion" type="text" placeholder="Ask about your food sample..." onkeydown="if(event.key==='Enter') askAssistant()">
    <button onclick="askAssistant()">Send</button>
  </div>
</div>

<!-- GRAPH -->

<div class="card graph-card">

<div class="graph-head">

<h3>Real-Time Sensing Telemetry & Estimation Graph</h3>

<span class="subtitle">Sampling Rate: 1 Hz</span>

</div>

<div class="graph">
<canvas id="sensorChart"></canvas>
</div>

</div>

<div class="footer">
FOOD QUALITY & ADULTERATION ANALYZER |
IoT BASED MONITORING SYSTEM
</div>

</div>

<script>

let selected = "Milk";
let running = false;
let chartLabels = [];
let chartValues = [];
let conductivityData = [];
let acidityData = [];
let saltData = [];

let testingRecords = [];
let readingNumber = 0;

const ctx = document.getElementById('sensorChart');

const chart = new Chart(ctx, {

  type: 'line',

  data: {

    labels: chartLabels,

    datasets: [

      {
        label: 'Conductivity (µS/cm)',
        data: conductivityData,
        borderColor: '#10c981',
        backgroundColor: 'transparent',
        borderWidth: 2,
        pointRadius: 2,
        tension: 0.3,
        yAxisID: 'y'
      },

      {
        label: 'Acidity (pH × 100)',
        data: acidityData,
        borderColor: '#38bdf8',
        backgroundColor: 'transparent',
        borderWidth: 2,
        borderDash: [5, 4],
        pointRadius: 2,
        tension: 0.3,
        yAxisID: 'y'
      },

      {
        label: 'Salt / Salinity (ppt × 200)',
        data: saltData,
        borderColor: '#f59e0b',
        backgroundColor: 'transparent',
        borderWidth: 2,
        pointRadius: 2,
        tension: 0.3,
        yAxisID: 'y'
      }

    ]

  },

  options: {

    responsive: true,
    maintainAspectRatio: false,
    animation: false,

    interaction: {
      intersect: false,
      mode: 'index'
    },

    plugins: {

      legend: {
        display: true,
        position: 'top',

        labels: {
          color: '#cbd5e1',
          usePointStyle: true,
          boxWidth: 12,
          font: {
            size: 10
          }
        }

      }

    },

    scales: {

      x: {
        ticks: {
          color: '#9ca3af',
          maxTicksLimit: 8
        },
        grid: {
          color: '#243044'
        }
      },

      y: {
        beginAtZero: true,
        suggestedMax: 1023,

        ticks: {
          color: '#9ca3af'
        },

        grid: {
          color: '#243044'
        }

      }

    }

  }

});

// PRODUCT SELECTION

function selectProduct(name,element) {

if(running) {
alert("Stop the current detection before changing product.");
return;
}

selected = name;

document.querySelectorAll('.product').forEach(p=>{
p.classList.remove('active');
});

element.classList.add('active');

document.getElementById('selectedProduct').innerText=name;

document.getElementById('processMessage').innerText=
name+" selected. Ready for testing.";

}

// START DETECTION

function startDetection() {

fetch('/start?product='+encodeURIComponent(selected))

.then(r=>{

if(!r.ok) throw new Error("Unable to start");

return r.text();

})

.then(msg=>{

running=true;

document.getElementById('processMessage').innerText=
"Detection started for "+selected;

document.getElementById('resultBadge').innerText=
"PROCESSING";

document.getElementById('resultBadge').className=
"status waiting";

document.getElementById('warningMessage').innerText=
"Waiting for the sample to activate the digital sensor.";

})

.catch(err=>alert(err.message));

}

// STOP DETECTION
function stopDetection() {

  if(!running) {
    alert("Testing is not currently running.");
    return;
  }

  fetch('/stop')

  .then(r => r.text())

  .then(msg => {

    // Stop collecting new readings
    running = false;

    document.getElementById('processMessage').innerText =
      "Testing stopped. Generating Excel report...";

    document.getElementById('resultBadge').innerText =
      "TEST COMPLETED";

    document.getElementById('resultBadge').className =
      "status";

    // Generate report automatically
    setTimeout(function() {

      downloadExcelReport();

    }, 500);

  })

  .catch(err => {

    alert("Unable to stop testing: " + err.message);

  });

}

// UPDATE SENSOR VALUES

function updateData() {

fetch('/data')

.then(r=>{

if(!r.ok) throw new Error("Data unavailable");

return r.json();

})

.then(data=>{

document.getElementById('selectedProduct').innerText=
data.product;

document.getElementById('analogValue').innerText=
data.analog+" ADC";

document.getElementById('analogSlider').value=
data.analog;

document.getElementById('phValue').innerText =
    Number(data.ph).toFixed(2);

document.getElementById('saltValue').innerText =
    Number(data.salt).toFixed(2);

document.getElementById('ecValue').innerText =
    Number(data.conductivity).toFixed(2);
    

let digital = document.getElementById('digitalStatus');

if(data.detected) {

digital.innerText="LOW (ACTIVE)";
digital.style.color="#10d69a";

} else {

digital.innerText="HIGH / INACTIVE";
digital.style.color="#f87171";

}

document.getElementById('confidenceText').innerText=
data.confidence.toFixed(1)+"%";

document.getElementById('confidenceBar').style.width=
data.confidence+"%";

let badge=document.getElementById('resultBadge');

let warning=document.getElementById('warningMessage');

if(!running) {

badge.innerText="STOPPED";
badge.className="status waiting";

} else if(!data.detected) {

badge.innerText="WAITING FOR SAMPLE";
badge.className="status waiting";

warning.innerText=
"Place the selected food sample on the EC sensor.";

} else if(data.status=="POSSIBLE ADULTERATION") {

badge.innerText="POSSIBLE ADULTERATION";
badge.className="status danger";

warning.innerText=
"Warning: Conductivity reading is outside the calibrated "+
data.product+" reference range. Further laboratory verification is required.";

} else {

badge.innerText="REFERENCE RANGE MATCH";
badge.className="status";

warning.innerText=
"Conductivity reading is within the calibrated "+
data.product+" reference range.";

}

// GRAPH UPDATE

// GRAPH UPDATE - THREE PARAMETERS

// SAVE EACH SENSOR READING

if(running && data.detected) {

  readingNumber++;

  testingRecords.push({

    sno: readingNumber,

    datetime: new Date().toLocaleString(),

    product: data.product,

    digital: "LOW",

    analog: data.analog,

    conductivity: data.conductivity,

    acidity: data.ph,

    salt: data.salt,

    confidence: data.confidence,

    result: data.status

  });

}

if(running && data.detected) {

  chartLabels.push(new Date().toLocaleTimeString());

  // Green: Conductivity
  conductivityData.push(data.conductivity);

  // Blue: Acidity (scaled pH)
  acidityData.push(data.ph * 100);

  // Orange: Salt (scaled salinity)
  saltData.push(data.salt * 200);

  // Keep latest 30 readings

  if(chartLabels.length > 30) {

    chartLabels.shift();
    conductivityData.shift();
    acidityData.shift();
    saltData.shift();

  }

  chart.update('none');
}

})


.catch(err=>{

console.log("Sensor update:",err.message);

});

}

function downloadExcelReport() {

  if(testingRecords.length === 0) {

    alert("No sensor readings available for Excel report.");

    return;

  }

  // Excel column headings

  let csv = [];

  csv.push([
    "S.No",
    "Date & Time",
    "Food Product",
    "Digital Output",
    "Analog ADC",
    "Conductivity",
    "Acidity pH",
    "Salt Salinity",
    "Confidence %",
    "Detection Result"
  ]);

  // Add all testing records

  testingRecords.forEach(row => {

    csv.push([

      row.sno,
      row.datetime,
      row.product,
      row.digital,
      row.analog,
      row.conductivity,
      row.acidity,
      row.salt,
      row.confidence,
      row.result

    ]);

  });

  // Convert to CSV format

  let csvContent = csv.map(row =>

    row.map(value => {

      let text = String(value ?? "");

      return '"' + text.replace(/"/g, '""') + '"';

    }).join(",")

  ).join("\r\n");

  // UTF-8 BOM for Excel compatibility

  let blob = new Blob(
    ["\uFEFF" + csvContent],
    {
      type:"text/csv;charset=utf-8;"
    }
  );

  let url = URL.createObjectURL(blob);

  let link = document.createElement("a");

  let now = new Date();

  let filename =
    "Food_Analyzer_" +
    selected + "_" +
    now.getFullYear() +
    (now.getMonth()+1).toString().padStart(2,'0') +
    now.getDate().toString().padStart(2,'0') +
    "_" +
    now.getHours().toString().padStart(2,'0') +
    now.getMinutes().toString().padStart(2,'0') +
    ".csv";

  link.href = url;

  link.download = filename;

  document.body.appendChild(link);

  link.click();

  document.body.removeChild(link);

  URL.revokeObjectURL(url);

  document.getElementById('processMessage').innerText =
    "Excel report generated successfully! " +
    testingRecords.length + " readings saved.";

}



function addChatMessage(message, who) {
  const log = document.getElementById('chatLog');
  const item = document.createElement('div');
  item.className = 'chat-message ' + (who === 'user' ? 'chat-user' : 'chat-bot');
  item.textContent = message;
  log.appendChild(item);
  log.scrollTop = log.scrollHeight;
}

function askQuick(question) {
  document.getElementById('chatQuestion').value = question;
  askAssistant();
}

function askAssistant() {
  const input = document.getElementById('chatQuestion');
  const question = input.value.trim();
  if (!question) return;
  addChatMessage(question, 'user');
  input.value = '';
  fetch('/ask?question=' + encodeURIComponent(question))
    .then(response => {
      if (!response.ok) throw new Error('Assistant request failed. Please login again.');
      return response.text();
    })
    .then(answer => addChatMessage(answer, 'bot'))
    .catch(error => addChatMessage(error.message, 'bot'));
}

setInterval(updateData,1000);

updateData();

</script>

</body>
</html>
)rawliteral";

  return page;
}


// ---------------- AUTHENTICATION ----------------

bool isAuthorized() {

  if (server.hasHeader("Cookie")) {

    String cookie = server.header("Cookie");

    if (cookie.indexOf("auth=1") >= 0) {
      return true;
    }
  }

  return false;
}

// ---------------- DASHBOARD CHAT ASSISTANT ----------------

void handleAsk() {
  if (!isAuthorized()) {
    server.send(401, "text/plain", "Please login to use the assistant.");
    return;
  }

  String question = server.arg("question");
  question.toLowerCase();
  question.trim();

  bool useFinal = hasFinalResult;
  bool useLive = !useFinal && hasValidReading;
  if (!useFinal && !useLive) {
    server.send(200, "text/plain", "No valid sample reading is available yet. Select a food category, press Start, and place the sample on the sensor. After a reading appears, ask me again.");
    return;
  }

  String product = useFinal ? finalProduct : selectedProduct;
  int raw = useFinal ? finalRawValue : ecRawValue;
  float ec = useFinal ? finalEC : estimatedEC;
  float ph = useFinal ? finalPH : estimatedPH;
  float salt = useFinal ? finalSalt : estimatedSalt;
  float confidence = useFinal ? finalConfidence : adulterationConfidence;
  String status = useFinal ? finalStatus : resultStatus;

  String answer = "";
  if (question.indexOf("hello") >= 0 || question.indexOf("hi") == 0) {
    answer = "Hello! I am the Food Analyzer assistant. Ask me about the latest sample values or its screening result.";
  } else if (question.indexOf("help") >= 0 || question.indexOf("what can") >= 0) {
    answer = "You can ask: What is the final result? Show all values. What is the pH? What is the conductivity? What is the salt estimate? What is the ADC value? Was adulteration detected?";
  } else if (question.indexOf("result") >= 0 || question.indexOf("adulter") >= 0 || question.indexOf("safe") >= 0 || question.indexOf("status") >= 0) {
    answer = "Screening result for " + product + ": " + status + ". ";
    if (status == "POSSIBLE ADULTERATION") answer += "The ADC reading is outside the configured reference range. This is only a screening indication; confirm using an appropriate laboratory test.";
    else if (status == "REFERENCE RANGE MATCH") answer += "The ADC reading is within the configured reference range. This does not certify that the food is completely safe or free from adulterants.";
    else answer += "A completed sample reading is not available.";
  } else if (question.indexOf("all") >= 0 || question.indexOf("value") >= 0 || question.indexOf("reading") >= 0 || question.indexOf("summary") >= 0 || question.indexOf("detail") >= 0) {
    answer = "";
    answer += "Test: " + product + "\n";
    answer += "Analog ADC: " + String(raw) + "\n";
    answer += "Conductivity estimate: " + String(ec, 2) + "\n";
    answer += "pH estimate: " + String(ph, 2) + "\n";
    answer += "Salt estimate: " + String(salt, 2) + " ppt\n";
    answer += "Confidence estimate: " + String(confidence, 1) + "%\n";
    answer += "Result: " + status;
  } else if (question.indexOf("conduct") >= 0 || question.indexOf("ec") >= 0) {
    answer = "The conductivity estimate for " + product + " is " + String(ec, 2) + ". The sketch currently calculates this as ADC × 2; it is not a calibrated physical conductivity measurement unless you calibrate the sensor with EC standards.";
  } else if (question.indexOf("ph") >= 0 || question.indexOf("acid") >= 0) {
    answer = "The estimated pH value for " + product + " is " + String(ph, 2) + ". This value is calculated from the EC sensor's ADC reading and is not a direct pH measurement.";
  } else if (question.indexOf("salt") >= 0 || question.indexOf("salin") >= 0) {
    answer = "The estimated salt/salinity value for " + product + " is " + String(salt, 2) + " ppt. It is a software estimate derived from the same ADC reading, not an independent salt measurement.";
  } else if (question.indexOf("confidence") >= 0 || question.indexOf("percent") >= 0) {
    answer = "The deviation-based confidence estimate is " + String(confidence, 1) + "%. It represents the ADC deviation from the configured reference range; it is not a statistically validated probability.";
  } else if (question.indexOf("analog") >= 0 || question.indexOf("adc") >= 0 || question.indexOf("raw") >= 0) {
    answer = "The last analog ADC reading for " + product + " is " + String(raw) + " (range 0 to 1023).";
  } else if (question.indexOf("product") >= 0 || question.indexOf("sample") >= 0 || question.indexOf("food") >= 0) {
    answer = "The recorded food category is " + product + ". Current screening result: " + status + ".";
  } else {
    answer = "I can answer questions about this test's result, ADC, conductivity estimate, pH estimate, salt estimate and confidence estimate. Try asking: 'Show all sensor values' or 'What is the final result?'";
  }

  if (useFinal) answer = "Last completed test\n" + answer;
  else answer = "Current live test\n" + answer;

  server.send(200, "text/plain", answer);
}

void handleRoot() {

  if (isAuthorized()) {
    server.send(200, "text/html", dashboardPage());
  } else {
    server.send(200, "text/html", loginPage());
  }
}

void handleLogin() {

  String username = server.arg("username");
  String userpass = server.arg("password");

  if (username == "admin" && userpass == "1234") {

    server.sendHeader("Set-Cookie",
                      "auth=1; Path=/; SameSite=Strict");

    server.sendHeader("Location", "/");

    server.send(303, "text/plain", "Login Successful");

  } else {

    server.send(401, "text/html",
                loginPage() + "<p>Invalid Username or Password</p>");
  }
}

void handleLogout() {

  server.sendHeader("Set-Cookie",
                    "auth=; Max-Age=0; Path=/");

  server.sendHeader("Location", "/");

  server.send(303, "text/plain", "Logged Out");
}

// ---------------- PRODUCT SELECTION ----------------

void handleStart() {

  if (!isAuthorized()) {
    server.send(401, "text/plain", "Please Login");
    return;
  }

  String product = server.arg("product");

  bool valid = false;

  for (int i = 0; i < PRODUCT_COUNT; i++) {

    if (product == products[i].name) {

      selectedProduct = product;
      productName = product;
      valid = true;
      break;
    }
  }

  if (!valid) {
    server.send(400, "text/plain", "Invalid Product");
    return;
  }

  detectionStarted = true;
  sampleDetected = false;
  hasValidReading = false;
  hasFinalResult = false;

  ecRawValue = 0;
  estimatedEC = 0;
  estimatedPH = 0;
  estimatedSalt = 0;
  adulterationConfidence = 0;
  resultStatus = "Waiting for Sample";

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Product Selected");

  lcd.setCursor(0, 1);
  lcd.print(selectedProduct);

  server.send(200, "text/plain",
              "Detection Started: " + selectedProduct);
}

void handleStop() {

  if (!isAuthorized()) {
    server.send(401, "text/plain", "Please Login");
    return;
  }

  detectionStarted = false;

  // Keep a snapshot of the last valid reading for the final result and chat.
  if (hasValidReading) {
    finalRawValue = ecRawValue;
    finalEC = estimatedEC;
    finalPH = estimatedPH;
    finalSalt = estimatedSalt;
    finalConfidence = adulterationConfidence;
    finalProduct = selectedProduct;
    finalStatus = resultStatus;
    hasFinalResult = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Test Completed");
    lcd.setCursor(0, 1);
    lcd.print(finalStatus == "POSSIBLE ADULTERATION" ? "Check Sample!" : "Reference Match");
  } else {
    hasFinalResult = false;
    finalStatus = "No sample reading was recorded";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No Sample Found");
    lcd.setCursor(0, 1);
    lcd.print("Try Again");
  }

  sampleDetected = false;
  server.send(200, "text/plain", hasFinalResult ? "Detection stopped. Final reading saved." : "Detection stopped. No sample reading was recorded.");
}

// ---------------- SENSOR CALCULATION ----------------

void readSensor() {

  digitalValue = digitalRead(EC_DIGITAL);

  if (!detectionStarted || digitalValue != LOW) {

    sampleDetected = false;

    return;
  }

  sampleDetected = true;
  hasValidReading = true;

  // Average of 10 ADC readings

  long total = 0;

  for (int i = 0; i < 10; i++) {

    total += analogRead(EC_ANALOG);

    delay(5);
  }

  ecRawValue = total / 10;
  // Conductivity estimation
estimatedEC = ecRawValue * 2.0;

// Estimated acidity (demo calibration)
estimatedPH = 7.0 - ((ecRawValue - 512.0) / 512.0) * 2.0;

estimatedPH = constrain(estimatedPH, 0.0, 14.0);

// Estimated salt / salinity (demo calibration)
estimatedSalt = (ecRawValue / 1023.0) * 1.5;

  // Normalized conductivity-related index.
  // This is not an actual EC measurement in mS/cm.

  conductivityIndex = (ecRawValue / 1023.0) * 100.0;

  int minimumValue = 0;
  int maximumValue = 1023;

  for (int i = 0; i < PRODUCT_COUNT; i++) {

    if (selectedProduct == products[i].name) {

      minimumValue = products[i].minimum;
      maximumValue = products[i].maximum;

      break;
    }
  }

  // Reference-range screening

  if (ecRawValue >= minimumValue &&
      ecRawValue <= maximumValue) {

    adulterationConfidence = 0;

    resultStatus = "REFERENCE RANGE MATCH";

  } else {

    float deviation = 0;

    if (ecRawValue < minimumValue) {

      deviation = minimumValue - ecRawValue;

    } else {

      deviation = ecRawValue - maximumValue;
    }

    float referenceSpan = maximumValue - minimumValue;

    if (referenceSpan < 1) {
      referenceSpan = 1;
    }

    adulterationConfidence =
        (deviation / referenceSpan) * 100.0;

    adulterationConfidence =
        constrain(adulterationConfidence, 0, 100);

    resultStatus = "POSSIBLE ADULTERATION";
  }

  // LCD display

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(selectedProduct);

  lcd.print(" EC:");

  lcd.print(ecRawValue);

  lcd.setCursor(0, 1);

  if (resultStatus == "REFERENCE RANGE MATCH") {

    lcd.print("Reference Match");

  } else {

    lcd.print("Check Sample!");
  }
}

// ---------------- SENSOR DATA API ----------------

void handleData() {

  if (!isAuthorized()) {
    server.send(401, "application/json",
                "{\"error\":\"Unauthorized\"}");
    return;
  }

  readSensor();

  String json = "{";

json += "\"product\":\"" + selectedProduct + "\",";

json += "\"detected\":";
json += sampleDetected ? "true," : "false,";

json += "\"analog\":";
json += String(ecRawValue) + ",";

json += "\"conductivity\":";
json += String(estimatedEC, 2) + ",";

json += "\"ph\":";
json += String(estimatedPH, 2) + ",";

json += "\"salt\":";
json += String(estimatedSalt, 2) + ",";

json += "\"confidence\":";
json += String(adulterationConfidence, 2) + ",";

json += "\"status\":\"" + resultStatus + "\"";

json += "}";

  server.send(200, "application/json", json);
}

// ---------------- SETUP ----------------


void setup() {

  Serial.begin(115200);
  pinMode(EC_DIGITAL, INPUT);

  lcd.begin();
  lcd.backlight();

  scrollTitle();
  connectWiFi();

  if (wifiConnected) {
    // ESP8266 Core 3.1.2 compatible
    server.collectHeaders("Cookie");
    server.on("/", HTTP_GET, handleRoot);
    server.on("/login", HTTP_POST, handleLogin);
    server.on("/logout", HTTP_GET, handleLogout);
    server.on("/start", HTTP_GET, handleStart);
    server.on("/stop", HTTP_GET, handleStop);
    server.on("/data", HTTP_GET, handleData);
    server.on("/ask", HTTP_GET, handleAsk);
    server.begin();

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Server Started");

    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());

    Serial.println();
    Serial.println("Food Analyzer Started");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("WiFi Connection Failed");

  }
}


// ---------------- MAIN LOOP ----------------

void loop() {

  if (wifiConnected) {

    server.handleClient();
  }
}