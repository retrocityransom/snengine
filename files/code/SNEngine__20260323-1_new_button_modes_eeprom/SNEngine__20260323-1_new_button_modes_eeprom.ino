/*
  SNEngine - NES/SNES2PCE controller adapter
  by RetroCityRansom (www.youtube.com/@RetroCityRansom)

  Features:
  - No noticable input lag (at least for me)
  - Autofire with adjustable speed (see below)
  - Multitap compatible (tested with 2 SNEngine adapters and 1 original CoreGrafx gamepad simultaneously)
  - 8 selectable button layouts (see below)

  Hotkey combos:
  - SELECT + UP: change autofire speed
  - SELECT + DOWN: cycle through different button layouts
*/
#include <EEPROM.h>

#define ADDR_TURBO_SPEED 0
#define ADDR_BUTTON_SWAP 1

#define SNES_CLOCK_BIT 0
#define SNES_LATCH_BIT 1
#define SNES_DATA_BIT  2

// SNES Button Map
#define SNES_B       0
#define SNES_Y       1
#define SNES_SELECT  2
#define SNES_START   3
#define SNES_UP      4
#define SNES_DOWN    5
#define SNES_LEFT    6
#define SNES_RIGHT   7
#define SNES_A       8
#define SNES_X       9
#define SNES_L       10
#define SNES_R       11

unsigned long lastAutoFire = 0;
bool turboState = false;
uint8_t turboSpeed = 16; 
bool comboLock = false;
uint8_t swapMode = 0; // Button layouts 0: Layout A, 1: Layout B, 2: Layout C, 3: Layout D, ...
const uint8_t MAX_MODES = 8;

