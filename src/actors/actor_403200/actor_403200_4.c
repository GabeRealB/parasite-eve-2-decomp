#include "common.h"

#include "actors/actor_403200.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s16 D_actor_403200_80141C58;

/// The script pair the death sequence's frame-0x1C cue spawns.
extern s32 D_actor_403200_80141C5C;
extern s32 D_actor_403200_80141C64;

/// Enemy spawn table the three launch states of `func_actor_403200_8013D9EC`
/// draw from.
extern TaskDesc D_actor_403200_8015E858;

/// This overlay's three task states -- spawn/setup, per-frame tick and
/// teardown -- dispatched through by state, the same shape as the sibling
/// enemy actors' tables.
extern GpEnemyTaskFuncTable3 D_actor_403200_801321B8;

/// Handwritten overlay-local follow helper. `arg1`/`arg2` select the axis pair
/// and `arg3` the mode; takes the task, not the work block.
void func_actor_403200_801408D8(Task* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Overlay-local hit-effect spawner (`actor_403200_3.c`): picks a rotation from
/// the attack id's param 0, hands `func_800FDB18` the part's coordinate with the
/// effect id from param 1, and returns. Only the coordinate and the id are read;
/// the caller's `a2` / `a3` are left over from the scan.
void func_actor_403200_80134044(GsCOORDINATE2* coord, s32 id);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80137CCC);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80137EB4);

/// Screen-shake driver for the enemy task: `func_actor_403200_8013FB54` writes a
/// level into `field_EAC`, and a change from the armed level in `field_EAD`
/// starts a shake of 5, 10 or 22 frames -- any other level is ignored. Each tick
/// spends one frame and drives `Display_ClampField126` off the frame counter's
/// low bits, so level 1 alternates 0 / 2, level 2 walks a four-frame 0 / 2 / 3 / 2
/// pattern and level 3 an eight-frame ramp that peaks at 4. The shake clears
/// itself once the counter runs out. Same body as
/// `func_actor_444000_8013A77C`, plus the null test on the work block.
void func_actor_403200_80138284(Task* arg0)
{
    Actor403200Work* work;
    s32              phase;

    work = (Actor403200Work*)arg0->idMap;
    if (work == NULL) {
        return;
    }

    if (work->field_EAC != work->field_EAD) {
        switch (work->field_EAC) {
            case 1:
                work->field_EAE = 5;
                break;
            case 2:
                work->field_EAE = 0xA;
                break;
            case 3:
                work->field_EAE = 0x16;
                break;
            case 0:
            default:
                return;
        }
        work->field_EAD = work->field_EAC;
    }

    if (work->field_EAE == 0) {
        Display_ClampField126(0);
        work->field_EAC = 0;
        work->field_EAD = 0;
        return;
    }
    work->field_EAE--;

    switch (work->field_EAC) {
        case 1:
            phase = work->field_EAE;
            if ((phase & 1) == 0) {
                work->field_EAF = 0;
            } else {
                work->field_EAF = 2;
            }
            Display_ClampField126(work->field_EAF);
            break;

        case 2:
            phase = work->field_EAE;
            switch (phase & 3) {
                case 0:
                    work->field_EAF = 0;
                    break;
                case 1:
                    work->field_EAF = 2;
                    break;
                case 2:
                    work->field_EAF = 3;
                    break;
                case 3:
                    work->field_EAF = 2;
                    break;
            }
            Display_ClampField126(work->field_EAF);
            break;

        case 3:
            phase = work->field_EAE;
            switch (phase & 7) {
                case 3:
                case 4:
                    work->field_EAF = 4;
                    break;
                case 2:
                case 5:
                    work->field_EAF = 3;
                    break;
                case 1:
                case 6:
                    work->field_EAF = 1;
                    break;
                case 0:
                case 7:
                    work->field_EAF = 0;
                    break;
            }
            Display_ClampField126(work->field_EAF);
            break;

        case 0:
        default:
            Display_ClampField126(0);
            break;
    }
}

