#include <SPI.h>
#include "max6675.h"
#include <Encoder.h>
#include <PID_v1.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// MAX6675 Pins
#define thermoDO 7
#define thermoCS 6
#define thermoCLK 5

// Encodr pin
#define encoderCLK 2
#define encoderDT 3
#define encoderSW 4

// PWM Out pin
#define heaterPin 9

// menu Config Vals
bool openMenu = false;
bool inSubMenu = false;
short menuId = 0;
short subMenuId = 0;

// Boost Mode
bool BoostMode = false;
bool defaultBoostMode = false;

// Btn vals
unsigned long btnPushTime = 0;
bool btnPush = false;
bool btnPushLong = false;

// Temp variables
double targetTemp = 0;
double defaultTargetTemp = 0;
double maxTemp = 0;
double minTemp = 50;
unsigned long tempLastUpdate = 0;
double currentTemp = 0;

// PID computation variables
double calculate_PWM = 0;
double Kp = 0;
double Ki = 0;
double Kd = 0;

PID myPID(&currentTemp, &calculate_PWM, &targetTemp, Kp, Ki, Kd, DIRECT);
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
Encoder myEnc(encoderCLK, encoderDT);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  // Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  displayInit();

  pinMode(heaterPin, OUTPUT);
  pinMode(encoderSW, INPUT_PULLUP);

  Kp = isnan(readFromEEPROM(8)) ? 1 : readFromEEPROM(8);
  Ki = isnan(readFromEEPROM(16)) ? 0 : readFromEEPROM(16);
  Kd = isnan(readFromEEPROM(24)) ? 0 : readFromEEPROM(24);

  defaultTargetTemp = targetTemp = isnan(readFromEEPROM(0)) ? 50 : readFromEEPROM(0);
  maxTemp = isnan(readFromEEPROM(32)) ? 480 : readFromEEPROM(32);
  defaultBoostMode = BoostMode = isnan(readFromEEPROM(40)) ? 0 : readFromEEPROM(40);

  myEnc.write(targetTemp);
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 255);
  myPID.SetTunings(Kp, Ki, Kd);
}

void loop() {

  getTemperature();
  updateSetting();
  analogWrite(heaterPin, calculate_PWM);
  if (BoostMode) {
    calculate_PWM = 255;
    if (currentTemp >= targetTemp)
      BoostMode = false;
  } else {
    myPID.Compute();
  }
  if (openMenu) displayMenu();
  else {
    displayHome(calculate_PWM);
    adjustTargetTemperature();
    // serialPrint();
  }
}

void adjustTargetTemperature() {

  long Position = myEnc.read();

  if (Position > maxTemp) {
    myEnc.write(maxTemp);
    Position = maxTemp;
  } else if (Position < minTemp) {
    myEnc.write(minTemp);
    Position = minTemp;
  }
  targetTemp = Position;
}

void getTemperature() {

  if (millis() - tempLastUpdate > 250) {
    tempLastUpdate = millis();
    currentTemp = thermocouple.readCelsius();
  }
}

void displayHome(double pwm) {
  // display home data
  display.fillRect(5, 4, 68, 28, SSD1306_BLACK);
  display.setCursor(5, 4);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(4);
  display.print(int(currentTemp));


  display.setCursor(15, 38);
  // display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.print("Current");

  display.fillRect(90, 10, 34, 14, SSD1306_BLACK);
  display.setCursor(90, 10);
  // display.setTextColor(SSD1306_BLACK);
  display.setTextSize(2);
  display.print(int(targetTemp));


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


  if (menuId == 0) {  // PID settings {KP,Ki,Kd}

    display.setCursor((display.width() / 2) - 7, 53);
    display.print("PID");

    display.setCursor(45, 6);
    display.print("Kp : ");
    display.print(Kp);

    display.setCursor(45, 18);
    display.print("Ki : ");
    display.print(Ki);

    display.setCursor(45, 30);
    display.print("Kd : ");
    display.print(Kd);


  } else if (menuId == 1) {  // Temp settings {TargetTemp , MaxTemp}
    display.setCursor((display.width() / 2) - 32, 53);
    display.print("Temperature");

    display.setCursor(17, 6);
    display.print("TargetTemp : ");
    display.print(int(defaultTargetTemp));

    display.setCursor(17, 18);
    display.print("MaxTemp    : ");
    display.print(int(maxTemp));

  } else if (menuId == 2) {  // Boost Mode {BoostMode}
    display.setCursor((display.width() / 2) - 32, 53);
    display.print("Boost Mode");

    display.setCursor(17, 18);
    display.print("BoostMode is ");
    if (defaultBoostMode) display.print("ON");
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
      saveToEEPROM(8, Kp);
      saveToEEPROM(16, Ki);
      saveToEEPROM(24, Kd);
    }
    if (menuId == 1) {
      saveToEEPROM(0, defaultTargetTemp);
      saveToEEPROM(32, maxTemp);
    }
    if (menuId == 2) {
      saveToEEPROM(40, defaultBoostMode);
    }

  } else if (openMenu and btnPushLong and !inSubMenu) {
    openMenu = false;
    myEnc.write(targetTemp);
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
            Kp += a / 10;
          if (subMenuId == 1)
            Ki += a / 10;
          if (subMenuId == 2)
            Kd += a / 10;

        } else if (menuId == 1) {
          if (subMenuId == 0)
            defaultTargetTemp += a * 5;
          if (subMenuId == 1)
            maxTemp += a * 10;

        } else if (menuId == 2) {
          if (subMenuId == 1) {
            if (defaultBoostMode) defaultBoostMode = false;
            else defaultBoostMode = true;
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
