#include "common.h"

#include "actors/actor_444000.h"
#include "actors/actor_444000_view.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include <psyq/inline_c.h>

/// `gpf 1`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s16 D_actor_444000_80144A68;
extern s16 D_actor_444000_80144A70;
extern s16 D_actor_444000_80144A72;
extern s32 D_actor_444000_80144A74;
extern s32 D_actor_444000_80144A7C;
extern s32 D_actor_444000_80144A84;
extern s32 D_actor_444000_80144A8C;
extern s32 Gp_LcgState;

/// Camera-target matrix the scene walks the player along; the fight yaws the
/// boss at its translation.
extern MATRIX* D_80073B8C;

/// World point the spinner chases: written by `func_actor_444000_8013E058`,
/// read here as the target of the per-tick step.
extern SVECTOR D_actor_444000_80161890;

/// Shared coordinate `func_actor_444000_80140BBC` rebuilds when the fight
/// reaches sub-state 0x2D of state 9, parented to the host model's fifth part.
extern Actor444000DropCoord D_actor_444000_801618B8;

/// Which of the three shared debris coordinates below the next launch uses,
/// cycled 0/1/2 by `func_actor_444000_801404C0`.
extern s16 D_actor_444000_80161850;
/// The three coordinates that debris effects are spawned on, each rebuilt in
/// view space from the first escort's second part.
extern GsCOORDINATE2 D_actor_444000_80161948[];
/// Spawn table of the enemy the arena fight drops in every tenth step.
extern TaskDesc D_actor_444000_801617DC;

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F30`:
/// allocate its `Actor444000SpinnerWork`, parent the model object to the world
/// coordinate, give it a random orientation off `Gp_LcgState`, point it at its
/// own light and colour matrices and step the task on. Bails to
/// `Gp_DestroyEnemy` when the overlay is shutting down or the allocation fails.
void func_actor_444000_8013A1C4(GpEnemy* enemy, Actor444000Spinner* task)
{
    Actor444000SpinnerWork* work;

    if (D_actor_444000_80144A68 == 1) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    work           = Mem_Calloc(0xA0, 0);
    task->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    task->extra->field_8->sub = &Gfx_ViewCoord;
    task->extra->field_C      = 0;

    switch ((u16)task->spawnArg1) {
        case 0:
            work->spin = 0x14;
            break;
        case 1:
            work->spin = 0x28;
            break;
        case 2:
            work->spin = 0x50;
            break;
        default:
            work->spin = 0x50;
            break;
    }

    task->field_24 = NULL;
    work->field_98 = 0;
    work->field_96 = 0;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixZ(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixX(&task->extra->field_8->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);

    task->extra->field_1C     = &work->lightMtx;
    task->extra->field_20     = &work->colorMtx;
    task->extra->field_8->flg = 0;
    Gp_UpdateCoord(task->extra->field_8);
    func_800D7A9C(task->extra, (VECTOR*)task->extra->field_8->workm.t, 0, 3);
    task->state++;
}

/// Reads and writes the scratchpad head. Written as inlines for the same
/// reason as `Actor444000_ReleaseRotScratch`: only inline-expanded code keeps
/// the absolute `lui $at` form of the scratch-head accesses.
static __inline__ u8* Actor444000_GetScratchHead(void)
{
    return *(u8**)G_SCRATCH_HEAD;
}

static __inline__ void Actor444000_SetScratchHead(void* head)
{
    *(void**)G_SCRATCH_HEAD = head;
}

/// Per-tick state of the spinner enemy. While `spin` is counting down the model
/// only yaws in place -- 0x40 on phase 1 and -0x3C on phase 3 of every four
/// frames -- and nothing else happens. Once it reaches zero the enemy homes on
/// `D_actor_444000_80161890`: the offset from the model root to that point is
/// squared against `field_98` in a `VECTOR3` borrowed off `G_SCRATCH_HEAD`, and
/// the task steps on when the enemy is inside that radius. `field_96` then ties
/// the spin rate to the step count (`field_98 += field_96 / 8`), the offset is
/// normalised and scaled by `field_98` through the GTE's `gpf` interpolator, and
/// the result is added to the root translation before the three rotations are
/// rebuilt from `field_98` and `field_96`. Bails to `Gp_DestroyEnemy` while the
/// overlay is shutting down.
void func_actor_444000_8013A3AC(GpEnemy* enemy, Actor444000Spinner* task)
{
    Actor444000SpinnerWork* work;
    SVECTOR                 step;
    SVECTOR*                stepp;
    VECTOR3*                sq;
    u8*                     head;
    s16                     angle;
    s32                     spin;
    s32                     phase;
    s32                     inside;

    work                      = task->field_1C;
    task->extra->field_8->flg = 0;
    Gp_UpdateCoord(task->extra->field_8);
    func_800D7A9C(task->extra, (VECTOR*)task->extra->field_8->workm.t, 0, 3);

    if (D_actor_444000_80144A68 == 1 || D_actor_444000_80144A72 == 0) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    task->extra->field_C = 0;

    spin = work->spin;
    if (spin != 0) {
        spin--;
        work->spin = spin;
        phase      = work->spin;
        if ((phase & 3) == 1) {
            Gfx_RotMatrixY(&task->extra->field_8->coord, 0x40, 0);
        }
        if ((work->spin & 3) == 3) {
            Gfx_RotMatrixY(&task->extra->field_8->coord, -0x3C, 0);
        }
        task->extra->field_8->flg = 0;
        Gp_UpdateCoord(task->extra->field_8);
        return;
    }

    stepp    = &step;
    *stepp   = D_actor_444000_80161890;
    step.vx -= task->extra->field_8->coord.t[0];
    step.vy -= task->extra->field_8->coord.t[1];
    step.vz -= task->extra->field_8->coord.t[2];

    head = Actor444000_GetScratchHead();
    sq   = (VECTOR3*)(head - sizeof(VECTOR3));
    Actor444000_SetScratchHead(sq);
    angle  = work->field_98;
    sq->vx = step.vx;
    sq->vy = stepp->vz;
    sq->vz = angle;
    sq->vx = sq->vx * sq->vx;
    sq->vy = sq->vy * sq->vy;
    sq->vz = sq->vz * sq->vz;
    Actor444000_SetScratchHead(head);
    inside = sq->vx + sq->vy >= sq->vz;
    if (!inside) {
        task->state++;
    }

    work->field_96++;
    work->field_98 += work->field_96 / 8;
    VectorNormalSS(stepp, stepp);

    gte_lddp((u16)work->field_98);
    gte_ldsv(stepp);
    gte_gpf12_real();
    gte_stsv(stepp);

    task->extra->field_8->coord.t[0] += step.vx;
    task->extra->field_8->coord.t[1] += step.vy;
    task->extra->field_8->coord.t[2] += step.vz;
    task->extra->field_8->flg         = 0;

    Gfx_RotMatrixY(&task->extra->field_8->coord, work->field_98 / 2, 0);
    Gfx_RotMatrixZ(&task->extra->field_8->coord, work->field_98 * 2, 0);
    Gfx_RotMatrixX(&task->extra->field_8->coord, work->field_96, 0);
}

/// Screen-shake driver for the enemy task: `func_actor_444000_80143490` writes a
/// level into `field_EAC`, and a change from the armed level in `field_EAD`
/// starts a shake of 5, 10 or 22 frames -- any other level is ignored. Each tick
/// spends one frame and drives `Display_ClampField126` off the frame counter's
/// low bits, so level 1 alternates 0 / 2, level 2 walks a four-frame 0 / 2 / 3 / 2
/// pattern and level 3 an eight-frame ramp that peaks at 4. The shake clears
/// itself once the counter runs out.
void func_actor_444000_8013A77C(Actor444000* task)
{
    Actor444000Work* work = task->field_1C;
    s32              phase;

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

/// Message 0x7D5 handler, the visibility control the event task drives the
/// boss with: each sub-command sets the host model's flag word and pushes it
/// onto all seven escorts' models, differing in what the flag word becomes and
/// whether the model buffers are (re)allocated first.
///
/// 0 brings the group back with buffers and the 0x80 flag, 1 clears the flag
/// before making sure the buffers exist, 2 raises bit 2 and reports it back
/// through the flag word, and 3 clears the word, pushes the clear, then raises
/// bit 2 on the host alone. The two that end with a cleared flag word also
/// reset the work block's state index.
s32 func_actor_444000_8013A958(Actor444000* task, s32 msgId, s32 arg2)
{
    TmdObject*       tmd;
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* buffers;
    Actor444000Work* rebuilt;
    TmdObject*       hostTmd;
    TmdObject*       escortTmd;
    s32              flags;
    s16              i;
    s16              j;

    tmd  = (TmdObject*)task->extra;
    work = task->field_1C;
    switch (arg2) {
        case 0:
            buffers = task->field_1C;
            if (tmd->field_18 == NULL) {
                Tmd_AllocBuffers(tmd);
            }
            for (j = 0; j < 7; j++) {
                if (buffers->field_ECC[j] != NULL) {
                    escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                    if (escortTmd->field_18 == NULL) {
                        Tmd_AllocBuffers(escortTmd);
                    }
                }
            }
            escorts                            = task->field_1C;
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
            escorts                            = task->field_1C;
            escorts->field_7F3                 = 0;
            ((TmdObject*)task->extra)->field_C = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)task->extra)->field_C;
                }
            }
            hostTmd = (TmdObject*)task->extra;
            rebuilt = task->field_1C;
            if (hostTmd->field_18 == NULL) {
                Tmd_AllocBuffers(hostTmd);
            }
            for (j = 0; j < 7; j++) {
                if (rebuilt->field_ECC[j] != NULL) {
                    escortTmd = (TmdObject*)rebuilt->field_ECC[j]->task->extra;
                    if (escortTmd->field_18 == NULL) {
                        Tmd_AllocBuffers(escortTmd);
                    }
                }
            }
            break;
        case 2:
            tmd->field_C |= 4;
            flags         = tmd->field_C;
            escorts       = task->field_1C;
            if (flags & 4) {
                escorts->field_7F3                 = 3;
                ((TmdObject*)task->extra)->field_C = 0x80;
            } else {
                escorts->field_7F3                 = 0;
                ((TmdObject*)task->extra)->field_C = flags;
            }
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)task->extra)->field_C;
                }
            }
            work->field_0 = 0;
            break;
        case 3:
            tmd->field_C                       = 0;
            escorts                            = task->field_1C;
            escorts->field_7F3                 = 0;
            ((TmdObject*)task->extra)->field_C = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)task->extra)->field_C;
                }
            }
            tmd->field_C |= 4;
            break;
    }
    return 0;
}

/// Rebuilds the host's root coordinate around the yaw it is already facing:
/// `ratan2` of the rotation's Z basis gives the yaw, `Gfx_RotMatrixY` rebuilds
/// the rotation from it, and `ScaleMatrix` widens it to 1.0 / 0.0 / 1.0 so the
/// model flattens vertically. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`, which is handed back before the coordinate is refreshed.
static __inline__ void Actor444000_RebuildRotation(Actor444000* task)
{
    GsCOORDINATE2*         coord = ((TmdObject*)task->extra)->field_8;
    Actor444000RotScratch* sc;
    s16                    ang;

    sc                                       = (Actor444000RotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(Actor444000RotScratch));
    *(Actor444000RotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = 0;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    ((TmdObject*)task->extra)->field_8->flg = 0;
    *(u8**)G_SCRATCH_HEAD                   = *(u8**)G_SCRATCH_HEAD + sizeof(Actor444000RotScratch);
    Gp_UpdateCoord(((TmdObject*)task->extra)->field_8);
}

/// The enemy task's 0x7DB message handler, listed in `D_actor_444000_80161818`.
/// The three payload bytes are always recorded in the work block; only messages
/// from sender 0x2804 act, and then on three of the selector's values. 0 and 1
/// both announce the state change with the same pair of cues, 1 additionally
/// re-arms the animation blocks and drops the model onto its start position,
/// and 19 switches the host and its fourth escort to light mode 2 before
/// raising eight floor vertices and flattening the model's rotation.
s32 func_actor_444000_8013ACD0(Actor444000* task, s32 msgId, Actor444000Msg7DB* msg)
{
    Actor444000Work* work  = task->field_1C;
    GpEnemy*         enemy = task->field_20;
    SVECTOR*         verts;
    s32              action;

    work->field_EC4 = msg->b[0];
    work->field_EC5 = msg->b[1];
    work->field_EC6 = msg->b[2];

    if (msg->h.id == 0x2804) {
        action = msg->h.action;
        switch (action) {
            case 0:
                work->field_0 = 0;
                SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x4020000A, 1);
                SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x4020000D, 1);
                break;

            case 1:
                work->field_7B3 = 0xA;
                work->field_7B0 = 2;
                func_actor_444000_8013441C(task);
                func_actor_444000_8013441C(task);
                func_actor_444000_8013441C(task);
                work->field_7B6 = 1;
                func_actor_444000_8013441C(task);
                work->field_7B6                                = 0x10;
                ((TmdObject*)task->extra)->field_8->coord.t[0] = -0xBB8;
                ((TmdObject*)task->extra)->field_8->coord.t[1] = 0;
                ((TmdObject*)task->extra)->field_8->coord.t[2] = -0x992;
                ((TmdObject*)task->extra)->field_8->flg        = 0;
                work->field_0                                  = 0x11;
                SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x4020000A, 1);
                SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x4020000D, 1);
                break;

            case 19:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                Gp_SetLightMode((GpObj4C*)work->field_ECC[3], 2);
                work->field_0   = action;
                work->field_2   = -1;
                work->field_F04 = 1;
                SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x54280007, 1);

                verts        = Gp_GridParams->field_8;
                verts[24].vy = 0x1F4;
                verts[25].vy = 0x1F4;
                verts[26].vy = 0x320;
                verts[27].vy = 0x320;
                verts[28].vy = 0x1F4;
                verts[29].vy = 0x1F4;
                verts[30].vy = 0x320;
                verts[31].vy = 0x320;

                Actor444000_RebuildRotation(task);
                break;
        }
    }
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_8013AFF8);

