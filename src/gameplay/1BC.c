#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
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

#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_gpl12_real() __asm__ volatile("nop; nop; .word 0x4BA8003E")

void func_800B1EFC(Task* arg0);
void func_800B3448(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800B3910(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800B6358(Task* task);
void func_800B6398(void);
void func_8017FBD8(void);

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
extern s32            D_8010D208[];
extern char           D_800939F8[];
extern GsCOORDINATE2  D_80070F10;
extern s32            D_80070F60;
extern u8             D_800626E8;
extern u16            D_80114D14;
extern s32            D_80114D20;

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AF590);

s16 func_800AF89C(u16 arg0, u16 arg1, u16 arg2, u16 arg3)
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
    p->field_1A8 = D_80070F60;
    p->field_1AC = rand();
    D_80070F60   = 0;
    srand(1);
    D_80114D20 = 0xFFFF;
    return i;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800AFA44);

void func_800AFF90(u16 arg0)
{
    s32           i;
    s32           bits;
    GpSndMaskRec* entry;

    i = 0;
    if (D_8010D1C4[0].mask != 0) {
        bits = arg0;
        do {
            entry = &D_8010D1C4[(u16)i];
            if (bits & entry->mask) {
                SndEvt_EnqueueType7(entry->flags, 0);
                SndBank_SetEnableFlags(0, entry->flags);
            }
            i++;
        } while (D_8010D1C4[(u16)i].mask != 0);
    }
}

void func_800B0034(u16 arg0)
{
    s32           i;
    s32           bits;
    GpSndMaskRec* entry;

    i = 0;
    if (D_8010D1C4[0].mask != 0) {
        bits = arg0;
        do {
            entry = &D_8010D1C4[(u16)i];
            if (bits & entry->mask) {
                SndBank_SetEnableFlags(1, entry->flags);
            }
            i++;
        } while (D_8010D1C4[(u16)i].mask != 0);
    }
}

void func_800B00C4(void)
{
    CdCmdQueue* p;

    p            = &CdCmd_Queue;
    p->field_1FE = 0xFF;
    p->field_23A = 1;
    p->field_214 = 0;
    p->field_212 = 0;
    p->field_216 = 0;
    p->field_240 = 0;
    D_80070F60   = p->field_1A8;
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

void func_800B015C(void* arg0)
{
    CdCmd_Queue.field_198 = arg0;
}

GpEnemy* func_800B0168(s32 bank, s32 type, s32 arg2, GpEnemy* parent)
{
    Task*    task;
    GpEnemy* ret;

    task = Task_Spawn(bank, type, arg2, 0);
    if (task != NULL) {
        ret = func_800B0494(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

GpEnemy* func_800B01AC(TaskDesc* table, s32 idx, s32 arg2, GpEnemy* parent)
{
    Task*    task;
    GpEnemy* ret;

    task = Task_SpawnFromTable(table, idx, arg2, 0);
    if (task != NULL) {
        ret = func_800B0494(task, parent);
    } else {
        ret = NULL;
    }
    return ret;
}

void func_800B01F0(GpEnemy* enemy, Task* task)
{
    func_800DAB38(&enemy->node);
    Mem_Free(enemy);
    Task_Kill(task);
}

void func_800B0234(Task* task)
{
    GpEnemy* enemy;

    enemy = task->spawnArg2;
    func_800DAB38(&enemy->node);
    Mem_Free(enemy);
    Task_Kill(task);
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B0278);

GpEnemy* func_800B0494(Task* task, GpEnemy* parent)
{
    GpEnemy* enemy;

    enemy = Mem_Calloc(0x60, 0);
    if (enemy == NULL) {
        printf(D_800939F8);
        Task_Kill(task);
        return NULL;
    }

    task->exitCallback = func_800B0234;
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

void func_800B0544(GpEnemy* enemy, Task* task)
{
    enemy->field_C = 0x78;
    task->state++;
}

void func_800B0560(GpEnemy* enemy, Task* task)
{
    enemy->field_C--;
    if (enemy->field_C == 0) {
        task->state++;
    }
}

void func_800B058C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_80093A10;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

s32 func_800B05E8(s32 arg0)
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

void func_800B065C(u8 arg0)
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

void func_800B0748(Task* task)
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

void func_800B082C(Task* task)
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

void func_800B0FDC(MATRIX* arg0, SVECTOR* arg1)
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

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B114C);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B1460);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B17D4);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B1D00);

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

void func_800B2088(u16* arg0, u16* arg1, s32 arg2, u16* arg3)
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

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2200);

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