void setup() {
  // Restore saved button settings
  turboSpeed = EEPROM.read(ADDR_TURBO_SPEED);
  if (turboSpeed != 16 && turboSpeed != 33) turboSpeed = 16;
  swapMode = EEPROM.read(ADDR_BUTTON_SWAP);
  if (swapMode >= MAX_MODES) swapMode = 0;

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
  bool turboTick = (currentMillis % (turboSpeed * 2)) < turboSpeed;
    // Stop interrupts to get stable timing
  noInterrupts();
  delay(16.67); // according to protocol
  // Read SNES pad input
  uint16_t pressedButtons = 0;
  PORTC |= (1 << SNES_LATCH_BIT); // set latch HIGH
  delayMicroseconds(12);
  PORTC &= ~(1 << SNES_LATCH_BIT); // set latch LOW
  
  for (uint8_t i = 0; i < 12; i++) {
    if (!(PINC & (1 << SNES_DATA_BIT))) pressedButtons |= (1 << i);
    PORTC |= (1 << SNES_CLOCK_BIT); // set clock HIGH
    delayMicroseconds(6);
    PORTC &= ~(1 << SNES_CLOCK_BIT); // set clock LOW
    delayMicroseconds(6);
  }

  // Restart interrupts for millis() and autofire
  interrupts();

  // Combo detection logic
  if ((pressedButtons & (1 << SNES_SELECT)) && (pressedButtons & (1 << SNES_UP))) {
    if (!comboLock) {
    turboSpeed = (turboSpeed == 33) ? 16 : 33;
      comboLock = true;
    }
  } else if ((pressedButtons & (1 << SNES_SELECT)) && (pressedButtons & (1 << SNES_DOWN))) {
    if (!comboLock) {
        swapMode = (swapMode + 1) % MAX_MODES;
        comboLock = true; 
    }
  } else if ((pressedButtons & (1 << SNES_SELECT)) && (pressedButtons & (1 << SNES_RIGHT))) {
    if (!comboLock) {
        EEPROM.update(ADDR_TURBO_SPEED, turboSpeed);
        EEPROM.update(ADDR_BUTTON_SWAP, swapMode);
        comboLock = true; 
    }
  }
  else {
    comboLock = false;
  }

if (!(pressedButtons & (1 << SNES_SELECT)) && !(pressedButtons & (1 << SNES_DOWN))) {
    comboLock = false;
}

  // Autofire
  if (millis() - lastAutoFire > turboSpeed) {
    turboState = !turboState;
    lastAutoFire = millis();
  }

  // Reset 
  uint8_t outPortD = 0xF0; 
  uint8_t outPortB = 0x0F; 

  // PCE I & II Mapping
  bool btnI = (pressedButtons & (1 << SNES_B)) || (pressedButtons & (1 << SNES_X)) || ((pressedButtons & (1 << SNES_L)) && turboState);
  bool btnII = (pressedButtons & (1 << SNES_A)) || (pressedButtons & (1 << SNES_Y)) || ((pressedButtons & (1 << SNES_R)) && turboState);

  uint8_t outI, outII;
  switch (swapMode) {
      case 0: // Layout A: B/X=I, L=Turbo I, A/Y=II, R=Turbo II
          outI  = (pressedButtons & (1 << SNES_B)) || (pressedButtons & (1 << SNES_X)) || 
                  ((pressedButtons & (1 << SNES_L)) && turboTick) ? 1 : 0;
          outII = (pressedButtons & (1 << SNES_A)) || (pressedButtons & (1 << SNES_Y)) || 
                  ((pressedButtons & (1 << SNES_R)) && turboTick) ? 1 : 0;
          break;

      case 1: // Layout B (=inverted layout A): B/X=II, L=Turbo II, A/Y=I, R=Turbo I
          outII = (pressedButtons & (1 << SNES_B)) || (pressedButtons & (1 << SNES_X)) || 
                  ((pressedButtons & (1 << SNES_L)) && turboTick) ? 1 : 0;
          outI  = (pressedButtons & (1 << SNES_A)) || (pressedButtons & (1 << SNES_Y)) || 
                  ((pressedButtons & (1 << SNES_R)) && turboTick) ? 1 : 0;
          break;

      case 2: // Layout C: B/L=I, Y=Turbo I, A/R=II, X=Turbo II
          outI  = (pressedButtons & (1 << SNES_B)) || (pressedButtons & (1 << SNES_L)) || 
                  ((pressedButtons & (1 << SNES_Y)) && turboTick) ? 1 : 0;
          outII = (pressedButtons & (1 << SNES_A)) || (pressedButtons & (1 << SNES_R)) || 
                  ((pressedButtons & (1 << SNES_X)) && turboTick) ? 1 : 0;
          break;

      case 3: // Layout D (=inverted layout C): B/L=II, Y=Turbo II, A/R=I, X=Turbo I
          outII  = (pressedButtons & (1 << SNES_B)) || (pressedButtons & (1 << SNES_L)) || 
                  ((pressedButtons & (1 << SNES_Y)) && turboTick) ? 1 : 0;
          outI = (pressedButtons & (1 << SNES_A)) || (pressedButtons & (1 << SNES_R)) || 
                  ((pressedButtons & (1 << SNES_X)) && turboTick) ? 1 : 0;
          break;

      case 4: // Layout E: X/L=I, B=Turbo I, Y/R=II, A=Turbo II
          outI  = (pressedButtons & (1 << SNES_X)) || (pressedButtons & (1 << SNES_L)) || 
                  ((pressedButtons & (1 << SNES_B)) && turboTick) ? 1 : 0;
          outII = (pressedButtons & (1 << SNES_Y)) || (pressedButtons & (1 << SNES_R)) || 
                  ((pressedButtons & (1 << SNES_A)) && turboTick) ? 1 : 0;
          break;

      case 5: // Layout F (=inverted layout E): X/L=II, B=Turbo II, Y/R=I, A=Turbo I
          outII = (pressedButtons & (1 << SNES_X)) || (pressedButtons & (1 << SNES_L)) || 
                  ((pressedButtons & (1 << SNES_B)) && turboTick) ? 1 : 0;
          outI  = (pressedButtons & (1 << SNES_Y)) || (pressedButtons & (1 << SNES_R)) || 
                  ((pressedButtons & (1 << SNES_A)) && turboTick) ? 1 : 0;
          break;

      case 6: // Layout G: X/L=I, Y=Turbo I, A/R=II, B=Turbo II
          outI  = (pressedButtons & (1 << SNES_X)) || (pressedButtons & (1 << SNES_L)) || 
                  ((pressedButtons & (1 << SNES_Y)) && turboTick) ? 1 : 0;
          outII = (pressedButtons & (1 << SNES_A)) || (pressedButtons & (1 << SNES_R)) || 
                  ((pressedButtons & (1 << SNES_B)) && turboTick) ? 1 : 0;
          break;

      case 7: // Layout H: X/L=II, Y=Turbo II, A/R=I, B=Turbo I
          outII  = (pressedButtons & (1 << SNES_X)) || (pressedButtons & (1 << SNES_L)) || 
                  ((pressedButtons & (1 << SNES_Y)) && turboTick) ? 1 : 0;
          outI = (pressedButtons & (1 << SNES_A)) || (pressedButtons & (1 << SNES_R)) || 
                  ((pressedButtons & (1 << SNES_B)) && turboTick) ? 1 : 0;
          break;
  }
  if (outI == 1)  outPortD &= ~(1 << 4);
  if (outII == 1) outPortD &= ~(1 << 5);
  if (pressedButtons & (1 << SNES_START))  outPortD &= ~(1 << 6);
  if (pressedButtons & (1 << SNES_SELECT)) outPortD &= ~(1 << 7);
  if (pressedButtons & (1 << SNES_UP))    outPortB &= ~(1 << 0);
  if (pressedButtons & (1 << SNES_RIGHT)) outPortB &= ~(1 << 1);
  if (pressedButtons & (1 << SNES_LEFT))  outPortB &= ~(1 << 2);
  if (pressedButtons & (1 << SNES_DOWN))  outPortB &= ~(1 << 3);

  // push output to multiplexer
  PORTD = (PORTD & 0x0F) | (outPortD & 0xF0);
  PORTB = (PORTB & 0xF0) | (outPortB & 0x0F);
}
