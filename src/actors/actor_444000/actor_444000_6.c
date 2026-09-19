#include "common.h"

#include "actors/actor_444000.h"
#include "actors/actor_444000_view.h"
#include "actors/actors_shared_80132cb8.h"
#include "actors/actors_shared_801433b8.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
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

/// Six-halfword block in the overlay's parameter data -- the same shape repeats
/// at the next label -- whose `[0]` is the damage pool the hit handler for groups
/// 3, 4 and 5 refills `field_F0A` with once it runs out. The aggregate
/// declaration is load-bearing as well as accurate: as a bare scalar the load
/// stops aliasing the neighbouring struct store and GCC fills the load delay
/// with it (see DECOMPILATION_LEARNINGS.md, "Struct-typing a body changes GCC
/// 2.8.1's aliasing").
extern s16 D_actor_444000_80144A3C[];
/// The same six-halfword shape as `D_actor_444000_80144A3C`, one label along:
/// `[0]` refills `field_F0C`, the pool the hit handler for groups 6, 7 and 8
/// draws down. Declared as an aggregate for the same aliasing reason.
extern s16 D_actor_444000_80144A4C[];
extern s16 D_actor_444000_80144A68;

/// Pair descriptors the host and its escorts publish as `GpEnemy::field_50`;
/// `field_4` is the hit-point pool each one starts with.
extern GpPairSrcE D_actor_444000_80144A28;
extern GpPairSrcE D_actor_444000_80144A38;
extern GpPairSrcE D_actor_444000_80144A48;
extern GpPairSrcE D_actor_444000_80144A58;

/// Animation-set tables: the host's two blocks, escort 0's two and escort 1's.
extern GpAnimSet* D_actor_444000_80161448[];
extern GpAnimSet* D_actor_444000_80161500[];
extern GpAnimSet* D_actor_444000_801615B8[];
/// The enemy task's message-handler table, parked in `Task::msgTable`.
extern void* D_actor_444000_80161818;
/// Spawn table of the seven escorts, indexed 0..6.
extern TaskDesc D_actor_444000_801616B0;
/// The enemy task itself, published for the overlay's other files.
extern Actor444000* D_actor_444000_80161878;
/// Effect argument block the spawn state points at the host's root coordinate.
extern GpEffArg D_actor_444000_80161880;
/// Shared 0x7DA payload buffer, also used by `func_actor_444000_80141618`.
extern Actor444000Msg7DA D_actor_444000_80161888;

extern s16 D_actor_444000_80144A70;
extern s16 D_actor_444000_80144A72;
extern s32 D_actor_444000_80144A74;
extern s32 D_actor_444000_80144A7C;
extern s32 D_actor_444000_80144A84;
extern s32 D_actor_444000_80144A8C;
extern s32 Gp_LcgState;

/// The halfword at `D_actor_444000_80161888 + 2` under its own label: the
/// escort-spawn tick reaches the action selector both ways, so both names are
/// declared (see DECOMPILATION_LEARNINGS.md, "A second label on the same run").
extern s16 D_actor_444000_8016188A;
/// Gameplay's escort `TaskDesc` table; entry 3 is the pair this boss spawns.
extern TaskDesc D_80172604;

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

/// The animation-set table the fight installs on the player through message
/// 0x3FF; entry 4 is rebuilt from the player's own weapon block before the
/// second (`field_4 == 4`) send.
extern GpAnimSet* D_actor_444000_80161670[];
/// The companion table used instead when the player is more than a quarter turn
/// off the host's facing, so the hold plays from the other side.
extern GpAnimSet* D_actor_444000_80161680[];
/// Set while the escort-order tick holds the player at a placement of its own;
/// 1 marks the plain re-placement, 0 the full grab.
extern s8 D_actor_444000_80161868;
/// Shared message 0x3E9 placement payload the escort-order tick sends slot 3.
extern Actor444000MsgPos D_actor_444000_80161908;
/// Reply buffer the fight hands message 0x3F8 before asking for the hold.
extern Actor444000Msg3F8 D_actor_444000_80161928;
extern GpAnimBlk*        Gp_PlayerAnimBlkTbl[];
extern u16               Gp_WeaponIdBase[];

/// Non-zero while the controller task is suspended; the arena tick uses it to
/// pick how much of the per-frame work still runs (see
/// `func_actor_444000_801423C4`).
extern u8 D_801153F4;

/// Global game-mode byte; sits inside a small flag block, so it is declared as
/// an array -- the load has to keep aliasing the scratch stores beside it (see
/// DECOMPILATION_LEARNINGS.md, "Declare a fixed-address global as an array").
extern s8 D_8007218B[];
/// Script pair the drag tick spawns every `period` frames.
extern s32 D_actor_444000_80144A94;
extern s32 D_actor_444000_80144AA0;

