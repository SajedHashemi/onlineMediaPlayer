#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "OnlinePlayer.h"
#include <QList.h>

OnlinePlayer op;

String STASSID = "ODIN";
String STAPASS = "odin2022!@#";

int sNumber = -1;
int aCounter = 0;
int vCounter = 0;

bool stopAfter = true;
bool playNext = true;

QList<String> fList;

bool isNumeric(String str){
  if(str.length()==0) return false;
  for(byte v=0;v<str.length();v++)
    if(str[v]<'0' || str[v]>'9') return false;
  return true;
}

void setup() {
  Serial.begin(9600);

  op.begin(TFT_PORTRATE, 26, -10);
  op.writeText("Init...");
  
  op.writeText(String("Connect to STA(" + STASSID + ")..."));
  WiFi.begin(STASSID.c_str(), STAPASS.c_str());
  while(WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  op.setURL("192.168.2.2:3660/data?file=");

  op.writeText("Get list of audios...");
  Serial.println("Get list of audios...");
  String aList = op.getFilesList("audios");
  while(aList!=""){
    byte lfpos = aList.indexOf("\n");
    if(lfpos>0){
      String m = aList.substring(0, lfpos);
      Serial.printf("%d> %s\r\n",fList.size()+1, m.c_str());
      fList.push_back(m);
      aCounter++;
    }
    aList = aList.substring(lfpos+1);
  }
  Serial.printf("Number of audios: %d\r\n",aCounter);

  op.writeText("Get list of videos...");
  Serial.println("\r\nGet list of videos...");
  aList = op.getFilesList("videos");
  while(aList!=""){
    byte lfpos = aList.indexOf("\n");
    if(lfpos>0){
      String m = aList.substring(0, lfpos);
      Serial.printf("%d> %s\r\n",fList.size()+1, m.c_str());
      fList.push_back(m);
      vCounter++;
    }
    aList = aList.substring(lfpos+1);
  }
  Serial.printf("Number of videos: %d\r\n",vCounter);
  Serial.printf("Number of Files: %d\r\n",fList.size());
  
  if(op.isConnected) op.writeText("Ready...");
  op.FFTEnable = true;
}

void loop(){
  if(Serial.available()){
    delay(100);
    String cmd = Serial.readString();
    cmd.trim();
    if(cmd.indexOf(">")==0){
      cmd = cmd.substring(1);
      if(cmd!=""){
        if(isNumeric(cmd)){
          sNumber = (cmd.toInt() >= fList.size())?fList.size()-1:cmd.toInt();
        }else if(cmd==">"){
          
        }else{
          byte i=0;
          for(; i<fList.size();i++) if(fList.at(i) == cmd) break;
          if(i<fList.size()) sNumber = i;
          else{
            sNumber = -1;
            op.writeText("File not found!");
          }
        }
      }else{
        sNumber = 0;
      }
      stopAfter = false;
    }
  }

  
  if(!op.isPlaying && op.isConnected && !stopAfter && sNumber>=0 && fList.size()>0) {
    Serial.printf("\r\nPlaying: %s", fList.at(sNumber).c_str());
    if(!op.playVideo(fList.at(sNumber)))
      op.playAudio(fList.at(sNumber));
    if(playNext) sNumber = (++sNumber >= fList.size())?0:sNumber;
    stopAfter = false;
  }
}