/// Applies the single hit collision group 0 took this frame: the first of the
/// group's five `GpRec18` records holding a type-2 attack id wins, and its
/// contact point drives the hit effect `func_actor_444000_80134688` spawns on
/// the group's coordinate. Damage is `Gp_ComputeDamage` of the attack id scaled
/// by the player's distance, quadrupled on a `Gp_RollEnemyChance` critical and
/// doubled again before it is reported, and the leftover `field_4C` bits 0xC
/// tick a second helping off through `Gp_TickObjFlag4`. The scratchpad frame
/// also keeps the yaw from the part's facing to the contact point, wrapped into
/// +/-0x800, which the drive step reads back at 0x2C.
///
/// The record scan is written with labels rather than a `for` loop on purpose:
/// as a real loop, GCC's `find_and_verify_loops` moves the match arm out of
/// line (see DECOMPILATION_LEARNINGS.md, "loop.c relocates a loop block that
/// ends in a jump out"). `mask` / `kind` and the two hoisted pointers then have
/// to be spelled out, since nothing lifts them out of a goto loop.
void func_actor_444000_8013C060(Actor444000* task)
{
    Actor444000HitScratch* sc;
    Actor444000Work*       work;
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

    cfg   = &Wip_SysConfig;
    enemy = task->field_20;
    work  = task->field_1C;
    sc    = (Actor444000HitScratch*)(SCRATCH_SP -= sizeof(Actor444000HitScratch));
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
        func_actor_444000_80134688(work->hits[0].coord, id);
        work->field_E8C = Gp_GetIdParam2(sc->id);
        Gp_GetIdParam0(sc->id);

        sc->delta.vx = cfg->field_4->t[0] - ((TmdObject*)task->extra)->field_8->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->field_4->t[1] - ((TmdObject*)task->extra)->field_8->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->field_4->t[2] - ((TmdObject*)task->extra)->field_8->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);
        if (Gp_RollEnemyChance(enemy, sc->id, 0) != 0) {
            sc->damage *= 4;
        }
        if (sc->damage != 0) {
            sc->rot.vy = 0x190;
            sc->rot.vx = 0;
            sc->rot.vz = 0x1F4;
            Gp_SpawnEff(0x6009C, &((TmdObject*)enemy->task->extra)->field_8[3], 3, &sc->rot);
        }
        ((TmdObject*)task->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)task->extra)->field_8);
        sc->rot.vx = ((TmdObject*)task->extra)->field_8->workm.t[0];
        sc->rot.vy = ((TmdObject*)task->extra)->field_8->workm.t[1];
        sc->rot.vz = ((TmdObject*)task->extra)->field_8->workm.t[2];
        sc->rot.vx = sc->pos.vx - ((TmdObject*)task->extra)->field_8->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)task->extra)->field_8->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)task->extra)->field_8->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)task->extra)->field_8->workm.m[2][0],
                       ((TmdObject*)task->extra)->field_8->workm.m[2][2]);
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

        if (work->field_7B3 != 4) {
            work->field_7C8 = 0;
            work->field_7C4 = 0;
        }
        sc->damage *= 2;
        func_800E2C78((GpObj40*)enemy, sc->id, sc->damage, 0);
        enemy->field_40 -= sc->damage;
        func_800DA6E8(&enemy->node, sc->damage, 0);
    }

    if (enemy->field_4C & 0xC) {
        sc->damage = Gp_TickObjFlag4((GpObj5C*)enemy);
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
        enemy->field_40 -= sc->damage;
    }

    SCRATCH_SP += sizeof(Actor444000HitScratch);
}

