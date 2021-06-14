#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

//define general constants
#define RXBUFFERLEN 100

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

//define button pins
#define BTN1 13
#define BTN2 14

// define timeout values 
#define SCREENREFRESH 1000
unsigned long rxDisplayStartTime = 0;
#define READRXFROMQCX 1000
unsigned long readFromQcxStartTime = 0;
#define WAITFORSENDERSCALL 9000


//define control variables
char* myCall = "DL9BDI";
char yourCall[100] ="";
char command[10] ="";

int strShiftCounter; 
int boxState = 0; /* controls state of qcx box and defines the flow: 
                    0: idle 
                    1: own call received, asking for sender call
                    2: sender call detected, wating for next command
                    3: asking sender for command
                    4: send-command detected, receiving message until sk
                    5: sk received, message complete
                  */

// message variables
char rxBuff[50];     // buffer for reading out decoded cw from qcx 
char keyBuff[4];     // buffer for checking if qcx is being keyed.
char rxTmpMsg[50];   // decodced message
char rxMsg[RXBUFFERLEN];
char txMsg[40];




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
  //Serial.print("*********************");
  //Serial.println(rxMsgStr);

  //erase area on display
  //tft.fillRect(0,50,tft.width(),160,ILI9341_BLACK);
  //print rx message 
  tft.setCursor(0, 50);
  tft.setTextColor(ILI9341_WHITE,ILI9341_BLACK);  
  tft.setTextSize(2);
  tft.println(rxMsgStr);
}


