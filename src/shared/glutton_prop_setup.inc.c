/* Part of the Glutton library; see glutton.h. */

/// Setup state of the handler table `D_actor_444000_80131E90`: look up the
/// area placement the parent's spawn record names (its top nibble) under the
/// current session location, give the model that placement's texture page and
/// CLUT, run its stream twice when it has one, and step the task on.
void gluttonPropSetup(Enemy* enemy, Task* task)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    u8               areaByte0;
    AreaVariant*     layout;
    AreaPlacement*   entry;
    TmdObject*       model;
    s32              idx;
    u32              raw;

    sessionKey = &gGameSession->location.loc;
    raw        = ((Enemy*)task->parent->spawnArg2.pointer)->placeKey;
    model      = task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> ENEMY_PLACE_INDEX_SHIFT;
    key.view   = areaByte0;
    areaSyncLocationVariant(&key);
    layout = areaGetVariant(&key);
    /* offset + base, not `&layout->placements[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry                    = gpAreaPlaceAt(layout->placements, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    task->state++;
}