/// The hit handler for collision groups 1 and 2 -- the same scan
/// `func_actor_444000_8013C060` runs for group 0, done twice: group 1 first,
/// and group 2 only if nothing landed on group 1. Both scans are written as
/// real `for` loops so `find_and_verify_loops` parks the match arm out of line
/// (see DECOMPILATION_LEARNINGS.md, "loop.c relocates a loop block that ends in
/// a jump out"), which is the opposite of what group 0's handler needed, and
/// the second scan needs its own locals: sharing `recs` / `pos` / `i` with the
/// first gives both loops one pseudo each and the wrong registers.
///
/// A hit takes the boss out of its 3 / 9 states into 8 on attack kind 2, and on
/// kinds 4 and 6 only one time in six. The damage is computed from how far the
/// player is from the host model, quadrupled when `Gp_RollEnemyChance` fires
/// and the boss is not already in one of the five states that ignore it, and
/// zero otherwise; it comes off the host, the fourth escort and the work
/// block's own pool. `sc->angle` is the yaw of the contact point relative to
/// that escort's facing, wrapped to +/-0x800.
void func_actor_444000_8013C4B0(Actor444000* task)
{
    Actor444000HitScratch* sc;
    Actor444000Work*       work;
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
    u16                    roll;

    cfg  = &Wip_SysConfig;
    host = task->field_20;
    work = task->field_1C;
    sc   = (Actor444000HitScratch*)(SCRATCH_SP -= sizeof(Actor444000HitScratch));
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
        coord = work->hits[1].coord;
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
    coord = work->hits[2].coord;
hit:
    func_actor_444000_80134688(coord, id);
    if (sc->id != 0) {
        work->field_E92 = Gp_GetIdParam2(sc->id);
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
                if (state != 3 && state != 9) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    roll        = ((u32)Gp_LcgState >> 16) % 6;
                    if (roll == 0) {
                        work->field_0 = 8;
                        work->field_2 = -1;
                    }
                }
                break;

            case 2:
                state = work->field_0;
                if (state != 3 && state != 9) {
                    work->field_0 = 8;
                    work->field_2 = -1;
                }
                break;
        }

        sc->delta.vx = cfg->field_4->t[0] - ((TmdObject*)task->extra)->field_8->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->field_4->t[1] - ((TmdObject*)task->extra)->field_8->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->field_4->t[2] - ((TmdObject*)task->extra)->field_8->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

        if (Gp_RollEnemyChance(work->field_ECC[3], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
            state != 9 && state != 0xE && state != 0xF) {
            sc->rot.vy = 0;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, ((TmdObject*)work->field_ECC[3]->task->extra)->field_8, 0, &sc->rot);
            if ((s16)work->field_0 != 9) {
                work->field_0 = 8;
                work->field_2 = -1;
            }
            sc->damage *= 4;
        } else {
            sc->damage = 0;
        }

        func_800E2C78((GpObj40*)host, sc->id, sc->damage, 0);
        ((GpObj40*)host)->field_40 -= sc->damage;
        func_800DA6E8(&work->field_ECC[3]->node, sc->damage, 0);
        work->field_F0E                                            -= sc->damage;
        ((TmdObject*)work->field_ECC[3]->task->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)work->field_ECC[3]->task->extra)->field_8);
        sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[3]->task->extra)->field_8->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[3]->task->extra)->field_8->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[3]->task->extra)->field_8->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)task->extra)->field_8->workm.m[2][0],
                       ((TmdObject*)task->extra)->field_8->workm.m[2][2]);
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

        if (work->field_7B3 != 4) {
            work->field_7C8 = 0;
            work->field_7C4 = 0;
        }
    }
