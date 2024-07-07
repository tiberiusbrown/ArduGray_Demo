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

    When using L4_Triplane, you can define one of the following macros to
    convert to L3 while retaining the plane behavior of L4_Triplane:
    - ABG_L3_CONVERT_LIGHTEN
        Convert light gray to white and dark gray to gray
    - ABG_L3_CONVERT_MIX
        Convert both light gray and dark gray to gray
    - ABG_L3_CONVERT_DARKEN
        Convert light gray to gray and dark gray to black
    
    When using L3 or L4_Triplane, allow per-plane contrast adjustment,
    which can improve contrast between different shades of gray but
    usually makes the whole image darker.
    - ABG_PLANE_CONTRAST

Default Template Configuration:
    
    ArduboyGBase a;
    ArduboyG     a;

    Both are configured with:
        ABG_Mode::L3
        ABG_Flags::None

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

#if !defined(ABG_UPDATE_EVERY_N_DEFAULT)
#define ABG_UPDATE_EVERY_N_DEFAULT 1
#endif
#if !defined(ABG_UPDATE_EVERY_N_DENOM_DEFAULT)
#define ABG_UPDATE_EVERY_N_DENOM_DEFAULT 1
#endif

#if !defined(ABG_REFRESH_HZ)
#if defined(OLED_SH1106)
#define ABG_REFRESH_HZ 120
#else
#define ABG_REFRESH_HZ 156
#endif
#endif

#if defined(ABG_UPDATE_HZ_DEFAULT)
#undef ABG_UPDATE_EVERY_N_DEFAULT
#undef ABG_UPDATE_EVERY_N_DENOM_DEFAULT
#define ABG_UPDATE_EVERY_N_DEFAULT (ABG_REFRESH_HZ / 3)
#define ABG_UPDATE_EVERY_N_DENOM_DEFAULT ABG_UPDATE_HZ_DEFAULT
#endif

#if !defined(ABG_CONTRAST_DEFAULT)
#define ABG_CONTRAST_DEFAULT 255
#endif
#if !defined(ABG_PRECHARGE_CYCLES)
#define ABG_PRECHARGE_CYCLES 1
#endif
#if !defined(ABG_DISCHARGE_CYCLES)
#define ABG_DISCHARGE_CYCLES 2
#endif

#if defined(OLED_SH1106) && !defined(ABG_NO_L3_CONVERSION) && !defined(ABG_L3_CONVERT_LIGHTEN) && !defined(ABG_L3_CONVERT_MIX) && !defined(ABG_L3_CONVERT_DARKEN)
#define ABG_L3_CONVERT_MIX
#endif

#if defined(ABG_L3_CONVERT_LIGHTEN)
#undef ABG_L3_CONVERT_MIX
#undef ABG_L3_CONVERT_DARKEN
#endif
#if defined(ABG_L3_CONVERT_MIX)
#undef ABG_L3_CONVERT_LIGHTEN
#undef ABG_L3_CONVERT_DARKEN
#endif
#if defined(ABG_L3_CONVERT_DARKEN)
#undef ABG_L3_CONVERT_LIGHTEN
#undef ABG_L3_CONVERT_MIX
#endif

#if defined(ABG_L3_CONVERT_LIGHTEN) || defined(ABG_L3_CONVERT_MIX) || defined(ABG_L3_CONVERT_DARKEN)
#define ABG_L4_TRIPLANE_PLANE_LIMIT 2
#else
#define ABG_L4_TRIPLANE_PLANE_LIMIT 3
#endif

#if defined(ABG_PLANE_CONTRAST)
// nothing yet
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
    
    Default = L3,
};

struct ABG_Flags
{
    enum
    {
        None = 0,
        
