//  SBC8080 Boot Loader Firmware
#include <SPI.h>
#include <SD.h>

// Pin assign
#define RES A0  // reset control
#define LEL A1  // latch lower address
#define LEH A2  // latch higher address
#define OC A3   // latch output control
#define HOLD A4
#define HLDA A5
#define WR 8    // D8: memory write control
#define XC 9    // D9: 8251 TXC/RXC

File fp;  // file pointer

// fatal error
void error() {
    SPI.end();  //SCK -> PORT
    while (true);
}

void setup() {
  // Boud rate generator
  // Special Thanks wsnak
  pinMode(9, OUTPUT);

  // OC1A(PB1/D9) toggle
  TCCR1A &= ~(1 << COM1A1);   // 0
  TCCR1A |=  (1 << COM1A0);   // 1

  // WGM13-10 = 0100 CTCモード
  TCCR1B &= ~(1 << WGM13);    // 0
  TCCR1B |=  (1 << WGM12);    // 1
  TCCR1A &= ~(1 << WGM11);    // 0
  TCCR1A &= ~(1 << WGM10);    // 0

  // ClockSource CS12-CS10 = 001 16MHz / 1 T= 0.0625us
  TCCR1B &= ~(1 << CS12);     // 0
  TCCR1B &= ~(1 << CS11);     // 0
  TCCR1B |=  (1 << CS10);     // 1

  //OCR1A = 50; //156.9kHz
  OCR1A = 51; //153.9kHz

  // SD card init
  delay(500);
  pinMode(10, OUTPUT);
  SD.begin();
  delay(500);
}

// Test program
const byte code[] = {
  0x31,0x00,0x80, //LXI SP,8000
  0x3E,0x00,      //MVI A,00
  0xD3,0x01,      //OUT UARTRC ;escape
  0xD3,0x01,      //OUT UARTRC ;make sure
  0xD3,0x01,      //OUT UARTRC ;make sure
  0x3E,0x40,      //MVI A,01000000B ;reset
  0xD3,0x01,      //OUT UARTRC
  0x3E,0x4E,      //MVI A,01001110B ;mode
  0xD3,0x01,      //OUT UARTRC
  0x3E,0x37,      //MVI A,00110111B ;command
  0xD3,0x01,      //OUT UARTRC
  0x3E,0x3E,      //MVI A,'>'
  0xF5,           //LOOP PUSH PSW
  0xDB,0x01,      //UWLOP1 IN UARTRC
  0xE6,0x01,      //ANI 00000001B
  0xCA,0x1A,0x00, //JZ UWLOP1
  0xF1,           //POP PSW
  0xD3,0x00,      //OUT UARTRD
  0xDB,0x01,      //USRTRD IN UARTRC
  0xE6,0x02,      //ANI 00000010B
  0xCA,0x24,0x00, //JZ USRTRD
  0xDB,0x00,      //IN UARTRD
  0xC3,0x19,0x00  //JMP LOOP
};

// read a char and return half byte
byte getValue() {
  byte c;

  c = fp.read();  //read a char

  if ((c >= 'A') && (c <= 'F')) //case HEX char
    return 10 + (c - 'A');  //get half byte

  if ((c >= 'a') && (c <= 'f')) //case hex char
    return 10 + (c - 'a');  //get half byte

  return c - '0'; //get half byte
}

// read 2 chars and return byte
byte getByte() {
  byte c1, c2;

  c1 = getValue();  //higher half
  c2 = getValue();  //lower half
  return (c1 << 4) | c2;  //get byte
}

// read 4 chars and return adress
word getWord() {
  byte c1, c2;

  c1 = getByte(); //upper address
  c2 = getByte(); //lower address
  return ((word)c1 << 8) | c2;  //get address
}

//  write a byte to RAM
void store(word adrs, byte data) {
  PORTD = adrs >> 8;
  digitalWrite(LEH, HIGH);
  delayMicroseconds(1);
  digitalWrite(LEH, LOW);
  PORTD = adrs & 0xff;
  digitalWrite(LEL, HIGH);
  delayMicroseconds(1);
  digitalWrite(LEL, LOW);
  PORTD = data;
  digitalWrite(WR, LOW);
  delayMicroseconds(1);
  digitalWrite(WR, HIGH);
}

//Intel HEX format
boolean setupHex() {
  byte len;
  word adrs;
  byte type;

  while (true) {
    while (fp.read() != ':'); //pass garbage
    len = getByte();  //length
    adrs = getWord(); //address
    type = getByte(); //type

    if (type == 0) {  //data
      while (len--) //all data
        store(adrs++, getByte()); //write to RAM
      getByte(); // checksum not care
    } else if (type == 1) { // EOF
      getByte(); // checksum not care
      return true; //break;
    } else {
      return false;  //unknown type
    }
  }
}

void loop() {
  pinMode(RES, INPUT_PULLUP); //RES release
  pinMode(HOLD, OUTPUT);
  digitalWrite(HOLD, LOW);  //HOLD disable
  DDRD = 0x00; // Bus HiZ
  pinMode(OC, OUTPUT);
  digitalWrite(OC, HIGH);  //Latch disable
  pinMode(WR, OUTPUT);
  digitalWrite(WR, HIGH); //Write normal
  pinMode(LEH, OUTPUT);
  digitalWrite(LEH, LOW); //Latch H normal
  pinMode(LEL, OUTPUT);
  digitalWrite(LEL, LOW); //Latch L normal
  pinMode(HLDA, INPUT); //HOLD acknowledge
  delay(500);

  while (digitalRead(HLDA) == HIGH); //Check HOLD acknowledge
  digitalWrite(HOLD, HIGH); //HOLD request
  while (digitalRead(HLDA) == LOW); //Check HOLD acknowledge

  DDRD = 0xff; // Bus enable
  digitalWrite(OC, LOW);  //Latch enable

  // set 8080 memory
  word address;
  boolean err_flg = false;

  delay(100); //wait for reset
  
  if (SD.exists("INITPROG.HEX")) {  //exist HEX file
    fp = SD.open("INITPROG.HEX", FILE_READ);  //file open
    if (fp) {
      if (!setupHex()) { //write to RAM
        err_flg = true;
      }
    } else {
       err_flg = true;
    }
    fp.close(); //file close
  } else {  //file not found
    err_flg = true;
  }

  if (err_flg) {
    // set test program
    for(address = 0; address < 0x30; address++){
      store(address, code[address]);
    }
  }
  
  DDRD = 0x00; // Bus HiZ
  digitalWrite(OC, HIGH); //Latch HiZ
  pinMode(WR, INPUT_PULLUP); //WR HiZ
//  digitalWrite(HOLD, LOW);  //HOLD disable
//  while (digitalRead(HLDA) == HIGH); //Check HOLD acknowledge
  pinMode(RES, OUTPUT);
  digitalWrite(RES, LOW);
  digitalWrite(HOLD, LOW);  //HOLD disable tomi9 added
  delay(500);
  digitalWrite(RES, HIGH);
  delay(200);
  pinMode(RES, INPUT_PULLUP);
  while (true);
}

