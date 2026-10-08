/* Part of the factory lift library; see factory_lift.h. */

void FACTORY_ROOM_INSTANCE_SET_VIEW9_SPRITE_VISIBLE(s32 visible)
{
    enum {
        FACTORY_ROOM_POWER_SPRITE_VIEW    = 9,
        FACTORY_ROOM_POWER_SPRITE_BATCH   = 1,
        FACTORY_ROOM_VISIBILITY_BYTE_MASK = 0xFF
    };
    const GameSession*     session;
    const GameLocationKey* location;
    SpriteBatch*           view9Batches;

    session  = gGameSession;
    location = &session->location.loc;
    // The separate power sprite belongs to the daytime background in both instances.
    if (location->stage == GAME_STAGE_DRYFIELD) {
        view9Batches = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1][FACTORY_ROOM_POWER_SPRITE_VIEW - 1].batches;
        if (!(visible & FACTORY_ROOM_VISIBILITY_BYTE_MASK)) {
            view9Batches[FACTORY_ROOM_POWER_SPRITE_BATCH].hidden = true;
            return;
        }
        view9Batches[FACTORY_ROOM_POWER_SPRITE_BATCH].hidden = false;
    }
}
