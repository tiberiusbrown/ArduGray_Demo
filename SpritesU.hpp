/*
Options

    SPRITESU_IMPLEMENTATION
    SPRITESU_OVERWRITE
    SPRITESU_PLUSMASK
    SPRITESU_FX
    SPRITESU_RECT
*/

#pragma once

#if defined(SPRITESU_FX)
#include <ArduboyFX.h>
#else
using uint24_t = __uint24;
#endif

struct SpritesU
{
#ifdef SPRITESU_OVERWRITE
    static void drawOverwrite(
        int16_t x, int16_t y, uint8_t const* image, uint16_t frame);
    static void drawOverwrite(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image);
#endif

#ifdef SPRITESU_OVERWRITE
    static void drawPlusMask(
        int16_t x, int16_t y, uint8_t const* image, uint16_t frame);
    static void drawPlusMask(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image);
#endif

#ifdef SPRITESU_FX
    static void drawOverwriteFX(
        int16_t x, int16_t y, uint24_t image, uint16_t frame);
    static void drawOverwriteFX(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame);
    static void drawPlusMaskFX(
        int16_t x, int16_t y, uint24_t image, uint16_t frame);
    static void drawPlusMaskFX(
        int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame);
#endif

#ifdef SPRITESU_RECT
    // color: zero for BLACK, 1 for WHITE
    static void fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color);
    static void fillRect_i8(int8_t x, int8_t y, uint8_t w, uint8_t h, uint8_t color);
#endif

    static constexpr uint8_t MODE_OVERWRITE   = 0;
    static constexpr uint8_t MODE_PLUSMASK    = 1;
    static constexpr uint8_t MODE_OVERWRITEFX = 2;
    static constexpr uint8_t MODE_PLUSMASKFX  = 3;

    static void drawBasic(
        int16_t x, int16_t y, uint8_t w, uint8_t h,
        uint24_t image, uint16_t frame, uint8_t mode);
    __attribute__((noinline)) static void drawBasicNoChecks(
        uint16_t w_and_h,
        uint24_t image, uint8_t mode,
        int16_t x, int16_t y);
};

#ifdef SPRITESU_IMPLEMENTATION

// from Mr. Blinky's ArduboyFX library
static __attribute__((always_inline)) uint8_t SpritesU_bitShiftLeftUInt8(uint8_t bit)
{
#ifdef ARDUINO_ARCH_AVR
    uint8_t result;
    asm volatile(
        "ldi    %[result], 1    \n" // 0 = 000 => 0000 0001
        "sbrc   %[bit], 1       \n" // 1 = 001 => 0000 0010
        "ldi    %[result], 4    \n" // 2 = 010 => 0000 0100
        "sbrc   %[bit], 0       \n" // 3 = 011 => 0000 1000
        "lsl    %[result]       \n"
        "sbrc   %[bit], 2       \n" // 4 = 100 => 0001 0000
        "swap   %[result]       \n" // 5 = 101 => 0010 0000
        :[result] "=&d" (result)    // 6 = 110 => 0100 0000
        :[bit]    "r"   (bit)       // 7 = 111 => 1000 0000
        :
    );
    return result;
#else
    return 1 << (bit & 7);
#endif
}

