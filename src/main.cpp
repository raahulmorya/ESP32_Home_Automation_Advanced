#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MFRC522.h>
#include <SPI.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <Update.h>
#include <updatePage.h>

// Sinric Pro Configuration
#define APP_KEY "dd3cc009-0749-47a4-8781-xxxxxxx" // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET "cde80a70-26a9-417c-a95f-xxxxxxxxxx" // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define LIGHT_ID "683cadf6929fcxxxxxxxx"                                                    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define FAN_ID "683caxxxxxxxxxxx"                                                      // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define DOOR_ID "683cae706868exxxxxxxxxf"       // Should look like "5dc1564130xxxxxxxxxxxxxx"

#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <SinricProLight.h>
#include <SinricProDoorbell.h>

// WiFi Credentials
const char *ssid = "OPTIMUS";
const char *password = "qqwweeaaaa";

// Web Server
WebServer server(80);

// Pin Definitions
#define LIGHT_RELAY_PIN 18
#define FAN_RELAY_PIN 23
#define PIR_PIN 13
#define RED_LED_PIN 12
#define TOUCH_PIN_1 4
#define TOUCH_PIN_2 15
#define SERVO_PIN 14
#define DHT_PIN 26
#define FLAME_PIN 25
#define TOUCH_THRESHOLD 40 // Lower = more sensitive

unsigned long lastTouchCheck = 0;
const unsigned long touchInterval = 200; // ms debounce

// RFID Pins
#define SPI_MOSI 27
#define SPI_MISO 19
#define SPI_SCK 5
#define RST_PIN 16
#define SS_PIN 17

// OLED Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT Settings
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Servo
Servo doorLock;

// static unsigned long lastTouchTime = 0;

// Variables
bool lightState = false;
bool fanState = false;
bool doorLocked = true;
bool motionDetected = false;
bool flameDetected = false;
float temperature = 0;
float humidity = 0;

// Authorized RFID Cards
byte authorizedCards[][4] = {
    {0x5A, 0xB8, 0x01, 0x01}, // Your new card
    {0x9C, 0x89, 0x25, 0x03}  // Your previous card (keep if still needed)
};

// Web Server Handlers
void handleRoot();
void handleLight();
void handleFan();
void handleDoor();
void handleStatus();
void handleNotFound();

// Sensor and Control Functions
void readDHT();
void checkPIR();
void checkFlame();
void checkTouch();
void checkRFID();
void toggleLight();
void toggleFan();
void lockDoor();
void unlockDoor();
void updateDisplay();
void displayAlert(const char *message);
void handleUpload();
void handleUpdate();
bool onDoorState(const String &deviceId, bool &state);
bool onFanState(const String &deviceId, bool &state);
bool onLightState(const String &deviceId, bool &state);
void syncStatesWithSinricPro();

// WiFi Setup
void setupWiFi();

