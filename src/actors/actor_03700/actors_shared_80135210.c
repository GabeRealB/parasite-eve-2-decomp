#include "common.h"

#include "actors/actors_shared_80135210.h"
#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately not declared by `gameplay/1BC.h`; its
/// definition takes `arg2` as `u16`, which would add a zero-extension no
/// caller has. See the note in that header.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Rebinds the work's animation id to its five helper slots. When the id has
/// changed since the last frame the remembered id follows it, the frame
/// counter restarts and every slot is pointed at the new id at weight 4;
/// otherwise the counter ticks and the slots are simply advanced by one.
///
/// Carried by two actor slots - `actor_103700` and `actor_203700` - which is
/// why it takes this header's minimal work view rather than either overlay's
/// own type; the shared span is in `configs/USA/overlays.toml`.
void ActorsShared80135210(ActorShared80135210* arg0)
{
    ActorShared80135210Work* work;
    s32                      i;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_248 != work->field_24A) {
        work->field_24A = (u16)work->field_248;
        work->field_24C = 0;
        do {
            func_800B4114(work, i, work->field_248, 0, 4);
            i++;
        } while (i < 6);
        return;
    }
    TOUCH_REG(i);
    work->field_24C += i;
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 6);
}
