The files here are updates to the Tally Pro light system produced by TD Video Services LLC.
They are binary files designed to upload directly to a NodeMCU 12E ESP8266 module.
The base code contains routines to allow OTA updates using these files.
Updates are backwards compatible, so you should feel confident to run the latest version on your devices. 

Main_04
  OBS only
  
Main_05
  OBS and ATEM
  Changes to configuration data, requires reconfiguration.  
  OTA update webpage
  
Main_06 
  OBS, ATEM and Vmix
  OTA update webpage
  Changes to configuration data, requires reconfiguration
  
Main_07
  Rear LED support - Tally Pro III, no rear LED in Tally Pro II or I
  Changes to setup and acknowledgement pages
  Changes to configuration data, requires reconfiguration
  
Main_08
  If light is configured, the configuration webpage shows existing config data
  
Main_081
  fixed issue with ackknowledgement screen - was not showing correct gateway and mask IPs
  
Main_082
  Added dropdown list in config page for available WiFi network SSIDs
  Cleared data entry fields before displaying config page
  Fixed Switch Port so that when unconfigured = "4444"
  
