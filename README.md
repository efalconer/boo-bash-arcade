# Boo-Bash Arcade

A two-player whack-a-mole style arcade game built with Arduino. Features motorized pop-up characters, LED lighting effects, and sound. Originally created for Halloween 2025.

## How It Works

Two players compete by pressing illuminated buttons as fast as possible. Each successful hit advances their character via stepper motor. The first player to reach the end position—or the player with the highest score when time runs out—wins.

### Game Flow

1. **Attract Mode** - Start button blinks, ambient music plays, rainbow LEDs cycle
2. **Countdown** - Buttons light up sequentially, LED strip turns blue
3. **Gameplay** (30 seconds) - Random buttons light up for each player; press them before they expire
4. **Finish** - Winner's buttons blink, motors reset to starting position

Difficulty increases during gameplay—more target buttons appear simultaneously every 5 seconds.

## Hardware Requirements

### Microcontroller
- Arduino Mega (or compatible board with sufficient pins)

### Motor Control
- 2x Adafruit Motor Shield V2 (stacked via I2C)
  - Top shield: address 0x60 (default)
  - Bottom shield: address 0x61 (rightmost jumper closed)
- 2x NEMA-17 stepper motors (200 steps/revolution)

### Inputs
- 18x Arcade buttons (9 per player, 3x3 grid)
- 1x Start button
- 1x Reset button
- 2x Volume buttons (up/down)

### Outputs
- 20x LEDs (one per button)
- 44x WS2812B addressable LED strip (game timer display)
- Adafruit Sound Board with SD card

## Pin Configuration

| Component | Pins |
|-----------|------|
| Player 1 Buttons | 24, 26, 28, 36, 38, 40, 48, 50, 52 |
| Player 1 LEDs | 14, 16, 22, 30, 32, 34, 42, 44, 46 |
| Player 2 Buttons | 25, 27, 29, 37, 39, 41, 49, 51, 53 |
| Player 2 LEDs | 15, 17, 23, 31, 33, 35, 43, 45, 47 |
| Start Button/LED | 2 / 5 |
| Reset Button/LED | 3 / 4 |
| Volume Up/Down | 9 / 8 |
| LED Strip | 7 |
| Sound Board (TX/RX/RST/ACT) | 12 / 11 / 13 / 10 |

## Required Libraries

- `Adafruit_MotorShield`
- `AccelStepper`
- `FastLED`
- `Adafruit_Soundboard`
- `SoftwareSerial` (built-in)
- `Wire` (built-in)

## Audio Files

Place these files on the sound board's storage:

| File | Purpose |
|------|---------|
| AMBIENT1-3.OGG | Background music (attract mode) |
| START1.OGG | Game start |
| HIT1-9.WAV | Button hit sounds |
| WIN1.WAV | Victory sound |
| REWIND1.OGG | Motor reset sound |

## Tuning Parameters

These values can be adjusted in the code:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `GAME_TIME` | 30000ms | Game duration |
| `MAX_MOTOR_POSITION` | 2700 | Steps to win |
| `POP_DURATION` | 800ms | How long buttons stay lit |
| `POP_DELAY` | 500ms | Minimum time between new targets |
| `INCREASE_TARGETS_EVERY` | 5000ms | Difficulty ramp interval |
| `CHARACTER_STEPS` | 200 | Motor steps per successful hit |

## Usage

1. Power on the arcade machine
2. Press the **Start** button to begin
3. Press lit buttons as fast as possible
4. First to max position or highest score after 30 seconds wins
5. Press **Reset** to return to attract mode
