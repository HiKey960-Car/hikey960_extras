Programming the sensor board.

The sensor board is connected to the hikey960 via /dev/ttyAMA3 and can be programmed from the hikey960. No need for additional hardware or configurations.

Create a directory /data/bin and copy the two files "avrdude" and "avrdude.conf" to that location.
Make sure that avrdude is set 755

To write a compiled hex to the sensor board, run the following;
/data/bin/avrdude -C /data/bin/avrdude.conf -v -patmega328p -carduino -P/dev/ttyAMA3 -D -Uflash:w:./SWI.ino.standard.hex:i


I suggest starting off with the standard "blink" example program, which will cause the RED LED between the P6 and EF plugs to start blinking with a 1-second-on, 1-second-off pattern.

If you install ArduinoDroid (https://play.google.com/store/apps/details?id=name.antonsmirnov.android.arduinodroid2) then you can build a sketch right on the car radio, but note that the editor can be a challenge to use depending on your display configuration (I find that the keyboard obscures the editor window), but you can certainly use it to compile examples or "sketches" that you copy onto the sdcard.

In ArduinoDroid, you can open sketches with MENU --> Sketch (use Open to find sketches you copied into /sdcard/, or you can find examples in... Examples). The "Lightning Bolt" icon will compile the sketch, which will compile them to a wonderfully obscure location... /data/data/name.antonsmirnov.android.arduinodroid2/build/

On the desktop, you can use the "official" Arduino IDE (https://www.arduino.cc/en/Main/Software). First make sure that you have it set up to use the proper device, so Tools --> Board --> "Arduino/Genuino Uno" (this is necessary, because there are chips of different architecture, like ARM Mx). When the "sketch" (which is their term for source file) is ready to try, use Sketch --> "Export Compiled Binary", which will create two files; {projectname}.ino.standard.hex, and {projectname}.ino.with_bootloader.standard.hex. You need the FIRST one {projectname}.ino.standard.hex. adb push that file to your hikey960, and run the avrdude command as above to install it to the sensors mezzanine.

