#include "common.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "rooms/dryfield_motel_room_1.h"

#include "gameplay/actor.h"
#include "gameplay/attachments.h"
#include "gameplay/display.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_targets.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"

/// Main-executable globals with no module header yet: `Player_Status.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `gDisplayState.pendingMode` and `Gp_StateC08.field_A` (the cutscene mode flag) gate the room task's
/// setup, and `Mc_SaveData[0].characterId` picks which of the two weapon-id bases that record
/// uses.

/// The cutscene script's two blocks, handed to `func_800E8634` by the room
/// task's state 0.
extern s32 D_dryfield_motel_room_1_8017E160;
extern s32 D_dryfield_motel_room_1_8017E340;

/// The room's script driver: runs the action `func_dryfield_motel_room_1_8017DFB0`
/// left in `Dmr1Work::field_2C`. Actions 1 and 2 send the 0x7DA message to the
/// slot-4 task and one of the two placement pairs as message 0x7D4 (action 2
/// also sends 0x3F3 to the slot-3 task); 3, 4 and 5 play a sound. Each of these
/// runs once and clears the action. Action 6 runs over several frames with
/// `field_2E` as its step: it sends a slot-3 weapon record, turns the
/// `field_34` angle one way or the other each frame while passing the stored
/// player position back as message 0x3E9, and ends four frames after the turn
/// completes, when it clears the action itself. Every path through
/// `func_dryfield_motel_room_1_8017DD3C` except its early return and its kill
/// ends here.
static void func_dryfield_motel_room_1_8017D7AC(Task* arg0)
{
    Dmr1Work*     work = (Dmr1Work*)arg0->work;
    PlayerStatus* cfg;
    s32           anim;
    s32           weaponId;
    Dmr1DriverBuf buf;
    GpAnimArg*    rec;

    switch (work->field_2C) {
        case 1:
            buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
            buf.msg.from.loc.area  = gGameSession->at4.loc.area;
            buf.msg.command        = 1;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf.msg, 0x7DB);
            Gp_DispatchMsgPtr(work->field_4, 0x7D4, &D_dryfield_motel_room_1_8017E0D0[0], 0);
            Gp_DispatchMsgPtr(work->field_8, 0x7D4, &D_dryfield_motel_room_1_8017E0D0[1], 0);
            break;
        case 2:
            buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
            buf.msg.from.loc.area  = gGameSession->at4.loc.area;
            buf.msg.command        = 2;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf.msg, 0x7DB);
            Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
            Gp_DispatchMsgPtr(work->field_4, 0x7D4, &D_dryfield_motel_room_1_8017E100[0], 0);
            Gp_DispatchMsgPtr(work->field_8, 0x7D4, &D_dryfield_motel_room_1_8017E100[1], 0);
            break;
        case 3:
        case 5:
            SndEvt_EnqueueType6(0x400C0005, 0, 0);
            break;
        case 4:
            SndEvt_EnqueueType6(0x400C0002, 0, 0);
            break;
        case 6:
            switch (work->field_2E) {
                case 0:
                    cfg            = &Player_Status;
                    work->field_14 = cfg->coordMtx->t[0];
                    work->field_18 = cfg->coordMtx->t[1];
                    work->field_1C = cfg->coordMtx->t[2];
                    work->field_24 = 0;
                    work->field_26 = 0;
                    work->field_28 = 0;
                    work->field_34 =
                        (((GameActor*)((Task*)work->field_0)->work)->field_52 + 0xC00) % 0x1000;
                    if (work->field_34 > 0x800) {
                        s32 weapon;

                        rec    = &buf.shifted.rec;
                        weapon = cfg->weapon;
                        if (Mc_SaveData[0].characterId == 1) {
                            anim = weapon + 1;
                        } else {
                            anim = weapon + 0x22;
                        }
                        buf.shifted.rec.animBlock.index = anim;
                        rec->field_4                    = 5;
                        rec->field_8                    = 1;
                        rec->field_C                    = 5;
                        buf.shifted.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.shifted.rec, 0);
                        Gp_DispatchMsg(work->field_0, 0x3FD, 0x30, 0);
                        work->field_2E += 1;
                    } else {
                        weaponId = cfg->weapon;
                        if (Mc_SaveData[0].characterId == 1) {
                            anim = weaponId + 1;
                        } else {
                            anim = weaponId + 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 6;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 5;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                        Gp_DispatchMsg(work->field_0, 0x3FD, 0x30, 0);
                        work->field_2E += 2;
                    }
                    return;
                case 1:
                    work->field_34 += 0x96;
                    work->field_26  = work->field_34 + 0x400;
                    if (work->field_34 > 0x1000) {
                        anim = Player_Status.weapon;
                        if (Mc_SaveData[0].characterId == 1) {
                            anim += 1;
                        } else {
                            anim += 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 1;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 3;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                        work->field_30 = 0;
                        work->field_2E = 3;
                        return;
                    }
                    Gp_DispatchMsgPtr(work->field_0, 0x3E9, &work->field_14, 0);
                    return;
                case 2:
                    work->field_34 -= 0x96;
                    work->field_26  = work->field_34 + 0x400;
                    if (work->field_34 < 0) {
                        anim = Player_Status.weapon;
                        if (Mc_SaveData[0].characterId == 1) {
                            anim += 1;
                        } else {
                            anim += 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 1;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 3;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                        work->field_30 = 0;
                        work->field_2E = 3;
                        return;
                    }
                    Gp_DispatchMsgPtr(work->field_0, 0x3E9, &work->field_14, 0);
                    return;
                case 3:
                    work->field_30 += 1;
                    if (work->field_30 >= 4) {
                        anim = Player_Status.weapon;
                        if (Mc_SaveData[0].characterId == 1) {
                            anim += 1;
                        } else {
                            anim += 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 9;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 10;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                        work->field_2C = 0;
                    }
                    return;
                default:
                    return;
            }
            return;
        case 0:
        default:
            break;
    }
    work->field_2C = 0;
}

/// Room entry point: allocate the `Dmr1Work` the room task hangs off
/// `Task::work` (killing the task if the allocation fails), zero it, park the
/// slot-3 task in `field_0` and the room task itself in
/// `D_dryfield_motel_room_1_8018159C`, then resolve the four placed objects
/// `field_4` .. `field_10` from the session id: the base id, then the id with
/// the 0x1000 / 0x2000 / 0x3000 index of `Gp_FindWorkById`'s search key.
static void func_dryfield_motel_room_1_8017DC2C(Task* arg0)
{
    Dmr1Work* work;
    s32       id;

    work       = (Dmr1Work*)Mem_Malloc(0x38, 0);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(work, 0, 0x38);
    work->field_0                    = gameGetPtrSlot(3);
    D_dryfield_motel_room_1_8018159C = arg0;
    id                               = gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8);
    work->field_4                    = (Task*)Gp_FindWorkById(id)->field_0;
    id                               = ((gGameSession->at4.loc.stage << 8) | 0x1000) | gGameSession->at4.loc.area;
    work->field_8                    = (Task*)Gp_FindWorkById(id)->field_0;
    id                               = ((gGameSession->at4.loc.stage << 8) | 0x2000) | gGameSession->at4.loc.area;
    work->field_C                    = (Task*)Gp_FindWorkById(id)->field_0;
    id                               = ((gGameSession->at4.loc.stage << 8) | 0x3000) | gGameSession->at4.loc.area;
    work->field_10                   = (Task*)Gp_FindWorkById(id)->field_0;
}
void func_dryfield_motel_room_1_8017DD3C(Task* arg0)
{
    Dmr1MsgBuf buf;
    s32        weaponId;
    s32        anim;

    switch (arg0->state) {
        case 0:
            if ((Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == 0)) {
                func_dryfield_motel_room_1_8017DC2C(arg0);
                weaponId                = Player_Status.weapon;
                anim                    = (Mc_SaveData[0].characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                buf.rec.animBlock.index = anim;
                buf.rec.field_4         = 1;
                buf.rec.field_8         = 1;
                buf.rec.field_C         = 5;
                buf.rec.field_10        = 0;
                Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &buf.rec, 0);
                func_800E8634(&D_dryfield_motel_room_1_8017E160, 0,
                              &D_dryfield_motel_room_1_8017E340);
                arg0->state = arg0->state + 1;
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
                buf.msg.from.loc.area  = gGameSession->at4.loc.area;
                buf.msg.command        = 4;
                Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf.msg, 0x7DB);
                arg0->state = arg0->state + 1;
                break;
            }
            break;
        case 2:
            if (gGameSession->at4.loc.view == arg0->state) {
                buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
                buf.msg.from.loc.area  = gGameSession->at4.loc.area;
                buf.msg.command        = 3;
                Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf.msg, 0x7DB);
                taskKill(arg0);
                return;
            }
            break;
    }
    func_dryfield_motel_room_1_8017D7AC(arg0);
}

