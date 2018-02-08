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

#include <EEPROM.h>

struct analogInput {
  int entries; // also serves as "enabled".
  int down; // index of keycode and value of key that is down.
  byte keycode[32];
  int value[32];
};

struct analogInput adc[6]; // there are actually 7 adc's, but A6 (the 7th) reads low.
int adcabs = 10; // reasonable?

byte d[13];
byte d_down[13];

int loopcounter = 0;

void load(){
  for (int i=0; i<13; i++){
    d[i] = 0;
    d_down[i] = 0;
  }

  // initialize adc structures
  for (int i=0; i<6; i++){
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
  Serial.begin(9600);
  Serial.print("DEBUG(Begin serial output)\n");

  // Set all pins to input, pull up.
  for (int i = 2; i < 13; i++){
    pinMode(i, INPUT_PULLUP);
  }
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  pinMode(A4, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);

  pinMode(LED_BUILTIN, OUTPUT);

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
  Serial.println(")\n");
}

void print_hex(uint8_t *data, uint8_t length){
  for (int i=0; i<length; i++){
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i],HEX);
  }
}

void loop() {
  if (Serial.available() > 0){
    Serial.print("DEBUG(data is available)\n");
    String input = Serial.readStringUntil('\n');
    if (input.charAt(0) == 'P'){
      byte inputstate[14];
      byte a;
      int adcval;
      int plooper = 0;

      // Enter PROGRAMMING mode
      //
      // The way the programming works is like this;
      // 1) Send 'P\n" to enter programming mode.
      // 2) Press and hold any key, while holding, send '{keycode}\n' where {keycode} is a byte
      //    -- repeat step 2 for all keys.
      // 3) Send 'E\n' to exit programming mode.
      //
      // ** There is no abort/save option.
      //
 
      int addy = 0;

      Serial.print("DEBUG(enter PROGRAMMING mode)\n");

      // clear eeprom
      for (int i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0);
      }

      Serial.print("DEBUG(cleared EEPROM)\n");

      while ( 1 ){

        // read first byte worth of digital inputs
        // ** NOTE: Don't use digital pins 0 or 1, because those are used by the serial port.
        inputstate[0] = 0;
        for (int i=2; i<8; i++){
          a = !digitalRead(i);
          inputstate[0] |= (a << i);
        }

        // read second byte worth of digital inputs
        inputstate[1] = 0;
        for (int i=0; i<5; i++){
          a = !digitalRead(i+8);
          inputstate[1] |= (a << i);
        }

        // read analog inputs
        for (int i=0; i<6; i++){
          adcval = analogRead(i);
          inputstate[2+(2*i)] = (byte)(adcval/256);
          inputstate[3+(2*i)] = (byte)(adcval%256);
        }

        // write input state to serial... 0x0000 03FF 03FF 03FF 03FF 03FF 03FF -- all switches OFF.
        Serial.print("PINPUT(0x");
        print_hex(inputstate, 14);
        Serial.print(")\n");

        if (Serial.available() > 0){
          String pinput = Serial.readStringUntil('\n');
          if (pinput.charAt(0) == 'E') break;

          // If we get to here, then the first byte on the serial must correspond to a keycode.
          // scan all the inputs and write keycode to eeprom.

          // Digital;
          for (int i=2; i<8; i++){
            boolean val = (inputstate[0] & (1 << i)) > 0;
            if (val){
              EEPROM.write(addy, 'D');
              EEPROM.write(addy+1, (byte)i);
              EEPROM.write(addy+2, pinput.charAt(0));
              addy+=3;
            }
          }
          for (int i=0; i<5; i++){
            boolean val = (inputstate[1] & (1 << i)) > 0;
            if (val){
              EEPROM.write(addy, 'D');
              EEPROM.write(addy+1, (byte)(i+8));
              EEPROM.write(addy+2, pinput.charAt(0));
              addy+=3;
            }
          }

          // Analog;
          for (int i=0; i<6; i++){
            int val = 256*inputstate[2+(2*i)] + inputstate[3+(2*i)];
            if (val < 1000){
              EEPROM.write(addy, 'A');
              EEPROM.write(addy+1, (byte)i);
              EEPROM.write(addy+2, pinput.charAt(0));
              EEPROM.write(addy+3, inputstate[2+(2*i)]);
              EEPROM.write(addy+4, inputstate[3+(2*i)]);
              addy+=5;
            }
          }
        }
        delay(500); // lets do this at half second intervals.
      }
      load();
      plooper++;
      if (plooper == 1) digitalWrite(LED_BUILTIN, HIGH);
      if (plooper >= 2){
        digitalWrite(LED_BUILTIN, LOW);
        plooper = 0;
      }
    }
  }

  // start with DIGITAL
  for (int i=2; i<13; i++){
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
  loopcounter++;
  if (loopcounter == 190) digitalWrite(LED_BUILTIN, HIGH);
  if (loopcounter >= 200){
    digitalWrite(LED_BUILTIN, LOW);
    loopcounter = 0;
    Serial.print("DEBUG(writing end of loop debug)\n");
  }
}
