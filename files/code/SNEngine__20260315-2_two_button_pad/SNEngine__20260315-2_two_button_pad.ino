/*
  SNEngine - NES/SNES2PCE controller adapter
  by RetroCityRansom (www.youtube.com/@RetroCityRansom)

  Features:
  - No noticable input lag (at least for me)
  - Autofire with adjustable speed (see below)
  - Multitap compatible (tested with 2 SNEngine adapters and 1 original CoreGrafx gamepad simultaneously)
  - 2 switchable button layouts (see below)

  Hotkey combos:
  - SELECT + UP: change autofire speed
  - SELECT + DOWN: swap button 1 and 2

  Notes on button mapping:
  - The default mapping SNES => PCE is:
    - B, X => Button I
    - L => Autofire Button I
    - A, Y => Button II
    - R => Autofire Button II
    - The rest is as to be expected (up => up, ..., start => start, ...)
*/

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
uint8_t turboSpeed = 33; 
bool comboLock = false;
bool swapButtons = false;

void setup() {
  // SNES Setup
  DDRC |= (1 << SNES_LATCH_BIT) | (1 << SNES_CLOCK_BIT);
  DDRC &= ~(1 << SNES_DATA_BIT);
  PORTC |= (1 << SNES_DATA_BIT);
  
  // Define port pins as outputs
  DDRD |= 0xF0; 
  DDRB |= 0x0F;
}

void loop() {
  // Stop interrupts to get stable timing
  noInterrupts();
  
  // Read SNES pad input
  uint16_t snesState = 0;
  PORTC |= (1 << SNES_LATCH_BIT);
  delayMicroseconds(12); // remove to tweak
  PORTC &= ~(1 << SNES_LATCH_BIT);
  delayMicroseconds(12); // remove to tweak

  for (uint8_t i = 0; i < 12; i++) {
    if (!(PINC & (1 << SNES_DATA_BIT))) snesState |= (1 << i);
    PORTC |= (1 << SNES_CLOCK_BIT);
    delayMicroseconds(4); // remove to tweak
    PORTC &= ~(1 << SNES_CLOCK_BIT);
    delayMicroseconds(4); // remove to tweak
  }

  // Restart interrupts for millis() and autofire
  interrupts();

  // Combo detection logic
  if ((snesState & (1 << SNES_SELECT)) && (snesState & (1 << SNES_UP))) {
    if (!comboLock) {
      turboSpeed = (turboSpeed == 33) ? 66 : 33;
      comboLock = true;
    }
  } else if ((snesState & (1 << SNES_SELECT)) && (snesState & (1 << SNES_DOWN))) {
    if (!comboLock) {
      swapButtons = !swapButtons;
      comboLock = true;
    }
  } else {
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
  bool btnI = (snesState & (1 << SNES_B)) || (snesState & (1 << SNES_X)) || ((snesState & (1 << SNES_L)) && turboState);
  bool btnII = (snesState & (1 << SNES_A)) || (snesState & (1 << SNES_Y)) || ((snesState & (1 << SNES_R)) && turboState);

  if (!swapButtons) {
    if (btnI)  outPortD &= ~(1 << 4);
    if (btnII) outPortD &= ~(1 << 5);
  } else {
    if (btnII) outPortD &= ~(1 << 4);
    if (btnI)  outPortD &= ~(1 << 5);
  }
  
  if (snesState & (1 << SNES_START))  outPortD &= ~(1 << 6);
  if (snesState & (1 << SNES_SELECT)) outPortD &= ~(1 << 7);

  if (snesState & (1 << SNES_UP))    outPortB &= ~(1 << 0);
  if (snesState & (1 << SNES_RIGHT)) outPortB &= ~(1 << 1);
  if (snesState & (1 << SNES_LEFT))  outPortB &= ~(1 << 2);
  if (snesState & (1 << SNES_DOWN))  outPortB &= ~(1 << 3);

  // push output to multiplexer
  PORTD = (PORTD & 0x0F) | (outPortD & 0xF0);
  PORTB = (PORTB & 0xF0) | (outPortB & 0x0F);
}