
/* 
Tally Pro Firmware
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
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#include <string.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ATEMstd.h>
#include "tallylight.h"

const char* host = "esp8266-webupdate";

int   vmixPort;                             // Configurable Port number (default = 8099)
//char  vMixHostName[32];                     // Hostname of the PC running vMix
byte  id = 0;                               // Device ID 0-255 (Default = 0) 

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
String SETUP_HTML;

WebSocketsClient webSocket;
WiFiClient vmix;

ATEMstd AtemSwitcher; 
IPAddress tempIP;

String     type_Pre = "PreviewSceneChanged";
String     type_Pro = "SwitchScenes";

int LED_Pre = 4;
int LED_Pro = 5;
int On = LOW;
int Off = HIGH;
 
void setup() {
  pinMode(LED_Pre, OUTPUT);    // Preview
  digitalWrite(LED_Pre, Off);
  pinMode(LED_Pro, OUTPUT);    // Program
  digitalWrite(LED_Pro, Off);
  pinMode(0, INPUT_PULLUP);    // Flash button on ESP8266
  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(1000);
  
  Serial.println("START");
  testLEDS();
  getMemory();
  configureWebServer();
  if (!checkMemory()) {
    Serial.print("CONFIGURED: ");
    Serial.println(CONFIGURED);
    while(CONFIGURED != "1"){  // Connection info not configured, so check for web page activity
         server.handleClient();       //Checks for web server activity
         delay(1000);
    }
  }  
  startWiFi();                 // Connect to the specified Network
  startWebSocket();            // Start a WebSocket

  MDNS.begin(host);

  httpUpdater.setup(&server);
  server.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);

  tempIP.fromString(atem_IP); 
  AtemSwitcher.begin(tempIP);
  Serial.println("Connecting to ATEM...");
  AtemSwitcher.connect();
  delay(500);
  if (AtemSwitcher.isConnected()){
      Serial.println("ATEM connected!");
      digitalWrite(LED_Pre, On);
      delay(250);
      digitalWrite(LED_Pre, Off);
      delay(250);
      digitalWrite(LED_Pre, On);
      delay(250);
      digitalWrite(LED_Pre, Off);
      delay(250);
  }else{
      Serial.println("ATEM connection FAILED!");
      digitalWrite(LED_Pro, On);
      delay(250);
      digitalWrite(LED_Pro, Off);
      delay(250);
      digitalWrite(LED_Pro, On);
      delay(250);
      digitalWrite(LED_Pro, Off);
      delay(250);
  }
  vMixConnect(); 
}

void loop() {
  webSocket.loop();            // constantly check for websocket events
  AtemSwitcher.runLoop();
  processATEM();
//String data = client.readStringUntil('\n');
  readvMixData(vmix.readStringUntil('\n'));
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

void testLEDS() {
  digitalWrite(LED_Pre, Off);
  delay(500);
  digitalWrite(LED_Pre, On);
  delay(500);
  digitalWrite(LED_Pre, Off);
  delay(500);
  
  digitalWrite(LED_Pro, Off);
  delay(500);
  digitalWrite(LED_Pro, On);
  delay(500);
  digitalWrite(LED_Pro, Off);
  delay(500);
}

void configureWebServer() {
  //WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet);
  WiFi.mode(WIFI_AP);
  IPAddress myIP = WiFi.softAPIP();
  Serial.println("");
  Serial.print("AP IP address: ");
  Serial.println(myIP); 
  // Setup server
  server.on("/", handleRoot);
  server.on("/action_page", handleForm);  
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("server(80) started");
}

void startWiFi() { // Connect to WiFI specified
  getMemory();
  WiFi.begin(wifi_ssid, wifi_pswd);             // connect
  Serial.print("Connecting to ");
  Serial.print(wifi_ssid); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {      // Wait for the Wi-Fi to connect
    digitalWrite(LED_Pro, On);
    delay(500);
    digitalWrite(LED_Pro, Off);
    delay(1000);
    Serial.print(++i); Serial.print(' ');
    if (i > 30 ) {
        Serial.println('\n');
        Serial.println("WiFi connection not established!");
        digitalWrite(LED_Pro, On);
        delay(250);
        digitalWrite(LED_Pro, Off);
        delay(250);
        digitalWrite(LED_Pro, On);
        delay(250);
        digitalWrite(LED_Pro, Off);
        delay(250);
        return;  
    }
  }
  
  Serial.println('\n');
  Serial.println("WiFi connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());              // Send the IP address of the ESP8266 to the computer
  digitalWrite(LED_Pre, On);
  delay(250);
  digitalWrite(LED_Pre, Off);
  delay(250);
  digitalWrite(LED_Pre, On);
  delay(250);
  digitalWrite(LED_Pre, Off);
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
    Serial.println("getMemory");
    wifi_ssid = read_from_Memory(offset_ssid);
    wifi_pswd = read_from_Memory(offset_pswd);
    obs_pswd = read_from_Memory(offset_obs_pswd);
    camera_name = read_from_Memory(offset_camera_name);
    if (camera_name.length() == 1  && isDigit(camera_name.charAt(0))){ 
      camera_num = camera_name.toInt();
    }else{
      camera_num = 0;
    }
    obs_IP = read_from_Memory(offset_obsIP);
    atem_pswd = read_from_Memory(offset_atem_pswd);
    atem_IP = read_from_Memory(offset_atemIP);
    vmix_IP = read_from_Memory(offset_vmixIP);
    vmixPort = read_from_Memory(offset_vmixPort).toInt();    
    Serial.println(wifi_ssid);
    Serial.println(wifi_pswd);
    Serial.println(camera_name);
    Serial.println(camera_num);
    Serial.println(obs_IP);
    Serial.println(obs_pswd);
    Serial.println(atem_IP);
    Serial.println(atem_pswd);
    Serial.println(vmix_IP);
    Serial.println(vmixPort);
  }

void startWebSocket() {                            // Start a WebSocket client
  webSocket.begin(obs_IP, 4444);                   // connect to the websocket
  Serial.println("WebSocket client started.");
  webSocket.onEvent(webSocketEvent);               // respond to message
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) { // When a WebSocket message is received
//     Serial.println("WebSocket message.");
//     Serial.print("WStype = ");   Serial.println(type);  
//     Serial.print("WS payload = ");
// since payload is a pointer we need to type cast to char
     for(int i = 0; i < length; i++) { Serial.print((char) payload[i]); }
     Serial.println();
     
     switch (type) {
            case WStype_DISCONNECTED:             // if the websocket is disconnected
//                 Serial.printf("[WSc] Disconnected!\n");
                 break;
            case WStype_CONNECTED:               // if a new websocket connection is established
                 Serial.printf("[WSc] Connected!\n");
                 webSocket.sendTXT("Connected");     // send message to server when Connected
                 digitalWrite(LED_Pre, On);
                 delay(2000);
                 digitalWrite(LED_Pre, Off);
                 break;
            case WStype_TEXT:                     // if new text data is received
                 Serial.println("[WSc] get Text");
                 Serial.print("WStype = ");   Serial.println(type);  
                 Serial.print("length = ");   Serial.println(length); 
                 Serial.print("WS payload = ");
                 processJSON(payload, length);
                 break;
            default:  
                 break;
     }                  
}

void processATEM(){
// Get camera_num fromscaen_name, use for index. 
// return if 0; 

  if (camera_num == 0){
    return;
  }
  
  boolean progOn = AtemSwitcher.getProgramTally(camera_num);
  boolean preOn = AtemSwitcher.getPreviewTally(camera_num);

     if (progOn) { 
        digitalWrite(LED_Pre, Off);
        digitalWrite(LED_Pro, On);
        Serial.println("PROGRAM");
     }else{
     if (preOn) { 
        digitalWrite(LED_Pre, On);
        digitalWrite(LED_Pro, Off);
        Serial.println("PREVIEW");
     }else{
        digitalWrite(LED_Pre, Off);
        digitalWrite(LED_Pro, Off);
     }
   }  
}

void vMixConnect() {
  IPAddress tempIP;    

  Serial.print("Connecting to vMix host: ");
  Serial.print(vmix_IP);
  Serial.print(":");
  Serial.println(String(vmixPort));
  
  tempIP.fromString(vmix_IP);
  if (vmix.connect(tempIP, vmixPort))
  {
    Serial.println("Connected!");

    // Subscribe to the tally events
    vmix.print("SUBSCRIBE TALLY\r\n");
    Serial.println("Subscribed to Tally");
  }
  else
  {
//    lastRetry = millis();                             // Remember current (relative) time in msecs.
    Serial.println("FAILED!");
  }
}

// ----- Read vMix Tally Packet -----
void readvMixData(String data) {
  if (data.indexOf("TALLY") == 0) {
     Serial.print("vMix Response: ");
     Serial.println(data);
     int tallyState = (data.charAt((data.indexOf("OK") + 2) + camera_num) - 48);  // Convert target tally state value (by device ID) to int
     Serial.println(tallyState);
     if (tallyState == 1) { 
        digitalWrite(LED_Pre, Off);
        digitalWrite(LED_Pro, On);
        Serial.println("PROGRAM");
     }else{
          if (tallyState == 2) { 
             digitalWrite(LED_Pre, On);
             digitalWrite(LED_Pro, Off);
             Serial.println("PREVIEW");
          }else{
               digitalWrite(LED_Pre, Off);
               digitalWrite(LED_Pro, Off);
          }
     }      
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

      if (Utype == type_Pre) {
              if (scene_name != camera_name) { 
                 digitalWrite(LED_Pre, Off);
                 return;
              }
              digitalWrite(LED_Pro, Off);
              digitalWrite(LED_Pre, On);
              return; 
      }
      if (Utype == type_Pro) {
              if (scene_name != camera_name) { 
                 digitalWrite(LED_Pro, Off);
                 return;
              }
              digitalWrite(LED_Pre, Off);
              digitalWrite(LED_Pro, On);
              return;                    
      }
      return;
    }

String createSETUPhtml(){
  String HTMLsetup = "<h1 align=\"center\">Tally Setup</h1><br>";
  HTMLsetup += "<h2 align=\"center\">Critical Info</h2><br>";
  HTMLsetup += "<FORM action=\"/action_page\">";
  HTMLsetup += "Local WiFi Network SSID:<input type=\"text\" name=\"network1\" value = \"\"><br>";
  HTMLsetup += "Local WiFi Password:<input type=\"text\" name=\"netpass1\" value = \"\"><br>";
  HTMLsetup += "OBS computer IP Address:<input type=\"text\" name=\"obsip\" value = \"\">";
  HTMLsetup += "OBS Websocket Password (if used):<input type=\"text\" name=\"obspass\" value = \"\"><br>";
  HTMLsetup += "ATEM IP Address:<input type=\"text\" name=\"atemip\" value = \"\">";
  HTMLsetup += "ATEM Password (if used):<input type=\"text\" name=\"atempass\" value = \"\"><br>";
  HTMLsetup += "Vmix IP Address:<input type=\"text\" name=\"vmixip\" value = \"\">";
  HTMLsetup += "Vmix Port:<input type=\"text\" name=\"vmixPort\" value = \"\"><br>";  
  HTMLsetup += "Scene Name (OBS) or Camera Number(ATEM, Vmix):<input type=\"text\" name=\"scenename\" value = \"\">";
  HTMLsetup += "<p align=\"center\"><INPUT type=\"submit\" value=\"Submit\"><INPUT type=\"reset\"></P></FORM></body></html>";
  return HTMLsetup;
}

String createACKhtml(){
  wifi_ssid = read_from_Memory(offset_ssid);
  wifi_pswd = read_from_Memory(offset_pswd);
  obs_IP = read_from_Memory(offset_obsIP);  
  obs_pswd = read_from_Memory(offset_obs_pswd);
  atem_IP = read_from_Memory(offset_atemIP);  
  atem_pswd = read_from_Memory(offset_atem_pswd);
  vmix_IP = read_from_Memory(offset_vmixIP);  
  vmixPort = read_from_Memory(offset_vmixPort).toInt();
  camera_name = read_from_Memory(offset_camera_name);
  
  String HTMLsetup = "<center><h1>Tally Setup</h1><p>";
  HTMLsetup += "<h2>Acknowledgement</h2><br>";
  HTMLsetup += "<b>Local WiFi Network SSID: </b>" + wifi_ssid + "<br>";
  HTMLsetup += "<b>Local WiFi Password: </b>" + wifi_pswd + "<br>";
  HTMLsetup += "<b>OBS IP Adress: </b>" + obs_IP + "<br>";
  HTMLsetup += "<b>OBS Websocket Password: </b>" + obs_pswd + "<br>";
  HTMLsetup += "<b>ATEM IP Adress: </b>" + atem_IP + "<br>";
  HTMLsetup += "<b>ATEM Password: </b>" + atem_pswd + "<br>";
  HTMLsetup += "<b>Vmix IP Adress: </b>" + vmix_IP + "<br>";
  HTMLsetup += "<b>Vmix Port: </b>" + String(vmixPort) + "<br>";  
  HTMLsetup += "<b>Scene Name or Camera Number: </b>" + camera_name + "<br><br>";
  HTMLsetup += "Your TallyLight is configured!</h2><br>";
  HTMLsetup += "The light will blink red while connecting to WiFi<br>";
  HTMLsetup += "This may take up to 30 seconds.<br>";  
  HTMLsetup += "the light will turn green for 5 seconds<br>";
  HTMLsetup += "after connecting to OBS<br>";  
  HTMLsetup += "<h2 align=\"center\">To reconfigure the TallyLight,</h2><p>";
  HTMLsetup += "<h2 align=\"center\">press and hold the RESET button for 1 second.</h2><br>";
  return HTMLsetup;
}

void handleRoot() {
     SETUP_HTML = HTMLheader + createSETUPhtml();
     Serial.println("SETUP html created");
     server.send(200, "text/html", SETUP_HTML);
}

void handleForm() {
     Serial.println("Submit pressed");
     String html_ssid = server.arg("network1"); 
     String html_pswd = server.arg("netpass1"); 
     String html_obsIP = server.arg("obsip");
     String html_obspass = server.arg("obspass");
     String html_atemIP = server.arg("atemip");
     String html_atempass = server.arg("atempass");  
     String html_vmixIP = server.arg("vmixip");
     String html_vmixPort = server.arg("vmixPort");
     String html_camera = server.arg("scenename"); 
     Serial.println(html_ssid);
     Serial.println(html_pswd);
     Serial.println(html_obsIP);     
     Serial.println(html_obspass);
     Serial.println(html_atemIP);     
     Serial.println(html_atempass);
     Serial.println(html_vmixIP);     
     Serial.println(html_vmixPort);
     
     Serial.println(html_camera);
     write_to_Memory(html_ssid,offset_ssid);
     write_to_Memory(html_pswd,offset_pswd);
     write_to_Memory(html_obsIP,offset_obsIP);
     write_to_Memory(html_obspass,offset_obs_pswd);
     write_to_Memory(html_atemIP,offset_atemIP);
     write_to_Memory(html_atempass,offset_atem_pswd);
     write_to_Memory(html_vmixIP,offset_vmixIP);
     write_to_Memory(html_vmixPort,offset_vmixPort);     
     write_to_Memory(html_camera,offset_camera_name);
     EEPROM.begin(512);
     EEPROM.write(offset_cflag, 'Y');
     EEPROM.commit();
     CONFIGURED = "1";
     sendAckPage();    
}

void sendAckPage() {
     SETUP_HTML = HTMLheader + createACKhtml();
     Serial.println("ACK html created");
     server.send(200, "text/html", SETUP_HTML);
}

//Shows when we get a misformed or wrong request for the web server
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
