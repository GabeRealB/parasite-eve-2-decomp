#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>

extern s32 Gp_LcgState;

/// The 14 anchor points of the patio's fountain spray, in the room object's own
/// space. The first three double as the jitter centres for the mist burst.
extern SVECTOR D_acropolis_patio_80182DDC[14];

/// Slow left turn-in-place: nudges the player's facing angle by -0x80 each
/// frame for 0x10 frames, wrapping it back into [-0x800, 0x800), then kills
/// itself. Any state other than 0 or 1 kills the task immediately.
void func_acropolis_patio_8017E054(Task* task)
{
    GameActor* actor;
    s16        angle;

    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;

    switch (task->state) {
        case 0:
            task->killCountdown = 0x10;
            task->state++;
            /* fallthrough */
        case 1:
            angle = actor->field_52 - 0x80;
            if (angle < -0x800) {
                angle = actor->field_52 + 0xF80;
            }
            actor->field_52 = angle;
            task->killCountdown--;
            if (task->killCountdown > 0) {
                break;
            }
            /* fallthrough */
        default:
            Task_Kill(task);
            break;
    }
}

/// Lights the patio fountain: a one-shot burst that seeds every jet and its
/// mist, then leaves the task idle for the rest of the room.
///
/// The 14 anchors of `D_acropolis_patio_80182DDC` are handed to `Gp_SpawnEff`
/// as three runs of effect 0x60087, each run differing only in the high bits of
/// the spawn argument - `0x03000200` for the three main jets, `0x02000000` for
/// the next four and a plain `0x100` for the remaining seven - so the anchor
/// index rides in the low byte and the flags pick the jet's size and blend.
///
/// The three main jets then get three puffs of mist each (effect 0x6008F).
/// Every puff re-uses the task's own `GpEffWork.field_10` triple as a scratch
/// offset: three 11-bit LCG draws centred on 0x400 give a `+/-0x400` jitter,
/// which is added to the jet's anchor before the spawn reads it. The work block
/// is scratch, not state - each spawn copies the vector out immediately - so
/// all nine puffs share it.
void func_acropolis_patio_8017E100(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    s32            i;
    s32            j;

    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->field_8;

    if (task->state == 0) {
        for (i = 0; i < 3; i++) {
            Gp_SpawnEff(0x60087, objCoord, i + 0x03000200, &D_acropolis_patio_80182DDC[i]);
        }
        for (i = 3; i < 7; i++) {
            Gp_SpawnEff(0x60087, objCoord, i + 0x02000000, &D_acropolis_patio_80182DDC[i]);
        }
        for (i = 7; i < 0xE; i++) {
            Gp_SpawnEff(0x60087, objCoord, i + 0x100, &D_acropolis_patio_80182DDC[i]);
        }
        task->state++;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_10  = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_12  = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_14  = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                work->field_10 += D_acropolis_patio_80182DDC[i].vx;
                work->field_12 += D_acropolis_patio_80182DDC[i].vy;
                work->field_14 += D_acropolis_patio_80182DDC[i].vz;
                Gp_SpawnEff(0x6008F, objCoord, i, (SVECTOR*)&work->field_10);
            }
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_4", func_acropolis_patio_8017E324);

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_4", func_acropolis_patio_8017E730);
