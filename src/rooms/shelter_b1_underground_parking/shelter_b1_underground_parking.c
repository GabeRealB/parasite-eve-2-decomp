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
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
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
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

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
#include "rooms/shop_tier.h"
#include "../../shared/action_prompt.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
#include "../../shared/room_cutscene.h"
#include "../../shared/room_variants.h"

static s32 _roomVariantResolveNeoArk(RoomEventMsg* request, RoomEventMsg* reply);

/// `ActionPromptHotspot::id` of the selector panel's enter button, which
/// commits the pending switch pattern. The four switch hotspots carry the bit
/// each one toggles in that pattern instead (8, 4, 2, 1).
#define SHELTER_B1_UNDERGROUND_PARKING_PANEL_HOTSPOT_ENTER 16

/// Amount the selector panel's closing fade darkens each frame.
#define SHELTER_B1_UNDERGROUND_PARKING_PANEL_FADE_STEP 6

/// RGB nibbles for the selector panel's screen-space glows, in red/green/blue order.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_RED      = 0xF00,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_GREEN    = 0x0F0,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_BLUE     = 0x00F,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_YELLOW   = 0xFF0,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DIM_GREY = 0x111,
};

/// Pixel radii of the panel's switch/selection indicators and fixed map nodes.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS = 7,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_MAP_NODE_RADIUS  = 10,
};

/// Pulse selection and rendering scales for this room's view-dependent lights.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_SLOW                = 0,
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_FAST                = 1,
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_SLOW           = 0x60, // Angle units per animation frame; 4096 units per turn
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_FAST           = 0x180,
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DIAMOND_RADIUS_SCALE      = 0xC0,
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_RADIUS_SCALE         = 0x300,
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_COLOR_FACTORS        = 0x10,  // Green only in the bit-disc format
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE         = 0x200,
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS        = 0x111, // Equal RGB factors
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_SIDE_BEAM_COLOR_FACTORS   = 0x210, // Red factor 2, green factor 1, blue factor 0
    SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSING_DISC_RADIUS_SCALE = 0x80,
};

/// Work block of the task that runs the parking lot's selector panel screen,
/// allocated zeroed by its first state and kept at `Task::work`.
///
/// The panel's idle state tests the cursor against the hotspot table and
/// latches the entry the player confirms in `choice` and `promptKind`; the
/// states after it open the command prompt for that entry and, when the player
/// accepts it, carry the choice out. A switch toggles its bit of the pending
/// pattern and returns to the idle state. The enter button commits a pattern
/// that differs from the current one, and the screen then leaves through a
/// fade to black that `fadeLevel` drives.
typedef struct {
    s32 field_0;      // Cleared when the task starts and never read; role unproven
    u8  unknown_4[4]; // Never read or written by the room; role unproven
    s16 fadeLevel;    // Darkness of the closing fade, drawn subtractively on all three channels (0 none, 0xFF black, where it ends)
    u8  unknown_A[2]; // Never read or written by the room; role unproven
    s16 choice;       // `ActionPromptHotspot::id` of the confirmed hotspot (8, 4, 2 or 1 the switch toggling that pattern bit, 16 the enter button)
    s8  promptKind;   // `ActionPromptHotspot::promptKind` of that hotspot, forwarded when its command prompt opens
    u8  unknown_F;    // Never read or written by the room; role unproven
} _ShelterB1UndergroundParkingPanelWork;
STATIC_ASSERT_SIZEOF(_ShelterB1UndergroundParkingPanelWork, 0x10);

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

extern void func_actor_161500_80131E38(void);

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
static ShopTier Shop_Data_80181950[SHOP_TIER_COUNT];

/// The shop list's row handlers and the balance panel beside it.
static UiListRowCallback Shop_Data_80181AD8[];
static UiObjectDesc      Shop_Data_80181BF4;

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
static const char                   Shop_Data_8017D6F4[];
static s32                          Shop_Data_80187628;
static const EquipmentWeaponSupply* Shop_Data_8018762C;

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

extern RoomCutsceneRecStorage D_shelter_b1_underground_parking_8018D75C;
extern TaskDesc               D_shelter_b1_underground_parking_80187200;
extern TaskDesc               D_shelter_b1_underground_parking_80187260[];
extern TaskDesc               D_shelter_b1_underground_parking_8018726C[];
extern ScreenFade             D_shelter_b1_underground_parking_8018D750;

/// The room's ambience table, one entry per view slot.
extern RoomAmbienceEntry D_shelter_b1_underground_parking_8018761C[];

extern EvsCommand D_shelter_b1_underground_parking_801872D8[];
extern EvsCommand D_shelter_b1_underground_parking_801873DC[];
extern EvsCommand D_shelter_b1_underground_parking_80187544[];

/// The task spawned from `D_shelter_b1_underground_parking_80187670`, and its
/// descriptor.
extern Task*    D_shelter_b1_underground_parking_8018D74C;
extern TaskDesc D_shelter_b1_underground_parking_80187670;

extern TaskMessageEntry D_shelter_b1_underground_parking_80187230[];

extern DVECTOR             D_shelter_b1_underground_parking_801876D4[];
extern u8                  D_shelter_b1_underground_parking_8018D788;
extern u8                  D_shelter_b1_underground_parking_8018D789;
extern TaskDesc            D_shelter_b1_underground_parking_80187664[];
extern ActionPromptHotspot D_shelter_b1_underground_parking_8018767C[];
extern u8                  D_shelter_b1_underground_parking_801876C4[];

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
static void _shelterB1UndergroundParkingPanelCommitSelection(Task* task);
static void func_shelter_b1_underground_parking_801847D0(Task* task);

static void func_shelter_b1_underground_parking_8018390C(void);
static void _shelterB1UndergroundParkingPanelResetSelection(void);
static void _shelterB1UndergroundParkingPanelApplySelection(void);
static void _shelterB1UndergroundParkingDrawPanelGlow(s16 screenX, s16 screenY, s16 pixelRadius, s16 packedRgb);
static void _shelterB1UndergroundParkingSetFastLightPulse(s16 fastPulse);

#define TELEPHONE_TITLE_BYTES "Telephone\0<\x9E"
#include "../../shared/telephone.h"

#define SHOP_CHARGE_TITLE_BYTES "Charge\0o"
#include "../../shared/shop.h"
#include "../../shared/cap_dialogue.h"

extern WorldCollisionGrid         D_shelter_b1_underground_parking_80187E50[1];
extern WorldCollisionGrid         D_shelter_b1_underground_parking_801884D4[1];
extern WorldCollisionGrid         D_shelter_b1_underground_parking_80188BC4[1];
extern WorldCollisionGrid         D_shelter_b1_underground_parking_8018912C[1];
extern WorldCollisionGrid         D_shelter_b1_underground_parking_80189754[1];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018B094[8];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018B2F4[8];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018B674[12];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018BA04[12];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018BD94[12];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018C124[12];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018C4B4[12];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018C844[20];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018CE34[16];
extern WorldCollisionTrigger      D_shelter_b1_underground_parking_8018D2F4[11];
extern WorldCoordRoomAmbientEntry D_shelter_b1_underground_parking_8018D638[25];
extern WorldCoordRoomLights       D_shelter_b1_underground_parking_8018B07C[1];
s32                               func_shelter_b1_underground_parking_80182830(Task*, s32, RoomEventMsg*, s32);
s32                               func_shelter_b1_underground_parking_80182A60(Task*, s32, s32, s32);
s32                               func_shelter_b1_underground_parking_80183284(Task*, s32, s32, s32);
s32                               func_shelter_b1_underground_parking_80183360(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32                        _shelterB1UndergroundParkingCapSoundCue(Task* roomTask, s32 messageId, s32 soundCue, s32 unused);
void                              func_shelter_b1_underground_parking_80182DB4(Task*);
static void                       _shelterB1UndergroundParkingAmbienceTask(Task* task);
void                              func_shelter_b1_underground_parking_801834D4(Task*);
void                              func_shelter_b1_underground_parking_80183560(Task*);
void                              func_shelter_b1_underground_parking_8018363C(Task*);
void                              func_shelter_b1_underground_parking_801836D8(Task*);
void                              func_shelter_b1_underground_parking_80183714(Task*);
static void                       _shelterB1UndergroundParkingSetRoom(u8 roomNumber);
static void                       _shelterB1UndergroundParkingSetPlayerTickHold(u8 holdPlayerTick);
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

ShelterB1UndergroundParkingShopSessionTaskDescStorage D_shelter_b1_underground_parking_801871F0 = { { { { TASK_BODY_NONE, 192 } }, Shop_SessionTask, { .value = 0 } }, { 0 } };

TaskDesc D_shelter_b1_underground_parking_80187200 = { { { TASK_BODY_NONE, 32 } }, roomDepartureTask, { .value = 0 } };

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_shelter_b1_underground_parking_80187230[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_underground_parking_80183360 },
    { 5105, func_shelter_b1_underground_parking_80183284 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_underground_parking_80182830 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_underground_parking_80182A60 },
    { ROOM_MESSAGE_SOUND, _shelterB1UndergroundParkingCapSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_underground_parking_80187260[1] = {
    { { { TASK_BODY_NONE, 32 } }, capDialogueLoopTask, { .value = 0 } },
};

TaskDesc D_shelter_b1_underground_parking_8018726C[7] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_801834D4, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_80183560, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_8018363C, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_80182DB4, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b1_underground_parking_801836D8, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _shelterB1UndergroundParkingAmbienceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_underground_parking_80183714, { .value = 0 } },
};

