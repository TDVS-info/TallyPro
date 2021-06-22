
/* 
Tally Pro Firmware
Version 084
Copyright 2021 Ted Dillenkofer

Support for OBS, ATEM and Vmix
  You must add the OBS websocket plugin to OBS if using OBS
     https://github.com/Palakis/obs-websocket/releases/tag/4.9.0
  ATEM is polled
  Vmix uses a TCP port connection and messaging
  No spaces or special characters in local WiFi SSID or password
    This is a limitation of the ESP8266 chip

This software is free: you can redistribute it and/or modify 
it under the terms of the GNU General Public License as published by the 
Free Software Foundation, either version 3 of the License, or (at your 
option) any later version.

The software is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. 
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this software. If not, see: http://www.gnu.org/licenses/.

IMPORTANT: If you want to use this software in your own projects and/or products,
please follow the license rules!
*/

#include <ESP8266WiFi.h>
//#include <ESPAsyncTCP.h>
//#include <ESPAsyncWebServer.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#include <string.h>
#include <WebSocketsClient.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ATEMstd.h>

#include "tallylight.h"

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
String SETUP_HTML;

WebSocketsServer wsServer(9666);
WebSocketsClient webSocket;
WiFiClient vmix;

ATEMstd AtemSwitcher; 
IPAddress tempIP;
IPAddress tempIP2;
IPAddress tempIP3;

String     type_Pre = "PreviewSceneChanged";
String     type_Pro = "SwitchScenes";

int On = LOW;
int Off = HIGH;
 
void setup() {
  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(1000);
  Serial.println("START");

  initPins();
  testLEDS();
  getMemory();
  startESPWiFi();
  configureWebServer();
  if (!checkMemory()) {
    Serial.println("NOT CONFIGURED");
    clearGlobals();
    startwsServer();
    while(CONFIGURED != "1"){ 
      server.handleClient();       //Checks for web server activity
      wsServer.loop();
//      delay(250);
    }

  }  
  startWiFi();                 // Connect to the specified Network
  startUpdater();
  switch(Switch_Type){
    case 0: 
      startWebSocket();            // Start a WebSocket to OBS
      break;
    case 1:  
      ATEMconnect();
      break;
    case 2:  
      vMixConnect();
  } 
}

void loop() {
  switch(Switch_Type){
    case 0: 
      webSocket.loop();            // constantly check for websocket events
      break;
    case 1:  
      AtemSwitcher.runLoop();
      processATEM();
      break;
    case 2:  
      readvMixData();
  }
  server.handleClient();       //Checks for web server activity
  MDNS.update();
  if (digitalRead(0) == LOW) {
     EEPROM.begin(512);
     EEPROM.write(offset_cflag, 255);
     EEPROM.commit();
     CONFIGURED = "0";
     Serial.println("Memory cleared");
     ESP.restart();
  }
}

//
//  functions
//
void initPins() {
  pinMode(D1, OUTPUT);    // Preview Front
  digitalWrite(D1, Off);
  pinMode(D2, OUTPUT);    // Program Front
  digitalWrite(D2, Off);
  pinMode(D5, OUTPUT);    // Preview Back
  digitalWrite(D5, Off);
  pinMode(D4, OUTPUT);    // Program Back
  digitalWrite(D4, Off);
  pinMode(0, INPUT_PULLUP);    // Flash button on ESP8266
}
void testLEDS() {
  digitalWrite(D1, Off);
  delay(250);
  digitalWrite(D1, On);
  delay(250);
  digitalWrite(D1, Off);
  delay(250);
  
  digitalWrite(D2, Off);
  delay(250);
  digitalWrite(D2, On);
  delay(250);
  digitalWrite(D2, Off);
  delay(250);

  digitalWrite(D4, Off);
  delay(250);
  digitalWrite(D4, On);
  delay(250);
  digitalWrite(D4, Off);
  delay(250);
  
  digitalWrite(D5, Off);
  delay(250);
  digitalWrite(D5, On);
  delay(250);
  digitalWrite(D5, Off);
  delay(250);
}

void startESPWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ESPssid, ESPpassword);
  WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet); 
  IPAddress myIP = WiFi.softAPIP();
  Serial.println("");
  Serial.print("AP IP address: ");
  Serial.println(myIP); 
}

void configureWebServer() {  
  // Setup server
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/action_page", handleForm);
  server.on("/action_control", handleControlForm);  
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("server(80) started");
}

