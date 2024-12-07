#include <SPI.h>
#include "max6675.h"
#include <Encoder.h>
#include <PID_v1.h>
#include <EEPROM.h>
// #include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// #define OLED_RESET -1
// #define SCREEN_ADDRESS 0x3C
// OLED Pins
#define OLED_MOSI 17  //SDA
#define OLED_CLK 18   //SCL
#define OLED_DC 15
#define OLED_CS 14
#define OLED_RESET 16

// Solder MAX6675 Pins
#define solderThermoDO 8
#define solderThermoCS 7
#define solderThermoCLK 6

// Heater MAX6675 Pins
#define heaterThermoDO 13
#define heaterThermoCS 12
#define heaterThermoCLK 6

// Encodr Pins
#define encoderCLK 3
#define encoderDT 4
#define encoderSW 5

// PWM Out Pins
#define solderPin 9
#define fanPin 10
#define heaterPin 11

// menu Config Vals
bool openMenu = true;
bool inSubMenu = false;
short menuId = 0;
short subMenuId = 0;
short homeId = 0;

// Power vals
bool Heater_ON = false;
bool Heater_Fan_ON = false;
bool Solder_ON = false;

// fan vals
int Fan_PWM = 50;
#define Fan_Min_PWM 25
#define Fan_Max_PWM 255


// Boost Mode
bool SolderBoostMode = false;
bool defaultSolderBoostMode = false;

bool HeaterBoostMode = false;
bool defaultHeaterBoostMode = false;


// Btn vals
unsigned long btnPushTime = 0;
bool btnPush = false;
bool btnPushLong = false;

// temp update vals
unsigned long tempLastUpdate = 0;

// Solder Temp variables
double SolderTargetTemp = 0;
double SolderdefaultTargetTemp = 0;
double SolderMaxTemp = 0;
double SolderMinTemp = 50;
double SolderTemp = 0;

// Heater Temp variables
double HeaterTargetTemp = 0;
double HeaterdefaultTargetTemp = 0;
double HeaterMaxTemp = 0;
double HeaterMinTemp = 50;
double HeaterTemp = 0;

// Solder PID computation variables
double SolderCalculate_PWM = 0;
double S_Kp = 0;
double S_Ki = 0;
double S_Kd = 0;

// Heater PID computation variables
double HeaterCalculate_PWM = 0;
double H_Kp = 2;
double H_Ki = 0;
double H_Kd = 0;


PID SolderPID(&SolderTemp, &SolderCalculate_PWM, &SolderTargetTemp, S_Kp, S_Ki, S_Kd, DIRECT);
PID HeaterPID(&HeaterTemp, &HeaterCalculate_PWM, &HeaterTargetTemp, H_Kp, H_Ki, H_Kd, DIRECT);
MAX6675 SolderThermocouple(solderThermoCLK, solderThermoCS, solderThermoDO);
MAX6675 HeaterThermocouple(heaterThermoCLK, heaterThermoCS, heaterThermoDO);
Encoder myEnc(encoderCLK, encoderDT);
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

