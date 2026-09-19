#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/wipsys.h"

#include "rooms/dryfield_motel_room_1.h"

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `D_80071075` and `D_80114C12` (the cutscene mode flag) gate the room task's
/// setup, and `D_8007218A` picks which of the two weapon-id bases that record
/// uses.
extern u8 D_80073BA9;
extern u8 D_80071075;
extern s8 D_8007218A;
extern s8 D_80114C12;

/// The cutscene script's two blocks, handed to `func_800E8634` by the room
/// task's state 0.
extern s32 D_dryfield_motel_room_1_8017E160;
extern s32 D_dryfield_motel_room_1_8017E340;

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1_2", func_dryfield_motel_room_1_8017D7AC);

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
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                func_dryfield_motel_room_1_8017DC2C(arg0);
                weaponId         = D_80073BA9;
                anim             = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                buf.rec.field_0  = anim;
                buf.rec.field_4  = 1;
                buf.rec.field_8  = 1;
                buf.rec.field_C  = 5;
                buf.rec.field_10 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&buf.rec, 0);
                func_800E8634((s32)&D_dryfield_motel_room_1_8017E160, 0,
                              (s32)&D_dryfield_motel_room_1_8017E340);
                arg0->state = arg0->state + 1;
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                buf.msg.field_0 = gGameSession->at4.loc.stage;
                buf.msg.field_1 = gGameSession->at4.loc.area;
                buf.msg.field_2 = 4;
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&buf.msg, 0x7DB);
                arg0->state = arg0->state + 1;
                break;
            }
            break;
        case 2:
            if (gGameSession->at4.loc.view == arg0->state) {
                buf.msg.field_0 = gGameSession->at4.loc.stage;
                buf.msg.field_1 = gGameSession->at4.loc.area;
                buf.msg.field_2 = 3;
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
    Dmr1Work*  work = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->work;
    Dmr1Msg7DA msg;

    Gp_ArmStateF0(1);
    msg.field_0 = gGameSession->at4.loc.stage;
    msg.field_1 = gGameSession->at4.loc.area;
    msg.field_2 = 3;
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

    work         = (Dmr1Work*)D_dryfield_motel_room_1_8018159C->work;
    weaponId     = D_80073BA9;
    anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.field_0  = (void*)anim;
    msg.field_4  = 9;
    msg.field_8  = 0;
    msg.field_C  = 0;
    msg.field_10 = 0;
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