/// Dispatch-table entry that still lives in assembly further down this file.
void func_actor_444000_80141DFC(Actor444000* arg0);

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

    work           = memCalloc(0xA0, 0);
    task->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    task->extra->coords->sub = &gGfxViewCoord;
    task->extra->flags       = 0;

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
    Gfx_RotMatrixY(&task->extra->coords->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixZ(&task->extra->coords->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixX(&task->extra->coords->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);

    task->extra->lightMtx    = &work->lightMtx;
    task->extra->colorMtx    = &work->colorMtx;
    task->extra->coords->flg = 0;
    Gp_UpdateCoord(task->extra->coords);
    func_800D7A9C(task->extra, (VECTOR*)task->extra->coords->workm.t, 0, 3);
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

    work                     = task->field_1C;
    task->extra->coords->flg = 0;
    Gp_UpdateCoord(task->extra->coords);
    func_800D7A9C(task->extra, (VECTOR*)task->extra->coords->workm.t, 0, 3);

    if (D_actor_444000_80144A68 == 1 || D_actor_444000_80144A72 == 0) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    task->extra->flags = 0;

    spin = work->spin;
    if (spin != 0) {
        spin--;
        work->spin = spin;
        phase      = work->spin;
        if ((phase & 3) == 1) {
            Gfx_RotMatrixY(&task->extra->coords->coord, 0x40, 0);
        }
        if ((work->spin & 3) == 3) {
            Gfx_RotMatrixY(&task->extra->coords->coord, -0x3C, 0);
        }
        task->extra->coords->flg = 0;
        Gp_UpdateCoord(task->extra->coords);
        return;
    }

    stepp    = &step;
    *stepp   = D_actor_444000_80161890;
    step.vx -= task->extra->coords->coord.t[0];
    step.vy -= task->extra->coords->coord.t[1];
    step.vz -= task->extra->coords->coord.t[2];

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

    task->extra->coords->coord.t[0] += step.vx;
    task->extra->coords->coord.t[1] += step.vy;
    task->extra->coords->coord.t[2] += step.vz;
    task->extra->coords->flg         = 0;

    Gfx_RotMatrixY(&task->extra->coords->coord, work->field_98 / 2, 0);
    Gfx_RotMatrixZ(&task->extra->coords->coord, work->field_98 * 2, 0);
    Gfx_RotMatrixX(&task->extra->coords->coord, work->field_96, 0);
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
            if (tmd->buffer == NULL) {
                Tmd_AllocBuffers(tmd);
            }
            for (j = 0; j < 7; j++) {
                if (buffers->field_ECC[j] != NULL) {
                    escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                    if (escortTmd->buffer == NULL) {
                        Tmd_AllocBuffers(escortTmd);
                    }
                }
            }
            escorts                          = task->field_1C;
            escorts->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0x80;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 1:
            escorts                          = task->field_1C;
            escorts->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            hostTmd = (TmdObject*)task->extra;
            rebuilt = task->field_1C;
            if (hostTmd->buffer == NULL) {
                Tmd_AllocBuffers(hostTmd);
            }
            for (j = 0; j < 7; j++) {
                if (rebuilt->field_ECC[j] != NULL) {
                    escortTmd = (TmdObject*)rebuilt->field_ECC[j]->task->extra;
                    if (escortTmd->buffer == NULL) {
                        Tmd_AllocBuffers(escortTmd);
                    }
                }
            }
            break;
        case 2:
            tmd->flags |= 4;
            flags       = tmd->flags;
            escorts     = task->field_1C;
            if (flags & 4) {
                escorts->field_7F3               = 3;
                ((TmdObject*)task->extra)->flags = 0x80;
            } else {
                escorts->field_7F3               = 0;
                ((TmdObject*)task->extra)->flags = flags;
            }
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 3:
            tmd->flags                       = 0;
            escorts                          = task->field_1C;
            escorts->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            tmd->flags |= 4;
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
    GsCOORDINATE2*         coord = ((TmdObject*)task->extra)->coords;
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

    ((TmdObject*)task->extra)->coords->flg = 0;
    *(u8**)G_SCRATCH_HEAD                  = *(u8**)G_SCRATCH_HEAD + sizeof(Actor444000RotScratch);
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
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
                work->field_7B6                               = 0x10;
                ((TmdObject*)task->extra)->coords->coord.t[0] = -0xBB8;
                ((TmdObject*)task->extra)->coords->coord.t[1] = 0;
                ((TmdObject*)task->extra)->coords->coord.t[2] = -0x992;
                ((TmdObject*)task->extra)->coords->flg        = 0;
                work->field_0                                 = 0x11;
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

/// Rebuilds the host's root coordinate from its own facing yaw with a uniform
/// 1.0 scale, marks the model for a rebuild and arms the first state. The
/// matrix lives in a frame taken off `G_SCRATCH_HEAD`, which is handed back
/// once the rotation has been copied out; inlined so each scratch-head access
/// keeps its own `lui` instead of sharing a CSE'd register.
static __inline__ void Actor444000_SeedRootCoord(Actor444000* task, Actor444000Work* work)
{
    GsCOORDINATE2*         coord = ((TmdObject*)task->extra)->coords;
    Actor444000RotScratch* sc;
    s16                    ang;

    sc                                       = (Actor444000RotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(Actor444000RotScratch));
    *(Actor444000RotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = 0x1000;
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

    work->field_0                    = 1;
    ((TmdObject*)task->extra)->flags = 0;
    *(u8**)G_SCRATCH_HEAD            = *(u8**)G_SCRATCH_HEAD + sizeof(Actor444000RotScratch);
}

/// Spawn state of the arena boss: allocate its `Actor444000Work`, wire the host
/// enemy up to the model's root coordinate and its nine collision objects, then
/// spawn the seven escorts that make up the rest of the creature.
///
/// The host takes collision groups 0..2 (group 0's record table doubles as its
/// own `GpEnemy::field_54`), escort 0 takes 3..5 and escort 1 takes 6..8; each
/// group is armed through `func_8010C980` on one of the model's part
/// coordinates. Escorts 2..5 are parented to a part coordinate at a fixed
/// offset and nothing else, and escort 6 is only spawned when the high half of
/// `Task::spawnArg1` is clear. Escort 3 is the one the colour updates treat as
/// the host's twin, so it shares the host's group-1 record table.
///
/// The root coordinate is flattened to its own yaw with a uniform 1.0 scale
/// (`Actor444000_SeedRootCoord`), the tenth collision object at `obj` is linked
/// by hand around the free coordinate `field_E3C`, and both the host and every
/// escort model are pointed at the work block's light and colour matrices
/// before the fight announces itself with message 0x7DA.
void func_actor_444000_8013AFF8(GpEnemy* enemy, Actor444000* task)
{
    Actor444000Work*   work;
    Actor444000Work*   buffers;
    Actor444000Work*   escorts;
    Actor444000Matrix* mtx;
    TmdObject*         tmd;
    TmdObject*         model;
    TmdObject*         escortTmd;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     freeCoord;
    GpEnemy*           esc;
    Task*              escTask;
    SVECTOR            dir;
    SVECTOR*           gteDir;
    VECTOR             pos;
    s16                i;
    s16                j;
    s16                k;

    tmd   = (TmdObject*)task->extra;
    coord = tmd->coords;

    work           = memCalloc(0xF24, 0);
    task->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)task);
        return;
    }

    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    task->exitCallback = ActorsShared801433b8;

    enemy->field_4     = &((TmdObject*)task->extra)->coords->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = -0xC8;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)task->extra)->coords[4];
    Gp_LinkNode(&enemy->node);
    enemy->field_4C = 0;
    enemy->field_40 = D_actor_444000_80144A28.field_4;
    enemy->field_50 = &D_actor_444000_80144A28;
    enemy->field_54 = (s32)work->hits[0].recs;

    func_800B3F84(&work->anim0, D_actor_444000_80161448, tmd, work->aux0, work->slots0);
    func_800B3F84(&work->anim1, D_actor_444000_80161448, tmd, work->aux1, work->slots1);

    work->field_7B0 = 2;
    work->field_7B3 = 2;
    work->field_EF8 = 1;
    work->field_7B1 = 0;
    work->field_7C4 = work->field_7C8 = 0;
    work->field_7B6 = work->field_7B8 = 0x10;

    func_8010C980(&((TmdObject*)task->extra)->coords[4], &work->hits[0].obj, work->hits[0].recs, 5, 0x20, 0x300);
    func_8010C980(&((TmdObject*)task->extra)->coords[4], &work->hits[1].obj, work->hits[1].recs, 5, 0x20, 0x300);
    func_8010C980(&((TmdObject*)task->extra)->coords[1], &work->hits[2].obj, work->hits[2].recs, 5, 0x20, 0xBB8);

    work->hits[1].obj.pos.vx = 0;
    work->hits[1].obj.pos.vy = 0;
    work->hits[1].obj.pos.vz = -0x100;
    work->hits[2].obj.pos.vx = 0;
    work->hits[2].obj.pos.vy = 0x400;
    work->hits[2].obj.pos.vz = -0x400;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &dir);
    dir.vy = 0;
    gteDir = &dir;
    VectorNormalSS(gteDir, gteDir);
    gte_lddp(0x1388);
    gte_ldsv(gteDir);
    gte_gpf12_real();
    gte_stsv(gteDir);

    work->anim.field_0  = NULL;
    work->anim.field_4  = 1;
    work->anim.field_8  = 0;
    work->anim.field_C  = 3;
    work->anim.field_10 = 1;
    work->field_F12     = 0;
    task->field_24      = &D_actor_444000_80161818;
    coord->sub          = &gGfxViewCoord;
    coord->flg          = 0;
    Gp_UpdateCoord(coord);

    D_actor_444000_80161880.coord      = ((TmdObject*)task->extra)->coords;
    D_actor_444000_80161880.spawnArgLo = 0x100;
    D_actor_444000_80161880.spawnArgHi = 2;
    work->field_2                      = -1;

    model   = (TmdObject*)task->extra;
    buffers = task->field_1C;
    if (model->buffer == NULL) {
        Tmd_AllocBuffers(model);
    }
    for (i = 0; i < 7; i++) {
        if (buffers->field_ECC[i] != NULL) {
            escortTmd = (TmdObject*)buffers->field_ECC[i]->task->extra;
            if (escortTmd->buffer == NULL) {
                Tmd_AllocBuffers(escortTmd);
            }
        }
    }

    Actor444000_SeedRootCoord(task, work);

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 0, 0, task->field_20);
    work->field_ECC[0]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = ((TmdObject*)task->extra)->coords;
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->coord.t[2] = 0;
    ((TmdObject*)work->field_ECC[0]->task->extra)->flags              = 0;
    func_800B3F84(&work->anim2, D_actor_444000_80161500, work->field_ECC[0]->task->extra, work->aux2,
                  work->slots2);
    func_800B3F84(&work->anim3, D_actor_444000_80161500, work->field_ECC[0]->task->extra, work->aux3,
                  work->slots3);
    work->field_ECC[0]->field_4     = &((TmdObject*)task->extra)->coords->coord;
    work->field_ECC[0]->field_48    = 0;
    work->field_ECC[0]->field_1C.vx = 0xC8;
    work->field_ECC[0]->field_1C.vy = 0;
    work->field_ECC[0]->field_1C.vz = 0x3E8;
    work->field_ECC[0]->field_18    = &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1];
    Gp_LinkNode(&work->field_ECC[0]->node);
    work->field_ECC[0]->field_4C = 0;
    work->field_ECC[0]->field_40 = D_actor_444000_80144A28.field_4;
    work->field_F0A              = D_actor_444000_80144A38.field_4;
    work->field_ECC[0]->field_50 = &D_actor_444000_80144A38;
    work->field_ECC[0]->field_54 = (s32)work->hits[3].recs;
    func_8010C980(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], &work->hits[3].obj, work->hits[3].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[2], &work->hits[4].obj, work->hits[4].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[3], &work->hits[5].obj, work->hits[5].recs, 5,
                  0x20, 0x300);

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 1, 0, task->field_20);
    work->field_ECC[1]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = ((TmdObject*)task->extra)->coords;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->coord.t[2] = 0;
    ((TmdObject*)work->field_ECC[1]->task->extra)->flags              = 0;
    func_800B3F84(&work->anim4, D_actor_444000_801615B8, work->field_ECC[1]->task->extra, work->aux4,
                  work->slots4);
    func_800B3F84(&work->anim5, D_actor_444000_801615B8, work->field_ECC[1]->task->extra, work->aux5,
                  work->slots5);
    work->field_ECC[1]->field_4     = &((TmdObject*)task->extra)->coords->coord;
    work->field_ECC[1]->field_48    = 0;
    work->field_ECC[1]->field_1C.vx = -0xC8;
    work->field_ECC[1]->field_1C.vy = 0;
    work->field_ECC[1]->field_1C.vz = 0x3E8;
    work->field_ECC[1]->field_18    = &((TmdObject*)work->field_ECC[1]->task->extra)->coords[1];
    Gp_LinkNode(&work->field_ECC[1]->node);
    work->field_ECC[1]->field_4C = 0;
    work->field_ECC[1]->field_40 = D_actor_444000_80144A28.field_4;
    work->field_F0C              = D_actor_444000_80144A48.field_4;
    work->field_ECC[1]->field_50 = &D_actor_444000_80144A48;
    work->field_ECC[1]->field_54 = (s32)work->hits[6].recs;
    func_8010C980(&((TmdObject*)work->field_ECC[1]->task->extra)->coords[1], &work->hits[6].obj, work->hits[6].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[1]->task->extra)->coords[2], &work->hits[7].obj, work->hits[7].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[1]->task->extra)->coords[3], &work->hits[8].obj, work->hits[8].recs, 5,
                  0x20, 0x300);

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 2, 0, task->field_20);
    work->field_ECC[2]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[4];
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] = 0x59;
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[2] = -0x64;
    ((TmdObject*)work->field_ECC[2]->task->extra)->flags              = 0;

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 3, 0, task->field_20);
    work->field_ECC[3]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[3];
    ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[2] = 0;
    ((TmdObject*)work->field_ECC[3]->task->extra)->flags              = 0;
    work->field_ECC[3]->field_4                                       = &((TmdObject*)task->extra)->coords->coord;
    work->field_ECC[3]->field_48                                      = 0;
    work->field_ECC[3]->field_1C.vx                                   = 0;
    work->field_ECC[3]->field_1C.vy                                   = 0;
    work->field_ECC[3]->field_1C.vz                                   = 0x514;
    work->field_ECC[3]->field_18                                      = ((TmdObject*)work->field_ECC[3]->task->extra)->coords;
    Gp_LinkNode(&work->field_ECC[3]->node);
    work->field_ECC[3]->field_4C = 0;
    work->field_ECC[3]->field_40 = D_actor_444000_80144A28.field_4;
    work->field_F0E              = D_actor_444000_80144A58.field_4;
    work->field_ECC[3]->field_50 = &D_actor_444000_80144A58;
    work->field_ECC[3]->field_54 = (s32)work->hits[1].recs;

    freeCoord                     = &work->field_E3C.c;
    work->field_E3C.c.sub         = ((TmdObject*)task->extra)->coords;
    work->field_E3C.ident.m00_m01 = 0x1000;
    mtx                           = (Actor444000Matrix*)&work->field_E3C.c.coord;
    mtx->ident.m02_m10            = 0;
    mtx->ident.m11_m12            = 0x1000;
    mtx->ident.m20_m21            = 0;
    mtx->ident.m22                = 0x1000;
    work->field_E3C.c.coord.t[0] = work->field_E3C.c.coord.t[1] = work->field_E3C.c.coord.t[2] = 0;
    work->field_E3C.c.flg                                                                      = 0;
    Gp_UpdateCoord(freeCoord);

    work->d4rec.field_C  = 0x1B58;
    work->d4rec.field_10 = 0x258;
    work->d4rec.field_12 = 0x258;
    work->d4rec.field_0  = 0;
    work->d4rec.field_2  = 0;
    work->d4rec.field_4  = 0;
    work->d4rec.field_8  = 0;
    work->d4rec.field_A  = 0;
    work->d4rec.field_14 = work->recs2;
    work->obj.coord      = freeCoord;
    work->obj.ctx.d4rec  = &work->d4rec;
    work->obj.pos.vx     = 0;
    work->obj.pos.vy     = -0xFA;
    work->obj.pos.vz     = 0x25F;
    work->obj.key        = 0x30000 | 0x20;
    work->obj.radius     = 0;
    work->obj.flags      = 3;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(work->recs2, 5, 0);
    work->obj.flags &= 0x7FFF;

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 4, 0, task->field_20);
    work->field_ECC[4]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[4];
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[2] = 0x14;
    ((TmdObject*)work->field_ECC[4]->task->extra)->flags              = 0;

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 5, 0, task->field_20);
    work->field_ECC[5]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[2];
    ((TmdObject*)work->field_ECC[5]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[5]->task->extra)->coords->coord.t[1] = 0x67C;
    ((TmdObject*)work->field_ECC[5]->task->extra)->coords->coord.t[2] = 0xC8;
    ((TmdObject*)work->field_ECC[5]->task->extra)->flags              = 0;

    if (task->field_36 == 0) {
        esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_444000_801616B0, 6, 0, task->field_20);
        work->field_ECC[6]                                                = esc;
        ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[1];
        ((TmdObject*)work->field_ECC[6]->task->extra)->coords->coord.t[0] = 0;
        ((TmdObject*)work->field_ECC[6]->task->extra)->coords->coord.t[1] = 0x62C;
        ((TmdObject*)work->field_ECC[6]->task->extra)->coords->coord.t[2] = 0x5DC;
        ((TmdObject*)work->field_ECC[6]->task->extra)->flags              = 0;
    } else {
        work->field_ECC[6] = NULL;
    }

    work->field_F04 = 0;
    work->field_F06 = 0;
    work->field_F08 = 0;
    work->field_F0C = D_actor_444000_80144A4C[0];
    work->field_F0A = D_actor_444000_80144A3C[0];

    escorts = task->field_1C;

    D_actor_444000_80144A68 = 0;

    ((TmdObject*)task->extra)->lightMtx = &escorts->lightMtx;
    ((TmdObject*)task->extra)->colorMtx = &escorts->colorMtx;
    for (j = 0; j < 7; j++) {
        esc = escorts->field_ECC[j];
        if (esc != NULL) {
            escTask                                = esc->task;
            ((TmdObject*)escTask->extra)->lightMtx = &escorts->lightMtx;
            ((TmdObject*)escTask->extra)->colorMtx = &escorts->colorMtx;
        }
    }

    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    if (work->field_EFA != work->field_EFC) {
        Gp_UpdateActorColor(enemy, &pos, 0, 0);
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
        work->field_EFC = work->field_EFA;
    }
    if (work->field_EFA != 0) {
        Gp_UpdateActorColor(enemy, &pos, 0, 0);
    } else {
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
    }
    func_actor_444000_8013441C(task);

    D_actor_444000_80161888.field_0 = 0;
    D_actor_444000_80161888.field_1 = 0x2C;
    D_actor_444000_80161888.field_2 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);

    work->field_E94 = work->field_E96 = 0xFA0;
    for (k = 0; k < 2; k++) {
        work->field_EE8[k] = NULL;
    }

    Gp_StateF0.field_6 = 0xA;
    Gp_ReleaseStateF0Add((GpObj20E*)task, 0x20);
    D_actor_444000_80161878 = task;
    work->field_F1B = work->field_F1C = 0;
    task->state                      += 1;
}

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
    PlayerStatus*          cfg;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    i;

    cfg   = &Player_Status;
    enemy = task->field_20;
    work  = task->field_1C;
    sc    = (Actor444000HitScratch*)(SCRATCH_SP -= sizeof(Actor444000HitScratch));
    pos   = &sc->pos;
    recs  = work->hits[0].recs;
    i     = 0;
    mask  = 0xFFFF0000;
    kind  = 0x20000;
