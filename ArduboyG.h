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

    Timer used for the frame ISR. Choices are one of:
    - ABG_TIMER1
    - ABG_TIMER3 (default)
    - ABG_TIMER4

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
        a.waitForNextPlane();
        if(a.needsUpdate())
            update();
        render();
    }

*/

#pragma once

#include <Arduboy2.h>

#include <avr/sleep.h>

#if !defined(ABG_SYNC_THREE_PHASE) && \
    !defined(ABG_SYNC_PARK_ROW) && \
    !defined(ABG_SYNC_SLOW_DRIVE)
#define ABG_SYNC_THREE_PHASE
#endif

#if !defined(ABG_TIMER3) && \
    !defined(ABG_TIMER4) && \
    !defined(ABG_TIMER1)
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

#if !defined(ABG_PRECHARGE_CYCLES)
#define ABG_PRECHARGE_CYCLES 3
#endif
#if !defined(ABG_DISCHARGE_CYCLES)
#define ABG_DISCHARGE_CYCLES 1
#endif

namespace abg_detail
{
    
extern uint8_t  contrast;
extern uint16_t timer_counter;
extern uint8_t  update_counter;
extern uint8_t  update_every_n;
extern uint8_t  update_every_n_denom;
extern uint8_t  current_plane;
#if defined(ABG_SYNC_THREE_PHASE)
extern uint8_t volatile current_phase;
#endif
extern bool volatile needs_display; // needs display work

template<class T>
inline uint8_t pgm_read_byte_inc(T const*& p)
{
    uint8_t r;
    asm volatile("lpm %[r], %a[p]+\n" : [p] "+&z" (p), [r] "=&r" (r));
    return r;
}
template<class T>
inline uint8_t deref_inc(T const*& p)
{
    uint8_t r;
    asm volatile("ld %[r], %a[p]+\n" : [p] "+&e" (p), [r] "=&r" (r) :: "memory");
    return r;
}

void send_cmds_(uint8_t const* d, uint8_t n);
void send_cmds_prog_(uint8_t const* d, uint8_t n);

template<uint8_t... CMDS> void send_cmds_prog()
{
    static uint8_t const CMDS_[] PROGMEM = { CMDS... };
    send_cmds_prog_(CMDS_, sizeof(CMDS_));
}

inline void send_cmds_helper() {}

template<class... CMDS>
inline void send_cmds_helper(uint8_t first, CMDS... rest)
{
    Arduboy2Base::SPItransfer(first);
    send_cmds_helper(rest...);
}

template<class... CMDS> inline void send_cmds(CMDS... cmds)
{
#if 1
    uint8_t const CMDS_[] = { cmds... };
    send_cmds_(CMDS_, sizeof(CMDS_));
#else
    Arduboy2Base::LCDCommandMode();
    send_cmds_helper(cmds...);
    Arduboy2Base::LCDDataMode();
#endif
}

extern uint8_t const YMASK0[8] PROGMEM;
extern uint8_t const YMASK1[8] PROGMEM;

#ifdef ABG_FAST_RECT_STATIC_DISPATCH
template<bool CLEAR>
void fast_rect(int16_t x, int16_t y, uint8_t w, uint8_t h);
#else
void fast_rect(int16_t x, int16_t y, uint8_t w, uint8_t h, bool clear);
#endif

enum class SpriteMode : uint8_t
{
    Overwrite,
    PlusMask,
    ExternalMask,
    OverwriteFX,
    PlusMaskFX,
};

template<SpriteMode SPRITE_MODE> void draw_sprite(
    int16_t x, int16_t y,
    uint8_t w, uint8_t h,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame);

template<SpriteMode SPRITE_MODE> void draw_sprite_dispatch(
    int16_t x, int16_t y,
    uint8_t w, uint8_t h,
    uint8_t pages,
    uint8_t const* image, uint8_t const* mask);

template<SpriteMode SPRITE_MODE> inline void draw_sprite(
    int16_t x, int16_t y,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame)
{
    uint8_t w, h;
    asm volatile(
        "lpm %[w], Z+\n"
        "lpm %[h], Z+\n"
        : [w] "=r" (w), [h] "=r" (h), [image] "+z" (image));
    draw_sprite<SPRITE_MODE>(x, y, w, h, image, frame, mask, mask_frame);
}

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
            0xD9, (ABG_PRECHARGE_CYCLES | (ABG_DISCHARGE_CYCLES << 4)),
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
#elif defined(ABG_TIMER1)
        // Fast PWM mode, prescaler /64
        OCR1A = timer_counter;
        TCCR1A = _BV(WGM11) | _BV(WGM10);
        TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11) | _BV(CS10);
        TCNT1 = 0;
        bitWrite(TIMSK1, OCIE1A, 1);
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
    
    // use this method to adjust contrast when using ABGMode::L4_Contrast
    static void setContrast(uint8_t f) { contrast = f; }
    
    static void setUpdateEveryN(uint8_t num, uint8_t denom = 1)
    {
        update_every_n = num;
        update_every_n_denom = denom;
        if(update_counter >= update_every_n)
            update_counter = 0;
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
        if(FLAGS & ABG_Flags::OptimizeFillRect)
        {
            color = planeColor(current_plane, color);
#ifdef ABG_FAST_RECT_STATIC_DISPATCH
            if(color) fast_rect<false>(x, y, w, 1);
            else      fast_rect<true >(x, y, w, 1);
#else
            fast_rect(x, y, w, 1, color == BLACK);
#endif
        }
        else Arduboy2Base::drawFastHLine(x, y, w, planeColor(current_plane, color));
    }
    
    template<uint8_t PLANE>
    static void drawFastHLine(
        int16_t x, int16_t y,
        uint8_t w,
        uint8_t color = WHITE)
    {
        if(FLAGS & ABG_Flags::OptimizeFillRect)
        {
            color = planeColor<PLANE>(color);
#ifdef ABG_FAST_RECT_STATIC_DISPATCH
            if(color) fast_rect<false>(x, y, w, 1);
            else      fast_rect<true >(x, y, w, 1);
#else
            fast_rect(x, y, w, 1, color == BLACK);
#endif
        }
        else Arduboy2Base::drawFastHLine(x, y, w, planeColor<PLANE>(color));
    }
    
    static void drawFastVLine(
        int16_t x, int16_t y,
        uint8_t h,
        uint8_t color = WHITE)
    {
        if(FLAGS & ABG_Flags::OptimizeFillRect)
        {
            color = planeColor(current_plane, color);
#ifdef ABG_FAST_RECT_STATIC_DISPATCH
            if(color) fast_rect<false>(x, y, 1, h);
            else      fast_rect<true >(x, y, 1, h);
#else
            fast_rect(x, y, 1, h, color == BLACK);
#endif
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
#ifdef ABG_FAST_RECT_STATIC_DISPATCH
            if(color) fast_rect<false>(x, y, 1, h);
            else      fast_rect<true >(x, y, 1, h);
#else
            fast_rect(x, y, 1, h, color == BLACK);
#endif
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
#ifdef ABG_FAST_RECT_STATIC_DISPATCH
            if(color) fast_rect<false>(x, y, w, h);
            else      fast_rect<true >(x, y, w, h);
#else
            fast_rect(x, y, w, h, color == BLACK);
#endif
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
#ifdef ABG_FAST_RECT_STATIC_DISPATCH
            if(color) fast_rect<false>(x, y, w, h);
            else      fast_rect<true >(x, y, w, h);
#else
            fast_rect(x, y, w, h, color == BLACK);
#endif
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
    
    void drawOverwriteMonochrome(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t const* bitmap)
    {
        static_assert(FLAGS & ABG_Flags::OptimizeDrawOverwrite,
            "drawOverwriteMonochrome requires ABG_Flags::OptimizeDrawOverwrite");
        draw_sprite_dispatch<SpriteMode::Overwrite>(x, y, w, h, (h + 7) / 8, bitmap, nullptr);
    }
    
    void drawPlusMask(
        int16_t x, int16_t y,
        uint8_t const* bitmap,
        uint8_t frame)
    {
        static_assert(MODE != ABG_Mode::L4_Triplane,
            "drawOverwrite does not support L4_Triplane mode");
        draw_sprite<SpriteMode::PlusMask>(x, y, bitmap, frame, nullptr, 0);
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
    
    void drawExternalMask(
        int16_t x, int16_t y,
        uint8_t w, uint8_t h,
        uint8_t const* bitmap, uint8_t const* mask)
    {
        static_assert(MODE != ABG_Mode::L4_Triplane,
            "drawExternalMask does not support L4_Triplane mode");
        static_assert(FLAGS & ABG_Flags::OptimizeDrawOverwrite,
            "Sized drawExternalMask requires ABG_Flags::OptimizeDrawOverwrite");
        draw_sprite<SpriteMode::ExternalMask>(x, y, w, h, bitmap, 0, mask, 0);
    }

    static uint8_t currentPlane()
    {
        return current_plane;
    }
    
    static bool needsUpdate()
    {
        if(update_counter >= update_every_n)
        {
            update_counter -= update_every_n;
            return true;
        }
        return false;
    }
    
    static void waitForNextPlane()
    {
        do
        {
            cli();
            for(;;)
            {
                if(needs_display)
                    break;
                sleep_enable();
                sei();
                sleep_cpu();
                sleep_disable();
                cli();
            }
            needs_display = false;
            sei();
            doDisplay();
        }
#if defined(ABG_SYNC_THREE_PHASE)
        while(current_phase != 3);
#elif defined(ABG_SYNC_PARK_ROW) || defined(ABG_SYNC_SLOW_DRIVE)
        while(0);
#endif
    }
    
    static bool nextFrame()
    {
        waitForNextPlane();
        return true;
    }
    static bool nextFrameDEV()
    {
        waitForNextPlane();
        return true;
    }
        
    ABG_NOT_SUPPORTED static void flipVertical();
    ABG_NOT_SUPPORTED static void paint8Pixels(uint8_t);
    ABG_NOT_SUPPORTED static void paintScreen(uint8_t const*);
    ABG_NOT_SUPPORTED static void paintScreen(uint8_t[], bool);
    ABG_NOT_SUPPORTED static void setFrameDuration(uint8_t);
    ABG_NOT_SUPPORTED static void setFrameRate(uint8_t);
    ABG_NOT_SUPPORTED static void display();
    ABG_NOT_SUPPORTED static void display(bool);

    // expose internal Arduboy2Core methods
    static void setCPUSpeed8MHz() { BASE::setCPUSpeed8MHz(); }
    static void bootSPI        () { BASE::bootSPI        (); }
    static void bootOLED       () { BASE::bootOLED       (); }
    static void bootPins       () { BASE::bootPins       (); }
    static void bootPowerSaving() { BASE::bootPowerSaving(); }
    
    // color conversion method
    static uint8_t color (uint8_t c) { return planeColor(current_plane, c); }
    static uint8_t colour(uint8_t c) { return planeColor(current_plane, c); }
    
protected:
    
    static void doDisplay()
    {
        uint8_t* b = getBuffer();
        
#if defined(ABG_SYNC_THREE_PHASE)
        uint8_t phase = current_phase;
        if(phase == 1)
        {
            if(MODE == ABG_Mode::L4_Contrast)
                send_cmds(0x81, (current_plane & 1) ? contrast : contrast / 2);
            send_cmds_prog<0xA8, 7, 0x22, 0, 7>();
        }
        else if(phase == 2)
        {
            paint(&b[128 * 7], false, 1, 0xf0);
            send_cmds_prog<0x22, 0, 7>();
        }
        else if(phase == 3)
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
                update_counter += update_every_n_denom;
        }
#elif defined(ABG_SYNC_PARK_ROW) || defined(ABG_SYNC_SLOW_DRIVE)
        if(MODE == ABG_Mode::L4_Contrast)
            send_cmds(0x81, (current_plane & 1) ? contrast : contrast / 2);
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
            update_counter += update_every_n_denom;
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
            "   sbiw  %A[count], 0              ;2        \n\t" //[delay]
            "   sbiw  %A[count], 0              ;2        \n\t" //[delay]
            "   and   __tmp_reg__, %[mask]      ;1        \n\t" //tmp &= mask
            "   out   %[spdr], __tmp_reg__      ;1        \n\t" //SPDR = tmp
            "   sbiw  %A[count], 1              ;2        \n\t" //len--
            "   brne  1b                        ;1/2 :18  \n\t" //len > 0
            "   in    __tmp_reg__, %[spsr]                \n\t" //read SPSR to clear SPIF
            "   sbiw  %A[count], 0                        \n\t"
            "   sbiw  %A[count], 0                        \n\t" // delay before resetting DORD
            "   sbiw  %A[count], 0                        \n\t" // below so it doesn't mess up
            "   sbiw  %A[count], 0                        \n\t" // the last transfer
            "   sbiw  %A[count], 0                        \n\t"
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

#if !defined(ABG_UPDATE_EVERY_N_DEFAULT)
#define ABG_UPDATE_EVERY_N_DEFAULT 1
#endif
#if !defined(ABG_UPDATE_EVERY_N_DENOM_DEFAULT)
#define ABG_UPDATE_EVERY_N_DENOM_DEFAULT 1
#endif
#if !defined(ABG_FPS_DEFAULT)
#define ABG_FPS_DEFAULT 135
#endif
#if !defined(ABG_CONTRAST_DEFAULT)
#define ABG_CONTRAST_DEFAULT 255
#endif

#if defined(ABG_TIMER3) || defined(ABG_TIMER1)
uint16_t timer_counter = F_CPU / 64 / ABG_FPS_DEFAULT;
#elif defined(ABG_TIMER4)
uint16_t timer_counter = F_CPU / 256 / ABG_FPS_DEFAULT;
#endif
uint8_t  update_counter;
uint8_t  update_every_n = ABG_UPDATE_EVERY_N_DEFAULT;
uint8_t  update_every_n_denom = ABG_UPDATE_EVERY_N_DENOM_DEFAULT;
uint8_t  current_plane;
#if defined(ABG_SYNC_THREE_PHASE)
uint8_t volatile current_phase;
#endif
bool volatile needs_display;
uint8_t contrast = ABG_CONTRAST_DEFAULT;

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
    do Arduboy2Base::SPItransfer(pgm_read_byte_inc(d));
    while(--n != 0);
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

#ifdef ABG_FAST_RECT_STATIC_DISPATCH
template<bool CLEAR>
void fast_rect(int16_t x, int16_t y, uint8_t w, uint8_t h)
#else
void fast_rect(int16_t x, int16_t y, uint8_t w, uint8_t h, bool CLEAR)
#endif
{
    if(y >=  64) return;
    if(x >= 128) return;
    if((w & h) == 0) return;
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
    if(x0 + w > 128)
        w = 128 - x0;

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

#ifdef ABG_FAST_RECT_STATIC_DISPATCH
template void fast_rect<true >(int16_t x, int16_t y, uint8_t w, uint8_t h);
template void fast_rect<false>(int16_t x, int16_t y, uint8_t w, uint8_t h);
#endif

template<SpriteMode SPRITE_MODE> void draw_sprite(
    int16_t x, int16_t y,
    uint8_t w, uint8_t h,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame)
{
    if(x + w <= 0) return;
    if(y + h <= 0) return;
    if(x >= 128 || y >= 64) return;
    if(image == nullptr) return;
    if(SPRITE_MODE == SpriteMode::ExternalMask && mask == nullptr) return;

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
        if(SPRITE_MODE == SpriteMode::PlusMask)
            image += plane_bytes * frame * 3;
    }
    
    draw_sprite_dispatch<SPRITE_MODE>(x, y, w, h, pages, image, mask);
}

template<SpriteMode SPRITE_MODE> void draw_sprite_dispatch(
    int16_t x, int16_t y,
    uint8_t w, uint8_t h,
    uint8_t pages,
    uint8_t const* image, uint8_t const* mask)
{
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
    
    if(SPRITE_MODE == SpriteMode::PlusMask)
        image_adv *= 3;
    
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
                mov %[count], %[n]
                
            L%=_top_loop:
            
                ; write one page from image to buf+128
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                ld %[buf_data], %a[buf]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], %B[image_data]
                st %a[buf]+, %[buf_data]
                dec %[count]
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
    
    if(SPRITE_MODE == SpriteMode::PlusMask)
    {
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
                mov %[count], %[n]
                
            L%=_top_loop:
                
                ; write one page from image to buf+128
                sbrc %[plane], 0
                adiw %[image], 1
                lpm %A[image_data], %a[image]+
                sbrs %[plane], 0
                adiw %[image], 1
                lpm %A[mask_data], %a[image]+
                
                mul %A[image_data], %[shift_coef]
                movw %[image_data], r0
                mul %A[mask_data], %[shift_coef]
                movw %[mask_data], r0
                
                ld %[buf_data], %a[buf]
                com %B[mask_data]
                and %[buf_data], %B[mask_data]
                or %[buf_data], %B[image_data]
                st %a[buf]+, %[buf_data]
                dec %[count]
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
                sbrc %[plane], 0
                adiw %[image], 1
                lpm %A[image_data], %a[image]+
                sbrs %[plane], 0
                adiw %[image], 1
                lpm %A[mask_data], %a[image]+
                
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
                sbrc %[plane], 0
                adiw %[image], 1
                lpm %A[image_data], %a[image]+
                sbrs %[plane], 0
                adiw %[image], 1
                lpm %A[mask_data], %a[image]+

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
            [pages]      "+&l" (pages),
            [count]      "=&l" (count),
            [buf_data]   "=&l" (buf_data),
            [image_data] "=&l" (image_data),
            [mask_data]  "=&l" (mask_data)
            :
            [n]          "l"   (n),
            [buf_adv]    "l"   (buf_adv),
            [image_adv]  "l"   (image_adv),
            [shift_coef] "l"   (shift_coef),
            [bottom]     "l"   (bottom),
            [page_start] "a"   (page_start),
            [plane]      "a"   (current_plane)
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
                mov %[count], %[n]
                
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
                dec %[count]
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
    uint8_t w, uint8_t h,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame);
template void draw_sprite<SpriteMode::PlusMask>(
    int16_t x, int16_t y,
    uint8_t w, uint8_t h,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame);
template void draw_sprite<SpriteMode::ExternalMask>(
    int16_t x, int16_t y,
    uint8_t w, uint8_t h,
    uint8_t const* image, uint8_t frame,
    uint8_t const* mask, uint8_t mask_frame);

template void draw_sprite_dispatch<SpriteMode::Overwrite>(
    int16_t x, int16_t y,
    uint8_t w, uint8_t h,
    uint8_t pages,
    uint8_t const* image, uint8_t const* mask);
template void draw_sprite_dispatch<SpriteMode::PlusMask>(
    int16_t x, int16_t y,
    uint8_t w, uint8_t h,
    uint8_t pages,
    uint8_t const* image, uint8_t const* mask);
template void draw_sprite_dispatch<SpriteMode::ExternalMask>(
    int16_t x, int16_t y,
    uint8_t w, uint8_t h,
    uint8_t pages,
    uint8_t const* image, uint8_t const* mask);

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

#elif defined(ABG_TIMER1)

ISR(TIMER1_COMPA_vect)
{
    using namespace abg_detail;
#if defined(ABG_SYNC_THREE_PHASE)
    if(++current_phase >= 4)
        current_phase = 1;
    if(current_phase == 1)
        OCR1A = (timer_counter >> 4) + 1; // phase 2 delay: 4 lines
    else if(current_phase == 2)
        OCR1A = timer_counter;            // phase 3 delay: 64 lines
    else if(current_phase == 3)
        OCR1A = (timer_counter >> 4) + 1; // phase 1 delay: 4 lines
#elif defined(ABG_SYNC_PARK_ROW) || defined(ABG_SYNC_SLOW_DRIVE)
    OCR1A = timer_counter;
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