        Default =
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

static constexpr uint8_t num_planes(ABG_Mode mode)
{
    return
        mode == ABG_Mode::L4_Contrast ? 2 :
        mode == ABG_Mode::L4_Triplane ? 3 :
        mode == ABG_Mode::L3          ? 2 :
        1;
}

#if defined(ABG_TIMER3) || defined(ABG_TIMER1)
constexpr uint16_t timer_counter = F_CPU / 64 / ABG_REFRESH_HZ;
#elif defined(ABG_TIMER4)
constexpr uint16_t timer_counter = F_CPU / 256 / ABG_REFRESH_HZ;
#endif
extern uint8_t  contrast;
extern uint8_t  plane_contrast_L4[3];
extern uint8_t  plane_contrast_L3[2];
extern uint8_t  update_counter;
extern uint8_t  update_every_n;
extern uint8_t  update_every_n_denom;
extern uint8_t  current_plane;
#if defined(ABG_SYNC_THREE_PHASE)
extern uint8_t volatile current_phase;
#endif
extern bool volatile needs_display;

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
            0xD9, ((ABG_PRECHARGE_CYCLES) | ((ABG_DISCHARGE_CYCLES) << 4)),
#if defined(OLED_SH1106)
            0xD5, 0xF0, // clock divider (not set in homemade package)
#endif
#if defined(ABG_SYNC_PARK_ROW) || defined(ABG_SYNC_SLOW_DRIVE)
            0x81, 255,  // default contrast
#endif
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
    static void setContrast(uint8_t f)
    {
        // have compiler optimize out assignment if it's not needed
        if(MODE == ABG_Mode::L4_Contrast)
            contrast = f;
    }
    
    static void setUpdateEveryN(uint8_t num, uint8_t denom = 1)
    {
        update_every_n = num;
        update_every_n_denom = denom;
        if(update_counter >= num)
            update_counter = 0;
    }
    
    static void setUpdateHz(uint8_t hz)
    {
        if(hz > ABG_REFRESH_HZ) hz = ABG_REFRESH_HZ;
        setUpdateEveryN(ABG_REFRESH_HZ / num_planes(MODE), hz);
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
        if(MODE == ABG_Mode::L4_Triplane)
        {
#if defined(ABG_L3_CONVERT_LIGHTEN)
            return current_plane;
#elif defined(ABG_L3_CONVERT_MIX)
            return current_plane << 1;
#elif defined(ABG_L3_CONVERT_DARKEN)
            return current_plane + 1;
#endif
        }
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
    
    static void waitForNextPlane(uint8_t clear = BLACK)
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
            doDisplay(clear);
        }
#if defined(ABG_SYNC_THREE_PHASE)
        while(current_phase != 3);
#elif defined(ABG_SYNC_PARK_ROW) || defined(ABG_SYNC_SLOW_DRIVE)
        while(0);
#endif
    }
        
    ABG_NOT_SUPPORTED static void flipVertical();
    ABG_NOT_SUPPORTED static void paint8Pixels(uint8_t);
    ABG_NOT_SUPPORTED static void paintScreen(uint8_t const*);
    ABG_NOT_SUPPORTED static void paintScreen(uint8_t[], bool);
    ABG_NOT_SUPPORTED static void setFrameDuration(uint8_t);
    ABG_NOT_SUPPORTED static void setFrameRate(uint8_t);
    ABG_NOT_SUPPORTED static void display();
    ABG_NOT_SUPPORTED static void display(bool);
    ABG_NOT_SUPPORTED static bool nextFrame();
    ABG_NOT_SUPPORTED static bool nextFrameDEV();

#if defined(ABG_TIMER1)
#define ABG_NO_PWM_LED __attribute__((error("This method cannot be used while using ABG_TIMER1.")))
    ABG_NO_PWM_LED static void setRGBled(uint8_t, uint8_t);
    ABG_NO_PWM_LED static void setRGBled(uint8_t, uint8_t, uint8_t);
#endif

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
    
