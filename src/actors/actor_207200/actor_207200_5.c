#include "common.h"

#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "actors/actor_207200.h"
#include "actors/actors_shared_80134700.h"
#include "actors/actors_shared_8013851c.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_207200_80149E30;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Spawns the pair of effects that carry this actor's death animation, hands
/// the spawned task `D_actor_207200_801517F8` as its setup argument, arms the
/// two timers on the work area and unlinks its third display object.
void func_actor_207200_8014CFEC(Actor207200* arg0)
{
    GpEffArg*         effArg;
    struct GpEffWork* effect;
    Actor207200Work*  work;
    Actor207200Ctx*   ctx;

    work = arg0->field_1C;
    ctx  = arg0->field_20;

    Gp_SpawnEff(0x6009C, (*(TmdObject**)&arg0->field_2C)->coords, 0, NULL);
    func_800DA6E8(&ctx->node, ctx->field_40 - 1, 0);
    D_80062730 = (s32)&D_actor_207200_801517F8;
    effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->field_2C)->coords + 3, 0, NULL);
    if (effect != NULL) {
        ActorsShared8013851c(effect->task, (Task*)arg0);
    }
    effArg = &work->field_3F4;
    func_800FDB18(5, (*(TmdObject**)&arg0->field_2C)->coords + 3, &D_actor_207200_80153F18, effArg);
    func_800FDB18(5, (*(TmdObject**)&arg0->field_2C)->coords + 3, &D_actor_207200_80153F18, effArg);
    work->field_4A4            = 1;
    work->field_48C            = 5;
    work->field_4A6            = 1;
    ctx->field_54              = (s32)&work->field_214.field_20[0];
    ctx->field_40              = 1;
    work->field_2C4.obj.flags &= 0x7FFF;
    Gp_UnlinkObj(&work->field_2C4.obj);
    arg0->field_2A = 0x14;
}

void func_actor_207200_8014D128(Actor207200* arg0)
{
    GpEffWork* effect;
    s32        r;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    r           = (Gp_LcgState >> 16) & 3;
    switch (r) {
        case 0:
        case 1:
            D_80062730 = (s32)&D_actor_207200_801517F8;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->field_2C)->coords + 3, 0, NULL);
            if (effect != NULL) {
                ActorsShared8013851c(effect->task, (Task*)arg0);
            }
            break;
        case 2:
            D_80062730 = (s32)&D_actor_207200_80151074;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->field_2C)->coords + 5, 0, NULL);
            if (effect != NULL) {
                ActorsShared8013851c(effect->task, (Task*)arg0);
            }
            break;
        case 3:
            D_80062730 = (s32)&D_actor_207200_80150BCC;
            effect     = Gp_SpawnEff(0x80005, (*(TmdObject**)&arg0->field_2C)->coords + 2, 0, NULL);
            if (effect != NULL) {
                ActorsShared8013851c(effect->task, (Task*)arg0);
            }
            break;
    }
    Gp_SpawnEff(0x60030, (*(TmdObject**)&arg0->field_2C)->coords + 3, 0x300, NULL);
    Gp_SpawnEff(0x60030, (*(TmdObject**)&arg0->field_2C)->coords + 2, 0x300, NULL);
}

void func_actor_207200_8014D280(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_207200_8014BEF4(Task* arg0);
void func_actor_207200_8014B628(Task* arg0);
void func_actor_207200_8014B87C(Task* arg0);
void func_actor_207200_8014D41C(Task* arg0);
void func_actor_207200_8014D49C(Task* arg0);
void func_actor_207200_8014D5C4(Task* arg0);
void func_actor_207200_8014D65C(Task* arg0);
void func_actor_207200_8014D70C(void* arg0, Task* arg1);
void func_actor_207200_8014D8DC(Task* arg0);
void func_actor_207200_8014D97C(Task* arg0, GsCOORDINATE2* arg1);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);

/// Global mode byte in the main executable shared by the enemy actors: 1 skips
/// the actor's per-frame update, 2 switches its model to the hidden pose, and
/// any other value runs the update normally.
extern u8 D_801153F4;

/// Per-frame tick of the actor's live state. `D_801153F4` gates it: mode 1
/// skips the update and runs only the tail, mode 2 puts the model in its
/// hidden pose (part flag 0x80, node flag 1) and returns without updating,
/// mode 0 clears both flags before falling into the update, and any other mode
/// updates directly. The update drives the model's two attach coordinates,
/// clears the display flags of the first two parts and recomputes the second
/// part's world matrix; the tail then colours the actor from that part and
/// draws its ground shadow.
void func_actor_207200_8014D2DC(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = D_801153F4;
    one   = 1;
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
    ((TmdObject*)arg1->extra)->flags = 0;
    arg0->node.flags                 = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->node.flags                 = one;
    return;
default_body:
    func_actor_207200_8014D41C(arg1);
    func_actor_207200_8014D8DC(arg1);
    func_actor_207200_8014BEF4(arg1);
    func_actor_207200_8014D49C(arg1);
    func_actor_207200_8014D5C4(arg1);
    func_actor_207200_8014D65C(arg1);
    func_actor_207200_8014D97C(arg1, &((TmdObject*)arg1->extra)->coords[2]);
    func_actor_207200_8014D97C(arg1, &((TmdObject*)arg1->extra)->coords[3]);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    func_actor_207200_8014D70C(arg0, arg1);
    ActorsShared80134700(arg1);
}

