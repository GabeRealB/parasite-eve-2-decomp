#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_breezeway.h"
#include "rooms/rooms_shared_8017ecb4.h"

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

/// Main-executable symbols with no module header yet: `D_80070F70` is the
/// frame counter the prop's swing angle is derived from, and `func_8004BFF8`
/// is the Y rotation builder `ActorsShared80139948` also reaches.
///
/// Its `angle` parameter is declared `s32` rather than the `s16` the actor
/// headers use because the call below feeds it `rsin`'s `int` result, which
/// the target passes through untruncated.
extern s32 D_80070F70;
void       func_8004BFF8(s32 angle, MATRIX* matrix);

/// Breathes the room's hanging prop: rebuilds the display object's coordinate
/// matrix as a pure Y rotation of `rsin(D_80070F70 * 16)` -- one full turn
/// every 256 frames -- off an identity built the same word-at-a-time way
/// `func_dryfield_breezeway_8017E464` builds the event work's two matrices, then
/// re-seeds the hotspot scan `func_dryfield_breezeway_8017EB8C` at the
/// prompt's own screen position and hit-tests it against the room's table.
///
/// Highlighting the cursor (`mode` 1) is the state the scan runs in; landing on
/// an entry confirms it (`mode` 2) and walks `D_dryfield_breezeway_80182DDC`
/// for the entry that was hit, which is the prop the player is looking at --
/// pressing confirm against it runs cap slot 3 and ends the script in state 5.
/// A cancel press (`buttons[1].state` 2) ends it in state 5 as well.
void func_dryfield_breezeway_8017E81C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    GsCOORDINATE2*    coord  = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    DbwEventWork*     work   = (DbwEventWork*)task->idMap;
    RoomHotspot*      hs     = D_dryfield_breezeway_80182DDC;
    MATRIX*           m;

    prompt->mode     = 1;
    prompt->targetId = 0x80;

    m                            = &coord->coord;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    *(s32*)&m->m[1][1]           = 0x1000;
    *(s16*)&m->m[2][2]           = 0x1000;
    *(s32*)&m->m[0][2]           = 0;
    *(s32*)&m->m[2][0]           = 0;

    func_8004BFF8(rsin(D_80070F70 * 0x10), m);
    func_dryfield_breezeway_8017EB8C(task, prompt->screen.xy.x, prompt->screen.xy.y);

    if (RoomsShared8017ecb4(hs, work->cursorX, work->cursorY) != 0) {
        prompt->mode = 2;
        while (hs->id != -1) {
            if (hs->hit != 0) {
                Gp_RunCapCmd1(3);
                task->state = 5;
                return;
            }
            hs++;
        }
    }

    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}
