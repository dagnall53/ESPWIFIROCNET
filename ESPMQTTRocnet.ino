//Compiler Directives and code options are in Directives.h

#include "Directives.h";

//----DO NOT FORGET TO UPLOAD THE SKETCH DATA ---
//   To check the code is working, in command prompt, set up a MQTT "debug" monitor: (e.g. For MQTT broker at 192.18.0.18) "CD C:\mosquitto  mosquitto_sub -h 192.168.0.18 -i "CMD_Prompt" -t debug -q 0" 

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#ifdef _Use_Wifi_Manager
       #include <WiFiManager.h>
#else
       #include "Secrets.h";
       String wifiSSID = SSID_RR;
       String wifiPassword = PASS_RR; 
#endif
#include <ESP8266WiFi.h>
/*
WiFiClient espClient;
PubSubClient client(espClient); in MQTT now
*/
#include <Servo.h>
#include <EEPROM.h>

// adding FTP support!//https://github.com/nailbuster/esp8266FTPServer
#include <ESP8266FtpServer.h> 

FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial
//FTP support
uint8_t wifiaddr;
uint8_t ip0;
uint8_t ip1;
uint8_t    subIPH;
uint8_t    subIPL;

IPAddress ipBroad;
IPAddress mosquitto;
int SDelay[12];
uint32_t LoopTimer;
uint32_t LocoCycle;
uint16_t MyLocoAddr ;
uint8_t Loco_motor_servo_demand = 0 ;
uint8_t Loco_servo_last_position;
bool Last_Direction;
bool Last_Set_Dir;

#include "Globals.h"
#include "Subroutines.h";
#include "MQTT.h"; //new place for mqtt stuff
#include "RocSUBS.h";
#include "Ports.h";

#ifdef _Audio
#include "Chuff.h";  
#endif

#ifdef _RFID
 #include "RFID_Subs.h";
#endif

extern  uint32_t Motor_Setting_Update_Time;

void ConnectionPrint() {
  Serial.println("");
  Serial.println(F("---------------------------Connected-----------------------"));
  Serial.print (F(" Connected to SSID:"));
  Serial.print(WiFi.SSID());
  Serial.print(F("  IP:"));
  Serial.println(WiFi.localIP());
  //Serial.println("-----------------------------------------------------------");      
 
}





void Status(){

  Serial.println();Serial.println();
  Serial.println(F("-----------------------------------------------------------"));
#ifdef _LOCO_SERVO_Driven_Port
#ifdef _Audio
  Serial.println(F("             ESP8266 MQTT Rocnet Loco decoder with Sound    ")); 
  #else 
  Serial.println(F("             ESP8266 MQTT Rocnet Loco decoder     ")); 
  #endif
#else
  #ifdef _Audio
  Serial.println(F("             ESP8266 MQTT Rocnet Static Node with Sound    ")); 
  #else 
  Serial.println(F("             ESP8266 MQTT Rocnet Static Node     ")); 
  #endif
#endif  
  Serial.println(F("-----------------------------------------------------------"));
  Serial.print(F(  "                    revision:"));
  Serial.print(SW_REV); Serial.println();
  Serial.println(F("-----------------------------------------------------------"));
  WiFi.setOutputPower(0.0); //  0 sets transmit power to 0dbm to lower power consumption, but reduces usable range.. try 30 for extra range

#ifdef _Use_Wifi_Manager
   WiFiManager wifiManager;  // this  stores ssid and password invisibly  !!
  //reset settings - for testing
  //wifiManager.resetSettings();
  wifiManager.autoConnect("ROCNODE ESP AP");  //ap name (with no password) to be used if last ssid password not found
#else    

  WiFi.mode(WIFI_STA);  //Alternate "normal" connection to wifi
  WiFi.setOutputPower(30);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  while (WiFi.status() != WL_CONNECTED) {delay(500);Serial.print(F("Trying to connect to {"));
  Serial.print(wifiSSID.c_str());
  Serial.println(F("} "));Serial.print(".");}
 
#endif

  //if you get here you have connected to the WiFi
  ipBroad = WiFi.localIP();
  ip0=ipBroad[0];
  ip1=ipBroad[1];
  subIPH = ipBroad[2];
  subIPL = ipBroad[3];
  wifiaddr = ipBroad[3];
  ConnectionPrint();
  ipBroad[3] = 255; //Set broadcast to local broadcast ip e.g. 192.168.0.255 // used in udp version of this program
  //   ++++++++++ MQTT setup stuff   +++++++++++++++++++++
  mosquitto[0] = ipBroad[0]; mosquitto[1] = ipBroad[1]; mosquitto[2] = ipBroad[2];
  mosquitto[3] = RN[14];                //saved mosquitto address, where the broker is! saved as RN[14],
  #ifdef myBrokerSubip; 
      mosquitto[3]= myBrokerSubip  //change to set  myBrokerSubip as your broker last ip address..(defined in secrets)..
  #endif
  Serial.print(F(" Mosquitto will first try to connect to:"));
  Serial.println(mosquitto);
  MQTT_Setup();

//  ------------------ IF rfid -------------------------
#ifdef  _RFID
 SetupRFID(); // note I have not tested this stuff recently.. 
#endif

  RocNodeID = getTwoBytesFromMessageHL(RN, 1);

#ifdef _ForceRocnetNodeID_to_subIPL
  RocNodeID =subIPL;
#endif
  
  Serial.println(F("------------------------- RocNet Node ---------------------"));
  Serial.print(F("                   My ROCNET NODE ID:"));
  Serial.println(RocNodeID);
  //  ------------------ IF rfid -------------------------
#ifdef _LOCO_SERVO_Driven_Port
  //++++++++++++++++++++Print Debug and Current setup information stuff    +++++++++++++++++++++++++++++
  Serial.println(F("---------------------- LOCO Setup   -----------------------"));
  Serial.print(F(  "          Short 'Locomotive Address' is"));
  Serial.println (MyLocoAddr);
  Serial.println();
  Serial.println(F("-------------------------- PORT Setup ---------------------"));
  Loco_motor_servo_demand = 90;
 digitalWrite (NodeMCUPinD[FRONTLight], 1);  //Turn off direction lights
 digitalWrite (NodeMCUPinD[BACKLight], 1); //Turn off direction lights
#endif


}

