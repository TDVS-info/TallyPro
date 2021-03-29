/*
  tallylight.h - static strings.
  Created by Ted Dillenkofer, January 22, 2021.
*/

#ifndef _TALLYLIGHT_H    
#define _TALLYLIGHT_H 

const String HTMLheader = "<!DOCTYPE HTML><html><head><meta content=\"text/html; charset=ISO-8859-1\"http-equiv=\"content-type\"><title>Lap Counter</title><style>\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\"</style></head><body>";

const char *ESPssid = "TallyLight";
const char *ESPpassword = "TallyLight";
IPAddress ap_local_IP(192,168,4,1);
IPAddress ap_gateway(192,168,4,254);
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

String   wifi_ssid;
String   wifi_pswd;
uint16_t Switch_Type;
String	 Switch_IP;
String   Switch_Port;
String   Switch_Pswd;
String   Camera_ID;
uint16_t camera_num;
uint16_t LED_pos;


String   memSSID;
String   memPassword;
String   memCamera;
String   memIP;
String   memPort;
uint16_t memIPpswd;
uint16_t memSwitchType;
uint16_t memLEDpos;
int      memCflag;
String   CONFIGURED;

#endif // _TALLYLIGHT_H
