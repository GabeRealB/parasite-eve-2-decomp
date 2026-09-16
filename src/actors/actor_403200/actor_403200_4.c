#include "common.h"

#include "actors/actor_403200.h"
#include "actors/actor_403200_view.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s16 D_actor_403200_80141C58;

/// Cleared by both halves of the launch state below; `actor_403200_5.c` exposes
/// it through the setter / getter pair `func_actor_403200_80141108` and
/// `func_actor_403200_80141114`.
extern s16 D_actor_403200_80141C5A;

/// Non-zero once the launch state has published the enemy's position to the
/// player, and cleared again when it restarts.
extern s8 D_actor_403200_8015F8E0;

/// Shared 0x7DA payload buffer, also used by the other states of this overlay.
extern Actor403200Msg7DA D_actor_403200_8015F8F4;

/// World point the launch tick hands the player as message 0x3E9, built from
/// the host model's root coordinate.
extern VECTOR3 D_actor_403200_8015F9C0;

/// Non-zero while the overlay is shutting down, which is what makes the
/// state-selecting tick below hold `field_6` at zero and re-roll its sub-state.
extern s16 D_actor_403200_80141C50;

/// LCG state the state-selecting tick below rolls its sub-state out of.
extern s32 Gp_LcgState;

/// The script pair the death sequence's frame-0x1C cue spawns.
extern s32 D_actor_403200_80141C5C;
extern s32 D_actor_403200_80141C64;

/// Enemy spawn table the three launch states of `func_actor_403200_8013D9EC`
/// draw from.
extern TaskDesc D_actor_403200_8015E858;

/// The scratch coordinate the debris effect of `func_actor_403200_8013DC3C` is
/// built on: `F920` is the whole `GsCOORDINATE2` and `F924` its `coord` matrix,
/// which splat names separately because the code takes that address directly.
extern GsCOORDINATE2 D_actor_403200_8015F920;
extern MATRIX        D_actor_403200_8015F924;

/// Animation table the stand-up tick publishes to the player in its message
/// 0x3FF, the same role `D_actor_444000_80161670` has for the arena tick.
extern GpAnimSet* D_actor_403200_8015E6AC[];

/// Absolute gate the hit handlers share, read as the halfword array its readers
/// index: the byte at +0 is the mode flag the rest of the game writes, and the
/// halfword at +2 is the "player hold is armed" gate the group 3-5 hit handler
/// tests before it lets a landed hit spawn its effect.
extern u16 D_801153F4[];

/// Reply buffer the stand-up tick passes with its message 0x3F8 before it asks
/// the player for the hold. Same shape as `D_actor_444000_80161928`.
extern Actor403200Msg3F8 D_actor_403200_8015FA00;

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

