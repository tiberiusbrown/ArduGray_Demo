/*

Optional Configuration Macros (define before including ArduboyG.h):

    Frame sync method. Choices are one of:
    - ABG_SYNC_THREE_PHASE (default)
        Loop around an additional 8 rows to cover the park row. Reduces
        both refresh rate and time available for rendering due to extra
        rows driven, but allows a framebuffer height of 64.
    - ABG_SYNC_PARK_ROW
        Sacrifice the bottom row as the parking row. Improves rendering
        time and refresh rate, but usable framebuffer height is 63.
    - ABG_SYNC_SLOW_DRIVE
        Slow down the display's row drive time as much as possible while
        parked to allow updating GDDRAM for the park row while it's being
        driven. Achieves the speed of ABG_SYNC_PARK_ROW without losing
        the 64th row, at the expense of slight glitches on the park row.

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

*/

#pragma once

#include <Arduboy2.h>

#if !defined(ABG_SYNC_THREE_PHASE) && \
    !defined(ABG_SYNC_PARK_ROW) && \
    !defined(ABG_SYNC_SLOW_DRIVE)
#define ABG_SYNC_THREE_PHASE
#endif

#if !defined(ABG_TIMER3) && \
    !defined(ABG_TIMER4)
#define ABG_TIMER3
#endif

#undef BLACK
#undef WHITE
constexpr uint8_t BLACK      = 0;
constexpr uint8_t DARK_GRAY  = 1;
constexpr uint8_t DARK_GREY  = 1;
constexpr uint8_t GRAY       = 1;
constexpr uint8_t GREY       = 1;
constexpr uint8_t LIGHT_GRAY = 2;
constexpr uint8_t LIGHT_GREY = 2;
constexpr uint8_t WHITE      = 3;
    
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
        OptimizeFillRect         = (1 << 0),
        OptimizeDrawOverwrite    = (1 << 1),
        OptimizeDrawExternalMask = (1 << 2),
        
        Default =
            OptimizeFillRect         |
            OptimizeDrawOverwrite    |
            OptimizeDrawExternalMask |
            0,
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

enum class SpriteMode : uint8_t
{
    Overwrite,
    PlusMask,
    ExternalMask,
};

template<SpriteMode SPRITE_MODE> void draw_sprite(
    int16_t x, int16_t y,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame);

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

        uint8_t sreg = SREG;
        cli();
#if defined(ABG_TIMER3)
        // Fast PWM mode, prescaler /64
        OCR3A = timer_counter;
        TCCR3A = _BV(WGM31) | _BV(WGM30);
        TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS31) | _BV(CS30);
        TCNT3 = 0;
        bitWrite(TIMSK3, OCIE3A, 1);
#elif defined(ABG_TIMER4)
        // Fast PWM mode, prescaler /256
        TC4H = (timer_counter >> 8);
        OCR4C = timer_counter;
        TCCR4A = 0;
        TCCR4B = 0x09; // prescaler /256
        TCCR4C = 0x01; // PWM4D=1 just to enable fast PWM
        TCCR4D = 0;    // WGM41,WGM40=00: fast PWM
        TC4H = 0;
        TCNT4 = 0;
        bitWrite(TIMSK4, TOIE4, 1);
