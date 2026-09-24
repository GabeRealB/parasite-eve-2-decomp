#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// The twelve bytes the room's message handler stages before spawning a task
/// from `D_shelter_b2_main_corridor_80182C44`. `field_1` takes either the
/// triggering message's `field_3` or a fixed value, chosen together with
/// `field_0`. Once the staged block has been passed through
/// `func_shelter_b2_main_corridor_8017E0FC`, the event task copies it whole and
/// reads the copy as a destination (`field_0` to `field_3`: stage, area, warp,
/// room), a halfword to send to the slot-3 game pointer (`field_4`, -1 for
/// none) and a sound to play (`field_8`, zero for none).
///
/// The layout matches the event blocks other rooms stage the same way
/// (`ShelterB4WaterSupplyEventDesc`, `NeoArkObservatoryEventDesc`); whether they
/// are one type is open. `pad_6` is never touched here.
typedef struct ShelterB2MainCorridorEventDesc {
    /* 0x0 */ u8   field_0;
    /* 0x1 */ u8   field_1;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ u8   field_3;
    /* 0x4 */ s16  field_4;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s32  field_8;
} ShelterB2MainCorridorEventDesc;
STATIC_ASSERT_SIZEOF(ShelterB2MainCorridorEventDesc, 0xC);

/// Parameters of a room exit, staged by the room's message handler for the task
/// spawned from `D_shelter_b2_main_corridor_80182C08`, which runs `capCmd`,
/// plays `sndId` and then moves the player. `flag` names a game-flag nibble that
/// is set once the exit has been taken; once set, the handler no longer claims
/// the message. A non-zero `field_A` makes the exit task start helper task
/// 0x31.
typedef struct ShelterB2MainCorridorExit {
    s32 capCmd;
    s32 sndId;
    s16 flag;
    u8  field_A;
} ShelterB2MainCorridorExit;
STATIC_ASSERT_SIZEOF(ShelterB2MainCorridorExit, 0xC);

/// One water surface: a rectangle at (`x`, `z`) spanning `width` along X and
/// `depth` along Z. A list of them ends at an entry whose `end` is -1; `end`
/// is not otherwise read.
typedef struct ShelterB2MainCorridorSurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s32 end;
} ShelterB2MainCorridorSurface;

/// Per-surface values the water drawer keeps in a block taken from the
/// scratchpad stack at `0x1F8003FC` rather than in registers. `dx` and `dz`
/// are the spacing between vertices along X and Z, and `wave` the height the
/// sine wave adds to the vertex being placed.
typedef struct ShelterB2MainCorridorWaterWork {
    s16 y;
    s16 dx;
    s16 wave;
    s16 dz;
    s16 x;
    s16 z;
} ShelterB2MainCorridorWaterWork;

/// `Gp_StateF0.field_4` (0x801153F4). Declared as a one-element array so the
/// store keeps the in-struct memory attribute a struct-member store has, which
/// makes it alias the task's argument load and keeps the two in source order.
extern u8  D_801153F4[1];
extern u8  D_80115690;
extern s16 D_80071076;
extern s32 D_8007107C;
extern s8  D_8007217B;
extern s32 D_8011572C;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115758;
extern s32 Gp_LcgState;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Spawn argument of the helper task 0x31 the room's exit task starts.
extern GpStateBD8 D_shelter_b2_main_corridor_8018964C;

/// The outgoing message of the exit being taken; the exit task copies its
/// destination into the save location.
extern RoomEventMsg D_shelter_b2_main_corridor_80189654;

/// Cleared whenever the handler considers an exit, set once the exit task has
/// been spawned. Nothing else in the room reads it.
extern u8 D_shelter_b2_main_corridor_8018965C;

/// A second copy of the staged event block, taken whole once the block has been
/// passed through `func_shelter_b2_main_corridor_8017E0FC`.
extern ShelterB2MainCorridorEventDesc D_shelter_b2_main_corridor_80189664;

/// The exit being taken, read by the exit task.
extern ShelterB2MainCorridorExit D_shelter_b2_main_corridor_80189674;

/// The staged event block, read by the task spawned from
/// `D_shelter_b2_main_corridor_80182C44`.
extern ShelterB2MainCorridorEventDesc D_shelter_b2_main_corridor_80189684;

/// Descriptor of the task spawned after the staged block has been copied.
extern TaskDesc D_shelter_b2_main_corridor_801828E4;

/// Descriptor of the task that carries out a staged exit.
extern TaskDesc D_shelter_b2_main_corridor_80182C08;

/// The room's message table, installed by its first task state.
extern GpMsgEntry D_shelter_b2_main_corridor_80182C14[];

/// Descriptor of the tasks the room's message handler spawns.
extern TaskDesc D_shelter_b2_main_corridor_80182C44;

/// Passed by address to `func_800E8614` when the room's one-shot flag event
/// fires; its contents are not read here.
extern s32 D_shelter_b2_main_corridor_80182CA8;

/// Tasks the room's first task state spawns.
extern TaskDesc D_shelter_b2_main_corridor_80182DE0[];

/// The room's water surfaces.
extern ShelterB2MainCorridorSurface D_shelter_b2_main_corridor_80182DEC[];