scan:
    if (recs[i].key == 0) {
        goto missed;
    }
    if ((recs[i].key & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key;
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
        func_actor_444000_80134688(work->hits[0].obj.coord, id);
        work->field_E8C = Gp_GetIdParam2(sc->id);
        Gp_GetIdParam0(sc->id);

        sc->delta.vx = cfg->coordMtx->t[0] - ((TmdObject*)task->extra)->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - ((TmdObject*)task->extra)->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - ((TmdObject*)task->extra)->coords->coord.t[2];
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
            Gp_SpawnEff(0x6009C, &((TmdObject*)enemy->task->extra)->coords[3], 3, &sc->rot);
        }
        ((TmdObject*)task->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
        sc->rot.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
        sc->rot.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
        sc->rot.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
        sc->rot.vx = sc->pos.vx - ((TmdObject*)task->extra)->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)task->extra)->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)task->extra)->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)task->extra)->coords->workm.m[2][0],
                       ((TmdObject*)task->extra)->coords->workm.m[2][2]);
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
    PlayerStatus*          cfg;
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

    cfg  = &Player_Status;
    host = task->field_20;
    work = task->field_1C;
    sc   = (Actor444000HitScratch*)(SCRATCH_SP -= sizeof(Actor444000HitScratch));
    pos  = &sc->pos;
    recs = work->hits[1].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[1].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[2].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
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
    coord = work->hits[2].obj.coord;
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

        sc->delta.vx = cfg->coordMtx->t[0] - ((TmdObject*)task->extra)->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - ((TmdObject*)task->extra)->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - ((TmdObject*)task->extra)->coords->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

        if (Gp_RollEnemyChance(work->field_ECC[3], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
            state != 9 && state != 0xE && state != 0xF) {
            sc->rot.vy = 0;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, ((TmdObject*)work->field_ECC[3]->task->extra)->coords, 0, &sc->rot);
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
        work->field_F0E                                           -= sc->damage;
        ((TmdObject*)work->field_ECC[3]->task->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)work->field_ECC[3]->task->extra)->coords);
        sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[3]->task->extra)->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[3]->task->extra)->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[3]->task->extra)->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)task->extra)->coords->workm.m[2][0],
                       ((TmdObject*)task->extra)->coords->workm.m[2][2]);
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

/// The hit handler for collision groups 3, 4 and 5 -- `func_actor_444000_8013C4B0`
/// done three times, the next group only scanned when the previous one landed
/// nothing and the part it hit reported no attack id back. Unlike groups 1 and 2
/// this one runs no `Gp_GetIdParam0` switch: the call is made and its kind
/// thrown away, so every hit is treated alike.
///
/// The damage is the distance-scaled hit quadrupled when `Gp_RollEnemyChance`
/// fires, then divided by six (never down to zero unless it already was), and
/// comes off the host, the first escort and `field_F0A`. Emptying that pool
/// spawns the same effect again and refills it from
/// `D_actor_444000_80144A3C`. Both effect spawns and the state change to 0xE
/// are skipped while the boss is in one of the seven states that ignore hits,
/// while `field_F08` is clear, or while the player hold is armed.
void func_actor_444000_8013CA60(Actor444000* task)
{
    Actor444000HitScratch* sc;
    Actor444000Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GsCOORDINATE2*         coord;
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

    cfg  = &Player_Status;
    host = task->field_20;
    work = task->field_1C;
    sc   = (Actor444000HitScratch*)(SCRATCH_SP -= sizeof(Actor444000HitScratch));
    pos  = &sc->pos;
    recs = work->hits[3].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[3].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[4].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[4].obj.coord;
    hit:
        func_actor_444000_80134688(coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[5].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key == 0) {
            goto missed3;
        }
        if ((recs3[i3].key & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key;
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
    func_actor_444000_80134688(work->hits[5].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    work->field_E8E = Gp_GetIdParam2(sc->id);
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - ((TmdObject*)task->extra)->coords->coord.t[0]) + 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - ((TmdObject*)task->extra)->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - ((TmdObject*)task->extra)->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[0], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 &&
        work->field_EC8 != 1) {
        sc->rot.vz = 0x1F4;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], 0, &sc->rot);
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
    ((GpObj40*)host)->field_40 -= sc->damage;
    work->field_F0A            -= sc->damage;
    if (work->field_F0A <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 && work->field_EC8 != 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0A = D_actor_444000_80144A3C[0];
    }

    func_800DA6E8(&work->field_ECC[0]->node, sc->damage, 0);
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)work->field_ECC[0]->task->extra)->coords);
    sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-((TmdObject*)task->extra)->coords->workm.m[2][0],
                   ((TmdObject*)task->extra)->coords->workm.m[2][2]);
    sc->angle = angle;
    if (angle < 0) {
    wrapUp3:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp3;
        }
    } else {
    wrapDown3:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown3;
        }
    }
    sc->angle = angle;

    if (work->field_7B3 != 4) {
        work->field_7C8 = 0;
        work->field_7C4 = 0;
    }
out:
    SCRATCH_SP += sizeof(Actor444000HitScratch);
}

/// The hit handler for collision groups 6, 7 and 8 -- the same three-scan shape
/// as `func_actor_444000_8013CA60` runs for groups 3, 4 and 5, with the next
/// group only scanned when the previous one landed nothing and the part it hit
/// reported no attack id back. `Gp_GetIdParam0` is called and its kind thrown
/// away here too.
///
/// Damage is the distance-scaled hit -- measured from an offset point rather
/// than the model origin -- quadrupled when `Gp_RollEnemyChance` fires, then
/// divided by six (never down to zero unless it already was), and comes off the
/// host, the second escort and `field_F0C`. Emptying that pool spawns the same
/// effect again and refills it from `D_actor_444000_80144A4C`. Both effect
/// spawns and the state change to 0xE are skipped while the boss is in one of
/// the seven states that ignore hits, while `field_F08` is clear, or while the
/// player hold is armed.
///
/// The second escort carries the damage and the effect, but `sc->angle` is the
/// yaw of the contact point relative to the *first* escort's facing.
void func_actor_444000_8013D128(Actor444000* task)
{
    Actor444000HitScratch* sc;
    Actor444000Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GsCOORDINATE2*         coord;
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

    cfg  = &Player_Status;
    host = task->field_20;
    work = task->field_1C;
    sc   = (Actor444000HitScratch*)(SCRATCH_SP -= sizeof(Actor444000HitScratch));
    pos  = &sc->pos;
    recs = work->hits[6].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[6].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[7].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[7].obj.coord;
    hit:
        func_actor_444000_80134688(coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[8].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key == 0) {
            goto missed3;
        }
        if ((recs3[i3].key & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key;
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
    func_actor_444000_80134688(work->hits[8].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    work->field_E90 = Gp_GetIdParam2(sc->id);
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - ((TmdObject*)task->extra)->coords->coord.t[0]) - 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - ((TmdObject*)task->extra)->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - ((TmdObject*)task->extra)->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[1], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 &&
        work->field_EC8 != 1) {
        sc->rot.vz = 0x3E8;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[1]->task->extra)->coords[1], 0, &sc->rot);
        sc->damage     *= 4;
        work->field_0   = 0xE;
        work->field_F0C = D_actor_444000_80144A4C[0];
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
    func_800DA6E8(&work->field_ECC[1]->node, sc->damage, 0);
    ((GpObj40*)host)->field_40 -= sc->damage;
    work->field_F0C            -= sc->damage;
    if (work->field_F0C <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 && work->field_EC8 != 1) {
        sc->rot.vz = 0x3E8;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[1]->task->extra)->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0C = D_actor_444000_80144A4C[0];
    }

    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)work->field_ECC[1]->task->extra)->coords);
    sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-((TmdObject*)task->extra)->coords->workm.m[2][0],
                   ((TmdObject*)task->extra)->coords->workm.m[2][2]);
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
out:
    SCRATCH_SP += sizeof(Actor444000HitScratch);
}

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
        escorts                          = arg0->field_1C;
        work->field_7F3                  = 3;
        ((TmdObject*)arg0->extra)->flags = 0x80;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
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
        ((TmdObject*)arg0->extra)->flags = 0;
        escorts                          = arg0->field_1C;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        work->field_EF4 = 0;
        work->field_EF6 = 0;
    }

    func_actor_444000_8013441C(arg0);

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[2]->task->extra)->coords, 0);
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)arg0->extra)->coords, 0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[4]->task->extra)->coords, 0);
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[3]->task->extra)->coords, 0x400);
    ((TmdObject*)work->field_ECC[3]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[0]->task->extra)->coords, 0);
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(((TmdObject*)work->field_ECC[1]->task->extra)->coords, 0);
    ((TmdObject*)work->field_ECC[2]->task->extra)->coords->flg = 0;
    Actor444000_ReleaseRotScratch();
}