void SpritesU::drawBasic(
    int16_t x, int16_t y, uint8_t w, uint8_t h,
    uint24_t image, uint16_t frame, uint8_t mode)
{
    if(x >= 128) return;
    if(y >= 64)  return;
    if(x + w <= 0) return;
    if(y + h <= 0) return;
    
    uint16_t w_and_h = (uint16_t(h) << 8) | w;
    
#if 0
    if(frame != 0)
    {
        uint8_t tmp = h >> 3;
        if(mode & 1) tmp *= 2;
        uint16_t tmp2 = tmp * w;
        tmp2 = tmp2 * frame;
        image += tmp2;
    }
#else
    uint16_t tmp, tmp2;
    asm volatile(R"ASM(
            cp   %A[frame], __zero_reg__
            cpc  %B[frame], __zero_reg__
            breq 1f
            
            ; add frame offset to image
            lsr  %[h]
            lsr  %[h]
            lsr  %[h]
            sbrc %[mode], 0
            lsl  %A[h]
            mul  %A[h], %[w]
            movw %A[tmp], r0
            mul  %A[tmp], %A[frame]
            movw %A[tmp2], r0
            mul  %A[tmp], %B[frame]
            add  %B[tmp2], r0
            mul  %B[tmp], %A[frame]
            add  %B[tmp2], r0
            add  %A[image], %A[tmp2]
            adc  %B[image], %B[tmp2]
            clr  __zero_reg__
            adc  %C[image], __zero_reg__
        1:
        )ASM"
        :
        [h]     "+&r" (h),
        [image] "+&r" (image),
        [tmp]   "=&r" (tmp),
        [tmp2]  "=&r" (tmp2)
        :
        [frame] "r"   (frame),
        [mode]  "r"   (mode),
        [w]     "r"   (w)
        );
#endif

    drawBasicNoChecks(w_and_h, image, mode, x, y);
}

void SpritesU::drawBasicNoChecks(
    uint16_t w_and_h,
    uint24_t image, uint8_t mode,
    int16_t x, int16_t y)
{
    uint8_t w = uint8_t(w_and_h);
    uint8_t h = uint8_t(w_and_h >> 8);
    
    uint8_t pages = h;
    uint8_t shift_coef;
    uint16_t shift_mask;
    int8_t page_start;
    uint8_t cols;
    uint8_t col_start;
    bool bottom;
    uint8_t buf_adv;
    uint16_t image_adv;
    uint8_t* buf = Arduboy2Base::sBuffer;
    
#if 1
    asm volatile(R"ASM(
            mov  %[col_start], %A[x]
            clr  %[bottom]
            mov  %[cols], %[w]
    
            lsr  %[pages]
            lsr  %[pages]
            lsr  %[pages]
            
            ; precompute vertical shift coef and mask
            ldi  %[shift_coef], 1
            sbrc %A[y], 1
            ldi  %[shift_coef], 4
            sbrc %A[y], 0
            lsl  %[shift_coef]
            sbrc %A[y], 2
            swap %[shift_coef]
            ldi  %[buf_adv], 0xff
            mul  %[buf_adv], %[shift_coef]
            movw %A[shift_mask], r0
            com  %A[shift_mask]
            com  %B[shift_mask]
            
            asr  %B[y]
            ror  %A[y]
            asr  %B[y]
            ror  %A[y]
            asr  %B[y]
            ror  %A[y]
            
            ; clip against top edge
            mov  %[page_start], %A[y]
            cpi  %[page_start], 0xff
            brge 2f
            com  %[page_start]
            sub  %[pages], %[page_start]
            sbrc %[mode], 0
            lsl  %[page_start]
            mul  %[page_start], %[w]
            add  %A[image], r0
            adc  %B[image], r1
            adc  %C[image], %[bottom]
            ldi  %[page_start], 0xff
        2:
            ; clip against left edge
            sbrs %B[x], 7
            rjmp 4f
            add %[cols], %A[x]
            sbrs %[mode], 0
            rjmp 3f
            lsl  %A[x]
            rol  %B[x]
        3:
            sub  %A[image], %A[x]
            sbc  %B[image], %B[x]
            sbc  %C[image], %[bottom]
            sbrc %B[x], 7
            inc  %C[image]
            clr  %[col_start]
        4:
            ; compute buffer start address
            ldi  %[buf_adv], 128
            mulsu %[page_start], %[buf_adv]
            add  r0, %[col_start]
            add  %A[buf], r0
            adc  %B[buf], r1
            
            ; clip against right edge
            sub  %[buf_adv], %[col_start]
            cp   %[cols], %[buf_adv]
            brlo 5f
            mov  %[cols], %[buf_adv]
        5:
            ; clip against bottom edge
            ldi  %[buf_adv], 7
            sub  %[buf_adv], %[page_start]
            cp   %[buf_adv], %[pages]
            brge 6f
            mov  %[pages], %[buf_adv]
            inc  %[bottom]
        6:
            ldi  %[buf_adv], 128
            sub  %[buf_adv], %[cols]
            mov  %A[image_adv], %[w]
            clr  %B[image_adv]
            sbrc %[mode], 1
            rjmp 7f
            sub  %A[image_adv], %[cols]
            sbc  %B[image_adv], %B[image_adv]
        7:
            sbrs %[mode], 0
            rjmp 8f
            lsl  %A[image_adv]
            rol  %B[image_adv]
        8:
            clr __zero_reg__
        )ASM"
        :
        [pages]      "+&r" (pages),
        [shift_coef] "=&d" (shift_coef),
        [shift_mask] "=&r" (shift_mask),
        [page_start] "=&a" (page_start),
        [cols]       "=&r" (cols),
        [col_start]  "=&r" (col_start),
        [bottom]     "=&r" (bottom),
        [buf]        "+&r" (buf),
        [buf_adv]    "=&a" (buf_adv),
        [image_adv]  "=&r" (image_adv),
        [x]          "+&r" (x),
        [y]          "+&r" (y),
        [image]      "+&r" (image)
        :
        [mode]       "r"   (mode),
        [w]          "r"   (w)
        );
    
