#include "common.h"

#include "actors/actors_shared_8016a074.h"
#include "main/wipsys.h"

/// Advances the actor out of state 0xD, branching on the player's current HP:
/// 0xE while there is any left, 0x16 once it has run out.
s32 ActorsShared8016a074(Task* task)
{
    ActorsShared8016a074Work* work = (ActorsShared8016a074Work*)task->idMap;
    WipSysConfig*             cfg  = &Wip_SysConfig;

    if (work->field_0 == 0xD) {
        if (cfg->field_18 > 0) {
            work->field_0 = 0xE;
        } else {
            work->field_0 = 0x16;
        }
    }
    return 1;
}
