#include "common.h"

#include "actors/actor_100400.h"
#include "main/task.h"
#include "main/gfx.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

void Actor00400_Fn001AC(GsCOORDINATE2* coord, u16 phase, u16 kind, u32 arg3);

/// The pair table the marker's collision object carries at `GpObj.field_18`,
/// packed with kind 1: `field_0` 0xA4, `field_2` 0xBE.
extern GpU16Pair Actor00400_D0FDC0;

/// First kill-path state, entered the frame the marker task is spawned:
/// `Actor00400_Fn02D48` walks it afterwards and `Actor00400_Fn0A28C` retires it.
///
/// `task->work` is the 0x64-byte `Actor100400MarkerWork` block
/// `Actor00400_SpawnMarker` allocated, and `task->extra` the `Actor100400Ctx`
/// whose `field_8` is the coordinate the marker is drawn at. That coordinate is
/// re-parented to `gGfxViewCoord` here, and the object is linked to it with its
/// two `GpRec18` slots zeroed, so the state `Actor00400_Fn02D48` runs can report
/// what the marker collides with. `field_5A` is seeded with the negative span
/// the spawner's tip overshot by, and the object's draw scale with 0x100.
///
/// The `task->extra` walk is repeated for `obj.field_8` rather than reusing
/// `coord`: the original re-reads it, which is what the second `lw` chain in
/// the target shows.
///
/// `coord` is assigned before `work` on purpose. sched1 emits each load where
/// its source order puts it, and that position is the quantity's `birth`:
/// writing `coord` second lands its `lw` one insn later, shortening its span
/// from 70 to 68 and raising its `QTY_CMP_PRI` from 1428 to 1470 — above the
/// task pointer's 1458 — so local-alloc hands the coordinate `$s1` and the task
/// pointer `$s2` instead of the reverse. See DECOMPILATION_LEARNINGS.md,
/// "A parameter competes in local-alloc on its raw span, not its doubled
/// `REG_LIVE_LENGTH`".
void Actor00400_Fn0A190(Task* task)
{
    Actor100400MarkerWork* work;
    GsCOORDINATE2*         coord;

    coord               = ((Actor100400Ctx*)task->extra)->field_8;
    work                = (Actor100400MarkerWork*)task->work;
    task->killCountdown = 0;
    work->field_60      = 0;
    coord->sub          = &gGfxViewCoord;
    coord->flg          = 0;
    work->obj.key       = Gp_PackPair(&Actor00400_D0FDC0, 1);
    work->obj.coord     = ((Actor100400Ctx*)task->extra)->field_8;
    work->obj.ctx.recs  = work->recs;
    work->obj.pos.vx    = 0;
    work->obj.pos.vy    = 0;
    work->obj.pos.vz    = 0;
    work->obj.radius    = 0x100;
    work->obj.flags     = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(work->recs, 2, 0);
    work->obj.flags |= 0xC000;
    Gp_UpdateCoord(coord);
    work->field_5A = -0x14;
    Actor00400_Fn001AC(coord, (u16)work->field_60, 0, 0x1300);
    task->state++;
}
