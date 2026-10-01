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
#include "../../shared/action_prompt.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
#include "../../shared/room_cutscene.h"
#include "../../shared/room_variants.h"

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
extern RoomDeparture gRoomDeparture;

/// The cutscene task's descriptor table; entry 0 runs a scene record, entry 1
/// is the scene's sub-task.
extern TaskDesc gRoomCutsceneTaskDescs[];

/// The "%" suffix appended to the play-data percentages.
static u8 Telephone_Data_80181A78[];

/// Descriptor of the play-data panels' shared frame.
static UiObjectDesc Telephone_Data_80181C90;

/// The item id the shop list's cursor last rested on.
static s32 Shop_Data_801819EC;

/// Flag tested as zero / non-zero when drawing the room's view-dependent
/// markers: it selects 0x180 or 0x60 as the second argument of their draw
/// calls. Its meaning is unproven.
extern u16 D_shelter_b1_underground_parking_8018D78C;

extern SVECTOR D_shelter_b1_underground_parking_8018771C[13];

extern void func_80131E38(void);

extern UiObjectDesc D_800611E4;

/// Labels of the nine play-data rows, and the help line each row shows while
/// it is selected.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// The unit suffix appended to the play-data counts.
static u8 Telephone_Data_80181A70[];

/// The list the weapon and PE usage panels fill.
static UiList Telephone_Data_80181C6C;

/// Title and list of the menu `func_shelter_b1_underground_parking_8017EDE8`
/// runs.
static const char Telephone_Data_8017D638[];
static UiList     Telephone_Data_80181CF4;

/// List of the menu `func_shelter_b1_underground_parking_8017F2A0` runs.
static UiList Telephone_Data_80181C44;

/// Texts the menu's four row handlers draw.
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Descriptors of the panels the rows open.
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;

/// The 0xFFFF-terminated item id lists `func_shelter_b1_underground_parking_8017F80C`
/// chooses from.
static u16 Shop_Data_801815F8[];
static u16 Shop_Data_80181600[];
static u16 Shop_Data_80181608[];
static u16 Shop_Data_80181610[];
static u16 Shop_Data_80181620[];
static u16 Shop_Data_80181630[];
static u16 Shop_Data_80181640[];
static u16 Shop_Data_80181648[];
static u16 Shop_Data_80181658[];
static u16 Shop_Data_80181668[];
static u16 Shop_Data_80181678[];
static u16 Shop_Data_80181680[];
static u16 Shop_Data_80181694[];
static u16 Shop_Data_801816AC[];
static u16 Shop_Data_801816C0[];
static u16 Shop_Data_801816C8[];
static u16 Shop_Data_801816D8[];
static u16 Shop_Data_801816F0[];
static u16 Shop_Data_80181704[];
static u16 Shop_Data_8018170C[];
static u16 Shop_Data_80181720[];
static u16 Shop_Data_8018173C[];
static u16 Shop_Data_8018174C[];
static u16 Shop_Data_80181758[];
static u16 Shop_Data_80181770[];
static u16 Shop_Data_8018178C[];
static u16 Shop_Data_801817A0[];
static u16 Shop_Data_801817A8[];
static u16 Shop_Data_801817BC[];
static u16 Shop_Data_801817DC[];
static u16 Shop_Data_801817EC[];
static u16 Shop_Data_801817F8[];
static u16 Shop_Data_80181810[];
static u16 Shop_Data_80181814[];
static u16 Shop_Data_80181818[];
static u16 Shop_Data_80181820[];
static u16 Shop_Data_80181830[];
static u16 Shop_Data_80181838[];
static u16 Shop_Data_80181840[];
static u16 Shop_Data_80181848[];
static u16 Shop_Data_80181854[];
static u16 Shop_Data_8018185C[];
static u16 Shop_Data_80181868[];
static u16 Shop_Data_80181870[];
static u16 Shop_Data_8018187C[];
static u16 Shop_Data_80181888[];
static u16 Shop_Data_80181890[];
static u16 Shop_Data_80181898[];
static u16 Shop_Data_801818A4[];
static u16 Shop_Data_801818B0[];
static u16 Shop_Data_801818B8[];
static u16 Shop_Data_801818C4[];
static u16 Shop_Data_801818D0[];
static u16 Shop_Data_801818DC[];
static u16 Shop_Data_801818E0[];
static u16 Shop_Data_801818EC[];
static u16 Shop_Data_801818F8[];
static u16 Shop_Data_80181904[];
static u16 Shop_Data_8018190C[];
static u16 Shop_Data_80181918[];
static u16 Shop_Data_80181924[];
static u16 Shop_Data_80181930[];
static u16 Shop_Data_80181938[];
static u16 Shop_Data_80181944[];
static u16 Shop_Data_80181AD4[];

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
static u8           Shop_Data_80181A0C[];
static u8           Shop_Data_80181A1C[];
static u8           Shop_Data_80181A20[];
static UiObjectDesc Shop_Data_80181B84;
static UiObjectDesc Shop_Data_80181BD8;

