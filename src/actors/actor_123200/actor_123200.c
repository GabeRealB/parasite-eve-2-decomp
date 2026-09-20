#include "common.h"

#include "actors/actor_123200.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_actor_123200_801332E0(Task* task);

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

extern u8 D_80072729;

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_8013215C);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801324A4);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801329F0);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80132B94);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801332E0);

s32 func_actor_123200_80133450(Actor123200Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_220 == v) {
                goto same;
            }
            arg0->field_220 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_220 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_220 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

/// Normalises `dir` in place and scales it to 0x3E8/0x1000 of unit length on
/// the GTE. The pointer stays in one register across `VectorNormalSS` because
/// the GTE loads read it back afterwards.
static __inline__ void Actor123200_ScaleForward(SVECTOR* dir)
{
    VectorNormalSS(dir, dir);
    gte_lddp(0x3E8);
    gte_ldsv(dir);
    gte_gpf12_real();
    gte_stsv(dir);
}

/// Spawn state of this enemy: allocates the work block, publishes it as
/// `Task::work`, reparents the model to `gGfxViewCoord`, seeds its animation
/// slots from `D_actor_123200_80137154` and hangs the enemy's display node off
/// part 2 of the model's coordinate array. The context's top `field_8` nibble
/// biases the three timers in `field_176`, `field_198` and `field_19A` -- up by
/// the nibble when its low bit is set, down by half of it otherwise.
void func_actor_123200_8013352C(GpEnemy* enemy, Task* task)
{
    SVECTOR          dir;
    Actor123200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    u32              scale;
    u32              flag;

    obj        = (TmdObject*)task->extra;
    coord      = obj->coords;
    work       = memCalloc(sizeof(Actor123200Work), false);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->msgTable = D_actor_123200_80137214;
    coord->sub     = &gGfxViewCoord;
    obj->flags     = 0;
    func_800B3F84(&work->anim, D_actor_123200_80137154, obj, work->poses, work->slots);

    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)task->extra)->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags    = 1;
    enemy->param         = &D_actor_123200_80134208;
    enemy->reactionFlags = 0;
    enemy->hpMax         = 0;
    enemy->hp            = 0;
    enemy->recs          = 0;

    work->field_174 = 1;
    work->field_170 = 2;
    work->field_176 = 0x10;
    work->field_178 = 0;
    func_actor_123200_801332E0(task);
    work->field_17E = 0;
    work->field_8   = 0;
    obj->lightMtx   = &work->field_1BC;
    obj->colorMtx   = &work->field_1DC;
    coord->flg      = 0;
    work->field_198 = 5;
    work->field_19A = 0x14;

    scale = (u16)(enemy->placeKey >> 12);
    flag  = scale & 1;
    if (flag == 1) {
        work->field_176 += enemy->placeKey >> 12;
        work->field_19A += enemy->placeKey >> 12;
        work->field_198 += enemy->placeKey >> 12;
    } else {
        work->field_176 -= scale >> 1;
        work->field_19A -= enemy->placeKey >> 13;
        work->field_198 -= enemy->placeKey >> 13;
    }

    work->field_1A8 = ((Actor123200CoordPos*)((TmdObject*)task->extra)->coords)->x;
    work->field_1AA = ((Actor123200CoordPos*)((TmdObject*)task->extra)->coords)->y;
    work->field_1AC = ((Actor123200CoordPos*)((TmdObject*)task->extra)->coords)->z;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &dir);
    dir.vy = 0;
    Actor123200_ScaleForward(&dir);

    work->field_0                      = 0;
    work->field_2                      = -1;
    D_actor_123200_80137248.coord      = ((TmdObject*)task->extra)->coords;
    D_actor_123200_80137248.spawnArgLo = 0x100;
    D_actor_123200_80137248.spawnArgHi = 1;
    task->state++;
}

/// Steps `coord` 5/0x1000 of the way along its own forward axis (column 2 of
/// its rotation, normalised and GPF-scaled) and flags it for rebuild. The
/// direction vector lives in an `SVECTOR` carved off the scratch head and
/// handed straight back.
static __inline__ void Actor123200_StepForward(GsCOORDINATE2* coord)
{
    u8*      head;
    SVECTOR* dir;

    head       = (u8*)SCRATCH_SP;
    dir        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_SP = (u32)dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(5);
    gte_ldsv(dir);
    gte_gpf12_real();
    gte_stsv(dir);

    coord->coord.t[0] += dir->vx;
    coord->coord.t[1] += dir->vy;
    coord->coord.t[2] += dir->vz;
    coord->flg         = 0;

    SCRATCH_SP = (u32)((u8*)SCRATCH_SP + sizeof(SVECTOR));
}