/// Height of the water surfaces.
extern s16 D_shelter_b2_main_corridor_80182E28;

/// Light positions the per-view drawer places beams and glows at.
extern SVECTOR D_shelter_b2_main_corridor_80182F7C[];
extern SVECTOR D_shelter_b2_main_corridor_80182F9C[];
extern SVECTOR D_shelter_b2_main_corridor_80182FAC[];
extern SVECTOR D_shelter_b2_main_corridor_80182FBC[];
extern SVECTOR D_shelter_b2_main_corridor_80182FCC[];
extern SVECTOR D_shelter_b2_main_corridor_8018305C[];
extern SVECTOR D_shelter_b2_main_corridor_8018306C[];

/// The two points, relative to the effect's parent coordinate, that the trail
/// effect's two edges follow. The second is also read by its own name.
extern SVECTOR D_shelter_b2_main_corridor_801830BC[];
extern SVECTOR D_shelter_b2_main_corridor_801830C4;

/// Areas the room re-applies when it clears its pending game-flag state.
extern GpAreaApplyRec D_shelter_b2_main_corridor_80189644;

/// Cursor into the primitive area the water surface is written to.
extern u8* D_shelter_b2_main_corridor_80189660;

s32  func_shelter_b2_main_corridor_8017E0FC(RoomEventMsg* in, RoomEventMsg* out);
void func_shelter_b2_main_corridor_8017E264(RoomEventMsg* msg);
void func_shelter_b2_main_corridor_8017E2D4(Task* arg0);
void func_shelter_b2_main_corridor_8017E330(Task* arg0);
void func_shelter_b2_main_corridor_8017E390(Task* arg0);
void func_shelter_b2_main_corridor_8017EBF4(Task* arg0);
void func_shelter_b2_main_corridor_8017F078(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_main_corridor_8017F860(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b2_main_corridor_8017FC4C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_main_corridor_8017FEE8(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b2_main_corridor_801806D0(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_main_corridor_80180BF0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b2_main_corridor_8018101C(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b2_main_corridor_801818A0(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_shelter_b2_main_corridor_80181F20(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// Carries out a staged event once the message handler has passed it through
/// `func_shelter_b2_main_corridor_8017E0FC`, from the copy in
/// `D_shelter_b2_main_corridor_80189664`. State 0 sends the event's `field_4`
/// to the slot-3 game pointer as message 0x3EE, unless it is -1, in which case
/// it skips to state 2; state 1 polls that pointer with message 0x3F0 until it
/// answers 0. States 2 and 3 queue the event's sound `field_8`, if any, and
/// wait for its voice to end. State 4 copies the event's stage, area, warp and
/// room into the save location and spawns the room-load task 0x11.
void func_shelter_b2_main_corridor_8017D6BC(Task* arg0)
{
    GpMsg3EE msg;
    void*    slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.field_12 = D_shelter_b2_main_corridor_80189664.field_4;
            if (msg.field_12 == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_shelter_b2_main_corridor_80189664.field_8 == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_shelter_b2_main_corridor_80189664.field_8, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_shelter_b2_main_corridor_80189664.field_8) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            D_80071076                = 1;
            Mc_SaveData.at4.loc.stage = D_shelter_b2_main_corridor_80189664.field_0;
            Mc_SaveData.at4.loc.area  = D_shelter_b2_main_corridor_80189664.field_1;
            Mc_SaveData.at4.loc.warp  = D_shelter_b2_main_corridor_80189664.field_2;
            Mc_SaveData.at4.loc.room  = D_shelter_b2_main_corridor_80189664.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// Carries out a room exit staged by the message handler in
/// `D_shelter_b2_main_corridor_80189674`. State 0 runs the exit's capture
/// command; state 1 waits for it to finish and, when the exit's `field_A` asks
/// for it, spawns helper task 0x31; states 2 and 3 queue the exit's stage
/// sound, if any, and wait for its voice to end. State 4 copies the destination
/// of the outgoing message `D_shelter_b2_main_corridor_80189654` into the save
/// location and spawns the room-load task 0x11.
void func_shelter_b2_main_corridor_8017D82C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4[0] = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_main_corridor_80189674.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_main_corridor_80189674.field_A != 0) {
                    D_shelter_b2_main_corridor_8018964C.field_0 = 0;
                    D_shelter_b2_main_corridor_8018964C.field_1 = 0;
                    D_shelter_b2_main_corridor_8018964C.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_main_corridor_8018964C);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_main_corridor_80189674.sndId != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_main_corridor_80189674.sndId, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_main_corridor_80189674.sndId)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_main_corridor_80189654.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_main_corridor_80189654.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b2_main_corridor_80189654.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b2_main_corridor_8017D9C4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    ShelterB2MainCorridorExit  staged;
    ShelterB2MainCorridorExit* p;
    s32                        capCmd;
    s16                        flag;
    s32                        sndId;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x1B && GameFlag_GetNibble(0xAB) == 0) {
        if (in->field_5 != 0) {
            return 0;
        }
        Gp_SetNibbleIf(in->field_6, 2);
        Gp_RunCapCmd1(1);
        return 0;
    }
    if (in->msgId == 0x1F && GameFlag_GetNibble(0xB1) == 0) {
        if (in->field_5 != 0) {
            return 0;
        }
        Gp_SetNibbleIf(in->field_6, 2);
        Gp_RunCapCmd1(2);
        return 0;
    }
    if ((in->msgId == 0x1F || in->msgId == 0x20 || in->msgId == 0x1B) && GameFlag_GetNibble(0xD1) == 2) {
        if (in->field_5 != 0) {
            return 2;
        }
        Gp_RunCapCmd1(4);
        return 2;
    }
    if (in->msgId == 0x22) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId        = 0x54210001;
        capCmd       = 0xA;
        staged.sndId = sndId;
        flag         = 0x133;
    } else if (in->msgId == 0x20) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId        = 0x54210001;
        capCmd       = 9;
        staged.sndId = sndId;
        flag         = 0x134;
    } else if (in->msgId == 0x1B) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId        = 0x54210001;
        capCmd       = 0xB;
        staged.sndId = sndId;
        flag         = 0x135;
    } else if (in->msgId == 0x1F) {
        func_shelter_b2_main_corridor_8017E264(out);
        sndId        = 0x54210001;
        capCmd       = 0xC;
        staged.sndId = sndId;
        flag         = 0x136;
    } else {
        return 1;
    }
    p                                   = &staged;
    staged.capCmd                       = capCmd;
    staged.flag                         = flag;
    staged.field_A                      = 0;
    D_shelter_b2_main_corridor_8018965C = 0;
    if (GameFlag_GetNibble(p->flag) == 0 || p->flag == 0) {
        if (out->field_5 != 0) {
            return 2;
        }
        D_shelter_b2_main_corridor_80189654 = *out;
        D_shelter_b2_main_corridor_80189674 = staged;
        if (p->flag != 0) {
            GameFlag_SetNibble(p->flag, 1);
        }
        Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C08, 0, 0, 0);
        D_shelter_b2_main_corridor_8018965C = 1;
        return 2;
    }
    return 1;
}

