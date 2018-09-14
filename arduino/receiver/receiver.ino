#define DEBUG 1 // set DEBUG = false to maximize performance
#define DEBUG_BUFFER 0
#define USB Serial
#define BLUETOOTH Serial1
#define BUFFER_SIZE 128
#define MIN_BUFFER 64

const int clearPin = 3;
const int outputEnablePin = 4;
const int latchPin =5;
const int clockPin = 6;

// 8 noten per shift register, 
// 12V, 120ohm => 100mA per poot, max 5 tegelijk wegens 500mA beperking
// 53 noten => 7 shift registers

const int numShiftReg = 7; 
const int pinOut[numShiftReg] = {7, 8, 9, 10, 11, 12, 14}; //TODO Lize: verander 14 naar 13, op teensy is dit ingebouwde LED

uint8_t notesIndex = 0;
byte notes[numShiftReg]   = {0b00111110, 0b00000000, 0b00111110, 0b00011110, 0b00000000, 0b00000000, 0b00000000};
byte allOn[numShiftReg]   = {0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111};
byte silence[numShiftReg] = {0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000};
byte regTest[numShiftReg] = {0b00000000, 0b11111111, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000};
   

bool connected = false;
unsigned long lastTick = 0;
unsigned long tickDelay = 10000; // microseconds

int led = 13;
byte buffer[BUFFER_SIZE];

uint16_t write_index = 0;
uint16_t read_index = 0;
uint16_t buffer_state_size = 0;
bool started = false;


// Interrupt on data sent on Bluetooth module 
void serialEvent1() {
  while(BLUETOOTH.available() > 0) {
    byte data = BLUETOOTH.read();

    if (data == 0x3F || data == 0x7F) {
      USB.print("ignore "); printHex(data); USB.println();
      continue;
    }

    buffer[write_index] = data;

    //USB.print(byteCount); USB.print(F(": ")); USB.print(data, HEX); USB.println();
    write_index = (write_index + 1) % BUFFER_SIZE;
    updateBufferState();
  }
}

void setup() {
  USB.begin(9600); // Debugging over USB
  BLUETOOTH.begin(9600); // Default communication rate of the Bluetooth module

  pinMode(led, OUTPUT); // controle led

  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(outputEnablePin, OUTPUT);
  pinMode(clearPin, OUTPUT);
  for (int i=0; i<numShiftReg; i++) pinMode(pinOut[i], OUTPUT);

  //WAIT FOR EVERYTHING TO STABILIZE BEFORE STARTING THE LOOP
  digitalWrite(led, HIGH); delay(500); digitalWrite(led, LOW); delay(500); digitalWrite(led, HIGH); delay(500); 
  digitalWrite(led, LOW); delay(500); digitalWrite(led, HIGH); delay(500); digitalWrite(led, LOW);

  if (DEBUG) { USB.println(F("Init")); }

  digitalWrite(outputEnablePin, 0);
  digitalWrite(clearPin, 1);

  shiftOutArray(regTest);
  delay(500);
  updateBufferState();
  if (DEBUG) { USB.println(F("Buffer initialized, we can start streaming")); }  
}

void loop() {
  // very precise ticker
  if (micros() >= lastTick + tickDelay) {
    onTick();
    lastTick = lastTick + tickDelay;
  }
}

void onTick() {
  if (buffer_state_size == 0) { started = false; }
  else if (buffer_state_size > MIN_BUFFER) { started = true; }

  if (started) {
    toOutput(buffer[read_index]);

    read_index = (read_index + 1) % BUFFER_SIZE;
    updateBufferState();
    //TODO: fix bug where last byte in buffer is never sent to output
  }
}

void toOutput(byte data) { 
  if (DEBUG) { 
    USB.print(F("toOutput: ")); 
    printHex(data);
    USB.print(F("  ")); USB.print(micros() - lastTick); USB.print(F("us"));
    USB.print(F("  buffer: ")); USB.print(buffer_state_size); 
    USB.println(); 
  }

  // group bytes in array of 7 bytes and shift out together
  notes[notesIndex] = data;
  notesIndex++;
  if (notesIndex == numShiftReg) {
    shiftOutArray(notes);
    notesIndex = 0;
  }
}

void updateBufferState() {
  if (write_index >= read_index) buffer_state_size = write_index - read_index;
  else buffer_state_size = write_index + (BUFFER_SIZE - read_index);

  if (DEBUG && DEBUG_BUFFER) { 
    USB.print(F("buffer size:"));  USB.print(buffer_state_size); 
    USB.print(F(" w:"));  USB.print(write_index); 
    USB.print(F(" r:"));  USB.print(read_index); 
    USB.println();
  }

  // when buffer is empty, we either have a buffer underrun or track has ended. --> close all pipes.
  if (buffer_state_size == 0) {
    if (DEBUG) { USB.print(F("BUFFER EMPTY")); USB.println(); }
    toOutput(buffer[read_index]);
    started = false;
    shiftOutArray(silence);
  }

  // when buffer overruns, we skip a part and move the read index a bit further
  if (buffer_state_size == BUFFER_SIZE-1) {
    if (DEBUG) { USB.print(F("BUFFER OVERRUN")); USB.println(); }
    read_index = (read_index + MIN_BUFFER) % BUFFER_SIZE;
  }

  digitalWrite(led, buffer_state_size > 0 ? HIGH : LOW); 
}


/*
  Hoe ik denk dat het werkt :-)

  er zitten 8 bits in een byte
  data = 7 bytes: data0, data1, ... data6
  er zijn 7 shift registers oftewel pinOut0, pinOut1, ... pinOut6

  We nemen bit0 van data0 en zetten die op pinOut0
  bit 0 van data1 op pinOut1
  ...
  bit 0 van data6 op pinOut6
  clockPin up-down
  bit 1 van data0 op pinOut0
  bit 1 van data1 op pinOut1
  ...
  bit 1 van data6 op pinOut6
  .......
  bit 7 van data6 op pinOut6

*/
void shiftOutArray(byte data[]) {
  int i=0, j=0;
  
  digitalWrite(latchPin, 0);
  digitalWrite(clockPin, 0); 

  USB.print(F("shiftOutArray: "));
  for (int d=0; d<numShiftReg; d++) { printHex(data[d]); USB.print(F(" ")); }
  USB.println();

  for (i=0; i<8; i++)  { // from LeastSignificantBit to HighestSignificantBit
    
    for (j=0; j<numShiftReg; j++) { // from shiftregister 0 to 6
      digitalWrite(pinOut[j], (data[j] & (1<<i)) ? 1 : 0); // bitI from dataJ on pinOutJ
    }

    //register shifts bits on upstroke of clock pin 
    digitalWrite(clockPin, 1);
    delayMicroseconds(30); // pulse op clockPin onafhankelijk van processor speed
    digitalWrite(clockPin, 0);
    
    //zero the data pin after shift to prevent bleed through ???? nodig???
    for (j=0; j<numShiftReg; j++) digitalWrite(pinOut[j], 0); // all pinOuts to 0
  }
  
  digitalWrite(clockPin, 0);
  digitalWrite(latchPin, 1);
}

void printHex(byte data) {
  USB.print("0x"); 
  if (data < 16) USB.print("0");
  USB.print(data, HEX); 
}