#include "common.h"
#include "main/pad.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

s32 Pad_CheckButtons(s32 arg0, s32 arg1, s32 arg2)
{
    PadState* p;
    u16       val;

    p = (PadState*)&Pad_States[arg0];
    switch (arg1) {
        case 1:
            val = p->field_6;
            break;
        case 3:
            val = p->field_8;
            break;
        default:
            val = p->field_4;
            break;
    }
    if (arg1 == 2) {
        return (val & arg2) == arg2;
    }
    return (val & arg2) != 0;
}

INCLUDE_ASM("main/nonmatchings/padutil", func_8002C8E4);

void Pad_SetCooldown(s32 arg0)
{
    volatile PadState* p;

    p          = &Pad_States[arg0];
    p->field_A = 0x3D;
}

void Pad_ClearCooldown(s32 arg0)
{
    volatile PadState* p;

    p          = &Pad_States[arg0];
    p->field_A = 0;
}

s32 Pad_ReadButtonsInv(s32 arg0)
{
    u16         sp;
    PadRawPort* base;

    base          = Pad_RawPorts;
    ((u8*)&sp)[1] = base[arg0].field_2;
    ((u8*)&sp)[0] = base[arg0].field_3;
    return (u16)~sp;
}

void Pad_ClearEvents(s32 arg0)
{
    PadState* p;
    s32       i;
    s32       j;
    s32       offset;
    PadEvent* entries;

    p      = (PadState*)&Pad_States[arg0];
    i      = 0;
    offset = 0x10;
    for (; i < 2; i++) {
        entries = p->field_10[i];
        for (j = 0; j < 8; j++) {
            entries[j].field_0 = 0;
            entries[j].field_1 = 0;
            entries[j].field_2 = 0;
        }
        offset += 0x20;
    }
    p->field_2 = 0;
}

s32 Pad_CheckSpecialCombo(void)
{
    volatile PadState* p;
    u16                val;
    s32                result;

    p   = Pad_States;
    val = p->field_4;
    if (val == 0x90F) {
        result = D_8005ED8A == 0x90F;
    } else {
        result = 0;
    }
    D_8005ED8A = val;
    if (p->field_A != 0) {
        D_8005ED8A = 0;
        result     = 0;
    }
    return result;
}
