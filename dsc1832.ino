#include <SPI.h>
#include <avr/pgmspace.h>
#include "CRC32.h"
#include <QueueArray.h>

#define CLK 3 // Keybus Yellow
#define DTA 4 // Keybus Green
#define DTA_OUT 8 // Keybus Green Output
#define BUTTON A0 // Analog interface for keypress

#define DEVICEID "0952"
#define MAX_BITS 200
#define KEYPAD_IDLE "111111111111111111111111"

int cmdp=0, cmdk=0;
String oldp="", oldk="", wordp="", wordk="", msgp="", msgk="";
char hex[] = "0123456789abcdef";
String st, st2, st2_a;
unsigned long lastData;
char buf[100];
long pulseTime=micros(),lastTime=micros();
long intervalTimer = 0;
long counter = 0;
int startWrite=0;
int buttonCheck=0;
const int buttonPin = 12;     // the number of the pushbutton pin
const int ledPin =  13;      // the number of the LED pin
// Variables will change:
int buttonPushCounter = 0;   // counter for the number of button presses
int buttonState = 0;         // current state of the button
int lastButtonState = 0;     // previous state of the button


PROGMEM const uint32_t crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

void setup()
{
  pinMode(CLK,INPUT);
  pinMode(DTA,INPUT);
  pinMode(DTA_OUT, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP); // sets analog pin for input
    // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  Serial.begin(115200);
  Serial.println("Booting");
 
  // Interrupt 1 = PIN 3 external
  attachInterrupt(digitalPinToInterrupt(CLK),clkCalled,CHANGE);
 
  Serial.println("Ready!");
}
void loop()
{
  // read the state of the pushbutton value:
  buttonState = digitalRead(buttonPin);

  if (buttonState != lastButtonState) {
    // if the state has changed, increment the counter
    if (buttonState == HIGH) {
      // if the current state is HIGH then the button
      // wend from off to on:
      buttonPushCounter++;
      Serial.println("on");
      Serial.print("number of button pushes:  ");
      Serial.println(buttonPushCounter);
    } else {
      // if the current state is LOW then the button
      // wend from on to off:
      Serial.println("off");
    }
    // Delay a little bit to avoid bouncing
    delay(50);
  }
  // save the current state as the last state,
  //for next time through the loop
  lastButtonState = buttonState;
  

//  Serial.println(intervalTimer);

  if ((intervalTimer < 5400) || (st.length() < 50)) return; 
  //if (waitCLKchange(1) < 2000) return;  // First falling clock after sync is being added to it's own string messing up the client.  Need to fix to write

  wordp = st;
  st = ""; 
  wordk = st2_a;
  //st2 = "1"; // need this because missing first clock


  //Serial.println(wordp);
  //Serial.println(wordk);
  cmdp = decode(wordp, msgp);
  cmdk = decode(wordk, msgk);
  if (cmdp != 0) {
    if (msgp != oldp) {
      oldp = msgp;
      serialPrint(wordp, msgp, cmdp, 1);
    }
  }   
  if (cmdk != 0) {
    if (msgk != "From Keypad idle ") {
      serialPrint(wordk, msgk, cmdk, 0);
    }
  }
}

void clkCalled()
{
  lastTime = pulseTime;
  pulseTime = micros();
  intervalTimer = pulseTime-lastTime;
  if (intervalTimer > 5000) { 
    counter++; 
    startWrite=0;
    st2_a = st2;
    st2="";
  }

  if (counter > 1) {
    if (digitalRead(CLK)) {
      if (st.length() > MAX_BITS) return; // Do not overflow the arduino's little ram
      if (digitalRead(DTA)) st += "1"; else st += "0";
    } else {
      startWrite++;
      if ((startWrite > 0) && (startWrite < 24)) { 
/*       if (startWrite%2)
         digitalWrite(DTA_OUT, HIGH);
       else
         digitalWrite(DTA_OUT, LOW);*/
      }

      if (st2.length() > MAX_BITS) return;
      if (digitalRead(DTA)) st2 += "1"; else st2 += "0";

    }
  }
}

unsigned long waitCLKchange(int currentState)
{
  unsigned long c = 0; 

  while (digitalRead(CLK) == currentState)
  {
    delayMicroseconds(10);
    c += 10;
    if (c > 10000) break;
  }
  return c;
}

String formatDisplay(String &st)
{
  String res;
  res = st.substring(0,8) + " " + st.substring(8,9) + " ";
  int grps = (st.length() - 9) / 8;
  for(int i=0;i<grps;i++)
  {
    res += st.substring(9+(i*8),9+((i+1)*8)) + " ";
  }
  res += st.substring((grps*8)+9,st.length());
  return res;
}

