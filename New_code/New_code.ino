#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include "DHT.h"
#include <MQUnifiedsensor.h>
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>

//Settings related to mqunified library
#define board "ESP32"
#define Voltage_Resolution 5
#define mq135pin 33
#define mq135type "MQ-135"
#define ADC_Bit_Resolution 12
#define RatioMQ135CleanAir 3.6
#define mq2pin 34
#define mq2type "MQ-2"
#define RatioMQ2CleanAir 9.83

const int buzzer = 25;

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

MQUnifiedsensor MQ135(board, Voltage_Resolution, ADC_Bit_Resolution, mq135pin, mq135type);
MQUnifiedsensor MQ2(board, Voltage_Resolution, ADC_Bit_Resolution, mq2pin, mq2type);

// Wi-Fi access point credentials
const char* ssid = "Rover_4.3_Readings";
const char* password = "12345678";

// WebSocket server port
const uint16_t webSocketPort = 8080;

// Create an instance of the web server and WebSocket server
AsyncWebServer server(80);
AsyncWebSocket webSocket("/ws");

// Sensor pins
// const uint8_t mq2SensorPin = 34;   // Change this to the MQ2 sensor pin
// const uint8_t mq135SensorPin = 33; // Change this to the MQ135 sensor pin
#define DHT11PIN 32  // Change this to the LM35 sensor pin

DHT dht(DHT11PIN, DHT11);

// Variables to store sensor values
int mq2Value = 0;
int mq135Value = 0;
int dhtValue = 0;

// Callback function when WebSocket client connects
void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  // WebSocket event handling code as before
  if (type == WS_EVT_CONNECT) {
    Serial.println("[WebSocket] Client connected!");
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("[WebSocket] Client disconnected!");
  } else if (type == WS_EVT_DATA) {
    Serial.print("[WebSocket] Message received: ");
    for (size_t i = 0; i < len; i++) {
      Serial.print((char)data[i]);
    }
    Serial.println();
    // Process the received message here as needed
  }
}

//Caliberating mq2
void calibrateSensor() {
  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ135.update();
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0 / 10);
  Serial.println("  done!.");
  
  if (isinf(calcR0)) {
    Serial.println("Warning: Connection issue, R0 is infinite (Open circuit detected) please check your wiring and supply");
    while (1);
  }
  
  if (calcR0 == 0) {
    Serial.println("Warning: Connection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply");
    while (1);
  }
}

void printHeader() {
  Serial.println("* Values from MQ-135 ***");
  Serial.println("|    CO   |  Alcohol |   CO2  |  Toluen  |  NH4  |  Aceton  |");
}

void initializeMQ2()
{
    MQ2.init();
}

void calibrateMQ2()
{
    Serial.print("Calibrating please wait.");
    float calcR0 = 0;
    for (int i = 1; i <= 10; i++)
    {
        MQ2.update(); // Update data, the arduino will read the voltage from the analog pin
        calcR0 += MQ2.calibrate(RatioMQ2CleanAir);
        Serial.print(".");
    }
    MQ2.setR0(calcR0 / 10);
    Serial.println("  done!.");

    if (isinf(calcR0))
    {
        Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply");
        while (1)
            ;
    }
    if (calcR0 == 0)
    {
        Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply");
        while (1)
            ;
    }
}

// void readMQ2()
// {
//     MQ2.update();     // Update data, the arduino will read the voltage from the analog pin
//     MQ2.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
// }

void printMQ2Data()
{
    MQ2.serialDebug(); // Will print the table on the serial port
}

