/*
  SNEngine - NES/SNES2PCE controller adapter
  by RetroCityRansom (www.youtube.com/@RetroCityRansom)

  Features:
  - No noticable input lag (at least for me)
  - Autofire with adjustable speed
  - Multitap compatible (tested with 2 SNEngine adapters and 1 original CoreGrafx gamepad simultaneously)
  - Switchable button layouts
  - Save settings to EEPROM
  - OLED support (I2C, SSD1306, 0.96", 128x64)

  Hotkey combos:
  - SELECT + UP: change autofire speed
  - SELECT + DOWN: cycle through different button layouts
  - SELECT + LEFT: Changes the buttons displayed on the OLED according to the attached controller type (SNES vs. XBOX vs. PSX button names).
  - SELECT + RIGHT: Save chosen autofire setting and button mapping as preset

  If you want to connect an optional I2C OLED display, attach SDA to A4 and SCL (or SCK) to the A5 pin of the Arduino.
*/
#include <EEPROM.h>
#include <U8x8lib.h>
#include <Wire.h>

#define ADDR_TURBO_SPEED 0
#define ADDR_BUTTON_SWAP 1
#define EEPROM_ADDR_CONTROLLER 2

#define SNES_CLOCK_BIT 0
#define SNES_LATCH_BIT 1
#define SNES_DATA_BIT 2

// SNES Button Map
#define SNES_B 0
#define SNES_Y 1
#define SNES_SELECT 2
#define SNES_START 3
#define SNES_UP 4
#define SNES_DOWN 5
#define SNES_LEFT 6
#define SNES_RIGHT 7
#define SNES_A 8
#define SNES_X 9
#define SNES_L 10
#define SNES_R 11

const uint8_t TURBO_MODE_1 = 44;
const uint8_t TURBO_MODE_2 = 40;
const uint8_t TURBO_MODE_3 = 36;
const uint8_t TURBO_MODE_4 = 32;
const uint8_t TURBO_MODE_5 = 24;
const uint8_t TURBO_MODE_6 = 16;

struct TurboSettings {
  uint8_t speed;
  const char* name;
};

const TurboSettings TURBO_MODES[] = {
  { TURBO_MODE_1, "11.36Hz" },
  { TURBO_MODE_2, "12.50Hz" },
  { TURBO_MODE_3, "13.89Hz" },
  { TURBO_MODE_4, "15.63Hz" },
  { TURBO_MODE_5, "20.83Hz" },
  { TURBO_MODE_6, "31.25Hz" }
};
const uint8_t NUM_TURBO_MODES = sizeof(TURBO_MODES) / sizeof(TURBO_MODES[0]);

uint8_t turboIdx = 0;
uint8_t turboSpeed = TURBO_MODE_1;

struct ButtonLayout {
  uint16_t maskPCE_I;
  uint16_t maskPCE_II;
  uint16_t maskPCE_I_AF;
  uint16_t maskPCE_II_AF;
  const char* name_snes;
  const char* name_xbox;
  const char* name_psx;
};

