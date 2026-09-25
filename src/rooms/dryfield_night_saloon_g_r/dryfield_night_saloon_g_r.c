#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8018055c.h"

extern void func_8002E53C(TextDrawReq* req, u8* text);
extern void func_800E8614(s32 arg0, s32 arg1);

extern u8  D_80062737;
extern s16 D_80071076;
extern u8  D_8007216D;
extern s16 D_8007A396;
extern s16 D_80114D08;

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg D_dryfield_night_saloon_g_r_80188FAC;
extern RoomEventReq D_dryfield_night_saloon_g_r_80188FB8;
extern u8           D_dryfield_night_saloon_g_r_80188FB4;

/// Descriptor of the event task `func_dryfield_night_saloon_g_r_8017DA04`.
extern TaskDesc D_dryfield_night_saloon_g_r_8017F90C;

/// Saved `Mc_SaveData.at4.loc.view` (area id), restored when the cutscene ends.
extern u8 D_dryfield_night_saloon_g_r_80188FA4;

extern GpMsgEntry D_dryfield_night_saloon_g_r_8017F918[];
extern TaskDesc   D_dryfield_night_saloon_g_r_8017F940[];

/// Cutscene script blobs handed to `func_800E8614` / `func_800E8634`.
extern s32 D_dryfield_night_saloon_g_r_801848DC;
extern s32 D_dryfield_night_saloon_g_r_80184B34;
extern s32 D_dryfield_night_saloon_g_r_80184D2C;
extern s32 D_dryfield_night_saloon_g_r_80183C94;
extern s32 D_dryfield_night_saloon_g_r_801847A4;

extern GpObj4A D_dryfield_night_saloon_g_r_80188BB8;

