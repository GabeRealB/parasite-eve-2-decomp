#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_north_maintenance_walkway.h"

extern GpMsgEntry D_shelter_b1_north_maintenance_walkway_80184A84[];
extern TaskDesc   D_shelter_b1_north_maintenance_walkway_80184AAC[];
extern u8         D_80071075;
extern s16        D_80071076;
extern u8         D_801153F4;
extern u8         D_80115690;

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115754;
extern s32 D_80115758;

extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184AB8[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B08[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B18[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B48[];

void func_shelter_b1_north_maintenance_walkway_8017DB54(u8 arg0);

/// Parameters of the event this room's message handler starts, latched into
/// the room's pending copy when it fires. `field_0` is the CAP command the
/// event task runs and `field_4` the stage sound it plays (0 for none).
/// `flagId` is the game-flag nibble that records the event as done: a set
/// nibble stops it firing again, and starting it sets the nibble (0 means no
/// flag). A non-zero `field_A` has the event task spawn task 0x31.
typedef struct {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _ShelterB1NorthMaintenanceWalkwayEvent;

extern s32      func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc D_shelter_b1_north_maintenance_walkway_80184A78;
/// Spawn payload of the task 0x31 the event task may start.
extern GpStateBD8                             D_shelter_b1_north_maintenance_walkway_80185B6C;
extern RoomEventMsg                           D_shelter_b1_north_maintenance_walkway_80185B74;
extern s8                                     D_shelter_b1_north_maintenance_walkway_80185B7C;
extern _ShelterB1NorthMaintenanceWalkwayEvent D_shelter_b1_north_maintenance_walkway_80185B80;

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB1NorthMaintenanceWalkwayStartEvent(
    RoomEventMsg* dst, _ShelterB1NorthMaintenanceWalkwayEvent* event)
{
    D_shelter_b1_north_maintenance_walkway_80185B7C = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_north_maintenance_walkway_80185B74 = *dst;
            D_shelter_b1_north_maintenance_walkway_80185B80 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_north_maintenance_walkway_80184A78, 0, 0, 0);
            D_shelter_b1_north_maintenance_walkway_80185B7C = 1;
        }
        return 2;
    }
    return 1;
}

/// The event task the room's message handler spawns. It runs the latched
/// event's CAP command and waits for it to finish, starting task 0x31 when the
/// event asks for it; then plays the event's stage sound (if any) and waits
/// for the voice to end. Finally it commits the latched message's area, warp
/// and room as the save location, respawns the player task as type 0x11 and
/// ends.
void func_shelter_b1_north_maintenance_walkway_8017D60C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b1_north_maintenance_walkway_80185B80.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b1_north_maintenance_walkway_80185B80.field_A != 0) {
                    D_shelter_b1_north_maintenance_walkway_80185B6C.field_0 = 0;
                    D_shelter_b1_north_maintenance_walkway_80185B6C.field_1 = 0;
                    D_shelter_b1_north_maintenance_walkway_80185B6C.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b1_north_maintenance_walkway_80185B6C);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b1_north_maintenance_walkway_80185B80.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b1_north_maintenance_walkway_80185B80.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b1_north_maintenance_walkway_80185B80.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b1_north_maintenance_walkway_80185B74.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b1_north_maintenance_walkway_80185B74.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b1_north_maintenance_walkway_80185B74.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Messages 0xB and 0xE start the room's event - command 3 /
/// 2 on flag 0x14D / 0x14E; any other message answers 1.
s32 func_shelter_b1_north_maintenance_walkway_8017D7A4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    _ShelterB1NorthMaintenanceWalkwayEvent event;
    s32                                    cmd;
    s32                                    snd;
    s16                                    flag;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 0xB) {
        goto message0E;
    }
    snd           = 0x540C0001;
    cmd           = 3;
    event.field_4 = snd;
    flag          = 0x14D;
start_event:
    event.field_0 = cmd;
    event.flagId  = flag;
    event.field_A = 0;
    return _shelterB1NorthMaintenanceWalkwayStartEvent(out, &event);