#else
    page_start = int8_t(y);
    col_start = uint8_t(x);
    bottom = false;
    cols = w;

    asm volatile(
        "lsr %[pages]\n"
        "lsr %[pages]\n"
        "lsr %[pages]\n"
        : [pages] "+&r" (pages));

    if(frame != 0)
    {
        shift_coef = pages;
        if(mode & 1) shift_coef *= 2;
        shift_mask = shift_coef * w;
        image_adv = shift_mask * frame;
        image += image_adv;
    }

    // precompute vertical shift coef and mask
    shift_coef = SpritesU_bitShiftLeftUInt8(y);
    shift_mask = ~(0xff * shift_coef);

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
    page_start = int8_t(y);
    if(page_start < -1)
    {
        uint8_t tp = (-1 - page_start);
        pages -= tp;
        if(mode & 1) tp *= 2;
        image += tp * w;
        page_start = -1;
    }

    // clip against left edge
    if(x < 0)
    {
        cols += x;
        if(mode & 1) x *= 2;
        image -= x;
        col_start = 0;
    }

    asm volatile(
        "mulsu %[page_start], %[c128]\n"
        "add %A[buf], r0\n"
        "adc %B[buf], r1\n"
        "clr __zero_reg__\n"
        "add %A[buf], %[col_start]\n"
        "adc %B[buf], __zero_reg__\n"
        :
        [buf]        "+&x" (buf)
        :
        [page_start] "a"   (page_start),
        [col_start]  "r"   (col_start),
        [c128]       "a"   ((uint8_t)128)
        );

    // clip against right edge
    {
        uint8_t max_cols = 128 - col_start;
        if(cols > max_cols)
            cols = max_cols;
    }

    // clip against bottom edge
    if(pages > uint8_t(7 - page_start))
    {
        pages = 7 - page_start;
        bottom = true;
    }

    buf_adv = 128 - cols;
    image_adv = w;
#ifdef SPRITESU_FX
    if(!(mode & 2))
#endif
        image_adv -= cols;
    if(mode & 1) image_adv *= 2;
#endif

    uint16_t image_data;
    uint16_t mask_data;
    uint8_t buf_data;
    uint8_t count;