/// The jukebox's track lists, one per game mode, each a run of track id and
/// name pairs.
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F0C[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F24[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F3C[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F54[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F6C[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184F84[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184FA4[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184FC4[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80184FE4[];
extern RoomsShared8018055cCourse D_dryfield_night_saloon_g_r_80185004[];

/// The jukebox menu's title, "SELECT". A stray 0x0D byte follows its
/// terminator, so the block stays in assembly.
extern char D_dryfield_night_saloon_g_r_8017D898[];

/// The jukebox's track list.
extern UiList D_dryfield_night_saloon_g_r_80185028;

/// Descriptor of the jukebox menu panel, whose task is
/// `func_dryfield_night_saloon_g_r_8017E28C`.
extern UiObjectDesc D_dryfield_night_saloon_g_r_8018504C;

/// Descriptor of the jukebox task `func_dryfield_night_saloon_g_r_8017E564`.
extern TaskDesc D_dryfield_night_saloon_g_r_80185068;

/// The room's effect positions in the model's local space. The frame hook
/// draws a quad at each of 0-10 and 20-27; 12 and 13 are the two ends
/// `func_dryfield_night_saloon_g_r_8017F0A4` is handed; 14-19 are the two
/// light shafts of `func_dryfield_night_saloon_g_r_8017EB38`.
extern SVECTOR D_dryfield_night_saloon_g_r_80185074[];

/// Entry 13 of `D_dryfield_night_saloon_g_r_80185074`, reached under a label
/// of its own.
extern SVECTOR D_dryfield_night_saloon_g_r_801850DC[];

/// Entries 14 and 17 of `D_dryfield_night_saloon_g_r_80185074`, the two
/// shaft roots, which the code also reaches under labels of their own.
extern SVECTOR D_dryfield_night_saloon_g_r_801850E4;
extern SVECTOR D_dryfield_night_saloon_g_r_801850FC;

/// One view bitmask per effect, tested against `1 << view`. Entries 0-10 gate
/// positions 0-10, entries 11 and 12 the two helper effects, and entries 13-20
/// gate positions 20-27.
extern s16 D_dryfield_night_saloon_g_r_80185154[];

void func_dryfield_night_saloon_g_r_8017DF90(Task* task);
void func_dryfield_night_saloon_g_r_8017E040(Task* task);
s32  func_dryfield_night_saloon_g_r_8017E698(s32 arg0);
void func_dryfield_night_saloon_g_r_8017E8B0(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_saloon_g_r_8017EB38(GsCOORDINATE2* coord);
void func_dryfield_night_saloon_g_r_8017F0A4(GsCOORDINATE2* coord, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

/// Event gate for the room's exit. Returns 1 when game-flag nibble
/// `req->flagId` already reads set (clear, for a negative id). Otherwise, when
/// `req->itemId` has been collected or is 0, it returns 2 and - unless
/// `msg->field_5` asks for a dry run - latches `msg` and `req`, sets the
/// nibble and spawns the event task. When the item is missing it returns 0
/// and, outside a dry run, runs cap command `req->field_4`.
s32 func_dryfield_night_saloon_g_r_8017D8A0(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                 = req->flagId;
    D_dryfield_night_saloon_g_r_80188FB4 = 0;
    neg                                  = flag < 0;
    got                                  = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_night_saloon_g_r_80188FAC = *msg;
                D_dryfield_night_saloon_g_r_80188FB8 = *req;
                id                                   = req->flagId;
                mode                                 = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_saloon_g_r_8017F90C, 0, 0, 0);
                D_dryfield_night_saloon_g_r_80188FB4 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns: runs the latched request's cap command,
/// plays its two sound ids in turn, each waited out, then warps to the area,
/// warp point and room the latched message names.
void func_dryfield_night_saloon_g_r_8017DA04(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_saloon_g_r_80188FB8.field_0);
            if (D_dryfield_night_saloon_g_r_80188FB8.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_saloon_g_r_80188FB8.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_saloon_g_r_80188FB8.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_saloon_g_r_80188FB8.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_saloon_g_r_80188FB8.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_saloon_g_r_80188FB8.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_night_saloon_g_r_80188FAC.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_saloon_g_r_80188FAC.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_night_saloon_g_r_80188FAC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_night_saloon_g_r_8017E050`: the entry tick
/// `func_dryfield_night_saloon_g_r_8017DF90`, the idle state
/// `func_dryfield_night_saloon_g_r_8017E040`, then `taskKill`.
const TaskFuncTable3 D_dryfield_night_saloon_g_r_8017D5DC = {
    { func_dryfield_night_saloon_g_r_8017DF90, func_dryfield_night_saloon_g_r_8017E040, taskKill },
};
/// Room cutscene task: case 0 saves the area id, forces `Mc_SaveData.at4.loc.view`
/// to 0xC, raises the script halt flags and starts cap command 0x13; the
/// following states wait for the cap to go idle, then start the jukebox task,
/// and case 4 restores the area id and kills the task.
void func_dryfield_night_saloon_g_r_8017DB74(Task* task)
{
    McSaveData* save;
    u8          temp;

    switch (task->state) {
        case 0:
            gGameSession->eventState             = 1;
            gGameSession->hideHud                = 1;
            Gp_StateF0.field_4                   = 2;
            save                                 = &Mc_SaveData;
            temp                                 = save->at4.loc.view;
            save->at4.loc.view                   = 0xC;
            D_dryfield_night_saloon_g_r_80188FA4 = temp;
            Gp_MsgPlayer3F3(0);
            Gp_RunCapCmd(0x13, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            func_dryfield_night_saloon_g_r_8017E698(0);
            task->state = task->state + 1;
            return;
        case 3:
            task->state = task->state + 1;
            return;
        case 4:
            gGameSession->eventState = 0;
            gGameSession->hideHud    = 0;
            D_80114D08               = 0xA;
            Gp_StateF0.field_4       = 0;
            Mc_SaveData.at4.loc.view = D_dryfield_night_saloon_g_r_80188FA4;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        default:
            return;
    }
    taskKill(task);
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r", D_dryfield_night_saloon_g_r_8017D600);

/// The jukebox's ten track lists: one per game mode, with list 4 standing in
/// before the first clear, and the second five used outside the debug attach
/// room.
const RoomsShared8018055cMenu D_dryfield_night_saloon_g_r_8017D870 = {
    {
        D_dryfield_night_saloon_g_r_80184F0C,
        D_dryfield_night_saloon_g_r_80184F24,
        D_dryfield_night_saloon_g_r_80184F3C,
        D_dryfield_night_saloon_g_r_80184F54,
        D_dryfield_night_saloon_g_r_80184F6C,
        D_dryfield_night_saloon_g_r_80184F84,
        D_dryfield_night_saloon_g_r_80184FA4,
        D_dryfield_night_saloon_g_r_80184FC4,
        D_dryfield_night_saloon_g_r_80184FE4,
        D_dryfield_night_saloon_g_r_80185004,
    },
};

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r", D_dryfield_night_saloon_g_r_8017D898);
/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`, and for area 0xF picks the destination
/// room from game-flag nibble 0x61 (unless `in->field_5` asks for a dry run),
/// then passes the warp through the event gate with the room's own request -
/// nibble 0x35, no item, cap command 2 and two stage sound ids. Any other area
/// answers 1.
s32 func_dryfield_night_saloon_g_r_8017DCA4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;

    *out  = *in;
    msgId = in->msgId;
    if (msgId == 0xF) {
        if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
        if (in->msgId == msgId) {
            req.field_0 = 2;
            req.field_4 = 2;
            req.field_8 = Gp_PackStageSndId(0x52120005);
            req.field_C = Gp_PackStageSndId(0x52120003);
            req.flagId  = 0x35;
            req.itemId  = 0;
            return func_dryfield_night_saloon_g_r_8017D8A0(&req, in);
        }
    }
    return 1;
}

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_night_saloon_g_r_8017DD7C(void)
{
    return 0;
}

s32 func_dryfield_night_saloon_g_r_8017DD84(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 4:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_dryfield_night_saloon_g_r_8017F940, 0, 0, 0);
            break;
        case 8:
            if (GameFlag_GetNibble(0x5A) == 0) {
                func_800E8614((s32)&D_dryfield_night_saloon_g_r_801848DC, 0);
                GameFlag_SetNibble(0x5A, 1);
            } else if (GameFlag_GetNibble(0x5A) == 1) {
                func_800E8614((s32)&D_dryfield_night_saloon_g_r_80184B34, 0);
            }
            break;
        case 10:
            if (GameFlag_GetNibble(0x5A) < 2) {
                func_800E8614((s32)&D_dryfield_night_saloon_g_r_80184D2C, 0);
            }
            break;
    }
    return 0;
}

/// Handler for this room's script entry 0x13EF, whose `GpMsg13EF` payload
/// arrives as `arg2`. `field_2 == 7` plays the room's first-visit cutscene
/// once (nibble 0x59). Then, in session phase 2 with nibble 0xB0 still clear,
/// `field_2 == 1` unlinks the room's 4A object and queues sound 0x5312000C,
/// while the room's own phase (`field_2 == 2`) announces the visit to the
/// slot-4 task with message 0x7DA carrying the session's two id bytes and a
/// non-zero action halfword, and sets nibble 0xB0. Always returns 0.
s32 func_dryfield_night_saloon_g_r_8017DE68(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    GpCmdArg msg;
    u8       temp_s0;

    if (arg2->field_2 == 7 && GameFlag_GetNibble(0x59) == 0) {
        func_800E8634((s32)&D_dryfield_night_saloon_g_r_80183C94, 0, (s32)&D_dryfield_night_saloon_g_r_801847A4);
        GameFlag_SetNibble(0x59, 1);
    }
    temp_s0 = gGameSession->at4.loc.place;
    if (temp_s0 == 2 && GameFlag_GetNibble(0xB0) == 0) {
        if (arg2->field_2 == 1) {
            Gp_UnlinkObj4A(0, &D_dryfield_night_saloon_g_r_80188BB8);
            SndEvt_EnqueueType6(0x5312000C, 0, 0);
        } else if (arg2->field_2 == temp_s0) {
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            msg.from.loc.area  = gGameSession->at4.loc.area;
            msg.command        = 1;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            GameFlag_SetNibble(0xB0, 1);
        }
    }
    return 0;
}

/// Room entry task tick: park the room's hotspot table in `Task::msgTable` -
/// the table whose 0x13EE entry is the room's own script task - register the
/// task in pointer slot 7, then, on the phase-2 visit whose nibble 0xB0 is
/// still clear, announce the room to the slot-4 task with message 0x7DA
/// carrying the session's two id bytes and a zero halfword. Then advance state.
void func_dryfield_night_saloon_g_r_8017DF90(Task* task)
{
    GpCmdArg msg;

    task->msgTable = D_dryfield_night_saloon_g_r_8017F918;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 2 && GameFlag_GetNibble(0xB0) == 0) {
        msg.from.loc.stage = gGameSession->at4.loc.stage;
        msg.from.loc.area  = gGameSession->at4.loc.area;
        msg.command        = 0;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    }
    task->state = task->state + 1;
}

/// The room task's idle state, entry 1 of its three-state table: does nothing.
/// The 0x10-byte local is never used, but the original reserved the frame.
void func_dryfield_night_saloon_g_r_8017E040(Task* task)
{
    char pad[0x10];
}

/// The room task: copies the three-state table
/// `D_dryfield_night_saloon_g_r_8017D5DC` onto the stack and runs the entry
/// for the task's current state - the entry tick, the idle state, then
/// `taskKill`.
void func_dryfield_night_saloon_g_r_8017E050(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_saloon_g_r_8017D5DC;
    sp.funcs[task->state](task);
}

/// Cutscene script callback: stores `arg0` as the session's room and in the
/// main-executable byte `D_8007216D`.
void func_dryfield_night_saloon_g_r_8017E0A8(u8 arg0)
{
    D_8007216D                 = arg0;
    gGameSession->at4.loc.room = arg0;
}

/// Row callback of the jukebox list: draws the row's track name, and on
/// confirm, when the row is not the one already chosen, plays the select
/// sound and, when the track differs from the one playing, fades the music
/// out and hands the track id to the menu task to load.
void func_dryfield_night_saloon_g_r_8017E0C0(DialogPrompt* prompt, UiObject* obj)
{
    RoomsShared8018055cMenu    menu;
    RoomsShared8018055cCourse* course;
    s32                        row;
    s32                        list;
    s32                        mode;

    row  = prompt->field_8;
    menu = D_dryfield_night_saloon_g_r_8017D870;

    list = 4;
    if (Mc_SaveData.clearCount != 0) {
        list = Mc_SaveData.gameMode;
    }
    if (Gp_IsDebugAttachRoom() == 0) {
        list += 5;
    }

    course              = &menu.lists[list][row];
    menu.req.x          = obj->baseX + (u16)prompt->field_18;
    menu.req.y          = (prompt->field_1A - 3) + obj->baseY;
    menu.req.otIndex    = (s16)obj->drawOrder + 1;
    menu.req.field_8    = prompt->field_1C;
    menu.req.glyphTable = 4;
    menu.req.field_E    = 1;
    menu.req.centerMode = 0;
    func_8002E53C(&menu.req, course->name);

    mode = prompt->field_C;
    if (mode == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            if (obj->owner->spawnArg1 != prompt->field_8) {
                SndEvt_EnqueueType6(0x16, 0, 0);
                if (obj->owner->status != course->id) {
                    SndEvt_EnqueueType2(0, 0x3C);
                    obj->owner->state  = mode;
                    obj->owner->status = course->id;
                    CdCmd_DropPending();
                }
                obj->owner->spawnArg1 = prompt->field_8;
            }
        }
    }
}

/// The jukebox menu task. Draws the title and, on its first tick, lays out the
/// track list (four rows, three in the debug attach room). While a chosen
/// track is pending it waits for the MIDI player to go idle, queues the
/// track's CD load, then starts it once the CD is idle and records it as the
/// current track. The menu or cancel button plays the back sound and closes
/// the panel.
void func_dryfield_night_saloon_g_r_8017E28C(Task* task)
{
    u8        param1[8];
    u8        param2[8];
    UiObject* obj;
    UiList*   menu;
    u8        flags;
    s32       sent;
    s32       state;
    u8        ready;

    obj  = task->spawnArg2;
    menu = &D_dryfield_night_saloon_g_r_80185028;

    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_dryfield_night_saloon_g_r_8017D898);
    if (task->state == 0) {
        task->spawnArg1 = -1;
        if (Gp_IsDebugAttachRoom() == 0) {
            menu->field_4 = 4;
        } else {
            menu->field_4 = 3;
        }
        if (menu->field_4 >= 0xB) {
            menu->field_5 = 0xA;
        } else {
            menu->field_5 = menu->field_4;
        }
        menu->field_10 = 0;
        menu->field_9  = 0;
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        menu->field_A = 1;
        Ui_SetListScrollFlag(menu, 1);
        obj->field_C = -((s16)obj->field_10 / 2);
        obj->field_E = -((s16)obj->field_12 / 2);
        if (Gp_IsDebugAttachRoom() == 0) {
            task->status = 0xFF;
        } else {
            task->status = 0xFE;
        }
        task->state += 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    flags = task->status;
    if (flags < 0xF1) {
        state = task->state;
        if (state == 1) {
            if (Midi_IsBusy(0) == 0) {
                param1[3] = 0;
                param1[2] = 4;
                param1[0] = flags;
                param2[0] = state;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
                sent = 1;
            } else {
                sent = 0;
            }
            if (sent == 1) {
                task->state += 1;
            }
        } else {
            if (CdCmd_IsIdle() & 0xFFFF) {
                SndEvt_EnqueueType1(flags, 0);
                SndEvt_EnqueueType5(flags, (u8)D_8007A396);
                ready      = 1;
                D_80062737 = flags;
            } else {
                ready = 0;
            }
            if (ready == 1) {
                task->state  = 1;
                task->status = 0xFF;
                if (Gp_IsDebugAttachRoom() == 0) {
                    gGameSession->flowFlags |= 3;
                }
                if (obj->status != 1) {
                    obj->field_2E = 6;
                }
            }
        }
    }
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu | Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            if (task->status != 0xFE) {
                if (task->status == 0xFF) {
                    obj->field_2E = 6;
                } else {
                    Ui_SetState4((Task*)obj, obj->owner);
                    obj->status = 0;
                }
            }
        }
    }
}

/// The jukebox task: opens the menu panel with the session's UI flag raised
/// and frame timing switched, waits for the panel to close, tears it down and,
/// ten frames later, restores timing, releases the primitive buffer and kills
/// itself.
void func_dryfield_night_saloon_g_r_8017E564(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_dryfield_night_saloon_g_r_8018504C, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        gGameSession->uiOpen = 1;
        task->spawnArg2      = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            Ui_TeardownTree(obj, obj->owner);
            task->killCountdown = 10;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            gGameSession->uiOpen = 0;
            taskKill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

/// Starts the jukebox task and reports success. Its argument is unused;
/// `func_dryfield_night_saloon_g_r_8017DB74` (state 2) still passes one.
s32 func_dryfield_night_saloon_g_r_8017E698(s32 arg0)
{
    Display_InitModeObj(&D_dryfield_night_saloon_g_r_80185068, 0, 0, 0);
    return 1;
}

/// Per-frame effect on the room's model task: recomputes the model's world
/// matrix, then draws every effect whose view mask includes the current view
/// `gGameSession->at4.loc.view`. Positions 0-5 and 20-22 are drawn with UV
/// column 0 and half-extent 0x200, 6-10 with column 1 and 0x1C0, and 23-27
/// with column 0 and 0x300; the two helpers in between take the model's coord.
void func_dryfield_night_saloon_g_r_8017E6C8(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;
    s32            i;

    coord = ((TmdObject*)arg0->extra)->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(coord);
    for (i = 0; i < 6; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i]) {
            func_dryfield_night_saloon_g_r_8017E8B0(&D_dryfield_night_saloon_g_r_80185074[i], 0, 0x200);
        }
    }
    for (i = 6; i < 11; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i]) {
            func_dryfield_night_saloon_g_r_8017E8B0(&D_dryfield_night_saloon_g_r_80185074[i], 1, 0x1C0);
        }
    }
    if (mask & D_dryfield_night_saloon_g_r_80185154[12]) {
        func_dryfield_night_saloon_g_r_8017EB38(coord);
    }
    if (mask & D_dryfield_night_saloon_g_r_80185154[11]) {
        func_dryfield_night_saloon_g_r_8017F0A4(coord, D_dryfield_night_saloon_g_r_801850DC,
                                                D_dryfield_night_saloon_g_r_801850DC - 1, 0x100);
    }
    for (i = 20; i < 23; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i - 7]) {
            func_dryfield_night_saloon_g_r_8017E8B0(&D_dryfield_night_saloon_g_r_80185074[i], 0, 0x200);
        }
    }
    for (i = 23; i < 28; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i - 7]) {
            func_dryfield_night_saloon_g_r_8017E8B0(&D_dryfield_night_saloon_g_r_80185074[i], 0, 0x300);
        }
    }
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// it projects, queues one semi-transparent `POLY_FT4` centred on it: tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380` and the 40-texel texture column
/// `arg1`. `arg2` is a signed half-extent scaled by depth; the grey level
/// follows the frame counter's low bit.
void func_dryfield_night_saloon_g_r_8017E8B0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
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
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
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
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x10;
}

