#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Cutscene / among-us mode byte, `Gp_StateC08.field_A`; the gameplay map
/// names only the struct's base, so splat keeps the raw address.
extern s8 D_80114C12;

/// Cutscene script blob handed to `func_800E8614`; unnamed in the gameplay
/// map and shared by several rooms.
extern u8 D_80165720;

/// Cutscene script blobs handed to `func_800E8634`; unnamed in the gameplay
/// map, which keeps the raw address. The first becomes the spawned task's
/// `spawnArg2` (`Gp_ScriptInit` reads it back as the script), the second is
/// parked in `D_801156D0` as the event-command stream.
extern s32 D_80165060;
extern s32 D_80165798;

/// This room's own `GpMsgEntry[]` message table, the one the balcony task parks
/// in `Task::field_24` (0x24) for `Gp_DispatchMsg` to walk. Ids 0x13EE-0x13F2;
/// the `0x7FFFFFFF` terminator is the last record.
extern GpMsgEntry D_dryfield_night_motel_balcony_80182804[];

void func_dryfield_night_motel_balcony_8017E3C8(void);

s32 func_dryfield_night_motel_balcony_8017DC18(void)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC20(void)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC28(void)
{
    return 0;
}

/// Balcony event entry: park the room's message table in `Task::field_24`,
/// publish the task in pointer slot 7, re-run the room's nine-entry per-object
/// flag pass, and on the one qualifying pass start the motel cutscene and
/// advance the story byte. Always ticks the task's state counter (0x30).
void func_dryfield_night_motel_balcony_8017DC30(Task* task)
{
    u8 field9;

    task->field_24 = D_dryfield_night_motel_balcony_80182804;
    Game_SetPtrSlot(task, 7);
    func_dryfield_night_motel_balcony_8017E3C8();
    field9 = Game_Session->field_9;
    if (field9 == 2 && Game_Session->field_5 == field9 && GameFlag_GetNibble(0x61) == 0) {
        func_800E8634((s32)&D_80165060, 0, (s32)&D_80165798);
        GameFlag_SetNibble(0x61, 1);
        GameFlag_SetNibble(0x10E, 1);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 1);
        Game_Session->field_69 = 0x85;
    }
    task->state = task->state + 1;
}

/// One-shot balcony event: while no cutscene is running and the event flag is
/// still unset, play the motel script and mark the flag done.
void func_dryfield_night_motel_balcony_8017DD0C(Task* task)
{
    if (Game_Session->field_1 == 0 && D_80114C12 != 1 && GameFlag_GetNibble(0x10E) == 1) {
        func_800E8614((s32)&D_80165720, 0);
        GameFlag_SetNibble(0x10E, 2);
    }
}
