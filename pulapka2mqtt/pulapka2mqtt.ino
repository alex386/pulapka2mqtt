/*
 Live mole trap for ESP8266 program
 Author: Aleksander Dawid
 Date: 30.09.2019 
 */

ADC_MODE(ADC_VCC);
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include "FS.h"
#include "AESLib.h"

AESLib aesLib;

#ifndef APSSID
#define APSSID "MTrap"
#define APPSK  "mtrapsetup"
#endif

const char *ssid = APSSID;
const char *password = APPSK;
const char *s1,*s2,*ipmqtt,*umqtt,*pmqtt,*topicmqtt;
String ssidval,passval,ipval,umval,pmval,tmval;
char lsid[20],lpass[20],ipm[20],um[20],pm[20],tm[20];


boolean RunSetup=true;

ESP8266WebServer server(80);

int timezone = 2;
int dst = 0;

time_t now;

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

WiFiClient wifiClient;
PubSubClient client(wifiClient);

int tries=50;
boolean connectioWasAlive = true;

/*===================================================================*/
/* AES Encryption of data in SPIFFS                                  */
/*===================================================================*/
char cleartext[128];
char ciphertext[512];

// AES Encryption Key
byte aes_key[] = { 0x15, 0x2E, 0x7E, 0xC6, 0x28, 0xAE, 0xA2, 0xD6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x2C };
byte aes_iv[N_BLOCK] = { 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };

byte enc_iv[N_BLOCK] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }; 
byte dec_iv[N_BLOCK] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

// Generate IV (once)
void aes_init() {
  aesLib.gen_iv(aes_iv);
  encrypt("ABCDEFGIJKLMN", aes_iv);
}

String encrypt(char * msg, byte iv[]) {  
  int msgLen = strlen(msg);
  char encrypted[2*msgLen];
  aesLib.encrypt64(msg, encrypted, aes_key, iv);  
  return String(encrypted);
}

String decrypt(char * msg, byte iv[]) {
  unsigned long ms = micros();
  int msgLen = strlen(msg); 
  char decrypted[msgLen]; 
  aesLib.decrypt64(msg, decrypted, aes_key, iv);  
  return String(decrypted);
}

/*===================================================================*/
/* GET METHOD HTTP                                                  */
/*===================================================================*/
void handleRoot() {  
  String sout="";
  sout +="<HTML><HEAD>";
  sout +="<meta name=viewport content=\"width=device-width, initial-scale=1\">";
  sout +="<STYLE>";
  sout +="H1 {font: bold 34px Verdana,Arial; color: green;}";
  sout +="TD,INPUT {font: bold 24px Verdana,Arial;}";
  sout +="INPUT.send {";
  sout +="font: bold 54px Verdana,Arial;"; 
  sout +="border-radius: 15px 15px 15px 15px;"; 
  sout +="background-color: #AEAEFF;";
  sout +="}";
  sout +="</STYLE></HEAD>";
  sout +="<BODY bgcolor=antiquewhite>";   
  sout +="<h1 align=center>Mole trap setup</h1>";        
  sout +="<FORM METHOD=POST ACTION=setup>";
  sout +="<TABLE align=center>";
  sout +="<TR><TD colspan=2 align=center>Router</TD></TR>";
  sout +="<TR><TD>SSID: </TD><TD><INPUT TYPE=text NAME=SSID SIZE=14 MAXLENGTH=16></INPUT></TD></TR>";
  sout +="<TR><TD>PASS: </TD><TD><INPUT TYPE=password NAME=PASS SIZE=14 MAXLENGTH=16></INPUT></TD></TR>";
  sout +="<TR><TD colspan=2 align=center>Event server</TD></TR>";
  sout +="<TR><TD>IP: </TD><TD><INPUT TYPE=text NAME=IP SIZE=14 MAXLENGTH=16></INPUT></TD></TR>";
  sout +="<TR><TD>USER: </TD><TD><INPUT TYPE=text NAME=USER SIZE=14 MAXLENGTH=16></INPUT></TD></TR>";
  sout +="<TR><TD>PASS: </TD><TD><INPUT TYPE=password NAME=EPASS SIZE=14 MAXLENGTH=16></INPUT></TD></TR>";
  sout +="<TR><TD>TOPIC: </TD><TD><INPUT TYPE=text NAME=TOPIC SIZE=14 MAXLENGTH=16></INPUT></TD></TR>";
  sout +="<TR><TD colspan=2 align=center><br><INPUT class=send TYPE=submit value=\"SEND\"></INPUT></TD></TR>";
  sout +="</TABLE></FORM></BODY></HTML>";
  server.send(200, "text/html", sout);
}
/*===================================================================*/
void ResetConfig(){
  if(SPIFFS.begin()){
    Serial.println("Try to remove file mtrap.txt");
    if (SPIFFS.exists("/mtrap.txt")) {
         SPIFFS.remove("/mtrap.txt");
         Serial.println("File mtrap.txt removed");
    }
  }
}
/*===================================================================*/
boolean ReadSetupFile(){
   bool chFileSys = SPIFFS.begin();
   if(chFileSys==true){
    File f = SPIFFS.open("/mtrap.txt", "r");
    if (!f) {     
       return true;
     }else{    
      String s=f.readStringUntil('\n'); 
      sprintf(ciphertext, "%s", s.c_str()); ssidval = decrypt(ciphertext, dec_iv);  ssidval.trim();
      
      s=f.readStringUntil('\n');
      sprintf(ciphertext, "%s", s.c_str()); passval = decrypt(ciphertext, dec_iv);  passval.trim();
        
      s=f.readStringUntil('\n');
      sprintf(ciphertext, "%s", s.c_str()); ipval = decrypt(ciphertext, dec_iv);  ipval.trim();
         
      s=f.readStringUntil('\n');
      sprintf(ciphertext, "%s", s.c_str()); umval = decrypt(ciphertext, dec_iv);  umval.trim();
          
      s=f.readStringUntil('\n');
      sprintf(ciphertext, "%s", s.c_str()); pmval = decrypt(ciphertext, dec_iv);  pmval.trim();
      
      s=f.readStringUntil('\n'); 
      sprintf(ciphertext, "%s", s.c_str()); tmval = decrypt(ciphertext, dec_iv);  tmval.trim();
           
      f.close();
     }
   
   }else{
    return true;
   }  
  return false;
}

