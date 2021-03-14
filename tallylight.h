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

int offset_ssid = 0;
int offset_pswd =32;
int offset_obs_pswd = 64;
int offset_camera_name = 96;
int offset_obsIP = 128;
int offset_atem_pswd = 160;
int offset_atemIP = 192;
int offset_cflag = 224;

String   wifi_ssid;
String   wifi_pswd;
String	 obs_IP;
String   obs_pswd;
String   atem_IP;
String   atem_pswd;
String   camera_name;
uint16_t camera_num;

String   memSSID;
String   memPassword;
String   memOBS;
String   memCamera;
String   memOBSIP;
String   memATEM;
String   memATEMIP;
uint16_t memATEMcam;
int      memCflag;
String   CONFIGURED;

#endif // _TALLYLIGHT_H