void setup() {
  Serial.begin(115200);

  // WiFi.softAP(ssid, password);
  // IPAddress IP = WiFi.softAPIP();
  // Serial.print("AP IP address: ");
  // Serial.println(IP);

  //Starting dht sensor
  dht.begin();

  pinMode(25, OUTPUT);

  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();

  //Starting mq135
  MQ135.setRegressionMethod(1);
  MQ135.init();
  calibrateSensor();
  printHeader();

  //Starting mq2
  MQ2.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ2.setA(574.25);
  MQ2.setB(-2.222); // Configure the equation to calculate LPG concentration

  initializeMQ2();
  calibrateMQ2();

  MQ2.serialDebug(true);

  //Create Wi-Fi access point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Initialize the web server
  server.begin();

  // Route for serving the web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = "<html>\
      <head>\
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>\
    <style>\
      * {\
      box-sizing: border-box;\
    }\
    body {\
      font-family: Arial, sans-serif;\
      margin: 0;\
      padding: 20px;\
      background-color: #f1f1f1;\
    }\
    .container {\
      display: flex;\
      flex-wrap: wrap;\
      justify-content: center;\
      align-items: center;\
      gap: 20px;\
      max-width: 100%;\
      max-height: 100vh;\
      margin: 0 auto;\
      padding: 20px;\
      box-sizing: border-box;\
    }\
    .card {\
      flex: 0 0 200px;\
      width: 100%;\
      max-width: 200px;\
      border-radius: 10px;\
      background-image: linear-gradient(rgb(3, 45, 196), rgb(23, 165, 99));\
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.2);\
      display: flex;\
      flex-direction: column;\
      justify-content: center;\
      align-items: center;\
      padding: 20px;\
      text-align: center;\
      transition: transform 0.3s;\
      box-sizing: border-box;\
    }\
    .card:hover {\
      transform: scale(1.05);\
    }\
    .card-icon {\
      font-size: 60px;\
      color: #ff6b6b;\
      margin-bottom: 10px;\
      transition: color 0.3s;\
    }\
    .card:hover .card-icon {\
      color: #ff3b3b;\
    }\
    .card-value {\
      font-size: 36px;\
      font-weight: bold;\
      color: #333;\
      margin-bottom: 10px;\
    }\
    .card-label {\
      font-size: 22px;\
      color: #ffffff;\
    }\
    .card:hover .card-label {\
      color: #e0f805;\
    }\
    .container_2 {\
      width: 100%;\
      min-height: 100vh;\
      display: flex;\
      align-items: center;\
      justify-content: center;\
    }\
    .btn {\
      padding: 10px 10px;\
      background: #fff;\
      border: 0;\
      outline: none;\
      cursor: pointer;\
      font-size: 22px;\
      font-weight: 200;\
      border-radius: 30px;\
    }\
    .popup {\
      width: 400px;\
      max-width: 90%;\
      background: #fff;\
      border: 5px #ff3b3b solid;\
      border-radius: 6px;\
      position: absolute;\
      top: 50%;\
      left: 50%;\
      transform: translate(-50%, -50%) scale(0.1);\
      text-align: center;\
      padding: 0 30px 30px;\
      color: #333;\
      visibility: hidden;\
      transition: transform 0.4s, top 0.4s;\
    }\
    .open-popup {\
      visibility: visible;\
      top: 50%;\
      transform: translate(-50%, -50%) scale(1);\
    }\
    .popup h2 {\
      font-size: 38px;\
      font-weight: 1000;\
      margin: 30px 10px;\
      color: #ff3b3b;\
      text-transform: uppercase;\
    }\
    .popup button {\
      width: 100%;\
      margin-top: 50px;\
      padding: 10px 0;\
      background: #ff3b3b;\
      color: #fff;\
      border: 0;\
      outline: none;\
      font-size: 18px;\
      border-radius: 4px;\
      cursor: pointer;\
      box-shadow: 0 5px 5px rgba(0, 0, 0, 0.2);\
    }\
        </style>\
      </head>\
      <body class=\'noselect\' align=\'center\' style=\'background-image:linear-gradient(rgb(0, 24, 109),rgb(95, 18, 95))\'>\
        <div class=\"container\">\
          <div class=\"card\">\
            <svg xmlns=\"http://www.w3.org/2000/svg\" height=\"10em\" viewBox=\"0 0 384 512\"><!--! Font Awesome Free 6.4.0 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --><style>svg{fill:#000000}</style><path d=\"M153.6 29.9l16-21.3C173.6 3.2 180 0 186.7 0C198.4 0 208 9.6 208 21.3V43.5c0 13.1 5.4 25.7 14.9 34.7L307.6 159C356.4 205.6 384 270.2 384 337.7C384 434 306 512 209.7 512H192C86 512 0 426 0 320v-3.8c0-48.8 19.4-95.6 53.9-130.1l3.5-3.5c4.2-4.2 10-6.6 16-6.6C85.9 176 96 186.1 96 198.6V288c0 35.3 28.7 64 64 64s64-28.7 64-64v-3.9c0-18-7.2-35.3-19.9-48l-38.6-38.6c-24-24-37.5-56.7-37.5-90.7c0-27.7 9-54.8 25.6-76.9z\"/>\
            </svg>\
            <div class=\"card-value\" id=\"mq2Value\"></div>\
            <div class=\"card-label\">Gas Level</div>\
          </div>\
          <div class=\"card\">\
            <svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\"><path d=\"M12 23C16.1421 23 19.5 19.6421 19.5 15.5C19.5 14.6345 19.2697 13.8032 19 13.0296C17.3333 14.6765 16.0667 15.5 15.2 15.5C19.1954 8.5 17 5.5 11 1.5C11.5 6.49951 8.20403 8.77375 6.86179 10.0366C5.40786 11.4045 4.5 13.3462 4.5 15.5C4.5 19.6421 7.85786 23 12 23ZM12.7094 5.23498C15.9511 7.98528 15.9666 10.1223 13.463 14.5086C12.702 15.8419 13.6648 17.5 15.2 17.5C15.8884 17.5 16.5841 17.2992 17.3189 16.9051C16.6979 19.262 14.5519 21 12 21C8.96243 21 6.5 18.5376 6.5 15.5C6.5 13.9608 7.13279 12.5276 8.23225 11.4932C8.35826 11.3747 8.99749 10.8081 9.02477 10.7836C9.44862 10.4021 9.7978 10.0663 10.1429 9.69677C11.3733 8.37932 12.2571 6.91631 12.7094 5.23498Z\"></path></svg>\
            <div class=\"card-value\" id=\"CO2Value\"></div>\
          <div class=\"card-label\">Carbon Dioxide</div>\
          </div>\
          <div class=\"card\">\
            <svg xmlns=\"http://www.w3.org/2000/svg\" height=\"10em\" viewBox=\"0 0 384 512\"><!--! Font Awesome Free 6.4.0 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --><path d=\"M192 512C86 512 0 426 0 320C0 228.8 130.2 57.7 166.6 11.7C172.6 4.2 181.5 0 191.1 0h1.8c9.6 0 18.5 4.2 24.5 11.7C253.8 57.7 384 228.8 384 320c0 106-86 192-192 192zM96 336c0-8.8-7.2-16-16-16s-16 7.2-16 16c0 61.9 50.1 112 112 112c8.8 0 16-7.2 16-16s-7.2-16-16-16c-44.2 0-80-35.8-80-80z\"/></svg>\
            <div class=\"card-value\" id=\"hum\"></div>\
            <div class=\"card-label\">Humidity</div>\
          </div>\
          <div class=\"card\">\
            <svg xmlns=\"http://www.w3.org/2000/svg\" height=\"10em\" viewBox=\"0 0 512 512\"><style>svg{fill:#000000}</style><path d=\"M416 64a32 32 0 1 1 0 64 32 32 0 1 1 0-64zm0 128A96 96 0 1 0 416 0a96 96 0 1 0 0 192zM96 112c0-26.5 21.5-48 48-48s48 21.5 48 48V276.5c0 17.3 7.1 31.9 15.3 42.5C217.8 332.6 224 349.5 224 368c0 44.2-35.8 80-80 80s-80-35.8-80-80c0-18.5 6.2-35.4 16.7-48.9C88.9 308.4 96 293.8 96 276.5V112zM144 0C82.1 0 32 50.2 32 112V276.5c0 .1-.1 .3-.2 .6c-.2 .6-.8 1.6-1.7 2.8C11.2 304.2 0 334.8 0 368c0 79.5 64.5 144 144 144s144-64.5 144-144c0-33.2-11.2-63.8-30.1-88.1c-.9-1.2-1.5-2.2-1.7-2.8c-.1-.3-.2-.5-.2-.6V112C256 50.2 205.9 0 144 0zm0 416c26.5 0 48-21.5 48-48c0-20.9-13.4-38.7-32-45.3V112c0-8.8-7.2-16-16-16s-16 7.2-16 16V322.7c-18.6 6.6-32 24.4-32 45.3c0 26.5 21.5 48 48 48z\"/></svg>\
            <div class=\"card-value\" id=\"lm35Value\"></div>\
            <div class=\"card-label\">Temperature</div>\
          </div>\
          </div>\
          <!--Popup-->\
        <div class='container_2'>\
          <div class='popup' id='popup'>\
            <h2>Warning</h2>\
            <p id='warningName'></p>\
            <button type='button' onclick='closePopup()'>Ok</button>\
          </div>\
        </div>\
      <script>\
      let popup = document.getElementById('popup');\
        function openPopup() {\
            popup.classList.add('open-popup');\
        }\
        function closePopup() {\
            popup.classList.remove('open-popup');\
        }\
      var socket = new WebSocket('ws://' + window.location.hostname + ':' + window.location.port + '/ws');\
      socket.onmessage = function(event) {\
        var sensorData = event.data.split(',');\
        document.getElementById('mq2Value').textContent = sensorData[0];\
        var lpgas = sensorData[0];\
        if (parseFloat(sensorData[0]) > 10000.00) {\
          openPopup();\
          popup.querySelector('p').textContent = 'Gas level is above the acceptable limit.';\
        }\
        \
        document.getElementById('CO2Value').textContent = sensorData[1];\
        var CO2 = sensorData[1];\
        if (parseFloat(sensorData[1]) > 5000.00) {\
          openPopup();\
          popup.querySelector('p').textContent = 'Carbon Daoxide level is above the acceptable limit.';\
        }\
        document.getElementById('lm35Value').textContent = sensorData[2];\
        var temp = sensorData[2];\
        if (parseFloat(sensorData[2]) > 40.00 || sensorData[2]<18.00) {\
          openPopup();\
          popup.querySelector('p').textContent = 'Temperature is above the acceptable limit.';\
        }\
        document.getElementById('hum').textContent = sensorData[3];\
        var hum = sensorData[3];\
      };\
    </script>\
    </body>\
    </html>";
    request->send(200, "text/html", html);
  });

  // Add the WebSocket server to the web server
  server.addHandler(&webSocket);

  // Assign callback functions for WebSocket events
  webSocket.onEvent(onWebSocketEvent);  

  // Set ADC resolution (9, 10, 11, or 12 bits)
  analogReadResolution(12);
}

