#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern Task* D_actor_146300_8014282C;

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300_4", func_actor_146300_8013291C);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300_4", func_actor_146300_8013299C);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300_4", func_actor_146300_80132A2C);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300_4", func_actor_146300_80132A98);

s32 func_actor_146300_80132B14(void)
{
    return 0;
}

/// State handler of the actor's model task: the spawn tick hangs the task's own
/// coordinate frame under part 4 of the published task's part array and steps
/// to state 1, and every later tick hands that part's world translation,
/// dropped by 0x320 in y, to `func_800D7A9C` for the part colour matrix.
void func_actor_146300_80132B1C(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->field_8;
    GsCOORDINATE2* parts = ((TmdObject*)D_actor_146300_8014282C->extra)->field_8;
    GsCOORDINATE2* part  = parts + 4;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg     = 0;
            extra->field_C = 0;
            coord->sub     = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}
