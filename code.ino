#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// =======================
// DISPLAY CONFIG
// =======================

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;
public:
  LGFX(void) {
    auto cfg = _bus.config();
    cfg.spi_host   = SPI2_HOST;
    cfg.spi_mode   = 0;
    cfg.freq_write = 40000000;
    cfg.pin_sclk   = 6;
    cfg.pin_mosi   = 7;
    cfg.pin_miso   = -1;
    cfg.pin_dc     = 2;
    _bus.config(cfg);
    _panel.setBus(&_bus);
    auto pcfg = _panel.config();
    pcfg.pin_cs       = 10;
    pcfg.pin_rst      = 3;
    pcfg.pin_busy     = -1;
    pcfg.panel_width  = 240;
    pcfg.panel_height = 240;
    _panel.config(pcfg);
    auto lcfg = _light.config();
    lcfg.pin_bl = -1;
    _light.config(lcfg);
    _panel.setLight(&_light);
    setPanel(&_panel);
  }
};

LGFX display;

// =======================
// COLORS (invert: black background, white eyes)
// =======================
#define BG_COLOR  TFT_BLACK
#define FG_COLOR  TFT_WHITE

// =======================
// GEOMETRY
// =======================
#define L_EYE_X     75
#define R_EYE_X    165
#define EYE_BASE_Y  110
#define EYE_W        46
#define EYE_H        46
#define EYE_R         8

#define LOOK_MAX_X   10
#define LOOK_MAX_Y    6

// Sprite covers the eye + entire range of motion + 2px margin
#define SPR_W  (EYE_W + LOOK_MAX_X * 2 + 4)   // 70px
#define SPR_H  (EYE_H + LOOK_MAX_Y * 2 + 4)   // 62px

// Position of the top-left corner of each sprite on the screen (fixed!)
#define L_SPR_X  (L_EYE_X - SPR_W / 2)
#define R_SPR_X  (R_EYE_X - SPR_W / 2)
#define SPR_Y    (EYE_BASE_Y - SPR_H / 2)

// The center inside the sprite
#define SPR_CX   (SPR_W / 2)
#define SPR_CY   (SPR_H / 2)

// Mouth
#define MOUTH_CX  120
#define MOUTH_CY  178
#define MOUTH_R    22
#define MOUTH_T     8

// =======================
// SPRITE - one buffer, used for the L and R eye in turn
// =======================
lgfx::LGFX_Sprite eyeSprite(&display);

// =======================
// CONDITION
// =======================
int  expression   = 0;
unsigned long lastExprChange = 0;
unsigned long exprDuration   = 9000;

unsigned long blinkTimer  = 0;

int  lookX = 0, lookY = 0;
int  targetLookX = 0, targetLookY = 0;
int  prevLookX = 9999, prevLookY = 9999;
unsigned long nextSaccade   = 0;
unsigned long lookStepTimer = 0;
#define LOOK_STEP_MS  55

bool eyeRolling = false;
int  rollStep   = 0;
unsigned long rollTimer = 0;
#define ROLL_STEP_MS  85

const int ROLL_N = 8;
const int ROLL_X[ROLL_N] = {  0,  7, 10,  7,  0, -7,-10, -7 };
const int ROLL_Y[ROLL_N] = { -6, -4,  0,  4,  6,  4,  0, -4 };

void renderEyeToSprite(int id, int ox, int oy) {
  eyeSprite.fillSprite(BG_COLOR);

  int cx = SPR_CX + ox;
  int cy = SPR_CY + oy;

  switch (id) {

    default:
    case 0: // normal
    case 1: // happy
    case 3: // sad
      eyeSprite.fillRoundRect(cx - EYE_W/2, cy - EYE_H/2, EYE_W, EYE_H, EYE_R, FG_COLOR);
      break;

    case 2:
      eyeSprite.fillRoundRect(cx - (EYE_W+8)/2, cy - (EYE_H+8)/2,
                               EYE_W+8, EYE_H+8, EYE_R, FG_COLOR);
      break;

    case 4:
      eyeSprite.fillRoundRect(cx - EYE_W/2, cy - 3, EYE_W, 6, 3, FG_COLOR);
      break;

    case 5: {
      eyeSprite.fillRoundRect(cx - EYE_W/2, cy - EYE_H/2, EYE_W, EYE_H, EYE_R, FG_COLOR);
      int browY = SPR_CY - EYE_H/2 - 8;
      for (int i = -2; i <= 2; i++)
        eyeSprite.drawLine(SPR_CX - EYE_W/2, browY - 7 + i,
                            SPR_CX + EYE_W/2, browY     + i, FG_COLOR);
      break;
    }

    case 6:
      eyeSprite.fillRoundRect(cx - EYE_W/2, cy, EYE_W, EYE_H/2, EYE_R, FG_COLOR);
      break;

    case 7: {
      eyeSprite.fillRoundRect(cx - EYE_W/2, cy - EYE_H/2, EYE_W, EYE_H, EYE_R, FG_COLOR);
      eyeSprite.fillCircle(cx - 7, cy - 6, 7, BG_COLOR);
      eyeSprite.fillCircle(cx + 7, cy - 6, 7, BG_COLOR);
      eyeSprite.fillTriangle(cx - 14, cy - 3, cx + 14, cy - 3, cx, cy + 11, BG_COLOR);
      break;
    }
  }
}

