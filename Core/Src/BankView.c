#include "BankView.h"
#include <stdio.h>

static View view;
static int pig_offset = 0;
static bool pig_count_up = true;
static int32_t current_balance = 0;

static void render() {
  gCoord swidth, sheight;
  swidth = gdispGetWidth();
  sheight = gdispGetHeight();
  gdispClear(Black);
  if (pig_count_up) {
    pig_offset++;
    if (pig_offset > 35) {
      pig_count_up = false;
    }
  } else {
    pig_offset--;
    if (pig_offset < -20) {
      pig_count_up = true;
    }
  }
  // ALEX's BANK ACCOUNT
  font_t DejaVuSans16 = gdispOpenFont("DejaVuSans16");
  char title_string[] = "Alex's Bank Account";
  int title_width = gdispGetStringWidth(title_string, DejaVuSans16) + 1;
  int title_height = gdispGetFontMetric(DejaVuSans16, fontHeight) + 1;
  gdispDrawStringBox((swidth / 2) - (title_width / 2), 10, title_width, title_height, title_string, DejaVuSans16, White,
                     justifyCenter);

  // PIG
  gdispImage piggy_img;
  gdispImageOpenFile(&piggy_img, "piggy_96_96.bmp");
  gdispImageDraw(&piggy_img, (((swidth / 2) - 35) - pig_offset), 30, 96, 96, 0, 0);
  gdispImageClose(&piggy_img);

  //"ROAD"
  gdispFillArea(0, 125, swidth, 6, White);

  // BALANCE
  font_t DejaVuSans32 = gdispOpenFont("DejaVuSans32");
  char money_string[16] = {'\0'};
  snprintf(money_string, sizeof(money_string), "$%ld", (long)current_balance);
  int money_width = gdispGetStringWidth(money_string, DejaVuSans32) + 1;
  int money_height = gdispGetFontMetric(DejaVuSans32, fontHeight) + 1;
  gdispDrawStringBox((swidth / 2) - (money_width / 2), 96 + 37, money_width, money_height, money_string, DejaVuSans32,
                     White, justifyCenter);

  gdispGFlush(gdispGetDisplay(0));
}

static void set_balance(int32_t balance) {
  current_balance = balance;
}

static View* init() {
  view.render = render;
  current_balance = 0;
  return &view;
}

const struct bankview BankView = {
    .init = init,
    .set_balance = set_balance,
};
