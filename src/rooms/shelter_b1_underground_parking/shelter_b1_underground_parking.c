#include "rooms/shelter_b1_underground_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/action_prompt.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
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
#include "main/gameflow.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
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

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

/// Work block of the parking-lot examine task, hung off the `Task::work` slot
/// (0x1C) -- that slot is *not* a `TaskIdMap` here. Reach it with
/// `(SbupExamineWork*)task->work`.
///
/// `func_shelter_b1_underground_parking_80184468` copies a matched hotspot's
/// two table fields into `field_C` and `promptKind`;
/// `func_shelter_b1_underground_parking_80184594` forwards `promptKind` to
/// `func_800D4E78` as the display mode of the prompt it spawns.
/// `fadeLevel` is the intensity of the closing fade: the last state raises it
/// each frame, clamps it at 0xFF and draws it on all three channels of
/// `Fade_DrawOverlay`.
typedef struct SbupExamineWork {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x4];
    /* 0x08 */ s16  fadeLevel;
    /* 0x0A */ byte pad_A[0x2];
    /* 0x0C */ s16  field_C;
    /* 0x0E */ s8   promptKind;
    /* 0x0F */ byte pad_F[0x1];
} SbupExamineWork;

/// The departure the departure task carries out.
extern RoomDeparture D_shelter_b1_underground_parking_8018D77C;

/// The cutscene task's descriptor table; entry 0 runs a scene record, entry 1
/// is the scene's sub-task.
extern TaskDesc D_shelter_b1_underground_parking_8018720C[];

/// The "%" suffix appended to the play-data percentages.
extern u8 D_shelter_b1_underground_parking_8018691C[];

/// Descriptor of the play-data panels' shared frame.
extern UiObjectDesc D_shelter_b1_underground_parking_80186B34;

/// The item id the shop list's cursor last rested on.
extern s32 D_shelter_b1_underground_parking_80186FB0;

/// Flag tested as zero / non-zero when drawing the room's view-dependent
/// markers: it selects 0x180 or 0x60 as the second argument of their draw
/// calls. Its meaning is unproven.
extern u16 D_shelter_b1_underground_parking_8018D78C;

extern SVECTOR D_shelter_b1_underground_parking_8018771C[13];

extern void func_80131E38(void);

extern UiObjectDesc D_800611E4;

/// Labels of the nine play-data rows, and the help line each row shows while
/// it is selected.
extern u8 D_shelter_b1_underground_parking_801868C4[];
extern u8 D_shelter_b1_underground_parking_801868CC[];
extern u8 D_shelter_b1_underground_parking_801868D0[];
extern u8 D_shelter_b1_underground_parking_801868D8[];
extern u8 D_shelter_b1_underground_parking_801868E4[];
extern u8 D_shelter_b1_underground_parking_801868F4[];
extern u8 D_shelter_b1_underground_parking_801868FC[];
extern u8 D_shelter_b1_underground_parking_80186904[];
extern u8 D_shelter_b1_underground_parking_8018690C[];
extern u8 D_shelter_b1_underground_parking_80186920[];
extern u8 D_shelter_b1_underground_parking_8018694C[];
extern u8 D_shelter_b1_underground_parking_80186970[];
extern u8 D_shelter_b1_underground_parking_801869A0[];
extern u8 D_shelter_b1_underground_parking_801869D4[];
extern u8 D_shelter_b1_underground_parking_80186A08[];
extern u8 D_shelter_b1_underground_parking_80186A40[];
extern u8 D_shelter_b1_underground_parking_80186A74[];
extern u8 D_shelter_b1_underground_parking_80186AAC[];

/// The unit suffix appended to the play-data counts.
extern u8 D_shelter_b1_underground_parking_80186914[];

/// The list the weapon and PE usage panels fill.
extern UiList D_shelter_b1_underground_parking_80186B10;

/// Title and list of the menu `func_shelter_b1_underground_parking_8017EDE8`
/// runs.
static const char D_shelter_b1_underground_parking_8017D638[];
extern UiList     D_shelter_b1_underground_parking_80186B98;

/// List of the menu `func_shelter_b1_underground_parking_8017F2A0` runs.
extern UiList D_shelter_b1_underground_parking_80186AE8;

/// Texts the menu's four row handlers draw.
extern u8 D_shelter_b1_underground_parking_8018689C[];
extern u8 D_shelter_b1_underground_parking_801868A4[];
extern u8 D_shelter_b1_underground_parking_801868B0[];
extern u8 D_shelter_b1_underground_parking_801868BC[];

/// Descriptors of the panels the rows open.
extern UiObjectDesc D_shelter_b1_underground_parking_80186B50;
extern UiObjectDesc D_shelter_b1_underground_parking_80186B6C;

/// The 0xFFFF-terminated item id lists `func_shelter_b1_underground_parking_8017F80C`
/// chooses from.
extern u16 D_shelter_b1_underground_parking_80186BBC[];
extern u16 D_shelter_b1_underground_parking_80186BC4[];
extern u16 D_shelter_b1_underground_parking_80186BCC[];
extern u16 D_shelter_b1_underground_parking_80186BD4[];
extern u16 D_shelter_b1_underground_parking_80186BE4[];
extern u16 D_shelter_b1_underground_parking_80186BF4[];
extern u16 D_shelter_b1_underground_parking_80186C04[];
extern u16 D_shelter_b1_underground_parking_80186C0C[];
extern u16 D_shelter_b1_underground_parking_80186C1C[];
extern u16 D_shelter_b1_underground_parking_80186C2C[];
extern u16 D_shelter_b1_underground_parking_80186C3C[];
extern u16 D_shelter_b1_underground_parking_80186C44[];
extern u16 D_shelter_b1_underground_parking_80186C58[];
extern u16 D_shelter_b1_underground_parking_80186C70[];
extern u16 D_shelter_b1_underground_parking_80186C84[];
extern u16 D_shelter_b1_underground_parking_80186C8C[];
extern u16 D_shelter_b1_underground_parking_80186C9C[];
extern u16 D_shelter_b1_underground_parking_80186CB4[];
extern u16 D_shelter_b1_underground_parking_80186CC8[];
extern u16 D_shelter_b1_underground_parking_80186CD0[];
extern u16 D_shelter_b1_underground_parking_80186CE4[];
extern u16 D_shelter_b1_underground_parking_80186D00[];
extern u16 D_shelter_b1_underground_parking_80186D10[];
extern u16 D_shelter_b1_underground_parking_80186D1C[];
extern u16 D_shelter_b1_underground_parking_80186D34[];
extern u16 D_shelter_b1_underground_parking_80186D50[];
extern u16 D_shelter_b1_underground_parking_80186D64[];
extern u16 D_shelter_b1_underground_parking_80186D6C[];
extern u16 D_shelter_b1_underground_parking_80186D80[];
extern u16 D_shelter_b1_underground_parking_80186DA0[];
extern u16 D_shelter_b1_underground_parking_80186DB0[];
extern u16 D_shelter_b1_underground_parking_80186DBC[];
extern u16 D_shelter_b1_underground_parking_80186DD4[];
extern u16 D_shelter_b1_underground_parking_80186DD8[];
extern u16 D_shelter_b1_underground_parking_80186DDC[];
extern u16 D_shelter_b1_underground_parking_80186DE4[];
extern u16 D_shelter_b1_underground_parking_80186DF4[];
extern u16 D_shelter_b1_underground_parking_80186DFC[];
extern u16 D_shelter_b1_underground_parking_80186E04[];
extern u16 D_shelter_b1_underground_parking_80186E0C[];
extern u16 D_shelter_b1_underground_parking_80186E18[];
extern u16 D_shelter_b1_underground_parking_80186E20[];
extern u16 D_shelter_b1_underground_parking_80186E2C[];
extern u16 D_shelter_b1_underground_parking_80186E34[];
extern u16 D_shelter_b1_underground_parking_80186E40[];
extern u16 D_shelter_b1_underground_parking_80186E4C[];
extern u16 D_shelter_b1_underground_parking_80186E54[];
extern u16 D_shelter_b1_underground_parking_80186E5C[];
extern u16 D_shelter_b1_underground_parking_80186E68[];
extern u16 D_shelter_b1_underground_parking_80186E74[];
extern u16 D_shelter_b1_underground_parking_80186E7C[];
extern u16 D_shelter_b1_underground_parking_80186E88[];
extern u16 D_shelter_b1_underground_parking_80186E94[];
extern u16 D_shelter_b1_underground_parking_80186EA0[];
extern u16 D_shelter_b1_underground_parking_80186EA4[];
extern u16 D_shelter_b1_underground_parking_80186EB0[];
extern u16 D_shelter_b1_underground_parking_80186EBC[];
extern u16 D_shelter_b1_underground_parking_80186EC8[];
extern u16 D_shelter_b1_underground_parking_80186ED0[];
extern u16 D_shelter_b1_underground_parking_80186EDC[];
extern u16 D_shelter_b1_underground_parking_80186EE8[];
extern u16 D_shelter_b1_underground_parking_80186EF4[];
extern u16 D_shelter_b1_underground_parking_80186EFC[];
extern u16 D_shelter_b1_underground_parking_80186F08[];
extern u16 D_shelter_b1_underground_parking_80187098[];

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
extern u8           D_shelter_b1_underground_parking_80186FD0[];
extern u8           D_shelter_b1_underground_parking_80186FE0[];
extern u8           D_shelter_b1_underground_parking_80186FE4[];
extern UiObjectDesc D_shelter_b1_underground_parking_80187148;
extern UiObjectDesc D_shelter_b1_underground_parking_8018719C;

/// The shop's unlockable stock rows.
extern RoomShopTier D_shelter_b1_underground_parking_80186F14[13];

/// The shop list's row handlers and the balance panel beside it.
extern UiListItemFunc D_shelter_b1_underground_parking_8018709C[];
extern UiObjectDesc   D_shelter_b1_underground_parking_801871B8;

/// Texts of the four rows that pick an entry of the shop's id list, and the
/// panel they open.
extern u8           D_shelter_b1_underground_parking_80187020[];
extern u8           D_shelter_b1_underground_parking_80187028[];
extern u8           D_shelter_b1_underground_parking_80187034[];
extern u8           D_shelter_b1_underground_parking_8018703C[];
extern UiObjectDesc D_shelter_b1_underground_parking_80187110;

/// List of the menu `func_shelter_b1_underground_parking_80180C90` runs, and
/// the panel it opens first.
extern UiList       D_shelter_b1_underground_parking_801870A4;
extern UiObjectDesc D_shelter_b1_underground_parking_8018712C;

/// The purchase confirmation: its text and the panels it answers with.
extern u8           D_shelter_b1_underground_parking_80186FB4[];
extern UiObjectDesc D_shelter_b1_underground_parking_80187164;
extern UiObjectDesc D_shelter_b1_underground_parking_801871D4;

/// The three messages the notice panel picks from.
extern u8 D_shelter_b1_underground_parking_80187044[];
extern u8 D_shelter_b1_underground_parking_80187058[];
extern u8 D_shelter_b1_underground_parking_80187068[];

/// The charge panel's title, and the quantity and item map of the slot it is
/// animating.
static const char D_shelter_b1_underground_parking_8017D774[];
extern s32        D_shelter_b1_underground_parking_8018D744;
extern GpItemMap* D_shelter_b1_underground_parking_8018D748;

/// Label of the held-quantity line.
extern u8 D_shelter_b1_underground_parking_80187088[];

/// Text the quantity picker draws beside the item.
extern u8 D_shelter_b1_underground_parking_80187094[];

/// Text of the row that closes its panel.
extern u8 D_shelter_b1_underground_parking_80186FC8[];

/// The list `func_shelter_b1_underground_parking_80181D88` drives.
extern UiList D_shelter_b1_underground_parking_801870D0;

/// The descriptor of the modal panel `func_shelter_b1_underground_parking_80181EB0`
/// runs.
extern UiObjectDesc D_shelter_b1_underground_parking_801870F4;

/// The scene sub-task the cutscene runner spawned, while it runs.
extern Task* D_shelter_b1_underground_parking_8018D754;

/// The area records applied when the scene hands the Dryfield story on.

extern s32 D_shelter_b1_underground_parking_8018D758;

// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    RoomCutsceneRec value;
    u8              retained[8];
} ShelterB1UndergroundParkingStorageD75C;
STATIC_ASSERT_SIZEOF(ShelterB1UndergroundParkingStorageD75C, 32);

extern ShelterB1UndergroundParkingStorageD75C D_shelter_b1_underground_parking_8018D75C;
extern TaskDesc                               D_shelter_b1_underground_parking_80187200;
extern TaskDesc                               D_shelter_b1_underground_parking_80187260[];
extern TaskDesc                               D_shelter_b1_underground_parking_8018726C[];
extern GpFadeWork                             D_shelter_b1_underground_parking_8018D750;

/// The room's ambience table, one entry per area.
extern RoomAmbienceEntry D_shelter_b1_underground_parking_8018761C[];

extern GpEvsCmd D_shelter_b1_underground_parking_801872D8[];
extern GpEvsCmd D_shelter_b1_underground_parking_801873DC[];
extern GpEvsCmd D_shelter_b1_underground_parking_80187544[];

/// The task spawned from `D_shelter_b1_underground_parking_80187670`, and its
/// descriptor.
extern Task*    D_shelter_b1_underground_parking_8018D74C;
extern TaskDesc D_shelter_b1_underground_parking_80187670;

extern GpMsgEntry D_shelter_b1_underground_parking_80187230[];

extern DVECTOR        D_shelter_b1_underground_parking_801876D4[];
extern u8             D_shelter_b1_underground_parking_8018D788;
extern u8             D_shelter_b1_underground_parking_8018D789;
extern TaskDesc       D_shelter_b1_underground_parking_80187664[];
extern OverlayHotspot D_shelter_b1_underground_parking_8018767C[];
extern u8             D_shelter_b1_underground_parking_801876C4[];

extern SVECTOR D_shelter_b1_underground_parking_80187714[];
extern SVECTOR D_shelter_b1_underground_parking_80187784[];
extern SVECTOR D_shelter_b1_underground_parking_801877A4[];

static s32  func_shelter_b1_underground_parking_80183124(RoomEventMsg* in, RoomEventMsg* out);
static void func_shelter_b1_underground_parking_80183810(Task* arg0);
static void func_shelter_b1_underground_parking_8018414C(s32 x, s32 y, s32 variant);
static void func_shelter_b1_underground_parking_80184304(Task* task);
static void func_shelter_b1_underground_parking_801843F0(Task* task);
static void func_shelter_b1_underground_parking_80184468(Task* task);
static void func_shelter_b1_underground_parking_80184594(Task* task);
static void func_shelter_b1_underground_parking_801845F8(Task* task);
static void func_shelter_b1_underground_parking_801846EC(Task* arg0);
static void func_shelter_b1_underground_parking_80184778(Task* task);
static void func_shelter_b1_underground_parking_801847D0(Task* task);