out:
    SCRATCH_SP += sizeof(Actor444000HitScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_8013CA60);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_8013D128);

/// Reset/teardown handler: when the work block is asking for a reset, arm the
/// re-spawn sequence and push the host's model flag word onto each of the seven
/// escorts' models. Otherwise run the ordinary re-arm while the sub-state
/// counter is still below 0xA, and once it reaches 2 release the host's and
/// every escort's model buffers.
void func_actor_444000_8013D810(Actor444000* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* dying;
    s16              i;
    s16              j;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        escorts                            = arg0->field_1C;
        work->field_7F3                    = 3;
        ((TmdObject*)arg0->extra)->field_C = 0x80;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)arg0->extra)->field_C;
            }
        }
        work->field_7B3 = 0xA;
        work->field_7B0 = 2;
        work->field_6   = 0;
        work->field_7B6 = 0x10;
        func_actor_444000_8013441C(arg0);
    } else {
        if (work->field_6 < 0xA) {
            func_actor_444000_8013441C(arg0);
        }
        if (work->field_6 == 2) {
            dying = arg0->field_1C;
            Tmd_FreeBuffers((TmdObject*)arg0->extra);
            for (j = 0; j < 7; j++) {
                if (dying->field_ECC[j] != NULL) {
                    Tmd_FreeBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
                }
            }
        }
    }
}

