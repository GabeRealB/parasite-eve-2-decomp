#include "common.h"

#include "main/fs.h"
#include "main/game.h"
#include "main/unknown_syms.h"

void func_80042364(s32 arg0, s32 arg1, s32 arg2)
{
    s8              param2[4];
    u8*             param1;
    register void** scratch asm("v1");
    register void*  head asm("a3");
    register void*  temp asm("v0");
    u8              f74;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    temp     = (u8*)head - 8;
    param1   = temp;
    *scratch = temp;

    param1[2]       = 2;
    param1[3]       = 0;
    ((u8*)head)[-8] = arg1;
    param2[0]       = arg0;

    if ((u8)arg2 < 5) {
        switch ((u8)arg2) {
            case 0:
                param2[1] = 3;
                param2[2] = -8;
                param2[3] = -3;
                break;
            case 1:
                param2[2] = 0;
                param2[1] = 0;
                param2[3] = -2;
                break;
            case 2:
                param2[1] = 3;
                param2[2] = 0;
                param2[3] = -2;
                break;
            case 3:
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                break;
            case 4:
                if (D_800626E8 != 0) {
                    param1[3] = D4F564_8005ED64->field_7;
                    param1[2] = D4F564_8005ED64->field_6;
                    param1[0] = func_800AD284();
                    f74       = D4F564_8005ED64->field_74;
                    param2[1] = 1;
                    param2[3] = 0;
                    param2[2] = 0;
                    param2[0] = f74;
                    func_8001D990(0x21, param1, (u8*)param2);
                    D_800626E8 = 0;
                }
                *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
                return;
        }
    }

    CdCmd_Enqueue(0x21, param1, (u8*)param2);
    D_800626E8              = 1;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

INCLUDE_ASM("main/nonmatchings/32B64", func_80042500);

INCLUDE_ASM("main/nonmatchings/32B64", func_80042838);

void func_800429C8(s32 arg0)
{
    GBytes4 sp10;
    u8      temp;

    sp10 = D_80013F18;
    if (D_80072311 == 0) {
        func_800260B0(1);
    } else {
        func_800260B0(0);
    }
    if ((arg0 & 0xFFFF) != 0) {
        D_8007A396 = arg0;
        if (D_80062737 != 0) {
            func_80051460(D_80062737, (u8)D_8007A396);
        } else {
            func_80051460(0, (u8)D_8007A396);
        }
    } else {
        temp       = sp10.data[Mc_SaveData.field_1aa];
        D_8007A396 = temp;
        if ((s8)Mc_SaveData.field_1aa == 3) {
            func_80051888();
        } else {
            func_800518E0();
        }
        if (D_80062737 != 0) {
            func_80051460(D_80062737, (u8)D_8007A396);
        } else {
            func_80051460(0, sp10.data[D_80072312]);
        }
    }
}