/// The hit handler for collision groups 1 and 2 -- the same scan
/// `func_actor_403200_80139A60` runs for group 0, done twice: group 1 first, and
/// group 2 only if nothing landed on group 1. The second scan carries its own
/// `recs2` / `pos2` / `i2`, because sharing `recs` / `pos` / `i` with the first
/// gives both loops one pseudo each and the wrong registers. Both scans are
/// written as real `for` loops rather than the group-0 handler's labels so
/// `find_and_verify_loops` parks the match arm out of line.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `Gp_GetIdParam2` of the attack id to all four per-group slots at 0xE8C and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the range `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `Gp_RollEnemyChance` fires, and zeroed unless the attack kind came back 2.
/// The damage also comes off the work block's `field_F0E` pool and the host's
/// remaining HP is mirrored onto the three escorts sharing its pool.
/// `sc->angle` is the yaw of the contact point relative to the fourth escort's
/// facing, wrapped to +/-0x800.
///
/// The attack kind drives a sub-state change: kinds 4 and 6 roll `Gp_LcgState`
/// and take the boss out of state 3 into 8 one time in six, kind 2 does it
/// outright, and both are gated on the `field_F1C` re-arm countdown.
///
/// `esc3` / `esc0` / `esc1` and the `hp` load are not spare: read as three
/// separate assignments the loaded pointers all share one register, and the
/// stores then interleave with their loads. Evaluating the three addresses
/// first is what puts them in `a0` / `a1` / `v1`, and the `hp` load has to sit
/// between the escort 3 and escort 0 ones to land where the original has it.
///
/// The `do` / `while (0)` around the angle wrap is load-bearing, not stylistic.
/// Its body sits at loop depth 1, so `flow.c`'s `REG_N_REFS (regno) +=
/// loop_depth` gives `sc` one reference more than the unwrapped form (37
/// against 36, `work` sitting at 37 on a longer live range). That is what ranks
/// `sc` above `work` in global-alloc and puts it in `$s1`; unwrapped the two
/// exchange registers and the function stops at 99.06%.
void func_actor_403200_80139E94(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               host;
    WipSysConfig*          cfg;
    GsCOORDINATE2*         coord;
    GpRec18*               recs;
    GpRec18*               recs2;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    param;
    u16                    roll;
    u16                    hp;
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg  = &Wip_SysConfig;
    host = (GpEnemy*)arg0->spawnArg2;
    work = (Actor403200Work*)arg0->idMap;
    sc   = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[1].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].field_4 == 0) {
            goto missed1;
        }
        if ((recs[i].field_4 & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].field_8;
            pos->vy = recs[i].field_A;
            pos->vz = recs[i].field_C;
            id      = recs[i].field_4;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[1].obj.field_8;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[2].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].field_4 == 0) {
            goto missed2;
        }
        if ((recs2[i2].field_4 & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].field_8;
            pos2->vy = recs2[i2].field_A;
            pos2->vz = recs2[i2].field_C;
            id       = recs2[i2].field_4;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    coord = work->hits[2].obj.field_8;
hit:
    func_actor_403200_80134044(coord, id);
    if (sc->id != 0) {
        param           = Gp_GetIdParam2(sc->id);
        work->field_E90 = param;
        work->field_E8E = param;
        work->field_E8C = param;
        work->field_E92 = param;
        switch (Gp_GetIdParam0(sc->id) & 0xFFFF) {
            case 0:
            case 1:
            case 3:
            case 5:
            case 7:
            case 8:
            case 9:
                break;

            case 4:
            case 6:
                state = work->field_0;
                if (state != 3) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    roll        = ((u32)Gp_LcgState >> 16) % 6;
                    if (roll == 0 && work->field_F1C == 0) {
                        work->field_0 = 8;
                        work->field_2 = -1;
                    }
                }
                break;

            case 2:
                if (work->field_F1C == 0 && (state = work->field_0, state != 3)) {
                    work->field_0 = 8;
                    work->field_2 = -1;
                }
                break;
        }

        sc->delta.vx = cfg->field_4->t[0] - ((TmdObject*)arg0->extra)->field_8->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->field_4->t[1] - ((TmdObject*)arg0->extra)->field_8->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->field_4->t[2] - ((TmdObject*)arg0->extra)->field_8->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

        if (Gp_RollEnemyChance(work->field_ECC[3], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
            state != 9 && state != 0xE && state != 0xF) {
            sc->rot.vy = 0;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, ((TmdObject*)work->field_ECC[3]->task->extra)->field_8, 0, &sc->rot);
            if (work->field_0 != 9 && work->field_F1C == 0) {
                work->field_0 = 8;
                work->field_2 = -1;
            }
            sc->damage *= 4;
        } else if ((Gp_GetIdParam0(sc->id) & 0xFFFF) != 2) {
            sc->damage = 0;
        }

        func_800E2C78((GpObj40*)host, sc->id, sc->damage, 0);
        host->field_40 -= sc->damage;
        func_800DA6E8(&work->field_ECC[3]->node, sc->damage, 0);
        work->field_F0E                                            -= sc->damage;
        esc3                                                        = work->field_ECC[3];
        hp                                                          = host->field_40;
        esc0                                                        = work->field_ECC[0];
        esc1                                                        = work->field_ECC[1];
        esc3->field_40                                              = hp;
        esc1->field_40                                              = hp;
        esc0->field_40                                              = hp;
        ((TmdObject*)work->field_ECC[3]->task->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)work->field_ECC[3]->task->extra)->field_8);
        sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[3]->task->extra)->field_8->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[3]->task->extra)->field_8->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[3]->task->extra)->field_8->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)arg0->extra)->field_8->workm.m[2][0],
                       ((TmdObject*)arg0->extra)->field_8->workm.m[2][2]);
        do {
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
        } while (0);
        sc->angle = angle;

        work->field_7C8 = 0;
        work->field_7C4 = 0;
    }
out:
    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// The hit handler for collision groups 3, 4 and 5 -- `func_actor_403200_80139E94`
/// done three times over the parts it does not cover, each group only scanned
/// when the previous one landed nothing and the part it hit reported no attack
/// id back. Like the sibling actor's `func_actor_444000_8013CA60`, this one runs
/// no `Gp_GetIdParam0` switch: the call is made and its kind thrown away, so
/// every hit is treated alike, and the group 3 arm is the one that gives up and
/// leaves the frame once it comes back empty.
///
/// The damage is the distance-scaled hit quadrupled when `Gp_RollEnemyChance`
/// fires, then divided by six (never down to zero unless it already was), and
/// comes off the host, the two escorts sharing its pool and `field_F0A`.
/// Emptying that pool spawns the same effect again and refills it to 0x32. Both
/// effect spawns and the state change to 0xE are skipped while the boss is in
/// one of the seven states that ignore hits, while the player hold is armed, or
/// while `D_801153F4[1]` is clear.
///
/// `pos` / `pos2` / `pos3` are all `&sc->pos`, and are not spare: each group's
/// scan writes the contact point through its own pointer, which is what keeps
/// the three `sh` pairs in `a3` then `a2` twice. `esc3` / `esc0` / `esc1` and
/// the `hp` load are the sibling's arrangement, but evaluated before
/// `func_800DA6E8` so `host->field_40` is still in a register and the three
/// stores reuse it; the pool subtraction after them carries the same `field_40`
/// value for the same reason.
void func_actor_403200_8013A4A0(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               host;
    WipSysConfig*          cfg;
    GpRec18*               recs;
    GpRec18*               recs2;
    GpRec18*               recs3;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    SVECTOR*               pos3;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    u32                    dmg;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    i3;
    s16                    param;
    u16                    hp;
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg  = &Wip_SysConfig;
    host = (GpEnemy*)arg0->spawnArg2;
    work = (Actor403200Work*)arg0->idMap;
    sc   = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[3].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].field_4 == 0) {
            goto missed1;
        }
        if ((recs[i].field_4 & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].field_8;
            pos->vy = recs[i].field_A;
            pos->vz = recs[i].field_C;
            id      = recs[i].field_4;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        func_actor_403200_80134044(work->hits[3].obj.field_8, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos2  = &sc->pos;
    recs2 = work->hits[4].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].field_4 == 0) {
            goto missed2;
        }
        if ((recs2[i2].field_4 & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].field_8;
            pos2->vy = recs2[i2].field_A;
            pos2->vz = recs2[i2].field_C;
            id       = recs2[i2].field_4;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        func_actor_403200_80134044(work->hits[4].obj.field_8, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[5].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].field_4 == 0) {
            goto missed3;
        }
        if ((recs3[i3].field_4 & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].field_8;
            pos3->vy = recs3[i3].field_A;
            pos3->vz = recs3[i3].field_C;
            id       = recs3[i3].field_4;
            goto found3;
        }
    }
