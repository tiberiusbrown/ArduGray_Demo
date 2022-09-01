/*

Options:

    ARDUGRAY_MODE
        Plane mode. Allowed values are:
        - ARDUGRAY_L4_CONTRAST
            4 levels in 2 frames using contrast.
        - ARDUGRAY_L4_TRIPLANE
            4 levels in 3 frames. Visible strobing from decreased image rate.
        - ARDUGRAY_L3
            3 levels in 2 frames. Best image quality.

    ARDUGRAY_SYNC    
        Frame sync method. Allowed values are:
        - ARDUGRAY_PARK_ROW
            Sacrifice the bottom row as the parking row. Improves image
            stability and refresh rate, but usable framebuffer height is 63.
        - ARDUGRAY_THREE_PHASE
            Loop around an additional 8 rows to cover the park row. Reduces
            both refresh rate due to extra rows drive and image stability due
            to tighter timing windows, but allows a framebuffer height of 64.
                        
    ARDUGRAY_HZ
        Target display refresh rate. Usually best left at the default value.
                    
    ARDUGRAY_UPDATE_EVERY_N
        Determines how many image cycles between game logic updates, as
        indicated by Ardugray::needsUpdate.

Example Usage:

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

*/

#pragma once

#include <Arduboy2.h>

#define ARDUGRAY_MODE_L4_CONTRAST 0
#define ARDUGRAY_MODE_L4_TRIPLANE 1
#define ARDUGRAY_MODE_L3          2

#define ARDUGRAY_PARK_ROW    0
#define ARDUGRAY_THREE_PHASE 1

#ifndef ARDUGRAY_MODE
#define ARDUGRAY_MODE ARDUGRAY_MODE_L4_CONTRAST
#endif

#ifndef ARDUGRAY_SYNC
#define ARDUGRAY_SYNC ARDUGRAY_THREE_PHASE
#endif

#ifndef ARDUGRAY_HZ
#define ARDUGRAY_HZ 135
#endif

#ifndef ARDUGRAY_UPDATE_EVERY_N
#define ARDUGRAY_UPDATE_EVERY_N 1
#endif

#undef BLACK
#undef WHITE

#define BLACK      0
#define DARK_GRAY  1
#define GRAY       1
#define LIGHT_GRAY 2
#define WHITE      3

#if ARDUGRAY_MODE < 0 || ARDUGRAY_MODE > 2
#error "ARDUGRAY_MODE must be 0, 1, or 2"
#endif

#if ARDUGRAY_UPDATE_EVERY_N < 1
#error "ARDUGRAY_UPDATE_EVERY_N must be greater than 0"
#endif

#ifdef __GNUC__
#define ARDUGRAY_NOT_SUPPORTED \
    __attribute__((error( \
    "This method cannot be called when using ArduGray." \
    )))
#else
// will still cause linker error
#define ARDUGRAY_NOT_SUPPORTED
#endif

#define ARDUGRAY_TIMER_COUNTER \
    (16000000 / 64 / ARDUGRAY_HZ)