const ButtonLayout LAYOUTS[] = {   
  { (1<<SNES_B)|(1<<SNES_X), (1<<SNES_A)|(1<<SNES_Y), (1<<SNES_L), (1<<SNES_R), "1=BX 2=AY T12=LR", "1=AY 2=BX T12=LR", "1=XT 2=OS T12=LR" },
  { (1<<SNES_A)|(1<<SNES_Y), (1<<SNES_B)|(1<<SNES_X), (1<<SNES_R), (1<<SNES_L), "1=AY 2=BX T12=RL", "1=BX 2=AY T12=RL", "1=OS 2=XT T12=RL" },
  { (1<<SNES_B)|(1<<SNES_L), (1<<SNES_A)|(1<<SNES_R), (1<<SNES_Y), (1<<SNES_X), "1=BL 2=AR T12=YX", "1=AL 2=BR T12=XY", "1=XL 2=OR T12=ST" },
  { (1<<SNES_A)|(1<<SNES_R), (1<<SNES_B)|(1<<SNES_L), (1<<SNES_X), (1<<SNES_Y), "1=AR 2=BL T12=XY", "1=BR 2=AL T12=YX", "1=OR 2=XL T12=TS" },
  { (1<<SNES_X)|(1<<SNES_L), (1<<SNES_Y)|(1<<SNES_R), (1<<SNES_B), (1<<SNES_A), "1=XL 2=YR T12=BA", "1=YL 2=XR T12=AB", "1=TL 2=SR T12=XO" },
  { (1<<SNES_Y)|(1<<SNES_R), (1<<SNES_X)|(1<<SNES_L), (1<<SNES_A), (1<<SNES_B), "1=YR 2=XL T12=AB", "1=XR 2=YL T12=BA", "1=SR 2=TL T12=OX" },
  { (1<<SNES_X)|(1<<SNES_L), (1<<SNES_A)|(1<<SNES_R), (1<<SNES_Y), (1<<SNES_B), "1=XL 2=AR T12=YB", "1=YL 2=BR T12=XA", "1=TL 2=OR T12=SX" },
  { (1<<SNES_A)|(1<<SNES_R), (1<<SNES_X)|(1<<SNES_L), (1<<SNES_B), (1<<SNES_Y), "1=AR 2=XL T12=BY", "1=BR 2=YL T12=AX", "1=OR 2=TL T12=XS" }
};
const uint8_t NUM_LAYOUTS = sizeof(LAYOUTS) / sizeof(LAYOUTS[0]);
const uint8_t NUM_CONTROLLER_TYPES = 3;
uint8_t controllerMode = 0;

bool upPressed = false;
bool downPressed = false;
bool leftPressed = false;
bool rightPressed = false;
uint8_t swapMode = 0;
uint8_t savedTurboSpeed = 0;
uint8_t savedSwapMode = 0;
uint8_t savedControllerMode = 0;

uint32_t lastReadTime = 0;
const uint8_t readInterval = 4;

// OLED
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(U8X8_PIN_NONE);
uint32_t lastInteractionTime = 0;
bool isOledDimmed = false;
const uint32_t OLED_DIM_TIMEOUT = 60000;
const uint8_t MAX_OLED_CONTRAST_VAL = 50;
const uint32_t HELP_TEXT_TIMEOUT = 10000;
const uint32_t SAVED_MSG_TIMEOUT = 3000;
uint32_t hideBottomMessagesAt = 0;
bool oledFound = false;
bool settingsSaved = true;

enum DisplayMode { 
  MODE_READY,
  MODE_CHANGED,
  MODE_SAVED_OK,
  MODE_ALREADY,
  MODE_HELP
};

DisplayMode currentMode = MODE_HELP;

void updateDisplay() {
  if (!oledFound) return;
  u8x8.setFont(u8x8_font_5x7_f); 

  char turboBuf[17];
  snprintf(turboBuf, sizeof(turboBuf), "TURBO: %s", TURBO_MODES[turboIdx].name);
  u8x8.drawString(0, 0, turboBuf);
  
  const char* currentLabel;
  switch (controllerMode) {
    case 1:  currentLabel = LAYOUTS[swapMode].name_xbox; break;
    case 2:  currentLabel = LAYOUTS[swapMode].name_psx;  break;
    default: currentLabel = LAYOUTS[swapMode].name_snes; break;
  }
  u8x8.drawString(0, 2, currentLabel);

  switch (controllerMode) {
    case 1:  u8x8.drawString(0, 1, "PCE-XBOX ->     "); break;
    case 2:  u8x8.drawString(0, 1, "PCE-PSX  ->     "); break;
    default: u8x8.drawString(0, 1, "PCE-SNES ->     "); break;
  }

  if (currentMode != MODE_CHANGED && millis() > hideBottomMessagesAt) {
      currentMode = MODE_READY;
  }

  switch (currentMode) {
    case MODE_CHANGED:  u8x8.drawString(0, 3, "SEL+RIGHT=SAVE  "); break;
    case MODE_SAVED_OK: u8x8.drawString(0, 3, "SETTINGS SAVED  "); break;
    case MODE_ALREADY:  u8x8.drawString(0, 3, "ALREADY SAVED   "); break;
    case MODE_HELP:     u8x8.drawString(0, 0, "HOTKEYS:        ");
                        u8x8.drawString(0, 1, "SEL+UP=TUR.SPEED");
                        u8x8.drawString(0, 2, "SEL+DOWN=LAYOUT ");
                        u8x8.drawString(0, 3, "SEL+LEFT=CONTRLR"); break;
    case MODE_READY:
    default:
      if (millis() > hideBottomMessagesAt) {
        u8x8.drawString(0, 3, "                ");
      }
      break;
  }
}


