/* Part of the fireball library; see fireball.h. */

/// Unless an event is running, draws from the gameplay LCG and on one call in
/// four spawns effect `D_80115728` on `arg0` with a random horizontal vector;
/// `arg1` is or-ed into the spawn flags.
void fireballSpawnEmber(GfxCoord* arg0, s32 arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;
    s32     ang;

    if (Gp_State1C->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            ang         = ((u32)Gp_LcgState >> 16) & 0xF80;
            memset(&sp18, 0, sizeof(sp18));
            sp18.vx = (u32)(rcos(ang) * 5) >> 5;
            sp18.vz = (u32)(rsin(ang) * 5) >> 5;
            sp10    = sp18;
            Gp_SpawnEff(D_80115728, arg0, arg1 | 0x20100200, &sp10);
        }
    }
}
