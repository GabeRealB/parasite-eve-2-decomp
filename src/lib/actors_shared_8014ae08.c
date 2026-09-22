#include "common.h"
#include "gameplay/1BC.h"
#include "actors/actor_207200.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_104600.h"
#include "actors/actors_shared_8014a1c4.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Clears the pending bits of the enemy work's flag byte at 0x4C. Bit 0x1 is
/// dropped on its own; bit 0x2 restarts the work's state machine at state 3
/// with the frame counter cleared; bits 0xC - the actor colour remap request -
/// are dropped last, after re-reading the byte.
///
/// Carried by three enemy slots - `actor_104600`, `actor_204600` and
/// `actor_207200` - which is why it takes the `Task` rather than either
/// overlay's own context type; the shared span is in `configs/USA/overlays.toml`.
void ActorsShared8014ae08(Task* arg0)
{
    GpEnemy*         enemy;
    Actor207200Work* work;
    u8               flags;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = arg0->work;
    flags = enemy->reactionFlags;
    if (flags != 0) {
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags = enemy->reactionFlags & 0xFD;
            work->field_286      = 3;
            work->field_28A      = 0;
        }
        flags = enemy->reactionFlags;
        if (flags & 0xC) {
            enemy->reactionFlags = flags & 0xF3;
        }
    }
}

void ActorsShared8014ae70(Task* task)
{
    Actor104600Work* work;

    work = (Actor104600Work*)task->work;
    switch (work->field_286) {
        case 0:
            ActorsShared8014a1c4(task);
            break;
        case 2:
            break;
        case 3:
            work->field_292 = 0;
            work->field_2A6 = 0;
            work->field_28A = work->field_28A + 1;
            if (work->field_28A >= 4) {
                work->field_28E = 1;
                work->field_290 = 0;
                work->field_28A = 0;
            }
            if (Gp_TickObjFlag2((GpObj5D*)task->spawnArg2) != 0) {
                work->field_286 = 0;
            }
            break;
    }
}

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

    work = arg0->work;
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