ActorTransform D_shelter_b1_underground_parking_801872C0 = { { 3155, 0, -247, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_shelter_b1_underground_parking_801872D8[10] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54140007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b1_underground_parking_801872C0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationPlayRequest D_shelter_b1_underground_parking_801873C8 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_shelter_b1_underground_parking_801873DC[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _shelterB1UndergroundParkingSetPlayerTickHold }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b1_underground_parking_801873C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54140003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54140009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _shelterB1UndergroundParkingSetRoom }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b1_underground_parking_80187544[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _shelterB1UndergroundParkingSetRoom }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
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

ActionPromptHotspot D_shelter_b1_underground_parking_8018767C[6] = {
    { -78, 77, 16, 16, 8, 1, 0 },
    { -48, 77, 16, 16, 4, 1, 0 },
    { -21, 77, 16, 16, 2, 1, 0 },
    { 3, 77, 16, 16, 1, 1, 0 },
    { 34, 77, 40, 16, SHELTER_B1_UNDERGROUND_PARKING_PANEL_HOTSPOT_ENTER, 1, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
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

WorldCoordRoomLighting D_shelter_b1_underground_parking_801877B4[8] = {
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
    { D_shelter_b1_underground_parking_8018B07C, D_shelter_b1_underground_parking_8018D638 },
};

WorldCollisionRoomResources D_shelter_b1_underground_parking_801877F4[8] = {
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
    gViewIdentityMap,
    D_shelter_b1_underground_parking_80187874,
    D_shelter_b1_underground_parking_8018788C,
    D_shelter_b1_underground_parking_801878A4,
    D_shelter_b1_underground_parking_801878BC,
    D_shelter_b1_underground_parking_801878D4,
    D_shelter_b1_underground_parking_801878EC,
    D_shelter_b1_underground_parking_80187904,
};

ViewCount D_shelter_b1_underground_parking_8018793C[8] = { 24, 24, 24, 24, 24, 24, 24, 24 };

DirectionWarpEntry D_shelter_b1_underground_parking_8018794C[2] = {
    { { { .word = 0 }, 1913, 0, -7180 }, { 0, 0, 0, 0 }, { { .word = 0 }, 1980, 0, -6660 }, { 0, 0, 0, 0 }, 0x54140002, 0x54140001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, 401, 0, -1200 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 401, 0, -1200 }, { 0, 0, 0, 0 }, 0x5414000A, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING },
};

static SVECTOR _gShelterB1UndergroundParkingCollision0A890Normals[6] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_normals.inc"
};

static SVECTOR _gShelterB1UndergroundParkingCollision0A890Verts[60] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_verts.inc"
};

static WorldCollisionGridFace _gShelterB1UndergroundParkingCollision0A890Faces[22] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_faces.inc"
};

static s16 _gShelterB1UndergroundParkingCollision0A890Cells[150] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1UndergroundParkingCollision0A890Cells[i])
static s16* _gShelterB1UndergroundParkingCollision0A890Table[20] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_underground_parking_80187E50[1] = {
    { NULL, _gShelterB1UndergroundParkingCollision0A890Normals, _gShelterB1UndergroundParkingCollision0A890Verts, _gShelterB1UndergroundParkingCollision0A890Faces, _gShelterB1UndergroundParkingCollision0A890Table, 5390, 8000, 5, 4, 4000, 22 },
};

static SVECTOR _gShelterB1UndergroundParkingCollision0AF14Normals[8] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_normals.inc"
};

static SVECTOR _gShelterB1UndergroundParkingCollision0AF14Verts[84] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_verts.inc"
};

static WorldCollisionGridFace _gShelterB1UndergroundParkingCollision0AF14Faces[33] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_faces.inc"
};

static s16 _gShelterB1UndergroundParkingCollision0AF14Cells[210] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1UndergroundParkingCollision0AF14Cells[i])
static s16* _gShelterB1UndergroundParkingCollision0AF14Table[20] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_underground_parking_801884D4[1] = {
    { NULL, _gShelterB1UndergroundParkingCollision0AF14Normals, _gShelterB1UndergroundParkingCollision0AF14Verts, _gShelterB1UndergroundParkingCollision0AF14Faces, _gShelterB1UndergroundParkingCollision0AF14Table, 5390, 8000, 5, 4, 4000, 33 },
};

static SVECTOR _gShelterB1UndergroundParkingCollision0B604Normals[8] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_normals.inc"
};

static SVECTOR _gShelterB1UndergroundParkingCollision0B604Verts[90] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_verts.inc"
};

static WorldCollisionGridFace _gShelterB1UndergroundParkingCollision0B604Faces[36] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_faces.inc"
};

static s16 _gShelterB1UndergroundParkingCollision0B604Cells[222] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1UndergroundParkingCollision0B604Cells[i])
static s16* _gShelterB1UndergroundParkingCollision0B604Table[20] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_underground_parking_80188BC4[1] = {
    { NULL, _gShelterB1UndergroundParkingCollision0B604Normals, _gShelterB1UndergroundParkingCollision0B604Verts, _gShelterB1UndergroundParkingCollision0B604Faces, _gShelterB1UndergroundParkingCollision0B604Table, 5390, 8000, 5, 4, 4000, 36 },
};

static SVECTOR _gShelterB1UndergroundParkingCollision0BB6CNormals[6] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_normals.inc"
};

static SVECTOR _gShelterB1UndergroundParkingCollision0BB6CVerts[68] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_verts.inc"
};

static WorldCollisionGridFace _gShelterB1UndergroundParkingCollision0BB6CFaces[27] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_faces.inc"
};

static s16 _gShelterB1UndergroundParkingCollision0BB6CCells[176] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1UndergroundParkingCollision0BB6CCells[i])
static s16* _gShelterB1UndergroundParkingCollision0BB6CTable[20] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_underground_parking_8018912C[1] = {
    { NULL, _gShelterB1UndergroundParkingCollision0BB6CNormals, _gShelterB1UndergroundParkingCollision0BB6CVerts, _gShelterB1UndergroundParkingCollision0BB6CFaces, _gShelterB1UndergroundParkingCollision0BB6CTable, 5390, 8000, 5, 4, 4000, 27 },
};

static SVECTOR _gShelterB1UndergroundParkingCollision0C194Normals[8] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_normals.inc"
};

static SVECTOR _gShelterB1UndergroundParkingCollision0C194Verts[83] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_verts.inc"
};

static WorldCollisionGridFace _gShelterB1UndergroundParkingCollision0C194Faces[30] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_faces.inc"
};

static s16 _gShelterB1UndergroundParkingCollision0C194Cells[186] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1UndergroundParkingCollision0C194Cells[i])
static s16* _gShelterB1UndergroundParkingCollision0C194Table[20] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_underground_parking_80189754[1] = {
    { NULL, _gShelterB1UndergroundParkingCollision0C194Normals, _gShelterB1UndergroundParkingCollision0C194Verts, _gShelterB1UndergroundParkingCollision0C194Faces, _gShelterB1UndergroundParkingCollision0C194Table, 5390, 8000, 5, 4, 4000, 30 },
};

ViewCamera D_shelter_b1_underground_parking_80189778[24] = {
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

SpriteView D_shelter_b1_underground_parking_8018AB9C[24] = {
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

WorldCoordRoomLights D_shelter_b1_underground_parking_8018B07C[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_b1_underground_parking_8018ACBC), D_shelter_b1_underground_parking_8018ACBC, 0, NULL },
};

