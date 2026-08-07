#include "common.h"
#include "main/fs.h"
#include "main/task.h"

#include <psyq/libcd.h>

#include "main/game.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/unknown_syms.h"
#include "psyq/libpress.h"

void* func_8001BB7C(void)
{
    CdCmdQueue* p;
    u16         slot;
    u8          kind;
    s32*        sizeRow;
    s32         size;

    p = &CdCmd_Queue;
    if (p->field_216 != 0) {
        if (p->field_238 == 0) {
            slot = p->field_190->field_1A;
            switch (slot) {
                case 0:
                    p->field_18C = Mem_Malloc(0x11000, 1);
                    break;
                case 1:
                    D4F564_8005ED64->field_7C = 0;
                    p->field_18C              = D_8005C36C;
                    break;
                case 2:
                    D4F564_8005ED64->field_7E = 0;
                    p->field_18C              = D_8005C370;
                    break;
                case 3:
                    D4F564_8005ED64->field_80 = 0;
                    p->field_18C              = D_8005C374;
                    break;
            }
            if (p->field_21E == 0) {
                DecDCTvlcBuild(p->field_18C);
                p->field_21E = 1;
            }
        } else {
            p->field_18C = NULL;
        }

        p->field_1A4 = NULL;
        kind         = p->field_190->field_3;
        switch (kind) {
            case 1:
                p->field_1A4 = Mem_Malloc(p->field_190->field_1E, 1);
                break;
            case 2:
                D4F564_8005ED64->field_7C = 0;
                p->field_1A4              = D_8005C36C;
                if (p->field_190->field_1A == 1) {
                    p->field_1A4 = (u8*)D_8005C36C + 0x11000;
                }
                break;
            case 3:
                D4F564_8005ED64->field_7E = 0;
                p->field_1A4              = D_8005C370;
                if (p->field_190->field_1A == 2) {
                    p->field_1A4 = (u8*)D_8005C370 + 0x11000;
                }
                break;
            case 4:
                D4F564_8005ED64->field_80 = 0;
                p->field_1A4              = D_8005C374;
                if (p->field_190->field_1A == 3) {
                    p->field_1A4 = (u8*)D_8005C374 + 0x11000;
                }
                break;
        }

        p->field_19C = (u32*)p->field_1A4;
        if (p->field_188 != 0) {
            p->field_184 = Mem_Malloc(p->field_188, 1);
        }
    }

    D_8006AC00 = NULL;
    if (D4F564_8005ED64->field_7 == 0) {
        D_8006AC00 = Mem_Malloc(0x4B000, 1);
    } else if (func_8001EDC8(&D4F564_8005ED64->field_4, 0, 0) < 0) {
        return NULL;
    } else {
        sizeRow = D_8005DCB4[Mc_SaveData.field_7];
        if (sizeRow != NULL) {
            size = sizeRow[Mc_SaveData.field_6];
            if (size != 0) {
                D_8006AC00 = Mem_Malloc(size, 1);
            }
        }
    }
    return D_8006AC00;
}

