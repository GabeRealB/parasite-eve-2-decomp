#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Rebinds the work's animation id to its three helper slots, unless
/// `Actor107000Work::field_2D2` says the rebind is off. When the id has changed
/// since the last frame the remembered id follows it, the frame counter
/// restarts and every slot is pointed at the new id at weight 0; otherwise the
/// counter ticks and the slots are simply advanced by one.
void func_actor_107000_80134680(Task* arg0)
{
    Actor107000Work* work;
    s32              i;

    work = arg0->idMap;
    if (work->field_2D2 == 0) {
        i = 1;
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            do {
                func_800B4114((GpAnimCtx*)work, i, work->field_2B8, 0, 0);
                i++;
            } while (i < 3);
            return;
        }
        TOUCH_REG(i);
        work->field_2BC = (u16)(work->field_2BC + i);
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 3);
    }
}
