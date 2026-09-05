#include "common.h"

#include "gameplay/3CD8.h"
#include "main/fs.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/rooms_shared_8017e28c.h"

extern u8  D_80062737;
extern s16 D_8007A396;

void RoomsShared8017e28c(Task* task)
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
    menu = &RoomsShared8017e28cMenu;

    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, RoomsShared8017e28cTitle);
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
            task->flags = 0xFF;
        } else {
            task->flags = 0xFE;
        }
        task->state += 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    flags = task->flags;
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
                task->state = 1;
                task->flags = 0xFF;
                if (Gp_IsDebugAttachRoom() == 0) {
                    Game_Session->field_69 |= 3;
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
            if (task->flags != 0xFE) {
                if (task->flags == 0xFF) {
                    obj->field_2E = 6;
                } else {
                    Ui_SetState4((Task*)obj, obj->owner);
                    obj->status = 0;
                }
            }
        }
    }
}
