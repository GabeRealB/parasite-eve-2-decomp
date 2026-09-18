#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_8013454c.h"
#include "actors/actors_shared_8014ca28.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

void func_actor_107000_80132474(Task* arg0);
void func_actor_107000_801334C8(Task* arg0, s32 arg1);
void func_actor_107000_80134810(Task* arg0, GsCOORDINATE2* arg1);
void func_actor_107000_80132E9C(Task* arg0);

/// Node 3's pair record, packed by `Gp_PackPair` into `obj1B4`; the wider view
/// of the same object is `D_actor_107000_80138748`, whose `field_4` seeds the
/// enemy's `field_40`.
extern GpU16Pair  D_actor_107000_80138744;
extern GpPairSrcE D_actor_107000_80138748;

/// Message dispatch table the spawn parks in `Task::field_24`.
extern u8 D_actor_107000_80139E50[];

/// The animation data `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_107000_80139E78[];

/// Offset the collapse arms spawn the 0x60080 effect at.
extern SVECTOR D_actor_107000_80139E90;

// actor_104600 (func_actor_104600_80131E68), actor_204600
// (func_actor_204600_80149E68) and actor_207000 (func_actor_207000_80149F0C)
// carry the same body, refused promotion for the reason its sibling below is:
// the pair table, the animation bank and the node-3 record it names -
// D_actor_107000_80138744, D_actor_107000_80138748 and
// D_actor_107000_80139E78 - are this overlay's own data, so one shared object
// could not link into the other three.

