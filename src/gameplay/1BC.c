#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libcd.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/cdaudio.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"

#define gte_rtv0_real()   __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real()   __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")
#define gte_rtir_real()   __asm__ volatile("nop; nop; .word 0x4A49E012")
#define gte_op12_real()   __asm__ volatile("nop; nop; .word 0x4B78000C")
#define gte_gpf12_real()  __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_gpl12_real()  __asm__ volatile("nop; nop; .word 0x4BA8003E")

/// `gte_MulMatrix0` from `psyq/gtemac.h`, but with the real `rtir` encoding.
#define Gp_MulMatrix0(r1, r2, r3)    \
    {                                \
        gte_SetRotMatrix(r1);        \
        gte_ldclmv(r2);              \
        gte_rtir_real();             \
        gte_stclmv(r3);              \
        gte_ldclmv((char*)(r2) + 2); \
        gte_rtir_real();             \
        gte_stclmv((char*)(r3) + 2); \
        gte_ldclmv((char*)(r2) + 4); \
        gte_rtir_real();             \
        gte_stclmv((char*)(r3) + 4); \
    }

void func_800B1EFC(Task* arg0);
void func_800B3448(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3);
void Gp_AnimSeekSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3);
void Gp_BindSlot4(Task* task);
void func_800B6398(void);
void func_8017FBD8(void);

extern char           D_80093A44[];
extern TaskFuncTable3 D_80093A1C;
extern TaskFuncTable3 D_80093A38;
extern TaskFuncTable3 D_80093A5C;
extern TaskDesc       D_80115D9C[];
extern TaskDesc       D_80119218[];
extern TaskDesc       D_8011922C[];
extern TaskDesc       D_801637C8[];
extern TaskDesc       D_8017D9E8[];
extern TaskDesc       D_80180DBC[];
extern TaskDesc       D_801810E4[];
extern TaskDesc       D_80181398[];
extern TaskDesc       D_80181638[];
extern TaskDesc       D_8018186C[];
extern TaskDesc       D_80181B30[];
extern TaskDesc       D_80181B88[];
extern TaskDesc       D_80181F18[];
extern TaskDesc       D_80182D0C[];
extern TaskDesc       D_80182E74[];
extern TaskDesc       D_80182FAC[];
extern TaskDesc       D_8018384C[];
extern s32            Gp_Slot4MsgTable[];
extern char           Gp_StrNewEnemyNull[];
extern GsCOORDINATE2  D_80070F10;
extern s32            Gp_LcgState;
extern u8             D_800626E8;
extern u16            D_80114D14;
extern s32            D_80114D20;

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AF590);