/// The escort-group reset the enemy runs whenever its state changes: it turns
/// the host model's flag word around and pushes it onto all seven escorts'
/// models, differing in what the word becomes and whether the model buffers are
/// (re)allocated first. `work->field_7F3` is cleared on every path, and the two
/// that end with the work block's state index reset are the ones that set the
/// word to 0x80.
///
/// Same body as `func_actor_444000_8013A958` without that sibling's
/// `TmdObject::field_18` buffer tests, so every escort is re-allocated
/// unconditionally.
s32 func_actor_403200_80138468(Task* task, s32 arg1, s32 arg2)
{
    Actor403200Work* work;
    Actor403200Work* buffers;
    Actor403200Work* escorts;
    Actor403200Work* rebuilt;
    s16              i;
    s16              j;

    work = (Actor403200Work*)task->idMap;
    switch (arg2) {
        case 0:
            buffers = (Actor403200Work*)task->idMap;
            Tmd_AllocBuffers((TmdObject*)task->extra);
            for (j = 0; j < 7; j++) {
                if (buffers->field_ECC[j] != NULL) {
                    Tmd_AllocBuffers((TmdObject*)buffers->field_ECC[j]->task->extra);
                }
            }
            escorts                            = (Actor403200Work*)task->idMap;
            escorts->field_7F3                 = 0;
            ((TmdObject*)task->extra)->field_C = 0x80;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)task->extra)->field_C;
                }
            }
            work->field_0 = 0;
            break;
        case 1:
            escorts                            = (Actor403200Work*)task->idMap;
            escorts->field_7F3                 = 0;
            ((TmdObject*)task->extra)->field_C = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)task->extra)->field_C;
                }
            }
            rebuilt = (Actor403200Work*)task->idMap;
            Tmd_AllocBuffers((TmdObject*)task->extra);
            for (j = 0; j < 7; j++) {
                if (rebuilt->field_ECC[j] != NULL) {
                    Tmd_AllocBuffers((TmdObject*)rebuilt->field_ECC[j]->task->extra);
                }
            }
            break;
        case 2:
            work->field_7F3                    = 0;
            escorts                            = (Actor403200Work*)work;
            ((TmdObject*)task->extra)->field_C = 0x80;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)task->extra)->field_C;
                }
            }
            work->field_0 = 0;
            break;
        case 3:
            i                                  = 0;
            escorts                            = (Actor403200Work*)task->idMap;
            escorts->field_7F3                 = 0;
            ((TmdObject*)task->extra)->field_C = 0x80;
            for (; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)task->extra)->field_C;
                }
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80138748);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80138AFC);

/// The group-0 hit handler: takes at most one hit this frame and turns it into
/// damage.
///
/// It carves a 0x30-byte `Actor403200HitScratch` off the scratchpad stack and
/// scans the five `GpRec18` records of `hits[0]` for the first whose `field_4`
/// high halfword is attack kind 2 -- the contact point goes into the frame's
/// `pos` and the id is kept. A record with `field_4` 0 ends the scan with no
/// hit. The scan is written with labels rather than a `for` so `loop.c` parks
/// the match arm out of line; the same shape as
/// `func_actor_444000_8013C060`'s.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `Gp_GetIdParam2` of the attack id to all four per-group slots at 0xE8C, and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the range `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `Gp_RollEnemyChance` fires. The contact point is re-read relative to the
/// part's world translation and `ratan2` of the pair against the part's facing
/// gives the yaw `angle`, wrapped to +/-0x800. The damage is doubled, applied
/// through `func_800E2C78` and `func_800DA6E8`, and the host's remaining HP is
/// mirrored onto the three escorts sharing its pool.
///
/// The second arm runs the same tick on the `field_4C` bits 0x2/0x8 hit, which
/// `Gp_TickObjFlag4` turns into damage of its own; that one only comes off the
/// host.
///
/// `esc3` / `esc0` / `esc1` and the `hp` load are not spare: read as three
/// separate assignments the loaded pointers all share one register, and the
/// stores then interleave with their loads (the scheduler cannot hoist a load
/// past a store through an unknown pointer). Evaluating the three addresses
/// first is what puts them in `a0` / `a1` / `v1`, and the `hp` load has to sit
/// between the escort 3 and escort 0 ones to land where the original has it.
void func_actor_403200_80139A60(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               enemy;
    GpRec18*               recs;
    WipSysConfig*          cfg;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    i;
    s16                    param;
    u16                    hp;
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg   = &Wip_SysConfig;
    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor403200Work*)arg0->idMap;
    sc    = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos   = &sc->pos;
    recs  = work->hits[0].recs;
    i     = 0;
    mask  = 0xFFFF0000;
    kind  = 0x20000;