/// Draws the room's two light shafts as Gouraud quads. Both shafts share the
/// roots at positions 14 and 17 of `D_dryfield_night_saloon_g_r_80185074`;
/// each root's tip lies at four times its offset to a later entry (15 and 18
/// for the first shaft, 16 and 19 for the second). All four corners are
/// moved to world space through `coord->workm` and projected through
/// `GsWSMATRIX`. The roots take a grey of 0x20 or 0x30 depending on the
/// parity of `gDisplayState.animFrame` and the tips are black, so the shaft
/// fades outward. The quad is sorted by `tipB`'s `otz` and skipped when that
/// is below 0x11.
void func_dryfield_night_saloon_g_r_8017EB38(GsCOORDINATE2* coord)
{
    u8*                    head;
    RoomLightShaftScratch* block;
    POLY_G4*               prim;
    SVECTOR*               dirA;
    SVECTOR*               dirB;
    s32                    i;
    s32                    j;
    s32                    rgb;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x24;
        *scratch = tmp;
        block    = (RoomLightShaftScratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_saloon_g_r_801850E4);
    gte_rtv0();
    gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->rootA);
    *(u16*)&block->rootA.vx = *(u16*)&block->rootA.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->rootA.vy = *(u16*)&block->rootA.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->rootA.vz = *(u16*)&block->rootA.vz + *(u16*)&coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_saloon_g_r_801850FC);
    gte_rtv0();
    gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->rootB);
    *(u16*)&block->rootB.vx = *(u16*)&block->rootB.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->rootB.vy = *(u16*)&block->rootB.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->rootB.vz = *(u16*)&block->rootB.vz + *(u16*)&coord->workm.t[2];

    for (i = 0; i < 2; i++) {
        j                      = i + 15;
        dirA                   = &D_dryfield_night_saloon_g_r_80185074[j];
        *(u16*)&block->tipA.vx = *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vx +
                                 (*(u16*)&dirA->vx - *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vx) * 4;
        *(u16*)&block->tipA.vy = *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vy +
                                 (*(u16*)&dirA->vy - *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vy) * 4;
        *(u16*)&block->tipA.vz = *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vz +
                                 (*(u16*)&dirA->vz - *(u16*)&D_dryfield_night_saloon_g_r_80185074[14].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((RoomLightShaftScratch*)(head - 0x24))->tipA);
        gte_rtv0();
        gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->tipA);
        *(u16*)&block->tipA.vx = *(u16*)&block->tipA.vx + *(u16*)&coord->workm.t[0];
        *(u16*)&block->tipA.vy = *(u16*)&block->tipA.vy + *(u16*)&coord->workm.t[1];
        *(u16*)&block->tipA.vz = *(u16*)&block->tipA.vz + *(u16*)&coord->workm.t[2];

        j                      = i + 18;
        dirB                   = &D_dryfield_night_saloon_g_r_80185074[j];
        *(u16*)&block->tipB.vx = *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vx +
                                 (*(u16*)&dirB->vx - *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vx) * 4;
        *(u16*)&block->tipB.vy = *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vy +
                                 (*(u16*)&dirB->vy - *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vy) * 4;
        *(u16*)&block->tipB.vz = *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vz +
                                 (*(u16*)&dirB->vz - *(u16*)&D_dryfield_night_saloon_g_r_80185074[17].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((RoomLightShaftScratch*)(head - 0x24))->tipB);
        gte_rtv0();
        gte_stsv(&((RoomLightShaftScratch*)(head - 0x24))->tipB);
        *(u16*)&block->tipB.vx = *(u16*)&block->tipB.vx + *(u16*)&coord->workm.t[0];
        *(u16*)&block->tipB.vy = *(u16*)&block->tipB.vy + *(u16*)&coord->workm.t[1];
        *(u16*)&block->tipB.vz = *(u16*)&block->tipB.vz + *(u16*)&coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->rootA);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&block->rootB, &((RoomLightShaftScratch*)(head - 0x24))->tipA,
                 &((RoomLightShaftScratch*)(head - 0x24))->tipB);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&block->otz);
        if (block->otz >= 0x11) {
            rgb = ((u8)gDisplayState.animFrame & 1) * 16 + 0x20;
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            setRGB0(prim, rgb, rgb, rgb);
            setRGB1(prim, rgb, rgb, rgb);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x24;
}

