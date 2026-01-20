/* note pull RST pin on SFX card for it to go into USB mode*/

#include <SoftwareSerial.h>
#include "Adafruit_Soundboard.h"
#include <Wire.h>
#include <AccelStepper.h>
#include <Adafruit_MotorShield.h>
#include <FastLED.h>

const bool isBackgroundMusicOn = true;

// Global variable to track the current hue, 8-bit integer wraps around at 255
uint8_t gHue = 0; 

int buttonPin = 0; 
int ledPin = 0; 
int buttonState = 0;
int startButtonPin = 2;
int startLedPin = 5;
int resetButtonPin = 3;
int resetLedPin = 4;

const int MAX_MOTOR_POSITION = 2700;

unsigned long attractDelay = 300;
unsigned long lastAttractUpdate = 0;
const unsigned long BUTTON_POP_DURATION = 800;
const unsigned long BUTTON_POP_DELAY = 500;
const unsigned long TARGET_INCREASE_DELAY = 5000;
// game settings
unsigned long gameDuration = 30000; // 30 seconds 
unsigned long gameStartTime = 0;
unsigned long nextTargetIncreaseTime = 0;

// motorshield & stepper motors
const int CHARACTER_STEPS = 200;

// LED strip
#define NUM_LEDS 44
#define LED_STRIP_PIN 7
// Define the array of leds
CRGBArray<NUM_LEDS> stripLeds;

unsigned long lastLedUpdate = 0;

Adafruit_MotorShield AFMSbot(0x61); // Rightmost jumper closed
Adafruit_MotorShield AFMStop(0x60); // Default address, no jumpers

// Connect two steppers with 200 steps per revolution (1.8 degree)
// to the top shield
Adafruit_StepperMotor *myStepper1 = AFMStop.getStepper(200, 1);
Adafruit_StepperMotor *myStepper2 = AFMStop.getStepper(200, 2);

long character1Position = 0;
long character2Position = 0;


// soundboard
// Choose any two pins that can be used with SoftwareSerial to RX & TX
#define SFX_TX 12
#define SFX_RX 11

// Connect to the RST pin on the Sound Board
#define SFX_RST 13

// You can also monitor the ACT pin for when audio is playing!
#define SFX_ACT 10

#define VOLUME_UP 9
#define VOLUME_DOWN 8
unsigned long last_volume_change = 0;
const long VOLUME_DELAY = 100;


const char *ambFilenames[] = {
  "AMBIENT1OGG",
  "AMBIENT2OGG",
  "AMBIENT3OGG",
};
const char *fxFilenames[] = {
  "HIT1    WAV",
  "HIT2    WAV",
  "HIT3    WAV",
  //"HIT4    WAV", // broken
  //"HIT5    WAV", // broken
  "HIT6    WAV",
  "HIT7    WAV",
  "HIT8    WAV",
  "HIT9    WAV",
};

const char *winFilenames[] = {
  "WIN1    WAV",
};

const char *startFilenames[] = {
  //"START1  WAV",
  "START1  OGG",
};

const char *rewindFilenames[] = {
  "REWIND1 OGG",
};

CRGB ledColors[] = {
  CRGB::Green,
  /*
  CRGB::Red,
  CRGB::Blue,
  CRGB::Yellow,
  CRGB::Orange,
  CRGB::Purple,
  */
};
  

#define numFXFiles (sizeof(fxFilenames)/sizeof(char *)) 
#define numAmbFiles (sizeof(ambFilenames)/sizeof(char *)) 
#define numWinFiles (sizeof(winFilenames)/sizeof(char *)) 
#define numStartFiles (sizeof(startFilenames)/sizeof(char *))
#define numRewindFiles (sizeof(rewindFilenames)/sizeof(char *)) 

// we'll be using software serial
SoftwareSerial ss = SoftwareSerial(SFX_TX, SFX_RX);

