#include "common.h"

#include "actors/actors_shared_80137cf4.h"
#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Rebinds the work's animation id to its six helper slots. When the id has
/// changed since the last frame the remembered id follows it, the frame counter
/// restarts and every slot is pointed at the new id at weight 8; otherwise the
/// counter ticks and the slots are simply advanced by one.
///
/// Carried by two enemy slots - `actor_107000` and `actor_207000` - which both
/// reach the block through `Task::work`, so the body takes the `Task` rather
/// than either carrier's own context type; the shared span is in
/// `configs/USA/overlays.toml`.
void ActorsShared80137cf4(Task* arg0)
{
    ActorsShared80137cf4Work* work;
    s32                       i;

    work = arg0->work;
    i    = 1;
    if (work->field_370 != (s16)work->field_372) {
        work->field_372 = work->field_370;
        work->field_374 = 0;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_370, 0, 8);
            i++;
        } while (i < 7);
        return;
    }
    TOUCH_REG(i);
    work->field_374 = (u16)(work->field_374 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 7);
}