/// The shop's unlockable stock rows.
static RoomShopTier Shop_Data_80181950[13];

/// The shop list's row handlers and the balance panel beside it.
static UiListItemFunc Shop_Data_80181AD8[];
static UiObjectDesc   Shop_Data_80181BF4;

/// Texts of the four rows that pick an entry of the shop's id list, and the
/// panel they open.
static u8           Shop_Data_80181A5C[];
static u8           Shop_Data_80181A64[];
static u8           Shop_Data_80181A70[];
static u8           Shop_Data_80181A78[];
static UiObjectDesc Shop_Data_80181B4C;

/// List of the menu `func_shelter_b1_underground_parking_80180C90` runs, and
/// the panel it opens first.
static UiList       Shop_Data_80181AE0;
static UiObjectDesc Shop_Data_80181B68;

/// The purchase confirmation: its text and the panels it answers with.
static u8           Shop_Data_801819F0[];
static UiObjectDesc Shop_Data_80181BA0;
static UiObjectDesc Shop_Data_80181C10;

/// The three messages the notice panel picks from.
static u8 Shop_Data_80181A80[];
static u8 Shop_Data_80181A94[];
static u8 Shop_Data_80181AA4[];

/// The charge panel's title, and the quantity and item map of the slot it is
/// animating.
static const char Shop_Data_8017D6F4[];
static s32        Shop_Data_80187628;
static GpItemMap* Shop_Data_8018762C;

/// Label of the held-quantity line.
static u8 Shop_Data_80181AC4[];

/// Text the quantity picker draws beside the item.
static u8 Shop_Data_80181AD0[];

/// Text of the row that closes its panel.
static u8 Shop_Data_80181A04[];

/// The list `func_shelter_b1_underground_parking_80181D88` drives.
static UiList Shop_Data_80181B0C;

/// The descriptor of the modal panel `func_shelter_b1_underground_parking_80181EB0`
/// runs.
static UiObjectDesc Shop_Data_80181B30;

/// The scene sub-task the cutscene runner spawned, while it runs.
extern Task* gRoomCutsceneSoundTask;

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
extern ScreenFade                             D_shelter_b1_underground_parking_8018D750;

/// The room's ambience table, one entry per area.
extern RoomAmbienceEntry D_shelter_b1_underground_parking_8018761C[];

extern EvsCommand D_shelter_b1_underground_parking_801872D8[];
extern EvsCommand D_shelter_b1_underground_parking_801873DC[];
extern EvsCommand D_shelter_b1_underground_parking_80187544[];

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

static void func_shelter_b1_underground_parking_80183810(Task* arg0);
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
static void func_shelter_b1_underground_parking_801857E0(s16 x, s16 y, s16 radius, s16 color);
static void func_shelter_b1_underground_parking_80186890(s16 arg0);

#define TELEPHONE_TITLE_BYTES "Telephone\0<\x9E"
#include "../../shared/telephone.h"

#define SHOP_CHARGE_TITLE_BYTES "Charge\0o"
#include "../../shared/shop.h"

