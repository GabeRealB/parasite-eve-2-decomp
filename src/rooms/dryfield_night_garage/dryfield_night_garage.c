#include "dryfield_night_garage_private.h"
#include "mapui/map_dryfield_full.h"

#include "common.h"

#include <psyq/libgte.h>
#include "rooms/dryfield_night_garage.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/player_state.h"
#include "gameplay/scene_runtime.h"

#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/message.h"
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

extern GpObj4C D_dryfield_night_garage_80186D7C[16];

/// Task descriptor table and cutscene script blobs owned by the main
/// executable.
extern TaskDesc D_8013B11C[];
extern s32      D_8013B570;
extern s32      D_8013B590;
extern s32      D_8013C388;

/// The 0xFFFF-terminated item id lists `func_dryfield_night_garage_8017D754`
/// chooses from, and the one it returns when no case matches.
extern u16 D_dryfield_night_garage_801815F8[];
extern u16 D_dryfield_night_garage_80181600[];
extern u16 D_dryfield_night_garage_80181608[];
extern u16 D_dryfield_night_garage_80181610[];
extern u16 D_dryfield_night_garage_80181620[];
extern u16 D_dryfield_night_garage_80181630[];
extern u16 D_dryfield_night_garage_80181640[];
extern u16 D_dryfield_night_garage_80181648[];
extern u16 D_dryfield_night_garage_80181658[];
extern u16 D_dryfield_night_garage_80181668[];
extern u16 D_dryfield_night_garage_80181678[];
extern u16 D_dryfield_night_garage_80181680[];
extern u16 D_dryfield_night_garage_80181694[];
extern u16 D_dryfield_night_garage_801816AC[];
extern u16 D_dryfield_night_garage_801816C0[];
extern u16 D_dryfield_night_garage_801816C8[];
extern u16 D_dryfield_night_garage_801816D8[];
extern u16 D_dryfield_night_garage_801816F0[];
extern u16 D_dryfield_night_garage_80181704[];
extern u16 D_dryfield_night_garage_8018170C[];
extern u16 D_dryfield_night_garage_80181720[];
extern u16 D_dryfield_night_garage_8018173C[];
extern u16 D_dryfield_night_garage_8018174C[];
extern u16 D_dryfield_night_garage_80181758[];
extern u16 D_dryfield_night_garage_80181770[];
extern u16 D_dryfield_night_garage_8018178C[];
extern u16 D_dryfield_night_garage_801817A0[];
extern u16 D_dryfield_night_garage_801817A8[];
extern u16 D_dryfield_night_garage_801817BC[];
extern u16 D_dryfield_night_garage_801817DC[];
extern u16 D_dryfield_night_garage_801817EC[];
extern u16 D_dryfield_night_garage_801817F8[];
extern u16 D_dryfield_night_garage_80181810[];
extern u16 D_dryfield_night_garage_80181814[];
extern u16 D_dryfield_night_garage_80181818[];
extern u16 D_dryfield_night_garage_80181820[];
extern u16 D_dryfield_night_garage_80181830[];
extern u16 D_dryfield_night_garage_80181838[];
extern u16 D_dryfield_night_garage_80181840[];
extern u16 D_dryfield_night_garage_80181848[];
extern u16 D_dryfield_night_garage_80181854[];
extern u16 D_dryfield_night_garage_8018185C[];
extern u16 D_dryfield_night_garage_80181868[];
extern u16 D_dryfield_night_garage_80181870[];
extern u16 D_dryfield_night_garage_8018187C[];
extern u16 D_dryfield_night_garage_80181888[];
extern u16 D_dryfield_night_garage_80181890[];
extern u16 D_dryfield_night_garage_80181898[];
extern u16 D_dryfield_night_garage_801818A4[];
extern u16 D_dryfield_night_garage_801818B0[];
extern u16 D_dryfield_night_garage_801818B8[];
extern u16 D_dryfield_night_garage_801818C4[];
extern u16 D_dryfield_night_garage_801818D0[];
extern u16 D_dryfield_night_garage_801818DC[];
extern u16 D_dryfield_night_garage_801818E0[];
extern u16 D_dryfield_night_garage_801818EC[];
extern u16 D_dryfield_night_garage_801818F8[];
extern u16 D_dryfield_night_garage_80181904[];
extern u16 D_dryfield_night_garage_8018190C[];
extern u16 D_dryfield_night_garage_80181918[];
extern u16 D_dryfield_night_garage_80181924[];
extern u16 D_dryfield_night_garage_80181930[];
extern u16 D_dryfield_night_garage_80181938[];
extern u16 D_dryfield_night_garage_80181944[];
extern u16 D_dryfield_night_garage_80181AD4[];

/// The shop's price ladder.
extern RoomShopTier D_dryfield_night_garage_80181950[13];

/// The item id the shop list's cursor last rested on.
extern s32 D_dryfield_night_garage_801819EC;

/// Messages and labels of the shop's panels.
extern u8 D_dryfield_night_garage_801819F0[];
extern u8 D_dryfield_night_garage_80181A04[];
extern u8 D_dryfield_night_garage_80181A0C[];
extern u8 D_dryfield_night_garage_80181A1C[];
extern u8 D_dryfield_night_garage_80181A20[];
extern u8 D_dryfield_night_garage_80181A5C[];
extern u8 D_dryfield_night_garage_80181A64[];
extern u8 D_dryfield_night_garage_80181A70[];
extern u8 D_dryfield_night_garage_80181A78[];
extern u8 D_dryfield_night_garage_80181A80[];
extern u8 D_dryfield_night_garage_80181A94[];
extern u8 D_dryfield_night_garage_80181AA4[];
extern u8 D_dryfield_night_garage_80181AC4[];
extern u8 D_dryfield_night_garage_80181AD0[];

/// Row handlers, lists and panel descriptors of the shop's panels.
extern UiListItemFunc D_dryfield_night_garage_80181AD8[];
extern UiList         D_dryfield_night_garage_80181AE0;
extern UiList         D_dryfield_night_garage_80181B0C;
extern UiObjectDesc   D_dryfield_night_garage_80181B30;
extern UiObjectDesc   D_dryfield_night_garage_80181B4C;
extern UiObjectDesc   D_dryfield_night_garage_80181B68;
extern UiObjectDesc   D_dryfield_night_garage_80181B84;
extern UiObjectDesc   D_dryfield_night_garage_80181BA0;
extern UiObjectDesc   D_dryfield_night_garage_80181BD8;
extern UiObjectDesc   D_dryfield_night_garage_80181BF4;
extern UiObjectDesc   D_dryfield_night_garage_80181C10;