/// Per-frame handler of a `actor_123200` instance. A pending restart on the
/// work block's `field_4` re-arms the model -- clearing `TmdObject.flags`
/// and its buffers, rewriting the 0x1B0/0x1B2/0x1B4 pair, the motion state
/// `field_170`/`field_174` and the frame counter `field_6`, and raising
/// `arg0->field_14` -- and returns. Otherwise the frame counter runs, 0xC bytes
/// are reserved off the scratch head, and unless the game is frozen the model
/// is stepped forward along its facing; the reservation is released around the
/// animation update and the model's coordinate is flagged for rebuild.
void func_actor_123200_80133820(Actor123200Ctx* arg0, Task* task)
{
    Actor123200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    work = (Actor123200Work*)task->work;
    if (work->field_4 != 0) {
        obj            = (TmdObject*)task->extra;
        arg0->field_14 = 1;
        obj->flags     = 0;
        Tmd_AllocBuffers(obj);
        work->field_1B0 = 0x115D;
        work->field_1B2 = 1;
        work->field_1B4 = 0x12D5;
        work->field_174 = 2;
        work->field_170 = 2;
        func_actor_123200_801332E0(task);
        ((TmdObject*)task->extra)->coords->flg = 0;
        work->field_6                          = 0;
        return;
    }
    work->field_6++;
    SCRATCH_SP -= 0xC;
    coord       = ((TmdObject*)task->extra)->coords;
    if (D_80072729 != 1) {
        Actor123200_StepForward(coord);
    }
    func_actor_123200_801332E0(task);
    SCRATCH_SP                            += 0xC;
    ((TmdObject*)task->extra)->coords->flg = 0;
}

static __inline__ void Actor123200_MoveForward(GsCOORDINATE2* coord)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        SOFT_TOUCH_REG(vec);
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(5);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_123200_801339F0(Actor123200Ctx* arg0, Task* task)
{
    Actor123200Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    work = (Actor123200Work*)task->work;
    if (work->field_4 != 0) {
        obj            = (TmdObject*)task->extra;
        arg0->field_14 = 1;
        obj->flags     = 0;
        Tmd_AllocBuffers(obj);
        work->field_1B0 = 0x115D;
        work->field_1B2 = 1;
        work->field_1B4 = 0x12D5;
        work->field_174 = 2;
        work->field_170 = 2;
        func_actor_123200_801332E0(task);
        ((TmdObject*)task->extra)->coords->flg = 0;
        work->field_6                          = 0;
        return;
    }
    work->field_6++;
    coord = ((TmdObject*)task->extra)->coords;
    Actor123200_MoveForward(coord);
    func_actor_123200_801332E0(task);
    ((TmdObject*)task->extra)->coords->flg = 0;
}

/// Per-frame tick: flags the model's coordinate for rebuild, refreshes its
/// colour from the part matrix's translation, then scales that matrix from the
/// work block's `field_21C`. The render mode in `D_801153F4` runs next -- modes
/// 0 and 1 draw the ground quad while the display mode is non-zero, and 1 and 2
/// return without ticking. The rest re-records the display mode in `field_2`
/// (`field_4` restarting the model when it changed), dispatches the display
/// mode's handler from `D_actor_123200_80131E24`, and plays the sound that
/// handler reports, panned and depth-tagged from the model's coordinate. A
/// raised `gGameSession->viewReady` flags the coordinate for rebuild again.
void func_actor_123200_80133BA0(Actor123200Ctx* arg0, Task* arg1)
{
    VECTOR                pos;
    Actor123200StateTable table;
    Actor123200Work*      work;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                                   = (Actor123200Work*)arg1->work;
    table                                  = D_actor_123200_80131E24;
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    pos.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
    Gp_UpdateActorColor((struct GpEnemy*)arg0, &pos, 0, 0);
    if (work->field_21C != 0x1000) {
        pos.vx = pos.vy = pos.vz = work->field_21C;
        ScaleMatrix(&work->field_1BC, &pos);
    }
    switch (D_801153F4) {
        case 0:
            if (work->field_0 != 0) {
                ((TmdObject*)arg1->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            break;
        case 1:
            if (work->field_0 != 0) {
                ((TmdObject*)arg1->extra)->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)((TmdObject*)arg1->extra)->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            return;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.fn[work->field_0](arg0, arg1);
    id = func_actor_123200_80133450(work);
    if (id != 0) {
        snd = id | ((arg0->field_8 >> 12) << 8);
        pan = (s8)Gp_GetObjPan(((TmdObject*)arg1->extra)->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(((TmdObject*)arg1->extra)->coords));
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)arg1->extra)->coords->flg = 0;
    }
}

s32 func_actor_123200_80133E30(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor123200Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor123200Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 1;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
        case 4:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

s32 func_actor_123200_80133EDC(Task* task, s32 arg1, Actor123200Msg* msg)
{
    Actor123200Work* work;
    Actor123200Ctx*  ctx;

    work            = (Actor123200Work*)task->work;
    ctx             = (Actor123200Ctx*)task->spawnArg2;
    work->field_194 = msg->bytes.b0;
    work->field_195 = msg->bytes.b1;
    work->field_196 = msg->bytes.b2;
    if (msg->words.type == 0xB02) {
        switch ((s32)msg->words.cmd) {
            case 1:
                if ((ctx->field_8 >> 12) == 1) {
                    work->field_21C = 0x1000;
                } else {
                    work->field_21C = 0x400;
                }
                work->field_0 = 2;
                break;
            case 2:
                work->field_21C = 0x1000;
                work->field_0   = 1;
                break;
            case 3:
                work->field_0 = 0;
                break;
            case 0:
                break;
        }
    }
    return 0;
}