void startWiFi() { // Connect to WiFI specified
  getMemory();
  WiFi.mode(WIFI_STA);
  if (wifi_sip.length() != 0){
    tempIP.fromString(wifi_sip);
    tempIP2.fromString(wifi_gateway);
    tempIP3.fromString(wifi_subnet);
     if (!WiFi.config(tempIP, tempIP2, tempIP3)) {
        Serial.println("STA Failed to configure");
   }
  }
  WiFi.begin(wifi_ssid, wifi_pswd);             // connect
  Serial.print("Connecting to ");
  Serial.print(wifi_ssid); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {      // Wait for the Wi-Fi to connect
    digitalWrite(D1, On);
    delay(500);
    digitalWrite(D1, Off);
    delay(1000);
    Serial.print(++i); Serial.print(' ');
    if (i > 30 ) {
        Serial.println('\n');
        Serial.println("WiFi connection not established!");
        digitalWrite(D1, On);
        delay(250);
        digitalWrite(D1, Off);
        delay(250);
        digitalWrite(D1, On);
        delay(250);
        digitalWrite(D1, Off);
        delay(250);
        return;  
    }
  }
  
  Serial.println('\n');
  Serial.println("WiFi connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());              // Send the IP address of the ESP8266 to the computer
  digitalWrite(D2, On);
  delay(250);
  digitalWrite(D2, Off);
  delay(250);
  digitalWrite(D2, On);
  delay(250);
  digitalWrite(D2, Off);
  delay(250);
  return;
}

boolean checkMemory() {
  Serial.println("checkMemory");
  char temp = char(EEPROM.read(offset_cflag)); // get configuration flag from memory
  Serial.print("configured: ");
  Serial.println(temp);
  if (temp == 'Y') {
    Serial.println("memory configured");
    return true;
  } else {
    Serial.println("Memory not configured");
    return false;  
  }
}

void getMemory() {
    wifi_ssid = read_from_Memory(offset_ssid);
    wifi_pswd = read_from_Memory(offset_pswd);
    wifi_sip = read_from_Memory(offset_sip);
    wifi_gateway = read_from_Memory(offset_gateway);
    wifi_subnet = read_from_Memory(offset_subnet);
    Switch_Type = read_from_Memory(offset_SwitchType).toInt();    
    Switch_IP = read_from_Memory(offset_SwitchIP);
    Switch_Port = read_from_Memory(offset_SwitchPort);
    Switch_Pswd = read_from_Memory(offset_SwitchPswd);
    Camera_ID = read_from_Memory(offset_Camera_ID);
    if (Camera_ID.length() == 1  && isDigit(Camera_ID.charAt(0))){ 
      camera_num = Camera_ID.toInt();
    }else{
      camera_num = 0;
    }
    LED_pos = read_from_Memory(offset_LED_pos).toInt();
    String LED_Levels = read_from_Memory(offset_LED_Levels);
    FrontRedLevel = unpackLevels(0, LED_Levels);
    FrontGreenLevel = unpackLevels(1, LED_Levels);
    BackRedLevel = unpackLevels(2, LED_Levels);
    BackGreenLevel = unpackLevels(3, LED_Levels);
    Serial.println("getMemory");
    Serial.println(wifi_ssid);
    Serial.println(wifi_pswd);
    Serial.println(wifi_sip);
    Serial.println(wifi_gateway);
    Serial.println(wifi_subnet);
    Serial.println(Switch_Type);
    Serial.println(Switch_IP);
    Serial.println(Switch_Port);
    Serial.println(Switch_Pswd);
    Serial.println(Camera_ID);
    Serial.println(camera_num);
    Serial.println(LED_pos);
    Serial.println(FrontRedLevel);
    Serial.println(FrontGreenLevel);
    Serial.println(BackRedLevel);
    Serial.println(BackGreenLevel);
  }
  
void startUpdater(){
  MDNS.begin(host);
  httpUpdater.setup(&server);
  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
}

void startWebSocket() {
  tempIP.fromString(Switch_IP);// Start a WebSocket client
  webSocket.begin(tempIP, Switch_Port.toInt());                   // connect to the websocket
  char __tempPswd[sizeof(Switch_Pswd)];
  Switch_Pswd.toCharArray(__tempPswd, sizeof(__tempPswd));
//  webSocket.setAuthorization("user", __tempPswd);
  Serial.println("WebSocket client started.");
  webSocket.onEvent(webSocketEvent);               // respond to message
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) { // When a WebSocket message is received
 //     Serial.println("WebSocket message.");
 //     Serial.print("WStype = ");   
 //     Serial.println(type);  
 //    Serial.print("WS payload = ");
 //    since payload is a pointer we need to type cast to char
 //    for(int i = 0; i < length; i++) { Serial.print((char) payload[i]); }
 //    Serial.println();
     
     switch (type) {
            case WStype_DISCONNECTED:             // if the websocket is disconnected
                // Serial.println("[WS] Disconnected!");
                 break;
            case WStype_CONNECTED:               // if a new websocket connection is established
                 Serial.println("[WS] Connected!");
                 webSocket.sendTXT("Connected");     // send message to server when Connected
                 digitalWrite(D2, On);
                 delay(2000);
                 digitalWrite(D2, Off);
                 break;
            case WStype_TEXT:                     // if new text data is received
                 Serial.print("WStype = ");   
                 Serial.println(type);  
                 processJSON(payload, length);
                 break;
            default:  
                 break;
     }                  
}

void processJSON(uint8_t * payload, size_t length) {
      // UType: PreviewSceneChanged
      //              SwitchScene
      // scene-name:  Cam_n
      
      DynamicJsonDocument jsonBuffer(length + 693);
      Serial.println();
      
      auto error = deserializeJson(jsonBuffer, payload);
      if (error) {
          Serial.print(F("deserializeJson() failed with code "));
          Serial.println(error.c_str());
          return;
      }

      String scene_name = jsonBuffer["scene-name"];
      Serial.print("Scene-Name");  
      Serial.print(" = "); 
      Serial.println(scene_name);

      String Utype = jsonBuffer["update-type"];       // "PreviewSceneChanged" or "SwitchScenes"        
      Serial.print("Update-Type"); 
      Serial.print(" = "); 
      Serial.println(Utype);
      if (Utype == type_Pre && scene_name == Camera_ID) {
          Serial.println("Turn 1"); 
          setLEDs(0, LED_pos); 
          setLEDs(1, LED_pos);
          return;
      }
      if (Utype == type_Pre && scene_name != Camera_ID) {
          Serial.println("Turn 2");
          setLEDs(3, LED_pos);
          return;        
      }
      if (Utype == type_Pro && scene_name == Camera_ID) {
          Serial.println("Turn 3");
          setLEDs(0, LED_pos);
          setLEDs(2, LED_pos);
          return;
      }
      if (Utype == type_Pro && scene_name != Camera_ID) {
          Serial.println("Turn 4");
          setLEDs(4, LED_pos);
          return;        
      }
      return;
}

void startwsServer() { // Start a WebSocket server
  wsServer.begin();                          // start the websocket server
  wsServer.onEvent(wsServerEvent);          // if there's an incomming websocket message, go to function 'wsServerEvent'
  Serial.println("WebSocket server started.");
}

void wsServerEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_ERROR:             // if the websocket is disconnected
        Serial.printf("[%u] Error!\n", num);
        break;
    case WStype_DISCONNECTED:             // if the websocket is disconnected
        Serial.printf("[%u] Disconnected!\n", num);
        break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = wsServer.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT: {                    // if new text data is received
      Serial.printf("[%u] get Text: %s Length: %d\n", num, payload, length);
      if (payload[0] == '#') { 
        Serial.println("wsText with #"); 
        int ledNum = payload[1];
        ledNum -= 48;
        int level = String((char *) payload).substring(2).toInt();
        level = map(level, 23, 1023, 1023, 23);
        Serial.println(ledNum);
        Serial.println(level);  
        switch(ledNum) {
          case 0: {
              digitalWrite(D2, Off);
              analogWrite(D1, level);
              break; 
              }                        
          case 1: {
              digitalWrite(D1, Off);
              analogWrite(D2, level);
              break;
              }
          case 2: {
              digitalWrite(D5, Off);
              analogWrite(D4, level);
              break;
              }
          case 3: {
              digitalWrite(D4, Off);
              analogWrite(D5, level);
              break; 
              } 
          default:                              
              break;
          } 
      }
    }
    default:
       break;
  }
}

