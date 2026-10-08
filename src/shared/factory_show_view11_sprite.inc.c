/* Part of the factory lift library; see factory_lift.h. */

void FACTORY_ROOM_INSTANCE_SET_VIEW11_SPRITE_VISIBLE(s32 visible)
{
    enum {
        FACTORY_ROOM_POWER_SCENE_SPRITE_VIEW  = 11,
        FACTORY_ROOM_POWER_SCENE_SPRITE_BATCH = 1,
        FACTORY_ROOM_VISIBILITY_BYTE_MASK     = 0xFF
    };
    const GameSession*     session;
    const GameLocationKey* location;
    SpriteBatch*           view11Batches;

    session  = gGameSession;
    location = &session->location.loc;
    // Only the daytime background has this sprite, including in the night instance.
    if (location->stage == GAME_STAGE_DRYFIELD) {
        view11Batches = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1][FACTORY_ROOM_POWER_SCENE_SPRITE_VIEW - 1].batches;
        if (!(visible & FACTORY_ROOM_VISIBILITY_BYTE_MASK)) {
            view11Batches[FACTORY_ROOM_POWER_SCENE_SPRITE_BATCH].hidden = true;
            return;
        }
        view11Batches[FACTORY_ROOM_POWER_SCENE_SPRITE_BATCH].hidden = false;
    }
}
