#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_patio.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

#define SCRATCH_SP (*(u32*)G_SCRATCH_HEAD)

extern s32 Gp_LcgState;

/// The 14 anchor points of the patio's fountain spray, in the room object's own
/// space. The first three double as the jitter centres for the mist burst.
extern SVECTOR D_acropolis_patio_80182DDC[14];

/// Per-anchor camera-view mask, one bit per 1-based `GameSession::field_4`
/// view: anchor `i` only draws while the room is being seen from a view its
/// mask names.
extern u16 D_acropolis_patio_80182E4C[14];

/// Slow left turn-in-place: nudges the player's facing angle by -0x80 each
/// frame for 0x10 frames, wrapping it back into [-0x800, 0x800), then kills
/// itself. Any state other than 0 or 1 kills the task immediately.
void func_acropolis_patio_8017E054(Task* task)
{
    GameActor* actor;
    s16        angle;

    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;

    switch (task->state) {
        case 0:
            task->killCountdown = 0x10;
            task->state++;
            /* fallthrough */
        case 1:
            angle = actor->field_52 - 0x80;
            if (angle < -0x800) {
                angle = actor->field_52 + 0xF80;
            }
            actor->field_52 = angle;
            task->killCountdown--;
            if (task->killCountdown > 0) {
                break;
            }
            /* fallthrough */
        default:
            Task_Kill(task);
            break;
    }
}

/// Lights the patio fountain: a one-shot burst that seeds every jet and its
/// mist, then leaves the task idle for the rest of the room.
///
/// The 14 anchors of `D_acropolis_patio_80182DDC` are handed to `Gp_SpawnEff`
/// as three runs of effect 0x60087, each run differing only in the high bits of
/// the spawn argument - `0x03000200` for the three main jets, `0x02000000` for
/// the next four and a plain `0x100` for the remaining seven - so the anchor
/// index rides in the low byte and the flags pick the jet's size and blend.
///
/// The three main jets then get three puffs of mist each (effect 0x6008F).
/// Every puff re-uses the task's own `GpEffWork.field_10` triple as a scratch
/// offset: three 11-bit LCG draws centred on 0x400 give a `+/-0x400` jitter,
/// which is added to the jet's anchor before the spawn reads it. The work block
/// is scratch, not state - each spawn copies the vector out immediately - so
/// all nine puffs share it.
void func_acropolis_patio_8017E100(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    s32            i;
    s32            j;

    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->field_8;

    if (task->state == 0) {
        for (i = 0; i < 3; i++) {
            Gp_SpawnEff(0x60087, objCoord, i + 0x03000200, &D_acropolis_patio_80182DDC[i]);
        }
        for (i = 3; i < 7; i++) {
            Gp_SpawnEff(0x60087, objCoord, i + 0x02000000, &D_acropolis_patio_80182DDC[i]);
        }
        for (i = 7; i < 0xE; i++) {
            Gp_SpawnEff(0x60087, objCoord, i + 0x100, &D_acropolis_patio_80182DDC[i]);
        }
        task->state++;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_10  = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_12  = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_14  = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                work->field_10 += D_acropolis_patio_80182DDC[i].vx;
                work->field_12 += D_acropolis_patio_80182DDC[i].vy;
                work->field_14 += D_acropolis_patio_80182DDC[i].vz;
                Gp_SpawnEff(0x6008F, objCoord, i, (SVECTOR*)&work->field_10);
            }
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_4", func_acropolis_patio_8017E324);

/// Draws and drifts one puff of the fountain's mist for the current frame.
///
/// The puff only exists for the camera views its anchor's mask in
/// `D_acropolis_patio_80182E4C` names, and the whole draw stops once
/// `Gp_State1C::field_4` reaches 4 (the room is fading out).
///
/// `GpEffWork::field_20` is the puff's mode and the per-frame step in
/// `field_10`..`field_14` is its velocity. In drift mode (0) the velocity is
/// re-rolled every frame as `0x10 - rand[0,0x1F]` per axis, a random walk
/// centred just above zero, and a 1-in-60 draw flips the puff into gather
/// mode. In gather mode (non-zero) the velocity is instead re-aimed at the
/// jet's own anchor once every fourth frame - the normalised direction from
/// the puff to the anchor, scaled by `GPF` at `dp = 0x20` - and jittered by
/// `+/-8` per axis every frame, with a 1-in-120 draw returning it to drift.
/// The velocity is then added to the effect coordinate's translation.
///
/// The result is projected through `GsWSMATRIX` into a 0xC-byte scratch frame
/// and drawn as a single grey `TILE_1` whose level is a fresh `rand[0,0xC0)`,
/// so the mist shimmers; the tile is dropped entirely inside `otz` 0x11.
void func_acropolis_patio_8017E730(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    ApMistScratch* sc;
    SVECTOR*       dir;
    SVECTOR*       anchors;
    TILE_1*        prim;
    u32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 < 4 &&
        ((D_acropolis_patio_80182E4C[task->spawnArg1] >> ((u8)Game_Session->field_4 - 1)) & 1)) {
        sc = (ApMistScratch*)(SCRATCH_SP -= 0xC);
        Gp_UpdateCoord(coord);
        if (task->state == 0) {
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_14 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state++;
        }
        if (work->field_20 != 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                anchors        = D_acropolis_patio_80182DDC;
                dir            = (SVECTOR*)&work->field_10;
                work->field_10 = *(u16*)&anchors[task->spawnArg1].vx -
                                 *(u16*)&coord->coord.t[0];
                work->field_12 = *(u16*)&anchors[task->spawnArg1].vy -
                                 *(u16*)&coord->coord.t[1];
                work->field_14 = *(u16*)&anchors[task->spawnArg1].vz -
                                 *(u16*)&coord->coord.t[2];
                VectorNormalSS(dir, dir);
                gte_lddp(0x20);
                gte_ldsv(dir);
                gte_gpf12_real();
                gte_stsv(dir);
            }
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_10 -= (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_12 -= (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_14 -= (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 0x78) == 0) {
                work->field_20 = 0;
            }
        } else {
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_10 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_12 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_14 = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 0x3C) == 0) {
                work->field_20 = 1;
            }
        }
        coord->coord.t[0] += work->field_10;
        coord->coord.t[1] += work->field_12;
        coord->coord.t[2] += work->field_14;
        coord->flg         = 0;
        sc->pos.vx         = *(u16*)&coord->workm.t[0];
        sc->pos.vy         = *(u16*)&coord->workm.t[1];
        sc->pos.vz         = *(u16*)&coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&sc->pos);
        gte_rtps_real();
        prim           = (TILE_1*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setTile1(prim);
        gte_stsxy(&prim->x0);
        gte_stszotz(&sc->otz);
        if (sc->otz >= 0x11) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            level       = (u32)Gp_LcgState >> 16;
            level      %= 0xC0;
            prim->r0    = level;
            prim->g0    = level;
            prim->b0    = level;
            addPrim((u_long*)(((((u32)sc->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 0, sc->otz);
        }
        SCRATCH_SP += 0xC;
    }
}
