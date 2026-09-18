#include "common.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "rooms/dryfield_night_water_hole.h"
#include "rooms/room_common.h"

extern s32 D_dryfield_night_water_hole_8018067C;
extern s32 D_dryfield_night_water_hole_801807FC;
/// Message 0x13F0 handler. Slot 7 dispatches it with the sender's command in
/// `arg2` - `Gp_PostDirIfCapIdle` passes `Gp_DirByte`, the cap driver loop a
/// task's `spawnArg1` - and only 2 concerns this room.
///
/// With progress nibble 0xB8 set the room's event task is spawned: this stages
/// a `DnwhEventDesc` for it, hands the code in `field_1` to the room's resolver
/// for one last say over `field_3`, publishes the descriptor to
/// `D_dryfield_night_water_hole_80183630` and spawns the task from
/// `D_dryfield_night_water_hole_801805EC`. The code staged is 0x2E, past the end
/// of the resolver's jump table, so the byte comes back as it went in.
///
/// Without it the event never ran: cap command 2 is armed, nibble 0x1BD records
/// it, and the sound is enqueued here instead of by the spawned task.
s32 func_dryfield_night_water_hole_8017DC28(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    DnwhEventDesc work;
    DnwhUtilParam param;

    if (arg2 == 2) {
        if (GameFlag_GetNibble(0xB8) != 0) {
            DnwhEventDesc* wp;
            void           (*resolve)(DnwhUtilParam*, DnwhUtilParam*) = func_dryfield_night_water_hole_8017D6AC;

            work.field_0 = 4;
            work.field_1 = 0x2E;
            work.field_3 = 1;
            work.field_2 = 3;
            work.field_8 = 0x53200007;
            work.field_4 = 0xC00;
            Gp_MsgPlayerWeapon(0);
            wp            = &work;
            param.field_0 = wp->field_1;
            param.field_2 = wp->field_2;
            param.field_3 = wp->field_3;
            param.field_5 = 0;
            resolve(&param, &param);
            wp->field_1                          = param.field_0;
            wp->field_2                          = param.field_2;
            wp->field_3                          = param.field_3;
            D_dryfield_night_water_hole_80183630 = work;
            Task_SpawnFromTable(&D_dryfield_night_water_hole_801805EC, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(2);
            GameFlag_SetNibble(0x1BD, 2);
            SndEvt_EnqueueType6(0x53200004, 0, 0);
        }
    }
    return 0;
}

s32 func_dryfield_night_water_hole_8017DD5C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp_s0;

    if ((in->field_2 == 2) && (GameFlag_GetNibble(0x95) == 0) && (gGameSession->at4.loc.place == 1)) {
        GameFlag_SetNibble(0x95, 1);
        func_800E8614((s32)&D_dryfield_night_water_hole_8018067C, 0);
    }
    temp_s0 = in->field_2;
    if ((temp_s0 == 1) && (GameFlag_GetNibble(0x95) == 0) && (gGameSession->at4.loc.place == temp_s0)) {
        GameFlag_SetNibble(0x95, 1);
        func_800E8614((s32)&D_dryfield_night_water_hole_801807FC, 0);
    }
    return 0;
}