// pass the software serial to Adafruit_soundboard, the second
// argument is the debug port (not used really) and the third 
// arg is the reset pin
Adafruit_Soundboard sfx = Adafruit_Soundboard(&ss, NULL, SFX_RST);


enum GameState {
  ATTRACT, // waiting for game to start
  COUNTDOWN, // start button pushed, counting down to game start
  RUNNING, // game is executing
  RESETTING, // game is resetting state
  FINISHED // game completed
};

// struct to track the current state information of each button
// including the light and button state for debouncing
struct ArcadeButton {
  
  enum ButtonMode {
    REGULAR,
    DECAY,
    BLINK,
    GAME,
  };

  ButtonMode mode = REGULAR;

  int buttonPin;
  int ledPin;
  unsigned long lastDebounceTime = 0;
  unsigned long debounceDelay = 50;
  int buttonState = HIGH;
  int ledState = LOW;

  unsigned long lightDecayTime;
  unsigned long nextBlinkTime;
  long blinkDelay;

  bool expired = false; // whether decay counter expired

  ArcadeButton(int buttonPin, int ledPin) {
    this->buttonPin = buttonPin;
    this->ledPin = ledPin;

    // setup pin modes
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);
  }

  // get state with debounce
  int getState() {
    int reading = digitalRead(buttonPin);

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
          buttonState = reading;
        }
        lastDebounceTime = millis();
    }

    return buttonState;
  }
  
  void reset() {
    turnOff();
    ledState = LOW;
    mode = REGULAR;
    expired = false;
    lightDecayTime = 0;
    nextBlinkTime = 0;
  }

  void turnOn() {
    digitalWrite(ledPin, HIGH);
    ledState = HIGH;
  }
  void turnOff() { 
    digitalWrite(ledPin, LOW);
    ledState = LOW;
  }

  void decayLight(long ms) {
    // turn on the led for the set amount of time
    mode = DECAY;
    lightDecayTime = millis() + ms;
    turnOn();
    ledState = HIGH;
    expired = false;
  }

  void blinkLight(long ms) {
    // turn led on and off ever ms
    mode = BLINK;
    nextBlinkTime = millis() + ms;
    blinkDelay = ms;
    turnOn();
  }

  // each button's update function needs to be called in the loop to update
  // the current state/behavior
  void update() {
    switch (mode) {
      case REGULAR:
        break;
      case DECAY:
        // check if the decay time has passed
        if (millis() > lightDecayTime) {
          turnOff();
          mode = REGULAR;
          expired = true;
        }
        break;
      case BLINK:
        if (millis() > nextBlinkTime) {
          if (ledState == LOW) {
            turnOn();
          } else {
            turnOff();
          }
          nextBlinkTime = millis() + blinkDelay;
        }
    }
  }
};


struct PlayerState {
  int score = 0;
  int targetButtonsPopped = 0;
  int numButtonsPopped = 0;
  unsigned long buttonPopTime = 0;
  bool buttonsPopped[9] = {false};

  void reset() {
    score = 0;
    targetButtonsPopped = 1;
    numButtonsPopped = 0;
    buttonPopTime = 0;
    for (int i=0;i<9;i++) {
      buttonsPopped[i] = false;
    }
  }
};

ArcadeButton startButton = ArcadeButton(startButtonPin, startLedPin);
ArcadeButton resetButton = ArcadeButton(resetButtonPin, resetLedPin);

GameState currentState = ATTRACT;

ArcadeButton player1Buttons[3][3] = {
  {ArcadeButton(24, 14), ArcadeButton(26, 16), ArcadeButton(28, 22)},
  {ArcadeButton(36, 30), ArcadeButton(38, 32), ArcadeButton(40, 34)},
  {ArcadeButton(48, 42), ArcadeButton(50, 44), ArcadeButton(52, 46)}
};