scan:
    if (recs[i].field_4 == 0) {
        goto missed;
    }
    if ((recs[i].field_4 & mask) == kind) {
        pos->vx = recs[i].field_8;
        pos->vy = recs[i].field_A;
        pos->vz = recs[i].field_C;
        id      = recs[i].field_4;
        goto found;
    }
    i++;
    if (i < 5) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        func_actor_403200_80134044(work->hits[0].obj.field_8, id);
        param           = Gp_GetIdParam2(sc->id);
        work->field_E90 = param;
        work->field_E8E = param;
        work->field_E8C = param;
        work->field_E92 = param;
        Gp_GetIdParam0(sc->id);

        sc->delta.vx = cfg->field_4->t[0] - ((TmdObject*)arg0->extra)->field_8->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->field_4->t[1] - ((TmdObject*)arg0->extra)->field_8->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->field_4->t[2] - ((TmdObject*)arg0->extra)->field_8->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);
        if (Gp_RollEnemyChance(enemy, sc->id, 0) != 0) {
            sc->damage *= 4;
        }
        if (sc->damage != 0) {
            sc->rot.vy = 0x320;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, &((TmdObject*)enemy->task->extra)->field_8[3], 3, &sc->rot);
        }
        ((TmdObject*)arg0->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->field_8);
        sc->rot.vx = sc->pos.vx - ((TmdObject*)arg0->extra)->field_8->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)arg0->extra)->field_8->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)arg0->extra)->field_8->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)arg0->extra)->field_8->workm.m[2][0],
                       ((TmdObject*)arg0->extra)->field_8->workm.m[2][2]);
        sc->angle = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
        sc->angle = angle;

        work->field_7C8 = 0;
        work->field_7C4 = 0;
        sc->damage     *= 2;
        func_800E2C78((GpObj40*)enemy, sc->id, sc->damage, 0);
        enemy->field_40 -= sc->damage;
        func_800DA6E8(&enemy->node, sc->damage, 0);
        esc3           = work->field_ECC[3];
        hp             = enemy->field_40;
        esc0           = work->field_ECC[0];
        esc1           = work->field_ECC[1];
        esc3->field_40 = hp;
        esc1->field_40 = hp;
        esc0->field_40 = hp;
    }

    if (enemy->field_4C & 0xC) {
        sc->damage = Gp_TickObjFlag4((GpObj5C*)enemy);
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
        if (sc->damage != 0) {
            func_800E2C78((GpObj40*)enemy, sc->id, sc->damage, 0);
            enemy->field_40 -= sc->damage;
        }
    }

    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80139E94);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013A4A0);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013AB70);

/// Reset handler: pushes the host model's `field_C` onto each of the seven
/// escorts, and once the sub-state counter has reached 2 releases the host's and
/// every escort's model buffers. Same shape as
/// `func_actor_403200_80141B40` with a second arm keyed on `field_6`.
///
/// The `modelFlag` copy is not redundant: the second arm's `0x80` has to reach
/// the store as a 32-bit value of its own, or the two arms merge it into the
/// first arm's constant and the second `li $v0, 0x80` disappears.
void func_actor_403200_8013B23C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    TmdObject*       tmd;
    s32              flag;
    s32              modelFlag;
    s16              i;
    s16              j;

    work = (Actor403200Work*)arg0->idMap;
    tmd  = (TmdObject*)arg0->extra;
    if (work->field_4 != 0) {
        tmd->field_C                       = 0x80;
        escorts                            = (Actor403200Work*)arg0->idMap;
        i                                  = 0;
        escorts->field_7F3                 = 0;
        ((TmdObject*)arg0->extra)->field_C = (flag = 0x80);
        for (; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C =
                    ((TmdObject*)arg0->extra)->field_C;
            }
        }
        work->field_6 = 0;
        return;
    }
    if (work->field_6 == 2) {
        tmd->field_C                       = 0x80;
        escorts                            = (Actor403200Work*)arg0->idMap;
        modelFlag                          = 0x80;
        i                                  = 0;
        escorts->field_7F3                 = 0;
        ((TmdObject*)arg0->extra)->field_C = (flag = modelFlag);
        for (; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C =
                    ((TmdObject*)arg0->extra)->field_C;
            }
        }
        dying = (Actor403200Work*)arg0->idMap;
        Tmd_FreeBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_FreeBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013B3C8);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013B740);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013B8C4);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013C84C);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013D028);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013D78C);

