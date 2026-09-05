#include "common.h"

#include "gameplay/268.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/rooms_shared_8018055c.h"

extern void func_8002E53C(TextDrawReq* req, u8* text);

void RoomsShared8018055c(DialogPrompt* prompt, UiObject* obj)
{
    RoomsShared8018055cMenu    menu;
    RoomsShared8018055cCourse* course;
    s32                        row;
    s32                        list;
    s32                        mode;

    row  = prompt->field_8;
    menu = RoomsShared8018055cCourses;

    list = 4;
    if (Mc_SaveData.field_E != 0) {
        list = Mc_SaveData.field_F;
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
                if (obj->owner->flags != course->id) {
                    SndEvt_EnqueueType2(0, 0x3C);
                    obj->owner->state = mode;
                    obj->owner->flags = course->id;
                    CdCmd_DropPending();
                }
                obj->owner->spawnArg1 = prompt->field_8;
            }
        }
    }
}