void setup() {
  Serial.begin(9600);
  // display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.begin(SSD1306_SWITCHCAPVCC);
  display.setRotation(2);

  pinMode(solderPin, OUTPUT);
  pinMode(heaterPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(encoderSW, INPUT_PULLUP);

  S_Kp = isnan(readFromEEPROM(1)) ? 1 : readFromEEPROM(1);
  S_Ki = isnan(readFromEEPROM(2)) ? 0 : readFromEEPROM(2);
  S_Kd = isnan(readFromEEPROM(3)) ? 0 : readFromEEPROM(3);

  SolderdefaultTargetTemp = SolderTargetTemp = isnan(readFromEEPROM(0)) ? 50 : readFromEEPROM(0);
  SolderMaxTemp = isnan(readFromEEPROM(4)) ? 480 : readFromEEPROM(4);
  defaultSolderBoostMode = SolderBoostMode = isnan(readFromEEPROM(5)) ? 0 : readFromEEPROM(5);

  H_Kp = isnan(readFromEEPROM(6)) ? 1 : readFromEEPROM(6);
  H_Ki = isnan(readFromEEPROM(7)) ? 0 : readFromEEPROM(7);
  H_Kd = isnan(readFromEEPROM(8)) ? 0 : readFromEEPROM(8);

  HeaterdefaultTargetTemp = HeaterTargetTemp = isnan(readFromEEPROM(9)) ? 50 : readFromEEPROM(9);
  HeaterMaxTemp = isnan(readFromEEPROM(10)) ? 500 : readFromEEPROM(10);
  defaultHeaterBoostMode = HeaterBoostMode = isnan(readFromEEPROM(11)) ? 0 : readFromEEPROM(11);

  myEnc.write(SolderTargetTemp);
  SolderPID.SetMode(AUTOMATIC);
  SolderPID.SetOutputLimits(0, 255);
  SolderPID.SetTunings(S_Kp, S_Ki, S_Kd);

  HeaterPID.SetMode(AUTOMATIC);
  HeaterPID.SetOutputLimits(0, 255);
  HeaterPID.SetTunings(H_Kp, H_Ki, H_Kd);


}

void loop() {

  getTemperature();
  btn_handler();
  analogWrite(solderPin, SolderCalculate_PWM);
  analogWrite(heaterPin, HeaterCalculate_PWM);
  analogWrite(fanPin, Fan_PWM);

  if (SolderBoostMode) {
    SolderCalculate_PWM = 255;
    if (SolderTemp >= SolderTargetTemp)
      SolderBoostMode = false;
  } else SolderPID.Compute();

  if (HeaterBoostMode) {
    HeaterCalculate_PWM = 255;
    if (HeaterTemp >= HeaterTargetTemp)
      HeaterBoostMode = false;
  } else HeaterPID.Compute();

  if (openMenu) displayMenu();
  else {
    displayHome();
    adjustSetUp();
  }
}

void adjustSetUp() {

  long Position = myEnc.read();
  switch (homeId) {
    case 0:
      if (Position > SolderMaxTemp) {
        myEnc.write(SolderMaxTemp);
        Position = SolderMaxTemp;
      } else if (Position < SolderMinTemp) {
        myEnc.write(SolderMinTemp);
        Position = SolderMinTemp;
      }
      SolderTargetTemp = Position;
      break;
    case 1:
      if (Position > Fan_Max_PWM) {
        myEnc.write(Fan_Max_PWM);
        Position = Fan_Max_PWM;
      } else if (Position < Fan_Min_PWM) {
        myEnc.write(Fan_Min_PWM);
        Position = Fan_Min_PWM;
      }
      Fan_PWM = Position;
      break;
    case 2:
      if (Position > HeaterMaxTemp) {
        myEnc.write(HeaterMaxTemp);
        Position = HeaterMaxTemp;
      } else if (Position < HeaterMinTemp) {
        myEnc.write(HeaterMinTemp);
        Position = HeaterMinTemp;
      }
      HeaterTargetTemp = Position;
      break;
  }
}

void getTemperature() {

  if (millis() - tempLastUpdate > 250) {
    tempLastUpdate = millis();

    SolderTemp = SolderThermocouple.readCelsius();

    // HeaterTemp = HeaterThermocouple.readCelsius();
    HeaterTemp = SolderTemp;
  }
}

void displayHome() {

  display.clearDisplay();  // Clear the buffer
  display.drawRect(0, 0, 7, 64, SSD1306_WHITE);
  display.drawRect(113, 0, 7, 64, SSD1306_WHITE);
  display.drawRect(121, 0, 7, 64, SSD1306_WHITE);

  display.drawLine(9, 27, 110, 27, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.print("Solder");
  display.setCursor(13, 15);
  display.print("Set:");
  display.print(int(SolderTargetTemp));
  display.setTextSize(3);
  display.setCursor(60, 3);
  display.print(int(SolderTemp));

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 30);
  display.print("Heater");

  display.setCursor(13, 43);
  display.print("Fan:");
  display.print(map(Fan_PWM, Fan_Min_PWM, Fan_Max_PWM, 0, 100));
  display.setCursor(13, 55);
  display.print("Set:");
  display.print(int(HeaterTargetTemp));
  display.setTextSize(3);
  display.setCursor(60, 35);
  display.print(int(HeaterTemp));
  // Solder bar
  int d = 0;
  d = map(SolderCalculate_PWM, 0, 255, 60, 0);
  display.fillRect(2, 2, 3, 60, SSD1306_WHITE);
  display.fillRect(2, 2, 3, d, SSD1306_BLACK);

  // fan bar
  d = map(Fan_PWM, 0, 255, 60, 0);
  display.fillRect(115, 2, 3, 60, SSD1306_WHITE);
  display.fillRect(115, 2, 3, d, SSD1306_BLACK);

  // heater bar
  d = map(HeaterCalculate_PWM, 0, 255, 60, 0);
  display.fillRect(123, 2, 3, 60, SSD1306_WHITE);
  display.fillRect(123, 2, 3, d, SSD1306_BLACK);
  if (homeId == 0) display.fillTriangle(8, 16, 8, 20, 10, 18, SSD1306_WHITE);
  else if (homeId == 1) display.fillTriangle(8, 44, 8, 48, 10, 46, SSD1306_WHITE);
  else if (homeId == 2) display.fillTriangle(8, 56, 8, 60, 10, 58, SSD1306_WHITE);
  display.display();
}

void saveToEEPROM(int address, float value) {
  if (value != readFromEEPROM(address * 8))
    EEPROM.put(address * 8, value);
}

float readFromEEPROM(int address) {
  float value;
  EEPROM.get(address * 8, value);
  return value;
}


void displayMenu() {
  display.clearDisplay();
  display.fillRect(0, display.height() - 16, display.width(), 16, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_INVERSE);

  switch (menuId) {
    case 0:  // PID settings {Kp,Ki,Kd}
      display.setCursor(35, 53);
      display.print("Solder PID");

      display.setCursor(40, 6);
      display.print("Kp : ");
      display.print(S_Kp);

      display.setCursor(40, 18);
      display.print("Ki : ");
      display.print(S_Ki);

      display.setCursor(40, 30);
      display.print("Kd : ");
      display.print(S_Kd);
      break;
    case 1:  // Temp settings {TargetTemp , MaxTemp}
      display.setCursor((display.width() / 2) - 32, 53);
      display.print("Temperature");

      display.setCursor(17, 6);
      display.print("TargetTemp : ");
      display.print(int(SolderdefaultTargetTemp));

      display.setCursor(17, 18);
      display.print("MaxTemp    : ");
      display.print(int(SolderMaxTemp));
      break;
    case 2:  // Boost Mode {BoostMode}
      display.setCursor((display.width() / 2) - 32, 53);
      display.print("Boost Mode");

      display.setCursor(17, 18);
      display.print("BoostMode is ");
      if (defaultSolderBoostMode) display.print("ON");
      else display.print("OFF");
      subMenuId = 1;
      break;
  }
  int ss;
  if (inSubMenu) {
    if (subMenuId == 0) ss = 9;
    if (subMenuId == 1) ss = 21;
    if (subMenuId == 2) ss = 33;
  } else ss = display.height() - 8;
  display.fillTriangle(7, ss, 11, ss - 3, 11, ss + 3, SSD1306_INVERSE);
  display.fillTriangle(117, ss - 3, 117, ss + 3, 121, ss, SSD1306_INVERSE);
  display.display();
}


void btn_handler() {
  btnPushLong = btnPush = false;
  if (!digitalRead(encoderSW)) {
    if (btnPushTime == 0)
      btnPushTime = millis();
  } else if (btnPushTime != 0) {
    if (millis() - btnPushTime > 1000) btnPushLong = true;
    else if (millis() - btnPushTime > 25) btnPush = true;
    btnPushTime = 0;
  }

  if (!openMenu and btnPushLong) {
    openMenu = true;
    // myEnc.write(0);
  } else if (!openMenu and btnPush) {
    homeId++;
    if (homeId > 2) homeId = 0;
    else if (homeId < 0) homeId = 2;
    switch (homeId) {
      case 0:
        myEnc.write(SolderTargetTemp);
        break;
      case 1:
        myEnc.write(Fan_PWM);
        break;
      case 2:
        myEnc.write(HeaterTargetTemp);
        break;
    }
  } else if (openMenu and btnPush and !inSubMenu) {
    inSubMenu = true;
  } else if (openMenu and btnPushLong and inSubMenu) {
    inSubMenu = false;
    if (menuId == 0) {
      saveToEEPROM(1, S_Kp);
      saveToEEPROM(2, S_Ki);
      saveToEEPROM(3, S_Kd);
    }
    if (menuId == 1) {
      saveToEEPROM(0, SolderdefaultTargetTemp);
      saveToEEPROM(4, SolderMaxTemp);
    }
    if (menuId == 2) {
      saveToEEPROM(5, defaultSolderBoostMode);
    }

  } else if (openMenu and btnPushLong and !inSubMenu) {
    openMenu = false;
    display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
  } else if (openMenu and btnPush and inSubMenu) {
    subMenuId++;
    if (menuId == 0) {
      if (subMenuId > 2) subMenuId = 0;
      if (subMenuId < 0) subMenuId = 2;
    }
    if (menuId == 1) {
      if (subMenuId > 1) subMenuId = 0;
      if (subMenuId < 0) subMenuId = 1;
    }
    if (menuId == 2) { subMenuId = 0; }
  }

  // change params
  if (openMenu) {
    float a = myEnc.readAndReset();
    delay(150);
    if (a != 0) {
      if (a > 0) a = 1;
      if (a < 0) a = -1;
      if (inSubMenu) {
        if (menuId == 0) {
          if (subMenuId == 0)
            S_Kp += a / 10;
          if (subMenuId == 1)
            S_Ki += a / 10;
          if (subMenuId == 2)
            S_Kd += a / 10;

        } else if (menuId == 1) {
          if (subMenuId == 0)
            SolderdefaultTargetTemp += a * 5;
          if (subMenuId == 1)
            SolderMaxTemp += a * 10;

        } else if (menuId == 2) {
          if (subMenuId == 1) {
            if (defaultSolderBoostMode) defaultSolderBoostMode = false;
            else defaultSolderBoostMode = true;
          }
        }
      } else {
        menuId++;
        if (menuId > 2) menuId = 0;
        if (menuId < 0) menuId = 2;
      }
    }
  }
}
