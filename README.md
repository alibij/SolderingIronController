# Arduino Soldering Iron Driver with PID Control

This project is an advanced soldering iron driver built using an Arduino Nano. It uses a MAX6675 thermocouple, a 128x64 OLED display, and a rotary encoder for user input. The system controls a Gordak 952 soldering iron handle with PID regulation to maintain precise temperature settings.

---

## Features

- **PID Temperature Control**: Ensures stable and accurate soldering temperatures.
- **Boost Mode**: Quickly heats the soldering iron to the target temperature.
- **User Interface**: 
  - Rotary encoder for adjusting settings.
  - OLED display for showing real-time temperature, target temperature, and power percentage.
- **EEPROM Storage**: Saves user preferences such as PID parameters and target temperatures.
- **Safety Limits**: Configurable maximum temperature to prevent overheating.
- **Menu System**: Adjust PID parameters, temperature settings, and Boost mode preferences.

---

## Hardware Requirements

- **Microcontroller**: Arduino Nano
- **Display**: 128x64 OLED (I2C)
- **Thermocouple Amplifier**: MAX6675
- **Rotary Encoder**: Rotary encoder with push-button
- **Soldering Iron Handle**: Gordak 952 (24V DC, 5A)
- **Power Supply**: 24V DC, 5A
- **MOSFET or Driver Circuit**: For PWM-based heater control

---

## Pin Configuration

| Component       | Arduino Pin |
|------------------|-------------|
| MAX6675 DO       | D7          |
| MAX6675 CS       | D6          |
| MAX6675 CLK      | D5          |
| Rotary Encoder CLK | D2        |
| Rotary Encoder DT  | D3        |
| Rotary Encoder SW  | D4        |
| PWM Heater Output  | D9        |
| OLED (I2C) SDA    | A4         |
| OLED (I2C) SCL    | A5         |

---

## Software Features

### Libraries Used

- **[SPI.h](https://www.arduino.cc/reference/en/libraries/spi/)**: For MAX6675 communication.
- **[Encoder.h](https://www.arduino.cc/reference/en/libraries/encoder/)**: For rotary encoder input.
- **[PID_v1.h](https://playground.arduino.cc/Code/PIDLibrary/)**: For PID temperature control.
- **[EEPROM.h](https://www.arduino.cc/en/Reference/EEPROM)**: To save and load settings.
- **[Wire.h](https://www.arduino.cc/en/Reference/Wire)**: For I2C communication.
- **[Adafruit_GFX.h](https://github.com/adafruit/Adafruit-GFX-Library)** and **[Adafruit_SSD1306.h](https://github.com/adafruit/Adafruit_SSD1306)**: For OLED display.

### Key Functionalities

1. **Temperature Reading**: Reads current temperature via the MAX6675 thermocouple amplifier.
2. **PWM Control**: Regulates power to the soldering iron using PID computations.
3. **User Settings**:
   - Adjust target temperature using the rotary encoder.
   - Modify PID parameters in the settings menu.
   - Enable or disable Boost mode.
4. **Data Persistence**:
   - Stores target temperature, max temperature, PID values, and Boost mode settings in EEPROM.
5. **Display Output**:
   - Real-time temperature and power usage.
   - Target temperature and menu options.

---

## Usage

1. **Setup**:
   - Connect the components as per the pin configuration table.
   - Upload the code to the Arduino Nano.
2. **Adjust Temperature**:
   - Rotate the encoder to set the desired target temperature.
3. **Enter Menu**:
   - Long press the encoder button to access settings.
   - Modify PID parameters, temperature limits, or Boost mode preferences.
4. **Save Settings**:
   - Long press the encoder button in a submenu to save settings to EEPROM.

---

## License

This project is released under the **MIT License**, which allows free use, modification, and distribution with proper attribution. For details, see the [LICENSE](LICENSE) file.

---

## Future Enhancements

- Add over-temperature protection with automatic shutdown.
- Implement a sleep mode to reduce power consumption when idle.
- Expand menu functionality for more advanced customization options.
- Add heater gun 

---

## Acknowledgments

- Inspired by existing soldering iron controllers.
- Utilizes open-source libraries for efficient development.

---

Feel free to contribute to this project by submitting issues or pull requests on GitHub!