/// Drag tick of the arena fight: the state the boss runs while it is hauling the
/// player in along the line between them.
///
/// A reset request (`field_4`) re-arms the block on animation 3, clears the host
/// model's flag word and pushes it onto each of the seven escorts' models, makes
/// sure the host and every escort has its model buffers allocated, re-seeds the
/// spinner target `D_actor_444000_80161890` from the fourth part of slot 4's
/// model and announces sub-state 2 through message 0x7DA.
///
/// Every tick then pins the player down to the arena floor, runs the ordinary
/// re-arm and stows the yaw from the host to the player -- relative to the
/// host's own facing, wrapped to +/-0x800 -- in `field_7C4`. The host's fifth
/// part is carried into view space, the player-relative offset from there gives
/// the direction and distance the pull works along, and the animation frame
/// picks how hard: `pull` is the phase's base strength and the frame divides
/// `-(pull + 0x19)` by 1, 2, 3, 4, 6 or 2/3 before `gte_gpf12` scales the
/// normalised direction by it. Frames outside 9..20 drop the pull and clear
/// `field_EFA`. `func_80105B74` hands the result to the player actor unless the
/// game is in mode 2 or 0xA or the player is already in mode 2.
///
/// Alongside that: a script fires every `period` frames while the frame sits in
/// 0xA..0x12, two cues play on frames 0x3C and 0xE8, the fight asks slot 3 for
/// the hold (message 0x3F8) once the player is inside 0x4B0 on frames 0xB..0xF
/// and phase 6 onward clamps the player back behind -0x52D0. Once `slots0[1]`
/// raises its flag the fight announces sub-state 3, moves to state 0xA and drops
/// its two spawned escorts.
void func_actor_444000_8013E058(Actor444000* task)
{
    Actor444000Work*        work  = task->field_1C;
    GpEnemy*                enemy = task->field_20;
    GpActorWork*            slot3;
    GameActor*              actor;
    Actor444000DragScratch* sc;
    Actor444000Work*        escorts;
    Actor444000Work*        buffers;
    PlayerStatus*           cfg;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    GsCOORDINATE2*          yawCoord;
    GsCOORDINATE2*          clamp;
    SVECTOR*                posp;
    SVECTOR*                dirp;
    TmdObject*              tmd;
    TmdObject*              escortTmd;
    s16                     angle;
    s16                     i;
    s16                     j;
    s16                     dz;

    slot3 = (GpActorWork*)Game_GetPtrSlot(3);
    sc    = (Actor444000DragScratch*)(SCRATCH_SP -= sizeof(Actor444000DragScratch));
    actor = slot3->actor;

    if (work->field_4 != 0) {
        work->field_7B3                  = 3;
        work->field_7B0                  = 2;
        escorts                          = task->field_1C;
        escorts->field_7F3               = 0;
        ((TmdObject*)task->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
            }
        }
        tmd     = (TmdObject*)task->extra;
        buffers = task->field_1C;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_EFE = 0;
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        posp            = &D_actor_444000_80161890;
        posp->vz        = 0;
        posp->vy        = 0;
        posp->vx        = 0;
        Actor444000_LocalToView(&((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords[3], posp);
        D_actor_444000_80161888.field_0 = 0;
        D_actor_444000_80161888.field_1 = 0x2C;
        D_actor_444000_80161888.field_2 = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);
    }

    coord = slot3->extra->coords;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1]         = 0;
        slot3->extra->coords->flg = 0;
    }
    func_actor_444000_8013441C(task);

    cfg        = &Player_Status;
    facing     = ((TmdObject*)task->extra)->coords;
    dirp       = &sc->dir;
    sc->dir.vx = *(u16*)&cfg->coordMtx->t[0] - *(u16*)&facing->coord.t[0];
    dirp->vy   = *(u16*)&cfg->coordMtx->t[1] - *(u16*)&facing->coord.t[1];
    dz         = *(u16*)&cfg->coordMtx->t[2] - *(u16*)&facing->coord.t[2];
    dirp->vz   = dz;
    yawCoord   = ((TmdObject*)task->extra)->coords;
    angle      = ratan2(sc->dir.vx, dz) - ratan2(-yawCoord->coord.m[2][0], yawCoord->coord.m[2][2]);
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

    sc->dir.vz = 0;
    sc->dir.vy = 0;
    sc->dir.vx = 0;
    Actor444000_LocalToView(&((TmdObject*)task->extra)->coords[4], &sc->dir);

    sc->dir.vx = *(u16*)&slot3->extra->coords->coord.t[0] - (u16)sc->dir.vx;
    sc->dir.vy = *(u16*)&slot3->extra->coords->coord.t[1] - (u16)sc->dir.vy;
    sc->dir.vz = *(u16*)&slot3->extra->coords->coord.t[2] - (u16)sc->dir.vz;
    sc->dist   = sc->dir.vx * sc->dir.vx;
    sc->dist  += sc->dir.vz * sc->dir.vz;
    sc->dist   = SquareRoot0(sc->dist);
    VectorNormalSS(&sc->dir, &sc->dir);

    switch (work->field_F08) {
        case 0:
            sc->period = 0x19;
            break;
        case 1:
            sc->period = 0x11;
            break;
        case 2:
        default:
            sc->period = 0xE;
            break;
    }
    if (((u32)((work->slots0[1].curRec & 0x3FF) - 0xA) < 9U) && ((work->field_6 % sc->period) == 0)) {
        Gp_SpawnScript18((s32)&D_actor_444000_80144A94, (s32)&D_actor_444000_80144AA0);
    }

    switch (work->field_F08) {
        case 0:
        case 6:
            sc->pull = 0;
            break;
        case 1:
            sc->pull = 5;
            break;
        case 2:
            sc->pull = 0xA;
            break;
        case 3:
        case 4:
        case 5:
        default:
            sc->pull = 0xF;
            break;
    }

    if (work->field_6 == 0x3C) {
        s32 id;
        s32 pan;

        id  = (((u16)enemy->field_8 >> 12) << 8) | 0x4020000A;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords));
    }
    if (work->field_6 == 0xE8) {
        SndEvt_EnqueueType7((((u16)enemy->field_8 >> 12) << 8) | 0x4020000A, 1);
    }

    work->field_EFA = 1;
    switch (work->slots0[1].curRec & 0x3FF) {
        case 9:
            gte_lddp(-(sc->pull + 0x19) / 4);
            gte_ldsv(&sc->dir);
            gte_gpf12_real();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x180;
            break;
        case 10:
            gte_lddp(-(sc->pull + 0x19) / 2);
            gte_ldsv(&sc->dir);
            gte_gpf12_real();
            gte_stsv(&sc->dir);
            break;
        case 11:
        case 13:
        case 15:
            gte_lddp(-(sc->pull + 0x19));
            gte_ldsv(&sc->dir);
            gte_gpf12_real();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x2B2;
            break;
        case 12:
        case 14:
            gte_lddp(-((sc->pull + 0x19) * 3) / 2);
            gte_ldsv(&sc->dir);
            gte_gpf12_real();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x500;
            break;
        case 16:
            gte_lddp(-(sc->pull + 0x19) / 3);
            gte_ldsv(&sc->dir);
            gte_gpf12_real();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x100;
            break;
        case 17:
        case 18:
            gte_lddp(-(sc->pull + 0x19) / 3);
            gte_ldsv(&sc->dir);
            gte_gpf12_real();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x400;
            break;
        case 19:
        case 20:
            sc->dir.vz = 0;
            sc->dir.vx = 0;
            gte_lddp(-(sc->pull + 0x19) / 6);
            gte_ldsv(&sc->dir);
            gte_gpf12_real();
            gte_stsv(&sc->dir);
            work->field_EFE = 0;
            break;
        default:
            work->field_EFA = 0;
            sc->dir.vz      = 0;
            sc->dir.vx      = 0;
            break;
    }

    if (((u32)((work->slots0[1].curRec & 0x3FF) - 0xB) < 5U) && (sc->dist < 0x4B0) && (work->field_F08 < 6)) {
        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&D_actor_444000_80161928, 0) == 0) {
            work->field_0   = 0xD;
            work->field_EC8 = 1;
        }
    }
    if (work->field_F08 >= 6) {
        clamp = slot3->extra->coords;
        if (clamp->coord.t[2] > -0x52D0) {
            clamp->coord.t[2] = -0x52D0;
        }
    }

    if (sc->dir.vx != 0 || sc->dir.vz != 0) {
        sc->push.vx = sc->dir.vx;
        sc->push.vy = 0;
        sc->push.vz = sc->dir.vz;
        if (D_8007218B[0] != 2 && D_8007218B[0] != 0xA && actor->field_954 != 2) {
            func_80105B74(&sc->push);
        }
    }

    if (work->slots0[1].flags & 1) {
        D_actor_444000_80161888.field_0 = 0;
        D_actor_444000_80161888.field_1 = 0x2C;
        D_actor_444000_80161888.field_2 = 3;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);
        work->field_0 = 0xA;
        for (sc->i = 0; sc->i < 2; sc->i++) {
            work->field_EE8[sc->i] = NULL;
        }
    }
    work->field_F1C = 0;
    SCRATCH_SP     += sizeof(Actor444000DragScratch);
}

/// Escort-order tick of the arena fight: the state the boss runs while it has
/// the player pinned in front of it.
///
/// A reset request raises the eight arena floor vertices `Gp_GridParams` keeps
/// at 24..31, re-arms the block on animation 0xF, clears the host model's flag
/// word and pushes it onto each of the seven escorts' models, makes sure the
/// host and every escort has its model buffers allocated, and -- if the player
/// has drifted inside 0xB54 -- drags them back out to that range along the
/// line between the two. It then places the player with
/// `Actor444000_PlacePlayerAhead`, hands the fight the battle flag, drops the
/// host and three of its escorts out of the actor slots and orders escort 3
/// through a 0x7DA message.
///
/// Every other tick runs the ordinary re-arm first. Animation 0xF hands over to
/// 0xE once the second slot raises its flag, and each animation fires one-shot
/// cues on the frames it reaches -- 0x19 under 0xF, 0x1D / 0x23 / 0x27 under
/// 0xE, the last two also kicking the pad -- with `field_7A8` remembering the
/// frame so none repeats while it is held. While message 0x3ED reports the
/// player free they are put back on the host's own position, and sub-state 0x17
/// re-places them and re-sends the 0x3FF animation.
/// Places the player in front of the host and points the pair at each other:
/// the host's fifth part is carried into view space, the yaw from there to the
/// player picks which of the two message-0x3FF animation tables the tick will
/// send (`..._80161680` past a quarter turn, `..._80161670` within it), and the
/// opposite yaw is stowed in `field_7C4` for the drive step. The normalised
/// direction scaled to 0x384 is where the player is asked to stand.
static __inline__ void Actor444000_PlacePlayerAhead(Actor444000* task, Actor444000Work* work,
                                                    Task* player, Actor444000WarpScratch* sc,
                                                    PlayerStatus* cfg)
{
    GsCOORDINATE2* coord;
    GsCOORDINATE2* facing;
    s16            angle;
    s32            yaw;

    sc->pos.vz = 0;
    sc->pos.vy = 0;
    sc->pos.vx = 0;
    Actor444000_LocalToView(&((TmdObject*)task->extra)->coords[4], &sc->pos);

    sc->dir.vx = sc->pos.vx - ((TmdObject*)player->extra)->coords->coord.t[0];
    sc->dir.vy = 0;
    sc->dir.vz = sc->pos.vz - ((TmdObject*)player->extra)->coords->coord.t[2];
    facing     = ((TmdObject*)player->extra)->coords;
    angle      = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    yaw       = angle;
    sc->angle = yaw;
    if (abs(sc->angle) > 0x400) {
        if (sc->angle > 0) {
            sc->angle = yaw - 0x800;
        } else {
            sc->angle = yaw + 0x800;
        }
        work->anim.field_0 = D_actor_444000_80161680;
    } else {
        work->anim.field_0 = D_actor_444000_80161670;
    }
    coord      = ((TmdObject*)player->extra)->coords;
    sc->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);

    sc->dir.vx = ((TmdObject*)player->extra)->coords->coord.t[0] - sc->pos.vx;
    sc->dir.vy = 0;
    sc->dir.vz = ((TmdObject*)player->extra)->coords->coord.t[2] - sc->pos.vz;
    facing     = ((TmdObject*)task->extra)->coords;
    angle      = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (angle < 0) {
    wrapUp2:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp2;
        }
    } else {
    wrapDown2:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown2;
        }
    }
    work->field_7C4 = angle;

    VectorNormalSS(&sc->dir, &sc->dir);
    gte_lddp(0x384);
    gte_ldsv(&sc->dir);
    gte_gpf12_real();
    gte_stsv(&sc->dir);

    D_actor_444000_80161908.pos.vx = sc->pos.vx + sc->dir.vx;
    D_actor_444000_80161908.pos.vy = ((TmdObject*)player->extra)->coords->coord.t[1];
    D_actor_444000_80161908.pos.vz = sc->pos.vz + sc->dir.vz;
    D_actor_444000_80161908.rot.vx = 0;
    D_actor_444000_80161908.rot.vy = sc->angle;
    D_actor_444000_80161908.rot.vz = 0;
    if (cfg->hp > 0) {
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_444000_80161908, 0);
    }
}

