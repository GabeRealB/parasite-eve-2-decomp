#include "common.h"

#include <psyq/libgte.h>

#include "main/fs.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/rooms_shared_8017f388.h"

extern u8           RoomsShared8017f114Msg[];
extern u8           D_acropolis_fire_escape_80181A00[];
extern u8           D_acropolis_fire_escape_80181A0C[];
extern u8           D_80071086;
extern UiObjectDesc D_800611E4;
extern UiObjectDesc D_acropolis_fire_escape_80181CAC;
extern char         RoomsShared8017ef20Title[];
extern UiList       RoomsShared8017ef20List;
extern UiObjectDesc RoomsShared8017e8b4Desc;

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017ea68Title);

void func_acropolis_fire_escape_8017F1F8(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_80181A00, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181CAC, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}
void func_acropolis_fire_escape_8017F2C0(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_80181A0C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&RoomsShared8017f388Desc, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}
