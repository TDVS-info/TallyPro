/*
  tallylight.h - static strings and globals.
  Created by Ted Dillenkofer, January 22, 2021.

*/

#ifndef _TALLYLIGHT_H    
#define _TALLYLIGHT_H 

const String Version = "086";

const String HTMLheader = "<!DOCTYPE HTML><html><head><meta content=\"text/html; charset=ISO-8859-1\"http-equiv=\"content-type\"><title>TPIII Seteup</title>\
<style>\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\
.slider { -webkit-appearance: none; margin: 14px; width: 600px; height: 25px; background: #FFD65C; outline: none; -webkit-transition: .2s; transition: opacity .2s;}\
.slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #003249; cursor: pointer;}\
.slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer;}\
</style></head><body>";

const char* ESPssid = "TPIII_086";
const char* ESPpassword = "TallyLight";
const char* mdnsName = "TPIII_086";
const char* host = "esp8266-webupdate";

IPAddress ap_local_IP(1,1,4,1);
IPAddress ap_gateway(1,1,4,1);
IPAddress ap_subnet(255,255,255,0);

#define OBS   0
#define ATEM  1
#define VMIX  2

#define both  0
#define front 1
#define back  2

int offset_cflag = 0;
int offset_ssid = 1;
int offset_pswd = 33;
int offset_SwitchType = 65;
int offset_SwitchIP = 67;
int offset_SwitchPort = 99;
int offset_SwitchPswd = 131;
int offset_Camera_ID = 163;
int offset_LED_pos = 195;
int offset_sip = 227;
int offset_gateway = 259;
int offset_subnet = 291;
int offset_LED_Levels = 323;

String   wifi_ssid;
String   wifi_pswd;
String   wifi_sip;
String   wifi_gateway;
String   wifi_subnet;
uint16_t Switch_Type;
String	 Switch_IP;
String   Switch_Port;
String   Switch_Pswd;
String   Camera_ID;
uint16_t camera_num;
uint16_t LED_pos;
uint16_t FrontRedLevel;
uint16_t FrontGreenLevel;
uint16_t BackRedLevel;
uint16_t BackGreenLevel;

String   CONFIGURED;

#endif // _TALLYLIGHT_H
