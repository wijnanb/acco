#define DEBUG 0 // set DEBUG = 0 to maximize performance
#define DEBUG_BUFFER 1
#define WARN_OUT_OF_SYNC 1
#define USB Serial
#define BLUETOOTH Serial1
#define BUFFER_SIZE 128
#define MIN_BUFFER 72 // FF, delay, 7 bytes 

const int clearPin = 3;
const int outputEnablePin = 4;
const int latchPin =5;
const int clockPin = 6;
const int led = 15; // ingebouwde led van Arduino

const int pinOffset = 7; // toevallig zelfde

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
const int pinOut[numShiftReg] = {7, 8, 9, 10, 11, 12, 13}; //TODO Lize: verander 14 naar 13, op teensy is dit ingebouwde LED

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

byte buffer[BUFFER_SIZE];
byte inputBuffer[2];
int inputBufferSize = 0;

uint16_t write_index = 0;
uint16_t read_index = 0;
uint16_t buffer_state_size = 0;
bool started = false;
bool waitingForSyncByte = false;
uint16_t outOfSyncCounter = 0;
unsigned long bytesReceived = 0;

unsigned long ledToggleDelay = 100;
unsigned long lastReceivedData = 0;

// Interrupt on data sent on Bluetooth module 
void serialEvent1() {
  while(BLUETOOTH.available() > 0) {
    lastReceivedData = millis();

    byte data = BLUETOOTH.read();
    bytesReceived++;
    // USB.print(F("received: ")); printHex(data); USB.print(F(" previous: ")); printHex(previousData); USB.println();

    // buffer last 2 bytes read to capture tempo, and start/end sequence
    inputBuffer[1] = inputBuffer[0];
    inputBuffer[0] = data;
    inputBufferSize = min(2, inputBufferSize + 1);

    if (inputBuffer[1] == TEMPO_BYTE) {
      tickDelay = inputBuffer[0];
      USB.print(F("set tempo: ")); USB.print(inputBuffer[0]); USB.println(F("ms"));
      inputBufferSize = 0;
    }

    if (inputBuffer[1] == START_BYTE && inputBuffer[0] == 0x00) {
      USB.print(F("received START_BYTE sequence ")); printHex(inputBuffer[1]); USB.print(F(" ")); printHex(inputBuffer[0]); USB.println();
      // clear buffer
      write_index = 0;
      read_index = 0;
      outOfSyncCounter = 0;
      inputBufferSize = 0;
      waitingForSyncByte = true;
    }

    if (inputBuffer[1] == 0x00 && inputBuffer[0] == END_BYTE) {
      USB.print(F("received END_BYTE ")); printHex(inputBuffer[1]); USB.print(F(" ")); printHex(inputBuffer[0]); USB.println();
      inputBufferSize = 0;
    }


    if (inputBufferSize == 2) {
      // write byte to next buffer
      buffer[write_index] = inputBuffer[1];
      write_index = (write_index + 1) % BUFFER_SIZE;
      updateBufferState();
    }
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

  USB.println(F("Init"));
  if (!DEBUG) { USB.println(F("Debug printing disabled. set DEBUG=1 to enable.")); }

  digitalWrite(outputEnablePin, 0);
  digitalWrite(clearPin, 1);

  shiftOutArray(regTest);
  delay(500);
  shiftOutArray(silence);

  updateBufferState();
  if (DEBUG) { USB.println(F("Buffer initialized, we can start streaming")); }  
}

void loop() {

  if (buffer_state_size == 0) { started = false; }
  else if (!started && buffer_state_size > MIN_BUFFER) {
    started = true; 
    currentTick = millis();
    nextTick = millis();
  }

  if (started) {

    // very precise ticker
    if (millis() >= nextTick) {
      measuredTickDelay = millis() - currentTick;
      currentTick = millis();

      //USB.print(F(" tick  now: ")); USB.print(currentTick); USB.println();
      printHex(buffer[read_index]); USB.println();

      if (buffer[read_index] == SYNC_BYTE) {
        read_index = (read_index + 1) % BUFFER_SIZE;
        updateBufferState();
        byte dt = buffer[read_index];

        USB.print(F("dt")); printHex(dt); USB.println();

        nextTick = nextTick + dt * tickDelay;
        read_index = (read_index + 1) % BUFFER_SIZE;
        updateBufferState();

        //USB.print(F("delay: "));  USB.println(dt * tickDelay);

        toOutput();
      } else {

        USB.println(F("NO SYNC BYTE"));
      }

      //USB.print(F(" next: in ")); USB.print(nextTick-currentTick);
      USB.print(millis()); USB.print(F(" measuredTickDelay: ")); USB.print(measuredTickDelay); USB.println();

    }
  }

  updateBufferState();
}

void toOutput() { 
  for (int i=0; i<7; i++) {
    notes[i] = buffer[read_index];
    read_index = (read_index + 1) % BUFFER_SIZE;
    updateBufferState();
  }
  
  shiftOutArray(notes);
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
  if (buffer_state_size == 0 && started) {
    started = false;
    if (DEBUG) { USB.print(F("BUFFER EMPTY")); USB.println(); }

    printStatistics();

    bytesReceived = 0;

    //TODO delay tickDelay otherwise last note is never played
    shiftOutArray(silence);
  }

  // when buffer overruns, we skip a part and move the read index a bit further
  if (buffer_state_size == BUFFER_SIZE-1) {
    if (DEBUG) { USB.print(F("BUFFER OVERRUN")); USB.println(); }
    read_index = (read_index + MIN_BUFFER) % BUFFER_SIZE;
  }
}

void printStatistics() {
  USB.print(F("received: ")); USB.print(bytesReceived); USB.println(F(" bytes"));
  USB.print(F("out of sync: ")); USB.print(outOfSyncCounter); USB.println(F(" times"));
}

void shiftOutArray(byte myDataOut[]) { //********************************************** SHIFT OUT ARRAY ***********************************************************
  // parallelle variant omdat alle klokken gelijk op gaan
 int i=0;
 int j=0;
 int pinState;


  USB.print(F("shiftOutArray "));
  for (int i=0; i<7; i++) {
    printHex(myDataOut[i]); USB.print(F(" "));
  }
  USB.println();
  
  unsigned long s = millis();

  digitalWrite(latchPin, 0);

  for (i=0; i<8; i++)  {
    digitalWrite(clockPin, 0); // voor alle registers
     
    for(j=0;j<numShiftReg;j++){ // voor pin pinOffset+j
      if ( myDataOut[j] & (1<<i) ) {
        pinState= 1;
      } else {  
        pinState= 0;
      }
      
      digitalWrite(pinOffset+j, pinState); // TODO reset this
    }
    
    //register shifts bits on upstroke of clock pin  
    digitalWrite(clockPin, 1);
    
    for(j=0;j<numShiftReg;j++){
      // for(j=0;j<5;j++){ // debug versie
       //zero the data pin after shift to prevent bleed through ???? nodig???
      digitalWrite(j+pinOffset, 0);
    }
  }

  //stop shifting
  digitalWrite(clockPin, 0);
  digitalWrite(latchPin, 1);

  USB.print(F("took ")); USB.println(millis()-s);
}


void printHex(byte data) {
  if (data < 16) USB.print(F("0"));
  USB.print(data, HEX); 
}