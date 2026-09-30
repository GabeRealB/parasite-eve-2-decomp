/* Part of the factory lift library; see factory_lift.h. */

/// As factoryShowView9Sprite, for view 11's layer.
void factoryShowView11Sprite(s32 show)
{
    GameSession*     g;
    GameLocationKey* sess;
    SpriteBatch*     batches;

    g    = gGameSession;
    sess = &g->location.loc;
    if (sess->stage == 2) {
        batches = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].field_0[sess->area - 1][10].field_4;
        if (!(show & 0xFF)) {
            batches[1].hidden = 1;
            return;
        }
        batches[1].hidden = 0;
    }
}
