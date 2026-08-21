/*
  M5-ATEM-TarryLight
  Copyright (c) 2020 Jun SUZUKI
  Licensed under the MIT License.
*/

#include <M5StickC.h>
#include <WiFi.h>
#include <SkaarhojPgmspace.h>
#include <ATEMbase.h>
#include <ATEMstd.h>


IPAddress switcherIp(192, 168, 24, 210);       // IP address of the ATEM switcher
ATEMstd AtemSwitcher;


// http://www.barth-dev.de/online/rgb565-color-picker/
#define GRAY  0x0020 //   8  8  8
//#define GREEN 0x0200 //   0 64  0
#define RED   0xF800 // 255  0  0


#define BTN_A_PIN 37
#define BTN_B_PIN 39
#define LED_PIN   10


#define LED_ON  LOW
#define LED_OFF HIGH


const char* ssid = "APSSID";
const char* password =  "**password**";


int cameraNumber = 1;

int last_value = 0;
int cur_value = 0;


int PreviewTallyPrevious = 1;
int ProgramTallyPrevious = 1;


int connect_cnt = 0;


double vbat = 0.0;
int8_t bat_charge_p = 0;


int disp_mode = 0;


void setup() {


  Serial.begin(115200);


  // initialize the M5StickC object
  M5.begin();
  delay(10);
  
  Serial.println();
  Serial.println();
  M5.Lcd.setRotation(3); // BtnB is on top.


  Serial.print("Connecting to ");
  M5.Lcd.println("Connecting to ");
  M5.Lcd.println(ssid);
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
    M5.Lcd.print(".");  
    connect_cnt++;


    if(connect_cnt>30){
      break;
    }
  }


  delay(10);
  if(WiFi.status() == WL_CONNECTED) { 
    M5.Lcd.println("");
    M5.Lcd.println("ATEM Mini Pro Tarry");
    M5.Lcd.print("IPAddr:");
    M5.Lcd.println(WiFi.localIP());
  } else {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(1, 1);
    M5.Lcd.println("Connection Failed.");  
    M5.Lcd.print("macAddr:");
    M5.Lcd.print(WiFi.macAddress());
    
  }


  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
  pinMode(LED_PIN,   OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);
   
  delay(60);
  AtemSwitcher.begin(switcherIp);
  AtemSwitcher.serialOutput(0x80);
  AtemSwitcher.connect();


}




void show_info(){


    M5.Lcd.setRotation(3); 
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(1, 1);
    M5.Lcd.println("ATEM Mini Pro Tarry");
    M5.Lcd.print("IPAddr:");
    M5.Lcd.println(WiFi.localIP());
    M5.Lcd.print("macAddr:");
    M5.Lcd.println(WiFi.macAddress());
    
    vbat = M5.Axp.GetVbatData() * 1.1 / 1000;
    bat_charge_p = int8_t((vbat - 3.0) / 1.2 * 100);
    if(bat_charge_p > 100){
      bat_charge_p = 100;
    }else if(bat_charge_p < 0){
      bat_charge_p = 0;
    }
    M5.Lcd.printf("Charge: %3d%%", bat_charge_p); 




  
}




void loop() {


  M5.update();


  if(M5.BtnA.wasPressed()){
    if(disp_mode == 0){
      cameraNumber++;
      if(cameraNumber>4) cameraNumber = 1;
      drawLabel(WHITE, GRAY, LED_OFF);
    }
  }


  if(M5.BtnB.wasPressed()){
    if(disp_mode == 0){
      disp_mode=1;
      M5.Lcd.fillScreen(BLACK);
    } else {
      disp_mode=0;
      PreviewTallyPrevious = 1;
      ProgramTallyPrevious = 1;
      drawLabel(WHITE, GRAY, LED_OFF);
    }
  }
  if(disp_mode == 1 ){
      show_info();   
  }


  if(M5.BtnB.pressedFor(2000)){
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(1, 1);
    M5.Lcd.println("ReConncet");
       
  }


  // Check for packets, respond to them etc. Keeping the connection alive!


  AtemSwitcher.runLoop();


  int ProgramTally = AtemSwitcher.getProgramTally(cameraNumber);
  int PreviewTally = AtemSwitcher.getPreviewTally(cameraNumber);


  if ((ProgramTallyPrevious != ProgramTally) || (PreviewTallyPrevious != PreviewTally)) { // changed?


    if ((ProgramTally && !PreviewTally) || (ProgramTally && PreviewTally) ) { // only program, or program AND preview
      drawLabel(RED, BLACK, LOW);
    } else if (PreviewTally && !ProgramTally) { // only preview
      drawLabel(GREEN, BLACK, HIGH);
    } else if (!PreviewTally || !ProgramTally) { // neither
      drawLabel(WHITE, GRAY, HIGH);
    }


  }


  ProgramTallyPrevious = ProgramTally;
  PreviewTallyPrevious = PreviewTally;


  delay(100);
}


void drawLabel(unsigned long int screenColor, unsigned long int labelColor, bool ledValue) {
  M5.Lcd.setRotation(0);
  digitalWrite(LED_PIN, ledValue);
  M5.Lcd.fillScreen(screenColor);
  M5.Lcd.setTextColor(labelColor, screenColor);
  M5.Lcd.drawString(String(cameraNumber), 15, 40, 8);
}
