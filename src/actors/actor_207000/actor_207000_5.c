#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 D_actor_207000_80149E5C;

// This slot's `func_actor_207000_8014E288` is the shared body
// `ActorsShared80136288`, matched in `src/actors/lib/`.

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014CF84);

void func_actor_207000_8014D280(Task* arg0, TmdObject* arg1, s32 arg2)
{
    Actor107000Spawn2Work* work;
    GsCOORDINATE2*         coord;
    s32                    soundId;
    s16                    state;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    state = work->field_370 - 1;
    switch (state) {
        case 0:
            work->field_36E++;
            if ((s16)work->field_36E > work->field_390) {
                work->field_36E = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_390 = (Gp_LcgState >> 16) % 20 + 80;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                if ((u16)((Gp_LcgState >> 16) % 100) < 31U) {
                    work->field_370 = 2;
                } else {
                    work->field_370 = 9;
                }
            }
            work->field_380 = 1;
            work->field_378 = 0;
            break;
        case 1:
            work->field_380 = 1;
            work->field_378 = 0;
            if ((s16)work->field_374 == 10) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460003;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_374 == 105) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460006;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if ((s16)work->field_374 >= 110) {
                work->field_370 = 1;
            }
            break;
        case 8:
            work->field_378 = 0;
            if ((s16)work->field_374 >= 18) {
                work->field_38E = 1;
            }
            if ((s16)work->field_374 >= work->field_392 + 18) {
                work->field_38E = 0;
                work->field_370 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_392 = (Gp_LcgState >> 16) % 50 + 50;
            }
            break;
        case 4:
            if ((s16)work->field_374 < 22) {
                return;
            }
            work->field_370 = 2;
            break;
        case 5:
            if ((s16)work->field_374 >= 57) {
                work->field_36A = 1;
                work->field_36E = 0;
                work->field_382 = 0;
            }
            break;
    }
    if (Gp_CountRec18Hi(work->field_214, 0x10000) != 0 && (u16)work->field_38E != 0) {
        work->field_36A = 1;
        work->field_36E = 0;
        work->field_382 = 0;
    }
    if (Gp_CountRec18Hi(work->field_24C, 0x10000) != 0 || work->field_388 != 0) {
        work->field_36A = 1;
        work->field_36E = 0;
        work->field_382 = 2;
    }
    Gp_ClearRec18Occupied(work->field_214);
}

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014D60C);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014DC28);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E094);

INCLUDE_RODATA("actors/nonmatchings/actor_207000/actor_207000_5", ActorsShared80138404Table);
