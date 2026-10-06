/* Part of the Zebra and Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Colours the actor from its model's second coordinate: takes a 0x10-byte
/// `VECTOR` off the scratch stack, fills it with that coordinate's world
/// position and hands it to `worldCoordUpdateActorColor` for the task's `spawnArg2`,
/// with zero for the unused arguments.
void stalkerZebraIvoryUpdateColor(Task* task)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    worldCoordUpdateActorColor(task->spawnArg2.pointer, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}