/// The room's own `GpMsgEntry[]` - the message table `func_dryfield_night_garage_8017FF2C`
/// publishes in `Task::msgTable`. It terminates with id 0x7FFFFFFF.
extern GpMsgEntry D_dryfield_night_garage_80181C38[];

/// Ally animation descriptor handed to `Gp_AllyAnimId`, then forwarded as the
/// payload of the 0x3E8 message.
extern GpAnimArg D_dryfield_night_garage_80181C68;

/// Script blob passed to `func_800E8614` when game flag 0x8E is already set.
extern GpEvsCmd D_dryfield_night_garage_80181C7C[];

/// Two layout templates and the live copy the resets restore from them.
extern GpGridParams D_dryfield_night_garage_80181D7C;
extern GpGridParams D_dryfield_night_garage_80181E40;
extern GpGridParams D_dryfield_night_garage_80183DD4;

extern TaskDesc D_dryfield_night_garage_80182C98[];
extern s32      D_dryfield_night_garage_80182DE0;
extern s32      D_dryfield_night_garage_80182DE4;
extern GpEvsCmd D_dryfield_night_garage_80182DF8[];
extern GpEvsCmd D_dryfield_night_garage_801831B8[];
/// The room's display nodes; bit 0x40 of a node's `field_4A` shows it. The
/// room toggles the first node and the third.

/// Work pair of the charge panel `func_dryfield_night_garage_8017F2F8`: the
/// animated quantity in 24.8 fixed point, and the item map of the slot being
/// charged.
extern s32        D_dryfield_night_garage_80187628;
extern GpItemMap* D_dryfield_night_garage_8018762C;

static void func_dryfield_night_garage_80180604(s32 arg0);

void func_dryfield_night_garage_8017DDC4(UiList *, UiObject *);

void func_dryfield_night_garage_8017E768(Task *);
void func_dryfield_night_garage_8017E9B8(UiList *, UiObject *);
void func_dryfield_night_garage_8017EBD8(Task *);
void func_dryfield_night_garage_8017ED80(Task *);
void func_dryfield_night_garage_8017EF64(UiList *, UiObject *);
void func_dryfield_night_garage_8017F178(Task *);
void func_dryfield_night_garage_8017FC14(UiList *, UiObject *);
void func_dryfield_night_garage_8017FCD0(Task *);