/// Hands the scratchpad frame `Actor444000_FlattenRotation` borrowed back to
/// `G_SCRATCH_HEAD`. Written as an inline like the rotation itself: only
/// inline-expanded code keeps the absolute `lui $at` form of the scratch-head
/// accesses, so a release written straight into the caller does not match.
static __inline__ void Actor444000_ReleaseRotScratch(void)
{
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + sizeof(Actor444000RotScratch);
}

/// Rebuilds one model's root coordinate around the yaw it already faces and
/// flattens it vertically: `ratan2` of the rotation's Z basis gives the yaw,
/// `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// 1.0 / `vy` / 1.0. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`; the caller releases it with
/// `Actor444000_ReleaseRotScratch` once it has cleared the coordinate again.
static __inline__ void Actor444000_FlattenRotation(GsCOORDINATE2* coord, s32 vy)
{
    Actor444000RotScratch* sc;
    s16                    ang;

    sc                                       = (Actor444000RotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(Actor444000RotScratch));
    *(Actor444000RotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = vy;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;
}

/// Re-arm handler run once the block asks for a reset: clear the host's model
/// flag word onto itself and every escort, drop the two counters at 0xEF4, then
/// step the animation on and flatten six of the models -- escorts 2, 4, 3, 0 and
/// 1 plus the host itself -- onto the ground plane. Escort 3 keeps a little
/// height (`vy` 0x400) where the rest are flattened outright. The last release
/// clears escort 2's coordinate flag again rather than escort 1's, which looks
/// like a copy-paste slip in the original but is what the ROM does.
void func_actor_444000_8013D96C(Actor444000* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    s16              i;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        ((TmdObject*)arg0->extra)->field_C = 0;
        escorts                            = arg0->field_1C;
        escorts->field_7F3                 = 0;
        ((TmdObject*)arg0->extra)->field_C = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)arg0->extra)->field_C;
            }
        }
        work->field_EF4 = 0;
        work->field_EF6 = 0;
    }

    func_actor_444000_8013441C(arg0);

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[2]->task->extra)->field_8, 0);
    ((TmdObject*)work->field_ECC[2]->task->extra)->field_8->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)arg0->extra)->field_8, 0);
    ((TmdObject*)arg0->extra)->field_8->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[4]->task->extra)->field_8, 0);
    ((TmdObject*)work->field_ECC[4]->task->extra)->field_8->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[3]->task->extra)->field_8, 0x400);
    ((TmdObject*)work->field_ECC[3]->task->extra)->field_8->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[0]->task->extra)->field_8, 0);
    ((TmdObject*)work->field_ECC[0]->task->extra)->field_8->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[1]->task->extra)->field_8, 0);
    ((TmdObject*)work->field_ECC[2]->task->extra)->field_8->flg = 0;
    Actor444000_ReleaseRotScratch();
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_8013E058);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_8013EC84);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_8013FB74);