missed3:
    id = 0;
found3:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    func_actor_403200_80134044(work->hits[5].obj.field_8, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    param           = Gp_GetIdParam2(sc->id);
    work->field_E90 = param;
    work->field_E8E = param;
    work->field_E8C = param;
    work->field_E92 = param;
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->field_4->t[0] - ((TmdObject*)arg0->extra)->field_8->coord.t[0]) + 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->field_4->t[1] - ((TmdObject*)arg0->extra)->field_8->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->field_4->t[2] - ((TmdObject*)arg0->extra)->field_8->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[0], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 &&
        D_801153F4[1] == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1], 0, &sc->rot);
        sc->damage   *= 4;
        work->field_0 = 0xE;
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        dmg = 1;
        if (sc->damage == 0) {
            sc->damage = 0;
            goto stored;
        }
    }
    sc->damage = dmg;
stored:
    func_800E2C78((GpObj40*)host, sc->id, sc->damage, 0);
    host->field_40  -= sc->damage;
    esc3             = work->field_ECC[3];
    hp               = host->field_40;
    esc0             = work->field_ECC[0];
    esc1             = work->field_ECC[1];
    esc3->field_40   = hp;
    esc1->field_40   = hp;
    esc0->field_40   = hp;
    work->field_F0A -= sc->damage;
    if ((s16)work->field_F0A <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 && D_801153F4[1] == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0A = 0x32;
    }

    func_800DA6E8(&work->field_ECC[0]->node, sc->damage, 0);
    ((TmdObject*)work->field_ECC[0]->task->extra)->field_8->flg = 0;
    Gp_UpdateCoord(((TmdObject*)work->field_ECC[0]->task->extra)->field_8);
    sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[0]->task->extra)->field_8->workm.t[0];
    sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[0]->task->extra)->field_8->workm.t[1];
    sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[0]->task->extra)->field_8->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-((TmdObject*)arg0->extra)->field_8->workm.m[2][0],
                   ((TmdObject*)arg0->extra)->field_8->workm.m[2][2]);
    do {
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
    } while (0);
    sc->angle = angle;

    work->field_7C8 = 0;
    work->field_7C4 = 0;
out:
    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

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