void ATEMconnect(){
  tempIP.fromString(Switch_IP); 
  AtemSwitcher.begin(tempIP);
  Serial.print("Connecting to ATEM...");
  AtemSwitcher.connect();
  delay(500);
  if (AtemSwitcher.isConnected()){
      Serial.println("ATEM connected!");
      digitalWrite(D1, On);
      delay(250);
      digitalWrite(D1, Off);
      delay(250);
      digitalWrite(D1, On);
      delay(250);
      digitalWrite(D1, Off);
      delay(250);
  }else{
      Serial.println("ATEM connection FAILED!");
      digitalWrite(D2, On);
      delay(250);
      digitalWrite(D2, Off);
      delay(250);
      digitalWrite(D2, On);
      delay(250);
      digitalWrite(D2, Off);
      delay(250);
  }
}

void processATEM(){
// Get camera_num froms Camera_ID, use for index. 
// return if 0; 

  if (camera_num == 0){
    return;
  }
  
  boolean progOn = AtemSwitcher.getProgramTally(camera_num);
  boolean preOn = AtemSwitcher.getPreviewTally(camera_num);

     if (progOn) {
        setLEDs(2, LED_pos); 
     }else{
     if (preOn) {
        setLEDs(1, LED_pos); 
     }else{
        setLEDs(0, LED_pos);
     }
   }  
}

void vMixConnect() {
  IPAddress tempIP;    

  Serial.print("Connecting to vMix host: ");
  Serial.print(Switch_IP);
  Serial.print(":");
  Serial.print(String(Switch_Port));
  Serial.print("...");
  
  tempIP.fromString(Switch_IP);
  if (vmix.connect(tempIP, Switch_Port.toInt()))
  {
    Serial.println("Connected!");

    // Subscribe to the tally events
    vmix.print("SUBSCRIBE TALLY\r\n");
    Serial.println("Subscribed to Tally");
  }else{
    Serial.println("FAILED!");
  }
}

void readvMixData() {
  String data = vmix.readStringUntil('\n');
  if (data.indexOf("TALLY") == 0) {
     Serial.print("vMix Response: ");
     Serial.println(data);
     int tallyState = (data.charAt((data.indexOf("OK") + 2) + camera_num) - 48);  // Convert target tally state value (by device ID) to int
     Serial.println(tallyState);
     if (tallyState == 1) { 
        setLEDs(2, LED_pos);
     }else{
     if (tallyState == 2) { 
        setLEDs(1, LED_pos); 
     }else{
        setLEDs(0, LED_pos); 
     }
    }      
  }
}