void setup() {
  turboSpeed = EEPROM.read(ADDR_TURBO_SPEED);
  bool valid = false;
  for (uint8_t i = 0; i < NUM_TURBO_MODES; i++) {
    if (turboSpeed == TURBO_MODES[i].speed) {
      turboIdx = i;
      valid = true;
      break;
    }
  }
  if (!valid) {
    turboIdx = 0; 
    turboSpeed = TURBO_MODES[turboIdx].speed;
  }
  swapMode = EEPROM.read(ADDR_BUTTON_SWAP);
  if (swapMode >= NUM_LAYOUTS) swapMode = 0;

  controllerMode = EEPROM.read(EEPROM_ADDR_CONTROLLER);
  if (controllerMode > NUM_CONTROLLER_TYPES) controllerMode = 0;

  savedTurboSpeed = turboSpeed;
  savedSwapMode = swapMode;
  savedControllerMode = controllerMode;
  
  Wire.begin(); 
  u8x8.begin();
  // Display test
  Wire.beginTransmission(0x3C); 
  if (Wire.endTransmission() == 0) {
    oledFound = true;
    u8x8.setPowerSave(0);
    u8x8.setContrast(MAX_OLED_CONTRAST_VAL);
    u8x8.clearDisplay();
  }

  // Splash the mermaid
  u8x8.clearDisplay();
  u8x8.setFont(u8x8_font_7x14B_1x2_f);
  u8x8.drawString(4, 1, "SN");
  u8x8.setInverseFont(1);
  u8x8.drawString(6, 1, "ENGINE");
  u8x8.setFont(u8x8_font_5x7_f);
  u8x8.setInverseFont(0);
  delay(3000);
  u8x8.clearDisplay();

  currentMode = MODE_HELP;
  hideBottomMessagesAt = millis() + HELP_TEXT_TIMEOUT;
  updateDisplay();

  // SNES Setup
  DDRC |= (1 << SNES_LATCH_BIT) | (1 << SNES_CLOCK_BIT);
  DDRC &= ~(1 << SNES_DATA_BIT);
  PORTC |= (1 << SNES_DATA_BIT);

  // Define port pins as outputs
  DDRD |= 0xF0;
  DDRB |= 0x0F;
}