/// State-change reset for the enemy's stand-up. It clears the host model's flag
/// word, walks the seven escorts pushing that word onto each of their models,
/// allocates every escort's model buffers and then arms the block -- `field_EF6`
/// and `field_EF4` at 1, `field_EFA` at 0, `field_E96` at 0xC80 -- before
/// playing the type-6 cue built from the spawn record's `field_8`. Same shape as
/// `func_actor_403200_8013B23C`'s reset half, with the buffer allocator on the
/// second walk in place of the release.
///
/// The state then writes its two cue frames, and the three states at 0x39, 0x45
/// and 0x4C spawn `field_EF0` from `D_actor_403200_8015E858`; every other state
/// in the 0x39..0x4C window falls through to the dispatcher.
///
/// The `state` copy is what keeps the switch index 16-bit: switched on
/// `field_6 - 0x39` directly the index is an `int`, and the `lh` the load
/// becomes carries the sign extension the original does with a separate
/// `sll`/`sra` pair (dropping 2 instructions and 2.8% of the match).
void func_actor_403200_8013D9EC(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
    s16              i;
    s16              j;
    s16              state;
    s32              sfx;
    s32              pan;

    work  = (Actor403200Work*)arg0->idMap;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_F1D                    = 6;
        work->field_7B3                    = 6;
        work->field_7B0                    = 2;
        escorts                            = (Actor403200Work*)arg0->idMap;
        escorts->field_7F3                 = 0;
        ((TmdObject*)arg0->extra)->field_C = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C =
                    ((TmdObject*)arg0->extra)->field_C;
            }
        }
        dying = (Actor403200Work*)arg0->idMap;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFA = 0;
        work->field_E96 = 0xC80;
        sfx             = (((u16)enemy->field_8 >> 12) << 8) | 0x40200017;
        pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(sfx, pan,
                            (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    state = work->field_6 - 0x39;
    switch (state) {
        case 6:
            work->field_7B3 = 0xC;
            work->field_7B0 = 1;
            break;
        case 13:
            work->field_7B3 = 0xC;
            work->field_7B0 = 2;
            break;
        case 0:
        case 12:
        case 19:
            spawned          = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 0, 0, arg0->spawnArg2);
            spawned->field_A = 0x900;
            work->field_EF0  = spawned;
            break;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->field_58 & 1) {
        work->field_0 = 7;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 3;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013DC3C);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013E2FC);