void setLEDs(int status, int which) {
  // status: 0 = off, 1 = preview, 2 = program, 3 = preview off, 4 = program off
  // which: 0 = both, 1 = front, 2 = back
  
     switch(status) {
        case 0: //All Off
          digitalWrite(D1, Off);
          digitalWrite(D2, Off);
          digitalWrite(D4, Off);
          digitalWrite(D5, Off);
          Serial.println("ALL OFF");
          break;
        case 1: // Red On
          if(which == both || which == back) { 
            digitalWrite(D1, Off);
            //digitalWrite(D2, On);
            analogWrite(D2, BackRedLevel);
          }
          if(which == both || which == front) {        
            digitalWrite(D4, Off);
            //digitalWrite(D5, On);
            analogWrite(D5, FrontRedLevel);
          }
          Serial.println("PREVIEW ON");
          break;     
       case 2: // Green On
        if(which == both || which == back) {
           //digitalWrite(D1, On);
           analogWrite(D1, BackGreenLevel);
           digitalWrite(D2, Off);
        }
        if(which == both || which == front) {
           //digitalWrite(D4, On);
           analogWrite(D4, FrontGreenLevel);
           digitalWrite(D5, Off);
        }
        Serial.println("PROGRAM ON");
        break;
       case 3: // Green Off
          if(which == both || which == back) { 

            digitalWrite(D2, Off);
          }
          if(which == both || which == front) {        
            digitalWrite(D5, Off);
          }
          Serial.println("PREVIEW ONLY OFF");
          break;
       case 4: // Red Off
          if(which == both || which == back) { 

            digitalWrite(D1, Off);
          }
          if(which == both || which == front) {        
            digitalWrite(D4, Off);
          }
          Serial.println("PROGRAM ONLY OFF");
          break;
     }    
}

void handleRoot() {
     SETUP_HTML = createSETUPhtml();
     Serial.println("SETUP html created");
     server.send(200, "text/html", SETUP_HTML);
}  

