#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"

#include "actors/actors_shared_80135d50.h"

/// Ramps the actor's light blend up or down depending on the flag at
/// `field_2A6`. Rising, the first frame switches the display object to light
/// mode 2 and the counter saturates at 0x12, where it raises the context flag
/// and sets the model's 0x80 bit. Falling, leaving 0x12 clears the context
/// flag and returns the object to light mode 0, and the counter bottoms out at
/// 0 with the model bits cleared.
void ActorsShared80135d50(Task* task)
{
    ActorsShared80135d50Work* work;
    ActorsShared80135d50Ctx*  ctx;
    TmdObject*                obj;

    work = (ActorsShared80135d50Work*)task->idMap;
    ctx  = (ActorsShared80135d50Ctx*)task->spawnArg2;
    obj  = (TmdObject*)task->extra;

    if (work->field_2A6 != 0) {
        if (work->field_2A4 == 0) {
            work->field_2A4++;
            obj->field_C = 2;
            Gp_SetLightMode(task->spawnArg2, 2);
        } else {
            work->field_2A4++;
            if (work->field_2A4 >= 0x12) {
                work->field_2A4 = 0x12;
                ctx->field_14   = 1;
                obj->field_C    = 0x80;
            }
        }
    } else {
        if (work->field_2A4 == 0x12) {
            work->field_2A4--;
            ctx->field_14 = 0;
            obj->field_C  = 2;
            Gp_SetLightMode(task->spawnArg2, 0);
        } else {
            work->field_2A4--;
            if (work->field_2A4 <= 0) {
                work->field_2A4 = 0;
                obj->field_C    = 0;
            }
        }
    }
}
