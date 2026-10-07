/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Spawns the husk model effect at node 4 and gives it the texture page and
/// CLUT of the enemy's area placement.
void maggotCaterpillarSpawnHusk(Task* actor)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    u8               areaByte0;
    AreaVariant*     layout;
    AreaPlacement*   entry;
    EffectWork*      eff;
    TmdObject*       model;
    s32              idx;
    u32              raw;

    D_80067704[0] = &gMaggotCaterpillarHuskModel;
    eff           = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, actor->extra.tmd->coords + 4, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = &gGameSession->location.loc;
    raw        = ((Enemy*)actor->spawnArg2.pointer)->placeKey;
    model      = eff->task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    areaSyncLocationVariant(&key);
    layout = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&layout->placements[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry                    = gpAreaPlaceAt(layout->placements, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}
