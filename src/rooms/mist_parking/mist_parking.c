#include "mist_parking_private.h"

#include "gameplay/message.h"
#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include "decomp/common.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/mist_parking.h"
#include "rooms/acropolis_square.h"

#include "gameplay/actor.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/area_transitions.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_targets.h"

#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/world_state.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include "gameplay/animation.h"

extern UiObjectDesc D_800611E4;

/// The 0xFFFF-terminated item id lists `func_mist_parking_8017D8F8` chooses
/// from.
extern u16 D_mist_parking_80186058[];
extern u16 D_mist_parking_80186060[];
extern u16 D_mist_parking_80186068[];
extern u16 D_mist_parking_80186070[];
extern u16 D_mist_parking_80186080[];
extern u16 D_mist_parking_80186090[];
extern u16 D_mist_parking_801860A0[];
extern u16 D_mist_parking_801860A8[];
extern u16 D_mist_parking_801860B8[];
extern u16 D_mist_parking_801860C8[];
extern u16 D_mist_parking_801860D8[];
extern u16 D_mist_parking_801860E0[];
extern u16 D_mist_parking_801860F4[];
extern u16 D_mist_parking_8018610C[];
extern u16 D_mist_parking_80186120[];
extern u16 D_mist_parking_80186128[];
extern u16 D_mist_parking_80186138[];
extern u16 D_mist_parking_80186150[];
extern u16 D_mist_parking_80186164[];
extern u16 D_mist_parking_8018616C[];
extern u16 D_mist_parking_80186180[];
extern u16 D_mist_parking_8018619C[];
extern u16 D_mist_parking_801861AC[];
extern u16 D_mist_parking_801861B8[];
extern u16 D_mist_parking_801861D0[];
extern u16 D_mist_parking_801861EC[];
extern u16 D_mist_parking_80186200[];
extern u16 D_mist_parking_80186208[];
extern u16 D_mist_parking_8018621C[];
extern u16 D_mist_parking_8018623C[];
extern u16 D_mist_parking_8018624C[];
extern u16 D_mist_parking_80186258[];
extern u16 D_mist_parking_80186270[];
extern u16 D_mist_parking_80186274[];
extern u16 D_mist_parking_80186278[];
extern u16 D_mist_parking_80186280[];
extern u16 D_mist_parking_80186290[];
extern u16 D_mist_parking_80186298[];
extern u16 D_mist_parking_801862A0[];
extern u16 D_mist_parking_801862A8[];
extern u16 D_mist_parking_801862B4[];
extern u16 D_mist_parking_801862BC[];
extern u16 D_mist_parking_801862C8[];
extern u16 D_mist_parking_801862D0[];
extern u16 D_mist_parking_801862DC[];
extern u16 D_mist_parking_801862E8[];
extern u16 D_mist_parking_801862F0[];
extern u16 D_mist_parking_801862F8[];
extern u16 D_mist_parking_80186304[];
extern u16 D_mist_parking_80186310[];
extern u16 D_mist_parking_80186318[];
extern u16 D_mist_parking_80186324[];
extern u16 D_mist_parking_80186330[];
extern u16 D_mist_parking_8018633C[];
extern u16 D_mist_parking_80186340[];
extern u16 D_mist_parking_8018634C[];
extern u16 D_mist_parking_80186358[];
extern u16 D_mist_parking_80186364[];
extern u16 D_mist_parking_8018636C[];
extern u16 D_mist_parking_80186378[];
extern u16 D_mist_parking_80186384[];
extern u16 D_mist_parking_80186390[];
extern u16 D_mist_parking_80186398[];
extern u16 D_mist_parking_801863A4[];
extern u16 D_mist_parking_80186534[];

static void func_mist_parking_80181E50(Task* task);

void func_mist_parking_8017DF68(UiList *, UiObject *);

void func_mist_parking_8017E90C(Task *);
void func_mist_parking_8017EB5C(UiList *, UiObject *);
void func_mist_parking_8017ED7C(Task *);
void func_mist_parking_8017EF24(Task *);
void func_mist_parking_8017F108(UiList *, UiObject *);
void func_mist_parking_8017F31C(Task *);
void func_mist_parking_8017FDB8(UiList *, UiObject *);
void func_mist_parking_8017FE74(Task *);

void func_mist_parking_8017E90C(Task *);
void func_mist_parking_8017F49C(Task *);
void func_mist_parking_8017F764(Task *);
void func_mist_parking_8017F938(Task *);
void func_mist_parking_8017FF9C(Task *);
void func_mist_parking_801800D0(UiList *, UiObject *);
void func_mist_parking_8018089C(UiList *, UiObject *);
void func_mist_parking_801812B4(Task *);
void func_mist_parking_80181760(Task *);
void func_mist_parking_80181920(Task *);
void func_mist_parking_80181B14(UiList *, UiObject *);
void func_mist_parking_80181BF8(UiList *, UiObject *);
void func_mist_parking_80181CC0(UiList *, UiObject *);
void func_mist_parking_80181D88(UiList *, UiObject *);
void func_mist_parking_80181E8C(Task *);
void func_mist_parking_80182628(Task *);

