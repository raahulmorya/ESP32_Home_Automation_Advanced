// #include <Arduino.h>
// #include <WiFi.h>
// #include <WebServer.h>
// #include <Wire.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_SSD1306.h>
// #include <MFRC522.h>
// #include <SPI.h>
// #include <DHT.h>
// #include <ESP32Servo.h>
// #include <Update.h>
// #include <updatePage.h>

// // WiFi Credentials
// const char *ssid = "OPTIMUS";
// const char *password = "qqwweeaaaa";

// // Define your custom SPI pins
// #define SPI_MOSI 27
// #define SPI_MISO 19
// #define SPI_SCK 5

// // RC522 pins
// #define SS_PIN 17
// #define RST_PIN 16
// MFRC522 mfrc522(SS_PIN, RST_PIN);

// // Web Server
// WebServer server(80);

// void handleRead();
// void handleRoot();
// void handleUpload();
// void handleUpdate();

// String webpage = ""; // Variable to hold the HTML content

// void setup()
// {
//     Serial.begin(115200);
//     SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SS_PIN);

//     mfrc522.PCD_Init(); // Init MFRC522

//     // Connect to WiFi
//     WiFi.begin(ssid, password);
//     Serial.println("Connecting to WiFi...");
//     while (WiFi.status() != WL_CONNECTED)
//     {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("");
//     Serial.print("Connected to WiFi. IP address: ");
//     Serial.println(WiFi.localIP());

//     // Set up web server routes
//     server.on("/", handleRoot);     // Handle root URL
//     server.on("/read", handleRead); // Handle RFID read request
//     server.on("/update", HTTP_GET, []()
//               {
// server.sendHeader("Connection", "close");
// server.send(200, "text/html", updateHTML); });

//     server.on("/update", HTTP_POST, handleUpdate, handleUpload);

//     server.begin();
//     Serial.println("HTTP server started");
// }

// void loop()
// {
//     server.handleClient(); // Handle client requests
// }

// void handleRoot()
// {
//     // Build the HTML page
//     webpage = "<!DOCTYPE html><html><head>";
//     webpage += "<title>ESP32 RFID Reader</title>";
//     webpage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
//     webpage += "<style>";
//     webpage += "body { font-family: Arial, sans-serif; margin: 0 auto; padding: 20px; max-width: 800px; }";
//     webpage += "h1 { color: #444; }";
//     webpage += ".card { background: #f9f9f9; border: 1px solid #ddd; padding: 15px; margin-bottom: 10px; border-radius: 5px; }";
//     webpage += "button { background: #4CAF50; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; }";
//     webpage += "button:hover { background: #45a049; }";
//     webpage += "</style>";
//     webpage += "</head><body>";
//     webpage += "<h1>ESP32 RFID Reader</h1>";
//     webpage += "<div class='card'>";
//     webpage += "<p>Place an RFID card near the reader and click the button below</p>";
//     webpage += "<button onclick='readCard()'>Read RFID Card</button>";
//     webpage += "</div>";
//     webpage += "<div id='result' class='card'></div>";
//     webpage += "<script>";
//     webpage += "function readCard() {";
//     webpage += "  fetch('/read')";
//     webpage += "    .then(response => response.text())";
//     webpage += "    .then(data => {";
//     webpage += "      document.getElementById('result').innerHTML = data;";
//     webpage += "    });";
//     webpage += "}";
//     webpage += "</script>";
//     webpage += "</body></html>";

//     server.send(200, "text/html", webpage);
// }
// void handleRead()
// {
//     String content = "";

//     // Look for new cards
//     if (!mfrc522.PICC_IsNewCardPresent())
//     {
//         content = "<p>No card present. Please try again.</p>";
//         server.send(200, "text/html", content);
//         return;
//     }

//     // Select one of the cards
//     if (!mfrc522.PICC_ReadCardSerial())
//     {
//         content = "<p>Error reading card. Please try again.</p>";
//         server.send(200, "text/html", content);
//         return;
//     }

//     // Card detected, build response
//     content += "<h3>RFID Card Detected</h3>";

//     // UID
//     content += "<p><strong>UID:</strong> ";
//     for (byte i = 0; i < mfrc522.uid.size; i++)
//     {
//         content += (mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
//         content += String(mfrc522.uid.uidByte[i], HEX);
//     }
//     content += "</p>";

//     // Card type
//     MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
//     content += "<p><strong>Card Type:</strong> " + String(mfrc522.PICC_GetTypeName(piccType)) + "</p>";

//     server.send(200, "text/html", content);

//     // Halt PICC
//     mfrc522.PICC_HaltA();
//     // Stop encryption on PCD
//     mfrc522.PCD_StopCrypto1();
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