void func_8001BE60(void)
{
    CdCmdQueue* state;
    CdCmdQueue* p;
    CdCmdEntry* entry;
    s16         ret;
    s32         idB0;
    s32         cmd;
    s32         busy;

    state = &CdCmd_Queue;
    idB0  = *(volatile u8*)&state->entries[state->readIdx].idB0;
    cmd   = state->entries[state->readIdx].cmd;
    idB0  = (s8)idB0;
    if (cmd == 0) {
        return;
    }
    if (cmd < 0) {
        return;
    }
    if (cmd >= 0x63) {
        return;
    }
    if (cmd < 0x61) {
        return;
    }
    switch (state->step) {
        case 0:
            if (state->busy == 0) {
                state->busy             = 1;
                Display_State.field_130 = 0xFF;
            }
            ret = func_8001E6AC(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    goto end_check;
                }
                if (ret != 2) {
                    goto end_check;
                }
                CdFlush();
            }
            entry = &state->entries[state->readIdx];
            if (entry->cmd == 0x61) {
                D_8005EAEC = 0;
                D_8005EAEE = 0;
            } else if (entry->cmd == 0x62) {
                entry->cmd = 0x61;
            }
            if ((s16)func_8001F180(idB0 & 0xFFFF) != 0) {
                p                = &CdCmd_Queue;
                busy             = p->busy;
                state->field_1FA = 1;
                state->field_1F4 = 1;
                if (busy != 0) {
                    p->busy                 = 0;
                    Display_State.field_130 = 0;
                }
                p->step      = 0;
                p->field_1fc = 0;
                p->field_222 = 0;
                p->field_242 = 0;
                if (p->readIdx != p->writeIdx) {
                    p->entries[p->readIdx].cmd = 0;
                    p->readIdx                 = p->readIdx + 1;
                    p->readIdx                 = p->readIdx % 8;
                }
                goto end_check;
            }
            state->field_1E8 = 1;
            func_8001FAE0(0, ((u16)state->field_1EA - 1) * 0xA);
            state->step = state->step + 1;
            /* fallthrough */
        case 1:
        end_check:
            if ((s16)func_8001FAE0(0, ((u16)state->field_1EA - 1) * 0xA) != 0) {
                p = &CdCmd_Queue;
                if (p->busy != 0) {
                    p->busy                 = 0;
                    Display_State.field_130 = 0;
                }
                p->step      = 0;
                p->field_1fc = 0;
                p->field_222 = 0;
                p->field_242 = 0;
                if (p->readIdx != p->writeIdx) {
                    p->entries[p->readIdx].cmd = 0;
                    p->readIdx                 = p->readIdx + 1;
                    p->readIdx                 = p->readIdx % 8;
                }
            }
            return;
    }
}

