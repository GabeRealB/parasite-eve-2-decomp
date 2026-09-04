#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/acropolis_sanctuary.h"
#include "rooms/room_common.h"

extern u8 D_80073BA9;

extern SVECTOR       D_acropolis_sanctuary_8017D5D0;
extern GpMsgEntry    D_acropolis_sanctuary_8018081C[];
extern RoomPlacement D_acropolis_sanctuary_801808BC;
extern GpRec14       D_acropolis_sanctuary_801809F8;
extern GpRec14       D_acropolis_sanctuary_80180A0C;
extern s32           D_acropolis_sanctuary_80180AE8;
extern u8            D_acropolis_sanctuary_80181814[];
extern TaskDesc      D_acropolis_sanctuary_80182240;
extern Task*         D_acropolis_sanctuary_80186C90;

extern void func_acropolis_sanctuary_8017DD78(void);
extern void func_acropolis_sanctuary_8017DF88(s32 arg0, s32 arg1);

s32 func_acropolis_sanctuary_8017D810(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0 && GameFlag_GetNibble(6) == 0) {
        func_800E8614((s32)&D_acropolis_sanctuary_80181814, 0);
    }
    return 0;
}

/// Message gate for the sanctuary hotspot registered under id 0x13EF: sub-id 1
/// arms the room's own task the first time it is seen, latching nibble 7 so a
/// second visit does nothing. The record is not copied to the outgoing one -
/// this handler only ever consumes the message (returns 0).
s32 func_acropolis_sanctuary_8017D848(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_2 == 1 && GameFlag_GetNibble(7) == 0) {
        GameFlag_SetNibble(7, 1);
        Task_SpawnFromTable(&D_acropolis_sanctuary_80182240, 0, 0, 0);
    }
    return 0;
}

void func_acropolis_sanctuary_8017D8A0(u32 arg0)
{
    func_acropolis_sanctuary_8017DF88((arg0 >> 8) & 0xFF, arg0 & 0xFF);
}

/// Republishes the player's weapon to slot 3: picks the room's 0x3E8 record by
/// the equipped-weapon index in `D_80073BA9`, has `Gp_PlayerWeaponId` stamp the
/// current weapon model id into its `field_0`, then sends it.
void func_acropolis_sanctuary_8017D8CC(void)
{
    if (D_80073BA9 == 2) {
        Gp_PlayerWeaponId(&D_acropolis_sanctuary_801809F8.field_0);
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&D_acropolis_sanctuary_801809F8, 0);
    } else {
        Gp_PlayerWeaponId(&D_acropolis_sanctuary_80180A0C.field_0);
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&D_acropolis_sanctuary_80180A0C, 0);
    }
}

/// State 0 of the sanctuary room task: publishes the room's message-handler
/// table under pointer slot 7 and advances to the next state. Unless nibble 6
/// has already reached 1 it also chains slot-4 message list 1 onto itself and,
/// when nibble 2 is set and that slot holds a task, places the actor by sending
/// it the 0x7D3 animation record followed by the 0x7D4 placement.
void func_acropolis_sanctuary_8017D930(Task* arg0)
{
    Task* slot;

    arg0->field_24 = D_acropolis_sanctuary_8018081C;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = arg0->state + 1;
    if (GameFlag_GetNibble(6) != 1) {
        slot = (Task*)Gp_LookupSlot4(1);
        Gp_MsgSlot4Chain(1, 1);
        if (GameFlag_GetNibble(2) != 0 && slot != NULL) {
            Gp_DispatchMsg(slot, 0x7D3, (s32)&D_acropolis_sanctuary_80180AE8, 0);
            Gp_DispatchMsg(slot, 0x7D4, (s32)&D_acropolis_sanctuary_801808BC, 0);
        }
    }
    func_acropolis_sanctuary_8017DD78();
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017D9E8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DA40);

/// Request entry point for the sanctuary cutscene task's work block: 0 and 1
/// arm the script at phase 1 or 2 respectively, rewinding `step` so the driver
/// runs the scene once, while 2 just plays the pair of sound events the scene
/// is cued with.
void func_acropolis_sanctuary_8017DCE0(s32 arg0)
{
    AcsCutsceneWork* work = (AcsCutsceneWork*)D_acropolis_sanctuary_80186C90->idMap;

    switch (arg0) {
        case 0:
            work->phase = 1;
            work->step  = 0;
            return;
        case 1:
            work->phase = 2;
            work->step  = 0;
            return;
        case 2:
            SndEvt_EnqueueType7(0x510C0007, 0);
            SndEvt_EnqueueType6(0x510C0008, 0, 0);
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DD78);

/// Toggles a pair of sprite commands for view `arg1` of the current room:
/// `arg0` zero draws the second command and skips the third, non-zero does the
/// reverse. `Gp_LinkViewSprts` reads `field_4` to decide whether to skip
/// OT-linking each command's prims.
void func_acropolis_sanctuary_8017DF88(s32 arg0, s32 arg1)
{
    GameSession*      g    = Game_Session;
    GameSessionFrom4* sess = (GameSessionFrom4*)&g->field_4;
    GpSprtCmd*        cmd;

    cmd = Gp_SprtTables[sess->field_3 - 1][g->field_74 - 1].field_0[sess->field_2 - 1][(arg1 & 0xFF) - 1].field_4;
    if ((arg0 & 0xFF) == 0) {
        cmd[1].field_4 = 0;
        cmd[2].field_4 = 1;
    } else {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 0;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E00C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E134);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E338);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017EC90);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017F4E8);

/// Spawns effect 0x60078 on the room task's model coordinate, seeded with the
/// fixed offset vector held in this unit's rodata. Always consumes the event
/// (returns 0).
s32 func_acropolis_sanctuary_8017F918(Task* task)
{
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->field_8;
    SVECTOR        vec   = D_acropolis_sanctuary_8017D5D0;

    Gp_SpawnEff(0x60078, coord, 0, &vec);
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017F974);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017FB18);

/// Per-frame visibility gate for the sanctuary's item object: hides the model
/// (`field_C` bit 0x80) while the camera sits on view 0xB or 0xD, or once the
/// item's 2-bit pickup flag has reached 2; otherwise shows it again with the
/// default flags.
void func_acropolis_sanctuary_80180264(Task* task)
{
    GpItemObj8* obj = task->spawnArg2;
    TmdObject*  tmd = task->extra;
    s32         flag;
    s32         view;

    flag = Gp_GetCurBit2Flag(obj->field_8);
    view = Gp_GetViewIndex();
    if (view == 0xB || view == 0xD || flag == 2) {
        tmd->field_C = 0x80;
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_801802E0);