    static void doDisplay(uint8_t clear)
    {
        uint8_t* b = getBuffer();
        
        uint16_t clearcfg = 1;
        {
            uint8_t p = current_plane;
#if defined(ABG_PLANE_CONTRAST)
            if(MODE == ABG_Mode::L3)
            {
                uint8_t contrast = plane_contrast_L3[0];
                if(p & 1) contrast = plane_contrast_L3[1];
                Arduboy2Base::LCDCommandMode();
                Arduboy2Base::SPItransfer(0x81);
                Arduboy2Base::SPItransfer(contrast);
                Arduboy2Base::LCDDataMode();
            }
            if(MODE == ABG_Mode::L4_Triplane)
            {
                uint8_t contrast = plane_contrast_L4[0];
                if(p & 1) contrast = plane_contrast_L4[1];
                if(p & 2) contrast = plane_contrast_L4[2];
                Arduboy2Base::LCDCommandMode();
                Arduboy2Base::SPItransfer(0x81);
                Arduboy2Base::SPItransfer(contrast);
                Arduboy2Base::LCDDataMode();
            }
#endif
            p += 1;
            if(p >= num_planes(MODE)) p = 0;
            if(planeColor(p, clear) != 0)
                clearcfg |= 0xff00;
        }
                
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
            paint(&b[128 * 7], 0, 0x0001, 0xf0);
            send_cmds_prog<0x22, 0, 7>();
        }
        else if(phase == 3)
        {
            send_cmds_prog<0x22, 0, 7>();
            paint(&b[128 * 7], 0, 0x0001, 0xff);
            send_cmds_prog<0xA8, 0>();
            paint(&b[128 * 0], clearcfg, 0x0107, 0xff);
            paint(&b[128 * 7], clearcfg, 0x0001, 0x00);

            if(MODE == ABG_Mode::L4_Triplane)
            {
                if(++current_plane >= ABG_L4_TRIPLANE_PLANE_LIMIT)
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
        paint(&b[128 * 7], clearcfg, 0x0001, 0x7f);
        send_cmds_prog<0xA8, 63>();
        paint(&b[128 * 0], clearcfg, 0x0107, 0xff);
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
            paint(&b[128 * 7], 0, 0x0001, 0xff);
            send_cmds_prog<
                0xA8, 63, 0x8D, 0x14, 0xD9, 0x31, 0xD5, 0xF0>();
            SREG = sreg;
        }
        paint(&b[128 * 0], clearcfg, 0x0107, 0xff);
        send_cmds_prog<0xA8, 0>();
        paint(&b[128 * 7], clearcfg, 0x0001, 0x00);
#endif
        uint8_t cp = current_plane;
        if(MODE == ABG_Mode::L4_Triplane)
        {
            if(++cp >= ABG_L4_TRIPLANE_PLANE_LIMIT)
                cp = 0;
        }
        else
            cp = !cp;
        if(cp == 0)
            update_counter += update_every_n_denom;
        current_plane = cp;
#endif
    }
    