void setup()
{
  Serial.begin(115200);

  // Initialize components
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(TOUCH_PIN_1, INPUT);
  pinMode(TOUCH_PIN_2, INPUT);
  pinMode(FLAME_PIN, INPUT);

  digitalWrite(LIGHT_RELAY_PIN, HIGH);
  digitalWrite(FAN_RELAY_PIN, HIGH);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.display();
  delay(2000);
  display.clearDisplay();

  // Initialize RFID
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SS_PIN);

  mfrc522.PCD_Init();

  // Initialize DHT
  dht.begin();

  // Initialize Servo
  doorLock.attach(SERVO_PIN);
  lockDoor();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  display.print("Connecting to WiFi");
  display.display();

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    display.print(".");
    display.display();
  }

  // Sinric Pro setup
  SinricProSwitch &myLight = SinricPro[LIGHT_ID];
  myLight.onPowerState(onLightState);

  SinricProSwitch &myFan = SinricPro[FAN_ID];
  myFan.onPowerState(onFanState);

  SinricProDoorbell &myDoor = SinricPro[DOOR_ID];
  myDoor.onPowerState(onDoorState);

  SinricPro.onConnected([]()
                        { Serial.println("Connected to SinricPro"); });
  SinricPro.onDisconnected([]()
                           { Serial.println("Disconnected from SinricPro"); });

  SinricPro.begin(APP_KEY, APP_SECRET);

  display.clearDisplay();
  display.print("WiFi connected");
  display.print("IP: ");
  display.print(WiFi.localIP());
  display.display();
  delay(2000);

  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/light", handleLight);
  server.on("/fan", handleFan);
  server.on("/door", handleDoor);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);

  server.on("/update", HTTP_GET, []()
            {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", updateHTML); });

  server.on("/update", HTTP_POST, handleUpdate, handleUpload);

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();
  SinricPro.handle();

  // Read sensors
  readDHT();
  checkPIR();
  checkFlame();
  checkTouch();
  checkRFID();

  // Update display
  updateDisplay();

  delay(100);
}

void handleRoot()
{
  String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Home Dashboard</title>
  <style>
    :root {
      --primary: #4361ee;
      --secondary: #3f37c9;
      --accent: #4895ef;
      --danger: #f72585;
      --success: #4cc9f0;
      --dark: #212529;
      --light: #f8f9fa;
    }
    
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    }
    
    body {
      background-color: #f5f7fa;
      color: var(--dark);
      line-height: 1.6;
    }
    
    .container {
      max-width: 1200px;
      margin: 0 auto;
      padding: 2rem;
    }
    
    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 2rem;
      padding-bottom: 1rem;
      border-bottom: 1px solid #e1e5ee;
    }
    
    h1 {
      color: var(--primary);
      font-size: 2rem;
    }
    
    .status-badge {
      background-color: var(--light);
      padding: 0.5rem 1rem;
      border-radius: 50px;
      font-weight: 600;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
    }
    
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 2rem;
    }
    
    .card {
      background: white;
      border-radius: 12px;
      padding: 1.5rem;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
      transition: transform 0.3s ease, box-shadow 0.3s ease;
    }
    
    .card:hover {
      transform: translateY(-5px);
      box-shadow: 0 10px 20px rgba(0,0,0,0.1);
    }
    
    .card-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 1rem;
    }
    
    .card-title {
      font-size: 1.2rem;
      font-weight: 600;
      color: var(--dark);
    }
    
    .card-icon {
      width: 40px;
      height: 40px;
      display: flex;
      align-items: center;
      justify-content: center;
      background-color: rgba(67, 97, 238, 0.1);
      border-radius: 50%;
      color: var(--primary);
    }
    
    .card-body {
      margin-bottom: 1.5rem;
    }
    
    .toggle-switch {
      position: relative;
      display: inline-block;
      width: 60px;
      height: 34px;
    }
    
    .toggle-switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      transition: .4s;
      border-radius: 34px;
    }
    
    .slider:before {
      position: absolute;
      content: "";
      height: 26px;
      width: 26px;
      left: 4px;
      bottom: 4px;
      background-color: white;
      transition: .4s;
      border-radius: 50%;
    }
    
    input:checked + .slider {
      background-color: var(--primary);
    }
    
    input:checked + .slider:before {
      transform: translateX(26px);
    }
    
    .btn {
      display: inline-block;
      padding: 0.8rem 1.5rem;
      border-radius: 8px;
      font-weight: 600;
      text-decoration: none;
      text-align: center;
      cursor: pointer;
      transition: all 0.3s ease;
      border: none;
    }
    
    .btn-primary {
      background-color: var(--primary);
      color: white;
    }
    
    .btn-primary:hover {
      background-color: var(--secondary);
    }
    
    .sensor-value {
      font-size: 2.5rem;
      font-weight: 700;
      color: var(--primary);
      margin: 1rem 0;
    }
    
    .sensor-unit {
      font-size: 1rem;
      color: #6c757d;
    }
    
    .alert {
      padding: 1rem;
      border-radius: 8px;
      margin-bottom: 1rem;
      font-weight: 600;
    }
    
    .alert-danger {
      background-color: rgba(247, 37, 133, 0.1);
      color: var(--danger);
      border-left: 4px solid var(--danger);
    }
    
    .alert-success {
      background-color: rgba(76, 201, 240, 0.1);
      color: var(--success);
      border-left: 4px solid var(--success);
    }
    
    @media (max-width: 768px) {
      .grid {
        grid-template-columns: 1fr;
      }
      
      header {
        flex-direction: column;
        align-items: flex-start;
        gap: 1rem;
      }
    }
    
    /* Animation for alerts */
    @keyframes pulse {
      0% { transform: scale(1); }
      50% { transform: scale(1.02); }
      100% { transform: scale(1); }
    }
    
    .pulse {
      animation: pulse 2s infinite;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>Smart Home Dashboard</h1>
      <div class="status-badge">
        <span id="connection-status">Connected</span>
      </div>
    </header>
    
    <div class="grid" id="dashboard-grid">
      <!-- Content will be updated via AJAX -->
    </div>
  </div>

  <script>
    // Initial load
    document.addEventListener('DOMContentLoaded', function() {
      updateDashboard();
      setInterval(updateDashboard, 1000); // Update every second
    });

    // Update dashboard with fresh data
    function updateDashboard() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          const grid = document.getElementById('dashboard-grid');
          
          // Generate HTML based on current state
          let html = `
            <!-- Light Control Card -->
            <div class="card">
              <div class="card-header">
                <h2 class="card-title">Light Control</h2>
                <div class="card-icon">💡</div>
              </div>
              <div class="card-body">
                <p>Current status: <strong>${data.light ? 'ON' : 'OFF'}</strong></p>
              </div>
              <button onclick="controlDevice('light')" class="btn btn-primary">
                ${data.light ? 'TURN OFF' : 'TURN ON'}
              </button>
            </div>
            
            <!-- Fan Control Card -->
            <div class="card">
              <div class="card-header">
                <h2 class="card-title">Fan Control</h2>
                <div class="card-icon">🌀</div>
              </div>
              <div class="card-body">
                <p>Current status: <strong>${data.fan ? 'ON' : 'OFF'}</strong></p>
                <p>Auto mode: <strong>${(data.temperature > 28 || data.temperature <= 26) ? 'ACTIVE' : 'INACTIVE'}</strong></p>
              </div>
              <button onclick="controlDevice('fan')" class="btn btn-primary">
                ${data.fan ? 'TURN OFF' : 'TURN ON'}
              </button>
            </div>
            
            <!-- Door Lock Card -->
            <div class="card">
              <div class="card-header">
                <h2 class="card-title">Door Lock</h2>
                <div class="card-icon">🚪</div>
              </div>
              <div class="card-body">
                <p>Current status: <strong>${data.doorLocked ? 'LOCKED' : 'UNLOCKED'}</strong></p>
              </div>
              <button onclick="controlDevice('door')" class="btn btn-primary">
                ${data.doorLocked ? 'UNLOCK' : 'LOCK'}
              </button>
            </div>
            
            <!-- Temperature Card -->
            <div class="card">
              <div class="card-header">
                <h2 class="card-title">Temperature</h2>
                <div class="card-icon">🌡️</div>
              </div>
              <div class="card-body">
                <div class="sensor-value">${data.temperature.toFixed(1)} <span class="sensor-unit">°C</span></div>
              </div>
            </div>
            
            <!-- Humidity Card -->
            <div class="card">
              <div class="card-header">
                <h2 class="card-title">Humidity</h2>
                <div class="card-icon">💧</div>
              </div>
              <div class="card-body">
                <div class="sensor-value">${data.humidity.toFixed(1)} <span class="sensor-unit">%</span></div>
              </div>
            </div>
            
            <!-- System Status Card -->
            <div class="card">
              <div class="card-header">
                <h2 class="card-title">System Status</h2>
                <div class="card-icon">📊</div>
              </div>
              <div class="card-body">
                <p>Motion: <strong>${data.motion ? 'DETECTED' : 'NO MOTION'}</strong></p>
                <p>Flame: <strong>${data.flame ? 'DETECTED! ALERT!' : 'NO FLAME'}</strong></p>
                ${data.motion ? `
                <div class="alert alert-success">
                  Motion detected in the room
                </div>
                ` : ''}
                ${data.flame ? `
                <div class="alert alert-danger pulse">
                  ⚠️ FIRE DETECTED! EMERGENCY!
                </div>
                ` : ''}
              </div>
            </div>
          `;
          
          grid.innerHTML = html;
        })
        .catch(error => console.error('Error fetching data:', error));
    }

    // Control devices
    function controlDevice(device) {
      fetch('/' + device)
        .then(() => updateDashboard());
    }
  </script>
