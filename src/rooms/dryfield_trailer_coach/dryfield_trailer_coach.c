#include "rooms/dryfield_trailer_coach.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_420700.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        GpAnimSet* sets[17];
        GpCopyArg  copy;
        u8         text0[16];
        u8         text1[20];
        u8         text2[12];
        u8*        options[4];
    } data;
    s32 words[35];
} DryfieldTrailerCoachAnimStorage5368;
STATIC_ASSERT_SIZEOF(DryfieldTrailerCoachAnimStorage5368, 140);

extern DryfieldTrailerCoachAnimStorage5368 D_dryfield_trailer_coach_80185368;

/// The "%" suffix the room's percentage formatters append.
extern u8 D_dryfield_trailer_coach_801845F8[];

/// The item id the shop list's cursor last rested on.
extern s32 D_dryfield_trailer_coach_80183D44;

/// Task descriptor table the room's cutscene tasks spawn from: the room spawns
/// entry 0 with a cutscene record as its argument, and
/// `func_dryfield_trailer_coach_80181D88` spawns entry 1 for the scene.
extern TaskDesc D_dryfield_trailer_coach_80184F7C[];

static void func_dryfield_trailer_coach_80181D4C(Task* task);

/// The 0xFFFF-terminated item id lists `func_dryfield_trailer_coach_8017D7F4`
/// chooses from.
extern u16 D_dryfield_trailer_coach_80183950[];
extern u16 D_dryfield_trailer_coach_80183958[];
extern u16 D_dryfield_trailer_coach_80183960[];
extern u16 D_dryfield_trailer_coach_80183968[];
extern u16 D_dryfield_trailer_coach_80183978[];
extern u16 D_dryfield_trailer_coach_80183988[];
extern u16 D_dryfield_trailer_coach_80183998[];
extern u16 D_dryfield_trailer_coach_801839A0[];
extern u16 D_dryfield_trailer_coach_801839B0[];
extern u16 D_dryfield_trailer_coach_801839C0[];
extern u16 D_dryfield_trailer_coach_801839D0[];
extern u16 D_dryfield_trailer_coach_801839D8[];
extern u16 D_dryfield_trailer_coach_801839EC[];
extern u16 D_dryfield_trailer_coach_80183A04[];
extern u16 D_dryfield_trailer_coach_80183A18[];
extern u16 D_dryfield_trailer_coach_80183A20[];
extern u16 D_dryfield_trailer_coach_80183A30[];
extern u16 D_dryfield_trailer_coach_80183A48[];
extern u16 D_dryfield_trailer_coach_80183A5C[];
extern u16 D_dryfield_trailer_coach_80183A64[];
extern u16 D_dryfield_trailer_coach_80183A78[];
extern u16 D_dryfield_trailer_coach_80183A94[];
extern u16 D_dryfield_trailer_coach_80183AA4[];
extern u16 D_dryfield_trailer_coach_80183AB0[];
extern u16 D_dryfield_trailer_coach_80183AC8[];
extern u16 D_dryfield_trailer_coach_80183AE4[];
extern u16 D_dryfield_trailer_coach_80183AF8[];
extern u16 D_dryfield_trailer_coach_80183B00[];
extern u16 D_dryfield_trailer_coach_80183B14[];
extern u16 D_dryfield_trailer_coach_80183B34[];
extern u16 D_dryfield_trailer_coach_80183B44[];
extern u16 D_dryfield_trailer_coach_80183B50[];
extern u16 D_dryfield_trailer_coach_80183B68[];
extern u16 D_dryfield_trailer_coach_80183B6C[];
extern u16 D_dryfield_trailer_coach_80183B70[];
extern u16 D_dryfield_trailer_coach_80183B78[];
extern u16 D_dryfield_trailer_coach_80183B88[];
extern u16 D_dryfield_trailer_coach_80183B90[];
extern u16 D_dryfield_trailer_coach_80183B98[];
extern u16 D_dryfield_trailer_coach_80183BA0[];
extern u16 D_dryfield_trailer_coach_80183BAC[];
extern u16 D_dryfield_trailer_coach_80183BB4[];
extern u16 D_dryfield_trailer_coach_80183BC0[];
extern u16 D_dryfield_trailer_coach_80183BC8[];
extern u16 D_dryfield_trailer_coach_80183BD4[];
extern u16 D_dryfield_trailer_coach_80183BE0[];
extern u16 D_dryfield_trailer_coach_80183BE8[];
extern u16 D_dryfield_trailer_coach_80183BF0[];
extern u16 D_dryfield_trailer_coach_80183BFC[];
extern u16 D_dryfield_trailer_coach_80183C08[];
extern u16 D_dryfield_trailer_coach_80183C10[];
extern u16 D_dryfield_trailer_coach_80183C1C[];
extern u16 D_dryfield_trailer_coach_80183C28[];
extern u16 D_dryfield_trailer_coach_80183C34[];
extern u16 D_dryfield_trailer_coach_80183C38[];
extern u16 D_dryfield_trailer_coach_80183C44[];
extern u16 D_dryfield_trailer_coach_80183C50[];
extern u16 D_dryfield_trailer_coach_80183C5C[];
extern u16 D_dryfield_trailer_coach_80183C64[];
extern u16 D_dryfield_trailer_coach_80183C70[];
extern u16 D_dryfield_trailer_coach_80183C7C[];
extern u16 D_dryfield_trailer_coach_80183C88[];
extern u16 D_dryfield_trailer_coach_80183C90[];
extern u16 D_dryfield_trailer_coach_80183C9C[];

/// The list returned when no case matches.
extern u16 D_dryfield_trailer_coach_80183E2C[];

void func_dryfield_trailer_coach_8017DE64(UiList*, UiObject*);

void func_dryfield_trailer_coach_8017E808(Task*);
void func_dryfield_trailer_coach_8017EA58(UiList*, UiObject*);
void func_dryfield_trailer_coach_8017EC78(Task*);
void func_dryfield_trailer_coach_8017EE20(Task*);
void func_dryfield_trailer_coach_8017F004(UiList*, UiObject*);
void func_dryfield_trailer_coach_8017F218(Task*);
void func_dryfield_trailer_coach_8017FCB4(UiList*, UiObject*);
void func_dryfield_trailer_coach_8017FD70(Task*);

void func_dryfield_trailer_coach_8017E808(Task*);
void func_dryfield_trailer_coach_8017F398(Task*);
void func_dryfield_trailer_coach_8017F660(Task*);
void func_dryfield_trailer_coach_8017F834(Task*);
void func_dryfield_trailer_coach_8017FE98(Task*);

void func_dryfield_trailer_coach_8017FFCC(UiList*, UiObject*);
void func_dryfield_trailer_coach_80180798(UiList*, UiObject*);
void func_dryfield_trailer_coach_801811B0(Task*);
void func_dryfield_trailer_coach_8018165C(Task*);
void func_dryfield_trailer_coach_8018181C(Task*);
void func_dryfield_trailer_coach_80181A10(UiList*, UiObject*);
void func_dryfield_trailer_coach_80181AF4(UiList*, UiObject*);
void func_dryfield_trailer_coach_80181BBC(UiList*, UiObject*);
void func_dryfield_trailer_coach_80181C84(UiList*, UiObject*);

