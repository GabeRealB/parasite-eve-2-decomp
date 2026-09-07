#include "common.h"

#include "actors/actors_shared_8013288c.h"

/// Message handler on the published work block: kind 0 arms the `field_4B4`
/// countdown at 0x14, kind 1 latches the byte flag at `field_4BC`.
s32 ActorsShared8013288c(Task* task, s32 arg1, ActorsShared8013288cMsg* msg)
{
    s32 kind;

    kind = msg->field_2;
    switch (kind) {
        case 0:
            ActorsShared80131f9cWork->field_4B4 = 0x14;
            break;
        case 1:
            ActorsShared80131f9cWork->field_4BC = kind;
            break;
    }
    return 0;
}
