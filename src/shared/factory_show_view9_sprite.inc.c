/* Part of the factory lift library; see factory_lift.h. */

/// In the day stage only, shows or hides sprite batch 1 of view 9's background
/// layer according to show.
void factoryShowView9Sprite(s32 show)
{
    GameSession*     g;
    GameLocationKey* sess;
    SpriteBatch*     batches;

    g    = gGameSession;
    sess = &g->location.loc;
    if (sess->stage == 2) {
        batches = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].field_0[sess->area - 1][8].field_4;
        if (!(show & 0xFF)) {
            batches[1].hidden = 1;
            return;
        }
        batches[1].hidden = 0;
    }
}
