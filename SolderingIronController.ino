
#include <SPI.h>
#include "max6675.h"
#include <Encoder.h>
#include <PID_v1.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// OLED Pins
#define OLED_CLK 25   //SCL
#define OLED_MOSI 26  //SDA
#define OLED_RESET 27
#define OLED_DC 28
#define OLED_CS 29
#define UpdateDisplyDuration 250  //ms
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64


// Solder MAX6675 Pins
#define solderThermoDO 16
#define solderThermoCLK 14
#define solderThermoCS 15

// Heater MAX6675 Pins
#define heaterThermoDO 16
#define heaterThermoCLK 14
#define heaterThermoCS 17

// Encodr Pins
#define encoderSW 4
#define encoderDT 6
#define encoderCLK 5
// #define ENCODER_DO_NOT_USE_INTERRUPTS

// PWM Out Pins
#define solderPin 10
#define fanPin 30
#define heaterPin 7

// fan vals
int Fan_PWM = 50;
#define Fan_Min_PWM 25

// menu location
#define Menu0 3
#define Menu1 14
#define Menu2 25
#define Menu3 36

// menu Config Vals
bool openMenu = false;
bool inSubMenu = false;
short menuId = 0;
short subMenuId = 0;
short homeId = 0;

// Power vals
#define Heater_Key_Pin 22
#define Solder_Key_Pin 23
bool CoolingMode = false;
bool Heater_ON = false;
bool Solder_ON = false;


// Boost Mode
bool SolderBoostMode = false;
bool defaultSolderBoostMode = false;

bool HeaterBoostMode = false;
bool defaultHeaterBoostMode = false;


// Solder Temp variables
double SolderTargetTemp = 0;
double SolderdefaultTargetTemp = 0;
double SolderMaxTemp = 0;
#define SolderMinTemp 50
double SolderTemp = 0;

// Heater Temp variables
double HeaterTargetTemp = 0;
double HeaterdefaultTargetTemp = 0;
double HeaterMaxTemp = 0;
#define HeaterMinTemp 50
double HeaterTemp = 0;

// Solder PID computation variables
double SolderCalculate_PWM = 0;
double S_Kp = 0;
double S_Ki = 0;
double S_Kd = 0;

// Heater PID computation variables
double HeaterCalculate_PWM = 0;
double H_Kp = 0;
double H_Ki = 0;
double H_Kd = 0;


PID SolderPID(&SolderTemp, &SolderCalculate_PWM, &SolderTargetTemp, S_Kp, S_Ki, S_Kd, DIRECT);
PID HeaterPID(&HeaterTemp, &HeaterCalculate_PWM, &HeaterTargetTemp, H_Kp, H_Ki, H_Kd, DIRECT);
MAX6675 SolderThermocouple(solderThermoCLK, solderThermoCS, solderThermoDO);
MAX6675 HeaterThermocouple(heaterThermoCLK, heaterThermoCS, heaterThermoDO);
Encoder myEnc(encoderCLK, encoderDT);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