void func_800B27C4(u16* arg0, u16* arg1, s32 arg2, u16* arg3)
{
    s32 i;

    for (i = 0; i < 0x10; i++) {
        func_800B2088(arg0, arg1, arg2, arg3);
        arg0++;
        arg1++;
        arg3++;
    }
}

void func_800B2840(u16* arg0, u16* arg1, s32 arg2, u16* arg3, s32 arg4)
{
    s32 i;

    for (i = 0; i < 0x10; i++) {
        if ((1 << i) & arg4) {
            func_800B2088(arg0, arg1, arg2, arg3);
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

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2998);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B2E90);

void func_800B3108(GpAnimBlendSrc* arg0, GpAnimMtxRec* arg1, GpAnimSlot* arg2)
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
        func_800B2998(arg0, arg1, arg2, s);
        dest = arg0->field_8;
        if (dest != NULL) {
            dest->vx = s->vec1.vx >> 3;
            dest->vy = s->vec1.vy >> 3;
            dest->vz = s->vec1.vz >> 3;
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x80;
    }
}

void func_800B32E8(GpAnimCtx* arg0, s32 arg1)
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

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B3448);

void func_800B3910(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3)
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
    asm volatile("" : "+r"(raw));
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
    asm volatile("" : "+r"(one));
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

void func_800B3CCC(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3)
{
    arg0->field_0  = arg1;
    arg0->field_4  = &arg2->field_34;
    arg0->field_8  = arg3;
    arg0->field_10 = arg2->field_30;
}

void func_800B3CE8(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3)
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