void _SetupOTA(String StuffToAdd){
  String Name;
  // ota stuff  Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]

 #ifdef _LOCO_SERVO_Driven_Port 
  Name="RN LOCO(";
  #else
  Name="RN STAT(";
 #endif
 Name=Name+StuffToAdd;
 Name=Name+")";
 Serial.printf("--- Setting OTA Hostname <%s> -------------\n",Name.c_str());
  Serial.printf("------------------------------------------------------\n");
 ArduinoOTA.setHostname(Name.c_str());
  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  //---------------------end ota stuff -------
   
}

void setup() {  
   Serial.begin(115200);
 // original location _SetupOTA(Nickname);
  
  Ten_Sec = 10000;

  SetPortPinIndex();  //sets up nodemcucross references 

//  ----------------------Setup from eeprom
  Data_Updated = false;
  EEPROM.begin(1024);

  if ((EEPROM.read(255) == 0xFF) && (EEPROM.read(256) == 0xFF)) { //eeprom is empty, first run. Can also set via CV[8]=8
    Serial.println(" ******* EPROM EMPTY....Setting Default EEPROM values *****");
    SetDefaultSVs();
    CV[1]=3; // set initial loco address as 3 (whatever  the setdefaults function says)
    #ifdef  _Force_Loco_Addr
      CV[1]= _Force_Loco_Addr;
    #endif
    SetWordIn_msg_loc_value(RN,1,subIPL); // sets initial RocNode address as the subip to prevent clashes
    WriteEEPROM();
    EPROM_Write_Delay = millis() + Ten_Sec;
    EEPROM.commit();
    delay(100);
  } 
  #ifdef _ForceDefaultEEPROM
    SetDefaultSVs();
    CV[1]=3; // set initial loco address as 3 (whatever  the setdefaults function says)
    SetWordIn_msg_loc_value(RN,1,subIPL); // sets initial RocNode address as the subip to prevent clashes
    #ifdef  _Force_Loco_Addr
      CV[1]= _Force_Loco_Addr;
    #endif
    WriteEEPROM();
    EPROM_Write_Delay = millis() + 1000;
    EEPROM.commit();
  #endif
  
  //if
  // Get the stored / initial values...

  ReadEEPROM();     //set the RN and CV registers

  
  #ifdef _LOCO_SERVO_Driven_Port 
   Pi03_Setting_options[_LOCO_SERVO_Driven_Port] = 32 + 10; // KEEP this i/o as a "SERVO" output regardless, 10= delay to use for servo changes = 100ms rate ;
  #ifdef _LocoPWMDirPort
   Pi03_Setting_options[_LOCO_SERVO_Driven_Port] = 128 + 10; // Set this i/o as a "PWM" output regardless, 10= delay to use for servo changes = 100ms rate ;
   #endif  
  #endif
  PortSetupReport();  //make any port direction changes.
  ImmediateStop(); // stop motor as soon as ports set up


#ifdef  _Force_Loco_Addr
       CV[1]= _Force_Loco_Addr;
#endif 
     MyLocoAddr=CV[1]; // short address
    if bitRead( CV[29],5){MyLocoAddr = CV[18] + (((CV[17]&63) * 256));} //long address
   #ifdef  _Force_Loco_Addr   // can only force short addresses
       MyLocoAddr= _Force_Loco_Addr;
       CV[1]=_Force_Loco_Addr;
       CV[17]=0;  // need to set these to forced loco address??
       CV[18]=_Force_Loco_Addr; //??
#endif 
  CV[8] = 0x0D; // DIY MFR code
  CV[7] = SW_REV; //ver

  Status();
  _SetupOTA(Nickname); // now we should have the nickname etc
 
  

 

  Motor_Speed = 0;
  Speed_demand = 0;
  Last_Speed_demand = 0; 
  
  Loco_motor_servo_demand=90;
  Motor_Setting_Update_Time=millis();
  connects = 0;
  oldconnects = 0;

  POWERON = true;
  WaitUntill = millis();

  SensorOutput_Inactive = true;

  RFIDCycle = millis();
  LoopTimer = millis();
  LocoCycle = millis();
  EPROM_Write_Delay = millis();
 
 
  //PortSetupReport();  // sends port configuration to serial monitor
  digitalWrite (NodeMCUPinD[SignalLed], SignalON) ;  /// turn On
  // set the servos neutral here??
  // ReadInputPorts();
  for (int i = 0 ; i <= 8; i++) {
    lastButtonState[i] = digitalRead(NodeMCUPinD[i]);
    ButtonState[i] = 0; // just set them all 0 at this stage 
    PortTimingDelay[i] = millis();
    ServoOff_Delay_Until[i] = millis() + 10000;
  }
//#ifdef _AudioNoDAC  // test to see if we can reuse the LRC pin after it has been defined for audionoDAC
//digitalWrite (NodeMCUPinD[I2SDAC_LRC], LOW) ;  /// turn off blue light? (unless needed!)
//#endif
  
  ResetWiFi = false;
  MSGReflected = true;
  MsgSendTime = millis();
  lastsec = millis();
  secs=0;
  mins=0;
  hrs=0;

  LenDebugMessage = 1;
  DebugMessage[0] = 0xFF;
  for (int i = 0 ; i <= 127; i++) { // clean out debug message 
    DebugMessage[i] = 0;
  }
  // for interrupt when if/when we add them later versions...
  //attachInterrupt(pin,hiInterrupt,RISING);


  // Serial.println("------------------------ Starting main loop ---------------");
  FlashMessage(" ----------Entering Main Loop----------  ", 5, 150, 150);
     PrintTime("Start");
   CV[47]=131; //Becoming obsolete, I am not using this feature now, but there are some message shows that still use this to show MQQT messages,Serial messages and the D4 lightflashing at loop frequency (approximates to "On" when working!...
   for (int i = 0 ; i <= 8; i++) { // set servo stuff to a default.
    SDelay[i] = 1000;
    SDemand[i] = 90;
    Pi03_Setting_LastUpdated[i] = millis();
  }


#ifdef  _DefaultPrintOut  // give a printout of whats set in the eeprom..
    Serial.println("-----------------CURRENT EEPROM SETTNGS --------------");
    WriteEEPROM();
    Serial.println(" ----// end of saved EEPROM set of defaults----");
  EEPROM.commit();
  delay(1000);
#endif




 /////FTP Setup, ensure SPIFFS is started before ftp;  /////////
#ifdef ESP32       //esp32 we send true to format spiffs if cannot mount //NOT TESTED!
  if (SPIFFS.begin(true)) {
#elif defined ESP8266
  if (SPIFFS.begin()) {
#endif
      Serial.println("SPIFFS opened!");
      ftpSrv.begin("esp8266","esp8266");    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
  } 
#ifdef _Audio
  SetUpAudio(millis());
#endif
 }  /// end of setup ///////


//+++++++++++++++++++++++++++++++++++++++++++++++++MAIN LOOP++++++++++++++++++++++++++++++++++++++++++++
extern long ChuffPeriod;

void loop() {
  ftpSrv.handleFTP();        //make sure in loop you call handleFTP()!!  

    LoopTimer = millis(); // idea is to use LoopTimer instead of millis to ensure synchronous behaviour in loop

#ifdef _Audio
  AudioLoop(LoopTimer);
  SoundEffects();
  if (TimeToChuff(LoopTimer)){ Chuff("/BBCH");} // select chuff sound samples "/BBCH" is my best sounding set.. or try "/ivor_" or "/Fenchurch"  
#endif

  digitalWrite (NodeMCUPinD[SignalLed] , SignalOFF) ; ///   turn OFF signal lamp
  ArduinoOTA.handle();
  // MQTT stuff, & check we are connected.. 
  if (!MQTT_Connected()) {    // was if (!client.connected()) {
    ImmediateStop();
    connects = connects + 1;
    reconnect();
  }
  MQTT_Loop(); // for client.loop(); //gets wifi messages etc..
 // experiment in qos  
 /* if (oldconnects != connects) {
    oldconnects = connects;  char DebugMsg[120]; int cx; DebugSprintfMsgSend( sprintf ( DebugMsg, "Reconnected, total connects now:%d", connects);
    DebugMsgSend("debug", DebugMsg);
  }
  */
  // Stop the motor if you lose connection  //
  if (( MSGReflected == false) && (LoopTimer >= MsgSendTime + 200)) {
       ImmediateStop();
    MQTTSendQ1 (SentTopic, SentMessage);
    DebugSprintfMsgSend(sprintf ( DebugMsg, "*RESENDING sensor msg--  "));
  }  //pseudo QoS1 resend

//Sign of life flash
  if (LoopTimer >= lastsec + 1000 ) {//sign of life flash 
    lastsec = lastsec + 1000;
    secs = secs + 1;
    Serial.print(".");
    digitalWrite (NodeMCUPinD[SignalLed] , SignalON) ; ///   turn On
     }
  //Switch these on and to check loop timing via oscilloscope and signal led 
  //   Phase = !Phase;
  //   digitalWrite(NodeMCUPinD[SignalLed], Phase);
  // 

 

  // +++++++++++++++can reset wifi on command "update node to sw 0"
  if ( ResetWiFi == true) { //reset settings - for testing purposes
#if _SERIAL_DEBUG
    Serial.println("  +++++++++++  RESETTING WiFi  +++++++++++++++  " );
    Serial.println("  +++++++++++  You will need to access the AP and browse 192.168.4.1  +++++++++++++++  " );
#endif
#ifdef _Use_Wifi_Manager
    WiFiManager wifiManager; wifiManager.resetSettings(); wifiManager.startConfigPortal("ROCNODE ESP AP"); //ap name (with no password) to be used if ssid password not stored/found
#else
  //  ESP.reset(); // needed ??
    #endif
    ResetWiFi = false; FlashMessage(" ++++++++ RESET AP Completed ++++++++ ", 10, 300, 300);

    ConnectionPrint();
  }
  // +++++++++++++++

  // +++++++++++++commit any changed writes to the  Eprom and change the ports if they have been altered..
  if ((LoopTimer >= EPROM_Write_Delay) && (Data_Updated)) {              // commit EEPROM only when needed..
    Data_Updated = false;
    DebugMsgSend("debug","Commiting EEPROM");
    Serial.println("Commiting EEPROM");

    DetachServo(-1);   // switches off ALL servos
    delay(50);
    EEPROM.commit();
    PortSetupReport();  //make any port direction changes.
    delay(50);
    // +++++++++  Set up other things that may have been changed...+++++++++
    RocNodeID = getTwoBytesFromMessageHL(RN, 1);
    //MyLocoAddr=CV[1];
    //MyLocoAddr = CV[18] + ((192-(CV[17]) * 256));
    MyLocoAddr=CV[1];
    if bitRead( CV[29],5){MyLocoAddr = CV[18] + (((CV[17]&63) * 256));}
       #ifdef  _Force_Loco_Addr
       MyLocoAddr= _Force_Loco_Addr;
       CV[1]=_Force_Loco_Addr;
       CV[17]=0;
       CV[18]=_Force_Loco_Addr;
#endif 
    Serial.println(F("---------------------- LOCO Setup   -----------------------"));
    Serial.print(F(  "          'Locomotive Address' is"));
    Serial.println (MyLocoAddr);

#ifdef _ForceRocnetNodeID_to_subIPL
  RocNodeID =subIPL;
#endif

                                                        }
  // +++++++++++END commit to EPROM
//periodic updates and checks
#ifdef _RFID
  checkRFID();
#endif
  DoLocoMotor(); // needs checks internally for loco port existance
  SERVOS();
  FLASHING();
  ReadInputPorts();
  DETACH();     // check if servos need detaching...
  DoRocNet();   // do any messages ! includes... if (Message_Length >=1)

  //delay(5);   // slow this down for tests
} //void loop



