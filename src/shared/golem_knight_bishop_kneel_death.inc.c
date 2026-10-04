/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Death on the ground: step 0 takes animation 0xE (and step 1) when
/// `downedPose` is 1, otherwise 0x12 and step 2, and starts an appearance
/// fade; steps 1 and 2 wait for `animFrame` to reach 0x10 or 0x16, then move
/// the task to its dead state (2) and drop back to step 0.
void golemKnightBishopKneelDeathSeq(Task* arg0)
{
    GolemKnightBishopWork* work;
    s16                    state;
    s32                    next;

    work  = arg0->work;
    state = work->step;
    switch (state) {
        case 0:
            next = work->downedPose;
            if (next == 1) {
                work->anim = 0xE;
                work->step = next;
            } else {
                work->anim = 0x12;
                work->step = 2;
            }
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->translucencyFadeFrames = 0xA;
            work->colorBlendFadeFrames   = 5;
            break;
        case 1:
            if (work->animFrame >= 0x10) {
                arg0->state = 2;
                work->step  = 0;
            }
            break;
        case 2:
            if (work->animFrame >= 0x16) {
                arg0->state = state;
                work->step  = 0;
            }
            break;
    }
}
