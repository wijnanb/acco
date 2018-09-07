// ************************************************
// spoel aanstuur programma voor draaiorgel
// receives chars from pc, after 7 chars: shift out
// 3: clear pin
// 4: output Enable
// 5: latch pin (RCK)
// 6: clock pin (SRCK)
// 7 - 13: 7 shift registers
// ************************************************


const int clearPin = 3;
const int outputEnablePin = 4;
const int latchPin =5;
const int clockPin = 6;

// 8 noten per shift register, 
// 12V, 120ohm => 100mA per poot, max 5 tegelijk wegens 500mA beperking
// 53 noten => 7 shift registers

const int numShiftReg = 7; 


const int pinOffset = 7; // toevallig zelfde
const int data1 = 7;
const int data2 = 8;
const int data3 = 9;
const int data4 = 10;
const int data5 = 11;
const int data6 = 12;
const int data7 = 13;

int i = 0;
    byte notes[numShiftReg] = {0b00111110,0b00000000,0b00111110,0b00011110,0b00000000,0b00000000,0b00000000} ;
    byte allOn[numShiftReg] = {0b11111111, 0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111};
    byte silence[numShiftReg] = {0b00000000, 0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000};
    byte regTest[numShiftReg] = {0b00000000, 0b11111111, 0b00000000,0b00000000,0b00000000,0b00000000,0b00000000};
    byte thisChar; // voor serial werkt makkelijker met char
    



void setup() {
    Serial.begin(9600);
    pinMode(latchPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(outputEnablePin, OUTPUT);
    pinMode(clearPin, OUTPUT);
    pinMode(data1, OUTPUT);
    pinMode(data2, OUTPUT);
    pinMode(data3, OUTPUT);
    pinMode(data4, OUTPUT);
    pinMode(data5, OUTPUT);
    pinMode(data6, OUTPUT);
    pinMode(data7, OUTPUT);

    digitalWrite(outputEnablePin, 0);
    digitalWrite(clearPin, 1);

    shiftOutArray(regTest);
    delay(500);
    shiftOutArray(silence);
    
   i=0;
}

void loop() {

  while(Serial.available()>0){
    thisChar = Serial.read();
    notes[i] = thisChar;
    i++;
      if(i==numShiftReg){
        shiftOutArray(notes);
        //Serial.println();
       // Serial.print("Received 7 bytes: ");
    //    for (int j=0; j<numShiftReg; j++)
        // Serial.println((char)notes[j]);
       // Serial.println("||untill here.");
        i=0;
      }  
   }
}

void shiftOutArray(byte myDataOut[]) { //********************************************** SHIFT OUT ARRAY ***********************************************************
// parallelle variant omdat alle klokken gelijk op gaan
 int i=0;
 int j=0;
 int pinState;
 
  digitalWrite(latchPin, 0);
  for (i=0; i<8; i++)  {
     digitalWrite(clockPin, 0); // voor alle registers
     for(j=0;j<numShiftReg;j++){ // voor pin pinOffset+j
     // for(j=0;j<5;j++){  // debug versie
    if ( myDataOut[j] & (1<<i) ) {
      pinState= 1;
    }
   else {  
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
}



