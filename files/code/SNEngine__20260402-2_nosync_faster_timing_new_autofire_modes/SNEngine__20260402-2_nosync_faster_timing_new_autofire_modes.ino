/*
  SNEngine - NES/SNES2PCE controller adapter
  by RetroCityRansom (www.youtube.com/@RetroCityRansom)

  Features:
  - No noticable input lag (at least for me)
  - Autofire with adjustable speed (see below)
  - Multitap compatible (tested with 2 SNEngine adapters and 1 original CoreGrafx gamepad simultaneously)
  - Switchable button layouts (SELECT + DOWN)
  - Save settings to EEPROM (SELECT + RIGHT)

  Hotkey combos:
  - SELECT + UP: change autofire speed
  - SELECT + DOWN: cycle through different button layouts
  - SELECT + RIGHT: Save chosen autofire setting and button mapping as preset
*/

#include <EEPROM.h>

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
  const char* name; // prep for display
};
const TurboSettings TURBO_MODES[] = {
  { TURBO_MODE_1, "44ms" },
  { TURBO_MODE_2, "40ms" },
  { TURBO_MODE_3, "36ms" },
  { TURBO_MODE_4, "32ms" },
  { TURBO_MODE_5, "24ms" },
  { TURBO_MODE_6, "16ms" }
};
const uint8_t NUM_TURBO_MODES = sizeof(TURBO_MODES) / sizeof(TURBO_MODES[0]);

uint8_t turboIdx = 0;
uint8_t turboSpeed = TURBO_MODE_1;

struct ButtonLayout {
  uint16_t maskPCE_I;
  uint16_t maskPCE_II;
  uint16_t maskPCE_I_AF;
  uint16_t maskPCE_II_AF;
  const char* name; // prep for display
};
const ButtonLayout LAYOUTS[] = {   
  // Layout A: B/X=I, L=Turbo I, A/Y=II, R=Turbo II
  { (1<<SNES_B)|(1<<SNES_X), (1<<SNES_A)|(1<<SNES_Y), (1<<SNES_L), (1<<SNES_R), "A - B/X=I, L=Turbo I, A/Y=II, R=Turbo II" },
  // Layout B: B/X=II, L=Turbo II, A/Y=I, R=Turbo I
  { (1<<SNES_A)|(1<<SNES_Y), (1<<SNES_B)|(1<<SNES_X), (1<<SNES_R), (1<<SNES_L), "B - B/X=II, L=Turbo II, A/Y=I, R=Turbo I" },
  // Layout C: B/L=I, Y=Turbo I, A/R=II, X=Turbo II
  { (1<<SNES_B)|(1<<SNES_L), (1<<SNES_A)|(1<<SNES_R), (1<<SNES_Y), (1<<SNES_X), "C - B/L=I, Y=Turbo I, A/R=II, X=Turbo II" },
  // Layout D: B/L=II, Y=Turbo II, A/R=I, X=Turbo I
  { (1<<SNES_A)|(1<<SNES_R), (1<<SNES_B)|(1<<SNES_L), (1<<SNES_X), (1<<SNES_Y), "D - B/L=II, Y=Turbo II, A/R=I, X=Turbo I" },
  // Layout E: X/L=I, B=Turbo I, Y/R=II, A=Turbo II
  { (1<<SNES_X)|(1<<SNES_L), (1<<SNES_Y)|(1<<SNES_R), (1<<SNES_B), (1<<SNES_A), "E - X/L=I, B=Turbo I, Y/R=II, A=Turbo II" },
  // Layout F: X/L=II, B=Turbo II, Y/R=I, A=Turbo I
  { (1<<SNES_Y)|(1<<SNES_R), (1<<SNES_X)|(1<<SNES_L), (1<<SNES_A), (1<<SNES_B), "F - X/L=II, B=Turbo II, Y/R=I, A=Turbo I" },
  // Layout G: X/L=I, Y=Turbo I, A/R=II, B=Turbo II
  { (1<<SNES_X)|(1<<SNES_L), (1<<SNES_A)|(1<<SNES_R), (1<<SNES_Y), (1<<SNES_B), "G - X/L=I, Y=Turbo I, A/R=II, B=Turbo II" },
  // Layout H: X/L=II, Y=Turbo II, A/R=I, B=Turbo I
  { (1<<SNES_A)|(1<<SNES_R), (1<<SNES_X)|(1<<SNES_L), (1<<SNES_B), (1<<SNES_Y), "H - X/L=II, Y=Turbo II, A/R=I, B=Turbo I" }
};
bool comboLock = false;
uint8_t swapMode = 0;
const uint8_t NUM_LAYOUTS = sizeof(LAYOUTS) / sizeof(LAYOUTS[0]);

