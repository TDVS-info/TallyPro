3/29/2021 Changes in MAIN_07

1. Added back LED
   Radio Butttons used to choose between both, only front or only back LEDS lighting up.
   
2. Modified setup page.  
   It is now much simpler, reformatted to be easier ot understand and use on a phone.
   Changes are based on the premise that it is unlikely you will be rnning more than one switch. Radio buttons used to select OBS, ATEM or Vmix switches. 
   Validation and pattern matching is used throughout.
   Automatic update of the port field to the correct port for each switch.  Can still be overridden if you have changed the port in the switch.
   
3. Reformatted the Acknowlegement page. 

4. Database is altered to accomodate new persistent data for switch type.

5. reordered functions in code to make more sense

6. streamlined existing code.

7. New setLEDs function. Consolidates LED light functions, removing them from switch handing routines.

While this code should not break Tally Pro II, it is designed for Tally Pro III which is currently being designed.  It will feature a smaller case with a built-in battery compartment. It will have a rounded rectangular shape and a printed circuit board. There will only be two wires for the back LED so that we can maintain access to the USB port for powering purposes, since Main_06 added OTA updates.  
