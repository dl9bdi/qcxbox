/*
 * qcx-box
 * 6/2021 by Matthias Renken, DL9BDI
 * 
 * A cw mailbox application for qcx transceivers.
 * Its purpose is to provide a message storage and retrieval system that can be used 
 * by cw code.
 * It is designed for ESP32 NodeMCU but can be adapted to other Arduino types.
 * A tft display type ILI9341 is utilized for status information.
 *
 */

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

//define general constants
#define RXBUFFERLEN 140
#define NUMBEROFMESSAGES 5
#define MESSAGELENGTH 140
#define FREQUENCY "FA00007038000;" //frequency for mailbox operation

// define pins for ILI9341 display 
#define TFT_CLK    18
#define TFT_MOSI   23
#define TFT_MISO   19
#define TFT_CS     22
#define TFT_DC     21
#define TFT_RST    15
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

//define pins for second serial port
#define RXD2 16
#define TXD2 17

//define button pins. these buttons are for test purposes only and have no functions for cw box functionality 
#define BTN1 13
#define BTN2 14

// define timeout values 
#define SCREENREFRESH 1000  // screen refresh time
unsigned long rxDisplayStartTime = 0; 
#define READRXFROMQCX 1000 // defines how often the qcx will be asked for received cw messages
unsigned long readFromQcxStartTime = 0;
#define WAITFORSENDERSCALL 9000 //time to wait for the sender sending his call
#define PROCESSTIMEOUT 10000  //timeout for process steps in loop
unsigned long processTimer = 0;
#define MESSGAEREADTIME 20000  // defines the time a sender has to key his message
unsigned long messageReadTimer = 0;

//define control variables
char* myCall = "DL9BDI";
char yourCall[100] ="";
String commandStr ="";

int strShiftCounter; 
int boxState = 0; //controls state of qcx box and defines the flow

// message variables
char rxBuff[50];     // buffer for reading out decoded cw from qcx 
char keyBuff[4];     // buffer for checking if qcx is being keyed.
char rxTmpMsg[50];   // decodced message
char rxMsg[RXBUFFERLEN]; //stores the text that was decoded by qcx
char txMsg[40];
char storedMsg[NUMBEROFMESSAGES][MESSAGELENGTH];  //storage for messages that come in
int currentMsgNo = 0;  //current number of message storage
boolean readNoFound;   // message number to read was found
int msgNoToSend;       // number of message to send out
int tempi=0;           // general purpose int
int tempj=0;           // general purpose int
String tempstr;        // general purpose string


// display variables
#define RXMSGTOPLINE 25    //top line of display where output window for rx string starts
#define RXMSGBOTTOMLINE 45 //bottom line of display where output window for rx string ends
#define LOGTOPLINE 70      //top line of display where output window for logging starts
#define LOGBOTTOMLINE 240  //bottom line of display where output window for logging ends
int actLogLine=0;          //line number in log window to place output line 

//
// clear rxmsg display area
//
void clearRxWindow(){
  tft.fillRect(0,RXMSGTOPLINE,tft.width(),RXMSGBOTTOMLINE,ILI9341_BLACK);
}


//
// clear logging display area
//
void clearLogWindow(){
  tft.fillRect(0,LOGTOPLINE,tft.width(),LOGBOTTOMLINE,ILI9341_BLACK);
}

//
// write line to log window
//
void logStr(String text, int fontColor){
  tft.setCursor(0, actLogLine*20+LOGTOPLINE); // place text at next line in log window
  tft.setTextColor(fontColor,ILI9341_BLACK);  
  tft.setTextSize(2);
  tft.println(text);
  actLogLine++;
  if (actLogLine>11){  // end of log region reached so clear log window
    clearLogWindow();
    actLogLine=0;
  }
}


//
// general printout routine for tft display
//
void tftPrint(int xPos, int yPos, String text, int fontColor, int fontSize){
  tft.setCursor(xPos, yPos);
  tft.setTextColor(fontColor,ILI9341_BLACK);  
  tft.setTextSize(fontSize);
  tft.println(text);
}


