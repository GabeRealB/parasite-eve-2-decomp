/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Message handler: on message 0x2C00 whose low nibble is 1..5, store the
/// message halfword in `command`. The five identical case bodies are
/// cross-jumped into one, but only separate bodies keep the jump table; a
/// single `case 1 ... 5` becomes a range test.
void madChaserCommandMsg(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (request->context.key == 0x2C00) {
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
