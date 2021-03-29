
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

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
String SETUP_HTML;

WebSocketsClient webSocket;
WiFiClient vmix;

ATEMstd AtemSwitcher; 
IPAddress tempIP;

String     type_Pre = "PreviewSceneChanged";
String     type_Pro = "SwitchScenes";

int On = LOW;
int Off = HIGH;
 
void setup() {
  pinMode(D1, OUTPUT);    // Preview Front
  digitalWrite(D1, Off);
  pinMode(D2, OUTPUT);    // Program Front
  digitalWrite(D2, Off);
  pinMode(D5, OUTPUT);    // Preview Back
  digitalWrite(D5, Off);
  pinMode(D4, OUTPUT);    // Program Back
  digitalWrite(D4, Off);
  pinMode(0, INPUT_PULLUP);    // Flash button on ESP8266
  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(1000);
  
  Serial.println("START");
  testLEDS();
  getMemory();
  configureWebServer();
  if (!checkMemory()) {
    Serial.println("NOT CONFIGURED");
    while(CONFIGURED != "1"){ 
      server.handleClient();       //Checks for web server activity
      delay(500);
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

  digitalWrite(D5, Off);
  delay(250);
  digitalWrite(D5, On);
  delay(250);
  digitalWrite(D5, Off);
  delay(250);
  
  digitalWrite(D4, Off);
  delay(250);
  digitalWrite(D4, On);
  delay(250);
  digitalWrite(D4, Off);
  delay(250);
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
    Switch_Type = read_from_Memory(offset_SwitchType).toInt();    
    Switch_IP = read_from_Memory(offset_SwitchIP);
    Switch_Port = read_from_Memory(offset_SwitchPort).toInt();
    Switch_Pswd = read_from_Memory(offset_SwitchPswd);
    Camera_ID = read_from_Memory(offset_Camera_ID);
    if (Camera_ID.length() == 1  && isDigit(Camera_ID.charAt(0))){ 
      camera_num = Camera_ID.toInt();
    }else{
      camera_num = 0;
    }
    LED_pos = read_from_Memory(offset_LED_pos).toInt();
    Serial.println("getMemory");
    Serial.println(wifi_ssid);
    Serial.println(wifi_pswd);
    Serial.println(Switch_Type);
    Serial.println(Switch_IP);
    Serial.println(Switch_Port);
    Serial.println(Switch_Pswd);
    Serial.println(Camera_ID);
    Serial.println(camera_num);
    Serial.println(LED_pos);
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
  Serial.println("WebSocket client started.");
  webSocket.onEvent(webSocketEvent);               // respond to message
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) { // When a WebSocket message is received
     Serial.println("WebSocket message.");
     Serial.print("WStype = ");   Serial.println(type);  
//     Serial.print("WS payload = ");
// since payload is a pointer we need to type cast to char
//     for(int i = 0; i < length; i++) { Serial.print((char) payload[i]); }
//     Serial.println();
     
     switch (type) {
            case WStype_DISCONNECTED:             // if the websocket is disconnected
                 Serial.println("[WS] Disconnected!");
                 break;
            case WStype_CONNECTED:               // if a new websocket connection is established
                 Serial.println("[WS] Connected!");
                 webSocket.sendTXT("Connected");     // send message to server when Connected
                 digitalWrite(D2, On);
                 delay(2000);
                 digitalWrite(D2, Off);
                 break;
            case WStype_TEXT:                     // if new text data is received
//                 Serial.print("[WS] get Text");
                 Serial.print("WStype = ");   Serial.println(type);  
                 Serial.print("length = ");   Serial.println(length); 
//                 Serial.println("WS payload = ");
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
      if (Utype == type_Pre) {
              if (scene_name != Camera_ID) { 
                  setLEDs(0, LED_pos); 
                  return;
              }
              setLEDs(1, LED_pos); 
              return; 
      }
      if (Utype == type_Pro) {
              if (scene_name != Camera_ID) { 
//                 setLEDs(0, LED_pos); 
                 return;
              }
              setLEDs(2, LED_pos); ;
              return;                    
      }
      return;
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
  // status: 0 = off, 1 = preview, 2 = program
  // which: 0 = both, 1 = front, 2 = back
  
     switch(status) {
        case 0:
          digitalWrite(D1, Off);
          digitalWrite(D2, Off);
          digitalWrite(D5, Off);
          digitalWrite(D4, Off);
          break;
        case 1:
          if(which == both || which == front) { 
            digitalWrite(D1, Off);
            digitalWrite(D2, On);
          }
          if(which == both || which == back) {        
            digitalWrite(D5, Off);
            digitalWrite(D4, On);
          }
          Serial.println("PREVIEW");
          break;     
       case 2: 
        if(which == both || which == front) {
           digitalWrite(D1, On);
           digitalWrite(D2, Off);
        }
        if(which == both || which == back) {
           digitalWrite(D5, On);
           digitalWrite(D4, Off);
        }
        Serial.println("PROGRAM");
        break;
     }
}


void handleRoot() {
     SETUP_HTML = HTMLheader + createSETUPhtml();
     Serial.println("SETUP html created");
     server.send(200, "text/html", SETUP_HTML);
}    

String createSETUPhtml(){
  String HTMLsetup = "<h1 align=\"center\">Tally Setup</h1><br>";
  HTMLsetup += "<h2 align=\"center\">Critical Info</h2><br>";
  HTMLsetup += "<FORM action=\"/action_page\">";
  HTMLsetup += "Local WiFi: Network SSID:<input type=\"text\" name=\"network1\" value = \"\" required>";
  HTMLsetup += "Password:<input type=\"text\" name=\"netpass1\" value = \"\" required><br><br>";
  
  HTMLsetup += "<div>Switcher: ";
  HTMLsetup += "<input type=\"radio\" id=\"obs\" name=\"swType\" value=\"0\" onclick=\"portOBS()\" checked><label for=\"obs\">OBS</label>";
  HTMLsetup += "<input type=\"radio\" id=\"atem\" name=\"swType\" value=\"1\" onclick=\"portATEM()\"><label for=\"atem\">ATEM</label>";
  HTMLsetup += "<input type=\"radio\" id=\"vmix\" name=\"swType\" value=\"2\" onclick=\"portVMIX()\"><label for=\"vmix\">Vmix</label></div><br>";
 
  HTMLsetup += "Switch IP Address:<input type=\"text\" name=\"ip\" value = \"\" required pattern=\"^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$\" placeholder=\"xxx.xxx.xxx.xxx\">";
  HTMLsetup += "Switch Port:<input type=\"text\" name=\"port\" value = \"4444\" id=\"portField\" required pattern=\"[0-9]{1,4}\"><br><br>";
  HTMLsetup += "Switch Password (if used):<input type=\"text\" name=\"pswd\" value = \"\"><br>";
  HTMLsetup += "Scene Name (OBS) or Camera Number (ATEM, Vmix):<input type=\"text\" name=\"scenename\" value = \"\" required><br><br>";
 
  HTMLsetup += "<div>LED configuration: ";
  HTMLsetup += "<input type=\"radio\" id=\"both\" name=\"ledpos\" value=\"0\" checked><label for=\"both\">Both</label>";
  HTMLsetup += "<input type=\"radio\" id=\"front\" name=\"ledpos\" value=\"1\"><label for=\"front\">Front Only</label>";
  HTMLsetup += "<input type=\"radio\" id=\"back\" name=\"ledpos\" value=\"2\"><label for=\"back\">Back Only</label></div><br>";
  HTMLsetup += "<p align=\"center\"><INPUT type=\"submit\" value=\"Submit\"><INPUT type=\"reset\"></p></FORM>";
  
  HTMLsetup += "<script>function portOBS(){document.getElementById(\"portField\").value = \"4444\";}</script>";
  HTMLsetup += "<script>function portATEM(){document.getElementById(\"portField\").value = \"9990\";}</script>";
  HTMLsetup += "<script>function portVMIX(){document.getElementById(\"portField\").value = \"8099\";}</script>";
  
  HTMLsetup += "</body></html>";
  return HTMLsetup;
}

void handleForm() {
     Serial.println("handleForm");
     String html_ssid = server.arg("network1"); 
     String html_pswd = server.arg("netpass1");
     String html_Switch_Type = server.arg("swType"); 
     String html_Switch_IP = server.arg("ip");
     String html_Switch_Port = server.arg("port");
     String html_Switch_Pswd = server.arg("pswd");
     String html_camera = server.arg("scenename");
     String html_LED_pos = server.arg("ledpos");
     Serial.println(html_ssid);
     Serial.println(html_pswd);
     Serial.println(html_Switch_Type);
     Serial.println(html_Switch_IP);
     Serial.println(html_Switch_Port);     
     Serial.println(html_Switch_Pswd);
     Serial.println(html_camera);
     Serial.println(html_LED_pos);

     Serial.println("handleForm memory writes");
     write_to_Memory(html_ssid,offset_ssid);
     write_to_Memory(html_pswd,offset_pswd);
     
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
     
     EEPROM.begin(512);
     EEPROM.write(offset_cflag, 'Y');
     EEPROM.commit();
     CONFIGURED = "1";
     sendAckPage();    
}

String createACKhtml(){
  wifi_ssid = read_from_Memory(offset_ssid);
  wifi_pswd = read_from_Memory(offset_pswd);
  Switch_Type = read_from_Memory(offset_SwitchType).toInt();
  Switch_IP = read_from_Memory(offset_SwitchIP);
  Switch_Port = read_from_Memory(offset_SwitchPort).toInt();  
  Switch_Pswd = read_from_Memory(offset_SwitchPswd);
  Camera_ID = read_from_Memory(offset_Camera_ID);
  LED_pos = read_from_Memory(offset_LED_pos).toInt();
    
  String HTMLsetup = "<center><h1>Tally Setup</h1><p>";
  HTMLsetup += "<h2>Acknowledgement</h2><br>";
  HTMLsetup += "<b>Local WiFi Network SSID: </b>" + wifi_ssid + "<br>";
  HTMLsetup += "<b>Local WiFi Password: </b>" + wifi_pswd + "<br><br>";
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
  HTMLsetup += "<b>Switch IP Adress: </b>" + Switch_IP + "<br>";
  HTMLsetup += "<b>Switch Port: </b>" + String(Switch_Port) + "<br>";
  if(Switch_Pswd.length() == 0){
     HTMLsetup += "<b>Switch Password: </b>N/A<br>";    
  }else{
     HTMLsetup += "<b>Switch Password: </b>" + Switch_Pswd + "<br>";
  }
  HTMLsetup += "<b>Scene Name or Camera Number: </b>" + Camera_ID + "<br>";
  switch(LED_pos){
    case both:
      HTMLsetup += "Both Front and Back LEDS will light<br>";
      break;
    case front:
      HTMLsetup += "Only Front LED will light<br>";
      break;
    case back:
      HTMLsetup += "Only Back LED will light<br><br>";           
  }  
  HTMLsetup += "Your TallyLight is configured!</h2><br><br>";
  HTMLsetup += "The light will blink red while connecting to WiFi<br>";
  HTMLsetup += "This may take up to 30 seconds.<br>";  
  HTMLsetup += "the light will turn green for 5 seconds<br>";
  HTMLsetup += "after connecting to the switcher<br>";  
  HTMLsetup += "<h2 align=\"center\">To reconfigure the TallyLight,</h2><p>";
  HTMLsetup += "<h2 align=\"center\">press and hold the RESET button for 1 second.</h2><br>";
  return HTMLsetup;
}

void sendAckPage() {
     SETUP_HTML = HTMLheader + createACKhtml();
     Serial.println("ACK html created");
     server.send(200, "text/html", SETUP_HTML);
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