// 
//retrieve qcx status
//
void getQcxStatus(){
  Serial.println("QCX ist bereit");
  Serial2.write("FA;");
  Serial2.readBytesUntil(';',rxMsg,39);
  tftPrint(0,20,rxMsg, ILI9341_GREEN, 1);

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
// initialize qcx box
//
void setup() {
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tftPrint(0,0,"QCX Box", ILI9341_WHITE, 2);

  Serial.begin(115200);
  Serial2.begin(38400, SERIAL_8N1, RXD2, TXD2);
  //Serial.println("Serial Txd is on pin: "+String(TX));
  //Serial.println("Serial Rxd is on pin: "+String(RX));
  //tftPrint(0,20,"Serial Txd is on pin: "+String(TXD2), ILI9341_GREEN, 1);
  //tftPrint(0,35,"Serial Rxd is on pin: "+String(RXD2), ILI9341_GREEN, 1);
  //tftPrint(0,50,"Serial paramet: 38400, SERIAL_8N1, RXD2, TXD2", ILI9341_GREEN, 1);
  if (Serial2.available()) {
    tftPrint(0,65,"Sending FA00007004000; to Seria  l2", ILI9341_GREEN, 1);  
    Serial2.print('FA00007004000;');
  
  }
  getQcxStatus();
  clearRxMsg();  //clear rx buffers at startup
  
  //initialize some values
  rxDisplayStartTime=millis();
  readFromQcxStartTime=millis();
}

//
// read cw messages decoded by qcx into global rx string
//
void readCwMessage(){
  int txBytes;         // no of bytes to be sent if qcx is still in transmission mode
  int rxLen;           // no of bytes in response
  char rxLenTmp[3];       // tmporary for number conversion
  
  Serial2.write("TB;"); // sending read command to qcx 
  //Serial.println("sending TB; in readcw");
  //if (Serial2.available()) {
  Serial2.readBytesUntil(';',rxBuff,39); // reading qcx response
    Serial.print("rxBuff: ");
    Serial.println(rxBuff);
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
      Serial.print("rxMsg: ");
      Serial.println(rxMsg);

      /*
      Serial.print("txbytes: ");
      Serial.println(txBytes);
      Serial.print("rxLen: ");
      Serial.println(rxLen);
      Serial.print("rxTmpMsg: ");
      Serial.println(rxTmpMsg);
      Serial.print("rxMsg: ");
      Serial.println(rxMsg);
      Serial.println("----------------------------------");
      */
    }
  //}  
  
 
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
//checks keying is in progress
//
boolean isKeying(){
  Serial2.write("KY;"); // sending kying request command to qcx 
  Serial2.readBytesUntil(';',keyBuff,3); // reading qcx response
  Serial.print("keyBuff: ");
  Serial.println(keyBuff);
  keyBuff[3]='\0';
  if (keyBuff[2]=='2'){ //qcx answer KY2 says no messages being sent
    return false;
  } else {
    return true; 
  }

}


void loop() {
 // possibilty to control routines with a button down.
 if (digitalRead(BTN1)==LOW){
  Serial.println("Button 1 pressed");
  //readCwMessage();
 }

  //if time for screenrefresh has passed, display rx message
  if ((millis()-rxDisplayStartTime)>SCREENREFRESH){
    tftPrintRxMsg();
    rxDisplayStartTime=millis(); //reset screentimer
  }

  //if time for qcx reading has passed, ask it
  /*
   * 
   /*if ((millis()-readFromQcxStartTime)>READRXFROMQCX){
    readCwMessage();
    readFromQcxStartTime=millis(); //reset screentimer
  }
  */
   int trimCounter=0; // counter to remove keyed characters from the beginning of the rxMsg 
 // start the state machine to handle message receiving and sending
 switch (boxState){
  case 0:  // box is ideling and listening
    if ((millis()-readFromQcxStartTime)>READRXFROMQCX){ // //if time for qcx reading has passed, ask it
      readCwMessage();
      readFromQcxStartTime=millis(); //reset screentimer
    }
    if (isContainedInRx(myCall)){  //callsign found
      tftPrint(0,150,"I am called", ILI9341_WHITE, 2);
      clearRxMsg();    //clear rx buffer to wait for the next command
      boxState=1;
    }
    break;
  case 1:  //mycall detected so ask who's calling by keying "qrz?"
    Serial2.write("KY QRZ?;"); 
    boxState=2;
    break;  
  case 2:  //waiting 3s for senders call. Take every signs in that time as senders call. Must be optimized

    Serial.println("Vor dem Warten");
    delay(WAITFORSENDERSCALL);  //wait some time to give the sender a chance to send his call
    Serial.println("Nach dem Warten");

    readCwMessage();            //read rx buffer
    Serial.print("rxMsg: ");
    Serial.println(rxMsg);
    trimCounter=0;
    strShiftCounter=0;
    for (int i=0; i<sizeof(rxMsg)-1;i++){
      /*Serial.print("i: ");
      Serial.print(i);
      Serial.print(" ");
      Serial.println(rxMsg[i]);
      */

      if (rxMsg[i]!=' '){
        if (trimCounter>3){ //remove the qrz? kyed by qcx from the beginning of rxMsg to get the pure call
          yourCall[strShiftCounter]=rxMsg[i];     //and take it as sender call  
          strShiftCounter++;
        } else {
          trimCounter++;
        }
        
         /*Serial.print("yourCallCounter: ");
         Serial.print(yourCallCounter);
         Serial.print(" ");
         Serial.println(yourCall[yourCallCounter]);
         */
      }
    }
    yourCall[strShiftCounter+1]='\0';
    tftPrint(0,170,"Called by", ILI9341_WHITE, 2);
    tftPrint(130,170,yourCall, ILI9341_WHITE, 2);

    Serial.print("yourCall: ");
    Serial.print(yourCall);
    Serial.print(" sizeof: ");
    Serial.println(sizeof(yourCall));
    clearRxMsg();               //clear rx buffer
    boxState=3;     
    break;    
 
  case 3:  //sending "waiting for command"
    Serial2.write("KY CMD?;"); 
    boxState=4;
    break;  
  case 4:  //waiting 3s for senders command. 

    delay(WAITFORSENDERSCALL);  //wait some time to give the sender a chance to send his command
    readCwMessage();            //read rx buffer
    Serial.print("rxMsg: ");
    Serial.println(rxMsg);
    trimCounter=0;

      trimCounter=0;
      strShiftCounter=0;
      for (int i=0; i<sizeof(rxMsg)-1;i++){
        /*Serial.print("i: ");
        Serial.print(i);
        Serial.print(" ");
        Serial.println(rxMsg[i]);
        */
  
        if (rxMsg[i]!=' '){
          if (trimCounter>3){ //remove the cmd? kyed by qcx from the beginning of rxMsg to get the pure command
            command[strShiftCounter]=rxMsg[i];     //and take it as sender call  
            strShiftCounter++;
          } else {
            trimCounter++;
          }
          
           /*Serial.print("yourCallCounter: ");
           Serial.print(yourCallCounter);
           Serial.print(" ");
           Serial.println(yourCall[yourCallCounter]);
           */
        }
      }
    command[strShiftCounter+1]='\0';
    tftPrint(0,190,"Command", ILI9341_WHITE, 2);
    tftPrint(100,190,command, ILI9341_WHITE, 2);
    Serial.print("commamd: ");
    Serial.println(command);
   
    clearRxMsg();               //clear rx buffer
    boxState=0;     
    break;      
    }
}
