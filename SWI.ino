/*
 * eeprom memory layout;   
 * 1B [A,D] | 1B pin# | 1B keycode | 2B ADC (analog only)
 * 
 * All pins, including ANALOG, are set to activate internal pull-up resistors.
 * In the case of the analog pins, this is to ensure readings above 1000 when
 * nothing is connected. Value of internal pull-up will be somewhere between
 * 30-35 kohm. Since SWI requires external pull up of around 1.6 kohm, this is
 * ok and won't do anything besides a minor skew of the values, which are
 * learned anyway.
 * 
 * 1/(1/30 + 1/1.6) = 1.52 kohm.
 */

//TODO PRINT LOTS OF DEBUG CODE TO SERIAL!!!

#include <EEPROM.h>

struct analogInput {
  int entries; // also serves as "enabled".
  int down; // index of keycode and value of key that is down.
  byte keycode[32];
  int value[32];
};

struct analogInput adc[7];
int adcabs = 10; // reasonable?

byte d[13];
byte d_down[13];

void load(){
  for (int i=0; i<13; i++){
    d[i] = 0;
    d_down[i] = 0;
  }

  // initialize adc structures
  for (int i=0; i<7; i++){
    adc[i].entries = 0;
    adc[i].down = -1; // because this is an index, values must start at 0. Negative means UP.
    for (int j=0; j<32; j++){
      adc[i].keycode[j] = 0;
      adc[i].value[j] = 0;
    }
  }

  // load key configuration from eeprom
  for (int i = 0; i < EEPROM.length();){
    byte type = EEPROM.read(i);
    byte pin;
    byte keycode;
    int adcval;
    if (type == 'A' && adc[pin].entries < 32){
      pin = EEPROM.read(i+1);
      keycode = EEPROM.read(i+2);

      adc[pin].keycode[adc[pin].entries] = EEPROM.read(i+2);
      adc[pin].value[adc[pin].entries] = EEPROM.read(i+3)<<8 + EEPROM.read(i+4);

      adc[pin].entries++;

      i+=5;
    } else if (type == 'D'){
      pin = EEPROM.read(i+1);
      d[pin] = EEPROM.read(i+2);
      i+=3;
    } else break;    
  }
}

void setup() {
  while (!Serial);
  delay(500);
  Serial.begin(115200);

  // Set all pins to input, pull up.
  for (int i = 0; i < 13; i++){
    pinMode(i, INPUT_PULLUP);
  }
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  pinMode(A4, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);
  pinMode(A6, INPUT_PULLUP);

  load();
}

int lookup(int input, int value){
  for (int i=0; i<adc[input].entries; i++){
    if (abs(value - adc[input].value[i]) < adcabs) return i;
  }
  return -1;
}

void send_serial(boolean down, byte keycode){
  if (down) Serial.print("KEYDOWN(");
  else Serial.print("KEYUP(");
  Serial.print(keycode, HEX);
  Serial.println(")");
}

void loop() {
  if (Serial.available() > 0){
    String input = Serial.readStringUntil('\n');
    if (input.charAt(0) == 'P'){

      /* Enter PROGRAMMING mode
       *
       * The way the programming works is like this;
       * 1) Send 'P\n" to enter programming mode.
       * 2) Press and hold any key, while holding, send '{keycode}\n' where {keycode} is a byte
       *    -- repeat step 2 for all keys.
       * 3) Send 'E\n' to exit programming mode.
       * 
       * ** There is no abort/save option.
       */
 
      int addy = 0;

      // clear eeprom
      for (int i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0);
      }
    
      while ( 1 ){
        if (Serial.available() > 0){
          String pinput = Serial.readStringUntil('\n');
          if (pinput.charAt(0) == 'E') break;

          // If we get to here, then the first byte on the serial must correspond to a keycode.
          // scan all the inputs and write keycode to eeprom.

          // Digital;
          for (int i=0; i<13; i++){
            byte val = digitalRead(i);
            if (val == 0){
              EEPROM.write(addy, 'D');
              EEPROM.write(addy+1, (byte)i);
              EEPROM.write(addy+2, pinput.charAt(0));
              addy+=3;
            }
          }

          // Analog;
          for (int i=0; i<6; i++){
            int val = analogRead(i);
            if (val < 1000){
              EEPROM.write(addy, 'A');
              EEPROM.write(addy+1, (byte)i);
              EEPROM.write(addy+2, pinput.charAt(0));
              EEPROM.write(addy+3, (byte)(val/256));
              EEPROM.write(addy+4, (byte)(val%256));
              addy+=5;
            }
          }
        }
        delay(10);
      }
      load();
    }
  }

  // start with DIGITAL
  for (int i=0; i<13; i++){
    if (d[i] > 0){
      int val = digitalRead(i); // 1 == off, 0 == on
      if (d_down[i] == 0 && val == 0){
        send_serial(1, d[i]);
        d_down[i] = 1;
      } else if (d_down[i] == 1 && val == 1){
        send_serial(0, d[i]);
        d_down[i] = 0;
      }
    }
  }

  // now ANALOG
  for (int i=0; i<6; i++){
    if (adc[i].entries > 0){
      int val = analogRead(i);
      if (adc[i].down > -1 && (val > 1000 || abs(adc[i].value[adc[i].down] - val) < adcabs)){ // if button was down and turns off or changes
        send_serial(0, adc[i].keycode[adc[i].down]);
        adc[i].down = -1;
      }
      if (val < 1000 && adc[i].down < 0){
        adc[i].down = lookup(i, val);
        send_serial(1, adc[i].keycode[adc[i].down]);
      }
    }
  }

  delay(10);

}
