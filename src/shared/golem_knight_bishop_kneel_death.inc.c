/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Sequence 0xA, the entrance: state 0 picks animation 0xE (and state 1) when
/// `field_6F0` is 1, otherwise 0x12 and state 2, and arms the timers
/// `field_6DA`..`field_6DE`; states 1 and 2 wait for the frame counter to reach
/// 0x10 or 0x16, then park 2 in the context's `field_30` and drop back to 0.
void golemKnightBishopKneelDeathSeq(Task* arg0)
{
    Actor402200Work* work;
    s16              state;
    s32              next;

    work  = arg0->work;
    state = work->field_6CE;
    switch (state) {
        case 0:
            next = work->field_6F0;
            if (next == 1) {
                work->field_6C0 = 0xE;
                work->field_6CE = next;
            } else {
                work->field_6C0 = 0x12;
                work->field_6CE = 2;
            }
            work->field_6DA = 1;
            work->field_6DC = 0xA;
            work->field_6DE = 5;
            break;
        case 1:
            if (work->field_6C4 >= 0x10) {
                arg0->state     = 2;
                work->field_6CE = 0;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x16) {
                arg0->state     = state;
                work->field_6CE = 0;
            }
            break;
    }
}