String createSETUPhtml(){
  Serial.print("CONFIGURED status: ");
  Serial.println(CONFIGURED);
  if (CONFIGURED == "0"){
    clearGlobals();
  }

  String HTMLsetup = HTMLheader + "<!DOCTYPE HTML><html><head><meta content=\"text/html; charset=ISO-8859-1\"http-equiv=\"content-type\"><title>TPIII Seteup</title>\
<style>\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; } \
.slider { -webkit-appearance: none; margin: 14px; width: 600px; height: 25px; background: #FFD65C; outline: none; -webkit-transition: .2s; transition: opacity .2s;} \
.slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #003249; cursor: pointer;} \
.slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer;} </style>";

  HTMLsetup += "<script>var connection = new WebSocket('ws://' + location.hostname + ':9666/', ['arduino']);\
  connection.onopen = function () { connection.send('Connect ' + new Date()); };\
  connection.onerror = function (error) { console.log('WebSocket Error ', error); };\
  connection.onmessage = function (e) { console.log('Server: ', e.data); };\
  connection.onclose = function () { console.log('WebSocket connection closed'); };</script> </head><body>";

  HTMLsetup += "<h1 align=\"center\">TPIII Setup</h1><br>";
  HTMLsetup += "<h2 align=\"center\">Critical Info</h2><br>";
  HTMLsetup += "<FORM action=\"/action_page\">";

  HTMLsetup += "<label for=\"nets\">Local WiFi: Network SSID:</label>";
  HTMLsetup += "<select name =\"nets\" id = \"nets\" autofocus required>";
  int numNets = WiFi.scanNetworks();
  for(int i=0;i<numNets;i++){
     HTMLsetup += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</option>";
  }
  HTMLsetup += "</select>";
 
  HTMLsetup += "Password:<input type=\"text\" name=\"netpass1\" value = \"" + wifi_pswd + "\" required><br><br>";
  HTMLsetup += "If you want to assign a static IP address to the light,<br>the next three fields MUST be populated, otherwise leave blank.<br>";
  HTMLsetup += "Static IP(optional):<input type=\"text\" name=\"sip1\" value = \"" + wifi_sip + "\" pattern=\"^(?:[0-9]{1,3}.){3}[0-9]{1,3}$\" placeholder=\"xxx.xxx.xxx.xxx\"><br>";
  HTMLsetup += "Gateway IP(optional):<input type=\"text\" name=\"gate1\" value = \"" + wifi_gateway + "\" pattern=\"^(?:[0-9]{1,3}.){3}[0-9]{1,3}$\" placeholder=\"xxx.xxx.xxx.xxx\"><br>";
  HTMLsetup += "Subnet Mask(optional):<input type=\"text\" name=\"subnet1\" value = \"" + wifi_subnet + "\" pattern=\"^(?:[0-9]{1,3}.){3}[0-9]{1,3}$\" placeholder=\"xxx.xxx.xxx.xxx\"><br><br>";

  HTMLsetup += "<div>Switcher: ";
  HTMLsetup += "<input type=\"radio\" id=\"obs\" name=\"swType\" value=\"0\" onclick=\"portOBS()\"";
  if (Switch_Type == 0){
    HTMLsetup += " checked";
  }
  HTMLsetup += "><label for=\"obs\">OBS</label>";  
  HTMLsetup += "<input type=\"radio\" id=\"atem\" name=\"swType\" value=\"1\" onclick=\"portATEM()\"";
  if (Switch_Type == 1){
    HTMLsetup += " checked";
  }
  HTMLsetup += "><label for=\"atem\">ATEM</label>";
  HTMLsetup += "<input type=\"radio\" id=\"vmix\" name=\"swType\" value=\"2\" onclick=\"portVMIX()\"";
  if (Switch_Type == 2){
    HTMLsetup += " checked";
  }
  HTMLsetup += "><label for=\"vmix\">Vmix</label></div><br>";
 
  HTMLsetup += "Switcher IP Address:<input type=\"text\" name=\"ip\" value = \"" + Switch_IP + "\" required pattern=\"^(?:[0-9]{1,3}.){3}[0-9]{1,3}$\" placeholder=\"xxx.xxx.xxx.xxx\">";
  if (Switch_Port.length() == 0) {Switch_Port = "4444";};
  HTMLsetup += "Switcher Port:<input type=\"text\" name=\"port\" value = \"" + Switch_Port + "\" id=\"portField\" required pattern=\"[0-9]{1,4}\"><br><br>";
  HTMLsetup += "Switcher Password (if used):<input type=\"text\" name=\"pswd\" value = \"" + Switch_Pswd + "\"><br>";
  HTMLsetup += "Scene Name (OBS) or Camera Number (ATEM, Vmix):<input type=\"text\" name=\"scenename\" value = \"" + Camera_ID + "\" required><br><br>";
 
  HTMLsetup += "<div>LED configuration: ";
  HTMLsetup += "<input type=\"radio\" id=\"both\" name=\"ledpos\" value=\"0\"";
  if (LED_pos == 0) {
    HTMLsetup += " checked";
  }
  HTMLsetup += "><label for=\"both\">Both</label>";
  HTMLsetup += "<input type=\"radio\" id=\"front\" name=\"ledpos\" value=\"1\"";
  if (LED_pos == 1){
    HTMLsetup += " checked";
  }
  HTMLsetup += "><label for=\"front\">Front Only</label>";
  HTMLsetup += "<input type=\"radio\" id=\"back\" name=\"ledpos\" value=\"2\"";
  if (LED_pos == 2){
    HTMLsetup += " checked";
  }
  HTMLsetup += "><label for=\"back\">Back Only</label></div><br>";
  
  HTMLsetup += "<style> td, th { text-align: left;}</style>";
  HTMLsetup += "<table>";
  HTMLsetup += "<tr><td>LED brightness:</td></tr>";
  HTMLsetup += "<tr><td>Back Red:</td><td><input type=\"range\" id=\"pwm0\" name=\"pwmSlider0\" min=\"23\" max=\"1023\" value=\"0\" step=\"100\" oninput=\"sendPWM0();\" class=\"slider\"></td></tr>";
  HTMLsetup += "<tr><td>Back Green:</td><td><input type=\"range\" id=\"pwm1\" name=\"pwmSlider1\" min=\"23\" max=\"1023\" value=\"0\" step=\"100\" oninput=\"sendPWM1();\" class=\"slider\"></td></tr>";
  HTMLsetup += "<tr><td>Front Red:</td><td><input type=\"range\" id=\"pwm2\" name=\"pwmSlider2\" min=\"23\" max=\"1023\" value=\"0\" step=\"100\" oninput=\"sendPWM2();\" class=\"slider\"></td></tr>";
  HTMLsetup += "<tr><td>Front Green:</td><td><input type=\"range\" id=\"pwm3\" name=\"pwmSlider3\" min=\"23\" max=\"1023\" value=\"0\" step=\"100\" oninput=\"sendPWM3();\" class=\"slider\"></td></tr>";
  HTMLsetup += "</table>";
 
  HTMLsetup += "<p align=\"center\"><INPUT type=\"submit\" value=\"Submit\"><INPUT type=\"reset\"></p></FORM>";
  
  HTMLsetup += "<script>function portOBS(){document.getElementById(\"portField\").value = \"4444\";}</script>";
  HTMLsetup += "<script>function portATEM(){document.getElementById(\"portField\").value = \"9990\";}</script>";
  HTMLsetup += "<script>function portVMIX(){document.getElementById(\"portField\").value = \"8099\";}</script>";

  HTMLsetup += "<script>function sendPWM0() {var sliderValue = document.getElementById('pwm0').value; var pwmstr0 = '#0' + sliderValue.toString(4); connection.send(pwmstr0); } </script>";
  HTMLsetup += "<script>function sendPWM1() {var sliderValue = document.getElementById('pwm1').value; var pwmstr1 = '#1' + sliderValue.toString(4); connection.send(pwmstr1); } </script>";
  HTMLsetup += "<script>function sendPWM2() {var sliderValue = document.getElementById('pwm2').value; var pwmstr2 = '#2' + sliderValue.toString(4); connection.send(pwmstr2); } </script>";
  HTMLsetup += "<script>function sendPWM3() {var sliderValue = document.getElementById('pwm3').value; var pwmstr3 = '#3' + sliderValue.toString(4); connection.send(pwmstr3); } </script>";

  HTMLsetup += "</body></html>";
  return HTMLsetup;
}

