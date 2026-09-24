#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/rooms_shared_8018055c.h"

extern void func_8002E53C(TextDrawReq* req, u8* text);

extern u8  D_80062737;
extern s16 D_8007A396;

/// The jukebox's ten track lists, one per game mode (4 before the first
/// clear), the second five used outside the debug attach room.
extern RoomsShared8018055cMenu D_dryfield_night_saloon_g_r_8017D870;

/// The jukebox menu's title, "SELECT".
extern char D_dryfield_night_saloon_g_r_8017D898[];

/// The jukebox's track list.
extern UiList D_dryfield_night_saloon_g_r_80185028;

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