void func_8001C0D4(void)
{
    CdCmdQueue* state;
    CdCmdQueue* p;
    s16         ret;
    s32         status;
    u8          req[4];
    u8          mode;

    state  = &CdCmd_Queue;
    req[3] = state->entries[state->readIdx].param0;
    req[2] = state->entries[state->readIdx].param1;
    req[0] = state->entries[state->readIdx].param2;
    req[1] = state->entries[state->readIdx].idB0;

    switch (state->step) {
        case 0:
            p = &CdCmd_Queue;
            {
                s32 busy;
                busy             = p->busy;
                state->field_222 = 1;
                if (busy == 0) {
                    p->busy                 = 1;
                    Display_State.field_130 = 0xFF;
                }
            }
            ret = func_8001E6AC(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    goto end_check;
                }
                if (ret != 2) {
                    goto end_check;
                }
                CdFlush();
            }
            mode = 0xA0;
            CdControlB(CdlSetmode, &mode, NULL);
            if ((s8)state->entries[state->readIdx].idB1 != 0) {
                state->step = 4;
                goto do_load;
            }
            if (req[3] == 0) {
                state->step = 4;
                goto do_load;
            }
            if (req[0] != 0) {
                state->step = 4;
                goto do_load;
            }
            state->step = state->step + 1;
            /* fallthrough */
        case 1:
            func_80023748(req[3], req[2], req[1]);
            goto increment_step;
        case 2: {
            s32 sync;
            s32 diskErr;

            sync    = CdSync(1, NULL);
            diskErr = CdlDiskError;
            asm("" : "+r"(diskErr));
            if (sync == diskErr) {
                CdSyncCallback(NULL);
                CdReadyCallback(NULL);
                goto wait_reset_step1;
            }
            F12D18_800257B0();
            status = Fs_CdOpStatus;
            switch (status) {
                case 0x80:
                    ret = func_8001E6AC(0, 0);
                    if (ret != 1) {
                        if (ret < 2) {
                            if (ret == 0) {
                                return;
                            }
                            goto end_check;
                        }
                        if (ret != 2) {
                            goto end_check;
                        }
                        CdFlush();
                    }
                    sync    = CdSync(1, NULL);
                    diskErr = CdlDiskError;
                    asm("" : "+r"(diskErr));
                    if (sync == diskErr) {
                    wait_reset_step1:
                        Fs_WaitDiskReset(1);
                    }
                    state->step = 1;
                    goto end_check;
                case 0xFF:
                    CdSyncCallback(NULL);
                    CdReadyCallback(NULL);
                    goto increment_step;
                case 0x10:
                case 0x20:
                case 0x40:
                    ret = func_8001E6AC(0, 0);
                    if (ret != 1) {
                        if (ret < 2) {
                            if (ret == 0) {
                                return;
                            }
                            goto end_check;
                        }
                        if (ret != 2) {
                            goto end_check;
                        }
                        CdFlush();
                    }
                    F12D18_80024EC0();
                    goto end_check;
            }
            goto end_check;
        }
        case 3:
            ret = func_8001E6AC(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    goto do_load;
                }
                if (ret != 2) {
                    goto do_load;
                }
                CdFlush();
            }
            func_8002397C(req[3], req[2], req[1]);
            state->step = state->step + 1;
            /* fallthrough */
        case 4:
        do_load:
            Fs_LoadFile(
                req,
                state->entries[state->readIdx].idB1,
                (s8)state->entries[state->readIdx].idB2,
                (s8)state->entries[state->readIdx].idB3);
        increment_step:
            state->step = state->step + 1;
            goto end_check;
        case 5: {
            s32 sync;
            s32 diskErr;

            sync    = CdSync(1, NULL);
            diskErr = CdlDiskError;
            asm("" : "+r"(diskErr));
            if (sync == diskErr) {
                CdSyncCallback(NULL);
                CdReadyCallback(NULL);
                goto wait_reset_step4;
            }
            F12D18_800257B0();
            status = Fs_CdOpStatus;
            switch (status) {
                case 0x80:
                    ret = func_8001E6AC(0, 0);
                    if (ret != 1) {
                        if (ret < 2) {
                            if (ret == 0) {
                                return;
                            }
                            goto end_check;
                        }
                        if (ret != 2) {
                            goto end_check;
                        }
                        CdFlush();
                    }
                    sync    = CdSync(1, NULL);
                    diskErr = CdlDiskError;
                    asm("" : "+r"(diskErr));
                    if (sync == diskErr) {
                    wait_reset_step4:
                        Fs_WaitDiskReset(1);
                    }
                    state->step = 4;
                    goto end_check;
                case 0xFF:
                    if (state->field_1FE != status) {
                        goto end_check;
                    }
                    CdSyncCallback(NULL);
                    CdReadyCallback(NULL);
                    p = &CdCmd_Queue;
                    if (p->busy != 0) {
                        p->busy                 = 0;
                        Display_State.field_130 = 0;
                    }
                    p->step      = 0;
                    p->field_1fc = 0;
                    p->field_222 = 0;
                    p->field_242 = 0;
                    if (p->readIdx != p->writeIdx) {
                        p->entries[p->readIdx].cmd = 0;
                        p->readIdx                 = p->readIdx + 1;
                        p->readIdx                 = p->readIdx % 8;
                    }
                    goto end_check;
                case 0x10:
                case 0x20:
                case 0x40:
                    ret = func_8001E6AC(0, 0);
                    if (ret == 1) {
                        F12D18_80024EC0();
                        goto end_check;
                    }
                    if (ret < 2) {
                        if (ret == 0) {
                            return;
                        }
                        goto end_check;
                    }
                    if (ret == 2) {
                        CdFlush();
                        F12D18_80024EC0();
                    }
                    goto end_check;
            }
            goto end_check;
        }
    }

end_check:
    if (state->field_200 != 0) {
        func_80040820();
    }
}

