#include "common.h"

#include "gameplay/268.h"
#include "main/task.h"
#include "main/tmd.h"

/// Spawn argument of a model task whose visibility follows a 2-bit game flag;
/// `flagId` selects the flag.
typedef struct {
    u8 unk0[8];
    u8 flagId;
} _SpawnArg;

void RoomsShared8017d5e4(Task* task)
{
    TmdObject* obj = task->extra;

    if (Gp_GetCurBit2Flag(((_SpawnArg*)task->spawnArg2)->flagId) == 2) {
        obj->flags |= 0x80;
    } else {
        obj->flags &= ~0x80;
    }
}