WorldCollisionTrigger D_shelter_b1_underground_parking_8018B094[8] = {
    { NULL, NULL, NULL, { 608, -3744, -3072, 0 }, { { -4222, -4192, -311, 0 }, { 4204, -4192, 298, 0 }, { -4222, 4192, -311, 0 }, { 4204, 4192, 298, 0 } }, { 295, 0, -4093, 0 }, { 0, 0, 4096, 0 }, 5948, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 656, -3776, -3391, 0 }, { { 4534, -4192, 308, 0 }, { -4560, -4192, -330, 0 }, { 4534, 4192, 308, 0 }, { -4560, 4192, -330, 0 } }, { -287, 0, 4085, 0 }, { 0, 0, 4096, 0 }, 6186, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2384, -3648, 2848, 0 }, { { -4080, -4192, -1088, 0 }, { 4080, -4192, 1088, 0 }, { -4080, 4192, -1088, 0 }, { 4080, 4192, 1088, 0 } }, { 1056, 0, -3964, 0 }, { 0, 0, 4096, 0 }, 5948, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -448, -3616, -192, 0 }, { { 5232, -4192, 2624, 0 }, { -5232, -4192, -2624, 0 }, { 5232, 4192, 2624, 0 }, { -5232, 4192, -2624, 0 } }, { -1842, 0, 3672, 0 }, { 0, 0, 4096, 0 }, 7186, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -512, -3616, -90, 0 }, { { -5296, -4192, -2634, 0 }, { 5296, -4192, 2646, 0 }, { -5296, 4192, -2646, 0 }, { 5296, 4192, 2634, 0 } }, { 1833, -6, -3678, 0 }, { 0, 0, 4096, 0 }, 7240, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2590, -3616, 2672, 0 }, { { 3440, -4192, 944, 0 }, { -3440, -4192, -944, 0 }, { 3440, 4192, 944, 0 }, { -3440, 4192, -944, 0 } }, { -1085, 0, 3951, 0 }, { 0, 0, 4096, 0 }, 5490, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4176, -3552, -80, 0 }, { { 32, -4192, -2864, 0 }, { -32, -4192, 2864, 0 }, { 32, 4192, -2864, 0 }, { -32, 4192, 2864, 0 } }, { 4095, 0, 45, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4288, -3520, -191, 0 }, { { -32, -4192, 2480, 0 }, { 32, -4192, -2480, 0 }, { -32, 4192, 2480, 0 }, { 32, 4192, -2480, 0 } }, { -4103, 0, -53, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_underground_parking_8018B2F4[8] = {
    { NULL, NULL, NULL, { 774, -3744, -5703, 0 }, { { -1271, -4192, -842, 0 }, { 1272, -4192, 842, 0 }, { -1271, 4192, -842, 0 }, { 1272, 4192, 842, 0 } }, { 2265, 0, -3423, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 2, 17, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 799, -3552, -5824, 0 }, { { 1272, -4192, 842, 0 }, { -1271, -4192, -842, 0 }, { 1272, 4192, 842, 0 }, { -1271, 4192, -842, 0 } }, { -2267, 0, 3422, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 17, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3198, -3648, -5760, 0 }, { { -1221, -4192, 914, 0 }, { 1222, -4192, -914, 0 }, { -1221, 4192, 914, 0 }, { 1222, 4192, -914, 0 } }, { -2461, 0, -3289, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 2, 17, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3230, -3712, -5888, 0 }, { { 1222, -4192, -914, 0 }, { -1221, -4192, 914, 0 }, { 1222, 4192, -914, 0 }, { -1221, 4192, 914, 0 } }, { 2459, 0, 3287, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 17, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2623, -3680, -1313, 0 }, { { -2005, -4192, 2, 0 }, { 2005, -4192, -1, 0 }, { -2005, 4192, 2, 0 }, { 2005, 4192, -1, 0 } }, { -4, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 17, 19, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2656, -3616, -1441, 0 }, { { 2005, -4192, -1, 0 }, { -2005, -4192, 2, 0 }, { 2005, 4192, -1, 0 }, { -2005, 4192, 2, 0 } }, { 3, 0, 4103, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 19, 17, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 591, -3712, -3985, 0 }, { { -178, -4192, -2803, 0 }, { 178, -4192, 2804, 0 }, { -178, 4192, -2803, 0 }, { 178, 4192, 2804, 0 } }, { 4089, 0, -260, 0 }, { 0, 0, 4096, 0 }, 5042, 0, 17, 18, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 704, -3680, -3969, 0 }, { { 178, -4192, 2804, 0 }, { -178, -4192, -2803, 0 }, { 178, 4192, 2804, 0 }, { -178, 4192, -2803, 0 } }, { -4090, 0, 259, 0 }, { 0, 0, 4096, 0 }, 5042, 0, 18, 17, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b1_underground_parking_8018B554[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_underground_parking_8018B560[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_shelter_b1_underground_parking_8018B570[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_underground_parking_8018B57C[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_shelter_b1_underground_parking_8018B58C[2] = {
    { 116, 615, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gStrideWalkTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_underground_parking_8018B5A4[2] = {
    { 116, 0, 0, 3700, 0, -114, 2488, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_underground_parking_8018B5C4[22] = {
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

WorldCollisionTrigger D_shelter_b1_underground_parking_8018B674[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4076, 0, -401, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4960, -64, 32, 0 }, { { -1568, 0, -2928, 0 }, { 1568, 0, -2928, 0 }, { -1568, 0, 2928, 0 }, { 1568, 0, 2928, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 3318, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3568, -64, 4064, 0 }, { { -624, 0, -720, 0 }, { 624, 0, -720, 0 }, { -624, 0, 720, 0 }, { 624, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 799, 0, -4017, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1712, -64, -4704, 0 }, { { -656, 0, -2288, 0 }, { 656, 0, -2288, 0 }, { -656, 0, 2288, 0 }, { 656, 0, 2288, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 2374, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -64, -2656, 0 }, { { -2080, 0, -560, 0 }, { 2080, 0, -560, 0 }, { -2080, 0, 560, 0 }, { 2080, 0, 560, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, 4096, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -160, -64, 4368, 0 }, { { -656, 0, -1376, 0 }, { 656, 0, -1376, 0 }, { -656, 0, 1376, 0 }, { 656, 0, 1376, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1519, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1936, -64, 3216, 0 }, { { -2272, 0, -496, 0 }, { 2272, 0, -496, 0 }, { -2272, 0, 496, 0 }, { 2272, 0, 496, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 2318, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2106, 0, 3512, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3040, -64, 4688, 0 }, { { -576, 0, -992, 0 }, { 576, 0, -992, 0 }, { -576, 0, 992, 0 }, { 576, 0, 992, 0 } }, { 0, 4095, 0, 0 }, { 3920, 0, -1189, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_underground_parking_8018BA04[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5536, -64, 32, 0 }, { { -1376, 0, -1360, 0 }, { 1376, 0, -1360, 0 }, { -1376, 0, 1360, 0 }, { 1376, 0, 1360, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 1932, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -64, 3936, 0 }, { { -688, 0, -720, 0 }, { 688, 0, -720, 0 }, { -688, 0, 720, 0 }, { 688, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { 798, 0, -4017, 0 }, 995, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3296, -64, -2784, 0 }, { { -1760, 0, -480, 0 }, { 1760, 0, -480, 0 }, { -1760, 0, 480, 0 }, { 1760, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1760, -64, -4448, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1792, -64, 3168, 0 }, { { -2208, 0, -480, 0 }, { 2208, 0, -480, 0 }, { -2208, 0, 480, 0 }, { 2208, 0, 480, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -192, -64, 4304, 0 }, { { 480, 0, -1296, 0 }, { 480, 0, 1296, 0 }, { -480, 0, -1296, 0 }, { -480, 0, 1296, 0 } }, { 0, 4117, 0, 0 }, { -4096, 0, 0, 0 }, 1378, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -1752, 0, 3703, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3024, -64, 4640, 0 }, { { -560, 0, -912, 0 }, { 560, 0, -912, 0 }, { -560, 0, 912, 0 }, { 560, 0, 912, 0 } }, { 0, 4096, 0, 0 }, { 3973, 0, -996, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_underground_parking_8018BD94[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4091, 0, 201, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8000, -64, 32, 0 }, { { -3616, 0, -2928, 0 }, { 3616, 0, -2928, 0 }, { -3616, 0, 2928, 0 }, { 3616, 0, 2928, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, 4096, 0 }, 4636, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -3424, -64, 3936, 0 }, { { -640, 0, -720, 0 }, { 640, 0, -720, 0 }, { -640, 0, 720, 0 }, { 640, 0, 720, 0 } }, { 0, 4102, 0, 0 }, { 601, 0, -4052, 0 }, 962, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3424, -64, -2816, 0 }, { { -1824, 0, -480, 0 }, { 1824, 0, -480, 0 }, { -1824, 0, 480, 0 }, { 1824, 0, 480, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1885, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1728, -64, -4176, 0 }, { { -480, 0, -1584, 0 }, { 480, 0, -1584, 0 }, { -480, 0, 1584, 0 }, { 480, 0, 1584, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 1654, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1792, -64, 3232, 0 }, { { -2208, 0, -480, 0 }, { 2208, 0, -480, 0 }, { -2208, 0, 480, 0 }, { 2208, 0, 480, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -208, -64, 4384, 0 }, { { -496, 0, -1344, 0 }, { 496, 0, -1344, 0 }, { -496, 0, 1344, 0 }, { 496, 0, 1344, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 1431, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2276, 0, 3405, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2976, -64, 4720, 0 }, { { -576, 0, -896, 0 }, { 576, 0, -896, 0 }, { -576, 0, 896, 0 }, { 576, 0, 896, 0 } }, { 0, 4095, 0, 0 }, { 4052, 0, -602, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_underground_parking_8018C124[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4017, 0, -799, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4688, -64, 32, 0 }, { { -816, 0, -624, 0 }, { 816, 0, -624, 0 }, { -816, 0, 624, 0 }, { 816, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3424, -64, 3936, 0 }, { { -640, 0, -720, 0 }, { 640, 0, -720, 0 }, { -640, 0, 720, 0 }, { 640, 0, 720, 0 } }, { 0, 4102, 0, 0 }, { 1380, 0, -3857, 0 }, 962, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3280, -64, -2752, 0 }, { { -1744, 0, -480, 0 }, { 1744, 0, -480, 0 }, { -1744, 0, 480, 0 }, { 1744, 0, 480, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 1805, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1632, -64, -4240, 0 }, { { -464, 0, -1616, 0 }, { 464, 0, -1616, 0 }, { -464, 0, 1616, 0 }, { 464, 0, 1616, 0 } }, { 0, 4107, 0, 0 }, { 4096, 0, 0, 0 }, 1678, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1776, -64, 3072, 0 }, { { -2176, 0, -480, 0 }, { 2176, 0, -480, 0 }, { -2176, 0, 480, 0 }, { 2176, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 2217, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -160, -64, 4608, 0 }, { { -464, 0, -1616, 0 }, { 464, 0, -1616, 0 }, { -464, 0, 1616, 0 }, { 464, 0, 1616, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 1678, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3552, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2276, 0, 3405, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3040, -64, 4656, 0 }, { { -544, 0, -896, 0 }, { 544, 0, -896, 0 }, { -544, 0, 896, 0 }, { 544, 0, 896, 0 } }, { 0, 4098, 0, 0 }, { 3784, 0, -1568, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_underground_parking_8018C4B4[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -3973, 0, -995, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -64, 3936, 0 }, { { -688, 0, -720, 0 }, { 688, 0, -720, 0 }, { -688, 0, 720, 0 }, { 688, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { 995, 0, -3973, 0 }, 995, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4896, -64, 64, 0 }, { { -596, 0, -1135, 0 }, { 588, 0, -1138, 0 }, { -589, 0, 1137, 0 }, { 595, 0, 1134, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3408, -64, -2752, 0 }, { { -1840, 0, -480, 0 }, { 1840, 0, -480, 0 }, { -1840, 0, 480, 0 }, { 1840, 0, 480, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1898, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1792, -64, -4352, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1680, -64, 3168, 0 }, { { -2112, 0, -480, 0 }, { 2112, 0, -480, 0 }, { -2112, 0, 480, 0 }, { 2112, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 2157, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -192, -64, 4432, 0 }, { { 480, 0, -1360, 0 }, { 480, 0, 1360, 0 }, { -480, 0, -1360, 0 }, { -480, 0, 1360, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1436, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3552, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -1752, 0, 3703, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2976, -64, 4720, 0 }, { { -608, 0, -928, 0 }, { 608, 0, -928, 0 }, { -608, 0, 928, 0 }, { 608, 0, 928, 0 } }, { 0, 4099, 0, 0 }, { 4051, 0, -601, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_underground_parking_8018C844[20] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4076, 0, -401, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 112, -64, 1120, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3936, -64, 4832, 0 }, { { -1024, 0, -1648, 0 }, { 1728, 0, -1648, 0 }, { -1024, 0, 464, 0 }, { 1728, 0, 464, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 2387, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 96, -64, -1120, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2064, -64, -48, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2256, -64, -48, 0 }, { { -368, 0, -1392, 0 }, { 368, 0, -1392, 0 }, { -368, 0, 1392, 0 }, { 368, 0, 1392, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1436, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3808, -64, -368, 0 }, { { -368, 0, -2176, 0 }, { 368, 0, -2176, 0 }, { -368, 0, 2176, 0 }, { 368, 0, 2176, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 2202, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3776, -64, 3776, 0 }, { { -1376, 0, -592, 0 }, { 1376, 0, -592, 0 }, { -1376, 0, 592, 0 }, { 1376, 0, 592, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2880, -64, 4480, 0 }, { { -672, 0, -1328, 0 }, { 672, 0, -1328, 0 }, { -672, 0, 1328, 0 }, { 672, 0, 1328, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 1487, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3840, -64, 3696, 0 }, { { -1376, 0, -1216, 0 }, { 1376, 0, -768, 0 }, { -1376, 0, 992, 0 }, { 1376, 0, 992, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1832, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2576, -64, 4448, 0 }, { { -1008, 0, -1328, 0 }, { 1008, 0, -1328, 0 }, { -1008, 0, 1328, 0 }, { 1008, 0, 1328, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1664, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3937, -64, 4832, 0 }, { { -1024, 0, -1904, 0 }, { 2368, 0, -1904, 0 }, { -1024, 0, 848, 0 }, { 2368, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 3029, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1760, -64, -4512, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3840, -64, -2752, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -256, -64, 4384, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2112, -64, 3232, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -1380, 0, 3856, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_underground_parking_8018CE34[16] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4052, 0, -601, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 112, -64, 1120, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3424, -64, 3936, 0 }, { { -640, 0, -720, 0 }, { 640, 0, -720, 0 }, { -640, 0, 720, 0 }, { 640, 0, 720, 0 } }, { 0, 4102, 0, 0 }, { 1751, 0, -3703, 0 }, 962, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 96, -64, -1120, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2064, -64, -48, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2256, -64, -48, 0 }, { { -368, 0, -1392, 0 }, { 368, 0, -1392, 0 }, { -368, 0, 1392, 0 }, { 368, 0, 1392, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1436, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4096, -64, -2848, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2112, -64, 3168, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1728, -64, -4192, 0 }, { { -368, 0, -1392, 0 }, { 368, 0, -1392, 0 }, { -368, 0, 1392, 0 }, { 368, 0, 1392, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1436, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -224, -64, 4480, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -4096, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3776, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2106, 0, 3513, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -192, 0 }, { { -400, 0, -2608, 0 }, { 400, 0, -2608, 0 }, { -400, 0, 2608, 0 }, { 400, 0, 2608, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 2635, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3008, -64, 4688, 0 }, { { -512, 0, -960, 0 }, { 512, 0, -960, 0 }, { -512, 0, 960, 0 }, { 512, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { 3856, 0, -1381, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_underground_parking_8018D2F4[11] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2496, -64, -240, 0 }, { { -608, 0, -848, 0 }, { 608, 0, -848, 0 }, { -608, 0, 848, 0 }, { 608, 0, 848, 0 } }, { 0, 4094, 0, 0 }, { 4076, 0, 401, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3808, -64, 3936, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1553, -64, -2208, 0 }, { { -624, 0, -928, 0 }, { 624, 0, -928, 0 }, { -624, 0, 928, 0 }, { 624, 0, 928, 0 } }, { 0, 4120, 0, 0 }, { 4096, 0, 0, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_CAP, 23, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3503, -64, -688, 0 }, { { -624, 0, -432, 0 }, { 624, 0, -432, 0 }, { -624, 0, 432, 0 }, { 624, 0, 432, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -4096, 0 }, 757, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1632, -64, -4320, 0 }, { { -624, 0, -928, 0 }, { 624, 0, -928, 0 }, { -624, 0, 928, 0 }, { 624, 0, 928, 0 } }, { 0, 4120, 0, 0 }, { 4096, 0, 0, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2720, -64, 864, 0 }, { { -624, 0, -832, 0 }, { 624, 0, -832, 0 }, { -624, 0, 832, 0 }, { 624, 0, 832, 0 } }, { 0, 4095, 0, 0 }, { -201, 0, -4091, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, 48, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3936, -64, 416, 0 }, { { -1616, 0, -1568, 0 }, { 240, 0, -1568, 0 }, { -1616, 0, -160, 0 }, { 240, 0, -160, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2848, -64, -288, 0 }, { { -496, 0, -496, 0 }, { 496, 0, -496, 0 }, { -496, 0, 272, 0 }, { 496, 0, 272, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
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

static const EquipmentWeaponSupply* Shop_Data_8018762C = NULL;

Task* D_shelter_b1_underground_parking_8018D74C = NULL;

ScreenFade D_shelter_b1_underground_parking_8018D750 = { 0 };

Task* gRoomCutsceneSoundTask = NULL;

s32 D_shelter_b1_underground_parking_8018D758 = 0;

RoomCutsceneRecStorage D_shelter_b1_underground_parking_8018D75C = { { 0 }, { 0 } };

RoomDeparture gRoomDeparture = { 0, 0, 0, 0, 0, { 0, 0 }, 0 };

u8 D_shelter_b1_underground_parking_8018D788 = 0;

u8 D_shelter_b1_underground_parking_8018D789 = 0;

u16 D_shelter_b1_underground_parking_8018D78A = 0xCCEE;

u16 D_shelter_b1_underground_parking_8018D78C = 0;

static inline s32 Shop_AddItemCount(s32 item, s32 count);
static void       func_shelter_b1_underground_parking_801826C0(Task* roomTask);
static void       _shelterB1UndergroundParkingDrawPanelIndicators(void);

#include "../../shared/telephone.inc.c"

static void _glowDrawBeam(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);
static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

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
/// below -0x1266 and Z inside [-0x7CF, 0x7D0), with `Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL` and
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

    task  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor = (GameActor*)task->work;
    coord = task->extra.tmd->coords;
    if ((actor->mode != GAME_ACTOR_MODE_SCRIPTED) && (capIsBusy() == 0) && (gGameSession->location.loc.room >= 7) &&
        (coord->coord.t[0] < -0x1266)) {
        z = coord->coord.t[2];
        if (z < 0x7D0) {
            if ((z >= -0x7CF) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                facing = (u16)actor->rotation.vy & 0xFFF;
                if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP) != 0) {
                    if ((u32)(facing - 0xA01) < 0x3FFU) {
                        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                        capStartSequenceSlot(0xA, 0, 1);
                        taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                    }
                }
                if ((padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_DOWN) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    capStartSequenceSlot(0xA, 0, 1);
                    taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                }
            }
        }
    }
}

/// The three states of the room's main task, run by
/// `shelterB1UndergroundParkingRoomTask`: set-up, the per-frame
/// handler, and the kill.
static const TaskFuncTable3 D_shelter_b1_underground_parking_8017D7F4 = {
    {
        func_shelter_b1_underground_parking_80183810,
        func_shelter_b1_underground_parking_801826C0,
        taskKill,
    },
};

/// Room event handler keyed on `msg->field_2`: 1 calls `func_actor_161500_80131E38` in
/// place 0x15, 0xA starts caption slot 0xA and sets nibble 0x1B4 to 2 while
/// the room is below 7, and 0xB / 0xC pick a caption or spawn per room.
s32 func_shelter_b1_underground_parking_80182830(Task* task, s32 msgId, RoomEventMsg* msg, s32 arg3)
{
    if (msg->warp == 1 && gGameSession->location.loc.variant == 0x15) {
        func_actor_161500_80131E38();
    }
    if (msg->warp == 0xA) {
        if ((u8)msg->room == 1 && gGameSession->location.loc.room < 7) {
            capStartSequenceSlot(0xA, 1, 0);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING, 2);
        }
    }
    if (msg->warp == 0xB) {
        switch (gGameSession->location.loc.room) {
            case 2:
                capRunCommandWithTransition(8);
                break;
            case 3:
                capRunCommandWithTransition(0xF);
                break;
            case 4:
                capRunCommandWithTransition(9);
                break;
            case 5:
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                capRunCommandWithTransition(0xC);
                taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 1, 0, 0);
                break;
            case 6:
            case 7:
            case 8:
                capRunCommandWithTransition(0xE);
                break;
        }
    }
    if (msg->warp == 0xC) {
        switch (gGameSession->location.loc.room) {
            case 6:
                capStartSequenceSlot(0xB, 1, 0);
                break;
            case 7:
                if (D_shelter_b1_underground_parking_8018D758 != 0) {
                    capRunCommandWithTransition(0x1E);
                } else if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < 6) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    capStartSequenceSlot(0xB, 1, 1);
                    taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 3, 0, 0);
                } else {
                    capStartSequenceSlot(0xB, 1, 2);
                }
                break;
            case 8:
                capStartSequenceSlot(0xB, 1, 2);
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
                if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) == 0) {
                    capRunCommandWithTransition(1);
                } else if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_0E7) == 0) {
                    if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_0E8) == 0) {
                        capRunCommandWithTransition(2);
                    } else {
                        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
                        taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                    }
                } else {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
                    taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                }
            } else {
                capRunCommandWithTransition(0x10);
            }
            break;
        case 7:
            switch (gGameSession->location.loc.room) {
                case 1:
                    capRunCommandWithTransition(7);
                    break;
                case 2:
                    capRunCommandWithTransition(8);
                    break;
                case 3:
                    capRunCommandWithTransition(0xF);
                    break;
                case 4:
                    capRunCommandWithTransition(9);
                    break;
                case 6:
                case 7:
                case 8:
                    capRunCommandWithTransition(0xE);
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
                    capRunCommandWithTransition(4);
                    break;
                case 6:
                    capRunCommandWithTransition(5);
                    break;
                case 7:
                    capRunCommandWithTransition(6);
                    break;
                case 8:
                    capRunCommandWithTransition(0x10);
                    break;
            }
            break;
        case 2:
        case 3:
        case 5:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, arg2, 0);
            break;
        case 13:
            st                  = &D_shelter_b1_underground_parking_8018D75C.rec;
            st->startSound      = 0x5414000B;
            st->endSound        = 0x5414000E;
            st->sceneSound      = 0x5414000C;
            st->afterSceneSound = 0x5414000D;
            st->view            = 0x14;
            if (D_shelter_b1_underground_parking_8018D758 == 0) {
                if (gameFlagGetNibble(GAME_FLAG_UNDERGROUND_PARKING_FIRST_SCENE) == 0) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    gameFlagSetNibble(GAME_FLAG_UNDERGROUND_PARKING_FIRST_SCENE, 1);
                    taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 1, 0);
                } else {
                    st->capSlot   = 1;
                    st->capFile   = 1;
                    st->skipScene = 0;
                    taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 9, st);
                }
            } else {
                D_shelter_b1_underground_parking_8018D758 = 0;
                st->capSlot                               = 0x1F;
                st->capFile                               = 0;
                st->skipScene                             = 1;
                taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, arg2, st);
            }
            break;
        case 22:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_shelter_b1_underground_parking_80187260, 0, arg2, 0);
            break;
        case 48:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 4, 0);
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
    RoomDeparture       rec;
    RoomEventMsg        msg;
    RoomDeparture*      p;
    RoomVariantResolver handler;

    switch (task->state) {
        case 0:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case 1:
            if (capGetVariantKey() == 0xB) {
                D_shelter_b1_underground_parking_8018D750.blend      = SCREEN_FADE_SUBTRACT;
                D_shelter_b1_underground_parking_8018D750.phase      = SCREEN_FADE_RUNNING;
                D_shelter_b1_underground_parking_8018D750.rampFrames = 0x1E;
                taskSpawn(1, 0x31, 0, &D_shelter_b1_underground_parking_8018D750);
                task->killCountdown = 0x1E;
                task->state++;
            } else {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
        case 2:
            if (task->killCountdown == 0) {
                if (gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE) == 0xA) {
                    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 9);
                }
                if (gameFlagGetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE) == 1) {
                    gameFlagSetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE, 2);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x1B;
                }
                handler      = _roomVariantResolveNeoArk;
                rec.stage    = GAME_STAGE_SHELTER_NEO_ARK;
                rec.area     = GAME_AREA_SHELTER_1F_PARKING_GARAGE;
                rec.room     = 1;
                rec.warp     = 1;
                rec.sndEvent = 0x54140008;
                rec.facing   = ROOM_DEPARTURE_SKIP_FACING;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
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
                taskSpawnFromTable(&D_shelter_b1_underground_parking_80187200, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

/// Runs the room's view-dependent ambience loop while its event is active.
///
/// Starts at state 0 with no work or body. View slots 0..8 supply sound-script
/// pan offsets and halved attenuation; other slots use zero for both. A view
/// change advances through three delay states before retuning the playing loop.
/// Clearing the event latch stops it with its existing ADSR release, restores
/// the slow light pulse and kills this task.
static void _shelterB1UndergroundParkingAmbienceTask(Task* task)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_SOUND         = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x0F),
        SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_START   = 0,
        SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_WAIT    = 1,
        SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_DELAY_1 = 2,
        SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_DELAY_2 = 3,
        SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_DELAY_3 = 4,
        SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_RETUNE  = 5,
    };

    s32 panOffset;
    s32 attenuation;
    u8  viewIndex;

    /// Gets the view slot's pan offset and halved attenuation in sound-script units.
    ///
    /// Captures this room's ambience table. `index` must be a side-effect-free
    /// unsigned-byte expression; it is evaluated up to three times. Outputs
    /// must be distinct writable s32 lvalues, each assigned once. Unlisted
    /// slots use the base pan and full level. Expands to a compound statement;
    /// use as a standalone statement. Local to this task; undefined below.
#define SHELTER_B1_UNDERGROUND_PARKING_GET_VIEW_AMBIENCE_MIX(index, outPanOffset, outAttenuation)  \
    {                                                                                              \
        if ((index) < ARRAY_SIZE(D_shelter_b1_underground_parking_8018761C)) {                     \
            (outPanOffset)   = D_shelter_b1_underground_parking_8018761C[(index)].panOffset;       \
            (outAttenuation) = D_shelter_b1_underground_parking_8018761C[(index)].attenuation / 2; \
        } else {                                                                                   \
            (outPanOffset)   = 0;                                                                  \
            (outAttenuation) = 0;                                                                  \
        }                                                                                          \
    }

    viewIndex = gGameSession->location.loc.view;
    SHELTER_B1_UNDERGROUND_PARKING_GET_VIEW_AMBIENCE_MIX(viewIndex, panOffset, attenuation);
#undef SHELTER_B1_UNDERGROUND_PARKING_GET_VIEW_AMBIENCE_MIX

    switch (task->state) {
        case SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_START:
            sndEvtRequestScriptStart(SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_SOUND, (s8)panOffset, (s8)attenuation);
            task->state = task->state + 1;
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_WAIT:
            if (D_shelter_b1_underground_parking_8018D758 == 0) {
                _shelterB1UndergroundParkingSetFastLightPulse(SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_SLOW);
                sndEvtRequestScriptStop(SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(task);
                break;
            }
            // The live save still carries the requested view during a transition.
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != gGameSession->location.loc.view) {
                task->state = task->state + 1;
            }
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_DELAY_1:
        case SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_DELAY_2:
        case SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_DELAY_3:
            task->state = task->state + 1;
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_RETUNE:
            sndEvtRequestScriptMix(SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_SOUND, (s8)panOffset, (s8)attenuation);
            task->state = SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_WAIT;
            break;
    }
}

/// Binds the Neo Ark room-variant definition to this room's private resolver.
///
/// Supply one function identifier with the `RoomVariantResolver` signature;
/// its prologue declaration establishes static linkage. Keep the binding
/// through the fragment, then undefine it. No arguments, runtime evaluation,
/// captured values, stringification or token pasting are involved.
#define ROOM_VARIANT_RESOLVE_NEO_ARK _roomVariantResolveNeoArk
#include "../../shared/room_variants_neo_ark.inc.c"
#undef ROOM_VARIANT_RESOLVE_NEO_ARK

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
        _shelterB1UndergroundParkingPanelCommitSelection,
        func_shelter_b1_underground_parking_801847D0,
    },
};

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Tests whether any linked room-event region has a latched hit.
///
/// Borrows the null-terminated pending trigger list without clearing hits or
/// unlinking entries. Returns 1 for a hit on an unflagged room-action trigger
/// with the room-event region ID, otherwise 0.
static inline s32 _shelterB1UndergroundParkingRoomTriggerHit(void)
{
    WorldCollisionTrigger* trigger;

    for (trigger = Gp_PendingObj4C; trigger != NULL; trigger = trigger->next) {
        if (trigger->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && trigger->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && trigger->hit != 0) {
            return 1;
        }
    }
    return 0;
}

s32 func_shelter_b1_underground_parking_80183284(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0x121 || arg2 == 0x122) {
        if (gGameSession->location.loc.room == 6) {
            if (_shelterB1UndergroundParkingRoomTriggerHit()) {
                taskSpawnFromTableOnDefaultList(D_shelter_b1_underground_parking_8018726C, 2, 0, 0);
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
    mapShelterRoomVariantResolve(in, out);
    if (D_shelter_b1_underground_parking_8018D758 == 0) {
        return 1;
    }
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        capRunCommandWithTransition(0x1E);
    }
    return 2;
}

/// Plays this room's sound for CAP cue 99 and ignores other sound cues.
///
/// Handles `ROOM_MESSAGE_SOUND`; only the first payload word is used. The
/// receiver, message ID and second payload are unused, and the result is zero.
static s32 _shelterB1UndergroundParkingCapSoundCue(Task* roomTask, s32 messageId, s32 soundCue, s32 unused)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_CAP_SOUND_CUE_99 = 0x63,
        SHELTER_B1_UNDERGROUND_PARKING_CAP_SOUND        = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x10),
    };

    if (soundCue == SHELTER_B1_UNDERGROUND_PARKING_CAP_SOUND_CUE_99) {
        sndEvtRequestScriptStart(SHELTER_B1_UNDERGROUND_PARKING_CAP_SOUND, 0, 0);
    }
    return 0;
}

#include "../../shared/cap_dialogue_loop.inc.c"

/// Spawns entry 0 of `D_shelter_b1_underground_parking_80187670` and kills
/// itself once that task has gone.
void func_shelter_b1_underground_parking_801834D4(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            D_shelter_b1_underground_parking_8018D74C = taskSpawnFromTable(&D_shelter_b1_underground_parking_80187670, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (taskPollKill(D_shelter_b1_underground_parking_8018D74C, &poll) != 0) {
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
            if (capIsBusy() == 0) {
                arg0->state += 1;
            }
            return;
        case 1:
            if (capGetVariantKey() == 0xB) {
                gGameSession->location.loc.room                            = 6;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 6;
                gGameSession->roomObjsDirty                                = state;
                evsStartScript(D_shelter_b1_underground_parking_801872D8, EVENT_SCRIPT_HUD_KEEP);
                gameFlagSetNibble(GAME_FLAG_UNDERGROUND_PARKING_STATE, 1);
                itemSetIdentified(0x123, 1);
            } else {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
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
        evsStartScriptWithSkip(D_shelter_b1_underground_parking_801873DC, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_shelter_b1_underground_parking_80187544);
        gameFlagSetNibble(GAME_FLAG_UNDERGROUND_PARKING_STATE, 2);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING, 0);
        arg0->state += 1;
        return;
    }
    taskKill(arg0);
}

void func_shelter_b1_underground_parking_801836D8(Task* arg0)
{
    if (capIsBusy() == 0) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        taskKill(arg0);
    }
}

void func_shelter_b1_underground_parking_80183714(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            capSelectLoadedFile(2);
            capSetTexturePage(0x300, 0);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            capReset();
            taskKill(task);
            break;
    }
}

/// Selects the area's room and requests deferred object relinking and view refresh.
///
/// `roomNumber` must be a valid one-based room in this area's resources. Updates
/// both the session location and the live save location; does not load an area.
static void _shelterB1UndergroundParkingSetRoom(u8 roomNumber)
{
    gGameSession->location.loc.room                            = roomNumber;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = roomNumber;
    gGameSession->roomObjsDirty                                = 1;
    gGameSession->viewDirty                                    = 1;
}

/// Sets the player actor tick hold from a room script's byte argument.
///
/// Zero resumes the actor tick; any nonzero byte suspends it. The room's scene
/// script passes zero after hiding the display so scripted movement can resume.
static void _shelterB1UndergroundParkingSetPlayerTickHold(u8 holdPlayerTick)
{
    D_80115768 = holdPlayerTick;
}

static void func_shelter_b1_underground_parking_80183810(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_underground_parking_80187230;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    _shelterB1UndergroundParkingPanelResetSelection();
    if (gGameSession->location.loc.variant == 0x15) {
        sceneSetPlacedActorDrawMode(0, 1);
    }
    if ((gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= 6) && (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_123) == 0)) {
        gameFlagSetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_123, 1);
        func_shelter_b1_underground_parking_8018390C();
    }
    arg0->state = arg0->state + 1;
}

void shelterB1UndergroundParkingRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b1_underground_parking_8017D7F4;
    states.funcs[task->state](task);
}

static void func_shelter_b1_underground_parking_8018390C(void)
{
    if (D_shelter_b1_underground_parking_8018D758 == 0) {
        D_shelter_b1_underground_parking_8018D758 = 1;
        _shelterB1UndergroundParkingSetFastLightPulse(SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_FAST);
        taskSpawnFromTable(D_shelter_b1_underground_parking_8018726C, 5, 0, 0);
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

/// Draws the panel diagram's fixed dim nodes and pending selection marker.
///
/// Requires a pending four-switch pattern in 0..15 and the current frame's
/// packet arena and ordering table. Coordinates are pixels relative to the
/// GPU draw offset; queued packets remain borrowed until GPU completion.
static inline void _shelterB1UndergroundParkingDrawPanelMapMarkers(void)
{
    _shelterB1UndergroundParkingDrawPanelGlow(0x11, -0x3D, SHELTER_B1_UNDERGROUND_PARKING_PANEL_MAP_NODE_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DIM_GREY);
    _shelterB1UndergroundParkingDrawPanelGlow(0x3D, -0x3D, SHELTER_B1_UNDERGROUND_PARKING_PANEL_MAP_NODE_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DIM_GREY);
    _shelterB1UndergroundParkingDrawPanelGlow(0x10, 9, SHELTER_B1_UNDERGROUND_PARKING_PANEL_MAP_NODE_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DIM_GREY);
    _shelterB1UndergroundParkingDrawPanelGlow(-0x42, -0x19, SHELTER_B1_UNDERGROUND_PARKING_PANEL_MAP_NODE_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DIM_GREY);
    _shelterB1UndergroundParkingDrawPanelGlow(-0x17, 0x2B, SHELTER_B1_UNDERGROUND_PARKING_PANEL_MAP_NODE_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DIM_GREY);
    _shelterB1UndergroundParkingDrawPanelGlow(0x3D, 0x2B, SHELTER_B1_UNDERGROUND_PARKING_PANEL_MAP_NODE_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DIM_GREY);
    _shelterB1UndergroundParkingDrawPanelGlow(
        D_shelter_b1_underground_parking_801876D4[D_shelter_b1_underground_parking_8018D789].vx,
        D_shelter_b1_underground_parking_801876D4[D_shelter_b1_underground_parking_8018D789].vy,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_GREEN);
}

/// Draws the pending panel pattern's four switch lamps and map indicators.
///
/// Each set bit lights its switch in red, green, blue or yellow; the complete
/// pattern selects one of sixteen green marker positions. Requires a pattern
/// in 0..15, current frame packet space and a depth ordering table. Uses panel
/// pixel coordinates without view projection, borrowing packets until GPU completion.
static void _shelterB1UndergroundParkingDrawPanelIndicators(void)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_RED    = 8,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_GREEN  = 4,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_BLUE   = 2,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_YELLOW = 1,
    };

    if (D_shelter_b1_underground_parking_8018D789 & SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_RED) {
        _shelterB1UndergroundParkingDrawPanelGlow(-0x46, 0x54, SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_RED);
    }
    if (D_shelter_b1_underground_parking_8018D789 & SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_GREEN) {
        _shelterB1UndergroundParkingDrawPanelGlow(-0x28, 0x54, SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_GREEN);
    }
    if (D_shelter_b1_underground_parking_8018D789 & SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_BLUE) {
        _shelterB1UndergroundParkingDrawPanelGlow(-0xE, 0x54, SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_BLUE);
    }
    if (D_shelter_b1_underground_parking_8018D789 & SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_YELLOW) {
        _shelterB1UndergroundParkingDrawPanelGlow(0xC, 0x54, SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_YELLOW);
    }
    _shelterB1UndergroundParkingDrawPanelMapMarkers();
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
    _ShelterB1UndergroundParkingPanelWork* work;
    ActionPromptHotspot*                   hs;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                    = taskSpawnFromTable(D_shelter_b1_underground_parking_80187664, 0, 1, 0);
    task->work                                                 = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x15;
    /* The once-loops fold away, but flow weights the references inside them
       by loop depth. The outer one keeps the state load below the mode store;
       the inner one lifts the work pointer's global-alloc priority back above
       the parameter's so the two keep their callee-saved homes. */
    do {
        task->state++;
        do {
            work->field_0 = 0;
        } while (0);
    } while (0);
    displayAcquireMenuHold();
    for (hs = D_shelter_b1_underground_parking_8018767C; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

static void func_shelter_b1_underground_parking_801843F0(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    capRunCommand(gameFlagGetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_0E7) == 0 ? 2 : 3, CAP_PLAYBACK_IN_PLACE);
    task->state++;
}

static void func_shelter_b1_underground_parking_80184468(Task* task)
{
    ActionPrompt*                          prompt = D_80114D28;
    ActionPromptHotspot*                   hs     = D_shelter_b1_underground_parking_8018767C;
    _ShelterB1UndergroundParkingPanelWork* work   = task->work;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    gGameSession->hideHud = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
                if (hs->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->choice        = hs->id;
                    work->promptKind    = hs->promptKind;
                    task->state         = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = 5;
    }
}

static void func_shelter_b1_underground_parking_80184594(Task* task)
{
    ActionPrompt*                          prompt = D_80114D28;
    _ShelterB1UndergroundParkingPanelWork* work   = task->work;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

static void func_shelter_b1_underground_parking_801845F8(Task* task)
{
    ActionPrompt*                          prompt = D_80114D28;
    _ShelterB1UndergroundParkingPanelWork* work   = task->work;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        if (work->choice == SHELTER_B1_UNDERGROUND_PARKING_PANEL_HOTSPOT_ENTER) {
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_PARKING_PANEL_BUTTON, 0, 0);
            if (D_shelter_b1_underground_parking_8018D788 != D_shelter_b1_underground_parking_8018D789) {
                if (gGameSession->location.loc.room == 1) {
                    sndEvtRequestScriptStart(SOUND_SHELTER_B1_PARKING_PANEL_APPLY_R1, 0, 0);
                } else {
                    sndEvtRequestScriptStart(SOUND_SHELTER_B1_PARKING_PANEL_APPLY, 0, 0);
                }
                task->state = 6;
                return;
            }
        } else {
            D_shelter_b1_underground_parking_8018D789 ^= work->choice;
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_PARKING_PANEL_BUTTON, 0, 0);
            task->state = 2;
            return;
        }
    }
    task->state = 2;
}

static void func_shelter_b1_underground_parking_801846EC(Task* arg0)
{
    D_80114D08 = 0xA;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill(arg0->spawnArg2.pointer);
    taskRequestKill(arg0, 0);
}

/// Commits the selector panel's pending switch pattern and starts its closing fade.
///
/// Runs in panel state 6 with a live action-prompt child in `spawnArg2.pointer`.
/// Draws the newly committed indicators, selects the room resources, clears the
/// countdown and kills that child before advancing to state 7. The parent and
/// its work remain alive to draw the fade on following frames.
static void _shelterB1UndergroundParkingPanelCommitSelection(Task* task)
{
    D_shelter_b1_underground_parking_8018D788 = D_shelter_b1_underground_parking_8018D789;
    _shelterB1UndergroundParkingDrawPanelIndicators();
    _shelterB1UndergroundParkingPanelApplySelection();
    task->killCountdown = 0;
    taskKill(task->spawnArg2.pointer);
    task->state++;
}

static void func_shelter_b1_underground_parking_801847D0(Task* task)
{
    _ShelterB1UndergroundParkingPanelWork* work = task->work;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    work->fadeLevel += SHELTER_B1_UNDERGROUND_PARKING_PANEL_FADE_STEP;
    if (work->fadeLevel >= 0x100) {
        work->fadeLevel = 0xFF;
    }
    fadeDrawOverlay(work->fadeLevel, work->fadeLevel, work->fadeLevel, GPU_BLEND_SUBTRACT);
    if (work->fadeLevel == 0xFF) {
        D_80114D08 = 0xA;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        displayReleaseMenuHold();
        gGameSession->eventState                                   = 0;
        gGameSession->hideHud                                      = 0;
        gGameSession->cutsceneHold                                 = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
        taskRequestKill(task, 0);
    }
}

/// Resets the pending pattern to zero and marks the panel selection uncommitted.
///
/// The committed byte's 0xFF sentinel differs from every pending four-switch
/// pattern, so confirming even the initial zero pattern applies a selection.
static void _shelterB1UndergroundParkingPanelResetSelection(void)
{
    enum { SHELTER_B1_UNDERGROUND_PARKING_PANEL_SELECTION_UNCOMMITTED = 0xFF };

    D_shelter_b1_underground_parking_8018D788 = SHELTER_B1_UNDERGROUND_PARKING_PANEL_SELECTION_UNCOMMITTED;
    D_shelter_b1_underground_parking_8018D789 = 0;
}

#include "../../shared/action_prompt_reset.inc.c"

/// Selects the room resources mapped to the committed panel switch pattern.
///
/// The low four bits select one of sixteen entries, whose room values are
/// 2..5. Updates both the session and live save and requests object relinking;
/// this does not change the view or load another area.
static void _shelterB1UndergroundParkingPanelApplySelection(void)
{
    enum { SHELTER_B1_UNDERGROUND_PARKING_PANEL_PATTERN_MASK = 0xF };

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & SHELTER_B1_UNDERGROUND_PARKING_PANEL_PATTERN_MASK];
    gGameSession->location.loc.room                            = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & SHELTER_B1_UNDERGROUND_PARKING_PANEL_PATTERN_MASK];
    gGameSession->roomObjsDirty                                = 1;
}

#include "../../shared/action_prompt_hit_test.inc.c"

/// Queues the cyan diamond, green disc and grey beam shared by six mapped views.
///
/// Uses the room's slow/fast pulse selection for the diamond; the disc and
/// beam flicker on frame parity. Requires the current view, scratch stack,
/// packet arena and ordering table under the same contract as
/// `shelterB1UndergroundParkingDrawGlowsTask`. The beam borrows the final two
/// consecutive points of the room's thirteen-point light table.
static inline void _shelterB1UndergroundParkingDrawCommonViewGlows(void)
{
    if (D_shelter_b1_underground_parking_8018D78C != 0) {
        _glowDrawDiamond(D_shelter_b1_underground_parking_80187714,
                         SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_FAST,
                         SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DIAMOND_RADIUS_SCALE);
    } else {
        _glowDrawDiamond(D_shelter_b1_underground_parking_80187714,
                         SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_SLOW,
                         SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DIAMOND_RADIUS_SCALE);
    }
    _glowDrawBitDisc(D_shelter_b1_underground_parking_8018771C,
                     SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_RADIUS_SCALE,
                     SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_COLOR_FACTORS);
    _glowDrawBeam(&D_shelter_b1_underground_parking_8018771C[11],
                  SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, 0,
                  SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
}

void shelterB1UndergroundParkingDrawGlowsTask(Task* unused)
{
    u8 mappedViewIndex;

    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
        case 10:
            _shelterB1UndergroundParkingDrawCommonViewGlows();
            break;
        case 3:
        case 7:
        case 11:
        case 12:
            _shelterB1UndergroundParkingDrawCommonViewGlows();
            break;
        case 8:
        case 13:
            _glowDrawBeam(D_shelter_b1_underground_parking_801877A4,
                          SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, 0,
                          SHELTER_B1_UNDERGROUND_PARKING_LIGHT_SIDE_BEAM_COLOR_FACTORS);
            // These views also show all five beams drawn by views 4 and 14.
        case 4:
        case 14:
            _glowDrawBeam(&D_shelter_b1_underground_parking_8018771C[1], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(&D_shelter_b1_underground_parking_8018771C[3], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(&D_shelter_b1_underground_parking_8018771C[5], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(&D_shelter_b1_underground_parking_8018771C[7], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(&D_shelter_b1_underground_parking_8018771C[9], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, 0, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            break;
        case 9:
        case 15:
        case 22:
        case 24:
            _glowDrawBeam(&D_shelter_b1_underground_parking_80187784[0], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, 0, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(&D_shelter_b1_underground_parking_80187784[2], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            break;
        case 16:
            _glowDrawBitDisc(D_shelter_b1_underground_parking_8018771C, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_RADIUS_SCALE, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_COLOR_FACTORS);
            break;
        case 18:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                _glowDrawDiamond(D_shelter_b1_underground_parking_80187714, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_FAST, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DIAMOND_RADIUS_SCALE);
            } else {
                _glowDrawDiamond(D_shelter_b1_underground_parking_80187714, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_SLOW, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DIAMOND_RADIUS_SCALE);
            }
            break;
        case 20:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                _glowDrawPulsingDisc(D_shelter_b1_underground_parking_80187714, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_FAST, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSING_DISC_RADIUS_SCALE);
            } else {
                _glowDrawPulsingDisc(D_shelter_b1_underground_parking_80187714, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_SLOW, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSING_DISC_RADIUS_SCALE);
            }
            break;
    }
}

#include "../../shared/glow_draw_beam.inc.c"

#include "../../shared/glow_draw_bit_disc.inc.c"

/// Draws a flickering additive glow in the selector panel's pixel coordinates.
///
/// `packedRgb` holds red, green and blue nibbles in bits 8..11, 4..7 and 0..3.
/// Each is scaled by 16, with 12 added on odd animation frames. Three eight-wedge
/// discs use radii `pixelRadius`, twice it and four times it, halving the colour
/// bytes after each disc. Callers use positive pixel radii of 7 or 10; radius
/// updates retain signed-halfword narrowing. The centre coordinates are relative
/// to the current GPU draw offset, with no view projection or clipping.
///
/// Requires space for 24 `POLY_G4` packets and 24 blend commands in the current
/// frame arena, and a current depth ordering table. Packets are sorted at depth
/// 64 and remain borrowed until GPU completion.
static void _shelterB1UndergroundParkingDrawPanelGlow(s16 screenX, s16 screenY, s16 pixelRadius, s16 packedRgb)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DEPTH        = 64,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_LAYER_COUNT  = 3,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_FLICKER_STEP = 12,
    };

    POLY_G4* prim;
    s32      layerIndex;
    s32      sweepAngle;
    s32      halfStepAngle;
    s32      nextAngle;
    u8       red;
    u8       green;
    u8       blue;
    s32      flicker;
    s32      colorWord;
    s32      redComponent;
    s32      greenComponent;

    // Unpack the RGB nibbles before applying the frame-parity flicker.
    layerIndex     = 0;
    flicker        = (gDisplayState.animFrame & 1) * SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_FLICKER_STEP;
    colorWord      = packedRgb;
    redComponent   = (colorWord >> 4) & 0xF0;
    greenComponent = colorWord & 0xF0;
    red            = flicker + redComponent;
    green          = flicker + greenComponent;
    blue           = flicker + ((packedRgb & 0xF) << 4);
    // Successive discs double the radius and halve the centre intensity.
    do {
        sweepAngle = 0;
        do {
            prim          = _glowAllocateBitDiscWedge(red, green, blue);
            prim->x0      = screenX + ((pixelRadius * rsin(sweepAngle)) >> GLOW_TRIG_SHIFT);
            halfStepAngle = sweepAngle + GLOW_SIXTEENTH_TURN;
            prim->y0      = screenY + ((pixelRadius * rcos(sweepAngle)) >> GLOW_TRIG_SHIFT);
            prim->x1      = screenX + ((pixelRadius * rsin(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            prim->y1      = screenY + ((pixelRadius * rcos(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = sweepAngle + GLOW_EIGHTH_TURN;
            prim->x2      = screenX;
            prim->y2      = screenY;
            prim->x3      = screenX + ((pixelRadius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            prim->y3      = screenY + ((pixelRadius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            sweepAngle    = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DEPTH << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DEPTH);
        } while (sweepAngle < GLOW_FULL_TURN);
        pixelRadius <<= 1;
        red         >>= 1;
        green       >>= 1;
        blue        >>= 1;
        layerIndex++;
    } while (layerIndex < SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_LAYER_COUNT);
}

#include "../../shared/glow_draw_diamond.inc.c"

#include "../../shared/glow_draw_pulsing_disc.inc.c"

/// Selects the cyan room light's pulse speed (0 slow, nonzero fast).
///
/// Retains all 16 argument bits in the room's flag; callers pass 0 or 1.
static void _shelterB1UndergroundParkingSetFastLightPulse(s16 fastPulse)
{
    D_shelter_b1_underground_parking_8018D78C = fastPulse;
}