void func_actor_444000_8013EC84(Actor444000* arg0)
{
    Actor444000WarpScratch* sc;
    Actor444000Work*        work;
    Actor444000Work*        escorts;
    Actor444000Work*        buffers;
    GpEnemy*                enemy;
    Task*                   player;
    PlayerStatus*           cfg;
    Task*                   target;
    TmdObject*              tmd;
    TmdObject*              escortTmd;
    SVECTOR*                verts;
    s16                     i;
    s16                     j;
    s32                     frame;
    s32                     cueId;
    s32                     cuePan;
    s32                     hitId;
    s32                     hitPan;
    s32                     endId;
    s32                     endPan;

    work   = arg0->field_1C;
    enemy  = arg0->field_20;
    player = Game_GetPtrSlot(3);
    cfg    = &Player_Status;

    if (work->field_4 != 0) {
        sc = (Actor444000WarpScratch*)(SCRATCH_SP -= sizeof(Actor444000WarpScratch));

        verts        = Gp_GridParams->field_8;
        verts[24].vy = 0x1F4;
        verts[25].vy = 0x1F4;
        verts[26].vy = 0x320;
        verts[27].vy = 0x320;
        verts[28].vy = 0x1F4;
        verts[29].vy = 0x1F4;
        verts[30].vy = 0x320;
        verts[31].vy = 0x320;

        work->field_7B3    = 0xF;
        work->field_7B0    = 2;
        escorts            = arg0->field_1C;
        escorts->field_7F3 = 0;

        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }

        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->field_1C;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }

        work->field_EFE = 0;
        work->field_EF4 = 0;
        work->field_EF6 = 1;
        work->field_EFA = 0;

        sc->delta.vx = ((TmdObject*)player->extra)->coords->coord.t[0] -
                       ((TmdObject*)arg0->extra)->coords->coord.t[0];
        sc->delta.vz = ((TmdObject*)player->extra)->coords->coord.t[2] -
                       ((TmdObject*)arg0->extra)->coords->coord.t[2];
        sc->dist = SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vz * sc->delta.vz);
        if (sc->dist < 0xB54) {
            sc->dir.vy = 0;
            sc->dir.vx = sc->delta.vx;
            sc->dir.vz = sc->delta.vz;
            VectorNormalSS(&sc->dir, &sc->dir);
            gte_lddp(0xCE4);
            gte_ldsv(&sc->dir);
            gte_gpf12_real();
            gte_stsv(&sc->dir);
            ((TmdObject*)player->extra)->coords->coord.t[0] =
                ((TmdObject*)arg0->extra)->coords->coord.t[0] + sc->dir.vx;
            ((TmdObject*)player->extra)->coords->coord.t[2] =
                ((TmdObject*)arg0->extra)->coords->coord.t[2] + sc->dir.vz;
            ((TmdObject*)player->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)player->extra)->coords);
        }

        Actor444000_PlacePlayerAhead(arg0, work, player, sc, cfg);

        D_actor_444000_80161868 = 0;
        Gp_StateC08.field_6    |= 1;
        Gp_PulseState1C();
        Gp_ClearNodeSlots(&enemy->node);
        Gp_ClearNodeSlots(&work->field_ECC[3]->node);
        Gp_ClearNodeSlots(&work->field_ECC[0]->node);
        Gp_ClearNodeSlots(&work->field_ECC[1]->node);
        SndEvt_EnqueueType7((((u16)enemy->field_8 >> 12) << 8) | 0x4020000A, 1);

        D_actor_444000_80161888.field_0 = 0;
        D_actor_444000_80161888.field_1 = 0x2C;
        D_actor_444000_80161888.field_2 = 3;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);
    } else {
        sc = (Actor444000WarpScratch*)(SCRATCH_SP -= sizeof(Actor444000WarpScratch));
        func_actor_444000_8013441C(arg0);

        if ((work->slots0[1].flags & 1) && work->field_7B3 == 0xF) {
            work->field_7B0 = 2;
            work->field_7B3 = 0xE;
        }

        if (work->field_7B3 == 0xF) {
            if (cfg->hp > 0) {
                target = Game_GetPtrSlot(3);
                Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 3), 0);
                if (cfg->hp <= 0) {
                    ((GameActor*)player->work)->field_956 = 0xA;
                    gGameSession->areaBgmCountdown        = 0x1E;
                    gGameSession->field_12E               = 0x36;
                    gGameSession->deathRestartDelay       = 0x5A;
                }
            }
            frame = work->slots0[3].curRec & 0x3FF;
            if (frame == 0x19 && work->field_7A8 != frame) {
                cueId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200011;
                cuePan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(cueId, cuePan,
                                    (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
            }
            work->field_7A8 = work->slots0[3].curRec & 0x3FF;
        }

        if (work->field_7B3 == 0xE) {
            frame = work->slots0[3].curRec & 0x3FF;
            if (frame == 0x1D && work->field_7A8 != frame) {
                Gp_SpawnPadLerp(4, 0xFF, 8);
            }
            frame = work->slots0[3].curRec & 0x3FF;
            if (frame == 0x23 && work->field_7A8 != frame) {
                hitId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200012;
                hitPan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(hitId, hitPan,
                                    (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
                Gp_SpawnPadLerp(4, 0xFF, 8);
            }
            frame = work->slots0[3].curRec & 0x3FF;
            if (frame == 0x27 && work->field_7A8 != frame) {
                endId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200012;
                endPan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
                SndEvt_EnqueueType6(endId, endPan,
                                    (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
                Gp_SpawnPadLerp(4, 0xFF, 8);
            }
            work->field_7A8 = work->slots0[3].curRec & 0x3FF;
        }

        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            D_actor_444000_80161908.pos.vx = ((TmdObject*)arg0->extra)->coords->coord.t[0];
            D_actor_444000_80161908.pos.vy = ((TmdObject*)arg0->extra)->coords->coord.t[1];
            D_actor_444000_80161908.pos.vz = ((TmdObject*)arg0->extra)->coords->coord.t[2];
            D_actor_444000_80161908.rot.vx = 0;
            D_actor_444000_80161908.rot.vy = 0;
            D_actor_444000_80161908.rot.vz = 0;
            Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_444000_80161908, 0);
            D_actor_444000_80161868 = 1;
        }

        if (work->field_6 == 0x17) {
            SndEvt_EnqueueType7((((u16)enemy->field_8 >> 12) << 8) | 0x4020000A, 1);
            Actor444000_PlacePlayerAhead(arg0, work, player, sc, cfg);
            work->anim.field_4 = 1;
            work->anim.field_8 = 0;
            work->anim.field_C = 0;
            work->field_F02    = 1;
            Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        }
    }

    SCRATCH_SP += sizeof(Actor444000WarpScratch);
}

/// Tick of the arena fight that runs the boss' two swipes and keeps the player
/// pinned in the scripted animation.
///
/// A reset request re-arms the block on animation 4, clears the host model's
/// flag word and pushes it onto each of the seven escorts' models, makes sure
/// the host and every escort has its model buffers allocated, then rebuilds the
/// free coordinate at `field_E3C` from `field_7C8` and plays the entry cue.
/// That coordinate is pushed through `Gp_UpdateCoord` again on every step.
///
/// The two swipes are one-shot: animation 4 reaching frame 0xC raises bit
/// 0x8000 of the collision object's flags, kicks the pad and fires two cues,
/// and animation 5 reaching frame 0x1C fires a third. `field_7AC` remembers the
/// frame each step so neither repeats while the frame is held, and the bit is
/// cleared on every step the first swipe is not live.
///
/// `field_6` then picks the blend weight in `field_7A4` (and hands over to
/// state 0xA at 0xDC), and while it sits in 0x29..0x2E the shared timer
/// `D_actor_444000_80144A70` climbs by 0x258 a step up to 0x1770 -- past 0x39 it
/// is wound back down again instead.
///
/// The rest is the player hold: once one of the collision object's five records
/// reports a hit of class 1, message 0x3F8 is asked whether the player can be
/// taken over and message 0x3F9 asks for the hold itself, with `field_ECA`
/// keeping that reply and `field_EC8` marking the hold as ours. While it is,
/// the 0x3FF animation is re-sent every step the reply and `field_EC8` agree
/// (or, if they do not, for the first 0x28 steps), and after 0x17 steps without
/// the hold the payload is swapped for the player's own weapon animation
/// (`field_4` 4). A reply of something other than 1 on that second stage
/// cancels the animation with message 0x3F1 and drops the hold.
void func_actor_444000_8013FB74(Actor444000* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* buffers;
    GpEnemy*         enemy;
    Task*            player;
    Task*            target;
    TmdObject*       tmd;
    TmdObject*       escortTmd;
    GsCOORDINATE2*   coord;
    GpRec18*         recs;
    s16              i;
    s16              j;
    s16              k;
    s16              mode;
    s32              found;
    s32              frame;
    s32              frame2;
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
    u16              count;

    work        = arg0->field_1C;
    enemy       = arg0->field_20;
    player      = Game_GetPtrSlot(3);
    SCRATCH_SP -= 0x30;

    if (work->field_4 != 0) {
        work->field_F1D                  = 0xB;
        work->field_7B3                  = 4;
        work->field_7B0                  = 2;
        escorts                          = arg0->field_1C;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->field_1C;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 1;
        work->field_EF8 = 1;
        Gfx_RotMatrixY(&work->field_E3C.c.coord, work->field_7C8, 1);
        work->field_E3C.c.flg = 0;
        Gp_UpdateCoord(&work->field_E3C.c);
        work->field_E96 = 0xC80;

        resetId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200017;
        resetPan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }

    coord                 = &work->field_E3C.c;
    work->field_E3C.c.flg = 0;
    Gp_UpdateCoord(coord);

    if (work->field_7B3 == 4 && (frame = work->slots0[1].curRec & 0x3FF) == 0xC &&
        work->field_7AC != frame) {
        Gfx_RotMatrixY(&work->field_E3C.c.coord, work->field_7C8, 1);
        work->field_E3C.c.flg = 0;
        Gp_UpdateCoord(coord);
        work->field_EAC  = 3;
        work->obj.flags |= 0x8000;
        Gp_SpawnPadLerp(0x30, 0xFF, 8);

        swipeId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200019;
        swipePan = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            swipeId, swipePan,
            (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) / 2));

        swipe2Id  = (((u16)enemy->field_8 >> 12) << 8) | 0x4020001A;
        swipe2Pan = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            swipe2Id, swipe2Pan,
            (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) / 2));
    } else {
        work->obj.flags &= 0x7FFF;
    }

    if (work->field_7B3 == 5 && (frame2 = work->slots0[2].curRec & 0x3FF) == 0x1C &&
        work->field_7AC != frame2) {
        work->field_EAC = 3;
        Gp_SpawnPadLerp(0x20, 0x8F, 8);

        hitId  = (((u16)enemy->field_8 >> 12) << 8) | 0x4020001B;
        hitPan = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) / 2));
    }

    if (work->field_7B3 == 4) {
        work->field_7AC = work->slots0[1].curRec & 0x3FF;
    } else {
        work->field_7AC = work->slots0[2].curRec & 0x3FF;
    }

    switch (work->field_6) {
        case 0x14:
            D_actor_444000_80144A70 = 0x640;
            work->field_7A4         = 0;
            break;
        case 0x22:
            work->field_7A4 = 1;
            break;
        case 0x2B:
            work->field_7A4 = 5;
            cueId           = (((u16)enemy->field_8 >> 12) << 8) | 0x40200018;
            cuePan          = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
            SndEvt_EnqueueType6(
                cueId, cuePan,
                (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) /
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

    if ((u32)((u16)work->field_6 - 0x29) < 6 && D_actor_444000_80144A70 < 0x1770) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 + 0x258;
    }

    func_actor_444000_8013441C(arg0);

    recs = work->recs2;
    for (k = 0; k < 5; k++) {
        if (recs[k].key == 0) {
            goto missed;
        }
        if ((recs[k].key & 0xFFFF0000) == 0x10000) {
            found = 1;
            goto scanned;
        }
    }
missed:
    found = 0;
scanned:
    if (found != 0 && Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&D_actor_444000_80161928, 0) == 0) {
        target          = Game_GetPtrSlot(3);
        work->field_ECA = Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 4), 0);
        if (work->field_ECA == 1) {
            ((GameActor*)player->work)->field_956 = 0xA;
        }
        work->anim.field_0 = D_actor_444000_80161670;
        work->field_EC8    = 1;
        work->anim.field_4 = 2;
        work->anim.field_8 = 0;
        work->anim.field_C = 0;
        Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        work->field_7CA = 0;
    }

    mode = work->field_EC8;
    if (mode == 1 && (s16)work->field_0 != 0xD) {
        count           = work->field_7CA + 1;
        work->field_7CA = count;
        if (work->field_ECA == mode) {
            if (work->anim.field_4 == 2) {
                work->anim.field_0 = D_actor_444000_80161670;
                work->anim.field_8 = 0;
                work->anim.field_C = 0;
                Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                work->field_7CA = 0;
            }
        } else if (work->anim.field_4 == 2 && (s16)count < 0x28) {
            work->anim.field_0 = D_actor_444000_80161670;
            work->anim.field_8 = 0;
            work->anim.field_C = 0;
            Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        }

        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            switch (work->anim.field_4) {
                case 2:
                    if (work->field_ECA != 1 && (s16)work->field_7CA >= 0x17) {
                        work->anim.field_0         = D_actor_444000_80161670;
                        D_actor_444000_80161670[4] = ((Actor444000AnimTable*)Gp_PlayerAnimBlkTbl
                                                          [Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Player_Status.weapon])
                                                         ->sets[7];
                        work->anim.field_4 = 4;
                        work->anim.field_8 = 1;
                        work->anim.field_C = 3;
                        Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                        work->field_7CA = 0;
                    }
                    break;
                case 4:
                    if (work->field_ECA != 1) {
                        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 2, 0);
                        work->field_EC8 = 0;
                    }
                    break;
            }
        }
    }

    if (work->field_6 == 0x3C && work->field_7B3 == 4) {
        work->field_7B3 = 5;
        work->field_7B0 = 1;
    }

    if (work->field_6 >= 0x39) {
        if (D_actor_444000_80144A70 >= 0xBB9) {
            D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
        } else {
            D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0x1E;
        }
    }

    SCRATCH_SP += 0x30;
}

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

        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;

        resetId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200017;
        resetPan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }

    if (work->field_6 == 0x3B) {
        cueId  = (((u16)enemy->field_8 >> 12) << 8) | 0x40200016;
        cuePan = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(cueId, cuePan,
                            (s8)Gp_GetObjDepth((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]));
    }

    if (work->field_6 == 0x3C) {
        hitId  = (((u16)enemy->field_8 >> 12) << 8) | 0x4020000D;
        hitPan = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(hitId, hitPan,
                            (s8)Gp_GetObjDepth((GpObj38*)&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]));
    }

    if ((s16)(u16)work->field_6 >= 0x3D) {
        if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
            if (D_actor_444000_80161850 >= 2) {
                D_actor_444000_80161850 = 0;
            } else {
                D_actor_444000_80161850 = (u16)D_actor_444000_80161850 + 1;
            }

            Actor444000_AccumulateRotation(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1],
                                           &D_actor_444000_80161948[D_actor_444000_80161850].coord);
            D_actor_444000_80161948[D_actor_444000_80161850].sub = &gGfxViewCoord;

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            Actor444000_LocalToView(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], &pos);

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

    if (work->slots0[1].flags & 1) {
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
        escorts                          = arg0->field_1C;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->field_1C;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_7B6 = 0x10;
    }
    if (work->field_7B3 == 0xD) {
        frame = work->slots0[2].curRec & 0x3FF;
        if (frame == 0x15 && work->field_7D8 != frame) {
            work->field_EAC = 3;
            Gp_SpawnScript18((s32)&D_actor_444000_80144A84, (s32)&D_actor_444000_80144A8C);
        }
        work->field_7D8 = work->slots0[2].curRec & 0x3FF;
    }
    if (work->field_7B3 == 9 && work->field_6 == 0x2D) {
        coords                                = ((TmdObject*)arg0->extra)->coords;
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
    if (work->field_7B3 == 0x14 && (work->slots0[1].flags & 1)) {
        work->field_7B3 = 0xD;
        work->field_7B0 = 1;
    }
    if (D_actor_444000_80144A70 >= 0x191) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
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
        obj                              = arg0->field_20;
        escorts                          = arg0->field_1C;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->field_1C;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
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
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    frame                                  = work->slots0[2].curRec & 0x3FF;
    if (frame == 0x1C && work->field_7D8 != frame) {
        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_444000_80144A74, (s32)&D_actor_444000_80144A7C);
    }
    work->field_7D8 = work->slots0[2].curRec & 0x3FF;
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
        tmd             = (TmdObject*)arg0->extra;
        obj             = arg0->field_20;
        obj->node.flags = 8;
        tmd->flags      = 0;
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
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
        SndEvt_EnqueueType7((((u16)obj->field_8 >> 12) << 8) | 0x4020000D, 1);
        return;
    }
    SCRATCH_SP -= 0xC;
    if (D_actor_444000_80144A70 >= 0x191) {
        work->field_7A4         = 0;
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(arg0);
    if (work->slots0[1].flags & 1) {
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
    coord  = ((TmdObject*)arg0->extra)->coords;
    d->vx  = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy  = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz  = D_80073B8C->t[2] - coord->coord.t[2];
    facing = ((TmdObject*)arg0->extra)->coords;
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
    if (work->field_F08 == 1 && ((TmdObject*)player->extra)->coords->coord.t[2] < -0x1D4C) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 2) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 3 && ((TmdObject*)player->extra)->coords->coord.t[2] < -0x30D4) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 4 && ((TmdObject*)player->extra)->coords->coord.t[2] < -0x3DB8 &&
        enemy->field_40 < 0x9C4) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 5 && ((TmdObject*)player->extra)->coords->coord.t[2] < -0x4268 &&
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
    sc->delta.vx = ((TmdObject*)player->extra)->coords->coord.t[0] -
                   ((TmdObject*)arg0->extra)->coords->coord.t[0] - 0x51F;
    sc->delta.vy = ((TmdObject*)player->extra)->coords->coord.t[1] -
                   ((TmdObject*)arg0->extra)->coords->coord.t[1] - 0xFA;
    sc->delta.vz = ((TmdObject*)player->extra)->coords->coord.t[2] -
                   ((TmdObject*)arg0->extra)->coords->coord.t[2] + 0x25F;
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

