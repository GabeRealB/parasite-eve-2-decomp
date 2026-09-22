#include "common.h"
#include "actors/actors_shared_801339c0.h"
#include "actors/actors_shared_80133cd0.h"
#include "actors/actors_shared_80134af4.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "gameplay/3A34.h"
#include "main/mem.h"

extern u8 D_801153F4;
void      func_800B4114(GpAnimCtx*, s32, s16, s32, s32);

#define SCRATCH_SP (*(u32*)G_SCRATCH_HEAD)

static __inline__ void update_color(GpEnemy* enemy, Task* task)
{
    GsCOORDINATE2* coord;
    VECTOR*        block;
    coord                 = &((TmdObject*)task->extra)->coords[1];
    block                 = (VECTOR*)(*(u8**)0x1F8003FC - 0x10);
    block->vx             = coord->workm.t[0];
    block->vy             = coord->workm.t[1];
    block->vz             = coord->workm.t[2];
    *(VECTOR**)0x1F8003FC = block;
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)0x1F8003FC += 0x10;
}

static __inline__ void update_anim(Task* task)
{
    ActorsShared801339c0Work* work;
    s32                       i;
    work = (ActorsShared801339c0Work*)task->work;
    if (work->field_2D2 == 0) {
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            for (i = 1; i < 3; i++) {
                func_800B4114(&work->context, i, work->field_2B8, 0, 0);
            }
        } else {
            work->field_2BC++;
            for (i = 1; i < 3; i++) {
                Gp_AnimTickIndex(&work->context, i);
            }
        }
    }
}

void ActorsShared801339c0(GpEnemy* arg0, Task* arg1)
{
    ActorsShared801339c0Work* work;
    GsCOORDINATE2*            coord;
    s32                       soundId;

    work = (ActorsShared801339c0Work*)arg1->work;
    switch (D_801153F4) {
        case 1:
            update_color(arg0, arg1);
            break;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            arg0->node.flags                 = 1;
            break;
        case 0:
        default:
            if (work->field_2E2 == 0) {
                return;
            }
            ActorsShared80134af4(arg1);
            ActorsShared80133cd0(arg1);
            update_anim(arg1);
            update_color(arg0, arg1);
            ((TmdObject*)arg1->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
            work->field_2BE -= 2;
            if (work->field_2BE < 0) {
                work->field_2BE = 0;
            }
            coord = ((TmdObject*)arg1->extra)->coords;
            if (coord->coord.t[1] >= 0) {
                soundId = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x402C0008;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                work->field_2B2                               = 1;
                work->field_2C8                               = 1;
                work->field_2BE                               = 0;
                work->field_2DE                               = 0;
                work->field_2B8                               = 2;
                work->field_2BA                               = 0;
                ((TmdObject*)arg1->extra)->coords->coord.t[1] = 0;
                arg1->state                                   = 1;
            } else if (work->field_2E0 == 0) {
                work->field_2DE += 10;
            } else {
                work->field_2DE += 20;
            }
            break;
    }
}

/// The specimen's slide step, the twin of the enemy walk in
/// `ActorsShared8013777c`: node 2's collision table is run through
/// `func_800E0C10` with a 0x48-byte scratch, and the response's mode decides
/// what the step does. The two are one body per carrier - the walk reaches a
/// larger work block at 0x24C and wipes a second table, this one reads the
/// table at 0x154 and the root motion `ActorsShared8013454c` saved.
///
/// Mode 1 is the sliding one. Only the first slide arms it - `field_2E0`
/// latches the mode, `field_2DE` is re-armed to -0x64 and the step length
/// `field_2BE` sheds a quarter of itself - and the response's Y translation is
/// added onto the model coordinate inside that guard, while X and Z are added
/// either way. Mode 2 is a snap back that writes the saved root translation
/// instead. Either way the collision table is wiped before the scratch is
/// handed back.
///
/// Carried by four slots - `actor_104600`, `actor_107000`, `actor_204600` and
/// `actor_207000` - which all reach the block through `Task::work`, so the
/// body takes the `Task` rather than any carrier's own context type; the
/// shared span is in `configs/USA/overlays.toml`.
void ActorsShared80133cd0(Task* arg0)
{
    ActorsShared80133cd0Scratch* scratch;
    ActorsShared80133cd0Work*    work;
    GsCOORDINATE2*               coord;
    s32                          movement;

    work     = (ActorsShared80133cd0Work*)arg0->work;
    scratch  = (ActorsShared80133cd0Scratch*)(SCRATCH_SP -= 0x48);
    coord    = ((TmdObject*)arg0->extra)->coords;
    movement = func_800E0C10(&work->field_154[0], &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->field_2E0 == 0) {
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                work->field_2DE    = -0x64;
                work->field_2BE    = (u16)work->field_2BE - work->field_2BE / 4;
                work->field_2E0    = movement;
            }
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_274;
            coord->coord.t[1] = work->field_278;
            coord->coord.t[2] = work->field_27C;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_154[0]);
    SCRATCH_SP += 0x48;
}
