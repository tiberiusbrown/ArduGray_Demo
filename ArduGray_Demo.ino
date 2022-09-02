#define ABG_IMPLEMENTATION
#include "ArduboyG.h"

ArduboyG a;

int16_t x, y;

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
    a.setCursor(20, 28);
    a.setTextColor(WHITE);
    a.print(F("Hello "));
    a.setTextColor(DARK_GRAY);
    a.print(F("ArduboyG!"));
    a.fillRect(x +  0, y, 5, 15, WHITE);
    a.fillRect(x +  5, y, 5, 15, LIGHT_GRAY);
    a.fillRect(x + 10, y, 5, 15, DARK_GRAY);
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