/// Consumes the pending flag bits on the actor's spawn object once the actor
/// has been set up. Bit 0x1 (the "flag 1" request) is cleared first; bit 0x2
/// then re-arms the six helper slots - back to state 3 with slot id 1 at weight
/// 9 and every frame counter reset - and clears itself; bits 0xC (the "flag 4"
/// request) are cleared last. Nothing happens while the whole byte is zero.
void func_actor_207200_8014D41C(Task* arg0)
{
    GpObj5D*         obj;
    Actor207200Work* work;
    u8               flags;

    obj   = arg0->spawnArg2;
    flags = obj->field_4C;
    work  = arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            obj->field_4C = flags & 0xFE;
        }
        if (obj->field_4C & 2) {
            obj->field_4C   = obj->field_4C & 0xFD;
            work->field_486 = 3;
            work->field_48E = 1;
            work->field_48A = 0;
            work->field_492 = 0;
            work->field_48C = 9;
            work->field_490 = 0;
        }
        flags = obj->field_4C;
        if (flags & 0xC) {
            obj->field_4C = flags & 0xF3;
        }
    }
}

/// Per-frame tick of the actor's six helper slots, driven by
/// `work->field_486`. The kill countdown on the task is decremented first and
/// clamped at zero. State 0 and state 1 hand the actor to the two helper
/// setup/tick bodies; state 3 runs the slot animation, counting
/// `work->field_48A` up to 0x3D frames before re-arming the slots with id 1 at
/// weight 9, and drops back to state 0 once `Gp_TickObjFlag2` reports that the
/// spawn argument is done; state 4 waits out `work->field_490` frames and then
/// either returns to state 1 when the actor is idle (`work->field_4A6 != 0`)
/// or clears both state words and marks `work->field_4A2`.
void func_actor_207200_8014D49C(Task* arg0)
{
    Actor207200Work* work;
    s16              countdown;

    work                = arg0->work;
    countdown           = (u16)arg0->killCountdown - 1;
    arg0->killCountdown = countdown;
    if (countdown < 0) {
        arg0->killCountdown = 0;
    }
    switch (work->field_486) {
        case 0:
            func_actor_207200_8014B628(arg0);
            break;
        case 1:
            func_actor_207200_8014B87C(arg0);
            break;
        case 3:
            work->field_48A = work->field_48A + 1;
            if ((s16)work->field_48A >= 0x3D) {
                work->field_48E = 1;
                work->field_48C = 9;
                work->field_490 = 0;
                work->field_48A = 0;
            }
            if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
                work->field_486 = 0;
            }
            break;
        case 4:
            if ((s16)work->field_490 >= 0x69) {
                if (work->field_4A6 != 0) {
                    work->field_486 = 0;
                    work->field_48C = 1;
                    break;
                }
                work->field_486 = 0;
                work->field_4A2 = 1;
            }
            break;
    }
}

/// Walks the model's root part forward. While the actor is not idle
/// (`work->field_4A6 == 0`) the part's current translation is remembered in the
/// work area, and the part is then displaced along its own forward axis - the
/// third basis column of its local matrix, scaled by `work->field_492` - and
/// lifted by 0x80.
void func_actor_207200_8014D5C4(Task* arg0)
{
    Actor207200Work* work;
    GsCOORDINATE2*   coord;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_4A6 == 0) {
        work->field_454 = coord->coord.t[0];
        work->field_458 = coord->coord.t[1];
        work->field_45C = coord->coord.t[2];
    }
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_492) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_492) >> 12;
}

/// Rebinds the work's animation id to its six helper slots. When the id has
/// changed since the last frame the remembered id follows it, the frame counter
/// restarts and every slot is pointed at the new id at weight 8; otherwise the
/// counter ticks and the slots are simply advanced by one.
void func_actor_207200_8014D65C(Task* arg0)
{
    Actor207200Work* work;
    s32              i;

    work = arg0->work;
    i    = 1;
    if (work->field_48C != (s16)work->field_48E) {
        work->field_48E = work->field_48C;
        work->field_490 = 0;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_48C, 0, 8);
            i++;
        } while (i < 7);
        return;
    }
    TOUCH_REG(i);
    work->field_490 = (u16)(work->field_490 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 7);
}

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_207200_8014D70C(void* arg0, Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}