//
// print current rx string to tft
//
void tftPrintRxMsg(){
  String rxMsgStr;
  rxMsgStr=rxMsg;
  tft.setCursor(0, RXMSGTOPLINE);
  tft.setTextColor(ILI9341_LIGHTGREY,ILI9341_BLACK);  
  tft.setTextSize(1);
  tft.println(rxMsgStr);
}


// 
//retrieve qcx status
//
void getQcxStatus(){
  Serial2.write("FA;"); //only reading current frequency, could be used for more status information
  Serial2.readBytesUntil(';',rxMsg,39);
  tftPrint(100,10,rxMsg, ILI9341_GREEN, 1);
}

//
// clears rx text variables
//
void clearRxMsg(){
  //clear message buffers
  for (int i=0; i<RXBUFFERLEN-1; i++){
    rxMsg[i]= ' '; 
  }
  rxMsg[RXBUFFERLEN-1]= '\0'; 
  for (int i=0; i<50-1; i++){
    rxBuff[i]=' '; 
    rxTmpMsg[i]=' '; 
  }
  rxBuff[50-1]='\0'; 
  rxTmpMsg[50-1]='\0';  
  rxMsg[RXBUFFERLEN-1]= '\0'; 

  for (int i=0; i<=3; i++){
    keyBuff[i]=' '; 
  }
  
}

//
// clears stored messages
//
void clearMessageStorage(){
  for (int i=0; i<NUMBEROFMESSAGES; i++){
    for (int j=0; j<MESSAGELENGTH-1; j++){
      storedMsg[i][j] = ' ';
    }
    storedMsg[i][MESSAGELENGTH] = '\0';
  }
}


//
// initialize qcx box
//
void setup() {
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tftPrint(0,0,"QCX Box", ILI9341_WHITE, 2);

  //initializing serial ports
  Serial.begin(115200); //for communication with Arduino serial monitor, not needed in normal operation
  Serial2.begin(38400, SERIAL_8N1, RXD2, TXD2); // for communication with qcx
  Serial2.print(FREQUENCY); //setting operation frequency
  
  
  getQcxStatus(); //getting a signal from qcx to see its alive
  clearRxMsg();   //clear rx buffers at startup
  clearMessageStorage(); //clears message store
  
  //initialize some values
  rxDisplayStartTime=millis();
  readFromQcxStartTime=millis();
  currentMsgNo = 0;
}

//
// read cw messages decoded by qcx into global rx string
//
void readCwMessage(){
  int txBytes;         // no of bytes to be sent if qcx is still in transmission mode
  int rxLen;           // no of bytes in response
  char rxLenTmp[3];    // tmporary for number conversion
  
  Serial2.write("TB;"); // sending read command to qcx 
  Serial2.readBytesUntil(';',rxBuff,39); // reading qcx response
  //check if response is valid
  if ((rxBuff[0]=='T') && (rxBuff[1]=='B')) { 

    //cutting response in pieces
    rxLenTmp[0]=rxBuff[2];
    rxLenTmp[1]='\0';
    txBytes = atoi(rxLenTmp);
  
    rxLenTmp[0]= rxBuff[3];
    rxLenTmp[1] =rxBuff[4];
    rxLenTmp[2]='\0';
    rxLen = atoi(rxLenTmp);
  
    if (rxLen>0){
      for (int i =0; i<rxLen; i++){
      rxTmpMsg[i]=rxBuff[i+5];
      }
      //transfer tmpMsg to rxMsg
      //first shift rxMsg left by number of new decodes characters   
      for (int i=0; i<=(RXBUFFERLEN-rxLen-2);i++){
        rxMsg[i]=rxMsg[i+rxLen];
      }
      //now add the newly decoded characters
      for (int i=0; i<(rxLen); i++){
        rxMsg[RXBUFFERLEN-rxLen-1+i]=rxTmpMsg[i];
      }
    }
  }
}

