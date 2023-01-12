#pragma once

#define ABG_TIMER1
#define ABG_SYNC_PARK_ROW
#define ABG_UPDATE_EVERY_N_MOD 11
#define ABG_UPDATE_EVERY_N_DENOM_MOD 7
#define ABG_UPDATE_EVERY_N_DEFAULT 1
#define ABG_UPDATE_EVERY_N_DENOM_DEFAULT 1
#define ABG_PRECHARGE_CYCLES 1
#define ABG_DISCHARGE_CYCLES 2
#define ABG_FPS_DEFAULT 156
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
