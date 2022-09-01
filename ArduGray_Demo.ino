#define ARDUGRAY_IMPLEMENTATION
#include "ArduGray.h"

ArduGray a;

int16_t x, y;

void update()
{
    // handle input and update game state
    if(a.pressed(UP_BUTTON))    --y;
    if(a.pressed(DOWN_BUTTON))  ++y;
    if(a.pressed(LEFT_BUTTON))  --x;
    if(a.pressed(RIGHT_BUTTON)) ++x;
}

void render()
{    
    a.setCursor(20, 28);
    a.setTextColor(WHITE);
    a.print(F("Hello "));
    a.setTextColor(DARK_GRAY);
    a.print(F("ArduGray!"));
    
    a.fillRect(x +  0, y, 5, 15, WHITE);
    a.fillRect(x +  5, y, 5, 15, LIGHT_GRAY);
    a.fillRect(x + 10, y, 5, 15, DARK_GRAY);
}

void setup()
{
    a.begin();
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
