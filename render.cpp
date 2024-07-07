#define SPRITESU_IMPLEMENTATION
#include "common.hpp"

#include "tile_img.hpp"

static uint8_t const TILEMAP[16 * 8] PROGMEM =
{
    18,19,146,59,134,155,170,6,171,37,37,38,27,17,19,43,
    35,161,162,163,28,134,155,154,7,7,7,135,74,52,52,75,
    27,177,178,179,11,11,134,135,81,74,52,52,219,50,50,51,
    27,193,194,195,17,3,17,19,74,219,50,50,202,53,53,91,
    28,209,128,211,17,35,142,74,219,202,53,53,91,156,157,158,
    19,59,74,52,52,52,52,219,202,91,17,19,118,172,173,174,
    1,3,90,203,50,50,202,53,91,118,6,6,187,204,205,206,
    34,34,43,90,53,53,91,59,118,171,114,154,135,220,221,222
};

void render()
{
    for(uint8_t y = 0; y < 8; ++y)
    {
        for(uint8_t x = 0; x < 16; ++x)
        {
            uint8_t tile = pgm_read_byte(&TILEMAP[y * 16 + x]) - 1;
            uint16_t frame = tile * 3 + a.currentPlane();
            SpritesU::drawOverwrite(
                x * 16 - ox,
                y * 16 - oy,
                TILE_IMG,
                frame);
        }
    }
    
    SpritesU::fillRect_i8(0, 0, 10, 40, a.color(BLACK));
    SpritesU::fillRect_i8(0, 10, 8, 8, a.color(DARK_GRAY));
    SpritesU::fillRect_i8(0, 20, 8, 8, a.color(LIGHT_GRAY));
    SpritesU::fillRect_i8(0, 30, 8, 8, a.color(WHITE));
}
