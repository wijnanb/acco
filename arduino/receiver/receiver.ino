#define DEBUG 1
#define USB Serial
#define BLUETOOTH Serial1

bool connected = false;
unsigned long lastTick = 0;
unsigned long tickDelay = 10000; // microseconds

int led = 13;
unsigned long byteCount = 0;


// Interrupt on data sent on Bluetooth module 
void serialEvent1() {
  while(BLUETOOTH.available() > 0) {
    byte data = BLUETOOTH.read();
    byteCount++;
    //USB.print(byteCount); USB.print(F(": ")); USB.print(data, HEX); USB.println();
  }

  // USB.print(F("byteCount: ")); USB.print(byteCount); USB.println();
}

void setup() {
  USB.begin(9600); // Debugging over USB
  BLUETOOTH.begin(9600); // Default communication rate of the Bluetooth module


  pinMode(led, OUTPUT); // controle led

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

  USB.println(F("init"));
}

void loop() {
  // very precise ticker
  if (micros() >= lastTick + tickDelay) {
    onTick();
    lastTick = lastTick + tickDelay;
  }
}

void onTick() {
  //USB.print(micros()); USB.print(" "); USB.println(F("tick"));
}