#ifdef SPRITESU_OVERWRITE
    if(mode == MODE_OVERWRITE)
    {
        uint8_t const* image_ptr = (uint8_t const*)image;
        asm volatile(R"ASM(

                cpi %[page_start], 0
                brge L%=_middle

                ; advance buf to next page
                subi %A[buf], lo8(-128)
                sbci %B[buf], hi8(-128)
                mov %[count], %[cols]

            L%=_top_loop:

                ; write one page from image to buf+128
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], r1
                st %a[buf]+, %[buf_data]
                dec %[count]
                brne L%=_top_loop

                ; decrement pages, reset buf back, advance image
                clr __zero_reg__
                dec %[pages]
                sub %A[buf], %[cols]
                sbc %B[buf], __zero_reg__
                add %A[image], %A[image_adv]
                adc %B[image], %B[image_adv]

            L%=_middle:

                tst %[pages]
                breq L%=_bottom

                ; need Y pointer for middle pages
                ;push r28
                ;push r29
                movw r28, %[buf]
                subi r28, lo8(-128)
                sbci r29, hi8(-128)

            L%=_middle_loop_outer:

                mov %[count], %[cols]

            L%=_middle_loop_inner:

                ; write one page from image to buf/buf+128
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], r0
                st %a[buf]+, %[buf_data]
                ld %[buf_data], Y
                and %[buf_data], %B[shift_mask]
                or %[buf_data], r1
                st Y+, %[buf_data]
                dec %[count]
                brne L%=_middle_loop_inner

                ; advance buf, buf+128, and image to the next page
                clr __zero_reg__
                add %A[buf], %[buf_adv]
                adc %B[buf], __zero_reg__
                add r28, %[buf_adv]
                adc r29, __zero_reg__
                add %A[image], %A[image_adv]
                adc %B[image], %B[image_adv]
                dec %[pages]
                brne L%=_middle_loop_outer

                ; done with Y pointer
                ;pop r29
                ;pop r28

            L%=_bottom:

                tst %[bottom]
                breq L%=_finish

            L%=_bottom_loop:

                ; write one page from image to buf
                lpm %A[image_data], %a[image]+
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], r0
                st %a[buf]+, %[buf_data]
                dec %[cols]
                brne L%=_bottom_loop

            L%=_finish:

                clr __zero_reg__

            )ASM"
            :
            [buf]        "+&x" (buf),
            [image]      "+&z" (image_ptr),
            [pages]      "+&r" (pages),
            [count]      "=&r" (count),
            [buf_data]   "=&r" (buf_data),
            [image_data] "=&r" (image_data)
            :
            [cols]       "r"   (cols),
            [buf_adv]    "r"   (buf_adv),
            [image_adv]  "r"   (image_adv),
            [shift_mask] "r"   (shift_mask),
            [shift_coef] "r"   (shift_coef),
            [bottom]     "r"   (bottom),
            [page_start] "r"   (page_start)
            :
            "r28", "r29", "memory"
            );
    }
    else
#endif
#ifdef SPRITESU_PLUSMASK
    if(mode & 2)
    {
        uint8_t const* image_ptr = (uint8_t const*)image;
        asm volatile(R"ASM(

                cpi %[page_start], 0
                brge L%=_middle

                ; advance buf to next page
                subi %A[buf], lo8(-128)
                sbci %B[buf], hi8(-128)
                mov %[count], %[cols]

            L%=_top_loop:

                ; write one page from image to buf+128
                lpm %A[image_data], %a[image]+
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

                ; decrement pages, reset buf back, advance image and mask
                clr __zero_reg__
                dec %[pages]
                sub %A[buf], %[cols]
                sbc %B[buf], __zero_reg__
                add %A[image], %[image_adv]
                adc %B[image], __zero_reg__

            L%=_middle:

                tst %[pages]
                breq L%=_bottom

                ; need Y pointer for middle pages
                ;push r28
                ;push r29
                movw r28, %[buf]
                subi r28, lo8(-128)
                sbci r29, hi8(-128)

            L%=_middle_loop_outer:

                mov %[count], %[cols]

            L%=_middle_loop_inner:
                ; write one page from image to buf/buf+128
                lpm %A[image_data], %a[image]+
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
                ;pop r29
                ;pop r28

            L%=_bottom:

                tst %[bottom]
                breq L%=_finish

            L%=_bottom_loop:

                ; write one page from image to buf
                lpm %A[image_data], %a[image]+
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
                dec %[cols]
                brne L%=_bottom_loop

            L%=_finish:

                clr __zero_reg__

            )ASM"
            :
            [buf]        "+&x" (buf),
            [image]      "+&z" (image_ptr),
            [pages]      "+&r" (pages),
            [count]      "=&r" (count),
            [buf_data]   "=&r" (buf_data),
            [image_data] "=&r" (image_data),
            [mask_data]  "=&r" (mask_data)
            :
            [cols]       "r"   (cols),
            [buf_adv]    "r"   (buf_adv),
            [image_adv]  "r"   (image_adv),
            [shift_coef] "r"   (shift_coef),
            [bottom]     "r"   (bottom),
            [page_start] "r"   (page_start)
            :
            "r28", "r29", "memory"
            );
    }
    else