/// State-change reset for the enemy's launch state: `func_actor_403200_8013B23C`'s
/// reset half with a yaw servo in the middle. It arms the stand-up pair
/// (`field_F1D` 2, `field_7B3` 3), turns animation slot 2 on, clears the host
/// model's flag word and walks the seven escorts pushing that word onto each of
/// their models, allocates the host's and every escort's buffers, and only then
/// turns the enemy to face the player -- the host root part's position made
/// relative to the player's root coordinate, `ratan2` of that pair less the
/// enemy's own facing, wrapped to +/-0x800 into `field_7C4`. On the way out it
/// runs the per-frame body, re-arms the state to 0xA on the animation slot's
/// flag, and latches `field_F06` once the state counter is past 0x14.
///
/// The switch is on the state counter and spawns from
/// `D_actor_403200_8015E858`, each of the eight counter values picking its own
/// table index; the spawned enemy is dropped, unlike the arena reset's. The
/// `state` copy is what keeps the switch index 16-bit, as in
/// `func_actor_403200_8013D9EC`.
void func_actor_403200_8013B3C8(Task* arg0)
{
    Actor403200Work*        work;
    Actor403200Work*        escorts;
    Actor403200Work*        dying;
    GsCOORDINATE2*          model;
    GsCOORDINATE2*          facing;
    Actor403200TurnScratch* sc;
    s16                     i;
    s16                     j;
    s16                     state;
    s16                     ang;

    sc   = (Actor403200TurnScratch*)(SCRATCH_SP -= sizeof(Actor403200TurnScratch));
    work = (Actor403200Work*)arg0->idMap;
    if (work->field_4 != 0) {
        work->field_F1D                    = 2;
        work->field_7B3                    = 3;
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
    }
    state = work->field_6 - 0x13;
    switch (state) {
        case 0:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 0, arg0->spawnArg2)->field_A = 0x900;
            break;
        case 7:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 1, arg0->spawnArg2)->field_A = 0x900;
            break;
        case 9:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 2, arg0->spawnArg2)->field_A = 0x900;
            break;
        case 0x10:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 3, arg0->spawnArg2)->field_A = 0x900;
            break;
        case 0x1F:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 4, arg0->spawnArg2)->field_A = 0x900;
            break;
        case 0x37:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 5, arg0->spawnArg2)->field_A = 0x900;
            break;
        case 0x3B:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 6, arg0->spawnArg2)->field_A = 0x900;
            break;
        case 0x3F:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 7, arg0->spawnArg2)->field_A = 0x900;
            break;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->field_58 & 1) {
        work->field_0 = 0xA;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 1;
    }
    model      = ((TmdObject*)arg0->extra)->field_8;
    sc->dir.vx = Wip_SysConfig.field_4->t[0] - model->coord.t[0];
    sc->dir.vy = Wip_SysConfig.field_4->t[1] - model->coord.t[1];
    sc->dir.vz = Wip_SysConfig.field_4->t[2] - model->coord.t[2];
    facing     = ((TmdObject*)arg0->extra)->field_8;
    ang        = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (ang < 0) {
    wrapUp:
        if (ang < -0x800) {
            ang += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (ang > 0x800) {
            ang -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = ang;
    SCRATCH_SP     += sizeof(Actor403200TurnScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013B740);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013B8C4);

/// State-change reset for the enemy's launch state, and the tick that walks it
/// out of sub-state 0xF into 0xE.
///
/// The reset half is `func_actor_403200_8013B23C`'s with a yaw servo in the
/// middle: it tells the scene (message 0x7DA, action 0x2C), arms sub-state 0xF
/// with animation 2, clears the host model's flag word and walks the seven
/// escorts pushing that word onto each of their models, allocates the host's and
/// every escort's buffers, and only then turns the enemy to face the player --
/// the fourth model part's position carried into view space, made relative to
/// the player's root coordinate with y zeroed, `ratan2` of that pair less the
/// enemy's own facing, wrapped to +/-0x800 into `field_7C4`. It tells the scene
/// a second time, arms `field_E96`, raises bit 0 of `Gp_StateC08.field_6`,
/// pulses the state and clears the node slot of the host and of escorts 3, 0
/// and 1.
///
/// The tick runs the per-frame body, steps 0xF to 0xE on the second animation
/// slot's flag, and while still in 0xF hands the player the launch message
/// (0x3F9) with `Wip_SysConfig.field_18` as its gate: the two arms either side
/// of that dispatch write the ramp timings into `Game_Session` and stamp escort
/// 3. The four one-shot cues all latch on the third animation slot's frame,
/// masked to ten bits, against the frame `field_7A8` saw last, and once the
/// state counter is past 0x18 the type-7 cue and the 0x3FF animation message go
/// out together.
///
/// Three things here are load-bearing. The yaw's arguments are read through
/// `posp` and the matrix half through `coord`: read straight off `view` the
/// stores would be forwarded into both arguments (two `sll`/`sra` pairs),
/// while through the pointer each stays a load out of the struct, which is what
/// the target does -- the second is reloaded from its slot, the first is folded
/// back onto `a0`, and `coord` is what keeps `field_8` in `s0` across the call.
/// The cue locals are declared inside each arm so local-alloc colours them per
/// block; hoisted to the top of the function they become one global pseudo and
/// the id and pan come out in each other's registers. And in the second 0x7DA
/// block `D_actor_403200_8015F8E0` is cleared before the `field_7C4` store, so
/// its address is the one computed first.
void func_actor_403200_8013C84C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    Task*            task;
    WipSysConfig*    cfg;
    SVECTOR          view;
    SVECTOR*         posp;
    GsCOORDINATE2*   coord;
    s16              i;
    s16              j;
    s16              yaw;

    work  = (Actor403200Work*)arg0->idMap;
    enemy = arg0->spawnArg2;
    task  = Game_GetPtrSlot(3);
    cfg   = &Wip_SysConfig;
    if (work->field_4 != 0) {
        D_actor_403200_8015F8F4.field_0 = 0;
        D_actor_403200_8015F8F4.field_1 = 0x2C;
        D_actor_403200_8015F8F4.field_2 = 3;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);
        D_actor_403200_80141C5A = 0;
        SndEvt_EnqueueType7((((u16)enemy->field_8 >> 12) << 8) | 0x4020000A, 1);
        work->field_7B3                    = 0xF;
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
        work->field_F06 = 6;
        work->field_EFE = 0;
        work->field_EF4 = 0;
        work->field_EFA = 0;
        view.vz         = 0;
        view.vy         = 0;
        view.vx         = 0;
        Actor403200_LocalToView(&((TmdObject*)arg0->extra)->field_8[4], &view);
        view.vx = ((TmdObject*)task->extra)->field_8[0].coord.t[0] - view.vx;
        view.vy = 0;
        view.vz = ((TmdObject*)task->extra)->field_8[0].coord.t[2] - view.vz;
        posp    = &view;
        coord   = ((TmdObject*)arg0->extra)->field_8;
        yaw     = ratan2(posp->vx, posp->vz) -
              ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (yaw < 0) {
        wrapUp:
            if (yaw < -0x800) {
                yaw += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (yaw > 0x800) {
                yaw -= 0x1000;
                goto wrapDown;
            }
        }
        D_actor_403200_8015F8E0         = 0;
        work->field_7C4                 = yaw;
        D_actor_403200_8015F8F4.field_0 = 0;
        D_actor_403200_8015F8F4.field_1 = 0x2C;
        D_actor_403200_8015F8F4.field_2 = 3;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);
        work->field_E96         = 0x9C4;
        D_actor_403200_80141C5A = 0;
        Gp_StateC08.field_6    |= 1;
        Gp_PulseState1C();
        Gp_ClearNodeSlots(&enemy->node);
        Gp_ClearNodeSlots(&work->field_ECC[3]->node);
        Gp_ClearNodeSlots(&work->field_ECC[0]->node);
        Gp_ClearNodeSlots(&work->field_ECC[1]->node);
        return;
    }

    SCRATCH_SP -= 0x3C;
    func_actor_403200_80133DD8(arg0);
    if ((work->field_58 & 1) && (work->field_7B3 == 0xF)) {
        work->field_7B0 = 2;
        work->field_7B3 = 0xE;
    }
    if (work->field_7B3 == 0xF) {
        if (cfg->field_18 > 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 3), 0);
            if (cfg->field_18 <= 0) {
                ((GameActor*)task->idMap)->field_956 = 0xA;
                Game_Session->field_12D              = 0x1E;
                Game_Session->field_12E              = 0x36;
                Game_Session->field_12F              = 0x5A;
            }
        }
        if (((work->field_9A & 0x3FF) == 0x19) && (work->field_7A8 != (work->field_9A & 0x3FF))) {
            s32 sfx;
            s32 pan;
            s32 depth;

            sfx   = (((u16)enemy->field_8 >> 12) << 8) | 0x40200011;
            pan   = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            depth = (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(sfx, pan, depth);
        }
        work->field_7A8 = work->field_9A & 0x3FF;
    }
    if (work->field_7B3 == 0xE) {
        if (((work->field_9A & 0x3FF) == 0x1E) && (work->field_7A8 != (work->field_9A & 0x3FF))) {
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->field_9A & 0x3FF) == 0x23) && (work->field_7A8 != (work->field_9A & 0x3FF))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->field_8 >> 12) << 8) | 0x40200012;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(sfx, pan,
                                (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->field_9A & 0x3FF) == 0x27) && (work->field_7A8 != (work->field_9A & 0x3FF))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->field_8 >> 12) << 8) | 0x40200012;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(sfx, pan,
                                (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        work->field_7A8 = work->field_9A & 0x3FF;
    }
    if ((Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) && (cfg->field_18 > 0)) {
        D_actor_403200_8015F9C0.vx = ((TmdObject*)arg0->extra)->field_8[0].coord.t[0];
        D_actor_403200_8015F9C0.vy = ((TmdObject*)arg0->extra)->field_8[0].coord.t[1];
        D_actor_403200_8015F9C0.vz = ((TmdObject*)arg0->extra)->field_8[0].coord.t[2];
        Gp_DispatchMsg(task, 0x3E9, (s32)&D_actor_403200_8015F9C0, 0);
        D_actor_403200_8015F8E0 = 1;
    }
    if (work->field_6 < 0x18) {
        SndEvt_EnqueueType7((((u16)enemy->field_8 >> 12) << 8) | 0x4020000A, 1);
        Gp_DispatchMsg(task, 0x3FF, (s32)&work->field_EB0, 0);
        work->field_7CA = 0;
    }
    SCRATCH_SP += 0x3C;
}