s32  func_dryfield_trailer_coach_80182578(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_dryfield_trailer_coach_80182580(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32  func_dryfield_trailer_coach_801825A8(Task*, s32, s32, GpMessageArg);
void func_dryfield_trailer_coach_80181D88(Task*);
void func_dryfield_trailer_coach_801822F4(Task*);
void func_dryfield_trailer_coach_801824E8(Task*);
void func_dryfield_trailer_coach_801827F8(Task*);

extern DryfieldTrailerCoachAnimStorage5368 D_dryfield_trailer_coach_80185368;
extern GpAnimArg                           D_dryfield_trailer_coach_80185038;
extern GpAnimArg                           D_dryfield_trailer_coach_8018504C;
extern GpAnimArg                           D_dryfield_trailer_coach_80185060;
extern GpAnimArg                           D_dryfield_trailer_coach_80185088;
extern GpAnimArg                           D_dryfield_trailer_coach_8018509C;
extern GpAnimArg                           D_dryfield_trailer_coach_801850B0;
extern GpAnimArg                           D_dryfield_trailer_coach_801850C4;
extern GpAnimArg                           D_dryfield_trailer_coach_801850EC;
extern GpAnimArg                           D_dryfield_trailer_coach_80185100;
extern GpAnimArg                           D_dryfield_trailer_coach_80185114;
extern GpAnimArg                           D_dryfield_trailer_coach_80185128;
extern GpAnimArg                           D_dryfield_trailer_coach_8018513C;
extern GpAnimArg                           D_dryfield_trailer_coach_80185150;
extern GpAnimArg                           D_dryfield_trailer_coach_80185178;
extern GpAnimArg                           D_dryfield_trailer_coach_801851B0;
extern GpAnimArg                           D_dryfield_trailer_coach_801851EC;
extern GpAnimArg                           D_dryfield_trailer_coach_80185200;
extern GpAnimArg                           D_dryfield_trailer_coach_80185214;
extern GpAnimArg                           D_dryfield_trailer_coach_80185228;
extern GpAnimArg                           D_dryfield_trailer_coach_8018523C;
extern GpAnimArg                           D_dryfield_trailer_coach_80185250;
extern GpAnimArg                           D_dryfield_trailer_coach_80185264;
extern GpAnimArg                           D_dryfield_trailer_coach_801852A0;
extern GpAnimArg                           D_dryfield_trailer_coach_801852B4;
extern GpAnimArg                           D_dryfield_trailer_coach_801852C8;
extern GpAnimArg                           D_dryfield_trailer_coach_801852DC;
extern GpAnimArg                           D_dryfield_trailer_coach_801852F0;
extern GpAnimArg                           D_dryfield_trailer_coach_80185304;
extern GpAnimArg                           D_dryfield_trailer_coach_80185318;
extern GpAnimArg                           D_dryfield_trailer_coach_8018532C;
extern GpAnimArg                           D_dryfield_trailer_coach_80185340;
extern GpAnimArg                           D_dryfield_trailer_coach_80185354;
extern GpAnimSet                           D_dryfield_trailer_coach_80184C28;
extern GpAnimSet                           D_dryfield_trailer_coach_80184F54;
extern GpCmdArg                            D_dryfield_trailer_coach_8018518C;
extern GpCmdArg                            D_dryfield_trailer_coach_80185190;
extern GpCmdArg                            D_dryfield_trailer_coach_80185194;
extern GpCmdArg                            D_dryfield_trailer_coach_80185198;
extern GpGridParams                        D_dryfield_trailer_coach_801876B4[1];
extern GpObj4C                             D_dryfield_trailer_coach_80189254[4];
extern GpObj4C                             D_dryfield_trailer_coach_80189384[12];
extern GpRoomBoundVec                      D_dryfield_trailer_coach_80189BAC[12];
extern GpRoomCoordSet                      D_dryfield_trailer_coach_80189B94[1];
extern GpXformArg                          D_dryfield_trailer_coach_80184FD8;
extern GpXformArg                          D_dryfield_trailer_coach_80184FF0;
extern GpXformArg                          D_dryfield_trailer_coach_80185008;
void                                       func_dryfield_trailer_coach_80182850(void);

u16 D_dryfield_trailer_coach_80183950[4] = {
    140, 143, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183958[4] = {
    172, 175, 0xFFFE, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183960[4] = {
    103, 98, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183968[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183978[8] = {
    131, 140, 143, 10, 70, 138, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183988[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183998[4] = {
    108, 100, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_801839A0[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_801839B0[8] = {
    132, 140, 143, 10, 70, 66, 138, 0xFFFF,
};

u16 D_dryfield_trailer_coach_801839C0[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_801839D0[4] = {
    98, 105, 106, 0xFFFF,
};

u16 D_dryfield_trailer_coach_801839D8[10] = {
    65, 59, 58, 1, 2, 6, 8, 4,
    0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_801839EC[12] = {
    131, 157, 140, 142, 143, 10, 70, 69,
    67, 138, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183A04[10] = {
    160, 161, 172, 173, 171, 169, 175, 0xFFFE,
    0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183A18[4] = {
    108, 100, 102, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183A20[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183A30[12] = {
    157, 9, 140, 142, 138, 143, 10, 70,
    69, 66, 67, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183A48[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183A5C[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183A64[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183A78[14] = {
    157, 9, 140, 142, 143, 10, 70, 69,
    66, 67, 68, 138, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183A94[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183AA4[6] = {
    103, 98, 100, 97, 107, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183AB0[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183AC8[14] = {
    140, 142, 138, 143, 10, 70, 69, 66,
    67, 68, 157, 9, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183AE4[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183AF8[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183B00[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183B14[16] = {
    140, 142, 138, 139, 143, 10, 70, 69,
    66, 67, 68, 144, 157, 9, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183B34[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183B44[6] = {
    100, 98, 97, 103, 107, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183B50[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183B68[2] = {
    139, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183B6C[2] = {
    171, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183B70[4] = {
    108, 13, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183B78[8] = {
    65, 59, 58, 60, 11, 55, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183B88[4] = {
    131, 138, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183B90[4] = {
    160, 171, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183B98[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183BA0[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183BAC[4] = {
    140, 138, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183BB4[6] = {
    160, 172, 171, 175, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183BC0[4] = {
    98, 13, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183BC8[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183BD4[6] = {
    131, 138, 143, 70, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183BE0[4] = {
    160, 171, 175, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183BE8[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183BF0[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183BFC[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183C08[4] = {
    171, 175, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183C10[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183C1C[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183C28[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183C34[2] = {
    171, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183C38[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183C44[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183C50[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183C5C[4] = {
    171, 175, 0xFFFE, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183C64[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183C70[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183C7C[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183C88[4] = {
    171, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_trailer_coach_80183C90[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_dryfield_trailer_coach_80183C9C[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

RoomShopTier D_dryfield_trailer_coach_80183CA8[13] = {
    { 0x38A4, { 109, 55, 2 }, { 0, 0 } },
    { 0x3E80, { 70, 10, 58 }, { 0, 0 } },
    { 0xABE0, { 69, 60, 161 }, { 0, 0 } },
    { 0xC738, { 66, 13, 6 }, { 0, 0 } },
    { 0xDEA8, { 67, 11, 97 }, { 0, 0 } },
    { 0xF230, { 68, 14, 56 }, { 0, 0 } },
    { 0x101D0, { 107, 162, 57 }, { 0, 0 } },
    { 0x10D88, { 142, 174, 173 }, { 0, 0 } },
    { 0x11940, { 136, 166, 54 }, { 0, 0 } },
    { 0x124F8, { 144, 167, 5 }, { 0, 0 } },
    { 0x30D40, { 139, 170, 3 }, { 0, 0 } },
    { 0x61A80, { 149, 63, 7 }, { 0, 0 } },
    { 0x7FFFFFFF, { 150, 61, 62 }, { 0, 0 } },
};

s32 D_dryfield_trailer_coach_80183D44 = -1;

u8 D_dryfield_trailer_coach_80183D48[20] = {
    80, 117, 114, 99, 104, 97, 115, 101, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
};

u8 D_dryfield_trailer_coach_80183D5C[8] = {
    80, 97, 115, 115, 0, 0, 0, 0,
};

u8 D_dryfield_trailer_coach_80183D64[16] = {
    66, 97, 116, 116, 101, 114, 105, 101, 115, 47, 70, 117, 101, 108, 0, 0,
};

u8 D_dryfield_trailer_coach_80183D74[4] = { 0 };

u8 D_dryfield_trailer_coach_80183D78[60] = {
    87, 101, 97, 112, 111, 110, 115, 32, 117, 115, 105, 110, 103, 32, 98, 97,
    116, 116, 101, 114, 105, 101, 115, 32, 111, 114, 32, 102, 117, 101, 108, 10,
    99, 97, 110, 32, 98, 101, 32, 114, 101, 108, 111, 97, 100, 101, 100, 32,
    102, 111, 114, 32, 102, 114, 101, 101, 46, 0, 0, 0,
};

u8 D_dryfield_trailer_coach_80183DB4[8] = {
    87, 101, 97, 112, 111, 110, 115, 0,
};

u8 D_dryfield_trailer_coach_80183DBC[12] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 0, 0,
};

u8 D_dryfield_trailer_coach_80183DC8[8] = {
    65, 114, 109, 111, 114, 0, 0, 0,
};

u8 D_dryfield_trailer_coach_80183DD0[8] = {
    73, 116, 101, 109, 115, 0, 0, 0,
};

u8 D_dryfield_trailer_coach_80183DD8[20] = {
    73, 110, 115, 117, 102, 102, 105, 99, 105, 101, 110, 116, 32, 66, 80, 46,
    0, 0, 0, 0,
};

u8 D_dryfield_trailer_coach_80183DEC[16] = {
    73, 110, 118, 101, 110, 116, 111, 114, 121, 32, 102, 117, 108, 108, 46, 0,
};

u8 D_dryfield_trailer_coach_80183DFC[32] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 32, 99, 97, 112, 97, 99,
    105, 116, 121, 32, 114, 101, 97, 99, 104, 101, 100, 46, 0, 0, 0, 0,
};

u8 D_dryfield_trailer_coach_80183E1C[12] = {
    65, 109, 111, 117, 110, 116, 0, 0, 0, 0, 0, 0,
};

u8 D_dryfield_trailer_coach_80183E28[4] = {
    120, 0, 0, 0,
};

u16 D_dryfield_trailer_coach_80183E2C[2] = {
    0xFFFF, 0,
};

UiListItemFunc D_dryfield_trailer_coach_80183E30[1] = {
    func_dryfield_trailer_coach_8017DE64,
};

UiListItemFunc D_dryfield_trailer_coach_80183E34[1] = {
    func_dryfield_trailer_coach_8017EA58,
};

UiList D_dryfield_trailer_coach_80183E38 = { D_dryfield_trailer_coach_80183E34, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_dryfield_trailer_coach_80183E5C[2] = {
    func_dryfield_trailer_coach_8017F004,
    func_dryfield_trailer_coach_8017FCB4,
};

UiList D_dryfield_trailer_coach_80183E64 = { D_dryfield_trailer_coach_80183E5C, 2, { .u = 2 }, 1, 10, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_dryfield_trailer_coach_80183E88 = { 2, 0xFF70, 0xFF98, 128, 40, 56, 0, 0, 192, func_dryfield_trailer_coach_8017EC78, 0 };

UiObjectDesc D_dryfield_trailer_coach_80183EA4 = { 2, 0xFF74, 0xFFA3, 188, 160, 48, 0, 0, 192, func_dryfield_trailer_coach_8017E808, 0 };

UiObjectDesc D_dryfield_trailer_coach_80183EC0 = { 0, 48, 4, 96, 60, 52, 0, 0, 192, func_dryfield_trailer_coach_8017EE20, 0 };

UiObjectDesc D_dryfield_trailer_coach_80183EDC = { 0, 48, 32, 70, 32, 20, 0, 0, 192, func_dryfield_trailer_coach_8017FD70, 0 };

UiObjectDesc D_dryfield_trailer_coach_80183EF8 = { 2, 0xFFA0, 0xFFD0, 192, 96, 8, 0, 0, 192, func_dryfield_trailer_coach_8017F218, 0 };

// Retained data: Complete UI descriptor follows the adjacent UI descriptors. Its last 12 bytes also resemble a TaskDesc, which is its embedded task seed.
UiObjectDesc D_dryfield_trailer_coach_80183F14 = { 0, 0xFF80, 0xFFE0, 160, 92, 48, 0, 0, 192, func_dryfield_trailer_coach_8017E808, 0 };

UiObjectDesc D_dryfield_trailer_coach_80183F30 = { 2, 0xFFB8, 0xFFDC, 144, 64, 32, 0, 0, 192, func_dryfield_trailer_coach_8017F398, 0 };

UiObjectDesc D_dryfield_trailer_coach_80183F4C = { 0, 48, 0xFFA3, 96, 97, 44, 0, 0, 192, func_dryfield_trailer_coach_8017F660, 0 };

UiObjectDesc D_dryfield_trailer_coach_80183F68 = { 3, 0xFFB8, 0xFFE0, 184, 48, 16, 0, 0, 192, func_dryfield_trailer_coach_8017F834, 0 };

TaskDesc D_dryfield_trailer_coach_80183F84 = { 0, 192, func_dryfield_trailer_coach_8017FE98, { .model = NULL } };

TmdBone D_dryfield_trailer_coach_80183F90[3] = {
#include "assets/dryfield_trailer_coach_model_06F94_skeleton.inc"
};

u32 D_dryfield_trailer_coach_80183FFC[3] = {
#include "assets/dryfield_trailer_coach_model_06F94_partVerts.inc"
};

SVECTOR D_dryfield_trailer_coach_80184008[56] = {
#include "assets/dryfield_trailer_coach_model_06F94_verts.inc"
};

SVECTOR D_dryfield_trailer_coach_801841C8[6] = {
#include "assets/dryfield_trailer_coach_model_06F94_normals.inc"
};

u32 D_dryfield_trailer_coach_801841F8[215] = {
#include "assets/dryfield_trailer_coach_model_06F94_stream.inc"
};

TmdSource D_dryfield_trailer_coach_80184554 = {
    0, 1768, 0, 3,
    D_dryfield_trailer_coach_80183FFC, D_dryfield_trailer_coach_80184008, D_dryfield_trailer_coach_801841C8, D_dryfield_trailer_coach_80183F90, D_dryfield_trailer_coach_801841F8,
};

u8 D_dryfield_trailer_coach_80184578[8] = {
    83,
    97,
    118,
    101,
    0,
    0,
    0,
    0,
};

u8 D_dryfield_trailer_coach_80184580[12] = {
    80,
    108,
    97,
    121,
    32,
    68,
    97,
    116,
    97,
    0,
    0,
    0,
};

u8 D_dryfield_trailer_coach_8018458C[12] = {
    87,
    101,
    97,
    112,
    111,
    110,
    32,
    68,
    97,
    116,
    97,
    0,
};

u8 D_dryfield_trailer_coach_80184598[8] = {
    80,
    69,
    32,
    68,
    97,
    116,
    97,
    0,
};

u8 D_dryfield_trailer_coach_801845A0[8] = {
    84,
    105,
    109,
    101,
    0,
    0,
    0,
    0,
};

u8 D_dryfield_trailer_coach_801845A8[4] = {
    87,
    111,
    110,
    0,
};

u8 D_dryfield_trailer_coach_801845AC[8] = {
    69,
    115,
    99,
    97,
    112,
    101,
    100,
    0,
};

u8 D_dryfield_trailer_coach_801845B4[12] = {
    66,
    97,
    116,
    116,
    108,
    101,
    115,
    32,
    119,
    111,
    110,
    0,
};

u8 D_dryfield_trailer_coach_801845C0[16] = {
    69,
    120,
    116,
    101,
    114,
    109,
    105,
    110,
    97,
    116,
    101,
    100,
    0,
    0,
    0,
    0,
};

u8 D_dryfield_trailer_coach_801845D0[8] = {
    83,
    97,
    118,
    101,
    100,
    0,
    0,
    0,
};

u8 D_dryfield_trailer_coach_801845D8[8] = {
    67,
    108,
    101,
    97,
    114,
    101,
    100,
    0,
};

u8 D_dryfield_trailer_coach_801845E0[8] = {
    77,
    97,
    120,
    32,
    69,
    88,
    80,
    0,
};

u8 D_dryfield_trailer_coach_801845E8[8] = {
    77,
    97,
    120,
    32,
    66,
    80,
    0,
    0,
};

u8 D_dryfield_trailer_coach_801845F0[8] = {
    32,
    116,
    105,
    109,
    101,
    115,
    0,
    0,
};

u8 D_dryfield_trailer_coach_801845F8[4] = {
    37,
    0,
    0,
    0,
};

u8 D_dryfield_trailer_coach_801845FC[44] = {
    84,
    111,
    116,
    97,
    108,
    32,
    97,
    109,
    111,
    117,
    110,
    116,
    32,
    111,
    102,
    10,
    116,
    105,
    109,
    101,
    32,
    115,
    112,
    101,
    110,
    116,
    32,
    102,
    111,
    114,
    32,
    116,
    104,
    105,
    115,
    32,
    103,
    97,
    109,
    101,
    46,
    0,
    0,
    0,
};

u8 D_dryfield_trailer_coach_80184628[36] = {
    78,
    117,
    109,
    98,
    101,
    114,
    32,
    111,
    102,
    32,
    115,
    97,
    118,
    101,
    115,
    10,
    117,
    115,
    101,
    100,
    32,
    105,
    110,
    32,
    116,
    104,
    105,
    115,
    32,
    103,
    97,
    109,
    101,
    46,
    0,
    0,
};

u8 D_dryfield_trailer_coach_8018464C[48] = {
    84,
    111,
    116,
    97,
    108,
    32,
    110,
    117,
    109,
    98,
    101,
    114,
    32,
    111,
    102,
    32,
    101,
    110,
    101,
    109,
    105,
    101,
    115,
    10,
    100,
    101,
    102,
    101,
    97,
    116,
    101,
    100,
    32,
    105,
    110,
    32,
    116,
    104,
    105,
    115,
    32,
    103,
    97,
    109,
    101,
    46,
    0,
    0,
};

u8 D_dryfield_trailer_coach_8018467C[52] = {
    84,
    111,
    116,
    97,
    108,
    32,
    110,
    117,
    109,
    98,
    101,
    114,
    32,
    111,
    102,
    32,
    101,
    115,
    99,
    97,
    112,
    101,
    115,
    10,
    102,
    114,
    111,
    109,
    32,
    98,
    97,
    116,
    116,
    108,
    101,
    32,
    105,
    110,
    32,
    116,
    104,
    105,
    115,
    32,
    103,
    97,
    109,
    101,
    46,
    0,
    0,
    0,
};

u8 D_dryfield_trailer_coach_801846B0[52] = {
    67,
    117,
    114,
    114,
    101,
    110,
    116,
    32,
    112,
    101,
    114,
    99,
    101,
    110,
    116,
    32,
    111,
    102,
    32,
    116,
    111,
    116,
    97,
    108,
    10,
    98,
    97,
    116,
    116,
    108,
    101,
    115,
    32,
    119,
    111,
    110,
    32,
    105,
    110,
    32,
    116,
    104,
    105,
    115,
    32,
    103,
    97,
    109,
    101,
    46,
    0,
    0,
};

u8 D_dryfield_trailer_coach_801846E4[56] = {
    67,
    117,
    114,
    114,
    101,
    110,
    116,
    32,
    112,
    101,
    114,
    99,
    101,
    110,
    116,
    32,
    111,
    102,
    32,
    116,
    111,
    116,
    97,
    108,
    10,
    101,
    110,
    101,
    109,
    105,
    101,
    115,
    32,
    100,
    101,
    102,
    101,
    97,
    116,
    101,
    100,
    32,
    105,
    110,
    32,
    116,
    104,
    105,
    115,
    32,
    103,
    97,
    109,
    101,
    46,
    0,
};

u8 D_dryfield_trailer_coach_8018471C[52] = {
    78,
    117,
    109,
    98,
    101,
    114,
    32,
    111,
    102,
    32,
    116,
    105,
    109,
    101,
    115,
    32,
    121,
    111,
    117,
    32,
    104,
    97,
    118,
    101,
    10,
    99,
    108,
    101,
    97,
    114,
    101,
    100,
    32,
    116,
    104,
    101,
    32,
    103,
    97,
    109,
    101,
    32,
    115,
    111,
    32,
    102,
    97,
    114,
    46,
    0,
    0,
    0,
};

u8 D_dryfield_trailer_coach_80184750[56] = {
    71,
    114,
    101,
    97,
    116,
    101,
    115,
    116,
    32,
    97,
    109,
    111,
    117,
    110,
    116,
    32,
    111,
    102,
    32,
    69,
    88,
    80,
    32,
    103,
    97,
    116,
    104,
    101,
    114,
    101,
    100,
    10,
    98,
    121,
    32,
    116,
    104,
    101,
    32,
    101,
    110,
    100,
    32,
    111,
    102,
    32,
    116,
    104,
    101,
    32,
    103,
    97,
    109,
    101,
    46,
    0,
};

u8 D_dryfield_trailer_coach_80184788[56] = {
    71,
    114,
    101,
    97,
    116,
    101,
    115,
    116,
    32,
    97,
    109,
    111,
    117,
    110,
    116,
    32,
    111,
    102,
    32,
    66,
    80,
    32,
    103,
    97,
    116,
    104,
    101,
    114,
    101,
    100,
    10,
    98,
    121,
    32,
    116,
    104,
    101,
    32,
    101,
    110,
    100,
    32,
    111,
    102,
    32,
    116,
    104,
    101,
    32,
    103,
    97,
    109,
    101,
    46,
    0,
    0,
};

UiListItemFunc D_dryfield_trailer_coach_801847C0[1] = {
    func_dryfield_trailer_coach_8017FFCC,
};

UiList D_dryfield_trailer_coach_801847C4 = { D_dryfield_trailer_coach_801847C0, 9, { .u = 9 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_dryfield_trailer_coach_801847E8[1] = {
    func_dryfield_trailer_coach_80180798,
};

UiList D_dryfield_trailer_coach_801847EC = { D_dryfield_trailer_coach_801847E8, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_dryfield_trailer_coach_80184810 = { 3, 0xFF70, 64, 288, 40, 56, 0, 0, 192, func_dryfield_trailer_coach_8018165C, 0 };

UiObjectDesc D_dryfield_trailer_coach_8018482C = { 2, 0xFF70, 0xFF98, 288, 120, 40, 0, 0, 192, func_dryfield_trailer_coach_8018181C, 0 };

UiObjectDesc D_dryfield_trailer_coach_80184848 = { 2, 0xFF70, 0xFF98, 288, 168, 40, 0, 0, 192, func_dryfield_trailer_coach_801811B0, 0 };

UiListItemFunc D_dryfield_trailer_coach_80184864[4] = {
    func_dryfield_trailer_coach_80181A10,
    func_dryfield_trailer_coach_80181AF4,
    func_dryfield_trailer_coach_80181BBC,
    func_dryfield_trailer_coach_80181C84,
};

UiList D_dryfield_trailer_coach_80184874 = { D_dryfield_trailer_coach_80184864, 4, { .u = 4 }, 1, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} DryfieldTrailerCoachPoseBank72D8;

DryfieldTrailerCoachPoseBank72D8 D_dryfield_trailer_coach_80184898 = { .poses = {
#include "assets/dryfield_trailer_coach_animation_07668_bank1.inc"
} };

GpPackedSvec D_dryfield_trailer_coach_801848BC[81] = {
#include "assets/dryfield_trailer_coach_animation_07668_bank4.inc"
};

GpAnimRec D_dryfield_trailer_coach_80184A00[128] = {
#include "assets/dryfield_trailer_coach_animation_07668_records.inc"
};

u16 D_dryfield_trailer_coach_80184C00[20] = {
#include "assets/dryfield_trailer_coach_animation_07668_indices.inc"
};

GpAnimSet D_dryfield_trailer_coach_80184C28 = {
    D_dryfield_trailer_coach_80184A00, D_dryfield_trailer_coach_80184C00,
    { NULL, D_dryfield_trailer_coach_80184898.words, NULL, NULL, D_dryfield_trailer_coach_801848BC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} DryfieldTrailerCoachPoseBank7690;

DryfieldTrailerCoachPoseBank7690 D_dryfield_trailer_coach_80184C50 = { .poses = {
#include "assets/dryfield_trailer_coach_animation_07994_bank1.inc"
} };

GpPackedSvec D_dryfield_trailer_coach_80184CA4[56] = {
#include "assets/dryfield_trailer_coach_animation_07994_bank4.inc"
};

GpAnimRec D_dryfield_trailer_coach_80184D84[106] = {
#include "assets/dryfield_trailer_coach_animation_07994_records.inc"
};

u16 D_dryfield_trailer_coach_80184F2C[20] = {
#include "assets/dryfield_trailer_coach_animation_07994_indices.inc"
};

GpAnimSet D_dryfield_trailer_coach_80184F54 = {
    D_dryfield_trailer_coach_80184D84, D_dryfield_trailer_coach_80184F2C,
    { NULL, D_dryfield_trailer_coach_80184C50.words, NULL, NULL, D_dryfield_trailer_coach_80184CA4, NULL, NULL, NULL },
};

TaskDesc D_dryfield_trailer_coach_80184F7C[3] = {
    { 0, 32, func_dryfield_trailer_coach_80181D88, { .model = NULL } },
    { 0, 32, func_dryfield_trailer_coach_801824E8, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_trailer_coach_80184FA0[4] = {
    { 5102, func_dryfield_trailer_coach_80182580 },
    { 5105, func_dryfield_trailer_coach_80182578 },
    { 5104, func_dryfield_trailer_coach_801825A8 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_trailer_coach_80184FC0[2] = {
    { 0, 32, func_dryfield_trailer_coach_801827F8, { .model = NULL } },
    { 0, 32, func_dryfield_trailer_coach_801822F4, { .model = NULL } },
};

GpXformArg D_dryfield_trailer_coach_80184FD8 = { { 4870, 0, -900, 0 }, { 0, -2560, 0, 0 } };

GpXformArg D_dryfield_trailer_coach_80184FF0 = { { 5270, 0, -500, 0 }, { 0, -2560, 0, 0 } };

GpXformArg D_dryfield_trailer_coach_80185008 = { { 4870, 0, -900, 0 }, { 0, 2560, 0, 0 } };

// Retained parameter record; layout follows the adjacent script arguments.
GpXformArg D_dryfield_trailer_coach_80185020 = { { 4400, 128, -2400, 0 }, { 0, 512, 0, 0 } };

GpAnimArg D_dryfield_trailer_coach_80185038 = { { .index = 1 }, 1, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_8018504C = { { .index = 6 }, 9, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185060 = { { .index = 1 }, 1, 0, 0, 0 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_dryfield_trailer_coach_80185074 = { { .index = 1 }, 47, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_80185088 = { { .index = 1 }, 58, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_8018509C = { { .index = 1 }, 59, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_801850B0 = { { .index = 1 }, 60, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_801850C4 = { { .index = 1 }, 61, 0, 0, 1 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_dryfield_trailer_coach_801850D8 = { { .index = 1 }, 47, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_801850EC = { { .index = 1 }, 47, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_80185100 = { { .index = 1 }, 48, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_80185114 = { { .index = 1 }, 49, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_80185128 = { { .index = 1 }, 50, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_8018513C = { { .index = 1 }, 51, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_80185150 = { { .index = 1 }, 52, 0, 0, 1 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_dryfield_trailer_coach_80185164 = { { .index = 1 }, 53, 0, 0, 1 };

GpAnimArg D_dryfield_trailer_coach_80185178 = { { .index = 1 }, 54, 0, 0, 0 };

GpCmdArg D_dryfield_trailer_coach_8018518C = { { .loc = { 2, 27 } }, 0 };

GpCmdArg D_dryfield_trailer_coach_80185190 = { { .loc = { 2, 27 } }, 1 };

GpCmdArg D_dryfield_trailer_coach_80185194 = { { .loc = { 2, 27 } }, 2 };

GpCmdArg D_dryfield_trailer_coach_80185198 = { { .loc = { 2, 27 } }, 3 };

GpAnimArg D_dryfield_trailer_coach_8018519C = { 0 };

GpAnimArg D_dryfield_trailer_coach_801851B0 = { { .index = 0 }, 1, 0, 0, 0 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_dryfield_trailer_coach_801851C4 = { { .index = 0 }, 2, 0, 0, 0 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_dryfield_trailer_coach_801851D8 = { { .index = 0 }, 3, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_801851EC = { { .index = 0 }, 4, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185200 = { { .index = 0 }, 5, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185214 = { { .index = 0 }, 6, 1, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185228 = { { .index = 0 }, 7, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_8018523C = { { .index = 0 }, 8, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185250 = { { .index = 0 }, 9, 1, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185264 = { { .index = 0 }, 10, 1, 0, 0 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_dryfield_trailer_coach_80185278 = { { .index = 0 }, 8, 1, 0, 0 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_dryfield_trailer_coach_8018528C = { { .index = 1 }, 0, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_801852A0 = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_801852B4 = { { .index = 1 }, 2, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_801852C8 = { { .index = 1 }, 3, 1, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_801852DC = { { .index = 1 }, 4, 1, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_801852F0 = { { .index = 1 }, 5, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185304 = { { .index = 1 }, 6, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185318 = { { .index = 1 }, 7, 1, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_8018532C = { { .index = 1 }, 2, 1, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185340 = { { .index = 1 }, 62, 0, 0, 0 };

GpAnimArg D_dryfield_trailer_coach_80185354 = { { .index = 1 }, 63, 0, 0, 0 };

DryfieldTrailerCoachAnimStorage5368 D_dryfield_trailer_coach_80185368 = { .data = { { &D_actor_420700_80133A7C, &D_actor_420700_80133CDC, &D_actor_420700_80134148, &D_actor_420700_80134D38, &D_actor_420700_80134F8C, &D_actor_420700_80135224, &D_actor_420700_801353F4, &D_actor_420700_80135BF8, NULL, NULL, NULL, &D_actor_420700_80132C00, &D_actor_420700_801331C8, &D_actor_420700_8013346C, &D_actor_420700_80133698, &D_dryfield_trailer_coach_80184F54, &D_dryfield_trailer_coach_80184C28 }, { { .words = D_dryfield_trailer_coach_80185368.words }, 32 }, { 143, 101, 138, 237, 130, 201, 130, 194, 130, 162, 130, 196, 0, 0, 0, 0 }, { 131, 86, 131, 70, 131, 139, 131, 94, 129, 91, 130, 201, 130, 194, 130, 162, 130, 196, 0, 0 }, { 145, 188, 130, 201, 137, 189, 130, 169, 129, 72, 0, 0 }, { D_dryfield_trailer_coach_80185368.data.text0, D_dryfield_trailer_coach_80185368.data.text1, D_dryfield_trailer_coach_80185368.data.text0, D_dryfield_trailer_coach_80185368.data.text2 } } };

GpEvsCmd D_dryfield_trailer_coach_801853F4[58] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018504C }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_trailer_coach_80185368.data.copy }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_trailer_coach_80184FD8 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018504C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185088 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018509C }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018513C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185214 }, { .value = 0 } },
    { 4, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185228 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018523C }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185264 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_801850B0 }, { .value = 0 } },
    { 4, { .value = 85 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018513C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851EC }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_dryfield_trailer_coach_80185190 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_801850C4 }, { .value = 0 } },
    { 4, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018513C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_dryfield_trailer_coach_80185194 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_dryfield_trailer_coach_8018518C }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_trailer_coach_80185008 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_80185964[17] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_trailer_coach_80185008 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_80185AFC[14] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185214 }, { .value = 0 } },
    { 4, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185228 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018523C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_trailer_coach_80182850 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_80185C4C[11] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackResult = func_800D4D2C }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185250 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851EC }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_80185D54[98] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_trailer_coach_80185368.data.copy }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185060 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 75 }, { .value = 75 }, { .value = 75 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_trailer_coach_80184FD8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185114 }, { .value = 0 } },
    { 4, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018513C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_801850EC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_trailer_coach_80184FD8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018513C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x521B0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185150 }, { .value = 0 } },
    { 4, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018513C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852A0 }, { .value = 0 } },
    { 4, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852C8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185114 }, { .value = 0 } },
    { 4, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018513C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185100 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018513C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852DC }, { .value = 0 } },
    { 4, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185318 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185114 }, { .value = 0 } },
    { 4, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018513C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852A0 }, { .value = 0 } },
    { 4, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852B4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185318 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x521B000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185250 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852F0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185128 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_trailer_coach_80184FF0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185178 }, { .value = 0 } },
    { 4, { .value = 89 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_trailer_coach_80185008 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851EC }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_80186684[17] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_trailer_coach_80185008 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_8018681C[25] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_trailer_coach_80185368.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_8018504C }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851B0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_dryfield_trailer_coach_80185198 }, { .value = 0 } },
    { 15, { .value = 0x521B000E }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185304 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_dryfield_trailer_coach_8018518C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851EC }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_80186A74[15] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_80186BDC[14] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_trailer_coach_80185368.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852A0 }, { .value = 0 } },
    { 4, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185250 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851EC }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_80186D2C[35] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 27 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_trailer_coach_80185368.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852A0 }, { .value = 0 } },
    { 4, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185250 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185354 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851B0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185318 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852A0 }, { .value = 0 } },
    { 4, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185250 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185340 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851B0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852A0 }, { .value = 0 } },
    { 4, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185250 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851EC }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_trailer_coach_80187074[14] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 28 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_trailer_coach_80185368.data.copy }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_trailer_coach_80185038 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801852A0 }, { .value = 0 } },
    { 4, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_8018532C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185250 }, { .value = 0 } },
    { 4, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_801851EC }, { .value = 0 } },
    { 4, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_trailer_coach_80185200 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_dryfield_trailer_coach_801871C4 = { 163, -1532, -766, 0 };

GpRoomObjRec D_dryfield_trailer_coach_801871CC[1] = {
    { D_dryfield_trailer_coach_801876B4, D_dryfield_trailer_coach_80189254, D_dryfield_trailer_coach_80189384, NULL },
};

GpRoomCoordRec D_dryfield_trailer_coach_801871DC[1] = {
    { D_dryfield_trailer_coach_80189B94, D_dryfield_trailer_coach_80189BAC },
};

u8 * D_dryfield_trailer_coach_801871E4[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_trailer_coach_801871E8[1] = {
    { { .bytes = { 11, 0 } } },
};

GpWarpRec D_dryfield_trailer_coach_801871EC[2] = {
    { { .words = { 2048, 1579, 0, -800 } }, { 0, 0, 0, 0 }, { .words = { 2048, 1579, 0, -800 } }, { 0, 0, 0, 0 }, 0x521B0002, 0x521B0001, 0, 2, 0, 471 },
    { { .words = { 2048, 1579, 0, -800 } }, { 0, 0, 0, 0 }, { .words = { 2048, 1579, 0, -800 } }, { 0, 0, 0, 0 }, 0x521B0002, 0x521B0001, 0, 5, 0, 471 },
};

SVECTOR D_dryfield_trailer_coach_8018725C[10] = {
    { 0, -4096, 0, 0 },
    { 2896, 0, 2896, 0 },
    { 0, 0, 4096, 0 },
    { -4096, 0, 0, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, 4096, 0, 0 },
    { -2896, 0, -2896, 0 },
    { -2896, 0, 2896, 0 },
    { -908, 0, -3994, 0 },
};

SVECTOR D_dryfield_trailer_coach_801872AC[65] = {
    { 2600, -900, -3350, 0 },
    { 4500, -900, -2300, 0 },
    { 8350, -900, -2300, 0 },
    { 8350, -900, -3350, 0 },
    { 4500, 100, -2300, 0 },
    { 3600, -900, -1400, 0 },
    { 3600, 100, -1400, 0 },
    { 2600, -900, -1400, 0 },
    { 2600, 100, -1400, 0 },
    { 8350, 100, -2300, 0 },
    { 2600, 100, -3350, 0 },
    { 8350, 100, -3350, 0 },
    { -100, -2100, -2650, 0 },
    { 1250, -2100, -2650, 0 },
    { 1250, -2100, -3350, 0 },
    { -100, -2100, -3350, 0 },
    { 1250, 100, -2650, 0 },
    { -100, 100, -2650, 0 },
    { 1250, 100, -3350, 0 },
    { 710, -1300, -210, 0 },
    { 710, 100, -210, 0 },
    { -200, 100, -210, 0 },
    { -200, -1300, -210, 0 },
    { 0x2710, -2300, -3250, 0 },
    { 0x2710, 100, -3250, 0 },
    { 0x2710, 100, 0, 0 },
    { 0x2710, -2300, 0, 0 },
    { 0, 100, 0, 0 },
    { 0, -2300, 0, 0 },
    { 0, -2300, -3250, 0 },
    { 1000, -1300, -500, 0 },
    { 1000, 100, -500, 0 },
    { 0, 100, -3250, 0 },
    { -200, 0, 200, 0 },
    { 0x27D8, 0, 200, 0 },
    { 0x27D8, 0, -3450, 0 },
    { -200, 0, -3450, 0 },
    { 9350, -500, -1000, 0 },
    { 0x2774, -500, -1000, 0 },
    { 0x2774, -500, -3350, 0 },
    { 9350, -500, -3350, 0 },
    { 9350, 100, -1000, 0 },
    { 9350, 100, -3350, 0 },
    { 9350, -2000, 100, 0 },
    { 0x2774, -2000, 100, 0 },
    { 0x2774, -2000, -1000, 0 },
    { 9350, -2000, -1000, 0 },
    { 0x2774, 100, -1000, 0 },
    { 9350, 100, 100, 0 },
    { 7000, -1050, -1950, 0 },
    { 7450, -1050, -1950, 0 },
    { 7850, -1050, -2350, 0 },
    { 6600, -1050, -2350, 0 },
    { 7450, 100, -1950, 0 },
    { 7000, 100, -1950, 0 },
    { 6600, 100, -2350, 0 },
    { 7850, 100, -2350, 0 },
    { 4550, -1300, -500, 0 },
    { 4550, 100, -500, 0 },
    { 8050, 100, -850, 0 },
    { 8050, -1300, -850, 0 },
    { 8050, -1300, 100, 0 },
    { 8050, 100, 100, 0 },
    { 6090, 100, -850, 0 },
    { 6090, -1300, -850, 0 },
};

GpGridFace D_dryfield_trailer_coach_801874B4[32] = {
    { { 1, 2, 0, 3 }, 0, 5 },
    { { 1, 5, 4, 6 }, 1, 5 },
    { { 5, 7, 6, 8 }, 2, 5 },
    { { 2, 1, 9, 4 }, 2, 5 },
    { { 7, 0, 8, 10 }, 3, 5 },
    { { 3, 2, 11, 9 }, 4, 5 },
    { { 13, 14, 12, 15 }, 0, 5 },
    { { 13, 12, 16, 17 }, 2, 5 },
    { { 14, 13, 18, 16 }, 4, 5 },
    { { 20, 21, 19, 22 }, 5, 5 },
    { { 24, 25, 23, 26 }, 3, 5 },
    { { 25, 27, 26, 28 }, 5, 5 },
    { { 28, 29, 26, 23 }, 6, 5 },
    { { 19, 30, 20, 31 }, 7, 5 },
    { { 27, 32, 28, 29 }, 4, 5 },
    { { 32, 24, 29, 23 }, 2, 5 },
    { { 34, 35, 33, 36 }, 0, 6 },
    { { 38, 39, 37, 40 }, 0, 5 },
    { { 37, 40, 41, 42 }, 3, 5 },
    { { 44, 45, 43, 46 }, 0, 5 },
    { { 46, 45, 41, 47 }, 5, 5 },
    { { 43, 46, 48, 41 }, 3, 5 },
    { { 50, 51, 49, 52 }, 0, 5 },
    { { 50, 49, 53, 54 }, 2, 5 },
    { { 49, 52, 54, 55 }, 8, 5 },
    { { 51, 50, 56, 53 }, 1, 5 },
    { { 52, 51, 55, 56 }, 5, 5 },
    { { 58, 31, 57, 30 }, 5, 5 },
    { { 60, 61, 59, 62 }, 4, 5 },
    { { 63, 64, 59, 60 }, 5, 5 },
    { { 0, 7, 1, 5 }, 0, 5 },
    { { 57, 64, 58, 63 }, 9, 5 },
};

s16 D_dryfield_trailer_coach_80187634[17] = {
    0,
    1,
    2,
    4,
    6,
    7,
    8,
    9,
    11,
    12,
    13,
    14,
    15,
    16,
    27,
    30,
    -1,
};

s16 D_dryfield_trailer_coach_80187658[20] = {
    0,
    1,
    2,
    3,
    5,
    11,
    12,
    15,
    16,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    -1,
};

s16 D_dryfield_trailer_coach_80187680[20] = {
    0,
    3,
    5,
    10,
    11,
    12,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    25,
    26,
    28,
    29,
    -1,
};

s16 * D_dryfield_trailer_coach_801876A8[3] = {
    D_dryfield_trailer_coach_80187634,
    D_dryfield_trailer_coach_80187658,
    D_dryfield_trailer_coach_80187680,
};

GpGridParams D_dryfield_trailer_coach_801876B4[1] = {
    { NULL, D_dryfield_trailer_coach_8018725C, D_dryfield_trailer_coach_801872AC, D_dryfield_trailer_coach_801874B4, D_dryfield_trailer_coach_801876A8, 200, 3450, 3, 1, 4000, 32 },
};

GpAreaTmdRec D_dryfield_trailer_coach_801876D8[2] = {
    { 106, 207, 3, 0, { 0, 0 }, D_8013EF68 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_trailer_coach_801876F0[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017BA14, D_dryfield_trailer_coach_801876D8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

GpViewRec D_dryfield_trailer_coach_80187758[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5000, 0x2710, 1625 } }, 230 },
    { { { { 373, 0, 4078 }, { 877, 4000, -80 }, { -3983, 881, 364 } }, { -5900, 1690, 2010 } }, 230 },
    { { { { 879, 0, -4000 }, { -892, 3992, -196 }, { 3899, 913, 857 } }, { -1154, 1800, 2260 } }, 230 },
    { { { { 595, 0, -4052 }, { -1245, 3897, -183 }, { 3856, 1258, 567 } }, { -6104, 1950, 2060 } }, 230 },
    { { { { -18, 0, -4095 }, { 0, 4096, 0 }, { 4095, 0, -18 } }, { -8071, 1241, 2144 } }, 230 },
    { { { { 2991, 0, -2797 }, { 212, 4084, 226 }, { 2789, -310, 2982 } }, { -5571, 801, 2134 } }, 230 },
    { { { { -3734, 0, -1681 }, { -909, 3445, 2019 }, { 1414, 2214, -3141 } }, { -6604, 1680, 1710 } }, 230 },
    { { { { -450, 0, 4071 }, { 108, 4094, 12 }, { -4069, 109, -450 } }, { -6545, 1155, 1159 } }, 230 },
    { { { { -3740, 0, 1669 }, { 740, 3671, 1658 }, { -1496, 1816, -3352 } }, { -4627, 1470, 492 } }, 221 },
    { { { { 1613, 0, 3764 }, { 1597, 3709, -684 }, { -3409, 1737, 1460 } }, { -525, 1650, 866 } }, 230 },
    { { { { 3255, 0, -2485 }, { 160, 4087, 210 }, { 2480, -265, 3248 } }, { -3895, 1115, 2089 } }, 289 },
};

GpSprtCmd D_dryfield_trailer_coach_801878E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_trailer_coach_801878F4[101] = {
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -72, 24, 750, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 40 } }, -72, 64, 750, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -64, 104, 750, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 48, 750, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 48, 585, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 48, 591, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 547, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 48, 591, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 48, 591, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 48, 591, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 48, 591, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 48, 582, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 48, 591, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 48, 551, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 56, 468, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 56, 492, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 56, 549, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 56, 522, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 56, 549, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 56, 549, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 56, 549, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 56, 549, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 56, 549, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 56, 562, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 56, 554, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 64, 443, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 64, 461, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 64, 484, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 64, 512, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 64, 512, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 64, 512, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 64, 512, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 64, 512, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 64, 512, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 64, 512, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 64, 533, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 72, 480, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 72, 453, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 72, 476, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 72, 480, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 72, 480, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 72, 479, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 72, 479, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 72, 479, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 72, 479, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 72, 490, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 80, 401, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 80, 401, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 80, 409, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 80, 451, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 80, 451, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 80, 451, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 80, 452, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 80, 452, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 80, 452, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 80, 467, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 88, 373, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 88, 373, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 88, 379, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 88, 386, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 88, 407, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 88, 407, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 88, 426, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 88, 426, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 88, 434, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 96, 353, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 96, 359, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 96, 366, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 96, 382, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 96, 385, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 96, 403, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 96, 404, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 96, 404, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 416, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 104, 347, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 104, 364, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 104, 365, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 104, 366, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 104, 384, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 104, 384, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 104, 384, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 104, 390, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 112, 347, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 112, 353, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 112, 351, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 112, 365, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 112, 365, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 112, 365, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 112, 365, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 112, 375, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -120, -24, 700, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -64, 48, 625, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 136, 16 } }, -136, 32, 625, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, 40, 675, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 40, 750, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 40, 725, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -120, 16, 700, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 16, 725, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -8, 1150, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, -8, 1150, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, 24, 1100, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_trailer_coach_801880D8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 94, 0, 0, { 2, 0 } },
    { 98, 3, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_trailer_coach_80188100[160] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 40, -24, 1562, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 0, 465, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 0, 481, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 0, 489, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 490, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 0, 493, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 0, 499, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 0, 502, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 8, 988, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 8, 990, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 8, 451, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 8, 440, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 8, 446, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 8, 453, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 8, 459, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 8, 466, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 8, 473, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 8, 475, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 16, 924, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 16, 921, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 16, 453, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 16, 442, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 16, 435, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 16, 427, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 16, 431, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 16, 437, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 16, 443, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 16, 445, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 16, 462, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 16, 449, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 24, 456, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 24, 445, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 24, 438, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 24, 430, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 24, 423, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 24, 413, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 24, 401, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 24, 400, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 24, 404, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 24, 403, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 32, 458, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 32, 448, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 32, 440, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 32, 433, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 32, 425, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 32, 419, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 32, 402, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 390, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 32, 380, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 32, 379, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 40, 461, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 40, 451, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 40, 443, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 40, 436, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 40, 428, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 40, 416, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 40, 404, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 40, 394, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 40, 382, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 40, 379, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 48, 464, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 48, 454, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 48, 446, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 48, 438, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 48, 431, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 48, 419, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 48, 405, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 48, 394, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 48, 383, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 48, 381, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 56, 467, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 56, 457, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 56, 449, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 56, 441, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 56, 433, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 56, 420, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 407, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 56, 396, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 56, 384, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 382, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 64, 495, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 64, 499, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 64, 484, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 64, 479, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 64, 465, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 64, 423, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 64, 413, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 64, 397, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 64, 386, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 64, 384, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 72, 484, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 72, 468, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 72, 467, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 455, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 72, 452, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 72, 451, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 451, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 399, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 392, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 72, 468, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 80, 482, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 80, 467, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 80, 450, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 80, 441, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 80, 441, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 80, 436, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 434, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 438, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 80, 445, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 80, 473, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 88, 466, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 88, 466, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 88, 454, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 88, 439, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 88, 423, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 88, 417, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 88, 417, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 88, 438, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 461, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 88, 450, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 96, 442, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 96, 442, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 96, 442, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 96, 440, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 96, 425, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 96, 412, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 96, 425, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 96, 442, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 442, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 96, 442, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 104, 420, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 104, 420, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 104, 420, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 104, 420, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 420, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 104, 413, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 104, 420, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 104, 420, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 420, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 104, 420, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 433, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 112, 430, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 112, 426, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 112, 423, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 112, 409, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 406, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 112, 403, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 400, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 400, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 400, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -48, 96, 375, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 48, 650, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 48, 450, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -56, 72, 400, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -40, 56, 450, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 0, 56, 500, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 32, 24, 600, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, 0, 64, 450, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, 0, 80, 400, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, 0, 104, 375, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_trailer_coach_80188D80[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 159, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_trailer_coach_80188DA0[41] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -64, 500, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 64, 450, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 88, 362, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 88, 361, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 88, 375, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 88, 394, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 96, 673, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 96, 343, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 96, 360, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 96, 382, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 96, 402, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 104, 332, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 104, 334, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 104, 366, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 104, 389, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 112, 610, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 112, 317, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 112, 340, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 112, 373, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 112, 397, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, 96, 475, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -152, 96, 475, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 80, 550, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 80, 487, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 64, 450, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -8, 450, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -96, 458, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 144, 8, 400, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, 48, 550, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 80, 56, 450, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, 56, 104, 375, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, 64, 80, 400, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 80, 550, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 56, 450, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -112, 56, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 72, 575, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, 24, 450, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 48, 450, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -104, 48, 587, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 128, 8, 450, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 16, 80, 400, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_trailer_coach_801890D4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 40, 0, 0, { 1, 0 } },
    { 40, 1, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_trailer_coach_801890F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_trailer_coach_80189104[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_trailer_coach_80189114[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_trailer_coach_80189124[5] = {
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 120, 8, 312, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 128, 8, 312, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 136, 8, 312, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 144, 8, 312, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 152, 8, 312, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_trailer_coach_80189188[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_trailer_coach_801891A0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_trailer_coach_801891B0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_trailer_coach_801891C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_trailer_coach_801891D0[11] = {
    { { .empty = D_dryfield_trailer_coach_801878E4 }, D_dryfield_trailer_coach_801878E4, NULL },
    { { .elements = D_dryfield_trailer_coach_801878F4 }, D_dryfield_trailer_coach_801880D8, NULL },
    { { .elements = D_dryfield_trailer_coach_80188100 }, D_dryfield_trailer_coach_80188D80, NULL },
    { { .elements = D_dryfield_trailer_coach_80188DA0 }, D_dryfield_trailer_coach_801890D4, NULL },
    { { .empty = D_dryfield_trailer_coach_801890F4 }, D_dryfield_trailer_coach_801890F4, NULL },
    { { .empty = D_dryfield_trailer_coach_80189104 }, D_dryfield_trailer_coach_80189104, NULL },
    { { .empty = D_dryfield_trailer_coach_80189114 }, D_dryfield_trailer_coach_80189114, NULL },
    { { .elements = D_dryfield_trailer_coach_80189124 }, D_dryfield_trailer_coach_80189188, NULL },
    { { .empty = D_dryfield_trailer_coach_801891A0 }, D_dryfield_trailer_coach_801891A0, NULL },
    { { .empty = D_dryfield_trailer_coach_801891B0 }, D_dryfield_trailer_coach_801891B0, NULL },
    { { .empty = D_dryfield_trailer_coach_801891C0 }, D_dryfield_trailer_coach_801891C0, NULL },
};

GpObj4C D_dryfield_trailer_coach_80189254[4] = {
    { NULL, NULL, NULL, { 3199, -1168, -737, 0 }, { { 13, -1904, -1570, 0 }, { -14, -1904, 1569, 0 }, { 13, 1904, -1570, 0 }, { -14, 1904, 1569, 0 } }, { 4110, 0, 35, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 3246, -1120, -801, 0 }, { { 16, -1888, 1619, 0 }, { -16, -1888, -1619, 0 }, { 16, 1888, 1619, 0 }, { -16, 1888, -1619, 0 } }, { -4096, 0, 39, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 7918, -1376, -1538, 0 }, { { -2, -2112, 1038, 0 }, { -1, -2112, -1043, 0 }, { -2, 2112, 1038, 0 }, { -1, 2112, -1043, 0 } }, { -4106, 0, -4, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 7806, -1376, -1538, 0 }, { { -7, -2112, -1048, 0 }, { -8, -2112, 1031, 0 }, { -7, 2112, -1048, 0 }, { -8, 2112, 1031, 0 } }, { 4097, 0, 1, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 3, 129, 0 },
};

GpObj4C D_dryfield_trailer_coach_80189384[12] = {
    { NULL, NULL, NULL, { 1536, -48, -784, 0 }, { { -416, 0, -272, 0 }, { 416, 0, -272, 0 }, { -416, 0, 272, 0 }, { 416, 0, 272, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 496, 0, 26, 18, 2, 0 },
    { NULL, NULL, NULL, { 3631, -64, -2321, 0 }, { { -656, 0, 1422, 0 }, { 158, 0, -2906, 0 }, { 323, 0, 1403, 0 }, { 1329, 0, 147, 0 } }, { 0, 4120, 0, 0 }, { 0, 0, -4096, 0 }, 2907, 2, 3, 255, 4, 0 },
    { NULL, NULL, NULL, { 160, -64, -656, 0 }, { { -416, 0, -448, 0 }, { 416, 0, -448, 0 }, { -416, 0, 448, 0 }, { 416, 0, 448, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 610, 2, 14, 255, 2, 0 },
    { NULL, NULL, NULL, { 128, -64, -1664, 0 }, { { -416, 0, -544, 0 }, { 416, 0, -544, 0 }, { -416, 0, 544, 0 }, { 416, 0, 544, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 683, 2, 15, 0, 2, 0 },
    { NULL, NULL, NULL, { 1856, -64, -3040, 0 }, { { -768, 0, -272, 0 }, { 768, 0, -272, 0 }, { -768, 0, 272, 0 }, { 768, 0, 272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 814, 2, 18, 0, 2, 0 },
    { NULL, NULL, NULL, { 9248, -64, -480, 0 }, { { -416, 0, -544, 0 }, { 416, 0, -544, 0 }, { -416, 0, 544, 0 }, { 416, 0, 544, 0 } }, { 0, 4104, 0, 0 }, { -4091, 0, 201, 0 }, 683, 2, 19, 0, 2, 0 },
    { NULL, NULL, NULL, { 9184, -64, -2224, 0 }, { { -416, 0, -1136, 0 }, { 416, 0, -1136, 0 }, { -416, 0, 1136, 0 }, { 416, 0, 1136, 0 } }, { 0, 4100, 0, 0 }, { -4091, 0, 201, 0 }, 1207, 2, 20, 0, 2, 0 },
    { NULL, NULL, NULL, { 6640, -64, -1136, 0 }, { { -1456, 0, -320, 0 }, { 1456, 0, -320, 0 }, { -1456, 0, 320, 0 }, { 1456, 0, 320, 0 } }, { 0, 4096, 0, 0 }, { -201, 0, -4091, 0 }, 1487, 2, 21, 0, 2, 0 },
    { NULL, NULL, NULL, { 6800, -64, -2080, 0 }, { { -1600, 0, -320, 0 }, { 1600, 0, -320, 0 }, { -1600, 0, 320, 0 }, { 1600, 0, 320, 0 } }, { 0, 4095, 0, 0 }, { -201, 0, 4090, 0 }, 1629, 2, 22, 0, 2, 0 },
    { NULL, NULL, NULL, { 4096, -64, -256, 0 }, { { -1424, 0, -832, 0 }, { 944, 0, -832, 0 }, { -1424, 0, 320, 0 }, { 944, 0, 320, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1649, 2, 23, 0, 4, 0 },
    { NULL, NULL, NULL, { 3296, -64, -2464, 0 }, { { -1440, 0, -848, 0 }, { 352, 0, -848, 0 }, { -1440, 0, 1680, 0 }, { 352, 0, 1680, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, -4096, 0 }, 2202, 2, 2, 0, 4, 0 },
    { NULL, NULL, NULL, { 8624, -64, -2496, 0 }, { { -336, 0, -736, 0 }, { 336, 0, -736, 0 }, { -336, 0, 736, 0 }, { 336, 0, 736, 0 } }, { 0, 4102, 0, 0 }, { 4016, 0, 798, 0 }, 807, 2, 22, 0, 130, 0 },
};

GpPointLight D_dryfield_trailer_coach_80189714[12] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2807, -936, -1738 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3280, 3076, 2870, { 0, 0 } }, 1587, 2185 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8192, -2555, -1604 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2460, 2460, 2460, { 0, 0 } }, 1120, 1672 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6380, -2555, -1625 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2460, 2460, 2460, { 0, 0 } }, 1057, 1718 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2944, -1003, -1164 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3278, 3278, 3278, { 0, 0 } }, 1839, 2505 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 490, -1975, -479 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3072, { 0, 0 } }, 1107, 1294 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 169, -2006, -1625 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3076, { 0, 0 } }, 1112, 1374 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -911, -2035, -2681 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3072, { 0, 0 } }, 1288, 1572 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1618, -1020, -130 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2870, 2870, 2870, { 0, 0 } }, 2591, 3272 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3750, -1440, -3 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1229, 1229, 1229, { 0, 0 } }, 2029, 2673 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6865, -1440, 10 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1229, 1229, 1229, { 0, 0 } }, 1935, 2601 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6885, -1440, -2420 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2666, 2666, 2663, { 0, 0 } }, 2646, 3283 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3665, -1440, -2318 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3280, 3280, 3280, { 0, 0 } }, 2164, 2832 },
};

GpRoomCoordSet D_dryfield_trailer_coach_80189B94[1] = {
    { 0, NULL, 12, D_dryfield_trailer_coach_80189714, 0, NULL },
};

GpRoomBoundVec D_dryfield_trailer_coach_80189BAC[12] = {
    { 11, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 1025, 1025, 1025, 1025 },
    { 1025, 1025, 1025, 1025 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 1025, 1025, 1025, 1025 },
    { 1025, 1025, 1025, 1025 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

s32 D_dryfield_trailer_coach_80189C0C[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

GpRoomParamRec D_dryfield_trailer_coach_80189C18[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_trailer_coach_80189C20[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_trailer_coach_80189C28[1] = {
    { 0, 0, 1, 0, D_dryfield_trailer_coach_80189C0C },
};

GpRoomParamRec * D_dryfield_trailer_coach_80189C30[8] = {
    D_dryfield_trailer_coach_80189C18,
    D_dryfield_trailer_coach_80189C18,
    D_dryfield_trailer_coach_80189C18,
    D_dryfield_trailer_coach_80189C18,
    D_dryfield_trailer_coach_80189C18,
    D_dryfield_trailer_coach_80189C20,
    D_dryfield_trailer_coach_80189C28,
    D_dryfield_trailer_coach_80189C18,
};

GpAreaApplyRec D_dryfield_trailer_coach_80189C50[15] = {
    { 2, 5, 7, 33 },
    { 2, 11, 2, 17 },
    { 2, 11, 7, 33 },
    { 2, 12, 2, 17 },
    { 2, 12, 7, 33 },
    { 2, 16, 2, 17 },
    { 2, 16, 7, 33 },
    { 2, 19, 2, 1 },
    { 2, 20, 2, 1 },
    { 2, 21, 2, 0 },
    { 2, 22, 3, 1 },
    { 2, 24, 3, 1 },
    { 2, 25, 2, 0 },
    { 2, 26, 2, 0 },
    { 255, 0, 0, 0 },
};

s32 D_dryfield_trailer_coach_80189C8C = 0;

GpItemMap * D_dryfield_trailer_coach_80189C90 = NULL;

// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    Task* value;
    u8    retained[4];
} DryfieldTrailerCoachStorage9C94;
STATIC_ASSERT_SIZEOF(DryfieldTrailerCoachStorage9C94, 8);

DryfieldTrailerCoachStorage9C94 D_dryfield_trailer_coach_80189C94 = { 0 };

RoomCutsceneRec D_dryfield_trailer_coach_80189C9C = { 0 };

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
extern u8 D_dryfield_trailer_coach_80183D78[];

extern u8 D_dryfield_trailer_coach_80183D64[];

extern UiObjectDesc D_dryfield_trailer_coach_80183F30;

extern u8 D_dryfield_trailer_coach_80183D74[];

extern UiObjectDesc D_dryfield_trailer_coach_80183EDC;

extern RoomShopTier D_dryfield_trailer_coach_80183CA8[13];

/// Messages and labels of the shop's panels.
extern u8 D_dryfield_trailer_coach_80183D48[];

extern u8 D_dryfield_trailer_coach_80183D5C[];

extern u8 D_dryfield_trailer_coach_80183DB4[];

extern u8 D_dryfield_trailer_coach_80183DBC[];

extern u8 D_dryfield_trailer_coach_80183DC8[];

extern u8 D_dryfield_trailer_coach_80183DD0[];

extern u8 D_dryfield_trailer_coach_80183DD8[];

extern u8 D_dryfield_trailer_coach_80183DEC[];

extern u8 D_dryfield_trailer_coach_80183DFC[];

extern u8 D_dryfield_trailer_coach_80183E1C[];

extern u8 D_dryfield_trailer_coach_80183E28[];

/// Row handlers, lists and panel descriptors of the shop's panels.
extern UiListItemFunc D_dryfield_trailer_coach_80183E30[];

extern UiList D_dryfield_trailer_coach_80183E38;

extern UiList D_dryfield_trailer_coach_80183E64;

extern UiObjectDesc D_dryfield_trailer_coach_80183EA4;

extern UiObjectDesc D_dryfield_trailer_coach_80183EC0;

extern UiObjectDesc D_dryfield_trailer_coach_80183EF8;

extern UiObjectDesc D_dryfield_trailer_coach_80183F4C;

extern UiObjectDesc D_dryfield_trailer_coach_80183F68;

/// Work pair of the charge panel `func_dryfield_trailer_coach_8017F398`: the
/// animated quantity in 24.8 fixed point, and the item map of the slot being
/// charged.
extern s32 D_dryfield_trailer_coach_80189C8C;

extern GpItemMap* D_dryfield_trailer_coach_80189C90;

/// Descriptor of the panel `func_dryfield_trailer_coach_8017FE98` opens.
extern UiObjectDesc D_dryfield_trailer_coach_80183E88;

extern u8 D_dryfield_trailer_coach_801845A0[];

extern u8 D_dryfield_trailer_coach_801845A8[];

extern u8 D_dryfield_trailer_coach_801845AC[];

extern u8 D_dryfield_trailer_coach_801845B4[];

extern u8 D_dryfield_trailer_coach_801845C0[];

extern u8 D_dryfield_trailer_coach_801845D0[];

extern u8 D_dryfield_trailer_coach_801845D8[];

extern u8 D_dryfield_trailer_coach_801845E0[];

extern u8 D_dryfield_trailer_coach_801845E8[];

extern u8 D_dryfield_trailer_coach_801845F0[];

extern u8 D_dryfield_trailer_coach_801845FC[];

extern u8 D_dryfield_trailer_coach_80184628[];

extern u8 D_dryfield_trailer_coach_8018464C[];

extern u8 D_dryfield_trailer_coach_8018467C[];

extern u8 D_dryfield_trailer_coach_801846B0[];

extern u8 D_dryfield_trailer_coach_801846E4[];

extern u8 D_dryfield_trailer_coach_8018471C[];

extern u8 D_dryfield_trailer_coach_80184750[];

extern u8 D_dryfield_trailer_coach_80184788[];

static const char D_dryfield_trailer_coach_8017D770[];

extern UiObjectDesc D_800611E4;

/// Lists of the usage panel and of the play-data menu, and the descriptor of
/// the frame the usage panel spawns.
extern UiList D_dryfield_trailer_coach_801847EC;

extern UiList D_dryfield_trailer_coach_80184874;

extern UiObjectDesc D_dryfield_trailer_coach_80184810;

/// List of the menu panel `func_dryfield_trailer_coach_8018181C` draws.
extern UiList D_dryfield_trailer_coach_801847C4;

/// Texts of the four menu rows below, and the panels two of them open.
extern u8 D_dryfield_trailer_coach_80184578[];

extern u8 D_dryfield_trailer_coach_80184580[];

extern u8 D_dryfield_trailer_coach_8018458C[];

extern u8 D_dryfield_trailer_coach_80184598[];

extern UiObjectDesc D_dryfield_trailer_coach_8018482C;

extern UiObjectDesc D_dryfield_trailer_coach_80184848;

/// The scene sub-task while it runs, NULL otherwise.
extern DryfieldTrailerCoachStorage9C94 D_dryfield_trailer_coach_80189C94;

extern GpEvsCmd D_dryfield_trailer_coach_80185AFC[];

extern GpEvsCmd D_dryfield_trailer_coach_80185C4C[];

extern GpEvsCmd D_dryfield_trailer_coach_80185D54[];

extern GpEvsCmd D_dryfield_trailer_coach_80186684[];

extern GpEvsCmd D_dryfield_trailer_coach_8018681C[];

extern GpEvsCmd D_dryfield_trailer_coach_80186A74[];

extern GpEvsCmd D_dryfield_trailer_coach_80186BDC[];

extern GpEvsCmd D_dryfield_trailer_coach_80186D2C[];

extern GpEvsCmd D_dryfield_trailer_coach_80187074[];

extern GpAreaApplyRec D_dryfield_trailer_coach_80189C50[];

/// Second descriptor of the trailer's spawn table (spawned by request 3).
extern TaskDesc D_dryfield_trailer_coach_80184FC0[];

/// The cutscene record this room hands `D_dryfield_trailer_coach_80184F7C`.
extern RoomCutsceneRec D_dryfield_trailer_coach_80189C9C;

static void func_dryfield_trailer_coach_801827D0(Task* arg0);

extern GpMsgEntry D_dryfield_trailer_coach_80184FA0[];

extern GpEvsCmd D_dryfield_trailer_coach_801853F4[];

extern GpEvsCmd D_dryfield_trailer_coach_80185964[];

static void func_dryfield_trailer_coach_80182888(Task* arg0);

static void func_dryfield_trailer_coach_8018291C(Task* task);

extern SVECTOR D_dryfield_trailer_coach_801871C4;

static u16*       func_dryfield_trailer_coach_8017D7F4(s32 mode);
static void       func_dryfield_trailer_coach_8017E2F0(RoomShopList* shop, UiObject* obj, s32 item);
static void       func_dryfield_trailer_coach_8017E43C(RoomShopList* shop, UiObject* obj);
static inline s32 _dryfield_trailer_coachAddItemCount(s32 item, s32 count);
static void       func_dryfield_trailer_coach_80180B94(UiList* list, UiObject* obj);
static void       func_dryfield_trailer_coach_80180E90(UiList* list, UiObject* obj);
static void       func_dryfield_trailer_coach_801816B8(u8* str, s32 decimals);
static u8*        func_dryfield_trailer_coach_80181728(u8* buf, s32 value, s32 decimals);
static void       func_dryfield_trailer_coach_8018190C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6);
static void       func_dryfield_trailer_coach_801826A0(Task* task);
static void       func_dryfield_trailer_coach_80182794(Task* task);
static void       func_dryfield_trailer_coach_801829A8(GpCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);
static void       func_dryfield_trailer_coach_80182EB4(GpCoord* coord, SVECTOR* data, s32 arg2, s32 arg3);

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData[0].state.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_dryfield_trailer_coach_80183E2C`.
static u16* func_dryfield_trailer_coach_8017D7F4(s32 mode)
{
    if (Mc_SaveData[0].state.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183A30;
                    case 1:
                        return D_dryfield_trailer_coach_80183A48;
                    case 2:
                        return D_dryfield_trailer_coach_80183A5C;
                    case 3:
                        return D_dryfield_trailer_coach_80183A64;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183A78;
                    case 1:
                        return D_dryfield_trailer_coach_80183A94;
                    case 2:
                        return D_dryfield_trailer_coach_80183AA4;
                    case 3:
                        return D_dryfield_trailer_coach_80183AB0;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183AC8;
                    case 1:
                        return D_dryfield_trailer_coach_80183AE4;
                    case 2:
                        return D_dryfield_trailer_coach_80183AF8;
                    case 3:
                        return D_dryfield_trailer_coach_80183B00;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183B14;
                    case 1:
                        return D_dryfield_trailer_coach_80183B34;
                    case 2:
                        return D_dryfield_trailer_coach_80183B44;
                    case 3:
                        return D_dryfield_trailer_coach_80183B50;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183978;
                    case 1:
                        return D_dryfield_trailer_coach_80183988;
                    case 2:
                        return D_dryfield_trailer_coach_80183998;
                    case 3:
                        return D_dryfield_trailer_coach_801839A0;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_801839EC;
                    case 1:
                        return D_dryfield_trailer_coach_80183A04;
                    case 2:
                        return D_dryfield_trailer_coach_80183A18;
                    case 3:
                        return D_dryfield_trailer_coach_80183A20;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_801839B0;
                    case 1:
                        return D_dryfield_trailer_coach_801839C0;
                    case 2:
                        return D_dryfield_trailer_coach_801839D0;
                    case 3:
                        return D_dryfield_trailer_coach_801839D8;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183950;
                    case 1:
                        return D_dryfield_trailer_coach_80183958;
                    case 2:
                        return D_dryfield_trailer_coach_80183960;
                    case 3:
                        return D_dryfield_trailer_coach_80183968;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183BFC;
                    case 1:
                        return D_dryfield_trailer_coach_80183C08;
                    case 2:
                        return D_dryfield_trailer_coach_80183C10;
                    case 3:
                        return D_dryfield_trailer_coach_80183C1C;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183C28;
                    case 1:
                        return D_dryfield_trailer_coach_80183C34;
                    case 2:
                        return D_dryfield_trailer_coach_80183C38;
                    case 3:
                        return D_dryfield_trailer_coach_80183C44;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183C50;
                    case 1:
                        return D_dryfield_trailer_coach_80183C5C;
                    case 2:
                        return D_dryfield_trailer_coach_80183C64;
                    case 3:
                        return D_dryfield_trailer_coach_80183C70;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183C7C;
                    case 1:
                        return D_dryfield_trailer_coach_80183C88;
                    case 2:
                        return D_dryfield_trailer_coach_80183C90;
                    case 3:
                        return D_dryfield_trailer_coach_80183C9C;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183B88;
                    case 1:
                        return D_dryfield_trailer_coach_80183B90;
                    case 2:
                        return D_dryfield_trailer_coach_80183B98;
                    case 3:
                        return D_dryfield_trailer_coach_80183BA0;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183BD4;
                    case 1:
                        return D_dryfield_trailer_coach_80183BE0;
                    case 2:
                        return D_dryfield_trailer_coach_80183BE8;
                    case 3:
                        return D_dryfield_trailer_coach_80183BF0;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183BAC;
                    case 1:
                        return D_dryfield_trailer_coach_80183BB4;
                    case 2:
                        return D_dryfield_trailer_coach_80183BC0;
                    case 3:
                        return D_dryfield_trailer_coach_80183BC8;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183B68;
                    case 1:
                        return D_dryfield_trailer_coach_80183B6C;
                    case 2:
                        return D_dryfield_trailer_coach_80183B70;
                    case 3:
                        return D_dryfield_trailer_coach_80183B78;
                }
                break;
        }
    }
    return D_dryfield_trailer_coach_80183E2C;
}

/// Draws one row of the shop list and handles its input, recording the row's
/// id as the cursor item while the row is selected. Row 0xFFFE is greyed out
/// and unselectable unless `Gp_HasMappedItem` answers non-zero, and opens its
/// own panel; row 0xFFFC is greyed out while the scan holds item 0x8F. Any
/// other row is an item with its price, greyed out when `func_800B7420`
/// refuses it; confirm opens the buy panel and button 0x10 the item's detail
/// panel.
void func_dryfield_trailer_coach_8017DE64(UiList* prompt, UiObject* obj)
{
    TextDrawReq   req;
    u8            buf[0x20];
    RoomShopList* shop;
    McItemScan*   scan;
    s32           y;
    s32           scaled;
    UiObject*     child;
    UiObject*     child2;
    s32           blocked;
    s32           status;
    s32           itemId;
    s32           price;

    shop    = (RoomShopList*)obj->owner->work;
    blocked = 0;
    itemId  = shop->items[prompt->field_8];
    /* &Mc_SaveData[0].state.carriedItems hoisted into a saved register here, as the original does,
       instead of being rematerialised at the Gp_SumScanQty call. */
    scan = &Mc_SaveData[0].state.carriedItems;
    if (prompt->field_C == 1) {
        D_dryfield_trailer_coach_80183D44 = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->panel.field_0.w;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam(D_dryfield_trailer_coach_80183D78, 0, 0);
            }
        }
        if (Gp_HasMappedItem() == 0) {
            prompt->field_1C = Ui_LookupTable(obj, 2);
            prompt->field_C  = 0;
        }
        req.x          = obj->panel.field_20.u + prompt->field_18;
        y              = obj->panel.field_22.u - 4;
        req.y          = prompt->field_1A + y;
        req.otIndex    = obj->panel.field_14.s + 1;
        req.field_8    = prompt->field_1C;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        Text_DrawString(&req, D_dryfield_trailer_coach_80183D64);
        if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183F30, 0, 1, 1, obj);
            obj->panel.field_0.w = 0;
        }
        return;
    }

    if (itemId == 0xFFFC) {
        status = obj->panel.field_0.w;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
            }
        }
        if (Gp_SumScanQty(scan, 0x8F) != 0) {
            blocked          = 1;
            prompt->field_1C = Ui_LookupTable(obj, 2);
        }
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_trailer_coach_80183D74, prompt->field_1C, 1, 0);
        if (prompt->field_C == 1 && blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            child = Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183EDC, itemId, 1, 1, obj);
            if (child != NULL) {
                Ui_ClampDialogRect(&(child)->panel, prompt, &(obj)->panel);
                obj->panel.field_0.w = 0;
            }
        }
        return;
    }

    price = Gp_ItemDescs[itemId].price;
    if (func_800B7420(itemId) != 0) {
        blocked          = 1;
        prompt->field_1C = Ui_LookupTable(obj, 2);
    }
    if (prompt->field_22 != 0x41) {
        status = obj->panel.field_0.w;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Gp_SetHolderItemText(itemId);
                Gp_SetPreviewItem(itemId, 0);
            }
        }
    }
    if (prompt->field_C == 1) {
        if (blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            child2 = Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183EDC, itemId, 1, 1, obj);
            if (child2 != NULL) {
                SndEvt_EnqueueType6(0x16, 0, 0);
                Ui_ClampDialogRect(&(child2)->panel, prompt, &(obj)->panel);
                obj->panel.field_0.w = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, itemId, 1, 1, obj);
            obj->panel.field_0.w = 0;
        }
    }
    Gp_DrawItemLabel(obj, prompt->field_18, prompt->field_1A, itemId, prompt->field_1C, 0);
    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: emits the scaled index before the table base so the
           `addu` is index-first, matching the original. */
        scaled = itemId * 4;
        Gp_DrawQty(obj, prompt->field_18, prompt->field_1A, gpItemStock(itemId)->perBuy, prompt->field_1C);
    }
    Text_ItoaUnsigned(buf, price);
    Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, buf, prompt->field_1C, 3, 2);
}

/// Adds an item id to the room's shop list, keeping one entry per item kind:
/// ids 0xF..0x32 are three consecutive levels of the same kind, so an entry of
/// the same kind is overwritten only by a higher level. In mode 0x10 the ids
/// 0x9D..0x9F, 0x8A and 0x65 are never added.
static void func_dryfield_trailer_coach_8017E2F0(RoomShopList* shop, UiObject* obj, s32 item)
{
    Task*         task = obj->owner;
    s32           mode = task->spawnArg1.value;
    RoomShopList* list = (RoomShopList*)task->work;
    s32           i;

    for (i = 0; i < shop->list.field_4; i++) {
        s32 cur = list->items[i];
        s32 q;

        if (cur == item) {
            return;
        }
        if (((mode & 0xFFFF) == 0x10) &&
            (((u32)(item - 0x9D) < 3U) || (item == 0x8A) || (item == 0x65))) {
            return;
        }
        if (((u32)(item - 0xF) < 0x24U) && ((u16)(cur - 0xF) < 0x24U)) {
            q = (item - 0xF) / 3;
            if ((q == (cur - 0xF) / 3) && (((item - 0xF) % 3 + 1) > ((cur - 0xF) % 3 + 1))) {
                list->items[i] = item;
                return;
            }
        }
    }

    Gp_SetItemSeenBit(item, 1);
    list->items[shop->list.field_4] = item;
    shop->list.field_4++;
}

/// Fills `shop` with the ids the shop currently offers, then sorts them by
/// `Gp_ItemSortKey`, caps the visible row count at 9 and clears the cursor
/// item.
///
/// The upper halfword of the owning task's `spawnArg1` is the mode, which picks
/// the fixed id list (`func_dryfield_trailer_coach_8017D7F4`) and, in game mode
/// 0, which items of each unlocked price row are added: mode 0 ids 0x80-0x9F
/// and 9, 0xA, 0xC, 0x42-0x46; mode 1 ids 0xA0-0xBF; mode 2 ids 0x60-0x7F and
/// 0xD; mode 3 ids 1-0x5F other than those. Mode 3 also adds, for each of the
/// twelve two-bit levels in `Mc_SaveData[0].state.shopStock`, the id of that level
/// (the first slot needs level 2). With `Mc_SaveData[0].state.demoScene` 1 every row
/// and level is unlocked first.
static void func_dryfield_trailer_coach_8017E43C(RoomShopList* shop, UiObject* obj)
{
    RoomShopList* list;
    u16*          ids;
    s32           mode;
    s32           tier;
    s32           slot;
    s32           level;
    s32           id;
    s32           item;
    s32           unlocked;
    s32           i;
    s32           j;
    s32           k;
    s32           key;
    s32           otherKey;
    u16           tmp;
    u8            count;

    mode = obj->owner->spawnArg1.value;
    ids  = func_dryfield_trailer_coach_8017D7F4(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        func_dryfield_trailer_coach_8017E2F0(shop, obj, *ids);
        ids++;
    }

    if (Mc_SaveData[0].state.demoScene == 1) {
        Mc_SaveData[0].state.shopTiers = 0x1FFF;
        Mc_SaveData[0].state.shopStock = -1;
    }

    if (Mc_SaveData[0].state.gameMode == 0) {
        if (Mc_SaveData[0].state.shopTiers != 0) {
            for (tier = 0; tier < 13; tier++) {
                unlocked = Mc_SaveData[0].state.shopTiers & (1 << tier);
                if (unlocked != 0) {
                    for (j = 0; j < 3; j++) {
                        item = D_dryfield_trailer_coach_80183CA8[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    func_dryfield_trailer_coach_8017E2F0(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    func_dryfield_trailer_coach_8017E2F0(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    func_dryfield_trailer_coach_8017E2F0(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    func_dryfield_trailer_coach_8017E2F0(shop, obj, item);
                                }
                                break;
                        }
                    }
                }
            }
        }

        if ((mode >> 16) == 3) {
            for (slot = 0; slot < 0xC; slot++) {
                level = (Mc_SaveData[0].state.shopStock >> (slot * 2)) & 3;
                if (slot == 0 ? level >= 2 : level > 0) {
                    /* The assignment keeps `+ 0xE` on the level instead of
                       letting GCC reassociate it onto the row base. */
                    func_dryfield_trailer_coach_8017E2F0(shop, obj, slot * 3 + (id = level + 0xE));
                }
            }
        }
    }

    list = (RoomShopList*)obj->owner->work;
    for (i = 0; i < shop->list.field_4 - 1; i++) {
        key = Gp_ItemSortKey(list->items[i]);
        for (k = i + 1; k < shop->list.field_4; k++) {
            otherKey = Gp_ItemSortKey(list->items[k]);
            if (otherKey < key) {
                tmp            = list->items[i];
                key            = otherKey;
                list->items[i] = list->items[k];
                list->items[k] = tmp;
            }
        }
    }

    count                = shop->list.field_4;
    shop->list.field_5.u = count;
    if ((s8)count >= 0xA) {
        shop->list.field_5.u = 9;
    }
    D_dryfield_trailer_coach_80183D44 = -1;
}

/// Titles and captions of the shop's panels.
static const u8 D_dryfield_trailer_coach_8017D6D0[] = "Select";
static const u8 D_dryfield_trailer_coach_8017D6D8[] = "BP";
static const u8 D_dryfield_trailer_coach_8017D6DC[] = "List";
static const u8 D_dryfield_trailer_coach_8017D6E4[] = "TOTAL";
static const u8 D_dryfield_trailer_coach_8017D6EC[] = "Notice";

/// "Charge", with a stray non-zero byte after its terminator that C cannot
/// place, so the string stays assembly.
/// "Charge", followed by the non-zero padding the original toolchain left.
static const char D_dryfield_trailer_coach_8017D6F4[8] = "Charge\0\xEF";

/// The shop's "Select" panel. On its first frame it allocates the
/// `RoomShopList` work block, fills it through
/// `func_dryfield_trailer_coach_8017E43C` and opens the panel
/// `D_dryfield_trailer_coach_80183F4C` beside it. Every frame it draws the list and the
/// "BP" caption; menu reports -1 and cancel 6 to the parent. A child that
/// reports 6 is torn down and the list takes input again; one that reports -1
/// passes it up.
void func_dryfield_trailer_coach_8017E808(Task* task)
{
    TextDrawReq   req;
    UiObject*     obj;
    RoomShopList* shop;
    Task*         head;
    Task*         child;
    Task*         next;
    UiObject*     childObj;
    void*         mem;
    s32           code;
    s32           x;
    s32           y;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, (char*)D_dryfield_trailer_coach_8017D6D0);
    if (task->state == 0) {
        mem = memCalloc(sizeof(RoomShopList), 0);
        if (mem != NULL) {
            shop               = mem;
            task->work         = (TaskIdMap*)shop;
            shop->list.funcs   = D_dryfield_trailer_coach_80183E30;
            shop->list.field_6 = 0;
            shop->list.field_7 = 0xF;
            func_dryfield_trailer_coach_8017E43C(shop, obj);
            Ui_LayoutListPanel(&shop->list, &(obj)->panel);
            shop->list.field_A = 1;
            Ui_SetListScrollFlag(&shop->list, 1);
            obj->panel.bounds.unsignedRect.h += 8;
            shop->list.field_17               = 8;
            Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183F4C, 0, 0, 0, obj);
            task->state += 1;
        }
    }
    shop = (RoomShopList*)task->work;
    Ui_UpdateListNoAnim(shop, obj);
    Ui_DrawHBar(&(obj)->panel, (s16)obj->panel.field_1C.s, (s16)obj->panel.field_1E.u, (s16)obj->panel.field_18.u + 6);

    x              = obj->panel.field_20.u - 2;
    req.x          = obj->panel.field_1E.u + x;
    y              = obj->panel.field_22.u + 2;
    req.y          = obj->panel.field_18.u + y;
    req.otIndex    = obj->panel.field_14.s + 1;
    req.field_8    = 0x606060;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 1;
    Text_DrawString(&req, D_dryfield_trailer_coach_8017D6D8);

    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        }
    }

    head = task->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            code     = childObj->field_2E;
            next     = child->nextSibling;
            if (code != -1) {
                if (code == 6) {
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->panel.field_0.w = 1;
                }
            } else {
                obj->field_2E = code;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

/// Row handler of the shop's mode menu. The last row is the exit, which
/// reports 6 on confirm. Any other row stores its index as the owning task's
/// mode (the upper halfword of `spawnArg1`) and draws that mode's label; the
/// row is greyed out and unselectable when the mode's item-id list is empty,
/// and confirm opens the shop list panel with the mode.
void func_dryfield_trailer_coach_8017EA58(UiList* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_trailer_coach_80183D5C, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = D_dryfield_trailer_coach_80183DB4;
    obj->owner->spawnArg1.value = (u16)obj->owner->spawnArg1.value;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = D_dryfield_trailer_coach_80183DBC;
            obj->owner->spawnArg1.value |= 0x10000;
            break;
        case 2:
            text                   = D_dryfield_trailer_coach_80183DC8;
            obj->owner->spawnArg1.value |= 0x20000;
            break;
        case 3:
            text                   = D_dryfield_trailer_coach_80183DD0;
            obj->owner->spawnArg1.value |= 0x30000;
            break;
    }

    if (*func_dryfield_trailer_coach_8017D7F4(obj->owner->spawnArg1.value) == 0xFFFF) {
        prompt->field_1C = Ui_LookupTable(obj, 2);
        prompt->field_C  = 0;
    }

    one2 = 1;
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, text, prompt->field_1C, one2, 0);

    status = obj->panel.field_0.w;
    if (((status >> 16) == one2) || (status == one2)) {
        if (prompt->field_10 == prompt->field_8) {
            Ui_SetHolderParam(Gp_StrEmpty, 0, 0);
        }
    }

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183EA4, obj->owner->spawnArg1, 1, 1, obj);
        obj->panel.field_0.w = 0;
    }
}

/// The shop's "List" panel, whose rows are the modes. Its first frame clears the item previews,
/// opens the list-row panel and the preview panel, and lays out its five-row
/// list. Cancel or menu reports -1. A child reporting 6 is torn down; one
/// reporting -1 releases `Wip_UiHolder` and passes the code up.
void func_dryfield_trailer_coach_8017EC78(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2.pointer;
    list          = &D_dryfield_trailer_coach_80183E38;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, (char*)D_dryfield_trailer_coach_8017D6DC);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183EC0, task->spawnArg1, 0, 1, obj);
        Ui_SpawnFromDesc(&D_8010D80C, 0, 0, 0, obj);
        list->field_4   = 5;
        list->field_5.u = 5;
        Ui_LayoutListPanel(list, &(obj)->panel);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0) {
        obj->field_2E = -1;
    }

    head = task->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2.pointer;
            code     = childObj->field_2E;
            next     = child->nextSibling;
            if (code != -1) {
                if (code == 6) {
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->panel.field_0.w = 1;
                }
            } else {
                Wip_UiHolder  = NULL;
                obj->field_2E = code;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

/// Balance panel: the "BP" caption with the player's BP, and the "TOTAL"
/// caption with the carried item count over the inventory's row capacity.
void func_dryfield_trailer_coach_8017EE20(Task* task)
{
    s8            digits[0x20];
    s8            total[0x20];
    TextDrawReq   req0;
    TextDrawReq   req1;
    UiObject*     obj;
    PlayerStatus* cfg;
    McItemScan*   scan;
    s8*           p;
    s32           x;
    s32           y;
    s32           y2;
    s32           col;
    s32           capacity;
    s32           count;

    obj = task->spawnArg2.pointer;
    cfg = &Player_Status;
    x   = (s16)obj->panel.field_1C.s + 2;
    col = (s16)obj->panel.field_1E.u - 2;
    y   = (s16)obj->panel.field_18.u;

    req0.x          = obj->panel.field_20.u + x;
    req0.y          = obj->panel.field_22.u + y + 9;
    req0.otIndex    = obj->panel.field_14.s + 1;
    req0.field_8    = 0x606060;
    req0.glyphTable = 5;
    req0.centerMode = 0;
    req0.field_E    = 1;
    Text_DrawString(&req0, D_dryfield_trailer_coach_8017D6D8);

    Text_ItoaUnsigned((u8*)digits, cfg->bp);
    Text_DrawPrompt(obj, col, y + 0x19, (u8*)digits, 0x606060, 3, 2);

    y2              = y + 0x28;
    req1.x          = obj->panel.field_20.u + x;
    req1.y          = obj->panel.field_22.u + (y2 - 6);
    req1.otIndex    = obj->panel.field_14.s + 1;
    req1.field_8    = 0x606060;
    req1.glyphTable = 5;
    req1.centerMode = 0;
    req1.field_E    = 1;
    Text_DrawString(&req1, (char*)D_dryfield_trailer_coach_8017D6E4);

    p        = total;
    scan     = &Mc_SaveData[0].state.carriedItems;
    count    = Gp_CountScanItems(scan);
    capacity = scan->rowCount;
    Text_ItoaUnsigned((u8*)p, count);
    while (*p != 0) {
        p++;
    }
    *p = '/';
    Text_ItoaUnsigned((u8*)(p + 1), capacity);
    Text_DrawPrompt(obj, col, y2 + 0xA, (u8*)total, 0x606060, 3, 2);
}

/// Row handler of the buy prompt. On confirm it checks the price against the
/// player's BP (notice 0 when short) and the inventory (notice 2 for a
/// stackable item already held, 1 otherwise when it cannot be added). When the
/// owning task's parent runs in mode 1 it opens the quantity picker; otherwise
/// it takes the price, gives one of the item and reports 6.
void func_dryfield_trailer_coach_8017F004(UiList* prompt, UiObject* obj)
{
    TextDrawReq   req;
    UiObject*     child;
    PlayerStatus* cfg;
    McItemScan*   scan;
    s32           itemId;
    s32           mode;
    s32           price;

    itemId = obj->owner->spawnArg1.value;

    req.x          = obj->panel.field_20.u + (u16)prompt->field_18;
    req.y          = obj->panel.field_22.u + (u16)prompt->field_1A;
    req.otIndex    = obj->panel.field_14.s + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, D_dryfield_trailer_coach_80183D48);

    mode = prompt->field_C;
    if (mode == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        cfg   = &Player_Status;
        price = Gp_ItemDescs[itemId].price;
        scan  = &Mc_SaveData[0].state.carriedItems;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183EF8, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183EF8, 1, 1, 1, obj);
                }
                obj->panel.field_0.w = 0;
            } else if ((obj->owner->parent->spawnArg1.value >> 16) == mode) {
                child = Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183F68, itemId, 1, 1, obj);
                if (child != NULL) {
                    Ui_ClampDialogRect(&(child)->panel, prompt, &(obj)->panel);
                    obj->panel.field_0.w = 0;
                }
            } else {
                cfg->bp -= price;
                Gp_GiveItem(scan, itemId, -1);
                obj->field_2E = 6;
            }
        } else {
            Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183EF8, 0, 1, 1, obj);
            obj->panel.field_0.w = 0;
        }
    }
}

/// Notice panel: shows one of three messages picked by `spawnArg1`, sized to
/// the text. Menu reports -1; confirm, cancel or 0xBC frames elapsing tell the
/// parent panel to close with 6.
void func_dryfield_trailer_coach_8017F218(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1.value;
    obj  = task->spawnArg2.pointer;
    switch (kind) {
        case 1:
            text = D_dryfield_trailer_coach_80183DEC;
            break;
        case 2:
            text = D_dryfield_trailer_coach_80183DFC;
            break;
        default:
            text = D_dryfield_trailer_coach_80183DD8;
            break;
    }

    Ui_DrawText(&(obj)->panel, (char*)D_dryfield_trailer_coach_8017D6EC);
    obj->field_2E = 0;
    if (task->state == 0) {
        Ui_SizeFromTextPlain(&(obj)->panel, text);
        task->killCountdown = 0xBC;
        task->state        += 1;
    }
    Text_DrawMultiLine(obj, (s16)obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 0xF, text, 0x606060, 1, 0);
    task->killCountdown -= gDisplayState.frameTicks;
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
            return;
        }
        if (task->killCountdown <= 0 || Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            ((UiObject*)task->parent->spawnArg2.pointer)->field_2E = 6;
            task->killCountdown                            = 0x7FFF;
        }
    }
}

/// The charge panel: steps through the mapped item slots, refilling each
/// slot's ammo or attachment quantity to its related quantity and animating a
/// bar from the old value up to the new one for at most 0xBC frames. Confirm
/// or cancel (or the timer running out) moves to the next slot; running out of
/// slots reports 6 to the parent.
void func_dryfield_trailer_coach_8017F398(Task* task)
{
    UiObject*   obj;
    GpItemMap*  map;
    McItemSlot* slot;
    s32         slotId;
    s32         itemId;
    s32         curItem;
    s32         relItem;
    s32         qty;
    s32         y;
    s32         h;
    s32         status;
    s16         countdown;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, (char*)D_dryfield_trailer_coach_8017D6F4);

    if (task->state == 0) {
        task->spawnArg1.value = 0;
        task->state     = task->state + 1;
    }
    if (task->state == 1) {
        slotId          = Gp_NextMappedSlot(task->spawnArg1.value);
        task->spawnArg1.value = slotId;
        if (slotId < 0) {
            obj->field_2E = 6;
        } else {
            map                               = Gp_GetItemMap(slotId);
            D_dryfield_trailer_coach_80189C90 = map;
            itemId                            = map->field_1;
            slot                              = Gp_GetItemSlot(itemId);
            if (D_dryfield_trailer_coach_80189C90->field_0 == 0) {
                D_dryfield_trailer_coach_80189C8C = slot->ammoQty;
                slot->ammoQty                     = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_dryfield_trailer_coach_80189C8C = slot->attachQty;
                slot->attachQty                   = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown                 = 0xBC;
            D_dryfield_trailer_coach_80189C8C <<= 8;
            task->state                         = task->state + 1;
        }
    }

    curItem = D_dryfield_trailer_coach_80189C90->field_1;
    relItem = D_dryfield_trailer_coach_80189C90->field_2;
    if (D_dryfield_trailer_coach_80189C90->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                              <<= 8;
    D_dryfield_trailer_coach_80189C8C += 0x40;
    if (qty < D_dryfield_trailer_coach_80189C8C) {
        D_dryfield_trailer_coach_80189C8C = qty;
    }

    y = (s16)obj->panel.field_18.u;
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar(&(obj)->panel, (s16)obj->panel.field_1C.s, (s16)obj->panel.field_1E.u, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, D_dryfield_trailer_coach_80189C8C >> 8, 0x606060);
    h = (s16)obj->panel.field_1A.u;
    func_800C0E20(&(obj)->panel, (s16)obj->panel.field_1C.s + 2, (s16)obj->panel.field_1E.u - 2, h - 6, qty,
                  D_dryfield_trailer_coach_80189C8C, 0x1741F);

    if (task->state == 2) {
        countdown           = task->killCountdown - 1;
        task->killCountdown = countdown;
        status              = obj->panel.field_0.w;
        if (status == 1 && (countdown <= 0 || Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskConfirm) != 0)) {
            task->state     = status;
            task->spawnArg1.value = task->spawnArg1.value + 1;
        }
    }
}

static inline s32 _dryfield_trailer_coachAddItemCount(s32 item, s32 count)
{
    s32         i;
    s32         n;
    McItemRec*  rec;
    McItemScan* scan;

    if ((u32)(item - 0xA0) < 0x20U) {
        count += Gp_ScanStackQty(&Mc_SaveData[0].state.carriedItems, item);
    } else {
        scan = &Mc_SaveData[0].state.carriedItems;
        rec  = Gp_GetItemTable(scan) + scan->firstRow;
        n    = scan->rowCount;
        for (i = 0; i < n; i++) {
            if (rec[i].itemId == item) {
                count++;
            }
        }
    }
    return count;
}

/// Draws the preview of the item the shop list's cursor rests on and, for an
/// item id below 0x100, the "Amount" caption with how many of it the player
/// already holds. Stackable items (0xA0..0xBF) ask the scan for their stack
/// quantity; everything else is counted by walking the item table.
void func_dryfield_trailer_coach_8017F660(Task* task)
{
    u8          buf[0x10];
    TextDrawReq req;
    UiObject*   obj;
    s32         item;
    s32         y;
    s32         ry;
    s32         count;

    item         = D_dryfield_trailer_coach_80183D44;
    obj          = task->spawnArg2.pointer;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && D_dryfield_trailer_coach_80183D44 == Gp_GetPreviewItem()) {
        func_800C7AE8(obj, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 2, 0x20);
    } else {
        func_800C7AE8(obj, obj->panel.field_1C.s + 2, (s16)obj->panel.field_18.u + 2, 0x120);
    }
    y = (s16)obj->panel.field_18.u + 0x50;
    if (item < 0x100) {
        req.x          = obj->panel.field_1C.s + (obj->panel.field_20.u + 2);
        ry             = obj->panel.field_22.u - 6;
        req.y          = ry + y;
        req.otIndex    = obj->panel.field_14.s + 1;
        req.glyphTable = 5;
        req.field_8    = 0x606060;
        req.centerMode = 0;
        req.field_E    = 1;
        Text_DrawString(&req, D_dryfield_trailer_coach_80183E1C);
        count = 0;
        count = _dryfield_trailer_coachAddItemCount(item, count);
        Text_DrawPrompt(obj, (s16)obj->panel.field_1E.u - 2, y + 0xA, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
    }
}

/// Quantity picker of the buy prompt. Up and down step the count between 1 and
/// the most the player can take: for a stackable item, what its stock ceiling
/// still allows in steps of its per-buy amount; otherwise the free inventory
/// rows; in both cases no more than the BP affords. It shows the unit and
/// total price. Confirm takes the total and gives the items; confirm or
/// cancel tells the parent panel to close with 6.
void func_dryfield_trailer_coach_8017F834(Task* task)
{
    u8          buf[0x20];
    TextDrawReq req;
    UiObject*   obj;
    UiObject*   parentObj;
    s32         itemId;
    s32         price;
    s32         maxQty;
    s32         scaled;
    s32         afford;
    s32         held;
    /* The stock ceiling stays in $v0, so the scan count the shop just fetched
       has to be copied out of the return register instead of coalescing into
       it. */
    register s32 maxHeld asm("v0");
    s32          count;
    s32          left;
    s32          top;
    s32          x;
    s32          y;
    s32          i;

    itemId = task->spawnArg1.value;
    obj    = task->spawnArg2.pointer;
    maxQty = 1;
    price  = Gp_ItemDescs[itemId].price;

    if (task->state == 0) {
        task->extraState.value = 1;
        Ui_UpdateLayoutSize(&(obj)->panel, 0, Ui_Scale15(3) - 3);
        task->state = task->state + 1;
    }

    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: emits the scaled index before the table base so the
           `addu` is index-first, matching the original. */
        scaled = itemId * 4;
        if (gpItemStock(itemId)->perBuy != 0) {
            held    = Gp_ScanStackQty(&Mc_SaveData[0].state.carriedItems, itemId);
            maxHeld = gpItemStock(itemId)->maxHeld;
            maxQty  = maxHeld - held;
            if (maxQty <= 0) {
                maxQty = 1;
            } else {
                maxQty = (maxQty - 1) / gpItemStock(itemId)->perBuy;
                maxQty = maxQty + 1;
            }
        }
    } else {
        maxQty = Mc_SaveData[0].state.carriedItems.rowCount - Gp_CountScanItems(&Mc_SaveData[0].state.carriedItems);
    }

    afford = Player_Status.bp / price;
    if (afford < maxQty) {
        maxQty = afford;
    }

    left = (s16)obj->panel.field_1C.s;
    x    = left + 2;
    top  = (s16)obj->panel.field_18.u;
    y    = top + 0xF;
    Gp_DrawItemLabel(obj, x, y, itemId, 0x606060, 0);
    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: same index-first ordering as above. */
        scaled = itemId * 4;
        Gp_DrawQty(obj, x, y, gpItemStock(itemId)->perBuy, 0x606060);
    }

    count = task->extraState.value;
    Text_DrawPrompt(obj, left + 0x98, y, D_dryfield_trailer_coach_80183E28, 0x606060, 3, 2);
    Text_DrawPrompt(obj, -x, y, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
    Ui_DrawHBar(&(obj)->panel, left, -x + 2, top + 0x12);

    req.x          = obj->panel.field_20.u - x;
    y              = top + 0x1A;
    req.y          = obj->panel.field_22.u + y;
    req.otIndex    = obj->panel.field_14.s + 1;
    req.field_8    = 0x606060;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 1;
    Text_DrawString(&req, D_dryfield_trailer_coach_8017D6D8);

    Text_DrawPrompt(obj, -x, top + 0x2B, Text_ItoaSigned(buf, count * price), 0x606060, 3, 2);

    if (obj->panel.field_0.w == 1) {
        parentObj = task->parent->spawnArg2.pointer;
        if (Pad_CheckButtons(0, 1, 0x3000) != 0) {
            if (task->extraState.value < maxQty) {
                task->extraState.value = task->extraState.value + 1;
                SndEvt_EnqueueType6(0x15, 0, 0);
            }
        } else if (Pad_CheckButtons(0, 1, 0xC000) != 0) {
            if (task->extraState.value >= 2) {
                task->extraState.value = task->extraState.value - 1;
                SndEvt_EnqueueType6(0x15, 0, 0);
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            Player_Status.bp -= price * task->extraState.value;
            for (i = 0; i < task->extraState.value; i++) {
                Gp_GiveItem(&Mc_SaveData[0].state.carriedItems, itemId, -1);
            }
            SndEvt_EnqueueType6(0x16, 0, 0);
            parentObj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            parentObj->field_2E = 6;
        }
    }
}

/// Row handler that draws a single message and reports 6 on confirm.
void func_dryfield_trailer_coach_8017FCB4(UiList* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->panel.field_20.u + (u16)prompt->field_18;
    req.y          = obj->panel.field_22.u + (u16)prompt->field_1A;
    req.otIndex    = obj->panel.field_14.s + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, D_dryfield_trailer_coach_80183D5C);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

/// A list panel over `D_dryfield_trailer_coach_80183E64`. Cancel reports 6 and
/// menu -1; a child reporting 6 is torn down, one reporting -1 passes it up.
void func_dryfield_trailer_coach_8017FD70(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_dryfield_trailer_coach_80183E64;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Ui_LayoutListPanel(list, &(obj)->panel);
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.field_0.w == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }

    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        code     = childObj->field_2E;
        if (code != -1) {
            if (code == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->panel.field_0.w = 1;
            }
        } else {
            obj->field_2E = -1;
        }
    }
}

/// Opens the panel `D_dryfield_trailer_coach_80183E88` with the task's
/// `spawnArg1` as its parameter, setting frame timing 0 and the session's UI
/// flag while it is open; once the panel reports -1 or 6 it is torn down, and
/// ten frames later frame timing 1 and the flag are restored and the task
/// kills itself.
void func_dryfield_trailer_coach_8017FE98(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80183E88, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        gGameSession->uiOpen = 1;
        task->spawnArg2.pointer      = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2.pointer;
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

void func_dryfield_trailer_coach_8017FFCC(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_dryfield_trailer_coach_801845FC,
                D_dryfield_trailer_coach_80184628,
                D_dryfield_trailer_coach_8018464C,
                D_dryfield_trailer_coach_8018467C,
                D_dryfield_trailer_coach_801846B0,
                D_dryfield_trailer_coach_801846E4,
                D_dryfield_trailer_coach_8018471C,
                D_dryfield_trailer_coach_80184750,
                D_dryfield_trailer_coach_80184788,
            };

            Ui_SetHolderParam(tbl[arg0->field_8], 0, 0);
        }
    }

    switch (arg0->field_8) {
        case 0: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_dryfield_trailer_coach_801845A0);
            Text_FormatTime(p, Mc_SaveData[0].state.playTime);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 1: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_dryfield_trailer_coach_801845D0);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, D_dryfield_trailer_coach_801845F0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 2: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_dryfield_trailer_coach_801845A8);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, D_dryfield_trailer_coach_801845F0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 3: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_dryfield_trailer_coach_801845AC);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, D_dryfield_trailer_coach_801845F0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 4: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_dryfield_trailer_coach_801845B4);
            if (Mc_SaveData[0].state.field_6CC == 0) {
                pct = 0;
            } else {
                pct = (Mc_SaveData[0].state.field_6CC * 10000) / (Mc_SaveData[0].state.field_6CC + Mc_SaveData[0].state.field_6CE);
            }
            if (pct < 100) {
                Text_ItoaPadded(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_dryfield_trailer_coach_801845F8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 5: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         total;
            s32         cnt;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            total          = Mc_SaveData[0].state.field_6CC;
            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_dryfield_trailer_coach_801845C0);
            cnt   = 326;
            total = total + (GameFlag_GetNibble(0x167) + GameFlag_GetNibble(0x168));
            if (total == 0) {
                pct = 0;
            } else {
                pct = (total * 10000) / cnt;
            }
            if (pct < 100) {
                Text_ItoaPadded(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_dryfield_trailer_coach_801845F8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            Ui_DrawHBar(&(arg1)->panel, arg1->panel.field_1C.s, (s16)arg1->panel.field_1E.u, arg0->field_1A + 3);
            arg0->field_1A = (u16)arg0->field_1A + 5;
            break;
        }
        case 6: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_dryfield_trailer_coach_801845D8);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, D_dryfield_trailer_coach_801845F0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 7: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_dryfield_trailer_coach_801845E0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_92C), arg0->field_1C, 3, 2);
            break;
        }
        case 8: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, D_dryfield_trailer_coach_801845E8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data menu `func_dryfield_trailer_coach_8018181C`.
static const char D_dryfield_trailer_coach_8017D748[] = "Play Data";

/// The completion figure `func_dryfield_trailer_coach_80180798` draws.
static const u8 D_dryfield_trailer_coach_8017D754[] = "100.0%";

/// Titles of the play-data panels: the two usage lists
/// `func_dryfield_trailer_coach_801811B0` draws (item and PE), and the menu
/// `func_dryfield_trailer_coach_80181364` draws. The last has two stray
/// non-zero bytes after its terminator that C cannot place, so it stays
/// assembly.
static const char D_dryfield_trailer_coach_8017D75C[] = "Weapon Data";
static const char D_dryfield_trailer_coach_8017D768[] = "PE Data";

/// "Telephone", followed by the non-zero padding the original toolchain left.
static const char D_dryfield_trailer_coach_8017D770[12] = "Telephone\0\xD0\xFF";

void func_dryfield_trailer_coach_80180798(UiList* arg0, UiObject* arg1)
{
    u8             buf[0x20];
    TextDrawReq    req;
    TextDrawReq*   r;
    RoomItemUsage* work;
    POLY_G4*       prim;
    u8*            p;
    u8*            q;
    s32            item;
    s32            value;
    s32            x;
    s32            y;
    s32            color;
    s32            textY;
    s32            limit;
    s32            n;
    s32            len;
    s32            i;
    s32            avail;
    s32            base;
    s32            barW;
    s32            barX;
    s32            rowY;
    s32            one;
    s32            tx;
    s32            ty;

    p     = buf;
    r     = &req;
    x     = arg0->field_18;
    y     = arg0->field_1A;
    work  = (RoomItemUsage*)arg1->owner->work;
    item  = work->itemIds[arg0->field_8];
    value = work->percents[arg0->field_8];
    color = arg0->field_1C;
    if (arg1->panel.field_8 != 5) {
        req.x          = arg1->panel.field_20.u + 0x11 + x;
        textY          = arg1->panel.field_22.u - 6;
        req.y          = textY + y;
        req.otIndex    = arg1->panel.field_14.s + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        Text_DrawString(r, (u8*)Gp_GetItemText(item, 0, 0));
        func_800CE5D0(arg1, x, y, item);
    }
    limit = 1;
    if (value >= 10000) {
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_dryfield_trailer_coach_8017D754, arg0->field_1C, 3, 2);
    } else {
        for (i = 2; i > 0; i--) {
            limit *= 10;
        }
        if (value < limit) {
            Text_ItoaPadded(p, value, 3);
        } else {
            Text_ItoaUnsigned(p, value);
        }
        n   = 2;
        q   = p;
        len = 0;
        while (*q != 0) {
            q++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            q[1] = q[0];
            q--;
        }
        q[1] = '.';
        Text_Strcat(p, D_dryfield_trailer_coach_801845F8);
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
    }

    base  = (s16)arg1->panel.field_1C.s + 0x80;
    avail = (s16)arg1->panel.field_1E.u - 0x4A;
    barW  = avail - base;
    barW  = (barW * work->barWidths[arg0->field_8]) >> 12;
    rowY  = arg0->field_1A - 0xC;
    barW  = barW + 2;
    barX  = avail - barW;
    if (barW >= 2) {
        prim                     = (POLY_G4*)gGpuPrimCursor;
        tx                       = arg1->panel.field_20.u + barX + 1;
        prim->x2                 = tx;
        prim->x0                 = tx;
        ty                       = arg1->panel.field_22.u;
        gGpuPrimCursor           = prim + 1;
        ty                       = ty + rowY;
        ty                      += 1;
        PRIM_COLOR_WORD(prim, 3) = PRIM_RGBC(0, 0, 0x01, 0);
        PRIM_COLOR_WORD(prim, 1) = PRIM_RGBC(0, 0, 0x01, 0);
        setlen(prim, 8);
        PRIM_COLOR_WORD(prim, 0) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        setcode(prim, 0x38);
        PRIM_COLOR_WORD(prim, 2) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        tx                       = (u16)prim->x0 + barW - 1;
        prim->y1                 = ty;
        prim->y0                 = ty;
        ty                      += 8;
        prim->y3                 = ty;
        prim->y2                 = ty;
        prim->x3                 = tx;
        prim->x1                 = tx;
        addPrim(gGpuCurrentOt + arg1->panel.field_14.s + 1, prim);
    }
    one = 1;
    Ui_DrawBeveledRect(&(arg1)->panel, barX, arg0->field_1A - 0xC, barW, 9, 0, one);
    if (((arg1->panel.field_0.w >> 16) == one) || (arg1->panel.field_0.w == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->panel.field_0.w = 0;
        }
    }
}

/// Builds the item-usage panel's three parallel arrays from the save's
/// per-item use counters (`Mc_SaveData[0].state.weaponUseCounts`, ids 0x80-0x9F).
///
/// Every id whose name is non-empty (a leading 0 or 0xA marks an unused row)
/// and whose counter is non-zero is marked seen and appended to `itemIds`,
/// while the counters are summed. The ids are then insertion-sorted by use
/// count, most-used first. Finally each row gets `percents` - its share of all
/// recorded uses in hundredths of a percent, rounded - and `barWidths`, its
/// counter as a 12-bit fraction of the top row's. Both are scaled down by
/// halving until the top counter fits in 17 bits, so the multiply and the
/// shift cannot overflow.
static void func_dryfield_trailer_coach_80180B94(UiList* list, UiObject* obj)
{
    RoomItemUsage* work;
    s32            count;
    s32            total;
    s32            i;
    s32            j;
    s32            k;
    s32            id;
    s32            tmp;
    s32            uses;
    s32            scale;
    s32            top;
    s32            shift;
    s16*           p;
    u8             c;

    count = 0;
    total = 0;
    work  = (RoomItemUsage*)obj->owner->work;
    p     = work->itemIds;

    for (i = 0; i < 0x20; i++) {
        id = i + 0x80;
        c  = *Gp_GetItemText(id, 0, 1);
        if ((c != 0) && (c != 0xA) && (Mc_SaveData[0].state.weaponUseCounts[i] > 0)) {
            Gp_SetItemSeenBit(id, 1);
            *p++ = id;
            count++;
            total += Mc_SaveData[0].state.weaponUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            uses = Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80];
            for (j = 0; j < i; j++) {
                if (Mc_SaveData[0].state.weaponUseCounts[work->itemIds[j] - 0x80] < uses) {
                    tmp = work->itemIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->itemIds[k + 1] = work->itemIds[k];
                    }
                    work->itemIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        top   = Mc_SaveData[0].state.weaponUseCounts[work->itemIds[0] - 0x80];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            work->percents[i] =
                (u32)((Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80] * scale) / total + 1) >> 1;
            work->barWidths[i] =
                (Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80] << shift) / top;
        }
    }

    list->field_4   = count;
    list->field_9.u = 0;
    list->field_10  = 0;
}

/// Fills the "Play Data" PE-usage panel's `RoomPeUsage` block from the
/// save's per-slot use counters.
///
/// Each of the twelve Parasite Energy slots owns three consecutive ids starting
/// at 0xF, one per level, so slot `i` at level `Mc_SaveData[0].state.attachLevels[i]`
/// prints as `i * 3 + 0xF + level - 1` (a slot the player has never levelled
/// keeps the base id). Every slot with a non-zero counter in
/// `Mc_SaveData[0].state.attachUseCounts` is appended and its counter summed. Levels
/// are addressed by page and column, with three slots per page. The ids are
/// then insertion-sorted by use count, most-used first, and each row gets
/// `percents`, its share of all recorded uses in hundredths of a percent, and
/// `barWidths`, its counter as a 12-bit fraction of the top row's. Both are
/// scaled down by halving until the top counter fits in 17 bits, so the
/// multiply and the shift cannot overflow.
static void func_dryfield_trailer_coach_80180E90(UiList* list, UiObject* obj)
{
    RoomPeUsage* work;
    s16*         p;
    s32          count;
    s32          total;
    s32          i;
    s32          j;
    s32          k;
    s32          id;
    s32          slot;
    s32          uses;
    s32          scale;
    s32          shift;
    s32          top;
    s32          tmp;

    count = 0;
    total = 0;
    i     = 0;
    work  = (RoomPeUsage*)obj->owner->work;
    p     = work->peIds;

    for (; i < 12; i++) {
        s32 useCount;

        useCount = Mc_SaveData[0].state.attachUseCounts[i];
        id       = i * 3 + 0xF;
        if (useCount > 0) {
            s32 page;
            s32 column;

            page   = i / 3;
            column = i % 3;
            *p     = id;
            if (Mc_SaveData[0].state.attachLevels[column + page * 3] != 0) {
                *p = id + (Mc_SaveData[0].state.attachLevels[column + page * 3] - 1u);
            }
            p++;
            count++;
            total += Mc_SaveData[0].state.attachUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            slot = (work->peIds[i] - 0xF) / 3;
            uses = Mc_SaveData[0].state.attachUseCounts[slot];
            for (j = 0; j < i; j++) {
                slot = (work->peIds[j] - 0xF) / 3;
                if (Mc_SaveData[0].state.attachUseCounts[slot] < uses) {
                    tmp = work->peIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->peIds[k + 1] = work->peIds[k];
                    }
                    work->peIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        slot  = (work->peIds[0] - 0xF) / 3;
        top   = Mc_SaveData[0].state.attachUseCounts[slot];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            slot               = (work->peIds[i] - 0xF) / 3;
            work->percents[i]  = (u32)((Mc_SaveData[0].state.attachUseCounts[slot] * scale) / total + 1) >> 1;
            slot               = (work->peIds[i] - 0xF) / 3;
            work->barWidths[i] = (Mc_SaveData[0].state.attachUseCounts[slot] << shift) / top;
        }
    }

    list->field_4   = count;
    list->field_9.u = 0;
    list->field_10  = 0;
}

/// Usage panel task: `spawnArg1` 0 lists items, anything else PE. The first
/// frame allocates the rows' work block and fills it; cancel closes the panel,
/// and a child panel reporting -1 or 6 is torn down.
void func_dryfield_trailer_coach_801811B0(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &D_dryfield_trailer_coach_801847EC;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_dryfield_trailer_coach_8017D75C);
    } else {
        Ui_DrawText(&(obj)->panel, D_dryfield_trailer_coach_8017D768);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80184810, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            func_dryfield_trailer_coach_80180B94(list, obj);
        } else {
            func_dryfield_trailer_coach_80180E90(list, obj);
        }
        Ui_InitList(list, &(obj)->panel);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2.pointer;
            next     = child->nextSibling;
            if (childObj->field_2E == -1 || childObj->field_2E == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->panel.field_0.w = 1;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

void func_dryfield_trailer_coach_80181364(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s32       ready;
    s32       sel;
    s32       kind;
    s32       mode;
    s32       one;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    ready         = Mc_SaveData[0].state.demoScene == 1;
    list          = &D_dryfield_trailer_coach_80184874;
    one           = 1;
    if (Mc_SaveData[0].state.clearCount > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            gGameSession->uiOpen = one;
            Ui_SpawnFromDesc(&D_800611E4, 0, 0, 0, obj);
            obj->panel.field_0.w = 0;
            obj->panel.field_4  |= 0x80000000;
            task->state          = task->state + 1;
        }
    } else if (task->state == 0) {
        Ui_LayoutListPanel(list, &(obj)->panel);
        obj->panel.field_0.w = one;
        gGameSession->uiOpen = one;
        Ui_SetListScrollFlag(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        Ui_DrawText(&(obj)->panel, D_dryfield_trailer_coach_8017D770);
        Ui_UpdateListNoAnim(list, obj);
    }
    if (obj->field_2E == 6) {
        obj->field_2E = 0;
        Ui_SetState4(obj, task);
        obj->panel.field_0.w = 0;
    }
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        if (task->state != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
        }
        gGameSession->uiOpen = 0;
        obj->field_2E        = -1;
        obj->field_2C        = 0x34;
    }
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        sel      = childObj->field_2E;
        switch (sel) {
            case 6:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else if (task->state == 3) {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                } else {
                    Ui_TeardownTree(childObj, childObj->owner);
                    SndEvt_EnqueueType6(0x3B, 0, 0);
                    Ui_StartCloseAnim(&(obj)->panel, task);
                    obj->panel.field_0.w = 1;
                }
                break;
            case -1:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                }
                break;
        }
    }
}

void func_dryfield_trailer_coach_8018165C(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = func_dryfield_trailer_coach_80181D4C;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
static void func_dryfield_trailer_coach_801816B8(u8* str, s32 decimals)
{
    s32 len = 0;
    s32 i;

    if (decimals > 0) {
        while (*str != 0) {
            str++;
            len++;
        }
        if (len < decimals) {
            decimals = len;
        }
        decimals++;
        for (i = 0; i < decimals; i++) {
            str[1] = str[0];
            str--;
        }
        str[1] = '.';
    }
}

/// Format `value` as a percentage with `decimals` fractional digits into `buf`:
/// print the integer with at least `decimals + 1` digits when it is small enough
/// (so "5" with two decimals becomes "0.05"), otherwise print it unpadded, then
/// shift the last `decimals` digits right by one and drop a '.' in front of
/// them. Appends "%" and returns `buf`.
static u8* func_dryfield_trailer_coach_80181728(u8* buf, s32 value, s32 decimals)
{
    s32 limit;
    s32 i;
    s32 len;
    s32 n;
    u8* p;

    limit = 1;
    for (i = decimals; i > 0; i--) {
        limit *= 10;
    }

    if (value < limit) {
        Text_ItoaPadded(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    n   = decimals;
    p   = buf;
    len = 0;
    if (n > 0) {
        while (*p != 0) {
            p++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            p[1] = p[0];
            p--;
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_dryfield_trailer_coach_801845F8);
    return buf;
}

void func_dryfield_trailer_coach_8018181C(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_trailer_coach_801847C4;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_dryfield_trailer_coach_8017D748);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80184810, 0, 0, 1, obj);
        Ui_LayoutListPanel(list, &(obj)->panel);
        obj->panel.bounds.unsignedRect.h += 5;
        list->field_A                     = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
}

/// Queues a gouraud-shaded rectangle into the current OT one slot past the
/// panel's draw order. Origin is `field_20`/`field_22` plus (`arg1`, `arg2`);
/// `arg3`/`arg4` are width and height. Left vertices take `arg5`, right vertices
/// take `arg6`. A zero color or width < 2 draws nothing.
static void func_dryfield_trailer_coach_8018190C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    POLY_G4* prim;
    s16      x;
    s16      y;

    if ((arg5 != 0) && (arg3 >= 2)) {
        prim     = (POLY_G4*)gGpuPrimCursor;
        x        = arg0->field_20.u + arg1 + 1;
        prim->x0 = prim->x2      = x;
        y                        = arg0->field_22.u;
        gGpuPrimCursor           = prim + 1;
        PRIM_COLOR_WORD(prim, 0) = arg5;
        setPolyG4(prim);
        PRIM_COLOR_WORD(prim, 2) = arg5;
        PRIM_COLOR_WORD(prim, 3) = arg6;
        PRIM_COLOR_WORD(prim, 1) = arg6;
        y                        = y + arg2 + 1;
        x                        = prim->x0 + arg3 - 1;
        prim->y0 = prim->y1 = y;
        prim->x1 = prim->x3 = x;
        y                   = y + arg4 - 1;
        prim->y2 = prim->y3 = y;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14.u + 1, prim);
    }
}

void func_dryfield_trailer_coach_80181A10(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_trailer_coach_80184578, prompt->field_1C, 1, 0);
    sel = prompt->field_C;
    if (sel == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0 && CdCmd_IsIdle() != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        gDisplayState.gameMode = 0xFF;
        Ui_SpawnFromDesc(&D_800611E4, 1, 0, 0, obj);
        obj->panel.field_0.w = 0;
        obj->field_2E        = 6;
        obj->owner->state    = sel;
    }
}

void func_dryfield_trailer_coach_80181AF4(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_trailer_coach_80184580, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_trailer_coach_8018482C, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_dryfield_trailer_coach_80181BBC(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_trailer_coach_8018458C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80184848, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_dryfield_trailer_coach_80181C84(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_trailer_coach_80184598, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_trailer_coach_80184848, 1, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Exit callback of a prompt task that registers its UI object as
/// `Wip_UiHolder`: releases the holder if the task still owns it, then frees
/// the UI object and kills the task.
static void func_dryfield_trailer_coach_80181D4C(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.

/// The room's cutscene runner: suppresses the player and ally HUD, loads and
/// starts the scene's caption slot, lets confirm or cancel cut the scene
/// sub-task short, applies the story-flag side effects when the scene ends,
/// and restores everything before killing itself.
void func_dryfield_trailer_coach_80181D88(Task* task)
{
    RoomCutsceneRec* rec;
    s32              killOut;
    s32              flag;
    s32              cmd;
    s32              fadeA;
    s32              fadeB;

    rec = (RoomCutsceneRec*)task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            D_dryfield_trailer_coach_80189C94.value = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (rec->field_0 > 0) {
                D_80115694                  = Mc_SaveData[0].state.at4.loc.view;
                Mc_SaveData[0].state.at4.loc.view = rec->field_0;
            } else {
                D_80115694 = -rec->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (rec->field_4 != 0) {
                SndEvt_EnqueueType6(rec->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (rec->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(rec->field_3);
                fadeB = 0;
                fadeA = rec->field_14;
                if (fadeA == 0) {
                    fadeA = 0x3C0;
                } else {
                    fadeB = rec->field_16;
                }
                func_800E6D4C(fadeA, fadeB);
            }
            if (rec->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_dryfield_trailer_coach_80189C94.value = Task_SpawnFromTable(D_dryfield_trailer_coach_80184F7C, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_dryfield_trailer_coach_80189C94.value);
                task->state++;
            } else if (Task_PollKill(D_dryfield_trailer_coach_80189C94.value, &killOut) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (rec->field_2 == 0) {
                SndEvt_EnqueueType6(rec->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (rec->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(rec->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((GP_LOC_WORD(Mc_SaveData[0].state.at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 1, 0, 0)) {
                        Gp_ApplyAreaRecs(D_acropolis_square_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1.value);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData[0].state.at4.loc.view = D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(rec->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (rec->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

/// Byte at 0x8007272D, written when the trailer-coach scene ends.

void func_dryfield_trailer_coach_801822F4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            func_800E8614(D_dryfield_trailer_coach_80185AFC, 1);
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                task->state = 2;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xB) {
                func_800E8614(D_dryfield_trailer_coach_80185C4C, 0);
            } else if (GameFlag_GetNibble(0x28) < 2) {
                func_800E8634(D_dryfield_trailer_coach_80185D54, 0,
                              D_dryfield_trailer_coach_80186684);
                GameFlag_SetNibble(0x28, 2);
                GameFlag_SetNibble(0x3A, 1);
                GameFlag_SetNibble(0x4B, 1);
                func_800E3FAC(0xA2, 0xF);
                Mc_SaveData[0].state.sceneEvent = 6;
                Gp_ApplyAreaRecs(D_dryfield_trailer_coach_80189C50);
            } else if (Gp_HasCollectedBit(0x111) == 0 && GameFlag_GetNibble(0x4F) != 0) {
                if (GameFlag_GetNibble(0xFD) == 0) {
                    GameFlag_SetNibble(0xFD, 1);
                    func_800E8614(D_dryfield_trailer_coach_80186D2C, 0);
                } else {
                    func_800E8614(D_dryfield_trailer_coach_80187074, 0);
                }
            } else if (GameFlag_GetNibble(0x28) == 2) {
                func_800E8634(D_dryfield_trailer_coach_8018681C, 0,
                              D_dryfield_trailer_coach_80186A74);
                GameFlag_SetNibble(0x28, 3);
            } else {
                func_800E8614(D_dryfield_trailer_coach_80186BDC, 0);
            }
            task->state++;
            break;
        case 3:
            taskKill(task);
            break;
    }
}

/// Plays the sound event in `spawnArg2` on its first frame and again at frame
/// 0x50, then asks for the task's own kill at frame 0x78.
void func_dryfield_trailer_coach_801824E8(Task* task)
{
    switch (task->state) {
        case 0x50:
        case 0x0:
            SndEvt_EnqueueType6(task->spawnArg2.value, 0, 0);
            task->state += 1;
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            break;
        default:
            task->state += 1;
            break;
    }
}

/// Always returns 0.
s32 func_dryfield_trailer_coach_80182578(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Location-message handler: copies the requested location onto the outgoing
/// record and answers 1.
s32 func_dryfield_trailer_coach_80182580(Task* task, s32 msgId, GpSaveLoc * src, GpSaveLoc * dst)
{
    *dst = *src;
    return 1;
}

/// Runs the trailer coach's day-2 hand-off. Request 3 spawns entry 1 of the
/// room's task table; request 0xE drops the save view back to 1 when it is on
/// 2, then either raises the `0x16C` flag and asks the cap system to run
/// command 0x1D, or fills in the room's cutscene record (view 0xA, slots 1,
/// files 3/4/5/6) and hands it to `D_dryfield_trailer_coach_80184F7C`. Always returns 0.
s32 func_dryfield_trailer_coach_801825A8(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 3) {
        Task_SpawnFromTable(D_dryfield_trailer_coach_80184FC0, 1, 0, 0);
    }
    if (arg2 == 0xE) {
        if (Mc_SaveData[0].state.at4.loc.warp == 2) {
            Mc_SaveData[0].state.at4.loc.warp = 1U;
        }
        if (GameFlag_GetNibble(0x16C) == 0) {
            GameFlag_SetNibble(0x16C, 1);
            Gp_RunCapCmd1(0x1D);
            return 0;
        }
        D_dryfield_trailer_coach_80189C9C.field_0  = 0xA;
        D_dryfield_trailer_coach_80189C9C.field_1  = 1;
        D_dryfield_trailer_coach_80189C9C.field_3  = 1;
        D_dryfield_trailer_coach_80189C9C.field_2  = 0;
        D_dryfield_trailer_coach_80189C9C.field_4  = 0x521B0003;
        D_dryfield_trailer_coach_80189C9C.field_8  = 0x521B0005;
        D_dryfield_trailer_coach_80189C9C.field_10 = 0x521B0004;
        D_dryfield_trailer_coach_80189C9C.field_C  = 0x521B0006;
        Task_SpawnFromTable(D_dryfield_trailer_coach_80184F7C, 0, 3, &D_dryfield_trailer_coach_80189C9C);
        return 0;
    }
    return 0;
}

/// The message pointers the two-line text block reads: entries 0-1 by default,
/// entries 2-3 when the task's `spawnArg1` is 1.

/// Opens a two-line text block: allocates the `RoomTextBlock` (killing the task
/// if that fails), links its two line nodes to the lines of
/// `D_dryfield_trailer_coach_80185368.data.options` chosen by `spawnArg1`, hands the list to
/// `Ui_SpawnTextBlock` and advances the task.
static void func_dryfield_trailer_coach_801826A0(Task* task)
{
    RoomTextBlock* block;
    TextLineNode*  node;
    u8**           line;
    s32            table;
    s32            off;
    s32            mode;
    s32            i;

    block = memCalloc(sizeof(RoomTextBlock), 0);
    node  = block->lines;
    if (block == NULL) {
        taskKill(task);
        return;
    }

    i                  = 0;
    mode               = 1;
    line               = D_dryfield_trailer_coach_80185368.data.options;
    table              = (s32)D_dryfield_trailer_coach_80185368.data.options;
    off                = 8;
    task->work         = (TaskIdMap*)block;
    task->exitCallback = func_dryfield_trailer_coach_801827D0;

    for (; i < 2; i++) {
        if (task->spawnArg1.value == mode) {
            node->text = *(u8**)(off + table);
        } else {
            node->text = *line;
        }
        node->next = node + 1;
        node++;
        line++;
        off += 4;
    }
    node[-1].next = NULL;

    block->desc.count   = 2;
    block->desc.lines   = block->lines;
    block->desc.field_8 = 0;
    block->field_C      = 0;
    Ui_SpawnTextBlock(&block->desc, 0, 0, 0);
    task->state++;
}

/// Waits for the text block parked at `Task::work` to report a result in
/// `TextBlockDesc::field_2`, stores it through `Task::spawnArg2` and advances
/// the task.
static void func_dryfield_trailer_coach_80182794(Task* task)
{
    s16 result;

    result = ((RoomTextBlock*)task->work)->desc.field_2;
    if (result != 0) {
        *(s32*)task->spawnArg2.pointer = result;
        task->state            = task->state + 1;
    }
}

/// Exit callback of the text-block task: kills it and calls
/// `Stage_SetEndingFlag`.
static void func_dryfield_trailer_coach_801827D0(Task* arg0)
{
    taskKill(arg0);
    Stage_SetEndingFlag();
}

/// State table of the room's cutscene task, run by
/// `func_dryfield_trailer_coach_80182950`.
static const TaskFuncTable3 D_dryfield_trailer_coach_8017D7DC = {
    {
        func_dryfield_trailer_coach_80182888,
        func_dryfield_trailer_coach_8018291C,
        taskKill,
    },
};

/// State table of the room's two-line text-block task, run by
/// `func_dryfield_trailer_coach_801827F8`: open the block, wait for its
/// result, then kill the task and call `Stage_SetEndingFlag`.
static const TaskFuncTable3 D_dryfield_trailer_coach_8017D7E8 = {
    {
        func_dryfield_trailer_coach_801826A0,
        func_dryfield_trailer_coach_80182794,
        func_dryfield_trailer_coach_801827D0,
    },
};

/// Runs the text-block task's current state through a stack copy of its state
/// table.
void func_dryfield_trailer_coach_801827F8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_trailer_coach_8017D7E8;
    sp.funcs[task->state](task);
}

void func_dryfield_trailer_coach_80182850(void)
{
    s32 cond;

    cond  = GameFlag_GetNibble(0x28) >= 2;
    cond += 1;
    Gp_StartCapSlot(3, 0, cond);
}

/// State 0 of the trailer-coach cutscene task. It parks the room's message
/// table in the task, then either starts the scene (day 2) or asks the stage
/// for area 1, and advances to state 1.
static void func_dryfield_trailer_coach_80182888(Task* arg0)
{
    arg0->msgTable = D_dryfield_trailer_coach_80184FA0;
    Game_SetPtrSlot(arg0, 7);
    if (Mc_SaveData[0].state.at4.loc.warp == 2) {
        func_800E8634(D_dryfield_trailer_coach_801853F4, 0, D_dryfield_trailer_coach_80185964);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 4);
    } else {
        Stage_RequestFromAreaTable(1);
    }
    arg0->state = (s32)(arg0->state + 1);
}

static void func_dryfield_trailer_coach_8018291C(Task* task)
{
    char pad[0x10];

    if (Mc_SaveData[0].state.at4.loc.view == 8) {
        gDisplayState.otDepthShift = 0;
    } else {
        gDisplayState.otDepthShift = 3;
    }
}

/// Runs the cutscene task's current state through a stack copy of its state
/// table.
void func_dryfield_trailer_coach_80182950(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_trailer_coach_8017D7DC;
    sp.funcs[task->state](task);
}

/// Draws a pulsing light shaft at a point in `arg0`'s space. `arg1` is rotated
/// by the coordinate's `workm` and offset by its translation, then projected
/// through `GsWSMATRIX` into a 0x14-byte `G_SCRATCH_HEAD` block; nothing is
/// drawn when `otz` is 0x10 or less. Two gouraud `POLY_G4` halves of half width
/// `(s16)arg3 * 32 / otz` and two `LINE_G3` diagonals meet at the projected
/// point, whose vertex pulses cyan as `rsin(animFrame * arg2) / 34 + 0x78`.
static void func_dryfield_trailer_coach_801829A8(GpCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    void**            scratch;
    u8*               head;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    LINE_G3*          line;
    s32               i;
    s32               color;
    s32               pulse;
    s32               twice;
    s32               t;
    s32               t2;

    Gp_UpdateCoord(arg0);
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x14;
    block    = (RoomShaftScratch*)(head - 0x14);

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomShaftScratch*)(head - 0x14))->vec);
    block->vec.vx = (u16)block->vec.vx + (u16)arg0->workm.t[0];
    block->vec.vy = (u16)block->vec.vy + (u16)arg0->workm.t[1];
    block->vec.vz = (u16)block->vec.vz + (u16)arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomShaftScratch*)(head - 0x14))->vec);
    gte_rtps();
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        pulse            = rsin(gDisplayState.animFrame * (s16)arg2);
        i                = 0;
        block->halfWidth = ((s16)arg3 << 5) / ((RoomShaftScratch*)(head - 0x14))->otz;
        color            = pulse / 34 + 0x78;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->halfWidth;
            prim->x1 = prim->x2 = block->sx;
            prim->x3            = block->sx + (u16)block->halfWidth;
            prim->y0 = prim->y2 = prim->y3 = block->sy;
            twice                          = i << 1;
            prim->y1                       = (block->sy - (u16)block->halfWidth) + block->halfWidth * twice;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, color, color);
            setRGB2(line, 0, 0, 0);
            t        = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->halfWidth * t);
            line->y0 = block->sy - (block->halfWidth * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->halfWidth * t);
            line->y2 = block->sy + (block->halfWidth * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Draws a pulsing glow at `data` in `coord`'s space: the point is projected
/// through `GsWSMATRIX`, and nothing is drawn when its `otz` is 16 or less.
/// Around the projected centre it lays a fan of gouraud `POLY_G4` wedges of
/// radius `rOuter`, each paired with one of half that radius, then four quads
/// reaching out from `rInner` towards `rOuter`. The centre vertex's intensity
/// is `rsin(animFrame * arg2) / 34 + 0x78`: the outer wedges and the quads use
/// half of it in green and blue, the inner wedges the full value in green and
/// half in blue.
///
/// `work` carries the intensity and later the scratch-head address. Sharing
/// one variable is what keeps the halving shift reading the intensity's own
/// register rather than `color`'s.
static void func_dryfield_trailer_coach_80182EB4(GpCoord* coord, SVECTOR* data, s32 arg2, s32 arg3)
{
    u8*              head;
    RoomGlowScratch* block;
    POLY_G4*         prim;
    s32              pulse;
    s32              color;
    s32              half;
    s32              size;
    s32              ang;
    s32              t;
    s32              t2;
    s32              u;
    s32              work;

    Gp_UpdateCoord(coord);
    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x18);
        block   = (RoomGlowScratch*)tmp;
    }

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(data);
    gte_rtv0();
    gte_stsv(&((RoomGlowScratch*)(head - 0x18))->vec);
    block->vec.vx += coord->workm.t[0];
    block->vec.vy += coord->workm.t[1];
    block->vec.vz += coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomGlowScratch*)(head - 0x18))->vec);
    gte_rtps();
    gte_stsxy(&((RoomGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&block->otz);
    if (((RoomGlowScratch*)(head - 0x18))->otz > 16) {
        pulse         = rsin(gDisplayState.animFrame * (s16)arg2);
        ang           = 0;
        size          = (s16)arg3;
        block->rOuter = (size * 64) / ((RoomGlowScratch*)(head - 0x18))->otz;
        work          = pulse / 34 + 0x78;
        color         = work;
        work        <<= 16;
        half          = work >> 17;
        block->rInner = (size * 8) / ((RoomGlowScratch*)(head - 0x18))->otz;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        color = (s16)color >> 1;
        ang   = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    work = (s32)G_SCRATCH_HEAD;
    SCRATCH_POP_BYTES_AT(work, 0x18);
}

/// Picks the trailer's shaft drawer for the current camera view. The
/// stage-visit byte `gGameSession->at4.loc.view` is used as a bit index: views 2
/// and 8 (bits 2 and 8, `0x104`) take `func_dryfield_trailer_coach_801829A8`
/// with the tall half-extent 0xC0, and view 10 (bit 10, `0x400`) takes `func_dryfield_trailer_coach_80182EB4`
/// with 0x30. `Task::extra` is the task's `TmdObject`, so `field_8` is the
/// coordinate both draws share.
void func_dryfield_trailer_coach_801838DC(Task* arg0)
{
    s32      mask;
    GpCoord* coord;

    mask  = 1 << gGameSession->at4.loc.view;
    coord = arg0->extra.tmd->coords;
    if (mask & 0x104) {
        func_dryfield_trailer_coach_801829A8(coord, &D_dryfield_trailer_coach_801871C4, 0x60, 0xC0);
        return;
    }
    if (mask & 0x400) {
        func_dryfield_trailer_coach_80182EB4(coord, &D_dryfield_trailer_coach_801871C4, 0x60, 0x30);
    }
}