namespace ardugray_detail
{

extern uint8_t  update_counter;
extern uint8_t  current_plane;
#if ARDUGRAY_SYNC == ARDUGRAY_THREE_PHASE
extern uint8_t  current_phase;
#endif
extern bool     needs_display; // needs display work and/or a call to RENDER_FUNC

// Plane              0  1  2
// ===========================
//
// Mode 0 BLACK       .  .
// Mode 0 DARK_GRAY   X  .
// Mode 0 LIGHT_GRAY  .  X
// Mode 0 WHITE       X  X
//
// Mode 1 BLACK       .  .  .
// Mode 1 DARK_GRAY   X  .  .
// Mode 1 LIGHT_GRAY  X  X  .
// Mode 1 WHITE       X  X  X
//
// Mode 2 BLACK       .  .
// Mode 2 GRAY        X  .
// Mode 2 WHITE       X  X

template<uint8_t PLANE>
static constexpr uint8_t planeColor(uint8_t color)
{
#if ARDUGRAY_MODE == 0
    return (color & (PLANE + 1)) ? WHITE : BLACK;
#elif ARDUGRAY_MODE == 1
    return (color > PLANE) ? WHITE : BLACK;
#else
    return (color > PLANE) ? WHITE : BLACK;
#endif
}

static uint8_t planeColor(uint8_t plane, uint8_t color)
{
    if(plane == 0)
        return planeColor<0>(color);
    else if(plane == 1 || ARDUGRAY_MODE != 1)
        return planeColor<1>(color);
    else
        return planeColor<2>(color);
}

static void send_cmds(uint8_t const* d, uint8_t n)
{
    Arduboy2Base::LCDCommandMode();
    while(n-- != 0)
        Arduboy2Base::SPItransfer(*d++);
    Arduboy2Base::LCDDataMode();
}
#define ARDUGRAY_SEND_CMDS(...) \
    do { \
        uint8_t const CMDS_[] = { __VA_ARGS__ }; \
        send_cmds(CMDS_, sizeof(CMDS_)); \
    } while(0)

static void send_cmds_prog(uint8_t const* d, uint8_t n)
{
    Arduboy2Base::LCDCommandMode();
    while(n-- != 0)
        Arduboy2Base::SPItransfer(pgm_read_byte(d++));
    Arduboy2Base::LCDDataMode();
}
#define ARDUGRAY_SEND_CMDS_PROG(...) \
    do { \
        static uint8_t const CMDS_[] PROGMEM = { __VA_ARGS__ }; \
        send_cmds_prog(CMDS_, sizeof(CMDS_)); \
    } while(0)

template<class BASE> struct ArduGray_Common : public BASE
{
    
    static void startGray()
    {
        ARDUGRAY_SEND_CMDS_PROG(
            0xC0, 0xA0, // reset to normal orientation
            0xD9, 0x31, // 1-cycle discharge, 3-cycle charge
            0xA8, 0,    // park at row 0
        );
    
        // Fast PWM mode, prescaler /64
        OCR3A = ARDUGRAY_TIMER_COUNTER;
        TCCR3A = _BV(WGM31) | _BV(WGM30);
        TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS31) | _BV(CS30);
        TCNT3 = 0;
        bitWrite(TIMSK3, OCIE3A, 1);
    }
    
