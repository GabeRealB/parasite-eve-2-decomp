#include "common.h"
#include "main/pad.h"

#include "main/game.h"
#include "main/unknown_syms.h"

#include <psyq/libpad.h>

void Pad_Init(void)
{
    u16                  half;
    u8                   one;
    volatile PadState*   base;
    register s32         offset asm("a2");
    volatile PadState*   p;
    u8*                  ptr;
    u32                  i;
    PadRawPort*          pad;
    volatile PadRawPort* vpad;
    u32                  j;
    u8                   ff;
    s32                  tmp;

    half   = 0xFF;
    one    = 1;
    base   = Pad_States;
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

    pad = Pad_RawPorts;
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

void Tmd_InitLists(void)
{
    D_80071210       = 0;
    Tmd_List.next    = NULL;
    Tmd_List.prev    = &Tmd_List;
    Tmd_ListAlt.next = NULL;
    Tmd_ListAlt.prev = &Tmd_ListAlt;
}