// Non-blocking timing variables
uint32_t lastReadTime = 0;
// const uint8_t readInterval = 16; // 16ms interval (approx. 60Hz polling)
const uint8_t readInterval = 4;

void setup() {
  // Restore saved button settings
  bool valid = false;
  turboSpeed = EEPROM.read(ADDR_TURBO_SPEED);
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
  
  // 
  swapMode = EEPROM.read(ADDR_BUTTON_SWAP);
  if (swapMode >= NUM_LAYOUTS) swapMode = 0;

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

  // Polling interval check
  if (currentMillis - lastReadTime >= readInterval) {
    lastReadTime = currentMillis;

    bool turboTick = (currentMillis % (turboSpeed * 2)) < turboSpeed;

    // Stop interrupts to get stable timing
    noInterrupts();

    // Read SNES pad input (STABILIZED 16-BIT)
    uint16_t b = 0;                  // pressedButtons
    PORTC |= (1 << SNES_LATCH_BIT);  // set latch HIGH
    delayMicroseconds(6);
    PORTC &= ~(1 << SNES_LATCH_BIT);  // set latch LOW
    delayMicroseconds(3);

    for (uint8_t i = 0; i < 16; i++) {
      if (!(PINC & (1 << SNES_DATA_BIT))) {
        if (i < 12) b |= (1UL << i);
      }
      PORTC |= (1 << SNES_CLOCK_BIT);  // set clock HIGH
      // delayMicroseconds(6);
      delayMicroseconds(3);
      PORTC &= ~(1 << SNES_CLOCK_BIT);  // set clock LOW
      //delayMicroseconds(6);
      delayMicroseconds(3);
    }

    // Restart interrupts for millis() and autofire
    interrupts();

    // Combo detection logic
    if (b & (1 << SNES_SELECT)) {
      if (!comboLock) {
        if (b & (1 << SNES_UP)) {
          turboIdx = (turboIdx + 1) % NUM_TURBO_MODES;
          turboSpeed = TURBO_MODES[turboIdx].speed;
          comboLock = true;
        } else if (b & (1 << SNES_DOWN)) {
          swapMode = (swapMode + 1) % NUM_LAYOUTS;
          comboLock = true;
        } else if (b & (1 << SNES_RIGHT)) {
          EEPROM.update(ADDR_TURBO_SPEED, turboSpeed);
          EEPROM.update(ADDR_BUTTON_SWAP, swapMode);
          comboLock = true;
        }
      }
    } else {
      comboLock = false;
    }

    // PCE I & II Mapping
    const ButtonLayout& current = LAYOUTS[swapMode]; // Set the button mapping
    bool pceI      = (b & current.maskPCE_I)  || ((b & current.maskPCE_I_AF)  && turboTick);
    bool pceII     = (b & current.maskPCE_II) || ((b & current.maskPCE_II_AF) && turboTick);
    bool pceStart  = (b & (1UL << SNES_START));
    bool pceSelect = (b & (1UL << SNES_SELECT)) && !comboLock; // try not to send SELECT while using hotkey; only works, if the other button is pressed first
    bool up    = (b & (1UL << SNES_UP));
    bool down  = (b & (1UL << SNES_DOWN));
    bool left  = (b & (1UL << SNES_LEFT));
    bool right = (b & (1UL << SNES_RIGHT));
    // Reset work variables for output ports
    uint8_t outPortD = 0xF0; 
    uint8_t outPortB = 0x0F;

    if (pceI)      outPortD &= ~(1 << 4);
    if (pceII)     outPortD &= ~(1 << 5);
    if (pceStart)  outPortD &= ~(1 << 6);
    if (pceSelect) outPortD &= ~(1 << 7);
    if (up)        outPortB &= ~(1 << 0);
    if (right)     outPortB &= ~(1 << 1);
    if (left)      outPortB &= ~(1 << 2);
    if (down)      outPortB &= ~(1 << 3);

    // push output to multiplexer
    PORTD = (PORTD & 0x0F) | (outPortD & 0xF0);
    PORTB = (PORTB & 0xF0) | (outPortB & 0x0F);

  }
}