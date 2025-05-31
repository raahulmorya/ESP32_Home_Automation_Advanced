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

// WiFi Credentials
const char *ssid = "OPTIMUS";
const char *password = "qqwweeaaaa";

// Web Server
WebServer server(80);

// Pin Definitions
#define LIGHT_RELAY_PIN 23
#define FAN_RELAY_PIN 18
#define PIR_PIN 13
#define RED_LED_PIN 12
#define TOUCH_PIN_1 4
#define TOUCH_PIN_2 15
#define SERVO_PIN 14
#define DHT_PIN 26
#define FLAME_PIN 25

// RFID Pins
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
    {0x12, 0x34, 0x56, 0x78},
    {0x9A, 0xBC, 0xDE, 0xF0}};



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
  SPI.begin();
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

// Web Server Handlers
void handleRoot()
{
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Home Automation</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;background-color:#f5f5f5;}";
  html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);}";
  html += "h1{color:#444;text-align:center;}";
  html += ".control{display:flex;justify-content:space-between;margin:15px 0;}";
  html += "button{padding:10px 20px;border:none;border-radius:5px;cursor:pointer;font-size:16px;font-weight:bold;}";
  html += ".on{background-color:#4CAF50;color:white;}";
  html += ".off{background-color:#f44336;color:white;}";
  html += ".status{margin-top:20px;padding:15px;background-color:#e9e9e9;border-radius:5px;}";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Home Automation System</h1>";

  // Light Control
  html += "<div class='control'>";
  html += "<span>Light: " + String(lightState ? "ON" : "OFF") + "</span>";
  html += "<a href='/light'><button class='" + String(lightState ? "on" : "off") + "'>";
  html += lightState ? "TURN OFF" : "TURN ON";
  html += "</button></a></div>";

  // Fan Control
  html += "<div class='control'>";
  html += "<span>Fan: " + String(fanState ? "ON" : "OFF") + "</span>";
  html += "<a href='/fan'><button class='" + String(fanState ? "on" : "off") + "'>";
  html += fanState ? "TURN OFF" : "TURN ON";
  html += "</button></a></div>";

  // Door Control
  html += "<div class='control'>";
  html += "<span>Door: " + String(doorLocked ? "LOCKED" : "UNLOCKED") + "</span>";
  html += "<a href='/door'><button class='" + String(doorLocked ? "off" : "on") + "'>";
  html += doorLocked ? "UNLOCK" : "LOCK";
  html += "</button></a></div>";

  // Status Information
  html += "<div class='status'>";
  html += "<h3>Current Status</h3>";
  html += "<p>Temperature: " + String(temperature) + " °C</p>";
  html += "<p>Humidity: " + String(humidity) + " %</p>";
  html += "<p>Motion: " + String(motionDetected ? "DETECTED" : "NO MOTION") + "</p>";
  html += "<p>Flame: " + String(flameDetected ? "DETECTED! ALERT!" : "NO FLAME") + "</p>";
  html += "</div>";

  html += "</div></body></html>";

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
  String json = "{";
  json += "\"light\":" + String(lightState ? "true" : "false") + ",";
  json += "\"fan\":" + String(fanState ? "true" : "false") + ",";
  json += "\"doorLocked\":" + String(doorLocked ? "true" : "false") + ",";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"humidity\":" + String(humidity) + ",";
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

// Existing sensor and control functions (from previous implementation)
void readDHT()
{
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Auto control fan based on temperature
  if (temperature > 28 && !fanState)
  {
    toggleFan();
  }
  else if (temperature <= 26 && fanState)
  {
    toggleFan();
  }
}

void checkPIR()
{
  motionDetected = digitalRead(PIR_PIN);

  if (motionDetected)
  {
    digitalWrite(RED_LED_PIN, HIGH);
    delay(100);
    digitalWrite(RED_LED_PIN, LOW);

    if (!lightState)
    {
      toggleLight();
    }
  }
  else
  {
    digitalWrite(RED_LED_PIN, LOW);
  }
}

void checkFlame()
{
  flameDetected = digitalRead(FLAME_PIN);

  if (flameDetected)
  {
    if (lightState)
      toggleLight();
    if (fanState)
      toggleFan();

    for (int i = 0; i < 5; i++)
    {
      digitalWrite(RED_LED_PIN, HIGH);
      delay(100);
      digitalWrite(RED_LED_PIN, LOW);
      delay(100);
    }

    displayAlert("FIRE DETECTED!");
  }
}

void checkTouch()
{
  static unsigned long lastTouchTime = 0;

  if (millis() - lastTouchTime > 500)
  {
    if (touchRead(TOUCH_PIN_1) < 20)
    {
      toggleLight();
      lastTouchTime = millis();
    }

    if (touchRead(TOUCH_PIN_2) < 20)
    {
      toggleFan();
      lastTouchTime = millis();
    }
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
  digitalWrite(LIGHT_RELAY_PIN, lightState ? HIGH : LOW);
}

void toggleFan()
{
  fanState = !fanState;
  digitalWrite(FAN_RELAY_PIN, fanState ? HIGH : LOW);
}

void lockDoor()
{
  doorLock.write(0);
  doorLocked = true;
}

void unlockDoor()
{
  doorLock.write(90);
  doorLocked = false;
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
  display.setTextSize(2);
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