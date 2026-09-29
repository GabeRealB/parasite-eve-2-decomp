#include "rooms/shelter_1f_heliport.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
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
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_shelter_1f_heliport_80182CB0[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_shelter_1f_heliport_80182CB0_value __asm__("D_shelter_1f_heliport_80182CB0");

extern void func_80131FBC(void);
extern void func_80132038(void);
extern void func_80132110(void);
extern void func_8013230C(void);
extern void func_801322A0(void);
extern void func_80149E38(void);
extern void func_80149E80(void);
extern void func_80149EBC(void);
extern void func_80149FA4(void);

extern TaskDesc D_80136CDC;

/// The 0xFFFF-terminated item id lists `func_shelter_1f_heliport_8017D730`
/// chooses from, and the one it returns when no case matches.
extern u16 D_shelter_1f_heliport_80180B54[];
extern u16 D_shelter_1f_heliport_80180B5C[];
extern u16 D_shelter_1f_heliport_80180B64[];
extern u16 D_shelter_1f_heliport_80180B6C[];
extern u16 D_shelter_1f_heliport_80180B7C[];
extern u16 D_shelter_1f_heliport_80180B8C[];
extern u16 D_shelter_1f_heliport_80180B9C[];
extern u16 D_shelter_1f_heliport_80180BA4[];
extern u16 D_shelter_1f_heliport_80180BB4[];
extern u16 D_shelter_1f_heliport_80180BC4[];
extern u16 D_shelter_1f_heliport_80180BD4[];
extern u16 D_shelter_1f_heliport_80180BDC[];
extern u16 D_shelter_1f_heliport_80180BF0[];
extern u16 D_shelter_1f_heliport_80180C08[];
extern u16 D_shelter_1f_heliport_80180C1C[];
extern u16 D_shelter_1f_heliport_80180C24[];
extern u16 D_shelter_1f_heliport_80180C34[];
extern u16 D_shelter_1f_heliport_80180C4C[];
extern u16 D_shelter_1f_heliport_80180C60[];
extern u16 D_shelter_1f_heliport_80180C68[];
extern u16 D_shelter_1f_heliport_80180C7C[];
extern u16 D_shelter_1f_heliport_80180C98[];
extern u16 D_shelter_1f_heliport_80180CA8[];
extern u16 D_shelter_1f_heliport_80180CB4[];
extern u16 D_shelter_1f_heliport_80180CCC[];
extern u16 D_shelter_1f_heliport_80180CE8[];
extern u16 D_shelter_1f_heliport_80180CFC[];
extern u16 D_shelter_1f_heliport_80180D04[];
extern u16 D_shelter_1f_heliport_80180D18[];
extern u16 D_shelter_1f_heliport_80180D38[];
extern u16 D_shelter_1f_heliport_80180D48[];
extern u16 D_shelter_1f_heliport_80180D54[];
extern u16 D_shelter_1f_heliport_80180D6C[];
extern u16 D_shelter_1f_heliport_80180D70[];
extern u16 D_shelter_1f_heliport_80180D74[];
extern u16 D_shelter_1f_heliport_80180D7C[];
extern u16 D_shelter_1f_heliport_80180D8C[];
extern u16 D_shelter_1f_heliport_80180D94[];
extern u16 D_shelter_1f_heliport_80180D9C[];
extern u16 D_shelter_1f_heliport_80180DA4[];
extern u16 D_shelter_1f_heliport_80180DB0[];
extern u16 D_shelter_1f_heliport_80180DB8[];
extern u16 D_shelter_1f_heliport_80180DC4[];
extern u16 D_shelter_1f_heliport_80180DCC[];
extern u16 D_shelter_1f_heliport_80180DD8[];
extern u16 D_shelter_1f_heliport_80180DE4[];
extern u16 D_shelter_1f_heliport_80180DEC[];
extern u16 D_shelter_1f_heliport_80180DF4[];
extern u16 D_shelter_1f_heliport_80180E00[];
extern u16 D_shelter_1f_heliport_80180E0C[];
extern u16 D_shelter_1f_heliport_80180E14[];
extern u16 D_shelter_1f_heliport_80180E20[];
extern u16 D_shelter_1f_heliport_80180E2C[];
extern u16 D_shelter_1f_heliport_80180E38[];
extern u16 D_shelter_1f_heliport_80180E3C[];
extern u16 D_shelter_1f_heliport_80180E48[];
extern u16 D_shelter_1f_heliport_80180E54[];
extern u16 D_shelter_1f_heliport_80180E60[];
extern u16 D_shelter_1f_heliport_80180E68[];
extern u16 D_shelter_1f_heliport_80180E74[];
extern u16 D_shelter_1f_heliport_80180E80[];
extern u16 D_shelter_1f_heliport_80180E8C[];
extern u16 D_shelter_1f_heliport_80180E94[];
extern u16 D_shelter_1f_heliport_80180EA0[];
extern u16 D_shelter_1f_heliport_80181030[];

/// Messages and labels of the shop's panels.
extern u8 D_shelter_1f_heliport_80180F4C[];
extern u8 D_shelter_1f_heliport_80180F60[];
extern u8 D_shelter_1f_heliport_80180F68[];
extern u8 D_shelter_1f_heliport_80180F78[];
extern u8 D_shelter_1f_heliport_80180F7C[];
extern u8 D_shelter_1f_heliport_80180FB8[];
extern u8 D_shelter_1f_heliport_80180FC0[];
extern u8 D_shelter_1f_heliport_80180FCC[];
extern u8 D_shelter_1f_heliport_80180FD4[];
extern u8 D_shelter_1f_heliport_80180FDC[];
extern u8 D_shelter_1f_heliport_80180FF0[];
extern u8 D_shelter_1f_heliport_80181000[];
extern u8 D_shelter_1f_heliport_80181020[];
extern u8 D_shelter_1f_heliport_8018102C[];

/// The shop's price ladder.
extern RoomShopTier D_shelter_1f_heliport_80180EAC[13];

/// The item id the shop list's cursor last rested on.
extern s32 D_shelter_1f_heliport_80180F48;

/// Row handlers, lists and panel descriptors of the shop's panels.
extern UiListItemFunc D_shelter_1f_heliport_80181034[];
extern UiList         D_shelter_1f_heliport_8018103C;
extern UiList         D_shelter_1f_heliport_80181068;
extern UiObjectDesc   D_shelter_1f_heliport_8018108C;
extern UiObjectDesc   D_shelter_1f_heliport_801810A8;
extern UiObjectDesc   D_shelter_1f_heliport_801810C4;
extern UiObjectDesc   D_shelter_1f_heliport_801810E0;
extern UiObjectDesc   D_shelter_1f_heliport_801810FC;
extern UiObjectDesc   D_shelter_1f_heliport_80181134;
extern UiObjectDesc   D_shelter_1f_heliport_80181150;
extern UiObjectDesc   D_shelter_1f_heliport_8018116C;

/// Descriptors of the room's event task and of its cap-script task.
extern TaskDesc D_shelter_1f_heliport_80181194;
extern TaskDesc D_shelter_1f_heliport_801811C8;

/// Message handlers the room's controller task installs in pointer slot 7.
extern GpMsgEntry D_shelter_1f_heliport_801811A0[];

extern u8 D_shelter_1f_heliport_801811D4[][4];

/// Offset `func_shelter_1f_heliport_801802AC` hands the mesh rebuild; only its
/// `vy` is ever set.
extern SVECTOR D_shelter_1f_heliport_80181204;

/// The mesh's pristine source and the working copy rebuilt from it.
extern GpGridParams D_shelter_1f_heliport_801812AC;
extern GpGridParams D_shelter_1f_heliport_80181974;

/// Work pair of the charge panel `func_shelter_1f_heliport_8017F2D4`: the
/// animated quantity in 24.8 fixed point, and the item map of the slot being
/// charged.
extern s32        D_shelter_1f_heliport_80182C98;
extern GpItemMap* D_shelter_1f_heliport_80182C9C;

/// The event the message handler latched for the room's event task: the spawn
/// argument of its helper task 0x31, the message, the flag saying one was
/// latched, and the event's parameters.
extern RoomFadeStorage  D_shelter_1f_heliport_80182CA0;
extern RoomEventMsg     D_shelter_1f_heliport_80182CA8;
extern RoomLatchedEvent D_shelter_1f_heliport_80182CB4;

static void func_shelter_1f_heliport_80180658(Task* task);
static void func_shelter_1f_heliport_80180748(Task* task);
static void func_shelter_1f_heliport_801807C0(void);
static void func_shelter_1f_heliport_8018085C(GpCoord* coord, SVECTOR* offset);

void func_shelter_1f_heliport_8017DDA0(UiList*, UiObject*);

void func_shelter_1f_heliport_8017E744(Task*);
void func_shelter_1f_heliport_8017E994(UiList*, UiObject*);
void func_shelter_1f_heliport_8017EBB4(Task*);
void func_shelter_1f_heliport_8017ED5C(Task*);
void func_shelter_1f_heliport_8017EF40(UiList*, UiObject*);
void func_shelter_1f_heliport_8017F154(Task*);
void func_shelter_1f_heliport_8017FBF0(UiList*, UiObject*);
void func_shelter_1f_heliport_8017FCAC(Task*);

extern GpGridParams   D_shelter_1f_heliport_80181974;
extern GpObj4C        D_shelter_1f_heliport_80182178[12];
extern GpObj4C        D_shelter_1f_heliport_80182508[21];
extern GpRoomBoundVec D_shelter_1f_heliport_80182B44[13];
extern GpRoomCoordSet D_shelter_1f_heliport_80182160[1];
s32                   func_shelter_1f_heliport_801800A0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                   func_shelter_1f_heliport_80180334(Task*, s32, s32, GpMessageArg);
s32                   func_shelter_1f_heliport_8018041C(Task*, s32, s32, GpMessageArg);
s32                   func_shelter_1f_heliport_801804BC(Task*, s32, RoomEventMsg*, GpMessageArg);
void                  func_shelter_1f_heliport_8017E744(Task*);
void                  func_shelter_1f_heliport_8017F2D4(Task*);
void                  func_shelter_1f_heliport_8017F59C(Task*);
void                  func_shelter_1f_heliport_8017F770(Task*);
void                  func_shelter_1f_heliport_8017FDD4(Task*);
void                  func_shelter_1f_heliport_8017FF08(Task*);
void                  func_shelter_1f_heliport_80180594(Task*);

u16 D_shelter_1f_heliport_80180B54[4] = {
    140, 143, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180B5C[4] = {
    172, 175, 0xFFFE, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180B64[4] = {
    103, 98, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180B6C[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180B7C[8] = {
    131, 140, 143, 10, 70, 138, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180B8C[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180B9C[4] = {
    108, 100, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180BA4[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180BB4[8] = {
    132, 140, 143, 10, 70, 66, 138, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180BC4[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180BD4[4] = {
    98, 105, 106, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180BDC[10] = {
    65, 59, 58, 1, 2, 6, 8, 4,
    0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180BF0[12] = {
    131, 157, 140, 142, 143, 10, 70, 69,
    67, 138, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180C08[10] = {
    160, 161, 172, 173, 171, 169, 175, 0xFFFE,
    0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180C1C[4] = {
    108, 100, 102, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180C24[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180C34[12] = {
    157, 9, 140, 142, 138, 143, 10, 70,
    69, 66, 67, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180C4C[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180C60[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180C68[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180C7C[14] = {
    157, 9, 140, 142, 143, 10, 70, 69,
    66, 67, 68, 138, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180C98[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180CA8[6] = {
    103, 98, 100, 97, 107, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180CB4[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180CCC[14] = {
    140, 142, 138, 143, 10, 70, 69, 66,
    67, 68, 157, 9, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180CE8[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180CFC[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180D04[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180D18[16] = {
    140, 142, 138, 139, 143, 10, 70, 69,
    66, 67, 68, 144, 157, 9, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180D38[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180D48[6] = {
    100, 98, 97, 103, 107, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180D54[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180D6C[2] = {
    139, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180D70[2] = {
    171, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180D74[4] = {
    108, 13, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180D7C[8] = {
    65, 59, 58, 60, 11, 55, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180D8C[4] = {
    131, 138, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180D94[4] = {
    160, 171, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180D9C[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180DA4[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180DB0[4] = {
    140, 138, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180DB8[6] = {
    160, 172, 171, 175, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180DC4[4] = {
    98, 13, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180DCC[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180DD8[6] = {
    131, 138, 143, 70, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180DE4[4] = {
    160, 171, 175, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180DEC[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180DF4[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180E00[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180E0C[4] = {
    171, 175, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180E14[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180E20[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180E2C[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180E38[2] = {
    171, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180E3C[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180E48[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180E54[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180E60[4] = {
    171, 175, 0xFFFE, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180E68[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180E74[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180E80[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180E8C[4] = {
    171, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_1f_heliport_80180E94[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_shelter_1f_heliport_80180EA0[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

RoomShopTier D_shelter_1f_heliport_80180EAC[13] = {
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

s32 D_shelter_1f_heliport_80180F48 = -1;

u8 D_shelter_1f_heliport_80180F4C[20] = {
    80, 117, 114, 99, 104, 97, 115, 101, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
};

u8 D_shelter_1f_heliport_80180F60[8] = {
    80, 97, 115, 115, 0, 0, 0, 0,
};

u8 D_shelter_1f_heliport_80180F68[16] = {
    66, 97, 116, 116, 101, 114, 105, 101, 115, 47, 70, 117, 101, 108, 0, 0,
};

u8 D_shelter_1f_heliport_80180F78[4] = { 0 };

u8 D_shelter_1f_heliport_80180F7C[60] = {
    87, 101, 97, 112, 111, 110, 115, 32, 117, 115, 105, 110, 103, 32, 98, 97,
    116, 116, 101, 114, 105, 101, 115, 32, 111, 114, 32, 102, 117, 101, 108, 10,
    99, 97, 110, 32, 98, 101, 32, 114, 101, 108, 111, 97, 100, 101, 100, 32,
    102, 111, 114, 32, 102, 114, 101, 101, 46, 0, 0, 0,
};

u8 D_shelter_1f_heliport_80180FB8[8] = {
    87, 101, 97, 112, 111, 110, 115, 0,
};

u8 D_shelter_1f_heliport_80180FC0[12] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 0, 0,
};

u8 D_shelter_1f_heliport_80180FCC[8] = {
    65, 114, 109, 111, 114, 0, 0, 0,
};

u8 D_shelter_1f_heliport_80180FD4[8] = {
    73, 116, 101, 109, 115, 0, 0, 0,
};

u8 D_shelter_1f_heliport_80180FDC[20] = {
    73, 110, 115, 117, 102, 102, 105, 99, 105, 101, 110, 116, 32, 66, 80, 46,
    0, 0, 0, 0,
};

u8 D_shelter_1f_heliport_80180FF0[16] = {
    73, 110, 118, 101, 110, 116, 111, 114, 121, 32, 102, 117, 108, 108, 46, 0,
};

u8 D_shelter_1f_heliport_80181000[32] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 32, 99, 97, 112, 97, 99,
    105, 116, 121, 32, 114, 101, 97, 99, 104, 101, 100, 46, 0, 0, 0, 0,
};

u8 D_shelter_1f_heliport_80181020[12] = {
    65, 109, 111, 117, 110, 116, 0, 0, 0, 0, 0, 0,
};

u8 D_shelter_1f_heliport_8018102C[4] = {
    120, 0, 0, 0,
};

u16 D_shelter_1f_heliport_80181030[2] = {
    0xFFFF, 0,
};

UiListItemFunc D_shelter_1f_heliport_80181034[1] = {
    func_shelter_1f_heliport_8017DDA0,
};

UiListItemFunc D_shelter_1f_heliport_80181038[1] = {
    func_shelter_1f_heliport_8017E994,
};

UiList D_shelter_1f_heliport_8018103C = { D_shelter_1f_heliport_80181038, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_shelter_1f_heliport_80181060[2] = {
    func_shelter_1f_heliport_8017EF40,
    func_shelter_1f_heliport_8017FBF0,
};

UiList D_shelter_1f_heliport_80181068 = { D_shelter_1f_heliport_80181060, 2, { .u = 2 }, 1, 10, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_shelter_1f_heliport_8018108C = { 2, 0xFF70, 0xFF98, 128, 40, 56, 0, 0, 192, func_shelter_1f_heliport_8017EBB4, 0 };

UiObjectDesc D_shelter_1f_heliport_801810A8 = { 2, 0xFF74, 0xFFA3, 188, 160, 48, 0, 0, 192, func_shelter_1f_heliport_8017E744, 0 };

UiObjectDesc D_shelter_1f_heliport_801810C4 = { 0, 48, 4, 96, 60, 52, 0, 0, 192, func_shelter_1f_heliport_8017ED5C, 0 };

UiObjectDesc D_shelter_1f_heliport_801810E0 = { 0, 48, 32, 70, 32, 20, 0, 0, 192, func_shelter_1f_heliport_8017FCAC, 0 };

UiObjectDesc D_shelter_1f_heliport_801810FC = { 2, 0xFFA0, 0xFFD0, 192, 96, 8, 0, 0, 192, func_shelter_1f_heliport_8017F154, 0 };

UiObjectDesc D_shelter_1f_heliport_80181118 = { 0, 0xFF80, 0xFFE0, 160, 92, 48, 0, 0, 192, func_shelter_1f_heliport_8017E744, 0 };

UiObjectDesc D_shelter_1f_heliport_80181134 = { 2, 0xFFB8, 0xFFDC, 144, 64, 32, 0, 0, 192, func_shelter_1f_heliport_8017F2D4, 0 };

UiObjectDesc D_shelter_1f_heliport_80181150 = { 0, 48, 0xFFA3, 96, 97, 44, 0, 0, 192, func_shelter_1f_heliport_8017F59C, 0 };

UiObjectDesc D_shelter_1f_heliport_8018116C = { 3, 0xFFB8, 0xFFE0, 184, 48, 16, 0, 0, 192, func_shelter_1f_heliport_8017F770, 0 };

TaskDesc D_shelter_1f_heliport_80181188 = { 0, 192, func_shelter_1f_heliport_8017FDD4, { .model = NULL } };

TaskDesc D_shelter_1f_heliport_80181194 = { 0, 32, func_shelter_1f_heliport_8017FF08, { .model = NULL } };

GpMsgEntry D_shelter_1f_heliport_801811A0[5] = {
    { 5102, func_shelter_1f_heliport_801800A0 },
    { 5105, func_shelter_1f_heliport_80180334 },
    { 5103, func_shelter_1f_heliport_801804BC },
    { 5104, func_shelter_1f_heliport_8018041C },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_1f_heliport_801811C8 = { 0, 32, func_shelter_1f_heliport_80180594, { .model = NULL } };

u8 D_shelter_1f_heliport_801811D4[12][4] = {
    { 2, 2, 2, 2 },
    { 1, 1, 1, 1 },
    { 2, 2, 1, 1 },
    { 1, 1, 2, 2 },
    { 1, 1, 2, 2 },
    { 2, 2, 1, 1 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 2, 2, 1, 1 },
};

SVECTOR D_shelter_1f_heliport_80181204 = { 0 };

SVECTOR D_shelter_1f_heliport_8018120C[4] = {
    { -4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
};

SVECTOR D_shelter_1f_heliport_8018122C[8] = {
    { -131, 500, 279, 0 },
    { -131, -808, 279, 0 },
    { -131, -808, -227, 0 },
    { -131, 500, -227, 0 },
    { 119, -808, -227, 0 },
    { 119, 500, -227, 0 },
    { 119, -808, 279, 0 },
    { 119, 500, 279, 0 },
};

GpGridFace D_shelter_1f_heliport_8018126C[4] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
};

s16 D_shelter_1f_heliport_8018129C[5] = {
    0,
    1,
    2,
    3,
    -1,
};

s16 * D_shelter_1f_heliport_801812A8[1] = {
    D_shelter_1f_heliport_8018129C,
};

GpGridParams D_shelter_1f_heliport_801812AC = { NULL, D_shelter_1f_heliport_8018120C, D_shelter_1f_heliport_8018122C, D_shelter_1f_heliport_8018126C, D_shelter_1f_heliport_801812A8, 131, 227, 1, 1, 4000, 4 };

GpRoomObjRec D_shelter_1f_heliport_801812D0[1] = {
    { &D_shelter_1f_heliport_80181974, D_shelter_1f_heliport_80182178, D_shelter_1f_heliport_80182508, NULL },
};

GpRoomCoordRec D_shelter_1f_heliport_801812E0[1] = {
    { D_shelter_1f_heliport_80182160, D_shelter_1f_heliport_80182B44 },
};

u8 * D_shelter_1f_heliport_801812E8[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_1f_heliport_801812EC[1] = {
    { { .bytes = { 12, 0 } } },
};

GpWarpRec D_shelter_1f_heliport_801812F0[2] = {
    { { .words = { 3072, 9920, 0, 3450 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x2792, 0, 2880 } }, { 0, 0, 0, 0 }, 0x55040002, 0x55040001, 0, 11, 0, 0 },
    { { .words = { 1024, 440, 0, 4340 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x2792, 0, 2880 } }, { 0, 0, 0, 0 }, 0, 0, 0, 3, 0, 0 },
};

SVECTOR D_shelter_1f_heliport_80181360[12] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { 0, 0, 4096, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { -4096, 0, 0, 0 },
    { 3122, 0, 2651, 0 },
    { -3156, 0, 2611, 0 },
    { -1055, 0, -3958, 0 },
};

SVECTOR D_shelter_1f_heliport_801813C0[75] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 1135, -2000, 2313, 0 },
    { 255, -2000, 3350, 0 },
    { 1135, -2000, 3350, 0 },
    { 255, 0, 3350, 0 },
    { 1135, 0, 3350, 0 },
    { 1135, 0, 2313, 0 },
    { 0, -2000, 8000, 0 },
    { 1110, -2000, 8000, 0 },
    { 1110, -2000, 6480, 0 },
    { 0, -2000, 6480, 0 },
    { 0, 0, 6480, 0 },
    { 1110, 0, 6480, 0 },
    { 1110, 0, 8000, 0 },
    { 7050, -2000, -490, 0 },
    { 7590, -2000, -490, 0 },
    { 7590, -2000, -2800, 0 },
    { 7050, -2000, -2800, 0 },
    { 7590, 0, -490, 0 },
    { 7050, 0, -490, 0 },
    { 7590, 0, -2800, 0 },
    { 9750, 0, 5400, 0 },
    { 9750, -2000, 5400, 0 },
    { 9750, -2000, 4640, 0 },
    { 9750, 0, 4640, 0 },
    { 0x2A62, -2000, 4640, 0 },
    { 0x2A62, 0, 4640, 0 },
    { 0x2A62, -2000, 5400, 0 },
    { 3000, -3000, 8000, 0 },
    { 3000, 0, 8000, 0 },
    { 0, 0, 8000, 0 },
    { 0, -3000, 8000, 0 },
    { 0, 0, 3650, 0 },
    { 0, -3000, 3650, 0 },
    { 2250, 0, 1000, 0 },
    { 2250, -3000, 1000, 0 },
    { 7050, 0, 1000, 0 },
    { 7050, -3000, 1000, 0 },
    { 7050, 0, -2800, 0 },
    { 7050, -3000, -2800, 0 },
    { 9000, 0, -2800, 0 },
    { 9000, -3000, -2800, 0 },
    { 9000, 0, 150, 0 },
    { 9000, -3000, 150, 0 },
    { 0x27D8, 0, 1600, 0 },
    { 0x27D8, -3000, 1600, 0 },
    { 0x2A62, 0, 1600, 0 },
    { 0x2A62, -3000, 1600, 0 },
    { 0x2A62, 0, 5400, 0 },
    { 0x2A62, -3000, 5400, 0 },
    { 9750, -3000, 5400, 0 },
    { 9000, 0, 5600, 0 },
    { 9000, -3000, 5600, 0 },
    { 3000, 0, 5600, 0 },
    { 3000, -3000, 5600, 0 },
    { 2250, -500, 1000, 0 },
    { 1400, -500, 2000, 0 },
    { 2250, -500, 2000, 0 },
    { 1400, 0, 2000, 0 },
    { 2250, 0, 2000, 0 },
};

GpGridFace D_shelter_1f_heliport_80181618[40] = {
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 16, 17, 18, 0xFFFF }, 4, 0 },
    { { 19, 20, 17, 18 }, 5, 0 },
    { { 20, 21, 18, 16 }, 6, 0 },
    { { 23, 24, 22, 25 }, 4, 0 },
    { { 25, 24, 26, 27 }, 7, 0 },
    { { 24, 23, 27, 28 }, 6, 0 },
    { { 30, 31, 29, 32 }, 4, 0 },
    { { 30, 29, 33, 34 }, 5, 0 },
    { { 31, 30, 35, 33 }, 6, 0 },
    { { 37, 38, 36, 39 }, 8, 0 },
    { { 38, 40, 39, 41 }, 7, 0 },
    { { 42, 40, 37, 38 }, 4, 0 },
    { { 44, 45, 43, 46 }, 7, 0 },
    { { 45, 47, 46, 48 }, 6, 0 },
    { { 47, 49, 48, 50 }, 9, 0 },
    { { 49, 51, 50, 52 }, 5, 0 },
    { { 51, 53, 52, 54 }, 6, 0 },
    { { 53, 55, 54, 56 }, 5, 0 },
    { { 55, 57, 56, 58 }, 8, 0 },
    { { 57, 59, 58, 60 }, 10, 0 },
    { { 59, 61, 60, 62 }, 5, 0 },
    { { 61, 63, 62, 64 }, 8, 0 },
    { { 63, 36, 64, 65 }, 7, 0 },
    { { 36, 66, 65, 67 }, 11, 0 },
    { { 66, 68, 67, 69 }, 7, 0 },
    { { 68, 44, 69, 43 }, 8, 0 },
    { { 36, 63, 59, 61 }, 4, 1 },
    { { 47, 68, 49, 0xFFFF }, 4, 1 },
    { { 68, 66, 49, 51 }, 4, 1 },
    { { 57, 55, 51, 53 }, 4, 1 },
    { { 51, 66, 57, 0xFFFF }, 4, 1 },
    { { 66, 36, 57, 59 }, 4, 1 },
    { { 47, 45, 68, 44 }, 4, 1 },
    { { 70, 71, 72, 0xFFFF }, 4, 0 },
    { { 73, 74, 71, 72 }, 5, 0 },
    { { 74, 49, 72, 70 }, 6, 0 },
};

s16 D_shelter_1f_heliport_801817F8[14] = {
    0,
    1,
    2,
    3,
    4,
    6,
    18,
    19,
    31,
    32,
    37,
    38,
    39,
    -1,
};

s16 D_shelter_1f_heliport_80181814[22] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    17,
    18,
    19,
    28,
    29,
    31,
    32,
    36,
    37,
    38,
    39,
    -1,
};

s16 D_shelter_1f_heliport_80181840[19] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    16,
    17,
    18,
    28,
    29,
    31,
    32,
    36,
    -1,
};

s16 D_shelter_1f_heliport_80181868[17] = {
    0,
    1,
    2,
    3,
    10,
    11,
    12,
    19,
    20,
    21,
    22,
    23,
    32,
    33,
    34,
    35,
    -1,
};

s16 D_shelter_1f_heliport_8018188C[26] = {
    0,
    1,
    2,
    3,
    10,
    11,
    12,
    13,
    14,
    15,
    19,
    20,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    -1,
};

s16 D_shelter_1f_heliport_801818C0[14] = {
    0,
    1,
    2,
    3,
    16,
    27,
    28,
    29,
    31,
    32,
    34,
    35,
    36,
    -1,
};

s16 D_shelter_1f_heliport_801818DC[20] = {
    0,
    1,
    2,
    3,
    10,
    11,
    12,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    30,
    32,
    33,
    34,
    35,
    -1,
};

s16 D_shelter_1f_heliport_80181904[22] = {
    0,
    1,
    2,
    3,
    13,
    14,
    15,
    19,
    20,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    30,
    32,
    33,
    34,
    35,
    -1,
};

s16 D_shelter_1f_heliport_80181930[16] = {
    0,
    1,
    2,
    3,
    13,
    14,
    15,
    25,
    26,
    27,
    28,
    30,
    32,
    34,
    35,
    -1,
};

s16 * D_shelter_1f_heliport_80181950[9] = {
    D_shelter_1f_heliport_801817F8,
    D_shelter_1f_heliport_80181814,
    D_shelter_1f_heliport_80181840,
    D_shelter_1f_heliport_80181868,
    D_shelter_1f_heliport_8018188C,
    D_shelter_1f_heliport_801818C0,
    D_shelter_1f_heliport_801818DC,
    D_shelter_1f_heliport_80181904,
    D_shelter_1f_heliport_80181930,
};

GpGridParams D_shelter_1f_heliport_80181974 = { NULL, D_shelter_1f_heliport_80181360, D_shelter_1f_heliport_801813C0, D_shelter_1f_heliport_80181618, D_shelter_1f_heliport_80181950, 0, 2800, 3, 3, 4000, 40 };

GpViewRec D_shelter_1f_heliport_80181998[12] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x3CE8, -3400 } }, 235 },
    { { { { 338, 0, -4082 }, { -81, 4095, -6 }, { 4081, 81, 337 } }, { -550, 1690, -2890 } }, 230 },
    { { { { 604, 0, 4051 }, { 79, 4095, -11 }, { -4050, 80, 604 } }, { -7570, 1700, -2750 } }, 230 },
    { { { { 3910, 0, 1218 }, { -32, 4094, 105 }, { -1217, -110, 3909 } }, { -2572, 1124, -1708 } }, 269 },
    { { { { -4049, 0, 618 }, { -54, 4079, -359 }, { -615, -363, -4033 } }, { -8235, 928, -4313 } }, 289 },
    { { { { 576, -68, 4054 }, { -143, 4092, 89 }, { -4052, -154, 573 } }, { -3633, 810, -4286 } }, 289 },
    { { { { 3673, -67, 1810 }, { -125, 4073, 407 }, { -1807, -420, 3651 } }, { -1386, 1256, -5618 } }, 289 },
    { { { { 2691, -68, 3086 }, { -5, 4094, 95 }, { -3087, -66, 2690 } }, { -2397, 1279, -5079 } }, 320 },
    { { { { -2704, -68, 3075 }, { -153, 4092, -44 }, { -3071, -144, -2705 } }, { -2303, 1279, -7476 } }, 348 },
    { { { { -4043, -68, 647 }, { -23, 4085, 285 }, { -650, 278, -4034 } }, { -1300, 1420, -7060 } }, 380 },
    { { { { 381, -42, -4077 }, { -3172, 2570, -323 }, { 2562, 3188, 206 } }, { -5360, 5890, -3200 } }, 230 },
    { { { { -1836, -49, 3660 }, { 2496, 2978, 1292 }, { -2677, 2810, -1305 } }, { -3140, 1450, -1920 } }, 329 },
};

GpSprtCmd D_shelter_1f_heliport_80181B48[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_1f_heliport_80181B58[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 72, 1239, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -40, 1539, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -16, 1500, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 8, 1500, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -48, 150, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -24, 1500, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 0, 1500, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 24, 1500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, -56, 1350, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, -24, 1375, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 8, 1375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -64, 1175, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 40, 1375, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -32, 1175, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 0, 1200, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 32, 1200, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 64, 1193, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -72, 1100, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -32, 1100, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 1100, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 48, 1100, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_1f_heliport_80181CFC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_80181D14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_80181D24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_80181D34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_80181D44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_80181D54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_80181D64[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_80181D74[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_80181D84[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_1f_heliport_80181D94[13] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 1100, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 1075, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 80, 1175, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 1100, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 1050, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 104, 1050, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 88, 1025, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 72, 1000, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 64, 900, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 104, 1150, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 104, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 88, 1150, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 88, 1175, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_1f_heliport_80181E98[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_heliport_80181EB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_1f_heliport_80181EC0[12] = {
    { { .empty = D_shelter_1f_heliport_80181B48 }, D_shelter_1f_heliport_80181B48, NULL },
    { { .elements = D_shelter_1f_heliport_80181B58 }, D_shelter_1f_heliport_80181CFC, NULL },
    { { .empty = D_shelter_1f_heliport_80181D14 }, D_shelter_1f_heliport_80181D14, NULL },
    { { .empty = D_shelter_1f_heliport_80181D24 }, D_shelter_1f_heliport_80181D24, NULL },
    { { .empty = D_shelter_1f_heliport_80181D34 }, D_shelter_1f_heliport_80181D34, NULL },
    { { .empty = D_shelter_1f_heliport_80181D44 }, D_shelter_1f_heliport_80181D44, NULL },
    { { .empty = D_shelter_1f_heliport_80181D54 }, D_shelter_1f_heliport_80181D54, NULL },
    { { .empty = D_shelter_1f_heliport_80181D64 }, D_shelter_1f_heliport_80181D64, NULL },
    { { .empty = D_shelter_1f_heliport_80181D74 }, D_shelter_1f_heliport_80181D74, NULL },
    { { .empty = D_shelter_1f_heliport_80181D84 }, D_shelter_1f_heliport_80181D84, NULL },
    { { .elements = D_shelter_1f_heliport_80181D94 }, D_shelter_1f_heliport_80181E98, NULL },
    { { .empty = D_shelter_1f_heliport_80181EB0 }, D_shelter_1f_heliport_80181EB0, NULL },
};

GpPointLight D_shelter_1f_heliport_80181F50[1] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5450, -4000, 3600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 3194, 3112, { 0, 0 } }, 4500, 5000 },
};

GpSpotLight D_shelter_1f_heliport_80181FB0[4] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 700, -1250, -1510 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 3194, 3112, { 0, 0 } }, { 2469, 809, 3165, 0 }, 0x4E20, 0x4E20, 887 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 450, -1500, 9740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 3194, 3112, { 0, 0 } }, { 2574, 780, -3089, 0 }, 0x3A98, 0x3A98, 887 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7900, -1750, 6890 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 3194, 3112, { 0, 0 } }, { 2537, 992, -3058, 0 }, 0x4E20, 0x4E20, 887 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7550, -1500, -6360 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 3194, 3112, { 0, 0 } }, { 2627, 0, 3142, 0 }, 0x4E20, 0x4E20, 887 },
};

GpRoomCoordSet D_shelter_1f_heliport_80182160[1] = {
    { 0, NULL, 1, D_shelter_1f_heliport_80181F50, 4, D_shelter_1f_heliport_80181FB0 },
};

GpObj4C D_shelter_1f_heliport_80182178[12] = {
    { NULL, NULL, NULL, { 4017, -3248, 3516, 0 }, { { 131, -3712, -3671, 0 }, { -135, -3712, 3667, 0 }, { 131, 3712, -3671, 0 }, { -135, 3712, 3667, 0 } }, { 4104, 0, 148, 0 }, { 0, 0, 4096, 0 }, 5221, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 4122, -3200, 3577, 0 }, { { -135, -3712, 3667, 0 }, { 131, -3712, -3670, 0 }, { -135, 3712, 3667, 0 }, { 131, 3712, -3670, 0 } }, { -4104, 0, -150, 0 }, { 0, 0, 4096, 0 }, 5221, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 94, -3232, 5853, 0 }, { { 1452, -3712, -676, 0 }, { -1451, -3712, 677, 0 }, { 1452, 3712, -676, 0 }, { -1451, 3712, 677, 0 } }, { 1733, 0, 3719, 0 }, { 0, 0, 4096, 0 }, 4039, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 110, -3232, 6014, 0 }, { { -1468, -3712, 669, 0 }, { 1468, -3712, -669, 0 }, { -1468, 3712, 669, 0 }, { 1468, 3712, -669, 0 } }, { -1702, 0, -3733, 0 }, { 0, 0, 4096, 0 }, 4039, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 6829, -3296, 606, 0 }, { { 1337, -3712, 569, 0 }, { -1337, -3712, -569, 0 }, { 1337, 3712, 569, 0 }, { -1337, 3712, -569, 0 } }, { -1611, 0, 3782, 0 }, { 0, 0, 4096, 0 }, 3982, 0, 11, 5, 1, 0 },
    { NULL, NULL, NULL, { 6813, -3168, 766, 0 }, { { -1306, -3712, -569, 0 }, { 1307, -3712, 570, 0 }, { -1306, 3712, -569, 0 }, { 1307, 3712, 570, 0 } }, { 1642, 0, -3771, 0 }, { 0, 0, 4096, 0 }, 3974, 0, 5, 11, 1, 0 },
    { NULL, NULL, NULL, { 9152, -3232, 575, 0 }, { { -1160, -3712, 828, 0 }, { 1160, -3712, -827, 0 }, { -1160, 3712, 828, 0 }, { 1160, 3712, -827, 0 } }, { -2388, 0, -3347, 0 }, { 0, 0, 4096, 0 }, 3974, 0, 5, 11, 1, 0 },
    { NULL, NULL, NULL, { 9183, -3200, 350, 0 }, { { 1154, -3712, -836, 0 }, { -1153, -3712, 836, 0 }, { 1154, 3712, -836, 0 }, { -1153, 3712, 836, 0 } }, { 2411, 0, 3327, 0 }, { 0, 0, 4096, 0 }, 3974, 0, 11, 5, 1, 0 },
    { NULL, NULL, NULL, { 2847, -3200, 5856, 0 }, { { 1449, -3712, 711, 0 }, { -1448, -3712, -710, 0 }, { 1449, 3712, 711, 0 }, { -1448, 3712, -710, 0 } }, { -1807, 0, 3682, 0 }, { 0, 0, 4096, 0 }, 4047, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 2846, -3265, 6046, 0 }, { { -1448, -3712, -710, 0 }, { 1449, -3712, 711, 0 }, { -1448, 3712, -710, 0 }, { 1449, 3712, 711, 0 } }, { 1806, 0, -3684, 0 }, { 0, 0, 4096, 0 }, 4047, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 6653, -2944, 3869, 0 }, { { 134, -3712, -3670, 0 }, { -133, -3712, 3670, 0 }, { 134, 3712, -3670, 0 }, { -133, 3712, 3670, 0 } }, { 4105, 0, 149, 0 }, { 0, 0, 4096, 0 }, 5221, 0, 11, 2, 1, 0 },
    { NULL, NULL, NULL, { 6782, -2912, 4031, 0 }, { { -178, -3712, 3669, 0 }, { 178, -3712, -3668, 0 }, { -178, 3712, 3669, 0 }, { 178, 3712, -3668, 0 } }, { -4092, 0, -199, 0 }, { 0, 0, 4096, 0 }, 5221, 0, 2, 11, 129, 0 },
};

GpObj4C D_shelter_1f_heliport_80182508[21] = {
    { NULL, NULL, NULL, { 416, -48, 4704, 0 }, { { -448, 0, -608, 0 }, { 448, 0, -608, 0 }, { -448, 0, 608, 0 }, { 448, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 754, 0, 28, 33, 2, 0 },
    { NULL, NULL, NULL, { 6960, -64, -1168, 0 }, { { -336, 0, -688, 0 }, { 1424, 0, -688, 0 }, { -336, 0, 1392, 0 }, { 1424, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1991, 5, 4, 0, 4, 0 },
    { NULL, NULL, NULL, { 832, -64, 3040, 0 }, { { -1008, 0, -848, 0 }, { 1104, 0, -848, 0 }, { -1008, 0, 1104, 0 }, { 1104, 0, 1104, 0 } }, { 0, 4094, 0, 0 }, { -4096, 0, 0, 0 }, 1557, 5, 2, 0, 4, 0 },
    { NULL, NULL, NULL, { 0x2880, -64, 5056, 0 }, { { -1552, 0, -1328, 0 }, { 752, 0, -1328, 0 }, { -1552, 0, 816, 0 }, { 752, 0, 816, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 2039, 5, 3, 0, 4, 0 },
    { NULL, NULL, NULL, { 544, -64, 6944, 0 }, { { -624, 0, -1168, 0 }, { 1296, 0, -1168, 0 }, { -624, 0, 400, 0 }, { 1296, 0, 400, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1740, 5, 1, 0, 4, 0 },
    { NULL, NULL, NULL, { 1968, -64, 7760, 0 }, { { -1120, 0, -288, 0 }, { 1120, 0, -288, 0 }, { -1120, 0, 288, 0 }, { 1120, 0, 288, 0 } }, { 0, 4114, 0, 0 }, { 600, 0, -4053, 0 }, 1152, 2, 33, 255, 2, 0 },
    { NULL, NULL, NULL, { 1792, -64, 1504, 0 }, { { -688, 0, -496, 0 }, { 1264, 0, -496, 0 }, { -688, 0, 1168, 0 }, { 1264, 0, 1168, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1717, 2, 34, 255, 4, 0 },
    { NULL, NULL, NULL, { 3344, -64, 5232, 0 }, { { -512, 0, -736, 0 }, { 512, 0, -736, 0 }, { -512, 0, 736, 0 }, { 512, 0, 736, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 896, 5, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 4416, -64, 5232, 0 }, { { -512, 0, -752, 0 }, { 512, 0, -752, 0 }, { -512, 0, 752, 0 }, { 512, 0, 752, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 909, 5, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 3872, -64, 4432, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 701, 5, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 3888, -64, 5920, 0 }, { { -1024, 0, -1904, 0 }, { 1024, 0, -1904, 0 }, { -1024, 0, -688, 0 }, { 1024, 0, -688, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2157, 5, 5, 0, 4, 0 },
    { NULL, NULL, NULL, { 2880, -64, 4992, 0 }, { { 0, 0, -1120, 0 }, { 896, 0, -1120, 0 }, { 0, 0, 960, 0 }, { 896, 0, 960, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1431, 5, 255, 0, 2, 0 },
    { NULL, NULL, NULL, { 4384, -64, 4928, 0 }, { { -512, 0, -1120, 0 }, { 640, 0, -1120, 0 }, { -512, 0, 960, 0 }, { 640, 0, 960, 0 } }, { 0, 4108, 0, 0 }, { 4096, 0, 0, 0 }, 1286, 5, 255, 0, 2, 0 },
    { NULL, NULL, NULL, { 3168, -64, 3872, 0 }, { { -512, 0, -192, 0 }, { 1920, 0, -192, 0 }, { -512, 0, 960, 0 }, { 1920, 0, 960, 0 } }, { 0, 4099, 0, 0 }, { 201, 0, -4091, 0 }, 2141, 5, 255, 0, 2, 0 },
    { NULL, NULL, NULL, { 9759, -64, 1519, 0 }, { { -648, 0, -163, 0 }, { 97, 0, -661, 0 }, { -96, 0, 662, 0 }, { 649, 0, 164, 0 } }, { 0, 4106, 0, 0 }, { -3703, 0, 1751, 0 }, 668, 0, 3, 18, 2, 0 },
    { NULL, NULL, NULL, { 5616, -64, 5296, 0 }, { { -3280, 0, -368, 0 }, { 3280, 0, -368, 0 }, { -3280, 0, 368, 0 }, { 3280, 0, 368, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -4096, 0 }, 3298, 2, 36, 0, 2, 0 },
    { NULL, NULL, NULL, { 2784, -64, 6592, 0 }, { { -448, 0, -1376, 0 }, { 448, 0, -1376, 0 }, { -448, 0, 1376, 0 }, { 448, 0, 1376, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 1442, 2, 36, 0, 2, 0 },
    { NULL, NULL, NULL, { 5040, -64, 1216, 0 }, { { -2080, 0, -368, 0 }, { 2080, 0, -368, 0 }, { -2080, 0, 368, 0 }, { 2080, 0, 368, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 2111, 2, 38, 0, 2, 0 },
    { NULL, NULL, NULL, { 8208, -64, -2592, 0 }, { { -816, 0, -368, 0 }, { 816, 0, -368, 0 }, { -816, 0, 368, 0 }, { 816, 0, 368, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 893, 2, 39, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x2880, -64, 3200, 0 }, { { -448, 0, -1376, 0 }, { 448, 0, -1376, 0 }, { -448, 0, 1376, 0 }, { 448, 0, 1376, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 1442, 2, 42, 0, 2, 0 },
    { NULL, NULL, NULL, { 3904, -64, 6080, 0 }, { { -1248, 0, -2624, 0 }, { 1248, 0, -2624, 0 }, { -1248, 0, -832, 0 }, { 1248, 0, -832, 0 } }, { 0, 4117, 0, 0 }, { 4096, 0, 0, 0 }, 2896, 5, 255, 0, 132, 0 },
};

GpRoomBoundVec D_shelter_1f_heliport_80182B44[13] = {
    { 12, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 1838, 1819, 1496, 1785 },
    { 1854, 1818, 1567, 1800 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

GpAreaTmdRec D_shelter_1f_heliport_80182BAC[3] = {
    { 116, 615, 0, 0, { 0, 0 }, D_801401B0 },
    { 115, 605, 1, 0, { 0, 0 }, D_80159DB0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_1f_heliport_80182BD0[3] = {
    { 116, 615, 0, 0, { 0, 0 }, D_801401B0 },
    { 144, 604, 1, 0, { 0, 0 }, D_80154C18 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_shelter_1f_heliport_80182BF4[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017AF30, D_shelter_1f_heliport_80182BAC },
    { D_map_neo_ark_8017AF80, D_shelter_1f_heliport_80182BD0 },
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

s32 D_shelter_1f_heliport_80182C5C[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

GpRoomParamRec D_shelter_1f_heliport_80182C68[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_1f_heliport_80182C70[1] = {
    { 0, 0, 1, 0, D_shelter_1f_heliport_80182C5C },
};

GpRoomParamRec * D_shelter_1f_heliport_80182C78[8] = {
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C70,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
    D_shelter_1f_heliport_80182C68,
};

s32 D_shelter_1f_heliport_80182C98 = 0;

GpItemMap * D_shelter_1f_heliport_80182C9C = NULL;

RoomFadeStorage D_shelter_1f_heliport_80182CA0 = { 0 };

RoomEventMsg D_shelter_1f_heliport_80182CA8 = { 0 };

s8 D_shelter_1f_heliport_80182CB0[4] = {
    0,
    2,
    68,
    -32,
};

RoomLatchedEvent D_shelter_1f_heliport_80182CB4 = { 0 };

static u16*           func_shelter_1f_heliport_8017D730(s32 mode);
static void           func_shelter_1f_heliport_8017E22C(RoomShopList* shop, UiObject* obj, s32 item);
static void           func_shelter_1f_heliport_8017E378(RoomShopList* shop, UiObject* obj);
static inline s32     _shelter_1f_heliportAddItemCount(s32 item, s32 count);
static __inline__ s32 _shelter1fHeliportStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData[0].state.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_shelter_1f_heliport_80181030`.
static u16* func_shelter_1f_heliport_8017D730(s32 mode)
{
    if (Mc_SaveData[0].state.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180C34;
                    case 1:
                        return D_shelter_1f_heliport_80180C4C;
                    case 2:
                        return D_shelter_1f_heliport_80180C60;
                    case 3:
                        return D_shelter_1f_heliport_80180C68;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180C7C;
                    case 1:
                        return D_shelter_1f_heliport_80180C98;
                    case 2:
                        return D_shelter_1f_heliport_80180CA8;
                    case 3:
                        return D_shelter_1f_heliport_80180CB4;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180CCC;
                    case 1:
                        return D_shelter_1f_heliport_80180CE8;
                    case 2:
                        return D_shelter_1f_heliport_80180CFC;
                    case 3:
                        return D_shelter_1f_heliport_80180D04;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180D18;
                    case 1:
                        return D_shelter_1f_heliport_80180D38;
                    case 2:
                        return D_shelter_1f_heliport_80180D48;
                    case 3:
                        return D_shelter_1f_heliport_80180D54;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180B7C;
                    case 1:
                        return D_shelter_1f_heliport_80180B8C;
                    case 2:
                        return D_shelter_1f_heliport_80180B9C;
                    case 3:
                        return D_shelter_1f_heliport_80180BA4;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180BF0;
                    case 1:
                        return D_shelter_1f_heliport_80180C08;
                    case 2:
                        return D_shelter_1f_heliport_80180C1C;
                    case 3:
                        return D_shelter_1f_heliport_80180C24;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180BB4;
                    case 1:
                        return D_shelter_1f_heliport_80180BC4;
                    case 2:
                        return D_shelter_1f_heliport_80180BD4;
                    case 3:
                        return D_shelter_1f_heliport_80180BDC;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180B54;
                    case 1:
                        return D_shelter_1f_heliport_80180B5C;
                    case 2:
                        return D_shelter_1f_heliport_80180B64;
                    case 3:
                        return D_shelter_1f_heliport_80180B6C;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180E00;
                    case 1:
                        return D_shelter_1f_heliport_80180E0C;
                    case 2:
                        return D_shelter_1f_heliport_80180E14;
                    case 3:
                        return D_shelter_1f_heliport_80180E20;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180E2C;
                    case 1:
                        return D_shelter_1f_heliport_80180E38;
                    case 2:
                        return D_shelter_1f_heliport_80180E3C;
                    case 3:
                        return D_shelter_1f_heliport_80180E48;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180E54;
                    case 1:
                        return D_shelter_1f_heliport_80180E60;
                    case 2:
                        return D_shelter_1f_heliport_80180E68;
                    case 3:
                        return D_shelter_1f_heliport_80180E74;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180E80;
                    case 1:
                        return D_shelter_1f_heliport_80180E8C;
                    case 2:
                        return D_shelter_1f_heliport_80180E94;
                    case 3:
                        return D_shelter_1f_heliport_80180EA0;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180D8C;
                    case 1:
                        return D_shelter_1f_heliport_80180D94;
                    case 2:
                        return D_shelter_1f_heliport_80180D9C;
                    case 3:
                        return D_shelter_1f_heliport_80180DA4;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180DD8;
                    case 1:
                        return D_shelter_1f_heliport_80180DE4;
                    case 2:
                        return D_shelter_1f_heliport_80180DEC;
                    case 3:
                        return D_shelter_1f_heliport_80180DF4;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180DB0;
                    case 1:
                        return D_shelter_1f_heliport_80180DB8;
                    case 2:
                        return D_shelter_1f_heliport_80180DC4;
                    case 3:
                        return D_shelter_1f_heliport_80180DCC;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180D6C;
                    case 1:
                        return D_shelter_1f_heliport_80180D70;
                    case 2:
                        return D_shelter_1f_heliport_80180D74;
                    case 3:
                        return D_shelter_1f_heliport_80180D7C;
                }
                break;
        }
    }
    return D_shelter_1f_heliport_80181030;
}

/// Draws one row of the shop list and handles its input, recording the row's
/// id as the cursor item while the row is selected. Row 0xFFFE is greyed out
/// and unselectable unless `Gp_HasMappedItem` answers non-zero, and opens its
/// own panel; row 0xFFFC is greyed out while the scan holds item 0x8F. Any
/// other row is an item with its price, greyed out when `func_800B7420`
/// refuses it; confirm opens the buy panel and button 0x10 the item's detail
/// panel.
void func_shelter_1f_heliport_8017DDA0(UiList* prompt, UiObject* obj)
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
        D_shelter_1f_heliport_80180F48 = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->panel.field_0.w;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam(D_shelter_1f_heliport_80180F7C, 0, 0);
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
        Text_DrawString(&req, D_shelter_1f_heliport_80180F68);
        if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            Ui_SpawnFromDesc(&D_shelter_1f_heliport_80181134, 0, 1, 1, obj);
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
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_1f_heliport_80180F78, prompt->field_1C, 1, 0);
        if (prompt->field_C == 1 && blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            child = Ui_SpawnFromDesc(&D_shelter_1f_heliport_801810E0, itemId, 1, 1, obj);
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
            child2 = Ui_SpawnFromDesc(&D_shelter_1f_heliport_801810E0, itemId, 1, 1, obj);
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
static void func_shelter_1f_heliport_8017E22C(RoomShopList* shop, UiObject* obj, s32 item)
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
/// the fixed id list (`func_shelter_1f_heliport_8017D730`) and, in game mode
/// 0, which items of each unlocked price row are added: mode 0 ids 0x80-0x9F
/// and 9, 0xA, 0xC, 0x42-0x46; mode 1 ids 0xA0-0xBF; mode 2 ids 0x60-0x7F and
/// 0xD; mode 3 ids 1-0x5F other than those. Mode 3 also adds, for each of the
/// twelve two-bit levels in `Mc_SaveData[0].state.shopStock`, the id of that level
/// (the first slot needs level 2). With `Mc_SaveData[0].state.demoScene` 1 every row
/// and level is unlocked first.
static void func_shelter_1f_heliport_8017E378(RoomShopList* shop, UiObject* obj)
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
    ids  = func_shelter_1f_heliport_8017D730(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        func_shelter_1f_heliport_8017E22C(shop, obj, *ids);
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
                        item = D_shelter_1f_heliport_80180EAC[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    func_shelter_1f_heliport_8017E22C(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    func_shelter_1f_heliport_8017E22C(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    func_shelter_1f_heliport_8017E22C(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    func_shelter_1f_heliport_8017E22C(shop, obj, item);
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
                    func_shelter_1f_heliport_8017E22C(shop, obj, slot * 3 + (id = level + 0xE));
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
    D_shelter_1f_heliport_80180F48 = -1;
}

/// Titles and captions of the shop's panels.
static const u8 D_shelter_1f_heliport_8017D6D0[] = "Select";
static const u8 D_shelter_1f_heliport_8017D6D8[] = "BP";
static const u8 D_shelter_1f_heliport_8017D6DC[] = "List";
static const u8 D_shelter_1f_heliport_8017D6E4[] = "TOTAL";
static const u8 D_shelter_1f_heliport_8017D6EC[] = "Notice";

/// "Charge", with a stray non-zero byte after its terminator that C cannot
/// place, so the string stays assembly.
/// "Charge", followed by the non-zero padding the original toolchain left.
static const char D_shelter_1f_heliport_8017D6F4[8] = "Charge\0"
                                                      "2";

/// The shop's "Select" panel. On its first frame it allocates the
/// `RoomShopList` work block, fills it through
/// `func_shelter_1f_heliport_8017E378` and opens the panel
/// `D_shelter_1f_heliport_80181150` beside it. Every frame it draws the list and the
/// "BP" caption; menu reports -1 and cancel 6 to the parent. A child that
/// reports 6 is torn down and the list takes input again; one that reports -1
/// passes it up.
void func_shelter_1f_heliport_8017E744(Task* task)
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
    Ui_DrawText(&(obj)->panel, (char*)D_shelter_1f_heliport_8017D6D0);
    if (task->state == 0) {
        mem = memCalloc(sizeof(RoomShopList), 0);
        if (mem != NULL) {
            shop               = mem;
            task->work         = (TaskIdMap*)shop;
            shop->list.funcs   = D_shelter_1f_heliport_80181034;
            shop->list.field_6 = 0;
            shop->list.field_7 = 0xF;
            func_shelter_1f_heliport_8017E378(shop, obj);
            Ui_LayoutListPanel(&shop->list, &(obj)->panel);
            shop->list.field_A = 1;
            Ui_SetListScrollFlag(&shop->list, 1);
            obj->panel.bounds.unsignedRect.h += 8;
            shop->list.field_17               = 8;
            Ui_SpawnFromDesc(&D_shelter_1f_heliport_80181150, 0, 0, 0, obj);
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
    Text_DrawString(&req, D_shelter_1f_heliport_8017D6D8);

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
void func_shelter_1f_heliport_8017E994(UiList* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_1f_heliport_80180F60, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = D_shelter_1f_heliport_80180FB8;
    obj->owner->spawnArg1.value = (u16)obj->owner->spawnArg1.value;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = D_shelter_1f_heliport_80180FC0;
            obj->owner->spawnArg1.value |= 0x10000;
            break;
        case 2:
            text                   = D_shelter_1f_heliport_80180FCC;
            obj->owner->spawnArg1.value |= 0x20000;
            break;
        case 3:
            text                   = D_shelter_1f_heliport_80180FD4;
            obj->owner->spawnArg1.value |= 0x30000;
            break;
    }

    if (*func_shelter_1f_heliport_8017D730(obj->owner->spawnArg1.value) == 0xFFFF) {
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
        Ui_SpawnFromDesc(&D_shelter_1f_heliport_801810A8, obj->owner->spawnArg1, 1, 1, obj);
        obj->panel.field_0.w = 0;
    }
}

/// The shop's "List" panel, whose rows are the modes. Its first frame clears
/// the item previews, opens the list-row panel and the preview panel, and lays
/// out its five-row list. Cancel or menu reports -1. A child reporting 6 is
/// torn down; one reporting -1 releases `Wip_UiHolder` and passes the code up.
void func_shelter_1f_heliport_8017EBB4(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2.pointer;
    list          = &D_shelter_1f_heliport_8018103C;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, (char*)D_shelter_1f_heliport_8017D6DC);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_shelter_1f_heliport_801810C4, task->spawnArg1, 0, 1, obj);
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
void func_shelter_1f_heliport_8017ED5C(Task* task)
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
    Text_DrawString(&req0, D_shelter_1f_heliport_8017D6D8);

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
    Text_DrawString(&req1, (char*)D_shelter_1f_heliport_8017D6E4);

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
void func_shelter_1f_heliport_8017EF40(UiList* prompt, UiObject* obj)
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
    Text_DrawString(&req, D_shelter_1f_heliport_80180F4C);

    mode = prompt->field_C;
    if (mode == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        cfg   = &Player_Status;
        price = Gp_ItemDescs[itemId].price;
        scan  = &Mc_SaveData[0].state.carriedItems;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_shelter_1f_heliport_801810FC, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_shelter_1f_heliport_801810FC, 1, 1, 1, obj);
                }
                obj->panel.field_0.w = 0;
            } else if ((obj->owner->parent->spawnArg1.value >> 16) == mode) {
                child = Ui_SpawnFromDesc(&D_shelter_1f_heliport_8018116C, itemId, 1, 1, obj);
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
            Ui_SpawnFromDesc(&D_shelter_1f_heliport_801810FC, 0, 1, 1, obj);
            obj->panel.field_0.w = 0;
        }
    }
}

/// Notice panel: shows one of three messages picked by `spawnArg1`, sized to
/// the text. Menu reports -1; confirm, cancel or 0xBC frames elapsing tell the
/// parent panel to close with 6.
void func_shelter_1f_heliport_8017F154(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1.value;
    obj  = task->spawnArg2.pointer;
    switch (kind) {
        case 1:
            text = D_shelter_1f_heliport_80180FF0;
            break;
        case 2:
            text = D_shelter_1f_heliport_80181000;
            break;
        default:
            text = D_shelter_1f_heliport_80180FDC;
            break;
    }

    Ui_DrawText(&(obj)->panel, (char*)D_shelter_1f_heliport_8017D6EC);
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
void func_shelter_1f_heliport_8017F2D4(Task* task)
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
    Ui_DrawText(&(obj)->panel, (char*)D_shelter_1f_heliport_8017D6F4);

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
            map                            = Gp_GetItemMap(slotId);
            D_shelter_1f_heliport_80182C9C = map;
            itemId                         = map->field_1;
            slot                           = Gp_GetItemSlot(itemId);
            if (D_shelter_1f_heliport_80182C9C->field_0 == 0) {
                D_shelter_1f_heliport_80182C98 = slot->ammoQty;
                slot->ammoQty                  = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_shelter_1f_heliport_80182C98 = slot->attachQty;
                slot->attachQty                = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown              = 0xBC;
            D_shelter_1f_heliport_80182C98 <<= 8;
            task->state                      = task->state + 1;
        }
    }

    curItem = D_shelter_1f_heliport_80182C9C->field_1;
    relItem = D_shelter_1f_heliport_80182C9C->field_2;
    if (D_shelter_1f_heliport_80182C9C->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                           <<= 8;
    D_shelter_1f_heliport_80182C98 += 0x40;
    if (qty < D_shelter_1f_heliport_80182C98) {
        D_shelter_1f_heliport_80182C98 = qty;
    }

    y = (s16)obj->panel.field_18.u;
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar(&(obj)->panel, (s16)obj->panel.field_1C.s, (s16)obj->panel.field_1E.u, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, D_shelter_1f_heliport_80182C98 >> 8, 0x606060);
    h = (s16)obj->panel.field_1A.u;
    func_800C0E20(&(obj)->panel, (s16)obj->panel.field_1C.s + 2, (s16)obj->panel.field_1E.u - 2, h - 6, qty,
                  D_shelter_1f_heliport_80182C98, 0x1741F);

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

static inline s32 _shelter_1f_heliportAddItemCount(s32 item, s32 count)
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
void func_shelter_1f_heliport_8017F59C(Task* task)
{
    u8          buf[0x10];
    TextDrawReq req;
    UiObject*   obj;
    s32         item;
    s32         y;
    s32         ry;
    s32         count;

    item         = D_shelter_1f_heliport_80180F48;
    obj          = task->spawnArg2.pointer;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && D_shelter_1f_heliport_80180F48 == Gp_GetPreviewItem()) {
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
        Text_DrawString(&req, D_shelter_1f_heliport_80181020);
        count = 0;
        count = _shelter_1f_heliportAddItemCount(item, count);
        Text_DrawPrompt(obj, (s16)obj->panel.field_1E.u - 2, y + 0xA, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
    }
}

/// Quantity picker of the buy prompt. Up and down step the count between 1 and
/// the most the player can take: for a stackable item, what its stock ceiling
/// still allows in steps of its per-buy amount; otherwise the free inventory
/// rows; in both cases no more than the BP affords. It shows the unit and
/// total price. Confirm takes the total and gives the items; confirm or
/// cancel tells the parent panel to close with 6.
void func_shelter_1f_heliport_8017F770(Task* task)
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
    Text_DrawPrompt(obj, left + 0x98, y, D_shelter_1f_heliport_8018102C, 0x606060, 3, 2);
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
    Text_DrawString(&req, D_shelter_1f_heliport_8017D6D8);

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
void func_shelter_1f_heliport_8017FBF0(UiList* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->panel.field_20.u + (u16)prompt->field_18;
    req.y          = obj->panel.field_22.u + (u16)prompt->field_1A;
    req.otIndex    = obj->panel.field_14.s + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, D_shelter_1f_heliport_80180F60);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

/// A list panel over `D_shelter_1f_heliport_80181068`. Cancel reports 6 and
/// menu -1; a child reporting 6 is torn down, one reporting -1 passes it up.
void func_shelter_1f_heliport_8017FCAC(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_shelter_1f_heliport_80181068;
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

/// Opens the UI object described by `D_shelter_1f_heliport_8018108C` for the
/// task's `spawnArg1`, waits until the object reports state -1 or 6, tears it
/// down, and ten frames later restores the frame timing, releases the
/// primitive buffer and kills the task.
void func_shelter_1f_heliport_8017FDD4(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_shelter_1f_heliport_8018108C, task->spawnArg1, 1, 1, NULL);
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

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_shelter_1f_heliport_8017FF08(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_1f_heliport_80182CB4.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_1f_heliport_80182CB4.fade != 0) {
                    D_shelter_1f_heliport_80182CA0.fade.field_0 = 0;
                    D_shelter_1f_heliport_80182CA0.fade.field_1 = 0;
                    D_shelter_1f_heliport_80182CA0.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_shelter_1f_heliport_80182CA0.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_1f_heliport_80182CB4.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_1f_heliport_80182CB4.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_1f_heliport_80182CB4.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant   = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_1f_heliport_80182CA8.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_1f_heliport_80182CA8.field_2;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_1f_heliport_80182CA8.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// State handlers of the room's controller task: installing its message
/// table, a per-frame state that runs `func_shelter_1f_heliport_801807C0`,
/// and the kill.
static const TaskFuncTable3 D_shelter_1f_heliport_8017D710 = {
    {
        func_shelter_1f_heliport_80180658,
        func_shelter_1f_heliport_80180748,
        taskKill,
    },
};

static __inline__ s32 _shelter1fHeliportStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_1f_heliport_80182CB0_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_1f_heliport_80182CA8 = *dst;
            D_shelter_1f_heliport_80182CB4 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_heliport_80181194, 0, 0, 0);
            D_shelter_1f_heliport_80182CB0_value = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_heliport_801800A0(Task* task, s32 msgId, RoomEventMsg * src, RoomEventMsg * dst)
{
    RoomLatchedEvent event;

    *dst = *src;
    func_map_neo_ark_80179B14(src, dst);
    if (src->prefix.packed == 0x1C && src->field_5 == 0) {
        SndEvt_EnqueueType7(0x55040006, 1);
        SndEvt_EnqueueType7(0x55040007, 1);
    }
    if (src->prefix.packed == 3) {
        if (GameFlag_GetNibble(0xE3) == 0 && gGameSession->at4.loc.place == 1) {
            if (src->field_5 == 0) {
                Gp_RunCapCmd1(0x2B);
            }
            return 2;
        }
        event.capCmd   = 0x29;
        event.stageSnd = 0x55040001;
        event.flagId   = 0;
        event.fade     = 1;
        if (src->field_5 == 0) {
            SndEvt_EnqueueType7(0x55040006, 1);
            SndEvt_EnqueueType7(0x55040007, 1);
        }
        return _shelter1fHeliportStartEvent(dst, &event);
    }
    return 1;
}

void func_shelter_1f_heliport_801802AC(s32 arg0)
{
    Task* task;
    Task* slotA;

    task  = gameGetPtrSlot(0xA);
    slotA = task;
    if (task == NULL) {
        task = gameGetPtrSlot(3);
    }
    if (slotA != NULL && GameFlag_GetNibble(0xE4) == 1) {
        D_shelter_1f_heliport_80181204.vy = 0;
    } else {
        D_shelter_1f_heliport_80181204.vy = 0x2710;
    }
    func_shelter_1f_heliport_8018085C(task->extra.tmd->coords, &D_shelter_1f_heliport_80181204);
}

s32 func_shelter_1f_heliport_80180334(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x124 && GameFlag_GetNibble(0xE4) == 1 && gameGetPtrSlot(0xA) != NULL) {
        found = 0;
        node  = Gp_PendingObj4C;
        while (node != NULL) {
            if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                found = 1;
                break;
            }
            node  = node->next;
            found = 0;
        }

        if (found != 0) {
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            GameFlag_SetNibble(0x4B, 9);
            Task_SpawnOnDefaultList(&D_80136CDC, 0, 0, 0);
            return 1;
        }
    }
    return 0;
}

s32 func_shelter_1f_heliport_8018041C(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    s32 need;

    switch (arg2) {
        case 0x21:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_1f_heliport_801811C8, 0, 0x21, 0);
            break;
        case 0x22:
            need = 1;
            if (gGameSession->at4.loc.place == 1) {
                need = 2;
            }
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x104) >= need ? 0x22 : 0x25, 0);
            break;
    }
    return 0;
}

s32 func_shelter_1f_heliport_801804BC(Task* arg0, s32 arg1, RoomEventMsg * in, GpMessageArg arg3)
{
    switch (in->field_2) {
        case 1:
            if (gGameSession->at4.loc.place == 1) {
                func_80149EBC();
            }
            if (gGameSession->at4.loc.place == 2) {
                func_80149E38();
            }
            break;
        case 2:
            func_80132038();
            break;
        case 3:
            func_80131FBC();
            break;
        case 4:
            func_80132110();
            break;
        case 5:
            func_801322A0();
            break;
    }
    return 0;
}

/// Runs cap command `spawnArg1` and waits for it to finish. When the cap
/// reports event key 0xF it sends `Gp_MsgPlayerWeapon(1)`, undoing the
/// `Gp_MsgPlayerWeapon(0)` its spawner sent, and kills itself; any other key
/// runs the command again.
void func_shelter_1f_heliport_80180594(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(task->spawnArg1.value);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
        advance:
            task->state += 1;
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xF) {
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            } else {
                task->state = 0;
            }
            break;
    }
}

static void func_shelter_1f_heliport_80180658(Task* arg0)
{
    arg0->msgTable = D_shelter_1f_heliport_801811A0;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 1) {
        func_80149E80();
    }
    if (gGameSession->at4.loc.place == 2) {
        func_80149FA4();
    }
    if (gameGetPtrSlot(0xA) != NULL) {
        func_8013230C();
    }
    func_shelter_1f_heliport_801802AC(0);
    func_shelter_1f_heliport_801807C0();
    Gpu_ResetGraphAndOt();
    Tmd_AllocMissingBuffers();
    SndEvt_EnqueueType6(0x55040006, 0, 0);
    SndEvt_EnqueueType6(0x55040007, 0, 0);
    arg0->state = arg0->state + 1;
}

static void func_shelter_1f_heliport_80180748(Task* task)
{
    func_shelter_1f_heliport_801807C0();
}

/// Runs the task's current state through its three-entry state table, copied
/// onto the stack before the call.
void func_shelter_1f_heliport_80180768(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_heliport_8017D710;
    sp.funcs[task->state](task);
}

static void func_shelter_1f_heliport_801807C0(void)
{
    s32 i;
    s32 idx = Mc_SaveData[0].state.at4.loc.view;

    if (gGameSession->at4.loc.place < 3 && idx < 12) {
        if (D_shelter_1f_heliport_801811D4[idx][0] != 0) {
            for (i = 0; i < 4; i++) {
                Gp_MsgSlot4Chain(i, D_shelter_1f_heliport_801811D4[idx][i]);
            }
        }
    }
}

/// Rebuilds the working mesh from its source under `coord`: the first four
/// vectors are rotated only, the eight after them rotated and translated and,
/// when `offset` is non-NULL, shifted by it afterwards.
static void func_shelter_1f_heliport_8018085C(GpCoord* coord, SVECTOR* offset)
{
    MATRIX        m;
    long          flag;
    s32           i;
    SVECTOR*      d;
    SVECTOR*      s;
    GpGridParams* dst = &D_shelter_1f_heliport_80181974;
    GpGridParams* src = &D_shelter_1f_heliport_801812AC;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    m = coord->coord;

    d = dst->field_4;
    s = src->field_4;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->field_8;
    s = src->field_8;
    if (offset != NULL) {
        for (i = 0; i < 8; i++) {
            RotTransSV(s, d, &flag);
            s++;
            d->vx += offset->vx;
            d->vy += offset->vy;
            d->vz += offset->vz;
            d++;
        }
    } else {
        for (i = 0; i < 8; i++) {
            RotTransSV(s++, d++, &flag);
        }
    }
}

void func_shelter_1f_heliport_80180B4C(Task* unused)
{
}
