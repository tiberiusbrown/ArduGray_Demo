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
    abg_detail::send_cmds_prog<0xDB, 0x20>();
    a.startGray();
}
