/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Latches a supported room command for the Mad Chaser's state machine.
///
/// Accepts only context key 0x2C00 (synthetic stage 0, area 44) and kinds 1..5.
/// Copies the whole command halfword, retaining its entry-move and spot bits;
/// unsupported requests leave the pending command intact. Borrows request only
/// through dispatch. messageId and unusedSecondArg are ignored; no result is returned.
static void _madChaserQueueCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedSecondArg)
{
    enum {
        MAD_CHASER_COMMAND_CONTEXT = 0x2C00,
    };
    MadChaserWork* work = task->work;

    if (request->context.key == MAD_CHASER_COMMAND_CONTEXT) {
        switch (request->command & MAD_CHASER_COMMAND_KIND_MASK) {
            case MAD_CHASER_COMMAND_EMERGE:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_PULL:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_VANISH:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_DROP_DEATH:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_SHRINK_DEATH:
                work->command = request->command;
                break;
        }
    }
}
