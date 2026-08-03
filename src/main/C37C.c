#include "common.h"

#include <psyq/libcd.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"
#include "psyq/libpress.h"

INCLUDE_ASM("main/nonmatchings/C37C", func_8001BB7C);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001BE60);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001C0D4);

void func_8001C620(void)
{
    CdCmdQueue* state;
    s32       cmd;
    s32       status;
    s16       ret;

    state = &CdCmd_Queue;
    cmd   = state->entries[state->readIdx].cmd;
    if (cmd < 0x54) {
        return;
    }
    switch (cmd) {
    case 0x54: {
        CdCmdEntry* entry;
        s32            field5;
        s32            step;

        entry  = &state->entries[state->readIdx];
        status = *(volatile u8*)&entry->param0;
        field5 = status;
        step   = state->step;
        field5 = (s8)field5;
        switch (step) {
        case 0:
            if (state->busy == 0) {
                state->busy      = 1;
                D_80070F68.field_130 = 0xFF;
            }
            Fs_SelectStage(field5 & 0xFF);
            goto increment_step;
        case 1:
            if (CdSync(1, NULL) == CdlDiskError) {
                CdSyncCallback(NULL);
                CdReadyCallback(NULL);
                goto wait_reset_clear_step;
            }
            F12D18_800257B0();
            status = Fs_CdOpStatus;
            switch (status) {
            case 0x80:
                ret = func_8001E6AC(0, 0);
                if (ret != step) {
                    if (ret < 2) {
                        return;
                    }
                    if (ret != 2) {
                        return;
                    }
                    CdFlush();
                }
                if (CdSync(1, NULL) == CdlDiskError) {
                wait_reset_clear_step:
                    Fs_WaitDiskReset(1);
                }
                state->step = 0;
                return;
            case 0xFF:
                CdSyncCallback(NULL);
                CdReadyCallback(NULL);
            increment_step:
                state->step = state->step + 1;
                return;
            case 0x10:
            case 0x20:
            case 0x40:
                ret = func_8001E6AC(0, 0);
                if (ret != 1) {
                    if (ret < 2) {
                        return;
                    }
                    if (ret != 2) {
                        return;
                    }
                    CdFlush();
                }
                F12D18_80024EC0();
                return;
            }
            return;
        case 2:
            ret = func_8001E6AC(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    return;
                }
                if (ret != step) {
                    return;
                }
                CdFlush();
            }
            Fs_InitFolderTable(field5 & 0xFF);
            goto cleanup;
        }
        return;
    }
    case 0x55:
        status = Fs_CdOpStatus;
        if (status != 0xFF) {
            goto case55_cont;
        }
    cleanup:
        if (state->busy != 0) {
            state->busy      = 0;
            D_80070F68.field_130 = 0;
        }
        state->step = 0;
        state->field_1fc = 0;
        state->field_222 = 0;
        state->field_242 = 0;
        if (state->readIdx != state->writeIdx) {
            u32 t;
            t = state->readIdx << 3;
            t += (u32)state;
            ((CdCmdEntry*)t)->cmd = 0;
            state->readIdx = state->readIdx + 1;
            state->readIdx = state->readIdx % 8;
        }
        return;
    case55_cont:
        if (status != 0x80) {
            return;
        }
        ret = func_8001E6AC(0, 0);
        if (ret != 1) {
            if (ret < 2) {
                return;
            }
            if (ret != 2) {
                return;
            }
            CdFlush();
        }
        if (CdSync(1, NULL) == CdlDiskError) {
            Fs_WaitDiskReset(1);
        }
        Fs_InitStage0Tables();
        return;
    }
}

INCLUDE_ASM("main/nonmatchings/C37C", func_8001C970);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001CA70);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001CDF0);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001CEFC);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D0E8);

INCLUDE_ASM("main/nonmatchings/C37C", CdCmd_Enqueue);

u16 func_8001D344(void)
{
    CdCmdQueue* p;

    p = &CdCmd_Queue;
    if (p->field_4c != 0) {
        return 0;
    }
    return p->writeIdx == p->readIdx;
}

u16 func_8001D37C(s16 arg0)
{
    return CdCmd_Queue.entries[arg0].cmd == 0;
}

void func_8001D39C(void)
{
    CdCmd_Queue.field_1EA = 1;
    if (func_80020394(&D4F564_8005ED64->field_4) != 0) {
        DecDCTvlcBuild(D_8005C36C);
        D4F564_8005ED64->field_7C = 0;
    }
}