void func_8001C620(void)
{
    CdCmdQueue* state;
    s32         cmd;
    s32         status;
    s16         ret;

    state = &CdCmd_Queue;
    cmd   = state->entries[state->readIdx].cmd;
    if (cmd < 0x54) {
        return;
    }
    switch (cmd) {
        case 0x54: {
            CdCmdEntry* entry;
            s32         field5;
            s32         step;

            entry  = &state->entries[state->readIdx];
            status = *(volatile u8*)&entry->param0;
            field5 = status;
            step   = state->step;
            field5 = (s8)field5;
            switch (step) {
                case 0:
                    if (state->busy == 0) {
                        state->busy             = 1;
                        Display_State.field_130 = 0xFF;
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
                state->busy             = 0;
                Display_State.field_130 = 0;
            }
            state->step      = 0;
            state->field_1fc = 0;
            state->field_222 = 0;
            state->field_242 = 0;
            if (state->readIdx != state->writeIdx) {
                u32 t;
                t                     = state->readIdx << 3;
                t                    += (u32)state;
                ((CdCmdEntry*)t)->cmd = 0;
                state->readIdx        = state->readIdx + 1;
                state->readIdx        = state->readIdx % 8;
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

s32 func_8001C970(void)
{
    CdCmdQueue* p;
    u8          cmd;

    p   = &CdCmd_Queue;
    cmd = p->entries[p->readIdx].cmd;
    if (cmd != 0) {
        p->field_40.cmd    = cmd;
        p->field_40.param0 = p->entries[p->readIdx].param0;
        p->field_40.param1 = p->entries[p->readIdx].param1;
        p->field_40.param2 = p->entries[p->readIdx].param2;
        p->field_40.idB0   = p->entries[p->readIdx].idB0;
        p->field_40.idB1   = p->entries[p->readIdx].idB1;
        p->field_40.idB2   = p->entries[p->readIdx].idB2;
        p->field_40.idB3   = p->entries[p->readIdx].idB3;
        p->field_4c        = 1;
        return 1;
    }
    if ((u16)p->field_20E != 0) {
        p->field_4c = 1;
        return 1;
    }
    return 0;
}

void func_8001CA70(void)
{
    CdCmdQueue* p;
    CdCmdQueue* q;
    u16*        statePtr;
    u16         ret;
    s32         temp;
    CdCmd190*   info;

    p = &CdCmd_Queue;
    switch (p->field_40.cmd >> 4) {
        case 0:
        case 1:
        case 2:
            p->field_4c = 0;
            return;
        case 3:
        case 4:
        case 6:
        case 7:
            if (p->field_242 != 0) {
                if ((p->field_40.cmd >> 4) == 7) {
                    func_8017D6D4();
                    return;
                }
                func_8001BE60();
                return;
            }
            statePtr = &p->field_1fc;
            switch (*statePtr) {
                case 0:
                    if (D_8006AC58 != 0) {
                        func_80026148();
                        p->field_1fc = p->field_1fc + 1;
                    } else {
                        p->field_1fc = 2;
                        goto case_2;
                    }
                    /* fallthrough */
                case 1:
                    if (func_800262A8() == 0) {
                        *statePtr = *statePtr + 1;
                    }
                    func_8001BE60();
                    ret = 0;
                    break;
                case 2:
                case_2:
                    if ((s16)func_8001F2FC(1)) {
                        ret = 1;
                    } else {
                        func_8001BE60();
                        ret = 0;
                    }
                    break;
                default:
                    ret = 0;
                    break;
            }
            if (ret != 0) {
                p->field_1E6 = 0;
                Mem_Set(&p->field_40, 0, 0x10);
                q = &CdCmd_Queue;
                if (q->busy != 0) {
                    q->busy                 = 0;
                    Display_State.field_130 = 0;
                }
                q->step      = 0;
                q->field_1fc = 0;
                q->field_222 = 0;
                q->field_242 = 0;
                if (q->readIdx != q->writeIdx) {
                    q->entries[q->readIdx].cmd = 0;
                    q->readIdx                 = q->readIdx + 1;
                    q->readIdx                 = q->readIdx % 8;
                }
            }
            return;
        case 8:
            if (p->field_242 != 0) {
                func_800AFA44();
                return;
            }
            if ((u16)p->field_20E != 0) {
                switch (p->field_1fc) {
                    case 0:
                        if (D_8007218B != 0) {
                            func_8005414C(0, 0, 0);
                        }
                        func_80056700();
                        p->field_1fc = p->field_1fc + 1;
                        return;
                    case 1:
                        if (D_80082798.field_2 == 4) {
                            info = p->field_190;
                            temp = info->field_14;
                            if (temp) {
                                func_800578E4(info->field_4 + temp);
                                p->field_1fc = p->field_1fc + 1;
                                return;
                            }
                            goto case8_cleanup;
                        }
                        return;
                    case 2:
                        if (D_80082798.field_4 == 0xA) {
                            goto case8_cleanup;
                        }
                        return;
                    default:
                        return;
                }
            } else {
            case8_cleanup:
                p = &CdCmd_Queue;
                Mem_Set(&p->field_40, 0, 0x10);
                p->field_50.cmd = 0;
                p->field_244    = 0;
                p->field_20E    = 0;
                func_800B0034(p->field_190->field_16);
                func_800B00C4();
                if (p->busy != 0) {
                    p->busy                 = 0;
                    Display_State.field_130 = 0;
                }
                p->step      = 0;
                p->field_1fc = 0;
                p->field_222 = 0;
                p->field_242 = 0;
                if (p->readIdx != p->writeIdx) {
                    p->entries[p->readIdx].cmd = 0;
                    p->readIdx                 = p->readIdx + 1;
                    p->readIdx                 = p->readIdx % 8;
                }
            }
            return;
        case 5:
        default:
            return;
    }
}

u16 func_8001CDF0(void)
{
    CdCmdQueue* p;
    u8          cmd;

    p = &CdCmd_Queue;
    if (p->field_4c != 0) {
        return 1;
    }
    cmd = p->entries[p->readIdx].cmd;
    if ((cmd >> 4) != 8) {
        if (cmd != 0) {
            goto do_work;
        }
    }
    return 0;
do_work:
    p->field_40.cmd    = cmd;
    p->field_40.param0 = p->entries[p->readIdx].param0;
    p->field_40.param1 = p->entries[p->readIdx].param1;
    p->field_40.param2 = p->entries[p->readIdx].param2;
    p->field_40.idB0   = p->entries[p->readIdx].idB0;
    p->field_40.idB1   = p->entries[p->readIdx].idB1;
    p->field_40.idB2   = p->entries[p->readIdx].idB2;
    p->field_40.idB3   = p->entries[p->readIdx].idB3;
    p->field_4c        = 2;
    return 1;
}

void func_8001CEFC(void)
{
    CdCmdQueue* p;
    CdCmdQueue* p2;
    u16*        statePtr;
    u16         ret;
    u8          result[8];
    CdlLOC      loc;
    s32         pos;

    p = &CdCmd_Queue;
    switch (p->field_40.cmd >> 4) {
        case 0:
        case 1:
        case 2:
            p->field_4c  = 0;
            p->field_212 = 0;
            break;
        case 4:
        case 6:
        case 7:
            if (p->field_242 != 0) {
                if ((p->field_40.cmd >> 4) == 7) {
                    func_8017D6D4();
                } else {
                    func_8001BE60();
                }
                break;
            }
            statePtr = &p->field_1d2;
            switch (*statePtr) {
                case 0:
                    if (D_8006AC58 != 0) {
                        func_80026148();
                        p->field_1d2 = p->field_1d2 + 1;
                    } else {
                        p->field_1d2 = 2;
                        goto case_2;
                    }
                    /* fallthrough */
                case 1:
                    if (func_800262A8() == 0) {
                        *statePtr = *statePtr + 1;
                    }
                    func_8001BE60();
                    ret = 0;
                    break;
                case 2:
                case_2:
                    if ((s16)func_8001F2FC(1)) {
                        ret = 1;
                    } else {
                        func_8001BE60();
                        ret = 0;
                    }
                    break;
                default:
                    ret = 0;
                    break;
            }
            if (ret != 0) {
                CdControlB(CdlGetlocL, NULL, result);
                loc.minute   = result[0];
                loc.second   = result[1];
                loc.sector   = result[2];
                loc.track    = 0;
                pos          = CdPosToInt(&loc);
                p2           = &CdCmd_Queue;
                p->field_48  = pos;
                p->field_4c  = 0;
                p->field_1d2 = 0;
                if (p2->busy != 0) {
                    p2->busy                = 0;
                    Display_State.field_130 = 0;
                }
                Mem_Set(p2, 0, 0x40);
                p2->writeIdx  = 0;
                p2->readIdx   = 0;
                p2->step      = 0;
                p2->field_1fc = 0;
                p2->field_1d2 = 0;
            }
            break;
        case 3:
        case 5:
        case 8:
            break;
    }
}

/* Alignment pad after the 9-entry func_8001CEFC jump table. */
static const s32 s_jtbl_pad_CEFC = 0;

u16 func_8001D0E8(void)
{
    unsigned int         new_var;
    CdCmdQueue*          p;
    register CdCmdQueue* q asm("a1");
    register CdCmdEntry* entry asm("v1");
    register u8*         a1buf asm("a0");
    u8*                  a2buf;
    u8                   sp10[4];
    u16                  ret;
    u32                  cmdHi;
    register s32         cmd asm("v0");
    u32                  t;

    p = &CdCmd_Queue;
    switch (p->field_40.cmd >> 4) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            break;
        case 6:
        case 7:
            switch (p->field_1d2) {
                case 0:
                    do {
                        sp10[3] = p->field_40.param0;
                        sp10[2] = p->field_40.param1;
                        sp10[0] = p->field_40.param2;
                        cmdHi   = p->field_40.cmd >> 4;
                        if (cmdHi == 6) {
                            q   = &CdCmd_Queue;
                            cmd = 0x62;
                            goto do_enqueue;
                        }
                        if (cmdHi != 7) {
                            goto after_enqueue;
                        }
                        q   = &CdCmd_Queue;
                        cmd = 0x73;
                    do_enqueue:
                        a1buf         = sp10;
                        t             = q->writeIdx;
                        t             = t * 8;
                        t             = t + (u32)q;
                        entry         = (CdCmdEntry*)t;
                        entry->cmd    = cmd;
                        entry->param0 = a1buf[3];
                        entry->param1 = a1buf[2];
                        entry->param2 = a1buf[0];
                        a2buf         = (u8*)&p->field_40;
                        entry->idB0   = a2buf[0];
                        entry->idB1   = a2buf[1];
                        entry->idB2   = a2buf[2];
                        entry->idB3   = a2buf[3];
                        q->writeIdx   = q->writeIdx + 1;
                        q->writeIdx   = q->writeIdx % 8;
                    after_enqueue:
                        p->field_1d2 = p->field_1d2 + 1;
                        return 0;
                    } while (0);
                case 1:
                    if (p->field_1FA != 0) {
                        goto do_clear;
                    }
                    if ((&CdCmd_Queue)->field_4c != 0) {
                        ret = 0;
                        goto check_ret;
                    }
                    if ((&CdCmd_Queue)->writeIdx != (&CdCmd_Queue)->readIdx) {
                        ret = 0;
                        goto check_ret;
                    }
                    new_var = 1;
                    ret     = new_var;
                check_ret:
                    if (ret == 0) {
                        goto return0;
                    }
                do_clear:
                    Mem_Set(&p->field_40, 0, 0x10);
                    p->field_1d2 = 0;
                    goto return1;
                return0:
                    return 0;
                default:
                    break;
            }
            break;
        case 8:
            break;
    }
return1:
    return new_var;
}

s32 CdCmd_Enqueue(s32 cmd, u8* paramA, u8* paramB)
{
    CdCmdQueue* p;
    CdCmdEntry* entry;
    u16         writeIdx;
    u16         next;

    p             = &CdCmd_Queue;
    entry         = &p->entries[p->writeIdx];
    entry->cmd    = cmd;
    entry->param0 = paramA[3];
    entry->param1 = paramA[2];
    entry->param2 = paramA[0];
    entry->idB0   = paramB[0];
    entry->idB1   = paramB[1];
    entry->idB2   = paramB[2];
    entry->idB3   = paramB[3];
    writeIdx      = p->writeIdx;
    next          = writeIdx + 1;
    p->writeIdx   = next;
    next          = p->writeIdx % 8;
    p->writeIdx   = next;
    return writeIdx;
}

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

s32 func_8001D424(void)
{
    CdCmdQueue* p;
    u16         i;
    u16         writeIdx;

    p        = &CdCmd_Queue;
    writeIdx = p->writeIdx;
    if (writeIdx == p->readIdx) {
        return 1;
    }

    i = p->readIdx + 1;
    i = i % 8;
    if (i != writeIdx) {
        do {
            p->entries[i].cmd = 0;
            i                 = i + 1;
            i                 = i % 8;
        } while (i != p->writeIdx);
    }

    p->writeIdx = p->readIdx + 1;
    p->writeIdx = p->writeIdx % 8;
    return 0;
}

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
    s32         ret;

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

    p            = &CdCmd_Queue;
    p->field_1FF = 1;
    p->field_236 = -1;
    p->field_21A = func_800AF89C(arg0, arg1, arg2, 0);
}

void func_8001D580(void)
{
}

void func_8001D588(void)
{
    CdCmd_Queue.field_50.cmd = 0;
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
    u8          sp10;

    p = &CdCmd_Queue;
    if (p->field_21A >= 0) {
        sp10         = p->field_21A;
        p->field_20E = 2;
        CdCmd_Enqueue(0x81, 0, &sp10);
    } else {
        p->field_20E = 1;
    }
}

void func_8001D628(void)
{
    CdCmdQueue* p;
    u8          sp10;

    p = &CdCmd_Queue;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        CdCmd_EnqueueReplace(0x81, 0, &sp10);
    }
}

void func_8001D66C(void)
{
    CdCmdQueue* p;
    u8          sp10;

    p = &CdCmd_Queue;
    if (p->field_21A >= 0) {
        sp10         = p->field_21A;
        p->field_20E = 2;
        CdCmd_Enqueue(0x82, 0, &sp10);
    }
}

void func_8001D6B8(void)
{
    CdCmdQueue* p;
    u8          sp10;

    p = &CdCmd_Queue;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        CdCmd_EnqueueReplace(0x82, 0, &sp10);
    }
}

void CdCmd_EnqueueReplace(s32 cmd, u8* paramA, u8* paramB)
{
    CdCmdQueue* p;
    CdCmdEntry* entry;

    p             = &CdCmd_Queue;
    entry         = &p->field_50;
    entry->cmd    = cmd;
    entry->param0 = paramA[3];
    entry->param1 = paramA[2];
    entry->param2 = paramA[0];
    entry->idB0   = paramB[0];
    entry->idB1   = paramB[1];
    entry->idB2   = paramB[2];
    entry->idB3   = paramB[3];
}

s32 func_8001D760(void)
{
    CdCmdQueue* p;
    CdCmdEntry* entry;
    u16         writeIdx;
    u16         next;
    u8          paramA[4];
    u8*         paramB;

    p = &CdCmd_Queue;
    if (*(volatile u8*)&p->field_50.cmd == 0) {
        return -1;
    }

    paramA[3] = p->field_50.param0;
    paramA[2] = p->field_50.param1;
    paramA[0] = p->field_50.param2;

    writeIdx      = p->writeIdx;
    entry         = &p->entries[writeIdx];
    entry->cmd    = p->field_50.cmd;
    entry->param0 = paramA[3];
    entry->param1 = paramA[2];
    entry->param2 = paramA[0];

    paramB      = (u8*)&p->field_50;
    entry->idB0 = paramB[0];
    entry->idB1 = paramB[1];
    entry->idB2 = paramB[2];
    entry->idB3 = paramB[3];

    p->field_50.cmd = 0;

    writeIdx    = p->writeIdx;
    next        = writeIdx + 1;
    p->writeIdx = next;
    next        = p->writeIdx % 8;
    p->writeIdx = next;
    return (s16)writeIdx;
}

INCLUDE_ASM("main/nonmatchings/cdcmd", func_8001D82C);

CdCmdEntry* func_8001D898(void)
{
    CdCmdQueue* p;
    s32         index;
    CdCmdEntry* entry;

    p     = &CdCmd_Queue;
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
        CdCmd_Queue.busy        = 1;
        Display_State.field_130 = 0xFF;
    }
}

