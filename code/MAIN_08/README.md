Takes into account "Switch Preview/Program Scenes After Transitioning" switch in OBS

Removed unused "mem" vaariables from tallylight.h

Added new variables to tallylight:

  int offset_sip = 197;
  int offset_gateway = 213;
  int offset_subnet = 229;
  
  String wifi_sip;
  String wifi_gateway;
  String wifi_subnet;
  
  Allows assignment of a static IP address to tally light
  
  You must know what IP you want, the network gateway and the subnet mask.
  
  for a typical small network this might be:
  
  IP address 192.168.1.110
  gateway: 192.168.1.1
  Subnet mask:255.255.255.0
  
  You should be careful to pick an address outside the pool of dynamically assigned addresses in your router.
  There is no error checking to make sure you have chosen an unused address in this release.
  
  If you do not add a static IP, the router will assign a free IP address to your light that does not conflict.