void loop() {
  uint32_t currentMillis = millis();

  // OLED
  if (currentMode != MODE_READY && currentMode != MODE_CHANGED) {
    if (currentMillis >= hideBottomMessagesAt) {
      currentMode = MODE_READY;
      updateDisplay();
    }
  }
  if (!isOledDimmed && (currentMillis - lastInteractionTime > OLED_DIM_TIMEOUT)) {
      u8x8.setPowerSave(1);
      isOledDimmed = true;
  }

  // Polling interval check
  if (currentMillis - lastReadTime >= readInterval) {
    lastReadTime = currentMillis;
    bool turboTick = (currentMillis % (turboSpeed * 2)) < turboSpeed;

    noInterrupts();

    // Read SNES pad input
    uint16_t b = 0;                  // pressedButtons
    PORTC |= (1 << SNES_LATCH_BIT);  // set latch HIGH
    delayMicroseconds(6);
    PORTC &= ~(1 << SNES_LATCH_BIT); // set latch LOW
    delayMicroseconds(3);

    for (uint8_t i = 0; i < 16; i++) {
      if (!(PINC & (1 << SNES_DATA_BIT))) {
        if (i < 12) b |= (1UL << i);
      }
      PORTC |= (1 << SNES_CLOCK_BIT);  // set clock HIGH
      delayMicroseconds(3);
      PORTC &= ~(1 << SNES_CLOCK_BIT);  // set clock LOW
      delayMicroseconds(3);
    }

    interrupts();

    // OLED wake up
    if (b > 0) { 
      lastInteractionTime = currentMillis;
      if (isOledDimmed) {
        u8x8.setPowerSave(0); 
        isOledDimmed = false;
        if (!settingsSaved) {
          currentMode = MODE_CHANGED;
          hideBottomMessagesAt = 0;
        } else {
          currentMode = MODE_HELP;
          hideBottomMessagesAt = currentMillis + HELP_TEXT_TIMEOUT;
        }
        updateDisplay();
      }
    }

    // Combo detection logic
    if (b & (1 << SNES_SELECT)) {
      if (b & (1 << SNES_UP)) {
        if (!upPressed) {
          turboIdx = (turboIdx + 1) % NUM_TURBO_MODES;
          turboSpeed = TURBO_MODES[turboIdx].speed;
          settingsSaved = (turboSpeed == savedTurboSpeed && 
                           swapMode == savedSwapMode && 
                           controllerMode == savedControllerMode);
          if (settingsSaved) {
            currentMode = MODE_READY;
            hideBottomMessagesAt = millis();
          } else {
            currentMode = MODE_CHANGED;
          }
          updateDisplay();
          upPressed = true;
        }
      } else { upPressed = false; }

      if (b & (1 << SNES_DOWN)) {
        if (!downPressed) {
          swapMode = (swapMode + 1) % NUM_LAYOUTS;
          settingsSaved = (turboSpeed == savedTurboSpeed && 
                           swapMode == savedSwapMode && 
                           controllerMode == savedControllerMode);
          if (settingsSaved) {
            currentMode = MODE_READY;
            hideBottomMessagesAt = millis();
          } else {
            currentMode = MODE_CHANGED;
          }
          updateDisplay();
          downPressed = true;
        }
      } else { downPressed = false; }

      if (b & (1 << SNES_LEFT)) {
        if (!leftPressed) {
          controllerMode = (controllerMode + 1) % NUM_CONTROLLER_TYPES;
          settingsSaved = (turboSpeed == savedTurboSpeed && 
                           swapMode == savedSwapMode && 
                           controllerMode == savedControllerMode);
          if (settingsSaved) {
            currentMode = MODE_READY;
            hideBottomMessagesAt = millis();
          } else {
            currentMode = MODE_CHANGED;
          }
          updateDisplay();
          leftPressed = true;
        }
      } else { leftPressed = false; }

    if (b & (1 << SNES_RIGHT)) {
    if (!rightPressed) {
      if (!settingsSaved) {
        EEPROM.update(ADDR_TURBO_SPEED, turboSpeed);
        EEPROM.update(ADDR_BUTTON_SWAP, swapMode);
        EEPROM.update(EEPROM_ADDR_CONTROLLER, controllerMode);

        savedTurboSpeed = turboSpeed; 
        savedSwapMode = swapMode;
        savedControllerMode = controllerMode;
        settingsSaved = true;
        
        currentMode = MODE_SAVED_OK;
      } else {
        currentMode = MODE_ALREADY;
      }
      hideBottomMessagesAt = millis() + SAVED_MSG_TIMEOUT;
      updateDisplay();
      rightPressed = true;
    }
  } else { rightPressed = false; }

    } else {
      upPressed = downPressed = rightPressed = false;
    }

    // PCE I & II Mapping
    const ButtonLayout& current = LAYOUTS[swapMode];
    bool pceI      = (b & current.maskPCE_I)  || ((b & current.maskPCE_I_AF)  && turboTick);
    bool pceII     = (b & current.maskPCE_II) || ((b & current.maskPCE_II_AF) && turboTick);
    
    // Reset work variables for output ports
    uint8_t outPortD = 0xF0; 
    uint8_t outPortB = 0x0F;

    if (pceI)              outPortD &= ~(1 << 4);
    if (pceII)             outPortD &= ~(1 << 5);
    if (b & (1UL << SNES_START))  outPortD &= ~(1 << 6);
    if (b & (1UL << SNES_SELECT)) outPortD &= ~(1 << 7);
    if (b & (1UL << SNES_UP))     outPortB &= ~(1 << 0);
    if (b & (1UL << SNES_RIGHT))  outPortB &= ~(1 << 1);
    if (b & (1UL << SNES_LEFT))   outPortB &= ~(1 << 2);
    if (b & (1UL << SNES_DOWN))   outPortB &= ~(1 << 3);

    // push output to multiplexer
    PORTD = (PORTD & 0x0F) | (outPortD & 0xF0);
    PORTB = (PORTB & 0xF0) | (outPortB & 0x0F);
  }
}