s32 func_mist_parking_801823F8(s32, s32, s32);
s32 func_mist_parking_801826B8(void);
s32 func_mist_parking_801826C0(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_mist_parking_801826E8(Task *, s32, GpMsg13EF *);
void func_mist_parking_80182750(s32);
void func_mist_parking_801827A0(s32);

u16 D_mist_parking_80186058[4] = {
    140, 143, 0xFFFF, 0,
};

u16 D_mist_parking_80186060[4] = {
    172, 175, 0xFFFE, 0xFFFF,
};

u16 D_mist_parking_80186068[4] = {
    103, 98, 0xFFFF, 0,
};

u16 D_mist_parking_80186070[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_mist_parking_80186080[8] = {
    131, 140, 143, 10, 70, 138, 0xFFFF, 0,
};

u16 D_mist_parking_80186090[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_mist_parking_801860A0[4] = {
    108, 100, 0xFFFF, 0,
};

u16 D_mist_parking_801860A8[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_mist_parking_801860B8[8] = {
    132, 140, 143, 10, 70, 66, 138, 0xFFFF,
};

u16 D_mist_parking_801860C8[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_mist_parking_801860D8[4] = {
    98, 105, 106, 0xFFFF,
};

u16 D_mist_parking_801860E0[10] = {
    65, 59, 58, 1, 2, 6, 8, 4,
    0xFFFF, 0,
};

u16 D_mist_parking_801860F4[12] = {
    131, 157, 140, 142, 143, 10, 70, 69,
    67, 138, 0xFFFF, 0,
};

u16 D_mist_parking_8018610C[10] = {
    160, 161, 172, 173, 171, 169, 175, 0xFFFE,
    0xFFFF, 0,
};

u16 D_mist_parking_80186120[4] = {
    108, 100, 102, 0xFFFF,
};

u16 D_mist_parking_80186128[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_mist_parking_80186138[12] = {
    157, 9, 140, 142, 138, 143, 10, 70,
    69, 66, 67, 0xFFFF,
};

u16 D_mist_parking_80186150[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_mist_parking_80186164[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_mist_parking_8018616C[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_mist_parking_80186180[14] = {
    157, 9, 140, 142, 143, 10, 70, 69,
    66, 67, 68, 138, 0xFFFF, 0,
};

u16 D_mist_parking_8018619C[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_mist_parking_801861AC[6] = {
    103, 98, 100, 97, 107, 0xFFFF,
};

u16 D_mist_parking_801861B8[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_mist_parking_801861D0[14] = {
    140, 142, 138, 143, 10, 70, 69, 66,
    67, 68, 157, 9, 0xFFFF, 0,
};

u16 D_mist_parking_801861EC[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_mist_parking_80186200[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_mist_parking_80186208[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_mist_parking_8018621C[16] = {
    140, 142, 138, 139, 143, 10, 70, 69,
    66, 67, 68, 144, 157, 9, 0xFFFF, 0,
};

u16 D_mist_parking_8018623C[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_mist_parking_8018624C[6] = {
    100, 98, 97, 103, 107, 0xFFFF,
};

u16 D_mist_parking_80186258[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_mist_parking_80186270[2] = {
    139, 0xFFFF,
};

u16 D_mist_parking_80186274[2] = {
    171, 0xFFFF,
};

u16 D_mist_parking_80186278[4] = {
    108, 13, 0xFFFF, 0,
};

u16 D_mist_parking_80186280[8] = {
    65, 59, 58, 60, 11, 55, 0xFFFF, 0,
};

u16 D_mist_parking_80186290[4] = {
    131, 138, 0xFFFF, 0,
};

u16 D_mist_parking_80186298[4] = {
    160, 171, 0xFFFF, 0,
};

u16 D_mist_parking_801862A0[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_mist_parking_801862A8[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_mist_parking_801862B4[4] = {
    140, 138, 0xFFFF, 0,
};

u16 D_mist_parking_801862BC[6] = {
    160, 172, 171, 175, 0xFFFF, 0,
};

u16 D_mist_parking_801862C8[4] = {
    98, 13, 0xFFFF, 0,
};

u16 D_mist_parking_801862D0[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_mist_parking_801862DC[6] = {
    131, 138, 143, 70, 0xFFFF, 0,
};

u16 D_mist_parking_801862E8[4] = {
    160, 171, 175, 0xFFFF,
};

u16 D_mist_parking_801862F0[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_mist_parking_801862F8[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_mist_parking_80186304[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_mist_parking_80186310[4] = {
    171, 175, 0xFFFF, 0,
};

u16 D_mist_parking_80186318[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_mist_parking_80186324[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_mist_parking_80186330[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_mist_parking_8018633C[2] = {
    171, 0xFFFF,
};

u16 D_mist_parking_80186340[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_mist_parking_8018634C[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_mist_parking_80186358[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_mist_parking_80186364[4] = {
    171, 175, 0xFFFE, 0xFFFF,
};

u16 D_mist_parking_8018636C[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_mist_parking_80186378[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_mist_parking_80186384[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_mist_parking_80186390[4] = {
    171, 0xFFFE, 0xFFFF, 0,
};

u16 D_mist_parking_80186398[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_mist_parking_801863A4[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

RoomShopTier D_mist_parking_801863B0[13] = {
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

s32 D_mist_parking_8018644C = -1;

u8 D_mist_parking_80186450[20] = {
    80, 117, 114, 99, 104, 97, 115, 101, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
};

u8 D_mist_parking_80186464[8] = {
    80, 97, 115, 115, 0, 0, 0, 0,
};

u8 D_mist_parking_8018646C[16] = {
    66, 97, 116, 116, 101, 114, 105, 101, 115, 47, 70, 117, 101, 108, 0, 0,
};

u8 D_mist_parking_8018647C[4] = { 0 };

u8 D_mist_parking_80186480[60] = {
    87, 101, 97, 112, 111, 110, 115, 32, 117, 115, 105, 110, 103, 32, 98, 97,
    116, 116, 101, 114, 105, 101, 115, 32, 111, 114, 32, 102, 117, 101, 108, 10,
    99, 97, 110, 32, 98, 101, 32, 114, 101, 108, 111, 97, 100, 101, 100, 32,
    102, 111, 114, 32, 102, 114, 101, 101, 46, 0, 0, 0,
};

u8 D_mist_parking_801864BC[8] = {
    87, 101, 97, 112, 111, 110, 115, 0,
};

u8 D_mist_parking_801864C4[12] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 0, 0,
};

u8 D_mist_parking_801864D0[8] = {
    65, 114, 109, 111, 114, 0, 0, 0,
};

u8 D_mist_parking_801864D8[8] = {
    73, 116, 101, 109, 115, 0, 0, 0,
};

u8 D_mist_parking_801864E0[20] = {
    73, 110, 115, 117, 102, 102, 105, 99, 105, 101, 110, 116, 32, 66, 80, 46,
    0, 0, 0, 0,
};

u8 D_mist_parking_801864F4[16] = {
    73, 110, 118, 101, 110, 116, 111, 114, 121, 32, 102, 117, 108, 108, 46, 0,
};

u8 D_mist_parking_80186504[32] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 32, 99, 97, 112, 97, 99,
    105, 116, 121, 32, 114, 101, 97, 99, 104, 101, 100, 46, 0, 0, 0, 0,
};

u8 D_mist_parking_80186524[12] = {
    65, 109, 111, 117, 110, 116, 0, 0, 0, 0, 0, 0,
};

u8 D_mist_parking_80186530[4] = {
    120, 0, 0, 0,
};

u16 D_mist_parking_80186534[2] = {
    0xFFFF, 0,
};

UiListItemFunc D_mist_parking_80186538[1] = {
    func_mist_parking_8017DF68,
};

UiListItemFunc D_mist_parking_8018653C[1] = {
    func_mist_parking_8017EB5C,
};

UiList D_mist_parking_80186540 = { D_mist_parking_8018653C, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_mist_parking_80186564[2] = {
    func_mist_parking_8017F108,
    func_mist_parking_8017FDB8,
};

UiList D_mist_parking_8018656C = { D_mist_parking_80186564, 2, { .u = 2 }, 1, 10, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_mist_parking_80186590 = { 2, 0xFF70, 0xFF98, 128, 40, 56, 0, 0, 192, func_mist_parking_8017ED7C, 0 };

UiObjectDesc D_mist_parking_801865AC = { 2, 0xFF74, 0xFFA3, 188, 160, 48, 0, 0, 192, func_mist_parking_8017E90C, 0 };

UiObjectDesc D_mist_parking_801865C8 = { 0, 48, 4, 96, 60, 52, 0, 0, 192, func_mist_parking_8017EF24, 0 };

UiObjectDesc D_mist_parking_801865E4 = { 0, 48, 32, 70, 32, 20, 0, 0, 192, func_mist_parking_8017FE74, 0 };

UiObjectDesc D_mist_parking_80186600 = { 2, 0xFFA0, 0xFFD0, 192, 96, 8, 0, 0, 192, func_mist_parking_8017F31C, 0 };

// Retained data: Complete UI descriptor follows the adjacent UI descriptors. Its last 12 bytes also resemble a TaskDesc, which is its embedded task seed.
UiObjectDesc D_mist_parking_8018661C = { 0, 0xFF80, 0xFFE0, 160, 92, 48, 0, 0, 192, func_mist_parking_8017E90C, 0 };

UiObjectDesc D_mist_parking_80186638 = { 2, 0xFFB8, 0xFFDC, 144, 64, 32, 0, 0, 192, func_mist_parking_8017F49C, 0 };

UiObjectDesc D_mist_parking_80186654 = { 0, 48, 0xFFA3, 96, 97, 44, 0, 0, 192, func_mist_parking_8017F764, 0 };

UiObjectDesc D_mist_parking_80186670 = { 3, 0xFFB8, 0xFFE0, 184, 48, 16, 0, 0, 192, func_mist_parking_8017F938, 0 };

TaskDesc D_mist_parking_8018668C = { 0, 192, func_mist_parking_8017FF9C, { .model = NULL } };

u8 D_mist_parking_80186698[8] = {
    83,
    97,
    118,
    101,
    0,
    0,
    0,
    0,
};

u8 D_mist_parking_801866A0[12] = {
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

u8 D_mist_parking_801866AC[12] = {
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

u8 D_mist_parking_801866B8[8] = {
    80,
    69,
    32,
    68,
    97,
    116,
    97,
    0,
};

u8 D_mist_parking_801866C0[8] = {
    84,
    105,
    109,
    101,
    0,
    0,
    0,
    0,
};

u8 D_mist_parking_801866C8[4] = {
    87,
    111,
    110,
    0,
};

u8 D_mist_parking_801866CC[8] = {
    69,
    115,
    99,
    97,
    112,
    101,
    100,
    0,
};

u8 D_mist_parking_801866D4[12] = {
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

u8 D_mist_parking_801866E0[16] = {
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

u8 D_mist_parking_801866F0[8] = {
    83,
    97,
    118,
    101,
    100,
    0,
    0,
    0,
};

u8 D_mist_parking_801866F8[8] = {
    67,
    108,
    101,
    97,
    114,
    101,
    100,
    0,
};

u8 D_mist_parking_80186700[8] = {
    77,
    97,
    120,
    32,
    69,
    88,
    80,
    0,
};

u8 D_mist_parking_80186708[8] = {
    77,
    97,
    120,
    32,
    66,
    80,
    0,
    0,
};

u8 D_mist_parking_80186710[8] = {
    32,
    116,
    105,
    109,
    101,
    115,
    0,
    0,
};

u8 D_mist_parking_80186718[4] = {
    37,
    0,
    0,
    0,
};

u8 D_mist_parking_8018671C[44] = {
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

u8 D_mist_parking_80186748[36] = {
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

u8 D_mist_parking_8018676C[48] = {
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

u8 D_mist_parking_8018679C[52] = {
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

u8 D_mist_parking_801867D0[52] = {
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

u8 D_mist_parking_80186804[56] = {
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

u8 D_mist_parking_8018683C[52] = {
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

u8 D_mist_parking_80186870[56] = {
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

u8 D_mist_parking_801868A8[56] = {
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

UiListItemFunc D_mist_parking_801868E0[1] = {
    func_mist_parking_801800D0,
};

UiList D_mist_parking_801868E4 = { D_mist_parking_801868E0, 9, { .u = 9 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_mist_parking_80186908[1] = {
    func_mist_parking_8018089C,
};

UiList D_mist_parking_8018690C = { D_mist_parking_80186908, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_mist_parking_80186930 = { 3, 0xFF70, 64, 288, 40, 56, 0, 0, 192, func_mist_parking_80181760, 0 };

UiObjectDesc D_mist_parking_8018694C = { 2, 0xFF70, 0xFF98, 288, 120, 40, 0, 0, 192, func_mist_parking_80181920, 0 };

UiObjectDesc D_mist_parking_80186968 = { 2, 0xFF70, 0xFF98, 288, 168, 40, 0, 0, 192, func_mist_parking_801812B4, 0 };

UiListItemFunc D_mist_parking_80186984[4] = {
    func_mist_parking_80181B14,
    func_mist_parking_80181BF8,
    func_mist_parking_80181CC0,
    func_mist_parking_80181D88,
};

UiList D_mist_parking_80186994 = { D_mist_parking_80186984, 4, { .u = 4 }, 1, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

TaskDesc D_mist_parking_801869B8[3] = {
    { 0, 32, func_mist_parking_80181E8C, { .model = NULL } },
    { 0, 32, func_mist_parking_80182628, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} MistParkingPoseBank941C;

MistParkingPoseBank941C D_mist_parking_801869DC = { .poses = {
#include "assets/mist_parking_animation_095D0_bank1.inc"
} };

GpPackedSvec D_mist_parking_801869F4[17] = {
#include "assets/mist_parking_animation_095D0_bank4.inc"
};

GpAnimRec D_mist_parking_80186A38[76] = {
#include "assets/mist_parking_animation_095D0_records.inc"
};

u16 D_mist_parking_80186B68[20] = {
#include "assets/mist_parking_animation_095D0_indices.inc"
};

GpAnimSet D_mist_parking_80186B90 = {
    D_mist_parking_80186A38, D_mist_parking_80186B68,
    { NULL, D_mist_parking_801869DC.words, NULL, NULL, D_mist_parking_801869F4, NULL, NULL, NULL },
};

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task *, s32, GpMsg13EF *);
        s32 (*call2)(Task *, s32, GpSaveLoc *, GpSaveLoc *);
        s32 (*call3)(s32, s32, s32);
    } handler;
} MistParkingMessageEntry;
STATIC_ASSERT_SIZEOF(MistParkingMessageEntry, 8);

MistParkingMessageEntry D_mist_parking_80186BB8[5] = {
    { 5102, { .call2 = func_mist_parking_801826C0 } },
    { 5103, { .call1 = func_mist_parking_801826E8 } },
    { 5105, { .call0 = func_mist_parking_801826B8 } },
    { 5104, { .call3 = func_mist_parking_801823F8 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

GpXformArg D_mist_parking_80186BE0 = { { 8448, 1, -2599, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_mist_parking_80186BF8 = { { 8448, 1, -3828, 0 }, { 0, 2048, 0, 0 } };

GpXformArg D_mist_parking_80186C10 = { { 3310, 0, -3550, 0 }, { 0, -1024, 0, 0 } };

GpAnimSet * D_mist_parking_80186C28[1] = {
    &D_mist_parking_80186B90,
};

GpCopyArg D_mist_parking_80186C2C = { { .sets = D_mist_parking_80186C28 }, 1 };

GpAnimArg D_mist_parking_80186C34 = { { .index = 1 }, 47, 0, 0, 0 };

GpAnimArg D_mist_parking_80186C48 = { { .index = 1 }, 1, 0, 0, 0 };

GpEvsCmd D_mist_parking_80186C5C[15] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_80186C48 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_801827A0 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51130001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_80186BE0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80182750 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51130002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_80186DC4[13] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_80186C48 }, { .value = 0 } },
    { 15, { .value = 0x51130001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_80186BF8 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80182750 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51130002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_80186EFC[12] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_80186C10 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_80186C2C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_80186C34 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_mist_parking_8018701C[1] = {
#include "assets/mist_parking_model_09CD4_skeleton.inc"
};

u32 D_mist_parking_80187040[1] = {
#include "assets/mist_parking_model_09CD4_partVerts.inc"
};

SVECTOR D_mist_parking_80187044[35] = {
#include "assets/mist_parking_model_09CD4_verts.inc"
};

u32 D_mist_parking_8018715C[78] = {
#include "assets/mist_parking_model_09CD4_stream.inc"
};

TmdSource D_mist_parking_80187294 = {
    0, 576, 0, 1,
    D_mist_parking_80187040, D_mist_parking_80187044, &D_mist_parking_80187044[35], D_mist_parking_8018701C, D_mist_parking_8018715C,
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} MistParkingPoseBank9CF8;

MistParkingPoseBank9CF8 D_mist_parking_801872B8 = { .poses = {
#include "assets/mist_parking_animation_09FD4_bank1.inc"
} };

GpPackedSvec D_mist_parking_80187300[46] = {
#include "assets/mist_parking_animation_09FD4_bank4.inc"
};

GpAnimRec D_mist_parking_801873B8[109] = {
#include "assets/mist_parking_animation_09FD4_records.inc"
};

u16 D_mist_parking_8018756C[20] = {
#include "assets/mist_parking_animation_09FD4_indices.inc"
};

GpAnimSet D_mist_parking_80187594 = {
    D_mist_parking_801873B8, D_mist_parking_8018756C,
    { NULL, D_mist_parking_801872B8.words, NULL, NULL, D_mist_parking_80187300, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[13];
    GpPackedSvec words[39];
} MistParkingPoseBank9FFC;

MistParkingPoseBank9FFC D_mist_parking_801875BC = { .poses = {
#include "assets/mist_parking_animation_0A774_bank1.inc"
} };

GpPackedSvec D_mist_parking_80187658[179] = {
#include "assets/mist_parking_animation_0A774_bank4.inc"
};

GpAnimRec D_mist_parking_80187924[250] = {
#include "assets/mist_parking_animation_0A774_records.inc"
};

u16 D_mist_parking_80187D0C[20] = {
#include "assets/mist_parking_animation_0A774_indices.inc"
};

GpAnimSet D_mist_parking_80187D34 = {
    D_mist_parking_80187924, D_mist_parking_80187D0C,
    { NULL, D_mist_parking_801875BC.words, NULL, NULL, D_mist_parking_80187658, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} MistParkingPoseBankA79C;

MistParkingPoseBankA79C D_mist_parking_80187D5C = { .poses = {
#include "assets/mist_parking_animation_0AC5C_bank1.inc"
} };

GpPackedSvec D_mist_parking_80187DA4[110] = {
#include "assets/mist_parking_animation_0AC5C_bank4.inc"
};

GpAnimRec D_mist_parking_80187F5C[166] = {
#include "assets/mist_parking_animation_0AC5C_records.inc"
};

u16 D_mist_parking_801881F4[20] = {
#include "assets/mist_parking_animation_0AC5C_indices.inc"
};

GpAnimSet D_mist_parking_8018821C = {
    D_mist_parking_80187F5C, D_mist_parking_801881F4,
    { NULL, D_mist_parking_80187D5C.words, NULL, NULL, D_mist_parking_80187DA4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[9];
    GpPackedSvec words[27];
} MistParkingPoseBankAC84;

MistParkingPoseBankAC84 D_mist_parking_80188244 = { .poses = {
#include "assets/mist_parking_animation_0B138_bank1.inc"
} };

GpPackedSvec D_mist_parking_801882B0[117] = {
#include "assets/mist_parking_animation_0B138_bank4.inc"
};

GpAnimRec D_mist_parking_80188484[147] = {
#include "assets/mist_parking_animation_0B138_records.inc"
};

u16 D_mist_parking_801886D0[20] = {
#include "assets/mist_parking_animation_0B138_indices.inc"
};

GpAnimSet D_mist_parking_801886F8 = {
    D_mist_parking_80188484, D_mist_parking_801886D0,
    { NULL, D_mist_parking_80188244.words, NULL, NULL, D_mist_parking_801882B0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} MistParkingPoseBankB160;

MistParkingPoseBankB160 D_mist_parking_80188720 = { .poses = {
#include "assets/mist_parking_animation_0B700_bank1.inc"
} };

GpPackedSvec D_mist_parking_80188798[143] = {
#include "assets/mist_parking_animation_0B700_bank4.inc"
};

GpAnimRec D_mist_parking_801889D4[177] = {
#include "assets/mist_parking_animation_0B700_records.inc"
};

u16 D_mist_parking_80188C98[20] = {
#include "assets/mist_parking_animation_0B700_indices.inc"
};

GpAnimSet D_mist_parking_80188CC0 = {
    D_mist_parking_801889D4, D_mist_parking_80188C98,
    { NULL, D_mist_parking_80188720.words, NULL, NULL, D_mist_parking_80188798, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} MistParkingPoseBankB728;

MistParkingPoseBankB728 D_mist_parking_80188CE8 = { .poses = {
#include "assets/mist_parking_animation_0BAB4_bank1.inc"
} };

GpPackedSvec D_mist_parking_80188D24[65] = {
#include "assets/mist_parking_animation_0BAB4_bank4.inc"
};

GpAnimRec D_mist_parking_80188E28[137] = {
#include "assets/mist_parking_animation_0BAB4_records.inc"
};

u16 D_mist_parking_8018904C[20] = {
#include "assets/mist_parking_animation_0BAB4_indices.inc"
};

GpAnimSet D_mist_parking_80189074 = {
    D_mist_parking_80188E28, D_mist_parking_8018904C,
    { NULL, D_mist_parking_80188CE8.words, NULL, NULL, D_mist_parking_80188D24, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[12];
    GpPackedSvec words[36];
} MistParkingPoseBankBADC;

MistParkingPoseBankBADC D_mist_parking_8018909C = { .poses = {
#include "assets/mist_parking_animation_0C1B0_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018912C[153] = {
#include "assets/mist_parking_animation_0C1B0_bank4.inc"
};

GpAnimRec D_mist_parking_80189390[238] = {
#include "assets/mist_parking_animation_0C1B0_records.inc"
};

u16 D_mist_parking_80189748[20] = {
#include "assets/mist_parking_animation_0C1B0_indices.inc"
};

GpAnimSet D_mist_parking_80189770 = {
    D_mist_parking_80189390, D_mist_parking_80189748,
    { NULL, D_mist_parking_8018909C.words, NULL, NULL, D_mist_parking_8018912C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} MistParkingPoseBankC1D8;

MistParkingPoseBankC1D8 D_mist_parking_80189798 = { .poses = {
#include "assets/mist_parking_animation_0C688_bank1.inc"
} };

GpPackedSvec D_mist_parking_801897EC[94] = {
#include "assets/mist_parking_animation_0C688_bank4.inc"
};

GpAnimRec D_mist_parking_80189964[175] = {
#include "assets/mist_parking_animation_0C688_records.inc"
};

u16 D_mist_parking_80189C20[20] = {
#include "assets/mist_parking_animation_0C688_indices.inc"
};

GpAnimSet D_mist_parking_80189C48 = {
    D_mist_parking_80189964, D_mist_parking_80189C20,
    { NULL, D_mist_parking_80189798.words, NULL, NULL, D_mist_parking_801897EC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[12];
    GpPackedSvec words[36];
} MistParkingPoseBankC6B0;

MistParkingPoseBankC6B0 D_mist_parking_80189C70 = { .poses = {
#include "assets/mist_parking_animation_0CD80_bank1.inc"
} };

GpPackedSvec D_mist_parking_80189D00[177] = {
#include "assets/mist_parking_animation_0CD80_bank4.inc"
};

GpAnimRec D_mist_parking_80189FC4[213] = {
#include "assets/mist_parking_animation_0CD80_records.inc"
};

u16 D_mist_parking_8018A318[20] = {
#include "assets/mist_parking_animation_0CD80_indices.inc"
};

GpAnimSet D_mist_parking_8018A340 = {
    D_mist_parking_80189FC4, D_mist_parking_8018A318,
    { NULL, D_mist_parking_80189C70.words, NULL, NULL, D_mist_parking_80189D00, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} MistParkingPoseBankCDA8;

MistParkingPoseBankCDA8 D_mist_parking_8018A368 = { .poses = {
#include "assets/mist_parking_animation_0D060_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018A38C[63] = {
#include "assets/mist_parking_animation_0D060_bank4.inc"
};

GpAnimRec D_mist_parking_8018A488[92] = {
#include "assets/mist_parking_animation_0D060_records.inc"
};

u16 D_mist_parking_8018A5F8[20] = {
#include "assets/mist_parking_animation_0D060_indices.inc"
};

GpAnimSet D_mist_parking_8018A620 = {
    D_mist_parking_8018A488, D_mist_parking_8018A5F8,
    { NULL, D_mist_parking_8018A368.words, NULL, NULL, D_mist_parking_8018A38C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} MistParkingPoseBankD088;

MistParkingPoseBankD088 D_mist_parking_8018A648 = { .poses = {
#include "assets/mist_parking_animation_0D3E4_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018A690[69] = {
#include "assets/mist_parking_animation_0D3E4_bank4.inc"
};

GpAnimRec D_mist_parking_8018A7A4[118] = {
#include "assets/mist_parking_animation_0D3E4_records.inc"
};

u16 D_mist_parking_8018A97C[20] = {
#include "assets/mist_parking_animation_0D3E4_indices.inc"
};

GpAnimSet D_mist_parking_8018A9A4 = {
    D_mist_parking_8018A7A4, D_mist_parking_8018A97C,
    { NULL, D_mist_parking_8018A648.words, NULL, NULL, D_mist_parking_8018A690, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} MistParkingPoseBankD40C;

MistParkingPoseBankD40C D_mist_parking_8018A9CC = { .poses = {
#include "assets/mist_parking_animation_0D6A4_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018A9FC[40] = {
#include "assets/mist_parking_animation_0D6A4_bank4.inc"
};

GpAnimRec D_mist_parking_8018AA9C[104] = {
#include "assets/mist_parking_animation_0D6A4_records.inc"
};

u16 D_mist_parking_8018AC3C[20] = {
#include "assets/mist_parking_animation_0D6A4_indices.inc"
};

GpAnimSet D_mist_parking_8018AC64 = {
    D_mist_parking_8018AA9C, D_mist_parking_8018AC3C,
    { NULL, D_mist_parking_8018A9CC.words, NULL, NULL, D_mist_parking_8018A9FC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} MistParkingPoseBankD6CC;

MistParkingPoseBankD6CC D_mist_parking_8018AC8C = { .poses = {
#include "assets/mist_parking_animation_0DA94_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018ACE0[62] = {
#include "assets/mist_parking_animation_0DA94_bank4.inc"
};

GpAnimRec D_mist_parking_8018ADD8[149] = {
#include "assets/mist_parking_animation_0DA94_records.inc"
};

u16 D_mist_parking_8018B02C[20] = {
#include "assets/mist_parking_animation_0DA94_indices.inc"
};

GpAnimSet D_mist_parking_8018B054 = {
    D_mist_parking_8018ADD8, D_mist_parking_8018B02C,
    { NULL, D_mist_parking_8018AC8C.words, NULL, NULL, D_mist_parking_8018ACE0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} MistParkingPoseBankDABC;

MistParkingPoseBankDABC D_mist_parking_8018B07C = { .poses = {
#include "assets/mist_parking_animation_0DE94_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018B0F4[85] = {
#include "assets/mist_parking_animation_0DE94_bank4.inc"
};

GpAnimRec D_mist_parking_8018B248[121] = {
#include "assets/mist_parking_animation_0DE94_records.inc"
};

u16 D_mist_parking_8018B42C[20] = {
#include "assets/mist_parking_animation_0DE94_indices.inc"
};

GpAnimSet D_mist_parking_8018B454 = {
    D_mist_parking_8018B248, D_mist_parking_8018B42C,
    { NULL, D_mist_parking_8018B07C.words, NULL, NULL, D_mist_parking_8018B0F4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} MistParkingPoseBankDEBC;

MistParkingPoseBankDEBC D_mist_parking_8018B47C = { .poses = {
#include "assets/mist_parking_animation_0E1D0_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018B4DC[65] = {
#include "assets/mist_parking_animation_0E1D0_bank4.inc"
};

GpAnimRec D_mist_parking_8018B5E0[98] = {
#include "assets/mist_parking_animation_0E1D0_records.inc"
};

u16 D_mist_parking_8018B768[20] = {
#include "assets/mist_parking_animation_0E1D0_indices.inc"
};

GpAnimSet D_mist_parking_8018B790 = {
    D_mist_parking_8018B5E0, D_mist_parking_8018B768,
    { NULL, D_mist_parking_8018B47C.words, NULL, NULL, D_mist_parking_8018B4DC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[10];
    GpPackedSvec words[30];
} MistParkingPoseBankE1F8;

MistParkingPoseBankE1F8 D_mist_parking_8018B7B8 = { .poses = {
#include "assets/mist_parking_animation_0E6E0_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018B830[109] = {
#include "assets/mist_parking_animation_0E6E0_bank4.inc"
};

GpAnimRec D_mist_parking_8018B9E4[165] = {
#include "assets/mist_parking_animation_0E6E0_records.inc"
};

u16 D_mist_parking_8018BC78[20] = {
#include "assets/mist_parking_animation_0E6E0_indices.inc"
};

GpAnimSet D_mist_parking_8018BCA0 = {
    D_mist_parking_8018B9E4, D_mist_parking_8018BC78,
    { NULL, D_mist_parking_8018B7B8.words, NULL, NULL, D_mist_parking_8018B830, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} MistParkingPoseBankE708;

MistParkingPoseBankE708 D_mist_parking_8018BCC8 = { .poses = {
#include "assets/mist_parking_animation_0EA0C_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018BD1C[56] = {
#include "assets/mist_parking_animation_0EA0C_bank4.inc"
};

GpAnimRec D_mist_parking_8018BDFC[106] = {
#include "assets/mist_parking_animation_0EA0C_records.inc"
};

u16 D_mist_parking_8018BFA4[20] = {
#include "assets/mist_parking_animation_0EA0C_indices.inc"
};

GpAnimSet D_mist_parking_8018BFCC = {
    D_mist_parking_8018BDFC, D_mist_parking_8018BFA4,
    { NULL, D_mist_parking_8018BCC8.words, NULL, NULL, D_mist_parking_8018BD1C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} MistParkingPoseBankEA34;

MistParkingPoseBankEA34 D_mist_parking_8018BFF4 = { .poses = {
#include "assets/mist_parking_animation_0EDE0_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018C054[84] = {
#include "assets/mist_parking_animation_0EDE0_bank4.inc"
};

GpAnimRec D_mist_parking_8018C1A4[117] = {
#include "assets/mist_parking_animation_0EDE0_records.inc"
};

u16 D_mist_parking_8018C378[20] = {
#include "assets/mist_parking_animation_0EDE0_indices.inc"
};

GpAnimSet D_mist_parking_8018C3A0 = {
    D_mist_parking_8018C1A4, D_mist_parking_8018C378,
    { NULL, D_mist_parking_8018BFF4.words, NULL, NULL, D_mist_parking_8018C054, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} MistParkingPoseBankEE08;

MistParkingPoseBankEE08 D_mist_parking_8018C3C8 = { .poses = {
#include "assets/mist_parking_animation_0F14C_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018C41C[74] = {
#include "assets/mist_parking_animation_0F14C_bank4.inc"
};

GpAnimRec D_mist_parking_8018C544[104] = {
#include "assets/mist_parking_animation_0F14C_records.inc"
};

u16 D_mist_parking_8018C6E4[20] = {
#include "assets/mist_parking_animation_0F14C_indices.inc"
};

GpAnimSet D_mist_parking_8018C70C = {
    D_mist_parking_8018C544, D_mist_parking_8018C6E4,
    { NULL, D_mist_parking_8018C3C8.words, NULL, NULL, D_mist_parking_8018C41C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} MistParkingPoseBankF174;

MistParkingPoseBankF174 D_mist_parking_8018C734 = { .poses = {
#include "assets/mist_parking_animation_0F574_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018C770[90] = {
#include "assets/mist_parking_animation_0F574_bank4.inc"
};

GpAnimRec D_mist_parking_8018C8D8[141] = {
#include "assets/mist_parking_animation_0F574_records.inc"
};

u16 D_mist_parking_8018CB0C[20] = {
#include "assets/mist_parking_animation_0F574_indices.inc"
};

GpAnimSet D_mist_parking_8018CB34 = {
    D_mist_parking_8018C8D8, D_mist_parking_8018CB0C,
    { NULL, D_mist_parking_8018C734.words, NULL, NULL, D_mist_parking_8018C770, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[3];
    GpPackedSvec words[9];
} MistParkingPoseBankF59C;

MistParkingPoseBankF59C D_mist_parking_8018CB5C = { .poses = {
#include "assets/mist_parking_animation_0F7F0_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018CB80[43] = {
#include "assets/mist_parking_animation_0F7F0_bank4.inc"
};

GpAnimRec D_mist_parking_8018CC2C[87] = {
#include "assets/mist_parking_animation_0F7F0_records.inc"
};

u16 D_mist_parking_8018CD88[20] = {
#include "assets/mist_parking_animation_0F7F0_indices.inc"
};

GpAnimSet D_mist_parking_8018CDB0 = {
    D_mist_parking_8018CC2C, D_mist_parking_8018CD88,
    { NULL, D_mist_parking_8018CB5C.words, NULL, NULL, D_mist_parking_8018CB80, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[5];
    GpPackedSvec words[15];
} MistParkingPoseBankF818;

MistParkingPoseBankF818 D_mist_parking_8018CDD8 = { .poses = {
#include "assets/mist_parking_animation_0FC60_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018CE14[99] = {
#include "assets/mist_parking_animation_0FC60_bank4.inc"
};

GpAnimRec D_mist_parking_8018CFA0[150] = {
#include "assets/mist_parking_animation_0FC60_records.inc"
};

u16 D_mist_parking_8018D1F8[20] = {
#include "assets/mist_parking_animation_0FC60_indices.inc"
};

GpAnimSet D_mist_parking_8018D220 = {
    D_mist_parking_8018CFA0, D_mist_parking_8018D1F8,
    { NULL, D_mist_parking_8018CDD8.words, NULL, NULL, D_mist_parking_8018CE14, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[4];
    GpPackedSvec words[12];
} MistParkingPoseBankFC88;

MistParkingPoseBankFC88 D_mist_parking_8018D248 = { .poses = {
#include "assets/mist_parking_animation_0FE5C_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018D278[17] = {
#include "assets/mist_parking_animation_0FE5C_bank4.inc"
};

GpAnimRec D_mist_parking_8018D2BC[78] = {
#include "assets/mist_parking_animation_0FE5C_records.inc"
};

u16 D_mist_parking_8018D3F4[20] = {
#include "assets/mist_parking_animation_0FE5C_indices.inc"
};

GpAnimSet D_mist_parking_8018D41C = {
    D_mist_parking_8018D2BC, D_mist_parking_8018D3F4,
    { NULL, D_mist_parking_8018D248.words, NULL, NULL, D_mist_parking_8018D278, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} MistParkingPoseBankFE84;

MistParkingPoseBankFE84 D_mist_parking_8018D444 = { .poses = {
#include "assets/mist_parking_animation_10174_bank1.inc"
} };

GpPackedSvec D_mist_parking_8018D498[54] = {
#include "assets/mist_parking_animation_10174_bank4.inc"
};

GpAnimRec D_mist_parking_8018D570[103] = {
#include "assets/mist_parking_animation_10174_records.inc"
};

u16 D_mist_parking_8018D70C[20] = {
#include "assets/mist_parking_animation_10174_indices.inc"
};

GpAnimSet D_mist_parking_8018D734 = {
    D_mist_parking_8018D570, D_mist_parking_8018D70C,
    { NULL, D_mist_parking_8018D444.words, NULL, NULL, D_mist_parking_8018D498, NULL, NULL, NULL },
};

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists and the high halfword one of
/// the group's four; `Mc_SaveData[0].state.gameMode` 2 and above has groups of its own,
/// and anything unmatched falls back to `D_mist_parking_80186534`.
static u16* func_mist_parking_8017D8F8(s32 mode)
{
    if (Mc_SaveData[0].state.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186138;
                    case 1:
                        return D_mist_parking_80186150;
                    case 2:
                        return D_mist_parking_80186164;
                    case 3:
                        return D_mist_parking_8018616C;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186180;
                    case 1:
                        return D_mist_parking_8018619C;
                    case 2:
                        return D_mist_parking_801861AC;
                    case 3:
                        return D_mist_parking_801861B8;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801861D0;
                    case 1:
                        return D_mist_parking_801861EC;
                    case 2:
                        return D_mist_parking_80186200;
                    case 3:
                        return D_mist_parking_80186208;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_8018621C;
                    case 1:
                        return D_mist_parking_8018623C;
                    case 2:
                        return D_mist_parking_8018624C;
                    case 3:
                        return D_mist_parking_80186258;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186080;
                    case 1:
                        return D_mist_parking_80186090;
                    case 2:
                        return D_mist_parking_801860A0;
                    case 3:
                        return D_mist_parking_801860A8;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801860F4;
                    case 1:
                        return D_mist_parking_8018610C;
                    case 2:
                        return D_mist_parking_80186120;
                    case 3:
                        return D_mist_parking_80186128;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801860B8;
                    case 1:
                        return D_mist_parking_801860C8;
                    case 2:
                        return D_mist_parking_801860D8;
                    case 3:
                        return D_mist_parking_801860E0;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186058;
                    case 1:
                        return D_mist_parking_80186060;
                    case 2:
                        return D_mist_parking_80186068;
                    case 3:
                        return D_mist_parking_80186070;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186304;
                    case 1:
                        return D_mist_parking_80186310;
                    case 2:
                        return D_mist_parking_80186318;
                    case 3:
                        return D_mist_parking_80186324;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186330;
                    case 1:
                        return D_mist_parking_8018633C;
                    case 2:
                        return D_mist_parking_80186340;
                    case 3:
                        return D_mist_parking_8018634C;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186358;
                    case 1:
                        return D_mist_parking_80186364;
                    case 2:
                        return D_mist_parking_8018636C;
                    case 3:
                        return D_mist_parking_80186378;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186384;
                    case 1:
                        return D_mist_parking_80186390;
                    case 2:
                        return D_mist_parking_80186398;
                    case 3:
                        return D_mist_parking_801863A4;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186290;
                    case 1:
                        return D_mist_parking_80186298;
                    case 2:
                        return D_mist_parking_801862A0;
                    case 3:
                        return D_mist_parking_801862A8;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801862DC;
                    case 1:
                        return D_mist_parking_801862E8;
                    case 2:
                        return D_mist_parking_801862F0;
                    case 3:
                        return D_mist_parking_801862F8;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801862B4;
                    case 1:
                        return D_mist_parking_801862BC;
                    case 2:
                        return D_mist_parking_801862C8;
                    case 3:
                        return D_mist_parking_801862D0;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186270;
                    case 1:
                        return D_mist_parking_80186274;
                    case 2:
                        return D_mist_parking_80186278;
                    case 3:
                        return D_mist_parking_80186280;
                }
                break;
        }
    }
    return D_mist_parking_80186534;
}

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
extern u8           D_mist_parking_80186480[];
extern u8           D_mist_parking_8018646C[];
extern u8           D_mist_parking_8018647C[];
extern UiObjectDesc D_mist_parking_801865E4;
extern UiObjectDesc D_mist_parking_80186638;

/// Draws one row of the shop list and handles its input. Row 0xFFFE is greyed
/// out unless `Gp_HasMappedItem` answers non-zero and opens its own panel;
/// row 0xFFFC is greyed out while the scan holds item 0x8F. Any other row is
/// an item with its price, greyed out when `func_800B7420` refuses it; confirm
/// opens the buy panel and button 0x10 the item's detail panel.
void func_mist_parking_8017DF68(UiList* prompt, UiObject* obj)
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
        D_mist_parking_8018644C = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->panel.field_0.w;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam(D_mist_parking_80186480, 0, 0);
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
        Text_DrawString(&req, D_mist_parking_8018646C);
        if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            Ui_SpawnFromDesc(&D_mist_parking_80186638, 0, 1, 1, obj);
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
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_8018647C, prompt->field_1C, 1, 0);
        if (prompt->field_C == 1 && blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            child = Ui_SpawnFromDesc(&D_mist_parking_801865E4, itemId, 1, 1, obj);
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
            child2 = Ui_SpawnFromDesc(&D_mist_parking_801865E4, itemId, 1, 1, obj);
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
/// the same kind is overwritten only by a higher level.
static void func_mist_parking_8017E3F4(RoomShopList* shop, UiObject* obj, s32 item)
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

extern RoomShopTier D_mist_parking_801863B0[13];

/// Fills `shop` with the ids the vending machine currently offers, then sorts
/// them by `Gp_ItemSortKey` and caps the visible row count at 9.
///
/// The upper halfword of the owning task's `spawnArg1` picks the machine's
/// mode, which decides both the fixed id list (`func_mist_parking_8017D8F8`)
/// and which of a price row's items the machine will stock: mode 0 takes tools
/// (0x80-0x9F) plus a handful of key items, mode 1 armour (0xA0-0xBF), mode 2
/// weapon parts (0x60-0x7F) and mode 3 everything up to 0x5F that the other
/// three modes do not carry. Mode 3 additionally offers the twelve two-bit
/// stock levels the save keeps in `Mc_SaveData[0].state.shopStock`, whose first slot
/// needs a level of 2 rather than 1.
static void func_mist_parking_8017E540(RoomShopList* shop, UiObject* obj)
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
    ids  = func_mist_parking_8017D8F8(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        func_mist_parking_8017E3F4(shop, obj, *ids);
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
                        item = D_mist_parking_801863B0[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    func_mist_parking_8017E3F4(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    func_mist_parking_8017E3F4(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    func_mist_parking_8017E3F4(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    func_mist_parking_8017E3F4(shop, obj, item);
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
                    func_mist_parking_8017E3F4(shop, obj, slot * 3 + (id = level + 0xE));
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
    D_mist_parking_8018644C = -1;
}

/// Titles and labels of the shop's panels.
static const u8 D_mist_parking_8017D6D0[] = "Select";
static const u8 D_mist_parking_8017D6D8[] = "BP";
static const u8 D_mist_parking_8017D6DC[] = "List";
static const u8 D_mist_parking_8017D6E4[] = "TOTAL";
static const u8 D_mist_parking_8017D6EC[] = "Notice";

/// "Charge", with a stray non-zero byte after its terminator that C cannot
/// place, so the string stays assembly.
/// "Charge", followed by the non-zero padding the original toolchain left.
static const char D_mist_parking_8017D6F4[8] = "Charge\0\xE2";

/// Messages of the shop's panels.
extern u8 D_mist_parking_80186450[];
extern u8 D_mist_parking_80186464[];
extern u8 D_mist_parking_801864BC[];
extern u8 D_mist_parking_801864C4[];
extern u8 D_mist_parking_801864D0[];
extern u8 D_mist_parking_801864D8[];
extern u8 D_mist_parking_801864E0[];
extern u8 D_mist_parking_801864F4[];
extern u8 D_mist_parking_80186504[];
extern u8 D_mist_parking_80186524[];
extern u8 D_mist_parking_80186530[];

/// Row handlers, lists and panel descriptors of the shop's panels.
extern UiListItemFunc D_mist_parking_80186538[];
extern UiList         D_mist_parking_80186540;
extern UiList         D_mist_parking_8018656C;
extern UiObjectDesc   D_mist_parking_801865AC;
extern UiObjectDesc   D_mist_parking_801865C8;
extern UiObjectDesc   D_mist_parking_80186600;
extern UiObjectDesc   D_mist_parking_80186654;
extern UiObjectDesc   D_mist_parking_80186670;

/// Work pair of the charge panel `func_mist_parking_8017F49C`.
extern s32        D_mist_parking_80195310;
extern GpItemMap* D_mist_parking_80195314;

/// The shop's "Select" panel. On its first frame it allocates the
/// `RoomShopList` work block, fills it through
/// `func_mist_parking_8017E540` and opens the panel
/// `D_mist_parking_80186654` beside it. Every frame it draws the list and the
/// "BP" caption; menu reports -1 and cancel 6 to the parent. A child that
/// reports 6 is torn down and the list takes input again; one that reports -1
/// passes it up.
void func_mist_parking_8017E90C(Task* task)
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
    Ui_DrawText(&(obj)->panel, (char*)D_mist_parking_8017D6D0);
    if (task->state == 0) {
        mem = memCalloc(sizeof(RoomShopList), 0);
        if (mem != NULL) {
            shop               = mem;
            task->work         = (TaskIdMap*)shop;
            shop->list.funcs   = D_mist_parking_80186538;
            shop->list.field_6 = 0;
            shop->list.field_7 = 0xF;
            func_mist_parking_8017E540(shop, obj);
            Ui_LayoutListPanel(&shop->list, &(obj)->panel);
            shop->list.field_A = 1;
            Ui_SetListScrollFlag(&shop->list, 1);
            obj->panel.bounds.unsignedRect.h += 8;
            shop->list.field_17               = 8;
            Ui_SpawnFromDesc(&D_mist_parking_80186654, 0, 0, 0, obj);
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
    Text_DrawString(&req, D_mist_parking_8017D6D8);

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

void func_mist_parking_8017EB5C(UiList* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_80186464, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = D_mist_parking_801864BC;
    obj->owner->spawnArg1.value = (u16)obj->owner->spawnArg1.value;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = D_mist_parking_801864C4;
            obj->owner->spawnArg1.value |= 0x10000;
            break;
        case 2:
            text                   = D_mist_parking_801864D0;
            obj->owner->spawnArg1.value |= 0x20000;
            break;
        case 3:
            text                   = D_mist_parking_801864D8;
            obj->owner->spawnArg1.value |= 0x30000;
            break;
    }

    if (*func_mist_parking_8017D8F8(obj->owner->spawnArg1.value) == 0xFFFF) {
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
        Ui_SpawnFromDesc(&D_mist_parking_801865AC, obj->owner->spawnArg1, 1, 1, obj);
        obj->panel.field_0.w = 0;
    }
}

void func_mist_parking_8017ED7C(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2.pointer;
    list          = &D_mist_parking_80186540;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, (char*)D_mist_parking_8017D6DC);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_mist_parking_801865C8, task->spawnArg1, 0, 1, obj);
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

void func_mist_parking_8017EF24(Task* task)
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
    Text_DrawString(&req0, D_mist_parking_8017D6D8);

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
    Text_DrawString(&req1, (char*)D_mist_parking_8017D6E4);

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

void func_mist_parking_8017F108(UiList* prompt, UiObject* obj)
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
    Text_DrawString(&req, D_mist_parking_80186450);

    mode = prompt->field_C;
    if (mode == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        cfg   = &Player_Status;
        price = Gp_ItemDescs[itemId].price;
        scan  = &Mc_SaveData[0].state.carriedItems;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_mist_parking_80186600, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_mist_parking_80186600, 1, 1, 1, obj);
                }
                obj->panel.field_0.w = 0;
            } else if ((obj->owner->parent->spawnArg1.value >> 16) == mode) {
                child = Ui_SpawnFromDesc(&D_mist_parking_80186670, itemId, 1, 1, obj);
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
            Ui_SpawnFromDesc(&D_mist_parking_80186600, 0, 1, 1, obj);
            obj->panel.field_0.w = 0;
        }
    }
}

void func_mist_parking_8017F31C(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1.value;
    obj  = task->spawnArg2.pointer;
    switch (kind) {
        case 1:
            text = D_mist_parking_801864F4;
            break;
        case 2:
            text = D_mist_parking_80186504;
            break;
        default:
            text = D_mist_parking_801864E0;
            break;
    }

    Ui_DrawText(&(obj)->panel, (char*)D_mist_parking_8017D6EC);
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

/// Panel that walks the mapped item slots one at a time: each slot's ammo or
/// attachment quantity is reset to its related quantity, and a bar animates
/// from the old value up to the new one for at most 0xBC frames. Confirm or
/// cancel (or the timer running out) moves to the next slot; when no slot is
/// left the panel reports code 6 to its parent.
void func_mist_parking_8017F49C(Task* task)
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
    Ui_DrawText(&(obj)->panel, (char*)D_mist_parking_8017D6F4);

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
            map                     = Gp_GetItemMap(slotId);
            D_mist_parking_80195314 = map;
            itemId                  = map->field_1;
            slot                    = Gp_GetItemSlot(itemId);
            if (D_mist_parking_80195314->field_0 == 0) {
                D_mist_parking_80195310 = slot->ammoQty;
                slot->ammoQty           = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_mist_parking_80195310 = slot->attachQty;
                slot->attachQty         = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown       = 0xBC;
            D_mist_parking_80195310 <<= 8;
            task->state               = task->state + 1;
        }
    }

    curItem = D_mist_parking_80195314->field_1;
    relItem = D_mist_parking_80195314->field_2;
    if (D_mist_parking_80195314->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                    <<= 8;
    D_mist_parking_80195310 += 0x40;
    if (qty < D_mist_parking_80195310) {
        D_mist_parking_80195310 = qty;
    }

    y = (s16)obj->panel.field_18.u;
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar(&(obj)->panel, (s16)obj->panel.field_1C.s, (s16)obj->panel.field_1E.u, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, D_mist_parking_80195310 >> 8, 0x606060);
    h = (s16)obj->panel.field_1A.u;
    func_800C0E20(&(obj)->panel, (s16)obj->panel.field_1C.s + 2, (s16)obj->panel.field_1E.u - 2, h - 6, qty,
                  D_mist_parking_80195310, 0x1741F);

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

static inline s32 _mist_parkingAddItemCount(s32 item, s32 count)
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
void func_mist_parking_8017F764(Task* task)
{
    u8          buf[0x10];
    TextDrawReq req;
    UiObject*   obj;
    s32         item;
    s32         y;
    s32         ry;
    s32         count;

    item         = D_mist_parking_8018644C;
    obj          = task->spawnArg2.pointer;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && D_mist_parking_8018644C == Gp_GetPreviewItem()) {
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
        Text_DrawString(&req, D_mist_parking_80186524);
        count = 0;
        count = _mist_parkingAddItemCount(item, count);
        Text_DrawPrompt(obj, (s16)obj->panel.field_1E.u - 2, y + 0xA, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
    }
}

void func_mist_parking_8017F938(Task* task)
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
    Text_DrawPrompt(obj, left + 0x98, y, D_mist_parking_80186530, 0x606060, 3, 2);
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
    Text_DrawString(&req, D_mist_parking_8017D6D8);

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

void func_mist_parking_8017FDB8(UiList* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->panel.field_20.u + (u16)prompt->field_18;
    req.y          = obj->panel.field_22.u + (u16)prompt->field_1A;
    req.otIndex    = obj->panel.field_14.s + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, D_mist_parking_80186464);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

void func_mist_parking_8017FE74(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_mist_parking_8018656C;
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

/// Descriptor of the panel `func_mist_parking_8017FF9C` opens.
extern UiObjectDesc D_mist_parking_80186590;

/// Opens the panel `D_mist_parking_80186590` with the task's `spawnArg1` as
/// its parameter, setting frame timing 0 and the session's UI flag while it is
/// open; once the panel reports -1 or 6 it is torn down, and ten frames later
/// frame timing 1 and the flag are restored and the task kills itself.
void func_mist_parking_8017FF9C(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_mist_parking_80186590, task->spawnArg1, 1, 1, NULL);
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

/// Labels, suffix and holder texts of the play-data summary rows.
extern u8 D_mist_parking_801866C0[];
extern u8 D_mist_parking_801866F0[];
extern u8 D_mist_parking_801866C8[];
extern u8 D_mist_parking_801866CC[];
extern u8 D_mist_parking_801866D4[];
extern u8 D_mist_parking_801866E0[];
extern u8 D_mist_parking_801866F8[];
extern u8 D_mist_parking_80186700[];
extern u8 D_mist_parking_80186708[];
extern u8 D_mist_parking_80186710[];
extern u8 D_mist_parking_8018671C[];
extern u8 D_mist_parking_80186748[];
extern u8 D_mist_parking_8018676C[];
extern u8 D_mist_parking_8018679C[];
extern u8 D_mist_parking_801867D0[];
extern u8 D_mist_parking_80186804[];
extern u8 D_mist_parking_8018683C[];
extern u8 D_mist_parking_80186870[];
extern u8 D_mist_parking_801868A8[];

void func_mist_parking_801800D0(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_mist_parking_8018671C,
                D_mist_parking_80186748,
                D_mist_parking_8018676C,
                D_mist_parking_8018679C,
                D_mist_parking_801867D0,
                D_mist_parking_80186804,
                D_mist_parking_8018683C,
                D_mist_parking_80186870,
                D_mist_parking_801868A8,
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
            Text_DrawString(&req, D_mist_parking_801866C0);
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
            Text_DrawString(&req, D_mist_parking_801866F0);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, D_mist_parking_80186710);
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
            Text_DrawString(&req, D_mist_parking_801866C8);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, D_mist_parking_80186710);
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
            Text_DrawString(&req, D_mist_parking_801866CC);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, D_mist_parking_80186710);
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
            Text_DrawString(&req, D_mist_parking_801866D4);
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
            Text_Strcat(p, D_mist_parking_80186718);
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
            Text_DrawString(&req, D_mist_parking_801866E0);
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
            Text_Strcat(p, D_mist_parking_80186718);
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
            Text_DrawString(&req, D_mist_parking_801866F8);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, D_mist_parking_80186710);
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
            Text_DrawString(&req, D_mist_parking_80186700);
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
            Text_DrawString(&req, D_mist_parking_80186708);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data menu panel `func_mist_parking_80181920` draws.
static const char D_mist_parking_8017D748[] = "Play Data";

/// One row of the "Play Data" item-usage list: the item's name, its share of
/// all recorded uses as `NN.NN%` (or a flat `100.0%` once it is the only item
/// used), and a gauge whose width is the row's `barWidths` fraction of the
/// panel. Confirming the row opens the item's detail panel.

void func_mist_parking_8018089C(UiList* prompt, UiObject* obj)
{
    u8             buf[0x20];
    u8*            p;
    u8*            q;
    RoomItemUsage* work;
    POLY_G4*       prim;
    s32            itemId;
    s32            pct;
    s32            scale;
    s32            remaining;
    s32            i;
    s32            len;
    s32            n;
    s32            right;
    s32            lo;
    s32            barY;
    s32            ry;
    s32            color;
    s32            barW;
    s32            barX;
    s32            x0;
    s32            x1;
    s32            y0;
    s32            status;
    s32            one;
    s32            px;
    s32            py;
    TextDrawReq    req;
    TextDrawReq*   r;

    p = buf;
    /* The request's address is live across Gp_GetItemText, so the last field is
       written through it while the rest stay sp-relative. */
    r      = &req;
    work   = (RoomItemUsage*)obj->owner->work;
    itemId = work->itemIds[prompt->field_8];
    pct    = work->percents[prompt->field_8];
    px     = prompt->field_18;
    py     = prompt->field_1A;
    color  = prompt->field_1C;

    if (obj->panel.field_8 != 5) {
        req.x          = obj->panel.field_20.u + 0x11 + px;
        ry             = obj->panel.field_22.u - 6;
        req.y          = ry + py;
        req.otIndex    = obj->panel.field_14.s + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        Text_DrawString(r, (u8*)Gp_GetItemText(itemId, 0, 0));
        func_800CE5D0(obj, px, py, itemId);
    }

    if (pct >= 0x2710) {
        Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, "100.0%", prompt->field_1C, 3, 2);
    } else {
        scale     = 1;
        remaining = 2;
        do {
            scale *= 10;
            remaining--;
        } while (remaining > 0);

        if (pct < scale) {
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
        Text_Strcat(p, D_mist_parking_80186718);
        Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, buf, prompt->field_1C, 3, 2);
    }

    lo    = (s16)obj->panel.field_1C.s + 0x80;
    right = (s16)obj->panel.field_1E.u - 0x4A;
    barY  = (s16)prompt->field_1A - 0xC;
    barW  = right - lo;
    barW  = (barW * work->barWidths[prompt->field_8]) >> 12;
    barW += 2;
    barX  = right - barW;
    if (barW >= 2) {
        prim     = (POLY_G4*)gGpuPrimCursor;
        x0       = obj->panel.field_20.u + barX + 1;
        prim->x2 = x0;
        prim->x0 = x0;

        gGpuPrimCursor           = prim + 1;
        y0                       = obj->panel.field_22.u;
        y0                       = y0 + barY;
        y0                      += 1;
        PRIM_COLOR_WORD(prim, 3) = PRIM_RGBC(0, 0, 0x01, 0);
        PRIM_COLOR_WORD(prim, 1) = PRIM_RGBC(0, 0, 0x01, 0);
        setlen(prim, 8);
        PRIM_COLOR_WORD(prim, 0) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        setcode(prim, 0x38);
        PRIM_COLOR_WORD(prim, 2) = PRIM_RGBC(0xb0, 0, 0x01, 0);

        x1 = (u16)prim->x0 + barW;
        x1--;
        prim->y1 = y0;
        prim->y0 = y0;
        y0      += 8;
        prim->y3 = y0;
        prim->y2 = y0;
        prim->x3 = x1;
        prim->x1 = x1;
        addPrim(gGpuCurrentOt + obj->panel.field_14.s + 1, prim);
    }

    one = 1;
    Ui_DrawBeveledRect(&(obj)->panel, barX, (s16)prompt->field_1A - 0xC, barW, 9, 0, one);

    status = obj->panel.field_0.w;
    if (((status >> 16) == one) || (status == one)) {
        if (prompt->field_10 == prompt->field_8) {
            Gp_SetPreviewItem(itemId, 0);
            Gp_SetHolderItemText(itemId);
        }
    }

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, 0x10) != 0) {
        SndEvt_EnqueueType6(3, 0, 0);
        Ui_SpawnFromDesc(&D_8010EFA0, itemId, 1, 1, obj);
        obj->panel.field_0.w = 0;
    }
}

/// Titles of the two usage lists `func_mist_parking_801812B4` draws (item and
/// PE).
static const char D_mist_parking_8017D75C[] = "Weapon Data";
static const char D_mist_parking_8017D768[] = "PE Data";

/// "Telephone", the title `func_mist_parking_80181468` draws, with two stray
/// non-zero bytes after its terminator that C cannot place, so the string
/// stays assembly.
static const char D_mist_parking_8017D770[];
/// "Telephone", followed by the non-zero padding the original toolchain left.
static const char D_mist_parking_8017D770[12] = "Telephone\0\xF2\xEF";

/// Lists of the usage panel and of the play-data menu, and the descriptor of
/// the frame the usage panel spawns.
extern UiList       D_mist_parking_8018690C;
extern UiList       D_mist_parking_80186994;
extern UiObjectDesc D_mist_parking_80186930;

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
static void func_mist_parking_80180C98(UiList* list, UiObject* obj)
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
static void func_mist_parking_80180F94(UiList* list, UiObject* obj)
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
void func_mist_parking_801812B4(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &D_mist_parking_8018690C;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_mist_parking_8017D75C);
    } else {
        Ui_DrawText(&(obj)->panel, D_mist_parking_8017D768);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_mist_parking_80186930, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            func_mist_parking_80180C98(list, obj);
        } else {
            func_mist_parking_80180F94(list, obj);
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

void func_mist_parking_80181468(Task* task)
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
    list          = &D_mist_parking_80186994;
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
        Ui_DrawText(&(obj)->panel, D_mist_parking_8017D770);
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

void func_mist_parking_80181760(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = func_mist_parking_80181E50;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
static void func_mist_parking_801817BC(u8* str, s32 decimals)
{
    s32 len;

    len = 0;
    if (decimals > 0) {
        while (*str != 0) {
            str++;
            len++;
        }
        if (len < decimals) {
            decimals = len;
        }
        decimals += 1;
        for (len = 0; len < decimals; len++) {
            str[1] = str[0];
            str--;
        }
        str[1] = '.';
    }
}

/// Renders `value` into `buf` as a fixed-point number with `decimals` digits
/// after the point, then appends the overlay's "%" suffix. The integer is
/// printed first (zero-padded to `decimals + 1` digits when it is too small to
/// fill them), then the last `decimals` characters are shifted one byte right
/// to open a slot for the '.'.

static u8* func_mist_parking_8018182C(u8* buf, s32 value, s32 decimals)
{
    s32 remaining;
    s32 len;
    s32 shifted;
    s32 count;
    s32 scale;
    u8* p;

    scale     = 1;
    remaining = decimals;
    if (decimals > 0) {
        do {
            scale *= 10;
            remaining--;
        } while (remaining > 0);
    }

    if (value < scale) {
        Text_ItoaPadded(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    count = decimals;
    p     = buf;
    len   = 0;
    if (count > 0) {
        if (*buf != 0) {
            do {
                p++;
                len++;
            } while (*p != 0);
        }
        if (len < count) {
            count = len;
        }
        count++;

        shifted = 0;
        if (count > 0) {
            do {
                p[1] = p[0];
                shifted++;
                p--;
            } while (shifted < count);
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_mist_parking_80186718);
    return buf;
}

/// List of the menu panel `func_mist_parking_80181920` draws.
extern UiList D_mist_parking_801868E4;

/// Texts of the four menu rows below, and the panels two of them open.
extern u8           D_mist_parking_80186698[];
extern u8           D_mist_parking_801866A0[];
extern u8           D_mist_parking_801866AC[];
extern u8           D_mist_parking_801866B8[];
extern UiObjectDesc D_mist_parking_8018694C;
extern UiObjectDesc D_mist_parking_80186968;

void func_mist_parking_80181920(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_mist_parking_801868E4;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_mist_parking_8017D748);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_mist_parking_80186930, 0, 0, 1, obj);
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
static void func_mist_parking_80181A10(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

void func_mist_parking_80181B14(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_80186698, prompt->field_1C, 1, 0);
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

void func_mist_parking_80181BF8(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_801866A0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mist_parking_8018694C, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_mist_parking_80181CC0(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_801866AC, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mist_parking_80186968, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_mist_parking_80181D88(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mist_parking_801866B8, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mist_parking_80186968, 1, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Exit callback of a prompt task that registers its UI object as
/// `Wip_UiHolder`: releases the holder if the task still owns it, then frees
/// the UI object and kills the task.
static void func_mist_parking_80181E50(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// The area records applied when the scene hands the Dryfield story on.

/// The room's cutscene runner: suppresses the player and ally HUD, loads and
/// starts the scene's caption slot, lets confirm or cancel cut the scene
/// sub-task short, applies the story-flag side effects when the scene ends,
/// and restores everything before killing itself.
void func_mist_parking_80181E8C(Task* task)
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
            D_mist_parking_80195318 = NULL;
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
            D_mist_parking_80195318 = Task_SpawnFromTable(D_mist_parking_801869B8, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_mist_parking_80195318);
                task->state++;
            } else if (Task_PollKill(D_mist_parking_80195318, &killOut) != 0) {
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

extern GpEvsCmd D_mist_parking_80186EFC[];
extern GpEvsCmd D_mist_parking_8018F0A4[];
extern GpEvsCmd D_mist_parking_8018F194[];

/// The scene record the room hands the cutscene runner.
extern RoomCutsceneRec D_mist_parking_8019533C;

s32 func_mist_parking_801823F8(s32 arg0, s32 arg1, s32 arg2)
{
    GameSession* session;
    u8           temp;

    switch (arg2) {
        case 15:
            temp = gGameSession->at4.loc.place;
            if (temp == 2) {
                if (GameFlag_GetNibble(0xF1) == 1) {
                    Gp_MsgPlayerWeapon(0);
                    func_800E8614(D_mist_parking_8018F0A4, 1);
                    GameFlag_SetNibble(0xF1, 2);
                } else if (GameFlag_GetNibble(0xF1) == temp) {
                    Gp_MsgPlayerWeapon(0);
                    func_800E8614(D_mist_parking_8018F194, 1);
                    GameFlag_SetNibble(0xF1, 3);
                } else if (GameFlag_GetNibble(0xF1) == 3) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_mist_parking_8018D75C, 8, 0, 0);
                }
            } else if (GameFlag_GetNibble(0xED) == 1) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(D_mist_parking_80190824, 4, 0, 0);
            }
            break;
        case 8:
            D_mist_parking_8019533C.field_0  = 9;
            D_mist_parking_8019533C.field_1  = 1;
            D_mist_parking_8019533C.field_3  = 3;
            D_mist_parking_8019533C.field_2  = 0;
            D_mist_parking_8019533C.field_4  = 0x51130003;
            D_mist_parking_8019533C.field_8  = 0x51130004;
            D_mist_parking_8019533C.field_10 = 0x5113000B;
            D_mist_parking_8019533C.field_C  = 0x51130012;
            Task_SpawnFromTable(D_mist_parking_801869B8, 0, 4, &D_mist_parking_8019533C);
            session                     = gGameSession;
            Mc_SaveData[0].state.at4.loc.warp = 2;
            session->at4.loc.warp       = 2;
            break;
        case 18:
            Gp_MsgPlayerWeapon(0);
            if (gGameSession->at4.loc.place == 1) {
                Task_SpawnFromTable(D_mist_parking_80190824, 3, 0, 0);
            } else {
                Task_SpawnFromTable(D_mist_parking_8018D75C, 7, 0, 0);
            }
            break;
        case 1:
            func_800E8614(D_mist_parking_80186EFC, 1);
            break;
    }
    return 0;
}

static void func_mist_parking_801827C0(Task* arg0);
static void func_mist_parking_80182888(Task* task);

/// State handlers of the task `func_mist_parking_80182898` runs: its set-up,
/// an empty per-frame state and the kill.
static const TaskFuncTable3 D_mist_parking_8017D7DC = {
    {
        func_mist_parking_801827C0,
        func_mist_parking_80182888,
        taskKill,
    },
};

/// Plays the sound event in `spawnArg2` on its first frame and again at frame
/// 0x50, then asks for the task's own kill at frame 0x78.
void func_mist_parking_80182628(Task* task)
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

/// Handler that answers 0.
s32 func_mist_parking_801826B8(void)
{
    return 0;
}

/// Message handler that copies the location record it is given onto the
/// reply record and answers 1.
s32 func_mist_parking_801826C0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

extern MistParkingMessageEntry D_mist_parking_80186BB8[5];
extern GpEvsCmd D_mist_parking_80186C5C[];
extern GpEvsCmd D_mist_parking_80186DC4[];
extern GpEvsCmd D_mist_parking_8018DF34[];
extern GpEvsCmd D_mist_parking_8018EDBC[];
extern GpEvsCmd D_mist_parking_8018EFE4[];

s32 func_mist_parking_801826E8(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        func_800E8614(D_mist_parking_80186C5C, 1);
    }
    if (arg2->field_2 == 2) {
        func_800E8614(D_mist_parking_80186DC4, 1);
        GameFlag_SetNibble(0xED, 1);
    }
    return 1;
}

void func_mist_parking_80182750(s32 arg0)
{
    if (GameFlag_GetNibble(0x7A) != 0) {
        arg0 += 2;
    }
    Mc_SaveData[0].state.at4.loc.room = arg0;
    gGameSession->at4.loc.room  = arg0;
    gGameSession->roomObjsDirty = 1;
}

void func_mist_parking_801827A0(s32 arg0)
{
    Gp_SpawnIfCapIdle(arg0, 0);
}

static void func_mist_parking_801827C0(Task* arg0)
{
    arg0->msgTable = D_mist_parking_80186BB8;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.place == 2) && (GameFlag_GetNibble(0xF1) == 0)) {
        if (Mc_SaveData[0].state.at4.loc.warp == 3) {
            func_800E3FAC(0xA2, 0x3C);
            func_mist_parking_801837A4(0);
            func_800E8634(D_mist_parking_8018DF34, 0, D_mist_parking_8018EDBC);
        } else {
            func_mist_parking_8018471C(0);
            func_800E8614(D_mist_parking_8018EFE4, 1);
        }
    }
    arg0->state = arg0->state + 1;
}

/// The empty per-frame state of `D_mist_parking_8017D7DC`.
static void func_mist_parking_80182888(Task* task)
{
    char pad[0x10];
}

extern GpCopyArg D_mist_parking_8018D82C;
extern s8  D_mist_parking_8018DA28[];

/// Runs the handler for the task's state from a stack copy of
/// `D_mist_parking_8017D7DC`.
void func_mist_parking_80182898(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_parking_8017D7DC;
    sp.funcs[task->state](task);
}

void func_mist_parking_801828F0(Task* task)
{
    GameActor* actor;
    GpWorkObj* work;
    s32        idx;
    s32        flag;
    u16        tick;

    actor = (GameActor*)(gameGetPtrSlot(3))->work;
    if (D_801156F9 == 0) {
        idx = actor->field_438[1].nextSet - 0x2F;
        if ((idx > 0) && (idx < D_mist_parking_8018D82C.count)) {
            flag = D_mist_parking_8018DA28[idx];
        } else {
            flag = 0;
        }
        if (task->state == 0) {
            if ((flag != 0) || (task->spawnArg1.value != 0)) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            work = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
            func_800B0928(gameGetPtrSlot(3), work->field_0, 0x200, 0x100, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}
