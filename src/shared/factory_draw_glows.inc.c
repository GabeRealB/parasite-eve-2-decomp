/* Part of the factory library; see factory_lift.h. */

/// Per-frame effect: draws up to three glowing discs at fixed points in the
/// room. The draw set is selected by the stage-visit byte
/// `gGameSession->location.loc.view` taken as a bit index, and each group also gates on a
/// story flag, so a disc only appears on the visits and after the event that
/// the flag records.
void factoryDrawGlows(Task* task)
{
    s32 state;

    state = 1 << gGameSession->location.loc.view;
#if DRYFIELD_TIME == DRYFIELD_NIGHT
    actorRenderComposeCoord(task->extra.coordBody->coord); /* the night build also refreshes the task's matrix */
#endif
    if (GameFlag_GetNibble(GAME_FLAG_FACTORY_POWER_ON) != 0 && (state & 0x15068) != 0) {
        glowDrawTintedDisc(&gFactoryGlowPos48, 0x100, 0x3660);
    }
    if (state & 0xF26C4) {
        if (GameFlag_GetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) == 1) {
            glowDrawTintedDisc(&gFactoryGlowPos4A1, 0x80, 0x5A00);
        } else if (GameFlag_GetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) == 2) {
            glowDrawTintedDisc(&gFactoryGlowPos4A2, 0x80, 0x50A0);
        }
    }
}