ArcadeButton player2Buttons[3][3] = {
  {ArcadeButton(25, 15), ArcadeButton(27, 17), ArcadeButton(29, 23)},
  {ArcadeButton(37, 31), ArcadeButton(39, 33), ArcadeButton(41, 35)},
  {ArcadeButton(49, 43), ArcadeButton(51, 45), ArcadeButton(53, 47)}
};

void flushInput() {
  // Read all available serial input to flush pending data.
  uint16_t timeoutloop = 0;
  while (timeoutloop++ < 40) {
    while(ss.available()) {
      ss.read();
      timeoutloop = 0;  // If char was received reset the timer
    }
    delay(1);
  }
}

void forwardstep1() {
  myStepper1->onestep(FORWARD, DOUBLE);
}

void backwardstep1() {
  myStepper1->onestep(BACKWARD, DOUBLE);
}

// wrappers for the second motor!
void forwardstep2() {
  myStepper2->onestep(FORWARD, DOUBLE);
}
void backwardstep2() {
  myStepper2->onestep(BACKWARD, DOUBLE);
}

// Now we'll wrap the 3 steppers in an AccelStepper object
AccelStepper stepper1(forwardstep1, backwardstep1);
AccelStepper stepper2(forwardstep2, backwardstep2);

void setup() {
  // initialize serial monitor
  Serial.begin(115200);

  // start the serial interface for our sound
  ss.begin(9600);
  pinMode(SFX_ACT, INPUT);

  // reduce the volume a bit (for testing)
  for (int i=0; i<25; i++) {
    sfx.volDown();
  }
  

  // setup the volume buttons
  pinMode(VOLUME_UP, INPUT_PULLUP);
  pinMode(VOLUME_DOWN, INPUT_PULLUP);

  // initialize the random number generator
  randomSeed(analogRead(A0)); 

  
  // initialize stepper motors
  AFMSbot.begin(); // Start the bottom shield
  AFMStop.begin(); // Start the top shield

  stepper1.setMaxSpeed(1000.0);
  //stepper1.setMaxSpeed(16.0);
  //stepper1.setSpeed(200.0);
  stepper1.setAcceleration(250.0);
  stepper1.moveTo(0);

  stepper2.setMaxSpeed(1000.0);
  //stepper2.setSpeed(200.0);
  //stepper2.setMaxSpeed(16.0);
  stepper2.setAcceleration(250.0);
  stepper2.moveTo(0);

  // led strip
  FastLED.addLeds<WS2812B, LED_STRIP_PIN, GRB>(stripLeds, NUM_LEDS);  // GRB ordering is typical
  resetLedClock();

  // start attract mode
  startAttract();

}

void stopAudio() {
  sfx.stop();
}

CRGB getRandomColor() {
  return ledColors[random(sizeof(ledColors)-1)];
}

void playAudioFile(const char* filename, bool force) {

  // if something is playing, stop it
  if (digitalRead(SFX_ACT) == LOW && force) {
    stopAudio();
  }
  
  if (!sfx.playTrack(const_cast<char *>(filename))) {
    Serial.println("Failed to play audio");
  }
  
}

void playFxFile(const char* filename) {
  stopAudio(); // always play the FX file
  if (!sfx.playTrack(const_cast<char *>(filename))) {
    Serial.println("Failed to play audio");
  }
  
}

void checkAndPlayAmbience() {
  // if nothing is playing start the ambience track
  if (digitalRead(SFX_ACT) == HIGH) {
    playAudioFile(ambFilenames[random(numAmbFiles)], false);
  }
}

void playRandomHitSound() {
  playAudioFile(fxFilenames[random(numFXFiles)], true);
}

void playRandomWinSound() {
  playAudioFile(winFilenames[random(numWinFiles)], true);
}

void playRandomWStartSound() {
  playAudioFile(startFilenames[random(numStartFiles)], true);
}

void playRandomWRewindSound() {
  playAudioFile(rewindFilenames[random(numRewindFiles)], true);
}

void lightButton(int player, int button) {
  int row = button/3;
  int col = button%3;

  if (player==1) {
    player1Buttons[row][col].turnOn();
  } else {
    player2Buttons[row][col].turnOn();
  }
}

