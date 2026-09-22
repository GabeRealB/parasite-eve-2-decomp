#include "common.h"
#include "main/wipsys.h"
#include "actors/actors_shared_80132d78.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "actors/actor_402200.h"
#include "actors/actors_shared_80137ca4.h"
#include "psyq/inline_c.h"

/// `rtv0`: rotate V0 through the loaded rotation matrix, no translation. The
/// `inline_c.h` macro of that name assembles to a different word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

s32 ActorsShared80132d78(ActorShared80132d78* arg0)
{
    ActorShared80132d78Work* work;
    s16                      count;
    s32                      i;

    work  = arg0->field_1C;
    count = work->field_6FA;
    for (i = 0; i < count; i++) {
        if (work->field_6B4[i].field_0 == 1) {
            if ((work->field_6B4[i].field_8 < Player_Status.coordMtx->t[0]) &&
                (Player_Status.coordMtx->t[0] < work->field_6B4[i].field_C)) {
                if ((Player_Status.coordMtx->t[2] < work->field_6B4[i].field_A) &&
                    (work->field_6B4[i].field_E < Player_Status.coordMtx->t[2])) {
                    work->field_708 = i;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/// Cue word `func_actor_402200_8013539C` and `func_actor_402200_801354B0`
/// queue, a separate `D_` symbol in the overlay's data 0x48 past the cue-id
/// table `D_actor_402200_80138420`.

/// Cue word the fade-out in `func_actor_402200_80134968` queues.

/// Cue-id table, indexed from `Actor402200Work::field_712`.

/// Cue words `func_actor_402200_80134194` queues next to
/// `D_actor_402200_80138468`.

/// Remaining-enemy count; a grab only starts while it is positive.

/// Difficulty index into `D_actor_402200_80153C0C`.

/// Per-difficulty HP above which the player always breaks the grab.

/// Animation block the grab's 0x3FF messages hand the player.

/// `func_800FDB18` argument record for the grab's finishing spark.

void ActorsShared80132e34(Actor402200* arg0)
{
    u8*                       head;
    Actor402200OffsetScratch* sc;
    Actor402200Work*          work;
    Actor402200Coord*         coord;
    u32                       random;
    s32                       angle;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(Actor402200OffsetScratch);
    sc                    = (Actor402200OffsetScratch*)(head - sizeof(Actor402200OffsetScratch));
    work                  = arg0->field_1C;
    if (work->field_6CE == 3) {
        coord           = ((Actor402200*)gameGetPtrSlot(3))->field_2C->field_8;
        work->field_6E6 = ratan2(coord->field_0.coord.m[0][2], coord->field_0.coord.m[2][2]) & 0xFFF;
        sc->in.vz       = -0x5AA;
        sc->in.vx       = 0;
        sc->in.vy       = 0;
        gte_SetRotMatrix(&coord->field_0.coord);
        gte_ldv0(&sc->in);
        gte_rtv0_real();
        gte_stlvnl(&sc->out);
        work->field_6A4  = Player_Status.coordMtx->t[0] + sc->out.vx;
        work->field_6A8  = Player_Status.coordMtx->t[1];
        work->field_6AC  = Player_Status.coordMtx->t[2] + sc->out.vz;
        work->field_5DE  = -0x3E8;
        work->field_5E0  = -0x7D0;
        work->field_5DC  = 0;
        work->field_5BA |= 0x4000;
        work->field_5DA |= 0x4000;
    } else if (work->field_6CE == 4) {
        if (work->field_6E8 != 0) {
            Gp_LcgState     = (Gp_LcgState * 5) + 0x71357911;
            work->field_6E6 = (Gp_LcgState >> 16) & 0xFFF;
        } else {
            random      = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState = random;
            angle       = (random >> 16) & 0x3FF;
            if (!((random >> 16) & 0x400)) {
                angle = -angle;
            }
            work->field_6E6 = angle;
        }
        work->field_5DC  = (u32)(rsin(work->field_6E6) * 0x7D) >> 8;
        work->field_5DE  = -0x3E8;
        work->field_5E0  = (u32)(rcos(work->field_6E6) * 0x7D) >> 8;
        coord            = ((Actor402200*)gameGetPtrSlot(3))->field_2C->field_8;
        work->field_6E6  = (work->field_6E6 + (ratan2(coord->field_0.coord.m[0][2], coord->field_0.coord.m[2][2]) & 0xFFF)) & 0xFFF;
        sc->in.vx        = (u32)(rsin(work->field_6E6) * 0x4B) >> 8;
        sc->in.vz        = (u32)(rcos(work->field_6E6) * 0x4B) >> 8;
        work->field_6A4  = Player_Status.coordMtx->t[0] + sc->in.vx;
        work->field_6A8  = Player_Status.coordMtx->t[1];
        work->field_6AC  = Player_Status.coordMtx->t[2] + sc->in.vz;
        work->field_5BA |= 0x4000;
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(Actor402200OffsetScratch);
}