/*===================================================================*/
/* POST METHOD HTTP                                                  */
/*===================================================================*/
void handleSetup() {
    String infout="";
    infout +="<HTML><HEAD>";
    infout +="<meta name=viewport content=\"width=device-width, initial-scale=1\">";
    infout +="<STYLE>";
    infout +="H1 {font: bold 28px Verdana,Arial; color: green;}";
    infout +="</STYLE></HEAD>";
    infout +="<BODY bgcolor=antiquewhite>"; 
    
  if (server.args() > 0){
   
    for (int i = 0; i < server.args();i++){
        if(server.argName(i)=="SSID"){
          ssidval=server.arg(i);
        }
        
        if(server.argName(i)=="PASS"){
          passval=server.arg(i);
        }

        if(server.argName(i)=="IP"){
          ipval=server.arg(i);
        }

        if(server.argName(i)=="USER"){
          umval=server.arg(i);
        }

        if(server.argName(i)=="EPASS"){
          pmval=server.arg(i);
        }

        if(server.argName(i)=="TOPIC"){
          tmval=server.arg(i);
        }
     
   } 

    
   bool chFileSys = SPIFFS.begin();
   if(chFileSys==true){
   
    File f = SPIFFS.open("/mtrap.txt", "w");
    
    sprintf(cleartext,"%-20s", ssidval.c_str()); 
    String encrypted = encrypt(cleartext, enc_iv);      
    f.println(encrypted); 

    sprintf(cleartext,"%-20s", passval.c_str()); 
    encrypted = encrypt(cleartext, enc_iv);      
    f.println(encrypted);

    sprintf(cleartext,"%-20s", ipval.c_str()); 
    encrypted = encrypt(cleartext, enc_iv);      
    f.println(encrypted); 
     
    sprintf(cleartext,"%-20s", umval.c_str()); 
    encrypted = encrypt(cleartext, enc_iv);      
    f.println(encrypted); 
   
    sprintf(cleartext,"%-20s", pmval.c_str()); 
    encrypted = encrypt(cleartext, enc_iv);      
    f.println(encrypted); 
   
    sprintf(cleartext,"%-20s", tmval.c_str()); 
    encrypted = encrypt(cleartext, enc_iv);      
    f.println(encrypted); 
       
    f.close();
    delay(200);
   }else{
    Serial.println("NOT Started SPIFFS ");
   }

  infout +="<h1 align=center>Configuration saved</h1>";      
  infout +="<h1 align=center>Restart in 10s ...</h1>"; 
   
  }else{
    infout +="<h1 align=center>No arguments try setup again in 10s</h1>";
  }

   infout +="</BODY></HTML>";
   server.send(200, "text/html", infout);
   delay(5000);
   ESP.deepSleep(10e6); 
}
/*===================================================================*/
void APwifi(){
  
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  
  server.on("/", handleRoot);
  server.on("/setup", handleSetup);
  
  server.begin();  
}
/*===================================================================*/
String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}
/*===================================================================*/
void setup() {

  Serial.begin(115200);
  Serial.println();

  aes_init();
  delay(5000);
 
 
  pinMode(4,INPUT);
 

  int stat4=digitalRead(4);
  if(stat4==0){
   ResetConfig();
    pinMode(2,OUTPUT);
    digitalWrite(2,HIGH);
    delay(100);
    digitalWrite(2,LOW);
    delay(100);
    digitalWrite(2,HIGH);
    delay(100);
    digitalWrite(2,LOW);
    delay(100);
    digitalWrite(2,HIGH);
    delay(100);
    digitalWrite(2,LOW);
  }
 
  if(RunSetup=ReadSetupFile()){
    Serial.println("Need to do setup again");
  }else{
    Serial.println("Ready to wait for mole...");
  }

  
  if(RunSetup){
    APwifi();
  }else{

   WiFi.mode(WIFI_STA);
   
  sprintf(lsid, "%s", ssidval.c_str());
  sprintf(lpass, "%s", passval.c_str());
  sprintf(ipm, "%s", ipval.c_str());
  sprintf(um, "%s", umval.c_str());
  sprintf(pm, "%s", pmval.c_str());
  sprintf(tm, "%s", tmval.c_str());

   s1 = lsid;
   s2 = lpass;
   ipmqtt=ipm;
   umqtt=um;
   pmqtt=pm;
   topicmqtt=tm;


    WiFi.begin(s1, s2);
    client.setServer(ipmqtt, 1883);
    client.setCallback(callback);
  
  }
  
  
}
/*===================================================================*/
void ConnectWiFi()
{
   
  if(tries==0){
   Serial.println("Not able to connect wait deep sleep 600s");
   ESP.deepSleep(600e6);  
  }
  

  if (WiFi.status() != WL_CONNECTED)
  {
    if (connectioWasAlive == true)
    {
      Serial.printf("Try connect to %s\n", WiFi.SSID().c_str());
      connectioWasAlive = false;
    }
    Serial.print(".");
    delay(400);
    tries--;
  }
  else if (connectioWasAlive == false)
  {
    
    
  connectioWasAlive = true;
  Serial.printf(" connected to %s\n", WiFi.SSID().c_str());

  String clientName;
  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);


  configTime(timezone * 3600, dst, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print("x");
    delay(1000);
  }
  delay(10);
  now = time(nullptr);
  
  while(now<50000){
    Serial.print("?");  
    now = time(nullptr);
    delay(10);
  }
  Serial.println("*");
  
  
  if (client.connect((char*) clientName.c_str(),umqtt,pmqtt)) {
    Serial.println("Connected to MQTT broker");
  }
  else {
    Serial.println("Not able to connect wait deep sleep 600s");
    ESP.deepSleep(600e6);
  }


    int nap1 = ESP.getVcc()/1000;
    int nap2 = ESP.getVcc() - nap1*1000;

    char volt[6];
    snprintf(volt,6,"%d.%d",nap1,nap2);
    
    String stime=ctime(&now);
    stime.trim();
    Serial.println(stime);
  
  String payload = "{\"status\":";
  payload += "\"active\"";
  payload += ",\"voltage\":";
  payload += volt;
  payload += ",\"time\":";
  payload += "\""+stime+"\"";
  payload += "}";
  
  if (client.connected()){
    
    if (client.publish(topicmqtt, (char*) payload.c_str(),true)) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
      
    }
  }
  delay(10);
  
  Serial.println("I'm going into deep sleep mode for ever");
  ESP.deepSleep(0);
  }else{
    Serial.printf("SOME ERROR == WL_CONNECTED\n");
    connectioWasAlive = false;
  }
}
    
/*===================================================================*/

void loop() {
  if(RunSetup){
    server.handleClient();
  }else{
    ConnectWiFi();
  }
  
}
