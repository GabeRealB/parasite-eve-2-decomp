/* Part of the hopping enemy library; see hopping_enemy.h. */

/// After 0x18 frames sets model flag 2, clears the frame counter, sets
/// `field_451` and advances the state.
void hopperDeathTurnTranslucent(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;
    TmdObject*       model;

    work            = (Actor341700Work*)arg0->work;
    model           = arg0->extra.tmd;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->flags    = model->flags | TMD_OBJECT_SEMI_TRANS;
        work->field_412 = 0U;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}