String formatDisplayKeypad(String &st)
{
  String res;
  res = st.substring(0,8) + " ";
  int grps = ((st.length()) / 8)-1;
  for(int i=0;i<grps;i++)
  {
    res += st.substring(8+(i*8),9+((i+1)*8)) + " ";
  }
  res += st.substring((grps*8)+9,st.length());
  return res;
}

unsigned int getBinaryData(String &st, int offset, int length)
{
  int buf = 0;
  for(int j=0;j<length;j++)
  {
    buf <<= 1;
    if (st[offset+j] == '1') buf |= 1;
  }
  return buf;
}

String formatSt(String &st)
{
  String res = DEVICEID + String(";");
  res += String(hex[getBinaryData(st,0,4)]) + String(hex[getBinaryData(st,4,4)]) + String(";");
  int grps = (st.length() - 9) / 4;
  for(int i=0;i<grps;i++)
  {
    res += String(hex[getBinaryData(st,9+(i*4),4)]);
  }
  char buf[100];
  res.toCharArray(buf,100);
  unsigned long crc = crc_string(buf);
  res += String(";") + String(crc,HEX);
  return res;
}

unsigned long crc_update(unsigned long crc, byte data)
{
    byte tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    return crc;
}

unsigned long crc_string(char *s)
{
  unsigned long crc = ~0L;
  while (*s)
    crc = crc_update(crc, *s++);
  crc = ~crc;
  return crc;
}

String hex8(int num) {
  char res[3];
  sprintf(&res[0], "0x%02X", num);
  return (res);
}

