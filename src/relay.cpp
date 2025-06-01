// #include <Arduino.h>
// #include <WiFi.h>
// #include <WebServer.h>
// #include <Update.h>
// #include <updatePage.h>

// // WiFi Credentials
// const char *ssid = "OPTIMUS";
// const char *password = "qqwweeaaaa";

// // Web Server
// WebServer server(80);

// // Relay pins
// #define RELAY1 18
// #define RELAY2 23

// // Touch pins
// // #define TOUCH1_PIN 32 // GPIO 4
// #define TOUCH1_PIN 4 //GPIO 4
// #define TOUCH2_PIN T3 // GPIO 15
// #define TOUCH_THRESHOLD 40

// bool relay1State = false;
// bool relay2State = false;

// unsigned long lastTouchCheck = 0;
// const unsigned long touchInterval = 200; // ms debounce

// void handleRoot();
// void handleUpdate();
// void handleUpload();

// void toggleRelay(int relayNum)
// {
//     if (relayNum == 1)
//     {
//         relay1State = !relay1State;
//         digitalWrite(RELAY1, relay1State ? HIGH : LOW);
//     }
//     else if (relayNum == 2)
//     {
//         relay2State = !relay2State;
//         digitalWrite(RELAY2, relay2State ? HIGH : LOW);
//     }
// }

// void setup()
// {
//     Serial.begin(115200);

//     pinMode(RELAY1, OUTPUT);
//     pinMode(RELAY2, OUTPUT);
//     digitalWrite(RELAY1, LOW);
//     digitalWrite(RELAY2, LOW);

//     WiFi.begin(ssid, password);
//     Serial.println("Connecting to WiFi...");
//     while (WiFi.status() != WL_CONNECTED)
//     {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("\nConnected to WiFi. IP: " + WiFi.localIP().toString());

//     // Web Routes
//     server.on("/", handleRoot);
//     server.on("/relay1", []()
//               {
//     toggleRelay(1);
//     server.sendHeader("Location", "/");
//     server.send(303); });
//     server.on("/relay2", []()
//               {
//     toggleRelay(2);
//     server.sendHeader("Location", "/");
//     server.send(303); });

//     server.on("/update", HTTP_GET, []()
//               {
//     server.sendHeader("Connection", "close");
//     server.send(200, "text/html", updateHTML); });
//     server.on("/update", HTTP_POST, handleUpdate, handleUpload);

//     server.begin();
//     Serial.println("HTTP server started");
// }

// void loop()
// {
//     server.handleClient();

//     // Touch input with debounce
//     if (millis() - lastTouchCheck > touchInterval)
//     {
//         if (touchRead(TOUCH1_PIN) < TOUCH_THRESHOLD)
//         {
//             toggleRelay(1);
//             Serial.println("Touch 1 detected");
//             delay(300); // avoid bouncing
//         }
//         if (touchRead(TOUCH2_PIN) < TOUCH_THRESHOLD)
//         {
//             toggleRelay(2);
//             Serial.println("Touch 2 detected");
//             delay(300);
//         }
//         lastTouchCheck = millis();
//     }
// }

// void handleRoot()
// {
//     String page = "<!DOCTYPE html><html><head><title>ESP32 Dashboard</title>";
//     page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
//     page += "<style>body{font-family:sans-serif;padding:20px;}button{padding:15px;font-size:16px;margin:10px;}</style>";
//     page += "</head><body><h1>ESP32 Relay Control</h1>";
//     page += "<p>Relay 1: " + String(relay1State ? "ON" : "OFF") + "</p>";
//     page += "<form action='/relay1' method='POST'><button>Toggle Relay 1</button></form>";
//     page += "<p>Relay 2: " + String(relay2State ? "ON" : "OFF") + "</p>";
//     page += "<form action='/relay2' method='POST'><button>Toggle Relay 2</button></form>";
//     page += "<hr><a href='/update'>OTA Firmware Update</a>";
//     page += "</body></html>";

//     server.send(200, "text/html", page);
// }


// void handleUpdate()
// {
//     server.sendHeader("Connection", "close");
//     server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
//     ESP.restart();
// }

// void handleUpload()
// {
//     HTTPUpload &upload = server.upload();
//     if (upload.status == UPLOAD_FILE_START)
//     {
//         Serial.printf("Update: %s\n", upload.filename.c_str());
//         if (!Update.begin(UPDATE_SIZE_UNKNOWN))
//         { // Start with max available size
//             Update.printError(Serial);
//         }
//     }
//     else if (upload.status == UPLOAD_FILE_WRITE)
//     {
//         // Flashing firmware to ESP
//         if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
//         {
//             Update.printError(Serial);
//         }
//     }
//     else if (upload.status == UPLOAD_FILE_END)
//     {
//         if (Update.end(true))
//         { // true to set the size to the current progress
//             Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
//         }
//         else
//         {
//             Update.printError(Serial);
//         }
//     }
// }