void func_dryfield_motel_room_1_8017DF08(void)
{
    Dmr1Work* work = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->work;
    GpCmdArg  msg;

    Gp_ArmStateF0(1);
    msg.from.loc.stage = gGameSession->at4.loc.stage;
    msg.from.loc.area  = gGameSession->at4.loc.area;
    msg.command        = 3;
    Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &msg, 0x7DB);
    Gp_DispatchMsgPtr(work->field_C, 0x7D4, &D_dryfield_motel_room_1_8017E130[0], 0);
    Gp_DispatchMsgPtr(work->field_10, 0x7D4, &D_dryfield_motel_room_1_8017E130[1], 0);
}

void func_dryfield_motel_room_1_8017DFB0(s16 arg0)
{
    Dmr1Work* work = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->work;

    work->field_2C = arg0;
    work->field_2E = 0;
}

void func_dryfield_motel_room_1_8017DFD0(void)
{
    Dmr1Work*     work;
    GpAnimArg     msg;
    PlayerStatus* cfg;
    s32           weaponId;
    s32           anim;

    work                = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->work;
    weaponId            = Player_Status.weapon;
    anim                = (Mc_SaveData[0].characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.animBlock.index = anim;
    msg.field_4         = 9;
    msg.field_8         = 0;
    msg.field_C         = 0;
    msg.field_10        = 0;
    Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &msg, 0);
    cfg            = &Player_Status;
    work->field_14 = cfg->coordMtx->t[0];
    work->field_18 = cfg->coordMtx->t[1];
    work->field_1C = cfg->coordMtx->t[2];
    work->field_24 = 0;
    work->field_26 = 0x500;
    work->field_28 = 0;
    Gp_DispatchMsgPtr(work->field_0, 0x3E9, &work->field_14, 0);
}
static void func_dryfield_motel_room_1_8017E0A0(void)
{
}
