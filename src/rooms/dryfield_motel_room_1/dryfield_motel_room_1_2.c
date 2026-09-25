#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "rooms/dryfield_motel_room_1.h"

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `D_80071075` and `D_80114C12` (the cutscene mode flag) gate the room task's
/// setup, and `D_8007218A` picks which of the two weapon-id bases that record
/// uses.
extern s8 D_8007218A;
extern s8 D_80114C12;

/// The cutscene script's two blocks, handed to `func_800E8634` by the room
/// task's state 0.
extern s32 D_dryfield_motel_room_1_8017E160;
extern s32 D_dryfield_motel_room_1_8017E340;

void func_dryfield_motel_room_1_8017D7AC(Task* arg0)
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
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&buf.msg, 0x7DB);
            Gp_DispatchMsg(work->field_4, 0x7D4, (s32)&D_dryfield_motel_room_1_8017E0D0[0], 0);
            Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_motel_room_1_8017E0D0[1], 0);
            break;
        case 2:
            buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
            buf.msg.from.loc.area  = gGameSession->at4.loc.area;
            buf.msg.command        = 2;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&buf.msg, 0x7DB);
            Gp_DispatchMsg(work->field_0, 0x3F3, 1, 0);
            Gp_DispatchMsg(work->field_4, 0x7D4, (s32)&D_dryfield_motel_room_1_8017E100[0], 0);
            Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_motel_room_1_8017E100[1], 0);
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
                        if (D_8007218A == 1) {
                            anim = weapon + 1;
                        } else {
                            anim = weapon + 0x22;
                        }
                        buf.shifted.rec.animBlock.index = anim;
                        rec->field_4                    = 5;
                        rec->field_8                    = 1;
                        rec->field_C                    = 5;
                        buf.shifted.rec.field_10        = 0;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&buf.shifted.rec, 0);
                        Gp_DispatchMsg(work->field_0, 0x3FD, 0x30, 0);
                        work->field_2E += 1;
                    } else {
                        weaponId = cfg->weapon;
                        if (D_8007218A == 1) {
                            anim = weaponId + 1;
                        } else {
                            anim = weaponId + 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 6;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 5;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&buf.rec, 0);
                        Gp_DispatchMsg(work->field_0, 0x3FD, 0x30, 0);
                        work->field_2E += 2;
                    }
                    return;
                case 1:
                    work->field_34 += 0x96;
                    work->field_26  = work->field_34 + 0x400;
                    if (work->field_34 > 0x1000) {
                        anim = Player_Status.weapon;
                        if (D_8007218A == 1) {
                            anim += 1;
                        } else {
                            anim += 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 1;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 3;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&buf.rec, 0);
                        work->field_30 = 0;
                        work->field_2E = 3;
                        return;
                    }
                    Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&work->field_14, 0);
                    return;
                case 2:
                    work->field_34 -= 0x96;
                    work->field_26  = work->field_34 + 0x400;
                    if (work->field_34 < 0) {
                        anim = Player_Status.weapon;
                        if (D_8007218A == 1) {
                            anim += 1;
                        } else {
                            anim += 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 1;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 3;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&buf.rec, 0);
                        work->field_30 = 0;
                        work->field_2E = 3;
                        return;
                    }
                    Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&work->field_14, 0);
                    return;
                case 3:
                    work->field_30 += 1;
                    if (work->field_30 >= 4) {
                        anim = Player_Status.weapon;
                        if (D_8007218A == 1) {
                            anim += 1;
                        } else {
                            anim += 0x22;
                        }
                        buf.rec.animBlock.index = anim;
                        buf.rec.field_4         = 9;
                        buf.rec.field_8         = 1;
                        buf.rec.field_C         = 10;
                        buf.rec.field_10        = 0;
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&buf.rec, 0);
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

void func_dryfield_motel_room_1_8017DC2C(Task* arg0)
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
            if ((D_80114C12 != 1) && (gDisplayState.pendingMode == 0)) {
                func_dryfield_motel_room_1_8017DC2C(arg0);
                weaponId                = Player_Status.weapon;
                anim                    = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                buf.rec.animBlock.index = anim;
                buf.rec.field_4         = 1;
                buf.rec.field_8         = 1;
                buf.rec.field_C         = 5;
                buf.rec.field_10        = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&buf.rec, 0);
                func_800E8634((s32)&D_dryfield_motel_room_1_8017E160, 0,
                              (s32)&D_dryfield_motel_room_1_8017E340);
                arg0->state = arg0->state + 1;
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
                buf.msg.from.loc.area  = gGameSession->at4.loc.area;
                buf.msg.command        = 4;
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&buf.msg, 0x7DB);
                arg0->state = arg0->state + 1;
                break;
            }
            break;
        case 2:
            if (gGameSession->at4.loc.view == arg0->state) {
                buf.msg.from.loc.stage = gGameSession->at4.loc.stage;
                buf.msg.from.loc.area  = gGameSession->at4.loc.area;
                buf.msg.command        = 3;
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&buf.msg, 0x7DB);
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
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    Gp_DispatchMsg(work->field_C, 0x7D4, (s32)&D_dryfield_motel_room_1_8017E130[0], 0);
    Gp_DispatchMsg(work->field_10, 0x7D4, (s32)&D_dryfield_motel_room_1_8017E130[1], 0);
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
    anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.animBlock.index = anim;
    msg.field_4         = 9;
    msg.field_8         = 0;
    msg.field_C         = 0;
    msg.field_10        = 0;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
    cfg            = &Player_Status;
    work->field_14 = cfg->coordMtx->t[0];
    work->field_18 = cfg->coordMtx->t[1];
    work->field_1C = cfg->coordMtx->t[2];
    work->field_24 = 0;
    work->field_26 = 0x500;
    work->field_28 = 0;
    Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&work->field_14, 0);
}
void func_dryfield_motel_room_1_8017E0A0(void)
{
}