void CdCmd_ClearQueue(void)
{
    Mem_Set(&CdCmd_Queue, 0, sizeof(CdCmd_Queue));
}

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D424);

void func_8001D498(void)
{
    CdCmdQueue* p;

    p = &CdCmd_Queue;
    if (func_8001EDC8(&D4F564_8005ED64->field_4, 0, 0) >= 0) {
        D_8006AC40 = D_8006AC00;
    }
    p->field_1E6 = 0;
}

s32 func_8001D4F0(void)
{
    CdCmdQueue* p;
    s32 ret;

    p = &CdCmd_Queue;
    if (p->field_21A >= 0) {
        return 2;
    }
    if (p->field_236 < 0) {
        return -1;
    }
    ret = 3;
    return ret;
}

s16 func_8001D524(void)
{
    return CdCmd_Queue.field_20E;
}

void func_8001D534(u16 arg0, u16 arg1, u16 arg2)
{
    CdCmdQueue* p;

    p = &CdCmd_Queue;
    p->field_1FF = 1;
    p->field_236 = -1;
    p->field_21A = func_800AF89C(arg0, arg1, arg2, 0);
}

void func_8001D580(void)
{
}

void func_8001D588(void)
{
    CdCmd_Queue.field_54 = 0;
    func_8001C970();
    func_800B00C4();
}

void func_8001D5B4(void)
{
}

void func_8001D5BC(void)
{
}

void func_8001D5C4(void)
{
}

void func_8001D5CC(void)
{
    CdCmdQueue* p;
    u8        sp10;

    p = &CdCmd_Queue;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        p->field_20E = 2;
        CdCmd_Enqueue(0x81, 0, &sp10);
    } else {
        p->field_20E = 1;
    }
}

void func_8001D628(void)
{
    CdCmdQueue* p;
    u8        sp10;

    p = &CdCmd_Queue;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        CdCmd_EnqueueReplace(0x81, 0, &sp10);
    }
}

void func_8001D66C(void)
{
    CdCmdQueue* p;
    u8        sp10;

    p = &CdCmd_Queue;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        p->field_20E = 2;
        CdCmd_Enqueue(0x82, 0, &sp10);
    }
}

void func_8001D6B8(void)
{
    CdCmdQueue* p;
    u8        sp10;

    p = &CdCmd_Queue;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        CdCmd_EnqueueReplace(0x82, 0, &sp10);
    }
}

INCLUDE_ASM("main/nonmatchings/C37C", CdCmd_EnqueueReplace);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D760);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D82C);

CdCmdEntry* func_8001D898(void)
{
    CdCmdQueue* p;
    s32 index;
    CdCmdEntry* entry;

    p = &CdCmd_Queue;
    index = D_8006AC04;
    entry = &p->entries[index];
    if (index == p->writeIdx) {
        return NULL;
    }
    D_8006AC04 = index + 1;
    D_8006AC04 = D_8006AC04 % 8;
    return entry;
}

void func_8001D8DC(void)
{
    if (CdCmd_Queue.busy == 0) {
        CdCmd_Queue.busy = 1;
        D_80070F68.field_130 = 0xFF;
    }
}

void func_8001D90C(void)
{
    if (CdCmd_Queue.busy != 0) {
        CdCmd_Queue.busy = 0;
        D_80070F68.field_130 = 0;
    }
}

void func_8001D934(void)
{
    CdCmdQueue* state;

    state = &CdCmd_Queue;
    Mem_Set(state, 0, 0x40);
    state->writeIdx = 0;
    state->readIdx = 0;
    state->step = 0;
    state->field_1fc = 0;
    state->field_1d2 = 0;
}

void func_8001D97C(void)
{
    D_8006AC04 = CdCmd_Queue.readIdx;
}

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D990);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001DA48);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001DAB8);

void func_8001DB84(void)
{
    CdCmdQueue* state; // The indirection is required.

    state = &CdCmd_Queue;
    switch (state->field_4c) {
    case 0:
        if (state->field_204 == 0) {
            switch (state->entries[state->readIdx].cmd >> 4) {
            case 0:
                break;
            case 2:
                func_8001C0D4();
                break;
            case 6:
                func_8001BE60();
                break;
            case 7:
                func_8017D6D4();
                break;
            case 5:
                func_8001C620();
                break;
            case 8:
                func_800AFA44();
                break;
            }
        }
        break;
    case 1:
        func_8001CA70();
        break;
    case 2:
        func_8001CEFC();
        break;
    }

    if (state->field_224 != 0) {
        F12D18_800225D4();
    }
}
