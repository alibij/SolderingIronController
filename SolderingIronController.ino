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
#define OLED_MOSI   17 //SDA
#define OLED_CLK   18 //SCL
#define OLED_DC    15
#define OLED_CS    14
#define OLED_RESET 16

// MAX6675 Pins
#define solderThermoDO 8
#define solderThermoCS 7
#define solderThermoCLK 6

// Encodr Pins
#define encoderCLK 3
#define encoderDT 4
#define encoderSW 5

// PWM Out Pins
#define heaterPin 9

// menu Config Vals
bool openMenu = false;
bool inSubMenu = false;
short menuId = 0;
short subMenuId = 0;

// Boost Mode
bool SolderBoostMode = false;
bool defaultSolderBoostMode = false;

// Btn vals
unsigned long btnPushTime = 0;
bool btnPush = false;
bool btnPushLong = false;

// Temp variables
double SolderTargetTemp = 0;
double SolderdefaultTargetTemp = 0;
double SolderMaxTemp = 0;
double SolderMinTemp = 50;
unsigned long tempLastUpdate = 0;
double SolderTemp = 0;

// PID computation variables
double SolderCalculate_PWM = 0;
double S_Kp = 0;
double S_Ki = 0;
double S_Kd = 0;

PID myPID(&SolderTemp, &SolderCalculate_PWM, &SolderTargetTemp, S_Kp, S_Ki, S_Kd, DIRECT);
MAX6675 thermocouple(solderThermoCLK, solderThermoCS, solderThermoDO);
Encoder myEnc(encoderCLK, encoderDT);
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

void setup() {
  // Serial.begin(9600);
  // display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.begin(SSD1306_SWITCHCAPVCC);
  displayInit();

  pinMode(heaterPin, OUTPUT);
  pinMode(encoderSW, INPUT_PULLUP);

  S_Kp = isnan(readFromEEPROM(8)) ? 1 : readFromEEPROM(8);
  S_Ki = isnan(readFromEEPROM(16)) ? 0 : readFromEEPROM(16);
  S_Kd = isnan(readFromEEPROM(24)) ? 0 : readFromEEPROM(24);

  SolderdefaultTargetTemp = SolderTargetTemp = isnan(readFromEEPROM(0)) ? 50 : readFromEEPROM(0);
  SolderMaxTemp = isnan(readFromEEPROM(32)) ? 480 : readFromEEPROM(32);
  defaultSolderBoostMode = SolderBoostMode = isnan(readFromEEPROM(40)) ? 0 : readFromEEPROM(40);

  myEnc.write(SolderTargetTemp);
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 255);
  myPID.SetTunings(S_Kp, S_Ki, S_Kd);
}

void loop() {

  getTemperature();
  updateSetting();
  analogWrite(heaterPin, SolderCalculate_PWM);
  if (SolderBoostMode) {
    SolderCalculate_PWM = 255;
    if (SolderTemp >= SolderTargetTemp)
      SolderBoostMode = false;
  } else {
    myPID.Compute();
  }
  if (openMenu) displayMenu();
  else {
    displayHome(SolderCalculate_PWM);
    adjustTargetTemperature();
    // serialPrint();
  }
}

void adjustTargetTemperature() {

  long Position = myEnc.read();

  if (Position > SolderMaxTemp) {
    myEnc.write(SolderMaxTemp);
    Position = SolderMaxTemp;
  } else if (Position < SolderMinTemp) {
    myEnc.write(SolderMinTemp);
    Position = SolderMinTemp;
  }
  SolderTargetTemp = Position;
}

void getTemperature() {

  if (millis() - tempLastUpdate > 250) {
    tempLastUpdate = millis();
    SolderTemp = thermocouple.readCelsius();
  }
}