/// Per-tick state of the arena fight once it is under way. A reset request
/// re-arms the block on animation 0xB, clears the host model's flag word and
/// pushes it onto each of the seven escorts' models, then plays the entry cue.
///
/// Sub-states 0x3B and 0x3C each fire a one-shot cue positioned at the first
/// escort's second coordinate. From 0x3D on the fight also drops debris: every
/// fifth step one of the three shared coordinates in
/// `D_actor_444000_80161948` is rebuilt at that escort's second part -- its
/// rotation accumulated up the parent chain, its origin carried into view
/// space, then turned a quarter turn each way so `Gfx_MatrixCol2` yields the
/// launch direction, which is normalised and scaled to 0x320 before being
/// added to the origin -- and an effect is spawned on it. Every tenth step a
/// fresh enemy is spawned from `D_actor_444000_801617DC` and remembered in
/// `field_EF0`.
///
/// The tick then runs the ordinary re-arm and hands over to state 0xA once the
/// second animation slot raises its flag.
void func_actor_444000_801404C0(Actor444000* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
    GsCOORDINATE2*   coord;
    SVECTOR          pos;
    SVECTOR*         posp;
    s16              i;
    s32              resetId;
    s32              resetPan;
    s32              cueId;
    s32              cuePan;
    s32              hitId;
    s32              hitPan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;

    if (work->field_4 != 0) {
        work->field_F1D    = 7;
        work->field_7B3    = 0xB;
        work->field_7B0    = 2;
        escorts            = arg0->field_1C;
        escorts->field_7F3 = 0;

        ((TmdObject*)arg0->extra)->field_C = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)arg0->extra)->field_C;
            }
        }
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;

        resetId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200017;
        resetPan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }

    if (work->field_6 == 0x3B) {
        cueId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200016;
        cuePan = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]);
        SndEvt_EnqueueType6(cueId, cuePan,
                            (s8)Gp_GetObjDepth((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]));
    }

    if (work->field_6 == 0x3C) {
        hitId  = (((u16)enemy->field_8 >> 12) << 8) | 0x4020000D;
        hitPan = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]);
        SndEvt_EnqueueType6(hitId, hitPan,
                            (s8)Gp_GetObjDepth((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1]));
    }

    if ((s16)(u16)work->field_6 >= 0x3D) {
        if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
            if (D_actor_444000_80161850 >= 2) {
                D_actor_444000_80161850 = 0;
            } else {
                D_actor_444000_80161850 = (u16)D_actor_444000_80161850 + 1;
            }

            Actor444000_AccumulateRotation(&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1],
                                           &D_actor_444000_80161948[D_actor_444000_80161850].coord);
            D_actor_444000_80161948[D_actor_444000_80161850].sub = &Gfx_ViewCoord;

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            Actor444000_LocalToView(&((TmdObject*)work->field_ECC[0]->task->extra)->field_8[1], &pos);

            D_actor_444000_80161948[D_actor_444000_80161850].coord.t[0] = pos.vx;
            D_actor_444000_80161948[D_actor_444000_80161850].coord.t[1] = pos.vy;
            D_actor_444000_80161948[D_actor_444000_80161850].coord.t[2] = pos.vz;
            Gfx_RotMatrixY(&D_actor_444000_80161948[D_actor_444000_80161850].coord, 0x80, 0);
            Gfx_RotMatrixX(&D_actor_444000_80161948[D_actor_444000_80161850].coord, -0x80, 0);
            Gfx_MatrixCol2(&D_actor_444000_80161948[D_actor_444000_80161850].coord, &pos);

            posp   = &pos;
            pos.vy = 0;
            VectorNormalSS(posp, posp);

            gte_lddp(0x320);
            gte_ldsv(posp);
            gte_gpf12_real();
            gte_stsv(posp);

            coord              = &D_actor_444000_80161948[D_actor_444000_80161850];
            coord->coord.t[0] += pos.vx;
            coord->coord.t[1] += pos.vy;
            coord->coord.t[2] += pos.vz;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            Gp_SpawnEff(0x60196, &D_actor_444000_80161948[D_actor_444000_80161850], 0x27A0D600, NULL);
        }
        if ((s16)((s16)(u16)work->field_6 % 10) == 4) {
            spawned          = Gp_SpawnEnemyFromTable(&D_actor_444000_801617DC, 2, 0, arg0->field_20);
            spawned->field_A = 0x900;
            work->field_EF0  = spawned;
        }
    }

    func_actor_444000_8013441C(arg0);

    if (work->slots0[1].field_10 & 1) {
        work->field_0 = 0xA;
        SndEvt_EnqueueType7((((u16)enemy->field_8 >> 12) << 8) | 0x4020000D, 1);
    }
}

/// Tick of the arena fight state that runs alongside `func_actor_444000_80140E28`:
/// on a reset request it clears the host model's flag word, pushes it onto each
/// of the seven escorts' models, makes sure the host and every escort has its
/// model buffers allocated and restores the normal blend weight.
///
/// The three state checks that follow are independent. In state 0xD the 0x18
/// script is spawned once, on the step the third animation slot first reaches
/// frame 0x15, which `field_7D8` remembers so the spawn does not repeat while
/// the frame is held. In state 9 at sub-state 0x2D the shared coordinate
/// `D_actor_444000_801618B8` is rebuilt as an identity sitting 100 units below
/// and 100 in front of the host model's fifth part, which it is parented to.
/// State 0x14 hands over to state 0xD once the second slot raises its flag.
/// The tick then runs the ordinary re-arm and re-flags the root coordinate for
/// rebuild.
void func_actor_444000_80140BBC(Actor444000* arg0)
{
    Actor444000Work*   work;
    Actor444000Work*   escorts;
    Actor444000Work*   buffers;
    TmdObject*         tmd;
    TmdObject*         escortTmd;
    Actor444000Matrix* mtx;
    GsCOORDINATE2*     coords;
    s16                i;
    s16                j;
    s32                frame;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        escorts                            = arg0->field_1C;
        escorts->field_7F3                 = 0;
        ((TmdObject*)arg0->extra)->field_C = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)arg0->extra)->field_C;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->field_1C;
        if (tmd->field_18 == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->field_18 == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_7B6 = 0x10;
    }
    if (work->field_7B3 == 0xD) {
        frame = work->slots0[2].field_2 & 0x3FF;
        if (frame == 0x15 && work->field_7D8 != frame) {
            work->field_EAC = 3;
            Gp_SpawnScript18((s32)&D_actor_444000_80144A84, (s32)&D_actor_444000_80144A8C);
        }
        work->field_7D8 = work->slots0[2].field_2 & 0x3FF;
    }
    if (work->field_7B3 == 9 && work->field_6 == 0x2D) {
        coords                                = ((TmdObject*)arg0->extra)->field_8;
        D_actor_444000_801618B8.ident.m00_m01 = 0x1000;
        mtx                                   = (Actor444000Matrix*)&D_actor_444000_801618B8.c.coord;
        mtx->ident.m02_m10                    = 0;
        mtx->ident.m11_m12                    = 0x1000;
        mtx->ident.m20_m21                    = 0;
        mtx->ident.m22                        = 0x1000;
        D_actor_444000_801618B8.c.coord.t[1]  = -0x64;
        D_actor_444000_801618B8.c.coord.t[0]  = 0;
        D_actor_444000_801618B8.c.coord.t[2]  = 0x64;
        D_actor_444000_801618B8.c.flg         = 0;
        D_actor_444000_801618B8.c.sub         = &coords[4];
        Gp_UpdateCoord(&D_actor_444000_801618B8.c);
    }
    if (work->field_7B3 == 0x14 && (work->slots0[1].field_10 & 1)) {
        work->field_7B3 = 0xD;
        work->field_7B0 = 1;
    }
    if (D_actor_444000_80144A70 >= 0x191) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    ((TmdObject*)arg0->extra)->field_8->flg = 0;
}

