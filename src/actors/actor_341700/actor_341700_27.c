#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016AC64);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016AF70);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016B2B8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016B804);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016B9A8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016C0F4);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016CC9C);

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_27", ActorsShared80135df4Table);

s32 func_actor_341700_8016CE28(Actor341700* arg0, s32 arg1, s32 arg2)
{
    TmdObject* obj = arg0->field_2C;

    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->field_C |= 4;
            break;
        case 3:
            obj->field_C = 4;
            break;
    }
    return 0;
}

/// The `0x2704` command handler, reached through the task's `Task::field_24`
/// table (`D_actor_341700_80175F5C`): the three leading bytes of `cmd` are
/// copied over `Actor341700SubWork::field_18` .. `field_1A` and the second
/// halfword, when the opcode matches, picks the state the work block moves to.
///
/// `case 2` is folded into `default` on purpose. The two bodies are the same,
/// so the case list keeps three nodes and GCC's decision tree balances around
/// `case 1`; dropping `case 2` makes `case 0` the root and the emitted branches
/// come out in a different order.
s32 func_actor_341700_8016CEB4(Task* task, s32 arg1, Actor341700Cmd* cmd)
{
    Actor341700SubWork* work = (Actor341700SubWork*)task->idMap;

    work->field_18 = cmd->bytes.field_0;
    work->field_19 = cmd->bytes.field_1;
    work->field_1A = cmd->bytes.field_2;

    if (cmd->halfs[0] == 0x2704) {
        switch (cmd->halfs[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 1:
                ((TmdObject*)task->extra)->field_8->flg = 0;
                work->field_0                           = 2;
                break;
            case 2:
            default:
                work->field_0 = 0;
                task->state   = 1;
                break;
        }
    }
    return 1;
}