#endif
        SREG = sreg;
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
    
    void drawOverwrite(
        int16_t x, int16_t y,
        uint8_t const* bitmap,
        uint8_t frame)
    {
        static_assert(MODE != ABG_Mode::L4_Triplane,
            "drawOverwrite does not support L4_Triplane mode");
        if(FLAGS & ABG_Flags::OptimizeDrawOverwrite)
            draw_sprite<SpriteMode::Overwrite>(x, y, bitmap, frame, nullptr, 0);
        else
        {
            frame *= 2;
            if(current_plane == 1)
                frame += 1;
            Sprites::drawOverwrite(x, y, bitmap, frame);
        }
    }
    
    void drawExternalMask(
        int16_t x, int16_t y,
        uint8_t const* bitmap, uint8_t const* mask,
        uint8_t frame, uint8_t mask_frame)
    {
        static_assert(MODE != ABG_Mode::L4_Triplane,
            "drawExternalMask does not support L4_Triplane mode");
        if(FLAGS & ABG_Flags::OptimizeDrawExternalMask)
            draw_sprite<SpriteMode::ExternalMask>(x, y, bitmap, frame, mask, mask_frame);
        else
        {
            frame *= 2;
            if(current_plane == 1)
                frame += 1;
            Sprites::drawExternalMask(x, y, bitmap, mask, frame, mask_frame);
        }
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
#elif defined(ABG_SYNC_PARK_ROW) || defined(ABG_SYNC_SLOW_DRIVE)
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
#elif defined(ABG_SYNC_PARK_ROW) || defined(ABG_SYNC_SLOW_DRIVE)
        if(MODE == ABG_Mode::L4_Contrast)
            send_cmds(0x81, (current_plane & 1) ? 0xf0 : 0x70);
#if defined(ABG_SYNC_PARK_ROW)
        paint(&b[128 * 7], true, 1, 0x7f);
        send_cmds_prog<0xA8, 63>();
        paint(&b[128 * 0], true, 7, 0xff);
        send_cmds_prog<0xA8, 0>();
#elif defined(ABG_SYNC_SLOW_DRIVE)
        {
            uint8_t sreg = SREG;
            cli();
            // 1. Run the dot clock into the ground
            // 2. Disable the charge pump
            // 3. Make phase 1 and 2 very large
            send_cmds_prog<
                0x22, 0, 7, 0x8D, 0x0, 0xD5, 0x0F, 0xD9, 0xFF>();
            paint(&b[128 * 7], false, 1, 0xff);
            send_cmds_prog<
                0xA8, 63, 0x8D, 0x14, 0xD9, 0x31, 0xD5, 0xF0>();
            SREG = sreg;
        }
        paint(&b[128 * 0], true, 7, 0xff);
        send_cmds_prog<0xA8, 0>();
        paint(&b[128 * 7], true, 1, 0x00);
#endif
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
        
    // Plane                               0  1  2
    // ============================================
    //
    // ABG_Mode::L4_Contrast   BLACK       .  .
    // ABG_Mode::L4_Contrast   DARK_GRAY   X  .
    // ABG_Mode::L4_Contrast   LIGHT_GRAY  .  X
    // ABG_Mode::L4_Contrast   WHITE       X  X
    //
    // ABG_Mode::L4_Triplane   BLACK       .  .  .
    // ABG_Mode::L4_Triplane   DARK_GRAY   X  .  .
    // ABG_Mode::L4_Triplane   LIGHT_GRAY  X  X  .
    // ABG_Mode::L4_Triplane   WHITE       X  X  X
    //
    // ABG_Mode::L3            BLACK       .  .
    // ABG_Mode::L3            GRAY        X  .
    // ABG_Mode::L3            WHITE       X  X

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
    size_t write(uint8_t c) override
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

#if defined(ABG_TIMER3)
uint16_t timer_counter = F_CPU / 64 / 135;
#elif defined(ABG_TIMER4)
uint16_t timer_counter = F_CPU / 256 / 135;
#endif
uint8_t  update_counter;
uint8_t  update_every_n = 1;
uint8_t  current_plane;
#if defined(ABG_SYNC_THREE_PHASE)
uint8_t  current_phase;
#endif
bool     needs_display;

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
    if(y + h <= 0) return;
    if(x + w <= 0) return;
    
    uint8_t y0 = (y < 0 ? 0 : uint8_t(y));
    h = (h >  64 ?  64 : h);
    uint8_t y1 = y + h;
    y1 = (y1 > 64 ? 64 : y1) - 1;
    
    uint8_t x0 = x;
    if(x < 0)
    {
        w += int8_t(x);
        x0 = 0;
    }

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

static inline void draw_overwrite_top(
    uint8_t* buf, uint8_t const* image, uint16_t mask, uint8_t shift_coef, uint8_t n)
{
    uint16_t image_data;
    uint8_t buf_data;
    uint8_t* buf2 = buf + 128;
    asm volatile(
        "L1_%=:\n"
        "lpm %A[image_data], Z+\n"
        "mul %A[image_data], %[shift_coef]\n"
        "movw %[image_data], r0\n"
        "ld %[buf_data], X\n"
        "and %[buf_data], %B[mask]\n"
        "or %[buf_data], %B[image_data]\n"
        "st X+, %[buf_data]\n"
        "dec %[n]\n"
        "brne L1_%=\n"
        "clr __zero_reg__\n"
        :
        [buf2]       "+&x" (buf2),
        [image]      "+&z" (image),
        [n]          "+&a" (n),
        [buf_data]   "=&r" (buf_data),
        [image_data] "=&r" (image_data)
        :
        [mask]       "r"   (mask),
        [shift_coef] "r"   (shift_coef)
        );
}

template<SpriteMode SPRITE_MODE> void draw_sprite(
    int16_t x, int16_t y,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame)
{
    if(image == nullptr) return;
    if(SPRITE_MODE == SpriteMode::ExternalMask && mask == nullptr) return;
    if(x >= 128 || y >= 64) return;
    
    uint8_t w, h;
    asm volatile(
        "lpm %[w], Z+\n"
        "lpm %[h], Z+\n"
        : [w] "=r" (w), [h] "=r" (h), [image] "+z" (image));

    if(x + w <= 0) return;
    if(y + h <= 0) return;
    
    uint8_t pages = h;
    asm volatile(
        "lsr %[pages]\n"
        "lsr %[pages]\n"
        "lsr %[pages]\n"
        : [pages] "+&r" (pages));
    if(h & 7) ++pages;
    
    {
        uint16_t plane_bytes = pages * w;
        uint16_t frame_bytes = plane_bytes * 2;
        if( SPRITE_MODE == SpriteMode::Overwrite ||
            SPRITE_MODE == SpriteMode::ExternalMask)
        {
            image += frame_bytes * frame;
            if(current_plane == 1)
                image += plane_bytes;
        }
        if(SPRITE_MODE == SpriteMode::ExternalMask)
            mask += plane_bytes * mask_frame;
    }
    
    uint8_t shift_coef;
    uint16_t shift_mask;
    {
        uint8_t y_offset = uint8_t(y) & 7;
        shift_coef = 1 << y_offset;
        shift_mask = ~(0xff * shift_coef);
    }
    
    // y /= 8 (round to -inf)
    asm volatile(
        "asr %B[y]\n"
        "ror %A[y]\n"
        "asr %B[y]\n"
        "ror %A[y]\n"
        "asr %B[y]\n"
        "ror %A[y]\n"
        : [y] "+&r" (y));
    
    // clip against top edge
    int8_t page_start = int8_t(y);
    if(page_start < -1)
    {
        uint8_t tp = (-1 - page_start);
        image += tp * w;
        pages -= tp;
        page_start = -1;
    }
    
    // clip against left edge
    uint8_t n = w;
    int8_t col_start = int8_t(x);
    if(col_start < 0)
    {
        image -= col_start;
        n += col_start;
        col_start = 0;
    }
    
    uint8_t* buf = Arduboy2Base::sBuffer;
    buf += page_start * 128;
    buf += col_start;

    // clip against right edge
    if(n > 128 - col_start)
        n = 128 - col_start;
    
    // clip against bottom edge
    bool bottom = false;
    if(pages > 7 - page_start)
    {
        pages = 7 - page_start;
        bottom = true;
    }
    
    uint8_t buf_adv = 128 - n;
    uint8_t image_adv = w - n;
    
    if(SPRITE_MODE == SpriteMode::Overwrite)
    {
        uint16_t image_data;
        uint8_t buf_data;
        uint8_t count;
        asm volatile(R"ASM(
        
                cpi %[page_start], 0
                brge L%=_middle
                
                ; advance buf to next page
                subi %A[buf], lo8(-128)
                sbci %B[buf], hi8(-128)
                
            L%=_top_loop:
            
                ; write one page from image to buf+128
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                ld %[buf_data], %a[buf]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], %B[image_data]
                st %a[buf]+, %[buf_data]
                dec %[n]
                brne L%=_top_loop
                
                ; decrement pages, reset buf back, advance image
                clr __zero_reg__
                dec %[pages]
                sub %A[buf], %[n]
                sbc %B[buf], __zero_reg__
                add %A[image], %[image_adv]
                adc %B[image], __zero_reg__
        
            L%=_middle:
            
                tst %[pages]
                breq L%=_bottom
            
                ; need Y pointer for middle pages
                push r28
                push r29
                movw r28, %[buf]
                subi r28, lo8(-128)
                sbci r29, hi8(-128)
                
            L%=_middle_loop_outer:
            
                mov %[count], %[n]
                
            L%=_middle_loop_inner:

                ; write one page from image to buf/buf+128
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                ld %[buf_data], Y
                and %[buf_data], %B[shift_mask]
                or %[buf_data], %B[image_data]
                st Y+, %[buf_data]
                dec %[count]
                brne L%=_middle_loop_inner
                
                ; advance buf, buf+128, and image to the next page
                clr __zero_reg__
                add %A[buf], %[buf_adv]
                adc %B[buf], __zero_reg__
                add r28, %[buf_adv]
                adc r29, __zero_reg__
                add %A[image], %[image_adv]
                adc %B[image], __zero_reg__
                dec %[pages]
                brne L%=_middle_loop_outer
                
                ; done with Y pointer
                pop r29
                pop r28
                
            L%=_bottom:
            
                tst %[bottom]
                breq L%=finish
                
            L%=_bottom_loop:
            
                ; write one page from image to buf
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                dec %[n]
                brne L%=_bottom_loop
            
            L%=finish:
            
                clr __zero_reg__
                
            )ASM"
            :
            [buf]        "+&x" (buf),
            [image]      "+&z" (image),
            [pages]      "+&a" (pages),
            [count]      "=&l" (count),
            [buf_data]   "=&a" (buf_data),
            [image_data] "=&a" (image_data)
            :
            [n]          "l"   (n),
            [buf_adv]    "l"   (buf_adv),
            [image_adv]  "l"   (image_adv),
            [shift_mask] "l"   (shift_mask),
            [shift_coef] "l"   (shift_coef),
            [bottom]     "l"   (bottom),
            [page_start] "a"   (page_start)
            );
    }
    
    if(SPRITE_MODE == SpriteMode::ExternalMask)
    {
        uint8_t const* image_temp;
        uint16_t image_data;
        uint16_t mask_data;
        uint8_t buf_data;
        uint8_t count;
        asm volatile(R"ASM(
        
                cpi %[page_start], 0
                brge L%=_middle
                
                ; advance buf to next page
                subi %A[buf], lo8(-128)
                sbci %B[buf], hi8(-128)
                
            L%=_top_loop:
                
                ; write one page from image to buf+128
                lpm %A[image_data], %a[image]+
                movw %[image_temp], %[image]
                movw %[image], %[mask]
                lpm %A[mask_data], %a[image]+
                movw %[mask], %[image]
                movw %[image], %[image_temp]
                
                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                mul %A[mask_data], %[shift_coef]
                movw %[mask_data], r0
                
                ld %[buf_data], %a[buf]
                com %B[mask_data]
                and %[buf_data], %B[mask_data]
                or %[buf_data], %B[image_data]
                st %a[buf]+, %[buf_data]
                dec %[n]
                brne L%=_top_loop
                
                ; decrement pages, reset buf back, advance image and mask
                clr __zero_reg__
                dec %[pages]
                sub %A[buf], %[n]
                sbc %B[buf], __zero_reg__
                add %A[image], %[image_adv]
                adc %B[image], __zero_reg__
                add %A[mask], %[image_adv]
                adc %B[mask], __zero_reg__
        
            L%=_middle:
            
                tst %[pages]
                breq L%=_bottom
            
                ; need Y pointer for middle pages
                push r28
                push r29
                movw r28, %[buf]
                subi r28, lo8(-128)
                sbci r29, hi8(-128)
                
            L%=_middle_loop_outer:
            
                mov %[count], %[n]
                
            L%=_middle_loop_inner:

                ; write one page from image to buf/buf+128
                lpm %A[image_data], %a[image]+
                movw %[image_temp], %[image]
                movw %[image], %[mask]
                lpm %A[mask_data], %a[image]+
                movw %[mask], %[image]
                movw %[image], %[image_temp]
                
                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                mul %A[mask_data], %[shift_coef]
                movw %[mask_data], r0
                
                ld %[buf_data], %a[buf]
                com %A[mask_data]
                and %[buf_data], %A[mask_data]
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                ld %[buf_data], Y
                com %B[mask_data]
                and %[buf_data], %B[mask_data]
                or %[buf_data], %B[image_data]
                st Y+, %[buf_data]
                dec %[count]
                brne L%=_middle_loop_inner
                
                ; advance buf, buf+128, and image to the next page
                clr __zero_reg__
                add %A[buf], %[buf_adv]
                adc %B[buf], __zero_reg__
                add r28, %[buf_adv]
                adc r29, __zero_reg__
                add %A[image], %[image_adv]
                adc %B[image], __zero_reg__
                add %A[mask], %[image_adv]
                adc %B[mask], __zero_reg__
                dec %[pages]
                brne L%=_middle_loop_outer
                
                ; done with Y pointer
                pop r29
                pop r28
                
            L%=_bottom:
            
                tst %[bottom]
                breq L%=finish
                
            L%=_bottom_loop:
            
                ; write one page from image to buf
                lpm %A[image_data], %a[image]+
                movw %[image_temp], %[image]
                movw %[image], %[mask]
                lpm %A[mask_data], %a[image]+
                movw %[mask], %[image]
                movw %[image], %[image_temp]

                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                mul %A[mask_data], %[shift_coef]
                movw %[mask_data], r0
                
                ld %[buf_data], %a[buf]
                com %A[mask_data]
                and %[buf_data], %A[mask_data]
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                dec %[n]
                brne L%=_bottom_loop
            
            L%=finish:
            
                clr __zero_reg__
                
            )ASM"
            :
            [buf]        "+&x" (buf),
            [image]      "+&z" (image),
            [mask]       "+&l" (mask),
            [pages]      "+&l" (pages),
            [count]      "=&l" (count),
            [buf_data]   "=&l" (buf_data),
            [image_data] "=&l" (image_data),
            [mask_data]  "=&l" (mask_data),
            [image_temp] "=&a" (image_temp)
            :
            [n]          "l"   (n),
            [buf_adv]    "l"   (buf_adv),
            [image_adv]  "l"   (image_adv),
            [shift_coef] "l"   (shift_coef),
            [bottom]     "l"   (bottom),
            [page_start] "a"   (page_start)
            );
    }
}