//
//send string to key to qcx up to 140 characters
//
void sendText(String text){
  char sendtext[150];
  int len = 0;
  sendtext[0]='K';
  sendtext[1]='Y';
  sendtext[2]=' ';
  len=text.length();
  if (len>140){
    len=140;
  }
  for (int i=0; i<=len; i++){
    sendtext[i+3]=text[i];
  }
  sendtext[len+3]=';';
  sendtext[len+4]='\0';
  logStr(String(text),ILI9341_GREEN);
  Serial2.write(sendtext);
}

//
//checks if searchText is contained in current rxMsg
//
boolean isContainedInRx(char* searchText){
  char * p;
  p=strstr(rxMsg, searchText);
  if (p){
    return true;
  } else {
    return false;
  }
}

//
//checks if message in storage is empty, i.e. consists of spaces only
//
boolean msgIsEmpty(int msgno){
boolean tmpbool = true;
  for (int i=0; i<MESSAGELENGTH; i++){
    if ((storedMsg[msgno][i]!=' ')&&((storedMsg[msgno][i]!='\0'))){
      tmpbool=false;
    }
  }
  return tmpbool;
}

//
//gives postition of first non-space character in current rxMsg
//
int firstNotEmptyPosInRx(){
  char p = ' ';
  int i = 0;
  while ((i<RXBUFFERLEN) && (p==' ')){
    p=rxMsg[i];
    i++;
  }
  return i-1;
}

//
//returns the first number in rxMsg
//
int firstNumberInRx(){
  int tmpi=-1;
  for (int i=0; i<RXBUFFERLEN; i++){
    if ((rxMsg[i]>=48) && (rxMsg[i]<=57)){
      tmpi=rxMsg[i]-48;      
    }
  }
  return tmpi;
}

