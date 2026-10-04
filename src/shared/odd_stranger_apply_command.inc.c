/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Message 0x301 / 0x1002 handler: copies the payload's three leading bytes
/// into `commandBytes`, then keys the actor's state off the message id and sub-id
/// (0x301/1 to 0x17, 0x1002/0 to 0, 0x1002/2 to 0x1C). The 0x1002/2 arm also
/// drops the model root to `(-0x595, 0, -0x5B1)` and turns it to -0x400.
/// Returns 1 when a state was set.
s32 oddStrangerApplyCommand(Task* arg0, s32 arg1, u16* arg2, s32 arg3)
{
    u16              room;
    u16              state;
    u16              state2;
    OddStrangerWork* work;

    work                  = arg0->work;
    work->commandBytes[0] = ((u8*)arg2)[0];
    work->commandBytes[1] = ((u8*)arg2)[1];
    work->commandBytes[2] = ((u8*)arg2)[2];
    room                  = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        if (state == 1) {
            work->state = ODD_STRANGER_STATE_DORMANT_SCRIPTED;
            return 1;
        }
        return 0;
    }
    if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->state = ODD_STRANGER_STATE_HIDDEN;
                return 1;
            case 2:
                work->state                         = ODD_STRANGER_STATE_AMBUSH;
                arg0->extra.tmd->coords->coord.t[0] = -0x595;
                arg0->extra.tmd->coords->coord.t[1] = 0;
                arg0->extra.tmd->coords->coord.t[2] = -0x5B1;
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}