void handleForm() {
     Serial.println("handleForm");
     String html_ssid = server.arg("nets");
     String html_pswd = server.arg("netpass1");
     String html_sip = server.arg("sip1");
     String html_gateway = server.arg("gate1");
     String html_subnet = server.arg("subnet1");
     String html_Switch_Type = server.arg("swType"); 
     String html_Switch_IP = server.arg("ip");
     String html_Switch_Port = server.arg("port");
     String html_Switch_Pswd = server.arg("pswd");
     String html_camera = server.arg("scenename");
     String html_LED_pos = server.arg("ledpos");
     String html_FrontRedLevel = server.arg("pwmSlider0");
     String html_FrontGreenLevel = server.arg("pwmSlider1");
     String html_BackRedLevel = server.arg("pwmSlider2");
     String html_BackGreenLevel = server.arg("pwmSlider3");
     Serial.println(html_ssid);
     Serial.println(html_pswd);
     Serial.println(html_sip);
     Serial.println(html_gateway);
     Serial.println(html_subnet);
     Serial.println(html_Switch_Type);
     Serial.println(html_Switch_IP);
     Serial.println(html_Switch_Port);     
     Serial.println(html_Switch_Pswd);
     Serial.println(html_camera);
     Serial.println(html_LED_pos);
     Serial.println(html_FrontRedLevel);
     Serial.println(html_FrontGreenLevel);
     Serial.println(html_BackRedLevel);
     Serial.println(html_BackGreenLevel);

     Serial.println("handleForm memory writes");
     write_to_Memory(html_ssid,offset_ssid);
     write_to_Memory(html_pswd,offset_pswd);
     write_to_Memory(html_sip,offset_sip);
     write_to_Memory(html_gateway,offset_gateway);
     write_to_Memory(html_subnet,offset_subnet);
     
     if (html_Switch_Type == "0") {
      write_to_Memory("0",offset_SwitchType);
      }
     if (html_Switch_Type == "1") {
      write_to_Memory("1",offset_SwitchType);
      }
     if (html_Switch_Type == "2") {
      write_to_Memory("2",offset_SwitchType);
      }
      
     write_to_Memory(html_Switch_IP,offset_SwitchIP);
     write_to_Memory(html_Switch_Port,offset_SwitchPort);    
     write_to_Memory(html_Switch_Pswd,offset_SwitchPswd);
     write_to_Memory(html_camera,offset_Camera_ID);

     if (html_LED_pos == "0") {
      write_to_Memory("0",offset_LED_pos);
      }
     if (html_LED_pos == "1") {
      write_to_Memory("1",offset_LED_pos);
      }
     if (html_LED_pos == "2") {
      write_to_Memory("2",offset_LED_pos);
      }
     String sLevels = packLevels(html_FrontRedLevel, html_FrontGreenLevel, html_BackRedLevel, html_BackGreenLevel);
     write_to_Memory(sLevels, offset_LED_Levels);
     EEPROM.begin(512);
     EEPROM.write(offset_cflag, 'Y');
     EEPROM.commit();
     CONFIGURED = "1";
     setLEDs(0,0);
     sendAckPage();    
}

