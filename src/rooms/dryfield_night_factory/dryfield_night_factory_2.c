#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_factory.h"

extern void Room_Util16(s32);
extern void Room_Util17(s32);
extern u8   D_8007216D;

/// Cutscene driver for the night factory room: silences both weapons, runs the
/// cap (cutscene) command in `Task::spawnArg1`, then waits for the cap to
/// report event key 3 before setting the two progress flags and starting the
/// follow-up cap slot. Any state past 4 restores the weapons and kills the task.

/// Cutscene driver for the night factory room: silences both weapons, runs the
/// cap (cutscene) command in `Task::spawnArg1`, then waits for the cap to
/// report event key 3 before setting the two progress flags and starting the
/// follow-up cap slot. Any state past 4 restores the weapons and kills the task.

void func_dryfield_night_factory_8017F330(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd(task->spawnArg1, 0);
            goto advance;
        case 1:
            if (GameFlag_GetNibble(0x48) <= 0) {
                if (Game_Session->field_7 == 2) {
                    Room_Util17(0);
                    SOFT_BARRIER();
                } else {
                    Room_Util17(0);
                }
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case 2:
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        case 3:
            if (Gp_GetCapEventKey() == 3) {
                GameFlag_SetNibble(0x48, 1);
                GameFlag_SetNibble(0x4A, 1);
                if (Game_Session->field_7 == 2) {
                    Room_Util17(1);
                    Room_Util16(1);
                    SOFT_BARRIER();
                } else {
                    Room_Util17(1);
                    Room_Util16(1);
                }
                Gp_StartCapSlot(task->spawnArg1, 1, 2);
            }
        advance:
            task->state++;
            return;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, 1, 0);
            Task_Kill(task);
            break;
    }
}

/// The room's second cutscene: states 0..2 silence both weapons, run the cap in
/// `Task::spawnArg1` and wait for it to report event key 1; states 3 and 6 count
/// `Task::killCountdown` up to and back down from 0x1E and tint the screen with
/// the count scaled to 0xFF over 30 steps; states 4 and 5 publish the progress
/// flags and tint it white, and anything past 6 restores the weapons and kills
/// the task.
///
/// `fade` does two jobs on purpose: state 4 reads the session variant through
/// it before testing it. That cross-block use is what makes the state-3/6 tint
/// value a *global* pseudo, and `local-alloc` only folds the `(u8)fade`
/// conversion into the division's quantity when that pseudo is local to one
/// block -- global, the conversion keeps its own quantity and takes `$a0` from
/// the argument move, while the division chain keeps `$v1`.
void func_dryfield_night_factory_8017F4F4(Task* task)
{
    u8 fade;

    switch (task->state) {
        case 0:
            if (GameFlag_GetNibble(0x47) != 0) {
                goto kill;
            }
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd1(task->spawnArg1);
            goto advance;
        case 2:
            if (Gp_GetCapEventKey() == 1) {
                task->killCountdown = 0;
                if (Game_Session->field_7 == 2) {
                    Gp_EnqueueStageSnd6(0x5217000C, 0, 0);
                }
                goto advance;
            }
            task->state = -1;
            return;
        case 3:
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown < 0x1E) {
                goto draw;
            }
            goto bump;
        case 4:
            Game_Session->field_52 = 1;
            GameFlag_SetNibble(0x47, 1);
            fade = Game_Session->field_7;
            if (fade == 2) {
                Gp_EnqueueStageSnd6(0x5217000B, 0, 0);
            }
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            goto advance;
        case 5:
            D_8007216D             = 2;
            Game_Session->field_5  = 2;
            Game_Session->field_76 = 1;
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            goto advance;
        case 1:
        advance:
            task->state = task->state + 1;
            return;
        case 6:
            task->killCountdown = task->killCountdown - 1;
            if (task->killCountdown > 0) {
                goto draw;
            }
        bump:
            task->state = task->state + 1;
        draw:
            fade = (task->killCountdown * 255) / 30;
            Fade_DrawOverlay(fade, fade, fade, 2);
            return;
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
        kill:
            Task_Kill(task);
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_2", func_dryfield_night_factory_8017F734);

void func_dryfield_night_factory_8017FA08(Task* task)
{
    GsCOORDINATE2*    coord;
    NightFactoryWork* work;
    TmdObject*        obj;
    s32               flag;
    s32               prev;

    /* The model pointer is read twice on purpose: the second read is what
       leaves the target's `move s4, v0` copy. */
    coord = ((TmdObject*)task->extra)->field_8;
    work  = (NightFactoryWork*)task->work;
    obj   = (TmdObject*)task->extra;
    flag  = GameFlag_GetNibble(0x49);
    prev  = work->field_0;
    if (flag != prev) {
        if ((flag ^ prev) & 1) {
            work->field_16 = 0;
        }
        if ((flag ^ work->field_0) & 2) {
            work->field_17 = 0;
        }
        work->field_0  = flag;
        work->field_14 = 0;
    }
    if (flag & 2) {
        func_dryfield_night_factory_8017E13C(task);
        if (flag & 1) {
            func_dryfield_night_factory_8017DA54(task);
        } else {
            func_dryfield_night_factory_8017DDD4(task);
        }
    } else {
        func_dryfield_night_factory_8017E480(task);
        if (flag & 1) {
            func_dryfield_night_factory_8017E7A4(task);
        } else {
            func_dryfield_night_factory_8017EBD4(task);
        }
    }
    work->field_14++;
    func_dryfield_night_factory_8017D858(task, 0, flag & 1);
    Gp_UpdateCoord(coord);
    func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
}