/// Spawn handler of the specimen, the `GpEnemyTaskFunc` the task dispatch runs
/// first: it allocates the `Actor107000SpawnWork` block, wires the enemy's four
/// `GpObj` render nodes and their `GpRec18` tables into it and hands the task
/// over to `ActorsShared8014ca28`. The spawn arg's high halfword is the variant
/// the model was spawned as - when it is 1 the specimen is killed instead, and
/// the same halfword plus the low one seed `field_2DC`/`field_2D6`. Variant 1
/// with a matching `spawnType` is the one that carries a streamed model: its
/// texture page and CLUT row are stepped before the model is re-streamed twice.
void func_actor_107000_80131F0C(GpEnemy* arg0, Task* arg1)
{
    Actor107000SpawnWork* work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        part;
    u16                   v;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->field_8;
    part  = &coord[1];
    if ((s16)(arg1->spawnArg1 >> 16) == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = Mem_Calloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap    = (TaskIdMap*)work;
    obj->field_C   = 0;
    coord->flg     = 0;
    obj->field_1C  = &work->field_DC;
    obj->field_20  = &work->field_BC;
    arg0->field_4  = &coord[1].coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18     = part;
    arg0->node.field_4 = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_50     = &D_actor_107000_80138748;
    arg0->field_54     = (s32)&work->rec154[0];
    arg0->field_40     = D_actor_107000_80138748.field_4;
    func_800B3F84((GpAnimCtx*)work, D_actor_107000_80139E78, (GpAnimObj*)obj, work->field_8C,
                  (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8      = 1;
    work->field_2BA      = 1;
    work->field_2AC      = 0x1000;
    work->field_2DA      = 0;
    work->field_2CE      = 0;
    work->field_2D4      = 0;
    work->field_2D2      = 0;
    work->field_2CC      = 0;
    arg1->killCountdown  = 0;
    work->field_284      = &((TmdObject*)arg1->extra)->field_8[1];
    work->field_288      = 0x100;
    work->field_28A      = 1;
    work->objFC.field_8  = coord;
    work->objFC.field_C  = &work->rec11C;
    work->objFC.field_10 = 0;
    work->objFC.field_12 = 0;
    work->objFC.field_14 = 0;
    work->objFC.field_18 = 0;
    work->objFC.field_1C = 0xBB8;
    work->objFC.flags    = 1U;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.field_8  = coord;
    work->obj134.field_C  = &work->rec154[0];
    work->obj134.field_10 = 0;
    work->obj134.field_12 = -0xC8;
    work->obj134.field_14 = 0;
    work->obj134.field_18 = 0x3002E;
    work->obj134.field_1C = 0xC8;
    work->obj134.flags    = 1U;
    work->objFC.flags     = (u16)(work->objFC.flags | 0x8000);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.field_8  = coord;
    work->obj1B4.field_C  = &work->rec1D4;
    work->obj1B4.field_10 = 0;
    work->obj1B4.field_12 = 0;
    work->obj1B4.field_14 = 0;
    work->obj134.flags    = (u16)(work->obj134.flags | 0xC200);
    work->obj1B4.field_18 = Gp_PackPair(&D_actor_107000_80138744, 0);
    work->obj1B4.field_1C = 0x3E8;
    work->obj1B4.flags    = 1U;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.field_8  = coord;
    work->obj1EC.field_C  = &work->rec20C;
    work->obj1EC.field_10 = 0;
    work->obj1EC.field_12 = 0;
    work->obj1EC.field_14 = 0;
    work->obj1EC.field_18 = 0x22323;
    work->obj1EC.field_1C = 0x3E8;
    work->obj1EC.flags    = 1U;
    work->obj1B4.flags    = (u16)(work->obj1B4.flags & 0x7FFF);
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    work->field_2DC    = (s16)(arg1->spawnArg1 >> 16);
    v                  = (u16)arg1->spawnArg1;
    work->field_2D6    = v;
    if ((s16)v == 1 && arg1->spawnType == (s16)v) {
        obj->field_24 = obj->field_24 + 1;
        obj->field_25 = obj->field_25 + 1;
        if (obj->field_18 != 0) {
            Tmd_ProcessStream(obj);
            Tmd_ProcessStream(obj);
        }
    }
    work->field_2B4    = 0;
    arg1->exitCallback = ActorsShared8014ca28;
    arg1->state       += 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", ActorsShared80135df4Table);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", D_actor_107000_80131E30);

// actor_104600 (func_actor_104600_801321F4), actor_204600
// (func_actor_204600_8014A1F4) and actor_207000 (func_actor_207000_8014A298)
// carry the same body, refused promotion for the reason its sibling below is:
// its three remaining calls - func_actor_107000_80132474,
// func_actor_107000_80132674 and func_actor_107000_80134810 - and the effect
// offset it spawns at are named in this overlay only, so one shared object
// could not link into the other three.

/// Per-frame dispatch of the caged specimen, on the reaction state in
/// `field_2B2`: 0 is the dormant arm `func_actor_107000_80132474` and 1 the
/// live handler `func_actor_107000_80132674`. 3 is the arm the reaction
/// dispatch shoots when the enemy's flag byte carries bit 0x2 - it suppresses
/// the rebind, waits out the generic flag-2 helper on the spawn arg and, once
/// that expires, wakes the specimen: the rebind is released and
/// `field_2B2`/`field_2C8` move to 1, the live stage. The arm ends in
/// `ActorsShared8013454c` either way.
///
/// 4 and 5 are the two collapse arms. Both drive the model's second coordinate
/// through `func_actor_107000_80134810`, count `field_2BC` up and spawn the
/// 0x60080 effect on the model's coordinate every 0x10 frames; 5 also counts
/// `field_2D4` and, on the third count, writes the same death sequence the
/// reaction dispatch does - a five-frame countdown, `field_2B4` cleared and the
/// task moved to state 2 - with the spawn arg's `field_40` cleared alongside.
/// Both arms end by re-suppressing the rebind, and the join the compiler builds
/// from their two assignments is what the original binary shows.
void func_actor_107000_80132298(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    u16              frames;

    work = (Actor107000Work*)arg0->idMap;
    switch (work->field_2B2) {
        case 0:
            func_actor_107000_80132474(arg0);
            return;
        case 1:
            func_actor_107000_80132674(arg0);
            return;
        case 3:
            work->field_2D2 = 1;
            if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
                work->field_2D2 = 0;
                work->field_2B2 = 1;
                work->field_2C8 = 1;
                work->field_2BE = 0;
            }
            ActorsShared8013454c(arg0);
            return;
        case 4:
            work->field_2AC = 0x1000;
            func_actor_107000_80134810(arg0, &((TmdObject*)arg0->extra)->field_8[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->field_8, 0x400, &D_actor_107000_80139E90);
                work->field_2BC = 0;
            }
            goto suppress_rebind;
        default:
            return;
        case 5:
            work->field_2AC = 0x1000;
            func_actor_107000_80134810(arg0, &((TmdObject*)arg0->extra)->field_8[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->field_8, 0x400, &D_actor_107000_80139E90);
                work->field_2BC = 0;
                frames          = work->field_2D4 + 1;
                work->field_2D4 = frames;
                if ((s16)frames >= 3) {
                    enemy               = arg0->spawnArg2;
                    arg0->killCountdown = 5;
                    work->field_2B4     = 0;
                    arg0->state         = 2;
                    enemy->field_40     = 0;
                }
            }
        suppress_rebind:
            work->field_2D2 = 1;
    }
}

// actor_104600 (func_actor_104600_801323D0), actor_204600
// (func_actor_204600_8014A3D0) and actor_207000 (func_actor_207000_8014A474)
// carry the same body. Unlike the siblings around it, nothing this one names is
// overlay-local - every call it makes is already shared, so one shared object
// *would* link into all four carriers. It is the span that cannot be placed:
// 0x654..0x854 sits inside this overlay's first code unit, and this overlay and
// actor_207000 already carry a hand-placed `rodata` cut, which is the layout
// `bulk_m2c_promote.py` refuses to re-derive (`already has hand-placed
// rodata/units cuts; splitting a unit there needs them re-derived by hand`).
// So the body stays matched in each carrier until that pass is done with a
// person re-deriving the cut.

/// Dormant arm of the caged specimen: the `field_2B2 == 0` arm of the per-frame
/// dispatch, run while the specimen is still caged. The collision record at
/// `field_11C` is polled for a 0x10000-kind occupant and, when one appears,
/// `field_2D8` latches - that latch is what wakes the specimen. It moves the
/// reaction stage and the animation to the live handler's, clears the render
/// node's 0x8000 flag, and arms the global state, which is the same three
/// writes the spawn handler's own node setup makes. The record is released
/// either way.
///
/// While the work plays animation 1 the arm also runs the sound cue the live
/// handler runs: `field_2D0` is counted down and, when it expires, re-rolled
/// from `Gp_LcgState` as `(state >> 16) % 100 + 0x50` frames - between 0x50 and
/// 0xB3 - with `field_2D6` picking between the two half-ids and the model's
/// pan and depth passed alongside. The whole assignment *and* its call are
/// written out in both arms, the way `func_actor_107000_80132674` writes them.
/// `field_2BE` is then re-armed from the frames spent on the current id (see
/// the two 0x29-frame windows below) and the id is restarted once it has spent
/// 0x63 frames.
void func_actor_107000_80132474(Task* arg0)
{
    Actor107000Work* work;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s32              soundId;
    u32              rng;

    coord              = ((TmdObject*)arg0->extra)->field_8;
    work               = (Actor107000Work*)arg0->idMap;
    *(u32*)0x1F8003FC -= 8;
    if (Gp_CountRec18Hi(&work->field_11C, 0x10000) != 0) {
        work->field_2D8 = 1;
    }
    if (work->field_2D8 != 0) {
        work->field_2B2 = 1;
        work->field_2C8 = 1;
        work->field_11A = (u16)(work->field_11A & 0x7FFF);
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(&work->field_11C);
    if (work->field_2B8 == 1) {
        countdown       = work->field_2D0 - 1;
        work->field_2D0 = countdown;
        if ((countdown << 16) <= 0) {
            rng             = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState     = rng;
            work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
            if (work->field_2D6 != 0) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 12) << 8) | 0x40460009;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            } else {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 12) << 8) | 0x402E0001;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
        }
        work->field_2C6 = 1;
        work->field_2BE = 0;
        if ((u32)(work->field_2BC - 1) < 0x29) {
            work->field_2BE = 0x14;
        }
        if ((u32)(work->field_2BC - 0x33) < 0x29) {
            work->field_2BE = -0x14;
        }
        if ((s16)work->field_2BC >= 0x63) {
            work->field_2BC = 0;
        }
        ActorsShared8013454c(arg0);
    }
    *(u32*)0x1F8003FC += 8;
}