s32 func_shelter_b2_main_corridor_8017DC88(Task* arg0, s32 arg1, GpMsg13EF* arg2, s32 arg3)
{
    s32 id;

    if (arg2->field_2 == 0xA) {
        if (arg2->field_3 == 7) {
            if (GameFlag_GetNibble(0xAE) != 0) {
                if (GameFlag_GetNibble(0xDA) != 0) {
                    D_shelter_b2_main_corridor_80189684.field_0 = 5;
                    D_shelter_b2_main_corridor_80189684.field_1 = arg2->field_3;
                } else {
                    D_shelter_b2_main_corridor_80189684.field_0 = 4;
                    D_shelter_b2_main_corridor_80189684.field_1 = 0x31;
                }
                D_shelter_b2_main_corridor_80189684.field_3 = 1;
                D_shelter_b2_main_corridor_80189684.field_2 = 1;
                D_shelter_b2_main_corridor_80189684.field_8 = 0;
                D_shelter_b2_main_corridor_80189684.field_4 = -1;
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 0, 6, 0);
            } else {
                Gp_RunCapCmd1(3);
                Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 1, 0x1C4, 0);
            }
        }
        if (arg2->field_3 == 8) {
            if (GameFlag_GetNibble(0xD1) == 2) {
                Gp_RunCapCmd1(4);
                return 0;
            }
            if (GameFlag_GetNibble(0xF8) != 0) {
                Gp_RunCapCmd1(8);
                Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 1, 0x1AF, 0);
                return 0;
            }
            if (GameFlag_GetNibble(0xDF) != 0) {
                id = 0xD;
            } else {
                id = 7;
            }
            D_shelter_b2_main_corridor_80189684.field_0 = 5;
            D_shelter_b2_main_corridor_80189684.field_1 = arg2->field_3;
            D_shelter_b2_main_corridor_80189684.field_3 = 1;
            D_shelter_b2_main_corridor_80189684.field_2 = 1;
            D_shelter_b2_main_corridor_80189684.field_8 = 0;
            D_shelter_b2_main_corridor_80189684.field_4 = -1;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 0, id, 0);
        }
    }
    if (arg2->field_2 == 1 && GameFlag_GetNibble(0x82) >= 2 && GameFlag_GetNibble(0xD3) == 0) {
        GameFlag_SetNibble(0xD3, 1);
        GameFlag_SetNibble(0xAE, 1);
        GameFlag_SetNibble(0x1C4, 0);
        func_800E8614((s32)&D_shelter_b2_main_corridor_80182CA8, 0);
    }
    return 0;
}

/// The room task's three states, run by
/// `func_shelter_b2_main_corridor_8017E338`: install the message table and
/// spawn the room's tasks, idle, and end.
const TaskFuncTable3 D_shelter_b2_main_corridor_8017D5F0 = {
    { func_shelter_b2_main_corridor_8017E2D4, func_shelter_b2_main_corridor_8017E330, taskKill }
};