/// Reset handler for the arena fight: on a reset request, clear the host
/// model's flag word, push it onto each of the seven escorts' models, make sure
/// the host and every escort has its model buffers allocated, then fast-forward
/// the animation by running the re-arm step an eighth of `field_F14` times
/// before restoring the normal blend weight and playing the entry cue.
///
/// Either way the tick then runs the ordinary re-arm, re-flags the root
/// coordinate for rebuild, and spawns the 0x18 script once -- on the step the
/// second animation slot first reaches frame 0x1C, which `field_7D8` remembers
/// so the spawn does not repeat while the frame is held.
void func_actor_444000_80140E28(Actor444000* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* buffers;
    GpEnemy*         obj;
    TmdObject*       tmd;
    TmdObject*       escortTmd;
    s16              i;
    s16              j;
    s16              k;
    s32              frame;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                                = arg0->field_20;
        escorts                            = arg0->field_1C;
        escorts->field_7F3                 = 0;
        ((TmdObject*)arg0->extra)->field_C = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->field_C = ((TmdObject*)arg0->extra)->field_C;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->field_1C;
        if (tmd->field_18 == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->field_18 == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_7B6 = 0x7F;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        for (k = 0; k < work->field_F14 / 8; k++) {
            func_actor_444000_8013441C(arg0);
        }
        work->field_7B6 = 0x10;
        SndEvt_EnqueueType7((((u16)obj->field_8 >> 12) << 8) | 0x4020000A, 1);
    }
    if (D_actor_444000_80144A70 >= 0x191) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    ((TmdObject*)arg0->extra)->field_8->flg = 0;
    frame                                   = work->slots0[2].field_2 & 0x3FF;
    if (frame == 0x1C && work->field_7D8 != frame) {
        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_444000_80144A74, (s32)&D_actor_444000_80144A7C);
    }
    work->field_7D8 = work->slots0[2].field_2 & 0x3FF;
}

