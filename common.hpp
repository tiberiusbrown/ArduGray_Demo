#pragma once

#define ABG_TIMER1
#define ABG_SYNC_PARK_ROW

#if defined(OLED_SH1106)
#define ABG_REFRESH_HZ 125
#else
#define ABG_REFRESH_HZ 156
#endif

#include "ArduboyG.h"
extern ArduboyGBase_Config<ABG_Mode::L4_Triplane> a;

#define SPRITESU_OVERWRITE
#define SPRITESU_PLUSMASK
#define SPRITESU_RECT
#include "SpritesU.hpp"

extern uint8_t ox;
extern uint8_t oy;

void update();
void render();