s32 func_dryfield_night_garage_801800C8(Task *, s32, GpMsg13EF *, s32);
s32 func_dryfield_night_garage_80180300(Task *, s32, s32, GpMessageArg);
s32 func_dryfield_night_garage_80180358(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_garage_80180360(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_night_garage_801803A4(Task *, s32, GpMessageArg, GpMessageArg);
void func_dryfield_night_garage_8017E768(Task *);
void func_dryfield_night_garage_8017F2F8(Task *);
void func_dryfield_night_garage_8017F5C0(Task *);
void func_dryfield_night_garage_8017F794(Task *);
void func_dryfield_night_garage_8017FDF8(Task *);

u16 D_dryfield_night_garage_801815F8[4] = {
    140, 143, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181600[4] = {
    172, 175, 0xFFFE, 0xFFFF,
};

u16 D_dryfield_night_garage_80181608[4] = {
    103, 98, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181610[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181620[8] = {
    131, 140, 143, 10, 70, 138, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181630[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181640[4] = {
    108, 100, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181648[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181658[8] = {
    132, 140, 143, 10, 70, 66, 138, 0xFFFF,
};

u16 D_dryfield_night_garage_80181668[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181678[4] = {
    98, 105, 106, 0xFFFF,
};

u16 D_dryfield_night_garage_80181680[10] = {
    65, 59, 58, 1, 2, 6, 8, 4,
    0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181694[12] = {
    131, 157, 140, 142, 143, 10, 70, 69,
    67, 138, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_801816AC[10] = {
    160, 161, 172, 173, 171, 169, 175, 0xFFFE,
    0xFFFF, 0,
};

u16 D_dryfield_night_garage_801816C0[4] = {
    108, 100, 102, 0xFFFF,
};

u16 D_dryfield_night_garage_801816C8[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_801816D8[12] = {
    157, 9, 140, 142, 138, 143, 10, 70,
    69, 66, 67, 0xFFFF,
};

u16 D_dryfield_night_garage_801816F0[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_dryfield_night_garage_80181704[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_dryfield_night_garage_8018170C[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_dryfield_night_garage_80181720[14] = {
    157, 9, 140, 142, 143, 10, 70, 69,
    66, 67, 68, 138, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_8018173C[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_8018174C[6] = {
    103, 98, 100, 97, 107, 0xFFFF,
};

u16 D_dryfield_night_garage_80181758[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181770[14] = {
    140, 142, 138, 143, 10, 70, 69, 66,
    67, 68, 157, 9, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_8018178C[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_dryfield_night_garage_801817A0[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_dryfield_night_garage_801817A8[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_dryfield_night_garage_801817BC[16] = {
    140, 142, 138, 139, 143, 10, 70, 69,
    66, 67, 68, 144, 157, 9, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_801817DC[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_801817EC[6] = {
    100, 98, 97, 103, 107, 0xFFFF,
};

u16 D_dryfield_night_garage_801817F8[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181810[2] = {
    139, 0xFFFF,
};

u16 D_dryfield_night_garage_80181814[2] = {
    171, 0xFFFF,
};

u16 D_dryfield_night_garage_80181818[4] = {
    108, 13, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181820[8] = {
    65, 59, 58, 60, 11, 55, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181830[4] = {
    131, 138, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181838[4] = {
    160, 171, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181840[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_dryfield_night_garage_80181848[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_night_garage_80181854[4] = {
    140, 138, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_8018185C[6] = {
    160, 172, 171, 175, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181868[4] = {
    98, 13, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181870[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_night_garage_8018187C[6] = {
    131, 138, 143, 70, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181888[4] = {
    160, 171, 175, 0xFFFF,
};

u16 D_dryfield_night_garage_80181890[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_dryfield_night_garage_80181898[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_night_garage_801818A4[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_801818B0[4] = {
    171, 175, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_801818B8[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_801818C4[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_night_garage_801818D0[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_801818DC[2] = {
    171, 0xFFFF,
};

u16 D_dryfield_night_garage_801818E0[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_dryfield_night_garage_801818EC[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_night_garage_801818F8[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_dryfield_night_garage_80181904[4] = {
    171, 175, 0xFFFE, 0xFFFF,
};

u16 D_dryfield_night_garage_8018190C[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181918[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_dryfield_night_garage_80181924[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_dryfield_night_garage_80181930[4] = {
    171, 0xFFFE, 0xFFFF, 0,
};

u16 D_dryfield_night_garage_80181938[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_dryfield_night_garage_80181944[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

RoomShopTier D_dryfield_night_garage_80181950[13] = {
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

s32 D_dryfield_night_garage_801819EC = -1;

u8 D_dryfield_night_garage_801819F0[20] = {
    80, 117, 114, 99, 104, 97, 115, 101, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
};

u8 D_dryfield_night_garage_80181A04[8] = {
    80, 97, 115, 115, 0, 0, 0, 0,
};

u8 D_dryfield_night_garage_80181A0C[16] = {
    66, 97, 116, 116, 101, 114, 105, 101, 115, 47, 70, 117, 101, 108, 0, 0,
};

u8 D_dryfield_night_garage_80181A1C[4] = { 0 };

u8 D_dryfield_night_garage_80181A20[60] = {
    87, 101, 97, 112, 111, 110, 115, 32, 117, 115, 105, 110, 103, 32, 98, 97,
    116, 116, 101, 114, 105, 101, 115, 32, 111, 114, 32, 102, 117, 101, 108, 10,
    99, 97, 110, 32, 98, 101, 32, 114, 101, 108, 111, 97, 100, 101, 100, 32,
    102, 111, 114, 32, 102, 114, 101, 101, 46, 0, 0, 0,
};

u8 D_dryfield_night_garage_80181A5C[8] = {
    87, 101, 97, 112, 111, 110, 115, 0,
};

u8 D_dryfield_night_garage_80181A64[12] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 0, 0,
};

u8 D_dryfield_night_garage_80181A70[8] = {
    65, 114, 109, 111, 114, 0, 0, 0,
};

u8 D_dryfield_night_garage_80181A78[8] = {
    73, 116, 101, 109, 115, 0, 0, 0,
};

u8 D_dryfield_night_garage_80181A80[20] = {
    73, 110, 115, 117, 102, 102, 105, 99, 105, 101, 110, 116, 32, 66, 80, 46,
    0, 0, 0, 0,
};

u8 D_dryfield_night_garage_80181A94[16] = {
    73, 110, 118, 101, 110, 116, 111, 114, 121, 32, 102, 117, 108, 108, 46, 0,
};

u8 D_dryfield_night_garage_80181AA4[32] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 32, 99, 97, 112, 97, 99,
    105, 116, 121, 32, 114, 101, 97, 99, 104, 101, 100, 46, 0, 0, 0, 0,
};

u8 D_dryfield_night_garage_80181AC4[12] = {
    65, 109, 111, 117, 110, 116, 0, 0, 0, 0, 0, 0,
};

u8 D_dryfield_night_garage_80181AD0[4] = {
    120, 0, 0, 0,
};

u16 D_dryfield_night_garage_80181AD4[2] = {
    0xFFFF, 0,
};

UiListItemFunc D_dryfield_night_garage_80181AD8[1] = {
    func_dryfield_night_garage_8017DDC4,
};

UiListItemFunc D_dryfield_night_garage_80181ADC[1] = {
    func_dryfield_night_garage_8017E9B8,
};

UiList D_dryfield_night_garage_80181AE0 = { D_dryfield_night_garage_80181ADC, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_dryfield_night_garage_80181B04[2] = {
    func_dryfield_night_garage_8017EF64,
    func_dryfield_night_garage_8017FC14,
};

UiList D_dryfield_night_garage_80181B0C = { D_dryfield_night_garage_80181B04, 2, { .u = 2 }, 1, 10, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_dryfield_night_garage_80181B30 = { 2, 0xFF70, 0xFF98, 128, 40, 56, 0, 0, 192, func_dryfield_night_garage_8017EBD8, 0 };

UiObjectDesc D_dryfield_night_garage_80181B4C = { 2, 0xFF74, 0xFFA3, 188, 160, 48, 0, 0, 192, func_dryfield_night_garage_8017E768, 0 };

UiObjectDesc D_dryfield_night_garage_80181B68 = { 0, 48, 4, 96, 60, 52, 0, 0, 192, func_dryfield_night_garage_8017ED80, 0 };

UiObjectDesc D_dryfield_night_garage_80181B84 = { 0, 48, 32, 70, 32, 20, 0, 0, 192, func_dryfield_night_garage_8017FCD0, 0 };

UiObjectDesc D_dryfield_night_garage_80181BA0 = { 2, 0xFFA0, 0xFFD0, 192, 96, 8, 0, 0, 192, func_dryfield_night_garage_8017F178, 0 };

// Retained data: Complete UI descriptor follows the adjacent UI descriptors. Its last 12 bytes also resemble a TaskDesc, which is its embedded task seed.
UiObjectDesc D_dryfield_night_garage_80181BBC = { 0, 0xFF80, 0xFFE0, 160, 92, 48, 0, 0, 192, func_dryfield_night_garage_8017E768, 0 };

UiObjectDesc D_dryfield_night_garage_80181BD8 = { 2, 0xFFB8, 0xFFDC, 144, 64, 32, 0, 0, 192, func_dryfield_night_garage_8017F2F8, 0 };

UiObjectDesc D_dryfield_night_garage_80181BF4 = { 0, 48, 0xFFA3, 96, 97, 44, 0, 0, 192, func_dryfield_night_garage_8017F5C0, 0 };

UiObjectDesc D_dryfield_night_garage_80181C10 = { 3, 0xFFB8, 0xFFE0, 184, 48, 16, 0, 0, 192, func_dryfield_night_garage_8017F794, 0 };

TaskDesc D_dryfield_night_garage_80181C2C = { 0, 192, func_dryfield_night_garage_8017FDF8, { .model = NULL } };

GpMsgEntry D_dryfield_night_garage_80181C38[6] = {
    { 5102, func_dryfield_night_garage_80180360 },
    { 5105, func_dryfield_night_garage_80180358 },
    { 5103, func_dryfield_night_garage_801800C8 },
    { 5104, func_dryfield_night_garage_801803A4 },
    { 5106, func_dryfield_night_garage_80180300 },
    { 0x7FFFFFFF, NULL },
};

GpAnimArg D_dryfield_night_garage_80181C68 = { { .index = 1 }, 1, 0, 0, 0 };

GpEvsCmd D_dryfield_night_garage_80181C7C[4] = {
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_8013B570 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_garage_80181C68 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_dryfield_night_garage_80181CDC[4] = {
    { -4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
};

SVECTOR D_dryfield_night_garage_80181CFC[8] = {
    { -722, 250, 250, 0 },
    { -722, -250, 250, 0 },
    { -722, -250, -250, 0 },
    { -722, 250, -250, 0 },
    { 1002, -250, -250, 0 },
    { 1002, 250, -250, 0 },
    { 1002, -250, 250, 0 },
    { 1002, 250, 250, 0 },
};

GpGridFace D_dryfield_night_garage_80181D3C[4] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
};

s16 D_dryfield_night_garage_80181D6C[5] = {
    0,
    1,
    2,
    3,
    -1,
};

s16 * D_dryfield_night_garage_80181D78[1] = {
    D_dryfield_night_garage_80181D6C,
};

GpGridParams D_dryfield_night_garage_80181D7C = { NULL, D_dryfield_night_garage_80181CDC, D_dryfield_night_garage_80181CFC, D_dryfield_night_garage_80181D3C, D_dryfield_night_garage_80181D78, 722, 250, 1, 1, 4000, 4 };

SVECTOR D_dryfield_night_garage_80181DA0[4] = {
    { -4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
};

SVECTOR D_dryfield_night_garage_80181DC0[8] = {
    { 1900, 33, 5590, 0 },
    { 1900, -1726, 5590, 0 },
    { 1900, -1726, 4890, 0 },
    { 1900, 33, 4890, 0 },
    { 2560, -1726, 4890, 0 },
    { 2560, 33, 4890, 0 },
    { 2560, -1726, 5590, 0 },
    { 2560, 33, 5590, 0 },
};

GpGridFace D_dryfield_night_garage_80181E00[4] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
};

s16 D_dryfield_night_garage_80181E30[5] = {
    0,
    1,
    2,
    3,
    -1,
};

s16 * D_dryfield_night_garage_80181E3C[1] = {
    D_dryfield_night_garage_80181E30,
};

GpGridParams D_dryfield_night_garage_80181E40 = { NULL, D_dryfield_night_garage_80181DA0, D_dryfield_night_garage_80181DC0, D_dryfield_night_garage_80181E00, D_dryfield_night_garage_80181E3C, -1900, -4890, 1, 1, 4000, 4 };

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} DryfieldNightGaragePoseBank48A4;

DryfieldNightGaragePoseBank48A4 D_dryfield_night_garage_80181E64 = { .poses = {
#include "assets/dryfield_night_garage_animation_04B80_bank1.inc"
} };

GpPackedSvec D_dryfield_night_garage_80181EAC[46] = {
#include "assets/dryfield_night_garage_animation_04B80_bank4.inc"
};

GpAnimRec D_dryfield_night_garage_80181F64[109] = {
#include "assets/dryfield_night_garage_animation_04B80_records.inc"
};

u16 D_dryfield_night_garage_80182118[20] = {
#include "assets/dryfield_night_garage_animation_04B80_indices.inc"
};

GpAnimSet D_dryfield_night_garage_80182140 = {
    D_dryfield_night_garage_80181F64, D_dryfield_night_garage_80182118,
    { NULL, D_dryfield_night_garage_80181E64.words, NULL, NULL, D_dryfield_night_garage_80181EAC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[8];
    GpPackedSvec words[24];
} DryfieldNightGaragePoseBank4BA8;

DryfieldNightGaragePoseBank4BA8 D_dryfield_night_garage_80182168 = { .poses = {
#include "assets/dryfield_night_garage_animation_04F54_bank1.inc"
} };

GpPackedSvec D_dryfield_night_garage_801821C8[84] = {
#include "assets/dryfield_night_garage_animation_04F54_bank4.inc"
};

GpAnimRec D_dryfield_night_garage_80182318[117] = {
#include "assets/dryfield_night_garage_animation_04F54_records.inc"
};

u16 D_dryfield_night_garage_801824EC[20] = {
#include "assets/dryfield_night_garage_animation_04F54_indices.inc"
};

GpAnimSet D_dryfield_night_garage_80182514 = {
    D_dryfield_night_garage_80182318, D_dryfield_night_garage_801824EC,
    { NULL, D_dryfield_night_garage_80182168.words, NULL, NULL, D_dryfield_night_garage_801821C8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} DryfieldNightGaragePoseBank4F7C;

DryfieldNightGaragePoseBank4F7C D_dryfield_night_garage_8018253C = { .poses = {
#include "assets/dryfield_night_garage_animation_05344_bank1.inc"
} };

GpPackedSvec D_dryfield_night_garage_80182590[62] = {
#include "assets/dryfield_night_garage_animation_05344_bank4.inc"
};

GpAnimRec D_dryfield_night_garage_80182688[149] = {
#include "assets/dryfield_night_garage_animation_05344_records.inc"
};

u16 D_dryfield_night_garage_801828DC[20] = {
#include "assets/dryfield_night_garage_animation_05344_indices.inc"
};

GpAnimSet D_dryfield_night_garage_80182904 = {
    D_dryfield_night_garage_80182688, D_dryfield_night_garage_801828DC,
    { NULL, D_dryfield_night_garage_8018253C.words, NULL, NULL, D_dryfield_night_garage_80182590, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} DryfieldNightGaragePoseBank536C;

DryfieldNightGaragePoseBank536C D_dryfield_night_garage_8018292C = { .poses = {
#include "assets/dryfield_night_garage_animation_056B0_bank1.inc"
} };

GpPackedSvec D_dryfield_night_garage_80182980[74] = {
#include "assets/dryfield_night_garage_animation_056B0_bank4.inc"
};

GpAnimRec D_dryfield_night_garage_80182AA8[104] = {
#include "assets/dryfield_night_garage_animation_056B0_records.inc"
};

u16 D_dryfield_night_garage_80182C48[20] = {
#include "assets/dryfield_night_garage_animation_056B0_indices.inc"
};

GpAnimSet D_dryfield_night_garage_80182C70 = {
    D_dryfield_night_garage_80182AA8, D_dryfield_night_garage_80182C48,
    { NULL, D_dryfield_night_garage_8018292C.words, NULL, NULL, D_dryfield_night_garage_80182980, NULL, NULL, NULL },
};

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData[0].state.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_dryfield_night_garage_80181AD4`.
static u16* func_dryfield_night_garage_8017D754(s32 mode)
{
    if (Mc_SaveData[0].state.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801816D8;
                    case 1:
                        return D_dryfield_night_garage_801816F0;
                    case 2:
                        return D_dryfield_night_garage_80181704;
                    case 3:
                        return D_dryfield_night_garage_8018170C;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181720;
                    case 1:
                        return D_dryfield_night_garage_8018173C;
                    case 2:
                        return D_dryfield_night_garage_8018174C;
                    case 3:
                        return D_dryfield_night_garage_80181758;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181770;
                    case 1:
                        return D_dryfield_night_garage_8018178C;
                    case 2:
                        return D_dryfield_night_garage_801817A0;
                    case 3:
                        return D_dryfield_night_garage_801817A8;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801817BC;
                    case 1:
                        return D_dryfield_night_garage_801817DC;
                    case 2:
                        return D_dryfield_night_garage_801817EC;
                    case 3:
                        return D_dryfield_night_garage_801817F8;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181620;
                    case 1:
                        return D_dryfield_night_garage_80181630;
                    case 2:
                        return D_dryfield_night_garage_80181640;
                    case 3:
                        return D_dryfield_night_garage_80181648;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181694;
                    case 1:
                        return D_dryfield_night_garage_801816AC;
                    case 2:
                        return D_dryfield_night_garage_801816C0;
                    case 3:
                        return D_dryfield_night_garage_801816C8;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181658;
                    case 1:
                        return D_dryfield_night_garage_80181668;
                    case 2:
                        return D_dryfield_night_garage_80181678;
                    case 3:
                        return D_dryfield_night_garage_80181680;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801815F8;
                    case 1:
                        return D_dryfield_night_garage_80181600;
                    case 2:
                        return D_dryfield_night_garage_80181608;
                    case 3:
                        return D_dryfield_night_garage_80181610;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801818A4;
                    case 1:
                        return D_dryfield_night_garage_801818B0;
                    case 2:
                        return D_dryfield_night_garage_801818B8;
                    case 3:
                        return D_dryfield_night_garage_801818C4;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801818D0;
                    case 1:
                        return D_dryfield_night_garage_801818DC;
                    case 2:
                        return D_dryfield_night_garage_801818E0;
                    case 3:
                        return D_dryfield_night_garage_801818EC;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801818F8;
                    case 1:
                        return D_dryfield_night_garage_80181904;
                    case 2:
                        return D_dryfield_night_garage_8018190C;
                    case 3:
                        return D_dryfield_night_garage_80181918;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181924;
                    case 1:
                        return D_dryfield_night_garage_80181930;
                    case 2:
                        return D_dryfield_night_garage_80181938;
                    case 3:
                        return D_dryfield_night_garage_80181944;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181830;
                    case 1:
                        return D_dryfield_night_garage_80181838;
                    case 2:
                        return D_dryfield_night_garage_80181840;
                    case 3:
                        return D_dryfield_night_garage_80181848;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_8018187C;
                    case 1:
                        return D_dryfield_night_garage_80181888;
                    case 2:
                        return D_dryfield_night_garage_80181890;
                    case 3:
                        return D_dryfield_night_garage_80181898;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181854;
                    case 1:
                        return D_dryfield_night_garage_8018185C;
                    case 2:
                        return D_dryfield_night_garage_80181868;
                    case 3:
                        return D_dryfield_night_garage_80181870;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181810;
                    case 1:
                        return D_dryfield_night_garage_80181814;
                    case 2:
                        return D_dryfield_night_garage_80181818;
                    case 3:
                        return D_dryfield_night_garage_80181820;
                }
                break;
        }
    }
    return D_dryfield_night_garage_80181AD4;
}

/// Draws one row of the shop list and handles its input, recording the row's
/// id as the cursor item while the row is selected. Row 0xFFFE is greyed out
/// and unselectable unless `Gp_HasMappedItem` answers non-zero, and opens its
/// own panel; row 0xFFFC is greyed out while the scan holds item 0x8F. Any
/// other row is an item with its price, greyed out when `func_800B7420`
/// refuses it; confirm opens the buy panel and button 0x10 the item's detail
/// panel.
void func_dryfield_night_garage_8017DDC4(UiList* prompt, UiObject* obj)
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
        D_dryfield_night_garage_801819EC = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->panel.field_0.w;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam(D_dryfield_night_garage_80181A20, 0, 0);
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
        Text_DrawString(&req, D_dryfield_night_garage_80181A0C);
        if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            Ui_SpawnFromDesc(&D_dryfield_night_garage_80181BD8, 0, 1, 1, obj);
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
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_garage_80181A1C, prompt->field_1C, 1, 0);
        if (prompt->field_C == 1 && blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            child = Ui_SpawnFromDesc(&D_dryfield_night_garage_80181B84, itemId, 1, 1, obj);
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
            child2 = Ui_SpawnFromDesc(&D_dryfield_night_garage_80181B84, itemId, 1, 1, obj);
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
static void func_dryfield_night_garage_8017E250(RoomShopList* shop, UiObject* obj, s32 item)
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
/// the fixed id list (`func_dryfield_night_garage_8017D754`) and, in game mode
/// 0, which items of each unlocked price row are added: mode 0 ids 0x80-0x9F
/// and 9, 0xA, 0xC, 0x42-0x46; mode 1 ids 0xA0-0xBF; mode 2 ids 0x60-0x7F and
/// 0xD; mode 3 ids 1-0x5F other than those. Mode 3 also adds, for each of the
/// twelve two-bit levels in `Mc_SaveData[0].state.shopStock`, the id of that level
/// (the first slot needs level 2). With `Mc_SaveData[0].state.demoScene` 1 every row
/// and level is unlocked first.
static void func_dryfield_night_garage_8017E39C(RoomShopList* shop, UiObject* obj)
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
    ids  = func_dryfield_night_garage_8017D754(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        func_dryfield_night_garage_8017E250(shop, obj, *ids);
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
                        item = D_dryfield_night_garage_80181950[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    func_dryfield_night_garage_8017E250(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    func_dryfield_night_garage_8017E250(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    func_dryfield_night_garage_8017E250(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    func_dryfield_night_garage_8017E250(shop, obj, item);
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
                    func_dryfield_night_garage_8017E250(shop, obj, slot * 3 + (id = level + 0xE));
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
    D_dryfield_night_garage_801819EC = -1;
}

/// Titles and captions of the shop's panels.
static const u8 D_dryfield_night_garage_8017D6D0[] = "Select";
static const u8 D_dryfield_night_garage_8017D6D8[] = "BP";
static const u8 D_dryfield_night_garage_8017D6DC[] = "List";
static const u8 D_dryfield_night_garage_8017D6E4[] = "TOTAL";
static const u8 D_dryfield_night_garage_8017D6EC[] = "Notice";

/// "Charge", with a stray non-zero byte after its terminator that C cannot
/// place, so the string stays assembly.
/// "Charge", followed by the non-zero padding the original toolchain left.
static const char D_dryfield_night_garage_8017D6F4[8] = "Charge\0\xF0";

/// The shop's "Select" panel. On its first frame it allocates the
/// `RoomShopList` work block, fills it through
/// `func_dryfield_night_garage_8017E39C` and opens the panel
/// `D_dryfield_night_garage_80181BF4` beside it. Every frame it draws the list and the
/// "BP" caption; menu reports -1 and cancel 6 to the parent. A child that
/// reports 6 is torn down and the list takes input again; one that reports -1
/// passes it up.
void func_dryfield_night_garage_8017E768(Task* task)
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
    Ui_DrawText(&(obj)->panel, (char*)D_dryfield_night_garage_8017D6D0);
    if (task->state == 0) {
        mem = memCalloc(sizeof(RoomShopList), 0);
        if (mem != NULL) {
            shop               = mem;
            task->work         = (TaskIdMap*)shop;
            shop->list.funcs   = D_dryfield_night_garage_80181AD8;
            shop->list.field_6 = 0;
            shop->list.field_7 = 0xF;
            func_dryfield_night_garage_8017E39C(shop, obj);
            Ui_LayoutListPanel(&shop->list, &(obj)->panel);
            shop->list.field_A = 1;
            Ui_SetListScrollFlag(&shop->list, 1);
            obj->panel.bounds.unsignedRect.h += 8;
            shop->list.field_17               = 8;
            Ui_SpawnFromDesc(&D_dryfield_night_garage_80181BF4, 0, 0, 0, obj);
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
    Text_DrawString(&req, D_dryfield_night_garage_8017D6D8);

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
void func_dryfield_night_garage_8017E9B8(UiList* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_garage_80181A04, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = D_dryfield_night_garage_80181A5C;
    obj->owner->spawnArg1.value = (u16)obj->owner->spawnArg1.value;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = D_dryfield_night_garage_80181A64;
            obj->owner->spawnArg1.value |= 0x10000;
            break;
        case 2:
            text                   = D_dryfield_night_garage_80181A70;
            obj->owner->spawnArg1.value |= 0x20000;
            break;
        case 3:
            text                   = D_dryfield_night_garage_80181A78;
            obj->owner->spawnArg1.value |= 0x30000;
            break;
    }

    if (*func_dryfield_night_garage_8017D754(obj->owner->spawnArg1.value) == 0xFFFF) {
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
        Ui_SpawnFromDesc(&D_dryfield_night_garage_80181B4C, obj->owner->spawnArg1, 1, 1, obj);
        obj->panel.field_0.w = 0;
    }
}

/// The shop's "List" panel, whose rows are the modes. Its first frame clears the item previews,
/// opens the list-row panel and the preview panel, and lays out its five-row
/// list. Cancel or menu reports -1. A child reporting 6 is torn down; one
/// reporting -1 releases `Wip_UiHolder` and passes the code up.
void func_dryfield_night_garage_8017EBD8(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2.pointer;
    list          = &D_dryfield_night_garage_80181AE0;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, (char*)D_dryfield_night_garage_8017D6DC);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_dryfield_night_garage_80181B68, task->spawnArg1, 0, 1, obj);
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
void func_dryfield_night_garage_8017ED80(Task* task)
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
    Text_DrawString(&req0, D_dryfield_night_garage_8017D6D8);

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
    Text_DrawString(&req1, (char*)D_dryfield_night_garage_8017D6E4);

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
void func_dryfield_night_garage_8017EF64(UiList* prompt, UiObject* obj)
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
    Text_DrawString(&req, D_dryfield_night_garage_801819F0);

    mode = prompt->field_C;
    if (mode == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        cfg   = &Player_Status;
        price = Gp_ItemDescs[itemId].price;
        scan  = &Mc_SaveData[0].state.carriedItems;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_dryfield_night_garage_80181BA0, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_dryfield_night_garage_80181BA0, 1, 1, 1, obj);
                }
                obj->panel.field_0.w = 0;
            } else if ((obj->owner->parent->spawnArg1.value >> 16) == mode) {
                child = Ui_SpawnFromDesc(&D_dryfield_night_garage_80181C10, itemId, 1, 1, obj);
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
            Ui_SpawnFromDesc(&D_dryfield_night_garage_80181BA0, 0, 1, 1, obj);
            obj->panel.field_0.w = 0;
        }
    }
}

/// Notice panel: shows one of three messages picked by `spawnArg1`, sized to
/// the text. Menu reports -1; confirm, cancel or 0xBC frames elapsing tell the
/// parent panel to close with 6.
void func_dryfield_night_garage_8017F178(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1.value;
    obj  = task->spawnArg2.pointer;
    switch (kind) {
        case 1:
            text = D_dryfield_night_garage_80181A94;
            break;
        case 2:
            text = D_dryfield_night_garage_80181AA4;
            break;
        default:
            text = D_dryfield_night_garage_80181A80;
            break;
    }

    Ui_DrawText(&(obj)->panel, (char*)D_dryfield_night_garage_8017D6EC);
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
void func_dryfield_night_garage_8017F2F8(Task* task)
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
    Ui_DrawText(&(obj)->panel, (char*)D_dryfield_night_garage_8017D6F4);

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
            map                              = Gp_GetItemMap(slotId);
            D_dryfield_night_garage_8018762C = map;
            itemId                           = map->field_1;
            slot                             = Gp_GetItemSlot(itemId);
            if (D_dryfield_night_garage_8018762C->field_0 == 0) {
                D_dryfield_night_garage_80187628 = slot->ammoQty;
                slot->ammoQty                    = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_dryfield_night_garage_80187628 = slot->attachQty;
                slot->attachQty                  = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown                = 0xBC;
            D_dryfield_night_garage_80187628 <<= 8;
            task->state                        = task->state + 1;
        }
    }

    curItem = D_dryfield_night_garage_8018762C->field_1;
    relItem = D_dryfield_night_garage_8018762C->field_2;
    if (D_dryfield_night_garage_8018762C->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                             <<= 8;
    D_dryfield_night_garage_80187628 += 0x40;
    if (qty < D_dryfield_night_garage_80187628) {
        D_dryfield_night_garage_80187628 = qty;
    }

    y = (s16)obj->panel.field_18.u;
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar(&(obj)->panel, (s16)obj->panel.field_1C.s, (s16)obj->panel.field_1E.u, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, D_dryfield_night_garage_80187628 >> 8, 0x606060);
    h = (s16)obj->panel.field_1A.u;
    func_800C0E20(&(obj)->panel, (s16)obj->panel.field_1C.s + 2, (s16)obj->panel.field_1E.u - 2, h - 6, qty,
                  D_dryfield_night_garage_80187628, 0x1741F);

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

static inline s32 _dryfieldNightGarageAddItemCount(s32 item, s32 count)
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
void func_dryfield_night_garage_8017F5C0(Task* task)
{
    u8          buf[0x10];
    TextDrawReq req;
    UiObject*   obj;
    s32         item;
    s32         y;
    s32         ry;
    s32         count;

    item         = D_dryfield_night_garage_801819EC;
    obj          = task->spawnArg2.pointer;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && D_dryfield_night_garage_801819EC == Gp_GetPreviewItem()) {
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
        Text_DrawString(&req, D_dryfield_night_garage_80181AC4);
        count = 0;
        count = _dryfieldNightGarageAddItemCount(item, count);
        Text_DrawPrompt(obj, (s16)obj->panel.field_1E.u - 2, y + 0xA, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
    }
}

/// Quantity picker of the buy prompt. Up and down step the count between 1 and
/// the most the player can take: for a stackable item, what its stock ceiling
/// still allows in steps of its per-buy amount; otherwise the free inventory
/// rows; in both cases no more than the BP affords. It shows the unit and
/// total price. Confirm takes the total and gives the items; confirm or
/// cancel tells the parent panel to close with 6.
void func_dryfield_night_garage_8017F794(Task* task)
{
    u8           buf[0x20];
    TextDrawReq  req;
    UiObject*    obj;
    UiObject*    parentObj;
    s32          itemId;
    s32          price;
    s32          maxQty;
    s32          afford;
    s32          held;
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
        RoomShopStock* stock = gpItemStock(itemId);

        if (stock->perBuy != 0) {
            held    = Gp_ScanStackQty(&Mc_SaveData[0].state.carriedItems, itemId);
            maxHeld = stock->maxHeld;
            maxQty  = maxHeld - held;
            if (maxQty <= 0) {
                maxQty = 1;
            } else {
                maxQty = (maxQty - 1) / stock->perBuy;
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
        RoomShopStock* stock = gpItemStock(itemId);

        Gp_DrawQty(obj, x, y, stock->perBuy, 0x606060);
    }

    count = task->extraState.value;
    Text_DrawPrompt(obj, left + 0x98, y, D_dryfield_night_garage_80181AD0, 0x606060, 3, 2);
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
    Text_DrawString(&req, D_dryfield_night_garage_8017D6D8);

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
void func_dryfield_night_garage_8017FC14(UiList* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->panel.field_20.u + (u16)prompt->field_18;
    req.y          = obj->panel.field_22.u + (u16)prompt->field_1A;
    req.otIndex    = obj->panel.field_14.s + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, D_dryfield_night_garage_80181A04);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

/// A list panel over `D_dryfield_night_garage_80181B0C`. Cancel reports 6 and
/// menu -1; a child reporting 6 is torn down, one reporting -1 passes it up.
void func_dryfield_night_garage_8017FCD0(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_dryfield_night_garage_80181B0C;
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

/// Opens the panel `D_dryfield_night_garage_80181B30` with the task's
/// `spawnArg1` as its parameter, setting frame timing 0 and the session's UI
/// flag while it is open; once the panel reports -1 or 6 it is torn down, and
/// ten frames later frame timing 1 and the flag are restored and the task
/// kills itself.
void func_dryfield_night_garage_8017FDF8(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_dryfield_night_garage_80181B30, task->spawnArg1, 1, 1, NULL);
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

/// State 0 of this room's message task, run when the garage scene starts.
/// Publishes the room's message table in `Task::msgTable` and the task itself
/// in pointer slot 7, clears the display bit on the first object entry, then
/// hands off to the player actor through messages 0x3E9 / 0x3E8.
static void func_dryfield_night_garage_8017FF2C(Task* task)
{
    GpObj4A* base;
    GpObj4A* obj;
    Task*    player;

    task->msgTable = D_dryfield_night_garage_80181C38;
    Game_SetPtrSlot(task, 7);
    (D_dryfield_night_garage_80186D7C + 3)->field_4A &= 0xBF;
    player                                      = gameGetPtrSlot(0xA);
    if (gGameSession->at4.loc.place == 3 && player != NULL) {
        Gp_DispatchMsgPtr(player, 0x3E9, &D_8013B570, 0);
        Gp_AllyAnimId(&D_dryfield_night_garage_80181C68.animBlock.index);
        Gp_DispatchMsgPtr(player, 0x3E8, &D_dryfield_night_garage_80181C68, 0);
        func_dryfield_night_garage_80180604(0);
        Gp_EndPlayerActorTask(player);
        if (GameFlag_GetNibble(0x8E) == 0) {
            Gp_FillAllyHp();
            GameFlag_SetNibble(0x8E, 1);
            func_800E8634(&D_8013B590, 0, &D_8013C388);
        } else {
            func_800E8614(D_dryfield_night_garage_80181C7C, 1);
        }
    }
    if (gGameSession->at4.loc.place == 2 && GameFlag_GetNibble(0x6C) > 0) {
        if (GameFlag_GetNibble(0x6C) == 1) {
            GameFlag_SetNibble(0x6C, 2);
        }
        base            = (D_dryfield_night_garage_80186D7C + 3);
        obj             = base + 2;
        base->field_4A |= 0x40;
        obj->field_4A  &= 0xBF;
    }
    task->state = (s32)(task->state + 1);
}

s32 func_dryfield_night_garage_801800C8(Task* task, s32 msgId, GpMsg13EF * msg, s32 arg3)
{
    GpObj4A* base;
    GpObj4A* obj;

    if (msg->field_2 == 6) {
        if (gGameSession->at4.loc.place == 2) {
            if (GameFlag_GetNibble(0x6C) == 0) {
                if (Gp_HasCollectedBit(0x113) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 6, 0);
                } else if (Gp_HasCollectedBit(0x117) == 0 && Gp_HasCollectedBit(0x118) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 7, 0);
                } else if (Gp_HasCollectedBit(0x118) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 8, 0);
                } else if (GameFlag_GetNibble(0x6C) == 0) {
                    base            = (D_dryfield_night_garage_80186D7C + 3);
                    obj             = base + 2;
                    base->field_4A |= 0x40;
                    obj->field_4A  &= 0xBF;
                    func_800E8634(D_dryfield_night_garage_80182DF8, 0,
                                  D_dryfield_night_garage_801831B8);
                    GameFlag_SetNibble(0x6C, 1);
                    func_800E3FAC(0xA2, 0x17);
                    Gp_ClearCollectedBit(0x118);
                    Mc_SaveData[0].state.sceneEvent = 5;
                }
            } else {
                Gp_MsgPlayerWeapon(0);
                if (GameFlag_GetNibble(0x6C) == 1) {
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0xA, 0);
                } else {
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0x15, 0);
                }
            }
        }
    }
    if (msg->field_2 == 1) {
        if (GameFlag_GetNibble(0x97) != 0) {
            Gp_StartCapSlot(0x14, 1, 0);
        } else {
            Gp_SpawnIfCapIdle(0x36, 0);
        }
    }
    if (msg->field_2 == 2 && gGameSession->at4.loc.place == 3 && gameGetPtrSlot(0xA) != NULL) {
        Task_SpawnFromTable(D_8013B11C, 1, 0, 0);
    }
    return 0;
}

/// Room event callback: event 9 plays stage sound 0x52030009 and event 0x6C
/// reads the caption event key. Always returns 0.
s32 func_dryfield_night_garage_80180300(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    switch (arg2) {
        case 0x9:
            Gp_EnqueueStageSnd6(0x52030009, 0, 0);
            break;
        case 0x6C:
            Gp_GetCapEventKey();
            break;
    }
    return 0;
}

s32 func_dryfield_night_garage_80180358(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_map_dryfield_full_80179954`. Always returns 1.
s32 func_dryfield_night_garage_80180360(Task* arg0, s32 arg1, RoomEventMsg * in, RoomEventMsg * out)
{
    *out = *in;
    func_map_dryfield_full_80179954(in, out);
    return 1;
}

s32 func_dryfield_night_garage_801803A4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// The empty per-frame state of `D_dryfield_night_garage_8017D6FC`.
static void func_dryfield_night_garage_801803AC(Task* task)
{
    char pad[0x10];
}

/// State handlers of the room's message task `func_dryfield_night_garage_801803BC`
/// runs: its set-up, an empty per-frame state and the kill.
static const TaskFuncTable3 D_dryfield_night_garage_8017D6FC = {
    {
        func_dryfield_night_garage_8017FF2C,
        func_dryfield_night_garage_801803AC,
        taskKill,
    },
};

/// The room's message task: runs the handler for its state from a stack copy
/// of `D_dryfield_night_garage_8017D6FC`.
void func_dryfield_night_garage_801803BC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_garage_8017D6FC;
    sp.funcs[task->state](task);
}

/// Resets the live layout lists from the other template, the same way as the
/// reset below, then shifts the eight-entry list by (0x126B, -0x84, z) where z
/// is 0x170C when `arg0` is zero and 0x2710 otherwise.
void func_dryfield_night_garage_80180414(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_dryfield_night_garage_80183DD4;
    src = &D_dryfield_night_garage_80181D7C;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx         = src->field_4[i].vx;
        dst->field_4[i].vy         = src->field_4[i].vy;
        dst->field_4[i].vz         = src->field_4[i].vz;
        dst->field_8[i * 2].vx     = src->field_8[i * 2].vx;
        dst->field_8[i * 2].vy     = src->field_8[i * 2].vy;
        dst->field_8[i * 2].vz     = src->field_8[i * 2].vz;
        dst->field_8[i * 2 + 1].vx = src->field_8[i * 2 + 1].vx;
        dst->field_8[i * 2 + 1].vy = src->field_8[i * 2 + 1].vy;
        dst->field_8[i * 2 + 1].vz = src->field_8[i * 2 + 1].vz;
        dst->field_C[i]            = src->field_C[i];
    }

    if (arg0 == 0) {
        d.vx = 0x126B;
        d.vy = -0x84;
        d.vz = 0x170C;
    } else {
        d.vx = 0x126B;
        d.vy = -0x84;
        d.vz = 0x2710;
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

/// Resets the live layout lists from the template: the four-entry vector list,
/// the eight-entry list two entries per pass, and the 12-byte records. The
/// eight-entry list is then raised by 0x7D0 on y when `arg0` is nonzero.
static void func_dryfield_night_garage_80180604(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_dryfield_night_garage_80183DD4;
    src = &D_dryfield_night_garage_80181E40;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx         = src->field_4[i].vx;
        dst->field_4[i].vy         = src->field_4[i].vy;
        dst->field_4[i].vz         = src->field_4[i].vz;
        dst->field_8[i * 2].vx     = src->field_8[i * 2].vx;
        dst->field_8[i * 2].vy     = src->field_8[i * 2].vy;
        dst->field_8[i * 2].vz     = src->field_8[i * 2].vz;
        dst->field_8[i * 2 + 1].vx = src->field_8[i * 2 + 1].vx;
        dst->field_8[i * 2 + 1].vy = src->field_8[i * 2 + 1].vy;
        dst->field_8[i * 2 + 1].vz = src->field_8[i * 2 + 1].vz;
        dst->field_C[i]            = src->field_C[i];
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0x7D0;
    }
    d.vz = 0;

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

void func_dryfield_night_garage_801807E4(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_StartCapSlot((s16)arg0->spawnArg1.value, 0, 0);
            Gp_DispatchMsgPtr(func_dryfield_night_garage_80180A64(0), 0x7DB, &D_dryfield_night_garage_80182DE0, 0);
            goto block_12;
        case 1:
            if (Gp_CapBusy() == 0) {
                func_800D4D2C(0x20);
                goto block_12;
            }
            return;
        case 2:
            Gp_StartCapSlot((s16)arg0->spawnArg1.value, 0, (s16)(GameFlag_GetNibble(0x107) + 1));
            if (GameFlag_GetNibble(0x107) == 0) {
                GameFlag_SetNibble(0x107, 1);
            }
        block_12:
            arg0->state = arg0->state + 1;
            return;
        case 3:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_MsgPlayerWeapon(1);
            Gp_DispatchMsgPtr(func_dryfield_night_garage_80180A64(0), 0x7DB, &D_dryfield_night_garage_80182DE4, 0);
        default:
            taskKill(arg0);
            break;
    }
}

/// Queues the replacement of overlay 0x82.
void func_dryfield_night_garage_80180924(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Queues the load of overlay 0x81.
void func_dryfield_night_garage_80180944(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Restores the stream random-number state.
void func_dryfield_night_garage_80180964(void)
{
    Gp_RestoreStreamRng();
}

/// Cancels the queued overlay replacement and restarts the CD queue.
void func_dryfield_night_garage_80180984(void)
{
    CdCmd_CancelReplaceAndActivate();
}
