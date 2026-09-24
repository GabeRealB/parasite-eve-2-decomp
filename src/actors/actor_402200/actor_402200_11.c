#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/wipsys.h"

#include "gameplay/3A34.h"

#include "actors/actor_402200.h"

/// Reports whether the player stands inside one of the actor's kind-1 boxes:
/// walks the `field_6FA` entries at `field_6B4` and, on the first kind-1 entry
/// whose box holds the player's world position (x between `field_8` and
/// `field_C`, z between `field_E` and `field_A`), parks its index in
/// `field_708` and answers 1. Otherwise it answers 0.
s32 func_actor_402200_80132D78(Actor402200* arg0)
{
    Actor402200Work* work;
    s16              count;
    s32              i;

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

/// Parks the actor's target position off the player (`gameGetPtrSlot(3)`).
/// In state 3 it takes `field_6E6` from the player's heading and places the
/// target 0x5AA behind the player, raising bit 0x4000 of `field_5BA` and
/// `field_5DA`; in state 4 it rolls an angle from `Gp_LcgState` (anywhere, or
/// within a quarter turn either side while `field_6E8` is clear), derives
/// `field_5DC` / `field_5E0` from it, adds the player's heading and places the
/// target 0x4B out along the result, raising bit 0x4000 of `field_5BA`.
void func_actor_402200_80132E34(Actor402200* arg0)
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
        gte_rtv0();
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