static void func_shelter_b1_underground_parking_8018390C(void);
static void func_shelter_b1_underground_parking_801848A4(void);
static void func_shelter_b1_underground_parking_8018491C(void);
static s32  func_shelter_b1_underground_parking_80184964(OverlayHotspot* table, s16 x, s16 y);
static void func_shelter_b1_underground_parking_8018543C(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b1_underground_parking_801857E0(s16 x, s16 y, s16 radius, s16 color);
static void func_shelter_b1_underground_parking_80185A94(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b1_underground_parking_80185F08(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b1_underground_parking_80186890(s16 arg0);

static void func_shelter_b1_underground_parking_8017F7D0(Task* task);
static void func_shelter_b1_underground_parking_801848BC(Task* task);
static void func_shelter_b1_underground_parking_80184C54(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_shelter_b1_underground_parking_8017FE7C(UiList*, UiObject*);

void func_shelter_b1_underground_parking_8017DA50(UiList*, UiObject*);
void func_shelter_b1_underground_parking_8017E21C(UiList*, UiObject*);
void func_shelter_b1_underground_parking_8017EC34(Task*);
void func_shelter_b1_underground_parking_8017F0E0(Task*);
void func_shelter_b1_underground_parking_8017F2A0(Task*);
void func_shelter_b1_underground_parking_8017F494(UiList*, UiObject*);
void func_shelter_b1_underground_parking_8017F578(UiList*, UiObject*);
void func_shelter_b1_underground_parking_8017F640(UiList*, UiObject*);
void func_shelter_b1_underground_parking_8017F708(UiList*, UiObject*);
void func_shelter_b1_underground_parking_80180820(Task*);
void func_shelter_b1_underground_parking_80180A70(UiList*, UiObject*);
void func_shelter_b1_underground_parking_80180C90(Task*);
void func_shelter_b1_underground_parking_80180E38(Task*);
void func_shelter_b1_underground_parking_8018101C(UiList*, UiObject*);
void func_shelter_b1_underground_parking_80181230(Task*);
void func_shelter_b1_underground_parking_80181CCC(UiList*, UiObject*);
void func_shelter_b1_underground_parking_80181D88(Task*);

void func_shelter_b1_underground_parking_80180820(Task*);
void func_shelter_b1_underground_parking_801813B0(Task*);
void func_shelter_b1_underground_parking_80181678(Task*);
void func_shelter_b1_underground_parking_8018184C(Task*);
void func_shelter_b1_underground_parking_80181EB0(Task*);

extern GpGridParams   D_shelter_b1_underground_parking_80187E50[1];
extern GpGridParams   D_shelter_b1_underground_parking_801884D4[1];
extern GpGridParams   D_shelter_b1_underground_parking_80188BC4[1];
extern GpGridParams   D_shelter_b1_underground_parking_8018912C[1];
extern GpGridParams   D_shelter_b1_underground_parking_80189754[1];
extern GpObj4C        D_shelter_b1_underground_parking_8018B094[8];
extern GpObj4C        D_shelter_b1_underground_parking_8018B2F4[8];
extern GpObj4C        D_shelter_b1_underground_parking_8018B674[12];
extern GpObj4C        D_shelter_b1_underground_parking_8018BA04[12];
extern GpObj4C        D_shelter_b1_underground_parking_8018BD94[12];
extern GpObj4C        D_shelter_b1_underground_parking_8018C124[12];
extern GpObj4C        D_shelter_b1_underground_parking_8018C4B4[12];
extern GpObj4C        D_shelter_b1_underground_parking_8018C844[20];
extern GpObj4C        D_shelter_b1_underground_parking_8018CE34[16];
extern GpObj4C        D_shelter_b1_underground_parking_8018D2F4[11];
extern GpRoomBoundVec D_shelter_b1_underground_parking_8018D638[25];
extern GpRoomCoordSet D_shelter_b1_underground_parking_8018B07C[1];
s32                   func_shelter_b1_underground_parking_80182830(Task*, s32, RoomEventMsg*, GpMessageArg);
s32                   func_shelter_b1_underground_parking_80182A60(Task*, s32, s32, s32);
s32                   func_shelter_b1_underground_parking_80183284(Task*, s32, s32, GpMessageArg);
s32                   func_shelter_b1_underground_parking_80183360(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                   func_shelter_b1_underground_parking_801833DC(Task*, s32, s32, GpMessageArg);
void                  func_shelter_b1_underground_parking_80181FE4(Task*);
void                  func_shelter_b1_underground_parking_80182154(Task*);
void                  func_shelter_b1_underground_parking_80182DB4(Task*);
void                  func_shelter_b1_underground_parking_80182FC8(Task*);
void                  func_shelter_b1_underground_parking_801831F4(Task*);
void                  func_shelter_b1_underground_parking_80183410(Task*);
void                  func_shelter_b1_underground_parking_801834D4(Task*);
void                  func_shelter_b1_underground_parking_80183560(Task*);
void                  func_shelter_b1_underground_parking_8018363C(Task*);
void                  func_shelter_b1_underground_parking_801836D8(Task*);
void                  func_shelter_b1_underground_parking_80183714(Task*);
void                  func_shelter_b1_underground_parking_801837D8(u8);
void                  func_shelter_b1_underground_parking_80183804(u8);
void                  func_shelter_b1_underground_parking_80184234(Task*);
void                  func_shelter_b1_underground_parking_80184284(Task*);

extern GpSprtCmd  D_shelter_b1_underground_parking_80189AD8[2];
extern GpSprtCmd  D_shelter_b1_underground_parking_80189AE8[2];
extern GpSprtCmd  D_shelter_b1_underground_parking_80189BFC[3];
extern GpSprtCmd  D_shelter_b1_underground_parking_80189E94[4];
extern GpSprtCmd  D_shelter_b1_underground_parking_80189EB4[2];
extern GpSprtCmd  D_shelter_b1_underground_parking_80189EC4[2];
extern GpSprtElem D_shelter_b1_underground_parking_80189AF8[13];
extern GpSprtElem D_shelter_b1_underground_parking_80189C14[32];

u8 D_shelter_b1_underground_parking_8018689C[8] = {
    83,
    97,
    118,
    101,
    0,
    0,
    0,
    0,
};

u8 D_shelter_b1_underground_parking_801868A4[12] = {
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

u8 D_shelter_b1_underground_parking_801868B0[12] = {
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

u8 D_shelter_b1_underground_parking_801868BC[8] = {
    80,
    69,
    32,
    68,
    97,
    116,
    97,
    0,
};

u8 D_shelter_b1_underground_parking_801868C4[8] = {
    84,
    105,
    109,
    101,
    0,
    0,
    0,
    0,
};

u8 D_shelter_b1_underground_parking_801868CC[4] = {
    87,
    111,
    110,
    0,
};

u8 D_shelter_b1_underground_parking_801868D0[8] = {
    69,
    115,
    99,
    97,
    112,
    101,
    100,
    0,
};

u8 D_shelter_b1_underground_parking_801868D8[12] = {
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

u8 D_shelter_b1_underground_parking_801868E4[16] = {
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

u8 D_shelter_b1_underground_parking_801868F4[8] = {
    83,
    97,
    118,
    101,
    100,
    0,
    0,
    0,
};

u8 D_shelter_b1_underground_parking_801868FC[8] = {
    67,
    108,
    101,
    97,
    114,
    101,
    100,
    0,
};

u8 D_shelter_b1_underground_parking_80186904[8] = {
    77,
    97,
    120,
    32,
    69,
    88,
    80,
    0,
};

u8 D_shelter_b1_underground_parking_8018690C[8] = {
    77,
    97,
    120,
    32,
    66,
    80,
    0,
    0,
};

u8 D_shelter_b1_underground_parking_80186914[8] = {
    32,
    116,
    105,
    109,
    101,
    115,
    0,
    0,
};

u8 D_shelter_b1_underground_parking_8018691C[4] = {
    37,
    0,
    0,
    0,
};

u8 D_shelter_b1_underground_parking_80186920[44] = {
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

u8 D_shelter_b1_underground_parking_8018694C[36] = {
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

u8 D_shelter_b1_underground_parking_80186970[48] = {
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

u8 D_shelter_b1_underground_parking_801869A0[52] = {
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

u8 D_shelter_b1_underground_parking_801869D4[52] = {
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

u8 D_shelter_b1_underground_parking_80186A08[56] = {
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

u8 D_shelter_b1_underground_parking_80186A40[52] = {
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

u8 D_shelter_b1_underground_parking_80186A74[56] = {
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

u8 D_shelter_b1_underground_parking_80186AAC[56] = {
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

UiListItemFunc D_shelter_b1_underground_parking_80186AE4[1] = {
    func_shelter_b1_underground_parking_8017DA50,
};

UiList D_shelter_b1_underground_parking_80186AE8 = { D_shelter_b1_underground_parking_80186AE4, 9, { .u = 9 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_shelter_b1_underground_parking_80186B0C[1] = {
    func_shelter_b1_underground_parking_8017E21C,
};

UiList D_shelter_b1_underground_parking_80186B10 = { D_shelter_b1_underground_parking_80186B0C, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_shelter_b1_underground_parking_80186B34 = { 3, 0xFF70, 64, 288, 40, 56, 0, 0, 192, func_shelter_b1_underground_parking_8017F0E0, 0 };

UiObjectDesc D_shelter_b1_underground_parking_80186B50 = { 2, 0xFF70, 0xFF98, 288, 120, 40, 0, 0, 192, func_shelter_b1_underground_parking_8017F2A0, 0 };

UiObjectDesc D_shelter_b1_underground_parking_80186B6C = { 2, 0xFF70, 0xFF98, 288, 168, 40, 0, 0, 192, func_shelter_b1_underground_parking_8017EC34, 0 };

UiListItemFunc D_shelter_b1_underground_parking_80186B88[4] = {
    func_shelter_b1_underground_parking_8017F494,
    func_shelter_b1_underground_parking_8017F578,
    func_shelter_b1_underground_parking_8017F640,
    func_shelter_b1_underground_parking_8017F708,
};

UiList D_shelter_b1_underground_parking_80186B98 = { D_shelter_b1_underground_parking_80186B88, 4, { .u = 4 }, 1, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

u16 D_shelter_b1_underground_parking_80186BBC[4] = {
    140, 143, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186BC4[4] = {
    172, 175, 0xFFFE, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186BCC[4] = {
    103, 98, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186BD4[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186BE4[8] = {
    131, 140, 143, 10, 70, 138, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186BF4[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186C04[4] = {
    108, 100, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186C0C[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186C1C[8] = {
    132, 140, 143, 10, 70, 66, 138, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186C2C[8] = {
    160, 172, 171, 169, 175, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186C3C[4] = {
    98, 105, 106, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186C44[10] = {
    65, 59, 58, 1, 2, 6, 8, 4,
    0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186C58[12] = {
    131, 157, 140, 142, 143, 10, 70, 69,
    67, 138, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186C70[10] = {
    160, 161, 172, 173, 171, 169, 175, 0xFFFE,
    0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186C84[4] = {
    108, 100, 102, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186C8C[8] = {
    65, 59, 1, 6, 8, 4, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186C9C[12] = {
    157, 9, 140, 142, 138, 143, 10, 70,
    69, 66, 67, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186CB4[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186CC8[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186CD0[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186CE4[14] = {
    157, 9, 140, 142, 143, 10, 70, 69,
    66, 67, 68, 138, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186D00[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186D10[6] = {
    103, 98, 100, 97, 107, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186D1C[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186D34[14] = {
    140, 142, 138, 143, 10, 70, 69, 66,
    67, 68, 157, 9, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186D50[10] = {
    162, 166, 173, 174, 171, 169, 170, 175,
    0xFFFE, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186D64[4] = {
    100, 98, 97, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186D6C[10] = {
    65, 59, 58, 1, 2, 3, 6, 8,
    4, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186D80[16] = {
    140, 142, 138, 139, 143, 10, 70, 69,
    66, 67, 68, 144, 157, 9, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186DA0[8] = {
    162, 173, 174, 171, 170, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186DB0[6] = {
    100, 98, 97, 103, 107, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186DBC[12] = {
    65, 59, 58, 1, 2, 3, 6, 7,
    8, 4, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186DD4[2] = {
    139, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186DD8[2] = {
    171, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186DDC[4] = {
    108, 13, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186DE4[8] = {
    65, 59, 58, 60, 11, 55, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186DF4[4] = {
    131, 138, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186DFC[4] = {
    160, 171, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186E04[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186E0C[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186E18[4] = {
    140, 138, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186E20[6] = {
    160, 172, 171, 175, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186E2C[4] = {
    98, 13, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186E34[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186E40[6] = {
    131, 138, 143, 70, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186E4C[4] = {
    160, 171, 175, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186E54[4] = {
    108, 100, 13, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186E5C[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186E68[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186E74[4] = {
    171, 175, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186E7C[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186E88[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186E94[6] = {
    140, 138, 143, 70, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186EA0[2] = {
    171, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186EA4[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186EB0[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186EBC[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186EC8[4] = {
    171, 175, 0xFFFE, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186ED0[6] = {
    108, 100, 98, 13, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186EDC[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186EE8[6] = {
    140, 138, 143, 70, 157, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186EF4[4] = {
    171, 0xFFFE, 0xFFFF, 0,
};

u16 D_shelter_b1_underground_parking_80186EFC[6] = {
    108, 100, 98, 103, 13, 0xFFFF,
};

u16 D_shelter_b1_underground_parking_80186F08[6] = {
    65, 59, 58, 60, 11, 0xFFFF,
};

RoomShopTier D_shelter_b1_underground_parking_80186F14[13] = {
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

s32 D_shelter_b1_underground_parking_80186FB0 = -1;

u8 D_shelter_b1_underground_parking_80186FB4[20] = {
    80, 117, 114, 99, 104, 97, 115, 101, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
};

u8 D_shelter_b1_underground_parking_80186FC8[8] = {
    80, 97, 115, 115, 0, 0, 0, 0,
};

u8 D_shelter_b1_underground_parking_80186FD0[16] = {
    66, 97, 116, 116, 101, 114, 105, 101, 115, 47, 70, 117, 101, 108, 0, 0,
};

u8 D_shelter_b1_underground_parking_80186FE0[4] = { 0 };

u8 D_shelter_b1_underground_parking_80186FE4[60] = {
    87, 101, 97, 112, 111, 110, 115, 32, 117, 115, 105, 110, 103, 32, 98, 97,
    116, 116, 101, 114, 105, 101, 115, 32, 111, 114, 32, 102, 117, 101, 108, 10,
    99, 97, 110, 32, 98, 101, 32, 114, 101, 108, 111, 97, 100, 101, 100, 32,
    102, 111, 114, 32, 102, 114, 101, 101, 46, 0, 0, 0,
};

u8 D_shelter_b1_underground_parking_80187020[8] = {
    87, 101, 97, 112, 111, 110, 115, 0,
};

u8 D_shelter_b1_underground_parking_80187028[12] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 0, 0,
};

u8 D_shelter_b1_underground_parking_80187034[8] = {
    65, 114, 109, 111, 114, 0, 0, 0,
};

u8 D_shelter_b1_underground_parking_8018703C[8] = {
    73, 116, 101, 109, 115, 0, 0, 0,
};

u8 D_shelter_b1_underground_parking_80187044[20] = {
    73, 110, 115, 117, 102, 102, 105, 99, 105, 101, 110, 116, 32, 66, 80, 46,
    0, 0, 0, 0,
};

u8 D_shelter_b1_underground_parking_80187058[16] = {
    73, 110, 118, 101, 110, 116, 111, 114, 121, 32, 102, 117, 108, 108, 46, 0,
};

u8 D_shelter_b1_underground_parking_80187068[32] = {
    65, 109, 109, 117, 110, 105, 116, 105, 111, 110, 32, 99, 97, 112, 97, 99,
    105, 116, 121, 32, 114, 101, 97, 99, 104, 101, 100, 46, 0, 0, 0, 0,
};

u8 D_shelter_b1_underground_parking_80187088[12] = {
    65, 109, 111, 117, 110, 116, 0, 0, 0, 0, 0, 0,
};

u8 D_shelter_b1_underground_parking_80187094[4] = {
    120, 0, 0, 0,
};

u16 D_shelter_b1_underground_parking_80187098[2] = {
    0xFFFF, 0,
};

UiListItemFunc D_shelter_b1_underground_parking_8018709C[1] = {
    func_shelter_b1_underground_parking_8017FE7C,
};

UiListItemFunc D_shelter_b1_underground_parking_801870A0[1] = {
    func_shelter_b1_underground_parking_80180A70,
};

UiList D_shelter_b1_underground_parking_801870A4 = { D_shelter_b1_underground_parking_801870A0, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_shelter_b1_underground_parking_801870C8[2] = {
    func_shelter_b1_underground_parking_8018101C,
    func_shelter_b1_underground_parking_80181CCC,
};

UiList D_shelter_b1_underground_parking_801870D0 = { D_shelter_b1_underground_parking_801870C8, 2, { .u = 2 }, 1, 10, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_shelter_b1_underground_parking_801870F4 = { 2, 0xFF70, 0xFF98, 128, 40, 56, 0, 0, 192, func_shelter_b1_underground_parking_80180C90, 0 };

UiObjectDesc D_shelter_b1_underground_parking_80187110 = { 2, 0xFF74, 0xFFA3, 188, 160, 48, 0, 0, 192, func_shelter_b1_underground_parking_80180820, 0 };

UiObjectDesc D_shelter_b1_underground_parking_8018712C = { 0, 48, 4, 96, 60, 52, 0, 0, 192, func_shelter_b1_underground_parking_80180E38, 0 };

UiObjectDesc D_shelter_b1_underground_parking_80187148 = { 0, 48, 32, 70, 32, 20, 0, 0, 192, func_shelter_b1_underground_parking_80181D88, 0 };

UiObjectDesc D_shelter_b1_underground_parking_80187164 = { 2, 0xFFA0, 0xFFD0, 192, 96, 8, 0, 0, 192, func_shelter_b1_underground_parking_80181230, 0 };

// Retained data: Complete UI descriptor follows the adjacent UI descriptors. Its last 12 bytes also resemble a TaskDesc, which is its embedded task seed.
UiObjectDesc D_shelter_b1_underground_parking_80187180 = { 0, 0xFF80, 0xFFE0, 160, 92, 48, 0, 0, 192, func_shelter_b1_underground_parking_80180820, 0 };

UiObjectDesc D_shelter_b1_underground_parking_8018719C = { 2, 0xFFB8, 0xFFDC, 144, 64, 32, 0, 0, 192, func_shelter_b1_underground_parking_801813B0, 0 };

UiObjectDesc D_shelter_b1_underground_parking_801871B8 = { 0, 48, 0xFFA3, 96, 97, 44, 0, 0, 192, func_shelter_b1_underground_parking_80181678, 0 };

UiObjectDesc D_shelter_b1_underground_parking_801871D4 = { 3, 0xFFB8, 0xFFE0, 184, 48, 16, 0, 0, 192, func_shelter_b1_underground_parking_8018184C, 0 };

ShelterB1UndergroundParkingStorage71F0 D_shelter_b1_underground_parking_801871F0 = { { 0, 192, func_shelter_b1_underground_parking_80181EB0, { .model = NULL } }, { 0 } };

TaskDesc D_shelter_b1_underground_parking_80187200 = { 0, 32, func_shelter_b1_underground_parking_80181FE4, { .model = NULL } };

TaskDesc D_shelter_b1_underground_parking_8018720C[3] = {
    { 0, 32, func_shelter_b1_underground_parking_80182154, { .model = NULL } },
    { 0, 32, func_shelter_b1_underground_parking_801831F4, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_shelter_b1_underground_parking_80187230[6] = {
    { 5102, func_shelter_b1_underground_parking_80183360 },
    { 5105, func_shelter_b1_underground_parking_80183284 },
    { 5103, func_shelter_b1_underground_parking_80182830 },
    { 5104, func_shelter_b1_underground_parking_80182A60 },
    { 5106, func_shelter_b1_underground_parking_801833DC },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_b1_underground_parking_80187260[1] = {
    { 0, 32, func_shelter_b1_underground_parking_80183410, { .model = NULL } },
};

TaskDesc D_shelter_b1_underground_parking_8018726C[7] = {
    { 0, 32, func_shelter_b1_underground_parking_801834D4, { .model = NULL } },
    { 0, 32, func_shelter_b1_underground_parking_80183560, { .model = NULL } },
    { 0, 32, func_shelter_b1_underground_parking_8018363C, { .model = NULL } },
    { 0, 32, func_shelter_b1_underground_parking_80182DB4, { .model = NULL } },
    { 0, 32, func_shelter_b1_underground_parking_801836D8, { .model = NULL } },
    { 0, 32, func_shelter_b1_underground_parking_80182FC8, { .model = NULL } },
    { 0, 192, func_shelter_b1_underground_parking_80183714, { .model = NULL } },
};

GpXformArg D_shelter_b1_underground_parking_801872C0 = { { 3155, 0, -247, 0 }, { 0, -1024, 0, 0 } };

GpEvsCmd D_shelter_b1_underground_parking_801872D8[10] = {
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54140007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b1_underground_parking_801872C0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpAnimArg D_shelter_b1_underground_parking_801873C8 = { { .index = 1 }, 1, 0, 0, 0 };

GpEvsCmd D_shelter_b1_underground_parking_801873DC[15] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_shelter_b1_underground_parking_80183804 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_underground_parking_801873C8 }, { .value = 0 } },
    { 15, { .value = 0x54140003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54140009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_shelter_b1_underground_parking_801837D8 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_shelter_b1_underground_parking_80187544[9] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_shelter_b1_underground_parking_801837D8 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

RoomAmbienceEntry D_shelter_b1_underground_parking_8018761C[9] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 1, 0, 38, 0 },
    { 1, 0, 76, 0 },
    { 8, 0, 76, 0 },
    { 8, 0, 102, 0 },
    { 15, 0, 64, 0 },
    { 8, 0, 76, 0 },
    { 8, 0, 76, 0 },
};

TaskDesc D_shelter_b1_underground_parking_80187664[1] = {
    { 0, 192, func_shelter_b1_underground_parking_80184234, { .model = NULL } },
};

TaskDesc D_shelter_b1_underground_parking_80187670 = { 0, 32, func_shelter_b1_underground_parking_80184284, { .model = NULL } };

OverlayHotspot D_shelter_b1_underground_parking_8018767C[6] = {
    { -78, 77, 16, 16, 8, 1, 0 },
    { -48, 77, 16, 16, 4, 1, 0 },
    { -21, 77, 16, 16, 2, 1, 0 },
    { 3, 77, 16, 16, 1, 1, 0 },
    { 34, 77, 40, 16, 16, 1, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

u8 D_shelter_b1_underground_parking_801876C4[16] = {
    3,
    3,
    2,
    5,
    2,
    3,
    3,
    3,
    3,
    3,
    2,
    3,
    3,
    4,
    3,
    2,
};

DVECTOR D_shelter_b1_underground_parking_801876D4[16] = {
    { -66, -62 },
    { -26, -62 },
    { 14, -62 },
    { 62, -62 },
    { -66, -27 },
    { -26, -27 },
    { 14, -27 },
    { 62, -27 },
    { -66, 9 },
    { -26, 9 },
    { 14, 9 },
    { 62, 9 },
    { -66, 41 },
    { -26, 41 },
    { 14, 41 },
    { 62, 41 },
};

SVECTOR D_shelter_b1_underground_parking_80187714[1] = {
    { -180, -1300, -5490, 0 },
};

// Lighting task addresses entry 11, whose capsule renderer consumes two points.
SVECTOR D_shelter_b1_underground_parking_8018771C[13] = {
    { 2020, -2290, -7580, 0 },
    { -3240, -5470, 3600, 0 },
    { -2550, -5470, 2680, 0 },
    { -1840, -5470, 1720, 0 },
    { -1150, -5470, 810, 0 },
    { 280, -5470, 810, 0 },
    { 970, -5470, 1720, 0 },
    { 1710, -5470, 2680, 0 },
    { 2400, -5470, 3600, 0 },
    { -1160, -5470, -1070, 0 },
    { -1840, -5470, -1980, 0 },
    { 1430, -3930, -6650, 0 },
    { 2580, -3930, -6650, 0 },
};

SVECTOR D_shelter_b1_underground_parking_80187784[4] = {
    { 6800, -2150, -2900, 0 },
    { 7960, -2150, -2900, 0 },
    { 6800, -2150, 2900, 0 },
    { 7960, -2150, 2900, 0 },
};

SVECTOR D_shelter_b1_underground_parking_801877A4[2] = {
    { -7090, -3080, 1780, 0 },
    { -8250, -3080, 1780, 0 },
};

GpRoomCoordRec D_shelter_b1_underground_parking_801877B4[8] = {
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
};

GpRoomObjRec D_shelter_b1_underground_parking_801877F4[8] = {
    { D_shelter_b1_underground_parking_80187E50, D_shelter_b1_underground_parking_8018B094, D_shelter_b1_underground_parking_8018B674, NULL },
    { D_shelter_b1_underground_parking_801884D4, D_shelter_b1_underground_parking_8018B094, D_shelter_b1_underground_parking_8018BA04, NULL },
    { D_shelter_b1_underground_parking_80188BC4, D_shelter_b1_underground_parking_8018B094, D_shelter_b1_underground_parking_8018BD94, NULL },
    { D_shelter_b1_underground_parking_801884D4, D_shelter_b1_underground_parking_8018B094, D_shelter_b1_underground_parking_8018C124, NULL },
    { D_shelter_b1_underground_parking_801884D4, D_shelter_b1_underground_parking_8018B094, D_shelter_b1_underground_parking_8018C4B4, NULL },
    { D_shelter_b1_underground_parking_8018912C, D_shelter_b1_underground_parking_8018B094, D_shelter_b1_underground_parking_8018C844, NULL },
    { D_shelter_b1_underground_parking_8018912C, D_shelter_b1_underground_parking_8018B094, D_shelter_b1_underground_parking_8018CE34, NULL },
    { D_shelter_b1_underground_parking_80189754, D_shelter_b1_underground_parking_8018B2F4, D_shelter_b1_underground_parking_8018D2F4, NULL },
};

u8 D_shelter_b1_underground_parking_80187874[24] = {
    1,
    2,
    11,
    4,
    5,
    24,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    15,
};

u8 D_shelter_b1_underground_parking_8018788C[24] = {
    1,
    2,
    7,
    4,
    5,
    9,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8 D_shelter_b1_underground_parking_801878A4[24] = {
    1,
    2,
    7,
    4,
    5,
    22,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8 D_shelter_b1_underground_parking_801878BC[24] = {
    1,
    2,
    11,
    4,
    5,
    15,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8 D_shelter_b1_underground_parking_801878D4[24] = {
    1,
    10,
    12,
    14,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8 D_shelter_b1_underground_parking_801878EC[24] = {
    1,
    10,
    12,
    13,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8 D_shelter_b1_underground_parking_80187904[24] = {
    1,
    16,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    2,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8 * D_shelter_b1_underground_parking_8018791C[8] = {
    D_8010CAF8,
    D_shelter_b1_underground_parking_80187874,
    D_shelter_b1_underground_parking_8018788C,
    D_shelter_b1_underground_parking_801878A4,
    D_shelter_b1_underground_parking_801878BC,
    D_shelter_b1_underground_parking_801878D4,
    D_shelter_b1_underground_parking_801878EC,
    D_shelter_b1_underground_parking_80187904,
};

GpViewCountRec D_shelter_b1_underground_parking_8018793C[8] = {
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
};

GpWarpRec D_shelter_b1_underground_parking_8018794C[2] = {
    { { .words = { 0, 1913, 0, -7180 } }, { 0, 0, 0, 0 }, { .words = { 0, 1980, 0, -6660 } }, { 0, 0, 0, 0 }, 0x54140002, 0x54140001, 0, 2, 0, 0 },
    { { .words = { 2048, 401, 0, -1200 } }, { 0, 0, 0, 0 }, { .words = { 2048, 401, 0, -1200 } }, { 0, 0, 0, 0 }, 0x5414000A, 0, 0, 3, 0, 436 },
};

SVECTOR D_shelter_b1_underground_parking_801879BC[6] = {
    { 0, -4096, 0, 0 },
    { 0, 4096, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, 0, 4096, 0 },
    { 4096, 0, 0, 0 },
    { -4096, 0, 0, 0 },
};

SVECTOR D_shelter_b1_underground_parking_801879EC[60] = {
    { -5390, 0, 5650, 0 },
    { 0x2AF8, 0, 5650, 0 },
    { 0x2AF8, 0, -8000, 0 },
    { -5390, 0, -8000, 0 },
    { 0x2AF8, -5000, 5650, 0 },
    { -5390, -5000, 5650, 0 },
    { -5390, -5000, -8000, 0 },
    { 0x2AF8, -5000, -8000, 0 },
    { 3000, -4000, -5650, 0 },
    { 4000, -4000, -5650, 0 },
    { 4000, -6000, -5650, 0 },
    { 3000, -6000, -5650, 0 },
    { 3000, 0, -5650, 0 },
    { 4000, 0, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { -5000, -4000, -5650, 0 },
    { -5000, 0, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, -6000, -5650, 0 },
    { -5000, -6000, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, 0, -7650, 0 },
    { 1000, -4000, -7650, 0 },
    { 2900, 0, -7650, 0 },
    { 2900, -4000, -7650, 0 },
    { 2900, 0, -5650, 0 },
    { 2900, -4000, -5650, 0 },
    { -3300, 0, 4150, 0 },
    { -3300, -5000, 4150, 0 },
    { -3300, -5000, 5650, 0 },
    { -3300, 0, 5650, 0 },
    { -5000, 0, 4150, 0 },
    { -5000, -5000, 4150, 0 },
    { 0, 0, 5650, 0 },
    { 0, -5000, 5650, 0 },
    { 0, -5000, 3350, 0 },
    { 0, 0, 3350, 0 },
    { 4000, -5000, 3350, 0 },
    { 4000, 0, 3350, 0 },
    { 2896, 187, -4145, 0 },
    { 2896, -4813, -4145, 0 },
    { 2896, -4813, -5645, 0 },
    { 2896, 187, -5645, 0 },
    { 4236, 187, -4145, 0 },
    { 4236, -4813, -4145, 0 },
    { -1900, 0, -2950, 0 },
    { -1900, -5000, -2950, 0 },
    { -5000, -5000, -2950, 0 },
    { -5000, 0, -2950, 0 },
    { -1900, 0, -5650, 0 },
    { -1900, -5000, -5650, 0 },
    { -5000, 0, -6250, 0 },
    { -5000, -6000, -6250, 0 },
    { -5000, -6000, 6250, 0 },
    { -5000, 0, 6250, 0 },
    { 4000, 0, 6250, 0 },
    { 4000, -6000, 6250, 0 },
    { 4000, -6000, -6250, 0 },
    { 4000, 0, -6250, 0 },
};

GpGridFace D_shelter_b1_underground_parking_80187BCC[22] = {
    { { 1, 2, 0, 3 }, 0, 1 },
    { { 5, 6, 4, 7 }, 1, 0 },
    { { 1, 0, 4, 5 }, 2, 0 },
    { { 9, 10, 8, 11 }, 3, 0 },
    { { 13, 9, 12, 8 }, 3, 0 },
    { { 15, 16, 14, 17 }, 3, 0 },
    { { 8, 11, 14, 18 }, 3, 0 },
    { { 19, 15, 18, 14 }, 3, 0 },
    { { 21, 22, 20, 23 }, 4, 0 },
    { { 22, 24, 23, 25 }, 3, 0 },
    { { 24, 26, 25, 27 }, 5, 0 },
    { { 20, 23, 27, 25 }, 1, 0 },
    { { 29, 30, 28, 31 }, 4, 0 },
    { { 33, 29, 32, 28 }, 2, 0 },
    { { 35, 36, 34, 37 }, 5, 0 },
    { { 36, 38, 37, 39 }, 2, 0 },
    { { 41, 42, 40, 43 }, 5, 0 },
    { { 45, 41, 44, 40 }, 3, 0 },
    { { 47, 48, 46, 49 }, 3, 0 },
    { { 51, 47, 50, 46 }, 4, 0 },
    { { 53, 54, 52, 55 }, 4, 0 },
    { { 57, 58, 56, 59 }, 5, 0 },
};

s16 D_shelter_b1_underground_parking_80187CD4[8] = {
    0,
    1,
    5,
    7,
    18,
    19,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_80187CE4[8] = {
    0,
    1,
    5,
    7,
    18,
    19,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_80187CF4[9] = {
    0,
    1,
    2,
    12,
    13,
    14,
    15,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_80187D08[8] = {
    0,
    1,
    2,
    12,
    13,
    14,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_80187D18[17] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    16,
    17,
    18,
    19,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80187D3C[12] = {
    0,
    5,
    6,
    7,
    8,
    11,
    16,
    17,
    18,
    19,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80187D54[7] = {
    0,
    1,
    2,
    14,
    15,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80187D64[8] = {
    0,
    1,
    2,
    12,
    14,
    15,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80187D74[15] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    16,
    17,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80187D94[10] = {
    0,
    3,
    4,
    6,
    10,
    11,
    16,
    17,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DA8[6] = {
    0,
    1,
    2,
    15,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DB4[6] = {
    0,
    1,
    2,
    15,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DC0[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DC8[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DD0[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DD8[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DE0[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DE8[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DF0[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_shelter_b1_underground_parking_80187DF8[4] = {
    0,
    1,
    2,
    -1,
};

s16 * D_shelter_b1_underground_parking_80187E00[20] = {
    D_shelter_b1_underground_parking_80187CD4,
    D_shelter_b1_underground_parking_80187CE4,
    D_shelter_b1_underground_parking_80187CF4,
    D_shelter_b1_underground_parking_80187D08,
    D_shelter_b1_underground_parking_80187D18,
    D_shelter_b1_underground_parking_80187D3C,
    D_shelter_b1_underground_parking_80187D54,
    D_shelter_b1_underground_parking_80187D64,
    D_shelter_b1_underground_parking_80187D74,
    D_shelter_b1_underground_parking_80187D94,
    D_shelter_b1_underground_parking_80187DA8,
    D_shelter_b1_underground_parking_80187DB4,
    D_shelter_b1_underground_parking_80187DC0,
    D_shelter_b1_underground_parking_80187DC8,
    D_shelter_b1_underground_parking_80187DD0,
    D_shelter_b1_underground_parking_80187DD8,
    D_shelter_b1_underground_parking_80187DE0,
    D_shelter_b1_underground_parking_80187DE8,
    D_shelter_b1_underground_parking_80187DF0,
    D_shelter_b1_underground_parking_80187DF8,
};

GpGridParams D_shelter_b1_underground_parking_80187E50[1] = {
    { NULL, D_shelter_b1_underground_parking_801879BC, D_shelter_b1_underground_parking_801879EC, D_shelter_b1_underground_parking_80187BCC, D_shelter_b1_underground_parking_80187E00, 5390, 8000, 5, 4, 4000, 22 },
};

SVECTOR D_shelter_b1_underground_parking_80187E74[8] = {
    { 0, -4096, 0, 0 },
    { 0, 4096, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, 0, 4096, 0 },
    { 4096, 0, 0, 0 },
    { -4096, 0, 0, 0 },
    { -2171, 0, -3473, 0 },
    { -2171, 0, 3473, 0 },
};

SVECTOR D_shelter_b1_underground_parking_80187EB4[84] = {
    { -5390, 0, 5650, 0 },
    { 0x2AF8, 0, 5650, 0 },
    { 0x2AF8, 0, -8000, 0 },
    { -5390, 0, -8000, 0 },
    { 0x2AF8, -5000, 5650, 0 },
    { -5390, -5000, 5650, 0 },
    { -5390, -5000, -8000, 0 },
    { 0x2AF8, -5000, -8000, 0 },
    { 3000, -4000, -5650, 0 },
    { 4000, -4000, -5650, 0 },
    { 4000, -6000, -5650, 0 },
    { 3000, -6000, -5650, 0 },
    { 3000, 0, -5650, 0 },
    { 4000, 0, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { -5000, -4000, -5650, 0 },
    { -5000, 0, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, -6000, -5650, 0 },
    { -5000, -6000, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, 0, -7650, 0 },
    { 1000, -4000, -7650, 0 },
    { 2900, 0, -7650, 0 },
    { 2900, -4000, -7650, 0 },
    { 2900, 0, -5650, 0 },
    { 2900, -4000, -5650, 0 },
    { 4000, -3000, 1710, 0 },
    { 4000, -3000, 6250, 0 },
    { 4000, -6000, 6250, 0 },
    { 4000, -6000, 1710, 0 },
    { 4000, 0, 1710, 0 },
    { 4000, 0, 6250, 0 },
    { 4000, -3000, -1710, 0 },
    { 4000, -3000, -6250, 0 },
    { 4000, 0, -6250, 0 },
    { 4000, 0, -1710, 0 },
    { 4000, -6000, -1710, 0 },
    { 4000, -6000, -6250, 0 },
    { 9820, -3000, 2930, 0 },
    { 4000, -3000, 2930, 0 },
    { 4000, -3000, -2960, 0 },
    { 9820, -3000, -2960, 0 },
    { 4000, 0, -2960, 0 },
    { 9820, 0, -2960, 0 },
    { 9820, 0, 2930, 0 },
    { 4000, 0, 2930, 0 },
    { -3300, 0, 4150, 0 },
    { -3300, -5000, 4150, 0 },
    { -3300, -5000, 5650, 0 },
    { -3300, 0, 5650, 0 },
    { -5000, 0, 4150, 0 },
    { -5000, -5000, 4150, 0 },
    { 0, 0, 5650, 0 },
    { 0, -5000, 5650, 0 },
    { 0, -5000, 3350, 0 },
    { 0, 0, 3350, 0 },
    { 4000, -5000, 3350, 0 },
    { 4000, 0, 3350, 0 },
    { 2896, 187, -4145, 0 },
    { 2896, -4813, -4145, 0 },
    { 2896, -4813, -5645, 0 },
    { 2896, 187, -5645, 0 },
    { 4236, 187, -4145, 0 },
    { 4236, -4813, -4145, 0 },
    { -1900, 0, -2950, 0 },
    { -1900, -5000, -2950, 0 },
    { -5000, -5000, -2950, 0 },
    { -5000, 0, -2950, 0 },
    { -1900, 0, -5650, 0 },
    { -1900, -5000, -5650, 0 },
    { -5000, 0, -6250, 0 },
    { -5000, -6000, -6250, 0 },
    { -5000, -6000, 6250, 0 },
    { -5000, 0, 6250, 0 },
    { 4790, 0, 1210, 0 },
    { 4790, -3000, 1210, 0 },
    { 4790, -3000, -1210, 0 },
    { 4790, 0, -1210, 0 },
    { 4800, -3000, 1210, 0 },
    { 4800, 0, 1210, 0 },
    { 4800, 0, -1210, 0 },
    { 4800, -3000, -1210, 0 },
};

GpGridFace D_shelter_b1_underground_parking_80188154[33] = {
    { { 1, 2, 0, 3 }, 0, 1 },
    { { 5, 6, 4, 7 }, 1, 0 },
    { { 1, 0, 4, 5 }, 2, 0 },
    { { 9, 10, 8, 11 }, 3, 0 },
    { { 13, 9, 12, 8 }, 3, 0 },
    { { 15, 16, 14, 17 }, 3, 0 },
    { { 8, 11, 14, 18 }, 3, 0 },
    { { 19, 15, 18, 14 }, 3, 0 },
    { { 21, 22, 20, 23 }, 4, 0 },
    { { 22, 24, 23, 25 }, 3, 0 },
    { { 24, 26, 25, 27 }, 5, 0 },
    { { 20, 23, 27, 25 }, 1, 0 },
    { { 29, 30, 28, 31 }, 5, 0 },
    { { 33, 29, 32, 28 }, 5, 0 },
    { { 35, 36, 34, 37 }, 5, 0 },
    { { 28, 31, 34, 38 }, 5, 0 },
    { { 39, 35, 38, 34 }, 5, 0 },
    { { 41, 42, 40, 43 }, 1, 0 },
    { { 44, 45, 42, 43 }, 3, 0 },
    { { 45, 46, 43, 40 }, 5, 0 },
    { { 46, 47, 40, 41 }, 2, 0 },
    { { 49, 50, 48, 51 }, 4, 0 },
    { { 53, 49, 52, 48 }, 2, 0 },
    { { 55, 56, 54, 57 }, 5, 0 },
    { { 56, 58, 57, 59 }, 2, 0 },
    { { 61, 62, 60, 63 }, 5, 0 },
    { { 65, 61, 64, 60 }, 3, 0 },
    { { 67, 68, 66, 69 }, 3, 0 },
    { { 71, 67, 70, 66 }, 4, 0 },
    { { 73, 74, 72, 75 }, 4, 0 },
    { { 77, 78, 76, 79 }, 5, 0 },
    { { 28, 80, 32, 81 }, 6, 0 },
    { { 83, 34, 82, 37 }, 7, 0 },
};

s16 D_shelter_b1_underground_parking_801882E0[8] = {
    0,
    1,
    5,
    7,
    27,
    28,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_801882F0[8] = {
    0,
    1,
    5,
    7,
    27,
    28,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_80188300[9] = {
    0,
    1,
    2,
    21,
    22,
    23,
    24,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_80188314[8] = {
    0,
    1,
    2,
    21,
    22,
    23,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_80188324[18] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    14,
    16,
    25,
    26,
    27,
    28,
    -1,
};

s16 D_shelter_b1_underground_parking_80188348[17] = {
    0,
    5,
    6,
    7,
    8,
    11,
    14,
    15,
    16,
    17,
    18,
    25,
    26,
    27,
    28,
    32,
    -1,
};

s16 D_shelter_b1_underground_parking_8018836C[12] = {
    0,
    1,
    2,
    12,
    13,
    15,
    17,
    20,
    23,
    24,
    31,
    -1,
};

s16 D_shelter_b1_underground_parking_80188384[9] = {
    0,
    1,
    2,
    12,
    13,
    21,
    23,
    24,
    -1,
};

s16 D_shelter_b1_underground_parking_80188398[18] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    14,
    16,
    17,
    18,
    25,
    26,
    -1,
};

s16 D_shelter_b1_underground_parking_801883BC[19] = {
    0,
    3,
    4,
    6,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    25,
    26,
    30,
    31,
    32,
    -1,
};

s16 D_shelter_b1_underground_parking_801883E4[15] = {
    0,
    1,
    2,
    12,
    13,
    14,
    15,
    16,
    17,
    20,
    24,
    30,
    31,
    32,
    -1,
};

s16 D_shelter_b1_underground_parking_80188404[9] = {
    0,
    1,
    2,
    12,
    13,
    17,
    20,
    24,
    -1,
};

s16 D_shelter_b1_underground_parking_80188418[6] = {
    0,
    1,
    17,
    18,
    19,
    -1,
};

s16 D_shelter_b1_underground_parking_80188424[8] = {
    0,
    1,
    17,
    18,
    19,
    30,
    32,
    -1,
};

s16 D_shelter_b1_underground_parking_80188434[9] = {
    0,
    1,
    2,
    17,
    19,
    20,
    30,
    31,
    -1,
};

s16 D_shelter_b1_underground_parking_80188448[7] = {
    0,
    1,
    2,
    17,
    19,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_80188458[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_80188460[6] = {
    0,
    1,
    17,
    18,
    19,
    -1,
};

s16 D_shelter_b1_underground_parking_8018846C[7] = {
    0,
    1,
    2,
    17,
    19,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_8018847C[4] = {
    0,
    1,
    2,
    -1,
};

s16 * D_shelter_b1_underground_parking_80188484[20] = {
    D_shelter_b1_underground_parking_801882E0,
    D_shelter_b1_underground_parking_801882F0,
    D_shelter_b1_underground_parking_80188300,
    D_shelter_b1_underground_parking_80188314,
    D_shelter_b1_underground_parking_80188324,
    D_shelter_b1_underground_parking_80188348,
    D_shelter_b1_underground_parking_8018836C,
    D_shelter_b1_underground_parking_80188384,
    D_shelter_b1_underground_parking_80188398,
    D_shelter_b1_underground_parking_801883BC,
    D_shelter_b1_underground_parking_801883E4,
    D_shelter_b1_underground_parking_80188404,
    D_shelter_b1_underground_parking_80188418,
    D_shelter_b1_underground_parking_80188424,
    D_shelter_b1_underground_parking_80188434,
    D_shelter_b1_underground_parking_80188448,
    D_shelter_b1_underground_parking_80188458,
    D_shelter_b1_underground_parking_80188460,
    D_shelter_b1_underground_parking_8018846C,
    D_shelter_b1_underground_parking_8018847C,
};

GpGridParams D_shelter_b1_underground_parking_801884D4[1] = {
    { NULL, D_shelter_b1_underground_parking_80187E74, D_shelter_b1_underground_parking_80187EB4, D_shelter_b1_underground_parking_80188154, D_shelter_b1_underground_parking_80188484, 5390, 8000, 5, 4, 4000, 33 },
};

SVECTOR D_shelter_b1_underground_parking_801884F8[8] = {
    { 0, -4096, 0, 0 },
    { 0, 4096, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, 0, 4096, 0 },
    { 4096, 0, 0, 0 },
    { -4096, 0, 0, 0 },
    { -2191, 0, -3461, 0 },
    { -1850, 0, 3654, 0 },
};

SVECTOR D_shelter_b1_underground_parking_80188538[90] = {
    { -5390, 0, 5650, 0 },
    { 0x2AF8, 0, 5650, 0 },
    { 0x2AF8, 0, -8000, 0 },
    { -5390, 0, -8000, 0 },
    { 0x2AF8, -5000, 5650, 0 },
    { -5390, -5000, 5650, 0 },
    { -5390, -5000, -8000, 0 },
    { 0x2AF8, -5000, -8000, 0 },
    { 3000, -4000, -5650, 0 },
    { 4000, -4000, -5650, 0 },
    { 4000, -6000, -5650, 0 },
    { 3000, -6000, -5650, 0 },
    { 3000, 0, -5650, 0 },
    { 4000, 0, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { -5000, -4000, -5650, 0 },
    { -5000, 0, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, -6000, -5650, 0 },
    { -5000, -6000, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, 0, -7650, 0 },
    { 1000, -4000, -7650, 0 },
    { 2900, 0, -7650, 0 },
    { 2900, -4000, -7650, 0 },
    { 2900, 0, -5650, 0 },
    { 2900, -4000, -5650, 0 },
    { 4000, -3000, 1710, 0 },
    { 4000, -3000, 6250, 0 },
    { 4000, -6000, 6250, 0 },
    { 4000, -6000, 1710, 0 },
    { 4000, 0, 1710, 0 },
    { 4000, 0, 6250, 0 },
    { 4000, -3000, -1710, 0 },
    { 4000, -3000, -6250, 0 },
    { 4000, 0, -6250, 0 },
    { 4000, 0, -1710, 0 },
    { 4000, -6000, -1710, 0 },
    { 4000, -6000, -6250, 0 },
    { 9820, -3000, 2930, 0 },
    { 4000, -3000, 2930, 0 },
    { 4000, -3000, -2960, 0 },
    { 9820, -3000, -2960, 0 },
    { 4000, 0, -2960, 0 },
    { 9820, 0, -2960, 0 },
    { 9820, 0, 2930, 0 },
    { 4000, 0, 2930, 0 },
    { -3300, 0, 4150, 0 },
    { -3300, -5000, 4150, 0 },
    { -3300, -5000, 5650, 0 },
    { -3300, 0, 5650, 0 },
    { -5000, 0, 4150, 0 },
    { -5000, -5000, 4150, 0 },
    { 0, 0, 5650, 0 },
    { 0, -5000, 5650, 0 },
    { 0, -5000, 3350, 0 },
    { 0, 0, 3350, 0 },
    { 4000, -5000, 3350, 0 },
    { 4000, 0, 3350, 0 },
    { 2896, 187, -4145, 0 },
    { 2896, -4813, -4145, 0 },
    { 2896, -4813, -5645, 0 },
    { 2896, 187, -5645, 0 },
    { 4236, 187, -4145, 0 },
    { 4236, -4813, -4145, 0 },
    { -1900, 0, -2950, 0 },
    { -1900, -5000, -2950, 0 },
    { -5000, -5000, -2950, 0 },
    { -5000, 0, -2950, 0 },
    { -1900, 0, -5650, 0 },
    { -1900, -5000, -5650, 0 },
    { -5000, 0, -6250, 0 },
    { -5000, -6000, -6250, 0 },
    { -5000, -6000, 6250, 0 },
    { -5000, 0, 6250, 0 },
    { 9820, -3000, 600, 0 },
    { 9820, 0, 600, 0 },
    { 4790, 0, 600, 0 },
    { 4790, -3000, 600, 0 },
    { 4790, -3000, -900, 0 },
    { 4790, 0, -900, 0 },
    { 9820, 0, -900, 0 },
    { 9820, -3000, -900, 0 },
    { 4790, 0, 1210, 0 },
    { 4790, -3000, 1210, 0 },
    { 4790, -3000, 600, 0 },
    { 4790, 0, 600, 0 },
    { 4790, -3000, -1310, 0 },
    { 4790, 0, -1310, 0 },
};

GpGridFace D_shelter_b1_underground_parking_80188808[36] = {
    { { 1, 2, 0, 3 }, 0, 1 },
    { { 5, 6, 4, 7 }, 1, 0 },
    { { 1, 0, 4, 5 }, 2, 0 },
    { { 9, 10, 8, 11 }, 3, 0 },
    { { 13, 9, 12, 8 }, 3, 0 },
    { { 15, 16, 14, 17 }, 3, 0 },
    { { 8, 11, 14, 18 }, 3, 0 },
    { { 19, 15, 18, 14 }, 3, 0 },
    { { 21, 22, 20, 23 }, 4, 0 },
    { { 22, 24, 23, 25 }, 3, 0 },
    { { 24, 26, 25, 27 }, 5, 0 },
    { { 20, 23, 27, 25 }, 1, 0 },
    { { 29, 30, 28, 31 }, 5, 0 },
    { { 33, 29, 32, 28 }, 5, 0 },
    { { 35, 36, 34, 37 }, 5, 0 },
    { { 28, 31, 34, 38 }, 5, 0 },
    { { 39, 35, 38, 34 }, 5, 0 },
    { { 41, 42, 40, 43 }, 1, 0 },
    { { 44, 45, 42, 43 }, 3, 0 },
    { { 45, 46, 43, 40 }, 5, 0 },
    { { 46, 47, 40, 41 }, 2, 0 },
    { { 49, 50, 48, 51 }, 4, 0 },
    { { 53, 49, 52, 48 }, 2, 0 },
    { { 55, 56, 54, 57 }, 5, 0 },
    { { 56, 58, 57, 59 }, 2, 0 },
    { { 61, 62, 60, 63 }, 5, 0 },
    { { 65, 61, 64, 60 }, 3, 0 },
    { { 67, 68, 66, 69 }, 3, 0 },
    { { 71, 67, 70, 66 }, 4, 0 },
    { { 73, 74, 72, 75 }, 4, 0 },
    { { 77, 78, 76, 79 }, 2, 0 },
    { { 81, 82, 80, 83 }, 3, 0 },
    { { 85, 86, 84, 87 }, 5, 0 },
    { { 28, 85, 32, 84 }, 6, 0 },
    { { 80, 88, 81, 89 }, 5, 0 },
    { { 88, 34, 89, 37 }, 7, 0 },
};

s16 D_shelter_b1_underground_parking_801889B8[8] = {
    0,
    1,
    5,
    7,
    27,
    28,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_801889C8[8] = {
    0,
    1,
    5,
    7,
    27,
    28,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_801889D8[9] = {
    0,
    1,
    2,
    21,
    22,
    23,
    24,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_801889EC[8] = {
    0,
    1,
    2,
    21,
    22,
    23,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_801889FC[18] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    14,
    16,
    25,
    26,
    27,
    28,
    -1,
};

s16 D_shelter_b1_underground_parking_80188A20[17] = {
    0,
    5,
    6,
    7,
    8,
    11,
    14,
    15,
    16,
    17,
    18,
    25,
    26,
    27,
    28,
    35,
    -1,
};

s16 D_shelter_b1_underground_parking_80188A44[12] = {
    0,
    1,
    2,
    12,
    13,
    15,
    17,
    20,
    23,
    24,
    33,
    -1,
};

s16 D_shelter_b1_underground_parking_80188A5C[9] = {
    0,
    1,
    2,
    12,
    13,
    21,
    23,
    24,
    -1,
};

s16 D_shelter_b1_underground_parking_80188A70[18] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    14,
    16,
    17,
    18,
    25,
    26,
    -1,
};

s16 D_shelter_b1_underground_parking_80188A94[22] = {
    0,
    3,
    4,
    6,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    25,
    26,
    30,
    31,
    32,
    33,
    34,
    35,
    -1,
};

s16 D_shelter_b1_underground_parking_80188AC0[18] = {
    0,
    1,
    2,
    12,
    13,
    14,
    15,
    16,
    17,
    20,
    24,
    30,
    31,
    32,
    33,
    34,
    35,
    -1,
};

s16 D_shelter_b1_underground_parking_80188AE4[9] = {
    0,
    1,
    2,
    12,
    13,
    17,
    20,
    24,
    -1,
};

s16 D_shelter_b1_underground_parking_80188AF8[6] = {
    0,
    1,
    17,
    18,
    19,
    -1,
};

s16 D_shelter_b1_underground_parking_80188B04[10] = {
    0,
    1,
    17,
    18,
    19,
    30,
    31,
    34,
    35,
    -1,
};

s16 D_shelter_b1_underground_parking_80188B18[11] = {
    0,
    1,
    2,
    17,
    19,
    20,
    30,
    31,
    32,
    33,
    -1,
};

s16 D_shelter_b1_underground_parking_80188B30[7] = {
    0,
    1,
    2,
    17,
    19,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_80188B40[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_80188B48[8] = {
    0,
    1,
    17,
    18,
    19,
    30,
    31,
    -1,
};

s16 D_shelter_b1_underground_parking_80188B58[9] = {
    0,
    1,
    2,
    17,
    19,
    20,
    30,
    31,
    -1,
};

s16 D_shelter_b1_underground_parking_80188B6C[4] = {
    0,
    1,
    2,
    -1,
};

s16 * D_shelter_b1_underground_parking_80188B74[20] = {
    D_shelter_b1_underground_parking_801889B8,
    D_shelter_b1_underground_parking_801889C8,
    D_shelter_b1_underground_parking_801889D8,
    D_shelter_b1_underground_parking_801889EC,
    D_shelter_b1_underground_parking_801889FC,
    D_shelter_b1_underground_parking_80188A20,
    D_shelter_b1_underground_parking_80188A44,
    D_shelter_b1_underground_parking_80188A5C,
    D_shelter_b1_underground_parking_80188A70,
    D_shelter_b1_underground_parking_80188A94,
    D_shelter_b1_underground_parking_80188AC0,
    D_shelter_b1_underground_parking_80188AE4,
    D_shelter_b1_underground_parking_80188AF8,
    D_shelter_b1_underground_parking_80188B04,
    D_shelter_b1_underground_parking_80188B18,
    D_shelter_b1_underground_parking_80188B30,
    D_shelter_b1_underground_parking_80188B40,
    D_shelter_b1_underground_parking_80188B48,
    D_shelter_b1_underground_parking_80188B58,
    D_shelter_b1_underground_parking_80188B6C,
};

GpGridParams D_shelter_b1_underground_parking_80188BC4[1] = {
    { NULL, D_shelter_b1_underground_parking_801884F8, D_shelter_b1_underground_parking_80188538, D_shelter_b1_underground_parking_80188808, D_shelter_b1_underground_parking_80188B74, 5390, 8000, 5, 4, 4000, 36 },
};

SVECTOR D_shelter_b1_underground_parking_80188BE8[6] = {
    { 0, -4096, 0, 0 },
    { 0, 4096, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, 0, 4096, 0 },
    { 4096, 0, 0, 0 },
    { -4096, 0, 0, 0 },
};

SVECTOR D_shelter_b1_underground_parking_80188C18[68] = {
    { -5390, 0, 5650, 0 },
    { 0x2AF8, 0, 5650, 0 },
    { 0x2AF8, 0, -8000, 0 },
    { -5390, 0, -8000, 0 },
    { 0x2AF8, -5000, 5650, 0 },
    { -5390, -5000, 5650, 0 },
    { -5390, -5000, -8000, 0 },
    { 0x2AF8, -5000, -8000, 0 },
    { 3000, -4000, -5650, 0 },
    { 4000, -4000, -5650, 0 },
    { 4000, -6000, -5650, 0 },
    { 3000, -6000, -5650, 0 },
    { 3000, 0, -5650, 0 },
    { 4000, 0, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { -5000, -4000, -5650, 0 },
    { -5000, 0, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, -6000, -5650, 0 },
    { -5000, -6000, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, 0, -7650, 0 },
    { 1000, -4000, -7650, 0 },
    { 2900, 0, -7650, 0 },
    { 2900, -4000, -7650, 0 },
    { 2900, 0, -5650, 0 },
    { 2900, -4000, -5650, 0 },
    { -3300, 0, 4150, 0 },
    { -3300, -5000, 4150, 0 },
    { -3300, -5000, 5650, 0 },
    { -3300, 0, 5650, 0 },
    { -5000, 0, 4150, 0 },
    { -5000, -5000, 4150, 0 },
    { 0, 0, 5650, 0 },
    { 0, -5000, 5650, 0 },
    { 0, -5000, 3350, 0 },
    { 0, 0, 3350, 0 },
    { 4000, -5000, 3350, 0 },
    { 4000, 0, 3350, 0 },
    { 2896, 187, -4145, 0 },
    { 2896, -4813, -4145, 0 },
    { 2896, -4813, -5645, 0 },
    { 2896, 187, -5645, 0 },
    { 4236, 187, -4145, 0 },
    { 4236, -4813, -4145, 0 },
    { -1900, 0, -2950, 0 },
    { -1900, -5000, -2950, 0 },
    { -5000, -5000, -2950, 0 },
    { -5000, 0, -2950, 0 },
    { -1900, 0, -5650, 0 },
    { -1900, -5000, -5650, 0 },
    { -5000, 0, -6250, 0 },
    { -5000, -6000, -6250, 0 },
    { -5000, -6000, 6250, 0 },
    { -5000, 0, 6250, 0 },
    { 4000, 0, 6250, 0 },
    { 4000, -6000, 6250, 0 },
    { 4000, -6000, -6250, 0 },
    { 4000, 0, -6250, 0 },
    { -1770, 0, 900, 0 },
    { -1770, -1800, 900, 0 },
    { -1770, -1800, -900, 0 },
    { -1770, 0, -900, 0 },
    { 2030, -1800, -900, 0 },
    { 2030, 0, -900, 0 },
    { 2030, -1800, 900, 0 },
    { 2030, 0, 900, 0 },
};

GpGridFace D_shelter_b1_underground_parking_80188E38[27] = {
    { { 1, 2, 0, 3 }, 0, 1 },
    { { 5, 6, 4, 7 }, 1, 0 },
    { { 1, 0, 4, 5 }, 2, 0 },
    { { 9, 10, 8, 11 }, 3, 0 },
    { { 13, 9, 12, 8 }, 3, 0 },
    { { 15, 16, 14, 17 }, 3, 0 },
    { { 8, 11, 14, 18 }, 3, 0 },
    { { 19, 15, 18, 14 }, 3, 0 },
    { { 21, 22, 20, 23 }, 4, 0 },
    { { 22, 24, 23, 25 }, 3, 0 },
    { { 24, 26, 25, 27 }, 5, 0 },
    { { 20, 23, 27, 25 }, 1, 0 },
    { { 29, 30, 28, 31 }, 4, 0 },
    { { 33, 29, 32, 28 }, 2, 0 },
    { { 35, 36, 34, 37 }, 5, 0 },
    { { 36, 38, 37, 39 }, 2, 0 },
    { { 41, 42, 40, 43 }, 5, 0 },
    { { 45, 41, 44, 40 }, 3, 0 },
    { { 47, 48, 46, 49 }, 3, 0 },
    { { 51, 47, 50, 46 }, 4, 0 },
    { { 53, 54, 52, 55 }, 4, 0 },
    { { 57, 58, 56, 59 }, 5, 0 },
    { { 61, 62, 60, 63 }, 5, 0 },
    { { 62, 64, 63, 65 }, 2, 0 },
    { { 64, 66, 65, 67 }, 4, 0 },
    { { 66, 61, 67, 60 }, 3, 0 },
    { { 66, 64, 61, 62 }, 0, 0 },
};

s16 D_shelter_b1_underground_parking_80188F7C[8] = {
    0,
    1,
    5,
    7,
    18,
    19,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_80188F8C[12] = {
    0,
    1,
    5,
    7,
    18,
    19,
    20,
    22,
    23,
    25,
    26,
    -1,
};

s16 D_shelter_b1_underground_parking_80188FA4[13] = {
    0,
    1,
    2,
    12,
    13,
    14,
    15,
    20,
    22,
    23,
    25,
    26,
    -1,
};

s16 D_shelter_b1_underground_parking_80188FC0[8] = {
    0,
    1,
    2,
    12,
    13,
    14,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_80188FD0[17] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    16,
    17,
    18,
    19,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80188FF4[17] = {
    0,
    5,
    6,
    7,
    8,
    11,
    16,
    17,
    18,
    19,
    21,
    22,
    23,
    24,
    25,
    26,
    -1,
};

s16 D_shelter_b1_underground_parking_80189018[12] = {
    0,
    1,
    2,
    14,
    15,
    21,
    22,
    23,
    24,
    25,
    26,
    -1,
};

s16 D_shelter_b1_underground_parking_80189030[8] = {
    0,
    1,
    2,
    12,
    14,
    15,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80189040[15] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    16,
    17,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_80189060[14] = {
    0,
    3,
    4,
    6,
    10,
    11,
    16,
    17,
    21,
    23,
    24,
    25,
    26,
    -1,
};

s16 D_shelter_b1_underground_parking_8018907C[10] = {
    0,
    1,
    2,
    15,
    21,
    23,
    24,
    25,
    26,
    -1,
};

s16 D_shelter_b1_underground_parking_80189090[6] = {
    0,
    1,
    2,
    15,
    21,
    -1,
};

s16 D_shelter_b1_underground_parking_8018909C[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_801890A4[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_801890AC[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_shelter_b1_underground_parking_801890B4[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_shelter_b1_underground_parking_801890BC[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_801890C4[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_801890CC[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_shelter_b1_underground_parking_801890D4[4] = {
    0,
    1,
    2,
    -1,
};

s16 * D_shelter_b1_underground_parking_801890DC[20] = {
    D_shelter_b1_underground_parking_80188F7C,
    D_shelter_b1_underground_parking_80188F8C,
    D_shelter_b1_underground_parking_80188FA4,
    D_shelter_b1_underground_parking_80188FC0,
    D_shelter_b1_underground_parking_80188FD0,
    D_shelter_b1_underground_parking_80188FF4,
    D_shelter_b1_underground_parking_80189018,
    D_shelter_b1_underground_parking_80189030,
    D_shelter_b1_underground_parking_80189040,
    D_shelter_b1_underground_parking_80189060,
    D_shelter_b1_underground_parking_8018907C,
    D_shelter_b1_underground_parking_80189090,
    D_shelter_b1_underground_parking_8018909C,
    D_shelter_b1_underground_parking_801890A4,
    D_shelter_b1_underground_parking_801890AC,
    D_shelter_b1_underground_parking_801890B4,
    D_shelter_b1_underground_parking_801890BC,
    D_shelter_b1_underground_parking_801890C4,
    D_shelter_b1_underground_parking_801890CC,
    D_shelter_b1_underground_parking_801890D4,
};

GpGridParams D_shelter_b1_underground_parking_8018912C[1] = {
    { NULL, D_shelter_b1_underground_parking_80188BE8, D_shelter_b1_underground_parking_80188C18, D_shelter_b1_underground_parking_80188E38, D_shelter_b1_underground_parking_801890DC, 5390, 8000, 5, 4, 4000, 27 },
};

SVECTOR D_shelter_b1_underground_parking_80189150[8] = {
    { 0, -4096, 0, 0 },
    { 0, 4096, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, 0, 4096, 0 },
    { 4096, 0, 0, 0 },
    { -4096, 0, 0, 0 },
    { -2428, 0, 3299, 0 },
    { 3053, 0, 2731, 0 },
};

SVECTOR D_shelter_b1_underground_parking_80189190[83] = {
    { -5390, 0, 5650, 0 },
    { 0x2AF8, 0, 5650, 0 },
    { 0x2AF8, 0, -8000, 0 },
    { -5390, 0, -8000, 0 },
    { 0x2AF8, -5000, 5650, 0 },
    { -5390, -5000, 5650, 0 },
    { -5390, -5000, -8000, 0 },
    { 0x2AF8, -5000, -8000, 0 },
    { 3000, -4000, -5650, 0 },
    { 4000, -4000, -5650, 0 },
    { 4000, -6000, -5650, 0 },
    { 3000, -6000, -5650, 0 },
    { 3000, 0, -5650, 0 },
    { 4000, 0, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { -5000, -4000, -5650, 0 },
    { -5000, 0, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, -6000, -5650, 0 },
    { -5000, -6000, -5650, 0 },
    { 1000, -4000, -5650, 0 },
    { 1000, 0, -5650, 0 },
    { 1000, 0, -7650, 0 },
    { 1000, -4000, -7650, 0 },
    { 2900, 0, -7650, 0 },
    { 2900, -4000, -7650, 0 },
    { 2900, 0, -5650, 0 },
    { 2900, -4000, -5650, 0 },
    { -3300, 0, 4150, 0 },
    { -3300, -5000, 4150, 0 },
    { -3300, -5000, 5650, 0 },
    { -3300, 0, 5650, 0 },
    { -5000, 0, 4150, 0 },
    { -5000, -5000, 4150, 0 },
    { 0, 0, 5650, 0 },
    { 0, -5000, 5650, 0 },
    { 0, -5000, 3350, 0 },
    { 0, 0, 3350, 0 },
    { 4000, -5000, 3350, 0 },
    { 4000, 0, 3350, 0 },
    { 2896, 187, -4145, 0 },
    { 2896, -4813, -4145, 0 },
    { 2896, -4813, -5645, 0 },
    { 2896, 187, -5645, 0 },
    { 4236, 187, -4145, 0 },
    { 4236, -4813, -4145, 0 },
    { -1900, 0, -5650, 0 },
    { -1900, -5000, -5650, 0 },
    { -1900, -5000, -1001, 0 },
    { -1900, 0, -1001, 0 },
    { -5000, 0, -6250, 0 },
    { -5000, -6000, -6250, 0 },
    { -5000, -6000, 6250, 0 },
    { -5000, 0, 6250, 0 },
    { 4000, 0, 6250, 0 },
    { 4000, -6000, 6250, 0 },
    { 4000, -6000, -6250, 0 },
    { 4000, 0, -6250, 0 },
    { 1986, 0, 586, 0 },
    { 1986, -2000, 586, 0 },
    { 4188, -2000, 586, 0 },
    { 4188, 0, 586, 0 },
    { -2250, -2781, 1250, 0 },
    { 2250, -2781, 1250, 0 },
    { 2250, -2781, -1250, 0 },
    { -2250, -2781, -1250, 0 },
    { -2250, 219, -1250, 0 },
    { 2250, 219, -1250, 0 },
    { 2250, 219, 1250, 0 },
    { -182, 250, -5422, 0 },
    { -182, -5181, -5422, 0 },
    { -522, -5181, -5672, 0 },
    { -522, 250, -5672, 0 },
    { 80, 250, -5422, 0 },
    { 80, -5181, -5422, 0 },
    { 304, 250, -5672, 0 },
    { 304, -5181, -5672, 0 },
    { 3446, -313, -385, 0 },
    { 3446, 187, -385, 0 },
    { 3446, 187, 2745, 0 },
    { 3446, -313, 2745, 0 },
    { 4236, -313, -385, 0 },
    { 4236, 187, -385, 0 },
};

GpGridFace D_shelter_b1_underground_parking_80189428[30] = {
    { { 1, 2, 0, 3 }, 0, 1 },
    { { 5, 6, 4, 7 }, 1, 0 },
    { { 1, 0, 4, 5 }, 2, 0 },
    { { 9, 10, 8, 11 }, 3, 0 },
    { { 13, 9, 12, 8 }, 3, 0 },
    { { 15, 16, 14, 17 }, 3, 0 },
    { { 8, 11, 14, 18 }, 3, 0 },
    { { 19, 15, 18, 14 }, 3, 0 },
    { { 21, 22, 20, 23 }, 4, 0 },
    { { 22, 24, 23, 25 }, 3, 0 },
    { { 24, 26, 25, 27 }, 5, 0 },
    { { 20, 23, 27, 25 }, 1, 0 },
    { { 29, 30, 28, 31 }, 4, 0 },
    { { 33, 29, 32, 28 }, 2, 0 },
    { { 35, 36, 34, 37 }, 5, 0 },
    { { 36, 38, 37, 39 }, 2, 0 },
    { { 41, 42, 40, 43 }, 5, 0 },
    { { 45, 41, 44, 40 }, 3, 0 },
    { { 47, 48, 46, 49 }, 4, 0 },
    { { 51, 52, 50, 53 }, 4, 0 },
    { { 55, 56, 54, 57 }, 5, 0 },
    { { 59, 60, 58, 61 }, 2, 0 },
    { { 63, 64, 62, 65 }, 0, 0 },
    { { 65, 64, 66, 67 }, 2, 0 },
    { { 64, 63, 67, 68 }, 4, 0 },
    { { 70, 71, 69, 72 }, 6, 0 },
    { { 74, 70, 73, 69 }, 3, 0 },
    { { 76, 74, 75, 73 }, 7, 0 },
    { { 78, 79, 77, 80 }, 5, 0 },
    { { 82, 78, 81, 77 }, 2, 0 },
};

s16 D_shelter_b1_underground_parking_80189590[10] = {
    0,
    1,
    5,
    7,
    18,
    19,
    25,
    26,
    27,
    -1,
};

s16 D_shelter_b1_underground_parking_801895A4[9] = {
    0,
    1,
    5,
    7,
    18,
    19,
    22,
    23,
    -1,
};

s16 D_shelter_b1_underground_parking_801895B8[12] = {
    0,
    1,
    2,
    12,
    13,
    14,
    15,
    18,
    19,
    22,
    23,
    -1,
};

s16 D_shelter_b1_underground_parking_801895D0[8] = {
    0,
    1,
    2,
    12,
    13,
    14,
    19,
    -1,
};

s16 D_shelter_b1_underground_parking_801895E0[19] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    16,
    17,
    18,
    20,
    25,
    26,
    27,
    -1,
};

s16 D_shelter_b1_underground_parking_80189608[20] = {
    0,
    5,
    6,
    7,
    8,
    11,
    16,
    17,
    18,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_80189630[14] = {
    0,
    1,
    2,
    14,
    15,
    18,
    20,
    21,
    22,
    23,
    24,
    28,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_8018964C[8] = {
    0,
    1,
    2,
    12,
    14,
    15,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_8018965C[15] = {
    0,
    1,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    16,
    17,
    20,
    -1,
};

s16 D_shelter_b1_underground_parking_8018967C[16] = {
    0,
    3,
    4,
    6,
    10,
    11,
    16,
    17,
    20,
    21,
    22,
    23,
    24,
    28,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_8018969C[12] = {
    0,
    1,
    2,
    15,
    20,
    21,
    22,
    23,
    24,
    28,
    29,
    -1,
};

s16 D_shelter_b1_underground_parking_801896B4[7] = {
    0,
    1,
    2,
    15,
    20,
    28,
    -1,
};

s16 D_shelter_b1_underground_parking_801896C4[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_801896CC[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_801896D4[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_shelter_b1_underground_parking_801896DC[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_shelter_b1_underground_parking_801896E4[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_801896EC[3] = {
    0,
    1,
    -1,
};

s16 D_shelter_b1_underground_parking_801896F4[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_shelter_b1_underground_parking_801896FC[4] = {
    0,
    1,
    2,
    -1,
};

s16 * D_shelter_b1_underground_parking_80189704[20] = {
    D_shelter_b1_underground_parking_80189590,
    D_shelter_b1_underground_parking_801895A4,
    D_shelter_b1_underground_parking_801895B8,
    D_shelter_b1_underground_parking_801895D0,
    D_shelter_b1_underground_parking_801895E0,
    D_shelter_b1_underground_parking_80189608,
    D_shelter_b1_underground_parking_80189630,
    D_shelter_b1_underground_parking_8018964C,
    D_shelter_b1_underground_parking_8018965C,
    D_shelter_b1_underground_parking_8018967C,
    D_shelter_b1_underground_parking_8018969C,
    D_shelter_b1_underground_parking_801896B4,
    D_shelter_b1_underground_parking_801896C4,
    D_shelter_b1_underground_parking_801896CC,
    D_shelter_b1_underground_parking_801896D4,
    D_shelter_b1_underground_parking_801896DC,
    D_shelter_b1_underground_parking_801896E4,
    D_shelter_b1_underground_parking_801896EC,
    D_shelter_b1_underground_parking_801896F4,
    D_shelter_b1_underground_parking_801896FC,
};

GpGridParams D_shelter_b1_underground_parking_80189754[1] = {
    { NULL, D_shelter_b1_underground_parking_80189150, D_shelter_b1_underground_parking_80189190, D_shelter_b1_underground_parking_80189428, D_shelter_b1_underground_parking_80189704, 5390, 8000, 5, 4, 4000, 30 },
};

GpViewRec D_shelter_b1_underground_parking_80189778[24] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 6700, 0x7530, 0 } }, 207 },
    { { { { -3932, 0, 1145 }, { 423, 3805, 1453 }, { -1064, 1513, -3654 } }, { -2240, 3770, -1530 } }, 257 },
    { { { { -4068, 0, 470 }, { 169, 3820, 1468 }, { -439, 1477, -3794 } }, { -2330, 3770, -5560 } }, 230 },
    { { { { 3526, 0, 2083 }, { 477, 3987, -807 }, { -2028, 938, 3432 } }, { -2830, 3820, 5540 } }, 207 },
    { { { { 3904, 0, 1237 }, { 348, 3930, -1099 }, { -1187, 1153, 3746 } }, { 1060, 3010, 580 } }, 257 },
    { { { { -1166, 0, -3926 }, { -943, 3975, 280 }, { 3811, 984, -1132 } }, { 1170, 2400, -1600 } }, 257 },
    { { { { -4068, 0, 470 }, { 169, 3820, 1468 }, { -439, 1477, -3794 } }, { -2330, 3770, -5560 } }, 230 },
    { { { { 3526, 0, 2083 }, { 477, 3987, -807 }, { -2028, 938, 3432 } }, { -2830, 3820, 5540 } }, 207 },
    { { { { -1166, 0, -3926 }, { -943, 3975, 280 }, { 3811, 984, -1132 } }, { 1170, 2400, -1600 } }, 257 },
    { { { { -3932, 0, 1145 }, { 423, 3805, 1453 }, { -1064, 1513, -3654 } }, { -2240, 3770, -1530 } }, 257 },
    { { { { -4068, 0, 470 }, { 169, 3820, 1468 }, { -439, 1477, -3794 } }, { -2330, 3770, -5560 } }, 230 },
    { { { { -4068, 0, 470 }, { 169, 3820, 1468 }, { -439, 1477, -3794 } }, { -2330, 3770, -5560 } }, 230 },
    { { { { 3526, 0, 2083 }, { 477, 3987, -807 }, { -2028, 938, 3432 } }, { -2830, 3820, 5540 } }, 207 },
    { { { { 3526, 0, 2083 }, { 477, 3987, -807 }, { -2028, 938, 3432 } }, { -2830, 3820, 5540 } }, 207 },
    { { { { -1166, 0, -3926 }, { -943, 3975, 280 }, { 3811, 984, -1132 } }, { 1170, 2400, -1600 } }, 257 },
    { { { { -4017, 0, 796 }, { 361, 3649, 1824 }, { -709, 1859, -3579 } }, { -2610, 2710, 3680 } }, 257 },
    { { { { -761, 0, -4024 }, { -1258, 3890, 237 }, { 3822, 1280, -722 } }, { 2770, 2710, 2400 } }, 257 },
    { { { { -749, 0, 4026 }, { 1241, 3896, 231 }, { -3830, 1262, -712 } }, { -3820, 2710, 2400 } }, 257 },
    { { { { 4075, 0, 405 }, { 192, 3605, -1934 }, { -357, 1944, 3587 } }, { -3190, 2710, 3250 } }, 257 },
    { { { { -3796, 0, 1536 }, { 629, 3736, 1554 }, { -1402, 1676, -3463 } }, { -390, 1800, 4710 } }, 257 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { 0x4092, 1420, 3350 } }, 6874 },
    { { { { -1166, 0, -3926 }, { -943, 3975, 280 }, { 3811, 984, -1132 } }, { 1170, 2400, -1600 } }, 257 },
    { { { { 711, 0, 4033 }, { 2858, 2890, -504 }, { -2846, 2902, 502 } }, { 1620, 1430, 1640 } }, 257 },
    { { { { -1166, 0, -3926 }, { -943, 3975, 280 }, { 3811, 984, -1132 } }, { 1170, 2400, -1600 } }, 257 },
};

GpSprtCmd D_shelter_b1_underground_parking_80189AD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_80189AE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_underground_parking_80189AF8[13] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -120, 1158, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -96, -80, 1514, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, -120, 689, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 479, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -128, -80, 823, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -160, -80, 493, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -24, 698, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 16, 666, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 32, 657, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 72, 651, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 88, 636, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 88, 625, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 104, 683, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_underground_parking_80189BFC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_underground_parking_80189C14[32] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 0, 1327, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -152, 16, 1339, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 32, 1367, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 40, 1358, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 32, 1341, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 48, 1374, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 56, 1364, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 64, 1370, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 80, 1311, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 96, 1247, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 80, 1309, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 88, 1235, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 72, 1375, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 72, 1375, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 80, 1381, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 80, 1287, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 104, 1210, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 104, 1224, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 88, 1307, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 96, 1300, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 96, 1299, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -24, 2239, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 8, 2318, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -24, 2293, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -8, 2308, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 8, 2343, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 24, 2392, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 8, 2391, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 24, 2394, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 24, 2358, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 0, 2337, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -24, 2273, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_underground_parking_80189E94[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { 21, 11, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_80189EB4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_80189EC4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_80189ED4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_80189EE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_80189EF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_underground_parking_80189F04[11] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 104, 780, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 88, 772, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 88, 700, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 80, 900, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 72, 863, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 80, 950, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 882, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, 104, 808, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 104, 873, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 80, 786, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 104, 843, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_underground_parking_80189FE0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_80189FF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_underground_parking_8018A008[42] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -120, 1158, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -96, -80, 1514, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, -120, 689, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 479, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -128, -80, 823, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -160, -80, 493, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -24, 698, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 16, 666, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 32, 657, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 72, 651, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 88, 636, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 88, 625, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 104, 683, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 16, 40, 1491, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 48, 1471, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 40, 1554, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 24, 1538, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 24, 1494, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 24, 1527, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 24, 1509, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 24, 1510, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 24, 1499, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 24, 1469, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 32, 1436, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 8, 1663, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, -16, 1640, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -16, 1586, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 8, 1586, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 0, 1622, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 0, 1614, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -8, 1542, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 8, 1543, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -8, 1606, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 8, 1653, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 8, 1361, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -8, 1391, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -8, 1586, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 8, 1631, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 40, 1486, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 8, 1465, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 16, 1517, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 24, 1527, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_underground_parking_8018A350[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 29, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_underground_parking_8018A370[65] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 0, 1327, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -152, 16, 1339, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 32, 1367, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 40, 1358, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 32, 1341, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 48, 1374, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 56, 1364, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 64, 1370, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 80, 1311, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 96, 1247, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 80, 1309, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 88, 1235, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 72, 1375, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 72, 1375, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 80, 1381, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 80, 1287, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 104, 1210, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 104, 1224, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 88, 1307, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 96, 1300, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 96, 1299, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -24, 2239, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 8, 2318, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -24, 2293, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -8, 2308, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 8, 2343, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 24, 2392, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 8, 2391, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 24, 2394, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 24, 2358, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 0, 2337, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -24, 2273, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, 72, 1465, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 56, 1587, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 48, 1680, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 48, 1575, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 1625, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1662, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, 24, 1813, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 32, 1692, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 56, 1575, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 40, 1584, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 48, 1553, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 48, 1496, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 64, 1777, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 56, 1428, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 56, 1383, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 72, 1345, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 72, 1350, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 48, 1394, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 48, 1532, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 40, 1384, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 40, 1578, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 32, 1595, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 16, 1540, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 16, 1558, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 16, 1733, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 16, 1659, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 32, 1400, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 16, 1702, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 16, 1598, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 24, 1576, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 32, 1589, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 56, 1393, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 56, 1403, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_underground_parking_8018A884[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 11, 0, 0, { 2, 0 } },
    { 32, 33, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018A8AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018A8BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018A8CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_underground_parking_8018A8DC[28] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -32, 1061, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -8, 1084, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 8, 1096, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 1149, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 40, 1131, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 56, 1136, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 64, 1122, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -80, 965, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -80, 1141, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -56, 1128, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, -32, 1206, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -8, 1169, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 16, 1237, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 24, 1181, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 32, 1152, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 56, 1034, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 64, 909, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 16, 868, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 32, 1173, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, -16, 1125, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -32, 807, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -88, 757, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -56, 1125, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, -88, 1025, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -80, 1060, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 0, 1171, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, -24, 1107, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, -48, 1103, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_underground_parking_8018AB0C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 20, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018AB2C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018AB3C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018AB4C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018AB5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018AB6C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018AB7C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_underground_parking_8018AB8C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b1_underground_parking_8018AB9C[24] = {
    { { .empty = D_shelter_b1_underground_parking_80189AD8 }, D_shelter_b1_underground_parking_80189AD8, NULL },
    { { .empty = D_shelter_b1_underground_parking_80189AE8 }, D_shelter_b1_underground_parking_80189AE8, NULL },
    { { .elements = D_shelter_b1_underground_parking_80189AF8 }, D_shelter_b1_underground_parking_80189BFC, NULL },
    { { .elements = D_shelter_b1_underground_parking_80189C14 }, D_shelter_b1_underground_parking_80189E94, NULL },
    { { .empty = D_shelter_b1_underground_parking_80189EB4 }, D_shelter_b1_underground_parking_80189EB4, NULL },
    { { .empty = D_shelter_b1_underground_parking_80189EC4 }, D_shelter_b1_underground_parking_80189EC4, NULL },
    { { .elements = D_shelter_b1_underground_parking_80189AF8 }, D_shelter_b1_underground_parking_80189BFC, NULL },
    { { .elements = D_shelter_b1_underground_parking_80189C14 }, D_shelter_b1_underground_parking_80189E94, NULL },
    { { .empty = D_shelter_b1_underground_parking_80189EF4 }, D_shelter_b1_underground_parking_80189EF4, NULL },
    { { .elements = D_shelter_b1_underground_parking_80189F04 }, D_shelter_b1_underground_parking_80189FE0, NULL },
    { { .elements = D_shelter_b1_underground_parking_80189AF8 }, D_shelter_b1_underground_parking_80189BFC, NULL },
    { { .elements = D_shelter_b1_underground_parking_8018A008 }, D_shelter_b1_underground_parking_8018A350, NULL },
    { { .elements = D_shelter_b1_underground_parking_8018A370 }, D_shelter_b1_underground_parking_8018A884, NULL },
    { { .elements = D_shelter_b1_underground_parking_8018A370 }, D_shelter_b1_underground_parking_8018A884, NULL },
    { { .empty = D_shelter_b1_underground_parking_8018A8BC }, D_shelter_b1_underground_parking_8018A8BC, NULL },
    { { .empty = D_shelter_b1_underground_parking_8018A8CC }, D_shelter_b1_underground_parking_8018A8CC, NULL },
    { { .elements = D_shelter_b1_underground_parking_8018A8DC }, D_shelter_b1_underground_parking_8018AB0C, NULL },
    { { .empty = D_shelter_b1_underground_parking_8018AB2C }, D_shelter_b1_underground_parking_8018AB2C, NULL },
    { { .empty = D_shelter_b1_underground_parking_8018AB3C }, D_shelter_b1_underground_parking_8018AB3C, NULL },
    { { .empty = D_shelter_b1_underground_parking_8018AB4C }, D_shelter_b1_underground_parking_8018AB4C, NULL },
    { { .empty = D_shelter_b1_underground_parking_8018AB5C }, D_shelter_b1_underground_parking_8018AB5C, NULL },
    { { .empty = D_shelter_b1_underground_parking_8018AB6C }, D_shelter_b1_underground_parking_8018AB6C, NULL },
    { { .empty = D_shelter_b1_underground_parking_8018AB7C }, D_shelter_b1_underground_parking_8018AB7C, NULL },
    { { .empty = D_shelter_b1_underground_parking_8018AB8C }, D_shelter_b1_underground_parking_8018AB8C, NULL },
};

GpPointLight D_shelter_b1_underground_parking_8018ACBC[10] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1570, -2500, 1829 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2866, 2867, 2867, { 0, 0 } }, 2816, 4608 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2410, -2500, 2490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 2816, 4608 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2410, -2500, -2730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 2816, 4608 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1580, -2139, -2730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 2816, 4608 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2108, -1663, -6654 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2304, 2867, 2266, { 0, 0 } }, 1301, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7240, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 1638, 1064, { 0, 0 } }, 2035, 2999 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x355C, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 1638, 1064, { 0, 0 } }, 4000, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x4D1C, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 1638, 1064, { 0, 0 } }, 4000, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7370, -2138, 2730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 2560, 4352 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7370, -2138, -2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 2560, 4352 },
};

GpRoomCoordSet D_shelter_b1_underground_parking_8018B07C[1] = {
    { 0, NULL, 10, D_shelter_b1_underground_parking_8018ACBC, 0, NULL },
};

GpObj4C D_shelter_b1_underground_parking_8018B094[8] = {
    { NULL, NULL, NULL, { 608, -3744, -3072, 0 }, { { -4222, -4192, -311, 0 }, { 4204, -4192, 298, 0 }, { -4222, 4192, -311, 0 }, { 4204, 4192, 298, 0 } }, { 295, 0, -4093, 0 }, { 0, 0, 4096, 0 }, 5948, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 656, -3776, -3391, 0 }, { { 4534, -4192, 308, 0 }, { -4560, -4192, -330, 0 }, { 4534, 4192, 308, 0 }, { -4560, 4192, -330, 0 } }, { -287, 0, 4085, 0 }, { 0, 0, 4096, 0 }, 6186, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -2384, -3648, 2848, 0 }, { { -4080, -4192, -1088, 0 }, { 4080, -4192, 1088, 0 }, { -4080, 4192, -1088, 0 }, { 4080, 4192, 1088, 0 } }, { 1056, 0, -3964, 0 }, { 0, 0, 4096, 0 }, 5948, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -448, -3616, -192, 0 }, { { 5232, -4192, 2624, 0 }, { -5232, -4192, -2624, 0 }, { 5232, 4192, 2624, 0 }, { -5232, 4192, -2624, 0 } }, { -1842, 0, 3672, 0 }, { 0, 0, 4096, 0 }, 7186, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -512, -3616, -90, 0 }, { { -5296, -4192, -2634, 0 }, { 5296, -4192, 2646, 0 }, { -5296, 4192, -2646, 0 }, { 5296, 4192, 2634, 0 } }, { 1833, -6, -3678, 0 }, { 0, 0, 4096, 0 }, 7240, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -2590, -3616, 2672, 0 }, { { 3440, -4192, 944, 0 }, { -3440, -4192, -944, 0 }, { 3440, 4192, 944, 0 }, { -3440, 4192, -944, 0 } }, { -1085, 0, 3951, 0 }, { 0, 0, 4096, 0 }, 5490, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 4176, -3552, -80, 0 }, { { 32, -4192, -2864, 0 }, { -32, -4192, 2864, 0 }, { 32, 4192, -2864, 0 }, { -32, 4192, 2864, 0 } }, { 4095, 0, 45, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 6, 3, 1, 0 },
    { NULL, NULL, NULL, { 4288, -3520, -191, 0 }, { { -32, -4192, 2480, 0 }, { 32, -4192, -2480, 0 }, { -32, 4192, 2480, 0 }, { 32, 4192, -2480, 0 } }, { -4103, 0, -53, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 3, 6, 129, 0 },
};

GpObj4C D_shelter_b1_underground_parking_8018B2F4[8] = {
    { NULL, NULL, NULL, { 774, -3744, -5703, 0 }, { { -1271, -4192, -842, 0 }, { 1272, -4192, 842, 0 }, { -1271, 4192, -842, 0 }, { 1272, 4192, 842, 0 } }, { 2265, 0, -3423, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 2, 17, 1, 0 },
    { NULL, NULL, NULL, { 799, -3552, -5824, 0 }, { { 1272, -4192, 842, 0 }, { -1271, -4192, -842, 0 }, { 1272, 4192, 842, 0 }, { -1271, 4192, -842, 0 } }, { -2267, 0, 3422, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 17, 2, 1, 0 },
    { NULL, NULL, NULL, { 3198, -3648, -5760, 0 }, { { -1221, -4192, 914, 0 }, { 1222, -4192, -914, 0 }, { -1221, 4192, 914, 0 }, { 1222, 4192, -914, 0 } }, { -2461, 0, -3289, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 2, 17, 1, 0 },
    { NULL, NULL, NULL, { 3230, -3712, -5888, 0 }, { { 1222, -4192, -914, 0 }, { -1221, -4192, 914, 0 }, { 1222, 4192, -914, 0 }, { -1221, 4192, 914, 0 } }, { 2459, 0, 3287, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 17, 2, 1, 0 },
    { NULL, NULL, NULL, { 2623, -3680, -1313, 0 }, { { -2005, -4192, 2, 0 }, { 2005, -4192, -1, 0 }, { -2005, 4192, 2, 0 }, { 2005, 4192, -1, 0 } }, { -4, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 17, 19, 1, 0 },
    { NULL, NULL, NULL, { 2656, -3616, -1441, 0 }, { { 2005, -4192, -1, 0 }, { -2005, -4192, 2, 0 }, { 2005, 4192, -1, 0 }, { -2005, 4192, 2, 0 } }, { 3, 0, 4103, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 19, 17, 1, 0 },
    { NULL, NULL, NULL, { 591, -3712, -3985, 0 }, { { -178, -4192, -2803, 0 }, { 178, -4192, 2804, 0 }, { -178, 4192, -2803, 0 }, { 178, 4192, 2804, 0 } }, { 4089, 0, -260, 0 }, { 0, 0, 4096, 0 }, 5042, 0, 17, 18, 1, 0 },
    { NULL, NULL, NULL, { 704, -3680, -3969, 0 }, { { 178, -4192, 2804, 0 }, { -178, -4192, -2803, 0 }, { 178, 4192, 2804, 0 }, { -178, 4192, -2803, 0 } }, { -4090, 0, 259, 0 }, { 0, 0, 4096, 0 }, 5042, 0, 18, 17, 129, 0 },
};

GpAreaTmdRec D_shelter_b1_underground_parking_8018B554[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b1_underground_parking_8018B560[1] = {
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaTmdRec D_shelter_b1_underground_parking_8018B570[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b1_underground_parking_8018B57C[1] = {
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaTmdRec D_shelter_b1_underground_parking_8018B58C[2] = {
    { 116, 615, 0, 0, { 0, 0 }, D_801401B0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b1_underground_parking_8018B5A4[2] = {
    { 116, 0, 0, 3700, 0, -114, 2488, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b1_underground_parking_8018B5C4[22] = {
    { NULL, NULL },
    { D_shelter_b1_underground_parking_8018B560, D_shelter_b1_underground_parking_8018B554 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_underground_parking_8018B57C, D_shelter_b1_underground_parking_8018B570 },
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
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_underground_parking_8018B5A4, D_shelter_b1_underground_parking_8018B58C },
};

GpObj4C D_shelter_b1_underground_parking_8018B674[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 0, 19, 19, 2, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 5, 10, 1, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4076, 0, -401, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 4960, -64, 32, 0 }, { { -1568, 0, -2928, 0 }, { 1568, 0, -2928, 0 }, { -1568, 0, 2928, 0 }, { 1568, 0, 2928, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 3318, 2, 7, 255, 2, 0 },
    { NULL, NULL, NULL, { -3568, -64, 4064, 0 }, { { -624, 0, -720, 0 }, { 624, 0, -720, 0 }, { -624, 0, 720, 0 }, { 624, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 799, 0, -4017, 0 }, 951, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { -1712, -64, -4704, 0 }, { { -656, 0, -2288, 0 }, { 656, 0, -2288, 0 }, { -656, 0, 2288, 0 }, { 656, 0, 2288, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 2374, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -3472, -64, -2656, 0 }, { { -2080, 0, -560, 0 }, { 2080, 0, -560, 0 }, { -2080, 0, 560, 0 }, { 2080, 0, 560, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, 4096, 0 }, 2141, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -160, -64, 4368, 0 }, { { -656, 0, -1376, 0 }, { 656, 0, -1376, 0 }, { -656, 0, 1376, 0 }, { 656, 0, 1376, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1519, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 1936, -64, 3216, 0 }, { { -2272, 0, -496, 0 }, { 2272, 0, -496, 0 }, { -2272, 0, 496, 0 }, { 2272, 0, 496, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 2318, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2106, 0, 3512, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { -3040, -64, 4688, 0 }, { { -576, 0, -992, 0 }, { 576, 0, -992, 0 }, { -576, 0, 992, 0 }, { 576, 0, 992, 0 } }, { 0, 4095, 0, 0 }, { 3920, 0, -1189, 0 }, 1144, 2, 4, 255, 130, 0 },
};

GpObj4C D_shelter_b1_underground_parking_8018BA04[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 0, 19, 19, 2, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 5, 10, 1, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 5536, -64, 32, 0 }, { { -1376, 0, -1360, 0 }, { 1376, 0, -1360, 0 }, { -1376, 0, 1360, 0 }, { 1376, 0, 1360, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 1932, 2, 7, 255, 2, 0 },
    { NULL, NULL, NULL, { -3472, -64, 3936, 0 }, { { -688, 0, -720, 0 }, { 688, 0, -720, 0 }, { -688, 0, 720, 0 }, { 688, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { 798, 0, -4017, 0 }, 995, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { -3296, -64, -2784, 0 }, { { -1760, 0, -480, 0 }, { 1760, 0, -480, 0 }, { -1760, 0, 480, 0 }, { 1760, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1823, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -1760, -64, -4448, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 1792, -64, 3168, 0 }, { { -2208, 0, -480, 0 }, { 2208, 0, -480, 0 }, { -2208, 0, 480, 0 }, { 2208, 0, 480, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 2246, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { -192, -64, 4304, 0 }, { { 480, 0, -1296, 0 }, { 480, 0, 1296, 0 }, { -480, 0, -1296, 0 }, { -480, 0, 1296, 0 } }, { 0, 4117, 0, 0 }, { -4096, 0, 0, 0 }, 1378, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -1752, 0, 3703, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { -3024, -64, 4640, 0 }, { { -560, 0, -912, 0 }, { 560, 0, -912, 0 }, { -560, 0, 912, 0 }, { 560, 0, 912, 0 } }, { 0, 4096, 0, 0 }, { 3973, 0, -996, 0 }, 1063, 2, 4, 255, 130, 0 },
};

GpObj4C D_shelter_b1_underground_parking_8018BD94[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 0, 19, 19, 2, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 5, 10, 1, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4091, 0, 201, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 8000, -64, 32, 0 }, { { -3616, 0, -2928, 0 }, { 3616, 0, -2928, 0 }, { -3616, 0, 2928, 0 }, { 3616, 0, 2928, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, 4096, 0 }, 4636, 2, 7, 255, 3, 0 },
    { NULL, NULL, NULL, { -3424, -64, 3936, 0 }, { { -640, 0, -720, 0 }, { 640, 0, -720, 0 }, { -640, 0, 720, 0 }, { 640, 0, 720, 0 } }, { 0, 4102, 0, 0 }, { 601, 0, -4052, 0 }, 962, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { -3424, -64, -2816, 0 }, { { -1824, 0, -480, 0 }, { 1824, 0, -480, 0 }, { -1824, 0, 480, 0 }, { 1824, 0, 480, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1885, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -1728, -64, -4176, 0 }, { { -480, 0, -1584, 0 }, { 480, 0, -1584, 0 }, { -480, 0, 1584, 0 }, { 480, 0, 1584, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 1654, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 1792, -64, 3232, 0 }, { { -2208, 0, -480, 0 }, { 2208, 0, -480, 0 }, { -2208, 0, 480, 0 }, { 2208, 0, 480, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 2246, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { -208, -64, 4384, 0 }, { { -496, 0, -1344, 0 }, { 496, 0, -1344, 0 }, { -496, 0, 1344, 0 }, { 496, 0, 1344, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 1431, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2276, 0, 3405, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { -2976, -64, 4720, 0 }, { { -576, 0, -896, 0 }, { 576, 0, -896, 0 }, { -576, 0, 896, 0 }, { 576, 0, 896, 0 } }, { 0, 4095, 0, 0 }, { 4052, 0, -602, 0 }, 1063, 2, 4, 255, 130, 0 },
};

GpObj4C D_shelter_b1_underground_parking_8018C124[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 0, 19, 19, 2, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 5, 10, 1, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4017, 0, -799, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 4688, -64, 32, 0 }, { { -816, 0, -624, 0 }, { 816, 0, -624, 0 }, { -816, 0, 624, 0 }, { 816, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1024, 2, 7, 255, 2, 0 },
    { NULL, NULL, NULL, { -3424, -64, 3936, 0 }, { { -640, 0, -720, 0 }, { 640, 0, -720, 0 }, { -640, 0, 720, 0 }, { 640, 0, 720, 0 } }, { 0, 4102, 0, 0 }, { 1380, 0, -3857, 0 }, 962, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { -3280, -64, -2752, 0 }, { { -1744, 0, -480, 0 }, { 1744, 0, -480, 0 }, { -1744, 0, 480, 0 }, { 1744, 0, 480, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 1805, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -1632, -64, -4240, 0 }, { { -464, 0, -1616, 0 }, { 464, 0, -1616, 0 }, { -464, 0, 1616, 0 }, { 464, 0, 1616, 0 } }, { 0, 4107, 0, 0 }, { 4096, 0, 0, 0 }, 1678, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 1776, -64, 3072, 0 }, { { -2176, 0, -480, 0 }, { 2176, 0, -480, 0 }, { -2176, 0, 480, 0 }, { 2176, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 2217, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { -160, -64, 4608, 0 }, { { -464, 0, -1616, 0 }, { 464, 0, -1616, 0 }, { -464, 0, 1616, 0 }, { 464, 0, 1616, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 1678, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3552, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2276, 0, 3405, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { -3040, -64, 4656, 0 }, { { -544, 0, -896, 0 }, { 544, 0, -896, 0 }, { -544, 0, 896, 0 }, { 544, 0, 896, 0 } }, { 0, 4098, 0, 0 }, { 3784, 0, -1568, 0 }, 1047, 2, 4, 255, 130, 0 },
};

GpObj4C D_shelter_b1_underground_parking_8018C4B4[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 0, 19, 19, 2, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 5, 10, 1, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -3973, 0, -995, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { -3472, -64, 3936, 0 }, { { -688, 0, -720, 0 }, { 688, 0, -720, 0 }, { -688, 0, 720, 0 }, { 688, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { 995, 0, -3973, 0 }, 995, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { 4896, -64, 64, 0 }, { { -596, 0, -1135, 0 }, { 588, 0, -1138, 0 }, { -589, 0, 1137, 0 }, { 595, 0, 1134, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1280, 5, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { -3408, -64, -2752, 0 }, { { -1840, 0, -480, 0 }, { 1840, 0, -480, 0 }, { -1840, 0, 480, 0 }, { 1840, 0, 480, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1898, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -1792, -64, -4352, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 1680, -64, 3168, 0 }, { { -2112, 0, -480, 0 }, { 2112, 0, -480, 0 }, { -2112, 0, 480, 0 }, { 2112, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 2157, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { -192, -64, 4432, 0 }, { { 480, 0, -1360, 0 }, { 480, 0, 1360, 0 }, { -480, 0, -1360, 0 }, { -480, 0, 1360, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1436, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3552, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -1752, 0, 3703, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { -2976, -64, 4720, 0 }, { { -608, 0, -928, 0 }, { 608, 0, -928, 0 }, { -608, 0, 928, 0 }, { 608, 0, 928, 0 } }, { 0, 4099, 0, 0 }, { 4051, 0, -601, 0 }, 1108, 2, 4, 255, 130, 0 },
};

GpObj4C D_shelter_b1_underground_parking_8018C844[20] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 0, 19, 19, 2, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 5, 10, 1, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4076, 0, -401, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 112, -64, 1120, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 2442, 5, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { -3936, -64, 4832, 0 }, { { -1024, 0, -1648, 0 }, { 1728, 0, -1648, 0 }, { -1024, 0, 464, 0 }, { 1728, 0, 464, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 2387, 2, 4, 255, 4, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { 96, -64, -1120, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 2455, 5, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { -2064, -64, -48, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1476, 5, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { 2256, -64, -48, 0 }, { { -368, 0, -1392, 0 }, { 368, 0, -1392, 0 }, { -368, 0, 1392, 0 }, { 368, 0, 1392, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1436, 5, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { 3808, -64, -368, 0 }, { { -368, 0, -2176, 0 }, { 368, 0, -2176, 0 }, { -368, 0, 2176, 0 }, { 368, 0, 2176, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 2202, 2, 14, 0, 2, 0 },
    { NULL, NULL, NULL, { -3776, -64, 3776, 0 }, { { -1376, 0, -592, 0 }, { 1376, 0, -592, 0 }, { -1376, 0, 592, 0 }, { 1376, 0, 592, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1492, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { -2880, -64, 4480, 0 }, { { -672, 0, -1328, 0 }, { 672, 0, -1328, 0 }, { -672, 0, 1328, 0 }, { 672, 0, 1328, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 1487, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { -3840, -64, 3696, 0 }, { { -1376, 0, -1216, 0 }, { 1376, 0, -768, 0 }, { -1376, 0, 992, 0 }, { 1376, 0, 992, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1832, 5, 255, 0, 2, 0 },
    { NULL, NULL, NULL, { -2576, -64, 4448, 0 }, { { -1008, 0, -1328, 0 }, { 1008, 0, -1328, 0 }, { -1008, 0, 1328, 0 }, { 1008, 0, 1328, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1664, 5, 255, 0, 2, 0 },
    { NULL, NULL, NULL, { -3937, -64, 4832, 0 }, { { -1024, 0, -1904, 0 }, { 2368, 0, -1904, 0 }, { -1024, 0, 848, 0 }, { 2368, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 3029, 5, 255, 0, 4, 0 },
    { NULL, NULL, NULL, { -1760, -64, -4512, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -3840, -64, -2752, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 2442, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -256, -64, 4384, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1476, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 2112, -64, 3232, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 2455, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -1380, 0, 3856, 0 }, 1247, 2, 1, 255, 130, 0 },
};

GpObj4C D_shelter_b1_underground_parking_8018CE34[16] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 0, 19, 19, 2, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 5, 10, 1, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4052, 0, -601, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 112, -64, 1120, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 2442, 5, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { -3424, -64, 3936, 0 }, { { -640, 0, -720, 0 }, { 640, 0, -720, 0 }, { -640, 0, 720, 0 }, { 640, 0, 720, 0 } }, { 0, 4102, 0, 0 }, { 1751, 0, -3703, 0 }, 962, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { 96, -64, -1120, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 2455, 5, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { -2064, -64, -48, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1476, 5, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { 2256, -64, -48, 0 }, { { -368, 0, -1392, 0 }, { 368, 0, -1392, 0 }, { -368, 0, 1392, 0 }, { 368, 0, 1392, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1436, 5, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { -4096, -64, -2848, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 2442, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 2112, -64, 3168, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 2455, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { -1728, -64, -4192, 0 }, { { -368, 0, -1392, 0 }, { 368, 0, -1392, 0 }, { -368, 0, 1392, 0 }, { 368, 0, 1392, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1436, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -224, -64, 4480, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1476, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 3776, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2106, 0, 3513, 0 }, 1247, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 3744, -64, -192, 0 }, { { -400, 0, -2608, 0 }, { 400, 0, -2608, 0 }, { -400, 0, 2608, 0 }, { 400, 0, 2608, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 2635, 2, 3, 255, 2, 0 },
    { NULL, NULL, NULL, { -3008, -64, 4688, 0 }, { { -512, 0, -960, 0 }, { 512, 0, -960, 0 }, { -512, 0, 960, 0 }, { 512, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { 3856, 0, -1381, 0 }, 1086, 2, 4, 255, 130, 0 },
};

GpObj4C D_shelter_b1_underground_parking_8018D2F4[11] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 0, 19, 19, 2, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, 5, 10, 1, 2, 0 },
    { NULL, NULL, NULL, { 2496, -64, -240, 0 }, { { -608, 0, -848, 0 }, { 608, 0, -848, 0 }, { -608, 0, 848, 0 }, { 608, 0, 848, 0 } }, { 0, 4094, 0, 0 }, { 4076, 0, 401, 0 }, 1039, 2, 22, 255, 2, 0 },
    { NULL, NULL, NULL, { -3808, -64, 3936, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1247, 2, 4, 255, 3, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { -1553, -64, -2208, 0 }, { { -624, 0, -928, 0 }, { 624, 0, -928, 0 }, { -624, 0, 928, 0 }, { 624, 0, 928, 0 } }, { 0, 4120, 0, 0 }, { 4096, 0, 0, 0 }, 1115, 2, 23, 0, 2, 0 },
    { NULL, NULL, NULL, { 3503, -64, -688, 0 }, { { -624, 0, -432, 0 }, { 624, 0, -432, 0 }, { -624, 0, 432, 0 }, { 624, 0, 432, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -4096, 0 }, 757, 5, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { -1632, -64, -4320, 0 }, { { -624, 0, -928, 0 }, { 624, 0, -928, 0 }, { -624, 0, 928, 0 }, { 624, 0, 928, 0 } }, { 0, 4120, 0, 0 }, { 4096, 0, 0, 0 }, 1115, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 2720, -64, 864, 0 }, { { -624, 0, -832, 0 }, { 624, 0, -832, 0 }, { -624, 0, 832, 0 }, { 624, 0, 832, 0 } }, { 0, 4095, 0, 0 }, { -201, 0, -4091, 0 }, 1039, 2, 48, 255, 2, 0 },
    { NULL, NULL, NULL, { 3936, -64, 416, 0 }, { { -1616, 0, -1568, 0 }, { 240, 0, -1568, 0 }, { -1616, 0, -160, 0 }, { 240, 0, -160, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 2246, 5, 1, 0, 4, 0 },
    { NULL, NULL, NULL, { 2848, -64, -288, 0 }, { { -496, 0, -496, 0 }, { 496, 0, -496, 0 }, { -496, 0, 272, 0 }, { 496, 0, 272, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 701, 5, 1, 0, 130, 0 },
};

GpRoomBoundVec D_shelter_b1_underground_parking_8018D638[25] = {
    { 24, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 2256, 2255, 2255, 2255 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

s32 D_shelter_b1_underground_parking_8018D700[3] = {
    0x10000015,
    0x10000017,
    0x10000015,
};

GpRoomParamRec D_shelter_b1_underground_parking_8018D70C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b1_underground_parking_8018D714[1] = {
    { 0, 0, 1, 0, D_shelter_b1_underground_parking_8018D700 },
};

GpRoomParamRec D_shelter_b1_underground_parking_8018D71C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec * D_shelter_b1_underground_parking_8018D724[8] = {
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D714,
    D_shelter_b1_underground_parking_8018D71C,
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D70C,
};

s32 D_shelter_b1_underground_parking_8018D744 = 0;

GpItemMap * D_shelter_b1_underground_parking_8018D748 = NULL;

Task * D_shelter_b1_underground_parking_8018D74C = NULL;

GpFadeWork D_shelter_b1_underground_parking_8018D750 = { 0 };

Task * D_shelter_b1_underground_parking_8018D754 = NULL;

s32 D_shelter_b1_underground_parking_8018D758 = 0;

ShelterB1UndergroundParkingStorageD75C D_shelter_b1_underground_parking_8018D75C = { { 0 }, { 0 } };

RoomDeparture D_shelter_b1_underground_parking_8018D77C = { 0, 0, 0, 0, 0, { 0, 0 }, 0 };

u8 D_shelter_b1_underground_parking_8018D788 = 0;

u8 D_shelter_b1_underground_parking_8018D789 = 0;

u16 D_shelter_b1_underground_parking_8018D78A = 0xCCEE;

u16 D_shelter_b1_underground_parking_8018D78C = 0;

static void       func_shelter_b1_underground_parking_8017E618(UiList* list, UiObject* obj);
static void       func_shelter_b1_underground_parking_8017E914(UiList* list, UiObject* obj);
static void       func_shelter_b1_underground_parking_8017F13C(u8* str, s32 decimals);
static u8*        func_shelter_b1_underground_parking_8017F1AC(u8* buf, s32 value, s32 decimals);
static void       func_shelter_b1_underground_parking_8017F390(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6);
static u16*       func_shelter_b1_underground_parking_8017F80C(s32 mode);
static void       func_shelter_b1_underground_parking_80180308(RoomShopList* shop, UiObject* obj, s32 item);
static void       func_shelter_b1_underground_parking_80180454(RoomShopList* shop, UiObject* obj);
static inline s32 _shelter_b1_underground_parkingAddItemCount(s32 item, s32 count);
static void       func_shelter_b1_underground_parking_801826C0(Task* roomTask);
static void       func_shelter_b1_underground_parking_80183958(RoomRect* rect, u8 r, u8 g, u8 b);
static void       func_shelter_b1_underground_parking_80183B9C(void);
static void       func_shelter_b1_underground_parking_80183CEC(Task* task);

void func_shelter_b1_underground_parking_8017DA50(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_shelter_b1_underground_parking_80186920,
                D_shelter_b1_underground_parking_8018694C,
                D_shelter_b1_underground_parking_80186970,
                D_shelter_b1_underground_parking_801869A0,
                D_shelter_b1_underground_parking_801869D4,
                D_shelter_b1_underground_parking_80186A08,
                D_shelter_b1_underground_parking_80186A40,
                D_shelter_b1_underground_parking_80186A74,
                D_shelter_b1_underground_parking_80186AAC,
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
            Text_DrawString(&req, D_shelter_b1_underground_parking_801868C4);
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
            Text_DrawString(&req, D_shelter_b1_underground_parking_801868F4);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, D_shelter_b1_underground_parking_80186914);
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
            Text_DrawString(&req, D_shelter_b1_underground_parking_801868CC);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, D_shelter_b1_underground_parking_80186914);
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
            Text_DrawString(&req, D_shelter_b1_underground_parking_801868D0);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, D_shelter_b1_underground_parking_80186914);
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
            Text_DrawString(&req, D_shelter_b1_underground_parking_801868D8);
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
            Text_Strcat(p, D_shelter_b1_underground_parking_8018691C);
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
            Text_DrawString(&req, D_shelter_b1_underground_parking_801868E4);
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
            Text_Strcat(p, D_shelter_b1_underground_parking_8018691C);
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
            Text_DrawString(&req, D_shelter_b1_underground_parking_801868FC);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, D_shelter_b1_underground_parking_80186914);
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
            Text_DrawString(&req, D_shelter_b1_underground_parking_80186904);
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
            Text_DrawString(&req, D_shelter_b1_underground_parking_8018690C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data menu.
static const char D_shelter_b1_underground_parking_8017D610[] = "Play Data";

/// Drawn in place of a usage percentage that reached 100.
static const u8 D_shelter_b1_underground_parking_8017D61C[] = "100.0%";

void func_shelter_b1_underground_parking_8017E21C(UiList* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_shelter_b1_underground_parking_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_shelter_b1_underground_parking_8018691C);
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

/// Builds the "Play Data" item-usage panel's three parallel arrays from the
/// save's per-item use counters (`Mc_SaveData[0].state.weaponUseCounts`, ids 0x80-0x9F).
///
/// Every id whose name is non-empty (a leading 0 or 0xA marks an unused row)
/// and whose counter is non-zero is marked seen and appended to `itemIds`,
/// while the counters are summed. The ids are then insertion-sorted by use
/// count, most-used first. Finally each row gets `percents` - its share of all
/// recorded uses in hundredths of a percent, rounded - and `barWidths`, its
/// counter as a 12-bit fraction of the top row's. Both are scaled down by
/// halving until the top counter fits in 17 bits, so the multiply and the
/// shift cannot overflow.
static void func_shelter_b1_underground_parking_8017E618(UiList* list, UiObject* obj)
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
static void func_shelter_b1_underground_parking_8017E914(UiList* list, UiObject* obj)
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

/// Titles of the weapon and PE usage panels.
static const char D_shelter_b1_underground_parking_8017D624[] = "Weapon Data";
static const char D_shelter_b1_underground_parking_8017D630[] = "PE Data";

void func_shelter_b1_underground_parking_8017EC34(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &D_shelter_b1_underground_parking_80186B10;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_shelter_b1_underground_parking_8017D624);
    } else {
        Ui_DrawText(&(obj)->panel, D_shelter_b1_underground_parking_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B34, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            func_shelter_b1_underground_parking_8017E618(list, obj);
        } else {
            func_shelter_b1_underground_parking_8017E914(list, obj);
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

/// Title of the menu below. The two bytes after its terminator are not zero,
/// so it stays assembly.
/// "Telephone", followed by the non-zero padding the original toolchain left.
static const char D_shelter_b1_underground_parking_8017D638[12] = "Telephone\0<\x9E";

void func_shelter_b1_underground_parking_8017EDE8(Task* task)
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
    list          = &D_shelter_b1_underground_parking_80186B98;
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
        /* The literal carries its trailing "\0\1" - the room's rodata has
         * those two bytes right after the string and nothing else claims them. */
        Ui_DrawText(&(obj)->panel, D_shelter_b1_underground_parking_8017D638);
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

void func_shelter_b1_underground_parking_8017F0E0(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = func_shelter_b1_underground_parking_8017F7D0;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
static void func_shelter_b1_underground_parking_8017F13C(u8* str, s32 decimals)
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
static u8* func_shelter_b1_underground_parking_8017F1AC(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_shelter_b1_underground_parking_8018691C);
    return buf;
}

void func_shelter_b1_underground_parking_8017F2A0(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b1_underground_parking_80186AE8;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_shelter_b1_underground_parking_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B34, 0, 0, 1, obj);
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
static void func_shelter_b1_underground_parking_8017F390(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    POLY_G4* prim;
    s16      x;
    s32      y;
    s16      bottom;

    if ((arg5 != 0) && (arg3 >= 2)) {
        prim           = (POLY_G4*)gGpuPrimCursor;
        x              = arg0->field_20.u + arg1 + 1;
        prim->x2       = x;
        prim->x0       = x;
        y              = arg0->field_22.u;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 8);
        PRIM_COLOR_WORD(prim, 0) = arg5;
        setcode(prim, 0x38);
        PRIM_COLOR_WORD(prim, 2) = arg5;
        PRIM_COLOR_WORD(prim, 3) = arg6;
        PRIM_COLOR_WORD(prim, 1) = arg6;
        y                       += arg2;
        y++;
        x        = prim->x0 + arg3 - 1;
        prim->y1 = y;
        prim->y0 = y;
        prim->x3 = x;
        prim->x1 = x;
        bottom   = y + arg4 - 1;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14.u + 1, prim);
    }
}

void func_shelter_b1_underground_parking_8017F494(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_8018689C, prompt->field_1C, 1, 0);
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

void func_shelter_b1_underground_parking_8017F578(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_801868A4, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B50, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_shelter_b1_underground_parking_8017F640(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_801868B0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B6C, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

void func_shelter_b1_underground_parking_8017F708(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_801868BC, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B6C, 1, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
static void func_shelter_b1_underground_parking_8017F7D0(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
/// `Mc_SaveData[0].state.gameMode` picks one of two sets of lists (below 2, or 2 and
/// up); within a set the low half of `mode` picks the list group (0x20, 0x21,
/// 0x30-0x33 and 0x40 each have one, every other value shares one) and the
/// high half the entry within it (0-3). A high half outside 0-3 gets a single
/// fallback list.
static u16* func_shelter_b1_underground_parking_8017F80C(s32 mode)
{
    if (Mc_SaveData[0].state.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186C9C;
                    case 1:
                        return D_shelter_b1_underground_parking_80186CB4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186CC8;
                    case 3:
                        return D_shelter_b1_underground_parking_80186CD0;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186CE4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186D00;
                    case 2:
                        return D_shelter_b1_underground_parking_80186D10;
                    case 3:
                        return D_shelter_b1_underground_parking_80186D1C;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186D34;
                    case 1:
                        return D_shelter_b1_underground_parking_80186D50;
                    case 2:
                        return D_shelter_b1_underground_parking_80186D64;
                    case 3:
                        return D_shelter_b1_underground_parking_80186D6C;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186D80;
                    case 1:
                        return D_shelter_b1_underground_parking_80186DA0;
                    case 2:
                        return D_shelter_b1_underground_parking_80186DB0;
                    case 3:
                        return D_shelter_b1_underground_parking_80186DBC;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186BE4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186BF4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186C04;
                    case 3:
                        return D_shelter_b1_underground_parking_80186C0C;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186C58;
                    case 1:
                        return D_shelter_b1_underground_parking_80186C70;
                    case 2:
                        return D_shelter_b1_underground_parking_80186C84;
                    case 3:
                        return D_shelter_b1_underground_parking_80186C8C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186C1C;
                    case 1:
                        return D_shelter_b1_underground_parking_80186C2C;
                    case 2:
                        return D_shelter_b1_underground_parking_80186C3C;
                    case 3:
                        return D_shelter_b1_underground_parking_80186C44;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186BBC;
                    case 1:
                        return D_shelter_b1_underground_parking_80186BC4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186BCC;
                    case 3:
                        return D_shelter_b1_underground_parking_80186BD4;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E68;
                    case 1:
                        return D_shelter_b1_underground_parking_80186E74;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E7C;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E88;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E94;
                    case 1:
                        return D_shelter_b1_underground_parking_80186EA0;
                    case 2:
                        return D_shelter_b1_underground_parking_80186EA4;
                    case 3:
                        return D_shelter_b1_underground_parking_80186EB0;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186EBC;
                    case 1:
                        return D_shelter_b1_underground_parking_80186EC8;
                    case 2:
                        return D_shelter_b1_underground_parking_80186ED0;
                    case 3:
                        return D_shelter_b1_underground_parking_80186EDC;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186EE8;
                    case 1:
                        return D_shelter_b1_underground_parking_80186EF4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186EFC;
                    case 3:
                        return D_shelter_b1_underground_parking_80186F08;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186DF4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186DFC;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E04;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E0C;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E40;
                    case 1:
                        return D_shelter_b1_underground_parking_80186E4C;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E54;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E5C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E18;
                    case 1:
                        return D_shelter_b1_underground_parking_80186E20;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E2C;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E34;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186DD4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186DD8;
                    case 2:
                        return D_shelter_b1_underground_parking_80186DDC;
                    case 3:
                        return D_shelter_b1_underground_parking_80186DE4;
                }
                break;
        }
    }
    return D_shelter_b1_underground_parking_80187098;
}

void func_shelter_b1_underground_parking_8017FE7C(UiList* prompt, UiObject* obj)
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
        D_shelter_b1_underground_parking_80186FB0 = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->panel.field_0.w;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam(D_shelter_b1_underground_parking_80186FE4, 0, 0);
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
        Text_DrawString(&req, D_shelter_b1_underground_parking_80186FD0);
        if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_8018719C, 0, 1, 1, obj);
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
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_80186FE0, prompt->field_1C, 1, 0);
        if (prompt->field_C == 1 && blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            child = Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187148, itemId, 1, 1, obj);
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
            child2 = Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187148, itemId, 1, 1, obj);
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
static void func_shelter_b1_underground_parking_80180308(RoomShopList* shop, UiObject* obj, s32 item)
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
/// `Gp_ItemSortKey` and caps the visible row count at 9.
///
/// The upper halfword of the owning task's `spawnArg1` is the shop's mode. It
/// picks the fixed id list and which items of each unlocked price row are
/// stocked: mode 0 takes 0x80-0x9F plus a handful of other ids, mode 1
/// 0xA0-0xBF, mode 2 0x60-0x7F and 0xD, and mode 3 everything from 1 to 0x5F
/// the other modes do not take, plus the twelve two-bit stock levels of
/// `Mc_SaveData[0].state.shopStock`. A demo save unlocks every row and every level.
static void func_shelter_b1_underground_parking_80180454(RoomShopList* shop, UiObject* obj)
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
    ids  = func_shelter_b1_underground_parking_8017F80C(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        func_shelter_b1_underground_parking_80180308(shop, obj, *ids);
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
                        item = D_shelter_b1_underground_parking_80186F14[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    func_shelter_b1_underground_parking_80180308(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    func_shelter_b1_underground_parking_80180308(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    func_shelter_b1_underground_parking_80180308(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    func_shelter_b1_underground_parking_80180308(shop, obj, item);
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
                    func_shelter_b1_underground_parking_80180308(shop, obj, slot * 3 + (id = level + 0xE));
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
    D_shelter_b1_underground_parking_80186FB0 = -1;
}

/// Title of the shop list.
static const u8 D_shelter_b1_underground_parking_8017D750[] = "Select";

/// Label drawn ahead of the BP balance and the purchase total.
static const u8 D_shelter_b1_underground_parking_8017D758[] = "BP";

/// The shop's "Select" panel. On its first frame it allocates the
/// `RoomShopList` work block, fills it through
/// `func_shelter_b1_underground_parking_80180454` and opens the panel
/// `D_shelter_b1_underground_parking_801871B8` beside it. Every frame it draws the list and the
/// "BP" caption; menu reports -1 and cancel 6 to the parent. A child that
/// reports 6 is torn down and the list takes input again; one that reports -1
/// passes it up.
void func_shelter_b1_underground_parking_80180820(Task* task)
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
    Ui_DrawText(&(obj)->panel, (char*)D_shelter_b1_underground_parking_8017D750);
    if (task->state == 0) {
        mem = memCalloc(sizeof(RoomShopList), 0);
        if (mem != NULL) {
            shop               = mem;
            task->work         = (TaskIdMap*)shop;
            shop->list.funcs   = D_shelter_b1_underground_parking_8018709C;
            shop->list.field_6 = 0;
            shop->list.field_7 = 0xF;
            func_shelter_b1_underground_parking_80180454(shop, obj);
            Ui_LayoutListPanel(&shop->list, &(obj)->panel);
            shop->list.field_A = 1;
            Ui_SetListScrollFlag(&shop->list, 1);
            obj->panel.bounds.unsignedRect.h += 8;
            shop->list.field_17               = 8;
            Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_801871B8, 0, 0, 0, obj);
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
    Text_DrawString(&req, D_shelter_b1_underground_parking_8017D758);

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

void func_shelter_b1_underground_parking_80180A70(UiList* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_80186FC8, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = D_shelter_b1_underground_parking_80187020;
    obj->owner->spawnArg1.value = (u16)obj->owner->spawnArg1.value;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = D_shelter_b1_underground_parking_80187028;
            obj->owner->spawnArg1.value |= 0x10000;
            break;
        case 2:
            text                   = D_shelter_b1_underground_parking_80187034;
            obj->owner->spawnArg1.value |= 0x20000;
            break;
        case 3:
            text                   = D_shelter_b1_underground_parking_8018703C;
            obj->owner->spawnArg1.value |= 0x30000;
            break;
    }

    if (*func_shelter_b1_underground_parking_8017F80C(obj->owner->spawnArg1.value) == 0xFFFF) {
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
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187110, obj->owner->spawnArg1, 1, 1, obj);
        obj->panel.field_0.w = 0;
    }
}

/// Title of the menu below.
static const u8 D_shelter_b1_underground_parking_8017D75C[] = "List";

void func_shelter_b1_underground_parking_80180C90(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2.pointer;
    list          = &D_shelter_b1_underground_parking_801870A4;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, (char*)D_shelter_b1_underground_parking_8017D75C);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_8018712C, task->spawnArg1, 0, 1, obj);
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

/// Label of the balance panel's item-count line.
static const u8 D_shelter_b1_underground_parking_8017D764[] = "TOTAL";

void func_shelter_b1_underground_parking_80180E38(Task* task)
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
    Text_DrawString(&req0, D_shelter_b1_underground_parking_8017D758);

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
    Text_DrawString(&req1, (char*)D_shelter_b1_underground_parking_8017D764);

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

void func_shelter_b1_underground_parking_8018101C(UiList* prompt, UiObject* obj)
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
    Text_DrawString(&req, D_shelter_b1_underground_parking_80186FB4);

    mode = prompt->field_C;
    if (mode == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        cfg   = &Player_Status;
        price = Gp_ItemDescs[itemId].price;
        scan  = &Mc_SaveData[0].state.carriedItems;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187164, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187164, 1, 1, 1, obj);
                }
                obj->panel.field_0.w = 0;
            } else if ((obj->owner->parent->spawnArg1.value >> 16) == mode) {
                child = Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_801871D4, itemId, 1, 1, obj);
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
            Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187164, 0, 1, 1, obj);
            obj->panel.field_0.w = 0;
        }
    }
}

/// Title of the notice panel.
static const u8 D_shelter_b1_underground_parking_8017D76C[] = "Notice";

void func_shelter_b1_underground_parking_80181230(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1.value;
    obj  = task->spawnArg2.pointer;
    switch (kind) {
        case 1:
            text = D_shelter_b1_underground_parking_80187058;
            break;
        case 2:
            text = D_shelter_b1_underground_parking_80187068;
            break;
        default:
            text = D_shelter_b1_underground_parking_80187044;
            break;
    }

    Ui_DrawText(&(obj)->panel, (char*)D_shelter_b1_underground_parking_8017D76C);
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

/// Title of the charge panel; the byte after its terminator is not zero, so
/// it stays assembly.
/// "Charge", followed by the non-zero padding the original toolchain left.
static const char D_shelter_b1_underground_parking_8017D774[8] = "Charge\0o";

/// The charge station's transfer panel: steps through the mapped item slots,
/// takes the slot's current level as the bar's starting value and its related
/// quantity as the target, then animates the bar up to it over 0xBC frames.
/// Confirm or cancel (or the timer running out) advances to the next slot;
/// running out of slots reports code 6 to the parent.
void func_shelter_b1_underground_parking_801813B0(Task* task)
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
    Ui_DrawText(&(obj)->panel, (char*)D_shelter_b1_underground_parking_8017D774);

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
            map                                       = Gp_GetItemMap(slotId);
            D_shelter_b1_underground_parking_8018D748 = map;
            itemId                                    = map->field_1;
            slot                                      = Gp_GetItemSlot(itemId);
            if (D_shelter_b1_underground_parking_8018D748->field_0 == 0) {
                D_shelter_b1_underground_parking_8018D744 = slot->ammoQty;
                slot->ammoQty                             = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_shelter_b1_underground_parking_8018D744 = slot->attachQty;
                slot->attachQty                           = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown                         = 0xBC;
            D_shelter_b1_underground_parking_8018D744 <<= 8;
            task->state                                 = task->state + 1;
        }
    }

    curItem = D_shelter_b1_underground_parking_8018D748->field_1;
    relItem = D_shelter_b1_underground_parking_8018D748->field_2;
    if (D_shelter_b1_underground_parking_8018D748->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                                      <<= 8;
    D_shelter_b1_underground_parking_8018D744 += 0x40;
    if (qty < D_shelter_b1_underground_parking_8018D744) {
        D_shelter_b1_underground_parking_8018D744 = qty;
    }

    y = (s16)obj->panel.field_18.u;
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar(&(obj)->panel, (s16)obj->panel.field_1C.s, (s16)obj->panel.field_1E.u, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->panel.field_1C.s + 2, y + 0x23, D_shelter_b1_underground_parking_8018D744 >> 8, 0x606060);
    h = (s16)obj->panel.field_1A.u;
    func_800C0E20(&(obj)->panel, (s16)obj->panel.field_1C.s + 2, (s16)obj->panel.field_1E.u - 2, h - 6, qty,
                  D_shelter_b1_underground_parking_8018D744, 0x1741F);

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

static inline s32 _shelter_b1_underground_parkingAddItemCount(s32 item, s32 count)
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
void func_shelter_b1_underground_parking_80181678(Task* task)
{
    u8          buf[0x10];
    TextDrawReq req;
    UiObject*   obj;
    s32         item;
    s32         y;
    s32         ry;
    s32         count;

    item         = D_shelter_b1_underground_parking_80186FB0;
    obj          = task->spawnArg2.pointer;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && D_shelter_b1_underground_parking_80186FB0 == Gp_GetPreviewItem()) {
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
        Text_DrawString(&req, D_shelter_b1_underground_parking_80187088);
        count = 0;
        count = _shelter_b1_underground_parkingAddItemCount(item, count);
        Text_DrawPrompt(obj, (s16)obj->panel.field_1E.u - 2, y + 0xA, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
    }
}

void func_shelter_b1_underground_parking_8018184C(Task* task)
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
    Text_DrawPrompt(obj, left + 0x98, y, D_shelter_b1_underground_parking_80187094, 0x606060, 3, 2);
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
    Text_DrawString(&req, D_shelter_b1_underground_parking_8017D758);

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

void func_shelter_b1_underground_parking_80181CCC(UiList* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->panel.field_20.u + (u16)prompt->field_18;
    req.y          = obj->panel.field_22.u + (u16)prompt->field_1A;
    req.otIndex    = obj->panel.field_14.s + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    Text_DrawString(&req, D_shelter_b1_underground_parking_80186FC8);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

void func_shelter_b1_underground_parking_80181D88(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_shelter_b1_underground_parking_801870D0;
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

/// Runs a modal panel over the room: takes the prim buffer and stops the frame
/// timer while the panel is up, waits for the panel to report closed, then
/// gives both back and ends the stage.
void func_shelter_b1_underground_parking_80181EB0(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_801870F4, task->spawnArg1, 1, 1, NULL);
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

/// The departure task, carrying out `D_shelter_b1_underground_parking_8018D77C`.
/// State 0 sends the departure's halfword to the slot-3 game pointer as message
/// 0x3EE, or skips to state 2 when it is 0xFFFF; state 1 polls the pointer with
/// 0x3F0 until it answers 0. States 2 and 3 play the departure's sound event and
/// wait for the voice to go quiet (0 skips both). State 4 commits the save
/// location the departure names, re-spawns the player task as type 0x11 and
/// kills itself.
void func_shelter_b1_underground_parking_80181FE4(Task* arg0)
{
    GpXformArg msg;
    void*      slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            /* Read unsigned, though the staging code stores it signed. */
            msg.rot.vy = (u16)D_shelter_b1_underground_parking_8018D77C.facing;
            if (msg.rot.vy == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsgPtr(slot, 0x3EE, &msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_shelter_b1_underground_parking_8018D77C.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_shelter_b1_underground_parking_8018D77C.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_shelter_b1_underground_parking_8018D77C.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            gDisplayState.roomVariant    = 1;
            Mc_SaveData[0].state.at4.loc.stage = D_shelter_b1_underground_parking_8018D77C.stage;
            Mc_SaveData[0].state.at4.loc.area  = D_shelter_b1_underground_parking_8018D77C.area;
            Mc_SaveData[0].state.at4.loc.warp  = D_shelter_b1_underground_parking_8018D77C.warp;
            Mc_SaveData[0].state.at4.loc.room  = D_shelter_b1_underground_parking_8018D77C.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// The room's cutscene runner: suppresses the player and ally HUD, loads and
/// starts the scene's caption slot, lets confirm or cancel cut the sub-task
/// short, applies the story-flag side effects when the scene ends, and
/// restores everything before killing itself.
void func_shelter_b1_underground_parking_80182154(Task* task)
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
            D_shelter_b1_underground_parking_8018D754 = NULL;
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
            D_shelter_b1_underground_parking_8018D754 = Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_shelter_b1_underground_parking_8018D754);
                task->state++;
            } else if (Task_PollKill(D_shelter_b1_underground_parking_8018D754, &killOut) != 0) {
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

/// Starts caption slot 0xA and spawns entry 4 of
/// `D_shelter_b1_underground_parking_8018726C` when the player asks for it.
///
/// The player must not be aiming (`field_954 != 2`), captions must be idle,
/// the session room must be 7 or later, and the model root must stand with X
/// below -0x1266 and Z inside [-0x7CF, 0x7D0), with `Gp_StateC08.field_A != 1` and
/// `gDisplayState.pendingMode` clear. Then the 0x1000 pad mask with the yaw in the 0x3FF-wide
/// window opening at 0xA01, or the 0x4000 mask with it in the window at 0x201,
/// takes the weapon away and runs the handoff.
static void func_shelter_b1_underground_parking_801826C0(Task* roomTask)
{
    Task*      task;
    GameActor* actor;
    GpCoord*   coord;
    s32        z;
    s32        facing;

    task  = gameGetPtrSlot(3);
    actor = (GameActor*)task->work;
    coord = task->extra.tmd->coords;
    if ((actor->field_954 != 2) && (Gp_CapBusy() == 0) && (gGameSession->at4.loc.room >= 7) &&
        (coord->coord.t[0] < -0x1266)) {
        z = coord->coord.t[2];
        if (z < 0x7D0) {
            if ((z >= -0x7CF) && (Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == 0)) {
                facing = (u16)actor->field_52 & 0xFFF;
                if (Pad_CheckButtons(0, 0, 0x1000) != 0) {
                    if ((u32)(facing - 0xA01) < 0x3FFU) {
                        Gp_MsgPlayerWeapon(0);
                        Gp_StartCapSlot(0xA, 0, 1);
                        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                    }
                }
                if ((Pad_CheckButtons(0, 0, 0x4000) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                    Gp_MsgPlayerWeapon(0);
                    Gp_StartCapSlot(0xA, 0, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                }
            }
        }
    }
}

/// The three states of the room's main task, run by
/// `func_shelter_b1_underground_parking_801838B4`: set-up, the per-frame
/// handler, and the kill.
static const TaskFuncTable3 D_shelter_b1_underground_parking_8017D7F4 = {
    {
        func_shelter_b1_underground_parking_80183810,
        func_shelter_b1_underground_parking_801826C0,
        taskKill,
    },
};

/// Room event handler keyed on `msg->field_2`: 1 calls `func_80131E38` in
/// place 0x15, 0xA starts caption slot 0xA and sets nibble 0x1B4 to 2 while
/// the room is below 7, and 0xB / 0xC pick a caption or spawn per room.
s32 func_shelter_b1_underground_parking_80182830(Task* task, s32 msgId, RoomEventMsg * msg, GpMessageArg arg3)
{
    if (msg->field_2 == 1 && gGameSession->at4.loc.place == 0x15) {
        func_80131E38();
    }
    if (msg->field_2 == 0xA) {
        if ((u8)msg->field_3 == 1 && gGameSession->at4.loc.room < 7) {
            Gp_StartCapSlot(0xA, 1, 0);
            GameFlag_SetNibble(0x1B4, 2);
        }
    }
    if (msg->field_2 == 0xB) {
        switch (gGameSession->at4.loc.room) {
            case 2:
                Gp_RunCapCmd1(8);
                break;
            case 3:
                Gp_RunCapCmd1(0xF);
                break;
            case 4:
                Gp_RunCapCmd1(9);
                break;
            case 5:
                Gp_MsgPlayerWeapon(0);
                Gp_RunCapCmd1(0xC);
                Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 1, 0, 0);
                break;
            case 6:
            case 7:
            case 8:
                Gp_RunCapCmd1(0xE);
                break;
        }
    }
    if (msg->field_2 == 0xC) {
        switch (gGameSession->at4.loc.room) {
            case 6:
                Gp_StartCapSlot(0xB, 1, 0);
                break;
            case 7:
                if (D_shelter_b1_underground_parking_8018D758 != 0) {
                    Gp_RunCapCmd1(0x1E);
                } else if (GameFlag_GetNibble(0x7A) < 6) {
                    Gp_MsgPlayerWeapon(0);
                    Gp_StartCapSlot(0xB, 1, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 3, 0, 0);
                } else {
                    Gp_StartCapSlot(0xB, 1, 2);
                }
                break;
            case 8:
                Gp_StartCapSlot(0xB, 1, 2);
                break;
        }
    }
    return 0;
}

s32 func_shelter_b1_underground_parking_80182A60(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    RoomCutsceneRec* st;

    switch (arg2) {
        case 1:
            if (gGameSession->at4.loc.room < 6) {
                if (GameFlag_GetNibble(0xC7) == 0) {
                    Gp_RunCapCmd1(1);
                } else if (GameFlag_GetNibble(0xE7) == 0) {
                    if (GameFlag_GetNibble(0xE8) == 0) {
                        Gp_RunCapCmd1(2);
                    } else {
                        Gp_MsgPlayerWeapon(0);
                        Gp_MsgPlayer3F3(0);
                        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                    }
                } else {
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgPlayer3F3(0);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                }
            } else {
                Gp_RunCapCmd1(0x10);
            }
            break;
        case 7:
            switch (gGameSession->at4.loc.room) {
                case 1:
                    Gp_RunCapCmd1(7);
                    break;
                case 2:
                    Gp_RunCapCmd1(8);
                    break;
                case 3:
                    Gp_RunCapCmd1(0xF);
                    break;
                case 4:
                    Gp_RunCapCmd1(9);
                    break;
                case 6:
                case 7:
                case 8:
                    Gp_RunCapCmd1(0xE);
                    break;
            }
            break;
        case 4:
            switch (gGameSession->at4.loc.room) {
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                    Gp_RunCapCmd1(4);
                    break;
                case 6:
                    Gp_RunCapCmd1(5);
                    break;
                case 7:
                    Gp_RunCapCmd1(6);
                    break;
                case 8:
                    Gp_RunCapCmd1(0x10);
                    break;
            }
            break;
        case 2:
        case 3:
        case 5:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, arg2, 0);
            break;
        case 13:
            st           = &D_shelter_b1_underground_parking_8018D75C.value;
            st->field_4  = 0x5414000B;
            st->field_8  = 0x5414000E;
            st->field_10 = 0x5414000C;
            st->field_C  = 0x5414000D;
            st->field_0  = 0x14;
            if (D_shelter_b1_underground_parking_8018D758 == 0) {
                if (GameFlag_GetNibble(0x158) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    GameFlag_SetNibble(0x158, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 1, 0);
                } else {
                    st->field_1 = 1;
                    st->field_3 = 1;
                    st->field_2 = 0;
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 0, 9, st);
                }
            } else {
                D_shelter_b1_underground_parking_8018D758 = 0;
                st->field_1                               = 0x1F;
                st->field_3                               = 0;
                st->field_2                               = 1;
                Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 0, arg2, st);
            }
            break;
        case 22:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_80187260, 0, arg2, 0);
            break;
        case 48:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 4, 0);
            break;
    }
    return 0;
}

/// Task body that waits for the caption to finish, then on caption key 0xB
/// spawns the 0x31 task and after 30 frames publishes
/// `D_shelter_b1_underground_parking_8018D77C` and spawns entry 0 of
/// `D_shelter_b1_underground_parking_80187200`. Any other key restores the
/// weapon and ends the task.
void func_shelter_b1_underground_parking_80182DB4(Task* task)
{
    RoomDeparture  rec;
    RoomEventMsg   msg;
    RoomDeparture* p;
    s32            (*handler)(RoomEventMsg*, RoomEventMsg*);

    switch (task->state) {
        case 0:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 0xB) {
                D_shelter_b1_underground_parking_8018D750.field_0 = 0;
                D_shelter_b1_underground_parking_8018D750.field_1 = 0;
                D_shelter_b1_underground_parking_8018D750.field_2 = 0x1E;
                Task_Spawn(1, 0x31, 0, &D_shelter_b1_underground_parking_8018D750);
                task->killCountdown = 0x1E;
                task->state++;
            } else {
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
        case 2:
            if (task->killCountdown == 0) {
                if (GameFlag_GetNibble(0x4B) == 0xA) {
                    GameFlag_SetNibble(0x4B, 9);
                }
                if (GameFlag_GetNibble(0x11F) == 1) {
                    GameFlag_SetNibble(0x11F, 2);
                    Mc_SaveData[0].state.sceneEvent = 0x1B;
                }
                handler      = func_shelter_b1_underground_parking_80183124;
                rec.stage    = 5;
                rec.area     = 1;
                rec.room     = 1;
                rec.warp     = 1;
                rec.sndEvent = 0x54140008;
                rec.facing   = -1;
                Gp_MsgPlayerWeapon(0);
                p           = &rec;
                msg.prefix.packed   = p->area;
                msg.field_2 = p->warp;
                msg.field_3 = p->room;
                msg.field_5 = 0;
                handler(&msg, &msg);
                p->area                                   = msg.prefix.packed;
                p->warp                                   = msg.field_2;
                p->room                                   = msg.field_3;
                D_shelter_b1_underground_parking_8018D77C = rec;
                Task_SpawnFromTable(&D_shelter_b1_underground_parking_80187200, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

/// Keeps the room's looping ambience in step with the area the session is in:
/// `gGameSession->at4.loc.view` selects an entry of the ambience table, and
/// state 0 starts the loop with `SndEvt_EnqueueType6`. Once
/// `D_shelter_b1_underground_parking_8018D758` is clear, state 1 queues a
/// `SndEvt_EnqueueType7` event for the loop and ends the task; otherwise it waits for the session's view to stop matching `Mc_SaveData[0].state.at4.loc.view`,
/// states 2 to 4 walk the task along, and state 5 retunes the loop to the new
/// entry with `SndEvt_EnqueueTypeA` and returns to state 1.
void func_shelter_b1_underground_parking_80182FC8(Task* task)
{
    s32 pan;
    s32 vol;
    u8  idx;

    idx = gGameSession->at4.loc.view;
    if (idx < 9) {
        pan = D_shelter_b1_underground_parking_8018761C[idx].pan;
        vol = D_shelter_b1_underground_parking_8018761C[idx].vol / 2;
    } else {
        pan = 0;
        vol = 0;
    }

    switch (task->state) {
        case 0:
            SndEvt_EnqueueType6(0x5414000F, (s8)pan, (s8)vol);
            task->state = task->state + 1;
            break;
        case 1:
            if (D_shelter_b1_underground_parking_8018D758 == 0) {
                func_shelter_b1_underground_parking_80186890(0);
                SndEvt_EnqueueType7(0x5414000F, 1);
                taskKill(task);
                break;
            }
            if (Mc_SaveData[0].state.at4.loc.view != gGameSession->at4.loc.view) {
                task->state = task->state + 1;
            }
            break;
        case 2:
        case 3:
        case 4:
            task->state = task->state + 1;
            break;
        case 5:
            SndEvt_EnqueueTypeA(0x5414000F, (s8)pan, (s8)vol);
            task->state = 1;
            break;
    }
}

/// Answers the marker query for one Neo Ark map room; the Neo Ark area map
/// carries the same body. Most rooms have no
/// marker; the five that do read a GameFlag nibble, either straight (plus one,
/// rooms 7 / 13 / 32) or folded into a fixed set of states (rooms 20 and 21).
static s32 func_shelter_b1_underground_parking_80183124(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_5 == 0) {
        switch (in->prefix.packed) {
            case 7:
                out->field_3 = GameFlag_GetNibble(0xE1) + 1;
                break;
            case 13:
                out->field_3 = GameFlag_GetNibble(0xD9) + 1;
                break;
            case 20:
                out->field_3 = 1;
                if (GameFlag_GetNibble(0xDD) != 0) {
                    if (GameFlag_GetNibble(0xDC) != 0) {
                        out->field_3 = 3;
                    } else {
                        out->field_3 = 2;
                    }
                }
                break;
            case 21:
                if (GameFlag_GetNibble(0xE9) != 0) {
                    out->field_3 = 4;
                } else {
                    out->field_3 = 1;
                }
                break;
            case 32:
                out->field_3 = GameFlag_GetNibble(0xDD) + 1;
                break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            default:
                break;
        }
    }
    return 1;
}

/// The examine task's eight states, run by
/// `func_shelter_b1_underground_parking_80184284`, from set-up to the closing
/// fade.
static const TaskFuncTable8 D_shelter_b1_underground_parking_8017D9A4 = {
    {
        func_shelter_b1_underground_parking_80184304,
        func_shelter_b1_underground_parking_801843F0,
        func_shelter_b1_underground_parking_80184468,
        func_shelter_b1_underground_parking_80184594,
        func_shelter_b1_underground_parking_801845F8,
        func_shelter_b1_underground_parking_801846EC,
        func_shelter_b1_underground_parking_80184778,
        func_shelter_b1_underground_parking_801847D0,
    },
};

/// A one-shot sound cue on a timer: fires its spawn argument as a sound event
/// on the first frame, repeats it at frame 0x50 and kills itself at 0x78.
void func_shelter_b1_underground_parking_801831F4(Task* task)
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

s32 func_shelter_b1_underground_parking_80183284(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x121 || arg2 == 0x122) {
        if (gGameSession->at4.loc.room == 6) {
            node = Gp_PendingObj4C;
            while (node != NULL) {
                if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                    found = 1;
                    goto check;
                }
                node = node->next;
            }
            found = 0;
        check:
            if (found != 0) {
                Task_SpawnOnDefaultList(D_shelter_b1_underground_parking_8018726C, 2, 0, 0);
                gGameSession->hideHud    = 1;
                gGameSession->eventState = 1;
                return 1;
            }
        }
    }
    return 0;
}

s32 func_shelter_b1_underground_parking_80183360(Task* arg0, s32 arg1, RoomEventMsg * in, RoomEventMsg * out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (D_shelter_b1_underground_parking_8018D758 == 0) {
        return 1;
    }
    if (in->field_5 == 0) {
        Gp_RunCapCmd1(0x1E);
    }
    return 2;
}

s32 func_shelter_b1_underground_parking_801833DC(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x54140010, 0, 0);
    }
    return 0;
}

void func_shelter_b1_underground_parking_80183410(Task* task)
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

/// Spawns entry 0 of `D_shelter_b1_underground_parking_80187670` and kills
/// itself once that task has gone.
void func_shelter_b1_underground_parking_801834D4(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            D_shelter_b1_underground_parking_8018D74C = Task_SpawnFromTable(&D_shelter_b1_underground_parking_80187670, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_shelter_b1_underground_parking_8018D74C, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

void func_shelter_b1_underground_parking_80183560(Task* arg0)
{
    s32 state = arg0->state;

    switch (state) {
        case 0:
            if (Gp_CapBusy() == 0) {
                arg0->state += 1;
            }
            return;
        case 1:
            if (Gp_GetCapEventKey() == 0xB) {
                gGameSession->at4.loc.room  = 6;
                Mc_SaveData[0].state.at4.loc.room = 6;
                gGameSession->roomObjsDirty = state;
                func_800E8614(D_shelter_b1_underground_parking_801872D8, 1);
                GameFlag_SetNibble(0xF4, 1);
                Gp_SetItemSeenBit(0x123, 1);
            } else {
                Gp_MsgPlayerWeapon(1);
            }
            taskKill(arg0);
            break;
    }
}

void func_shelter_b1_underground_parking_8018363C(Task* arg0)
{
    if (arg0->state == 0) {
        SetDispMask(0);
        D_80115768            = 1;
        gGameSession->hideHud = 1;
        func_800E8634(D_shelter_b1_underground_parking_801873DC, 0, D_shelter_b1_underground_parking_80187544);
        GameFlag_SetNibble(0xF4, 2);
        GameFlag_SetNibble(0x1B4, 0);
        arg0->state += 1;
        return;
    }
    taskKill(arg0);
}

void func_shelter_b1_underground_parking_801836D8(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        Gp_MsgPlayerWeapon(1);
        taskKill(arg0);
    }
}

void func_shelter_b1_underground_parking_80183714(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x300, 0);
            Gp_RunCapCmd(task->spawnArg1.value, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

void func_shelter_b1_underground_parking_801837D8(u8 arg0)
{
    gGameSession->at4.loc.room  = arg0;
    Mc_SaveData[0].state.at4.loc.room = arg0;
    gGameSession->roomObjsDirty = 1;
    gGameSession->viewDirty     = 1;
}

/// Room script callback: latch this room's script argument into `D_80115768`.
void func_shelter_b1_underground_parking_80183804(u8 arg0)
{
    D_80115768 = arg0;
}

static void func_shelter_b1_underground_parking_80183810(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_underground_parking_80187230;
    Game_SetPtrSlot(arg0, 7);
    func_shelter_b1_underground_parking_801848A4();
    if (gGameSession->at4.loc.place == 0x15) {
        Gp_MsgSlot4Chain(0, 1);
    }
    if ((GameFlag_GetNibble(0x7A) >= 6) && (GameFlag_GetNibble(0x123) == 0)) {
        GameFlag_SetNibble(0x123, 1);
        func_shelter_b1_underground_parking_8018390C();
    }
    arg0->state = arg0->state + 1;
}

/// Dispatches a task through the three-entry state table, copied onto the
/// stack first.
void func_shelter_b1_underground_parking_801838B4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_underground_parking_8017D7F4;
    sp.funcs[task->state](task);
}

static void func_shelter_b1_underground_parking_8018390C(void)
{
    if (D_shelter_b1_underground_parking_8018D758 == 0) {
        D_shelter_b1_underground_parking_8018D758 = 1;
        func_shelter_b1_underground_parking_80186890(1);
        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 5, 0, 0);
    }
}

/// Outlines `rect` on screen in (`r`, `g`, `b`) with four unconnected flat
/// `LINE_F2`s -- top, right, bottom and left edge of the rectangle spanning
/// (`x`, `y`) to (`x + w`, `y + h`) -- each linked into `gGpuCurrentOt[1]`.
static void func_shelter_b1_underground_parking_80183958(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}

static void func_shelter_b1_underground_parking_80183B9C(void)
{
    if (D_shelter_b1_underground_parking_8018D789 & 8) {
        func_shelter_b1_underground_parking_801857E0(-0x46, 0x54, 7, 0xF00);
    }
    if (D_shelter_b1_underground_parking_8018D789 & 4) {
        func_shelter_b1_underground_parking_801857E0(-0x28, 0x54, 7, 0xF0);
    }
    if (D_shelter_b1_underground_parking_8018D789 & 2) {
        func_shelter_b1_underground_parking_801857E0(-0xE, 0x54, 7, 0xF);
    }
    if (D_shelter_b1_underground_parking_8018D789 & 1) {
        func_shelter_b1_underground_parking_801857E0(0xC, 0x54, 7, 0xFF0);
    }
    func_shelter_b1_underground_parking_801857E0(0x11, -0x3D, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(0x3D, -0x3D, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(0x10, 9, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(-0x42, -0x19, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(-0x17, 0x2B, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(0x3D, 0x2B, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(
        D_shelter_b1_underground_parking_801876D4[D_shelter_b1_underground_parking_8018D789].vx,
        D_shelter_b1_underground_parking_801876D4[D_shelter_b1_underground_parking_8018D789].vy, 7, 0xF0);
}

static void func_shelter_b1_underground_parking_80183CEC(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1.value) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28[port];
        pad    = &Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons.halfwords[0];
        heldp  = &prompt->buttons.halfwords[1];
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                                          = 0;
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos = prompt->screen.packed;
                    *statep                                                             = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_shelter_b1_underground_parking_8018414C(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues one 16x24 textured quad -- the room's on-screen action prompt icon --
/// at (`x`, `y`) into the head of the current OT. `variant` selects the palette,
/// 0x3C87 when it is 2 and 0x3C88 otherwise, and 0 draws nothing at all.
static void func_shelter_b1_underground_parking_8018414C(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

/// Two-state dispatcher of the room's action-prompt task: builds the handler
/// pair on the stack and calls the entry `Task::state` names.
void func_shelter_b1_underground_parking_80184234(Task* task)
{
    TaskFunc states[2] = { func_shelter_b1_underground_parking_801848BC, func_shelter_b1_underground_parking_80183CEC };

    states[task->state](task);
}

/// The examine task: dispatches through its eight state handlers, copied onto
/// the stack first.
void func_shelter_b1_underground_parking_80184284(Task* task)
{
    TaskFuncTable8 fns;

    fns = D_shelter_b1_underground_parking_8017D9A4;
    fns.funcs[task->state](task);
}

static void func_shelter_b1_underground_parking_80184304(Task* task)
{
    SbupExamineWork* st;
    OverlayHotspot*  hs;

    st = memCalloc(0x10, 0);
    if (st == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer             = Task_SpawnFromTable(D_shelter_b1_underground_parking_80187664, 0, 1, 0);
    task->work                  = (TaskIdMap*)st;
    Mc_SaveData[0].state.at4.loc.view = 0x15;
    /* The once-loops fold away, but flow weights the references inside them
       by loop depth. The outer one keeps the state load below the mode store;
       the inner one lifts the work pointer's global-alloc priority back above
       the parameter's so the two keep their callee-saved homes. */
    do {
        task->state++;
        do {
            st->field_0 = 0;
        } while (0);
    } while (0);
    Display_AcquireRef();
    for (hs = D_shelter_b1_underground_parking_8018767C; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

static void func_shelter_b1_underground_parking_801843F0(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    Gp_RunCapCmd(GameFlag_GetNibble(0xE7) == 0 ? 2 : 3, 0);
    task->state++;
}

static void func_shelter_b1_underground_parking_80184468(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    OverlayHotspot*   hs     = D_shelter_b1_underground_parking_8018767C;
    SbupExamineWork*  work   = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    gGameSession->hideHud = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_shelter_b1_underground_parking_80184964(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons.slots[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->field_C    = hs->id;
                    work->promptKind = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons.slots[1].state == 2) {
        task->state = 5;
    }
}

static void func_shelter_b1_underground_parking_80184594(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    SbupExamineWork*  work   = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

static void func_shelter_b1_underground_parking_801845F8(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    SbupExamineWork*  work   = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        if (work->field_C == 0x10) {
            SndEvt_EnqueueType6(0x54140004, 0, 0);
            if (D_shelter_b1_underground_parking_8018D788 != D_shelter_b1_underground_parking_8018D789) {
                if (gGameSession->at4.loc.room == 1) {
                    SndEvt_EnqueueType6(0x54140006, 0, 0);
                } else {
                    SndEvt_EnqueueType6(0x54140005, 0, 0);
                }
                task->state = 6;
                return;
            }
        } else {
            D_shelter_b1_underground_parking_8018D789 ^= work->field_C;
            SndEvt_EnqueueType6(0x54140004, 0, 0);
            task->state = 2;
            return;
        }
    }
    task->state = 2;
}

static void func_shelter_b1_underground_parking_801846EC(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState    = 0;
    gGameSession->hideHud       = 0;
    gGameSession->cutsceneHold  = 0;
    Mc_SaveData[0].state.at4.loc.view = 2;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill((Task*)arg0->spawnArg2.pointer);
    Task_RequestKill(arg0, 0);
}

/// Commits the pending destination selected in the parking-lot map task:
/// promotes the pending value into the committed one, tears down the prompt
/// display, applies the selection to the session, then kills the child task
/// spawned for the selection UI and advances to the next state.
static void func_shelter_b1_underground_parking_80184778(Task* task)
{
    D_shelter_b1_underground_parking_8018D788 = D_shelter_b1_underground_parking_8018D789;
    func_shelter_b1_underground_parking_80183B9C();
    func_shelter_b1_underground_parking_8018491C();
    task->killCountdown = 0;
    taskKill((Task*)task->spawnArg2.pointer);
    task->state++;
}

static void func_shelter_b1_underground_parking_801847D0(Task* task)
{
    SbupExamineWork* work = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    work->fadeLevel += 6;
    if (work->fadeLevel >= 0x100) {
        work->fadeLevel = 0xFF;
    }
    Fade_DrawOverlay((u8)work->fadeLevel, (u8)work->fadeLevel, (u8)work->fadeLevel, 2);
    if (work->fadeLevel == 0xFF) {
        D_80114D08 = 0xA;
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        Display_ReleaseRef();
        gGameSession->eventState    = 0;
        gGameSession->hideHud       = 0;
        gGameSession->cutsceneHold  = 0;
        Mc_SaveData[0].state.at4.loc.view = 2;
        Task_RequestKill(task, 0);
    }
}

static void func_shelter_b1_underground_parking_801848A4(void)
{
    D_shelter_b1_underground_parking_8018D788 = 0xFF;
    D_shelter_b1_underground_parking_8018D789 = 0;
}

/// Resets both action-prompt slots before a script's first cursor scan and steps
/// the caller on one state: clears each slot's leading words and its two
/// trailing shorts, parks the target id at 0x100 with `field_E` at 0xF, and
/// marks the slot as highlighted (`mode` 1).
static void func_shelter_b1_underground_parking_801848BC(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0                     = 0;
        prompt->field_4                     = 0;
        prompt->targetId                    = 0x100;
        prompt->field_E                     = 0xF;
        prompt->buttons.slots[0].heldFrames = 0;
        prompt->buttons.slots[1].heldFrames = 0;
        prompt->mode                        = 1;
    }
    task->state = task->state + 1;
}

/// Looks up the low nibble of `D_shelter_b1_underground_parking_8018D788` in
/// the byte table `D_shelter_b1_underground_parking_801876C4`, stores the
/// result as the current room (both `Mc_SaveData[0].state.at4.loc.room` and the session's
/// `at4.loc.room`) and flags the room objects for relinking.
static void func_shelter_b1_underground_parking_8018491C(void)
{
    Mc_SaveData[0].state.at4.loc.room = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & 0xF];
    gGameSession->at4.loc.room  = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & 0xF];
    gGameSession->roomObjsDirty = 1;
}

/// Marks every hotspot of `table` under (`x`, `y`) as hit; answers whether any
/// was.
static s32 func_shelter_b1_underground_parking_80184964(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

void func_shelter_b1_underground_parking_80184A18(Task* unused)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
        case 10:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            func_shelter_b1_underground_parking_8018543C(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_8018771C[11], 0x200, 0, 0x111);
            break;
        case 3:
        case 7:
        case 11:
        case 12:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            func_shelter_b1_underground_parking_8018543C(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_8018771C[11], 0x200, 0, 0x111);
            break;
        case 8:
        case 13:
            func_shelter_b1_underground_parking_80184C54(D_shelter_b1_underground_parking_801877A4, 0x200, 0, 0x210);
        case 4:
        case 14:
            func_shelter_b1_underground_parking_80184C54(&(D_shelter_b1_underground_parking_8018771C + 1)[0], 0x200, -0x400, 0x111);
            func_shelter_b1_underground_parking_80184C54(&(D_shelter_b1_underground_parking_8018771C + 1)[2], 0x200, -0x400, 0x111);
            func_shelter_b1_underground_parking_80184C54(&(D_shelter_b1_underground_parking_8018771C + 1)[4], 0x200, 0x800, 0x111);
            func_shelter_b1_underground_parking_80184C54(&(D_shelter_b1_underground_parking_8018771C + 1)[6], 0x200, 0x800, 0x111);
            func_shelter_b1_underground_parking_80184C54(&(D_shelter_b1_underground_parking_8018771C + 1)[8], 0x200, 0, 0x111);
            break;
        case 9:
        case 15:
        case 22:
        case 24:
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_80187784[0], 0x200, 0, 0x111);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_80187784[2], 0x200, 0x800, 0x111);
            break;
        case 16:
            func_shelter_b1_underground_parking_8018543C(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            break;
        case 18:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            break;
        case 20:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                func_shelter_b1_underground_parking_80185F08(D_shelter_b1_underground_parking_80187714, 0x180, 0x80);
            } else {
                func_shelter_b1_underground_parking_80185F08(D_shelter_b1_underground_parking_80187714, 0x60, 0x80);
            }
            break;
    }
}

/// Projects the two world-space points `arg0[0]` and `arg0[1]` and, if the
/// second lies at an OTZ of at least 0x11, queues gouraud wedges around both
/// and between them, rotated by `arg2`; `arg1` is a signed half-extent. The
/// inner vertex is scaled by the frame-counter blend byte
/// `((field_8 & 1) * 8 | 0x20)`: red is `blend * ((arg3 << 16) >> 24)`, green
/// `blend * (((arg3 << 16) >> 20) & 1)`, blue `blend * (arg3 & 1)`.
static void func_shelter_b1_underground_parking_80184C54(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 1);
        base      = (s16)arg2;
        b         = blend * (arg3 & 1);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, if
/// the resulting OTZ is at least 0x11, queues four gouraud `POLY_G4` wedges
/// around the projected centre. `arg1` is a signed half-extent; the on-screen
/// radius is `(s16)arg1 * 64 / otz`. `arg2` scales the inner vertex by the
/// frame-counter blend byte `((field_8 & 1) * 8 | 0x20)`: red is
/// `blend * ((arg2 << 16) >> 24)`, green `blend * (((arg2 << 16) >> 20) & 1)`,
/// blue `blend * (arg2 & 1)`.
static void func_shelter_b1_underground_parking_8018543C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    u8*                ds_ptr;
    DisplayState*      ds;
    s32                radius;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0xC);
        block   = (RoomDraw25Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        radius        = ((s16)arg1 * 64) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        ds_ptr        = (u8*)&gDisplayState;
        packed        = arg2 << 16;
        blend         = (((u8)((DisplayState*)ds_ptr)->animFrame & 1) * 8) | 0x20;
        r             = blend * (packed >> 24);
        g             = blend * ((packed >> 20) & 1);
        b             = blend * (arg2 & 1);
        ang           = 0;
        ds            = (DisplayState*)ds_ptr;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0xC);
}

static void func_shelter_b1_underground_parking_801857E0(s16 x, s16 y, s16 radius, s16 color)
{
    POLY_G4* prim;
    s32      i;
    s32      ang;
    s32      t;
    u8       r;
    u8       g;
    u8       b;
    s32      base;
    s32      c;
    s32      rMask;
    s32      gMask;

    i     = 0;
    base  = (gDisplayState.animFrame & 1) * 12;
    c     = color;
    rMask = (c >> 4) & 0xF0;
    gMask = c & 0xF0;
    r     = base + rMask;
    g     = base + gMask;
    b     = base + ((color & 0xF) << 4);
    do {
        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = x + ((radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = y + ((radius * rcos(ang)) >> 12);
            prim->x1 = x + ((radius * rsin(t)) >> 12);
            prim->y1 = y + ((radius * rcos(t)) >> 12);
            t        = ang + 0x200;
            prim->x2 = x;
            prim->y2 = y;
            prim->x3 = x + ((radius * rsin(t)) >> 12);
            prim->y3 = y + ((radius * rcos(t)) >> 12);
            ang      = t;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)0x40 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, 0x40);
        } while (ang < 0x1000);
        radius <<= 1;
        r      >>= 1;
        g      >>= 1;
        b      >>= 1;
        i++;
    } while (i < 3);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a signed
/// half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1` scales
/// `gDisplayState.animFrame` into `rsin` so the lit vertex pulses as
/// `rsin(...) / 34 + 0x78` on green and blue.
static void func_shelter_b1_underground_parking_80185A94(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x10);
        block   = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
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
            setRGB1(line, 0, pulse, pulse);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges around the projected centre. `arg2` is a signed
/// half-extent; on-screen radii are `(s16)arg2 * 64 / otz` (outer) and
/// `(s16)arg2 * 8 / otz` (inner). `arg1` scales `gDisplayState.animFrame` into
/// `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
static void func_shelter_b1_underground_parking_80185F08(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                pulse;
    s32                color;
    s32                half;
    s32                size;
    s32                ang;
    s32                t;
    s32                t2;
    s32                u;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw05Scratch*)(head - 0x14))->sx);
    gte_stflg(&((RoomDraw05Scratch*)(head - 0x14))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
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
            setRGB2(prim, 0, color, color);
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

        color = half;
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
    SCRATCH_POP_BYTES(0x14);
}

static void func_shelter_b1_underground_parking_80186890(s16 arg0)
{
    D_shelter_b1_underground_parking_8018D78C = arg0;
}
