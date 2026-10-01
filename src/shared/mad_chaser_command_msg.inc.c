/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Message handler: on message 0x2C00 whose low nibble is 1..5, store the
/// message halfword in `field_44C`. The five identical case bodies are
/// cross-jumped into one, but only separate bodies keep the jump table; a
/// single `case 1 ... 5` becomes a range test.
void madChaserCommandMsg(Task* arg0, s32 arg1, ActorCommand* request)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (request->context.key == 0x2C00) {
        switch (request->command & 0xF) {
            case 1:
                work->field_44C = request->command;
                break;
            case 2:
                work->field_44C = request->command;
                break;
            case 3:
                work->field_44C = request->command;
                break;
            case 4:
                work->field_44C = request->command;
                break;
            case 5:
                work->field_44C = request->command;
                break;
        }
    }
}