extern GpGridParams               D_shelter_b1_underground_parking_80187E50[1];
extern GpGridParams               D_shelter_b1_underground_parking_801884D4[1];
extern GpGridParams               D_shelter_b1_underground_parking_80188BC4[1];
extern GpGridParams               D_shelter_b1_underground_parking_8018912C[1];
extern GpGridParams               D_shelter_b1_underground_parking_80189754[1];
extern GpObj4C                    D_shelter_b1_underground_parking_8018B094[8];
extern GpObj4C                    D_shelter_b1_underground_parking_8018B2F4[8];
extern GpObj4C                    D_shelter_b1_underground_parking_8018B674[12];
extern GpObj4C                    D_shelter_b1_underground_parking_8018BA04[12];
extern GpObj4C                    D_shelter_b1_underground_parking_8018BD94[12];
extern GpObj4C                    D_shelter_b1_underground_parking_8018C124[12];
extern GpObj4C                    D_shelter_b1_underground_parking_8018C4B4[12];
extern GpObj4C                    D_shelter_b1_underground_parking_8018C844[20];
extern GpObj4C                    D_shelter_b1_underground_parking_8018CE34[16];
extern GpObj4C                    D_shelter_b1_underground_parking_8018D2F4[11];
extern WorldCoordRoomAmbientEntry D_shelter_b1_underground_parking_8018D638[25];
extern GpRoomCoordSet             D_shelter_b1_underground_parking_8018B07C[1];
s32                               func_shelter_b1_underground_parking_80182830(Task*, s32, RoomEventMsg*, TaskMessageArg);
s32                               func_shelter_b1_underground_parking_80182A60(Task*, s32, s32, s32);
s32                               func_shelter_b1_underground_parking_80183284(Task*, s32, s32, TaskMessageArg);
s32                               func_shelter_b1_underground_parking_80183360(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                               func_shelter_b1_underground_parking_801833DC(Task*, s32, s32, TaskMessageArg);
void                              func_shelter_b1_underground_parking_80182DB4(Task*);
void                              func_shelter_b1_underground_parking_80182FC8(Task*);
void                              func_shelter_b1_underground_parking_80183410(Task*);
void                              func_shelter_b1_underground_parking_801834D4(Task*);
void                              func_shelter_b1_underground_parking_80183560(Task*);
void                              func_shelter_b1_underground_parking_8018363C(Task*);
void                              func_shelter_b1_underground_parking_801836D8(Task*);
void                              func_shelter_b1_underground_parking_80183714(Task*);
void                              func_shelter_b1_underground_parking_801837D8(u8);
void                              func_shelter_b1_underground_parking_80183804(u8);
void                              func_shelter_b1_underground_parking_80184234(Task*);
void                              func_shelter_b1_underground_parking_80184284(Task*);

extern SpriteBatch  D_shelter_b1_underground_parking_80189AD8[2];
extern SpriteBatch  D_shelter_b1_underground_parking_80189AE8[2];
extern SpriteBatch  D_shelter_b1_underground_parking_80189BFC[3];
extern SpriteBatch  D_shelter_b1_underground_parking_80189E94[4];
extern SpriteBatch  D_shelter_b1_underground_parking_80189EB4[2];
extern SpriteBatch  D_shelter_b1_underground_parking_80189EC4[2];
extern SpriteSource D_shelter_b1_underground_parking_80189AF8[13];
extern SpriteSource D_shelter_b1_underground_parking_80189C14[32];

#include "../../shared/telephone_data.inc.c"

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

ShelterB1UndergroundParkingStorage71F0 D_shelter_b1_underground_parking_801871F0 = { { { { TASK_BODY_NONE, 192 } }, Shop_SessionTask, { .value = 0 } }, { 0 } };

TaskDesc D_shelter_b1_underground_parking_80187200 = { { { TASK_BODY_NONE, 32 } }, roomDepartureTask, { .value = 0 } };

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

GpMsgEntry D_shelter_b1_underground_parking_80187230[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_underground_parking_80183360 },
    { 5105, func_shelter_b1_underground_parking_80183284 },
    { 5103, func_shelter_b1_underground_parking_80182830 },
    { 5104, func_shelter_b1_underground_parking_80182A60 },
    { 5106, func_shelter_b1_underground_parking_801833DC },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_b1_underground_parking_80187260[1] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_80183410, { .value = 0 } },
};

TaskDesc D_shelter_b1_underground_parking_8018726C[7] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_801834D4, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_80183560, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_8018363C, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_80182DB4, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_801836D8, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_80182FC8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_underground_parking_80183714, { .value = 0 } },
};