String createACKhtml(){
  wifi_ssid = read_from_Memory(offset_ssid);
  wifi_pswd = read_from_Memory(offset_pswd);
  wifi_sip = read_from_Memory(offset_sip);
  wifi_gateway = read_from_Memory(offset_gateway);
  wifi_subnet = read_from_Memory(offset_subnet);
  Switch_Type = read_from_Memory(offset_SwitchType).toInt();
  Switch_IP = read_from_Memory(offset_SwitchIP);
  Switch_Port = read_from_Memory(offset_SwitchPort);  
  Switch_Pswd = read_from_Memory(offset_SwitchPswd);
  Camera_ID = read_from_Memory(offset_Camera_ID);
  LED_pos = read_from_Memory(offset_LED_pos).toInt();
  String LED_Levels = read_from_Memory(offset_LED_Levels);
  FrontRedLevel = unpackLevels(0, LED_Levels);
  FrontGreenLevel = unpackLevels(1, LED_Levels);
  BackRedLevel = unpackLevels(2, LED_Levels);
  BackGreenLevel = unpackLevels(3, LED_Levels); 
  
  String HTMLsetup = "<!DOCTYPE HTML><html><head><meta content=\"text/html; charset=ISO-8859-1\"http-equiv=\"content-type\"><title>TPIII Ackknowlegement</title>\
  <style type=\"text\css\"> td, th { text-align: left;} . centerText{text-align: center;}</style></head><body>";
  HTMLsetup += "<center><h1>TallyLight Setup</h1><p>";
  HTMLsetup += "<h2>Acknowledgement</h2><br>";
 
  HTMLsetup += "<table>";
  HTMLsetup += "<tr><td><br/></td></tr>";
  HTMLsetup += "<tr><td><b>Software Version: </b></td><td>" + Version + "</td></tr>";
  HTMLsetup += "<tr><td><b>Local WiFi Network SSID: </b></td><td>" + wifi_ssid + "</td></tr>";
  HTMLsetup += "<tr><td><b>Local WiFi Password: </b></td><td>" + wifi_pswd + "</td></tr>";
  if (wifi_sip.length() == 0) {
    HTMLsetup += "<tr><th colspan=\"2\">TallyPro IP address will be dynamically assigned</th></tr>";
  }else{
    HTMLsetup += "<tr><td><b>TallyPro Static IP Address: </b></td><td>" + wifi_sip + "</td></tr>";
    HTMLsetup += "<tr><td><b>TallyPro Gateway Address: </b></td><td>" + wifi_gateway + "</td></tr>";
    HTMLsetup += "<tr><td><b>TallyPro Subnet Mask: </b></td><td>" + wifi_subnet + "</td></tr>";
  }
  HTMLsetup += "<tr><td><br/></td></tr>";

  switch(Switch_Type){
    case 0:
      HTMLsetup += "<tr><th class=\"centerText\" colspan=\"2\">Using OBS as the switcher</th></tr>";
      break;
    case 1:
      HTMLsetup += "<tr><th class=\"centerText\" colspan=\"2\">Using ATEM as the switcher</th></tr>";
      break;
    case 2:
      HTMLsetup += "<tr><th class=\"centerText\" colspan=\"2\">Using Vmix as the switcher</th></tr>";           
  }
  HTMLsetup += "<tr><td><b>Switcher IP Adress: </b></td><td>" + Switch_IP + "</td></tr>";
  HTMLsetup += "<tr><td><b>Switcher Port: </b></td><td>" + Switch_Port + "</td></tr>";
  if(Switch_Pswd.length() == 0){
     HTMLsetup += "<tr><td><b>Switch Password: </b></td><td>N/A</td></tr>";    
  }else{
     HTMLsetup += "<tr><td><b>Switch Password: </b></td><td>" + Switch_Pswd + "</td></tr>";
  }
  HTMLsetup += "<tr><td><b>Scene Name or Camera Number: </b></td><td>" + Camera_ID + "</td></tr>";
  HTMLsetup += "<tr><td><br/></td></tr>";

  switch(LED_pos){
    case both:
      HTMLsetup += "<tr><th class=\"centerText\" colspan=\"2\">Both Front and Back LEDS will light</th></tr>";
      HTMLsetup += "<tr><td><b>Front Red Brightness</b></td><td>" + String(map(FrontRedLevel,23,1023,0, 100)) + " %</td></tr>";
      Serial.println(String(map(FrontRedLevel,23,1023,0, 100)));
      HTMLsetup += "<tr><td><b>Front Green Brightness</b></td><td>" + String(map(FrontGreenLevel,23,1023,0,100)) + " %</td></tr>";
      Serial.println(String(map(FrontGreenLevel,23,1023,0, 100)));
      HTMLsetup += "<tr><td><b>Back Red Brightness</b></td><td>" + String(map(BackRedLevel,23,1023,0,100)) + " %</td></tr>";
      Serial.println(String(map(BackRedLevel,23,1023,0, 100)));
      HTMLsetup += "<tr><td><b>Back Green Brightness</b></td><td>" + String(map(BackGreenLevel,23,1023,0,100)) + " % </td></tr>";
      Serial.println(String(map(BackGreenLevel,23,1023,0, 100)));
      break;
    case front:
      HTMLsetup += "<tr><th class=\"centerText\" colspan=\"2\">Only Front LED will light</th></tr>";
      HTMLsetup += "<tr><td><b>Front Red Brightness</b></td><td>" + String(map(FrontRedLevel,23,1023,0,100)) + " %</td></tr>";
      HTMLsetup += "<tr><td><b>Front Green Brightness</b></td><td>" + String(map(FrontGreenLevel,23,1023,0,100)) + " %</td></tr>";
      break;
    case back:
      HTMLsetup += "<tr><th class=\"centerText\" colspan=\"2\">Only Back LED will light</th></tr>";
      HTMLsetup += "<tr><td><b>Back Red Brightness</b>" + String(map(BackRedLevel,23,1023,0,100)) + " %</td></tr>";
      HTMLsetup += "<tr><td><b>Back Green Brightness</b>" + String(map(BackGreenLevel,23,1023,0,100)) + " %</td></tr></table>";           
  }  
  HTMLsetup += "Your TallyLight is configured!</h2><br><br>";
  HTMLsetup += "The light will blink red while connecting to WiFi<br>";
  HTMLsetup += "This may take up to 30 seconds.<br>";  
  HTMLsetup += "the light will turn green for 5 seconds<br>";
  HTMLsetup += "after connecting to the switcher<br>";  
  HTMLsetup += "<b>To reconfigure the TallyLight,<br>press and hold the RESET button (!) for 1 second.</b><br>";
  HTMLsetup += "</body></html>";
  return HTMLsetup;
}

void sendAckPage() {
     SETUP_HTML = createACKhtml();
     Serial.println("ACK html created");
     server.send(200, "text/html", SETUP_HTML);
     delay(250);
}