</body>
</html>
)=====";

  server.send(200, "text/html", html);
}

void handleLight()
{
  toggleLight();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleFan()
{
  toggleFan();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDoor()
{
  if (doorLocked)
  {
    unlockDoor();
  }
  else
  {
    lockDoor();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStatus()
{
  // Ensure we have valid temperature and humidity readings
  float temp = temperature;
  float hum = humidity;

  if (isnan(temp))
    temp = 0.0;
  if (isnan(hum))
    hum = 0.0;

  String json = "{";
  json += "\"light\":" + String(lightState ? "true" : "false") + ",";
  json += "\"fan\":" + String(fanState ? "true" : "false") + ",";
  json += "\"doorLocked\":" + String(doorLocked ? "true" : "false") + ",";
  json += "\"temperature\":" + String(temp) + ",";
  json += "\"humidity\":" + String(hum) + ",";
  json += "\"motion\":" + String(motionDetected ? "true" : "false") + ",";
  json += "\"flame\":" + String(flameDetected ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void readDHT()
{
  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();

  // Only update values if they're valid
  if (!isnan(newTemp))
    temperature = newTemp;
  if (!isnan(newHum))
    humidity = newHum;

  // // Auto control fan based on temperature
  // if (temperature > 28 && !fanState)
  // {
  //   toggleFan();
  // }
  // else if (temperature <= 26 && fanState)
  // {
  //   toggleFan();
  // }
}

void checkPIR()
{
  motionDetected = digitalRead(PIR_PIN);

  if (motionDetected)
  {
    digitalWrite(RED_LED_PIN, HIGH);
    delay(100);
    digitalWrite(RED_LED_PIN, LOW);
  }
  else
  {
    digitalWrite(RED_LED_PIN, LOW);
  }
}

void checkFlame()
{
  flameDetected = !digitalRead(FLAME_PIN); //Ative Low

  if (flameDetected)
  {
    digitalWrite(LIGHT_RELAY_PIN, HIGH); // Turn OFF light
    digitalWrite(FAN_RELAY_PIN, HIGH);   // Turn OFF fan
    lightState = false;
    fanState = false;

    displayAlert("FIRE DETECTED!");
  }
}

void checkTouch()
{
  if (millis() - lastTouchCheck > touchInterval)
  {
    if (touchRead(TOUCH_PIN_1) < TOUCH_THRESHOLD)
    {
      toggleLight();
      delay(300); // avoid bouncing
    }

    if (touchRead(TOUCH_PIN_2) < TOUCH_THRESHOLD)
    {
      toggleFan();
      delay(300); // avoid bouncing
    }
    lastTouchCheck = millis();
  }
}

void checkRFID()
{
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
  {
    return;
  }

  bool authorized = false;
  for (byte i = 0; i < sizeof(authorizedCards) / sizeof(authorizedCards[0]); i++)
  {
    if (memcmp(mfrc522.uid.uidByte, authorizedCards[i], 4) == 0)
    {
      authorized = true;
      break;
    }
  }

  if (authorized)
  {
    if (doorLocked)
    {
      unlockDoor();
    }
    else
    {
      lockDoor();
    }
    displayAlert(doorLocked ? "DOOR LOCKED" : "DOOR UNLOCKED");
  }
  else
  {
    displayAlert("UNAUTHORIZED CARD");
  }

  mfrc522.PICC_HaltA();
}

void toggleLight()
{
  lightState = !lightState;
  digitalWrite(LIGHT_RELAY_PIN, lightState ? LOW : HIGH);
  syncStatesWithSinricPro();
}

void toggleFan()
{
  fanState = !fanState;
  digitalWrite(FAN_RELAY_PIN, fanState ? LOW : HIGH);
  syncStatesWithSinricPro();
}

void lockDoor()
{
  doorLock.write(90);
  doorLocked = true;
  syncStatesWithSinricPro();
}

void unlockDoor()
{
  doorLock.write(0);
  doorLocked = false;
  syncStatesWithSinricPro();
}
void updateDisplay()
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.print("Light: ");
  display.println(lightState ? "ON " : "OFF");
  display.print("Fan: ");
  display.println(fanState ? "ON " : "OFF");
  display.print("Door: ");
  display.println(doorLocked ? "LOCKED" : "UNLOCKED");

  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");
  display.print("Humidity: ");
  display.print(humidity);
  display.println(" %");

  display.print("IP: ");
  display.print(WiFi.localIP());

  if (motionDetected)
  {
    display.setCursor(0, 56);
    display.print("MOTION DETECTED!");
  }

  if (flameDetected)
  {
    display.setCursor(0, 56);
    display.print("FIRE DETECTED!");
  }

  display.display();
}

void displayAlert(const char *message)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(message);
  display.display();
  delay(2000);
  updateDisplay();
}

void handleUpdate()
{
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  ESP.restart();
}

void handleUpload()
{
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    { // Start with max available size
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    // Flashing firmware to ESP
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
    {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (Update.end(true))
    { // true to set the size to the current progress
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    }
    else
    {
      Update.printError(Serial);
    }
  }
}

// Sinric Pro callbacks
bool onLightState(const String &deviceId, bool &state)
{
  if (lightState != state)
  {
    toggleLight();
  }
  return true;
}

bool onFanState(const String &deviceId, bool &state)
{
  if (fanState != state)
  {
    toggleFan();
  }
  return true;
}

bool onDoorState(const String &deviceId, bool &state)
{
  if (doorLocked == state)
  { // state=true means lock, false means unlock
    if (doorLocked)
    {
      unlockDoor();
    }
    else
    {
      lockDoor();
    }
  }
  return true;
}

void syncStatesWithSinricPro()
{
  SinricProSwitch &myLight = SinricPro[LIGHT_ID];
  myLight.sendPowerStateEvent(lightState);

  SinricProSwitch &myFan = SinricPro[FAN_ID];
  myFan.sendPowerStateEvent(fanState);

  SinricProDoorbell &myDoor = SinricPro[DOOR_ID];
  myDoor.sendPowerStateEvent(doorLocked);
}