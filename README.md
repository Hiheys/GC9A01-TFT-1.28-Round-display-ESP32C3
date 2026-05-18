# ESP32-C3 Super Mini + GC9A01 Round Display — Animated Face

A fun beginner project: an animated face with blinking eyes and changing expressions running on a 240×240 round TFT display.

![Animated face showing happy, sad, surprised, angry and winking expressions]

---

## Hardware Required

| Component | Details |
|---|---|
| ESP32-C3 Super Mini | HW-466AB or similar (built-in USB, no CH340) |
| Round TFT Display 1.28" | GC9A01 driver, 240×240 px |
| Jumper wires | 7× female-to-female |
| USB-C cable | **Must support data transfer** (not charging-only) |

> ⚠️ **Important:** Many USB-C cables are charge-only. If your ESP32 is not recognized by your computer, try a different cable first.

---

## Wiring

The display has 7 pins in this order (left to right): `VCC GND SCL SDA DC CS RES`

| Display Pin | ESP32-C3 Super Mini Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO6 |
| SDA | GPIO7 |
| DC | GPIO2 |
| CS | GPIO10 |
| RES | GPIO3 |

> ⚠️ Always use **3.3V**, never 5V — the GC9A01 is not 5V tolerant.

> ℹ️ This display has no separate backlight (BL) pin — it is always on.

---

## Software Setup

### 1. Install Arduino IDE 2

Download from [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)

### 2. Add ESP32 board support

Open Arduino IDE → **File → Preferences** and add this URL to "Additional boards manager URLs":

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Then go to **Tools → Board → Boards Manager**, search for `esp32` and install **esp32 by Espressif Systems**.

### 3. Install LovyanGFX library

Go to **Sketch → Include Library → Manage Libraries**, search for `LovyanGFX` and install it.

### 4. Select board and port

- **Tools → Board → ESP32 Arduino → ESP32C3 Dev Board**
- **Tools → USB CDC on Boot → Enabled**
- **Tools → Port → COM\_** (whichever COM port appears when ESP32 is connected)

---

## Windows Driver Troubleshooting

The ESP32-C3 Super Mini (HW-466AB) uses **built-in USB** — it does **not** use CH340 or CP210x chips. Do not install those drivers.

### If the device is not recognized (Code 43 error):

1. Open **PowerShell as Administrator** and run:

```powershell
Invoke-WebRequest 'https://dl.espressif.com/dl/idf-env/idf-env.exe' -OutFile .\idf-env.exe; .\idf-env.exe driver install --espressif
```

2. Restart your computer and reconnect the ESP32.

### If the device keeps connecting and disconnecting:

The board likely has a bad factory firmware that crashes in a loop. Fix it by flashing a blank sketch:

1. Enter bootloader mode: hold **BOOT** button, plug in USB, release **BOOT** after 2 seconds
2. In Arduino IDE, open **File → New Sketch** and upload this:

```cpp
void setup() {}
void loop() {}
```

3. After upload, unplug and replug — the device should now stay connected.

---

## Code

Upload the following sketch to your ESP32-C3:

```cpp
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;

public:
  LGFX(void) {
    auto cfg = _bus.config();
    cfg.spi_host  = SPI2_HOST;
    cfg.spi_mode  = 0;
    cfg.freq_write = 40000000;
    cfg.pin_sclk  = 6;
    cfg.pin_mosi  = 7;
    cfg.pin_miso  = -1;
    cfg.pin_dc    = 2;
    _bus.config(cfg);
    _panel.setBus(&_bus);

    auto pcfg = _panel.config();
    pcfg.pin_cs   = 10;
    pcfg.pin_rst  = 3;
    pcfg.pin_busy = -1;
    pcfg.panel_width  = 240;
    pcfg.panel_height = 240;
    _panel.config(pcfg);

    auto lcfg = _light.config();
    lcfg.pin_bl = -1;  // No backlight pin on this display
    _light.config(lcfg);
    _panel.setLight(&_light);

    setPanel(&_panel);
  }
};

LGFX display;

// ─── Mood definitions ──────────────────────────────────────────────────────

struct Mood {
  const char* name;
  uint32_t    faceColor;
  int         mouthType;   // 0=smile 1=sad 2=open circle 3=flat line
  bool        wink;        // true = left eye closed
};

const Mood moods[] = {
  { "Happy",     0x2ECC71, 0, false },
  { "Sad",       0x3498DB, 1, false },
  { "Surprised", 0xE67E22, 2, false },
  { "Wink",      0x9B59B6, 0, true  },
  { "Angry",     0xE74C3C, 3, false },
};
const int MOOD_COUNT = 5;

int  currentMood = 0;
bool globalBlink = false;

// ─── Drawing helpers ───────────────────────────────────────────────────────

void drawEye(int cx, int cy, bool closed) {
  if (closed) {
    display.drawLine(cx - 22, cy, cx + 22, cy, TFT_WHITE);
    display.drawLine(cx - 22, cy + 1, cx + 22, cy + 1, TFT_WHITE);
    display.drawLine(cx - 22, cy + 2, cx + 22, cy + 2, TFT_WHITE);
  } else {
    // Tall eye (tall ellipse)
    display.fillEllipse(cx, cy, 22, 38, TFT_WHITE);
    display.fillEllipse(cx, cy + 4, 12, 16, TFT_BLACK);
    display.fillEllipse(cx + 5, cy - 4, 4, 5, TFT_WHITE);
  }
}

void drawMouth(int cx, int cy, int type) {
  switch (type) {
    case 0:  // Smile
      for (int t = 0; t < 4; t++)
        display.drawArc(cx, cy + 18, 40 + t, 40 + t, 27, 153, TFT_WHITE);
      break;
    case 1:  // Sad
      for (int t = 0; t < 4; t++)
        display.drawArc(cx, cy + 65, 40 + t, 40 + t, 207, 333, TFT_WHITE);
      break;
    case 2:  // Open circle
      display.fillCircle(cx, cy + 30, 18, TFT_WHITE);
      display.fillCircle(cx, cy + 30, 10, 0x000000);
      break;
    case 3:  // Flat / angry
      display.drawLine(cx - 30, cy + 28, cx + 30, cy + 20, TFT_WHITE);
      display.drawLine(cx - 30, cy + 29, cx + 30, cy + 21, TFT_WHITE);
      display.drawLine(cx - 30, cy + 30, cx + 30, cy + 22, TFT_WHITE);
      break;
  }
}

void drawFace() {
  const Mood& m = moods[currentMood];
  const int cx = 120, cy = 120;

  display.fillCircle(cx, cy, 115, m.faceColor);

  bool leftClosed  = globalBlink || m.wink;
  bool rightClosed = globalBlink;

  drawEye(cx - 45, cy - 25, leftClosed);
  drawEye(cx + 45, cy - 25, rightClosed);
  drawMouth(cx, cy - 10, m.mouthType);
}

// ─── Main ──────────────────────────────────────────────────────────────────

void setup() {
  display.init();
  display.setRotation(0);
  display.fillScreen(TFT_BLACK);
  drawFace();
}

unsigned long lastMoodChange  = 0;
unsigned long lastBlink       = 0;
bool          blinkActive     = false;
unsigned long blinkStart      = 0;

void loop() {
  unsigned long now = millis();

  // Change mood every 3 seconds
  if (now - lastMoodChange > 3000) {
    lastMoodChange = now;
    currentMood = (currentMood + 1) % MOOD_COUNT;
    display.fillScreen(TFT_BLACK);
    drawFace();
  }

  // Random blink every ~4 seconds
  if (!blinkActive && now - lastBlink > 4000 + random(2000)) {
    blinkActive   = true;
    blinkStart    = now;
    globalBlink   = true;
    display.fillScreen(TFT_BLACK);
    drawFace();
  }

  // End blink after 150 ms
  if (blinkActive && now - blinkStart > 150) {
    blinkActive = false;
    globalBlink = false;
    lastBlink   = now;
    display.fillScreen(TFT_BLACK);
    drawFace();
  }
}
```

---

## How It Works

The sketch uses **LovyanGFX** to drive the GC9A01 over SPI. Every 3 seconds the face cycles through 5 moods:

Random eye blinks happen independently of mood changes.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| Black screen, backlight on | Check wiring — most likely SDA/SCL swapped |
| White screen | Wrong SPI mode or missing RST connection |
| Garbage / artifacts | Lower `freq_write` from 40000000 to 20000000 |
| Upload fails | Enter bootloader: hold BOOT, plug USB, release BOOT |
| Device not recognized | Try a different USB cable (data cable, not charge-only) |

---

## License

MIT — do whatever you want with it.