/// State-change reset for the enemy's stand-up, plus the swipe tick that runs
/// on every step afterwards. The reset half is `func_actor_403200_8013B23C`'s
/// with the buffer allocator on the second walk in place of the release: it
/// clears the host model's flag word, walks the seven escorts pushing that word
/// onto each of their models, allocates the host's and every escort's buffers,
/// rebuilds the free coordinate `field_E3C` from `field_7C8` and arms `field_E96`
/// at 0xC80 before playing the entry cue.
///
/// Every step then clears that coordinate's flag and updates it, and each of the
/// two swipe sub-states watches one animation slot's frame: sub-state 4 raises
/// bit 0x8000 of the tenth collision object's `flags` and fires its two cues once
/// `slots0[1]` reaches frame 0xC, sub-state 5 clears `field_EFA` and fires its
/// single cue on `slots0[2]` frame 0x1C. Both cues are positioned on the first
/// escort's second coordinate at half depth, and whichever sub-state is live is
/// the one whose frame `field_7AC` is refreshed from -- the shared mask is what
/// makes the pair one-shot. The switch on `field_6` arms the escort pose index
/// `field_7A4` for seven states, 0x14 and 0xDC also seeding the shared countdown
/// `D_actor_403200_80141C58` and re-arming `field_0`, and the 0x29..0x2E window
/// raises that countdown by 0x258 while it is still under 0x1770.
///
/// The tail runs the per-frame body, scans the tenth collision object's five
/// `recs2` records for one whose high half is 0x10000, and -- when it finds one,
/// the enemy's HP is positive and the player's 0x3F8 query comes back zero --
/// asks the player for the hold (0x3F9) and re-sends it the animation, stamping
/// the player's `field_956` when the hold was taken. Past frame 0x39 the shared
/// countdown is walked down 0x1E, or 0xC8 once it is past 0xBB9, and past 0x15
/// the state arms `field_F06`.
///
/// The countdown's two arms are load-bearing: the `>= 0xBB9` test reads the
/// halfword signed (`lh`) while each arm subtracts from it zero-extended
/// (`lhu`), and writing the pair as one assignment off a shared temp lets CSE
/// fold the compare onto the earlier zero-extended load, which costs an
/// `sll`/`sra` re-extension pair the original does not have.
void func_actor_403200_8013D028(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpRec18*         recs;
    GpEnemy*         enemy;
    Task*            task;
    Task*            target;
    s16              i;
    s16              j;
    s16              k;
    s16              frame;
    s16              frame2;
    s16              reply;
    s32              found;
    s32              resetId;
    s32              resetPan;
    s32              swipeId;
    s32              swipePan;
    s32              swipe2Id;
    s32              swipe2Pan;
    s32              hitId;
    s32              hitPan;
    s32              cueId;
    s32              cuePan;

    work        = (Actor403200Work*)arg0->idMap;
    enemy       = arg0->spawnArg2;
    task        = Game_GetPtrSlot(3);
    SCRATCH_SP -= 0x30;

    if (work->field_4 != 0) {
        work->field_F1D                    = 0xB;
        work->field_7B3                    = 4;
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
        work->field_EF4 = 0;
        work->field_EFA = 1;
        work->field_EF8 = 1;
        Gfx_RotMatrixY(&work->field_E3C.coord, work->field_7C8, 1);
        work->field_E3C.flg = 0;
        Gp_UpdateCoord(&work->field_E3C);
        work->field_E96 = 0xC80;
        resetId         = (((u16)enemy->field_8 >> 12) << 8) | 0x40200017;
        resetPan        = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }

    work->field_E3C.flg = 0;
    Gp_UpdateCoord(&work->field_E3C);

    if (work->field_7B3 == 4 && (frame = work->field_4A & 0x3FF) == 0xC &&
        work->field_7AC != frame) {
        work->field_EAC  = 3;
        work->obj.flags |= 0x8000;
        Gp_SpawnPadLerp(0x30, 0xFF, 8);
        swipeId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200019;
        swipePan = (s8)Gp_GetObjPan(
            (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]);
        SndEvt_EnqueueType6(
            swipeId, swipePan,
            (s8)(Gp_GetObjDepth(
                     (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]) /
                 2));
        swipe2Id  = (((u16)enemy->field_8 >> 12) << 8) | 0x4020001A;
        swipe2Pan = (s8)Gp_GetObjPan(
            (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]);
        SndEvt_EnqueueType6(
            swipe2Id, swipe2Pan,
            (s8)(Gp_GetObjDepth(
                     (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]) /
                 2));
    } else {
        work->obj.flags &= 0x7FFF;
    }

    if (work->field_7B3 == 5 && (frame2 = work->field_72 & 0x3FF) == 0x1C &&
        work->field_7AC != frame2) {
        work->field_EFA = 0;
        work->field_EAC = 3;
        Gp_SpawnPadLerp(0x20, 0x7F, 8);
        hitId  = (((u16)enemy->field_8 >> 12) << 8) | 0x4020001B;
        hitPan = (s8)Gp_GetObjPan(
            (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)(Gp_GetObjDepth(
                     (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]) /
                 2));
    }

    if (work->field_7B3 == 4) {
        work->field_7AC = work->field_4A & 0x3FF;
    } else {
        work->field_7AC = work->field_72 & 0x3FF;
    }

    switch (work->field_6) {
        case 0x14:
            D_actor_403200_80141C58 = 0x640;
            work->field_7A4         = 0;
            break;
        case 0x22:
            work->field_7A4 = 1;
            break;
        case 0x2B:
            work->field_7A4 = 5;
            cueId           = (((u16)enemy->field_8 >> 12) << 8) | 0x40200018;
            cuePan          = (s8)Gp_GetObjPan(
                (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]);
            SndEvt_EnqueueType6(
                cueId, cuePan,
                (s8)(Gp_GetObjDepth(
                         (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]) /
                     2));
            break;
        case 0x2D:
            work->field_7A4 = 2;
            break;
        case 0x38:
            work->field_7A4 = 4;
            break;
        case 0x44:
            work->field_7A4 = 3;
            break;
        case 0xDC:
            work->field_0 = 0xA;
            break;
    }

    if ((u32)((u16)work->field_6 - 0x29) < 6 && D_actor_403200_80141C58 < 0x1770) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 + 0x258;
    }

    func_actor_403200_80133DD8(arg0);

    recs = work->recs2;
    for (k = 0; k < 5; k++) {
        if (recs[k].field_4 == 0) {
            goto missed;
        }
        if ((recs[k].field_4 & 0xFFFF0000) == 0x10000) {
            found = 1;
            goto scanned;
        }
    }