void func_shelter_b2_main_corridor_8017DEB0(Task* arg0)
{
    RoomEventMsg param;
    s32          (*resolve)(RoomEventMsg*, RoomEventMsg*);

    switch (arg0->state) {
        case 0:
            D_801153F4[0] = 1;
            Gp_RunCapCmd1(arg0->spawnArg1);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            arg0->state++;
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xC) {
                D_801153F4[0] = 0;
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                break;
            }
            arg0->state++;
            break;
        case 3:
            arg0->state++;
            break;
        case 4:
            if (D_shelter_b2_main_corridor_80189684.field_1 == 7 && GameFlag_GetNibble(0xD1) == 2) {
                GameFlag_SetNibble(0x4C, 9);
            }
            if (D_shelter_b2_main_corridor_80189684.field_1 == 0x31) {
                if (GameFlag_GetNibble(0xD1) == 2) {
                    GameFlag_SetNibble(0x4C, 9);
                }
                if (GameFlag_GetNibble(0xDA) == 0) {
                    GameFlag_SetNibble(0xDA, 1);
                    GameFlag_SetNibble(0x7A, 5);
                }
            }
            if (D_shelter_b2_main_corridor_80189684.field_1 == 8) {
                if (GameFlag_GetNibble(0xDF) == 1) {
                    GameFlag_SetNibble(0xF8, 1);
                }
            }
            resolve = func_shelter_b2_main_corridor_8017E0FC;
            Gp_MsgPlayerWeapon(0);
            param.msgId   = D_shelter_b2_main_corridor_80189684.field_1;
            param.field_2 = D_shelter_b2_main_corridor_80189684.field_2;
            param.field_3 = D_shelter_b2_main_corridor_80189684.field_3;
            param.field_5 = 0;
            resolve(&param, &param);
            D_shelter_b2_main_corridor_80189684.field_1 = param.msgId;
            D_shelter_b2_main_corridor_80189684.field_2 = param.field_2;
            D_shelter_b2_main_corridor_80189684.field_3 = param.field_3;
            D_shelter_b2_main_corridor_80189664         = D_shelter_b2_main_corridor_80189684;
            Task_SpawnFromTable(&D_shelter_b2_main_corridor_801828E4, 0, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Answers the marker query for one Neo Ark map room; the Neo Ark area map
/// carries the same body. Most rooms have no
/// marker; the five that do read a GameFlag nibble, either straight (plus one,
/// rooms 7 / 13 / 32) or folded into a fixed set of states (rooms 20 and 21).
s32 func_shelter_b2_main_corridor_8017E0FC(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_5 == 0) {
        switch (in->msgId) {
            case 7:
                out->field_3 = GameFlag_GetNibble(0xE1) + 1;
                break;
            case 13:
                out->field_3 = GameFlag_GetNibble(0xD9) + 1;
                break;
            case 20:
                out->field_3 = 1;
                if (GameFlag_GetNibble(0xDD) != 0) {
                    if (GameFlag_GetNibble(0xDC) != 0) {
                        out->field_3 = 3;
                    } else {
                        out->field_3 = 2;
                    }
                }
                break;
            case 21:
                if (GameFlag_GetNibble(0xE9) != 0) {
                    out->field_3 = 4;
                } else {
                    out->field_3 = 1;
                }
                break;
            case 32:
                out->field_3 = GameFlag_GetNibble(0xDD) + 1;
                break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            default:
                break;
        }
    }
    return 1;
}

s32 func_shelter_b2_main_corridor_8017E1CC(void)
{
    return 0;
}

s32 func_shelter_b2_main_corridor_8017E1D4(void)
{
    return 0;
}

s32 func_shelter_b2_main_corridor_8017E1DC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 9) {
        SndEvt_EnqueueType6(0x54210000 | 9, 0, 0);
    }
    return 0;
}

/// Waits for the capture command the message handler started to finish, then
/// sets the game-flag nibble named by the task's spawn argument to 2 unless
/// the capture ended on event key 0xC, and ends the task.
void func_shelter_b2_main_corridor_8017E210(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() != 0xC) {
            GameFlag_SetNibble(arg0->spawnArg1, 2);
        }
        taskKill(arg0);
    }
}

void func_shelter_b2_main_corridor_8017E264(RoomEventMsg* msg)
{
    if ((GameFlag_GetNibble(0x4C) == 9) && (GameFlag_GetNibble(0xD1) == 3) && (msg->field_5 == 0)) {
        GameFlag_SetNibble(0x4C, 0);
        Gp_ApplyAreaRecs(&D_shelter_b2_main_corridor_80189644);
    }
}

