#include "common.h"

#include "gameplay/1BC.h"

#include "actors/actor_207200.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Rebinds the work's animation id to its three helper slots. When the id has
/// changed since the last frame the remembered id follows it, the frame
/// counter restarts and every slot is pointed at the new id at weight 8;
/// otherwise the counter ticks and the slots are simply advanced by one.
///
/// Carried by three enemy slots - `actor_104600`, `actor_204600` and
/// `actor_207200` - which is why it takes the `Task` rather than either
/// overlay's own context type; the shared span is in `configs/USA/overlays.toml`.
void ActorsShared8014af2c(Task* arg0)
{
    Actor207200Work* work;
    s32              i;

    work = arg0->idMap;
    i    = 1;
    if (work->field_28C != (s16)work->field_28E) {
        work->field_28E = work->field_28C;
        work->field_290 = 0;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_28C, 0, 8);
            i++;
        } while (i < 3);
        return;
    }
    TOUCH_REG(i);
    work->field_290 = (u16)(work->field_290 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 3);
}
