/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Updates body lighting and colour from part 1's cached translation.
///
/// Requires a live model with at least two coordinates and its Enemy spawn
/// argument. Copies cached XYZ without refreshing the part; the lighting
/// query interprets the sample as world coordinates. Reserves one VECTOR on
/// the initialized scratch stack, plus the lighting query's nested workspace,
/// and releases it before returning. No sample pointer survives the call.
static void _stalkerZebraIvoryUpdateColor(Task* task)
{
    GfxCoord* sampleCoord;
    void**    scratchCursor;
    u8*       scratchTop;
    VECTOR*   samplePosition;

    sampleCoord                          = &task->extra.tmd->coords[1];
    scratchCursor                        = SCRATCH_HEAD_ADDR;
    scratchTop                           = SCRATCH_HEAD_AT(scratchCursor, void);
    samplePosition                       = (VECTOR*)(scratchTop - sizeof(VECTOR));
    samplePosition->vx                   = sampleCoord->workm.t[0];
    samplePosition->vy                   = sampleCoord->workm.t[1];
    samplePosition->vz                   = sampleCoord->workm.t[2];
    SCRATCH_HEAD_AT(scratchCursor, void) = samplePosition;
    worldCoordUpdateActorColor(task->spawnArg2.pointer, samplePosition, 0, 0);
    SCRATCH_POP_BYTES_AT(scratchCursor, sizeof(VECTOR));
}