#endif
#ifdef SPRITESU_FX
    {
        uint8_t* bufn;
        uint8_t reseek = (w != cols);
        image += ((uint24_t)FX::programDataPage << 8);
        asm volatile(R"ASM(

                rjmp L%=_begin

            L%=_seek:

                ; seek subroutine
                cbi %[fxport], %[fxbit]
                nop
                out %[spdr], %[sfc_read]
                add %A[image], %A[image_adv]
                adc %B[image], %B[image_adv]
                adc %C[image], __zero_reg__
                in r0, %[spsr]
                sbrs r0, %[spif]
                rjmp .-6
                out %[spdr], %C[image]
                in r0, %[spsr]
                sbrs r0, %[spif]
                rjmp .-6
                out %[spdr], %B[image]
                in r0, %[spsr]
                sbrs r0, %[spif]
                rjmp .-6
                out %[spdr], %A[image]
                in r0, %[spsr]
                sbrs r0, %[spif]
                rjmp .-6
                out %[spdr], __zero_reg__
                in r0, %[spsr]
                sbrs r0, %[spif]
                rjmp .-6
                ret

            L%=_begin:

                ; initial seek
                sub %A[image], %A[image_adv]
                sbc %B[image], %B[image_adv]
                sbc %C[image], __zero_reg__
                rcall L%=_seek
                cp %[page_start], __zero_reg__
                brlt L%=_top
                tst %[pages]
                brne L%=_middle_skip_reseek
                rjmp L%=_bottom_dispatch

            L%=_top:

                ; init buf
                subi %A[buf], lo8(-128)
                sbci %B[buf], hi8(-128)
                mov %[count], %[cols]

                ; loop dispatch
                sbrc %[mode], 0
                rjmp L%=_top_loop_masked

            L%=_top_loop:

                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], r1
                st %a[buf]+, %[buf_data]
                lpm
                adiw r24, 0
                dec %[count]
                brne L%=_top_loop
                rjmp L%=_top_loop_done

            L%=_top_loop_masked:

                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                movw %A[image_data], r0
                lpm
                lpm
                lpm
                lpm
                nop
                in %A[shift_mask], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[shift_mask], %[shift_coef]
                movw %A[shift_mask], r0
                ld %[buf_data], %a[buf]
                com %B[shift_mask]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], %B[image_data]
                st %a[buf]+, %[buf_data]
                lpm
                dec %[count]
                brne L%=_top_loop_masked

            L%=_top_loop_done:

                ; decrement pages, reset buf back
                clr __zero_reg__
                sub %A[buf], %[cols]
                sbc %B[buf], __zero_reg__
                dec %[pages]
                brne L%=_middle
                rjmp L%=_finish

            L%=_middle:

                ; only seek again if necessary
                tst %[reseek]
                breq L%=_middle_skip_reseek
                in r0, %[spsr]
                sbi %[fxport], %[fxbit]
                lpm
                rcall L%=_seek

            L%=_middle_skip_reseek:

                movw %[bufn], %[buf]
                subi %A[bufn], lo8(-128)
                sbci %B[bufn], hi8(-128)

            L%=_middle_loop_outer:

                mov %[count], %[cols]

                ; loop dispatch
                sbrc %[mode], 0
                rjmp L%=_middle_loop_inner_masked

            L%=_middle_loop_inner:

                ; write one page from image to buf/buf+128
                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], r0
                st %a[buf]+, %[buf_data]
                ld %[buf_data], %a[bufn]
                and %[buf_data], %B[shift_mask]
                or %[buf_data], r1
                st %a[bufn]+, %[buf_data]
                dec %[count]
                brne L%=_middle_loop_inner
                rjmp L%=_middle_loop_outer_next

            L%=_middle_loop_inner_masked:

                ; write one page from image to buf/buf+128
                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                movw %A[image_data], r0
                ld %[buf_data], %a[buf]
                ld %B[shift_mask], %a[bufn]
                lpm
                lpm
                lpm
                in %A[shift_mask], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[shift_mask], %[shift_coef]
                com r0
                and %[buf_data], r0
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                com r1
                and %B[shift_mask], r1
                or %B[shift_mask], %B[image_data]
                st %a[bufn]+, %B[shift_mask]
                nop
                dec %[count]
                brne L%=_middle_loop_inner_masked

            L%=_middle_loop_outer_next:

                ; advance buf to the next page
                clr __zero_reg__
                add %A[buf], %[buf_adv]
                adc %B[buf], __zero_reg__
                dec %[pages]
                brne L%=_middle

            L%=_bottom:

                tst %[bottom]
                breq L%=_finish

                ; seek if needed
                tst %[reseek]
                breq L%=_bottom_dispatch
                in r0, %[spsr]
                sbi %[fxport], %[fxbit]
                lpm
                rcall L%=_seek

            L%=_bottom_dispatch:

                ; loop dispatch
                sbrc %[mode], 0
                rjmp L%=_bottom_loop_masked

            L%=_bottom_loop:

                ; write one page from image to buf
                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                ld %[buf_data], %a[buf]
                and %[buf_data], %A[shift_mask]
                or %[buf_data], r0
                st %a[buf]+, %[buf_data]
                lpm
                adiw r24, 0
                dec %[cols]
                brne L%=_bottom_loop
                rjmp L%=_finish

            L%=_bottom_loop_masked:

                ; write one page from image to buf
                in %A[image_data], %[spdr]
                out %[spdr], __zero_reg__
                mul %A[image_data], %[shift_coef]
                movw %A[image_data], r0
                lpm
                lpm
                lpm
                lpm
                nop
                in %[pages], %[spdr]
                out %[spdr], __zero_reg__
                mul %[pages], %[shift_coef]
                mov %[pages], r0
                ld %[buf_data], %a[buf]
                com %[pages]
                and %[buf_data], %[pages]
                or %[buf_data], %A[image_data]
                st %a[buf]+, %[buf_data]
                lpm
                dec %[cols]
                brne L%=_bottom_loop_masked
                lpm

            L%=_finish:

                clr __zero_reg__
                sbi %[fxport], %[fxbit]
                in r0, %[spsr]

            )ASM"
            :
            [buf]        "+&x" (buf),
            [bufn]       "=&z" (bufn),
            [image]      "+&r" (image),
            [pages]      "+&r" (pages),
            [count]      "=&r" (count),
            [buf_data]   "=&r" (buf_data),
            [image_data] "=&r" (image_data)
            :
            [cols]       "r"   (cols),
            [buf_adv]    "r"   (buf_adv),
            [image_adv]  "r"   (image_adv),
            [shift_coef] "r"   (shift_coef),
            [shift_mask] "r"   (shift_mask),
            [bottom]     "r"   (bottom),
            [page_start] "r"   (page_start),
            [reseek]     "r"   (reseek),
            [mode]       "r"   (mode),
            [sfc_read]   "r"   (SFC_READ),
            [fxport]     "I"   (_SFR_IO_ADDR(FX_PORT)),
            [fxbit]      "I"   (FX_BIT),
            [spdr]       "I"   (_SFR_IO_ADDR(SPDR)),
            [spsr]       "I"   (_SFR_IO_ADDR(SPSR)),
            [spif]       "I"   (SPIF)
            :
            "memory"
            );
    }