missed:
    found = 0;
scanned:
    if (found != 0 && enemy->field_40 > 0 &&
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&D_actor_403200_8015FA00, 0) == 0) {
        target          = Game_GetPtrSlot(3);
        reply           = Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 4), 0);
        work->field_ECA = reply;
        if (reply == 1) {
            ((GameActor*)task->idMap)->field_956 = 0xA;
        }
        work->field_EB0.field_0 = D_actor_403200_8015E6AC;
        work->field_EC8         = 1;
        work->field_EB0.field_4 = 2;
        work->field_EB0.field_8 = 0;
        work->field_EB0.field_C = 0;
        Gp_DispatchMsg(task, 0x3FF, (s32)&work->field_EB0, 0);
        work->field_7CA = 0;
    }

    if (work->field_6 == 0x3C && work->field_7B3 == 4) {
        work->field_7B3 = 5;
        work->field_7B0 = 1;
    }

    if (work->field_6 >= 0x39) {
        if (D_actor_403200_80141C58 >= 0xBB9) {
            D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        } else {
            D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0x1E;
        }
    }

    if (work->field_6 >= 0x15) {
        work->field_F06 = 4;
    }

    SCRATCH_SP += 0x30;
}

/// State-change reset for the enemy's stand-up, and the height servo that runs
/// on every tick afterwards. The reset half is `func_actor_403200_8013B23C`'s
/// with the buffer allocator on the second walk in place of the release: it
/// clears the host model's flag word, walks the seven escorts pushing that word
/// onto each of their models, allocates the host's and every escort's buffers
/// and then parks the root coordinate at x 0x1068, y 0x7D0, z -0x1770, arming
/// `field_E96` at 0xFA0.
///
/// The servo steps that root y by +0x50 / -0x64 while `field_6` is at or past
/// 0x3D, and by the gentler +0x14 / -0x1E while it is between 0x15 and 0x3D, so
/// the enemy eases back to the ground as it finishes standing up; below 0x15 it
/// stops moving. Which way each step goes is the frame's position inside its
/// group of four -- `frame % 4 < 2` on the `s16` local, whose 16-bit
/// truncation is what puts the `sll 16` / `sra 16` pair in front of the `slti`.
void func_actor_403200_8013D78C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    s16              i;
    s16              j;
    s16              frame;

    work = (Actor403200Work*)arg0->idMap;
    if (work->field_4 != 0) {
        work->field_7B3                    = 1;
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
        ((TmdObject*)arg0->extra)->field_8->coord.t[1] = 0x7D0;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] = 0x1068;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] = -0x1770;
        work->field_E96                                = 0xFA0;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->field_58 & 1) {
        work->field_0 = 1;
    }
    if (((TmdObject*)arg0->extra)->field_8->coord.t[1] > 0) {
        frame = work->field_6;
        if (frame >= 0x3D) {
            ((TmdObject*)arg0->extra)->field_8->coord.t[1] +=
                ((frame % 4) < 2) ? 0x50 : -0x64;
        } else if (frame >= 0x15) {
            ((TmdObject*)arg0->extra)->field_8->coord.t[1] +=
                ((frame % 4) < 2) ? 0x14 : -0x1E;
        }
    }
    if (((TmdObject*)arg0->extra)->field_8->coord.t[1] < 0) {
        ((TmdObject*)arg0->extra)->field_8->coord.t[1] = 0;
    }
    ((TmdObject*)arg0->extra)->field_8->flg = 0;
}

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