void func_actor_444000_8014105C(Actor444000* arg0)
{
    Actor444000Work* work;
    GpEnemy*         obj;
    TmdObject*       tmd;
    s32              state;
    s32              id;
    s32              pan;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        tmd               = (TmdObject*)arg0->extra;
        obj               = arg0->field_20;
        obj->node.field_4 = 8;
        tmd->field_C      = 0;
        state             = work->field_7B3;
        work->field_EF4   = 0;
        work->field_EF6   = 0;
        work->field_EFA   = 1;
        if (state != 0xD) {
            work->field_7B0 = 1;
            work->field_7B3 = 0xD;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        id  = (((u16)obj->field_8 >> 12) << 8) | 0x40200004;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        SndEvt_EnqueueType7((((u16)obj->field_8 >> 12) << 8) | 0x4020000D, 1);
        return;
    }
    SCRATCH_SP -= 0xC;
    if (D_actor_444000_80144A70 >= 0x191) {
        work->field_7A4         = 0;
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    if (work->slots0[1].field_10 & 1) {
        work->field_0 = 0xA;
    }
    SCRATCH_SP += 0xC;
}

/// Idle/approach tick of the arena fight: re-arms the block on request, keeps
/// the boss yawed at `D_80073B8C` (the camera-target matrix the player walks
/// along) and then picks the state to run next.
///
/// `field_7C4` is that yaw, relative to the host part's own facing and wrapped
/// into +/-0x800. `field_F10` is a stagger countdown -- while it is positive the
/// tick only spins it down, and the reset arms it to 0x28 if it is not already
/// running.
///
/// The choice is a ladder: `field_F1C` picks state 3 outright, then each attack
/// pattern in `field_F08` has a depth the player has to be past before the
/// fight advances to state 9, the last two only while the boss still has HP in
/// hand. Failing all of those, `field_F1A` picks 0xF and pattern 6 picks 7, and
/// otherwise the distance from the player to a point just in front of the host
/// picks between 3, 7 and 0xB on a coin flip off `Gp_LcgState`.
///
/// `coord` and `facing` are the same coordinate read twice on purpose: the
/// stores into `vec` cut the first read's value, and the second read has to
/// outlive the first `ratan2` call.
void func_actor_444000_801411C8(Actor444000* arg0)
{
    Actor444000HitScratch* sc;
    Actor444000Work*       work;
    GpEnemy*               enemy;
    Task*                  player;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         facing;
    SVECTOR                vec;
    SVECTOR*               d;
    s16                    angle;

    work   = arg0->field_1C;
    player = Game_GetPtrSlot(3);
    enemy  = arg0->field_20;

    if (work->field_4 != 0) {
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFE = 0;
        if (work->field_F10 == 0) {
            work->field_F10 = 0x28;
        }
        work->field_7B3 = 1;
        work->field_7B0 = 1;
        work->field_EFA = 0;
    }

    d      = &vec;
    coord  = ((TmdObject*)arg0->extra)->field_8;
    d->vx  = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy  = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz  = D_80073B8C->t[2] - coord->coord.t[2];
    facing = ((TmdObject*)arg0->extra)->field_8;
    angle  = ratan2(d->vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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

    if (D_actor_444000_80144A70 >= 0x191) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
        work->field_7A4         = 0;
    }
    func_actor_444000_8013441C(arg0);

    if (work->field_F10 > 0) {
        work->field_F10 = work->field_F10 - 1;
        return;
    }
    if (work->field_F1C > 0) {
        work->field_0 = 3;
        return;
    }

    if (work->field_F08 == 0) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 1 && ((TmdObject*)player->extra)->field_8->coord.t[2] < -0x1D4C) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 2) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 3 && ((TmdObject*)player->extra)->field_8->coord.t[2] < -0x30D4) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 4 && ((TmdObject*)player->extra)->field_8->coord.t[2] < -0x3DB8 &&
        enemy->field_40 < 0x9C4) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 5 && ((TmdObject*)player->extra)->field_8->coord.t[2] < -0x4268 &&
        enemy->field_40 < 0x7D0) {
        work->field_0 = 9;
        return;
    }

    if ((s8)work->field_F1A > 0) {
        work->field_0 = 0xF;
        return;
    }
    if (work->field_F08 == 6) {
        work->field_0 = 7;
        return;
    }

    sc           = (Actor444000HitScratch*)(SCRATCH_SP -= sizeof(Actor444000HitScratch));
    sc->delta.vx = ((TmdObject*)player->extra)->field_8->coord.t[0] -
                   ((TmdObject*)arg0->extra)->field_8->coord.t[0] - 0x51F;
    sc->delta.vy = ((TmdObject*)player->extra)->field_8->coord.t[1] -
                   ((TmdObject*)arg0->extra)->field_8->coord.t[1] - 0xFA;
    sc->delta.vz = ((TmdObject*)player->extra)->field_8->coord.t[2] -
                   ((TmdObject*)arg0->extra)->field_8->coord.t[2] + 0x25F;
    sc->dist = SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vy * sc->delta.vy +
                           sc->delta.vz * sc->delta.vz);
    if (sc->dist < 0x2329) {
        if (sc->dist >= 0xED9) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                work->field_0 = 7;
            } else {
                work->field_0 = 3;
            }
        } else {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                work->field_0 = 3;
            } else {
                work->field_0 = 0xB;
            }
        }
    } else {
        work->field_0 = 3;
    }
    SCRATCH_SP += sizeof(Actor444000HitScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_80141618);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_80141DFC);

/// Keeps the player inside the arena: clamps the player model's root
/// translation every tick. `t[1]` (height) is never allowed above 0, and `t[2]`
/// (depth) is capped at 0 in front and -26000 at the back. The `t[2]` ladder
/// then picks the `t[0]` (lateral) corridor for that depth band, so the walls
/// narrow and widen as the player moves through the room.
void func_actor_444000_80142254(void)
{
    Task* player;
    s32   z;

    player = Game_GetPtrSlot(3);

    if (((TmdObject*)player->extra)->field_8->coord.t[1] > 0) {
        ((TmdObject*)player->extra)->field_8->coord.t[1] = 0;
    }

    z = ((TmdObject*)player->extra)->field_8->coord.t[2];
    if (z > 0) {
        ((TmdObject*)player->extra)->field_8->coord.t[2] = 0;
    } else if (z > -1000) {
        if (((TmdObject*)player->extra)->field_8->coord.t[0] < 0) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 0;
        }
        if (((TmdObject*)player->extra)->field_8->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 16500;
        }
    } else if (z > -5000) {
        if (((TmdObject*)player->extra)->field_8->coord.t[0] < 0) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 0;
        }
        if (((TmdObject*)player->extra)->field_8->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 16500;
        }
    } else if (z > -7000) {
        if (((TmdObject*)player->extra)->field_8->coord.t[0] < 9500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 9500;
        }
        if (((TmdObject*)player->extra)->field_8->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 16500;
        }
    } else if (z > -13200) {
        if (((TmdObject*)player->extra)->field_8->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->field_8->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 16500;
        }
    } else if (z > -14750) {
        if (((TmdObject*)player->extra)->field_8->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->field_8->coord.t[0] > 17500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 17500;
        }
    } else if (z > -21250) {
        if (((TmdObject*)player->extra)->field_8->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->field_8->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 16500;
        }
    } else if (z > -22800) {
        if (((TmdObject*)player->extra)->field_8->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->field_8->coord.t[0] > 17500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 17500;
        }
    } else if (z > -26000) {
        if (((TmdObject*)player->extra)->field_8->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->field_8->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->field_8->coord.t[0] = 16500;
        }
    } else {
        ((TmdObject*)player->extra)->field_8->coord.t[2] = -26000;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_801423C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_6", func_actor_444000_80142F28);
