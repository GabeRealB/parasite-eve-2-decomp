#include "common.h"

#include <psyq/libgte.h>

#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017fdb8.h"

void RoomsShared8017fdb8(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->baseX + (u16)prompt->field_18;
    req.y          = obj->baseY + (u16)prompt->field_1A;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, RoomsShared8017fdb8Msg);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}