/// Escort-spawn tick of the arena fight: re-arms the block on request and, on
/// that first pass, tops the two escort slots (`field_EE8`) back up to two live
/// enemies, seeding each one's model texture page from the current area record
/// and stamping its slot index into `GpEnemy::field_8`. Every tick it then
/// yaws the host at the player, and at sub-state 0x46 / 0x78 it sends escort 0
/// or 1 a 0x7DB order whose action is picked from `field_F08` and a coin flip.
void func_actor_444000_80141618(Actor444000* task)
{
    Actor444000SpawnScratch* sc;
    Actor444000Work*         work;
    GpEnemy*                 host;
    GpEnemy*                 escort;
    PlayerStatus*            cfg;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;
    TmdObject*               model;
    GpCdRec10*               entry;
    GpAreaKey                key;
    GpAreaKey*               sessionKey;
    s32                      cueId;
    s32                      cuePan;
    s32                      blastId;
    s32                      blastPan;
    s32                      rnd;
    s32                      state;
    s16                      angle;
    u32                      frame;

    sc   = (Actor444000SpawnScratch*)(SCRATCH_SP -= sizeof(Actor444000SpawnScratch));
    work = task->field_1C;
    host = task->field_20;
    if (work->field_4 != 0) {
        work->field_EF4 = 0;
        work->field_EF6 = 1;
        state           = work->field_7B3;
        work->field_7B6 = 0x10;
        work->field_EFA = 0;
        if (state != 0x13) {
            work->field_7B3 = 0x13;
            work->field_7B0 = 1;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        for (sc->i = 0; sc->i < 2; sc->i++) {
            if (work->field_EE8[sc->i] == NULL && (u8)work->field_F1B < 8 && work->field_F08 < 6) {
                work->field_EE8[sc->i] = Gp_SpawnEnemyFromTable(&D_80172604, 3, 2, NULL);
                if (work->field_EE8[sc->i] != NULL) {
                    work->field_F1B++;
                    model      = (TmdObject*)work->field_EE8[sc->i]->task->extra;
                    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                    key.stage  = sessionKey->stage;
                    key.area   = sessionKey->area;
                    key.room   = sessionKey->room;
                    key.view   = sessionKey->view;
                    Gp_SyncAreaKeyIndex(&key);
                    entry        = (GpCdRec10*)((s32)Gp_GetNestedAreaRec(&key)->field_0 + 0x20);
                    model->tpage = entry->field_D;
                    model->clut  = entry->field_E;
                    if (model->buffer != NULL) {
                        Tmd_ProcessStream(model);
                        Tmd_ProcessStream(model);
                    }
                    work->field_EE8[sc->i]->field_A = 0x900;
                    escort                          = work->field_EE8[sc->i];
                    escort->field_8                |= sc->i << 12;
                    work->field_F1C++;
                }
            }
        }
        work->field_E96 = 0xC80;
        SndEvt_EnqueueType7((((u16)host->field_8 >> 12) << 8) | 0x4020000D, 1);
    }
    if (D_actor_444000_80144A70 >= 0x191) {
        D_actor_444000_80144A70 = (u16)D_actor_444000_80144A70 - 0xC8;
    }
    func_actor_444000_8013441C(task);
    if (work->field_7B3 == 0x13 && (frame = work->slots0[1].curRec & 0x3FF) >= 4 && frame < 0xD) {
        work->field_EFA = 1;
    } else {
        work->field_EFA = 0;
    }
    cfg          = &Player_Status;
    coord        = ((TmdObject*)task->extra)->coords;
    sc->delta.vx = cfg->coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = cfg->coordMtx->t[1] - coord->coord.t[1];
    sc->delta.vz = cfg->coordMtx->t[2] - coord->coord.t[2];
    facing       = ((TmdObject*)task->extra)->coords;
    angle        = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    if ((work->slots0[1].flags & 1) && work->field_7B3 == 0x13) {
        work->field_7B3 = 1;
        work->field_7B0 = 1;
        work->field_7B6 = 0x10;
        func_actor_444000_8013441C(task);
    }
    if (work->field_6 >= 0x14B || (work->field_7B3 == 1 && work->field_F1C == 0)) {
        work->field_0 = 3;
    }
    if (work->field_6 == 6) {
        cueId  = (((u16)host->field_8 >> 12) << 8) | 0x40200004;
        cuePan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(cueId, cuePan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords));
    }
    if (work->field_6 == 0x3B) {
        blastId  = (((u16)host->field_8 >> 12) << 8) | 0x40200010;
        blastPan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(blastId, blastPan, (s8)(Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords) / 2));
        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_444000_80144A74, (s32)&D_actor_444000_80144A7C);
    }
    if (work->field_6 != 0x46 && work->field_6 != 0x78) {
        goto out;
    }
    if (work->field_6 == 0x46) {
        sc->i = 0;
    } else {
        sc->i = 1;
    }
    if (work->field_EE8[sc->i] != NULL && work->field_F08 < 6) {
        D_actor_444000_80161888.field_0 = 0;
        D_actor_444000_80161888.field_1 = 0x2C;
        switch (work->field_F08) {
            case 0:
            case 1:
                if (sc->i == 0) {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_8016188A = 3;
                    } else {
                        D_actor_444000_8016188A = 4;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_8016188A = 5;
                    } else {
                        D_actor_444000_8016188A = 6;
                    }
                }
                break;
            case 2:
            case 3:
                if (sc->i == 0) {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_8016188A = 0xd;
                    } else {
                        D_actor_444000_8016188A = 8;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_8016188A = 7;
                    } else {
                        D_actor_444000_8016188A = 0xe;
                    }
                }
                break;
            case 4:
            case 5:
                if (sc->i == 0) {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_8016188A = 9;
                    } else {
                        D_actor_444000_8016188A = 0xf;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_444000_8016188A = 9;
                    } else {
                        D_actor_444000_8016188A = 0xf;
                    }
                }
                break;
        }
        D_actor_444000_80161888.field_2  = (u16)D_actor_444000_80161888.field_2 << 8;
        rnd                              = (Gp_LcgState * 5) + 0x71357911;
        D_actor_444000_80161888.field_2 |= ((((u32)rnd >> 16) % 3) * 0x10) | 1;
        Gp_LcgState                      = rnd;
        Gp_DispatchMsg(work->field_EE8[sc->i]->task, 0x7DB, (s32)&D_actor_444000_80161888, 0);
    }