void handleControl() {
     SETUP_HTML = HTMLheader + createControl();
     Serial.println("Control html created");
     server.send(200, "text/html", SETUP_HTML);
}

String createControl() {
  String HTMLsetup = "<h1 align=\"center\">TallyLight Control</h1><br>";
  HTMLsetup += "<h2 align=\"center\">Critical Info</h2><br>";
  HTMLsetup += "<FORM action=\"/action_control\">";
  HTMLsetup += "Tally Light IP:<input type=\"text\" name=\"tlip\" value = \"\" pattern=\"^(?:[0-9]{1,3}.){3}[0-9]{1,3}$\" placeholder=\"xxx.xxx.xxx.xxx\"><br>";
  HTMLsetup += "<p align=\"center\"><INPUT type=\"submit\" value=\"Submit\"><INPUT type=\"reset\"></p></FORM>";
  return HTMLsetup;  
}

void handleControlForm() {
 /* if () {

  }
  wifi_ssid = read_from_Memory(offset_ssid);
  wifi_sip = read_from_Memory(offset_sip);
  Switch_Type = read_from_Memory(offset_SwitchType).toInt();
  Camera_ID = read_from_Memory(offset_Camera_ID);
  String HTMLsetup = "<center><h1>TallyLight Control</h1><p>";
  HTMLsetup += "<h2>Information</h2><br>";
  HTMLsetup += "<b>TallyPro Static IP Address: </b>" + wifi_sip + "<br>";
  HTMLsetup += "<b>Local WiFi Network SSID: </b>" + wifi_ssid + "<br>";
  switch(Switch_Type){
    case 0:
      HTMLsetup += "Using OBS as the switcher<br>";
      break;
    case 1:
      HTMLsetup += "Using ATEM as the switcher<br>";
      break;
    case 2:
      HTMLsetup += "Using Vmix as the switcher<br>";           
  }
  HTMLsetup += "<b>Scene Name or Camera Number: </b>" + Camera_ID + "<br>";
*/
}

void handleNotFound(){
  String message = "File Not Found\n\nURI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  message +="<H2><a href=\"/\">go home</a></H2><br>";
  server.send(404, "text/plain", message);
}

void clearGlobals(){
wifi_ssid = "";
wifi_pswd = "";
wifi_sip = "";
wifi_gateway = "";
wifi_subnet = "";
Switch_Type = 0;
Switch_IP = "";
Switch_Port = "";
Switch_Pswd = "";
Camera_ID = "";
camera_num = 0;
LED_pos = 0;
FrontRedLevel = 0;
FrontGreenLevel = 0;
BackRedLevel = 0;
BackGreenLevel = 0;
}

String packLevels(String sFR, String sFG, String sBR, String sBG){
 String packed;
 packed = ";" + sFR + ";" + sFG + ";" + sBR + ";" + sBG + ";";
 return packed;
}

int unpackLevels(int pos, String levels){
int unpacked;
int ind1;
int ind2;
String temp;

ind1 = 0;
ind2 = levels.indexOf(";", 1);
for (int i = 0; i<pos; i++){
    ind1=ind2;
    ind2=levels.indexOf(";", ind2+1);
}

//Serial.println(levels.substring(ind1+1, ind2));
temp = levels.substring(ind1+1, ind2);
//Serial.println(levels);
//Serial.println(ind1);
//Serial.println(ind2);
//Serial.println(temp);
unpacked = temp.toInt();
//Serial.println(unpacked);
return unpacked;
}

void write_to_Memory(String s, int offset){
  char arS[32];
  EEPROM.begin(512);
  s.toCharArray(arS, s.length()+1);
  Serial.println(arS);
  EEPROM.put(offset,arS);
  EEPROM.commit();
  EEPROM.end();
}

String read_from_Memory(int offset) {
  char arS[32];
  String s;
  EEPROM.begin(512);
  s = EEPROM.get(offset,arS);
  return s;
}

void clear_Memory(int memsize) {
    for (int i = 0 ; i <= memsize ; i++) {
      EEPROM.begin(512);
      EEPROM.write(i, 255);
      EEPROM.commit();
      EEPROM.end();
    }
}
/*
String authWebsocket(String password, String challenge, String salt){
  byte hash[SHA256_BLOCK_SIZE];
  byte hash2[SHA256_BLOCK_SIZE];
  //char texthash[2*SHA256_BLOCK_SIZE+1];
  Sha256* secret_hash = new Sha256();
  Sha256* secret_hash2 = new Sha256();

  String secret_string = password + salt;
  byte text[secret_string.length()];
  secret_string.getBytes(text, sizeof(text));
  secret_hash->update(text, sizeof(text));
  secret_hash->final(hash);
  String secret = base64::encode(String((char *)hash));

  String arString = secret + challenge;
  byte text2[arString.length()];
  arString.getBytes(text2, sizeof(text2));
  secret_hash2->update(text2, sizeof(text2));
  secret_hash2->final(hash2);
  String secret2 = base64::encode(String((char *)hash2));

  delete secret_hash;
  delete secret_hash2;
  return secret2;
}
*/