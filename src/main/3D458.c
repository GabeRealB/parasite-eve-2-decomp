#include "common.h"

#include <psyq/libspu.h>
#include <psyq/libapi.h>

#include "main/unknown_syms.h"

void func_8004CC58(s32 arg0)
{
    s32* temp_v0;
    u16  temp_s1;

    D_800680C0 = 0;
    if (arg0 == 1) {
        goto wait_spu_transfer;
    }
    if (arg0 < 2) {
        if (arg0 == 0) {
            goto init_spu;
        }
        goto end;
    }
    if (arg0 == 2) {
        goto setup_events;
    }
    goto end;

init_spu:
    SpuInit();
    D_800680A5 = 0;
    goto unknown;

wait_spu_transfer:
    SpuIsTransferCompleted(1);

unknown:
    D_800680BC = 0;
    func_8002604C();

setup_events:
    func_8004D5A8();
    func_800509B4();
    func_8004DDF0();
    func_8004E788(3);
    func_8004DF10();
    func_8004D0A0();
    func_8004D88C();
    func_80053E68();
    func_80053FF4(0);
    func_80050D20(0);

    temp_v0  = func_8004D5D8(4);
    *temp_v0 = 0;

    func_8004D460(&func_80053F60, 0, 0x8801, temp_v0);
    if (D_800680A5 != 0) {
        DisableEvent(D_8007E0D0);
        CloseEvent(D_8007E0D0);
        StopRCnt(RCntCNT0);
        D_800680A5 = 0;
    }

    temp_s1 = D_8007108C;
    if (temp_s1 == 1) {
        D_800680A4 = 0;
        D_8007E0CC = 0;
        SetRCnt(RCntCNT0, 0xffff, RCntMdINTR | RCntMdSC);
        ResetRCnt(RCntCNT0);
        StartRCnt(RCntCNT0);
        EnterCriticalSection();
        D_8007E0D0 = OpenEvent(RCntCNT0, EvSpINT, EvMdINTR, func_8004D7D4);

        // HACK: What is this? The control flow of this function already
        // looks bad. To add insult to injury, This is the output that we
        // want:
        //
        // jal      OpenEvent
        // addiu    a3, a3, %lo(func_8004D7D4)
        // sw       v0, %lo(D_8007E0D0)(s0)
        // jal      ExitCriticalSection
        // nop
        //
        // And this is the assembly that we get without this line:
        //
        // jal      OpenEvent
        // addiu    a3, a3, %lo(func_8004D7D4)
        // jal      ExitCriticalSection
        // sw       v0, %lo(D_8007E0D0)(s0)
        //
        // Somehow the developers managed to insert the additional nop
        // instruction, and the only way I could think of is to insert
        // an empty assembler instruction. Maybe it has something to do with
        // the compiler/maspsx version, or rewriting the function with more
        // sensible control flow could fix it, but this matches.
        asm("");

        ExitCriticalSection();
        EnableEvent(D_8007E0D0);
        D_800680A5 = temp_s1;
    }
    D_800680A4 = 0;
    D_8007E0CC = 0;

end:
    D_800680C0 = 1;
}

INCLUDE_ASM("main/nonmatchings/3D458", func_8004CE28);

void func_8004CFC8(void)
{
    func_8004CC58(0);
}

INCLUDE_ASM("main/nonmatchings/3D458", func_8004CFE8);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D008);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D0A0);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D0F0);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D150);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D19C);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D200);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D298);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D2EC);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D35C);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D460);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D5A8);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D5D8);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D6C8);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D7D4);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D820);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D88C);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D8BC);

INCLUDE_ASM("main/nonmatchings/3D458", func_8004D94C);
