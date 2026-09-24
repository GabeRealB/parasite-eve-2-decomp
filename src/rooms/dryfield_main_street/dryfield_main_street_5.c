#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_main_street.h"

extern u8 D_801156F9;

/// Advances the shared LCG and yields the high half of the new state.
#define DRYFIELD_MAIN_STREET_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

extern s32     D_80070F70;
extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern u16     D_dryfield_main_street_80181B94[];
extern SVECTOR D_dryfield_main_street_80181BA4;
extern u32     Gp_LcgState;

void func_dryfield_main_street_8017E354(s32 arg0)
{
    Task* t = D_dryfield_main_street_80185630;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    taskKill(D_dryfield_main_street_80185630);
    D_dryfield_main_street_80185630 = NULL;
}

void func_dryfield_main_street_8017E3A8(Task* task)
{
    GpWorkObj* work;
    u16        tick;

    if (D_801156F9 == 0) {
        if (task->state == 0) {
            if (task->spawnArg1 != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            work = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
            func_800B0928(gameGetPtrSlot(3), (Task*)work->field_0, 0x300, 0x200, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

void func_dryfield_main_street_8017E4A4(void)
{
    D_dryfield_main_street_80185630 = 0;
}

/// Per-frame room task. On its first run it stores the ids 0x60293-0x60295 in
/// three gameplay globals. Each run it publishes the current view's
/// `roomEffectMode`. In view 8 it spawns 0x30 randomly placed 0x601B1 effects
/// on entering the view, and one more on each run with bit 0 of `D_80070F70`
/// set while it stays. `spawnArg1` holds the view seen on the previous run.
void func_dryfield_main_street_8017E4B0(Task* task)
{
    s32 i;

    if (task->state == 0) {
        D_80115758  = 0x60293;
        D_8011572C  = 0x60294;
        D_80115750  = 0x60295;
        task->state = 1;
    }
    Gp_State1C->roomEffectMode = D_dryfield_main_street_80181B94[(Gp_GetViewIndex() & 0xFF) - 1];
    if ((Gp_GetViewIndex() & 0xFF) == 8) {
        if (task->spawnArg1 != (Gp_GetViewIndex() & 0xFF)) {
            for (i = 0; i < 0x30; i++) {
                D_dryfield_main_street_80181BA4.vx = DRYFIELD_MAIN_STREET_RAND() % 300 - 0x4A1;
                D_dryfield_main_street_80181BA4.vy = DRYFIELD_MAIN_STREET_RAND() % 600 - 0x4E7;
                D_dryfield_main_street_80181BA4.vz = 0x2927 - DRYFIELD_MAIN_STREET_RAND() % 700;
                Gp_SpawnEff(0x601B1, NULL, (DRYFIELD_MAIN_STREET_RAND() & 0x10FF) + 0x103100,
                            &D_dryfield_main_street_80181BA4);
            }
        } else if (D_80070F70 & 1) {
            D_dryfield_main_street_80181BA4.vx = DRYFIELD_MAIN_STREET_RAND() % 300 - 0x4A1;
            D_dryfield_main_street_80181BA4.vy = DRYFIELD_MAIN_STREET_RAND() % 600 - 0x4E7;
            D_dryfield_main_street_80181BA4.vz = 0x2927 - DRYFIELD_MAIN_STREET_RAND() % 700;
            Gp_SpawnEff(0x601B1, NULL, (DRYFIELD_MAIN_STREET_RAND() & 0x10FF) | 0x82100,
                        &D_dryfield_main_street_80181BA4);
        }
    }
    task->spawnArg1 = Gp_GetViewIndex() & 0xFF;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_5", func_dryfield_main_street_8017E830);
