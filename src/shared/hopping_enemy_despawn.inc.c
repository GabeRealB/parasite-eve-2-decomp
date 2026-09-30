/* Part of the hopping enemy library; see hopping_enemy.h. */

/// After 0x24 frames destroys the enemy, first telling slot-4 task 0 with
/// message 0x13F4 when in place 1 of stage 4 areas 0x27/0x28.
void hopperDespawn(Task* arg0)
{
    Actor341700Work* work;
    u16              ticks;

    work            = (Actor341700Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((gGameSession->location.loc.stage == 4) && ((u32)(gGameSession->location.loc.area - 0x27) < 2U) && (gGameSession->location.loc.variant == 1)) {
            Gp_DispatchMsg(Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
    }
}