message0E:
    if (in->msgId == 0xE) {
        snd           = 0x540C0003;
        cmd           = 2;
        event.field_4 = snd;
        flag          = 0x14E;
        goto start_event;
    }
    return 1;
}

void func_shelter_b1_north_maintenance_walkway_8017D918(Task* arg0)
{
    SVECTOR unused;

    switch (arg0->state) {
        case 0:
            if (Gp_StateF0.field_0 == 1) {
                gGameSession->flowFlags |= 0x80;
                gGameSession->flowFlags |= 0x40;
                arg0->state++;
            }
            break;
        case 1:
            if (Gp_StateF0.field_6 == 0) {
                Gp_StateF0.field_1  = 0x3C;
                arg0->killCountdown = 0x3E;
                arg0->state++;
            }
            break;
        case 2:
            if (arg0->killCountdown == 0) {
                if (D_80071075 == 0) {
                    Gp_SpawnIfCapIdle(1, 0);
                    taskKill(arg0);
                }
            } else {
                arg0->killCountdown--;
            }
            break;
    }
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA34(void)
{
    return 0;
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA3C(void)
{
    return 0;
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA44(void)
{
    return 0;
}

void func_shelter_b1_north_maintenance_walkway_8017DA4C(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_north_maintenance_walkway_80184A84;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 2) {
        Task_SpawnFromTable(D_shelter_b1_north_maintenance_walkway_80184AAC, 0, 0, 0);
        if (GameFlag_GetNibble(0x157) == 0) {
            GameFlag_SetNibble(0x157, 1);
            Gp_SpawnIfCapIdle(4, 0);
        }
    }
    func_shelter_b1_north_maintenance_walkway_8017DB54(GameFlag_GetNibble(0x84));
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state: does nothing.
void func_shelter_b1_north_maintenance_walkway_8017DAF4(Task* task)
{
}

/// The room task's three states: set-up, idle and exit.
const TaskFuncTable3 D_shelter_b1_north_maintenance_walkway_8017D5D8 = {
    { func_shelter_b1_north_maintenance_walkway_8017DA4C, func_shelter_b1_north_maintenance_walkway_8017DAF4, taskKill },
};

/// The room task. Runs the handler for its current state from the room's
/// three-entry state table: set-up, an idle tick, and `taskKill`.
void func_shelter_b1_north_maintenance_walkway_8017DAFC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_north_maintenance_walkway_8017D5D8;
    sp.funcs[task->state](task);
}

void func_shelter_b1_north_maintenance_walkway_8017DB54(u8 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* rec;
    GpSprtCmd* cmd;
    s32        mode;

    rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    mode = arg0 & 0xFF;
    if (mode == 0) {
        cmd            = rec[2].field_4;
        cmd[2].field_4 = 1;
    } else if (mode == 1) {
        cmd            = rec[2].field_4;
        cmd[2].field_4 = 0;
    }
}

void func_shelter_b1_north_maintenance_walkway_8017DBC8(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60247;
        D_80115744  = 0x60253;
        D_8011573C  = 0x6025E;
        D_80115720  = 0x6026A;
        D_80115758  = 0x601CB;
        D_8011572C  = 0x601E7;
        D_80115750  = 0x60203;
        D_80115734  = 0x6021F;
        D_80115730  = 0x6022A;
        D_80115754  = 0x60235;
        arg0->state = 1;
    }

    switch (gGameSession->at4.loc.view) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184B18;
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[0], 0x200, 0x800);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[4], 0x200, -0x400);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184B08;
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[0], 0x200, 0x800);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[2], 0x200, 0x800);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[4], 0x200, -0x400);
            break;
        }
        case 4:
        case 6: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184B48;
            func_shelter_b1_north_maintenance_walkway_8017E55C(&p[0], 0x200);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-18], 0x200, 0);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-16], 0x200, 0);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-14], 0x200, 0);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-12], 0x200, -0x400);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-10], 0x200, -0x400);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-8], 0x200, -0x400);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[-6], 0x200, 0x800);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_north_maintenance_walkway_80184AB8;
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[0], 0x200, 0);
            func_shelter_b1_north_maintenance_walkway_8017DDE0(&p[6], 0x200, -0x400);
            break;
        }
    }
}