void func_shelter_b2_main_corridor_8017E2D4(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_main_corridor_80182C14;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b2_main_corridor_80182DE0, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state.
void func_shelter_b2_main_corridor_8017E330(Task* arg0)
{
}

/// Runs one tick of the room task through the three-state table
/// `D_shelter_b2_main_corridor_8017D5F0`, copying the table onto the stack and
/// calling the entry for the task's current state.
void func_shelter_b2_main_corridor_8017E338(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_main_corridor_8017D5F0;
    sp.funcs[task->state](task);
}

/// Draws each surface in `D_shelter_b2_main_corridor_80182DEC` at height
/// `D_shelter_b2_main_corridor_80182E28` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along X, each strip running along Z and
/// projected through the view matrix. The seam between the strips is lifted by
/// a sine wave that runs along Z and scrolls with the display frame counter.
/// The outer edges are coloured (0x80, 0, 0) and the seam (0x20, 0x20, 0x20);
/// each quad is followed by a draw-mode packet selecting blend mode 2. Quads
/// the projection flags as invalid are skipped. The primitive cursor is reset
/// to the current buffer's half of the primitive area first, and nothing is
/// drawn in views 10 and 11 of stage 4, area 0x21. The per-surface values live
/// in a work block pushed on the scratchpad stack for the duration of the call.
/// Runs as the water task's second state; the task itself is not read.
void func_shelter_b2_main_corridor_8017E390(Task* arg0)
{
    SVECTOR                         v0, v1, v2, v3;
    s32                             sxy0, sxy1, sxy2, sxy3;
    s32                             p, flag;
    s32                             phase;
    ShelterB2MainCorridorSurface*   e;
    ShelterB2MainCorridorWaterWork* w;
    u8*                             head;
    POLY_G4*                        poly;
    DR_MODE*                        dr;
    s32                             otz;
    s32                             i;
    GpAreaKey*                      k;

    e     = D_shelter_b2_main_corridor_80182DEC;
    phase = -(gDisplayState.animFrame * 16);
    k     = &gGameSession->at4.loc;
    if (k->stage == 4) {
        if (k->area == 0x21) {
            if ((u32)(gGameSession->at4.loc.view - 0xA) < 2) {
                return;
            }
        }
    }
    if (D_8007217B == 0) {
        D_shelter_b2_main_corridor_80189660 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b2_main_corridor_80189660 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    head              = *(u8**)0x1F8003FC;
    gGfxViewCoord.flg = 0;
    *(u8**)0x1F8003FC = head - 0xC;
    w                 = (ShelterB2MainCorridorWaterWork*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = D_shelter_b2_main_corridor_80182E28;
    for (; e->end != -1; e++) {
        w->dx = e->width / 2;
        w->dz = e->depth / 16;
        w->x  = e->x;
        w->z  = e->z;
        for (i = 0; i < 16; i++) {
            v0.vx   = w->x;
            v0.vy   = w->y;
            v0.vz   = w->z + w->dz * i;
            v1.vx   = w->x;
            v1.vy   = w->y;
            v1.vz   = w->z + w->dz * (i + 1);
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                                = (POLY_G4*)D_shelter_b2_main_corridor_80189660;
                D_shelter_b2_main_corridor_80189660 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0x80;
                poly->r1         = 0x80;
                poly->g0         = 0;
                poly->b0         = 0;
                poly->g1         = 0;
                poly->b1         = 0;
                poly->r2         = 0x20;
                poly->g2         = 0x20;
                poly->b2         = 0x20;
                poly->r3         = 0x20;
                poly->g3         = 0x20;
                poly->b3         = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                  = (DR_MODE*)D_shelter_b2_main_corridor_80189660;
                D_shelter_b2_main_corridor_80189660 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx   = w->x + w->dx;
            v1.vy   = w->y + w->wave;
            v1.vz   = w->z + w->dz * (i + 1);
            v2.vx   = w->x + w->dx * 2;
            v2.vy   = w->y;
            v2.vz   = w->z + w->dz * i;
            v3.vx   = w->x + w->dx * 2;
            v3.vy   = w->y;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                                = (POLY_G4*)D_shelter_b2_main_corridor_80189660;
                D_shelter_b2_main_corridor_80189660 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r2         = 0x80;
                poly->r3         = 0x80;
                poly->g2         = 0;
                poly->b2         = 0;
                poly->g3         = 0;
                poly->b3         = 0;
                poly->r0         = 0x20;
                poly->g0         = 0x20;
                poly->b0         = 0x20;
                poly->r1         = 0x20;
                poly->g1         = 0x20;
                poly->b1         = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                  = (DR_MODE*)D_shelter_b2_main_corridor_80189660;
                D_shelter_b2_main_corridor_80189660 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// The water task: runs its state, first
/// `func_shelter_b2_main_corridor_8017EBF4` and then the surface drawer above,
/// and each tick publishes the room's water height to the session.
void func_shelter_b2_main_corridor_8017EB8C(Task* task)
{
    TaskFunc states[2] = { func_shelter_b2_main_corridor_8017EBF4, func_shelter_b2_main_corridor_8017E390 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b2_main_corridor_80182E28;
}

/// First state of the water task: clears the session's `field_80` or
/// `field_7E`, chosen by `D_8007217B`, and advances to the next state.
void func_shelter_b2_main_corridor_8017EBF4(Task* arg0)
{
    if (D_8007217B == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b2_main_corridor_8017EC34(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_80115758  = 0x601D3;
            D_8011572C  = 0x601EF;
            D_80115750  = 0x6020B;
            D_8011574C  = 0x6016A;
            D_80115738  = 0x6016B;
            arg0->state = 1;
        case 1:
            switch (Gp_GetViewIndex() & 0xFF) {
                case 2: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182F7C;
                    func_shelter_b2_main_corridor_8017FEE8(&p[0], 0x200, 0, 0x111);
                    func_shelter_b2_main_corridor_8017FEE8(&p[2], 0x200, 0, 0x111);
                    break;
                }
                case 3: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182FAC;
                    func_shelter_b2_main_corridor_8017FEE8(&p[0], 0x200, 0x800, 0x10);
                    func_shelter_b2_main_corridor_8017FEE8(&p[2], 0x200, 0, 0x10);
                    func_shelter_b2_main_corridor_8017FEE8(&p[4], 0x200, 0, 0x10);
                    func_shelter_b2_main_corridor_8017FEE8(&p[6], 0x200, 0x800, 0x100);
                    func_shelter_b2_main_corridor_801806D0(&p[14], 1, 0x300);
                    func_shelter_b2_main_corridor_801806D0(&p[21], 1, 0x300);
                    break;
                }
                case 4: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182F9C;
                    func_shelter_b2_main_corridor_8017FEE8(&p[0], 0x200, 0x800, 0x10);
                    func_shelter_b2_main_corridor_8017FEE8(&p[24], 0x200, 0x800, 0x111);
                    func_shelter_b2_main_corridor_8017FEE8(&p[28], 0x200, -0x400, 0x111);
                    func_shelter_b2_main_corridor_8017FEE8(&p[32], 0x200, 0x800, 0x111);
                    break;
                }
                case 5:
                    func_shelter_b2_main_corridor_8017FEE8(D_shelter_b2_main_corridor_80182FBC, 0x200, 0, 0x10);
                    break;
                case 6:
                    func_shelter_b2_main_corridor_8017FEE8(D_shelter_b2_main_corridor_80182FAC, 0x200, 0x800, 0x10);
                case 7: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182FCC;
                    func_shelter_b2_main_corridor_8017FEE8(&p[0], 0x200, 0, 0x10);
                    func_shelter_b2_main_corridor_8017FEE8(&p[2], 0x200, 0x800, 0x100);
                    break;
                }
                case 8: {
                    SVECTOR* p = D_shelter_b2_main_corridor_80182FAC;
                    func_shelter_b2_main_corridor_8017FEE8(&p[0], 0x200, 0x800, 0x10);
                    func_shelter_b2_main_corridor_8017FEE8(&p[24], 0x200, 0x800, 0x111);
                    func_shelter_b2_main_corridor_8017FEE8(&p[28], 0x200, -0x400, 0x111);
                    func_shelter_b2_main_corridor_8017FEE8(&p[32], 0x200, 0x800, 0x111);
                    break;
                }
                case 9:
                    func_shelter_b2_main_corridor_8017FEE8(D_shelter_b2_main_corridor_80182FCC, 0x200, 0, 0x10);
                    break;
                case 10: {
                    SVECTOR* p = D_shelter_b2_main_corridor_8018305C;
                    func_shelter_b2_main_corridor_8017FEE8(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_main_corridor_8017FEE8(&p[4], 0x200, 0x800, 0x111);
                    func_shelter_b2_main_corridor_8017FEE8(&p[8], 0x200, 0x400, 0x111);
                    break;
                }
                case 11: {
                    SVECTOR* p = D_shelter_b2_main_corridor_8018306C;
                    func_shelter_b2_main_corridor_8017FEE8(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_main_corridor_8017FEE8(&p[4], 0x200, 0x800, 0x111);
                    func_shelter_b2_main_corridor_8017FEE8(&p[8], 0x200, 0x400, 0x111);
                    break;
                }
            }
            break;
    }
}

/// Per-frame driver of an expanding, fading flash. While the room's event
/// state is 0 it updates the task's coordinate, ticks the age counter and
/// draws the flash through `func_shelter_b2_main_corridor_8017F078` at size
/// `field_26`, seeded from the spawn argument and grown by 0x20 a frame, and
/// brightness `field_24`, which starts at 0x40 and drops by 2 a frame; the
/// first frame also turns the coordinate about Y by a random angle. The work
/// block is released once the brightness falls under 2. Once the event state
/// is non-zero it only draws, releasing the block from event state 4 on.
void func_shelter_b2_main_corridor_8017EF24(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b2_main_corridor_8017F078(coord, (s16)work->field_26, (s16)work->field_24);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->field_22++;
        if (task->state == 0) {
            work->field_24 = 0x40;
            work->field_26 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->flg  = 0;
            task->state = 1;
        }
        work->field_26 += 0x20;
        func_shelter_b2_main_corridor_8017F078(coord, (s16)work->field_26, (s16)work->field_24);
        work->field_24 -= 2;
        if ((s16)work->field_24 < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat textured quad at `arg0`: the four corners of the unit quad
/// `D_80111E38`, scaled by `arg1`, are rotated by the coordinate's world
/// matrix and offset by its translation, then projected through `GsWSMATRIX`.
/// If the projection is valid, one semi-transparent `POLY_FT4` (tpage 0x2B,
/// clut 0x43D1, UV 0,0x38 to 0x37,0x6F) is queued with all three colour
/// channels set to `arg2`. The work block lives on the scratchpad stack.
void func_shelter_b2_main_corridor_8017F078(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    MATRIX*        wm;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    wm  = &arg0->workm;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D1;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        setRGB0(prim, arg2, arg2, arg2);
        prim->u0 = 0;
        prim->u1 = 0x37;
        prim->u2 = 0;
        prim->v2 = 0x6F;
        prim->u3 = 0x37;
        prim->v3 = 0x6F;
        setSemiTrans(prim, 1);
        prim->x0 = *(u16*)&block->sxy0.vx;
        prim->y0 = *(u16*)&block->sxy0.vy;
        prim->x1 = *(u16*)&block->sxy1.vx;
        prim->y1 = *(u16*)&block->sxy1.vy;
        prim->x2 = *(u16*)&block->sxy2.vx;
        prim->y2 = *(u16*)&block->sxy2.vy;
        prim->x3 = *(u16*)&block->sxy3.vx;
        prim->y3 = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

void func_shelter_b2_main_corridor_8017F3AC(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b2_main_corridor_8017F860(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            } else {
                func_shelter_b2_main_corridor_8017FC4C(coord, work->field_20, (s16)work->field_24);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            state          = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                kind           = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            return;
        case 1:
            func_shelter_b2_main_corridor_8017F860(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            func_shelter_b2_main_corridor_8017FC4C(coord, work->field_20, (s16)work->field_24);
            break;
        default:
            return;
    }
    if ((s16)work->field_2A != 0) {
        coord->coord.t[0] += work->field_10.vx;
        coord->coord.t[1] += work->field_10.vy;
        coord->coord.t[2] += work->field_10.vz;
        coord->flg         = 0;
        work->field_10.vy += 6;
    }
    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D3) is queued
/// as a square rotated by angle `arg3` about the projected point, with
/// on-screen half-diagonal `(s16)arg2 * 31 / otz`. `arg1` picks the 32-texel
/// frame at u = `arg1 * 32`, v 0xE0 to 0xFF.
void func_shelter_b2_main_corridor_8017F860(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    s32                u0;
    s32                ang;
    s32                ang2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    TOUCH_REG_USE(arg2, scratch);
    head                                        = *scratch;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw19Scratch*)(head - 0x1C);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (arg1 & 0xFFFF) << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Draws an upright sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued
/// as an axis-aligned square of half-side `r = (s16)arg2 * 55 / otz`, raised
/// so the projected point sits three quarters of the way down it. `arg1` picks
/// one of eight 56-texel frames in a grid four wide, starting at v 0x70.
void func_shelter_b2_main_corridor_8017FC4C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw23Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    u32                cell;
    s32                u1;
    s32                vbase;
    s32                v0;
    s32                v1;
    s32                sarg;
    s32                t;
    s16                xy;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG_USE(arg2, scratch);
    head                                        = *scratch;
    ((RoomDraw23Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw23Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    tex                                         = arg1;
    SOFT_TOUCH_REG(tex);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw23Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw23Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw23Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        SOFT_BARRIER();
        cell  = (u16)tex;
        tex   = (cell & 3) * 0x38;
        vbase = ((cell & 7) >> 2) * 0x38;
        v0    = vbase + 0x70;
        SOFT_USE_REG(v0);
        u1       = tex + 0x37;
        v1       = vbase - 0x59;
        prim->v2 = v1;
        prim->v3 = v1;
        TOUCH_REG(u1);
        sarg          = (s16)arg2;
        prim->v0      = v0;
        prim->v1      = v0;
        t             = sarg * 0x38;
        prim->u0      = tex;
        prim->u1      = u1;
        prim->u2      = tex;
        prim->u3      = u1;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        v1            = (*(u16*)&block->sy - *(u16*)&block->radius) - (block->radius >> 1);
        xy            = v1;
        ds            = &gDisplayState;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + (block->radius >> 1);
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Draws a flickering light beam from `arg0[0]` to `arg0[1]`. Both points are
/// projected through the view matrix; unless the far end is nearer than OTZ
/// 0x11, gouraud `POLY_G4` wedges around each end (radius `(s16)arg1 * 64 /
/// otz` at that end) are joined by quads between the two, each fading from the
/// beam colour on the axis to black at the rim. `arg2` turns the wedges about
/// the axis. `arg3` packs the colour: the red factor in bits 8-15 and the
/// green and blue factors in bits 4 and 0, each multiplying an intensity that
/// alternates between 0x20 and 0x28 with the display frame counter.
void func_shelter_b2_main_corridor_8017FEE8(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 1);
        base      = (s16)arg2;
        b         = blend * (arg3 & 1);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            SCHED_BARRIER();
            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Draws a flickering glow sprite at the world-space point `arg0`. The point
/// is projected through the view matrix; unless it is nearer than OTZ 0x11,
/// one semi-transparent `POLY_FT4` (tpage 0x2B) is queued as an axis-aligned
/// square of half-side `(s16)arg2 * 39 / otz` centred on it. `arg1` picks the
/// 40-texel frame at u = `arg1 * 40` and its clut `0x4380 | (arg1 & 0x3F)`.
/// All three colour channels alternate between 0x20 and 0x30 with the display
/// frame counter.
void func_shelter_b2_main_corridor_801806D0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw25Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0xC;
    block   = (RoomDraw25Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        ds          = &gDisplayState;
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw25Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sx - *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sx + *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sy - *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw25Scratch*)tmp)->sy + *(u16*)&((RoomDraw25Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw25Scratch*)(head - 0xC))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0xC;
}

/// Per-frame driver of a burst of light in red, half blue and quarter green.
/// Over the number of frames given by the spawn argument the burst's
/// brightness `scale` and size `angle` grow together, drawn as a glow at that
/// size, a dimmer glow at twice it and a ring closing in around them. At the
/// peak the screen is flashed in the burst's colour, and the burst then fades
/// out through a larger billboard glow, shrinking by 8 and dimming by 0x10 a
/// frame until its brightness drops to 0x10. The work block is then released,
/// as it is once the room's event state reaches 4; from event state 1 on the
/// burst is no longer advanced or drawn.
void func_shelter_b2_main_corridor_8018094C(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = (u16)work->scale >> 1;
                func_shelter_b2_main_corridor_8018101C(coord, (s16)work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b2_main_corridor_8018101C(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b2_main_corridor_80180BF0(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = (u16)work->scale >> 2;
                    rgb[2]      = (u16)work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if ((s16)work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    func_shelter_b2_main_corridor_80181F20(coord, (s16)((s16)work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Draws a ring around the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, sixteen gouraud
/// `POLY_G4` segments are queued between on-screen radii `(s16)arg1 * 64 /
/// (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`, black at the first
/// and coloured `rgb` at the second.
void func_shelter_b2_main_corridor_80180BF0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = *(u16*)&arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws a round glow at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, eight gouraud
/// `POLY_G4` wedges of on-screen radius `(s16)arg1 * 64 / (otz + 1)` are
/// queued around the projected point, coloured `rgb` at the centre and black
/// at the rim.
void func_shelter_b2_main_corridor_8018101C(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Per-frame driver of a ribbon trail swept by two points of a moving parent.
/// The first frame allocates two eight-slot histories of coordinates, places
/// the task's own coordinate at the first point under the parent and fills
/// every slot with the two points' current positions. Each later frame records
/// the two positions in the slot the age counter selects and draws the ribbon
/// through `func_shelter_b2_main_corridor_801818A0`. The work block is
/// released once the age reaches the spawn argument; from the room's event
/// state 2 on the effect is frozen.
void func_shelter_b2_main_corridor_801813B0(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    coords   = (GsCOORDINATE2*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GsCOORDINATE2*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_shelter_b2_main_corridor_801830BC[0].vx;
                objCoord->coord.t[1] = D_shelter_b2_main_corridor_801830BC[0].vy;
                objCoord->coord.t[2] = D_shelter_b2_main_corridor_801830BC[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_b2_main_corridor_801830BC[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_shelter_b2_main_corridor_801830C4.vx;
                coord.coord.t[1] = D_shelter_b2_main_corridor_801830C4.vy;
                coord.coord.t[2] = D_shelter_b2_main_corridor_801830C4.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_shelter_b2_main_corridor_801818A0(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the ribbon between two eight-slot coordinate histories `arg0` and
/// `arg1` as seven gouraud `POLY_G4` quads, walking back from the newest slot
/// `arg2`; each quad joins the positions of two consecutive slots in both
/// histories and is skipped when its projection is invalid. The ribbon fades
/// from intensity 0x40 at the newest slot by 9 per slot. `arg3` packs the
/// colour: the red factor in bits 8 up and the green and blue factors in bits
/// 4-5 and 0-1, each multiplying that intensity.
void func_shelter_b2_main_corridor_801818A0(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GsCOORDINATE2*     a;
    GsCOORDINATE2*     b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomDraw03Scratch);
        blk                     = (RoomDraw03Scratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomDraw03Scratch);
}

/// Per-frame driver of an explosion at the task's coordinate. The first frame
/// spawns effect 0x60076 and then either, with a non-zero spawn argument,
/// effect 0x60070 and a spray of further 0x60070 sparks thrown at random
/// velocities over the next frames, or two 0x6007C effects and two expanding
/// rings drawn through `func_shelter_b2_main_corridor_80180BF0` in a fading
/// orange. The work block is released once the age reaches 7, or
/// when the room's event state reaches 4; from event state 1 on nothing is
/// advanced or drawn.
void func_shelter_b2_main_corridor_80181C98(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    u8             rgb[4];

    objCoord = ((TmdObject*)task->extra)->coords;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = (u16)work->angle >> 1;
            rgb[2]       = (u16)work->angle >> 2;
            func_shelter_b2_main_corridor_80180BF0(objCoord, 0x100, 0x100, rgb);
            func_shelter_b2_main_corridor_80180BF0(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a star-shaped flare at the coordinate's world position. The position
/// is projected through `GsWSMATRIX`; if the projection is valid, gouraud
/// `POLY_G4` wedges are queued around the projected point: a glow of on-screen
/// radius `r = arg1 * 64 / (otz + 1)` at half the colour `arg2`, a glow of
/// radius `r / 2` at the full colour, and four spikes reaching out to `2 * r`
/// at half the colour. Every wedge fades from its colour at the centre to
/// black.
void func_shelter_b2_main_corridor_80181F20(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
