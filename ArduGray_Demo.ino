#define ABG_IMPLEMENTATION
#include "ArduboyG.h"

ArduboyG a;

int16_t x, y;

static uint8_t const SPRITES[] PROGMEM =
{
    8, 8,
    
    0xaa, 0xae, 0xa0, 0xae, 0xea, 0x0a, 0xea, 0xaa, // plane 0
    0x66, 0x60, 0x60, 0x66, 0x06, 0x06, 0x66, 0x66, // plane 1
};

void update()
{
    // Handle input and update game state here.
    if(a.pressed(UP_BUTTON))    --y;
    if(a.pressed(DOWN_BUTTON))  ++y;
    if(a.pressed(LEFT_BUTTON))  --x;
    if(a.pressed(RIGHT_BUTTON)) ++x;
}

void render()
{
    // Draw your game graphics here.

    uint8_t ox = x & 7;
    uint8_t oy = y & 7;
    for(int16_t sy = 0; sy < 72; sy += 8)
        for(int16_t sx = 0; sx < 136; sx += 8)
            a.drawOverwrite(sx - ox, sy - oy, SPRITES, 0);

    a.setCursor(20, 28);
    a.setTextColor(WHITE);
    a.print(F("Hello "));
    a.setTextColor(DARK_GRAY);
    a.print(F("ArduboyG!"));
}

void setup()
{
    a.begin();
    
    // Initialize your game state here.
    
    // This method kicks off the frame ISR that handles refreshing
    // the screen. Usually you would call this at the end of setup().
    a.startGray();
}

void loop()
{
    if(!a.nextFrame())
        return;
    if(a.needsUpdate())
        update();
    render();
}