// Renders sprite and pushes for both eyes - one SPI call per eye
void pushEyes(int id, int ox, int oy) {
  renderEyeToSprite(id, ox, oy);
  eyeSprite.pushSprite(L_SPR_X, SPR_Y);
  eyeSprite.pushSprite(R_SPR_X, SPR_Y);
}

// =======================
// LIPS - drawn only when changing emotions
// =======================

void drawMouth(int id) {
  // Clear entire mouth area (solid rectangle, no artifacts)
  display.fillRect(MOUTH_CX - MOUTH_R - 2, MOUTH_CY - MOUTH_R - 2,
                   (MOUTH_R + 2) * 2, (MOUTH_R + 2) * 2, BG_COLOR);

  switch (id) {
    case 1: // smile
      display.fillArc(MOUTH_CX, MOUTH_CY - 6, MOUTH_R, MOUTH_R - MOUTH_T, 200, 340, FG_COLOR);
      break;
    case 2: // surprised "O"
      display.fillCircle(MOUTH_CX, MOUTH_CY, MOUTH_T + 2, FG_COLOR);
      break;
    case 3: // sad
      display.fillArc(MOUTH_CX, MOUTH_CY + 6, MOUTH_R, MOUTH_R - MOUTH_T, 20, 160, FG_COLOR);
      break;
    default: // neutral
      display.fillRoundRect(MOUTH_CX - MOUTH_R, MOUTH_CY - 3, MOUTH_R * 2, 6, 3, FG_COLOR);
      break;
  }
}

int mouthFor(int id) {
  switch (id) {
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    default: return 0;
  }
}

// =======================
// FULL RENDER (only when changing emotions)
// =======================

void renderFullFace(int id) {
  display.fillScreen(BG_COLOR);
  drawMouth(mouthFor(id));
  pushEyes(id, 0, 0);
  lookX = lookY = 0;
  prevLookX = prevLookY = 0;
}

// =======================
// BLINK
// =======================

void doBlink() {
  eyeSprite.fillSprite(BG_COLOR);
  eyeSprite.fillRoundRect(SPR_CX - EYE_W/2, SPR_CY - 3, EYE_W, 6, 3, FG_COLOR);
  eyeSprite.pushSprite(L_SPR_X, SPR_Y);
  eyeSprite.pushSprite(R_SPR_X, SPR_Y);
  delay(100);
  pushEyes(expression, lookX, lookY);
}

// =======================
// EYE ROLLING
// =======================

void startEyeRoll() {
  eyeRolling = true;
  rollStep   = 0;
  rollTimer  = millis() - ROLL_STEP_MS;
}

void updateEyeRoll() {
  if (!eyeRolling) return;
  if (millis() - rollTimer < ROLL_STEP_MS) return;
  rollTimer = millis();

  if (rollStep < ROLL_N) {
    pushEyes(expression, ROLL_X[rollStep], ROLL_Y[rollStep]);
    rollStep++;
  } else {
    lookX = targetLookX = 0;
    lookY = targetLookY = 0;
    prevLookX = prevLookY = 0;
    eyeRolling = false;
    pushEyes(expression, 0, 0);
    blinkTimer = millis();
  }
}

// =======================
// LOOK AROUND
// =======================

void updateLook() {
  if (eyeRolling) return;

  if (millis() > nextSaccade) {
    targetLookX = random(-LOOK_MAX_X, LOOK_MAX_X + 1);
    targetLookY = random(-LOOK_MAX_Y, LOOK_MAX_Y + 1);
    nextSaccade  = millis() + random(1000, 3200);
  }

  if (millis() - lookStepTimer < LOOK_STEP_MS) return;
  lookStepTimer = millis();

  if (lookX < targetLookX) lookX++;
  else if (lookX > targetLookX) lookX--;
  if (lookY < targetLookY) lookY++;
  else if (lookY > targetLookY) lookY--;

  if (lookX != prevLookX || lookY != prevLookY) {
    pushEyes(expression, lookX, lookY);
    prevLookX = lookX;
    prevLookY = lookY;
  }
}

// =======================
// SETUP
// =======================

void setup() {
  display.invertDisplay(true);
  display.init();
  display.setRotation(0);
  randomSeed(analogRead(0) ^ millis());

  eyeSprite.setColorDepth(16);
  eyeSprite.createSprite(SPR_W, SPR_H);

  renderFullFace(0);
  blinkTimer     = millis() + 2500;
  nextSaccade    = millis() + 1500;
  lastExprChange = millis();
  lookStepTimer  = millis();
}

// =======================
// LOOP
// =======================

void loop() {
  unsigned long now = millis();

  if (eyeRolling) {
    updateEyeRoll();
    return;
  }

  // Change mood after 9–13 s
  if (now - lastExprChange > exprDuration) {
    int prev = expression;
    do { expression = random(0, 8); } while (expression == prev);
    exprDuration   = random(9000, 13000);
    lastExprChange = now;

    if (random(0, 5) == 0) {
      expression = prev;
      startEyeRoll();
    } else {
      renderFullFace(expression);
    }
  }

  // Blink after 2–5 s
  if (now - blinkTimer > (unsigned long)random(2000, 5000)) {
    doBlink();
    blinkTimer = millis();
  }

  // Look around
  updateLook();
}
