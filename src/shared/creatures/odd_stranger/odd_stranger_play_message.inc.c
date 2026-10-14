/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Maps an animation message's selector to a script clip and reenters DOWN.
///
/// Borrows `request` for dispatch and reads only `animationId`: selectors
/// 0..4 map to clips 34, 35, 36, 37 and 39; other selectors keep `animId`.
/// Always sets `ODD_STRANGER_STATE_DOWN` and forces state entry, returning zero.
/// It does not request a clip restart or install a bank. `messageId`, `unused`
/// and the request's other fields are ignored. Requires initialized work.
static s32 _oddStrangerPlayMessage(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unused)
{
    enum { ODD_STRANGER_FORCE_STATE_ENTRY = -1 };

    OddStrangerWork* work = task->work;

    switch (request->animationId) {
        case 0:
            work->animId = ODD_STRANGER_ANIM_SCRIPT_0;
            break;
        case 1:
            work->animId = ODD_STRANGER_ANIM_SCRIPT_1;
            break;
        case 2:
            work->animId = ODD_STRANGER_ANIM_SCRIPT_2;
            break;
        case 3:
            work->animId = ODD_STRANGER_ANIM_SCRIPT_3;
            break;
        case 4:
            work->animId = ODD_STRANGER_ANIM_SCRIPT_4;
            break;
    }
    work->state     = ODD_STRANGER_STATE_DOWN;
    work->prevState = ODD_STRANGER_FORCE_STATE_ENTRY;
    return 0;
}