void turnOffButton(int player, int button) {
  int row = button/3;
  int col = button%3;

  if (player==1) {
    player1Buttons[row][col].turnOff();
  } else {
    player2Buttons[row][col].turnOff();
  }
}

void turnOffAllButtons() {
  for (int i = 0; i<9; i++) {
    turnOffButton(1, i);
    turnOffButton(2, i);
  }

  startButton.turnOff();
  resetButton.turnOff();
}

void resetAllButtons() {
  int row, col;
  for (int i = 0; i<9; i++) {
    row = i/3;
    col = i%3;

    player1Buttons[row][col].reset();
    player2Buttons[row][col].reset();
  }

  startButton.reset();
  resetButton.reset();
}

void turnOnAllButtons() {
  for (int i = 0; i<9; i++) {
    lightButton(1, i);
    lightButton(2, i);
  }
}

void turnOnAllPlayerButtons(int player) {
  for (int i = 0; i<9; i++) {
    if (player == 1) {
      lightButton(1, i);
    } else {
      lightButton(2, i);
    }
  }
}

void blinkAllButtons(int cycles) {
  resetAllButtons();
  
  for (int c = 0; c<cycles; c++) {
    turnOnAllButtons();
    delay(200);
    turnOffAllButtons();
    delay(200);
  }
}

void blinkPlayerButtons(int player, int cycles) {
  resetAllButtons();
  
  for (int c = 0; c<cycles; c++) {
    turnOnAllPlayerButtons(player);
    delay(200);
    turnOffAllButtons();
    delay(200);
  }
}

bool startButtonPushed() {
  return startButton.getState() == LOW;
}

bool resetButtonPushed() {
  return resetButton.getState() == LOW;
}

void writeDebug(int row, int col, int player) {
  Serial.print("Player: ");
  Serial.print(player);
  Serial.print(" Row:");
  Serial.print(row);
  Serial.print(" Col:");
  Serial.println(col);
}

void countdownToStart() {
  Serial.println("Countdown to start");
  playRandomWStartSound();
  resetAllButtons();

  stripLeds.fill_solid(CRGB::Blue);
  FastLED.show();

  // cycle down the buttons in order as a countdown for now
  for (int i = 0; i<9; i++) {
    lightButton(1, i);
    lightButton(2, i);
    delay(350);
  }

  turnOffAllButtons();
}


void attractDisplayUpdate() {
  // play the background music
  if (isBackgroundMusicOn) {
    checkAndPlayAmbience();
  }
  // randomly light a button
  if (millis() > lastAttractUpdate + attractDelay) {
    lastAttractUpdate = millis();
    attractDelay = random(100,300);
    int button = random(9);
    int row = button/3;
    int col = button%3;
    if (random(2)==0) {
      player1Buttons[row][col].decayLight(random(400,2000));
    } else {
      player2Buttons[row][col].decayLight(random(400,2000));
    }
  }

  // update LED strip
  if (millis() > lastLedUpdate + 20) {
    gHue++;
    fill_rainbow(stripLeds, NUM_LEDS, gHue, 7); 
    lastLedUpdate = millis();
  }
  
}

void startAttract() {
  currentState = ATTRACT;
  startButton.blinkLight(1000);
  stopAudio(); // stop any other background FX
}

void updateButtons() {
  resetButton.update();
  startButton.update();
  for (int row=0; row<3; row++) {
    for (int col=0; col<3; col++) {
      player1Buttons[row][col].update();
      player2Buttons[row][col].update();
    }
  }
}


PlayerState p1 = PlayerState();
PlayerState p2 = PlayerState();


