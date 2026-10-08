/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Flings a detached husk model from body coordinate 4 with its placement textures.
///
/// Requires a live model with coordinate 4 and an owning enemy whose placement
/// index exists in the current area variant. Publishes the borrowed model
/// source for the bank-4 effect descriptor; the carrier must stay loaded while
/// the effect uses it. Allocation failure leaves no husk. On success, both
/// primitive-buffer halves are rebuilt after applying texture-page and CLUT
/// offsets. The parent actor retains ownership of its own model and work.
static void _maggotCaterpillarSpawnHusk(Task* actor)
{
    enum { MAGGOT_CATERPILLAR_HUSK_SIZE_ARGUMENT = 0x100 };
    GameLocationKey  location;
    GameLocationKey* currentLocation;
    u8               currentView;
    AreaVariant*     areaVariant;
    AreaPlacement*   placement;
    EffectWork*      huskEffect;
    TmdObject*       huskModel;
    s32              placementIndex;
    u32              placeKey;

    D_80067704[0] = &gMaggotCaterpillarHuskModel;
    huskEffect    = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, actor->extra.tmd->coords + 4, MAGGOT_CATERPILLAR_HUSK_SIZE_ARGUMENT, NULL);
    if (huskEffect == NULL) {
        return;
    }
    currentLocation = &gGameSession->location.loc;
    placeKey        = ((Enemy*)actor->spawnArg2.pointer)->placeKey;
    huskModel       = huskEffect->task->extra.tmd;
    location.stage  = currentLocation->stage;
    location.area   = currentLocation->area;
    location.room   = currentLocation->room;
    currentView     = currentLocation->view;
    placementIndex  = placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    location.view   = currentView;
    areaSyncLocationVariant(&location);
    areaVariant = areaGetVariant(&location);
    // Preserve the table helper's scaled-offset-first address calculation.
    placement                    = gpAreaPlaceAt(areaVariant->placements, placementIndex);
    huskModel->texturePageOffset = placement->texturePageOffset;
    huskModel->clutRowOffset     = placement->clutRowOffset;
    if (huskModel->buffer != NULL) {
        tmdBuildBufferHalf(huskModel);
        tmdBuildBufferHalf(huskModel);
    }
}