static int decode(String word, String &msg) {
  int cmd = 0, zones = 0, button = 0;
  char year3[2],year4[2],month[2],day[2],hour[2],minute[2];
  msg = "";
  cmd = getBinaryData(word,0,8);

  // decoding the messages
  if (cmd == 0x05) {
    if (getBinaryData(word,12,1)) msg += "Error ";
    if (getBinaryData(word,13,1)) msg += "Bypass ";
    if (getBinaryData(word,14,1)) msg += "Memory ";
    if (getBinaryData(word,15,1)) msg += "Armed ";
    if (getBinaryData(word,16,1)) msg += "Ready ";
  } else if (cmd == 0xa5) {
    int year3 = getBinaryData(word,9,4);
    int year4 = getBinaryData(word,13,4);
    int month = getBinaryData(word,19,4);
    int day = getBinaryData(word,23,5);
    int hour = getBinaryData(word,28,5);
    int minute = getBinaryData(word,33,6);
    msg += "Date: 20" + String(year3) + String(year4) + "-" + String(month) + "-" + String(day) + " " + String(hour) + ":" + String(minute);
    int arm = getBinaryData(word,41,2);
    int master = getBinaryData(word,43,1);
    int user = getBinaryData(word,43,6); // 0-36
    if (arm == 0x02) {
      msg += " armed";
      user = user - 0x19;
    }
    if (arm == 0x03) {
      msg += " disarmed";
    }
    if (arm > 0) {
      if (master) msg += " master code"; else msg += " user";
      user += 1; // shift to 1-32, 33, 34
      if (user > 34) user += 5; // convert to system code 40, 41, 42
      msg += " " + String(user);
    }
  } else if (cmd == 0x27) {
    msg += "Zones1: ";
    int zones = getBinaryData(word,8+1+8+8+8+8,8);
    if (zones & 1) msg += "1 ";
    if (zones & 2) msg += "2 ";
    if (zones & 4) msg += "3 ";
    if (zones & 8) msg += "4 ";
    if (zones & 16) msg += "5 ";
    if (zones & 32) msg += "6 ";
    if (zones & 64) msg += "7 ";
    if (zones & 128) msg += "8 ";
    if (zones == 0) msg += "Ready ";
  } else if (cmd == 0x2d) {
    msg += "Zone2: ";
    int zones = getBinaryData(word,41,8);
    if (zones & 1) msg += "1 ";
    if (zones & 2) msg += "2 ";
    if (zones & 4) msg += "3 ";
    if (zones & 8) msg += "4 ";
    if (zones & 16) msg += "5 ";
    if (zones & 32) msg += "6 ";
    if (zones & 64) msg += "7 ";
    if (zones & 128) msg += "8 ";
    if (zones == 0) msg += "Ready ";
  } else if (cmd == 0x34) {
    msg += "Zone3: ";
    int zones = getBinaryData(word,41,8);
    if (zones & 1) msg += "1 ";
    if (zones & 2) msg += "2 ";
    if (zones & 4) msg += "3 ";
    if (zones & 8) msg += "4 ";
    if (zones & 16) msg += "5 ";
    if (zones & 32) msg += "6 ";
    if (zones & 64) msg += "7 ";
    if (zones & 128) msg += "8 ";
    if (zones == 0) msg += "Ready ";
  } else if (cmd == 0x3e) {
    msg += "Zone4: ";
    int zones = getBinaryData(word,41,8);
    if (zones & 1) msg += "1 ";
    if (zones & 2) msg += "2 ";
    if (zones & 4) msg += "3 ";
    if (zones & 8) msg += "4 ";
    if (zones & 16) msg += "5 ";
    if (zones & 32) msg += "6 ";
    if (zones & 64) msg += "7 ";
    if (zones & 128) msg += "8 ";
    if (zones == 0) msg += "Ready ";
  } else if (cmd == 0x0a) {
    msg += "Panel Program Mode";
  } else if (cmd == 0x63) {
    msg += "Alarm Memory Group 2";
  } else if (cmd == 0x64) {
    msg += "Beep Command Group 1";
  } else if (cmd == 0x69) {
    msg += "Beep Command Group 2";
  } else if (cmd == 0x5d) {
    msg += "Alarm Memory Group 1";
  } else if (cmd == 0x39) {
    msg += "Undefined command from panel";
  } else if (cmd == 0xb1) {
    msg += "Zone Configuration";
  } else if (cmd == 0x11) {
    msg += "Query available keypads";
  } else if (cmd == 0xff) { // keypad to panel data
    msg += "From Keypad ";
    if (getBinaryData(word,8,32) == 0xffffffff)
      msg += "idle";
    else {
      button = getBinaryData(word,8,16); //bits 11~14 data; 15~16 CRC 
      if (button ==  0x947f)
        msg += "button * pressed ";
      else if (button == 0x96ff)
        msg += "button # pressed ";
      else if (button == 0x807f)
        msg += "button 0 pressed ";
      else if (button == 0x82ff)
        msg += "button 1 pressed ";
      else if (button == 0x857f)
        msg += "button 2 pressed ";
      else if (button == 0x87ff)
        msg += "button 3 pressed ";
      else if (button == 0x88ff)
        msg += "button 4 pressed ";
      else if (button == 0x8b7f)
        msg += "button 5 pressed ";
      else if (button == 0x8dff)
        msg += "button 6 pressed ";
      else if (button == 0x8e7f)
        msg += "button 7 pressed ";
      else if (button == 0x917f)
        msg += "button 8 pressed ";
      else if (button == 0x937f)
        msg += "button 9 pressed ";
      else if (button == 0xd7ff)
        msg += "stay button pressed ";
      else if (button == 0xd8ff)
        msg += "away button pressed ";
      else if (button == 0xddff)
        msg += "chime button pressed ";
      else if (button == 0xf0ff)
        msg += "exit pressed ";
      else if (button == 0xffff)
        msg += "idle ";
      else {
        msg += "unknown keypad msg ";
        msg += button;
      }
    }
  } else
    msg += "Unknown command from panel";
    
  return cmd; // return command associated with the message

} // decode

void serialPrint(String word, String msg, int cmd, int type) {
  if (type==1) {
     Serial.print("Panel : ");
//     Serial.print(word);
     Serial.print(formatDisplay(word));
  } else {
     Serial.print("Client: "); 
//     Serial.print(word); 
     Serial.print(formatDisplayKeypad(word));
  }
  Serial.print("-> ");
  Serial.print(counter);
  Serial.print(": ");
  //Serial.print(hex8(cmd));
  Serial.print(cmd, HEX);
  Serial.print(":");
  Serial.println(msg);
}

int readButtons(int pin)
// returns the button number pressed, or zero for none pressed 
// int pin is the analog pin number to read 
{
  int b,c = 0;
  c=analogRead(pin); // get the analog value  
  if (c>1000)
  {
    b=0; // buttons have not been pressed
  }   
else
  if (c>440 && c<470)
  {
    b=1; // button 1 pressed
  }     
  else
    if (c<400 && c>370)
    {
      b=2; // button 2 pressed
    }       
    else
      if (c>280 && c<310)
      {
        b=3; // button 3 pressed
      }         
      else
        if (c>150 && c<180)
        {
          b=4; // button 4 pressed
        }           
        else
          if (c<20)
          {
            b=5; // button 5 pressed
          }
return b;
}