/// The state that rains debris on the arena `func_actor_403200_8013D9EC` opens.
///
/// A reset request re-arms the block on animation 0xB, clears the host model's
/// flag word and pushes it onto each of the seven escorts' models, makes sure
/// the host and every escort has its model buffers allocated, and plays the
/// entry cue.
///
/// Sub-states 0x3B and 0x3C each fire a one-shot cue positioned on part 1 of
/// the first escort. From 0x3D on the state also drops debris: every third step
/// the shared scratch coordinate is rebuilt on that same part -- its rotation
/// accumulated up the parent chain, its origin carried into view space, then
/// turned a quarter turn each way so `Gfx_MatrixCol2` yields the launch
/// direction, which is normalised and scaled to 0x320 before being added to the
/// origin -- and an effect is spawned on it. Every tenth step a fresh enemy is
/// spawned from `D_actor_403200_8015E858` and remembered in `field_EF0`.
///
/// The tick then runs the per-frame body and hands over to state 0xA once the
/// second animation slot raises its flag.
void func_actor_403200_8013DC3C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
    SVECTOR          pos;
    SVECTOR*         posp;
    s16              i;
    s16              j;
    s32              resetId;
    s32              resetPan;
    s32              cueId;
    s32              cuePan;
    s32              hitId;
    s32              hitPan;

    work  = (Actor403200Work*)arg0->idMap;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_F1D                    = 7;
        work->field_7B3                    = 0xB;
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
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        work->field_E96 = 0xC80;
        resetId         = (((u16)enemy->field_8 >> 12) << 8) | 0x40200017;
        resetPan        = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }

    if (work->field_6 == 0x3B) {
        cueId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200016;
        cuePan = (s8)Gp_GetObjPan(
            (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]);
        SndEvt_EnqueueType6(
            cueId, cuePan,
            (s8)Gp_GetObjDepth(
                (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]));
    }

    if (work->field_6 == 0x3C) {
        hitId  = (((u16)enemy->field_8 >> 12) << 8) | 0x4020000D;
        hitPan = (s8)Gp_GetObjPan(
            (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)Gp_GetObjDepth(
                (GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]));
    }

    if ((s16)(u16)work->field_6 >= 0x3D) {
        if ((s16)((s16)(u16)work->field_6 % 3) == 0) {
            Actor403200_AccumulateRotation(
                &((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1],
                &D_actor_403200_8015F924);

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            Actor403200_LocalToView(&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1],
                                    &pos);

            D_actor_403200_8015F920.sub        = &Gfx_ViewCoord;
            D_actor_403200_8015F920.coord.t[0] = pos.vx;
            D_actor_403200_8015F920.coord.t[1] = pos.vy;
            D_actor_403200_8015F920.coord.t[2] = pos.vz;
            Gfx_RotMatrixY(&D_actor_403200_8015F920.coord, 0x80, 0);
            Gfx_RotMatrixX(&D_actor_403200_8015F920.coord, -0x80, 0);
            Gfx_MatrixCol2(&D_actor_403200_8015F920.coord, &pos);

            posp   = &pos;
            pos.vy = 0;
            VectorNormalSS(posp, posp);

            gte_lddp(0x320);
            gte_ldsv(posp);
            gte_gpf12_real();
            gte_stsv(posp);

            D_actor_403200_8015F920.coord.t[0] += pos.vx;
            D_actor_403200_8015F920.coord.t[1] += pos.vy;
            D_actor_403200_8015F920.coord.t[2] += pos.vz;
            D_actor_403200_8015F920.flg         = 0;
            Gp_UpdateCoord(&D_actor_403200_8015F920);
            Gp_SpawnEff(0x60199, &D_actor_403200_8015F920, 0x97A0D680, NULL);
        }
        if ((s16)((s16)(u16)work->field_6 % 10) == 4) {
            spawned          = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 2, 0, arg0->spawnArg2);
            spawned->field_A = 0x900;
            work->field_EF0  = spawned;
        }
    }

    func_actor_403200_80133DD8(arg0);

    if (work->field_58 & 1) {
        work->field_0 = 0xA;
        SndEvt_EnqueueType7((((u16)enemy->field_8 >> 12) << 8) | 0x4020000D, 1);
    }

    if (work->field_6 >= 0x15) {
        work->field_F06 = 5;
    }
}

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

