#include <stdio.h>
#include <iostream>
#include <fstream>
#include <tchar.h>
#include "SerialClass.h"	// Library described above
#include <string>

using namespace std;
// application reads from the specified serial port and reports the collected data
int _tmain(int argc, _TCHAR* argv[])
{ // ****************************** CONNECT SERIAL ***************************
	printf("playSongs.exe: makes serial connection and sends byte files to arduino\n\n");
	Serial* SP;
    bool connected = false;
    while(!connected){
	// *** ANDROID *** input text (COM port)
	cout << endl << "Enter COM port number"<<endl;
    string comnr = "";
    getline(cin, comnr);

    string compoort = "\\\\.\\COM";
    compoort+= comnr;
    const char* comprt = compoort.c_str();

    // *** ANDROID *** connection button
	SP = new Serial(comprt);    // adjust as needed

	// *** ANDROID *** connection status text field
	if (SP->IsConnected())
		printf("We're connected \n");
		connected = true;
}
 // ******************************* READ DATA FROM FILE ************************
 int numReg = 7;
 string inputfile = "";
 string textfile = "";
 string exitString = ("e");
 string connectString = ("c");
 bool stopPlaying = false;

 while(inputfile.compare(exitString)!=0){

	unsigned char incomingData[256] = "";			// don't forget to pre-allocate memory
	unsigned char outgoingData[7]={0,0,0,0,0,0,0};
		//printf("%s\n",incomingData);
	int dataLength = 255;
	int readResult = 0;
	bool gelukt;

        // send 0 to make sure all solenoids are off while waiting
        gelukt = SP->WriteData(outgoingData, numReg);

 cout << endl << "Please enter filename(.txt). (e = end, c = reconnect usb)"<<endl;
 stopPlaying = false;
 // *** ANDROID *** input text (filename)
  getline(cin, inputfile);
 if(inputfile.compare(exitString)==0) return 0;
 if(inputfile.compare(connectString)==0){ // reconnect
 // no longer connected
            cout << "Reconnect: please enter COM port nr" << endl;
            string comnr = "";
    getline(cin, comnr);

    string compoort = "\\\\.\\COM";
    compoort+= comnr;
    const char* comprt = compoort.c_str();

	SP = new Serial(comprt);    // adjust as needed

	if (SP->IsConnected())
		printf("We're connected \n");
		connected = true;

 }
 textfile = inputfile + string(".txt");

//open file
// *** ANDROID *** push button (play)
std::ifstream infile(textfile.c_str(), ios::binary);
if(infile.fail()){
        cout << "Error: file doesn't exist. mohool" << endl;
    }
else{ // file exists
            cout << "Playing song (space = stop)" << endl;
            Sleep(2000); // tijd om balgen op te pompen

    //get length of file
infile.seekg(0, infile.end);
size_t length = infile.tellg();
infile.seekg(0, infile.beg);
printf("lengte bestand= ");
printf("%i",length);


char prebuffer[length];
unsigned char buffer[length];

    //read file
infile.read(&prebuffer[0], length);
  for(unsigned k=0; k<length; k++){
        buffer[k]=prebuffer[k];
      //  cout << "in char " << buffer[k] << '\t';
        //cout << static_cast<int>(buffer[k]) << '\t';
        //buffer[k]=static_cast<byte>(buffer[k]);
  }
 // cout << endl;

int songLength = (length-3)/8;
int times[songLength];
unsigned char notes[songLength*numReg];

for(int i=0;i<songLength;i++){
    times[i]=static_cast<int>(buffer[songLength*numReg+i+2])*10; // 1/100s nr ms
    for(int j=0; j<numReg; j++){
        notes[i*numReg+j]=buffer[i*numReg+j+1];
      //     cout << static_cast<int>( notes[i*numReg+j]) << '\t';
    }
    //cout << "times: " << times[i];
    //cout << endl;
}



	//while(SP->IsConnected())
	if(SP->IsConnected())
	{
	  printf("outgoing data \n");
	  // *** ANDROID *** interruptable (stop button)
	  int index = 0;
	    while((index<songLength)&&(!stopPlaying)){
          //     printf("\n");
                for (int index2 = 0; index2<numReg; index2++)
                       outgoingData[index2]=notes[index*numReg+index2];
        gelukt = SP->WriteData(outgoingData, numReg);
   
if (GetAsyncKeyState(VK_SPACE))
{
	stopPlaying = true; //User pressed space
}
		// *** ANDROID *** time untill next note: scalable with slider?
		Sleep(times[index]); 
		index++;
		} // for
	    } // if connected
	}// else file exists
 }
 unsigned char outgoingData[7]={0,0,0,0,0,0,0};
 SP->WriteData(outgoingData, numReg);

	return 0;
}