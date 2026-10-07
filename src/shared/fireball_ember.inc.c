#include "main/random.h"

/* Part of the fireball library; see fireball.h. */

/// Unless an event is running, draws from the gameplay LCG and on one call in
/// four spawns effect `gRoomEffectMoteId` on `arg0` with a random horizontal vector;
/// `arg1` is or-ed into the spawn flags.
void fireballSpawnEmber(GfxCoord* arg0, s32 arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;
    s32     ang;

    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 3) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            ang             = (gRandomLcgState >> 16) & 0xF80;
            memset(&sp18, 0, sizeof(sp18));
            sp18.vx = (u32)(rcos(ang) * 5) >> 5;
            sp18.vz = (u32)(rsin(ang) * 5) >> 5;
            sp10    = sp18;
            effectSpawn(gRoomEffectMoteId, arg0, arg1 | 0x20100200, &sp10);
        }
    }
}