    // clear: low byte is whether to clear, high byte is clear color
    // pages: low byte is page count, high byte is starting page (which is unused for SSD1306)
    __attribute__((naked, noinline))
    static void paint(uint8_t* image, uint16_t clear, uint16_t pages, uint8_t mask)
    {
        // image: r24:r25
        // clear: r22:r23
        // pages: r20:r21
        // mask : r18
#if defined(OLED_SH1106) || defined(LCD_ST7565)
        asm volatile(
        
            R"ASM(
                 
                ; set buffer pointer to end of buffer pages
                movw r26, r24
                ldi  r19, 128
                mul  r20, r19
                movw r24, r0
                clr  __zero_reg__
                add  r26, r24
                adc  r27, r25
                
                ; add OLED_SET_PAGE_ADDRESS
                subi r21, -(%[page_cmd])
           
                ; outer loop
            1:  ldi  r19, %[DORD2]
                out  %[spcr], r19
                cbi  %[dc_port], %[dc_bit]
                out  %[spdr], r21 ; set page
                rcall 3f
                rcall 3f
                rjmp .+0
                ldi  r24, %[col_cmd]
                out  %[spdr], r24 ; set column hi
                rcall 3f
                rcall 3f
                rcall 3f
                sbi  %[dc_port], %[dc_bit]
                ldi  r19, %[DORD1]
                out  %[spcr], r19
                ldi  r24, 128
           
                ; main loop: send buffer in reverse direction, masking bytes
            2:  ld   __tmp_reg__, -X
                mov  r19, __tmp_reg__
                cpse r22, __zero_reg__
                mov  r19, r23
                st   X, r19
                lpm  r19, Z
                lpm  r19, Z
                and  __tmp_reg__, r18
                out  %[spdr], __tmp_reg__
                dec  r24
                brne 2b
                
                rcall 3f
                rcall 3f
                inc  r21
                dec  r20
                brne 1b
                                
                ; delay for final byte, then reset SPCR DORD and clear SPIF
                rcall 3f
                rcall 3f
                ldi  r19, %[DORD2]
                in   __tmp_reg__, %[spsr]
                out  %[spcr], r19
            3:  ret
            
            )ASM"
            
            :
            : [spdr]     "I"   (_SFR_IO_ADDR(SPDR)),
              [spsr]     "I"   (_SFR_IO_ADDR(SPSR)),
              [spcr]     "I"   (_SFR_IO_ADDR(SPCR)),
              [page_cmd] "M"   (OLED_SET_PAGE_ADDRESS),
              [col_cmd]  "M"   (OLED_SET_COLUMN_ADDRESS_HI),
              [DORD1]    "i"   (_BV(SPE) | _BV(MSTR) | _BV(DORD)),
              [DORD2]    "i"   (_BV(SPE) | _BV(MSTR)),
              [dc_port]  "I"   (_SFR_IO_ADDR(DC_PORT)),
              [dc_bit]   "I"   (DC_BIT)
            );
#else
        asm volatile(
        
            R"ASM(
        
                ; set SPCR DORD to MSB-to-LSB order
                ldi  r19, %[DORD1]
                out  %[spcr], r19
                
                ; init counter and set buffer pointer to end of buffer pages
                movw r26, r24
                ldi  r19, 128
                mul  r20, r19
                movw r24, r0
                clr  __zero_reg__
                add  r26, r24
                adc  r27, r25
        
                ; main loop: send buffer in reverse direction, masking bytes
            1:  ld   r21, -X
                mov  r19, r21
                cpse r22, __zero_reg__
                mov  r19, r23
                st   X, r19
                lpm
                rjmp .+0
                and  r21, r18
                out  %[spdr], r21
                sbiw r24, 1
                brne 1b
                
                ; delay for final byte, then reset SPCR DORD and clear SPIF
                rcall 2f
                rcall 2f
                ldi  r19, %[DORD2]
                in   __tmp_reg__, %[spsr]
                out  %[spcr], r19
            2:  ret
        
            )ASM"
            
            :
            : [spdr]    "I"   (_SFR_IO_ADDR(SPDR)),
              [spsr]    "I"   (_SFR_IO_ADDR(SPSR)),
              [spcr]    "I"   (_SFR_IO_ADDR(SPCR)),
              [DORD1]   "i"   (_BV(SPE) | _BV(MSTR) | _BV(DORD)),
              [DORD2]   "i"   (_BV(SPE) | _BV(MSTR))
            );
#endif
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

uint8_t  update_counter;
uint8_t  update_every_n = ABG_UPDATE_EVERY_N_DEFAULT;
uint8_t  update_every_n_denom = ABG_UPDATE_EVERY_N_DENOM_DEFAULT;
uint8_t  current_plane;
#if defined(ABG_SYNC_THREE_PHASE)
uint8_t volatile current_phase;
#endif
bool volatile needs_display;
uint8_t  contrast = ABG_CONTRAST_DEFAULT;
uint8_t  plane_contrast_L4[3] = { 25, 85, 255 };
uint8_t  plane_contrast_L3[2] = { 64, 255 };

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
