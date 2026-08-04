#include "common.h"

#include "main/game.h"
#include "main/unknown_syms.h"

#include <psyq/libpad.h>

void func_80028664(void)
{
    u16                 half;
    u8                  one;
    volatile GStruct25* base;
    register s32        offset asm("a2");
    volatile GStruct25* p;
    u8*                 ptr;
    u32                 i;
    GStruct49*          pad;
    volatile GStruct49* vpad;
    u32                 j;
    u8                  ff;
    s32                 tmp;

    half   = 0xFF;
    one    = 1;
    base   = D_80071620;
    tmp    = (s32)base;
    p      = base;
    offset = 0;
    do {
        ptr = (u8*)(offset + tmp);
        for (i = 0; i < 0x5C; i++) {
            *ptr++ = 0;
        }
        p->field_5A = 0;
        p->field_5B = 0;
        p->field_0  = half;
        p->field_3  = one;
        p++;
        offset += 0x5C;
    } while (p < base + 2);

    pad = D_800711C8;
    PadInitDirect((u8*)pad, (u8*)(pad + 1));
    j = 0;
    PadStartCom();
    vpad = pad;
    for (; j < 2; j++) {
        ff            = 0xFF;
        vpad->field_2 = ff;
        vpad->field_3 = ff;
        vpad++;
    }
}

void func_80028718(void)
{
    D_80071210      = 0;
    D_800711B8.next = NULL;
    D_800711B8.prev = &D_800711B8;
    D_800711C0.next = NULL;
    D_800711C0.prev = &D_800711C0;
}