// actor_104600 (func_actor_104600_801325D0), actor_204600
// (func_actor_204600_8014A5D0) and actor_207000 (func_actor_207000_8014A674)
// carry the same body, refused promotion because both of its remaining calls -
// func_actor_107000_80132E9C and func_actor_107000_801334C8 - are named in this
// overlay only, so one shared object could not link into the other three.

/// Per-frame handler of the caged specimen, dispatched on the reaction stage in
/// `field_2C8`: 1 is the live specimen, 2 the death throes. Every stage ends in
/// the shared epilogue, so the switch's default is a jump straight there.
///
/// Stage 1 ticks `field_2D0` down and, when it runs out, re-rolls it from
/// `Gp_LcgState` as `(state >> 16) % 100 + 0x50` frames - between 0x50 and 0xB3.
/// The re-roll also cues a sound event: `field_2D6` picks between the two
/// half-ids, the actor id in bits 12+ of the context's `field_8` supplies the
/// sound bank, and the pan and depth of the model's coordinate are passed
/// alongside. As in `func_actor_107000_80132D8C`, the whole assignment *and* its
/// call are written out in both arms - the join the compiler builds from them is
/// what the original binary shows. The stage then re-arms `field_2BE`, switches
/// the animation to 2, runs the frame through `func_actor_107000_80132E9C` and
/// `ActorsShared8013454c`, and restarts `field_2BC` once it has spent 0x1D
/// frames on the id.
///
/// Stage 2 counts `field_2D4` up and advances `field_2AC` by 0xC8 a frame; on
/// the fifth frame the specimen is killed - `func_actor_107000_801334C8` runs
/// with a zero argument, the kill countdown is armed to 5, the reaction flag
/// `field_2B4` is cleared, the task state latches the stage it just ran, and the
/// enemy's `field_40` HP is zeroed.
void func_actor_107000_80132674(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s16              mode;
    s32              soundId;
    u32              rng;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor107000Work*)arg0->idMap;
    enemy = arg0->spawnArg2;
    mode  = work->field_2C8;
    switch (mode) {
        case 1:
            countdown       = work->field_2D0 - 1;
            work->field_2D0 = countdown;
            if ((countdown << 16) <= 0) {
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
                if (work->field_2D6 != 0) {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x40460009;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                } else {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402E0001;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
            }
            work->field_2BE = 0x14;
            work->field_2B8 = 2;
            func_actor_107000_80132E9C(arg0);
            ActorsShared8013454c(arg0);
            if ((s16)work->field_2BC >= 0x1D) {
                work->field_2BC = 0;
            }
            break;
        case 2:
            work->field_2D4 = work->field_2D4 + 1;
            work->field_2AC = work->field_2AC + 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                func_actor_107000_801334C8(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = mode;
                enemy->field_40     = 0;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_8013283C);

/// Damage reaction of the caged specimen. `arg1` is taken off the context's
/// HP, the same amount is pushed through the lock-slot updater, and a depleted
/// specimen switches the task to its death state (2) with a five-frame
/// countdown while `field_2B4` is cleared on the work.
///
/// A live one cues a sound event instead: `field_2D6` picks between the two
/// half-ids, the actor id in bits 12+ of the context's `field_8` supplies the
/// sound bank, and the pan and depth of the model's coordinate are passed
/// alongside. The same call statement is written out in both arms - the join
/// the compiler builds from it is what the original binary shows.
///
/// `field_2CC` is then re-armed and, for the id the animation is playing
/// (`field_2B8 == 1`), latched into `field_2D8`.
void func_actor_107000_80132D8C(Task* arg0, s32 arg1)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              anim;
    s32              soundId;

    enemy            = arg0->spawnArg2;
    obj              = arg0->extra;
    coord            = obj->field_8;
    work             = (Actor107000Work*)arg0->idMap;
    enemy->field_40 -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->field_40 < 0) {
        func_actor_107000_801334C8(arg0, 0);
        arg0->state         = 2;
        arg0->killCountdown = 5;
        work->field_2B4     = 0;
        return;
    }
    if (work->field_2D6 != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x4046000A;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    } else {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402E0002;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
    anim            = work->field_2B8;
    work->field_2CC = 0xF;
    if (anim == 1) {
        work->field_2D8 = anim;
    }
}

// actor_104600 (func_actor_104600_80132DF8), actor_204600
// (func_actor_204600_8014ADF8) and actor_207000 (func_actor_207000_8014AE9C)
// carry the same body. It is promotable - its only externals are `Player_Status`
// and the libgte pair, none of them overlay-local - but the shared span
// renumbers every unit after it in this overlay and in the three carriers, and
// splat never rewrites a `.c` that exists, so it needs the pass that rehomes
// the carriers' `INCLUDE_*` lines at the same time.

/// Turns the caged specimen toward the player, one 0x20 step a frame. The
/// 0x18-byte scratch the model's heading is rebuilt in carries the XZ offset
/// from the model's coordinate to the player (`Player_Status.coordMtx`), and the
/// heading `field_2B0` is re-steered against it: at most 0x21 off, the target
/// heading is taken outright; otherwise the field is stepped one 0x20 toward it,
/// with a difference of 0x801 or more first folded through the 0x1000 wrap so
/// the specimen turns the short way round. The result is written back as the
/// scratch's Y rotation and `RotMatrix` rebuilds the coordinate from the
/// scratch's angle triple.
void func_actor_107000_80132E9C(Task* arg0)
{
    Actor107000Work*       work;
    GsCOORDINATE2*         coord;
    Actor107000RotScratch* sc;
    s16                    cur;
    s32                    want;
    s16                    diff;
    s32                    adiff;
    s16                    turn;
    s16                    wrap;
    s32                    current;

    coord      = ((TmdObject*)arg0->extra)->field_8;
    work       = (Actor107000Work*)arg0->idMap;
    sc         = (Actor107000RotScratch*)(*(u32*)0x1F8003FC -= 0x18);
    sc->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    sc->vec.vy = 0;
    sc->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
    cur        = work->field_2B0 & 0xFFF;
    diff       = want - cur;
    adiff      = diff >= 0 ? diff : -diff;
    turn       = diff;
    if (adiff < 0x21) {
        work->field_2B0 = want;
    } else {
        if (adiff >= 0x801) {
            wrap = diff - 0x1000;
            if (diff <= 0) {
                wrap = 0x1000 - diff;
            }
            turn = wrap;
        }
        current = work->field_2B0;
        if (turn <= 0) {
            cur = current - 0x20;
        } else {
            cur = current + 0x20;
        }
        work->field_2B0 = cur;
    }
    sc->rot.vx = 0;
    sc->rot.vy = work->field_2B0;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    *(u32*)0x1F8003FC += 0x18;
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_80132FD4);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_801334C8);

