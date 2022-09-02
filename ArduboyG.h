/*

Optional Configuration Macros (define before including ArduboyG.h):

    Frame sync method. Choices are one of:
    - ABG_SYNC_THREE_PHASE (default)
        Loop around an additional 8 rows to cover the park row. Reduces
        both refresh rate due to extra rows drive and image stability due
        to tighter timing windows, but allows a framebuffer height of 64.
    - ABG_SYNC_PARK_ROW
        Sacrifice the bottom row as the parking row. Improves image
        stability and refresh rate, but usable framebuffer height is 63.

Default Template Configuration:
    
    ArduboyGBase a;
    ArduboyG     a;

Custom Template Configuration:

    ArduboyGBase_Config<ABG_Mode::L3, ABG_Flags:None> a;
    ArduboyG_Config    <ABG_Mode::L3, ABG_Flags:None> a;
                    
Example Usage:

    #define ABG_IMPLEMENTATION
    #include "ArduboyG.h"

    ArduboyG a;

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
        a.print(F("ArduboyG!"));
        
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

#if !defined(ABG_SYNC_THREE_PHASE) && !defined(ABG_SYNC_PARK_ROW)
#define ABG_SYNC_THREE_PHASE
#endif
#ifdef ABG_SYNC_THREE_PHASE
#undef ABG_SYNC_PARK_ROW
#endif
#ifdef ABG_SYNC_PARK_ROW
#undef ABG_SYNC_THREE_PHASE
#endif

#ifndef ABG_REFRESH_HZ
#define ABG_REFRESH_HZ 135
#endif

#undef BLACK
#undef WHITE
static constexpr uint8_t BLACK      = 0;
static constexpr uint8_t DARK_GRAY  = 1;
static constexpr uint8_t DARK_GREY  = 1;
static constexpr uint8_t GRAY       = 1;
static constexpr uint8_t GREY       = 1;
static constexpr uint8_t LIGHT_GRAY = 2;
static constexpr uint8_t LIGHT_GREY = 2;
static constexpr uint8_t WHITE      = 3;
    
enum class ABG_Mode : uint8_t
{
    L4_Contrast,
    L4_Triplane,
    L3,
    
    Default = L4_Contrast,
};

struct ABG_Flags
{
    enum
    {
        None = 0,
        OptimizeFillRect = (1 << 0),
        
        Default = OptimizeFillRect,
    };
};

#ifdef __GNUC__
#define ABG_NOT_SUPPORTED __attribute__((error( \
    "This method cannot be called when using ArduboyG.")))
#else
// will still cause linker error
#define ABG_NOT_SUPPORTED
#endif

namespace abg_detail
{
    
extern uint16_t timer_counter;
extern uint8_t  update_counter;
extern uint8_t  update_every_n;
extern uint8_t  current_plane;
#if defined(ABG_SYNC_THREE_PHASE)
extern uint8_t  current_phase;
#endif
extern bool     needs_display; // needs display work

void send_cmds_(uint8_t const* d, uint8_t n);
void send_cmds_prog_(uint8_t const* d, uint8_t n);

template<uint8_t... CMDS> void send_cmds_prog()
{
    static uint8_t const CMDS_[] PROGMEM = { CMDS... };
    send_cmds_prog_(CMDS_, sizeof(CMDS_));
}

template<class... CMDS> void send_cmds(CMDS... cmds)
{
    uint8_t const CMDS_[] = { cmds... };
    send_cmds_(CMDS_, sizeof(CMDS_));
}

extern uint8_t const YMASK0[8] PROGMEM;
extern uint8_t const YMASK1[8] PROGMEM;

template<bool CLEAR>
void fast_rect(int16_t x, int16_t y, uint8_t w, uint8_t h);

template<
    class    BASE,
    ABG_Mode MODE,
    uint32_t FLAGS
>
struct ArduboyG_Common : public BASE
{
    
    static void startGray()
    {
        send_cmds_prog<
            0xC0, 0xA0, // reset to normal orientation
            0xD9, 0x31, // 1-cycle discharge, 3-cycle charge
            0xA8, 0     // park at row 0
        >();
    
        // Fast PWM mode, prescaler /64
        OCR3A = timer_counter;
        TCCR3A = _BV(WGM31) | _BV(WGM30);
        TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS31) | _BV(CS30);
        TCNT3 = 0;
        bitWrite(TIMSK3, OCIE3A, 1);
    }
    
    static void startGrey() { startGray(); }
    
    static void setUpdateEveryN(uint8_t images)
    {
        update_every_n = images;
    }
    
    static void setRefreshHz(uint8_t hz)
    {
        timer_counter = (F_CPU / 64) / hz;
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
        if(FLAGS & ABG_Flags::OptimizeFillRect)
        {
            color = planeColor(current_plane, color);
            if(color) fast_rect<false>(x, y, 1, h);
            else      fast_rect<true >(x, y, 1, h);
        }
        else Arduboy2Base::drawFastVLine(x, y, h, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawFastVLine(
        int16_t x, int16_t y,
        uint8_t h,
        uint8_t color = WHITE)
    {
        if(FLAGS & ABG_Flags::OptimizeFillRect)
        {
            color = planeColor<PLANE>(color);
            if(color) fast_rect<false>(x, y, 1, h);
            else      fast_rect<true >(x, y, 1, h);
        }
        else Arduboy2Base::drawFastVLine(x, y, h, planeColor<PLANE>(color));
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
        if(FLAGS & ABG_Flags::OptimizeFillRect)
        {
            drawFastHLine(x, y, w, color);
            drawFastHLine(x, y+h-1, w, color);
            drawFastVLine(x, y, h, color);
            drawFastVLine(x+w-1, y, h, color);
        }
        else Arduboy2Base::drawRect(x, y, w, h, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        if(FLAGS & ABG_Flags::OptimizeFillRect)
        {
            drawFastHLine<PLANE>(x, y, w, color);
            drawFastHLine<PLANE>(x, y+h-1, w, color);
            drawFastVLine<PLANE>(x, y, h, color);
            drawFastVLine<PLANE>(x+w-1, y, h, color);
        }
        else Arduboy2Base::drawRect(x, y, w, h, planeColor<PLANE>(color));
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
        if(FLAGS & ABG_Flags::OptimizeFillRect)
        {
            color = planeColor(current_plane, color);
            if(color) fast_rect<false>(x, y, w, h);
            else      fast_rect<true >(x, y, w, h);
        }
        else Arduboy2Base::fillRect(x, y, w, h, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void fillRect(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t color = WHITE)
    {
        if(FLAGS & ABG_Flags::OptimizeFillRect)
        {
            color = planeColor<PLANE>(color);
            if(color) fast_rect<false>(x, y, w, h);
            else      fast_rect<true >(x, y, w, h);
        }
        else Arduboy2Base::fillRect(x, y, w, h, planeColor<PLANE>(color));
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
        if(update_counter >= update_every_n)
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
#if defined(ABG_SYNC_THREE_PHASE)
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
    
    ABG_NOT_SUPPORTED static void flipVertical();
    ABG_NOT_SUPPORTED static void paint8Pixels(uint8_t);
    ABG_NOT_SUPPORTED static void paintScreen(uint8_t const*);
    ABG_NOT_SUPPORTED static void paintScreen(uint8_t[], bool);
    ABG_NOT_SUPPORTED static void setFrameDuration(uint8_t);
    ABG_NOT_SUPPORTED static void setFrameRate(uint8_t);
    ABG_NOT_SUPPORTED static void display();
    ABG_NOT_SUPPORTED static void display(bool);

protected:
    
    static void doDisplay()
    {
        uint8_t* b = getBuffer();
        
#if defined(ABG_SYNC_THREE_PHASE)
        if(current_phase == 1)
        {
            if(MODE == ABG_Mode::L4_Contrast)
                send_cmds(0x81, (current_plane & 1) ? 0xf0 : 0x70);
            send_cmds_prog<0xA8, 7, 0x22, 0, 7>();
        }
        else if(current_phase == 2)
        {
            paint(&b[128 * 7], false, 1, 0xf0);
            send_cmds_prog<0x22, 0, 7>();
        }
        else if(current_phase == 3)
        {
            send_cmds_prog<0x22, 0, 7>();
            paint(&b[128 * 7], false, 1, 0xff);
            send_cmds_prog<0xA8, 0>();
            paint(&b[128 * 0], true, 7, 0xff);
            paint(&b[128 * 7], true, 1, 0x00);

            if(MODE == ABG_Mode::L4_Triplane)
            {
                if(++current_plane >= 3)
                    current_plane = 0;
            }
            else
                current_plane = !current_plane;
            if(current_plane == 0)
                ++update_counter;
        }
#elif defined(ABG_SYNC_PARK_ROW)
        if(MODE == ABG_Mode::L4_Contrast)
            send_cmds(0x81, (current_plane & 1) ? 0xf0 : 0x70);
        paint(&b[128 * 7], true, 1, 0x7F);
        send_cmds_prog<0xA8, 63>();
        paint(&b[128 * 0], true, 7, 0xFF);
        send_cmds_prog<0xA8, 0>();
        if(MODE == ABG_Mode::L4_Triplane)
        {
            if(++current_plane >= 3)
                current_plane = 0;
        }
        else
            current_plane = !current_plane;
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
        
    // Plane              0  1  2
    // ===========================
    //
    // ABG_Mode 0 BLACK       .  .
    // ABG_Mode 0 DARK_GRAY   X  .
    // ABG_Mode 0 LIGHT_GRAY  .  X
    // ABG_Mode 0 WHITE       X  X
    //
    // ABG_Mode 1 BLACK       .  .  .
    // ABG_Mode 1 DARK_GRAY   X  .  .
    // ABG_Mode 1 LIGHT_GRAY  X  X  .
    // ABG_Mode 1 WHITE       X  X  X
    //
    // ABG_Mode 2 BLACK       .  .
    // ABG_Mode 2 GRAY        X  .
    // ABG_Mode 2 WHITE       X  X

    template<uint8_t PLANE>
    static constexpr uint8_t planeColor(uint8_t color)
    {
        return
            MODE == ABG_Mode::L4_Contrast ? ((color & (PLANE + 1)) ? 1 : 0) :
            MODE == ABG_Mode::L4_Triplane ? ((color > PLANE) ? 1 : 0) :
            MODE == ABG_Mode::L3          ? ((color > PLANE) ? 1 : 0) :
            0;
    }

    static uint8_t planeColor(uint8_t plane, uint8_t color)
    {
        if(plane == 0)
            return planeColor<0>(color);
        else if(plane == 1 || MODE != ABG_Mode::L4_Triplane)
            return planeColor<1>(color);
        else
            return planeColor<2>(color);
    }
    
};

} // namespace abg_detail

template<
    ABG_Mode MODE = ABG_Mode::Default,
    uint8_t FLAGS = ABG_Flags::Default
>
using ArduboyGBase_Config = abg_detail::ArduboyG_Common<
    Arduboy2Base, MODE, FLAGS>;

template<
    ABG_Mode MODE = ABG_Mode::Default,
    uint8_t FLAGS = ABG_Flags::Default
>
struct ArduboyG_Config : public abg_detail::ArduboyG_Common<
    Arduboy2, MODE, FLAGS>
{
    
    static void startGray()
    {
        ArduboyGBase_Config<MODE, FLAGS>::startGray();
        setTextColor(WHITE); // WHITE is 3 not 1
    }
    
    static void startGrey() { startGray(); }
    
    static void drawChar(
        int16_t x, int16_t y,
        uint8_t c,
        uint8_t color, uint8_t bg,
        uint8_t size)
    {        
        using A = Arduboy2;
        color = planeColor(abg_detail::current_plane, color);
        bg    = planeColor(abg_detail::current_plane, bg);
        
        if(color == bg)
            fillRect(x, y, size * A::fullCharacterWidth, size * A::characterHeight, bg);
        else
            Arduboy2::drawChar(x, y, c, color, bg, size);
    }
    
    // duplicate from Arduboy2 code to use overridden drawChar
    virtual size_t write(uint8_t c) override
    {        
        using A = Arduboy2;
    
        if ((c == '\r') && !A::textRaw)
        {
            return 1;
        }

        if (((c == '\n') && !A::textRaw) ||
            (A::textWrap && (A::cursor_x > (WIDTH - (A::characterWidth * A::textSize)))))
        {
            A::cursor_x = 0;
            A::cursor_y += A::fullCharacterHeight * A::textSize;
        }

        if ((c != '\n') || A::textRaw)
        {
            drawChar(A::cursor_x, A::cursor_y, c, A::textColor, A::textBackground, A::textSize);
            A::cursor_x += A::fullCharacterWidth * A::textSize;
        }

        return 1;
    }
};
    
using ArduboyGBase = ArduboyGBase_Config<>;
using ArduboyG     = ArduboyG_Config<>;

#ifdef ABG_IMPLEMENTATION

namespace abg_detail
{

uint16_t timer_counter = F_CPU / 64 / 135;
uint8_t update_counter;
uint8_t update_every_n = 1;
uint8_t current_plane;
#if defined(ABG_SYNC_THREE_PHASE)
uint8_t current_phase;
#endif
bool    needs_display;

void send_cmds_(uint8_t const* d, uint8_t n)
{
    Arduboy2Base::LCDCommandMode();
    while(n-- != 0)
        Arduboy2Base::SPItransfer(*d++);
    Arduboy2Base::LCDDataMode();
}
void send_cmds_prog_(uint8_t const* d, uint8_t n)
{
    Arduboy2Base::LCDCommandMode();
    while(n-- != 0)
        Arduboy2Base::SPItransfer(pgm_read_byte(d++));
    Arduboy2Base::LCDDataMode();
}

uint8_t const YMASK0[8] PROGMEM =
{
    0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80
};
uint8_t const YMASK1[8] PROGMEM =
{
    0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
};

template<bool CLEAR>
void fast_rect(int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    if(y >=  64) return;
    if(x >= 128) return;
    if(w == 0 || h == 0) return;
    
    uint8_t y0 = (y < 0 ? 0 : uint8_t(y));
    h = (h >  64 ?  64 : h);
    uint8_t y1 = y0 + h;
    y1 = (y1 > 64 ? 64 : y1) - 1;
    
    uint8_t x0 = (x < 0 ? 0 : uint8_t(x));
    if(x0 + w >= 128) w = 128 - x0;

    uint8_t t0 = y0 & 0xf8;
    uint8_t t1 = y1 & 0xf8;
    uint8_t m0 = pgm_read_byte(&YMASK0[y0 & 7]);
    uint8_t m1 = pgm_read_byte(&YMASK1[y1 & 7]);

    uint8_t* p = &Arduboy2Base::sBuffer[t0 * (128 / 8) + x0];
    uint8_t advance = 128 - w;

    if(t0 == t1)
    {
        uint8_t m = m0 & m1;
        if(CLEAR) m = ~m;
        for(uint8_t n = w; n != 0; --n)
        {
            if(CLEAR) *p++ &= m;
            else      *p++ |= m;
        }
        return;
    }

    {
        uint8_t m = m0;
        if(CLEAR) m = ~m;
        for(uint8_t n = w; n != 0; --n)
        {
            if(CLEAR) *p++ &= m;
            else      *p++ |= m;
        }
        p += advance;
    }

    for(int8_t t = t1 - t0 - 8; t > 0; t -= 8)
    {
        for(uint8_t n = w; n != 0; --n)
            *p++ = (CLEAR ? 0x00 : 0xff);
        p += advance;
    }

    {
        uint8_t m = m1;
        if(CLEAR) m = ~m;
        for(uint8_t n = w; n != 0; --n)
        {
            if(CLEAR) *p++ &= m;
            else      *p++ |= m;
        }
    }
}

template void fast_rect<true >(int16_t x, int16_t y, uint8_t w, uint8_t h);
template void fast_rect<false>(int16_t x, int16_t y, uint8_t w, uint8_t h);

}

ISR(TIMER3_COMPA_vect)
{
    using namespace abg_detail;
#if defined(ABG_SYNC_THREE_PHASE)
    if(++current_phase >= 4) current_phase = 1;

    if(current_phase == 1)
        OCR3A = (timer_counter >> 4) + 1; // phase 2 delay: 4 lines
    else if(current_phase == 2)
        OCR3A = timer_counter;            // phase 3 delay: 64 lines
    else if(current_phase == 3)
        OCR3A = (timer_counter >> 4) + 1; // phase 1 delay: 4 lines
#elif defined(ABG_SYNC_PARK_ROW)
    OCR3A = timer_counter;
#endif

    needs_display = true;
}

#endif