out:
    SCRATCH_SP += sizeof(Actor444000SpawnScratch);
}

/// Runs the arena attack sequence: restores the host and escort models, handles
/// animation cues and spawns the additional escort, then keeps the host facing
/// the camera-target matrix through the shared drive step.
void func_actor_444000_80141DFC(Actor444000* arg0)
{
    Actor444000Work* work;
    Actor444000Work* escorts;
    Actor444000Work* buffers;
    GpEnemy*         obj;
    GpEnemy*         child;
    GpAreaKey*       sessionKey;
    GpAreaRec*       rec;
    GpCdRec10*       entry;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   headCoord;
    TmdObject*       tmd;
    TmdObject*       escortTmd;
    TmdObject*       model;
    SVECTOR          vec;
    SVECTOR*         v;
    GpAreaKey        key;
    GpAreaKey*       keyPtr;
    u8               areaByte0;
    s16              i;
    s16              j;
    s16              angle;
    s32              id;
    s32              pan;

    work = arg0->field_1C;
    obj  = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_7B3                  = 0xE;
        work->field_7B0                  = 1;
        escorts                          = arg0->field_1C;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)arg0->extra)->flags;
            }
        }
        tmd     = (TmdObject*)arg0->extra;
        buffers = arg0->field_1C;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = (TmdObject*)buffers->field_ECC[j]->task->extra;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 0;
        work->field_EFE = 0;
    }
    switch (work->field_6) {
        case 0x64:
        case 0x104:
            if ((s8)work->field_F1A > 0) {
                work->field_7B3 = 0x10;
                work->field_7B0 = 1;
                work->field_EF4 = 1;
                work->field_F1A = work->field_F1A - 1;
            } else {
                work->field_0   = 0xA;
                work->field_EFE = 0;
            }
            break;
        case 0x74:
            id  = (((u16)obj->field_8 >> 12) << 8) | 0x40200017;
            pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
            break;
        case 0x1A4:
            work->field_0   = 0xA;
            work->field_EFE = 0;
            work->field_F1A = 0;
            break;
        case 0x9B:
        case 0x113:
            work->field_EFE = 0x80;
            break;
        case 0xAF:
        case 0x145:
            child           = Gp_SpawnEnemyFromTable(&D_actor_444000_801617DC, 3, 0, arg0->field_20);
            child->field_A  = 0x900;
            work->field_EF0 = child;
            if (child != NULL) {
                model      = (TmdObject*)child->task->extra;
                sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                key.stage  = sessionKey->stage;
                key.area   = sessionKey->area;
                key.room   = sessionKey->room;
                areaByte0  = sessionKey->view;
                SOFT_BARRIER();
                keyPtr = &key;
                TOUCH_REG(keyPtr);
                key.view = areaByte0;
                Gp_SyncAreaKeyIndex(keyPtr);
                rec          = Gp_GetNestedAreaRec(&key);
                entry        = (GpCdRec10*)((s32)rec->field_0 + 0x20);
                model->tpage = entry->field_D;
                model->clut  = entry->field_E;
                if (model->buffer != NULL) {
                    Tmd_ProcessStream(model);
                    Tmd_ProcessStream(model);
                }
                work->field_EFE = 0;
            }
            break;
    }
    coord     = ((TmdObject*)arg0->extra)->coords;
    v         = &vec;
    v->vx     = D_80073B8C->t[0] - coord->coord.t[0];
    v->vy     = D_80073B8C->t[1] - coord->coord.t[1];
    v->vz     = D_80073B8C->t[2] - coord->coord.t[2];
    headCoord = ((TmdObject*)arg0->extra)->coords;
    angle     = ratan2(v->vx, v->vz) - ratan2(-headCoord->coord.m[2][0], headCoord->coord.m[2][2]);
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
    func_actor_444000_8013441C(arg0);
    if (work->field_7B3 == 0x10 && (work->slots0[1].flags & 1)) {
        work->field_7B3 = 0xE;
        work->field_7B0 = 1;
    }
}

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

    if (((TmdObject*)player->extra)->coords->coord.t[1] > 0) {
        ((TmdObject*)player->extra)->coords->coord.t[1] = 0;
    }

    z = ((TmdObject*)player->extra)->coords->coord.t[2];
    if (z > 0) {
        ((TmdObject*)player->extra)->coords->coord.t[2] = 0;
    } else if (z > -1000) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 0) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 0;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -5000) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 0) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 0;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -7000) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 9500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 9500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -13200) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -14750) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 17500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 17500;
        }
    } else if (z > -21250) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else if (z > -22800) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 17500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 17500;
        }
    } else if (z > -26000) {
        if (((TmdObject*)player->extra)->coords->coord.t[0] < 11500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 11500;
        }
        if (((TmdObject*)player->extra)->coords->coord.t[0] > 16500) {
            ((TmdObject*)player->extra)->coords->coord.t[0] = 16500;
        }
    } else {
        ((TmdObject*)player->extra)->coords->coord.t[2] = -26000;
    }
}

/// Per-frame body of the boss task: refreshes the host model's coordinate,
/// times out the "model buffers freed" countdown, re-lights the host and its
/// escorts, then runs the state handler `Actor444000Work::field_0` selects and
/// republishes every collision group.
///
/// `D_801153F4` gates how much of that runs. While the controller task is
/// suspended (1 or 2) the tick only pushes the host's `TmdObject::flags`
/// onto the escorts and clears the collision tables, and returns; only the
/// running case (0) and anything else falls through to the state machine.
/// Within the suspended cases the view index decides whether that flag word is
/// 0x80 (hidden) or 0.
///
/// `field_7F3` is a countdown armed when the fight hides the models: while it
/// runs the host is flagged hidden, and the step that takes it to zero also
/// raises bit 2 and hands every model's buffers back with `Tmd_FreeBuffers`.
///
/// `field_EFA` selects which of the two bodies is the "live" one -- the host
/// (`enemy`) or escort 3 (`field_ECC[3]`) -- and that choice drives the colour
/// update, the link-node slots and which collision groups publish their
/// `0x8000` bit this frame. States 0, 1, 5, 0xC, 0x12 and 0x13 are the inert
/// ones: they park both bodies on slot 1 and clear every group.
///
/// `field_F12` is the death timer, only started once the host's HP is gone:
/// step 0 tells the scene (message 0x7DA, action 0x2C) and latches
/// `D_actor_444000_80144A68`, step 3 tells the player's task (0x13F4) and moves
/// the fight to state 0x12 with the two death cues.
///
/// The dispatch table is a local, as in `func_actor_444000_80142F28`.
void func_actor_444000_801423C4(GpEnemy* enemy, Actor444000* task)
{
    PlayerStatus*    cfg  = &Player_Status;
    Actor444000Work* work = task->field_1C;
    VECTOR           pos;
    void             (*handlers[0x15])(Actor444000*) = {
        func_actor_444000_8013D810,
        func_actor_444000_80143F4C,
        NULL,
        func_actor_444000_8013E058,
        NULL,
        func_actor_444000_80140BBC,
        NULL,
        func_actor_444000_801404C0,
        func_actor_444000_8014105C,
        func_actor_444000_8013482C,
        func_actor_444000_801411C8,
        func_actor_444000_8013FB74,
        func_actor_444000_80140E28,
        func_actor_444000_8013EC84,
        func_actor_444000_80141618,
        func_actor_444000_80141DFC,
        func_actor_444000_801434C4,
        func_actor_444000_801435CC,
        func_actor_444000_80135448,
        func_actor_444000_8013D96C,
        NULL,
    };
    Actor444000Work* escorts;
    Actor444000Work* flagged;
    s32              view;
    s16              i;
    s16              j;

    view                                     = Gp_GetViewIndex() & 0xFF;
    ((TmdObject*)task->extra)->coords[0].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[0]);

    escorts = task->field_1C;
    if (escorts->field_7F3 != 0) {
        ((TmdObject*)task->extra)->flags = 0x80;
        if (--escorts->field_7F3 == 0) {
            ((TmdObject*)task->extra)->flags |= 4;
            Tmd_FreeBuffers((TmdObject*)task->extra);
            for (j = 0; j < 7; j++) {
                if (escorts->field_ECC[j] != NULL) {
                    ((TmdObject*)escorts->field_ECC[j]->task->extra)->flags |= 4;
                    Tmd_FreeBuffers((TmdObject*)escorts->field_ECC[j]->task->extra);
                }
            }
        }
    }

    func_actor_444000_80142254();

    pos.vx = ((TmdObject*)task->extra)->coords[3].workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords[3].workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords[3].workm.t[2];

    if (work->field_EFA != work->field_EFC) {
        Gp_UpdateActorColor(enemy, &pos, 0, 0);
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
        work->field_EFC = work->field_EFA;
    }
    Gp_UpdateActorColor(work->field_EFA != 0 ? enemy : work->field_ECC[3], &pos, 0, 0);

    if ((s16)work->field_0 == 0xB) {
        ((TmdObject*)work->field_ECC[4]->task->extra)->otOffset = -1;
    } else {
        ((TmdObject*)work->field_ECC[4]->task->extra)->otOffset = 0;
    }

    switch (D_801153F4) {
        case 0:
            if ((s16)work->field_0 != 0) {
                if (view == 9) {
                    flagged                          = task->field_1C;
                    flagged->field_7F3               = 0;
                    ((TmdObject*)task->extra)->flags = 0x80;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                                ((TmdObject*)task->extra)->flags;
                        }
                    }
                } else {
                    flagged                          = task->field_1C;
                    flagged->field_7F3               = 0;
                    ((TmdObject*)task->extra)->flags = 0;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                                ((TmdObject*)task->extra)->flags;
                        }
                    }
                }
            }
            break;

        case 1:
            if ((s16)work->field_0 != 0) {
                if (view == 9) {
                    flagged                          = task->field_1C;
                    flagged->field_7F3               = 0;
                    ((TmdObject*)task->extra)->flags = 0x80;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                                ((TmdObject*)task->extra)->flags;
                        }
                    }
                } else {
                    flagged                          = task->field_1C;
                    flagged->field_7F3               = 0;
                    ((TmdObject*)task->extra)->flags = 0;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                                ((TmdObject*)task->extra)->flags;
                        }
                    }
                }
            }
            Gp_ClearRec18Occupied(work->hits[0].recs);
            Gp_ClearRec18Occupied(work->hits[1].recs);
            Gp_ClearRec18Occupied(work->hits[2].recs);
            Gp_ClearRec18Occupied(work->hits[3].recs);
            Gp_ClearRec18Occupied(work->hits[4].recs);
            Gp_ClearRec18Occupied(work->hits[5].recs);
            Gp_ClearRec18Occupied(work->hits[6].recs);
            Gp_ClearRec18Occupied(work->hits[7].recs);
            Gp_ClearRec18Occupied(work->hits[8].recs);
            Gp_ClearRec18Occupied(work->recs2);
            return;

        case 2:
            flagged                          = task->field_1C;
            flagged->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0x80;
            for (i = 0; i < 7; i++) {
                if (flagged->field_ECC[i] != NULL) {
                    ((TmdObject*)flagged->field_ECC[i]->task->extra)->flags =
                        ((TmdObject*)task->extra)->flags;
                }
            }
            Gp_ClearRec18Occupied(work->hits[0].recs);
            Gp_ClearRec18Occupied(work->hits[1].recs);
            Gp_ClearRec18Occupied(work->hits[2].recs);
            Gp_ClearRec18Occupied(work->hits[3].recs);
            Gp_ClearRec18Occupied(work->hits[4].recs);
            Gp_ClearRec18Occupied(work->hits[5].recs);
            Gp_ClearRec18Occupied(work->hits[6].recs);
            Gp_ClearRec18Occupied(work->hits[7].recs);
            Gp_ClearRec18Occupied(work->hits[8].recs);
            Gp_ClearRec18Occupied(work->recs2);
            return;
    }

    SCRATCH_SP -= 0x1C;

    if (enemy->field_40 > 0) {
        if (work->field_EC8 != 1 && cfg->hp > 0 && (s16)work->field_0 != 0xD) {
            if (work->field_E92 > 0) {
                work->field_E92--;
            } else {
                func_actor_444000_8013C4B0(task);
            }
            if (work->field_E8C > 0) {
                work->field_E8C--;
            } else {
                func_actor_444000_8013C060(task);
            }
            if (work->field_E8E > 0) {
                work->field_E8E--;
            } else {
                func_actor_444000_8013CA60(task);
            }
            if (work->field_E90 > 0) {
                work->field_E90--;
            } else {
                func_actor_444000_8013D128(task);
            }
        }
    }
    if (enemy->field_40 <= 0) {
        if (cfg->hp <= 0) {
            enemy->field_40         = 1;
            D_actor_444000_80144A68 = 0;
        }
        if (enemy->field_40 <= 0 && (s16)work->field_0 != 0) {
            switch (work->field_F12) {
                case 0:
                    D_actor_444000_80144A68         = 1;
                    D_actor_444000_80161888.field_0 = 0;
                    D_actor_444000_80161888.field_1 = 0x2C;
                    D_actor_444000_80161888.field_2 = 3;
                    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_actor_444000_80161888, 0x7DB);
                    break;

                case 3:
                    if (cfg->hp > 0) {
                        if (work->field_F08 == 6) {
                            Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, 2, 0);
                        } else {
                            Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, 1, 0);
                        }
                        work->field_0 = 0x12;
                        SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x4020000A, 1);
                        SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x4020000D, 1);
                    }
                    break;
            }
            if (work->field_F12 < 0x100) {
                work->field_F12++;
            }
        }
    }

    if (work->field_2 != (s16)work->field_0) {
        work->field_4 = 1;
        work->field_6 = 0;
    } else {
        if (work->field_6 < 0x7FFF) {
            work->field_6++;
        }
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    handlers[(s16)work->field_0](task);

    if (work->field_0 < 2 || (s16)work->field_0 == 5 || (s16)work->field_0 == 0x12 ||
        (s16)work->field_0 == 0x13 || (s16)work->field_0 == 0xC) {
        enemy->node.flags              = 1;
        work->field_ECC[3]->node.flags = 1;
        work->field_ECC[0]->node.flags = 1;
        work->field_ECC[1]->node.flags = 1;
    } else if (work->field_EFA != 0) {
        if (Gp_NodeSlotMask(&work->field_ECC[3]->node) != 0) {
            Gp_AssignNodeSlot0(&enemy->node);
        }
        enemy->node.flags              = 8;
        work->field_ECC[3]->node.flags = 5;
        work->field_ECC[0]->node.flags = 5;
        work->field_ECC[1]->node.flags = 5;
    } else {
        if (Gp_NodeSlotMask(&enemy->node) != 0) {
            Gp_AssignNodeSlot0(&work->field_ECC[3]->node);
        }
        enemy->node.flags              = 1;
        work->field_ECC[3]->node.flags = 8;
        work->field_ECC[0]->node.flags = 8;
        work->field_ECC[1]->node.flags = 8;
    }

    if ((s16)work->field_0 != 0 && (s16)work->field_0 != 0x12 && (s16)work->field_0 != 0x13 &&
        (s16)work->field_0 != 5 && (s16)work->field_0 != 0xC && work->field_EFA == 1) {
        work->hits[0].obj.flags |= 0x8000;
    } else {
        work->hits[0].obj.flags &= 0x7FFF;
    }

    if ((s16)work->field_0 != 0 && (s16)work->field_0 != 0x12 && (s16)work->field_0 != 0x13 &&
        (s16)work->field_0 != 5 && (s16)work->field_0 != 0xC && work->field_EFA != 1) {
        work->hits[1].obj.flags |= 0x8000;
        work->hits[2].obj.flags |= 0x8000;
    } else {
        work->hits[1].obj.flags &= 0x7FFF;
        work->hits[2].obj.flags &= 0x7FFF;
    }

    if ((s16)work->field_0 != 0 && (s16)work->field_0 != 5 && (s16)work->field_0 != 0xC &&
        (s16)work->field_0 != 0x13 && (s16)work->field_0 != 0x12) {
        work->hits[3].obj.flags |= 0x8000;
        work->hits[4].obj.flags |= 0x8000;
        work->hits[5].obj.flags |= 0x8000;
    } else {
        work->hits[3].obj.flags &= 0x7FFF;
        work->hits[4].obj.flags &= 0x7FFF;
        work->hits[5].obj.flags &= 0x7FFF;
    }

    if ((s16)work->field_0 != 0 && (s16)work->field_0 != 5 && (s16)work->field_0 != 0xC &&
        (s16)work->field_0 != 0x13 && (s16)work->field_0 != 0x12) {
        work->hits[6].obj.flags |= 0x8000;
        work->hits[7].obj.flags |= 0x8000;
        work->hits[8].obj.flags |= 0x8000;
    } else {
        work->hits[6].obj.flags &= 0x7FFF;
        work->hits[7].obj.flags &= 0x7FFF;
        work->hits[8].obj.flags &= 0x7FFF;
    }

    Gp_ClearRec18Occupied(work->hits[0].recs);
    Gp_ClearRec18Occupied(work->hits[1].recs);
    Gp_ClearRec18Occupied(work->hits[2].recs);
    Gp_ClearRec18Occupied(work->hits[3].recs);
    Gp_ClearRec18Occupied(work->hits[4].recs);
    Gp_ClearRec18Occupied(work->hits[5].recs);
    Gp_ClearRec18Occupied(work->hits[6].recs);
    Gp_ClearRec18Occupied(work->hits[7].recs);
    Gp_ClearRec18Occupied(work->hits[8].recs);
    Gp_ClearRec18Occupied(work->recs2);

    if ((s16)work->field_0 != 5) {
        func_actor_444000_8013A77C(task);
    }

    SCRATCH_SP += 0x1C;
}

