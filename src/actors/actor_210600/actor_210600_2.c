#include "common.h"

#include "actors/actor_210600.h"

/// Message 0x7DB handler, listed in `D_actor_210600_8015A4CC`. When the payload
/// comes from sender 0x401 and its selector is 1, it arms four of the work
/// block's state fields: the 0x886 argument, the 0x882 hold, the 0x890 flag and
/// the 0x87C counter.
s32 func_actor_210600_8014B770(Task* task, s32 msgId, Actor210600Msg* msg)
{
    Actor210600Work* work;
    u16              selector;

    work = (Actor210600Work*)task->idMap;
    if (msg->field_0 == 0x401) {
        selector = msg->field_2;
        if (selector == 1) {
            work->field_886.half = 0x10;
            work->field_882      = selector;
            work->field_890      = 0;
            work->field_87C      = 2;
        }
    }
    return 1;
}
