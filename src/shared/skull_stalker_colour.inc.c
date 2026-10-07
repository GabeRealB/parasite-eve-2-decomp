/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Updates the model's lighting and color blend at its second part's composed origin.
///
/// The live enemy must own the task; coordinate 1 must already be composed.
/// Reserves one VECTOR on the scratch stack for the XYZ sample in game units;
/// the lighting query takes additional scratch and retains no sample pointer.
static void _skullStalkerUpdateColor(Enemy* enemy, Task* task)
{
    GfxCoord* sampleCoord;
    void**    cursorSlot;
    VECTOR*   scratchHead;
    VECTOR*   worldPosition;

    sampleCoord                         = &task->extra.tmd->coords[1];
    cursorSlot                          = SCRATCH_HEAD_ADDR;
    scratchHead                         = SCRATCH_HEAD_AT(cursorSlot, VECTOR);
    worldPosition                       = scratchHead - 1;
    worldPosition->vx                   = sampleCoord->workm.t[0];
    worldPosition->vy                   = sampleCoord->workm.t[1];
    worldPosition->vz                   = sampleCoord->workm.t[2];
    SCRATCH_HEAD_AT(cursorSlot, VECTOR) = worldPosition;
    worldCoordUpdateActorColor(enemy, worldPosition, 0, 0);
    SCRATCH_POP_BYTES_AT(cursorSlot, sizeof(*worldPosition));
}