#endif
    {} // empty final else block, if needed
}

#ifdef SPRITESU_OVERWRITE
void SpritesU::drawOverwrite(
    int16_t x, int16_t y, uint8_t const* image, uint16_t frame)
{
    uint8_t w, h;
    asm volatile(
        "lpm %[w], Z+\n"
        "lpm %[h], Z+\n"
        : [w] "=r" (w), [h] "=r" (h), [image] "+z" (image));
    drawBasic(x, y, w, h, (uint24_t)image, frame, MODE_OVERWRITE);
}
void SpritesU::drawOverwrite(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image)
{
    drawBasic(x, y, w, h, (uint24_t)image, 0, MODE_OVERWRITE);
}
#endif

#ifdef SPRITESU_PLUSMASK
void SpritesU::drawPlusMask(
    int16_t x, int16_t y, uint8_t const* image, uint16_t frame)
{
    uint8_t w, h;
    asm volatile(
        "lpm %[w], Z+\n"
        "lpm %[h], Z+\n"
        : [w] "=r" (w), [h] "=r" (h), [image] "+z" (image));
    drawBasic(x, y, w, h, (uint24_t)image, frame, MODE_PLUSMASK);
}
void SpritesU::drawPlusMask(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t const* image)
{
    drawBasic(x, y, w, h, (uint24_t)image, 0, MODE_PLUSMASK);
}
#endif

