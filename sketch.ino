#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// Screen Layout Constants
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Physical Hardware Pin Definitions
#define PIN_BUZZER    25
#define PIN_LED_BLUE  26  // Changed from GREEN to BLUE
#define PIN_LED_RED   27
#define BTN_UP        13
#define BTN_DOWN      14
#define BTN_LEFT      23
#define BTN_RIGHT     19
#define BTN_SHOOT     18
#define BTN_MENU      4

// Common Constants & Display Heights
#define DEG_TO_RAD 0.01745329
#define RENDER_HEIGHT 52 
#define MAX_ENTITIES 6

// Central 10-in-1 Arcade Game States
enum Scenes { 
  SPLASH_START, MAIN_MENU, SCENE_DOOM, SCENE_SNAKE, SCENE_TETRIS, SCENE_RACING, 
  SCENE_INVADERS, SCENE_FLAPPY, SCENE_BREAKOUT, SCENE_PONG, SCENE_CHOPPER,
  SCENE_ASTEROIDS, SCENE_FROGGER
};

enum EntityTypes { E_FLOOR, E_WALL, E_PLAYER, E_ENEMY, E_KEY, E_MEDIKIT };

struct Coords { double x; double y; };
struct Vector2D { double x; double y; };

struct Player {
  Coords pos; Vector2D dir; Vector2D plane;
  double velocity;
  int16_t health; uint8_t keys;
};

struct Entity {
  uint8_t type; Coords pos; uint8_t state; uint8_t timer;
  double distance;
  int16_t health; bool alive;
  unsigned long respawnTimestamp; Coords spawnOrigin;             
};

// Global Architecture Loop Control Variables
uint8_t currentScene = SPLASH_START;
uint8_t menuSelection = 0; 
bool exit_scene = false;
uint8_t flash_screen = 0;
bool invert_screen = false;
uint16_t arcadeGlobalScore = 0;
uint16_t gameLevel = 1;              
unsigned long genericTickTimer = 0;

// Hardware State Flag for Async LED System
volatile bool triggerActionFlash = false;

// Doom 3D System Variables
uint16_t waveCount = 1;
uint8_t weaponFrame = 0; 
Player player;
Entity entity[MAX_ENTITIES];
uint8_t num_entities = 0;
float depthBuffer[SCREEN_WIDTH];

