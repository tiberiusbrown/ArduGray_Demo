#define ABG_IMPLEMENTATION
#include "common.hpp"

decltype(a) a;

void loop()
{
    a.waitForNextPlane();
    if(a.needsUpdate())
        update();
    render();
}

void setup()
{  
    a.boot();
    a.startGray();
}
