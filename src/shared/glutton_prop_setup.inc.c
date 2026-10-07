/* Part of the Glutton library; see glutton.h. */

/// Applies the host's area-placement texture offsets to an escort model.
///
/// Requires a live TMD body and parent enemy. The parent's place index must
/// select a placement in the synchronized variant of the current session
/// location. Rebuilds both primitive-buffer halves when present, then advances
/// the escort task. The enemy argument is unused.
static void _gluttonPropSetup(Enemy* enemy, Task* task)
{
    GameLocationKey        location;
    const GameLocationKey* sessionLocation;
    AreaVariant*           variant;
    AreaPlacement*         placement;
    TmdObject*             model;
    s32                    placeIndex;
    u32                    placeKey;

    sessionLocation = &gGameSession->location.loc;
    placeKey        = ((Enemy*)task->parent->spawnArg2.pointer)->placeKey;
    model           = task->extra.tmd;
    location.stage  = sessionLocation->stage;
    location.area   = sessionLocation->area;
    location.room   = sessionLocation->room;
    placeIndex      = placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    location.view   = sessionLocation->view;
    areaSyncLocationVariant(&location);
    variant = areaGetVariant(&location);
    // Apply the host's placement to this escort's existing primitive stream.
    placement                = gpAreaPlaceAt(variant->placements, placeIndex);
    model->texturePageOffset = placement->texturePageOffset;
    model->clutRowOffset     = placement->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    task->state++;
}