    static void drawBitmap(
        int16_t x, int16_t y,
        uint8_t const* bitmap,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawBitmap(x, y, bitmap, w, h, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawBitmap(
        int16_t x, int16_t y,
        uint8_t const* bitmap,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawBitmap(x, y, bitmap, w, h, planeColor<PLANE>(color));
    }
    
    static void drawSlowXYBitmap(
        int16_t x, int16_t y,
        uint8_t const* bitmap,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawSlowXYBitmap(x, y, bitmap, w, h, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawSlowXYBitmap(
        int16_t x, int16_t y,
        uint8_t const* bitmap,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawSlowXYBitmap(x, y, bitmap, w, h, planeColor<PLANE>(color));
    }
    
    static void drawCompressed(
        int16_t sx, int16_t sy,
        uint8_t const* bitmap,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawCompressed(sx, sy, bitmap, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawCompressed(
        int16_t sx, int16_t sy,
        uint8_t const* bitmap,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawCompressed(sx, sy, bitmap, planeColor<PLANE>(color));
    }
    
    static void drawPixel(
        int16_t x, int16_t y,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawPixel(x, y, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawPixel(
        int16_t x, int16_t y,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawPixel(x, y, planeColor<PLANE>(color));
    }
    
    static void drawFastHLine(
        int16_t x, int16_t y,
        uint8_t w,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawFastHLine(x, y, w, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawFastHLine(
        int16_t x, int16_t y,
        uint8_t w,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawFastHLine(x, y, w, planeColor<PLANE>(color));
    }
    
    static void drawFastVLine(
        int16_t x, int16_t y,
        uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawFastVLine(x, y, h, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawFastVLine(
        int16_t x, int16_t y,
        uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawFastVLine(x, y, h, planeColor<PLANE>(color));
    }
    
    static void drawLine(
        int16_t x0, int16_t y0,
        int16_t x1, int16_t y1,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawLine(x0, y0, x1, y1, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawLine(
        int16_t x0, int16_t y0,
        int16_t x1, int16_t y1,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawLine(x0, y0, x1, y1, planeColor<PLANE>(color));
    }
    
    static void drawCircle(
        int16_t x0, int16_t y0,
        uint8_t r,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawCircle(x0, y0, r, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawCircle(
        int16_t x0, int16_t y0,
        uint8_t r,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawCircle(x0, y0, r, planeColor<PLANE>(color));
    }
    
    static void drawTriangle(
        int16_t x0, int16_t y0,
        int16_t x1, int16_t y1,
        int16_t x2, int16_t y2,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawTriangle(x0, y0, x1, y1, x2, y2, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawTriangle(
        int16_t x0, int16_t y0,
        int16_t x1, int16_t y1,
        int16_t x2, int16_t y2,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawTriangle(x0, y0, x1, y1, x2, y2, planeColor<PLANE>(color));
    }
    
    static void drawRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawRect(x, y, w, h, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawRect(x, y, w, h, planeColor<PLANE>(color));
    }
    
    static void drawRoundRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t r,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawRoundRect(x, y, w, h, r, planeColor(current_plane, color));
    }

    template<uint8_t PLANE>
    static void drawRoundRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t r,
        uint8_t color = WHITE)
    {
        Arduboy2Base::drawRoundRect(x, y, w, h, r, planeColor<PLANE>(color));
    }
    
    static void fillCircle(
        int16_t x0, int16_t y0,
        uint8_t r,
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillCircle(x0, y0, r, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void fillCircle(
        int16_t x0, int16_t y0,
        uint8_t r,
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillCircle(x0, y0, r, planeColor<PLANE>(color));
    }
    
    static void fillTriangle(
        int16_t x0, int16_t y0,
        int16_t x1, int16_t y1,
        int16_t x2, int16_t y2,
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillTriangle(x0, y0, x1, y1, x2, y2, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void fillTriangle(
        int16_t x0, int16_t y0,
        int16_t x1, int16_t y1,
        int16_t x2, int16_t y2,
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillTriangle(x0, y0, x1, y1, x2, y2, planeColor<PLANE>(color));
    }
    
    static void fillRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillRect(x, y, w, h, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void fillRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillRect(x, y, w, h, planeColor<PLANE>(color));
    }
    
    static void fillRoundRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t r,
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillRoundRect(x, y, w, h, r, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void fillRoundRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t r,
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillRoundRect(x, y, w, h, r, planeColor<PLANE>(color));
    }
    
    static void fillScreen(
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillScreen(planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void fillScreen(
        uint8_t color = WHITE)
    {
        Arduboy2Base::fillScreen(planeColor<PLANE>(color));
    }    

    static uint8_t currentPlane()
    {
        return current_plane;
    }
    
    static bool needsUpdate()
    {
        if(update_counter >= ARDUGRAY_UPDATE_EVERY_N)
        {
            update_counter = 0;
            ++BASE::frameCount; // to allow everyXFrames
            return true;
        }
        return false;
    }
    
    static bool nextFrame()
    {
        if(!needs_display) return false;
        needs_display = false;
        doDisplay();
#if ARDUGRAY_SYNC == ARDUGRAY_THREE_PHASE
        return current_phase == 3;
#else
        return true;
#endif
    }
    static bool nextFrameDEV()
    {
        bool r = nextFrame();
        if(needs_display)
            TXLED1;
        else
            TXLED0;
        return r;
    }
    
    ARDUGRAY_NOT_SUPPORTED static void flipVertical();
    ARDUGRAY_NOT_SUPPORTED static void paint8Pixels(uint8_t);
    ARDUGRAY_NOT_SUPPORTED static void paintScreen(uint8_t const*);
    ARDUGRAY_NOT_SUPPORTED static void paintScreen(uint8_t[], bool);
    ARDUGRAY_NOT_SUPPORTED static bool setFrameDuration(uint8_t);
    ARDUGRAY_NOT_SUPPORTED static bool setFrameRate(uint8_t);
    ARDUGRAY_NOT_SUPPORTED static bool display();
    ARDUGRAY_NOT_SUPPORTED static bool display(bool);

protected:
    
    static void doDisplay()
    {
        uint8_t* b = getBuffer();
        
#if ARDUGRAY_SYNC == ARDUGRAY_THREE_PHASE
        if(current_phase == 1)
        {
#if ARDUGRAY_MODE == ARDUGRAY_L4_CONTRAST
            ARDUGRAY_SEND_CMDS(0x81, (current_plane & 1) ? 0xf0 : 0x70);
#endif
            ARDUGRAY_SEND_CMDS_PROG(0xA8, 7, 0x22, 0, 7);
        }
        else if(current_phase == 2)
        {
            paint(&b[128 * 7], false, 1, 0xf0);
            ARDUGRAY_SEND_CMDS_PROG(0x22, 0, 7);
        }
        else if(current_phase == 3)
        {
            ARDUGRAY_SEND_CMDS_PROG(0x22, 0, 7);
            paint(&b[128 * 7], false, 1, 0xff);
            ARDUGRAY_SEND_CMDS_PROG(0xA8, 0);
            paint(&b[128 * 0], true, 7, 0xff);
            paint(&b[128 * 7], true, 1, 0x00);

#if ARDUGRAY_MODE == ARDUGRAY_L4_TRIPLANE
            if(++current_plane >= 3)
                current_plane = 0;
#else
            current_plane = !current_plane;
#endif
            if(current_plane == 0)
                ++update_counter;
        }
#elif ARDUGRAY_SYNC == ARDUGRAY_PARK_ROW
#if ARDUGRAY_MODE == ARDUGRAY_L4_CONTRAST
        ARDUGRAY_SEND_CMDS(0x81, (current_plane & 1) ? 0xf0 : 0x70);
#endif
        paint(&b[128 * 7], true, 1, 0x7F);
        ARDUGRAY_SEND_CMDS_PROG(0xA8, 63);
        paint(&b[128 * 0], true, 7, 0xFF);
        ARDUGRAY_SEND_CMDS_PROG(0xA8, 0);
#if ARDUGRAY_MODE == ARDUGRAY_L4_TRIPLANE
        if(++current_plane >= 3)
            current_plane = 0;
#else
        current_plane = !current_plane;
#endif
        if(current_plane == 0)
            ++update_counter;
#endif
    }
    
    static void paint(uint8_t* image, bool clear, uint8_t pages, uint8_t mask)
    {
        uint16_t count = pages * 128;
        SPCR = _BV(SPE) | _BV(MSTR) | _BV(DORD); // MSB-to-LSB
        image += count;
        asm volatile(
            "1: ld    __tmp_reg__, -%a[ptr]     ;2        \n\t" //tmp = *(--image)
            "   mov   r16, __tmp_reg__          ;1        \n\t" //tmp2 = tmp
            "   cpse  %[clear], __zero_reg__    ;1/2      \n\t" //if(clear)
            "   mov   r16, __zero_reg__         ;1        \n\t" //    tmp2 = 0;
            "   st    %a[ptr], r16              ;2        \n\t" //*(image) = tmp2
            "   subi  r16, 0                    ;1        \n\t" //[delay]
            "   sbiw  %a[count], 0              ;2        \n\t" //[delay]
            "   sbiw  %a[count], 0              ;2        \n\t" //[delay]
            "   and   __tmp_reg__, %[mask]      ;1        \n\t" //tmp &= mask
            "   out   %[spdr], __tmp_reg__      ;1        \n\t" //SPDR = tmp
            "   sbiw  %a[count], 1              ;2        \n\t" //len--
            "   brne  1b                        ;1/2 :18  \n\t" //len > 0
            "   in    __tmp_reg__, %[spsr]                \n\t" //read SPSR to clear SPIF
            "   sbiw  %a[count], 0                        \n\t"
            "   sbiw  %a[count], 0                        \n\t" // delay before resetting DORD
            "   sbiw  %a[count], 0                        \n\t" // below so it doesn't mess up
            "   sbiw  %a[count], 0                        \n\t" // the last transfer
            "   sbiw  %a[count], 0                        \n\t"
            : [ptr]     "+&e" (image),
              [count]   "+&w" (count)
            : [spdr]    "I"   (_SFR_IO_ADDR(SPDR)),
              [spsr]    "I"   (_SFR_IO_ADDR(SPSR)),
              [clear]   "r"   (clear),
              [mask]    "r"   (mask)
            : "r16"
        );
        SPCR = _BV(SPE) | _BV(MSTR); // LSB-to-MSB
    }
        
};

} // namespace ardugray_detail

using ArduGrayBase = ardugray_detail::ArduGray_Common<Arduboy2Base>;

struct ArduGray : public ardugray_detail::ArduGray_Common<Arduboy2>
{
    
    static void startGray()
    {
        ArduGrayBase::startGray();
        setTextColor(WHITE); // WHITE is 3 not 1
    }
    
    static void drawChar(
        int16_t x, int16_t y,
        uint8_t c,
        uint8_t color, uint8_t bg,
        uint8_t size)
    {
        using namespace ardugray_detail;
        color = planeColor(current_plane, color);
        bg    = planeColor(current_plane, bg);
        
        if(color == bg)
            fillRect(x, y, size * fullCharacterWidth, size * fullCharacterHeight, bg);
        else
            Arduboy2::drawChar(x, y, c, color, bg, size);
    }
    
    // duplicate from Arduboy2 code to use overridden drawChar
    virtual size_t write(uint8_t c) override
    {        
        if ((c == '\r') && !textRaw)
        {
            return 1;
        }

        if (((c == '\n') && !textRaw) ||
            (textWrap && (cursor_x > (WIDTH - (characterWidth * textSize)))))
        {
            cursor_x = 0;
            cursor_y += fullCharacterHeight * textSize;
        }

        if ((c != '\n') || textRaw)
        {
            drawChar(cursor_x, cursor_y, c, textColor, textBackground, textSize);
            cursor_x += fullCharacterWidth * textSize;
        }

        return 1;
    }
};

#ifdef ARDUGRAY_IMPLEMENTATION

namespace ardugray_detail
{

uint8_t update_counter;
uint8_t current_plane;
#if ARDUGRAY_SYNC == ARDUGRAY_THREE_PHASE
uint8_t current_phase;
#endif
bool    needs_display;

}

ISR(TIMER3_COMPA_vect)
{
    using namespace ardugray_detail;
#if ARDUGRAY_SYNC == ARDUGRAY_THREE_PHASE
    if(++current_phase >= 4) current_phase = 1;

    if(current_phase == 1)
        OCR3A = (ARDUGRAY_TIMER_COUNTER >> 4) + 1; // phase 2 delay: 4 lines
    else if(current_phase == 2)
        OCR3A = ARDUGRAY_TIMER_COUNTER;            // phase 3 delay: 64 lines
    else if(current_phase == 3)
        OCR3A = (ARDUGRAY_TIMER_COUNTER >> 4) + 1; // phase 1 delay: 4 lines
#elif ARDUGRAY_SYNC == ARDUGRAY_PARK_ROW
    OCR3A = ARDUGRAY_TIMER_COUNTER;
#endif

    needs_display = true;
}

#endif
