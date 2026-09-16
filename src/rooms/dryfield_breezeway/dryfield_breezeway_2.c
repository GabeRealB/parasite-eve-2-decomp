#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_breezeway.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017DEC0);

void func_dryfield_breezeway_8017E010(Task* arg0)
{
    DbwWork* work;
    s32      id;

    switch (arg0->state) {
        case 0:
            work        = (DbwWork*)Mem_Malloc(0x14, 0);
            arg0->idMap = (TaskIdMap*)work;
            if (work == NULL) {
                Task_Kill(arg0);
            } else {
                Mem_Set(work, 0, 0x14);
                work->field_0                 = (void*)Game_GetPtrSlot(3);
                D_dryfield_breezeway_801843C0 = arg0;
                id                            = Game_Session->field_6 | (Game_Session->field_7 << 8);
                work->field_4                 = (void*)Gp_FindWorkById(id)->field_0;
                id                            = ((Game_Session->field_7 << 8) | 0x1000) | Game_Session->field_6;
                work->field_8                 = (void*)Gp_FindWorkById(id)->field_0;
            }
            arg0->state += 1;
            return;
        case 1:
            Task_Kill(arg0);
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E114);

void func_dryfield_breezeway_8017E2D4(void)
{
    DbwWork*  work;
    DbwMsg7DA msg;

    work        = (DbwWork*)D_dryfield_breezeway_801843C0->idMap;
    msg.field_0 = Game_Session->field_7;
    msg.field_1 = Game_Session->field_6;
    msg.field_2 = 2;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_breezeway_80181E28, 0);
}

void func_dryfield_breezeway_8017E350(void)
{
    Gp_ArmStateF0(1);
}

void func_dryfield_breezeway_8017E370(s16 arg0)
{
    DbwWork* work;

    work          = (DbwWork*)D_dryfield_breezeway_801843C0->idMap;
    work->field_C = arg0;
    work->field_E = 0;
}

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern s8 D_8007218A;
extern u8 D_80073BA9;

void func_dryfield_breezeway_8017E390(void)
{
    DbwMsgBuf buf;
    DbwWork*  work;
    s32       id;

    id               = D_80073BA9;
    buf.rec.field_0  = (D_8007218A == 1) ? id + 1 : id + 0x22;
    buf.rec.field_4  = 9;
    buf.rec.field_8  = 0;
    buf.rec.field_C  = 0;
    buf.rec.field_10 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&buf, 0);

    work            = (DbwWork*)D_dryfield_breezeway_801843C0->idMap;
    buf.msg.field_0 = Game_Session->field_7;
    buf.msg.field_1 = Game_Session->field_6;
    buf.msg.field_2 = 2;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&buf, 0x7DB);
    Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_breezeway_80181E28, 0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E464);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E65C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E81C);
