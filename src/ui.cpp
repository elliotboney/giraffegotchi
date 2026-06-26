#include "ui.h"

// Palette
#define C_BODY_HAPPY  TFT_YELLOW
#define C_BODY_HUNGRY 0xC5A0   // dull olive-yellow (looks queasy)
#define C_SPOT        0xA340   // brown
#define C_LEG         0xB320   // darker tan
#define C_HORN        0x6300   // dark brown

const Rect FEED_BTN = {110, 196, 100, 38};

// Region the giraffe occupies — cleared on every redraw. Sits in the band
// between the top hunger bar (y<=29) and the bottom feed button (y>=196),
// so nothing the giraffe draws collides with either.
static const int GX = 40, GY = 34, GW = 192, GH = 160;  // y 34..194

void drawGiraffe(TFT_eSPI& tft, Mood mood) {
  const uint16_t body = (mood == Mood::Hungry) ? C_BODY_HUNGRY : C_BODY_HAPPY;

  tft.fillRect(GX, GY, GW, GH, BG_COLOR);  // clear previous frame

  // Tail
  tft.drawLine(55, 158, 42, 184, C_LEG);
  tft.drawLine(42, 184, 48, 188, C_LEG);

  // Legs
  tft.fillRect(70, 176, 12, 18, C_LEG);
  tft.fillRect(96, 176, 12, 18, C_LEG);
  tft.fillRect(126, 176, 12, 18, C_LEG);
  tft.fillRect(150, 176, 12, 18, C_LEG);

  // Body
  tft.fillRoundRect(55, 128, 115, 48, 18, body);

  // Neck + head
  tft.fillRect(146, 78, 26, 52, body);
  tft.fillRoundRect(150, 50, 52, 32, 10, body);

  // Ear
  tft.fillTriangle(150, 56, 142, 50, 152, 68, body);

  // Ossicones (horns)
  tft.drawLine(166, 50, 164, 40, C_HORN);
  tft.fillCircle(164, 39, 4, C_HORN);
  tft.drawLine(190, 50, 192, 40, C_HORN);
  tft.fillCircle(192, 39, 4, C_HORN);

  // Spots
  tft.fillCircle(80, 146, 7, C_SPOT);
  tft.fillCircle(108, 158, 6, C_SPOT);
  tft.fillCircle(135, 142, 8, C_SPOT);
  tft.fillCircle(156, 108, 5, C_SPOT);

  // Eye + mouth vary by mood
  if (mood == Mood::Happy) {
    tft.fillCircle(186, 63, 5, TFT_WHITE);
    tft.fillCircle(187, 64, 2, TFT_BLACK);
    // smile
    tft.drawLine(172, 76, 180, 80, TFT_BLACK);
    tft.drawLine(180, 80, 190, 76, TFT_BLACK);
  } else {
    // droopy closed eye
    tft.drawLine(181, 64, 191, 66, TFT_BLACK);
    // frown
    tft.drawLine(172, 80, 180, 76, TFT_BLACK);
    tft.drawLine(180, 76, 190, 80, TFT_BLACK);
  }
}

void drawHungerBar(TFT_eSPI& tft, uint8_t hunger) {
  const int x = 10, y = 10, w = 200, h = 18;
  tft.drawRect(x - 1, y - 1, w + 2, h + 2, TFT_WHITE);
  const int fill = map(hunger, 0, 100, 0, w);
  const uint16_t col = (hunger < Pet::HUNGRY_THRESHOLD) ? TFT_RED : TFT_GREEN;
  tft.fillRect(x, y, fill, h, col);
  tft.fillRect(x + fill, y, w - fill, h, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, BG_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Hunger", x + w + 8, y + 2, 2);
}

void drawFeedButton(TFT_eSPI& tft) {
  tft.fillRoundRect(FEED_BTN.x, FEED_BTN.y, FEED_BTN.w, FEED_BTN.h, 8, TFT_DARKGREEN);
  tft.drawRoundRect(FEED_BTN.x, FEED_BTN.y, FEED_BTN.w, FEED_BTN.h, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("FEED", FEED_BTN.x + FEED_BTN.w / 2, FEED_BTN.y + FEED_BTN.h / 2, 4);
}