ActorTransform D_shelter_b1_underground_parking_801872C0 = { { 3155, 0, -247, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_shelter_b1_underground_parking_801872D8[10] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54140007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b1_underground_parking_801872C0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

AnimationPlayRequest D_shelter_b1_underground_parking_801873C8 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_shelter_b1_underground_parking_801873DC[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_shelter_b1_underground_parking_80183804 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_underground_parking_801873C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54140003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54140009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_shelter_b1_underground_parking_801837D8 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_shelter_b1_underground_parking_80187544[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_shelter_b1_underground_parking_801837D8 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_underground_parking_80184234, { .value = 0 } },
};

TaskDesc D_shelter_b1_underground_parking_80187670 = { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_80184284, { .value = 0 } };

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

u8* D_shelter_b1_underground_parking_8018791C[8] = {
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
#include "assets/shelter_b1_underground_parking_collision_0A890_normals.inc"
};

SVECTOR D_shelter_b1_underground_parking_801879EC[60] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_verts.inc"
};

GpGridFace D_shelter_b1_underground_parking_80187BCC[22] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_faces.inc"
};

s16 D_shelter_b1_underground_parking_80187CD4[150] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_underground_parking_80187CD4[i])
s16* D_shelter_b1_underground_parking_80187E00[20] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_underground_parking_80187E50[1] = {
    { NULL, D_shelter_b1_underground_parking_801879BC, D_shelter_b1_underground_parking_801879EC, D_shelter_b1_underground_parking_80187BCC, D_shelter_b1_underground_parking_80187E00, 5390, 8000, 5, 4, 4000, 22 },
};

SVECTOR D_shelter_b1_underground_parking_80187E74[8] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_normals.inc"
};

SVECTOR D_shelter_b1_underground_parking_80187EB4[84] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_verts.inc"
};

GpGridFace D_shelter_b1_underground_parking_80188154[33] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_faces.inc"
};

s16 D_shelter_b1_underground_parking_801882E0[210] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_underground_parking_801882E0[i])
s16* D_shelter_b1_underground_parking_80188484[20] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_underground_parking_801884D4[1] = {
    { NULL, D_shelter_b1_underground_parking_80187E74, D_shelter_b1_underground_parking_80187EB4, D_shelter_b1_underground_parking_80188154, D_shelter_b1_underground_parking_80188484, 5390, 8000, 5, 4, 4000, 33 },
};

SVECTOR D_shelter_b1_underground_parking_801884F8[8] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_normals.inc"
};

SVECTOR D_shelter_b1_underground_parking_80188538[90] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_verts.inc"
};

GpGridFace D_shelter_b1_underground_parking_80188808[36] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_faces.inc"
};

s16 D_shelter_b1_underground_parking_801889B8[222] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_underground_parking_801889B8[i])
s16* D_shelter_b1_underground_parking_80188B74[20] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_underground_parking_80188BC4[1] = {
    { NULL, D_shelter_b1_underground_parking_801884F8, D_shelter_b1_underground_parking_80188538, D_shelter_b1_underground_parking_80188808, D_shelter_b1_underground_parking_80188B74, 5390, 8000, 5, 4, 4000, 36 },
};

SVECTOR D_shelter_b1_underground_parking_80188BE8[6] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_normals.inc"
};

SVECTOR D_shelter_b1_underground_parking_80188C18[68] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_verts.inc"
};

GpGridFace D_shelter_b1_underground_parking_80188E38[27] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_faces.inc"
};

s16 D_shelter_b1_underground_parking_80188F7C[176] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_underground_parking_80188F7C[i])
s16* D_shelter_b1_underground_parking_801890DC[20] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_underground_parking_8018912C[1] = {
    { NULL, D_shelter_b1_underground_parking_80188BE8, D_shelter_b1_underground_parking_80188C18, D_shelter_b1_underground_parking_80188E38, D_shelter_b1_underground_parking_801890DC, 5390, 8000, 5, 4, 4000, 27 },
};

SVECTOR D_shelter_b1_underground_parking_80189150[8] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_normals.inc"
};

SVECTOR D_shelter_b1_underground_parking_80189190[83] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_verts.inc"
};

GpGridFace D_shelter_b1_underground_parking_80189428[30] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_faces.inc"
};

s16 D_shelter_b1_underground_parking_80189590[186] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_underground_parking_80189590[i])
s16* D_shelter_b1_underground_parking_80189704[20] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_table.inc"
};
#undef GRID_CELL

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