//
// main loop
//
void loop() {
// testing routine that can be startet by a button if necessary for any purpose
  if (digitalRead(BTN1)==LOW){
  }


 int trimCounter=0; // counter to remove keyed characters from the beginning of the rxMsg 

 // start the state machine to handle message receiving and sending
 switch (boxState){
  case 0:  // box is ideling and listening
    if ((millis()-readFromQcxStartTime)>READRXFROMQCX){ // //if time for qcx reading has passed, ask it
      readCwMessage();
      readFromQcxStartTime=millis(); //reset screentimer
      tftPrintRxMsg();
    }
    if (isContainedInRx(myCall)){  //my callsing is found in received message so someone is calling me
      logStr(String("I am called"),ILI9341_GREEN);
      clearRxMsg();    //clear rx buffer to wait for the next command
      boxState=1;
    }
    break;
    
  case 1:  //mycall detected so ask who's calling by keying "qrz?"
    sendText("QRZ?");
    boxState=2;
    break;  
    
  case 2:  //waiting 3s for senders call. Take every signs in that time as senders call. Must be optimized
    delay(WAITFORSENDERSCALL);  //wait some time to give the sender a chance to send his call
    readCwMessage();            //read rx buffer
    trimCounter=0;
    strShiftCounter=0;
    for (int i=0; i<sizeof(rxMsg)-1;i++){
      if (rxMsg[i]!=' '){
        if (trimCounter>3){ //remove the qrz? kyed by qcx from the beginning of rxMsg to get the pure call
          yourCall[strShiftCounter]=rxMsg[i];     //and take it as sender call  
          strShiftCounter++;
        } else {
          trimCounter++;
        }
      }
    }
    yourCall[strShiftCounter+1]='\0';
    logStr("Called by "+ String(yourCall),ILI9341_WHITE);
    clearRxMsg();               //clear rx buffer
    boxState=3;     
    break;    
 
  case 3:  //sending "waiting for command"
    clearRxMsg();               //clear rx buffer
    sendText("CMD?");
    boxState=4;
    processTimer = millis();
    break;  

  case 4:  //waiting for senders command, this routine could be used for senders authentication
    readCwMessage(); //read rx buffer
    commandStr="";
    // looking for a command in rx string
    if (isContainedInRx("SEND") || isContainedInRx("SND")){  // a new message shall be stored
      commandStr="send";
    } 
    else if (isContainedInRx("LIST") || isContainedInRx("LST")){  // a stored message shall be retrieved
      commandStr="list";
    }
    if ((commandStr!="")||((millis()-processTimer)>PROCESSTIMEOUT)){   //a valid command is found or timeout for process step 
     if (commandStr!=""){  // valid command detected
        boxState=5;   
        logStr("Command: " + String(commandStr),ILI9341_WHITE);
     } else {  //process timed out, no command detected

        sendText("73");
        logStr("Command timed out",ILI9341_WHITE);
        clearRxMsg();    //clear rx buffer to wait for the next command
        boxState=0;     
     }
     clearRxMsg();               //clear rx buffer
    }
    break;      

  case 5:  //reacting on command 
     sendText("K");
     messageReadTimer = millis();
      if (commandStr=="send"){ //sender wants something to be stored
        while (((millis()-messageReadTimer)< MESSGAEREADTIME) && (!isContainedInRx("SK"))){ // wait until received string contains "SK" or timeout
          readCwMessage();            //read rx buffer
          delay(READRXFROMQCX);
        }
        if (isContainedInRx("SK")){ //message to store is completed so store it  
          storedMsg[currentMsgNo][0]='d';  //start the stored message with "de"
          storedMsg[currentMsgNo][1]='e';
          storedMsg[currentMsgNo][2]=' ';
          for (int i= 0; i<strShiftCounter; i++){  // now add senders call 
            storedMsg[currentMsgNo][i+3]= yourCall[i];   
          }
          storedMsg[currentMsgNo][strShiftCounter+4]=' '; //add seperation space
          tempi=firstNotEmptyPosInRx();
          
          for (int i= tempi+1; i<MESSAGELENGTH-1; i++){  //now copy the real message messagestore and get rid of all leading spaces but leave out the first character as this is 'k' from former transmission
            tempj= i-tempi+strShiftCounter+5;
            storedMsg[currentMsgNo][tempj]= rxMsg[i];   
          }
          storedMsg[currentMsgNo][tempj+1]='\0';
          sendText("MSG NO "+ String(currentMsgNo) + " 73 ");
          currentMsgNo++;
          if (currentMsgNo>NUMBEROFMESSAGES-1){
            currentMsgNo=0;
          }

        } else {  //timed out
          sendText("73"); //say goodbye
          readCwMessage();            //read rx buffer
          clearRxMsg();    //clear rx buffer to wait for the next command

        }
        
      } 
      else if (commandStr=="list"){ //sender wants to read something from box
        messageReadTimer = millis();
        readNoFound = false; 
        while (((millis()-messageReadTimer)< MESSGAEREADTIME) && (readNoFound==false)){ // wait until timeout or number found
          readCwMessage(); //read rx buffer
          msgNoToSend=firstNumberInRx(); // the first digit found in the rxMsg is taken as storage number to be keyed
          if ((msgNoToSend>=0)&&(msgNoToSend<=NUMBEROFMESSAGES)){ //there is a digit in rxMsg that fits to a message storage
            readNoFound=true;
          }
          delay(READRXFROMQCX);
        }
        if (readNoFound){ //number found, lets send this message
          if (msgIsEmpty(msgNoToSend)==true){ //the called message number is empty
            sendText("NO MSG 73");
          } else {
            sendText(storedMsg[msgNoToSend]);          
          }
        } else {  //timed out
          sendText("73"); //say goodbye
          readCwMessage();            //read rx buffer
          clearRxMsg();    //clear rx buffer to wait for the next command
        }
      }
      clearRxMsg();               //clear rx buffer
      boxState=0;     
    
      break;  
    }
}