void loop() {
  // Read sensor values
  //mq2Value = analogRead(mq2SensorPin);
  //Serial.print(mq2Value);
  Serial.print(" ");
  // mq135Value = analogRead(mq135SensorPin);

  //Read DHT Values
  float humi = dht.readHumidity();
  float temp = dht.readTemperature();
 
  // Serial.print(temp);
  // Serial.print(" | ");
  // Serial.print(humi);
  // Serial.print(" | ");
  // Serial.println("");

  //Read MQ135 Values
  MQ135.update();
  
  MQ135.setA(605.18);
  MQ135.setB(-3.937);
  float CO = MQ135.readSensor();

  MQ135.setA(110.47);
  MQ135.setB(-2.862);
  float CO2 = MQ135.readSensor();

  MQ135.setA(102.2);
  MQ135.setB(-2.473);
  float NH4 = MQ135.readSensor();

  Serial.print("|   ");

  Serial.print(CO2 + 400);
  Serial.print("   |   ");
  Serial.println(" ");

  lcd.setCursor(0, 0);
  lcd.print("CO=");
  lcd.setCursor(3, 0);
  lcd.print(CO);
  Serial.print("CO = ");
  Serial.println(CO);

  lcd.setCursor(8, 0);
  lcd.print(" CO2=");
  lcd.setCursor(13, 0);
  lcd.print(CO2 + 400);

  lcd.setCursor(0, 1);
  lcd.print("NH4=");
  lcd.setCursor(4, 1);
  lcd.print(NH4);
  Serial.print("NH4 = ");
  Serial.println(NH4);

  //Read MQ2 Values
  MQ2.update();     // Update data, the arduino will read the voltage from the analog pin
  float mq2Value = MQ2.readSensor();
  //printMQ2Data();

  lcd.setCursor(0, 2);
  lcd.print("CH4=");

  lcd.setCursor(4, 2);
  lcd.print(mq2Value);
  Serial.print("CH4 = ");
  Serial.println(mq2Value);

  lcd.setCursor(20, 1);
  lcd.print("Temp=");
  lcd.setCursor(26, 1);
  lcd.print(temp);

// Buzzer
  if (CO > 400 || CO2 > 10000 || NH4 > 100 || mq2Value > 100){
    digitalWrite(buzzer, HIGH);
    delay(1000);
    digitalWrite(buzzer, LOW);
    delay (1000);
  } else {
    digitalWrite(buzzer, LOW);
  }

  // Send sensor values to WebSocket clients
  String message = String(mq2Value) + "," + String(CO2) + "," + String(temp) + "," + String(humi);
  webSocket.textAll(message);

  // Other code as before
  webSocket.cleanupClients();
  delay(1000);
}