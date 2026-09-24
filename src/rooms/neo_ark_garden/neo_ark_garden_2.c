#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/neo_ark_garden.h"
#include "rooms/room_common.h"

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;
extern u32 Gp_LcgState;

extern SVECTOR D_neo_ark_garden_801813D8;
extern SVECTOR D_neo_ark_garden_801813E0[];

/// Garden ambience task tick. On its first tick it installs three effect ids
/// and moves `state` to 1. `spawnArg1` holds the view seen on the previous
/// tick; whenever `Gp_GetViewIndex()` differs from it, `soundDelay` restarts at
/// 4, and once it has run down the current view's pair of 0x550F0003 /
/// 0x550F0004 loops is enqueued every tick. In views 2, 4 and 5 the first such
/// tick with `state` still 1 also plays them once through
/// `SndEvt_EnqueueType6` and moves `state` to 2. Views 2 and 4 additionally
/// roll two 1-in-4 chances per tick, while no event is running, to spawn
/// effect 0x60070 at the first two points of `D_neo_ark_garden_801813E0`;
/// view 4 also updates the last two points, and view 3 draws the marker at
/// `D_neo_ark_garden_801813D8`.
void func_neo_ark_garden_8017EA9C(Task* task)
{
    NeoArkGardenAmbience* work;
    u32                   rnd;

    work = task->spawnArg2;
    if (task->state == 0) {
        task->state = 1;
        D_80115734  = 0x60228;
        D_80115730  = 0x60233;
        D_80115754  = 0x6023E;
    }
    if (task->spawnArg1 != (Gp_GetViewIndex() & 0xFF)) {
        work->soundDelay = 4;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            if (work->soundDelay == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    SndEvt_EnqueueType6(0x550F0003, -8, 0x32);
                    SndEvt_EnqueueType6(0x550F0004, 0, 0x32);
                }
                SndEvt_EnqueueTypeA(0x550F0003, -8, 0x32);
                SndEvt_EnqueueTypeA(0x550F0004, 0, 0x32);
            } else {
                work->soundDelay--;
            }
            if (Gp_State1C->eventState == 0) {
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[0]);
                }
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[1]);
                }
            }
            break;
        case 3:
            if (work->soundDelay == 0) {
                SndEvt_EnqueueTypeA(0x550F0003, -0xF, 0x4C);
                SndEvt_EnqueueTypeA(0x550F0004, -0xE, 0x4C);
            } else {
                work->soundDelay--;
            }
            func_neo_ark_garden_8017EFB8(&D_neo_ark_garden_801813D8, 0x600, 0xC0);
            break;
        case 4:
            if (work->soundDelay == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    SndEvt_EnqueueType6(0x550F0003, -0xC, 0);
                    SndEvt_EnqueueType6(0x550F0004, 0xC, 0);
                }
                SndEvt_EnqueueTypeA(0x550F0003, -0xC, 0);
                SndEvt_EnqueueTypeA(0x550F0004, 0xC, 0);
            } else {
                work->soundDelay--;
            }
            func_neo_ark_garden_8017F42C(&D_neo_ark_garden_801813E0[2]);
            func_neo_ark_garden_8017F42C(&D_neo_ark_garden_801813E0[3]);
            if (Gp_State1C->eventState == 0) {
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[0]);
                }
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[1]);
                }
            }
            break;
        case 5:
            if (work->soundDelay == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    SndEvt_EnqueueType6(0x550F0003, -0xE, 0x40);
                    SndEvt_EnqueueType6(0x550F0004, -0xD, 0x40);
                }
                SndEvt_EnqueueTypeA(0x550F0003, -0xE, 0x40);
                SndEvt_EnqueueTypeA(0x550F0004, -0xD, 0x40);
            } else {
                work->soundDelay--;
            }
            break;
        case 6:
            if (work->soundDelay == 0) {
                SndEvt_EnqueueTypeA(0x550F0003, 0xD, 0x4C);
                SndEvt_EnqueueTypeA(0x550F0004, 0xF, 0x4C);
            } else {
                work->soundDelay--;
            }
            break;
        case 7:
            if (work->soundDelay == 0) {
                SndEvt_EnqueueTypeA(0x550F0003, -0xC, 0);
                SndEvt_EnqueueTypeA(0x550F0004, -0xC, 0);
            } else {
                work->soundDelay--;
            }
            break;
    }
    task->spawnArg1 = Gp_GetViewIndex() & 0xFF;
}
