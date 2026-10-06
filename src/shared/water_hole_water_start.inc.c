/* Part of the water hole library; see water_hole.h. */

/// Prepares the actor buffer used for water primitives and advances to drawing.
///
/// Borrows the live water task in state zero. With no saved companion, the
/// drawer uses actor buffer 2 and this clears its session marker `field_80`;
/// otherwise it uses buffer 1 and this clears `field_7E`. Their nonzero meanings
/// are unproven. Previous uses of the selected buffer must have ended before
/// the drawing state overwrites it. No buffer is allocated or cleared here.
static void _waterHoleWaterStart(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    task->state = task->state + 1;
}
