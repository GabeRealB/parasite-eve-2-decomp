#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/pad.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"

extern UiObject*    D_80067634;
extern UiObjectDesc D_800611E4;
extern UiList       D_acropolis_fire_escape_80181CF4;
void                func_acropolis_fire_escape_8017E298(UiList* list, UiObject* obj);
void                RoomsShared80180f94(UiList* list, UiObject* obj);

extern UiList       D_acropolis_fire_escape_80181C6C;
extern UiObjectDesc D_acropolis_fire_escape_80181C90;

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", D_acropolis_fire_escape_8017D610);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017de9cHundred);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", func_acropolis_fire_escape_8017E298);