void func_800B3DB4(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx           = arg1->field_15;
    arg0->field_C = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

void func_800B3DF4(GpAnimCtx* arg0, GpAnimSlot* arg1)
{
    u8 idx;

    idx           = arg1->field_15;
    arg0->field_C = arg1 - idx;
    func_800B3448(arg0, idx, 0, 0);
}

void func_800B3E34(GpAnimCtx* arg0, GpAnimSlot* arg1)
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

void func_800B3F60(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4)
{
    arg0->field_0  = arg1;
    arg0->field_4  = &arg2->field_34;
    arg0->field_8  = arg3;
    arg0->field_10 = arg2->field_30;
    arg0->field_C  = arg4;
}

void func_800B3F84(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4)
{
    func_800B3F60(arg0, arg1, arg2, arg3, arg4);
}

void func_800B3FA8(GpAnimCtx* arg0, s32 arg1, s32 arg2)
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

void func_800B404C(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
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

void func_800B40F4(GpAnimCtx* arg0, s32 arg1, s32 arg2)
{
    func_800B3910(arg0, arg1, arg2, 0);
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

void func_800B4248(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
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
    asm volatile("" ::"r"(off));
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
        asm volatile("" : "=r"(trans) : "r"(trans));
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

void func_800B43E0(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, GpAnimPose* arg3, s32 arg4,
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

void func_800B4514(GpAnimCtx* arg0, s32 arg1)
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

GpAnimRec* func_800B4668(GpAnimCtx* arg0, GpAnimSlot* arg1)
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

void func_800B47A8(GpAnimCtx* arg0, s32 arg1, s32 arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6,
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

void func_800B48FC(GpEnemy* arg0)
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

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4AF8);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B4E54);

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B51F4);

void func_800B56AC(void)
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
                rec   = D_8010CBCC[idx];
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

void func_800B57EC(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1)
{
    GsCOORDINATE2* dest;

    dest = arg1;
    if (dest->sub != arg0) {
        func_80098F58(arg0);
        func_80098F58(dest);
        dest->sub = arg0;
        func_800A8864(&arg0->workm, &dest->workm, &dest->coord);
        dest->flg = 0;
    }
}

GpWorkObj* func_800B584C(u16 arg0)
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

void func_800B58D4(TmdObject* arg0, s32 arg1, s32 arg2)
{
    arg0->field_24 = arg1;
    arg0->field_25 = arg2;
    if (arg0->field_18 != NULL) {
        Tmd_ProcessStream(arg0);
        Tmd_ProcessStream(arg0);
    }
}

void func_800B5914(s32 arg0)
{
    GpAreaRec* rec;
    GpAreaObj* obj;
    GpAreaKey* key;

    key = (GpAreaKey*)&Mc_SaveData.field_4;
    rec = D_8010CBCC[key->field_3];
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

s32 func_800B59A8(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* obj;
    s32        val;

    rec = D_8010CBCC[arg0->field_3];
    if (rec != NULL) {
        obj = rec[arg0->field_2].field_4;
        if (obj != NULL) {
            val = obj->field_1 & 2;
            return val != 0;
        }
    }
    return 0;
}

GpAreaObj* func_800B5A08(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* ret;

    rec = D_8010CBCC[arg0->field_3];
    if (rec == NULL) {
        ret = NULL;
    } else {
        ret = rec[arg0->field_2].field_4;
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B5A48);

void func_800B5B30(GpAreaKey* arg0, s32 arg1, s32 arg2)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = D_8010CBCC[arg0->field_3];
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

void func_800B5BFC(s32 arg0, GpAreaKey* arg1)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = D_8010CBCC[arg1->field_3];
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

GpAreaObj* func_800B5C88(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* ret;

    rec = D_8010CBCC[arg0->field_3];
    ret = NULL;
    if (rec != NULL) {
        rec = rec[arg0->field_2].field_0;
        if (rec != NULL) {
            ret = rec[arg0->field_5].field_4;
        }
    }
    return ret;
}

GpAreaRec* func_800B5CE8(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaRec* ret;

    rec = D_8010CBCC[arg0->field_3];
    ret = NULL;
    if (rec != NULL) {
        ret = rec[arg0->field_2].field_0;
        if (ret != NULL) {
            ret = &ret[arg0->field_5];
        }
    }
    return ret;
}

void func_800B5D44(GpAreaKey* arg0)
{
    u32        key;
    GpAreaRec* rec;
    GpAreaObj* obj;

    key = *(u32*)&arg0->field_0 & 0xFFFF0000;
    rec = D_8010CBCC[arg0->field_3];
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
    TaskFunc funcs[2] = { func_800B6358, func_800B6398 };

    funcs[arg0->state](arg0);
}

s32 func_800B5E08(Task* arg0, Task* arg1, s32 arg2, Task** arg3)
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

s32 func_800B5E78(Task* arg0, Task* arg1, s32 arg2, Task** arg3)
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

s32 func_800B5EE8(Task* arg0)
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

s32 func_800B5F5C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
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
            func_800AC464(arg0, arg3, arg2, 0);
        }
        arg0 = next;
    } while (arg0 != child);
    return 0;
}

void func_800B5FEC(void)
{
    Task_KillChildren(Game_GetPtrSlot(4));
}

void func_800B6014(void)
{
}

void func_800B601C(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaRec* rec2;
    GpAreaObj* obj;

    rec           = D_8010CBCC[arg0->field_3];
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

void func_800B6118(SVECTOR* arg0, GpDirSrc* arg1, SVECTOR* arg2)
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
    asm("" : "+r"(srcx), "+r"(dstx));
    scratch                       = (void**)G_SCRATCH_HEAD;
    head                          = *scratch;
    block                         = (SVECTOR*)(head - 0x28);
    vec                           = block;
    ((SVECTOR*)(head - 0x28))->vx = srcx - dstx;
    asm("" : "+r"(block));
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

void func_800B62D4(void)
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

void func_800B6358(Task* task)
{
    Game_SetPtrSlot(task, 4);
    task->field_24 = D_8010D208;
    task->state++;
}

void func_800B6398(void)
{
    func_800D9DFC();
}

typedef struct {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ byte pad_2[2];
    /* 0x04 */ u16  field_4;
    /* 0x06 */ byte pad_6[0xA];
} GpBit2Off2;
STATIC_ASSERT_SIZEOF(GpBit2Off2, 0x10);

s32 func_800B63B8(s32 arg0)
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
    lists = D_8010D230[idx].field_0;
    found = 0;
    if (lists != NULL) {
        if (lists->field_0 != (GpBit2Rec*)0x7FFFFFFF) {
            term = 0xFFFF;
            do {
                rec     = lists->field_0;
                matched = 0;
                if (rec != NULL) {
                    if (rec->field_0 != term) {
                        attrs = D_8010E3B8;
                        tail  = (GpBit2Off2*)&rec->field_2;
                        do {
                            if (rec->field_0 == arg0) {
                                item       = tail->field_0;
                                extra      = tail->field_4;
                                D_80114DEC = arg0;
                                D_80114DDC = item;
                                D_80114DDE = extra;
                                if (item < 0x100U) {
                                    if (func_800B7420(tail->field_0) != 0) {
                                        if ((u32)(tail->field_0 - 0x80) < 0x20U) {
                                            D_80114DDC = 0x3D;
                                        } else {
                                            D_80114DDC = 0xD;
                                        }
                                        D_80114DD0 = 1;
                                        D_80114DC8 = 1;
                                    } else if ((u32)(tail->field_0 - 0xA0) < 0x20U) {
                                        if (func_800BB470(arg0) != 3) {
                                            idx        = tail->field_0 - 0xA0;
                                            D_80114DD0 = attrs[idx].field_0;
                                        } else {
                                            idx        = tail->field_0 - 0xA0;
                                            D_80114DD0 = attrs[idx].field_2;
                                        }
                                        D_80114DC8 = 1;
                                    } else {
                                        D_80114DD0 = 1;
                                        D_80114DC8 = 1;
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

void func_800B6950(u16 arg0)
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
    asm("lui %0, %%hi(D_8010D230)" : "=r"(hi));
    idx8 = sess->field_3;
    asm("addiu %0, %1, %%lo(D_8010D230)" : "=r"(tmp) : "r"(hi));
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
                        enemy = func_800B01AC(&desc->field_4, 0, desc->field_0, NULL);
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

void func_800B6B44(GameSessionFrom4* arg0)
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

    lists = D_8010D230[arg0->field_3].field_0;
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
                    enemy = func_800B01AC(&desc->field_4, 0, desc->field_0, NULL);
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

void func_800B6CF0(void)
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
    qty0  = D_8010E238;
    qty1  = D_8010D278;
    for (i = 0; i < 8; i++) {
        map  = &D_8010D2F8[i];
        id   = map->field_1;
        slot = (GpItemSlot*)((id << 3) + (s32)slots);
        alt  = slot;
        if (map->field_0 == 0) {
            mapped = map->field_2;
            asm volatile("" : "+r"(id) : "r"(mapped));
            id           -= 0x80;
            slot->field_0 = mapped;
            count         = 0;
            if ((u32)id < 0x20) {
                count = qty0[id].field_0;
            }
            slot->field_1 = count;
        } else {
            mapped = map->field_2;
            asm volatile("" : "+r"(id) : "r"(mapped));
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

s32 func_800B6DA4(s32 arg0, s32 arg1)
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
                    func_800BB2D4(&Mc_SaveData.field_5BC, slot->field_0, 1);
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
                        func_800BB2D4(&save->field_5BC, slot->field_2, 1);
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

s32 func_800B6EE0(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
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
    table = func_800BB500(scan);
    if ((u32)(arg1 - 0x80) < 0x20U) {
        found = 0;
        if (arg1 >= 0xA0) {
            index   = scan->field_0;
            shifted = func_800BBCCC(table, scan, &index, arg1);
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
            asm volatile("" : "+r"(shifted));
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
                qtyTable = D_8010E038;
            } else {
                qtyTable = D_8010D078;
            }
            row = (GpItemQty*)(off + (s32)qtyTable);
            asm volatile("" : "+r"(row));
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
                have   = (s16)func_800BBCCC(table, scan, &index2, arg2);
                have  -= func_800BAFF4(scan, arg2);
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

INCLUDE_ASM("gameplay/nonmatchings/1BC", func_800B715C);
