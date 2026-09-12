#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s16 D_actor_444000_80144A68;
extern s16 D_actor_444000_80144A70;
extern s32 Gp_LcgState;

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013A3AC);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013A958);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013AFF8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013C060);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013C4B0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013CA60);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013D128);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013E058);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013EC84);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_8013FB74);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_801404C0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80140BBC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80140E28);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_801411C8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80141618);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80141DFC);

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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_801423C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_5", func_actor_444000_80142F28);