void displayHome(double pwm) {
  // display home data
  display.fillRect(5, 4, 68, 28, SSD1306_BLACK);
  display.setCursor(5, 4);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(4);
  display.print(int(SolderTemp));


  display.setCursor(15, 38);
  // display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.print("Current");

  display.fillRect(90, 10, 34, 14, SSD1306_BLACK);
  display.setCursor(90, 10);
  // display.setTextColor(SSD1306_BLACK);
  display.setTextSize(2);
  display.print(int(SolderTargetTemp));


  display.setCursor(90, 30);
  // display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.print("Target");

  // display power bar
  int d = map(pwm, 0, 255, 0, 124);
  display.fillRect(2, display.height() - 14, 124, 12, SSD1306_BLACK);
  display.fillRect(2, display.height() - 14, d, 12, SSD1306_WHITE);
  d = map(pwm, 0, 255, 0, 100);
  display.setCursor((display.width() / 2) - 7, 53);
  display.setTextColor(SSD1306_INVERSE);
  display.setTextSize(1);
  display.print(d);
  display.print(" %");

  display.display();
}

void saveToEEPROM(int address, float value) {
  if (value != readFromEEPROM(address))
    EEPROM.put(address, value);
}

float readFromEEPROM(int address) {
  float value;
  EEPROM.get(address, value);
  return value;
}

void displayInit() {

  display.clearDisplay();
  display.setRotation(2);

  display.drawRect(0, display.height() - 16, display.width(), 16, SSD1306_WHITE);
  display.display();
}

void displayMenu() {
  display.clearDisplay();
  display.fillRect(0, display.height() - 16, display.width(), 16, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_INVERSE);


  if (menuId == 0) {  // PID settings {Kp,Ki,Kd}

    display.setCursor((display.width() / 2) - 7, 53);
    display.print("PID");

    display.setCursor(45, 6);
    display.print("Kp : ");
    display.print(S_Kp);

    display.setCursor(45, 18);
    display.print("Ki : ");
    display.print(S_Ki);

    display.setCursor(45, 30);
    display.print("Kd : ");
    display.print(S_Kd);


  } else if (menuId == 1) {  // Temp settings {TargetTemp , MaxTemp}
    display.setCursor((display.width() / 2) - 32, 53);
    display.print("Temperature");

    display.setCursor(17, 6);
    display.print("TargetTemp : ");
    display.print(int(SolderdefaultTargetTemp));

    display.setCursor(17, 18);
    display.print("MaxTemp    : ");
    display.print(int(SolderMaxTemp));

  } else if (menuId == 2) {  // Boost Mode {BoostMode}
    display.setCursor((display.width() / 2) - 32, 53);
    display.print("Boost Mode");

    display.setCursor(17, 18);
    display.print("BoostMode is ");
    if (defaultSolderBoostMode) display.print("ON");
    else display.print("OFF");
    subMenuId = 1;
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

void updateSetting() {
  btnPushLong = btnPush = false;
  if (!digitalRead(encoderSW)) {
    if (btnPushTime == 0)
      btnPushTime = millis();
  } else if (btnPushTime != 0 ){
    if (millis() - btnPushTime > 1000) btnPushLong = true;
    else if (millis() - btnPushTime > 25) btnPush = true;
    btnPushTime = 0;
  }

  if (!openMenu and btnPushLong) {
    openMenu = true;
    // myEnc.write(0);
  } else if (openMenu and btnPush and !inSubMenu) {
    inSubMenu = true;
  } else if (openMenu and btnPushLong and inSubMenu) {
    inSubMenu = false;
    if (menuId == 0) {
      saveToEEPROM(8, S_Kp);
      saveToEEPROM(16, S_Ki);
      saveToEEPROM(24, S_Kd);
    }
    if (menuId == 1) {
      saveToEEPROM(0, SolderdefaultTargetTemp);
      saveToEEPROM(32, SolderMaxTemp);
    }
    if (menuId == 2) {
      saveToEEPROM(40, defaultSolderBoostMode);
    }

  } else if (openMenu and btnPushLong and !inSubMenu) {
    openMenu = false;
    myEnc.write(SolderTargetTemp);
    display.fillRect(0, 0, 128, 47, SSD1306_BLACK);
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
