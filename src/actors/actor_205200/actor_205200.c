#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "actors/actor_205200.h"
#include "actors/actors_shared_80149e54.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

/// Work block of the controller task, allocated by its setup handler
/// `func_actor_205200_8014A72C`. It spawns the parts, tracks the live ones
/// (`field_0`/`field_18`, indexed by the slot each part took) and drives the
/// looping sound and the screen-wave timer.
typedef struct Actor205200CtrlWork {
    /* 0x00 */ GsCOORDINATE2* field_0[3];  // coords of the parts, measured by `func_actor_205200_8014ACD4`
    /* 0x0C */ GsCOORDINATE2* field_C;     // nearest of `field_0` to the stage view
    /* 0x10 */ u32            field_10;    // its distance
    /* 0x14 */ s32            field_14;    // sound-event id `func_actor_205200_8014A958` plays
    /* 0x18 */ s16            field_18[3]; // 1 marks the matching `field_0` slot live
    /* 0x1E */ s16            field_1E;    // kind from the placement; selects the spawn tables
    /* 0x20 */ s16            field_20;    // live part count, also the next part's slot and the timer reload index
    /* 0x22 */ u16            field_22;    // countdown `func_actor_205200_8014AB98` ticks in both of its sub-states
    /* 0x24 */ s16            field_24;    // state of `func_actor_205200_8014A958` (0 wait, 1 run, 2 stop, 3 done)
    /* 0x26 */ s16            field_26;    // sub-state of `func_actor_205200_8014AB98`
    /* 0x28 */ s16            field_28;    // set while the screen wave is running
    /* 0x2A */ s16            field_2A;    // delay before the sound starts
    /* 0x2C */ s16            field_2C;    // set when a part dies, forcing the nearest part to be re-measured
    /* 0x2E */ s16            field_2E;    // raised by message 0x7DB; stops the sound and sends the parts to state 2
} Actor205200CtrlWork;
STATIC_ASSERT_SIZEOF(Actor205200CtrlWork, 0x30);

/// Work block of a part task, allocated by its spawn handler
/// `func_actor_205200_8014AE0C`. `field_78` is the slot the part took in the
/// controller's `field_0` / `field_18` arrays.
typedef struct Actor205200Part {
    /* 0x00 */ GpObj    obj;
    /* 0x20 */ GpRec18  recs[3];
    /* 0x68 */ GpEffArg field_68; // record the part's effects are spawned with
    /* 0x70 */ s16      field_70; // hit-stun countdown; hits are ignored while non-zero
    /* 0x72 */ s16      field_72; // state of the teardown handler `func_actor_205200_8014B484`
    /* 0x74 */ u16      field_74; // effect timer
    /* 0x76 */ s16      field_76; // spark cooldown
    /* 0x78 */ s16      field_78;
    /* 0x7A */ byte     pad_7A[2];
} Actor205200Part;
STATIC_ASSERT_SIZEOF(Actor205200Part, 0x7C);

extern s16           D_800691CA;
extern u8            D_80070F87;
extern u16           D_80071078;
extern u8            D_801153F4;
extern s32           D_actor_205200_8014CA5C;
extern ActorWaveCtx* D_actor_205200_80156814;
extern ActorWaveRec  D_actor_205200_80156818[9];
extern ActorWaveRec  D_actor_205200_80156868[30];
extern POLY_FT4      D_actor_205200_80156A98[][30][8];
extern u16           D_actor_205200_8014C9CC[];
extern s16           D_actor_205200_8014CA1C[];
extern TaskDesc      D_actor_205200_8014CA60;
extern u8            D_actor_205200_8014CA78[];
extern TaskDesc      D_actor_205200_8014CA44;
/// Context of the screen-wave task: `func_actor_205200_8014AB98` sets its
/// length and peak and spawns the task with it, and the controller's state
/// handlers drive its mode (`field_4`) - 2 while idle, 1 to ramp the wave down.
extern ActorWaveCtx D_actor_205200_8015B458;
extern GpPairSrcE   D_actor_205200_8014C9BC;
extern SVECTOR*     D_actor_205200_8014CA24[];
extern u16*         D_actor_205200_8014CA34[];

void func_8017E090(s32, s32);
void func_8017EE08(s32, s32);
void func_80182A14(s32, s32);

