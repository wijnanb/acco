#define DEBUG 1 // set DEBUG = false to maximize performance
#define DEBUG_BUFFER 0
#define USB Serial
#define BLUETOOTH Serial1
#define BUFFER_SIZE 128
#define MIN_BUFFER 64

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

  toOutput(0);

  //WAIT FOR EVERYTHING TO STABILIZE BEFORE STARTING THE LOOP
  digitalWrite(led, HIGH); 
  delay(500);
  digitalWrite(led, LOW);
  delay(500);
  digitalWrite(led, HIGH); 
  delay(500);
  digitalWrite(led, LOW);
  delay(500);
  digitalWrite(led, HIGH); 
  delay(500);
  digitalWrite(led, LOW);

  if (DEBUG) { USB.println(F("init")); }
  updateBufferState();
}

void loop() {
  // very precise ticker
  if (micros() >= lastTick + tickDelay) {
    onTick();
    lastTick = lastTick + tickDelay;
  }
}

void onTick() {
  if (buffer_state_size > 0) {
    
    if (buffer_state_size == 0) { started = false; }
    else if (buffer_state_size > MIN_BUFFER) { started = true; }

    if (started) {
      byte data = buffer[read_index];
      toOutput(data);
      read_index = (read_index + 1) % BUFFER_SIZE;
      updateBufferState();
    }
  }

  if (!started) {
    // always send "close all pipes to output"
    toOutput(0);
  }
}

void toOutput(byte data) {
  
  // don't print when "close all pipes" is sent
  if (data != 0) {
    if (DEBUG) { 
      USB.print(F("toOutput: 0x")); 
      if (data < 16) USB.print("0");
      USB.print(data, HEX); 
      USB.print(F("  ")); USB.print(micros() - lastTick); USB.print(F("us"));
      USB.print(F("  buffer: ")); USB.print(buffer_state_size); 
      USB.println(); 
    }
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
    started = false;
  }

  // when buffer overruns, we skip a part and move the read index a bit further
  if (buffer_state_size == BUFFER_SIZE-1) {
    if (DEBUG) { USB.print(F("BUFFER OVERRUN")); USB.println(); }
    read_index = (read_index + MIN_BUFFER) % BUFFER_SIZE;
  }

  digitalWrite(led, buffer_state_size > 0 ? HIGH : LOW); 
}