void func_8001D90C(void)
{
    if (CdCmd_Queue.busy != 0) {
        CdCmd_Queue.busy        = 0;
        Display_State.field_130 = 0;
    }
}

void func_8001D934(void)
{
    CdCmdQueue* state;

    state = &CdCmd_Queue;
    Mem_Set(state, 0, 0x40);
    state->writeIdx  = 0;
    state->readIdx   = 0;
    state->step      = 0;
    state->field_1fc = 0;
    state->field_1d2 = 0;
}

void func_8001D97C(void)
{
    D_8006AC04 = CdCmd_Queue.readIdx;
}

void func_8001D990(s32 cmd, u8* paramA, u8* paramB)
{
    CdCmdQueue* p;
    CdCmdEntry* entry;
    u16         writeIdx;
    u16         next;

    p = &CdCmd_Queue;
    if ((p->entries[p->readIdx].cmd >> 4) != 8) {
        entry         = &p->entries[p->writeIdx];
        entry->cmd    = cmd;
        entry->param0 = paramA[3];
        entry->param1 = paramA[2];
        entry->param2 = paramA[0];
        entry->idB0   = paramB[0];
        entry->idB1   = paramB[1];
        entry->idB2   = paramB[2];
        entry->idB3   = paramB[3];
        writeIdx      = p->writeIdx;
        next          = writeIdx + 1;
        p->writeIdx   = next;
        next          = p->writeIdx % 8;
        p->writeIdx   = next;
    }
}

void func_8001DA48(void)
{
    CdCmdQueue* state;

    state = &CdCmd_Queue;
    if (state->busy != 0) {
        state->busy             = 0;
        Display_State.field_130 = 0;
    }
    state->step      = 0;
    state->field_1fc = 0;
    state->field_222 = 0;
    state->field_242 = 0;
    if (state->readIdx != state->writeIdx) {
        state->entries[state->readIdx].cmd = 0;
        state->readIdx                     = state->readIdx + 1;
        state->readIdx                     = state->readIdx % 8;
    }
}

void func_8001DAB8(void)
{
    CdCmdQueue* p;

    p                  = &CdCmd_Queue;
    p->field_40.cmd    = p->entries[p->readIdx].cmd;
    p->field_40.param0 = p->entries[p->readIdx].param0;
    p->field_40.param1 = p->entries[p->readIdx].param1;
    p->field_40.param2 = p->entries[p->readIdx].param2;
    p->field_40.idB0   = p->entries[p->readIdx].idB0;
    p->field_40.idB1   = p->entries[p->readIdx].idB1;
    p->field_40.idB2   = p->entries[p->readIdx].idB2;
    p->field_40.idB3   = p->entries[p->readIdx].idB3;
}

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
