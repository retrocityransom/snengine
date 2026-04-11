/*
  SNEngine - NES/SNES2PCE controller adapter
  by RetroCityRansom (www.youtube.com/@RetroCityRansom)

  Features:
  - No noticable input lag (at least for me)
  - Autofire with adjustable speed
  - Multitap compatible (tested with 2 SNEngine adapters and 1 original CoreGrafx gamepad simultaneously)
  - Switchable button layouts
  - Save settings to EEPROM
  - OLED support -> 0.96" 128X64 I2C OLED (SSD1306)

  Hotkey combos:
  - SELECT + UP: change autofire speed
  - SELECT + DOWN: cycle through different button layouts
  - SELECT + RIGHT: Save chosen autofire setting and button mapping as preset

  If you want to connect an optional OLED display, attach SDA to A4 and SCL to the A5 pin of the Arduino.
*/
#include <EEPROM.h>
#include <U8x8lib.h> //U8g2 
#include <Wire.h>

#define ADDR_TURBO_SPEED 0
#define ADDR_BUTTON_SWAP 1

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
  const char* name;
};

const ButtonLayout LAYOUTS[] = {   
  { (1<<SNES_B)|(1<<SNES_X), (1<<SNES_A)|(1<<SNES_Y), (1<<SNES_L), (1<<SNES_R), "1=BX 2=AY T12=LR" },
  { (1<<SNES_A)|(1<<SNES_Y), (1<<SNES_B)|(1<<SNES_X), (1<<SNES_R), (1<<SNES_L), "1=AY 2=BX T12=RL" },
  { (1<<SNES_B)|(1<<SNES_L), (1<<SNES_A)|(1<<SNES_R), (1<<SNES_Y), (1<<SNES_X), "1=BL 2=AR T12=YX" },
  { (1<<SNES_A)|(1<<SNES_R), (1<<SNES_B)|(1<<SNES_L), (1<<SNES_X), (1<<SNES_Y), "1=AR 2=BL T12=XY" },
  { (1<<SNES_X)|(1<<SNES_L), (1<<SNES_Y)|(1<<SNES_R), (1<<SNES_B), (1<<SNES_A), "1=XL 2=YR T12=BA" },
  { (1<<SNES_Y)|(1<<SNES_R), (1<<SNES_X)|(1<<SNES_L), (1<<SNES_A), (1<<SNES_B), "1=YR 2=XL T12=AB" },
  { (1<<SNES_X)|(1<<SNES_L), (1<<SNES_A)|(1<<SNES_R), (1<<SNES_Y), (1<<SNES_B), "1=XL 2=AR T12=YB" },
  { (1<<SNES_A)|(1<<SNES_R), (1<<SNES_X)|(1<<SNES_L), (1<<SNES_B), (1<<SNES_Y), "1=AR 2=XL T12=BY" }
};

bool upPressed = false;
bool downPressed = false;
bool rightPressed = false;
uint8_t swapMode = 0;
const uint8_t NUM_LAYOUTS = sizeof(LAYOUTS) / sizeof(LAYOUTS[0]);
uint8_t savedTurboSpeed = 0;
uint8_t savedSwapMode = 0;

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
  u8x8.drawString(0, 1, LAYOUTS[swapMode].name);

  if (currentMode != MODE_CHANGED && millis() > hideBottomMessagesAt) {
      currentMode = MODE_READY;
  }

  switch (currentMode) {
    case MODE_CHANGED:
      u8x8.drawString(0, 2, "SETTINGS CHANGED"); 
      u8x8.drawString(0, 3, "SEL+RGT TO SAVE "); 
      break;

    case MODE_SAVED_OK:
      u8x8.drawString(0, 2, "                "); 
      u8x8.drawString(0, 3, "SETTINGS SAVED  ");
      break;

    case MODE_ALREADY:
      u8x8.drawString(0, 2, "ALREADY SAVED   "); 
      u8x8.drawString(0, 3, "NO WRITE NEEDED ");
      break;

    case MODE_HELP:
      u8x8.drawString(0, 2, "SEL+DOWN=LAYOUT "); 
      u8x8.drawString(0, 3, "SEL+UP=TR.SPEED ");
      break;

    case MODE_READY:
    default:
      u8x8.drawString(0, 2, "                "); 
      u8x8.drawString(0, 3, "                "); 
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

  savedTurboSpeed = turboSpeed;
  savedSwapMode = swapMode;

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

    // Stop interrupts to get stable timing
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

    // Restart interrupts for millis() and autofire
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
          settingsSaved = (turboSpeed == savedTurboSpeed && swapMode == savedSwapMode);
          currentMode = settingsSaved ? MODE_READY : MODE_CHANGED;
          updateDisplay();
          upPressed = true;
        }
      } else { upPressed = false; }

      if (b & (1 << SNES_DOWN)) {
        if (!downPressed) {
          swapMode = (swapMode + 1) % NUM_LAYOUTS;
          settingsSaved = (turboSpeed == savedTurboSpeed && swapMode == savedSwapMode);
          currentMode = settingsSaved ? MODE_READY : MODE_CHANGED;
          updateDisplay();
          downPressed = true;
        }
      } else { downPressed = false; }

  if (b & (1 << SNES_RIGHT)) {
  if (!rightPressed) {
    if (!settingsSaved) {
      EEPROM.update(ADDR_TURBO_SPEED, turboSpeed);
      EEPROM.update(ADDR_BUTTON_SWAP, swapMode);

      savedTurboSpeed = turboSpeed; 
      savedSwapMode = swapMode;
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