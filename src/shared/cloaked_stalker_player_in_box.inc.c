/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Reports whether the player stands inside one of the actor's kind-1 boxes:
/// walks the `field_6FA` entries at `field_6B4` and, on the first kind-1 entry
/// whose box holds the player's world position (x between `field_8` and
/// `field_C`, z between `field_E` and `field_A`), parks its index in
/// `field_708` and answers 1. Otherwise it answers 0.
s32 stalkerPlayerInBox(Task* arg0)
{
    Actor402200Work* work;
    s16              count;
    s32              i;

    work  = arg0->work;
    count = work->field_6FA;
    for (i = 0; i < count; i++) {
        if (work->field_6B4[i].field_0 == 1) {
            if ((work->field_6B4[i].field_8 < Player_Status.coordMtx->t[0]) &&
                (Player_Status.coordMtx->t[0] < work->field_6B4[i].field_C)) {
                if ((Player_Status.coordMtx->t[2] < work->field_6B4[i].field_A) &&
                    (work->field_6B4[i].field_E < Player_Status.coordMtx->t[2])) {
                    work->field_708 = i;
                    return 1;
                }
            }
        }
    }
    return 0;
}
