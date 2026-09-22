#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017eb5c.h"
#include "rooms/rooms_shared_8017fdb8.h"

extern char Gp_StrEmpty[];

void RoomsShared8017eb5c(DialogPrompt* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, RoomsShared8017fdb8Msg, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = RoomsShared8017eb5cMode0;
    obj->owner->spawnArg1 = (u16)obj->owner->spawnArg1;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = RoomsShared8017eb5cMode1;
            obj->owner->spawnArg1 |= 0x10000;
            break;
        case 2:
            text                   = RoomsShared8017eb5cMode2;
            obj->owner->spawnArg1 |= 0x20000;
            break;
        case 3:
            text                   = RoomsShared8017eb5cMode3;
            obj->owner->spawnArg1 |= 0x30000;
            break;
    }

    if (*RoomsShared8017eb5cIdList(obj->owner->spawnArg1) == 0xFFFF) {
        prompt->field_1C = Ui_LookupTable(obj, 2);
        prompt->field_C  = 0;
    }

    one2 = 1;
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, text, prompt->field_1C, one2, 0);

    status = obj->status;
    if (((status >> 16) == one2) || (status == one2)) {
        if (prompt->field_10 == prompt->field_8) {
            Ui_SetHolderParam((s32)Gp_StrEmpty, 0, 0);
        }
    }

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&RoomsShared8017eb5cListDesc, obj->owner->spawnArg1, 1, 1, obj);
        obj->status = 0;
    }
}