/// State 0x12, the enemy's death sequence: the model is torn down and rebuilt
/// so the collapse animation can run on it.
///
/// A reset request clears the host model's `field_C` and pushes the cleared
/// word onto each of the seven escorts' own model objects, allocates the host's
/// and every escort's model buffers, forces `field_F14 / 4` extra per-frame
/// steps -- stopping early once `field_58` bit 0 is set -- and then re-arms the
/// animation slot at 0x10, plays the type-7 death cue and leaves the yaw target
/// at 0xFA0 and the escort pose cleared.
///
/// The rest of the tick winds the shared `D_actor_403200_80141C58` counter down
/// by 0xC8 once it has passed 0x1F4, runs the per-frame body, clears the host
/// coordinate's rebuild flag, and on frame 0x1C of `field_72` arms the screen
/// shake at level 3 and spawns the `D_actor_403200_80141C5C` script pair. While
/// `field_7B3` is still 0x12 four one-shot cues fire on frames 0x33, 0x3D, 0x4E
/// and 0x71 of `field_9A`, each latching the frame it saw in `field_7A8`.
void func_actor_403200_8013E5A8(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         obj;
    s16              i;
    s16              j;
    s32              state;
    s32              frame;

    work  = (Actor403200Work*)arg0->idMap;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                                = arg0->spawnArg2;
        escorts                            = (Actor403200Work*)arg0->idMap;
        work->field_7F3                    = 0;
        ((TmdObject*)arg0->extra)->field_C = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C =
                    ((TmdObject*)arg0->extra)->field_C;
            }
        }
        dying = (Actor403200Work*)arg0->idMap;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_7B6 = 0x40;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        j               = 0;
        while (j < work->field_F14 / 4) {
            func_actor_403200_80133DD8(arg0);
            j++;
            if (work->field_58 & 1) {
                break;
            }
        }
        work->field_F06 = 7;
        work->field_7B6 = 0x10;
        SndEvt_EnqueueType7((((u16)obj->field_8 >> 12) << 8) | 0x4020000A, 1);
        work->field_7A4 = 0;
        work->field_E96 = 0xFA0;
    }
    if (D_actor_403200_80141C58 >= 0x1F5) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
    }
    func_actor_403200_80133DD8(arg0);
    ((TmdObject*)arg0->extra)->field_8->flg = 0;
    state                                   = work->field_72 & 0x3FF;
    if (state == 0x1C && work->field_7D8 != state) {
        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
    }
    work->field_7D8 = work->field_72 & 0x3FF;
    if (work->field_7B3 == 0x12) {
        frame = work->field_9A & 0x3FF;
        if (frame == 0x33 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200013;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(id, pan,
                                (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
        frame = work->field_9A & 0x3FF;
        if (frame == 0x3D && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200003;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(id, pan,
                                (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
        frame = work->field_9A & 0x3FF;
        if (frame == 0x4E && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200014;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(id, pan,
                                (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
        frame = work->field_9A & 0x3FF;
        if (frame == 0x71 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200015;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(id, pan,
                                (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
        work->field_7A8 = work->field_9A & 0x3FF;
    }
}

/// The enemy's attack-launch body: when the dispatcher has flagged the state
/// change it re-arms the work block (`field_7A4` at 3, `field_E96` at 0xC80) and
/// plays the two launch cues -- a type-6 with the object's pan and depth, then
/// type-7s for ids 0x0D and 0x09 -- and otherwise runs the per-frame body,
/// winding the shared `D_actor_403200_80141C58` counter down by 0xC8 once it has
/// passed 0x190 and clearing `field_F06` once `field_6` has passed 0x14.
void func_actor_403200_8013E9C0(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         obj;
    s32              state;
    s32              id;
    s32              pan;

    work = (Actor403200Work*)arg0->idMap;
    if (work->field_4 != 0) {
        obj             = arg0->spawnArg2;
        state           = work->field_7B3;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        work->field_EFA = 1;
        if (state != 0xD) {
            work->field_7B0 = 1;
            work->field_7B3 = 0xD;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        id  = (((u16)obj->field_8 >> 12) << 8) | 0x40200004;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(id, pan,
                            (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_7A4 = 3;
        work->field_E96 = 0xC80;
        SndEvt_EnqueueType7((((u16)obj->field_8 >> 12) << 8) | 0x4020000D, 1);
        SndEvt_EnqueueType7((((u16)obj->field_8 >> 12) << 8) | 0x40200009, 1);
        return;
    }
    SCRATCH_SP -= 0xC;
    func_actor_403200_80133DD8(arg0);
    if (D_actor_403200_80141C58 >= 0x191) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        work->field_7A4         = 0;
    }
    if (work->field_58 & 1) {
        work->field_0 = 0xA;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 0;
    }
    SCRATCH_SP += 0xC;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013EB64);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013EF6C);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013F700);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013FB54);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_801408D8);

/// The enemy's upkeep tick, run by the dispatcher through the same
/// `D_actor_403200_801321B8` table the other tasks in this overlay use. It drops
/// each of the two escort slots whose HP has run out, then walks the work
/// block's `field_E94` toward `field_E96` by 0x32 a tick -- snapping once the
/// two are within 0x33 -- calls the follow helper with the new value, and
/// finally lifts the host's own X up to the escort's so the party never sinks
/// below the enemy. The tick ends by dispatching on `state` through the local
/// copy of the handler table.
void func_actor_403200_80140E6C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;
    Actor403200Work*      work;
    Task*                 player;
    GpEnemy*              enemy;
    s32                   diff;
    s32                   y;
    GsCOORDINATE2*        playerCoord;
    GsCOORDINATE2*        selfCoord;

    sp     = D_actor_403200_801321B8;
    player = Game_GetPtrSlot(3);
    work   = (Actor403200Work*)arg0->idMap;
    enemy  = arg0->spawnArg2;
    if (work != NULL) {
        if (work->field_EE8 != NULL && work->field_EE8->field_40 <= 0) {
            work->field_EE8 = NULL;
        }
        if (work->field_EEC != NULL && work->field_EEC->field_40 <= 0) {
            work->field_EEC = NULL;
        }
        diff = work->field_E96 - work->field_E94;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff >= 0x33) {
            if (work->field_E94 < work->field_E96) {
                work->field_E94 = (u16)work->field_E94 + 0x32;
            } else {
                work->field_E94 = (u16)work->field_E94 - 0x32;
            }
        } else {
            work->field_E94 = (u16)work->field_E96;
        }
        func_actor_403200_801408D8(arg0, work->field_E94, work->field_E98, 0);
        playerCoord = ((TmdObject*)player->extra)->field_8;
        selfCoord   = ((TmdObject*)arg0->extra)->field_8;
        y           = selfCoord->coord.t[0] + work->field_E94;
        if (playerCoord->coord.t[0] < y) {
            playerCoord->coord.t[0] = y;
        }
    }
    sp.funcs[arg0->state](enemy, arg0);
}

/* `migrate_rodata_to_functions` folds this run into the `.s` of
 * `func_actor_403200_80140E6C`, whose body is C here, so its bytes have to be
 * emitted in this unit: the twelve bytes that follow the table
 * `func_actor_403200_8013FB54` carries. */
#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "nonmatching D_actor_403200_801321B8\n"
        "dlabel D_actor_403200_801321B8\n"
        "    .word func_actor_403200_80138AFC\n"
        "    .word func_actor_403200_8013FB54\n"
        "    .word Gp_DestroyEnemy\n"
        "enddlabel D_actor_403200_801321B8\n"
        ".section .text");
#endif
