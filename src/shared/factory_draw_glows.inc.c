/* Part of the factory library; see factory_lift.h. */

void FACTORY_DRAW_GLOWS_TASK(Task* task)
{
    enum {
        // Bits select 1-based logical room views, before camera/image remapping.
        FACTORY_GLOW_POWER_VIEW_MASK      = (1 << 3) | (1 << 5) | (1 << 6) | (1 << 12) | (1 << 14) | (1 << 16),
        FACTORY_GLOW_LAMP_VIEW_MASK       = (1 << 2) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 10) | (1 << 13) | (1 << 16) | (1 << 17) | (1 << 18) | (1 << 19),
        FACTORY_GLOW_LAMP_FIRST_POSITION  = 1, // Lamp progress after the power scene
        FACTORY_GLOW_LAMP_SECOND_POSITION = 2, // Lamp progress after the lamp scene or sound command
        // Pixel radii are scale * 64 / (camera Z / 4 + 1).
        FACTORY_GLOW_POWER_RADIUS_SCALE = 0x100,
        FACTORY_GLOW_LAMP_RADIUS_SCALE  = 0x80,
        // High nibble is the odd-frame flicker shift; low three are RGB * 16.
        FACTORY_GLOW_POWER_COLOR       = (3 << 12) | (6 << 8) | (6 << 4),
        FACTORY_GLOW_LAMP_FIRST_COLOR  = (5 << 12) | (10 << 8),
        FACTORY_GLOW_LAMP_SECOND_COLOR = (5 << 12) | (10 << 4),
    };
    s32 viewBit;

    viewBit = 1 << gGameSession->location.loc.view;
#if DRYFIELD_TIME == DRYFIELD_NIGHT
    // Preserve the night task's coordinate refresh even though the glows use world points.
    actorRenderComposeCoord(task->extra.coordBody->coord);
#endif
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) != 0 && (viewBit & FACTORY_GLOW_POWER_VIEW_MASK) != 0) {
        _glowDrawTintedDisc(&gFactoryGlowPos48, FACTORY_GLOW_POWER_RADIUS_SCALE, FACTORY_GLOW_POWER_COLOR);
    }
    // Lamp progress selects one position independently of the power flag.
    if (viewBit & FACTORY_GLOW_LAMP_VIEW_MASK) {
        if (gameFlagGetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) == FACTORY_GLOW_LAMP_FIRST_POSITION) {
            _glowDrawTintedDisc(&gFactoryGlowPos4A1, FACTORY_GLOW_LAMP_RADIUS_SCALE, FACTORY_GLOW_LAMP_FIRST_COLOR);
        } else if (gameFlagGetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) == FACTORY_GLOW_LAMP_SECOND_POSITION) {
            _glowDrawTintedDisc(&gFactoryGlowPos4A2, FACTORY_GLOW_LAMP_RADIUS_SCALE, FACTORY_GLOW_LAMP_SECOND_COLOR);
        }
    }
}