extern int __heap_start, *__brkval;
int freeMemory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void setup() {
  Serial1.begin(9600);
  Serial1.println("all is good");
  display.begin(SSD1306_SWITCHCAPVCC);
  display.setRotation(0);

  pinMode(solderPin, OUTPUT);
  pinMode(heaterPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(encoderSW, INPUT_PULLUP);
  pinMode(encoderDT, INPUT_PULLUP);
  pinMode(encoderCLK, INPUT_PULLUP);
  pinMode(Heater_Key_Pin, INPUT_PULLUP);
  pinMode(Solder_Key_Pin, INPUT_PULLUP);

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

  display.clearDisplay();
}

void loop() {
  getTemperature();
  MenuHandler();
  applyPower();
  if (openMenu) {
    displayMenu();
  } else {
    displayHome();
    adjustSetUp();
  }

}

bool displayUpdateAccess() {
  static unsigned long lastUpdateDisplay = 0;
  if (millis() - lastUpdateDisplay > UpdateDisplyDuration) {
    lastUpdateDisplay = millis();
    return true;
  }
  return false;
}

void applyPower() {

  Solder_ON = !digitalRead(Solder_Key_Pin);
  Heater_ON = !digitalRead(Heater_Key_Pin);

  if (!Heater_ON) {
    if (HeaterTemp > 50) {
      Fan_PWM = 130;
      CoolingMode = true;
    } else {
      Fan_PWM = 0;
      CoolingMode = false;
    }
  } else if (Fan_PWM < Fan_Min_PWM) Fan_PWM = 52;

  if (Solder_ON) {
    if (SolderBoostMode) {
      SolderCalculate_PWM = 255;
      if (SolderTemp >= SolderTargetTemp)
        SolderBoostMode = false;
    } else
      SolderPID.Compute();
  } else {
    SolderCalculate_PWM = 0;
    SolderBoostMode = defaultSolderBoostMode;
    if (homeId == 0) homeId = 1;
  }

  if (Heater_ON) {
    if (HeaterBoostMode) {
      HeaterCalculate_PWM = 255;
      if (HeaterTemp >= HeaterTargetTemp)
        HeaterBoostMode = false;
    } else
      HeaterPID.Compute();
  } else {
    HeaterCalculate_PWM = 0;
    HeaterBoostMode = defaultHeaterBoostMode;
    if (homeId == 1 || homeId == 2) homeId = 0;
  }

  analogWrite(solderPin, SolderCalculate_PWM);
  analogWrite(heaterPin, HeaterCalculate_PWM);
  analogWrite(fanPin, Fan_PWM);
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
      if (Position > 255) {
        myEnc.write(255);
        Position = 255;
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

  static unsigned long TempLastUpdate = 0;
  static bool TempLastUpdateState = false;

  if (millis() - TempLastUpdate >= 250) {
    TempLastUpdate = millis();

    if (TempLastUpdateState) SolderTemp = SolderThermocouple.readCelsius();
    else HeaterTemp = HeaterThermocouple.readCelsius();

    TempLastUpdateState = !TempLastUpdateState;
  }
}

void displayHome() {
  if (displayUpdateAccess()) {
    display.clearDisplay();  // Clear the buffer
    display.drawRect(0, 0, 7, 64, SSD1306_WHITE);
    display.drawRect(113, 0, 7, 64, SSD1306_WHITE);
    display.drawRect(121, 0, 7, 64, SSD1306_WHITE);

    display.drawLine(9, 27, 110, 27, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(10, 0);
    display.print("Solder");
    if (Solder_ON) {
      display.setCursor(13, 15);
      display.print("Set:");
      display.print(int(SolderTargetTemp));
      display.setTextSize(3);
      display.setCursor(60, 3);
      display.print(int(SolderTemp));
    } else {
      // display.setTextColor(SSD1306_WHITE);
      display.setTextSize(3);
      display.setCursor(55, 3);
      display.print("OFF");
    }

    display.setTextSize(1);
    // display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 30);
    display.print("Heater");
    if (Heater_ON) {
      display.setCursor(13, 43);
      display.print("Fan:");
      display.print(map(Fan_PWM, 0, 255, 0, 100));
      display.setCursor(13, 55);
      display.print("Set:");
      display.print(int(HeaterTargetTemp));
      display.setTextSize(3);
      display.setCursor(60, 35);
      display.print(int(HeaterTemp));
    } else if (CoolingMode) {
      display.setTextSize(2);
      display.setCursor(20, 45);
      display.print("COOLING");

    } else {
      display.setTextSize(3);
      display.setCursor(55, 35);
      display.print("OFF");
    }
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
    if (homeId == 0 && Solder_ON) display.fillTriangle(8, 16, 8, 20, 10, 18, SSD1306_WHITE);
    else if (homeId == 1 && Heater_ON) display.fillTriangle(8, 44, 8, 48, 10, 46, SSD1306_WHITE);
    else if (homeId == 2 && Heater_ON) display.fillTriangle(8, 56, 8, 60, 10, 58, SSD1306_WHITE);
    display.display();
  }
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
  if (displayUpdateAccess()) {
    display.clearDisplay();
    display.fillRect(0, display.height() - 16, display.width(), 16, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_INVERSE);
    display.setCursor(35, 53);
    switch (menuId) {

      case 0:  // Solder PID settings {Kp,Ki,Kd}
      case 1:  // Heater PID settings {Kp,Ki,Kd}

        if (menuId == 0)
          display.print("Solder PID");
        else if (menuId == 1)
          display.print("Heater PID");

        display.setCursor(35, Menu0);
        display.print("Kp : ");
        display.print((menuId == 0) ? S_Kp : H_Kp);

        display.setCursor(35, Menu1);
        display.print("Ki : ");
        display.print((menuId == 0) ? S_Ki : H_Ki);

        display.setCursor(35, Menu2);
        display.print("Kd : ");
        display.print((menuId == 0) ? S_Kd : H_Kd);
        break;
      case 2:  // Temp settings {TargetTemp , MaxTemp}

        display.print("Temperature");

        display.setCursor(16, Menu0);
        display.print("SolderTarget:");
        display.print(int(SolderdefaultTargetTemp));

        display.setCursor(16, Menu1);
        display.print("SolderMax   :");
        display.print(int(SolderMaxTemp));

        display.setCursor(16, Menu2);
        display.print("HeaterTarget:");
        display.print(int(HeaterdefaultTargetTemp));

        display.setCursor(16, Menu3);
        display.print("HeaterMax   :");
        display.print(int(HeaterMaxTemp));
        break;
      case 3:  // Boost Mode {BoostMode}

        display.print("Boost Mode");

        display.setCursor(24, Menu0);
        display.print("Solder : ");
        display.print((defaultSolderBoostMode) ? "ON" : "OFF");

        display.setCursor(24, Menu1);
        display.print("Heater : ");
        display.print((defaultHeaterBoostMode) ? "ON" : "OFF");

        break;
    }

    int ss = 56;
    int size = 4;
    if (inSubMenu) {
      size = 2;
      if (subMenuId == 0) ss = Menu0 + 3;
      else if (subMenuId == 1) ss = Menu1 + 3;
      else if (subMenuId == 2) ss = Menu2 + 3;
      else if (subMenuId == 3) ss = Menu3 + 3;
    }
    display.fillTriangle(3, ss, 7, ss - size, 7, ss + size, SSD1306_INVERSE);
    display.fillTriangle(119, ss - size, 119, ss + size, 123, ss, SSD1306_INVERSE);
    display.display();
  }
}

void MenuHandler() {
  static unsigned long btnPushTime = 0;
  bool btnPush = false;
  bool btnPushLong = false;

  if (!digitalRead(encoderSW)) {
    if (btnPushTime == 0) btnPushTime = millis();
  } else if (btnPushTime != 0) {
    unsigned long NowTime = millis();
    if (NowTime - btnPushTime > 1000) btnPushLong = true;
    else if (NowTime - btnPushTime > 20) btnPush = true;
    btnPushTime = 0;
  }

  if (!openMenu and btnPushLong) {
    openMenu = true;

  } else if (!openMenu and btnPush) {
    homeId++;

    short homeId_min = Solder_ON ? 0 : 1;
    short homeId_max = Heater_ON ? 2 : 0;
    homeId = (homeId > homeId_max) ? homeId_min : (homeId < homeId_min) ? homeId_max
                                                                        : homeId;

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
    switch (menuId) {
      case 0:
        saveToEEPROM(1, S_Kp);
        saveToEEPROM(2, S_Ki);
        saveToEEPROM(3, S_Kd);
        SolderPID.SetTunings(S_Kp, S_Ki, S_Kd);
        break;
      case 1:
        saveToEEPROM(6, H_Kp);
        saveToEEPROM(7, H_Ki);
        saveToEEPROM(8, H_Kd);
        HeaterPID.SetTunings(H_Kp, H_Ki, H_Kd);
        break;
      case 2:
        saveToEEPROM(0, SolderdefaultTargetTemp);
        saveToEEPROM(4, SolderMaxTemp);
        saveToEEPROM(9, HeaterdefaultTargetTemp);
        saveToEEPROM(10, HeaterMaxTemp);
        break;
      case 3:
        saveToEEPROM(5, defaultSolderBoostMode);
        saveToEEPROM(11, defaultHeaterBoostMode);
        break;
    }

  } else if (openMenu and btnPushLong and !inSubMenu) {
    openMenu = false;
    display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
    switch (homeId) {
      case 0:
        myEnc.write(SolderTargetTemp);
      case 1:
        myEnc.write(Fan_PWM);
      case 2:
        myEnc.write(HeaterTargetTemp);
    }
  } else if (openMenu and btnPush and inSubMenu) {
    subMenuId++;
    switch (menuId) {
      case 0:
      case 1:
        if (subMenuId > 2) subMenuId = 0;
        if (subMenuId < 0) subMenuId = 2;
        break;
      case 2:
        if (subMenuId > 3) subMenuId = 0;
        if (subMenuId < 0) subMenuId = 3;
        break;
      case 3:
        if (subMenuId > 1) subMenuId = 0;
        if (subMenuId < 0) subMenuId = 1;
        break;
    }
  }

  // change params
  if (openMenu) {
    float a = myEnc.read();
    if (a != 0) {
      if (a > 0) a = 1;
      if (a < 0) a = -1;

      if (inSubMenu) {
        switch (menuId) {
          case 0:
            if (subMenuId == 0) {
              S_Kp += a / 10;
              if (S_Kp <= 0) S_Kp = 0;
            }
            if (subMenuId == 1) {
              S_Ki += a / 10;
              if (S_Ki <= 0) S_Ki = 0;
            }
            if (subMenuId == 2) {
              S_Kd += a / 10;
              if (S_Kd <= 0) S_Kd = 0;
            }
            break;
          case 1:
            if (subMenuId == 0) {
              H_Kp += a / 10;
              if (H_Kp <= 0) H_Kp = 0;
            }
            if (subMenuId == 1) {
              H_Ki += a / 10;
              if (H_Ki <= 0) H_Ki = 0;
            }
            if (subMenuId == 2) {
              H_Kd += a / 10;
              if (H_Kd <= 0) H_Kd = 0;
            }
            break;
          case 2:
            if (subMenuId == 0) {
              SolderdefaultTargetTemp += a * 5;
              if (SolderdefaultTargetTemp > SolderMaxTemp) SolderdefaultTargetTemp = SolderMaxTemp;
              if (SolderdefaultTargetTemp < SolderMinTemp) SolderdefaultTargetTemp = SolderMinTemp;
            }
            if (subMenuId == 1) {
              SolderMaxTemp += a * 10;
              if (SolderMaxTemp < SolderMinTemp) SolderMaxTemp = SolderMinTemp;
            }
            if (subMenuId == 2) {
              HeaterdefaultTargetTemp += a * 5;
              if (HeaterdefaultTargetTemp > HeaterMaxTemp) HeaterdefaultTargetTemp = HeaterMaxTemp;
              if (HeaterdefaultTargetTemp < HeaterMinTemp) HeaterdefaultTargetTemp = HeaterMinTemp;
            }
            if (subMenuId == 3) {
              HeaterMaxTemp += a * 10;
              if (HeaterMaxTemp < HeaterMinTemp) HeaterMaxTemp = HeaterMinTemp;
            }
            break;
          case 3:
            if (subMenuId == 0)
              defaultSolderBoostMode = defaultSolderBoostMode ? false : true;
            if (subMenuId == 1)
              defaultHeaterBoostMode = defaultHeaterBoostMode ? false : true;
            break;
        }
      } else {
        menuId += a;
        if (menuId > 3) menuId = 0;
        if (menuId < 0) menuId = 3;
      }
      delay(100);
      myEnc.readAndReset();
    }
  }
}