/// Per-frame tail of the arena fight: keeps the camera pulled back far enough
/// to hold both the boss and the player, then runs the state the task is in.
///
/// `field_E94` is the camera distance actually in use and `field_E96` the one
/// the current state asks for -- 0xBB8 while the boss is grappling (state 3),
/// 0xD48 for the close patterns and 0x1388 otherwise -- walked 0x32 per frame
/// until the two are within 0x33 of each other. `field_E98` is the companion
/// height the floor-marker helpers take.
///
/// Most states hand that pair to `ActorsShared80132cb8`, which drops the marker
/// under the boss. The exception is pattern 1 in state 9: it uses
/// `func_actor_444000_801371E8` instead, floors the player's own x at 0x2CEC,
/// and pushes the player back by the boss part's view-space depth less 0x7D0 --
/// part 4 of the boss model carried up the coordinate chain by
/// `Actor444000_LocalToView`. Whether the camera distance is then added to x or
/// subtracted from z is the same split: patterns other than 1-in-state-9 widen
/// x, the rest pull z in, and pattern 2 additionally floors z at 0x251C.
///
/// States 0, 5, 0xC, 0x12 and 0x13 skip all of that. State 0 -- and any state
/// the calls above dropped back to 0 -- also resets the two floor quads
/// `Gp_GridParams` keeps at vertices 24..31 to their default heights, and
/// state 5 still wants the marker.
///
/// The dispatch table is a local: `Task::state` picks the spawn state, this
/// tick, or `Gp_DestroyEnemy`.
void func_actor_444000_80142F28(Actor444000* arg0)
{
    void (*handlers[3])(GpEnemy*, Task*) = {
        (void (*)(GpEnemy*, Task*))func_actor_444000_8013AFF8,
        (void (*)(GpEnemy*, Task*))func_actor_444000_801423C4,
        Gp_DestroyEnemy,
    };
    SVECTOR          result;
    Actor444000Work* work;
    GpEnemy*         enemy;
    Task*            player;
    SVECTOR*         verts;
    s32              diff;
    s16              state;

    enemy  = arg0->field_20;
    player = Game_GetPtrSlot(3);
    work   = arg0->field_1C;
    if (work != NULL) {
        if (work->field_EE8[0] != NULL && work->field_EE8[0]->field_40 <= 0) {
            work->field_EE8[0] = NULL;
        }
        if (work->field_EE8[1] != NULL && work->field_EE8[1]->field_40 <= 0) {
            work->field_EE8[1] = NULL;
        }

        state = work->field_0;
        if (state == 3) {
            work->field_E96 = 0xBB8;
            work->field_E98 = 0x190;
        } else if (state == 9) {
            work->field_E96 = 0x1388;
            work->field_E98 = 0x190;
        } else if (state == 0x11) {
            work->field_E96 = 0x1388;
            work->field_E98 = 0x190;
        } else if (work->field_F08 != 0) {
            work->field_E96 = 0xD48;
            work->field_E98 = 0x190;
        } else {
            work->field_E96 = 0x1388;
            work->field_E98 = 0x190;
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

        state = work->field_0;
        if (state != 0) {
            /* Split so that `0x12` and `0x13` are not the innermost `&&` pair:
               `fold_range_test` would turn two adjacent constants into one
               `sltiu` range check. */
            if (state != 0x12) {
                if (state != 0x13 && state != 5 && state != 0xC) {
                    if (work->field_F08 == 1 && state == 9) {
                        func_actor_444000_801371E8((Task*)arg0, work->field_E94, 6);
                        {
                            GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;

                            if (playerCoord->coord.t[0] < 0x2CEC) {
                                playerCoord->coord.t[0] = 0x2CEC;
                            }
                        }
                        result.vx = result.vy = result.vz = 0;
                        Actor444000_LocalToView(((TmdObject*)arg0->extra)->coords + 4, &result);
                        {
                            GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;
                            s32            z           = result.vz - 0x7D0;

                            if (z < playerCoord->coord.t[2]) {
                                playerCoord->coord.t[2] = z;
                            }
                        }
                    } else {
                        ActorsShared80132cb8((Task*)arg0, work->field_E94, work->field_E98, 6);
                    }

                    if (work->field_F08 == 0 || (work->field_F08 == 1 && (s16)work->field_0 != 9)) {
                        GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;
                        GsCOORDINATE2* selfCoord   = ((TmdObject*)arg0->extra)->coords;
                        s32            x           = work->field_E94 + selfCoord->coord.t[0];

                        if (playerCoord->coord.t[0] < x) {
                            playerCoord->coord.t[0] = x;
                        }
                    } else {
                        GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;
                        GsCOORDINATE2* selfCoord   = ((TmdObject*)arg0->extra)->coords;
                        s32            z           = selfCoord->coord.t[2] - work->field_E94;

                        if (z < playerCoord->coord.t[2]) {
                            playerCoord->coord.t[2] = z;
                        }
                    }

                    if (work->field_F08 == 2) {
                        GsCOORDINATE2* playerCoord = ((TmdObject*)player->extra)->coords;

                        if (playerCoord->coord.t[0] < 0x251C) {
                            playerCoord->coord.t[2] = 0x251C;
                        }
                    }
                }
            }
            /* Re-read: the calls above can drop the fight back to state 0. The
               `goto` is what lets the `state == 0` edge reach the reset
               directly, as the ROM does. */
            if ((s16)work->field_0 != 0) {
                goto skipGrid;
            }
        }

        verts        = Gp_GridParams->field_8;
        verts[24].vy = 0x1F4;
        verts[25].vy = 0x1F4;
        verts[26].vy = 0x320;
        verts[27].vy = 0x320;
        verts[28].vy = 0x1F4;
        verts[29].vy = 0x1F4;
        verts[30].vy = 0x320;
        verts[31].vy = 0x320;

    skipGrid:
        state = work->field_0;
        if (state == 5) {
            ActorsShared80132cb8((Task*)arg0, work->field_E94, work->field_E98, 6);
        }
    }

    handlers[arg0->state](enemy, (Task*)arg0);
}
