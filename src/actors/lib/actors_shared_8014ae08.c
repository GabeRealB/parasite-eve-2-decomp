#include "common.h"

#include "gameplay/1BC.h"

#include "actors/actor_207200.h"

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
    flags = enemy->field_4C;
    if (flags != 0) {
        if (flags & 1) {
            enemy->field_4C = flags & 0xFE;
        }
        if (enemy->field_4C & 2) {
            enemy->field_4C = enemy->field_4C & 0xFD;
            work->field_286 = 3;
            work->field_28A = 0;
        }
        flags = enemy->field_4C;
        if (flags & 0xC) {
            enemy->field_4C = flags & 0xF3;
        }
    }
}