// actor_104600 (func_actor_104600_801335EC), actor_204600
// (func_actor_204600_8014B5EC) and actor_207000 (func_actor_207000_8014B690)
// carry the same body, refused promotion because it reads its own overlay's
// data - the two pair records, the animation source and the message table are
// named per overlay, so one shared object could not resolve them in the other
// three.

/// Spawn/setup handler of the caged specimen, entry 0 of
/// `D_actor_107000_80131E30`. A task already on this handler (`spawnArg1`'s
/// high halfword reads 1) is torn down instead of spawned.
///
/// Otherwise it allocates the 0x2E4-byte work block and hangs it off the task:
/// the model's coordinate array feeds `field_18` with its second element, the
/// context's `field_4` with that element's matrix, and the block's two
/// `MATRIX`es become the model's colour and light matrices. The work's own
/// collision record is re-rolled from the same coordinate, and the four list
/// nodes are linked into the global object lists with their `GpRec18` tables -
/// the first three leave the 0x8000 last-element bit clear, and the second
/// node's 0x4000 bit is cleared once the third has been built.
///
/// The animation context is then seeded from `func_800B3F84` over the three
/// slots, slots 1 and 2 are reset, and the task moves to handler 3.
void func_actor_107000_80133690(GpEnemy* arg0, Task* arg1)
{
    Actor107000SpawnWork* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        part;
    TmdObject*            obj;
    s32                   one;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->field_8;
    part  = &coord[1];
    one   = 1;
    if ((s16)(arg1->spawnArg1 >> 16) == one) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = Mem_Calloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap     = (TaskIdMap*)work;
    work->field_2DC = (s16)(arg1->spawnArg1 >> 16);
    work->field_2D6 = (u16)arg1->spawnArg1;
    obj->field_C    = 0x80;
    coord->flg      = 0;
    obj->field_1C   = &work->field_DC;
    obj->field_20   = &work->field_BC;
    arg0->field_4   = &coord[1].coord;
    arg0->field_48  = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18     = part;
    arg0->node.field_4 = one;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_50     = &D_actor_107000_80138748;
    arg0->field_54     = (s32)&work->rec154[0];
    arg0->field_40     = D_actor_107000_80138748.field_4;
    func_800B3F84((GpAnimCtx*)work, D_actor_107000_80139E78, (GpAnimObj*)obj, work->field_8C,
                  (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8      = 1;
    work->field_2BA      = 1;
    work->field_2AC      = 0x1000;
    work->field_2DA      = 0;
    work->field_2CE      = 0;
    work->field_2D4      = 0;
    work->field_2D2      = 0;
    work->field_2CC      = 0;
    arg1->killCountdown  = 0;
    work->field_284      = &((TmdObject*)arg1->extra)->field_8[1];
    work->field_288      = 0x100;
    work->field_28A      = 1;
    work->objFC.field_8  = coord;
    work->objFC.field_C  = &work->rec11C;
    work->objFC.field_10 = 0;
    work->objFC.field_12 = 0;
    work->objFC.field_14 = 0;
    work->objFC.field_18 = 0;
    work->objFC.field_1C = 0xBB8;
    work->objFC.flags    = 1U;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.field_8  = coord;
    work->obj134.field_C  = &work->rec154[0];
    work->obj134.field_10 = 0;
    work->obj134.field_12 = -0xC8;
    work->obj134.field_14 = 0;
    work->obj134.field_18 = 0x3002E;
    work->obj134.field_1C = 0xC8;
    work->obj134.flags    = 1U;
    work->objFC.flags     = (u16)(work->objFC.flags & 0x7FFF);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.field_8  = coord;
    work->obj1B4.field_C  = &work->rec1D4;
    work->obj1B4.field_10 = 0;
    work->obj1B4.field_12 = 0;
    work->obj1B4.field_14 = 0;
    work->obj134.flags    = (u16)(work->obj134.flags & 0x3DFF);
    work->obj1B4.field_18 = Gp_PackPair(&D_actor_107000_80138744, 0);
    work->obj1B4.field_1C = 0x3E8;
    work->obj1B4.flags    = 1U;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.field_8  = coord;
    work->obj1EC.field_C  = &work->rec20C;
    work->obj1EC.field_10 = 0;
    work->obj1EC.field_12 = 0;
    work->obj1EC.field_14 = 0;
    work->obj1EC.field_18 = 0x22323;
    work->obj1EC.field_1C = 0x3E8;
    work->obj1EC.flags    = 1U;
    work->obj1B4.flags    = (u16)(work->obj1B4.flags & 0x7FFF);
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->field_2E2    = 0;
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    arg1->field_24     = D_actor_107000_80139E50;
    arg1->state        = 3;
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000", func_actor_107000_801339C0);

INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", D_actor_107000_80131E5C);
INCLUDE_RODATA("actors/nonmatchings/actor_107000/actor_107000", ActorsShared801385d4Table);