template void draw_sprite<SpriteMode::Overwrite>(
    int16_t x, int16_t y,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame);
//template void draw_sprite<SpriteMode::PlusMask>(
//    int16_t x, int16_t y,
//    uint8_t const* image, uint8_t frame,
//    uint8_t const* mask, uint8_t mask_frame);
template void draw_sprite<SpriteMode::ExternalMask>(
    int16_t x, int16_t y,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame);

}

#if defined(ABG_TIMER3)
ISR(TIMER3_COMPA_vect)
{
    using namespace abg_detail;
#if defined(ABG_SYNC_THREE_PHASE)
    if(++current_phase >= 4)
        current_phase = 1;
    if(current_phase == 1)
        OCR3A = (timer_counter >> 4) + 1; // phase 2 delay: 4 lines
    else if(current_phase == 2)
        OCR3A = timer_counter;            // phase 3 delay: 64 lines
    else if(current_phase == 3)
        OCR3A = (timer_counter >> 4) + 1; // phase 1 delay: 4 lines
#elif defined(ABG_SYNC_PARK_ROW) || defined(ABG_SYNC_SLOW_DRIVE)
    OCR3A = timer_counter;
#endif
    needs_display = true;
}
#elif defined(ABG_TIMER4)
ISR(TIMER4_OVF_vect)
{
    using namespace abg_detail;
#if defined(ABG_SYNC_THREE_PHASE)
    if(++current_phase >= 4)
        current_phase = 1;
    uint16_t top = 0;
    if(current_phase == 1)
        top = (timer_counter >> 4) + 1; // phase 2 delay: 4 lines
    else if(current_phase == 2)
        top = timer_counter;            // phase 3 delay: 64 lines
    else if(current_phase == 3)
        top = (timer_counter >> 4) + 1; // phase 1 delay: 4 lines
    TC4H = (top >> 8);
    OCR4C = top;
#elif defined(ABG_SYNC_PARK_ROW) || defined(ABG_SYNC_SLOW_DRIVE)
    TC4H = (timer_counter >> 8);
    OCR4C = timer_counter;
#endif
    needs_display = true;
}
#endif

#endif // ABG_IMPLEMENTATION