#ifdef SPRITESU_FX
void SpritesU::drawOverwriteFX(
    int16_t x, int16_t y, uint24_t image, uint16_t frame)
{
    FX::seekData(image);
    uint8_t w = FX::readPendingUInt8();
    uint8_t h = FX::readEnd();
    drawBasic(x, y, w, h, image + 2, frame, MODE_OVERWRITEFX);
}
void SpritesU::drawOverwriteFX(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame)
{
    drawBasic(x, y, w, h, image + 2, frame, MODE_OVERWRITEFX);
}
void SpritesU::drawPlusMaskFX(
    int16_t x, int16_t y, uint24_t image, uint16_t frame)
{
    FX::seekData(image);
    uint8_t w = FX::readPendingUInt8();
    uint8_t h = FX::readEnd();
    drawBasic(x, y, w, h, image + 2, frame, MODE_PLUSMASKFX);
}
void SpritesU::drawPlusMaskFX(
    int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint16_t frame)
{
    drawBasic(x, y, w, h, image + 2, frame, MODE_PLUSMASKFX);
}
#endif

#ifdef SPRITESU_RECT
void SpritesU::fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if(x >= 128) return;
    if(x + w <= 0) return;
    if(y + h <= 0) return;
    fillRect_i8((int8_t)x, (int8_t)y, w, h, color);
}

// from Mr. Blinky's ArduboyFX library
static __attribute__((always_inline)) uint8_t SpritesU_bitShiftLeftMaskUInt8(uint8_t bit)
{
#ifdef ARDUINO_ARCH_AVR
    uint8_t result;
    asm volatile(
        "ldi    %[result], 1    \n" // 0 = 000 => 1111 1111 = -1
        "sbrc   %[bit], 1       \n" // 1 = 001 => 1111 1110 = -2
        "ldi    %[result], 4    \n" // 2 = 010 => 1111 1100 = -4
        "sbrc   %[bit], 0       \n" // 3 = 011 => 1111 1000 = -8
        "lsl    %[result]       \n"
        "sbrc   %[bit], 2       \n" // 4 = 100 => 1111 0000 = -16
        "swap   %[result]       \n" // 5 = 101 => 1110 0000 = -32
        "neg    %[result]       \n" // 6 = 110 => 1100 0000 = -64
        :[result] "=&d" (result)    // 7 = 111 => 1000 0000 = -128
        :[bit]    "r"   (bit)
        :
    );
    return result;
#else
    return (0xFF << (bit & 7)) & 0xFF;
#endif
}