s16 Gp_FindStreamSlot(u16 arg0, u16 arg1, u16 arg2, u16 arg3)
{
    CdCmdQueue*  p;
    StreamSlot*  slot;
    u16          count;
    register s32 i asm("s2");
    register s32 found asm("t1");
    s32          temp;

    p = &CdCmd_Queue;
    if (arg0 == 0) {
        slot  = (StreamSlot*)Fs_Streams;
        count = 0xA;
    } else {
        slot  = Stream_Slots;
        count = 0xF;
    }

    i     = 0;
    found = i;
    for (; (u16)i < count; i++, slot++) {
        if (slot->field_0 == 2) {
            if (slot->field_4 != 0) {
                if (slot->field_C == arg0) {
                    if (slot->field_E == arg1) {
                        if (slot->field_10 == arg2) {
                            if (slot->field_12 == arg3) {
                                found = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    {
        register u32 valid asm("v0");
        valid = found & 0xFFFF;
        if (valid == 0) {
            return -1;
        }
    }

    Mem_Set(p->field_58, 0, 0x12C);
    p->field_190 = (CdCmd190*)slot;
    p->field_216 = 1;
    temp         = slot->field_8;
    if (temp != 0) {
        p->field_188 = temp;
    } else {
        p->field_188 = 0;
    }
    p->field_23A = 0;
    p->field_21E = 0;
    p->field_218 = 0;
    p->field_21C = 0;
    D_80114D14   = 0;
    p->field_238 = slot->field_18;
    p->field_1A8 = Gp_LcgState;
    p->field_1AC = rand();
    Gp_LcgState  = 0;
    srand(1);
    D_80114D20 = 0xFFFF;
    return i;
}

void Gp_StepCdAudioCmd(void)
{
    register s32 one asm("s0");
    register s32 i_s1 asm("s1");
    CdCmdQueue*  p;
    s32          seed;
    s16          ret;
    s32          save23;

    p = &CdCmd_Queue;
    {
        s32 cmd;
        cmd = p->entries[p->readIdx].cmd;
        if (cmd == 0) {
            goto end_check;
        }
        if (cmd < 0) {
            goto end_check;
        }
        if (cmd >= 0x83) {
            goto end_check;
        }
        if (cmd < 0x81) {
            goto end_check;
        }
    }

    switch (p->step) {
        case 0:
            CdCmd_SetBusy();
            p->field_20E = 2;
            ret          = CdCmd_PollStatus(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    break;
                }
                if (ret != 2) {
                    break;
                }
                CdFlush();
            }
            if (p->field_212 == 0) {
                p->step = p->step + 1;
                break;
            }
            p->step = 6;
            goto case6;
        case 1:
        case 2:
            p->step = p->step + 1;
            break;
        case 3: {
            register CdCmd190* info asm("a0");
            register s32       size asm("a1");
            register s32       temp asm("v0");
            s32                extra;
            s32                end;

            info         = p->field_190;
            p->field_242 = 1;
            if (info->field_3 != 0) {
                size = info->field_1C;
                info = (CdCmd190*)info->field_4;
                temp = size - 1;
                if (temp < 0) {
                    extra = size + 0x7FE;
                } else {
                    extra = temp;
                }
                extra = extra >> 11;
                if (extra != 0) {
                    info = (CdCmd190*)((s32)info + 1);
                    info = (CdCmd190*)((s32)info + extra);
                }
                end = (s32)info;
                Fs_ReadSectorEx(p->field_190->field_4, end, p->field_1A4, 0);
                p->step = p->step + 1;
            } else {
                p->step = 5;
            }
            break;
        }
        case 4:
            if (Fs_CdOpStatus != 0xFF) {
                break;
            }
            ret = CdCmd_PollStatus(0, 0);
            if (ret != 1) {
                if (ret < 2) {
                    if (ret == 0) {
                        return;
                    }
                    break;
                }
                if (ret != 2) {
                    break;
                }
                CdFlush();
                p->step = 3;
                break;
            }
            p->step = p->step + 1;
            break;
        case 5: {
            CdCmd190*     info;
            s32           sector;
            s32           bits;
            u16           maskbits;
            GpSndMaskRec* entry;

            info   = p->field_190;
            sector = info->field_4;
            if (info->field_3 != 0) {
                sector += 1;
                sector += (info->field_1C - 1) / 0x800;
            }
            CdAudio_StartTrack(sector, p->field_190->field_2);
            i_s1     = 0;
            maskbits = p->field_190->field_16;
            if (Gp_SndMaskTable[0].mask != 0) {
                bits = maskbits;
                do {
                    entry = &Gp_SndMaskTable[(u16)i_s1];
                    if (bits & entry->mask) {
                        SndEvt_EnqueueType7(entry->flags, 0);
                        SndBank_SetEnableFlags(0, entry->flags);
                    }
                    i_s1++;
                } while (Gp_SndMaskTable[(u16)i_s1].mask != 0);
            }
            p->field_248 = 0;
            p->field_244 = 1;
            p->step      = p->step + 1;
            break;
        }
        case 6:
        case6: {
            s32 cmd;

            if (CdAudio_Phase.field_0 != 3) {
                break;
            }
            one          = 1;
            p->field_212 = one;
            cmd          = p->entries[p->readIdx].cmd;
            if (cmd == 0x82) {
                CdCmd_LoadActiveEntry();
                CdCmd_AdvanceRead();
                break;
            }
            if (cmd != 0x81) {
                break;
            }
            save23       = Mc_SaveData.field_23;
            p->field_20E = one;
            if (save23 != 0) {
                SndEvt_EnqueueType6(0, 0, 0);
            }
            if (p->field_190->field_3 != 0) {
                p->field_240 = one;
            }
            p->field_1A0 = 0;
            CdAudio_RequestStopB();
            p->field_244 = one;
            p->field_242 = 0;
            p->step      = p->step + 1;
            break;
        }
        case 7: {
            CdCmd190*     info;
            s32           i;
            s32           bits;
            u16           maskbits;
            GpSndMaskRec* entry;

            if (CdAudio_Phase.field_1 != 4) {
                break;
            }
            if (Mc_SaveData.field_23 != 0) {
                SndEvt_EnqueueType6(0, 0, 0);
            }
            Mem_Set(&p->field_40, 0, 0x10);
            info            = p->field_190;
            p->field_50.cmd = 0;
            if (info->field_14 != 0) {
                CdAudio_JumpToSector(info->field_4 + info->field_14);
                p->field_242 = 1;
                p->step      = p->step + 1;
                break;
            }
            i        = 0;
            maskbits = info->field_16;
            if (Gp_SndMaskTable[0].mask != 0) {
                bits = maskbits;
                do {
                    entry = &Gp_SndMaskTable[(u16)i];
                    if (bits & entry->mask) {
                        SndBank_SetEnableFlags(1, entry->flags);
                    }
                    i++;
                } while (Gp_SndMaskTable[(u16)i].mask != 0);
            }
            {
                CdCmdQueue* q;
                s32         ff;
                q            = &CdCmd_Queue;
                seed         = q->field_1AC;
                ff           = 0xFF;
                p->field_244 = 0;
                p->field_20E = 0;
                q->field_1FE = ff;
                q->field_23A = 1;
                q->field_214 = 0;
                q->field_212 = 0;
                q->field_216 = 0;
                q->field_240 = 0;
                Gp_LcgState  = q->field_1A8;
                srand(seed);
            }
            CdCmd_AdvanceRead();
            break;
        }
        case 8: {
            s32           i;
            s32           bits;
            u16           maskbits;
            GpSndMaskRec* entry;

            if (CdAudio_Phase.field_4 != 0xA) {
                break;
            }
            i        = 0;
            maskbits = p->field_190->field_16;
            if (Gp_SndMaskTable[0].mask != 0) {
                bits = maskbits;
                do {
                    entry = &Gp_SndMaskTable[(u16)i];
                    if (bits & entry->mask) {
                        SndBank_SetEnableFlags(1, entry->flags);
                    }
                    i++;
                } while (Gp_SndMaskTable[(u16)i].mask != 0);
            }
            {
                CdCmdQueue* q;
                s32         ff;
                q            = &CdCmd_Queue;
                seed         = q->field_1AC;
                ff           = 0xFF;
                p->field_242 = 0;
                p->field_244 = 0;
                p->field_20E = 0;
                q->field_1FE = ff;
                q->field_23A = 1;
                q->field_214 = 0;
                q->field_212 = 0;
                q->field_216 = 0;
                q->field_240 = 0;
                Gp_LcgState  = q->field_1A8;
                srand(seed);
            }
            CdCmd_AdvanceRead();
            break;
        }
    }

end_check:
    CdCmd_StepVlcRebuild();
}

void Gp_ApplySndMasks(u16 arg0)
{
    s32           i;
    s32           bits;
    GpSndMaskRec* entry;

    i = 0;
    if (Gp_SndMaskTable[0].mask != 0) {
        bits = arg0;
        do {
            entry = &Gp_SndMaskTable[(u16)i];
            if (bits & entry->mask) {
                SndEvt_EnqueueType7(entry->flags, 0);
                SndBank_SetEnableFlags(0, entry->flags);
            }
            i++;
        } while (Gp_SndMaskTable[(u16)i].mask != 0);
    }
}

void Gp_ApplySndBankMasks(u16 arg0)
{
    s32           i;
    s32           bits;
    GpSndMaskRec* entry;

    i = 0;
    if (Gp_SndMaskTable[0].mask != 0) {
        bits = arg0;
        do {
            entry = &Gp_SndMaskTable[(u16)i];
            if (bits & entry->mask) {
                SndBank_SetEnableFlags(1, entry->flags);
            }
            i++;
        } while (Gp_SndMaskTable[(u16)i].mask != 0);
    }
}

void Gp_RestoreStreamRng(void)
{
    CdCmdQueue* p;

    p            = &CdCmd_Queue;
    p->field_1FE = 0xFF;
    p->field_23A = 1;
    p->field_214 = 0;
    p->field_212 = 0;
    p->field_216 = 0;
    p->field_240 = 0;
    Gp_LcgState  = p->field_1A8;
    srand(p->field_1AC);
}

s32 func_800B0118(s32 arg0, s32 arg1)
{
    s16 temp;

    temp = arg0;
    if (temp != 0) {
        D_80114D20  = temp;
        D_8005EC80 |= 8;
    } else {
        D_8005EC80 &= ~8;
    }
    return 0;
}

void Gp_SetStreamBuf(void* arg0)
{
    CdCmd_Queue.field_198 = arg0;
}

GpEnemy* Gp_SpawnEnemy(s32 bank, s32 type, s32 arg2, GpEnemy* parent)
{
    Task*    task;
    GpEnemy* ret;

    task = Task_Spawn(bank, type, arg2, 0);
    if (task != NULL) {
        ret = Gp_AllocEnemy(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

GpEnemy* Gp_SpawnEnemyFromTable(TaskDesc* table, s32 idx, s32 arg2, GpEnemy* parent)
{
    Task*    task;
    GpEnemy* ret;

    task = Task_SpawnFromTable(table, idx, arg2, 0);
    if (task != NULL) {
        ret = Gp_AllocEnemy(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

void Gp_DestroyEnemy(GpEnemy* enemy, Task* task)
{
    Gp_UnlinkNode(&enemy->node);
    Mem_Free(enemy);
    Task_Kill(task);
}

void Gp_EnemyTaskExit(Task* task)
{
    GpEnemy* enemy;

    enemy = task->spawnArg2;
    Gp_UnlinkNode(&enemy->node);
    Mem_Free(enemy);
    Task_Kill(task);
}

Task* Gp_CopyCoordOffset(Task* arg0, GsCOORDINATE2* arg1, SVECTOR* arg2)
{
    GameActorExt*  extra;
    GsCOORDINATE2* dest;
    GsCOORDINATE2* world;

    if (arg0 == NULL) {
        return NULL;
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - 8;
    world                   = &D_80070F10;
    extra                   = (GameActorExt*)arg0->extra;
    dest                    = (GsCOORDINATE2*)extra->field_8;
    if (arg1->sub == world) {
        dest->coord = arg1->coord;
        gte_SetRotMatrix(&arg1->coord);
        gte_SetTransMatrix(&arg1->coord);
        gte_ldv0(arg2);
        gte_rtv0tr_real();
        gte_stlvnl(dest->coord.t);
    } else {
        Gp_UpdateCoord(arg1);
        dest->workm = arg1->workm;
        gte_SetRotMatrix(&arg1->workm);
        gte_SetTransMatrix(&arg1->workm);
        gte_ldv0(arg2);
        gte_rtv0tr_real();
        gte_stlvnl(dest->workm.t);
        Gp_WorldToLocal(&world->workm, &dest->workm, &dest->coord);
    }
    dest->sub               = &D_80070F10;
    dest->flg               = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
    return arg0;
}

GpEnemy* Gp_AllocEnemy(Task* task, GpEnemy* parent)
{
    GpEnemy* enemy;

    enemy = Mem_Calloc(0x60, 0);
    if (enemy == NULL) {
        printf(Gp_StrNewEnemyNull);
        Task_Kill(task);
        return NULL;
    }

    task->exitCallback = Gp_EnemyTaskExit;
    task->spawnArg2    = enemy;
    enemy->task        = task;
    enemy->field_18    = &D_80070F10;
    if (parent != NULL) {
        Task_Reparent(parent->task, task);
    } else {
        Task_Reparent(Game_GetPtrSlot(4), enemy->task);
    }
    return enemy;
}

void Gp_EnemyWaitStart(GpEnemy* enemy, Task* task)
{
    enemy->field_C = 0x78;
    task->state++;
}

void Gp_EnemyWaitTick(GpEnemy* enemy, Task* task)
{
    enemy->field_C--;
    if (enemy->field_C == 0) {
        task->state++;
    }
}

void Gp_EnemyDispatch(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Gp_EnemyWaitFuncs;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

s32 Gp_TryEnqueueSndCd(s32 arg0)
{
    u8 param1[8];
    u8 param2[8];

    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[0] = arg0;
        param1[3] = 0;
        param1[2] = 5;
        param2[0] = 1;
        param2[1] = 1;
        param2[3] = 0;
        param2[2] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        D_800626E8 = 1;
        return 0;
    }
    return 0xFF;
}

void Gp_EnqueueSndCd(u8 arg0)
{
    u8  param1[8];
    u8  param2[8];
    s32 flag;

    if (Game_Session->field_129 != arg0) {
        SndEvt_EnqueueType7(0xE0000000, 8);
        flag      = 1;
        param1[3] = 0;
        param1[2] = 5;
        param1[0] = arg0;
        param2[0] = flag;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        D_800626E8              = flag;
        Game_Session->field_129 = arg0;
    }
}

void func_800B06F0(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80093A1C;
    sp.funcs[arg0->state](arg0);
}

void Gp_StartStageLoad(Task* task)
{
    s32           i;
    u8            param1[8];
    u8            param2[8];
    FsFolderSlot* table;
    s32           fileId;

    if (Midi_IsBusy(0) == 0) {
        Display_State.field_12f = 1;
        i                       = 0;
        table                   = D_8006C338;
        do {
            table[(u8)i].field_0 = 0;
            i++;
        } while ((u8)i < 0x32);

        fileId = 0xA;
        if (Game_Session->field_128 != 0xFF) {
            param1[2] = 4;
            param1[0] = 0x62;
            param1[3] = 0;
            param2[0] = 1;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
            fileId = 9;
        }
        CdCmd_EnqueueLoadFile(fileId, 0, 3);
        Display_State.field_104 = 0;
        task->state++;
    }
}

void Gp_FinishStageLoad(Task* task)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        Display_State.field_100 = 1;
        if (Game_Session->field_128 == 0xFF) {
            Task_SpawnFromTable(D_8011922C, 0, 0, 0);
            Task_Kill(task);
        } else {
            task->spawnArg2 = Task_SpawnFromTable(D_80115D9C, 0, 0, 0);
            SndEvt_EnqueueType1(0x62, 0);
        }
        task->state++;
    }
}

void func_800B08D8(Task* task)
{
    s32           out;
    DisplayState* ds;

    if (Task_PollKill(task->spawnArg2, &out) != 0) {
        ds                  = &Display_State;
        task->killCountdown = 0;
        ds->field_11e       = 1;
        ds->field_12f       = 0;
        Task_Kill(task);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0928);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0CF4);

void Gp_MtxToEuler(MATRIX* arg0, SVECTOR* arg1)
{
    SVECTOR in;
    SVECTOR out;
    MATRIX  mtx;
    s32     one;
    s16     len;

    mtx      = *arg0;
    one      = 0x1000;
    mtx.t[2] = 0;
    mtx.t[1] = 0;
    mtx.t[0] = 0;
    in.vx    = 0;
    in.vy    = 0;
    in.vz    = one;
    ApplyMatrixSV(&mtx, &in, &out);
    arg1->vx = -ratan2(out.vy, out.vz);
    len      = SquareRoot12((out.vz * out.vz + out.vy * out.vy) >> 12);
    arg1->vy = ratan2(out.vx, len);
    in.vx    = 0;
    in.vy    = one;
    in.vz    = 0;
    ApplyMatrixSV(&mtx, &in, &out);
    in.vx = -arg1->vx;
    in.vy = -arg1->vy;
    in.vz = 0;
    RotMatrixZYX(&in, &mtx);
    ApplyMatrixSV(&mtx, &out, &in);
    arg1->vz = -ratan2(in.vx, in.vy);
}

SVECTOR* Gp_ExtractEuler(SVECTOR* arg0, MATRIX* arg1)
{
    SVECTOR ang0;
    SVECTOR ang1;
    s32     cos0;
    s32     sin0;
    s32     cos1;
    s32     sin1;
    s16     vx0;
    s16     vx1;

    vx0     = -ratan2(arg1->m[1][2], arg1->m[2][2]);
    ang0.vx = vx0;
    if (vx0 <= 0) {
        vx1 = vx0 + 0x800;
    } else {
        vx1 = vx0 - 0x800;
    }
    ang1.vx = vx1;

    sin0 = rsin(ang0.vx);
    cos0 = rcos(ang0.vx);
    sin1 = rsin(ang1.vx);
    cos1 = rcos(ang1.vx);

    ang0.vy = ratan2(arg1->m[0][2], (arg1->m[2][2] * cos0) / 4096 - (arg1->m[1][2] * sin0) / 4096);
    ang1.vy = ratan2(arg1->m[0][2], (arg1->m[2][2] * cos1) / 4096 - (arg1->m[1][2] * sin1) / 4096);

    {
        register s32 t1 asm("v1");
        register s32 m asm("v0");
        m  = arg1->m[1][0];
        t1 = (m * cos0) / 4096;
        {
            register s32 prod asm("a2");
            register s32 y asm("a1");
            m    = arg1->m[2][0];
            prod = m * sin0;
            if (prod < 0) {
                prod += 0xFFF;
            }
            m       = prod >> 12;
            y       = t1 + m;
            ang0.vz = ratan2(y, (arg1->m[1][1] * cos0) / 4096 + (arg1->m[2][1] * sin0) / 4096);
        }
    }

    {
        register s32 tmp asm("v0");
        register s32 ax asm("a1");
        register s32 ay asm("a0");
        register s32 az asm("v1");

        tmp     = ratan2((arg1->m[1][0] * cos1) / 4096 + (arg1->m[2][0] * sin1) / 4096,
                         (arg1->m[1][1] * cos1) / 4096 + (arg1->m[2][1] * sin1) / 4096);
        ang1.vz = tmp;
        tmp     = (s16)tmp;
        if (tmp < 0) {
            tmp = -tmp;
        }

        ax = ang0.vx;
        ay = ang0.vy;
        az = ang0.vz;
        if (ax < 0) {
            ax = -ax;
        }
        if (ay < 0) {
            ay = -ay;
        }
        ax += ay;
        TOUCH_REG(ax);
        if (az < 0) {
            az = -az;
        }
        TOUCH_REG2(az, ax);
        sin0 = ax + az;

        ay = ang1.vx;
        az = ang1.vy;
        if (ay < 0) {
            ay = -ay;
        }
        if (az < 0) {
            az = -az;
        }
        ay  += az;
        cos0 = ay + tmp;

        if (sin0 < cos0) {
            *arg0 = ang0;
        } else {
            *arg0 = ang1;
        }
    }
    return arg0;
}

void Gp_LerpOrthonormal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2, s32 arg3)
{
    MATRIX mtx;
    MATRIX diffs;
    VECTOR vec[3];
    VECTOR tmp;
    VECTOR nrm;
    s32    i;
    s32    best;
    s32    len;
    s32    ret;

    best = 0;
    for (i = 0; i < 3; i++) {
        diffs.m[i][0] = arg1->m[i][0] - arg0->m[i][0];
        diffs.m[i][1] = arg1->m[i][1] - arg0->m[i][1];
        diffs.m[i][2] = arg1->m[i][2] - arg0->m[i][2];
    }
    for (i = 0; i < 3; i++) {
        vec[i].vx = arg0->m[i][0] + (diffs.m[i][0] * arg3) / ONE;
        vec[i].vy = arg0->m[i][1] + (diffs.m[i][1] * arg3) / ONE;
        vec[i].vz = arg0->m[i][2] + (diffs.m[i][2] * arg3) / ONE;
    }

    len = -1;

    gte_ldopv1(&vec[0]);
    gte_ldopv2(&vec[1]);
    gte_op12_real();
    gte_stlvnl(&tmp);
    ret = VectorNormal(&tmp, &nrm);
    if (len < ret) {
        len  = ret;
        best = 2;
    }

    gte_ldopv1(&vec[1]);
    gte_ldopv2(&vec[2]);
    gte_op12_real();
    gte_stlvnl(&tmp);
    ret = VectorNormal(&tmp, &nrm);
    if (len < ret) {
        len  = ret;
        best = 0;
    }

    gte_ldopv1(&vec[0]);
    gte_ldopv2(&vec[2]);
    gte_op12_real();
    gte_stlvnl(&tmp);
    if (len < VectorNormal(&tmp, &nrm)) {
        best = 1;
    }

    switch (best) {
        case 0:
            mtx.m[1][0] = vec[1].vx;
            mtx.m[1][1] = vec[1].vy;
            mtx.m[1][2] = vec[1].vz;
            mtx.m[2][0] = vec[2].vx;
            mtx.m[2][1] = vec[2].vy;
            mtx.m[2][2] = vec[2].vz;
            MatrixNormal_1(&mtx, arg2);
            break;
        case 1:
            mtx.m[0][0] = vec[0].vx;
            mtx.m[0][1] = vec[0].vy;
            mtx.m[0][2] = vec[0].vz;
            mtx.m[2][0] = vec[2].vx;
            mtx.m[2][1] = vec[2].vy;
            mtx.m[2][2] = vec[2].vz;
            MatrixNormal_2(&mtx, arg2);
            break;
        case 2:
            mtx.m[0][0] = vec[0].vx;
            mtx.m[0][1] = vec[0].vy;
            mtx.m[0][2] = vec[0].vz;
            mtx.m[1][0] = vec[1].vx;
            mtx.m[1][1] = vec[1].vy;
            mtx.m[1][2] = vec[1].vz;
            MatrixNormal_0(&mtx, arg2);
            break;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B17D4);

void Gp_ComposeParentWorld(GsCOORDINATE2* arg0, MATRIX* arg1, SVECTOR* arg2)
{
    SVECTOR tmp;
    MATRIX* m;
    s32     one;

    if (arg0->sub != &D_80070F10) {
        Gp_ComposeParentWorld(arg0->sub, arg1, arg2);
    } else {
        one                = ONE;
        m                  = arg1;
        *(s32*)m           = one;
        *(s32*)&m->m[0][2] = 0;
        *(s32*)&m->m[1][1] = one;
        *(s32*)&m->m[2][0] = 0;
        m->m[2][2]         = one;
        arg2->vx           = 0;
        arg2->vy           = 0;
        arg2->vz           = 0;
    }

    tmp.vx = *(u16*)&arg0->coord.t[0];
    tmp.vy = *(u16*)&arg0->coord.t[1];
    tmp.vz = *(u16*)&arg0->coord.t[2];
    gte_SetRotMatrix(arg1);
    gte_ldv0(&tmp);
    gte_rtv0_real();
    gte_stsv(&tmp);
    arg2->vx += tmp.vx;
    arg2->vy += tmp.vy;
    arg2->vz += tmp.vz;
    gte_ldclmv(&arg0->coord);
    gte_rtir_real();
    gte_stclmv(arg1);
    gte_ldclmv(&arg0->coord.m[0][1]);
    gte_rtir_real();
    gte_stclmv(&arg1->m[0][1]);
    gte_ldclmv(&arg0->coord.m[0][2]);
    gte_rtir_real();
    gte_stclmv(&arg1->m[0][2]);
}

void func_800B1EFC(Task* arg0)
{
    TILE*          p;
    DR_TPAGE*      dr;
    s8             yoff;
    register Task* t asm("t1");
    register s32   color asm("a2");
    register s32   y asm("a0");
    register s32   scaled asm("v0");

    t = arg0;
    if (t->spawnArg1 > 0) {
        if (t->killCountdown > 0) {
            t->killCountdown--;
            scaled = (u8)t->killCountdown << 3;
            color  = ~scaled;
        } else {
            t->spawnArg1--;
            color = 0xFF;
        }
    } else {
        t->killCountdown++;
        scaled = (u8)t->killCountdown << 3;
        color  = ~scaled;
        if (t->killCountdown >= 0x1F) {
            t->state++;
        }
    }

    p          = (TILE*)D_80071190;
    y          = -0x78;
    D_80071190 = (DR_TPAGE*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->x0      = -0xA0;
    p->y0      = y;
    yoff       = Display_State.vramYOffset;
    p->b0      = color;
    p->g0      = color;
    p->r0      = color;
    dr         = (DR_TPAGE*)D_80071190;
    p->w       = 0x140;
    p->h       = 0xF0;
    p->y0      = y - yoff;
    D_80071190 = dr + 1;
    if (t->spawnArg2 == 0) {
        setlen(dr, 1);
        dr->code[0] = 0xE1000240;
    } else {
        setlen(dr, 1);
        dr->code[0] = 0xE1000220;
    }
    addPrim(Gpu_CurrentOt, p);
    addPrim(Gpu_CurrentOt, dr);
}

void Gp_BlendRgb555(u16* arg0, u16* arg1, s32 arg2, u16* arg3)
{
    void**        scratch;
    u8*           head;
    GpRgbScratch* c0;
    GpRgbScratch* c1;
    GpRgbScratch* out;
    u16           color;
    u16           packed;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    c0       = (GpRgbScratch*)(head - 0x18);
    *scratch = c0;

    color = *arg0;
    c0->b = color;
    c0->g = color;
    c0->r = (color & 0x1F) << 7;
    c0->g = (c0->g << 2) & 0xF80;
    c0->b = (c0->b >> 3) & 0xF80;

    c1    = (GpRgbScratch*)(head - 0x10);
    color = *arg1;
    c1->b = color;
    c1->g = color;
    c1->r = (color & 0x1F) << 7;
    c1->g = (c1->g << 2) & 0xF80;
    c1->b = (c1->b >> 3) & 0xF80;

    gte_lddp(arg2);
    gte_ldsv(c0);
    gte_gpf12_real();
    gte_lddp(0x1000 - arg2);
    gte_ldsv(c1);
    gte_gpl12_real();
    out = (GpRgbScratch*)(head - 8);
    gte_stsv(out);

    packed = ((out->b >> 2) & 0x3E0) | ((out->g >> 7) & 0x1F);
    packed = (packed << 5) | ((out->r >> 7) & 0x1F);
    *arg3  = packed;
    if ((s16)*arg0 < 0 || (s16)*arg1 < 0) {
        *arg3 = packed | 0x8000;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

void func_800B2200(Task* arg0)
{
    register Task* t asm("t2");
    register s32   y asm("a1");
    GpFadeWork*    work;
    TILE*          tile;
    DR_TPAGE*      dr;
    s32            color;
    s8             yoff;

    t    = arg0;
    work = t->spawnArg2;

    if (t->state == 0) {
        t->killCountdown = 0;
        if (work->field_2 <= 0) {
            work->field_2 = 0x20;
        }
        t->state = t->state + 1;
    }
    if ((t->state == 2) && (work->field_1 == 1)) {
        t->killCountdown = work->field_2;
    }

    color      = (t->killCountdown * 0xFF0) / work->field_2;
    tile       = (TILE*)D_80071190;
    y          = -0x78;
    tile->y0   = y;
    D_80071190 = (DR_TPAGE*)(tile + 1);
    setlen(tile, 3);
    setcode(tile, 0x62);
    tile->x0 = -0xA0;
    yoff     = Display_State.vramYOffset;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    color    = color >> 4;
    tile->b0 = color;
    tile->g0 = color;
    tile->r0 = color;
    dr       = (DR_TPAGE*)D_80071190;
    tile->y0 = y - yoff;

    D_80071190 = dr + 1;
    if (work->field_0 == 0) {
        setlen(dr, 1);
        dr->code[0] = 0xE1000240;
    } else {
        setlen(dr, 1);
        dr->code[0] = 0xE1000220;
    }

    if (t->spawnArg1 != 0) {
        u_long* ot;

        ot = Gpu_CurrentOt;
        addPrim(&ot[t->spawnArg1], tile);
        addPrim(&ot[t->spawnArg1], dr);
    } else {
        u_long* ot;

        ot = Gpu_CurrentOt;
        if ((ot == (u_long*)Gpu_OrderingTables[0].org) || (ot == (u_long*)Gpu_OrderingTables[1].org)) {
            addPrim(ot, tile);
            addPrim(ot, dr);
        } else {
            addPrim(&ot[-0xA], tile);
            addPrim(&ot[-0xA], dr);
        }
    }

    switch (t->state) {
        case 1:
            t->killCountdown = t->killCountdown + 1;
            if (t->killCountdown == work->field_2) {
                t->state = t->state + 1;
            }
            break;
        case 2:
            if (work->field_1 == 1) {
                t->state = 3;
            }
            break;
        case 3:
            t->killCountdown = t->killCountdown - 1;
            if (t->killCountdown <= 0) {
                work->field_1 = 2;
                Task_Kill(t);
            }
            break;
        default:
            Task_Kill(t);
            break;
    }
}

void func_800B25B0(void)
{
    switch (*(u32*)&Mc_SaveData.field_4 & 0xFFFF0000) {
        case 0x51B0000:
            Task_SpawnFromTable(D_80181F18, 0, 0, 0);
            break;
        case 0x50F0000:
            Task_SpawnFromTable(D_80181398, 0, 0, 0);
            break;
        case 0x50E0000:
            Task_SpawnFromTable(D_80181B30, 0, 0, 0);
            break;
        case 0x50D0000:
            Task_SpawnFromTable(D_8018384C, 0, 0, 0);
            break;
        case 0x50C0000:
            Task_SpawnFromTable(D_801810E4, 1, 0, 0);
            break;
        case 0x5070000:
            Task_SpawnFromTable(D_80180DBC, 0, 0, 0);
            break;
        case 0x21E0000:
            Task_SpawnFromTable(D_80182D0C, 0, 1, 0);
            break;
        case 0x31E0000:
            Task_SpawnFromTable(D_80182E74, 0, 1, 0);
            break;
        case 0x4120000:
            Task_SpawnFromTable(D_80181B88, 0, 0, 0);
            break;
        case 0x51F0000:
            Task_SpawnFromTable(D_8017D9E8, 0, 0, 0);
            break;
        case 0x51E0000:
            Task_SpawnFromTable(D_8018186C, 0, 0, 0);
            Task_SpawnFromTable(D_8018186C, 1, 0, 0);
            break;
        case 0x51D0000:
            Task_SpawnFromTable(D_80181638, 0, 0, 0);
            break;
        case 0x4160000:
            Task_SpawnFromTable(D_801637C8, 0, 0, 0);
            break;
        case 0x4300000:
            Task_SpawnFromTable(D_80182FAC, 0, 0, 0);
            break;
        case 0x1140000:
            func_8017FBD8();
            break;
    }
}

void Gp_BlendRgb555Clut(u16* arg0, u16* arg1, s32 arg2, u16* arg3)
{
    s32 i;

    for (i = 0; i < 0x10; i++) {
        Gp_BlendRgb555(arg0, arg1, arg2, arg3);
        arg0++;
        arg1++;
        arg3++;
    }
}

void Gp_BlendRgb555ClutMasked(u16* arg0, u16* arg1, s32 arg2, u16* arg3, s32 arg4)
{
    s32 i;

    for (i = 0; i < 0x10; i++) {
        if ((1 << i) & arg4) {
            Gp_BlendRgb555(arg0, arg1, arg2, arg3);
        }
        arg0++;
        arg1++;
        arg3++;
    }
}

void func_800B28E0(Task* task)
{
    task->killCountdown = 0x20;
    task->state++;
    func_800B1EFC(task);
}

void func_800B2910(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80093A38;
    sp.funcs[arg0->state](arg0);
}

Task* func_800B2968(void)
{
    return Task_SpawnFromTable(D_80119218, 0, 0, 0);
}

void Gp_BlendAnimRot(GpAnimBlendSrc* arg0, GpAnimMtxRec* arg1, GpAnimSlot* arg2,
                     GpAnimScratch80* s)
{
    if (arg2->field_17 != 0) {
        RotMatrix_gte(&s->vec0, &s->mtx0);
        if (arg0->field_10 == 1) {
            RotMatrix_gte(&s->vec1, &s->mtx1);
            TransposeMatrix(&s->mtx0, &s->mtx2);
            Gp_MulMatrix0(&s->mtx1, &s->mtx2, &s->mtx2);
            Gfx_MatrixToEuler(&s->mtx2, &arg2->field_18);
        }
        gte_lddp(s->invBlend);
        gte_ldsv(&arg2->field_18);
        gte_gpf12_real();
        gte_stsv(&s->vec1);
        RotMatrix_gte(&s->vec1, &s->mtx2);
        if (arg0->field_C == NULL) {
            Gp_MulMatrix0(&s->mtx2, &s->mtx0, &arg1->mtx);
            if (arg0->field_8 != NULL) {
                Gfx_MatrixToEuler(&arg1->mtx, &s->vec1);
            }
            arg1->field_0 = 0;
        } else {
            Gp_MulMatrix0(&s->mtx2, &s->mtx0, &s->mtx2);
            Gfx_MatrixToEuler(&s->mtx2, &s->vec1);
            arg0->field_C->rot = s->vec1;
        }
    } else {
        gte_lddp(s->blend);
        gte_ldsv(&s->vec0);
        gte_gpf12_real();
        gte_lddp(s->invBlend);
        gte_ldsv(&s->vec1);
        gte_gpl12_real();
        gte_stsv(&s->vec1);
        if (arg0->field_C == NULL) {
            RotMatrix_gte(&s->vec1, &arg1->mtx);
            arg1->field_0 = 0;
        } else {
            arg0->field_C->rot = s->vec1;
        }
    }
}

void Gp_AnimBlendPose(GpAnimBlendSrc* arg0, GpAnimMtxRec* arg1, GpAnimSlot* arg2)
{
    register void**           scratch asm("v1");
    register GpAnimScratch80* tmp asm("v0");
    register s32              blend asm("v1");
    register GpPackedPose*    dest asm("v1");
    GpPackedPose*             p;
    GpAnimScratch80*          s;
    s32                       inv;
    s32                       z;

    if (arg2->field_E != 0) {
        scratch  = (void**)G_SCRATCH_HEAD;
        tmp      = *scratch;
        tmp     -= 1;
        *scratch = tmp;
        if (arg0->field_0 != arg0->field_4) {
            s        = tmp;
            blend    = (s16)arg2->field_C << 12;
            s->blend = blend;
            blend    = blend / (s32)arg2->field_E;
            inv      = 0x1000 - blend;
            s->blend = blend;
        } else {
            s        = tmp;
            inv      = 0x1000;
            s->blend = 0;
        }
        s->invBlend = inv;
        COMPILER_BARRIER();
        gte_lddp(s->blend);
        gte_ldsv(arg0->field_0);
        gte_gpf12_real();
        gte_lddp(s->invBlend);
        gte_ldsv(arg0->field_4);
        gte_gpl12_real();
        gte_stsv(&s->trans);
        if (arg0->field_C == NULL) {
            arg1->mtx.t[0] = s->trans.vx;
            arg1->mtx.t[1] = s->trans.vy;
            z              = s->trans.vz;
            arg1->field_0  = 0;
            arg1->mtx.t[2] = z;
        } else {
            arg0->field_C->trans.vx = s->trans.vx;
            arg0->field_C->trans.vy = s->trans.vy;
            arg0->field_C->trans.vz = s->trans.vz;
        }
        p          = (GpPackedPose*)arg0->field_0;
        s->vec0.vx = p->rx;
        s->vec0.vy = p->ry;
        s->vec0.vz = p->rz;
        p          = (GpPackedPose*)arg0->field_4;
        s->vec1.vx = p->rx;
        s->vec1.vy = p->ry;
        s->vec1.vz = p->rz;
        Gp_BlendAnimRot(arg0, arg1, arg2, s);
        dest = (GpPackedPose*)arg0->field_8;
        if (dest != NULL) {
            dest->vx = s->trans.vx;
            dest->vy = s->trans.vy;
            dest->vz = s->trans.vz;
            dest->rx = s->vec1.vx;
            dest->ry = s->vec1.vy;
            dest->rz = s->vec1.vz;
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x80;
    }
}

void Gp_AnimBlendPacked(GpAnimBlendSrc* arg0, GpAnimMtxRec* arg1, GpAnimSlot* arg2)
{
    register void**           scratch asm("v1");
    register GpAnimScratch80* tmp asm("v0");
    register s32              blend asm("v1");
    register GpPackedSvec*    p asm("v1");
    GpPackedSvec*             dest;
    GpAnimScratch80*          s;
    s32                       inv;

    if (arg2->field_E != 0) {
        scratch  = (void**)G_SCRATCH_HEAD;
        tmp      = *scratch;
        tmp     -= 1;
        *scratch = tmp;
        if (arg0->field_0 != arg0->field_4) {
            s        = tmp;
            blend    = (s16)arg2->field_C << 12;
            s->blend = blend;
            blend    = blend / (s32)arg2->field_E;
            inv      = 0x1000 - blend;
            s->blend = blend;
        } else {
            s        = tmp;
            inv      = 0x1000;
            s->blend = 0;
        }
        s->invBlend = inv;
        p           = arg0->field_0;
        s->vec0.vx  = p->vx << 3;
        s->vec0.vy  = p->vy << 3;
        s->vec0.vz  = p->vz << 3;
        p           = arg0->field_4;
        s->vec1.vx  = p->vx << 3;
        s->vec1.vy  = p->vy << 3;
        s->vec1.vz  = p->vz << 3;
        Gp_BlendAnimRot(arg0, arg1, arg2, s);
        dest = arg0->field_8;
        if (dest != NULL) {
            dest->vx = s->vec1.vx >> 3;
            dest->vy = s->vec1.vy >> 3;
            dest->vz = s->vec1.vz >> 3;
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x80;
    }
}

void Gp_AnimAdvanceSlot(GpAnimCtx* arg0, s32 arg1)
{
    GpAnimSlot* slot;
    GpAnimSet** sets;
    GpAnimRec*  recs;
    GpAnimRec*  rec;
    u16         idx;
    s32         setIdx;
    u16         val;

    slot           = &arg0->field_C[arg1];
    slot->field_10 = 0;
    if (*(s32*)&slot->field_0 != *(s32*)&slot->field_4) {
        sets = slot->field_20;
        do {
            *(s32*)&slot->field_0 = *(s32*)&slot->field_4;
            idx                   = slot->field_6 + 1;
            setIdx                = slot->field_4;
            recs                  = sets[setIdx]->field_0;
            while ((s8)recs[idx].field_3 < 0) {
                rec = (GpAnimRec*)((idx << 2) + (s32)recs);
                if (rec->field_3 < 0xC0) {
                    idx = rec->field_0;
                    if (idx == slot->field_6) {
                        slot->field_10 |= 1;
                    }
                    slot->field_10 |= 2;
                } else {
                    idx             = slot->field_6;
                    slot->field_10 |= 1;
                    break;
                }
            }
            slot->field_6 = idx;
            slot->field_4 = setIdx;
            if (slot->field_10 & 3) {
                break;
            }
        } while (*(s32*)&slot->field_0 != *(s32*)&slot->field_4);
    }

    val           = slot->field_20[slot->field_4]->field_0[slot->field_6].field_2 << 4;
    slot->field_E = val;
    slot->field_C = val;
    func_800B3448(arg0, arg1, 0, 0);
}

void func_800B3448(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GpAnimScratch18*  s;
    GpAnimSlot*       slot;
    GpAnimMtxRec*     mtx;
    GpAnimSet*        set;
    GpAnimRec*        recs;
    GpAnimRec*        rec;
    GpPackedSvec*     poses;
    u16               idx;
    u16               idx2;
    s32               setIdx;
    s32               setIdx2;
    u16               lim;
    u16               val;
    s16               rem;
    s32               op;
    u16               base;
    GpAnimScratch18** head;

    head           = (GpAnimScratch18**)G_SCRATCH_HEAD;
    slot           = &arg0->field_C[arg1];
    mtx            = &((GpAnimMtxRec*)arg0->field_4)[slot->field_14];
    *head          = *head - 1;
    s              = *head;
    slot->field_10 = 0;
    if (slot->field_16 == 1) {
        if (*(s32*)&slot->field_4 == *(s32*)&slot->field_0) {
            slot->field_10 = 0x100;
        } else {
            slot->field_16 = 0;
        }
    } else {
        if (Game_Session->field_0 != 0) {
            base          = slot->field_C - 1;
            slot->field_C = base - (((s8)slot->field_9 - 1) >> 1);
        } else {
            slot->field_C -= (s8)slot->field_9;
        }
    }

    rem = slot->field_C;
    if (rem <= 0) {
        slot->pad_A = 0;
        while ((s16)slot->field_C <= 0) {
            *(s32*)&slot->field_0 = *(s32*)&slot->field_4;
            idx                   = slot->field_6 + 1;
            setIdx                = slot->field_4;
            recs                  = slot->field_20[setIdx]->field_0;
            while ((s8)recs[idx].field_3 < 0) {
                rec = (GpAnimRec*)((idx << 2) + (s32)recs);
                if (rec->field_3 < 0xC0) {
                    idx = rec->field_0;
                    if (idx == slot->field_6) {
                        slot->field_10 |= 1;
                    }
                    slot->field_10 |= 2;
                } else {
                    idx             = slot->field_6;
                    slot->field_10 |= 1;
                    break;
                }
            }
            slot->field_4  = setIdx;
            slot->field_6  = idx;
            recs           = slot->field_20[slot->field_4]->field_0;
            val            = recs[slot->field_6].field_2 << 4;
            slot->field_E  = val;
            slot->field_C += val;
        }
        if (slot->field_10 & 1) {
            slot->field_16  = 1;
            slot->field_10 |= 0x100;
        } else {
            slot->field_16 = 0;
        }
    } else if (slot->field_E < rem) {
        slot->pad_A = 0;
        while ((s16)slot->field_C > slot->field_E) {
            slot->field_C        -= slot->field_E;
            *(s32*)&slot->field_4 = *(s32*)&slot->field_0;
            setIdx2               = slot->field_0;
            idx2                  = slot->field_2 - 1;
            lim                   = slot->field_20[setIdx2]->field_4[slot->field_15];
            if (idx2 < lim) {
                idx2            = lim;
                slot->field_10 |= 1;
            }
            slot->field_2 = idx2;
            slot->field_0 = setIdx2;
            recs          = slot->field_20[slot->field_4]->field_0;
            val           = recs[slot->field_6].field_2 << 4;
            slot->field_E = val;
        }
        if (slot->field_10 & 1) {
            slot->field_16  = 1;
            slot->field_10 |= 0x100;
        } else {
            slot->field_16 = 0;
        }
    }

    op              = slot->field_B;
    s->src.field_10 = slot->field_17;
    slot->field_17  = 0;
    if (slot->field_0 == 0x7FFF) {
        s->src.field_0 = (GpPackedSvec*)((s32)arg0->field_8 + (arg1 << 4));
        slot->field_17 = 1;
    } else {
        recs           = slot->field_20[slot->field_0]->field_0;
        poses          = slot->field_20[slot->field_0]->field_8[op];
        s->src.field_0 = &poses[recs[slot->field_2].field_0];
    }
    if (slot->field_4 == 0x7FFF) {
        s->src.field_4 = (GpPackedSvec*)((s32)arg0->field_8 + (arg1 << 4));
        slot->field_17 = 1;
    } else {
        set            = slot->field_20[slot->field_4];
        recs           = set->field_0;
        poses          = set->field_8[op];
        s->src.field_4 = &poses[recs[slot->field_6].field_0];
    }
    if ((s->src.field_10 == 0) && (slot->field_17 == 1)) {
        s->src.field_10 = slot->field_17;
    } else {
        s->src.field_10 = 0;
    }
    s->src.field_8 = (GpPackedSvec*)arg3;
    s->src.field_C = (GpAnimPose*)arg2;
    switch (op) {
        case 1:
            Gp_AnimBlendPose(&s->src, mtx, slot);
            break;
        case 2:
            printf(D_80093A44);
            break;
        case 4:
            Gp_AnimBlendPacked(&s->src, mtx, slot);
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

void Gp_AnimSeekSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    register GpAnimSlot* p asm("s0");
    register s32         one asm("v0");
    register s32         raw asm("a2");
    register s32         scaled asm("v1");
    register s32         off asm("t0");
    register s32         f8 asm("a3");
    GpAnimSlot*          slot;
    GpAnimSet*           set;
    GpAnimRec*           recs;
    GpAnimRec*           recs2;
    GpAnimRec*           rec;
    u16                  idx;
    u16                  val;
    s32                  extra;
    s32                  saved2;

    extra  = arg3;
    slot   = &arg0->field_C[arg1];
    raw    = arg2;
    saved2 = raw;
    off    = arg1 << 4;
    f8     = (s32)arg0->field_8;
    TOUCH_REG(raw);
    scaled = raw << 2;
    recs2  = (*(GpAnimSet**)(scaled + (s32)slot->field_20))->field_0;
    func_800B3448(arg0, arg1, 0, f8 + off);
    slot->field_0 = 0x7FFF;
    set           = slot->field_20[(u16)saved2];
    recs          = set->field_0;
    idx           = set->field_4[slot->field_15] + extra;
    p             = slot;
    while ((s8)recs[idx].field_3 < 0) {
        rec = (GpAnimRec*)((idx << 2) + (s32)recs);
        if (rec->field_3 < 0xC0) {
            idx = rec->field_0;
            if (idx == p->field_6) {
                p->field_10 |= 1;
            }
            p->field_10 |= 2;
        } else {
            idx          = p->field_6;
            p->field_10 |= 1;
            break;
        }
    }
    one = 1;
    TOUCH_REG(one);
    val           = one << 4;
    p->field_6    = idx;
    p->field_4    = saved2;
    p->field_E    = val;
    p->field_C    = val;
    p->field_17   = 0;
    val           = recs2[slot->field_6].field_2 << 4;
    slot->field_E = val;
    slot->field_C = val;
}

void func_800B3AA4(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    GpAnimSlot* slot;
    GpAnimSet** sets;
    GpAnimSet*  set;
    GpAnimRec*  recs;
    GpAnimRec*  rec;
    u16         recIdx;
    u16         val;
    u8          op;
    s32         setIdx;

    if (Mc_SaveData.field_23 == 1) {
        u8  idx;
        s32 off;

        idx            = arg1->field_15;
        setIdx         = arg3;
        arg0->field_C  = arg1 - idx;
        arg1->field_14 = arg2;
        idx            = arg1->field_15;
        off            = idx << 4;
        slot           = &arg0->field_C[idx];
        func_800B3448(arg0, idx, 0, (s32)arg0->field_8 + off);
        slot->field_0 = 0x7FFF;
        set           = slot->field_20[(u16)setIdx];
        recs          = set->field_0;
        recIdx        = set->field_4[slot->field_15] + arg4;
        while ((s8)recs[recIdx].field_3 < 0) {
            rec = (GpAnimRec*)((recIdx << 2) + (s32)recs);
            if (rec->field_3 < 0xC0) {
                recIdx = rec->field_0;
                if (recIdx == slot->field_6) {
                    slot->field_10 |= 1;
                }
                slot->field_10 |= 2;
            } else {
                recIdx          = slot->field_6;
                slot->field_10 |= 1;
                break;
            }
        }
        slot->field_6  = recIdx;
        slot->field_4  = setIdx;
        val            = arg5 << 4;
        slot->field_E  = val;
        slot->field_C  = val;
        slot->field_17 = 0;
    } else {
        if (arg3 == 0) {
            arg3 = 1;
        } else if (arg3 < 0) {
            arg3 = -arg3;
        }

        arg1->field_9  = 0x10;
        arg1->field_C  = 0;
        arg1->field_0  = arg3;
        arg1->field_2  = 0;
        arg1->field_14 = arg2;
        arg1->field_15 = arg2;
        arg1->field_4  = arg3;
        sets           = arg0->field_0;
        arg1->field_20 = sets;
        arg1->field_6  = sets[arg3]->field_4[arg1->field_15];
        arg1->field_2  = arg1->field_20[arg3]->field_4[arg1->field_15];
        op             = arg1->field_20[arg1->field_4]->field_0[arg1->field_6].field_3;
        arg1->field_12 = 0;
        arg1->field_10 = 0;
        arg1->field_16 = 0;
        arg1->field_B  = op & 0xF;
    }
}

void Gp_AnimInitCtx(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3)
{
    arg0->field_0  = arg1;
    arg0->field_4  = &arg2->field_34;
    arg0->field_8  = arg3;
    arg0->field_10 = arg2->field_30;
}

void Gp_AnimInitSlot(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3)
{
    GpAnimSet** sets;
    u8          op;

    if (arg3 == 0) {
        arg3 = 1;
    } else if (arg3 < 0) {
        arg3 = -arg3;
    }

    arg1->field_9  = 0x10;
    arg1->field_C  = 0;
    arg1->field_0  = arg3;
    arg1->field_2  = 0;
    arg1->field_14 = arg2;
    arg1->field_15 = arg2;
    arg1->field_4  = arg3;
    sets           = arg0->field_0;
    arg1->field_20 = sets;
    arg1->field_6  = sets[arg3]->field_4[arg1->field_15];
    arg1->field_2  = arg1->field_20[arg3]->field_4[arg1->field_15];
    op             = arg1->field_20[arg1->field_4]->field_0[arg1->field_6].field_3;
    arg1->field_12 = 0;
    arg1->field_10 = 0;
    arg1->field_16 = 0;
    arg1->field_B  = op & 0xF;
}

void Gp_AnimTickSlot(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx           = arg1->field_15;
    arg0->field_C = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

void Gp_AnimTickSlot2(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx           = arg1->field_15;
    arg0->field_C = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

void Gp_AnimTickSlot3(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx           = arg1->field_15;
    arg0->field_C = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

void func_800B3E74(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3)
{
    GpAnimRec* recs;
    u16        val;

    recs = arg1->field_20[arg3]->field_0;
    func_800B3AA4(arg0, arg1, arg2, arg3, 0, 8);
    val           = recs[arg1->field_6].field_2 << 4;
    arg1->field_E = val;
    arg1->field_C = val;
}

void func_800B3EE8(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4)
{
    GpAnimRec* recs;
    u16        val;

    recs = arg1->field_20[arg3]->field_0;
    func_800B3AA4(arg0, arg1, arg2, arg3, arg4, 8);
    val           = recs[arg1->field_6].field_2 << 4;
    arg1->field_E = val;
    arg1->field_C = val;
}

void Gp_AnimInitCtxSlots(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4)
{
    arg0->field_0  = arg1;
    arg0->field_4  = &arg2->field_34;
    arg0->field_8  = arg3;
    arg0->field_10 = arg2->field_30;
    arg0->field_C  = arg4;
}

void func_800B3F84(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4)
{
    Gp_AnimInitCtxSlots(arg0, arg1, arg2, arg3, arg4);
}

void Gp_AnimResetSlot(GpAnimCtx* arg0, s32 arg1, s32 arg2)
{
    GpAnimSlot* slot;
    GpAnimSet** sets;
    u8          op;

    slot           = &arg0->field_C[arg1];
    slot->field_9  = 0x10;
    slot->field_C  = 0;
    slot->field_0  = arg2;
    slot->field_2  = 0;
    slot->field_14 = arg1;
    slot->field_15 = arg1;
    slot->field_4  = arg2;
    sets           = arg0->field_0;
    slot->field_20 = sets;
    slot->field_6  = sets[arg2]->field_4[slot->field_15];
    op             = slot->field_20[slot->field_4]->field_0[slot->field_6].field_3;
    slot->field_10 = 0;
    slot->field_16 = 0;
    slot->field_12 = 0;
    slot->field_B  = op & 0xF;
}

void Gp_AnimResetSlotEx(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    GpAnimSlot* slot;
    GpAnimSet** sets;
    u8          op;

    slot           = &arg0->field_C[arg1];
    slot->field_9  = 0x10;
    slot->field_C  = 0;
    slot->field_0  = arg2;
    slot->field_2  = 0;
    slot->field_14 = arg4;
    slot->field_15 = arg3;
    slot->field_4  = arg2;
    sets           = arg0->field_0;
    slot->field_20 = sets;
    slot->field_6  = sets[arg2]->field_4[slot->field_15];
    op             = slot->field_20[slot->field_4]->field_0[slot->field_6].field_3;
    slot->field_10 = 0;
    slot->field_16 = 0;
    slot->field_12 = 0;
    slot->field_B  = op & 0xF;
}

void Gp_AnimSeekSlot(GpAnimCtx* arg0, s32 arg1, s32 arg2)
{
    Gp_AnimSeekSlotEx(arg0, arg1, arg2, 0);
}

void func_800B4114(GpAnimCtx* arg0, s32 arg1, u16 arg2, s32 arg3, s32 arg4)
{
    GpAnimSlot* slot;
    GpAnimSet*  set;
    GpAnimRec*  recs;
    GpAnimRec*  rec;
    u16         idx;
    u16         val;
    s32         off;

    off  = arg1 << 4;
    slot = &arg0->field_C[arg1];
    func_800B3448(arg0, arg1, 0, (s32)arg0->field_8 + off);
    slot->field_0 = 0x7FFF;
    set           = slot->field_20[arg2];
    recs          = set->field_0;
    idx           = set->field_4[slot->field_15] + arg3;
    while ((s8)recs[idx].field_3 < 0) {
        rec = (GpAnimRec*)((idx << 2) + (s32)recs);
        if (rec->field_3 < 0xC0) {
            idx = rec->field_0;
            if (idx == slot->field_6) {
                slot->field_10 |= 1;
            }
            slot->field_10 |= 2;
        } else {
            idx             = slot->field_6;
            slot->field_10 |= 1;
            break;
        }
    }
    slot->field_6  = idx;
    slot->field_4  = arg2;
    val            = arg4 << 4;
    slot->field_E  = val;
    slot->field_C  = val;
    slot->field_17 = 0;
}

void Gp_AnimWritePoseBlend(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
                           s32 arg5)
{
    void**            scratch;
    register void*    head asm("t1");
    GpAnimSlot*       slot;
    GpAnimMtxRec*     dest;
    register SVECTOR* st asm("a0");
    register SVECTOR* trans asm("t0");
    SVECTOR*          rot;
    s32               idx;
    s32               off;

    scratch  = (void**)G_SCRATCH_HEAD;
    slot     = &arg0->field_C[arg1];
    head     = *scratch;
    idx      = slot->field_14;
    trans    = (SVECTOR*)((u8*)head - 0x10);
    *scratch = trans;
    off      = idx * 0x50;
    USE_REG(off);
    dest = &((GpAnimMtxRec*)arg0->field_4)[idx];
    if (slot->field_B == 1) {
        st = trans;
        gte_lddp(arg4);
        gte_ldsv(&arg2->trans);
        gte_gpf12_real();
        gte_lddp(arg5);
        gte_ldsv(&arg3->trans);
        gte_gpl12_real();
        gte_stsv(st);
        dest->mtx.t[0] = trans->vx;
        COPY_REG(trans, trans);
        dest->mtx.t[1] = trans->vy;
        dest->mtx.t[2] = trans->vz;
    }
    gte_lddp(arg4);
    gte_ldsv(&arg2->rot);
    gte_gpf12_real();
    gte_lddp(arg5);
    gte_ldsv(&arg3->rot);
    gte_gpl12_real();
    rot = (SVECTOR*)((u8*)head - 8);
    gte_stsv(rot);
    RotMatrix_gte(rot, &dest->mtx);
    dest->field_0           = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void Gp_AnimWritePoseCopy(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
                          s32 arg5)
{
    void**        scratch;
    void*         head;
    GpAnimSlot*   slot;
    GpAnimMtxRec* dest;
    SVECTOR*      rot;
    s32           idx;

    scratch  = (void**)G_SCRATCH_HEAD;
    slot     = &arg0->field_C[arg1];
    head     = *scratch;
    idx      = slot->field_14;
    *scratch = (u8*)head - 0x10;
    dest     = &((GpAnimMtxRec*)arg0->field_4)[idx];
    if (slot->field_B == 1) {
        dest->mtx.t[0] = arg2->trans.vx;
        dest->mtx.t[1] = arg2->trans.vy;
        dest->mtx.t[2] = arg2->trans.vz;
    }
    gte_lddp(arg4);
    gte_ldsv(&arg2->rot);
    gte_gpf12_real();
    gte_lddp(arg5);
    gte_ldsv(&arg3->rot);
    gte_gpl12_real();
    rot = (SVECTOR*)((u8*)head - 8);
    gte_stsv(rot);
    RotMatrix_gte(rot, &dest->mtx);
    dest->field_0           = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void Gp_AnimTickIndex(GpAnimCtx* arg0, s32 arg1)
{
    func_800B3448(arg0, arg1, 0, 0);
}

void func_800B4538(GpAnimCtx* arg0, s32 arg1, s32 arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6)
{
    GpAnimSlot* slot;
    GpAnimSet*  set;
    GpAnimRec*  recs;
    GpAnimRec*  rec;
    u16         idx;
    u16         val;
    s32         off;

    off  = arg1 << 4;
    slot = &arg0->field_C[arg1];
    func_800B3448(arg0, arg1, arg2, (s32)arg0->field_8 + off);
    slot->field_0 = 0x7FFF;
    set           = slot->field_20[arg3];
    recs          = set->field_0;
    idx           = set->field_4[slot->field_15] + arg4;
    while ((s8)recs[idx].field_3 < 0) {
        rec = (GpAnimRec*)((idx << 2) + (s32)recs);
        if (rec->field_3 < 0xC0) {
            idx = rec->field_0;
            if (idx == slot->field_6) {
                slot->field_10 |= 1;
            }
            slot->field_10 |= 2;
        } else {
            idx             = slot->field_6;
            slot->field_10 |= 1;
            break;
        }
    }
    slot->field_6  = idx;
    slot->field_4  = arg3;
    val            = arg6 << 4;
    slot->field_E  = val;
    slot->field_C  = val;
    slot->field_17 = 0;
}

GpAnimRec* Gp_AnimGetRec(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u16        idx;
    GpAnimRec* ret;

    idx = arg1->field_0;
    switch (idx) {
        case 0x7FFF:
            return NULL;
        default:
            ret  = arg1->field_20[idx]->field_0;
            ret += arg1->field_2;
            return ret;
    }
}

void func_800B46A4(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3)
{
    GpAnimRec* recs;
    GpAnimRec* rec;

    recs = arg1->field_20[arg2]->field_0;
    while ((s8)recs[arg3].field_3 < 0) {
        rec = (GpAnimRec*)((arg3 << 2) + (s32)recs);
        if (rec->field_3 < 0xC0) {
            arg3 = rec->field_0;
            if (arg3 == arg1->field_6) {
                arg1->field_10 |= 1;
            }
            arg1->field_10 |= 2;
        } else {
            arg3            = arg1->field_6;
            arg1->field_10 |= 1;
            break;
        }
    }
    arg1->field_6 = arg3;
    arg1->field_4 = arg2;
}

void func_800B4754(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3)
{
    u16 limit;

    limit = arg1->field_20[arg2]->field_4[arg1->field_15];
    if (arg3 < limit) {
        arg3            = limit;
        arg1->field_10 |= 1;
    }
    arg1->field_2 = arg3;
    arg1->field_0 = arg2;
}

void Gp_AnimPlaySlot(GpAnimCtx* arg0, s32 arg1, s32 arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6,
                     void* arg7)
{
    GpAnimSlot*    slot;
    GpAnimSet*     set;
    GpAnimRec*     recs;
    GpAnimRec*     rec;
    u16            idx;
    u16            val;
    s32            off;
    register s32   extra asm("s3");
    register void* sets asm("s1");

    extra = arg4;
    sets  = arg7;
    off   = arg1 << 4;
    slot  = &arg0->field_C[arg1];
    func_800B3448(arg0, arg1, arg2, (s32)arg0->field_8 + off);
    slot->field_0 = 0x7FFF;
    if (sets != NULL) {
        arg0->field_0  = sets;
        slot->field_20 = sets;
    }
    set  = slot->field_20[arg3];
    recs = set->field_0;
    idx  = set->field_4[slot->field_15] + extra;
    while ((s8)recs[idx].field_3 < 0) {
        rec = (GpAnimRec*)((idx << 2) + (s32)recs);
        if (rec->field_3 < 0xC0) {
            idx = rec->field_0;
            if (idx == slot->field_6) {
                slot->field_10 |= 1;
            }
            slot->field_10 |= 2;
        } else {
            idx             = slot->field_6;
            slot->field_10 |= 1;
            break;
        }
    }
    slot->field_6  = idx;
    slot->field_4  = arg3;
    val            = arg6 << 4;
    slot->field_E  = val;
    slot->field_C  = val;
    slot->field_17 = 0;
}

void Gp_SaveEnemyPose(GpEnemy* arg0)
{
    McPosRec*         rec;
    GameSessionFrom4* loc;
    GameActorExt*     extra;
    GsCOORDINATE2*    coord;
    register SVECTOR* euler asm("s1");
    u16               id;
    s32               i;

    rec   = Mc_SaveData.field_28;
    loc   = (GameSessionFrom4*)&Mc_SaveData.field_4;
    extra = (GameActorExt*)arg0->task->extra;
    coord = (GsCOORDINATE2*)extra->field_8;
    if (arg0->field_4B == 0) {
        arg0->field_4B = 1;
    }
    id = arg0->field_8;
    for (i = 0; i < 0x20; i++, rec++) {
        if (rec->field_A == id) {
            return;
        }
    }

    {
        register void** scratch asm("v1");
        register void*  tmp asm("v0");

        scratch  = (void**)G_SCRATCH_HEAD;
        rec      = Mc_SaveData.field_28;
        tmp      = *scratch;
        i        = 0;
        tmp      = (u8*)tmp - 8;
        euler    = tmp;
        *scratch = euler;
    }
    for (; i < 0x20; i++, rec++) {
        if (rec->field_3 == 0) {
            break;
        }
    }
    if (i == 0x20) {
        register u32 hi asm("v0");
        register u32 key asm("v1");

        rec  = Mc_SaveData.field_28;
        i    = 0;
        hi   = loc->field_3;
        key  = loc->field_2;
        hi <<= 8;
        key  = hi | key;
        for (; i < 0x1F; i++, rec++) {
            if ((rec->field_A & 0xFFF) != key) {
                break;
            }
        }
        for (; i < 0x1F; i++, rec++) {
            rec[0] = rec[1];
        }
    }
    rec->field_3 = arg0->field_4B;
    rec->field_A = arg0->field_8;
    rec->field_4 = coord->coord.t[0];
    rec->field_6 = coord->coord.t[1];
    rec->field_8 = coord->coord.t[2];
    Gfx_MatrixToEuler(&coord->coord, euler);
    euler->vx               = (s16)euler->vx >> 8;
    rec->field_0            = euler->vx;
    euler->vy               = (s16)euler->vy >> 8;
    rec->field_1            = euler->vy;
    euler->vz               = (s16)euler->vz >> 8;
    rec->field_2            = euler->vz;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void Gp_SpawnArea(GpAreaKey* arg0)
{
    GpAreaRec*    recs;
    GpAreaRec*    nested;
    GpAreaObj*    obj;
    GpAreaPlace*  place;
    GpAreaTmdRec* entry;
    GpEnemy*      enemy;
    Task*         task;
    TmdObject*    extra;
    GpCoordPose*  coord;
    u16           id;
    s32           packed;
    s32           fp;
    s32           i;

    recs = Gp_AreaTables[arg0->field_3];
    Gp_ResetLinkState();
    if (recs == NULL) {
        return;
    }
    nested = recs[arg0->field_2].field_0;
    obj    = recs[arg0->field_2].field_4;
    if (nested == NULL) {
        return;
    }
    func_800B5A48(arg0, obj);
    place = (GpAreaPlace*)nested[arg0->field_5].field_0;
    fp    = 0;
    if (place == NULL) {
        return;
    }
    if (place->field_0 == 0xFF) {
        return;
    }
    do {
        entry = (GpAreaTmdRec*)nested[arg0->field_5].field_4;
        id    = entry->field_0;
        if (id != 0xFF) {
            packed = fp << 24;
            do {
                if (id == place->field_0) {
                    if (obj->field_1 & 2) {
                        McPosRec*    rec;
                        s32          found;
                        s32          j;
                        register s32 key asm("v1");
                        s32          t;
                        register s32 lo asm("a0");

                        rec   = Mc_SaveData.field_28;
                        found = 0;
                        j     = found;
                        key   = packed >> 12;
                        t     = arg0->field_3;
                        lo    = arg0->field_2;
                        key   = key | (t << 8);
                        key   = key | lo;
                        for (; j < 0x20; j++, rec++) {
                            if (rec->field_A == key) {
                                found = 1;
                                break;
                            }
                        }
                        if (found == 0) {
                            break;
                        }
                    }
                    enemy = Gp_SpawnEnemyFromTable((TaskDesc*)entry->field_8, entry->field_5,
                                                   (place->field_1 << 16) | place->field_2, NULL);
                    if (enemy != NULL) {
                        register s32 f3 asm("v1");
                        register s32 f2 asm("a0");
                        register s32 v asm("v0");

                        v               = 0x900;
                        f3              = arg0->field_3;
                        f2              = arg0->field_2;
                        enemy->field_A  = v;
                        v               = packed >> 12;
                        enemy->field_3C = place;
                        v               = v | (f3 << 8);
                        task            = enemy->task;
                        f2              = f2 | v;
                        enemy->field_8  = f2;
                        if (task->spawnType != 0) {
                            extra = (TmdObject*)task->extra;
                            coord = (GpCoordPose*)extra->field_8;
                            if (task->spawnType == 1) {
                                extra->field_24 = place->field_D;
                                extra->field_25 = place->field_E;
                                if (extra->field_18 != NULL) {
                                    Tmd_ProcessStream(extra);
                                    Tmd_ProcessStream(extra);
                                }
                            }
                            if (!(obj->field_1 & 2)) {
                                coord->coord.t[0] = place->field_4;
                                coord->coord.t[1] = place->field_6;
                                coord->coord.t[2] = place->field_8;
                                coord->field_46   = place->field_A;
                                Gfx_RotMatrixY(&coord->coord, place->field_A, 1);
                            } else {
                                McPosRec* rec;

                                rec = Mc_SaveData.field_28;
                                i   = 0;
                                do {
                                    if (rec->field_A == enemy->field_8) {
                                        coord->coord.t[0] = rec->field_4;
                                        coord->coord.t[1] = rec->field_6;
                                        coord->coord.t[2] = rec->field_8;
                                        coord->field_44   = rec->field_0 << 8;
                                        coord->field_46   = rec->field_1 << 8;
                                        coord->field_48   = rec->field_2 << 8;
                                        RotMatrix_gte((SVECTOR*)&coord->field_44,
                                                      &coord->coord);
                                        enemy->field_4B = rec->field_3;
                                        break;
                                    }
                                    i++;
                                    rec++;
                                } while (i < 0x20);
                                if (i == 0x20) {
                                    Gp_DestroyEnemy(enemy, enemy->task);
                                }
                            }
                        }
                    }
                    break;
                }
                entry++;
                id = entry->field_0;
            } while (id != 0xFF);
        }
        fp++;
        place++;
    } while (place->field_0 != 0xFF);
}

void func_800B4E54(GsCOORDINATE2* arg0, u32 arg1, SVECTOR* arg2)
{
    void**              scratch;
    u8*                 head;
    GpFloorQuadScratch* block;
    POLY_FT4*           prim;
    register s32        c0 asm("v1");
    register s32        f7 asm("a1");

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x40;
        block    = (GpFloorQuadScratch*)tmp;
        *scratch = tmp;
    }
    if (arg2 == NULL) {
        ((GpFloorQuadScratch*)(head - 0x40))->vec[0].vx = -(arg1 >> 1);
        block->vec[0].vy                                = 0;
        block->vec[0].vz                                = -(arg1 >> 1);
    } else {
        ((GpFloorQuadScratch*)(head - 0x40))->vec[0].vx = *(u16*)&arg2->vx - (arg1 >> 1);
        block->vec[0].vy                                = *(u16*)&arg2->vy;
        block->vec[0].vz                                = *(u16*)&arg2->vz - (arg1 >> 1);
    }
    block->vec[3].vy = block->vec[2].vy = block->vec[1].vy = *(u16*)&block->vec[0].vy;
    block->vec[1].vx = block->vec[3].vx = *(u16*)&block->vec[0].vx + arg1;
    block->vec[2].vx                    = block->vec[0].vx;
    block->vec[2].vz = block->vec[3].vz = *(u16*)&block->vec[0].vz + arg1;
    block->vec[1].vz                    = block->vec[0].vz;
    Gp_UpdateCoord(arg0);
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    block->maxotz = 0;

    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    gte_ldv0(&block->vec[1]);
    gte_rtps_real();
    gte_stsxy(&block->sxy1);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    gte_ldv0(&block->vec[2]);
    gte_rtps_real();
    gte_stsxy(&block->sxy2);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    gte_ldv0(&block->vec[3]);
    gte_rtps_real();
    gte_stsxy(&block->sxy3);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->otz > block->maxotz) {
        block->maxotz = block->otz;
    }

    if (block->flag >= 0) {
        c0         = 0xC0;
        f7         = 0xF7;
        prim       = (POLY_FT4*)D_80071190;
        D_80071190 = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        *(u32*)&prim->x0 = *(u32*)&block->sxy0;
        *(u32*)&prim->x1 = *(u32*)&block->sxy1;
        *(u32*)&prim->x2 = *(u32*)&block->sxy2;
        *(u32*)&prim->x3 = *(u32*)&block->sxy3;
        prim->v0         = 0x98;
        prim->v1         = 0x98;
        prim->v2         = 0xCF;
        prim->v3         = 0xCF;
        prim->tpage      = 0x48;
        prim->u0         = c0;
        prim->u2         = c0;
        prim->g0         = c0;
        prim->b0         = c0;
        prim->r0         = c0;
        prim->u1         = f7;
        prim->u3         = f7;
        prim->clut       = 0x4283;
        addPrim(&Gpu_CurrentOt[block->maxotz >> 4], prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x40;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B51F4);

void Gp_ApplyAreaTmdFlags(void)
{
    Task*         head;
    Task*         iter;
    GpAreaKey*    key;
    GpAreaRec*    rec;
    GpAreaTmdRec* table;
    GpAreaTmdRec* entry;
    GpWorkObj*    work;
    TmdObject*    extra;
    u8*           bytes;
    u16           id;
    u16           flags;
    u16           limit;
    u8            idx;

    head = ((Task*)Game_GetPtrSlot(4))->firstChild;
    if (head != NULL) {
        iter = head;
        do {
            work = iter->spawnArg2;
            if (iter->spawnType == 1) {
                key   = (GpAreaKey*)&Mc_SaveData.field_4;
                idx   = key->field_3;
                extra = iter->extra;
                rec   = Gp_AreaTables[idx];
                bytes = work->field_3C;
                table = NULL;
                if (rec != NULL) {
                    rec = rec[key->field_2].field_0;
                    if (rec != NULL) {
                        table = (GpAreaTmdRec*)rec[key->field_5].field_4;
                    }
                }
                entry = table;
                id    = entry->field_0;
                if (id != 0xFF) {
                    limit = 0xFF;
                    do {
                        if (id == *bytes) {
                            flags = *entry->field_8;
                            if (flags == 1) {
                                extra->field_C &= 0xFFFB;
                            } else if (flags == 0x101) {
                                extra->field_C |= 4;
                            }
                            break;
                        }
                        entry++;
                        id = entry->field_0;
                    } while (id != limit);
                }
            }
            iter = iter->nextSibling;
        } while (iter != head);
    }
}

void Gp_ReparentCoord(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1)
{
    GsCOORDINATE2* dest;

    dest = arg1;
    if (dest->sub != arg0) {
        Gp_UpdateCoord(arg0);
        Gp_UpdateCoord(dest);
        dest->sub = arg0;
        Gp_WorldToLocal(&arg0->workm, &dest->workm, &dest->coord);
        dest->flg = 0;
    }
}

GpWorkObj* Gp_FindWorkById(u16 arg0)
{
    Task*      head;
    Task*      iter;
    GpWorkObj* work;
    s32        key;

    work = NULL;
    head = ((Task*)Game_GetPtrSlot(4))->firstChild;
    if (head != NULL) {
        iter = head;
        work = iter->spawnArg2;
        key  = arg0;
        if (work->field_8.as_u16 != key) {
        loop:
            iter = iter->nextSibling;
            work = NULL;
            if (iter != head) {
                work = iter->spawnArg2;
                if (work->field_8.as_u16 != key) {
                    goto loop;
                }
            }
        }
    }
    return work;
}

void Gp_SetTmdBytes(TmdObject* arg0, s32 arg1, s32 arg2)
{
    arg0->field_24 = arg1;
    arg0->field_25 = arg2;
    if (arg0->field_18 != NULL) {
        Tmd_ProcessStream(arg0);
        Tmd_ProcessStream(arg0);
    }
}

void Gp_SetCurAreaFlag2(s32 arg0)
{
    GpAreaRec* rec;
    GpAreaObj* obj;
    GpAreaKey* key;

    key = (GpAreaKey*)&Mc_SaveData.field_4;
    rec = Gp_AreaTables[key->field_3];
    if (rec != NULL) {
        obj = rec[key->field_2].field_4;
        if (obj != NULL) {
            if (obj->field_0 == key->field_5) {
                if (arg0 == 0) {
                    obj->field_1 &= 0xFD;
                    return;
                }
                obj->field_1 |= 2;
            }
        }
    }
}

s32 Gp_GetAreaFlag2(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* obj;
    s32        val;

    rec = Gp_AreaTables[arg0->field_3];
    if (rec != NULL) {
        obj = rec[arg0->field_2].field_4;
        if (obj != NULL) {
            val = obj->field_1 & 2;
            return val != 0;
        }
    }
    return 0;
}

GpAreaObj* Gp_GetAreaObj(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* ret;

    rec = Gp_AreaTables[arg0->field_3];
    if (rec == NULL) {
        ret = NULL;
    } else {
        ret = rec[arg0->field_2].field_4;
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5A48);

void Gp_SetAreaObjId(GpAreaKey* arg0, s32 arg1, s32 arg2)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = Gp_AreaTables[arg0->field_3];
    if (rec != NULL) {
        obj = rec[arg0->field_2].field_4;
        if (obj != NULL) {
            if (arg2 != -1) {
                obj->field_0 = arg1;
                if (arg2 == 0) {
                    obj->field_1 &= 0xFE;
                } else {
                    obj->field_1 |= 1;
                }
                obj->field_1 &= 0xFD;
            } else if (obj->field_0 != arg1) {
                obj->field_0 = arg1;
                obj->field_1 = (obj->field_1 | 1) & 0xFD;
            } else {
                obj->field_1 &= 0xFE;
            }
            func_800B5A48(arg0, obj);
        }
    }
}

void Gp_SetAreaFlag2(s32 arg0, GpAreaKey* arg1)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = Gp_AreaTables[arg1->field_3];
    if (rec != NULL) {
        obj = rec[arg1->field_2].field_4;
        if (obj != NULL) {
            if (obj->field_0 == arg1->field_5) {
                if (arg0 == 0) {
                    obj->field_1 &= 0xFD;
                    return;
                }
                obj->field_1 |= 2;
            }
        }
    }
}

GpAreaObj* Gp_GetNestedAreaObj(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* ret;

    rec = Gp_AreaTables[arg0->field_3];
    ret = NULL;
    if (rec != NULL) {
        rec = rec[arg0->field_2].field_0;
        if (rec != NULL) {
            ret = rec[arg0->field_5].field_4;
        }
    }
    return ret;
}

GpAreaRec* Gp_GetNestedAreaRec(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaRec* ret;

    rec = Gp_AreaTables[arg0->field_3];
    ret = NULL;
    if (rec != NULL) {
        ret = rec[arg0->field_2].field_0;
        if (ret != NULL) {
            ret = &ret[arg0->field_5];
        }
    }
    return ret;
}

void Gp_SetAreaFlag0(GpAreaKey* arg0)
{
    u32        key;
    GpAreaRec* rec;
    GpAreaObj* obj;

    key = *(u32*)&arg0->field_0 & 0xFFFF0000;
    rec = Gp_AreaTables[arg0->field_3];
    if (key != 0x3260000) {
        if (rec != NULL) {
            obj = rec[arg0->field_2].field_4;
            if (obj != NULL) {
                obj->field_1 |= 1;
            }
        }
    }
}

void func_800B5DB8(Task* arg0)
{
    TaskFunc funcs[2] = { Gp_BindSlot4, func_800B6398 };

    funcs[arg0->state](arg0);
}

s32 Gp_FindChildType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3)
{
    Task* child;
    s32   ret;

    *arg3 = NULL;
    child = arg0->firstChild;
    ret   = -1;
    if (child == NULL) {
        return ret;
    }
    arg1 = child;
    do {
        arg0 = arg1->spawnArg2;
        if (((((GpWorkObj*)arg0)->field_A >> 8) == 9) && (((GpWorkObj*)arg0)->field_8.as_u16 == arg2)) {
            *arg3 = arg1;
            ret   = 0;
            break;
        }
        arg1 = arg1->nextSibling;
    } while (arg1 != child);
    return ret;
}

s32 Gp_FindChildExceptType9(Task* arg0, Task* arg1, s32 arg2, Task** arg3)
{
    Task* child;
    s32   ret;

    *arg3 = NULL;
    child = arg0->firstChild;
    ret   = -1;
    if (child == NULL) {
        return ret;
    }
    arg1 = child;
    do {
        arg0 = arg1->spawnArg2;
        if (((((GpWorkObj*)arg0)->field_A >> 8) != 9) && (((GpWorkObj*)arg0)->field_8.as_u8 == arg2)) {
            *arg3 = arg1;
            ret   = 0;
            break;
        }
        arg1 = arg1->nextSibling;
    } while (arg1 != child);
    return ret;
}

s32 Gp_ExitChildrenType9(Task* arg0)
{
    Task*      child;
    Task*      next;
    GpWorkObj* work;
    u32        type;

    child = arg0->firstChild;
    if (child == NULL) {
        return 0;
    }
    arg0 = child;
    do {
        work = (GpWorkObj*)arg0->spawnArg2;
        type = work->field_A >> 8;
        next = arg0->nextSibling;
        if (type == 9) {
            Task_CallExit(arg0);
        }
        arg0 = next;
    } while (arg0 != child);
    return 0;
}

s32 Gp_SendMsgType9(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*      child;
    Task*      next;
    GpWorkObj* work;
    u32        type;

    child = arg0->firstChild;
    if (child == NULL) {
        return 0;
    }
    arg0 = child;
    do {
        work = (GpWorkObj*)arg0->spawnArg2;
        type = work->field_A >> 8;
        next = arg0->nextSibling;
        if (type == 9) {
            Gp_DispatchMsg(arg0, arg3, arg2, 0);
        }
        arg0 = next;
    } while (arg0 != child);
    return 0;
}

void Gp_KillSlot4Children(void)
{
    Task_KillChildren(Game_GetPtrSlot(4));
}

void func_800B6014(void)
{
}

void Gp_SyncAreaKeyIndex(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaRec* rec2;
    GpAreaObj* obj;

    rec           = Gp_AreaTables[arg0->field_3];
    arg0->field_5 = 1;
    if (rec == NULL) {
        return;
    }
    rec2 = rec[arg0->field_2].field_0;
    obj  = rec[arg0->field_2].field_4;
    if (rec2 == NULL) {
        return;
    }
    if (obj->field_0 == 0) {
        obj->field_0  = 1;
        obj->field_1 |= 1;
    }
    arg0->field_5 = obj->field_0;
}

void func_800B6094(Task* task)
{
    if (task->spawnArg1 & 1) {
        task->killCountdown = 0;
    }
    task->state++;
}

void func_800B60C0(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80093A5C;
    sp.funcs[arg0->state](arg0);
}

void Gp_MakeDirOffset(SVECTOR* arg0, GpDirSrc* arg1, SVECTOR* arg2)
{
    void**            scratch;
    u8*               head;
    register SVECTOR* vec asm("s2");
    SVECTOR*          block;
    MATRIX*           mtx;
    GsCOORDINATE2*    coord;
    SVECTOR           tmp;
    SVECTOR*          tmpp;
    s32               scale;
    u16               srcx;
    u16               dstx;

    srcx = arg1->pos.vx;
    dstx = arg0->vx;
    SOFT_TOUCH_REG2(srcx, dstx);
    scratch                       = (void**)G_SCRATCH_HEAD;
    head                          = *scratch;
    block                         = (SVECTOR*)(head - 0x28);
    vec                           = block;
    ((SVECTOR*)(head - 0x28))->vx = srcx - dstx;
    SOFT_TOUCH_REG(block);
    vec->vy  = arg1->pos.vy - arg0->vy;
    *scratch = block;
    vec->vz  = arg1->pos.vz - arg0->vz;
    coord    = &D_80070F10;
    scale    = SquareRoot0(Gfx_ApplyMatrixNoSf(vec, vec));
    scale    = scale - arg1->field_2;
    if (scale >= 0) {
        scale = -scale;
    }
    VectorNormalSS(block, block);
    mtx = (MATRIX*)(head - 0x20);
    TransposeMatrix(&coord->workm, mtx);
    tmp = *(SVECTOR*)(head - 0x28);
    gte_SetRotMatrix(mtx);
    __asm__ volatile("addiu %0, $sp, 0x10" : "=r"(tmpp));
    gte_ldv0(tmpp);
    gte_rtv0_real();
    gte_stsv(vec);
    gte_lddp(scale);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(arg2);
    *scratch = (u8*)*scratch + 0x28;
}

void Gp_FreeSlot4TmdBuffers(void)
{
    Task*      child;
    Task*      iter;
    TmdObject* obj;

    child = ((Task*)Game_GetPtrSlot(4))->firstChild;
    if (child != NULL) {
        iter = child;
        do {
            if (iter->spawnType == 1) {
                obj           = iter->extra;
                obj->field_C |= 4;
                Tmd_FreeBuffers(obj);
            }
            iter = iter->nextSibling;
        } while (iter != child);
    }
}

void Gp_BindSlot4(Task* task)
{
    Game_SetPtrSlot(task, 4);
    task->field_24 = Gp_Slot4MsgTable;
    task->state++;
}

void func_800B6398(void)
{
    Gp_DrawTargetCursor();
}

typedef struct {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ byte pad_2[2];
    /* 0x04 */ u16  field_4;
    /* 0x06 */ byte pad_6[0xA];
} GpBit2Off2;
STATIC_ASSERT_SIZEOF(GpBit2Off2, 0x10);

s32 Gp_LookupBit2Item(s32 arg0)
{
    GpBit2List*  lists;
    GpBit2Rec*   rec;
    GpBit2Off2*  tail;
    GpItemA0*    attrs;
    s32          idx;
    s32          matched;
    register u16 item asm("v1");
    u16          extra;
    s32          term;
    s32          found;

    idx   = Mc_SaveData.field_7;
    lists = Gp_Bit2Banks[idx].field_0;
    found = 0;
    if (lists != NULL) {
        if (lists->field_0 != (GpBit2Rec*)0x7FFFFFFF) {
            term = 0xFFFF;
            do {
                rec     = lists->field_0;
                matched = 0;
                if (rec != NULL) {
                    if (rec->field_0 != term) {
                        attrs = Gp_StackLimits;
                        tail  = (GpBit2Off2*)&rec->field_2;
                        do {
                            if (rec->field_0 == arg0) {
                                item          = tail->field_0;
                                extra         = tail->field_4;
                                Gp_PubItemId  = arg0;
                                Gp_PubItemLoc = item;
                                D_80114DDE    = extra;
                                if (item < 0x100U) {
                                    if (func_800B7420(tail->field_0) != 0) {
                                        if ((u32)(tail->field_0 - 0x80) < 0x20U) {
                                            Gp_PubItemLoc = 0x3D;
                                        } else {
                                            Gp_PubItemLoc = 0xD;
                                        }
                                        Gp_PubItemQty   = 1;
                                        Gp_PubItemReady = 1;
                                    } else if ((u32)(tail->field_0 - 0xA0) < 0x20U) {
                                        if (Gp_GetCurBit2Flag(arg0) != 3) {
                                            idx           = tail->field_0 - 0xA0;
                                            Gp_PubItemQty = attrs[idx].field_0;
                                        } else {
                                            idx           = tail->field_0 - 0xA0;
                                            Gp_PubItemQty = attrs[idx].field_2;
                                        }
                                        Gp_PubItemReady = 1;
                                    } else {
                                        Gp_PubItemQty   = 1;
                                        Gp_PubItemReady = 1;
                                    }
                                }
                                found   = 1;
                                matched = found;
                                break;
                            }
                            rec++;
                            tail++;
                        } while (rec->field_0 != term);
                    }
                }
                if (matched == 1) {
                    break;
                }
                lists++;
            } while (lists->field_0 != (GpBit2Rec*)0x7FFFFFFF);
        }
    }
    return found;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B65B0);

void Gp_SpawnPlaceById(u16 arg0)
{
    GameSessionFrom4*    sess;
    register s32         hi asm("v1");
    register GpBit2Bank* tmp asm("a1");
    register GpBit2Bank* banks asm("t1");
    GpBit2List*          lists;
    GpEnemyPlace*        place;
    GpEnemyDesc*         desc;
    GpEnemy*             enemy;
    Task*                task;
    GameActorExt*        extra;
    GpCoordPlace*        coord;
    u16                  term;
    s32                  id;
    u16                  recId;
    u8                   idx8;

    sess = (GameSessionFrom4*)&Mc_SaveData.field_4;
    asm("lui %0, %%hi(Gp_Bit2Banks)" : "=r"(hi));
    idx8 = sess->field_3;
    asm("addiu %0, %1, %%lo(Gp_Bit2Banks)" : "=r"(tmp) : "r"(hi));
    lists = tmp[idx8].field_0;
    if (lists == NULL) {
        return;
    }
    place = (GpEnemyPlace*)lists[sess->field_2].field_0;
    if (place == NULL) {
        return;
    }
    term = 0xFFFF;
    id   = place->field_0;
    if (id == term) {
        return;
    }
    banks = tmp;
    do {
        if ((u16)id == (u16)arg0) {
            s32           temp;
            register u32* flags asm("v0");
            register s32  idx asm("v1");
            s32           shift;
            u32           word;

            temp   = (u16)id;
            flags  = banks[Game_Session->field_7].field_4;
            idx    = temp >> 4;
            flags += idx;
            shift  = (temp & 0xF) * 2;
            word   = *flags;
            word  &= 3 << shift;
            word >>= shift;
            if (word == 0) {
                return;
            }
            desc  = lists[sess->field_2].field_4;
            recId = desc->field_0;
            if (recId != term) {
                do {
                    if (recId == place->field_2) {
                        enemy = Gp_SpawnEnemyFromTable(&desc->field_4, 0, desc->field_0, NULL);
                        if (enemy != NULL) {
                            task = enemy->task;
                            if (task->spawnType != 0) {
                                extra             = (GameActorExt*)task->extra;
                                coord             = (GpCoordPlace*)extra->field_8;
                                enemy->field_8    = place->field_0 | (place->field_4 << 8);
                                enemy->field_A    = place->field_2;
                                coord->coord.t[0] = place->field_8;
                                coord->coord.t[1] = place->field_A;
                                coord->coord.t[2] = place->field_C;
                                coord->field_46   = place->field_E;
                                if (coord->field_46 != 0) {
                                    Gfx_RotMatrixY(&coord->coord, (s16)place->field_E, 1);
                                }
                                coord->flg = 0;
                            }
                        }
                        return;
                    }
                    desc++;
                    recId = desc->field_0;
                } while (recId != term);
            }
            return;
        }
        place++;
        id = place->field_0;
    } while (id != term);
}

void Gp_SpawnPlaces(GameSessionFrom4* arg0)
{
    GpBit2List*   lists;
    GpEnemyPlace* place;
    GpEnemyDesc*  desc;
    GpEnemy*      enemy;
    Task*         task;
    GameActorExt* extra;
    GpCoordPlace* coord;
    u16           term;
    u16           id;

    lists = Gp_Bit2Banks[arg0->field_3].field_0;
    if (lists == NULL) {
        return;
    }
    place = (GpEnemyPlace*)lists[arg0->field_2].field_0;
    if (place == NULL) {
        return;
    }
    term = 0xFFFF;
    if (place->field_0 == term) {
        return;
    }
    do {
        desc = lists[arg0->field_2].field_4;
        id   = desc->field_0;
        if (id != term) {
            do {
                if (id == place->field_2) {
                    enemy = Gp_SpawnEnemyFromTable(&desc->field_4, 0, desc->field_0, NULL);
                    if (enemy != NULL) {
                        task = enemy->task;
                        if (task->spawnType != 0) {
                            extra             = (GameActorExt*)task->extra;
                            coord             = (GpCoordPlace*)extra->field_8;
                            enemy->field_8    = place->field_0 | (place->field_4 << 8);
                            enemy->field_A    = place->field_2;
                            coord->coord.t[0] = place->field_8;
                            coord->coord.t[1] = place->field_A;
                            coord->coord.t[2] = place->field_C;
                            coord->field_46   = place->field_E;
                            if (coord->field_46 != 0) {
                                Gfx_RotMatrixY(&coord->coord, (s16)place->field_E, 1);
                            }
                            coord->flg = 0;
                        }
                    }
                    break;
                }
                desc++;
                id = desc->field_0;
            } while (id != term);
        }
        place++;
    } while (place->field_0 != term);
}

void Gp_ApplyItemMap(void)
{
    s32          i;
    GpItemSlot*  slots;
    GpItemQty*   qty0;
    GpItemQty*   qty1;
    GpItemMap*   map;
    GpItemSlot*  slot;
    GpItemSlot*  alt;
    s32          id;
    register s32 count asm("a1");
    s32          mapped;

    i     = 0;
    slots = Mc_SaveData.field_1C8;
    qty0  = Gp_RelatedQty0;
    qty1  = Gp_RelatedQty1;
    for (i = 0; i < 8; i++) {
        map  = &Gp_ItemMaps[i];
        id   = map->field_1;
        slot = (GpItemSlot*)((id << 3) + (s32)slots);
        alt  = slot;
        if (map->field_0 == 0) {
            mapped = map->field_2;
            TOUCH_REG_USE(id, mapped);
            id           -= 0x80;
            slot->field_0 = mapped;
            count         = 0;
            if ((u32)id < 0x20) {
                count = qty0[id].field_0;
            }
            slot->field_1 = count;
        } else {
            mapped = map->field_2;
            TOUCH_REG_USE(id, mapped);
            id           -= 0x80;
            slot->field_2 = mapped;
            count         = 0;
            if ((u32)id < 0x20) {
                count = qty1[id].field_0;
            }
            alt->field_3 = count;
        }
    }
}

s32 Gp_ConsumeSlotQty(s32 arg0, s32 arg1)
{
    GpItemSlot* slots;
    GpItemSlot* slot;
    s32*        counts;
    s32*        counter;
    McSaveData* save;
    s32         off8;
    s32         off4;
    s32         count;

    off8    = arg0 << 3;
    slots   = Mc_SaveData.field_1C8;
    slot    = (GpItemSlot*)(off8 + (s32)slots);
    off4    = arg0 << 2;
    counts  = (s32*)((s32)slots + 0x4C0);
    counter = (s32*)(off4 + (s32)counts);

    if (arg1 == 1) {
        if (slot->field_0 != 0) {
            count = slot->field_1;
            if (count != 0) {
                if (Mc_SaveData.field_5C2 == 0) {
                    slot->field_1 = count - 1;
                    Gp_ConsumeScanQty(&Mc_SaveData.field_5BC, slot->field_0, 1);
                    count = *counter;
                    if (count <= 0xF423E) {
                        *counter = count + 1;
                    }
                }
                goto done;
            }
        }
    }
    if (arg1 == 0x101) {
        count = slot->field_2;
        if (count != 0) {
            if (count != 0xFF) {
                count = slot->field_3;
                if (count != 0) {
                    save = &Mc_SaveData;
                    if (save->field_5C2 == 0) {
                        slot->field_3 = count - 1;
                        Gp_ConsumeScanQty(&save->field_5BC, slot->field_2, 1);
                        count = *counter;
                        if (count <= 0xF423E) {
                            *counter = count + 1;
                        }
                    }
                }
            }
        }
    }

done:
    if (!(arg1 & 0x100)) {
        return slot->field_1;
    }
    return slot->field_3;
}

s32 Gp_EquipRelatedBank(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32          index;
    s32          index2;
    GpItemRec*   table;
    GpItemScan*  scan;
    GpItemSlot*  slot;
    s32          found;
    s32          have;
    register s32 shifted asm("v0");

    scan  = &Mc_SaveData.field_5BC;
    table = Gp_GetItemTable(scan);
    if ((u32)(arg1 - 0x80) < 0x20U) {
        found = 0;
        if (arg1 >= 0xA0) {
            index   = scan->field_0;
            shifted = Gp_FindScanQty(table, scan, &index, arg1);
            shifted = shifted << 16;
        } else {
            register s32        i asm("v1");
            register s32        count asm("v0");
            register s32        end asm("a0");
            register s32        off asm("v0");
            register s32        limit asm("a1");
            register GpItemRec* rec asm("a0");

            i     = scan->field_0;
            count = scan->field_1;
            end   = i + count;
            if (i < end) {
                off   = i << 2;
                limit = end;
                rec   = (GpItemRec*)(off + (s32)table);
                for (; i < limit; i++, rec++) {
                    if (rec->field_0 == arg1) {
                        found = 1;
                        break;
                    }
                }
            }
            shifted = found;
            TOUCH_REG(shifted);
            shifted = shifted << 16;
        }
        if (shifted > 0) {
            register s32        off asm("v0");
            register GpItemQty* qtyTable asm("v1");
            register s32        maxQty asm("a1");
            register s32        clamped asm("a0");
            register s32        i asm("v1");
            register s32        idx asm("a0");
            GpItemQty*          row;

            off = arg1 << 2;
            if (arg0 == 0) {
                qtyTable = Gp_QtyById0;
            } else {
                qtyTable = Gp_QtyById1;
            }
            row = (GpItemQty*)(off + (s32)qtyTable);
            TOUCH_REG(row);
            idx    = arg1 - 0x80;
            maxQty = 0;
            if ((u32)idx < 0x20U) {
                off    = idx << 2;
                maxQty = *(u8*)((s32)qtyTable + off + 0x200);
            }
            clamped = maxQty;
            i       = 0;
            for (; i < 3; i++) {
                if (((u8*)((s32)row + i))[1] == arg2) {
                    break;
                }
            }
            if (i != 3) {
                if (arg3 < 0) {
                    arg3 = clamped;
                }
                if (clamped < arg3) {
                    arg3 = clamped;
                }
                index2 = scan->field_0;
                slot   = &Mc_SaveData.field_1C8[arg1];
                have   = (s16)Gp_FindScanQty(table, scan, &index2, arg2);
                have  -= Gp_CountEquippedRelated(scan, arg2);
                if (arg0 == 0) {
                    if (slot->field_0 == arg2) {
                        have += slot->field_1;
                    }
                } else if (slot->field_2 == arg2) {
                    have += slot->field_3;
                }
                if (have > 0) {
                    goto success;
                }
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    }
    return -1;
success:
    if (arg3 != 0) {
        if (have < arg3) {
            arg3 = have;
        }
        if (arg0 == 0) {
            slot->field_0 = arg2;
            slot->field_1 = arg3;
        } else if (slot->field_2 != 0xFF) {
            slot->field_2 = arg2;
            slot->field_3 = arg3;
        } else {
            arg3 = -1;
        }
        return arg3;
    }
    return 0;
}

s32 Gp_EquipRelatedItem(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32          index;
    s32          index2;
    GpItemRec*   table;
    GpItemSlot*  slot;
    s32          found;
    s32          have;
    register s32 useSecond asm("s5");
    register s32 shifted asm("v0");

    table = Gp_GetItemTable(arg0);
    if ((u32)(arg2 - 0xA0) >= 0x20U) {
        goto fail;
    }
    useSecond = 0;
    TOUCH_REG(useSecond);
    if ((u32)(arg1 - 0x80) >= 0x20U) {
        goto fail;
    }
    found = 0;
    if (arg1 >= 0xA0) {
        index   = arg0->field_0;
        shifted = Gp_FindScanQty(table, arg0, &index, arg1);
        shifted = shifted << 16;
    } else {
        register s32        i asm("v1");
        register s32        count asm("v0");
        register s32        end asm("a0");
        register s32        off asm("v0");
        register s32        limit asm("a1");
        register GpItemRec* rec asm("a0");

        i     = arg0->field_0;
        count = arg0->field_1;
        end   = i + count;
        if (i < end) {
            off   = i << 2;
            limit = end;
            rec   = (GpItemRec*)(off + (s32)table);
            for (; i < limit; i++, rec++) {
                if (rec->field_0 == arg1) {
                    found = 1;
                    break;
                }
            }
        }
        shifted = found;
        TOUCH_REG(shifted);
        shifted = shifted << 16;
    }
    if (shifted <= 0) {
    fail:
        SCHED_BARRIER();
        return -1;
    }
    {
        register s32        off asm("v0");
        register GpItemQty* qtyTable asm("v1");
        register s32        maxQty asm("a1");
        register s32        clamped asm("a0");
        register s32        i asm("v1");
        register s32        idx asm("a0");
        GpItemQty*          row;

        off      = arg1 << 2;
        qtyTable = Gp_QtyById0;
        TOUCH_REG(qtyTable);
        row = (GpItemQty*)(off + (s32)qtyTable);
        TOUCH_REG(row);
        idx    = arg1 - 0x80;
        maxQty = 0;
        if ((u32)idx < 0x20U) {
            off    = idx << 2;
            maxQty = *(u8*)((s32)qtyTable + off + 0x200);
        }
        clamped = maxQty;
        i       = 0;
        for (; i < 3; i++) {
            if (((u8*)((s32)row + i))[1] == arg2) {
                break;
            }
        }
        if (i == 3) {
            off       = arg1 << 2;
            useSecond = 1;
            qtyTable  = Gp_QtyById1;
            TOUCH_REG(qtyTable);
            row = (GpItemQty*)(off + (s32)qtyTable);
            TOUCH_REG(row);
            idx    = arg1 - 0x80;
            maxQty = 0;
            if ((u32)idx < 0x20U) {
                off    = idx << 2;
                maxQty = *(u8*)((s32)qtyTable + off + 0x200);
            }
            clamped = maxQty;
            i       = 0;
            for (; i < 3; i++) {
                if (((u8*)((s32)row + i))[1] == arg2) {
                    break;
                }
            }
            if (i == 3) {
                return -1;
            }
        }
        if (arg3 < 0) {
            arg3 = clamped;
        }
        if (clamped < arg3) {
            arg3 = clamped;
        }
        index2 = arg0->field_0;
        slot   = &Mc_SaveData.field_1C8[arg1];
        have   = (s16)Gp_FindScanQty(table, arg0, &index2, arg2);
        have  -= Gp_CountEquippedRelated(arg0, arg2);
        if (slot->field_0 == arg2) {
            have += slot->field_1;
        }
        if (slot->field_2 == arg2) {
            have += slot->field_3;
        }
        if (have <= 0) {
            return -1;
        }
        if (arg3 != 0) {
            if (have < arg3) {
                arg3 = have;
            }
            if (useSecond == 0) {
                slot->field_0 = arg2;
                slot->field_1 = arg3;
            } else if (slot->field_2 != 0xFF) {
                slot->field_2 = arg2;
                slot->field_3 = arg3;
            }
            goto join;
        }
        asm volatile("" : "=r"(arg3));
        arg3 = 0;
    join:
        if (arg3 > 0) {
            Gp_SetItemSeenBit(arg2, 1);
        }
        return arg3;
    }
}