/// Draws a tapered beam between two points of `coord`'s local space. `arg1`
/// and `arg2` are rotated by `coord->workm` and offset by its translation,
/// then projected through `GsWSMATRIX`; nothing is drawn unless the far end's
/// `otz` is at least 0x11; the near end's `otz` is not clamped. The two ends
/// get screen radii `(s16)arg3 * 64 / otz`.
///
/// Each quarter-turn step of an angle running 0..0x800 queues three
/// `POLY_G4`s: a wedge around the near end, a quad joining the two ends, and a
/// wedge around the far end walked backwards from 0x1000. The centre vertices
/// take a grey of 0x20 or 0x30 depending on the parity of
/// `gDisplayState.animFrame`, the rim vertices are black. Each primitive goes
/// into the OT bucket of its own end's `otz` with a `Gp_AddTpageShift` tpage.
void func_dryfield_night_saloon_g_r_8017F0A4(GsCOORDINATE2* coord, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                extent;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    *(u16*)&block->vec0.vx = *(u16*)&block->vec0.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->vec0.vy = *(u16*)&block->vec0.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->vec0.vz = *(u16*)&block->vec0.vz + *(u16*)&coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(arg2);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    *(u16*)&block->vec1.vx = *(u16*)&block->vec1.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->vec1.vy = *(u16*)&block->vec1.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->vec1.vz = *(u16*)&block->vec1.vz + *(u16*)&coord->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        extent    = (s16)arg3 * 64;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) | 0x20;
        block->r0 = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        block->r1 = extent / block->otz1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
            prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0x1000 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0x1000 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xE00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xE00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
}