void func_actor_205200_8014AB98(Task* arg0);
void func_actor_205200_8014ACD4(Task* arg0);
s32  func_actor_205200_8014B914(s32 arg0);
void func_actor_205200_8014B9D4(GpEnemy* arg0, Task* arg1);
void func_actor_205200_8014BA94(Task* arg0);

/// Screen-wave task, spawned through `D_actor_205200_8014CA44` with the
/// context `func_actor_205200_8014AB98` fills. State 0 seeds random phases and
/// speeds for the 9 column and 30 row waves and builds, for each display
/// buffer, a grid of textured quads that re-draws the frame buffer. State 1
/// ramps the amplitude up to the context's peak, back down once its mode turns
/// to 1, and kills the task at mode 2; each frame it displaces every quad
/// vertex by the sine of its row and column waves.
void func_actor_205200_80149E54(Task* arg0)
{
    ActorWaveScratch* scratch;
    ActorWaveScratch* head;
    ActorWaveCtx*     ctx;
    ActorWaveRec*     cols;
    POLY_FT4*         p;
    DR_STP*           stp;
    s32               i;
    s32               j;
    s32               k;
    s32               rowIndex;
    s32               rowBack;
    s32               u0;
    s32               u1;
    s32               v0;
    s32               v1;
    s32               waveX0;
    s32               waveY0;
    s32               waveX1;
    s32               waveY1;
    s32               waveX2;
    s32               waveY2;
    s32               waveX3;
    s32               waveY3;
    ActorWaveRec*     row;
    POLY_FT4(*grid)
    [8];
    s32 tpage0;
    s32 tpage1;

    head                                = *(ActorWaveScratch**)G_SCRATCH_HEAD;
    D_800691CA                          = 2;
    *(ActorWaveScratch**)G_SCRATCH_HEAD = head - 1;
    cols                                = head[-1].cols;
    scratch                             = head - 1;
    switch (arg0->state) {
        case 0:
            for (i = 0; i < 9; i++) {
                D_actor_205200_80156818[i].phase  = 0;
                D_actor_205200_80156818[i].offset = (u32)rand() >> 3;
                D_actor_205200_80156818[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            for (i = 0; i < 30; i++) {
                D_actor_205200_80156868[i].phase  = 0;
                D_actor_205200_80156868[i].offset = (u32)rand() >> 3;
                D_actor_205200_80156868[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            D_actor_205200_8014CA5C          = 0;
            D_actor_205200_80156814          = arg0->spawnArg2;
            D_actor_205200_80156814->field_6 = 0;
            D_actor_205200_80156814->field_4 = 0;
            Display_ClampField126(-8);
            for (i = 0; i < 2; i++) {
                tpage0 = getTPage(2, 0, 0, i << 8);
                tpage1 = getTPage(2, 0, 128, i << 8);
                grid   = D_actor_205200_80156A98[i];
                for (j = -1; j < 29; j++) {
                    p = grid[j];
                    for (k = 0; k < 8; p++, k++) {
                        setPolyFT4(p);
                        if (D_actor_205200_80156814->field_8 == 0) {
                            setShadeTex(p, 1);
                        } else {
                            setShadeTex(p, 0);
                            p->r0 = D_actor_205200_80156814->field_9;
                            p->g0 = D_actor_205200_80156814->field_A;
                            p->b0 = D_actor_205200_80156814->field_B;
                        }
                        u0 = k * 40;
                        u1 = (k + 1) * 40;
                        if (u1 == 320) {
                            u1 = 319;
                        }
                        if (u0 < 128) {
                            p->tpage = tpage0;
                        } else {
                            p->tpage = tpage1;
                            u0      -= 128;
                            u1      -= 128;
                        }
                        v1 = (j + 1) * 8 + i * 16;
                        if (j != -1) {
                            v0 = j * 8 + i * 16;
                        } else {
                            v0 = i * 16 + 8;
                            v1 = i * 16;
                        }
                        p->u0 = u0;
                        p->v0 = v0;
                        p->u1 = u1;
                        p->v1 = v0;
                        do {
                            p->u2 = u0;
                            p->v2 = v1;
                            p->u3 = u1;
                        } while (0);
                        p->v3 = v1;
                    }
                }
            }
            arg0->state++;
            break;
        case 1:
            ctx = D_actor_205200_80156814;
            switch (ctx->field_4) {
                case 0:
                    if (ctx->field_6 < ctx->field_0) {
                        ctx->field_6++;
                    }
                    break;
                case 1:
                    if (ctx->field_6 > 0) {
                        if (D_801153F4 == 0) {
                            ctx->field_6--;
                        }
                    } else {
                        ctx->field_4 = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_actor_205200_8014CA5C = D_actor_205200_80156814->field_6 * D_actor_205200_80156814->field_2 / D_actor_205200_80156814->field_0;
            for (i = 0; i < 9; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_actor_205200_80156818[i].phase += D_actor_205200_80156818[i].speed;
                }
                *(s32*)&cols[i] = *(s32*)&D_actor_205200_80156818[i];
            }
            for (i = 0; i < 30; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_actor_205200_80156868[i].phase += D_actor_205200_80156868[i].speed;
                }
                *(s32*)&scratch->rows[i] = *(s32*)&D_actor_205200_80156868[i];
            }
            rowIndex = -1;
            for (j = -1; j < 29; rowIndex += 2, j++, rowIndex--) {
                rowBack = -rowIndex;
                row     = scratch->rows - rowBack;
                grid    = D_actor_205200_80156A98[D_80070F87];
                p       = grid[j];
                for (k = 0; k < 8; k++, p++) {
                    if (j != -1) {
                        waveX0 = D_actor_205200_8014CA5C * (rsin((j << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x0  = k * 40 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_actor_205200_8014CA5C * (rsin((k << 10) + row->phase + row->offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_actor_205200_8014CA5C * (rsin((j << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 40 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_actor_205200_8014CA5C * (rsin(((k + 1) << 10) + row->phase + row->offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        p->x0 = k * 40 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 40 - 160;
                        p->y1 = -112;
                    }
                    {
                        ActorWaveRec* next = row + 1;
                        waveX2             = D_actor_205200_8014CA5C * (rsin(((j + 1) << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x2              = k * 40 + (s16)((waveX2 >> 20) - 160);
                        waveY2             = D_actor_205200_8014CA5C * (rsin((k << 10) + row[1].phase + next->offset) << 3);
                        p->y2              = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3             = D_actor_205200_8014CA5C * (rsin(((j + 1) << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x3              = (k + 1) * 40 + (s16)((waveX3 >> 20) - 160);
                        waveY3             = D_actor_205200_8014CA5C * (rsin(((k + 1) << 10) + row[1].phase + next->offset) << 3);
                        p->y3              = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    addPrim(&gGpuCurrentOt[3], p);
                }
                SOFT_USE_REG(p);
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
    *(ActorWaveScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_205200_8014A72C(GpEnemy* enemy, Task* task)
{
    Actor205200CtrlWork* work;
    u16                  kind;
    s32                  i;
    u16                  timer;

    kind = ((u16*)enemy->place)[1];
    if ((u16)(kind - 1) >= 3) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    work = memCalloc(0x30, false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work                      = (TaskIdMap*)work;
    work->field_1E                  = kind;
    D_actor_205200_8015B458.field_4 = 2;
    for (i = 0; i < D_actor_205200_8014CA1C[work->field_1E]; i++) {
        Gp_SpawnEnemyFromTable(&D_actor_205200_8014CA60, 1, 0, enemy);
    }
    timer          = D_actor_205200_8014C9CC[D_actor_205200_8014CA1C[work->field_1E]];
    work->field_2A = 5;
    work->field_22 = timer;
    /* The empty `case 0` is load-bearing: a fourth case node makes GCC root
       the decision tree at 1 (`beq 1; slti <2`) instead of at 2. */
    switch (work->field_1E) {
        case 1:
            func_8017E090(0, 0);
            func_8017E090(1, 0);
            GameFlag_SetNibble(0x142, 0);
            GameFlag_SetNibble(0x143, 0);
            break;
        case 2:
            func_8017EE08(0, 0);
            func_8017EE08(1, 0);
            func_8017EE08(2, 0);
            GameFlag_SetNibble(0x144, 0);
            GameFlag_SetNibble(0x145, 0);
            break;
        case 3:
            func_80182A14(0, 0);
            func_80182A14(1, 0);
            GameFlag_SetNibble(0x153, 0);
            GameFlag_SetNibble(0x154, 0);
            break;
        case 0:
            break;
    }
    task->msgTable = D_actor_205200_8014CA78;
    task->state    = 1;
}

void func_actor_205200_8014A958(GpEnemy* enemy, Task* task)
{
    Actor205200CtrlWork* work = task->work;
    s16                  state;
    s32                  pulse;

    if (gGameSession->eventState != 0 || work->field_2E != 0) {
        pulse = work->field_28;
        if (pulse == 1) {
            D_actor_205200_8015B458.field_4 = pulse;
            work->field_28                  = 0;
        }
        if (work->field_2E != 0) {
            work->field_24 = 3;
            if (work->field_2E != 0) {
                if (work->field_14 != 0) {
                    SndEvt_EnqueueType7(work->field_14, 1);
                    work->field_14 = 0;
                }
            }
        }
    } else if (D_801153F4 == 0) {
        state = work->field_24;
        switch (state) {
            case 0:
                if ((u16)--work->field_2A == 0) {
                    func_actor_205200_8014ACD4(task);
                    if (work->field_C != NULL) {
                        work->field_14 = ((enemy->placeKey >> 12) << 8) | 0x40340001;
                        SndEvt_EnqueueType6(
                            work->field_14, 0, (s8)func_actor_205200_8014B914(work->field_10));
                        work->field_24 = 1;
                    }
                }
                break;
            case 1:
                if (gGameSession->viewReady == state || work->field_2C == state) {
                    work->field_2C = 0;
                    func_actor_205200_8014ACD4(task);
                    if (work->field_C != NULL) {
                        SndEvt_EnqueueTypeA(
                            work->field_14, 0, (s8)func_actor_205200_8014B914(work->field_10));
                    }
                }
                func_actor_205200_8014AB98(task);
                if (work->field_20 <= 0) {
                    work->field_24 = 2;
                }
                break;
            case 2:
                pulse = work->field_28;
                if (pulse == 1) {
                    D_actor_205200_8015B458.field_4 = pulse;
                    work->field_28                  = 0;
                }
                SndEvt_EnqueueType7(work->field_14, 1);
                work->field_24 = 3;
                if (work->field_1E == state) {
                    SndEvt_EnqueueType2(0, 0x3C);
                }
                break;
        }
        ((TmdObject*)task->extra)->coords->flg = 0;
    }
}

void func_actor_205200_8014AB98(Task* arg0)
{
    Actor205200CtrlWork* work  = arg0->work;
    s32                  state = work->field_26;

    switch (state) {
        case 0:
            if ((s16)--work->field_22 <= 0) {
                if (D_actor_205200_8015B458.field_4 == 2) {
                    D_actor_205200_8015B458.field_0 = 0xF;
                    D_actor_205200_8015B458.field_2 = 0xA0;
                    Task_SpawnFromTable(&D_actor_205200_8014CA44, 0, 0, (s32)&D_actor_205200_8015B458);
                    Gp_ArmStateF0(1);
                    work->field_28 = 1;
                    SndEvt_EnqueueType6(((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40340002, 0, 0);
                }
                work->field_22 = 20;
                work->field_26 = 1;
            }
            break;
        case 1:
            if ((s16)--work->field_22 <= 0) {
                D_actor_205200_8015B458.field_4 = state;
                work->field_22                  = D_actor_205200_8014C9CC[work->field_20];
                work->field_26                  = 0;
                Gp_SpendMp(1);
                work->field_28 = 0;
            }
            break;
    }
}

void func_actor_205200_8014ACD4(Task* arg0)
{
    Actor205200CtrlWork* work = arg0->work;
    GpViewRec*           view;
    VECTOR               d;
    u32                  dist;
    s32                  i;

    work->field_C  = NULL;
    work->field_10 = -1;
    view           = Gp_GetStageView(&gGameSession->at4.loc);
    for (i = 0; i < 3; i++) {
        if (work->field_18[i] == 1) {
            work->field_0[i]->flg = 0;
            Gp_UpdateCoord(work->field_0[i]);
            d.vx = view->mtx.t[0] + work->field_0[i]->coord.t[0];
            d.vy = view->mtx.t[1] + work->field_0[i]->coord.t[1];
            d.vz = view->mtx.t[2] + work->field_0[i]->coord.t[2];
            dist = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
            if (dist < work->field_10) {
                work->field_C  = work->field_0[i];
                work->field_10 = dist;
            }
        }
    }
}

void func_actor_205200_8014AE0C(GpEnemy* arg0, Task* arg1)
{
    GsCOORDINATE2*       coord;
    Actor205200CtrlWork* pwork;
    Actor205200Part*     part;
    SVECTOR*             pos;
    SVECTOR              rot;
    MATRIX*              mat;
    u16*                 tbl;

    coord = ((TmdObject*)arg1->extra)->coords;
    pwork = (Actor205200CtrlWork*)arg1->parent->work;
    part  = memCalloc(0x7CU, false);
    if (part == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)part;
    part->field_78 = pwork->field_20;
    pwork->field_20++;
    pwork->field_0[part->field_78]  = coord;
    pwork->field_18[part->field_78] = 1;
    tbl                             = D_actor_205200_8014CA34[pwork->field_1E];
    rot.vx                          = 0;
    mat                             = &coord->coord;
    rot.vy                          = tbl[part->field_78];
    rot.vz                          = 0;
    RotMatrix(&rot, mat);
    pos               = D_actor_205200_8014CA24[pwork->field_1E];
    coord->coord.t[0] = pos[part->field_78].vx;
    coord->coord.t[1] = pos[part->field_78].vy;
    coord->coord.t[2] = pos[part->field_78].vz;
    coord->sub        = &gGfxViewCoord;
    coord->flg        = 0;
    arg0->field_4     = mat;
    arg0->field_48    = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = coord;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &D_actor_205200_8014C9BC;
    arg0->recs       = part->recs;
    arg0->hp         = D_actor_205200_8014C9BC.hpMax;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    part->field_68.spawnArgLo = 0x400;
    part->field_68.spawnArgHi = 3;
    part->field_68.coord      = coord;
    part->obj.coord           = coord;
    part->obj.ctx.recs        = part->recs;
    part->obj.pos.vx          = 0;
    part->obj.pos.vy          = 0;
    part->obj.pos.vz          = 0;
    part->obj.key             = 0x30034;
    part->obj.radius          = 0x1C2;
    part->obj.flags           = 1;
    Gp_LinkObj(2, &part->obj);
    Gp_InitRec18Table(part->recs, 3, 0);
    part->obj.flags |= 0x8000;
    arg1->state      = 1;
}

/// Hit handling of a live part: applies the part's damage-kind hits (records
/// of kind 2) to the owning enemy's HP, killing the part at zero, and otherwise
/// arms the hit-stun timer `field_70`, the effect timer `field_74` and the
/// spark cooldown `field_76`. `arg1` is passed as 1 by
/// `func_actor_205200_8014B9D4` and unused.
void func_actor_205200_8014B048(Task* arg0, s32 arg1)
{
    VECTOR*              vec;
    Actor205200Part*     part;
    GpEnemy*             enemy;
    GsCOORDINATE2*       coord;
    Actor205200CtrlWork* parentWork;
    s32                  damage;
    s32                  i;
    s32                  snd;
    s32                  hitTime;
    s32                  clamped;

    vec   = --*(VECTOR**)0x1F8003FC;
    coord = ((TmdObject*)arg0->extra)->coords;
    part  = (Actor205200Part*)arg0->work;
    enemy = arg0->spawnArg2;
    if (part->field_70 != 0) {
        part->field_70--;
        if (part->field_70 <= 0) {
            part->field_70 = 0;
        }
    }
    if (part->field_76 != 0) {
        part->field_76--;
    }
    if (part->field_70 == 0) {
        for (i = 0; i < 3; i++) {
            if ((part->recs[i].key & 0xFFFF0000) != 0x20000) {
                continue;
            }
            if (part->recs[i].key & 0x8000) {
                func_800DA6E8(&enemy->node, 0, 0);
                break;
            }
            vec->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
            vec->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            damage  = Gp_ComputeDamage(part->recs[i].key, SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz), 0, 0);
            if (Gp_RollEnemyChance(enemy, part->recs[i].key, 0) != 0) {
                damage *= 4;
                Gp_SpawnEff(0x6009C, coord, 0, NULL);
            }
            func_800DA6E8(&enemy->node, damage, 0);
            enemy->hp -= damage;
            if (enemy->hp <= 0) {
                arg0->state                                                          = 2;
                part->field_72                                                       = 0;
                ((Actor205200CtrlWork*)arg0->parent->work)->field_18[part->field_78] = 0;
                ((Actor205200CtrlWork*)arg0->parent->work)->field_0[part->field_78]  = NULL;
                Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
                Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
                Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
                Gp_SpawnEff(0x6005C, coord, 0x02002600, NULL);
                Gp_SpawnEff(0x6005C, coord, 0x02002600, NULL);
                snd = ((enemy->placeKey >> 12) << 8) | 0x40340004;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                Gp_SpawnPadLerp(10, 0xFF, 0x80);
            } else if (damage > 0) {
                if (part->field_76 == 0) {
                    if ((Gp_GetIdParam0(part->recs[i].key) & 0xFFFF) == 7) {
                        func_800FDB18(3, coord, NULL, &part->field_68);
                    }
                    func_800FDB18(7, coord, NULL, &part->field_68);
                    part->field_76 = 10;
                }
                if (damage < 201) {
                    clamped = damage;
                } else {
                    clamped = 200;
                }
                part->field_74 = (clamped * 120) / 200 + 30;
                hitTime        = Gp_GetIdParam2(part->recs[i].key);
                if (hitTime > 0) {
                    part->field_70 = hitTime;
                }
                snd = ((enemy->placeKey >> 12) << 8) | 0x40340003;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
        }
    }
    Gp_ClearRec18Occupied(part->recs);
    *(VECTOR**)0x1F8003FC += 1;
}

void func_actor_205200_8014B484(GpEnemy* arg0, Task* arg1)
{
    Actor205200Part*     part;
    GsCOORDINATE2*       coord;
    Actor205200CtrlWork* work;
    GpViewRec*           view;
    VECTOR               d;
    s32                  dist;
    s32                  snd;
    s32                  pan;
    s32                  vol;

    part  = (Actor205200Part*)arg1->work;
    coord = ((TmdObject*)arg1->extra)->coords;
    work  = (Actor205200CtrlWork*)arg1->parent->work;
    if (Gp_StateF0.field_4 != 0) {
        return;
    }
    switch (part->field_72) {
        case 0:
            Gp_SpawnEff(0x60070, coord, 0x32001400, NULL);
            Gp_SpawnEff(0x60070, coord, 0x32001400, NULL);
            Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
            Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            Gp_ReleaseStateF0Add(arg1, 0x34);
            arg0->recs     = 0;
            work->field_2C = 1;
            work->field_20--;
            Gp_StateF0.field_1D |= 1;
            switch (work->field_1E) {
                case 1:
                    func_8017E090((u8)part->field_78, 1);
                    GameFlag_SetNibble(part->field_78 + 0x142, 1);
                    break;
                case 2:
                    func_8017EE08((u8)part->field_78, 1);
                    GameFlag_SetNibble(part->field_78 + 0x144, 1);
                    break;
                case 3:
                    func_80182A14((u8)part->field_78, 1);
                    GameFlag_SetNibble(part->field_78 + 0x153, 1);
                    break;
                case 0:
                    break;
            }
            part->field_72 = 1;
            part->field_74 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F) + 0x1E;
            part->field_76 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F) + 0x1E;
            break;
        case 1:
            if ((s16)--part->field_74 <= 0) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                part->field_74 = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
                func_800FDB18(7, coord, NULL, &part->field_68);
                Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
                view = Gp_GetStageView(&gGameSession->at4.loc);
                d.vx = view->mtx.t[0] + coord->coord.t[0];
                d.vy = view->mtx.t[1] + coord->coord.t[1];
                d.vz = view->mtx.t[2] + coord->coord.t[2];
                dist = SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz);
                snd  = ((arg0->placeKey >> 12) << 8) | 0x40340005;
                pan  = (s8)Gp_GetObjPan(coord);
                vol  = dist - D_80071078;
                if (vol >= 0x7FFF) {
                    vol = 0x7FFF;
                }
                if (vol < -0x7FFF) {
                    vol = -0x7FFF;
                }
                SndEvt_EnqueueType6(snd, pan, (s16)vol >> 8);
            }
            if ((s16)--part->field_76 <= 0) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                part->field_76 = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
                Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
                Gp_SpawnEff(0x60070, coord, 0xF2001400, NULL);
            }
            break;
        case 2:
            ((void (*)(Task*, s32))Gp_ReleaseStateF0)(arg1, 0x34);
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            part->field_72 = 3;
            break;
    }
}

/// Update of the actor's controller task: dispatches on its state to the
/// setup handler `func_actor_205200_8014A72C` (state 0) or the per-frame
/// handler `func_actor_205200_8014A958` (state 1), passing the task's enemy
/// record along with the task.
void func_actor_205200_8014B8C0(Task* task)
{
    GpEnemyTaskFunc fns[2] = {
        func_actor_205200_8014A72C,
        func_actor_205200_8014A958,
    };

    fns[task->state](task->spawnArg2, task);
}

s32 func_actor_205200_8014B914(s32 arg0)
{
    s32 delta;

    delta = arg0 - D_80071078;
    if (delta >= 0x7FFF) {
        delta = 0x7FFF;
    }
    if (delta < -0x7FFF) {
        delta = -0x7FFF;
    }
    return delta >> 8;
}

/// Message 0x7DB handler of the controller, listed in
/// `D_actor_205200_8014CA78`. A non-zero payload halfword raises
/// `Actor205200CtrlWork.field_2E` unless it is already set.
s32 func_actor_205200_8014B94C(Task* arg0, s32 arg1, Actor205200Msg7DB* arg2)
{
    Actor205200CtrlWork* work;

    work = arg0->work;
    if (arg2->field_2 != 0 && work->field_2E == 0) {
        work->field_2E = 1;
    }
    return 0;
}

/// State handlers of a part task - spawn, per-frame tick and teardown - that
/// `func_actor_205200_8014B978` dispatches through by state.
const GpEnemyTaskFuncTable3 D_actor_205200_80149E24 = {
    func_actor_205200_8014AE0C,
    func_actor_205200_8014B9D4,
    func_actor_205200_8014B484,
};

/// Update of a part task: runs the handler of `D_actor_205200_80149E24` that
/// `Task::state` selects, through a stack copy of the table.
void func_actor_205200_8014B978(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_205200_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Per-frame tick of a live part. `D_801153F4` gates the body: mode 1 runs
/// none of it, mode 2 raises the node flag to 1 and returns, mode 0 raises it
/// to 8 before falling in, and any other mode enters it directly. The body
/// applies the part's hits, ticks its effect timer and, once the controller's
/// 0x7DB flag is up, pushes this task to state 2 and the part to its state 2.
/// The dispatch is written as gotos because that is the shape the switch's
/// binary decision tree leaves behind - mode 0 shares the body with the
/// default path, so its `break` is a jump into it.
void func_actor_205200_8014B9D4(GpEnemy* arg0, Task* arg1)
{
    Actor205200Part*     part;
    Actor205200CtrlWork* parentWork;
    s32                  state;
    s32                  one;

    part       = (Actor205200Part*)arg1->work;
    parentWork = (Actor205200CtrlWork*)arg1->parent->work;
    state      = D_801153F4;
    one        = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg0->node.flags = 8;
    goto default_body;
case2:
    arg0->node.flags = one;
    return;
default_body:
    func_actor_205200_8014B048(arg1, one);
    if ((s16)part->field_74 != 0) {
        func_actor_205200_8014BA94(arg1);
    }
    if (parentWork->field_2E == 1) {
        arg1->state    = 2;
        part->field_72 = 2;
    }
case1:
    return;
}

/// Counts a part's effect timer down and queues effect 7 every 0x40 ticks.
void func_actor_205200_8014BA94(Task* arg0)
{
    Actor205200Part* part;
    u16              timer;

    part           = (Actor205200Part*)arg0->work;
    timer          = part->field_74 - 1;
    part->field_74 = timer;
    if (!(timer & 0x3F)) {
        func_800FDB18(7, ((TmdObject*)arg0->extra)->coords, NULL, &part->field_68);
    }
}
