/* Part of the Mad Chaser library; see mad_chaser.h. */

/// After 0x24 frames destroys the enemy, first telling slot-4 task 0 with
/// message 0x13F4 when in place 1 of stage 4 areas 0x27/0x28.
void madChaserDespawn(Task* arg0)
{
    MadChaserWork* work;
    u16            ticks;

    work            = (MadChaserWork*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((gGameSession->location.loc.stage == 4) && ((u32)(gGameSession->location.loc.area - 0x27) < 2U) && (gGameSession->location.loc.variant == 1)) {
            Gp_DispatchMsg(Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
    }
}
