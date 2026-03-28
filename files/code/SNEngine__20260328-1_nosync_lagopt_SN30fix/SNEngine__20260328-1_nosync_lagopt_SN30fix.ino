/*
  SNEngine - NES/SNES2PCE controller adapter
  by RetroCityRansom (www.youtube.com/@RetroCityRansom)

  Features:
  - No noticable input lag (at least for me)
  - Autofire with adjustable speed (see below)
  - Multitap compatible (tested with 2 SNEngine adapters and 1 original CoreGrafx gamepad simultaneously)
  - 8 switchable button layouts (SELECT + DOWN)
  - Save settings to EEPROM (SELECT + RIGHT)

  Hotkey combos:
  - SELECT + UP: change autofire speed
  - SELECT + DOWN: cycle through 8 different button layouts
  - SELECT + RIGHT: Save chosen autofire setting and button mapping as preset
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

uint8_t turboSpeed = 16; 
bool comboLock = false;
uint8_t swapMode = 0;
const uint8_t MAX_MODES = 8; // Button layouts 0: Layout A, 1: Layout B, 2: Layout C, 3: Layout D, ...

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

  // Read SNES pad input (STABILIZED 16-BIT)
  uint16_t b = 0; // pressedButtons
  PORTC |= (1 << SNES_LATCH_BIT); // set latch HIGH
  // delayMicroseconds(12);
  delayMicroseconds(6);
  PORTC &= ~(1 << SNES_LATCH_BIT); // set latch LOW
  //delayMicroseconds(6); 
  delayMicroseconds(3); 
  
  for (uint8_t i = 0; i < 16; i++) {
    if (!(PINC & (1 << SNES_DATA_BIT))) {
      if (i < 12) b |= (1UL << i);
    }
    PORTC |= (1 << SNES_CLOCK_BIT); // set clock HIGH
    // delayMicroseconds(6);
    delayMicroseconds(3);
    PORTC &= ~(1 << SNES_CLOCK_BIT); // set clock LOW
    //delayMicroseconds(6);
    delayMicroseconds(3);
  }

  // Restart interrupts for millis() and autofire
  interrupts();

  // Combo detection logic
  if (b & (1 << SNES_SELECT)) {
    if (!comboLock) {
      if (b & (1 << SNES_UP)) { turboSpeed = (turboSpeed == 33) ? 16 : 33; comboLock = true; }
      else if (b & (1 << SNES_DOWN)) { swapMode = (swapMode + 1) % MAX_MODES; comboLock = true; }
      else if (b & (1 << SNES_RIGHT)) { EEPROM.update(ADDR_TURBO_SPEED, turboSpeed); EEPROM.update(ADDR_BUTTON_SWAP, swapMode); comboLock = true; }
    }
  } else {
    comboLock = false;
  }

  // PCE I & II Mapping (OPTIMIZED PERFORMANCE CASES - FIXED L/R CLAMPING)
  bool oI = 0, oII = 0;
  switch (swapMode) {
    // Layout A: B/X=I, L=Turbo I, A/Y=II, R=Turbo II
    case 0: oI=((b&(1<<SNES_B))||(b&(1<<SNES_X))||((b&(1<<SNES_L))&&turboTick)); oII=((b&(1<<SNES_A))||(b&(1<<SNES_Y))||((b&(1<<SNES_R))&&turboTick)); break;
    // Layout B (=inverted layout A): B/X=II, L=Turbo II, A/Y=I, R=Turbo I
    case 1: oII=((b&(1<<SNES_B))||(b&(1<<SNES_X))||((b&(1<<SNES_L))&&turboTick)); oI=((b&(1<<SNES_A))||(b&(1<<SNES_Y))||((b&(1<<SNES_R))&&turboTick)); break;
    // Layout C: B/L=I, Y=Turbo I, A/R=II, X=Turbo II
    case 2: oI=((b&(1<<SNES_B))||(b&(1<<SNES_L))||((b&(1<<SNES_Y))&&turboTick)); oII=((b&(1<<SNES_A))||(b&(1<<SNES_R))||((b&(1<<SNES_X))&&turboTick)); break;
    // Layout D (=inverted layout C): B/L=II, Y=Turbo II, A/R=I, X=Turbo I
    case 3: oII=((b&(1<<SNES_B))||(b&(1<<SNES_L))||((b&(1<<SNES_Y))&&turboTick)); oI=((b&(1<<SNES_A))||(b&(1<<SNES_R))||((b&(1<<SNES_X))&&turboTick)); break;
    // Layout E: X/L=I, B=Turbo I, Y/R=II, A=Turbo II
    case 4: oI=((b&(1<<SNES_X))||(b&(1<<SNES_L))||((b&(1<<SNES_B))&&turboTick)); oII=((b&(1<<SNES_Y))||(b&(1<<SNES_R))||((b&(1<<SNES_A))&&turboTick)); break;
    // Layout F (=inverted layout E): X/L=II, B=Turbo II, Y/R=I, A=Turbo I
    case 5: oII=((b&(1<<SNES_X))||(b&(1<<SNES_L))||((b&(1<<SNES_B))&&turboTick)); oI=((b&(1<<SNES_Y))||(b&(1<<SNES_R))||((b&(1<<SNES_A))&&turboTick)); break;
    // Layout G: X/L=I, Y=Turbo I, A/R=II, B=Turbo II
    case 6: oI=((b&(1<<SNES_X))||(b&(1<<SNES_L))||((b&(1<<SNES_Y))&&turboTick)); oII=((b&(1<<SNES_A))||(b&(1<<SNES_R))||((b&(1<<SNES_B))&&turboTick)); break;
    // Layout H: X/L=II, Y=Turbo II, A/R=I, B=Turbo I
    case 7: oII=((b&(1<<SNES_X))||(b&(1<<SNES_L))||((b&(1<<SNES_Y))&&turboTick)); oI=((b&(1<<SNES_A))||(b&(1<<SNES_R))||((b&(1<<SNES_B))&&turboTick)); break;
  }

  // Reset work variables for output ports (PCE: 0 = active)
  uint8_t outPortD = 0xF0; 
  uint8_t outPortB = 0x0F; 

  // Branchless Bit-Mapping to Output Ports
  outPortD ^= ((uint8_t)oI << 4);
  outPortD ^= ((uint8_t)oII << 5);
  outPortD ^= ((uint8_t)((b >> SNES_START) & 1) << 6);
  outPortD ^= ((uint8_t)((b >> SNES_SELECT) & 1) << 7);

  outPortB ^= ((uint8_t)((b >> SNES_UP) & 1) << 0);
  outPortB ^= ((uint8_t)((b >> SNES_RIGHT) & 1) << 1);
  outPortB ^= ((uint8_t)((b >> SNES_LEFT) & 1) << 2);
  outPortB ^= ((uint8_t)((b >> SNES_DOWN) & 1) << 3);

  // push output to multiplexer
  PORTD = (PORTD & 0x0F) | (outPortD & 0xF0);
  PORTB = (PORTB & 0xF0) | (outPortB & 0x0F);

  //delay(12); 
}