void popRandomButton(PlayerState* p, ArcadeButton buttons[][3]) {
  bool buttonFound = false;
  int button = 0;
  while (!buttonFound) {
    button = random(9);
    if (p->buttonsPopped[button]) continue; // already popped, try again
    buttonFound = true; // break out of loop on next try 
    
    // set the button's state
    p->buttonsPopped[button] = true;
    // turn it on with decay
    buttons[button/3][button%3].decayLight(BUTTON_POP_DURATION);
  }
}

void startGame() {
  currentState = RUNNING;
  resetAllButtons();

  stopAudio(); // stop any other background FX

  // reset game state
  p1.reset();
  p2.reset();

  gameStartTime = millis();
  nextTargetIncreaseTime = millis() + TARGET_INCREASE_DELAY;

  startClock();
}

void resetCharacters() {
  playRandomWRewindSound();
  currentState = RESETTING;
  Serial.print("Character 1 position: ");
  Serial.println(character1Position);
  Serial.print("Motor 1 position: ");
  Serial.println(stepper1.currentPosition());
  stepper1.moveTo(0);
  character1Position = 0;

  // motor 2 is in opposite direction
  Serial.print("Character 2 position: ");
  Serial.println(character2Position);
  Serial.print("Motor 2 position: ");
  Serial.println(stepper2.currentPosition());
  stepper2.moveTo(0);
  character2Position = 0;
}

void moveCharacter1() {
  // move the character
  stepper1.move(CHARACTER_STEPS);
  character1Position += CHARACTER_STEPS;
}

void moveCharacter2() {
  // move the character
  stepper2.move(-CHARACTER_STEPS);
  // motors are reversed so moves in opposite direction
  character2Position -= CHARACTER_STEPS;
}



/*
  Returns true if player successfully hits a button
*/
bool processPlayerInputs(ArcadeButton buttons[][3], PlayerState* p) {
  // read inputs/update score
  bool buttonHit = false; // track whether button hit to only trigger sound once per round
  ArcadeButton *button;
  for (int i=0; i<9; i++) {
    // see if button was either pushed or decayed
    if (p->buttonsPopped[i]) {
      button = &buttons[i/3][i%3];
      if (button->getState() == LOW) {
        // increment score
        p->score++;
        //Serial.println("Player scored");
        p->buttonsPopped[i] = false;
        p->numButtonsPopped--;
        p->buttonPopTime = millis() + BUTTON_POP_DELAY;
        button->reset();
        buttonHit = true;
      } else if (button->expired) {
        button->reset();
        p->buttonsPopped[i] = false;
        p->numButtonsPopped--;
        p->buttonPopTime = millis() + BUTTON_POP_DELAY;
        //Serial.println("Player missed a button");
      }
    }
  }

  return buttonHit;
}

void gameLoopUpdate() {
  ArcadeButton *button;
  if (millis() > gameStartTime + gameDuration) {
    Serial.println("Time limit reached");
    finishGame();
    return;
  }

    // if either player has reached the end, stop the game too
  if (abs(stepper1.currentPosition()) >= MAX_MOTOR_POSITION || abs(stepper2.currentPosition()) >= MAX_MOTOR_POSITION) {
    Serial.println("End of board reached");
    // stop the motors
    stepper1.stop();
    stepper2.stop();
    finishGame();
    return;
  }

  // read inputs/update score
  if (processPlayerInputs(player1Buttons, &p1)) {
    playRandomHitSound();
    flushInput();
    moveCharacter1();
    
  }
  
  if (processPlayerInputs(player2Buttons, &p2)) {
    playRandomHitSound();
    flushInput();
    moveCharacter2();
  }
  
  // if numButtonsPopped is below the target, pick a random button to light
  if (p1.numButtonsPopped < p1.targetButtonsPopped && millis() > p1.buttonPopTime) {
    popRandomButton(&p1, player1Buttons);
    p1.numButtonsPopped++;
    //p1.buttonPopTime = millis() + BUTTON_POP_DELAY;
  }
  
  if (p2.numButtonsPopped < p2.targetButtonsPopped && millis() > p2.buttonPopTime) {
    popRandomButton(&p2, player2Buttons);
    p2.numButtonsPopped++;
    //p2.buttonPopTime = millis() + BUTTON_POP_DELAY;
  }
  
  // if enough time has passed, increase the number of targets
  if (millis() > nextTargetIncreaseTime) {
    p1.targetButtonsPopped++;
    p2.targetButtonsPopped++;
    nextTargetIncreaseTime = millis() + TARGET_INCREASE_DELAY;
  }
  
}