/// State-selecting tick of the enemy's approach: on the tick the dispatcher has
/// flagged a state change it re-arms the work block -- the two flags, the
/// stagger countdown at 0x28, the yaw target at 0xE10 and the animation slot at
/// 0x10 -- and winds the shared `D_actor_403200_80141C58` counter down by 0xC8
/// once it has passed 0x190.
///
/// It then runs the per-frame body and aims the enemy at the camera: the
/// camera's translation minus the part's own translation gives the pair
/// `ratan2` turns into a yaw, taken relative to the part's facing the same way
/// the group-0 hit handler does it, and the result is wrapped to +/-0x800 into
/// `field_7C4`. `D_actor_403200_80141C50` holding `field_6` at zero makes the
/// per-frame body's animation re-arm win the next tick.
///
/// Once the `field_F10` stagger countdown has run out it walks the three
/// `field_F08` sub-states, in which the player-relative range and the enemy's
/// remaining HP pick the next state, and a roll of `Gp_LcgState` breaks the tie
/// between the two strafing states; the state already in `field_F1D` is never
/// re-selected twice in a row. A positive heal counter in `field_F1A` overrides
/// all of it with the heal state 0xF.
///
/// The x range that sub-state 0 tests is the player-relative offset read back
/// out of the frame, not `dist`: the two share only the frame, and the y test
/// carries the -0xFA the z one carries +0x25F, the offsets the hit handler puts
/// on the same pair.
void func_actor_403200_8013EB64(Task* arg0)
{
    Actor403200ApproachScratch* sc;
    Actor403200Work*            work;
    GpEnemy*                    enemy;
    Task*                       player;
    GsCOORDINATE2*              coord;
    GsCOORDINATE2*              facing;
    SVECTOR*                    view;
    s16                         angle;

    work   = (Actor403200Work*)arg0->idMap;
    enemy  = arg0->spawnArg2;
    player = Game_GetPtrSlot(3);

    if (work->field_4 != 0) {
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFE = 0;
        if (work->field_F10 == 0) {
            work->field_F10 = 0x28;
        }
        work->field_E96 = 0xE10;
        work->field_7B3 = 1;
        work->field_7B0 = 1;
        work->field_EFA = 0;
        work->field_7B6 = 0x10;
    }
    if (D_actor_403200_80141C58 >= 0x191) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        work->field_7A4         = 0;
    }
    sc = (Actor403200ApproachScratch*)(SCRATCH_SP -= sizeof(Actor403200ApproachScratch));
    func_actor_403200_80133DD8(arg0);

    coord    = ((TmdObject*)arg0->extra)->field_8;
    view     = &sc->view;
    view->vx = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
    view->vy = Wip_SysConfig.field_4->t[1] - coord->coord.t[1];
    view->vz = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
    facing   = ((TmdObject*)arg0->extra)->field_8;
    angle    = ratan2(view->vx, view->vz) -
            ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    work->field_7C4 = angle;
    if (D_actor_403200_80141C50 == 1) {
        work->field_6 = 0;
    }
    if (work->field_F10 <= work->field_6) {
        sc->delta.vx = ((TmdObject*)player->extra)->field_8->coord.t[0] -
                       ((TmdObject*)arg0->extra)->field_8->coord.t[0];
        sc->delta.vy = (((TmdObject*)player->extra)->field_8->coord.t[1] -
                        ((TmdObject*)arg0->extra)->field_8->coord.t[1]) -
                       0xFA;
        sc->delta.vz = (((TmdObject*)player->extra)->field_8->coord.t[2] -
                        ((TmdObject*)arg0->extra)->field_8->coord.t[2]) +
                       0x25F;
        sc->dist = SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vy * sc->delta.vy +
                               sc->delta.vz * sc->delta.vz);
        switch (work->field_F08) {
            case 0:
                if (enemy->field_40 < 0x5DC) {
                    work->field_0 = 9;
                } else if (((TmdObject*)player->extra)->field_8->coord.t[0] -
                               ((TmdObject*)arg0->extra)->field_8->coord.t[0] >=
                           0x2711) {
                    work->field_0 = 2;
                } else if (work->field_F1D != 3) {
                    work->field_0 = 3;
                } else {
                    work->field_0 = 2;
                }
                break;
            case 1:
                if (enemy->field_40 < 0x320) {
                    work->field_0 = 9;
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
                        work->field_0 = 3;
                    } else if (sc->dist >= 0x20D1) {
                        if (work->field_F1D == 6) {
                            work->field_0 = 2;
                        } else {
                            work->field_0 = 6;
                        }
                    } else if (sc->dist >= 0x189D) {
                        if (work->field_F1D == 7) {
                            work->field_0 = 2;
                        } else {
                            work->field_0 = 7;
                        }
                    } else {
                        work->field_0 = 2;
                    }
                }
                break;
            case 2:
                if (sc->dist >= 0x2329) {
                    if (work->field_F1D == 2) {
                        work->field_0 = 6;
                    } else {
                        work->field_0 = 2;
                    }
                } else if (work->field_F1D == 2) {
                    work->field_0 = 0xB;
                } else {
                    work->field_0 = 2;
                }
                break;
        }
        if ((s8)work->field_F1A > 0) {
            work->field_0 = 0xF;
        }
    }
    SCRATCH_SP += sizeof(Actor403200ApproachScratch);
}

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
