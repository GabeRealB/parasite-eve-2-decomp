#include "common.h"

#include "main/task.h"
#include "actors/actor_310600.h"

/// Re-aims the actor's animation state: publishes the three leading words of
/// `arg2` onto the work block and pushes the state change through the shared
/// command handler. `arg1` is unused.
void func_actor_310600_80162C94(Task* arg0, s32 arg1, Actor310600Cmd* arg2)
{
    Actor310600Work* work;
    Actor310600Cmd   cmd;

    work = (Actor310600Work*)arg0->work;

    work->field_47C = 1;
    work->field_4F8 = arg2->animId;
    work->field_4FC = arg2->state;
    work->field_500 = arg2->path;

    cmd.animId = 0;
    cmd.state  = 0xC;
    cmd.path   = 0;
    cmd.param  = 0;
    cmd.unk10  = 0;

    func_actor_310600_8016246C(arg0, 0x7D3, &cmd, 0);
}
