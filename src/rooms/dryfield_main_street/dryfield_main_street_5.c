#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_main_street.h"

extern u8 D_801156F9;

/// The `inline_c.h` GTE command lacks the two leading nops this code has.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

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

/// A spark the room spawns: each frame it draws tile `field_20` of the 5-wide
/// sprite grid with `func_dryfield_main_street_8017EA88`, at half-extent
/// `field_24` (the low 12 bits of `spawnArg1`) and spin `field_26` (random),
/// and drifts along `field_10`. The first run picks a random direction,
/// normalises it and scales it by `field_2A` (bits 16-23 of `spawnArg1`,
/// default 0x40). Every `field_28` frames (bits 12-14, default 1) the tile
/// advances; after tile 9 the spark releases itself.
void func_dryfield_main_street_8017E830(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            vz;
    s16            f2a;
    u32            rng2;
    u32            rng3;

    work->field_22++;
    if (task->state == 0) {
        work->field_24 = (*(u16*)&task->spawnArg1) & 0xFFF;
        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
        work->field_26 = (Gp_LcgState >> 16) & 0xFFF;

        if (task->spawnArg1 & 0xF000) {
            work->field_28 = (task->spawnArg1 >> 12) & 0x7;
        } else {
            work->field_28 = 1;
        }

        work->field_22 = 0;
        task->state    = 1;

        if (task->spawnArg1 & 0xFF0000) {
            f2a = (task->spawnArg1 >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->field_2A    = f2a;
        work->field_10.vy = 0;
        rng2              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng2;
        work->field_10.vx = -(((u32)rng2 >> 16) & 0x7F);
        rng3              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng3;
        vz                = 0x80 - (((u32)rng3 >> 16) & 0xFF);
        work->field_10.vz = vz;
        VectorNormalSS(&work->field_10, &work->field_10);

        gte_lddp(work->field_2A);
        gte_ldsv(&work->field_10);
        gte_gpf12_real();
        gte_stsv(&work->field_10);
    }

    func_dryfield_main_street_8017EA88(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);

    coord->coord.t[0] += work->field_10.vx;
    coord->coord.t[1] += work->field_10.vy;
    coord->coord.t[2] += work->field_10.vz;
    coord->flg         = 0;

    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}