SpriteBatch D_shelter_b1_underground_parking_80189AD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_80189AE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_underground_parking_80189AF8[13] = {
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

SpriteBatch D_shelter_b1_underground_parking_80189BFC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_underground_parking_80189C14[32] = {
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

SpriteBatch D_shelter_b1_underground_parking_80189E94[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { 21, 11, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_80189EB4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_80189EC4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_80189ED4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_80189EE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_80189EF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_underground_parking_80189F04[11] = {
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

SpriteBatch D_shelter_b1_underground_parking_80189FE0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_80189FF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_underground_parking_8018A008[42] = {
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

SpriteBatch D_shelter_b1_underground_parking_8018A350[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 29, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_underground_parking_8018A370[65] = {
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

SpriteBatch D_shelter_b1_underground_parking_8018A884[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 11, 0, 0, { 2, 0 } },
    { 32, 33, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018A8AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018A8BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018A8CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_underground_parking_8018A8DC[28] = {
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

SpriteBatch D_shelter_b1_underground_parking_8018AB0C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018AB2C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018AB3C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018AB4C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018AB5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018AB6C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018AB7C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_underground_parking_8018AB8C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
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

WorldCoordPointLight D_shelter_b1_underground_parking_8018ACBC[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1570, -2500, 1829 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 2867, 2867 }, { 0, 0 } }, 2816, 4608 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2410, -2500, 2490 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2816, 4608 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2410, -2500, -2730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2816, 4608 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1580, -2139, -2730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2816, 4608 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2108, -1663, -6654 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2304, 2867, 2266 }, { 0, 0 } }, 1301, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7240, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2035, 2999 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x355C, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 4000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x4D1C, -2500, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 4000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7370, -2138, 2730 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2560, 4352 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7370, -2138, -2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2560, 4352 },
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
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_underground_parking_8018B560[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaTmdRec D_shelter_b1_underground_parking_8018B570[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_underground_parking_8018B57C[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaTmdRec D_shelter_b1_underground_parking_8018B58C[2] = {
    { 116, 615, 0, 0, { 0, 0 }, D_801401B0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_underground_parking_8018B5A4[2] = {
    { 116, 0, 0, 3700, 0, -114, 2488, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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

WorldCoordRoomAmbientEntry D_shelter_b1_underground_parking_8018D638[25] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b1_underground_parking_8018D638) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 2256, 2255, 2255, 2255 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b1_underground_parking_8018D700 = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionSurfaceProperties D_shelter_b1_underground_parking_8018D70C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_underground_parking_8018D714[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_underground_parking_8018D700 },
};

WorldCollisionSurfaceProperties D_shelter_b1_underground_parking_8018D71C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_shelter_b1_underground_parking_8018D724[8] = {
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D714,
    D_shelter_b1_underground_parking_8018D71C,
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D70C,
    D_shelter_b1_underground_parking_8018D70C,
};

static s32 Shop_Data_80187628 = 0;

static GpItemMap* Shop_Data_8018762C = NULL;

Task* D_shelter_b1_underground_parking_8018D74C = NULL;

ScreenFade D_shelter_b1_underground_parking_8018D750 = { 0 };

Task* gRoomCutsceneSoundTask = NULL;

s32 D_shelter_b1_underground_parking_8018D758 = 0;

ShelterB1UndergroundParkingStorageD75C D_shelter_b1_underground_parking_8018D75C = { { 0 }, { 0 } };

RoomDeparture gRoomDeparture = { 0, 0, 0, 0, 0, { 0, 0 }, 0 };

u8 D_shelter_b1_underground_parking_8018D788 = 0;

u8 D_shelter_b1_underground_parking_8018D789 = 0;

u16 D_shelter_b1_underground_parking_8018D78A = 0xCCEE;

u16 D_shelter_b1_underground_parking_8018D78C = 0;

static inline s32 Shop_AddItemCount(s32 item, s32 count);
static void       func_shelter_b1_underground_parking_801826C0(Task* roomTask);
static void       func_shelter_b1_underground_parking_80183B9C(void);

#include "../../shared/telephone.inc.c"

void func_shelter_b1_underground_parking_8017EDE8(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/room_event_departure_task.inc.c"

#include "../../shared/room_cutscene_task.inc.c"

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
    GfxCoord*  coord;
    s32        z;
    s32        facing;

    task  = gameGetPtrSlot(3);
    actor = (GameActor*)task->work;
    coord = task->extra.tmd->coords;
    if ((actor->mode != GAME_ACTOR_MODE_SCRIPTED) && (Gp_CapBusy() == 0) && (gGameSession->location.loc.room >= 7) &&
        (coord->coord.t[0] < -0x1266)) {
        z = coord->coord.t[2];
        if (z < 0x7D0) {
            if ((z >= -0x7CF) && (Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                facing = (u16)actor->rotation.vy & 0xFFF;
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
s32 func_shelter_b1_underground_parking_80182830(Task* task, s32 msgId, RoomEventMsg* msg, TaskMessageArg arg3)
{
    if (msg->warp == 1 && gGameSession->location.loc.variant == 0x15) {
        func_80131E38();
    }
    if (msg->warp == 0xA) {
        if ((u8)msg->room == 1 && gGameSession->location.loc.room < 7) {
            Gp_StartCapSlot(0xA, 1, 0);
            GameFlag_SetNibble(0x1B4, 2);
        }
    }
    if (msg->warp == 0xB) {
        switch (gGameSession->location.loc.room) {
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
    if (msg->warp == 0xC) {
        switch (gGameSession->location.loc.room) {
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
            if (gGameSession->location.loc.room < 6) {
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
            switch (gGameSession->location.loc.room) {
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
            switch (gGameSession->location.loc.room) {
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
                    Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, 9, st);
                }
            } else {
                D_shelter_b1_underground_parking_8018D758 = 0;
                st->field_1                               = 0x1F;
                st->field_3                               = 0;
                st->field_2                               = 1;
                Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, arg2, st);
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
/// `gRoomDeparture` and spawns entry 0 of
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
                D_shelter_b1_underground_parking_8018D750.blend      = SCREEN_FADE_SUBTRACT;
                D_shelter_b1_underground_parking_8018D750.phase      = SCREEN_FADE_RUNNING;
                D_shelter_b1_underground_parking_8018D750.rampFrames = 0x1E;
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
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x1B;
                }
                handler      = roomVariantResolveNeoArk;
                rec.stage    = 5;
                rec.area     = 1;
                rec.room     = 1;
                rec.warp     = 1;
                rec.sndEvent = 0x54140008;
                rec.facing   = ROOM_DEPARTURE_SKIP_FACING;
                Gp_MsgPlayerWeapon(0);
                p             = &rec;
                msg.areaId    = p->area;
                msg.warp      = p->warp;
                msg.room      = p->room;
                msg.queryOnly = ROOM_EVENT_EXECUTE;
                handler(&msg, &msg);
                p->area        = msg.areaId;
                p->warp        = msg.warp;
                p->room        = msg.room;
                gRoomDeparture = rec;
                Task_SpawnFromTable(&D_shelter_b1_underground_parking_80187200, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

/// Keeps the room's looping ambience in step with the area the session is in:
/// `gGameSession->location.loc.view` selects an entry of the ambience table, and
/// state 0 starts the loop with `SndEvt_EnqueueType6`. Once
/// `D_shelter_b1_underground_parking_8018D758` is clear, state 1 queues a
/// `SndEvt_EnqueueType7` event for the loop and ends the task; otherwise it waits for the session's view to stop matching `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view`,
/// states 2 to 4 walk the task along, and state 5 retunes the loop to the new
/// entry with `SndEvt_EnqueueTypeA` and returns to state 1.
void func_shelter_b1_underground_parking_80182FC8(Task* task)
{
    s32 pan;
    s32 vol;
    u8  idx;

    idx = gGameSession->location.loc.view;
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
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != gGameSession->location.loc.view) {
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

#include "../../shared/room_variants_neo_ark.inc.c"

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

#include "../../shared/room_cutscene_sound_task.inc.c"

s32 func_shelter_b1_underground_parking_80183284(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x121 || arg2 == 0x122) {
        if (gGameSession->location.loc.room == 6) {
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

s32 func_shelter_b1_underground_parking_80183360(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (D_shelter_b1_underground_parking_8018D758 == 0) {
        return 1;
    }
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        Gp_RunCapCmd1(0x1E);
    }
    return 2;
}

s32 func_shelter_b1_underground_parking_801833DC(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
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
                gGameSession->location.loc.room                            = 6;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 6;
                gGameSession->roomObjsDirty                                = state;
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
    gGameSession->location.loc.room                            = arg0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = arg0;
    gGameSession->roomObjsDirty                                = 1;
    gGameSession->viewDirty                                    = 1;
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
    if (gGameSession->location.loc.variant == 0x15) {
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

#include "../../shared/action_prompt_outline_rect.inc.c"

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

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// Two-state dispatcher of the room's action-prompt task: builds the handler
/// pair on the stack and calls the entry `Task::state` names.
void func_shelter_b1_underground_parking_80184234(Task* task)
{
    TaskFunc states[2] = { actionPromptReset, actionPromptMoveCursors };

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
    task->spawnArg2.pointer                                    = Task_SpawnFromTable(D_shelter_b1_underground_parking_80187664, 0, 1, 0);
    task->work                                                 = st;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x15;
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
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
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
                if (gGameSession->location.loc.room == 1) {
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
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill(arg0->spawnArg2.pointer);
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
    taskKill(task->spawnArg2.pointer);
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
        gGameSession->eventState                                   = 0;
        gGameSession->hideHud                                      = 0;
        gGameSession->cutsceneHold                                 = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
        Task_RequestKill(task, 0);
    }
}

static void func_shelter_b1_underground_parking_801848A4(void)
{
    D_shelter_b1_underground_parking_8018D788 = 0xFF;
    D_shelter_b1_underground_parking_8018D789 = 0;
}

#include "../../shared/action_prompt_reset.inc.c"

/// Looks up the low nibble of `D_shelter_b1_underground_parking_8018D788` in
/// the byte table `D_shelter_b1_underground_parking_801876C4`, stores the
/// result as the current room (both `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room` and the session's
/// `location.loc.room`) and flags the room objects for relinking.
static void func_shelter_b1_underground_parking_8018491C(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & 0xF];
    gGameSession->location.loc.room                            = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & 0xF];
    gGameSession->roomObjsDirty                                = 1;
}

#include "../../shared/action_prompt_hit_test.inc.c"

void func_shelter_b1_underground_parking_80184A18(Task* unused)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
        case 10:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                glowDrawDiamond(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                glowDrawDiamond(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            glowDrawBitDisc(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            glowDrawBeam(&D_shelter_b1_underground_parking_8018771C[11], 0x200, 0, 0x111);
            break;
        case 3:
        case 7:
        case 11:
        case 12:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                glowDrawDiamond(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                glowDrawDiamond(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            glowDrawBitDisc(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            glowDrawBeam(&D_shelter_b1_underground_parking_8018771C[11], 0x200, 0, 0x111);
            break;
        case 8:
        case 13:
            glowDrawBeam(D_shelter_b1_underground_parking_801877A4, 0x200, 0, 0x210);
        case 4:
        case 14:
            glowDrawBeam(&(D_shelter_b1_underground_parking_8018771C + 1)[0], 0x200, -0x400, 0x111);
            glowDrawBeam(&(D_shelter_b1_underground_parking_8018771C + 1)[2], 0x200, -0x400, 0x111);
            glowDrawBeam(&(D_shelter_b1_underground_parking_8018771C + 1)[4], 0x200, 0x800, 0x111);
            glowDrawBeam(&(D_shelter_b1_underground_parking_8018771C + 1)[6], 0x200, 0x800, 0x111);
            glowDrawBeam(&(D_shelter_b1_underground_parking_8018771C + 1)[8], 0x200, 0, 0x111);
            break;
        case 9:
        case 15:
        case 22:
        case 24:
            glowDrawBeam(&D_shelter_b1_underground_parking_80187784[0], 0x200, 0, 0x111);
            glowDrawBeam(&D_shelter_b1_underground_parking_80187784[2], 0x200, 0x800, 0x111);
            break;
        case 16:
            glowDrawBitDisc(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            break;
        case 18:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                glowDrawDiamond(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                glowDrawDiamond(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            break;
        case 20:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                glowDrawPulsingDisc(D_shelter_b1_underground_parking_80187714, 0x180, 0x80);
            } else {
                glowDrawPulsingDisc(D_shelter_b1_underground_parking_80187714, 0x60, 0x80);
            }
            break;
    }
}

#include "../../shared/glow_draw_beam.inc.c"

#include "../../shared/glow_draw_bit_disc.inc.c"

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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)0x40 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
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

#include "../../shared/glow_draw_diamond.inc.c"

#include "../../shared/glow_draw_pulsing_disc.inc.c"

static void func_shelter_b1_underground_parking_80186890(s16 arg0)
{
    D_shelter_b1_underground_parking_8018D78C = arg0;
}