#define LEVEL_WIDTH 16
#define LEVEL_HEIGHT 16
const uint8_t levelMap[LEVEL_HEIGHT][LEVEL_WIDTH] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1},
  {1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
  {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,0,0,0,0,0,0,0,1,1,0,0,1},
  {1,0,1,1,0,0,0,0,0,0,0,1,1,0,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// SNAKE GAME REGISTERS
struct SnakeSegment { int16_t x; int16_t y; };
SnakeSegment snake[60]; 
uint8_t snakeLength = 4;
int8_t snakeDirX = 1; int8_t snakeDirY = 0;
int16_t foodX = 10; int16_t foodY = 8;

// TETRIS GAME REGISTERS
#define TETRIS_ROWS 16
#define TETRIS_COLS 10
uint8_t tetrisGrid[TETRIS_ROWS][TETRIS_COLS];
int8_t currentPieceX = 3; int8_t currentPieceY = 0;
uint8_t currentPieceType = 0; uint8_t currentRotation = 0;
const uint16_t tetrisShapes[7][4] = {
  {0xCC00, 0xCC00, 0xCC00, 0xCC00}, {0x0F00, 0x4444, 0x0F00, 0x4444},
  {0x4E00, 0x4644, 0x0E40, 0x4C44}, {0x4460, 0x0E80, 0xC440, 0x2E00},
  {0x44C0, 0x8E00, 0x6440, 0x0E20}, {0x06C0, 0x8C40, 0x06C0, 0x8C40},
  {0x0C60, 0x4C80, 0x0C60, 0x4C80}
};

// TRAFFIC RACER REGISTERS
struct EnemyCar { int16_t x; int16_t y; bool active; uint8_t lane; };
EnemyCar traffic[3];
int16_t playerCarX = 56; int16_t playerCarY = 40;
float roadScrollOffset = 0; float currentSpeed = 2.0;

// SPACE INVADERS REGISTERS
struct Invader { int16_t x; int16_t y; bool alive; };
Invader invaders[12];
float invaderVelocityX = 2.0;
float invaderStepY = 0;
int16_t invShipX = 58;
int16_t invLaserX = -1; int16_t invLaserY = -1;

// FLAPPY BIRD REGISTERS
float birdY = 20.0; float birdVelocity = 0.0;
int16_t pipeX = 128; int16_t pipeGapY = 16;
int16_t pipeGapHeight = 18;

// BRICK BREAKOUT & PONG PHYSICS REGISTERS
int16_t paddleX = 44; int16_t paddleY = 49;
float ballX = 64; float ballY = 30;
float ballVelX = 1.5; float ballVelY = -1.5;
uint8_t brickMatrix[4][7];
int16_t pongAIY = 20;

// CHOPPLIFTER AIR RESCUE CORE REGISTERS
float copterX = 20.0; float copterY = 15.0;
float chopperLaserX = -1; float chopperLaserY = -1;
float groundTurretX = 128.0; float turretMissileX = -1; float turretMissileY = -1;
int16_t rescueCount = 0; int16_t chopperLives = 3;

// ASTEROIDS VECTOR SYSTEM REGISTERS
struct Asteroid { float x; float y; float vx; float vy; bool active; uint8_t size; };
Asteroid rocks[4];
float astShipX = 64.0; float astShipY = 26.0;
float astShipVx = 0.0; float astShipVy = 0.0;
float astShipAngle = 0.0;
float astLaserX = -1.0; float astLaserY = -1.0;
float astLaserVx = 0.0; float astLaserVy = 0.0;

// FROGGER/CROSSY STREAM REGISTERS
int16_t frogX = 60; int16_t frogY = 46;
int16_t obstacleX[4] = {0, 40, 80, 20};
float obstacleSpeed[4] = {1, -2, 2, -1};

// Shared Controller Inputs
const int buttonPins[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_SHOOT, BTN_MENU};
volatile bool sharedButtonStates[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
volatile bool isShootingIntentActive = false;

volatile uint8_t soundRequestSignal = 0;
#define SFX_NONE    0
#define SFX_SHOOT   1
#define SFX_HIT     2
#define SFX_HURT    3
#define SFX_WAVE    4
#define SFX_ITEM    5

TaskHandle_t Core0Task;
double delta = 1.0; 

void initializePlayerState(); void initializeSnakeState(); void initializeTetrisState();
void initializeRacingState(); void initializeInvadersState(); void initializeFlappyState();
void initializeBreakoutState(); void initializePongState(); void initializeChopperState();
void initializeAsteroidsState(); void initializeFroggerState();
void loopSplashStartScene(); void loopArcadeHomeMenu(); void loopGamePlayScene(); 
void loopSnakeGameScene(); void loopTetrisGameScene(); void loopRacingGameScene(); 
void loopInvadersGameScene(); void loopFlappyGameScene(); void loopBreakoutGameScene(); 
void loopPongGameScene(); void loopChopperGameScene(); void loopAsteroidsGameScene(); 
void loopFroggerGameScene(); void spawnWaveEntities();
void spawnEntityNode(uint8_t type, double x, double y);

void Core0Engine(void * pvParameters);

void setup() {
  Wire.begin(21, 22);
  Wire.setClock(400000); 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  xTaskCreatePinnedToCore(Core0Engine, "Core0Task", 4000, NULL, 1, &Core0Task, 0);
}

// ================================================================
// CORE 0: ASYNCHRONOUS DEBOUNCER, TONAL DRIVER & BLUE LED CONTROLLER
// ================================================================
void Core0Engine(void * pvParameters) {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  digitalWrite(PIN_LED_BLUE, HIGH); 
  digitalWrite(PIN_LED_RED, LOW);

  for (int i = 0; i < 6; i++) pinMode(buttonPins[i], INPUT_PULLUP);
  bool lastBtnStates[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
  unsigned long lastDebounce[6] = {0};
  unsigned long actionFlashTimer = 0;
  bool menuLatchFlag = true;

  for(;;) {
    for (int i = 0; i < 6; i++) {
      int reading = digitalRead(buttonPins[i]);
      if (reading != lastBtnStates[i]) lastDebounce[i] = millis();
      if ((millis() - lastDebounce[i]) > 15) sharedButtonStates[i] = reading;
      lastBtnStates[i] = reading;
    }

    if (sharedButtonStates[4] == LOW && !isShootingIntentActive) {
      isShootingIntentActive = true;
      triggerActionFlash = true;
      soundRequestSignal = SFX_SHOOT;
    }
    if (sharedButtonStates[4] == HIGH) {
      isShootingIntentActive = false;
    }

    if (triggerActionFlash) {
      digitalWrite(PIN_LED_RED, HIGH);
      actionFlashTimer = millis();
      triggerActionFlash = false;
    }
    if (digitalRead(PIN_LED_RED) == HIGH && (millis() - actionFlashTimer >= 80)) {
      digitalWrite(PIN_LED_RED, LOW);
    }

    if (sharedButtonStates[5] == LOW) {
      if (menuLatchFlag) {
        exit_scene = true;
        menuLatchFlag = false;
        tone(PIN_BUZZER, 400, 25); 
      }
    } else { menuLatchFlag = true; }

    if (soundRequestSignal != SFX_NONE) {
      switch (soundRequestSignal) {
        case SFX_SHOOT: tone(PIN_BUZZER, 180, 30); break;
        case SFX_HIT:   tone(PIN_BUZZER, 260, 20); break;
        case SFX_HURT:  tone(PIN_BUZZER, 70, 70);  break;
        case SFX_ITEM:  tone(PIN_BUZZER, 600, 60); break;
        case SFX_WAVE:  tone(PIN_BUZZER, 350, 40); delay(40); tone(PIN_BUZZER, 550, 60); break;
      }
      soundRequestSignal = SFX_NONE; 
    }
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

// ================================================================
// CORE 1: CENTRAL GAME ROUTER
// ================================================================
void loop() {
  switch (currentScene) {
    case SPLASH_START:    loopSplashStartScene();   break;
    case MAIN_MENU:       loopArcadeHomeMenu();     break;
    case SCENE_DOOM:      loopGamePlayScene();      break;
    case SCENE_SNAKE:     loopSnakeGameScene();     break;
    case SCENE_TETRIS:    loopTetrisGameScene();    break;
    case SCENE_RACING:    loopRacingGameScene();    break;
    case SCENE_INVADERS:  loopInvadersGameScene();  break;
    case SCENE_FLAPPY:    loopFlappyGameScene();    break;
    case SCENE_BREAKOUT:  loopBreakoutGameScene();  break;
    case SCENE_PONG:      loopPongGameScene();      break;
    case SCENE_CHOPPER:   loopChopperGameScene();   break;
    case SCENE_ASTEROIDS: loopAsteroidsGameScene(); break;
    case SCENE_FROGGER:   loopFroggerGameScene();   break;
  }
}

// ================================================================
// DYNAMIC STARFIELD SPLASH SCREEN MODULE
// ================================================================
void loopSplashStartScene() {
  exit_scene = false; 
  struct Star { float x; float y; float speed; };
  const uint8_t NUM_STARS = 15;
  Star stars[NUM_STARS];
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].x = random(0, SCREEN_WIDTH);
    stars[i].y = random(0, SCREEN_HEIGHT);
    stars[i].speed = random(10, 35) / 10.0;
  }

  unsigned long blinkTimer = millis();
  bool textVisible = true;

  do {
    display.clearDisplay();
    for (int i = 0; i < NUM_STARS; i++) {
      stars[i].x -= stars[i].speed;
      if (stars[i].x < 0) {
        stars[i].x = SCREEN_WIDTH;
        stars[i].y = random(0, SCREEN_HEIGHT);
      }
      display.drawPixel((int)stars[i].x, (int)stars[i].y, SSD1306_WHITE);
    }

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(19, 20);
    display.print("= POTETO ARCADE =");

    if (millis() - blinkTimer > 400) {
      textVisible = !textVisible;
      blinkTimer = millis();
    }
    
    if (textVisible) {
      display.setCursor(10, 44);
      display.print(">> PRESS ANY KEY <<");
    }

    display.display();
    delay(15);
    for (int i = 0; i < 5; i++) {
      if (sharedButtonStates[i] == LOW) {
        tone(PIN_BUZZER, 550, 50);
        delay(50); tone(PIN_BUZZER, 800, 80);
        triggerActionFlash = true;
        exit_scene = true;
      }
    }
  } while (!exit_scene);

  for (int w = 0; w <= SCREEN_WIDTH / 2; w += 8) {
    display.clearDisplay();
    display.drawRect(SCREEN_WIDTH/2 - w, 0, w*2, SCREEN_HEIGHT, SSD1306_WHITE);
    display.display();
    delay(5);
  }
  
  currentScene = MAIN_MENU;
}

// ================================================================
// ARCADE HOME MENU
// ================================================================
void loopArcadeHomeMenu() {
  exit_scene = false; bool btnReleased = false;
  do {
    display.clearDisplay();
    display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(12, 0); display.print("CORE-1 10-IN-1 ARCADE");
    display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    const char* titles[] = {
      "1.DOOM NANO 3D", "2.RETRO SNAKE", "3.TETRIS BLOCKS", "4.TRAFFIC RACER",
      "5.SPACE INVADERS", "6.FLAPPY BIRD", "7.BRICK BREAKOUT v2", "8.PONG ACTION",
      "9.CHOPPER RESCUE", "10.CROSSY FROGGER"
    };

    int startIdx = (menuSelection >= 5) ? 5 : 0;
    for (int i = 0; i < 5; i++) {
      int curr = startIdx + i;
      if (menuSelection == curr) {
        display.fillRect(1, 11 + (i * 10), 126, 10, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else { display.setTextColor(SSD1306_WHITE); }
      display.setCursor(4, 12 + (i * 10)); display.print(titles[curr]);
    }
    display.display();

    if (sharedButtonStates[0] == HIGH && sharedButtonStates[1] == HIGH) btnReleased = true;
    if (btnReleased) {
      if (sharedButtonStates[0] == LOW) { menuSelection = (menuSelection - 1 + 10) % 10; btnReleased = false; tone(PIN_BUZZER, 280, 15); }
      if (sharedButtonStates[1] == LOW) { menuSelection = (menuSelection + 1) % 10; btnReleased = false; tone(PIN_BUZZER, 280, 15); }
    }

    if (sharedButtonStates[4] == LOW) {
      tone(PIN_BUZZER, 480, 50);
      triggerActionFlash = true;
      gameLevel = 1; 
      if (menuSelection == 0) { currentScene = SCENE_DOOM; initializePlayerState(); }
      if (menuSelection == 1) { currentScene = SCENE_SNAKE; initializeSnakeState(); }
      if (menuSelection == 2) { currentScene = SCENE_TETRIS; initializeTetrisState(); }
      if (menuSelection == 3) { currentScene = SCENE_RACING; initializeRacingState(); }
      if (menuSelection == 4) { currentScene = SCENE_INVADERS; initializeInvadersState(); }
      if (menuSelection == 5) { currentScene = SCENE_FLAPPY; initializeFlappyState(); }
      if (menuSelection == 6) { currentScene = SCENE_BREAKOUT; initializeBreakoutState(); }
      if (menuSelection == 7) { currentScene = SCENE_PONG; initializePongState(); }
      if (menuSelection == 8) { currentScene = SCENE_CHOPPER; initializeChopperState(); }
      if (menuSelection == 9) { currentScene = SCENE_FROGGER; initializeFroggerState(); }
      exit_scene = true;
    }
    delay(30);
  } while (!exit_scene);
}

void drawBottomHUD(const char* name) {
  display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
  display.fillRect(0, 53, 128, 11, SSD1306_BLACK);
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 55); display.print(name);
  display.setCursor(76, 55); display.print("L"); display.print(gameLevel);
  display.setCursor(98, 55); display.print("S:"); display.print(arcadeGlobalScore);
}

// ================================================================
// DOOM NANO 3D RAYCASTING SYSTEM
// ================================================================
void initializePlayerState() {
  player.pos = {2.5, 2.5};
  player.dir = {1.0, 0.0};
  player.plane = {0.0, 0.66};
  player.health = 100;
  waveCount = 1;
  arcadeGlobalScore = 0;
  spawnWaveEntities();
}

void spawnWaveEntities() {
  num_entities = 0;
  spawnEntityNode(E_ENEMY, 4.5, 4.5);
  spawnEntityNode(E_ENEMY, 12.5, 3.5);
  spawnEntityNode(E_MEDIKIT, 8.5, 8.5);
  if (waveCount > 1) spawnEntityNode(E_ENEMY, 3.5, 12.5);
  if (waveCount > 2) spawnEntityNode(E_ENEMY, 12.5, 12.5);
  soundRequestSignal = SFX_WAVE;
}

void spawnEntityNode(uint8_t type, double x, double y) {
  if (num_entities >= MAX_ENTITIES) return;
  entity[num_entities].type = type;
  entity[num_entities].pos = {x, y};
  entity[num_entities].spawnOrigin = {x, y};
  entity[num_entities].alive = true;
  entity[num_entities].health = (type == E_ENEMY) ? (40 + waveCount * 10) : 0;
  entity[num_entities].state = 0;
  entity[num_entities].timer = 0;
  num_entities++;
}

void handlePlayerMovement() {
  double rotSpeed = 0.06;
  double moveSpeed = 0.07;

  if (sharedButtonStates[2] == LOW) { 
    double oldDirX = player.dir.x;
    player.dir.x = player.dir.x * cos(-rotSpeed) - player.dir.y * sin(-rotSpeed);
    player.dir.y = oldDirX * sin(-rotSpeed) + player.dir.y * cos(-rotSpeed);
    double oldPlaneX = player.plane.x;
    player.plane.x = player.plane.x * cos(-rotSpeed) - player.plane.y * sin(-rotSpeed);
    player.plane.y = oldPlaneX * sin(-rotSpeed) + player.plane.y * cos(-rotSpeed);
  }
  if (sharedButtonStates[3] == LOW) { 
    double oldDirX = player.dir.x;
    player.dir.x = player.dir.x * cos(rotSpeed) - player.dir.y * sin(rotSpeed);
    player.dir.y = oldDirX * sin(rotSpeed) + player.dir.y * cos(rotSpeed);
    double oldPlaneX = player.plane.x;
    player.plane.x = player.plane.x * cos(rotSpeed) - player.plane.y * sin(rotSpeed);
    player.plane.y = oldPlaneX * sin(rotSpeed) + player.plane.y * cos(rotSpeed);
  }

  if (sharedButtonStates[0] == LOW) { 
    double nX = player.pos.x + player.dir.x * moveSpeed;
    double nY = player.pos.y + player.dir.y * moveSpeed;
    if (levelMap[(int)nY][(int)nX] == 0) { player.pos.x = nX; player.pos.y = nY; }
  }
  if (sharedButtonStates[1] == LOW) { 
    double nX = player.pos.x - player.dir.x * moveSpeed;
    double nY = player.pos.y - player.dir.y * moveSpeed;
    if (levelMap[(int)nY][(int)nX] == 0) { player.pos.x = nX; player.pos.y = nY; }
  }

  if (sharedButtonStates[4] == LOW && weaponFrame == 0) {
    weaponFrame = 3; 
    for (uint8_t i = 0; i < num_entities; i++) {
      if (entity[i].type == E_ENEMY && entity[i].alive && entity[i].distance < 6.0) {
        double spriteX = entity[i].pos.x - player.pos.x;
        double spriteY = entity[i].pos.y - player.pos.y;
        double invDet = 1.0 / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
        double transformX = invDet * (player.dir.y * spriteX - player.dir.x * spriteY);
        double transformY = invDet * (-player.plane.y * spriteX + player.plane.x * spriteY);
        int spriteScreenX = int((SCREEN_WIDTH / 2) * (1 + transformX / transformY));
        if (transformY > 0 && spriteScreenX > 32 && spriteScreenX < 96) {
          entity[i].health -= 35;
          flash_screen = 2;
          soundRequestSignal = SFX_HIT;
          triggerActionFlash = true;
          if(entity[i].health <= 0) {
            entity[i].alive = false;
            arcadeGlobalScore += 50;
            entity[i].respawnTimestamp = millis() + 5000;
          }
        }
      }
    }
  }
}

void updateEntities() {
  bool anyEnemyAlive = false;
  for (uint8_t i = 0; i < num_entities; i++) {
    if (entity[i].type == E_MEDIKIT) {
      if (entity[i].alive) {
        double dx = entity[i].pos.x - player.pos.x;
        double dy = entity[i].pos.y - player.pos.y;
        entity[i].distance = sqrt(dx*dx + dy*dy);
        if (entity[i].distance < 0.6) {
          entity[i].alive = false;
          player.health = min(100, player.health + 40);
          soundRequestSignal = SFX_ITEM;
          triggerActionFlash = true;
        }
      }
    }
    else if (entity[i].type == E_ENEMY) {
      if (entity[i].alive) {
        anyEnemyAlive = true;
        double dx = entity[i].pos.x - player.pos.x;
        double dy = entity[i].pos.y - player.pos.y;
        entity[i].distance = sqrt(dx*dx + dy*dy);

        if (entity[i].distance > 1.0) {
          entity[i].pos.x -= (dx / entity[i].distance) * 0.02;
          entity[i].pos.y -= (dy / entity[i].distance) * 0.02;
        } else {
          if (entity[i].timer == 0) {
            player.health -= (6 + waveCount);
            flash_screen = 1;
            soundRequestSignal = SFX_HURT;
            triggerActionFlash = true;
            entity[i].timer = 40; 
          } else { entity[i].timer--; }
        }
      } else {
        if (millis() > entity[i].respawnTimestamp) {
          entity[i].alive = true;
          entity[i].pos = entity[i].spawnOrigin;
          entity[i].health = 30 + waveCount * 10;
        }
      }
    }
  }

  if (!anyEnemyAlive) {
    waveCount++;
    spawnWaveEntities();
  }
  if (player.health <= 0) exit_scene = true;
}

void renderMapWorld() {
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    double cameraX = 2 * x / (double)SCREEN_WIDTH - 1;
    double rayDirX = player.dir.x + player.plane.x * cameraX;
    double rayDirY = player.dir.y + player.plane.y * cameraX;

    int mapX = int(player.pos.x);
    int mapY = int(player.pos.y);

    double sideDistX, sideDistY;
    double deltaDistX = (rayDirX == 0) ? 1e30 : abs(1.0 / rayDirX);
    double deltaDistY = (rayDirY == 0) ? 1e30 : abs(1.0 / rayDirY);
    double perpWallDist;

    int stepX, stepY;
    int hit = 0, side;

    if (rayDirX < 0) { stepX = -1; sideDistX = (player.pos.x - mapX) * deltaDistX; }
    else { stepX = 1; sideDistX = (mapX + 1.0 - player.pos.x) * deltaDistX; }
    if (rayDirY < 0) { stepY = -1; sideDistY = (player.pos.y - mapY) * deltaDistY; }
    else { stepY = 1; sideDistY = (mapY + 1.0 - player.pos.y) * deltaDistY; }

    while (hit == 0) {
      if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
      else { sideDistY += deltaDistY; mapY += stepY; side = 1; }
      if (levelMap[mapY][mapX] > 0) hit = 1;
    }

    if (side == 0) perpWallDist = (sideDistX - deltaDistX);
    else          perpWallDist = (sideDistY - deltaDistY);

    depthBuffer[x] = perpWallDist;
    int lineHeight = (int)(RENDER_HEIGHT / perpWallDist);
    int drawStart = -lineHeight / 2 + RENDER_HEIGHT / 2;
    if (drawStart < 0) drawStart = 0;
    int drawEnd = lineHeight / 2 + RENDER_HEIGHT / 2;
    if (drawEnd >= RENDER_HEIGHT) drawEnd = RENDER_HEIGHT - 1;

    if (side == 1) {
      for (int y = drawStart; y <= drawEnd; y += 2) display.drawPixel(x, y, SSD1306_WHITE);
    } else {
      display.drawFastVLine(x, drawStart, drawEnd - drawStart, SSD1306_WHITE);
    }
  }
}

void renderSprites() {
  int spriteOrder[MAX_ENTITIES];
  for (int i = 0; i < num_entities; i++) spriteOrder[i] = i;
  for (int i = 0; i < num_entities - 1; i++) {
    for (int j = i + 1; j < num_entities; j++) {
      if (entity[spriteOrder[i]].distance < entity[spriteOrder[j]].distance) {
        int temp = spriteOrder[i];
        spriteOrder[i] = spriteOrder[j];
        spriteOrder[j] = temp;
      }
    }
  }

  for (int i = 0; i < num_entities; i++) {
    int idx = spriteOrder[i];
    if (!entity[idx].alive) continue;

    double spriteX = entity[idx].pos.x - player.pos.x;
    double spriteY = entity[idx].pos.y - player.pos.y;

    double invDet = 1.0 / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
    double transformX = invDet * (player.dir.y * spriteX - player.dir.x * spriteY);
    double transformY = invDet * (-player.plane.y * spriteX + player.plane.x * spriteY);

    if (transformY <= 0.2) continue;

    int spriteScreenX = int((SCREEN_WIDTH / 2) * (1 + transformX / transformY));
    int spriteHeight = abs(int(RENDER_HEIGHT / transformY));
    int drawStartY = -spriteHeight / 2 + RENDER_HEIGHT / 2;
    if (drawStartY < 0) drawStartY = 0;
    int drawEndY = spriteHeight / 2 + RENDER_HEIGHT / 2;
    if (drawEndY >= RENDER_HEIGHT) drawEndY = RENDER_HEIGHT - 1;

    int spriteWidth = abs(int(RENDER_HEIGHT / transformY));
    int drawStartX = -spriteWidth / 2 + spriteScreenX;
    int drawEndX = spriteWidth / 2 + spriteScreenX;

    for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
      if (stripe >= 0 && stripe < SCREEN_WIDTH && transformY < depthBuffer[stripe]) {
        int midY = (drawStartY + drawEndY) / 2;
        int rad = spriteHeight / 5; if (rad < 2) rad = 2;
        if (stripe == spriteScreenX) {
          if (entity[idx].type == E_ENEMY) {
            display.drawCircle(stripe, midY - rad, rad, SSD1306_WHITE);
            display.drawRect(stripe - rad, midY, rad * 2, rad * 2, SSD1306_WHITE);
          } else {
            display.drawRect(stripe - 2, midY - 2, 5, 5, SSD1306_WHITE);
            display.drawFastHLine(stripe - 4, midY, 9, SSD1306_WHITE);
            display.drawFastVLine(stripe, midY - 4, 9, SSD1306_WHITE);
          }
        }
      }
    }
  }
}

void loopGamePlayScene() {
  exit_scene = false;
  do {
    handlePlayerMovement(); 
    updateEntities(); 
    
    display.clearDisplay();
    if (flash_screen == 1) { display.fillRect(0, 0, 128, 64, SSD1306_WHITE); flash_screen = 0; }
    else if (flash_screen == 2) { display.drawRect(0, 0, 128, 52, SSD1306_WHITE); flash_screen = 0; }
    else { renderMapWorld(); renderSprites(); }

    int cx = SCREEN_WIDTH / 2;
    if (weaponFrame > 0) {
      display.fillTriangle(cx, RENDER_HEIGHT, cx - 12, RENDER_HEIGHT - 14, cx + 12, RENDER_HEIGHT - 14, SSD1306_WHITE);
      display.fillRect(cx - 3, RENDER_HEIGHT - 6, 7, 6, SSD1306_BLACK);
      weaponFrame--;
    } else {
      display.fillRect(cx - 1, RENDER_HEIGHT - 9, 3, 9, SSD1306_WHITE);
    }

    display.drawFastHLine(0, RENDER_HEIGHT, SCREEN_WIDTH, SSD1306_WHITE);
    display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, RENDER_HEIGHT + 3); display.print("HP:"); display.print(player.health);
    display.setCursor(50, RENDER_HEIGHT + 3); display.print("W:"); display.print(waveCount);
    display.setCursor(85, RENDER_HEIGHT + 3); display.print("S:"); display.print(arcadeGlobalScore);

    display.display();
    delay(10);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// BRICK BREAKOUT v2 MODULE 
// ================================================================
void initializeBreakoutState() {
  paddleX = 44;
  ballX = 64; ballY = 32;
  ballVelX = 1.2 + (gameLevel * 0.25); 
  ballVelY = -(1.2 + (gameLevel * 0.25));
  for(int r=0; r<4; r++) { for(int c=0; c<7; c++) brickMatrix[r][c] = 1; }
}

void loopBreakoutGameScene() {
  exit_scene = false;
  do {
    if (sharedButtonStates[2] == LOW && paddleX > 5) paddleX -= 3;
    if (sharedButtonStates[3] == LOW && paddleX < 97) paddleX += 3;
    
    ballX += ballVelX; ballY += ballVelY;
    if (ballX <= 4) { ballX = 4; ballVelX = -ballVelX; tone(PIN_BUZZER, 350, 5); triggerActionFlash = true; }
    if (ballX >= 121) { ballX = 121; ballVelX = -ballVelX; tone(PIN_BUZZER, 350, 5); triggerActionFlash = true; }
    if (ballY <= 4) { ballY = 4; ballVelY = -ballVelY; tone(PIN_BUZZER, 350, 5); triggerActionFlash = true; }
    
    if (ballY >= 47 && ballY <= 49) {
      if (ballX >= paddleX - 1 && ballX <= paddleX + 25) {
        ballVelY = -abs(ballVelY);
        ballY = 46; 
        ballVelX = ((ballX + 1) - (paddleX + 12)) / 4.5; 
        tone(PIN_BUZZER, 550, 10);
        triggerActionFlash = true;
      }
    }
    if (ballY > 50) { tone(PIN_BUZZER, 70, 300); triggerActionFlash = true; exit_scene = true; } 
    
    if (ballY < 24 && ballY >= 4) {
      int brc = (int)((ballX - 8) / 16);
      int brr = (int)((ballY - 4) / 5);
      if (brc >= 0 && brc < 7 && brr >= 0 && brr < 4) {
        if (brickMatrix[brr][brc] == 1) { 
          brickMatrix[brr][brc] = 0;
          ballVelY = -ballVelY; 
          arcadeGlobalScore += 15; 
          tone(PIN_BUZZER, 650, 8); 
          triggerActionFlash = true;
        }
      }
    }
    
    bool bricksLeft = false;
    for(int r=0; r<4; r++) {
      for(int c=0; c<7; c++) { if (brickMatrix[r][c] == 1) bricksLeft = true; }
    }
    
    if (!bricksLeft) {
      gameLevel++;
      tone(PIN_BUZZER, 500, 100); delay(100); tone(PIN_BUZZER, 800, 150);
      initializeBreakoutState(); 
    }
    
    display.clearDisplay();
    display.drawRect(2, 1, 124, 51, SSD1306_WHITE); 
    display.fillRect(paddleX, paddleY, 24, 3, SSD1306_WHITE); 
    display.fillRect((int)ballX, (int)ballY, 3, 3, SSD1306_WHITE);
    for(int r=0; r<4; r++) { 
      for(int c=0; c<7; c++) { 
        if (brickMatrix[r][c] == 1) display.fillRect(8 + (c * 16), 4 + (r * 5), 13, 4, SSD1306_WHITE);
      } 
    }
    drawBottomHUD("BREAKOUT v2"); display.display(); delay(15);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// CHOPPLIFTER AIR RESCUE
// ================================================================
void initializeChopperState() {
  copterX = 24.0; copterY = 15.0; chopperLives = 3; rescueCount = 0;
  chopperLaserX = -1; chopperLaserY = -1; groundTurretX = 120.0; turretMissileX = -1; turretMissileY = -1;
}

void loopChopperGameScene() {
  exit_scene = false; bool shootReleased = true;
  do {
    if (sharedButtonStates[0] == LOW && copterY > 4) copterY -= 1.5;
    if (sharedButtonStates[1] == LOW && copterY < 40) copterY += 1.5;
    if (sharedButtonStates[2] == LOW && copterX > 6) copterX -= 1.5;
    if (sharedButtonStates[3] == LOW && copterX < 110) copterX += 1.5;

    if (sharedButtonStates[4] == HIGH) shootReleased = true;
    if (sharedButtonStates[4] == LOW && shootReleased && chopperLaserX < 0) {
      chopperLaserX = copterX + 8;
      chopperLaserY = copterY + 2;
      shootReleased = false; triggerActionFlash = true;
    }

    if (chopperLaserX >= 0) {
      chopperLaserX += 4;
      if (chopperLaserX > 124) chopperLaserX = -1;
    }

    groundTurretX -= (1.0 + (gameLevel * 0.3));
    if (groundTurretX < 2) { groundTurretX = 126; }

    if (turretMissileX < 0 && random(0, 50) < (2 + gameLevel)) {
      turretMissileX = groundTurretX;
      turretMissileY = 44;
    }
    if (turretMissileX >= 0) {
      turretMissileX -= (2.0 + (gameLevel * 0.2));
      turretMissileY -= 1.0;
      if (turretMissileX < 2 || turretMissileY < 2) turretMissileX = -1;
    }

    if (chopperLaserX >= 0 && abs(chopperLaserX - groundTurretX) < 6 && chopperLaserY >= 42) {
      arcadeGlobalScore += 50;
      groundTurretX = 126; chopperLaserX = -1;
      tone(PIN_BUZZER, 500, 30); triggerActionFlash = true;
    }
    if (turretMissileX >= 0 && abs(turretMissileX - (copterX + 4)) < 6 && abs(turretMissileY - (copterY + 2)) < 5) {
      chopperLives--;
      turretMissileX = -1;
      tone(PIN_BUZZER, 90, 200); triggerActionFlash = true;
      if (chopperLives <= 0) exit_scene = true;
    }

    if (copterY >= 38 && copterX >= 4 && copterX <= 16) {
      rescueCount++;
      arcadeGlobalScore += 100; 
      tone(PIN_BUZZER, 800, 40); 
      triggerActionFlash = true;
      copterY = 15;
      if (rescueCount % 3 == 0) {
        gameLevel++;
        tone(PIN_BUZZER, 900, 150);
      }
    }

    display.clearDisplay();
    display.drawRect(1, 1, 126, 51, SSD1306_WHITE); 
    
    int cx = (int)copterX;
    int cy = (int)copterY;
    display.fillRect(cx, cy, 8, 4, SSD1306_WHITE);
    display.drawFastHLine(cx - 3, cy - 1, 12, SSD1306_WHITE);
    display.drawFastHLine(cx + 1, cy + 4, 6, SSD1306_WHITE);

    if (chopperLaserX >= 0) display.drawFastHLine((int)chopperLaserX, (int)chopperLaserY, 4, SSD1306_WHITE);
    display.fillRect(4, 42, 16, 8, SSD1306_WHITE); 
    display.setCursor(6, 34); display.setTextColor(SSD1306_WHITE); display.print("LZ");
    
    display.fillRect((int)groundTurretX, 45, 8, 5, SSD1306_WHITE);
    display.drawLine((int)groundTurretX + 4, 45, (int)groundTurretX, 41, SSD1306_WHITE);
    if (turretMissileX >= 0) display.drawCircle((int)turretMissileX, (int)turretMissileY, 1, SSD1306_WHITE);

    display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
    display.setCursor(2, 55); display.print("HP:"); display.print(chopperLives);
    display.setCursor(44, 55); display.print("RSC:"); display.print(rescueCount);
    display.setCursor(88, 55); display.print("LV:"); display.print(gameLevel);
    display.display(); delay(20);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// RETRO SNAKE
// ================================================================
void initializeSnakeState() {
  snakeLength = 4; snakeDirX = 1; snakeDirY = 0;
  for (int i = 0; i < snakeLength; i++) snake[i] = { (int16_t)(10 - i), 6 };
  foodX = random(2, 23); foodY = random(2, 10);
}

void loopSnakeGameScene() {
  exit_scene = false; genericTickTimer = millis();
  do {
    if (sharedButtonStates[2] == LOW && snakeDirX == 0) { snakeDirX = -1; snakeDirY = 0; }
    if (sharedButtonStates[3] == LOW && snakeDirX == 0) { snakeDirX = 1;  snakeDirY = 0; }
    if (sharedButtonStates[0] == LOW && snakeDirY == 0) { snakeDirX = 0;  snakeDirY = -1; }
    if (sharedButtonStates[1] == LOW && snakeDirY == 0) { snakeDirX = 0;  snakeDirY = 1; }
    
    long loopDelayThresh = max(35, 130 - (gameLevel * 15));
    if (millis() - genericTickTimer > loopDelayThresh) {
      genericTickTimer = millis();
      for (int i = snakeLength - 1; i > 0; i--) snake[i] = snake[i - 1];
      snake[0].x += snakeDirX; snake[0].y += snakeDirY;
      
      if (snake[0].x < 1) snake[0].x = 23; if (snake[0].x > 23) snake[0].x = 1;
      if (snake[0].y < 1) snake[0].y = 10; if (snake[0].y > 10) snake[0].y = 1;
      
      for (int i = 1; i < snakeLength; i++) { 
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) { 
          tone(PIN_BUZZER, 80, 200); triggerActionFlash = true; exit_scene = true; 
        } 
      }
      
      if (snake[0].x == foodX && snake[0].y == foodY) { 
        if (snakeLength < 58) snakeLength++;
        arcadeGlobalScore += 10; 
        tone(PIN_BUZZER, 600, 20); 
        triggerActionFlash = true; 
        foodX = random(2, 23); foodY = random(2, 10);
        if (snakeLength % 3 == 0) { gameLevel++; }
      }
    }
    display.clearDisplay();
    display.drawRect(2, 1, 124, 51, SSD1306_WHITE); 
    display.fillRect(foodX * 5 + 1, foodY * 4 + 1, 4, 3, SSD1306_WHITE);
    for (int i = 0; i < snakeLength; i++) display.fillRect(snake[i].x * 5 + 1, snake[i].y * 4 + 1, 4, 3, SSD1306_WHITE);
    drawBottomHUD("RETRO SNAKE"); display.display(); delay(15);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// TETRIS BLOCK ENGINE
// ================================================================
void initializeTetrisState() {
  memset(tetrisGrid, 0, sizeof(tetrisGrid));
  currentPieceX = 3; currentPieceY = 0;
  currentPieceType = random(0, 7); currentRotation = 0;
}

bool checkTetrisCollision(int8_t px, int8_t py, uint8_t rot) {
  uint16_t shape = tetrisShapes[currentPieceType][rot];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if ((shape & (0x8000 >> (r * 4 + c))) != 0) {
        int8_t targetX = px + c;
        int8_t targetY = py + r;
        if (targetX < 0 || targetX >= TETRIS_COLS || targetY >= TETRIS_ROWS) return true;
        if (targetY >= 0 && tetrisGrid[targetY][targetX] != 0) return true;
      }
    }
  }
  return false;
}

void lockTetrisPiece() {
  uint16_t shape = tetrisShapes[currentPieceType][currentRotation];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if ((shape & (0x8000 >> (r * 4 + c))) != 0) {
        int8_t targetY = currentPieceY + r;
        if (targetY >= 0) tetrisGrid[targetY][currentPieceX + c] = 1;
      }
    }
  }
  
  for (int r = TETRIS_ROWS - 1; r >= 0; r--) {
    bool rowFull = true;
    for (int c = 0; c < TETRIS_COLS; c++) { if (tetrisGrid[r][c] == 0) rowFull = false; }
    if (rowFull) {
      arcadeGlobalScore += 100; tone(PIN_BUZZER, 750, 40); triggerActionFlash = true;
      for (int moveR = r; moveR > 0; moveR--) {
        for (int c = 0; c < TETRIS_COLS; c++) tetrisGrid[moveR][c] = tetrisGrid[moveR - 1][c];
      }
      for (int c = 0; c < TETRIS_COLS; c++) tetrisGrid[0][c] = 0;
      r++; 
      if (arcadeGlobalScore % 300 == 0) gameLevel++;
    }
  }
}

void loopTetrisGameScene() {
  exit_scene = false; genericTickTimer = millis(); unsigned long inputCooldown = 0;
  do {
    if (millis() - inputCooldown > 160) {
      if (sharedButtonStates[2] == LOW && !checkTetrisCollision(currentPieceX - 1, currentPieceY, currentRotation)) { currentPieceX--; inputCooldown = millis(); }
      if (sharedButtonStates[3] == LOW && !checkTetrisCollision(currentPieceX + 1, currentPieceY, currentRotation)) { currentPieceX++; inputCooldown = millis(); }
      if (sharedButtonStates[0] == LOW) { 
        uint8_t nextRot = (currentRotation + 1) % 4;
        if (!checkTetrisCollision(currentPieceX, currentPieceY, nextRot)) currentRotation = nextRot;
        inputCooldown = millis(); tone(PIN_BUZZER, 400, 5);
      }
    }

    long dropInterval = max(100, 800 - (gameLevel * 90));
    if (sharedButtonStates[1] == LOW) dropInterval = 30;

    if (millis() - genericTickTimer > dropInterval) {
      genericTickTimer = millis();
      if (!checkTetrisCollision(currentPieceX, currentPieceY + 1, currentRotation)) {
        currentPieceY++;
      } else {
        lockTetrisPiece();
        currentPieceX = 3; currentPieceY = 0;
        currentPieceType = random(0, 7); currentRotation = 0;
        if (checkTetrisCollision(currentPieceX, currentPieceY, currentRotation)) {
          tone(PIN_BUZZER, 100, 400); triggerActionFlash = true; exit_scene = true;
        }
      }
    }

    display.clearDisplay();
    display.drawRect(38, 1, 52, 51, SSD1306_WHITE);
    for (int r = 0; r < TETRIS_ROWS; r++) {
      for (int c = 0; c < TETRIS_COLS; c++) {
        if (tetrisGrid[r][c] != 0) display.fillRect(40 + (c * 5), 2 + (r * 3), 4, 2, SSD1306_WHITE);
      }
    }

    uint16_t shape = tetrisShapes[currentPieceType][currentRotation];
    for (int r = 0; r < 4; r++) {
      for (int c = 0; c < 4; c++) {
        if ((shape & (0x8000 >> (r * 4 + c))) != 0) {
          int16_t drawX = 40 + ((currentPieceX + c) * 5);
          int16_t drawY = 2 + ((currentPieceY + r) * 3);
          if (drawY >= 2 && drawY < 50) display.fillRect(drawX, drawY, 4, 2, SSD1306_WHITE);
        }
      }
    }
    drawBottomHUD("TETRIS BLOCKS"); display.display(); delay(15);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// HIGHWAY TRAFFIC RACER
// ================================================================
void initializeRacingState() {
  playerCarX = 56; playerCarY = 40; roadScrollOffset = 0; currentSpeed = 2.0;
  for (int i = 0; i < 3; i++) {
    traffic[i].lane = i; traffic[i].x = 24 + (i * 32) + 10;
    traffic[i].y = -random(20, 80); traffic[i].active = true;
  }
}

void loopRacingGameScene() {
  exit_scene = false;
  do {
    if (sharedButtonStates[2] == LOW && playerCarX > 22) playerCarX -= 2;
    if (sharedButtonStates[3] == LOW && playerCarX < 94) playerCarX += 2;

    currentSpeed = 2.0 + (gameLevel * 0.4);
    roadScrollOffset += currentSpeed;
    if (roadScrollOffset >= 16) roadScrollOffset = 0;

    for (int i = 0; i < 3; i++) {
      if (traffic[i].active) {
        traffic[i].y += (currentSpeed - 0.8);
        if (traffic[i].y > 52) {
          traffic[i].y = -random(16, 64);
          arcadeGlobalScore += 10; tone(PIN_BUZZER, 500, 10);
          if (arcadeGlobalScore % 60 == 0) gameLevel++;
        }
        if (traffic[i].y > playerCarY - 10 && traffic[i].y < playerCarY + 10) {
          if (abs(playerCarX - traffic[i].x) < 10) {
            tone(PIN_BUZZER, 80, 300); triggerActionFlash = true; exit_scene = true;
          }
        }
      }
    }

    display.clearDisplay();
    display.drawFastVLine(18, 0, 52, SSD1306_WHITE); display.drawFastVLine(110, 0, 52, SSD1306_WHITE);
    for (int y = (int)roadScrollOffset - 16; y < 52; y += 16) {
      display.drawFastVLine(48, y, 8, SSD1306_WHITE); display.drawFastVLine(80, y, 8, SSD1306_WHITE);
    }

    display.fillRect(playerCarX, playerCarY, 10, 10, SSD1306_WHITE);
    display.drawRect(playerCarX + 2, playerCarY - 2, 6, 2, SSD1306_WHITE);
    for (int i = 0; i < 3; i++) {
      if (traffic[i].active && traffic[i].y > -10) {
        display.fillRect(traffic[i].x, (int)traffic[i].y, 10, 10, SSD1306_WHITE);
      }
    }
    drawBottomHUD("HIGHWAY RACER"); display.display(); delay(15);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// SPACE INVADERS MODULE
// ================================================================
void initializeInvadersState() {
  invShipX = 58; invLaserX = -1; invLaserY = -1; 
  invaderVelocityX = 1.2 + (gameLevel * 0.3); 
  invaderStepY = 12;
  for (int r = 0; r < 2; r++) {
    for (int c = 0; c < 6; c++) {
      int idx = r * 6 + c;
      invaders[idx].x = 15 + c * 16; invaders[idx].y = 4 + r * 10; invaders[idx].alive = true;
    }
  }
}

void loopInvadersGameScene() {
  exit_scene = false; bool shootLatch = true;
  do {
    if (sharedButtonStates[2] == LOW && invShipX > 4) invShipX -= 2;
    if (sharedButtonStates[3] == LOW && invShipX < 114) invShipX += 2;

    if (sharedButtonStates[4] == HIGH) shootLatch = true;
    if (sharedButtonStates[4] == LOW && shootLatch && invLaserY < 0) {
      invLaserX = invShipX + 5; invLaserY = 44; shootLatch = false; triggerActionFlash = true;
    }

    if (invLaserY >= 0) {
      invLaserY -= 3;
      if (invLaserY < 0) invLaserX = -1;
    }

    bool shiftDown = false;
    for (int i = 0; i < 12; i++) {
      if (invaders[i].alive) {
        invaders[i].x += invaderVelocityX;
        if (invaders[i].x > 116 || invaders[i].x < 2) shiftDown = true;
        if (invaders[i].y >= 42) { tone(PIN_BUZZER, 70, 500); exit_scene = true; }
      }
    }

    if (shiftDown) {
      invaderVelocityX = -invaderVelocityX;
      for (int i = 0; i < 12; i++) { if (invaders[i].alive) invaders[i].y += 3; }
    }

    if (invLaserY >= 0) {
      for (int i = 0; i < 12; i++) {
        if (invaders[i].alive && invLaserX >= invaders[i].x && invLaserX <= invaders[i].x + 10) {
          if (invLaserY >= invaders[i].y && invLaserY <= invaders[i].y + 6) {
            invaders[i].alive = false; invLaserY = -1; invLaserX = -1;
            arcadeGlobalScore += 20; tone(PIN_BUZZER, 600, 15); triggerActionFlash = true;
          }
        }
      }
    }

    bool clearRound = true;
    for (int i = 0; i < 12; i++) { if (invaders[i].alive) clearRound = false; }
    if (clearRound) { gameLevel++; initializeInvadersState(); }

    display.clearDisplay();
    display.fillRect(invShipX, 46, 11, 5, SSD1306_WHITE); display.fillRect(invShipX + 4, 43, 3, 3, SSD1306_WHITE);
    if (invLaserY >= 0) display.drawFastVLine(invLaserX, invLaserY, 4, SSD1306_WHITE);

    for (int i = 0; i < 12; i++) {
      if (invaders[i].alive) display.fillRect(invaders[i].x, invaders[i].y, 9, 6, SSD1306_WHITE);
    }
    drawBottomHUD("INVADERS"); display.display(); delay(15);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// FLAPPY BIRD FLIGHT SIMULATOR
// ================================================================
void initializeFlappyState() {
  birdY = 20.0; birdVelocity = 0.0; pipeX = 128; pipeGapY = 16; pipeGapHeight = max(11, 22 - gameLevel);
}

void loopFlappyGameScene() {
  exit_scene = false; bool flapLatch = true;
  do {
    if (sharedButtonStates[4] == HIGH) flapLatch = true;
    if (sharedButtonStates[4] == LOW && flapLatch) {
      birdVelocity = -1.8; flapLatch = false; tone(PIN_BUZZER, 450, 15); triggerActionFlash = true;
    }

    birdVelocity += 0.12; birdY += birdVelocity;
    if (birdY < 1) birdY = 1;
    if (birdY > 46) { tone(PIN_BUZZER, 80, 300); triggerActionFlash = true; exit_scene = true; }

    pipeX -= (2 + (gameLevel / 2));
    if (pipeX < -14) {
      pipeX = 128; pipeGapY = random(6, 26);
      arcadeGlobalScore += 1; tone(PIN_BUZZER, 800, 10); triggerActionFlash = true;
      if (arcadeGlobalScore > 0 && arcadeGlobalScore % 2 == 0) gameLevel++;
    }

    int activeGapHeight = max(11, 20 - gameLevel);
    if (pipeX <= 22 && pipeX >= 6) {
      if ((int)birdY <= pipeGapY || (int)birdY >= (pipeGapY + activeGapHeight - 3)) {
        tone(PIN_BUZZER, 90, 400); triggerActionFlash = true; exit_scene = true;
      }
    }

    display.clearDisplay();
    display.fillRect(14, (int)birdY, 4, 4, SSD1306_WHITE);
    display.fillRect(pipeX, 0, 12, pipeGapY, SSD1306_WHITE);
    display.fillRect(pipeX, pipeGapY + activeGapHeight, 12, 52 - (pipeGapY + activeGapHeight), SSD1306_WHITE);
    drawBottomHUD("FLAPPY BIRD"); display.display(); delay(15);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// PONG RETRO ACTION
// ================================================================
void initializePongState() {
  paddleX = 2; pongAIY = 20; ballX = 64; ballY = 24; ballVelX = 1.5; ballVelY = 1.0;
}

void loopPongGameScene() {
  exit_scene = false;
  do {
    if (sharedButtonStates[0] == LOW && paddleX > 2) paddleX -= 2;
    if (sharedButtonStates[1] == LOW && paddleX < 38) paddleX += 2;

    ballX += ballVelX; ballY += ballVelY;
    if (ballY <= 2 || ballY >= 49) { ballVelY = -ballVelY; tone(PIN_BUZZER, 300, 5); }

    if ((int)ballY > pongAIY + 4 && pongAIY < 38) pongAIY += 1;
    if ((int)ballY < pongAIY + 4 && pongAIY > 2) pongAIY -= 1;

    if (ballX <= 7 && ballX >= 5) {
      if (ballY >= paddleX - 1 && ballY <= paddleX + 13) {
        ballVelX = -ballVelX; ballX = 8; tone(PIN_BUZZER, 600, 10);
      }
    }
    if (ballX >= 119 && ballX <= 121) {
      if (ballY >= pongAIY - 1 && ballY <= pongAIY + 13) {
        ballVelX = -ballVelX; ballX = 118; tone(PIN_BUZZER, 500, 10);
      }
    }

    if (ballX < 2) { arcadeGlobalScore = max(0, arcadeGlobalScore - 10); initializePongState(); tone(PIN_BUZZER, 120, 150); }
    if (ballX > 124) { arcadeGlobalScore += 25; gameLevel++; initializePongState(); tone(PIN_BUZZER, 800, 80); triggerActionFlash = true; }

    display.clearDisplay();
    display.drawRect(1, 1, 126, 51, SSD1306_WHITE);
    display.fillRect(4, paddleX, 3, 12, SSD1306_WHITE);
    display.fillRect(121, pongAIY, 3, 12, SSD1306_WHITE);
    display.fillRect((int)ballX, (int)ballY, 3, 3, SSD1306_WHITE);
    drawBottomHUD("PONG ACTION"); display.display(); delay(15);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// ASTEROIDS ENGINE (FIXED: IMPLEMENTED UNRESOLVED REFERENCE)
// ================================================================
void initializeAsteroidsState() {
  astShipX = 64.0; astShipY = 26.0; astShipVx = 0.0; astShipVy = 0.0; astShipAngle = 0.0;
  astLaserX = -1.0; astLaserY = -1.0;
  for (int i = 0; i < 4; i++) {
    rocks[i].x = random(0, SCREEN_WIDTH);
    rocks[i].y = random(0, 40);
    rocks[i].vx = (random(-10, 10) / 10.0) * (0.5 + gameLevel * 0.2);
    rocks[i].vy = (random(-10, 10) / 10.0) * (0.5 + gameLevel * 0.2);
    rocks[i].active = true;
  }
}

void loopAsteroidsGameScene() {
  exit_scene = false;
  initializeAsteroidsState();
  do {
    // Simple orientation rotation mechanics
    if (sharedButtonStates[2] == LOW) astShipAngle -= 0.1;
    if (sharedButtonStates[3] == LOW) astShipAngle += 0.1;
    
    // Thruster acceleration forward path logic
    if (sharedButtonStates[0] == LOW) {
      astShipVx += cos(astShipAngle) * 0.05;
      astShipVy += sin(astShipAngle) * 0.05;
    }
    
    // Position Update & Border Wraparound Handling
    astShipX += astShipVx; astShipY += astShipVy;
    astShipVx *= 0.98; astShipVy *= 0.98; // Drag friction falloff
    if (astShipX < 2) astShipX = 126; if (astShipX > 126) astShipX = 2;
    if (astShipY < 2) astShipY = 50;  if (astShipY > 50)  astShipY = 2;

    // Laser weapon registration system processing
    if (sharedButtonStates[4] == LOW && astLaserX < 0) {
      astLaserX = astShipX; astLaserY = astShipY;
      astLaserVx = cos(astShipAngle) * 3.0; astLaserY = sin(astShipAngle) * 3.0;
      tone(PIN_BUZZER, 700, 10); triggerActionFlash = true;
    }
    if (astLaserX >= 0) {
      astLaserX += astLaserVx; astLaserY += astLaserVy;
      if (astLaserX < 0 || astLaserX > 128 || astLaserY < 0 || astLaserY > 52) astLaserX = -1;
    }

    // Process Active Asteroid Drift updates
    for (int i = 0; i < 4; i++) {
      if (rocks[i].active) {
        rocks[i].x += rocks[i].vx; rocks[i].y += rocks[i].vy;
        if (rocks[i].x < 2) rocks[i].x = 126; if (rocks[i].x > 126) rocks[i].x = 2;
        if (rocks[i].y < 2) rocks[i].y = 50;  if (rocks[i].y > 50)  rocks[i].y = 2;

        // Vector Collision against ship checks
        if (abs(astShipX - rocks[i].x) < 6 && abs(astShipY - rocks[i].y) < 6) {
          tone(PIN_BUZZER, 90, 200); triggerActionFlash = true; exit_scene = true;
        }
      }
    }

    display.clearDisplay();
    display.drawRect(1, 1, 126, 51, SSD1306_WHITE);
    
    // Draw simple triangular ship profile geometry
    int x1 = astShipX + cos(astShipAngle) * 6; int y1 = astShipY + sin(astShipAngle) * 6;
    int x2 = astShipX + cos(astShipAngle + 2.5) * 4; int y2 = astShipY + sin(astShipAngle + 2.5) * 4;
    int x3 = astShipX + cos(astShipAngle - 2.5) * 4; int y3 = astShipY + sin(astShipAngle - 2.5) * 4;
    display.drawTriangle(x1, y1, x2, y2, x3, y3, SSD1306_WHITE);

    if (astLaserX >= 0) display.drawPixel((int)astLaserX, (int)astLaserY, SSD1306_WHITE);
    for (int i = 0; i < 4; i++) {
      if (rocks[i].active) display.drawCircle((int)rocks[i].x, (int)rocks[i].y, 4, SSD1306_WHITE);
    }

    drawBottomHUD("ASTEROIDS"); display.display(); delay(15);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}

// ================================================================
// CROSSY FROGGER
// ================================================================
void initializeFroggerState() {
  frogX = 62; frogY = 46;
  for (int i = 0; i < 4; i++) { obstacleX[i] = random(0, 80); obstacleSpeed[i] = (i % 2 == 0 ? 1.0 : -1.0) * (0.8 + gameLevel * 0.25); }
}

void loopFroggerGameScene() {
  exit_scene = false; bool releaseX = true;
  do {
    if (sharedButtonStates[0] == HIGH && sharedButtonStates[1] == HIGH && sharedButtonStates[2] == HIGH && sharedButtonStates[3] == HIGH) releaseX = true;
    if (releaseX) {
      if (sharedButtonStates[0] == LOW && frogY > 4)  { frogY -= 10; releaseX = false; tone(PIN_BUZZER, 600, 5); }
      if (sharedButtonStates[1] == LOW && frogY < 40) { frogY += 10; releaseX = false; tone(PIN_BUZZER, 600, 5); }
      if (sharedButtonStates[2] == LOW && frogX > 10) { frogX -= 10; releaseX = false; tone(PIN_BUZZER, 600, 5); }
      if (sharedButtonStates[3] == LOW && frogX < 110) { frogX += 10; releaseX = false; tone(PIN_BUZZER, 600, 5); }
    }

    for (int i = 0; i < 4; i++) {
      obstacleX[i] += obstacleSpeed[i];
      if (obstacleX[i] > 120) obstacleX[i] = 0;
      if (obstacleX[i] < 0) obstacleX[i] = 120;

      int obstacleRowY = 10 + (i * 10);
      if (frogY == obstacleRowY) {
        if (frogX >= obstacleX[i] - 4 && frogX <= obstacleX[i] + 12) {
          tone(PIN_BUZZER, 80, 250); triggerActionFlash = true; frogX = 62; frogY = 46;
        }
      }
    }

    if (frogY <= 6) {
      arcadeGlobalScore += 50; gameLevel++; tone(PIN_BUZZER, 880, 100); triggerActionFlash = true; initializeFroggerState();
    }

    display.clearDisplay();
    display.drawRect(1, 1, 126, 51, SSD1306_WHITE);
    display.fillRect(frogX, frogY, 4, 4, SSD1306_WHITE);
    for (int i = 0; i < 4; i++) { display.fillRect(obstacleX[i], 10 + (i * 10), 12, 5, SSD1306_WHITE); }
    drawBottomHUD("CROSSY FROGGER"); display.display(); delay(15);
  } while (!exit_scene);
  currentScene = MAIN_MENU;
}