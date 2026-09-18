#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Cutscene script blob argument of `func_800E8634`.
extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);

/// Byte at 0x8007272D, written when the garage scene ends.
extern s8 D_8007272D;

/// One entry of the room's 0x98-byte display-object table. Only the flag byte
/// at 0x4A is touched here: bit 6 shows the entry, clearing it hides it.
typedef struct {
    /* 0x00 */ u8 pad_0[0x4A];
    /* 0x4A */ u8 field_4A;
    /* 0x4B */ u8 pad_4B[0x4D];
} DryfieldNightGarageObj;

STATIC_ASSERT_SIZEOF(DryfieldNightGarageObj, 0x98);

extern TaskDesc               D_8013B11C[];
extern s32                    D_dryfield_night_garage_80182DE0;
extern s32                    D_dryfield_night_garage_80182DE4;
extern TaskDesc               D_dryfield_night_garage_80182C98[];
extern s32                    D_dryfield_night_garage_80182DF8;
extern s32                    D_dryfield_night_garage_801831B8;
extern DryfieldNightGarageObj D_dryfield_night_garage_80186E60[];

/// The room's own `GpMsgEntry[]` - the message table `func_dryfield_night_garage_8017FF2C`
/// publishes in `Task::field_24`. It terminates with id 0x7FFFFFFF.
extern GpMsgEntry D_dryfield_night_garage_80181C38[];

/// Ally animation descriptor handed to `Gp_AllyAnimId`, then forwarded as the
/// payload of the 0x3E8 message.
extern s32 D_dryfield_night_garage_80181C68;

/// Script blob passed to `func_800E8614` when game flag 0x8E is already set.
extern s32 D_dryfield_night_garage_80181C7C;

/// Cutscene script blobs owned by the main executable.
extern s32 D_8013B570;
extern s32 D_8013B590;
extern s32 D_8013C388;

s32 func_800D4D2C(s32 arg0);

s32 func_dryfield_night_garage_80180A64(s32 arg0);

s32 func_dryfield_night_garage_80180604(s32 arg0);

/// State 0 of this room's message task, run when the garage scene starts.
/// Publishes the room's message table in `Task::field_24` and the task itself
/// in pointer slot 7, clears the display bit on the first object entry, then
/// hands off to the player actor through messages 0x3E9 / 0x3E8.
void func_dryfield_night_garage_8017FF2C(Task* task)
{
    DryfieldNightGarageObj* base;
    DryfieldNightGarageObj* obj;
    Task*                   player;

    task->field_24 = D_dryfield_night_garage_80181C38;
    Game_SetPtrSlot(task, 7);
    D_dryfield_night_garage_80186E60->field_4A &= 0xBF;
    player                                      = Game_GetPtrSlot(0xA);
    if (gGameSession->at4.loc.place == 3 && player != NULL) {
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_8013B570, 0);
        Gp_AllyAnimId(&D_dryfield_night_garage_80181C68);
        Gp_DispatchMsg(player, 0x3E8, (s32)&D_dryfield_night_garage_80181C68, 0);
        func_dryfield_night_garage_80180604(0);
        Gp_EndPlayerActorTask((GpActorWork*)player);
        if (GameFlag_GetNibble(0x8E) == 0) {
            Gp_FillAllyHp();
            GameFlag_SetNibble(0x8E, 1);
            func_800E8634((s32)&D_8013B590, 0, (s32)&D_8013C388);
        } else {
            func_800E8614((s32)&D_dryfield_night_garage_80181C7C, 1);
        }
    }
    if (gGameSession->at4.loc.place == 2 && GameFlag_GetNibble(0x6C) > 0) {
        if (GameFlag_GetNibble(0x6C) == 1) {
            GameFlag_SetNibble(0x6C, 2);
        }
        base            = D_dryfield_night_garage_80186E60;
        obj             = base + 1;
        base->field_4A |= 0x40;
        obj->field_4A  &= 0xBF;
    }
    task->state = (s32)(task->state + 1);
}

s32 func_dryfield_night_garage_801800C8(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    DryfieldNightGarageObj* base;
    DryfieldNightGarageObj* obj;

    if (msg->field_2 == 6) {
        if (gGameSession->at4.loc.place == 2) {
            if (GameFlag_GetNibble(0x6C) == 0) {
                if (Gp_HasCollectedBit(0x113) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 6, 0);
                } else if (Gp_HasCollectedBit(0x117) == 0 && Gp_HasCollectedBit(0x118) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 7, 0);
                } else if (Gp_HasCollectedBit(0x118) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 8, 0);
                } else if (GameFlag_GetNibble(0x6C) == 0) {
                    base            = D_dryfield_night_garage_80186E60;
                    obj             = base + 1;
                    base->field_4A |= 0x40;
                    obj->field_4A  &= 0xBF;
                    func_800E8634((s32)&D_dryfield_night_garage_80182DF8, 0,
                                  (s32)&D_dryfield_night_garage_801831B8);
                    GameFlag_SetNibble(0x6C, 1);
                    func_800E3FAC(0xA2, 0x17);
                    Gp_ClearCollectedBit(0x118);
                    D_8007272D = 5;
                }
            } else {
                Gp_MsgPlayerWeapon(0);
                if (GameFlag_GetNibble(0x6C) == 1) {
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0xA, 0);
                } else {
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0x15, 0);
                }
            }
        }
    }
    if (msg->field_2 == 1) {
        if (GameFlag_GetNibble(0x97) != 0) {
            Gp_StartCapSlot(0x14, 1, 0);
        } else {
            Gp_SpawnIfCapIdle(0x36, 0);
        }
    }
    if (msg->field_2 == 2 && gGameSession->at4.loc.place == 3 && Game_GetPtrSlot(0xA) != NULL) {
        Task_SpawnFromTable(D_8013B11C, 1, 0, 0);
    }
    return 0;
}
