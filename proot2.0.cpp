/*
PROTOGEN FACE - ESP32 LED Matrix Controller  v5.0 By Theodor_de(นายธนพล สุดสวาท)
Panel layout (physical chain order):
0=EYE0  1=EYE1  2=MOUTH3  3=MOUTH2  4=MOUTH1  5=MOUTH0  6=NOSE
 Pins:  DIN->GPIO14  CLK->GPIO27  CS->GPIO26
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <string.h>
#include <stdlib.h>

#define PIN_DIN   14
#define PIN_CLK   27
#define PIN_CS    26

#define NUM_PANELS 7

#define PANEL_EYE0    0
#define PANEL_EYE1    1
#define PANEL_NOSE    6
#define PANEL_MOUTH3  2
#define PANEL_MOUTH2  3
#define PANEL_MOUTH1  4
#define PANEL_MOUTH0  5

#define MAX7219_NOOP        0x00
#define MAX7219_DIGIT0      0x01
#define MAX7219_DECODEMODE  0x09
#define MAX7219_INTENSITY   0x0A
#define MAX7219_SCANLIMIT   0x0B
#define MAX7219_SHUTDOWN    0x0C
#define MAX7219_DISPLAYTEST 0x0F

const char* AP_SSID     = "Theodor_de";
const char* AP_PASSWORD = "proot6484";

WebServer server(80);
Preferences prefs;

int  brightness    = 8;
int  blinkInterval = 3000;
bool blinkEnabled  = true;
bool autoAnimate   = false;
int  autoInterval  = 8000;
int  currentExpr   = 0;

unsigned long lastBlink      = 0;
unsigned long blinkStart     = 0;
bool          isBlinking     = false;

const int     BLINK_STEP_MS  = 16;
const int     BLINK_HOLD_MS  = 55;
int           blinkPhase     = 0;
int           blinkRow       = 0;
unsigned long blinkStepTime  = 0;
// ─────────────────────────────────────────────────────────

unsigned long lastAutoSwitch = 0;

uint8_t framebuf[NUM_PANELS][8];
uint8_t savedEyes[2][8];
bool    customDrawActive = false;

void spiSendByte(uint8_t b);
void maxSendAll(uint8_t reg, uint8_t val);
void maxSendRow(uint8_t reg, uint8_t* data);
void maxInit();
void maxSetIntensity(uint8_t val);
void fbSetPanel(int panel, byte* d);
void fbFlush();
void fbClear();
void drawExpression(int idx);
void drawBlink();
void updateBlinkAnim();
void restoreEyes();
void bootAnimation();
void fbSetPixel(int panel, int row, int col, bool on);

byte nose[8] = {
  0b11111111,0b01111110,0b00111100,0b00011000,
  0b00000000,0b00000000,0b00000000,0b00000000
};

byte eye_closed[8] = {
  0b00000000,0b00000000,0b11111111,0b11111111,
  0b00000000,0b00000000,0b00000000,0b00000000
};

struct Expr {
  const char* name;
  byte eye0[8], eye1[8], m0[8], m1[8], m2[8], m3[8];
};

Expr expressions[] = {
  {"Normal",
   {0b11100000,0b11111000,0b11111110,0b00000111,0b00000001,0,0,0},
   {0b00001111,0b00111111,0b11111111,0b11111110,0b11111100,0b01111000,0,0},
   {0b10000000,0b11000000,0b11110000,0b01111000,0b00111100,0b00011111,0b00000111,0},
   {0,0b00000011,0b00001111,0b00111110,0b11110000,0b11000000,0b10000000,0},
   {0b11110000,0b11111110,0b10011111,0b00001111,0b00000001,0,0,0},
   {0b00001111,0b00011101,0b01110001,0b11111111,0b11111110,0b11110000,0,0},
  },
  {"Happy",
   {0b00000001,0b00000111,0b00011111,0b01111110,0b11111000,0b11100000,0,0},
   {0b10000000,0b11100000,0b11111000,0b01111110,0b00011111,0b00000111,0,0},
   {0,0b10000000,0b11100000,0b11111000,0b01111111,0b00011111,0b00000011,0},
   {0,0b00000001,0b00000111,0b00111111,0b11111100,0b11100000,0b10000000,0},
   {0,0,0b00000011,0b10001111,0b11111110,0b11111111,0b11000000,0},
   {0,0,0b11000000,0b11110001,0b01111111,0b11111110,0b00000011,0},
  },
  {"Angry",
   {0b11111110,0b01111110,0b00111110,0b00011111,0b00001111,0,0,0},
   {0,0b11110000,0b11111000,0b11111100,0b01111110,0b00111111,0,0},
   {0,0,0b11111111,0b11111111,0,0,0,0},
   {0,0,0b11111111,0b11111111,0,0,0,0},
   {0,0,0b11111111,0b11111111,0,0,0,0},
   {0,0,0b11111111,0b11111111,0,0,0,0},
  },
  {"Sad",
   {0b00000001,0b00000111,0b00011111,0b01111100,0b11110000,0b11000000,0,0},
   {0b10000000,0b11100000,0b11111000,0b00111110,0b00001111,0b00000011,0,0},
   {0b00000011,0b00011111,0b01111111,0b11111000,0b11100000,0b10000000,0,0},
   {0b11000000,0b11100000,0b11111100,0b00111111,0b00000111,0b00000001,0,0},
   {0b11000000,0b11111111,0b11111110,0b10001111,0b00000011,0,0,0},
   {0b00000011,0b11111111,0b01111111,0b11110001,0b11000000,0,0,0},
  },
  {"Wink",
   {0b11100000,0b11111000,0b11111110,0b00000111,0b00000001,0,0,0},
   {0,0,0b11111111,0b11111111,0,0,0,0},
   {0b10000000,0b11000000,0b11110000,0b01111000,0b00111100,0b00011111,0b00000111,0},
   {0,0b00000011,0b00001111,0b00111110,0b11110000,0b11000000,0b10000000,0},
   {0b11110000,0b11111110,0b10011111,0b00001111,0b00000001,0,0,0},
   {0b00001111,0b00011101,0b01110001,0b11111111,0b11111110,0b11110000,0,0},
  },
  {"Surp.",
   {0b11000000,0b01100000,0b00110000,0b00010000,0b00010000,0b00110000,0b01100000,0b11000000},
   {0b00000011,0b00000110,0b00001100,0b00001000,0b00001000,0b00001100,0b00000110,0b00000011},
   {0b00000000,0b00000000,0b00000000,0b00000001,0b00000011,0b00001111,0b00111100,0b11111000},
   {0b00000000,0b00000000,0b00000000,0b10000000,0b11110000,0b00111100,0b00001111,0b00000011},
   {0b00000000,0b00000000,0b00000000,0b00000111,0b00011111,0b01111000,0b11100000,0b10000000},
   {0b00111111,0b00011100,0b01110010,0b11110001,0b11001001,0b00100101,0b00010010,0b00001100},
  },
  {"Dead",
   {0b10000001,0b01000010,0b00100100,0b00011000,0b00011000,0b00100100,0b01000010,0b10000001},
   {0b10000001,0b01000010,0b00100100,0b00011000,0b00011000,0b00100100,0b01000010,0b10000001},
   {0b10000000,0b11000000,0b11110000,0b01111000,0b00111100,0b00011111,0b00000111,0},
   {0,0b00000011,0b00001111,0b00111110,0b11110000,0b11000000,0b10000000,0},
   {0b11110000,0b11111110,0b10011111,0b00001111,0b00000001,0,0,0},
   {0b00001111,0b00011101,0b01110001,0b11111111,0b11111110,0b11110000,0,0},
  },
  {"UwU",
   {0b00011000,0b00111100,0b01111110,0b11111111,0b01111110,0b00111100,0b00011000,0},
   {0b00011000,0b00111100,0b01111110,0b11111111,0b01111110,0b00111100,0b00011000,0},
   {0,0,0,0b11111111,0b11111110,0b11100000,0,0},
   {0,0,0,0b11111111,0b00111111,0b00000111,0,0},
   {0,0,0,0b11111111,0b11111110,0b11100000,0,0},
   {0,0,0,0b11111111,0b00111111,0b00000111,0,0},
  },
};

const int EXPR_COUNT = sizeof(expressions) / sizeof(expressions[0]);

bool  gameActive  = false;
bool  gameOver    = false;
bool  gamePaused  = false;
int   score       = 0;
int   highScore   = 0;

int   difficulty       = 1;
float difficultyStart[3]   = {0.030f, 0.040f, 0.055f};
float difficultyStep[3]    = {0.004f, 0.006f, 0.008f};
float difficultyCap[3]     = {0.10f,  0.14f,  0.19f};

int           lastMilestone = 0;
unsigned long flashUntil    = 0;
const int     MILESTONE_STEP = 50;
const int     FLASH_MS       = 130;

float dinoY       = 4;
bool  dinoJumping = false;
bool  dinoDucking = false;
float dinoVY      = 0.0f;

const float GRAVITY   = 0.10f;
const float JUMP_VY   = -3.5f;

#define FIELD_W  32
#define FIELD_H  8
#define GROUND_Y 6
#define DINO_X   4
#define MAX_OBS  3

struct Obs {
  int  x;
  int  type;
  int  birdY;
  bool active;
  bool scored;
};

Obs   obstacles[MAX_OBS];
float obsX[MAX_OBS];

float         gameSpeed    = 0.04f;
unsigned long lastFrame    = 0;
unsigned long deathTime    = 0;

void gameInit();
void dinoJump();
void drawDinoOnField(bool field[FIELD_H][FIELD_W]);
void drawObstacleOnField(bool field[FIELD_H][FIELD_W], const Obs& o);
bool checkCollision(const Obs& o);
void fieldToFramebuf(bool field[FIELD_H][FIELD_W]);
void drawScoreBar();
void drawDinoEye();
void gameFrame();

void handleGameStart();
void handleGamePause();
String framebufToHex();
void handleGameJump();
void handleGameDuck();
void handleGameExit();
void handleGameStatus();
void handleRoot();
void handleExpr();
void handleBlink();
void handleBrightness();
void handleAuto();
void handleClear();
void handleReset();
void handleDraw();
void handleDrawAll();
void handleStatus();

void spiSendByte(uint8_t b) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(PIN_DIN, (b >> i) & 1);
    digitalWrite(PIN_CLK, HIGH);
    digitalWrite(PIN_CLK, LOW);
  }
}

void maxSendAll(uint8_t reg, uint8_t val) {
  digitalWrite(PIN_CS, LOW);
  for (int i = 0; i < NUM_PANELS; i++) {
    spiSendByte(reg);
    spiSendByte(val);
  }
  digitalWrite(PIN_CS, HIGH);
}

void maxSendRow(uint8_t reg, uint8_t* data) {
  digitalWrite(PIN_CS, LOW);
  for (int i = NUM_PANELS - 1; i >= 0; i--) {
    spiSendByte(reg);
    spiSendByte(data[i]);
  }
  digitalWrite(PIN_CS, HIGH);
}

void maxInit() {
  pinMode(PIN_DIN, OUTPUT);
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_CLK, LOW);
  maxSendAll(MAX7219_DISPLAYTEST, 0x00);
  maxSendAll(MAX7219_SCANLIMIT,   0x07);
  maxSendAll(MAX7219_DECODEMODE,  0x00);
  maxSendAll(MAX7219_INTENSITY,   brightness);
  maxSendAll(MAX7219_SHUTDOWN,    0x01);
  for (int r = 0; r < 8; r++) maxSendAll(MAX7219_DIGIT0 + r, 0x00);
}

void maxSetIntensity(uint8_t val) {
  maxSendAll(MAX7219_INTENSITY, val & 0x0F);
}

void fbSetPanel(int panel, byte* d) {
  for (int r = 0; r < 8; r++) framebuf[panel][r] = d[r];
}

void fbFlush() {
  for (int r = 0; r < 8; r++) {
    uint8_t rv[NUM_PANELS];
    for (int p = 0; p < NUM_PANELS; p++) rv[p] = framebuf[p][r];
    maxSendRow(MAX7219_DIGIT0 + r, rv);
  }
}

void fbClear() {
  memset(framebuf, 0, sizeof(framebuf));
  fbFlush();
}
String framebufToHex() {
  String hex;
  hex.reserve(NUM_PANELS * 8 * 2);
  char buf[3];
  for (int p = 0; p < NUM_PANELS; p++)
    for (int r = 0; r < 8; r++) {
      snprintf(buf, sizeof(buf), "%02x", framebuf[p][r]);
      hex += buf;
    }
  return hex;
}

void gameInit() {
  score         = 0;
  gameOver      = false;
  gamePaused    = false;
  lastMilestone = 0;
  flashUntil    = 0;
  gameSpeed     = difficultyStart[difficulty];
  dinoY       = GROUND_Y - 2;
  dinoJumping = false;
  dinoDucking = false;
  dinoVY      = 0;
  obsX[0] = 46; obsX[1] = 74; obsX[2] = 104;
  obstacles[0] = {46,  0, 2, true, false};
  obstacles[1] = {74,  1, 3, true, false};
  obstacles[2] = {104, 2, 2, true, false};
  lastFrame = millis();
}

void dinoJump() {
  if (!dinoJumping && !gameOver) { dinoJumping = true; dinoVY = JUMP_VY; }
}

void drawDinoOnField(bool field[FIELD_H][FIELD_W]) {
  if (dinoDucking) {
    for (int dc = 0; dc < 2; dc++) {
      int x = DINO_X + dc;
      if (x < FIELD_W) { field[GROUND_Y-1][x] = true; field[GROUND_Y][x] = true; }
    }
  } else {
    int top = (int)dinoY;
    for (int dc = 0; dc < 2; dc++) {
      int x = DINO_X + dc;
      if (x < FIELD_W) {
        if (top   >= 0 && top   < FIELD_H) field[top  ][x] = true;
        if (top+1 >= 0 && top+1 < FIELD_H) field[top+1][x] = true;
        if (top+2 >= 0 && top+2 < FIELD_H) field[top+2][x] = true;
      }
    }
  }
}

void drawObstacleOnField(bool field[FIELD_H][FIELD_W], const Obs& o) {
  int x = o.x;
  if (o.type == 0) {
    for (int r = GROUND_Y-2; r <= GROUND_Y; r++)
      if (x >= 0 && x < FIELD_W) field[r][x] = true;
    if (x-1 >= 0 && x-1 < FIELD_W) field[GROUND_Y-1][x-1] = true;
  } else if (o.type == 1) {
    for (int c = 0; c <= 1; c++) {
      int cx = x + c*2;
      for (int r = GROUND_Y-2; r <= GROUND_Y; r++)
        if (cx >= 0 && cx < FIELD_W) field[r][cx] = true;
    }
  } else {
    for (int dc = 0; dc < 3; dc++) {
      int bx = x + dc;
      if (bx >= 0 && bx < FIELD_W) {
        if (o.birdY   < FIELD_H) field[o.birdY  ][bx] = true;
        if (o.birdY+1 < FIELD_H) field[o.birdY+1][bx] = true;
      }
    }
  }
}

bool checkCollision(const Obs& o) {
  int dx0 = DINO_X, dx1 = DINO_X + 1;
  int dy0 = dinoDucking ? GROUND_Y-1 : (int)dinoY;
  int dy1 = dinoDucking ? GROUND_Y   : (int)dinoY + 2;
  int ox0, ox1, oy0, oy1;
  if (o.type == 0)      { ox0=o.x-1; ox1=o.x;   oy0=GROUND_Y-2; oy1=GROUND_Y; }
  else if (o.type == 1) { ox0=o.x;   ox1=o.x+3;  oy0=GROUND_Y-2; oy1=GROUND_Y; }
  else                  { ox0=o.x;   ox1=o.x+2;  oy0=o.birdY;    oy1=o.birdY+1; }
  return !(dx1 < ox0 || dx0 > ox1 || dy1 < oy0 || dy0 > oy1);
}

void fieldToFramebuf(bool field[FIELD_H][FIELD_W]) {
  for (int c = 0; c < FIELD_W; c++) field[GROUND_Y][c] = true;
  int panelMap[4] = {5, 4, 3, 2};
  for (int seg = 0; seg < 4; seg++) {
    int panel = panelMap[seg];
    for (int r = 0; r < FIELD_H; r++) {
      uint8_t byt = 0;
      for (int bit = 0; bit < 8; bit++)
        if (field[r][seg*8+bit]) byt |= (1 << (7-bit));
      framebuf[panel][r] = byt;
    }
  }
}

void drawScoreBar() {
  int pts = (score < 56) ? score : 56;
  for (int r = 0; r < 8; r++) {
    uint8_t b = 0;
    for (int c = 0; c < 8; c++) if (r*8+c < pts) b |= (1 << (7-c));
    framebuf[PANEL_EYE1][r] = b;
  }
}

void drawDinoEye() {
  if (gameOver) {
    byte dead[8] = {0b10000001,0b01000010,0b00100100,0b00011000,
                    0b00011000,0b00100100,0b01000010,0b10000001};
    fbSetPanel(PANEL_EYE0, dead);
  } else {
    fbSetPanel(PANEL_EYE0, expressions[0].eye0);
  }
}

void gameFrame() {
  unsigned long now = millis();
  float dt = (float)(now - lastFrame);
  lastFrame = now;
  if (dt > 50) dt = 50;

  if (dinoJumping) {
    dinoVY += GRAVITY;
    dinoY  += dinoVY * dt * 0.05f;
    if (dinoY >= GROUND_Y - 2) { dinoY = GROUND_Y-2; dinoJumping = false; dinoVY = 0; }
  }

  float move = gameSpeed * dt;
  for (int i = 0; i < MAX_OBS; i++) {
    if (!obstacles[i].active) continue;
    obsX[i] -= move;
    obstacles[i].x = (int)obsX[i];

    if (!obstacles[i].scored && obsX[i] < DINO_X - 2) {
      obstacles[i].scored = true;
      score++;
      if (score > highScore) highScore = score;
      if (score % 10 == 0) {
        float cap = difficultyCap[difficulty];
        gameSpeed = (gameSpeed + difficultyStep[difficulty] < cap) ? gameSpeed + difficultyStep[difficulty] : cap;
      }
      if (score > 0 && score % MILESTONE_STEP == 0 && score != lastMilestone) {
        lastMilestone = score;
        flashUntil    = now + FLASH_MS;
      }
    }

    if (obsX[i] < -5) {
      int minGap = 16 + (int)(gameSpeed * 130.0f);
      obsX[i] = FIELD_W + minGap + random(0, 24);
      obstacles[i].x = (int)obsX[i];
      obstacles[i].scored = false;
      int t = random(0, 10);
      if (t < 5) obstacles[i].type = 0;
      else if (t < 8) obstacles[i].type = 1;
      else obstacles[i].type = 2;
      obstacles[i].birdY = random(0, 2) ? 2 : 3;
    }
    if (!gameOver && checkCollision(obstacles[i])) {
      gameOver  = true;
      deathTime = now;
      prefs.putUInt("hi", highScore);
    }
  }

  bool field[FIELD_H][FIELD_W];
  memset(field, 0, sizeof(field));
  drawDinoOnField(field);
  for (int i = 0; i < MAX_OBS; i++)
    if (obstacles[i].active) drawObstacleOnField(field, obstacles[i]);
  fieldToFramebuf(field);
  drawScoreBar();
  drawDinoEye();

  if (now < flashUntil) {
    int panelMap[4] = {5, 4, 3, 2};
    for (int seg = 0; seg < 4; seg++)
      for (int r = 0; r < 8; r++)
        framebuf[panelMap[seg]][r] ^= 0xFF;
  }

  fbFlush();
}

void drawExpression(int idx) {
  Expr& e = expressions[idx];
  fbSetPanel(PANEL_EYE0,   e.eye0);
  fbSetPanel(PANEL_EYE1,   e.eye1);
  fbSetPanel(PANEL_NOSE,   nose);
  fbSetPanel(PANEL_MOUTH0, e.m0);
  fbSetPanel(PANEL_MOUTH1, e.m1);
  fbSetPanel(PANEL_MOUTH2, e.m2);
  fbSetPanel(PANEL_MOUTH3, e.m3);
  fbFlush();
  customDrawActive = false;
  memcpy(savedEyes[0], framebuf[PANEL_EYE0], 8);
  memcpy(savedEyes[1], framebuf[PANEL_EYE1], 8);
}

void drawBlink() {
  // Save whatever is showing (expression or custom draw)
  memcpy(savedEyes[0], framebuf[PANEL_EYE0], 8);
  memcpy(savedEyes[1], framebuf[PANEL_EYE1], 8);
  // Start animation
  blinkPhase    = 0;
  blinkRow      = 0;
  blinkStepTime = millis();
}

void updateBlinkAnim() {
  unsigned long now = millis();
  if (now - blinkStepTime < (unsigned long)BLINK_STEP_MS) return;

  if (blinkPhase == 0) {
    for (int r = 0; r < 8; r++) {
      if (r < blinkRow) {
        framebuf[PANEL_EYE0][r] = 0x00;
        framebuf[PANEL_EYE1][r] = 0x00;
      } else if (r == blinkRow) {
        uint8_t lid0 = 0, lid1 = 0;
        for (int rr = blinkRow; rr < 8; rr++) {
          lid0 |= savedEyes[0][rr];
          lid1 |= savedEyes[1][rr];
        }
        framebuf[PANEL_EYE0][r] = lid0;
        framebuf[PANEL_EYE1][r] = lid1;
      } else {
        framebuf[PANEL_EYE0][r] = savedEyes[0][r];
        framebuf[PANEL_EYE1][r] = savedEyes[1][r];
      }
    }
    fbFlush();
    blinkRow++;
    if (blinkRow >= 8) {
      memset(framebuf[PANEL_EYE0], 0x00, 8);
      memset(framebuf[PANEL_EYE1], 0x00, 8);
      fbFlush();
      blinkPhase    = 1;
      blinkStepTime = now;
      return;
    }

  } else if (blinkPhase == 1) {
    if (now - blinkStepTime >= (unsigned long)BLINK_HOLD_MS) {
      blinkPhase    = 2;
      blinkRow      = 7;
      blinkStepTime = now;
    }
    return;

  } else if (blinkPhase == 2) {

    for (int r = 0; r < 8; r++) {
      if (r > blinkRow) {
        framebuf[PANEL_EYE0][r] = savedEyes[0][r];
        framebuf[PANEL_EYE1][r] = savedEyes[1][r];
      } else if (r == blinkRow) {
        uint8_t lid0 = 0, lid1 = 0;
        for (int rr = 0; rr <= blinkRow; rr++) {
          lid0 |= savedEyes[0][rr];
          lid1 |= savedEyes[1][rr];
        }
        framebuf[PANEL_EYE0][r] = lid0;
        framebuf[PANEL_EYE1][r] = lid1;
      } else {
        framebuf[PANEL_EYE0][r] = 0x00;
        framebuf[PANEL_EYE1][r] = 0x00;
      }
    }
    fbFlush();
    blinkRow--;
    if (blinkRow < 0) {
      fbSetPanel(PANEL_EYE0, savedEyes[0]);
      fbSetPanel(PANEL_EYE1, savedEyes[1]);
      fbFlush();
      isBlinking = false;
      lastBlink  = millis();
      return;
    }
  }

  blinkStepTime = now;
}

void restoreEyes() {
  fbSetPanel(PANEL_EYE0, savedEyes[0]);
  fbSetPanel(PANEL_EYE1, savedEyes[1]);
  fbFlush();
}

void bootAnimation() {
  for (int r = 0; r < 8; r++) { maxSendAll(MAX7219_DIGIT0+r, 0xFF); delay(35); }
  delay(120);
  for (int r = 7; r >= 0; r--) { maxSendAll(MAX7219_DIGIT0+r, 0x00); delay(35); }
  delay(80);
}

void fbSetPixel(int panel, int row, int col, bool on) {
  if (panel < 0 || panel >= NUM_PANELS || row < 0 || row > 7 || col < 0 || col > 7) return;
  if (on) framebuf[panel][row] |=  (1 << (7-col));
  else    framebuf[panel][row] &= ~(1 << (7-col));
}

const char* HTML_PAGE = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Protogen Face</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
:root{
  --b:#0088ff;--b2:#0099ff;--b66:#0088ff66;--b40:#0088ff40;
  --dark:#060810;--card:#0a0c14;--brd:#001e33;--txt:#d0eeff;--dim:#224466;
  --on:#001833;--cell:38px;
}
body{background:var(--dark);color:var(--txt);font-family:'Courier New',monospace;min-height:100vh;padding:12px 12px 40px;max-width:520px;margin:0 auto}
h1{text-align:center;color:var(--b);font-size:1.25em;letter-spacing:5px;text-transform:uppercase;margin-bottom:2px;text-shadow:0 0 14px var(--b),0 0 30px var(--b66)}
.sub{text-align:center;font-size:.62em;color:#002244;letter-spacing:3px;margin-bottom:16px}
.card{background:var(--card);border:1px solid var(--brd);border-radius:14px;padding:14px;margin-bottom:11px}
.card h2{font-size:.68em;letter-spacing:3px;color:var(--b);text-transform:uppercase;margin-bottom:11px;padding-bottom:8px;border-bottom:1px solid var(--brd);opacity:.85}
.pv{display:flex;justify-content:center;padding:4px 0 2px}
canvas{border-radius:8px;cursor:crosshair}
.eg{display:grid;grid-template-columns:repeat(4,1fr);gap:7px}
.eb{background:#000b1a;border:2px solid var(--brd);color:var(--txt);border-radius:10px;padding:10px 3px 8px;font-family:inherit;font-size:.7em;cursor:pointer;text-align:center;transition:.12s;line-height:1.5;touch-action:manipulation}
.eb:active{transform:scale(.93)}
.eb.on{background:var(--on);border-color:var(--b);color:var(--b);box-shadow:0 0 12px var(--b40)}
.ei{font-size:1.25em;display:block}
.sr{display:flex;align-items:center;gap:10px;margin-bottom:12px}
.sl{min-width:78px;font-size:.73em;color:var(--dim);letter-spacing:1px}
input[type=range]{flex:1;-webkit-appearance:none;height:4px;background:#001a2e;border-radius:2px;outline:none;cursor:pointer}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;border-radius:50%;background:var(--b);cursor:pointer;box-shadow:0 0 8px var(--b)}
.sv{min-width:36px;text-align:right;font-size:.8em;color:var(--b);font-weight:bold}
.tr{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}
.tl{font-size:.76em;color:var(--dim);letter-spacing:1px}
.tg{position:relative;width:52px;height:28px;flex-shrink:0}
.tg input{display:none}
.ts{position:absolute;inset:0;background:#000d1f;border:1px solid var(--brd);border-radius:14px;cursor:pointer;transition:.22s}
.ts:before{content:'';position:absolute;width:22px;height:22px;left:2px;top:2px;background:#002244;border-radius:50%;transition:.22s}
input:checked+.ts{background:var(--on);border-color:var(--b)}
input:checked+.ts:before{transform:translateX(24px);background:var(--b);box-shadow:0 0 8px var(--b)}
.br{display:flex;gap:7px}
.bn{flex:1;background:#000b1a;border:2px solid var(--brd);color:var(--txt);border-radius:10px;padding:13px 4px;font-family:inherit;font-size:.76em;cursor:pointer;letter-spacing:.8px;transition:.12s;text-transform:uppercase;touch-action:manipulation}
.bn:active{transform:scale(.95)}
.bn:hover,.bn.act{border-color:var(--b);color:var(--b)}
.bn.blue{background:var(--on);border-color:var(--b);color:var(--b);box-shadow:0 0 10px var(--b40)}
.ps{display:grid;grid-template-columns:repeat(7,1fr);gap:4px;margin-bottom:10px}
.pb{background:#000b1a;border:1px solid var(--brd);color:var(--dim);border-radius:6px;padding:6px 2px;font-family:inherit;font-size:.6em;cursor:pointer;text-align:center;transition:.12s;touch-action:manipulation}
.pb:active{transform:scale(.93)}
.pb.on{background:var(--on);border-color:var(--b);color:var(--b)}
#drawGrid{display:grid;grid-template-columns:repeat(8,var(--cell));grid-template-rows:repeat(8,var(--cell));gap:3px;margin:0 auto;width:fit-content}
.px{width:var(--cell);height:var(--cell);border-radius:50%;background:#001220;border:1px solid #001e33;cursor:pointer;transition:background .08s;touch-action:manipulation}
.px.on{background:var(--b);box-shadow:0 0 7px var(--b)}
.tool-row{display:flex;gap:7px;margin-bottom:10px;align-items:center}
.tool-btn{flex:1;background:#000b1a;border:2px solid var(--brd);color:var(--dim);border-radius:8px;padding:9px 4px;font-family:inherit;font-size:.7em;cursor:pointer;text-align:center;touch-action:manipulation;transition:.12s}
.tool-btn.on{background:var(--on);border-color:var(--b);color:var(--b)}
.st{text-align:center;font-size:.63em;color:#001833;letter-spacing:2px;margin-top:14px;padding-top:12px;border-top:1px solid var(--brd);line-height:2}
.st b{color:var(--b)}
.dot{animation:bk 1.3s infinite}
@keyframes bk{0%,100%{opacity:1}50%{opacity:.15}}

/* ── Dino game ─────────────────────────────────────────── */
.diffRow{display:flex;gap:7px;margin-bottom:8px}
.diffbn{flex:1;background:#000b1a;border:2px solid var(--brd);color:var(--txt);border-radius:10px;padding:13px 4px;font-family:inherit;font-size:.78em;font-weight:bold;letter-spacing:1.5px;cursor:pointer;touch-action:manipulation;transition:.12s}
.diffbn:active{transform:scale(.95)}
.diffbn.on{border-color:var(--b);color:var(--b);background:var(--on);box-shadow:0 0 10px var(--b40)}
.hint{font-size:.62em;color:var(--dim);letter-spacing:.5px;text-align:center;margin-top:8px;line-height:1.6}
.scoreRow{display:flex;align-items:center;gap:10px;margin-bottom:10px}
.scoreBlk{flex:1;background:#000b1a;border:1px solid var(--brd);border-radius:10px;padding:8px 4px;text-align:center}
.scoreLbl{display:block;font-size:.62em;color:var(--dim);letter-spacing:2px;margin-bottom:2px}
.scoreVal{display:block;font-size:1.5em;color:var(--b);font-weight:bold;text-shadow:0 0 8px var(--b)}
.scoreVal.dim{color:var(--txt);text-shadow:none}
.pausebn{width:46px;height:46px;flex-shrink:0;background:#000b1a;border:2px solid var(--brd);color:var(--dim);border-radius:10px;font-size:1em;cursor:pointer;touch-action:manipulation}
.pausebn:active{transform:scale(.93)}
.pausebn.on{border-color:var(--b);color:var(--b)}
.banner{text-align:center;padding:12px;margin-bottom:10px;background:#001833;border:1px solid var(--b);border-radius:10px;font-size:.85em;color:var(--b);letter-spacing:2px}
.banner.over{letter-spacing:1px}
.goScore{font-size:.72em;color:var(--txt);letter-spacing:1px;margin-top:6px;opacity:.85}
.jumpbn{width:100%;background:#001833;border:2px solid var(--b);color:var(--b);border-radius:14px;padding:30px 4px;margin-bottom:8px;font-family:inherit;font-size:1em;cursor:pointer;letter-spacing:3px;text-transform:uppercase;touch-action:manipulation;box-shadow:0 0 16px var(--b40);transition:.08s}
.jumpbn:active{transform:scale(.97);box-shadow:0 0 26px var(--b)}
.gameRow2{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.secbn{background:#000b1a;border:2px solid var(--brd);color:var(--txt);border-radius:10px;padding:14px 4px;font-family:inherit;font-size:.8em;cursor:pointer;touch-action:manipulation}
.secbn:active{transform:scale(.95)}
</style>
</head>
<body>
<h1>&#11041; PROTOGEN &#11041;</h1>
<div class="sub">FACE CONTROLLER v5.0</div>
<div class="card"><h2 id="pvTitle">Live Preview</h2><div class="pv"><canvas id="cv"></canvas></div></div>
<div class="card cfg">
  <h2>Expression</h2>
  <div class="eg" id="eg">
    <button class="eb on" onclick="setE(0)"><span class="ei">&#9673;</span>Normal</button>
    <button class="eb" onclick="setE(1)"><span class="ei">&#9696;</span>Happy</button>
    <button class="eb" onclick="setE(2)"><span class="ei">&#9699;</span>Angry</button>
    <button class="eb" onclick="setE(3)"><span class="ei">&#9701;</span>Sad</button>
    <button class="eb" onclick="setE(4)"><span class="ei">&#9673;&#8212;</span>Wink</button>
    <button class="eb" onclick="setE(5)"><span class="ei">&#9675;</span>Surp.</button>
    <button class="eb" onclick="setE(6)"><span class="ei">&#10005;</span>Dead</button>
    <button class="eb" onclick="setE(7)"><span class="ei">&#9697;</span>UwU</button>
  </div>
</div>
<div class="card cfg">
  <h2>&#9999; Draw Mode</h2>
  <div class="ps">
    <button class="pb on" id="pb0" onclick="selPanel(0)">EYE0</button>
    <button class="pb" id="pb1" onclick="selPanel(1)">EYE1</button>
    <button class="pb" id="pb2" onclick="selPanel(2)">M3</button>
    <button class="pb" id="pb3" onclick="selPanel(3)">M2</button>
    <button class="pb" id="pb4" onclick="selPanel(4)">M1</button>
    <button class="pb" id="pb5" onclick="selPanel(5)">M0</button>
    <button class="pb" id="pb6" onclick="selPanel(6)">NOSE</button>
  </div>
  <div class="tool-row">
    <button class="tool-btn on" id="toolDraw" onclick="setTool('draw')">&#9999; DRAW</button>
    <button class="tool-btn" id="toolErase" onclick="setTool('erase')">&#11036; ERASE</button>
    <button class="tool-btn" id="toolFill" onclick="fillPanel(true)">&#9635; FILL</button>
    <button class="tool-btn" id="toolClearP" onclick="fillPanel(false)">&#9634; CLEAR</button>
    <button class="tool-btn" id="toolInvert" onclick="invertPanel()">&#11041; INVERT</button>
  </div>
  <div id="drawGrid"></div>
  <div class="br" style="margin-top:10px">
    <button class="bn" onclick="pushPanel()">&#9654; SEND TO FACE</button>
    <button class="bn" onclick="pushAll()">&#9654;&#9654; SEND ALL</button>
    <button class="bn" onclick="resetDraw()">&#8634; RELOAD EXPR</button>
  </div>
</div>
<div class="card cfg">
  <h2>Blink</h2>
  <div class="tr">
    <span class="tl">AUTO BLINK</span>
    <label class="tg"><input type="checkbox" id="blinkOn" checked onchange="api('/api/blink?enabled='+(this.checked?1:0))"><div class="ts"></div></label>
  </div>
  <div class="sr">
    <span class="sl">INTERVAL</span>
    <input type="range" min="500" max="10000" step="250" value="3000" id="blinkMs"
      oninput="V('blinkV',(this.value/1000).toFixed(1)+'s')"
      onchange="api('/api/blink?interval='+this.value)">
    <span class="sv" id="blinkV">3.0s</span>
  </div>
  <div class="br">
    <button class="bn" onclick="api('/api/blink?trigger=1')">&#9654; BLINK NOW</button>
  </div>
</div>
<div class="card cfg">
  <h2>Display</h2>
  <div class="sr">
    <span class="sl">BRIGHTNESS</span>
    <input type="range" min="0" max="15" value="8" id="bri"
      oninput="V('briV',this.value)"
      onchange="api('/api/brightness?val='+this.value)">
    <span class="sv" id="briV">8</span>
  </div>
  <div class="tr">
    <span class="tl">AUTO CYCLE</span>
    <label class="tg"><input type="checkbox" id="autoOn" onchange="api('/api/auto?enabled='+(this.checked?1:0))"><div class="ts"></div></label>
  </div>
  <div class="sr">
    <span class="sl">CYCLE TIME</span>
    <input type="range" min="2000" max="20000" step="1000" value="8000" id="autoMs"
      oninput="V('autoV',(this.value/1000)+'s')"
      onchange="api('/api/auto?interval='+this.value)">
    <span class="sv" id="autoV">8s</span>
  </div>
</div>
<div class="card cfg">
  <h2>Quick Actions</h2>
  <div class="br">
    <button class="bn" onclick="randE()">&#11041; RANDOM</button>
    <button class="bn" onclick="api('/api/clear')">&#10005; CLEAR</button>
    <button class="bn" onclick="api('/api/reset')">&#8634; RESET</button>
  </div>
</div>
<div class="card cfg" id="gameLaunchCard">
  <h2>&#128006; Dino Game &middot; Best: <span id="hiPreview">0</span></h2>
  <div class="diffRow">
    <button class="diffbn" onclick="startGame(0)">EASY</button>
    <button class="diffbn on" onclick="startGame(1)">NORMAL</button>
    <button class="diffbn" onclick="startGame(2)">HARD</button>
  </div>
  <div class="hint">tap a difficulty to play &middot; keyboard: space/&uarr; jump, &darr; duck, P pause</div>
</div>
<div class="card" id="gameCard" style="display:none">
  <div class="scoreRow">
    <div class="scoreBlk"><span class="scoreLbl">SCORE</span><span id="gScore" class="scoreVal">0</span></div>
    <div class="scoreBlk"><span class="scoreLbl">BEST</span><span id="gHi" class="scoreVal dim">0</span></div>
    <button id="pauseBtn" class="pausebn" onclick="togglePause()">&#10074;&#10074;</button>
  </div>

  <div id="pauseBanner" class="banner" style="display:none">PAUSED &middot; TAP TO RESUME</div>
  <div id="gameOverBanner" class="banner over" style="display:none">
    <div id="goTitle">GAME OVER</div>
    <div id="goScore" class="goScore"></div>
  </div>

  <button id="jumpBtn" class="jumpbn" ontouchstart="doJump()" onmousedown="doJump()">
    &#9650; JUMP
  </button>
  <div class="gameRow2">
    <button class="secbn" ontouchstart="doDuckOn()" ontouchend="doDuckOff()" onmousedown="doDuckOn()" onmouseup="doDuckOff()">&#9660; DUCK</button>
    <button class="secbn" onclick="exitGame()">&#10005; EXIT</button>
  </div>
  <div class="hint">the preview above mirrors the LED matrix live</div>
</div>
<div class="st">
  <span class="dot">&#9679;</span> CONNECTED &middot; ESP32 PROTOGEN<br>
  IP: <b id="ipLbl">192.168.4.1</b>
</div>

<script>
const PREVIEW_ORDER=[5,4,3,2,6,0,1];
let fb=Array.from({length:7},()=>new Uint8Array(8));
const EXPRS=[
  {0:[0b11100000,0b11111000,0b11111110,0b00000111,0b00000001,0,0,0],1:[0b00001111,0b00111111,0b11111111,0b11111110,0b11111100,0b01111000,0,0],5:[0b10000000,0b11000000,0b11110000,0b01111000,0b00111100,0b00011111,0b00000111,0],4:[0,0b00000011,0b00001111,0b00111110,0b11110000,0b11000000,0b10000000,0],3:[0b11110000,0b11111110,0b10011111,0b00001111,0b00000001,0,0,0],2:[0b00001111,0b00011101,0b01110001,0b11111111,0b11111110,0b11110000,0,0],6:[0b11111111,0b01111110,0b00111100,0b00011000,0,0,0,0]},
  {0:[0b00000001,0b00000111,0b00011111,0b01111110,0b11111000,0b11100000,0,0],1:[0b10000000,0b11100000,0b11111000,0b01111110,0b00011111,0b00000111,0,0],5:[0,0b10000000,0b11100000,0b11111000,0b01111111,0b00011111,0b00000011,0],4:[0,0b00000001,0b00000111,0b00111111,0b11111100,0b11100000,0b10000000,0],3:[0,0,0b00000011,0b10001111,0b11111110,0b11111111,0b11000000,0],2:[0,0,0b11000000,0b11110001,0b01111111,0b11111110,0b00000011,0],6:[0b11111111,0b01111110,0b00111100,0b00011000,0,0,0,0]},
  {0:[0b11111110,0b01111110,0b00111110,0b00011111,0b00001111,0,0,0],1:[0,0b11110000,0b11111000,0b11111100,0b01111110,0b00111111,0,0],5:[0,0,0b11111111,0b11111111,0,0,0,0],4:[0,0,0b11111111,0b11111111,0,0,0,0],3:[0,0,0b11111111,0b11111111,0,0,0,0],2:[0,0,0b11111111,0b11111111,0,0,0,0],6:[0b11111111,0b01111110,0b00111100,0b00011000,0,0,0,0]},
  {0:[0b00000001,0b00000111,0b00011111,0b01111100,0b11110000,0b11000000,0,0],1:[0b10000000,0b11100000,0b11111000,0b00111110,0b00001111,0b00000011,0,0],5:[0b00000011,0b00011111,0b01111111,0b11111000,0b11100000,0b10000000,0,0],4:[0b11000000,0b11100000,0b11111100,0b00111111,0b00000111,0b00000001,0,0],3:[0b11000000,0b11111111,0b11111110,0b10001111,0b00000011,0,0,0],2:[0b00000011,0b11111111,0b01111111,0b11110001,0b11000000,0,0,0],6:[0b11111111,0b01111110,0b00111100,0b00011000,0,0,0,0]},
  {0:[0b11100000,0b11111000,0b11111110,0b00000111,0b00000001,0,0,0],1:[0,0,0b11111111,0b11111111,0,0,0,0],5:[0b10000000,0b11000000,0b11110000,0b01111000,0b00111100,0b00011111,0b00000111,0],4:[0,0b00000011,0b00001111,0b00111110,0b11110000,0b11000000,0b10000000,0],3:[0b11110000,0b11111110,0b10011111,0b00001111,0b00000001,0,0,0],2:[0b00001111,0b00011101,0b01110001,0b11111111,0b11111110,0b11110000,0,0],6:[0b11111111,0b01111110,0b00111100,0b00011000,0,0,0,0]},
  {0:[0b11000000,0b01100000,0b00110000,0b00010000,0b00010000,0b00110000,0b01100000,0b11000000],1:[0b00000011,0b00000110,0b00001100,0b00001000,0b00001000,0b00001100,0b00000110,0b00000011],5:[0b00000000,0b00000000,0b00000000,0b00000001,0b00000011,0b00001111,0b00111100,0b11111000],4:[0b00000000,0b00000000,0b00000000,0b10000000,0b11110000,0b00111100,0b00001111,0b00000011],3:[0b00000000,0b00000000,0b00000000,0b00000111,0b00011111,0b01111000,0b11100000,0b10000000],2:[0b00111111,0b00011100,0b01110010,0b11110001,0b11001001,0b00100101,0b00010010,0b00001100],6:[0b11100111,0b01100110,0b00100100,0,0,0,0,0]},
  {0:[0b10000001,0b01000010,0b00100100,0b00011000,0b00011000,0b00100100,0b01000010,0b10000001],1:[0b10000001,0b01000010,0b00100100,0b00011000,0b00011000,0b00100100,0b01000010,0b10000001],5:[0b10000000,0b11000000,0b11110000,0b01111000,0b00111100,0b00011111,0b00000111,0],4:[0,0b00000011,0b00001111,0b00111110,0b11110000,0b11000000,0b10000000,0],3:[0b11110000,0b11111110,0b10011111,0b00001111,0b00000001,0,0,0],2:[0b00001111,0b00011101,0b01110001,0b11111111,0b11111110,0b11110000,0,0],6:[0b11111111,0b01111110,0b00111100,0b00011000,0,0,0,0]},
  {0:[0b00011000,0b00111100,0b01111110,0b11111111,0b01111110,0b00111100,0b00011000,0],1:[0b00011000,0b00111100,0b01111110,0b11111111,0b01111110,0b00111100,0b00011000,0],5:[0,0,0,0b11111111,0b11111110,0b11100000,0,0],4:[0,0,0,0b11111111,0b00111111,0b00000111,0,0],3:[0,0,0,0b11111111,0b11111110,0b11100000,0,0],2:[0,0,0,0b11111111,0b00111111,0b00000111,0,0],6:[0b11111111,0b01111110,0b00111100,0b00011000,0,0,0,0]}
];
function loadExprToFb(idx){const e=EXPRS[idx];for(let p=0;p<7;p++){const rows=e[p]||[0,0,0,0,0,0,0,0];for(let r=0;r<8;r++) fb[p][r]=rows[r]||0;}}
const cv=document.getElementById('cv');
const ctx=cv.getContext('2d');
const PX=5,G=1;
cv.width=7*8*(PX+G);cv.height=8*(PX+G);
let gameFb=Array.from({length:7},()=>new Uint8Array(8));
let mirrorMode=false;
function drawPreview(){
  const src=mirrorMode?gameFb:fb;
  ctx.fillStyle='#060810';ctx.fillRect(0,0,cv.width,cv.height);
  PREVIEW_ORDER.forEach((panel,pi)=>{
    for(let r=0;r<8;r++){
      const byte=src[panel][r]||0;
      for(let bit=0;bit<8;bit++){
        const on=(byte>>(7-bit))&1;
        ctx.fillStyle=on?'#0088ff':'#001e33';
        ctx.beginPath();
        ctx.arc((pi*8+bit)*(PX+G)+PX/2,r*(PX+G)+PX/2,PX/2-.3,0,Math.PI*2);
        ctx.fill();
      }
    }
  });
}
let curPanel=0,drawTool='draw',isMouseDown=false;
function buildGrid(){
  const grid=document.getElementById('drawGrid');grid.innerHTML='';
  for(let r=0;r<8;r++) for(let c=0;c<8;c++){
    const d=document.createElement('div');d.className='px';
    d.dataset.r=r;d.dataset.c=c;
    const on=(fb[curPanel][r]>>(7-c))&1;if(on) d.classList.add('on');
    d.addEventListener('pointerdown',e=>{isMouseDown=true;paintCell(d);e.preventDefault();});
    d.addEventListener('pointerenter',e=>{if(isMouseDown) paintCell(d);});
    grid.appendChild(d);
  }
}
document.addEventListener('pointerup',()=>isMouseDown=false);
function paintCell(d){
  const r=+d.dataset.r,c=+d.dataset.c;
  const val=drawTool==='erase'?false:true;
  if(val) fb[curPanel][r]|=(1<<(7-c));
  else fb[curPanel][r]&=~(1<<(7-c));
  d.classList.toggle('on',val);drawPreview();
}
function refreshGrid(){
  document.querySelectorAll('#drawGrid .px').forEach(d=>{
    const r=+d.dataset.r,c=+d.dataset.c;
    d.classList.toggle('on',!!((fb[curPanel][r]>>(7-c))&1));
  });drawPreview();
}
function selPanel(p){curPanel=p;document.querySelectorAll('.pb').forEach((b,i)=>b.classList.toggle('on',i===p));refreshGrid();}
function setTool(t){drawTool=t;document.getElementById('toolDraw').classList.toggle('on',t==='draw');document.getElementById('toolErase').classList.toggle('on',t==='erase');}
function fillPanel(on){for(let r=0;r<8;r++) fb[curPanel][r]=on?0xFF:0x00;refreshGrid();}
function invertPanel(){for(let r=0;r<8;r++) fb[curPanel][r]^=0xFF;refreshGrid();}
function pushPanel(){let hex='';for(let r=0;r<8;r++) hex+=fb[curPanel][r].toString(16).padStart(2,'0');api('/api/draw?panel='+curPanel+'&data='+hex);}
function pushAll(){let hex='';for(let p=0;p<7;p++) for(let r=0;r<8;r++) hex+=fb[p][r].toString(16).padStart(2,'0');api('/api/drawAll?data='+hex);}
function resetDraw(){loadExprToFb(cur);refreshGrid();api('/api/expr?id='+cur);}
let cur=0;
function setE(id){cur=id;document.querySelectorAll('.eb').forEach((b,i)=>b.classList.toggle('on',i===id));loadExprToFb(id);refreshGrid();api('/api/expr?id='+id);}
function randE(){setE(Math.floor(Math.random()*8));}
function V(id,v){document.getElementById(id).textContent=v;}
function api(url){fetch(url).catch(()=>{});}
let gameRunning=false,gamePoll=null,gameDiff=1;
let lastScoreSeen=0,lastMilestoneSeen=0,wasOver=false,wasPaused=false,bestAtStart=0;
function vibrate(ms){ if(navigator.vibrate) navigator.vibrate(ms); }
const DIFF_NAMES=['EASY','NORMAL','HARD'];
function startGame(diff){
  gameDiff=diff;
  fetch('/api/game/start?diff='+diff).then(()=>{
    gameRunning=true;wasOver=false;wasPaused=false;lastScoreSeen=0;lastMilestoneSeen=0;
    bestAtStart=parseInt(document.getElementById('hiPreview').textContent)||0;
    mirrorMode=true;
    document.getElementById('pvTitle').innerHTML='&#128006; LIVE \u2014 '+DIFF_NAMES[diff]+' MODE';
    document.querySelectorAll('.diffbn').forEach((b,i)=>b.classList.toggle('on',i===diff));
    document.querySelectorAll('.cfg').forEach(c=>c.style.display='none');
    document.getElementById('gameCard').style.display='block';
    document.getElementById('gameOverBanner').style.display='none';
    document.getElementById('pauseBanner').style.display='none';
    if(gamePoll) clearInterval(gamePoll);
    gamePoll=setInterval(pollGame,150);
    pollGame();
  });
}
function exitGame(){
  fetch('/api/game/exit').then(()=>{
    gameRunning=false;clearInterval(gamePoll);mirrorMode=false;
    document.getElementById('pvTitle').textContent='Live Preview';
    document.getElementById('gameOverBanner').style.display='none';
    document.getElementById('pauseBanner').style.display='none';
    document.querySelectorAll('.cfg').forEach(c=>c.style.display='block');
    document.getElementById('gameCard').style.display='none';
    loadExprToFb(cur);refreshGrid();
  });
}
function togglePause(){ if(gameRunning) fetch('/api/game/pause'); }
function doJump(){
  if(!gameRunning) return;
  fetch('/api/game/jump');
  vibrate(12);
  const b=document.getElementById('jumpBtn');
  b.style.transform='scale(.96)';setTimeout(()=>b.style.transform='',90);
}
function doDuckOn(){ if(gameRunning) fetch('/api/game/duck?v=1'); }
function doDuckOff(){ if(gameRunning) fetch('/api/game/duck?v=0'); }
function pollGame(){
  fetch('/api/game/status').then(r=>r.json()).then(d=>{
    document.getElementById('gScore').textContent=d.score;
    document.getElementById('gHi').textContent=d.hi;
    document.getElementById('hiPreview').textContent=d.hi;

    // unpack the live LED framebuffer into the mirror canvas
    for(let p=0;p<7;p++) for(let r=0;r<8;r++){
      const idx=(p*8+r)*2;
      gameFb[p][r]=parseInt(d.fb.substr(idx,2),16)||0;
    }
    drawPreview();

    // haptic pulse the instant a new 50pt milestone is crossed
    if(d.score>lastMilestoneSeen && d.score%50===0 && d.score>0){ lastMilestoneSeen=d.score; vibrate(35); }
    lastScoreSeen=d.score;

    const pb=document.getElementById('pauseBanner'),ob=document.getElementById('gameOverBanner');
    const jb=document.getElementById('jumpBtn'),pausebn=document.getElementById('pauseBtn');
    pausebn.classList.toggle('on',d.paused);

    if(d.over){
      if(!wasOver){ wasOver=true; vibrate([40,30,60]); }
      const isNewBest=d.score>0 && d.score>=d.hi && d.score>bestAtStart;
      document.getElementById('goTitle').textContent=isNewBest?'\u2605 NEW BEST! \u2605':'GAME OVER';
      document.getElementById('goScore').textContent='Score '+d.score+'  \u00b7  Best '+d.hi;
      ob.style.display='block';pb.style.display='none';
      jb.innerHTML='&#9650; TAP TO RESTART';
    } else if(d.paused){
      wasOver=false;
      ob.style.display='none';pb.style.display='block';
      jb.innerHTML='&#9650; TAP TO RESUME';
    } else {
      wasOver=false;
      ob.style.display='none';pb.style.display='none';
      jb.innerHTML='&#9650; JUMP';
    }
  }).catch(()=>{});
}
document.addEventListener('keydown',e=>{
  if(!gameRunning) return;
  if(e.code==='Space'||e.code==='ArrowUp'){ e.preventDefault(); doJump(); }
  else if(e.code==='ArrowDown'){ if(!e.repeat) doDuckOn(); e.preventDefault(); }
  else if(e.code==='KeyP'){ togglePause(); }
  else if(e.code==='Escape'){ exitGame(); }
});
document.addEventListener('keyup',e=>{
  if(!gameRunning) return;
  if(e.code==='ArrowDown') doDuckOff();
});
loadExprToFb(0);buildGrid();drawPreview();
fetch('/api/status').then(r=>r.json()).then(d=>{
  document.getElementById('blinkOn').checked=d.blinkEnabled;
  document.getElementById('blinkMs').value=d.blinkInterval;V('blinkV',(d.blinkInterval/1000).toFixed(1)+'s');
  document.getElementById('bri').value=d.brightness;V('briV',d.brightness);
  document.getElementById('autoOn').checked=d.autoAnimate;
  document.getElementById('autoMs').value=d.autoInterval;V('autoV',(d.autoInterval/1000)+'s');
  document.getElementById('ipLbl').textContent=d.ip;
  document.getElementById('hiPreview').textContent=d.highScore||0;
  cur=d.currentExpr;
  document.querySelectorAll('.eb').forEach((b,i)=>b.classList.toggle('on',i===cur));
  loadExprToFb(cur);refreshGrid();
}).catch(()=>{});
</script>
</body>
</html>
)rawhtml";

void handleGameStart() {
  if (server.hasArg("diff"))
    difficulty = constrain(server.arg("diff").toInt(), 0, 2);
  gameActive = true;
  gameInit();
  server.send(200, "text/plain", "OK");
}
void handleGameJump() {
  if (gameOver) {
    gameInit();
  } else if (gamePaused) {
    gamePaused = false;
    lastFrame  = millis();
  } else {
    dinoJump();
  }
  server.send(200, "text/plain", "OK");
}
void handleGamePause() {
  if (gameActive && !gameOver) {
    gamePaused = !gamePaused;
    if (!gamePaused) lastFrame = millis();
  }
  server.send(200, "text/plain", "OK");
}
void handleGameDuck() {
  dinoDucking = server.hasArg("v") ? server.arg("v").toInt() : true;
  server.send(200, "text/plain", "OK");
}
void handleGameExit() {
  gameActive = false;
  gameOver   = false;
  gamePaused = false;
  drawExpression(currentExpr);
  server.send(200, "text/plain", "OK");
}
void handleGameStatus() {
  String j = "{";
  j += "\"score\":"  + String(score) + ",";
  j += "\"hi\":"     + String(highScore) + ",";
  j += "\"over\":"   + String(gameOver   ? "true" : "false") + ",";
  j += "\"paused\":" + String(gamePaused ? "true" : "false") + ",";
  j += "\"diff\":"   + String(difficulty) + ",";
  j += "\"fb\":\""   + framebufToHex() + "\"";
  j += "}";
  server.send(200, "application/json", j);
}
void handleRoot()       { server.send(200, "text/html", HTML_PAGE); }
void handleExpr() {
  if (server.hasArg("id")) {
    currentExpr = constrain(server.arg("id").toInt(), 0, EXPR_COUNT-1);
    drawExpression(currentExpr);
  }
  server.send(200, "text/plain", "OK");
}
void handleBlink() {
  if (server.hasArg("enabled"))  blinkEnabled  = server.arg("enabled").toInt();
  if (server.hasArg("interval")) blinkInterval = constrain(server.arg("interval").toInt(), 200, 30000);
  if (server.hasArg("trigger"))  { isBlinking = true; blinkStart = millis(); drawBlink(); }
  server.send(200, "text/plain", "OK");
}
void handleBrightness() {
  if (server.hasArg("val")) {
    brightness = constrain(server.arg("val").toInt(), 0, 15);
    maxSetIntensity(brightness);
  }
  server.send(200, "text/plain", "OK");
}
void handleAuto() {
  if (server.hasArg("enabled"))  autoAnimate  = server.arg("enabled").toInt();
  if (server.hasArg("interval")) autoInterval = constrain(server.arg("interval").toInt(), 1000, 60000);
  server.send(200, "text/plain", "OK");
}
void handleClear() { fbClear(); server.send(200, "text/plain", "OK"); }
void handleReset() { currentExpr = 0; drawExpression(0); server.send(200, "text/plain", "OK"); }
void handleDraw() {
  customDrawActive = true;
  if (server.hasArg("panel") && server.hasArg("data")) {
    int panel = constrain(server.arg("panel").toInt(), 0, NUM_PANELS-1);
    String hex = server.arg("data");
    for (int r = 0; r < 8 && r*2+1 < (int)hex.length(); r++)
      framebuf[panel][r] = (uint8_t)strtol(hex.substring(r*2, r*2+2).c_str(), nullptr, 16);
    fbFlush();
  }
  server.send(200, "text/plain", "OK");
}
void handleDrawAll() {
  customDrawActive = true;
  if (server.hasArg("data")) {
    String hex = server.arg("data");
    for (int p = 0; p < NUM_PANELS; p++)
      for (int r = 0; r < 8; r++) {
        int idx = (p*8+r)*2;
        if (idx+1 < (int)hex.length())
          framebuf[p][r] = (uint8_t)strtol(hex.substring(idx, idx+2).c_str(), nullptr, 16);
      }
    fbFlush();
  }
  server.send(200, "text/plain", "OK");
}
void handleStatus() {
  String j = "{";
  j += "\"currentExpr\":"   + String(currentExpr) + ",";
  j += "\"blinkEnabled\":"  + String(blinkEnabled  ? "true" : "false") + ",";
  j += "\"blinkInterval\":" + String(blinkInterval) + ",";
  j += "\"brightness\":"    + String(brightness) + ",";
  j += "\"autoAnimate\":"   + String(autoAnimate  ? "true" : "false") + ",";
  j += "\"autoInterval\":"  + String(autoInterval) + ",";
  j += "\"highScore\":"     + String(highScore) + ",";
  j += "\"ip\":\""          + WiFi.softAPIP().toString() + "\"";
  j += "}";
  server.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== PROTOGEN FACE BOOT ===");
  prefs.begin("dino", false);
  highScore = prefs.getUInt("hi", 0);
  maxInit();
  bootAnimation();
  drawExpression(currentExpr);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  server.on("/",                handleRoot);
  server.on("/api/expr",        handleExpr);
  server.on("/api/blink",       handleBlink);
  server.on("/api/brightness",  handleBrightness);
  server.on("/api/auto",        handleAuto);
  server.on("/api/clear",       handleClear);
  server.on("/api/reset",       handleReset);
  server.on("/api/draw",        handleDraw);
  server.on("/api/drawAll",     handleDrawAll);
  server.on("/api/status",      handleStatus);
  server.on("/api/game/start",  handleGameStart);
  server.on("/api/game/jump",   handleGameJump);
  server.on("/api/game/pause",  handleGamePause);
  server.on("/api/game/duck",   handleGameDuck);
  server.on("/api/game/exit",   handleGameExit);
  server.on("/api/game/status", handleGameStatus);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.println("WiFi: " + String(AP_SSID) + " / " + String(AP_PASSWORD));
  Serial.println("UI:   http://192.168.4.1");
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  if (gameActive) {
    if (gamePaused) {
    } else if (!gameOver) {
      gameFrame();
    } else if (now - deathTime < 400 && (now / 100) % 2 == 0) {
      drawDinoEye();
    }
    return;
  }

  if (!isBlinking && blinkEnabled) {
    if (now - lastBlink >= (unsigned long)blinkInterval) {
      isBlinking = true;
      blinkStart = now;
      drawBlink();
    }
  }

  if (isBlinking) updateBlinkAnim();

  if (autoAnimate && !isBlinking) {
    if (now - lastAutoSwitch >= (unsigned long)autoInterval) {
      lastAutoSwitch = now;
      currentExpr = (currentExpr + 1) % EXPR_COUNT;
      drawExpression(currentExpr);
    }
  }
}