void SpritesU::fillRect_i8(int8_t x, int8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if(w == 0 || h == 0) return;
    if(y >= 64)  return;
    if(x + w <= 0) return;
    if(y + h <= 0) return;

    if(color & 1) color = 0xff;

    // clip coords
    uint8_t xc = x;
    uint8_t yc = y;

    // TODO: extreme clipping behavior

    // clip
    if(y < 0)
        h += y, yc = 0;
    if(x < 0)
        w += x, xc = 0;
    if(h >= uint8_t(64 - yc))
        h = 64 - yc;
    if(w >= uint8_t(128 - xc))
        w = 128 - xc;
    uint8_t y1 = yc + h;

    uint8_t c0 = SpritesU_bitShiftLeftMaskUInt8(yc); // 11100000
    uint8_t m1 = SpritesU_bitShiftLeftMaskUInt8(y1); // 11000000
    uint8_t m0 = ~c0; // 00011111
    uint8_t c1 = ~m1; // 00111111

    uint8_t r0 = yc;
    uint8_t r1 = y1 - 1;
    asm volatile(
        "lsr %[r0]\n"
        "lsr %[r0]\n"
        "lsr %[r0]\n"
        "lsr %[r1]\n"
        "lsr %[r1]\n"
        "lsr %[r1]\n"
        : [r0] "+&r" (r0),
          [r1] "+&r" (r1));

    uint8_t* buf = Arduboy2Base::sBuffer;
    asm volatile(
        "mul %[r0], %[c128]\n"
        "add %A[buf], r0\n"
        "adc %B[buf], r1\n"
        "clr __zero_reg__\n"
        "add %A[buf], %[x]\n"
        "adc %B[buf], __zero_reg__\n"
        :
        [buf]  "+&e" (buf)
        :
        [r0]   "r"   (r0),
        [x]    "r"   (xc),
        [c128] "r"   ((uint8_t)128)
        );

    uint8_t rows = r1 - r0; // middle rows + 1
    uint8_t f = 0;
    uint8_t bot = c1;
    if(m0  == 0) ++rows; // no top fragment
    if(bot == 0) ++rows; // no bottom fragment
    c0 &= color;
    c1 &= color;

    uint8_t col;
    uint8_t buf_adv = 128 - w;

    asm volatile(R"ASM(
            tst  %[rows]
            brne L%=_top
            or   %[m1], %[m0]
            and  %[c1], %[c0]
            rjmp L%=_bottom_loop

        L%=_top:
            tst  %[m0]
            breq L%=_middle
            mov  %[col], %[w]

        L%=_top_loop:
            ld   __tmp_reg__, %a[buf]
            and  __tmp_reg__, %[m0]
            or   __tmp_reg__, %[c0]
            st   %a[buf]+, __tmp_reg__
            dec  %[col]
            brne L%=_top_loop
            add  %A[buf], %[buf_adv]
            adc  %B[buf], __zero_reg__

        L%=_middle:
            dec  %[rows]
            breq L%=_bottom

        L%=_middle_outer_loop:
            mov  %[col], %[w]
            sbrs %[col], 0
            rjmp L%=_middle_inner_loop
            inc  %[col]
            rjmp L%=_middle_inner_loop_odd
            
        L%=_middle_inner_loop:
            st   %a[buf]+, %[color]
        L%=_middle_inner_loop_odd:
            st   %a[buf]+, %[color]
            subi %[col], 2
            brne L%=_middle_inner_loop
            add  %A[buf], %[buf_adv]
            adc  %B[buf], __zero_reg__
            dec  %[rows]
            brne L%=_middle_outer_loop

        L%=_bottom:
            tst  %[bot]
            breq L%=_finish

        L%=_bottom_loop:
            ld   __tmp_reg__, %a[buf]
            and  __tmp_reg__, %[m1]
            or   __tmp_reg__, %[c1]
            st   %a[buf]+, __tmp_reg__
            dec  %[w]
            brne L%=_bottom_loop

        L%=_finish:
        )ASM"
        :
        [buf]     "+&e" (buf),
        [w]       "+&r" (w),
        [rows]    "+&r" (rows),
        [col]     "=&r" (col)
        :
        [buf_adv] "r"   (buf_adv),
        [color]   "r"   (color),
        [m0]      "r"   (m0),
        [m1]      "r"   (m1),
        [c0]      "r"   (c0),
        [c1]      "r"   (c1),
        [bot]     "r"   (bot)
        );
}
#endif

#endif
