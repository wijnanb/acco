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
const int led = 13; // ingebouwde led van Arduino

// status leds betekenen:
// Bluetooth rode LED: 
// flikkert snel --> niet geconnecteerd met Android toestel
// flikkert 2x dan pause --> geconnecteerd met Android toestel
// Arduino ingebouwde led:
// 3x flikkert bij opstarten --> boot
// flikkert --> receiving data


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

const byte START_BYTE = 0x3F;
const byte END_BYTE   = 0x7F;
const byte TEMPO_BYTE = 0xEF;
const byte SYNC_BYTE  = 0xFF;

bool connected = false;
unsigned long measuredTickDelay = 0;
unsigned long nextTick = 0;
unsigned long currentTick = 0;
unsigned long tickDelay = 10000; // microseconds. default delay between notes

byte previousData = 0;
byte buffer[BUFFER_SIZE];

uint16_t write_index = 0;
uint16_t read_index = 0;
uint16_t buffer_state_size = 0;
bool started = false;
bool waitingForSyncByte = false;
uint16_t outOfSyncCounter = 0;

unsigned long ledToggleDelay = 100;
unsigned long lastReceivedData = 0;

// Interrupt on data sent on Bluetooth module 
void serialEvent1() {
  while(BLUETOOTH.available() > 0) {
    lastReceivedData = millis();

    byte data = BLUETOOTH.read();
    // USB.print(F("received: ")); printHex(data); USB.print(F(" previous: ")); printHex(previousData); USB.println();

    if (previousData == TEMPO_BYTE) {
      tickDelay = data * 1000;
      USB.print(F("set tempo: ")); USB.print(data); USB.println(F("ms"));
    }

    previousData = data;

    if (data == START_BYTE) {
      USB.print(F("received START_BYTE ")); printHex(data); USB.println();
      // clear buffer
      write_index = 0;
      read_index = 0;
      continue;
    }

    if (data == TEMPO_BYTE) {
      USB.print(F("received TEMPO_BYTE ")); printHex(data); USB.println();
      // next byte is tempo byte
      continue;
    }

    if (data == TEMPO_BYTE) {
      USB.print(F("received END_BYTE ")); printHex(data); USB.println();
      USB.print(F("outOfSyncCounter: ")); USB.print(outOfSyncCounter); USB.println();
      continue;
    }

    buffer[write_index] = data;

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

  currentTick = micros();
  nextTick = currentTick + tickDelay;
}

void loop() {
  // very precise ticker
  if (micros() >= nextTick) {
    measuredTickDelay = micros() - currentTick;
    currentTick = micros();
    nextTick = nextTick + tickDelay;

    // USB.print(F("    tick  now: ")); USB.print(currentTick); 
    // USB.print(F(" next: in ")); USB.print(nextTick-currentTick);
    // USB.print(F(" measuredTickDelay: ")); USB.print(measuredTickDelay); USB.println();
    onTick();

    digitalWrite(led, (millis() < lastReceivedData + ledToggleDelay) ? ((millis()/ledToggleDelay) % 2 == 0) : 0);
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
    USB.print(F("* toOutput: ")); 
    printHex(data);
    USB.print(F("  ")); USB.print(measuredTickDelay); USB.print(F("us"));
    USB.print(F("  buffer: ")); USB.print(buffer_state_size); 
    USB.println(); 
  }

  // group bytes in array of 7 bytes and shift out together. 8 bytes data = 0xFF + 7 bytes

  if (data == SYNC_BYTE) {
    USB.println(F("- SYNC_BYTE"));
    
    if (!waitingForSyncByte) {
      outOfSyncCounter++;
      USB.println(F("- !!!! Received SYNC_BYTE too early")); 
    }
    notesIndex = 0;
    waitingForSyncByte = false;
    return;
  }

  if (waitingForSyncByte) {
    USB.println(F("- !!!! Out of sync, waiting for SYNC_BYTE")); 
    outOfSyncCounter++;
  }

  notes[notesIndex] = data;
  notesIndex++;
  if (notesIndex == numShiftReg) {
    shiftOutArray(notes);
    waitingForSyncByte = true;
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
    started = false;
    if (DEBUG) { USB.print(F("BUFFER EMPTY")); USB.println(); }

    //TODO delay tickDelay otherwise last note is never played
    shiftOutArray(silence);
  }

  // when buffer overruns, we skip a part and move the read index a bit further
  if (buffer_state_size == BUFFER_SIZE-1) {
    if (DEBUG) { USB.print(F("BUFFER OVERRUN")); USB.println(); }
    read_index = (read_index + MIN_BUFFER) % BUFFER_SIZE;
  }
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