void finishGame() {
  currentState = FINISHED;
  resetAllButtons();
  // display score, etc.
  Serial.print("Player 1 score: ");
  Serial.println(p1.score);
  Serial.print("Player 2 score: ");
  Serial.println(p2.score);

  playRandomWinSound();

  // blink the winning player's side to indicate win for now
  if (p1.score > p2.score) {
    blinkPlayerButtons(1, 5);
  } else if (p2.score > p1.score) {
    blinkPlayerButtons(2, 5);
  } else {
    // tie, blink all 
    blinkAllButtons(5);
  }
    
}

void stopGame() {
  // interrupt game/reset
  currentState = FINISHED;
}


void processVolumeButtons() {
  // short circuit if we've changed the volume recently
  if (millis() < last_volume_change + VOLUME_DELAY) {
    return;
  }

  // see if volume up or down is pressed
  // SFX board doesn't seem to respond to volume commands if it's playing a file
  if (digitalRead(VOLUME_UP) == LOW) {
    sfx.stop();
    for (int i = 0; i<5; i++) {
      sfx.volUp();
    }
    last_volume_change = millis();
    Serial.println("Volume up.");
  } else if (digitalRead(VOLUME_DOWN) == LOW) {
    sfx.stop();
    for (int i = 0; i<5; i++) {
      sfx.volDown();
    }
    last_volume_change = millis();
    Serial.println("Volume down.");
  }
}

unsigned long nextTick = 0;
int currentLed = NUM_LEDS-1;
unsigned long tickDuration = 0;

void resetLedClock() {
  //stripLeds.fill_solid(CRGB::Red);
  stripLeds.fill_rainbow(CRGB::Green);
  currentLed = NUM_LEDS-1;
}

void startClock() {
  stripLeds.fill_solid(CRGB::Green);
  nextTick = millis() + tickDuration;
  tickDuration = gameDuration/NUM_LEDS; // divide up the LEDs evenly
  Serial.print("Tick Duration: ");
  Serial.println(tickDuration);
}

void updateLedClock() {
  if (millis() > nextTick && currentLed >= 0) {
    stripLeds[currentLed] = CRGB::Black;
    currentLed--;
    nextTick = millis() + tickDuration;
  }
}


void loop() {
  // update motors
  stepper1.run();
  stepper2.run();

  // allow buttons to update their state
  updateButtons();

  processVolumeButtons();

  if (startButtonPushed()) {
    Serial.println("Start button pushed");
  }

  if (resetButtonPushed()) {
    Serial.println("Reset button pushed");
    playRandomWinSound();
    blinkAllButtons(5);
    stopGame();
  }

  // wait in attract mode until start button is pushed
  switch (currentState) {
    case ATTRACT:
      if (startButtonPushed()) {
        // transition to countdown and start the game
        countdownToStart();
        startGame();
      } else {
        attractDisplayUpdate();
      }
      break;
    case RUNNING:
      gameLoopUpdate();
      updateLedClock();
      break;
    case RESETTING:
      // check if we're finished resetting
      if (stepper1.distanceToGo() == 0 && stepper2.distanceToGo() == 0) {
        Serial.println("Reset complete.");
        Serial.print("Motor 1 position: ");
        Serial.println(stepper1.currentPosition());
        Serial.print("Motor 2 position: ");
        Serial.println(stepper2.currentPosition());
        startAttract();
      }
      break;
    case FINISHED:
      resetLedClock();
      resetCharacters();
      break;
  }

  FastLED.show();
    
}

