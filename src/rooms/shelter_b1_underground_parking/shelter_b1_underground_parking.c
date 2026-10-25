#include "rooms/shelter_b1_underground_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_161500.h"

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

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
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

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"

/// Selects the room-selection panel's private action-prompt hotspot test.
///
/// Bind a function identifier with signature
/// `s32(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY)`; coordinates
/// are screen-centred pixels. The static prototype establishes local linkage
/// before the shared header. Keep the binding through the hit-test fragment,
/// then undefine it. This alias captures no arguments, evaluates nothing and
/// uses neither stringification nor token pasting.
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault
static s32 ACTION_PROMPT_HIT_TEST(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
#include "../../shared/room/action_prompt.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_events.h"
#include "../../shared/room/room_cutscene.h"
#include "../../shared/room/room_variants.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

static void _actionPromptResetDefault(Task* task);
static s32  _roomVariantResolveNeoArk(RoomEventMsg* request, RoomEventMsg* reply);

/// View, CAP and frame-count operands of the room's two event scenes.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_ROOM6_ARRIVAL_VIEW                 = 3,
    SHELTER_B1_UNDERGROUND_PARKING_ROOM6_ARRIVAL_HOLD_FRAMES          = 60,
    SHELTER_B1_UNDERGROUND_PARKING_SCENE_FADE_FRAMES                  = 30,
    SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_CAP_SLOT              = 17,
    SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_HIDE_FRAMES           = 1,
    SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_DISPLAY_WAIT_FRAMES   = 2,
    SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_SKIP_FADE_WAIT_FRAMES = 8,
    SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_PLAYER_CLIP           = 1,
};

/// The panel's five buttons open the Push action in the hotspot command menu.
enum { SHELTER_B1_UNDERGROUND_PARKING_PANEL_PROMPT_PUSH = 1 };

/// Switch hotspot IDs are the bits toggled in the pending selector pattern.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_RED    = 8,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_GREEN  = 4,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_BLUE   = 2,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_YELLOW = 1,
};

/// `ActionPromptHotspot::id` of the selector panel's enter button, which
/// commits the pending switch pattern. The four switch hotspots carry the bit
/// each one toggles in that pattern instead (8, 4, 2, 1).
#define SHELTER_B1_UNDERGROUND_PARKING_PANEL_HOTSPOT_ENTER 16

/// RGB intensity units added to the selector panel's subtractive fade per update.
///
/// The signed-halfword level is clamped to black at 255 after addition; from
/// zero, six units per callback reaches black on call 43.
#define SHELTER_B1_UNDERGROUND_PARKING_PANEL_FADE_STEP 6

/// Selector panel phases stored in the task's byte-sized state.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_SCAN           = 2,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_OPEN_COMMANDS  = 3,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_HANDLE_COMMAND = 4,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_CANCEL         = 5,
    SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_COMMIT         = 6,
};

/// Entries of the room-owned auxiliary task table used by these handlers.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_TASK_PANEL_SESSION        = 0,
    SHELTER_B1_UNDERGROUND_PARKING_TASK_ROOM6_CHOICE         = 1,
    SHELTER_B1_UNDERGROUND_PARKING_TASK_ROOM7_SCENE          = 2,
    SHELTER_B1_UNDERGROUND_PARKING_TASK_GARAGE_DEPARTURE     = 3,
    SHELTER_B1_UNDERGROUND_PARKING_TASK_RESUME_AFTER_CAPTION = 4,
    SHELTER_B1_UNDERGROUND_PARKING_TASK_AMBIENCE             = 5,
    SHELTER_B1_UNDERGROUND_PARKING_TASK_ALTERNATE_CAP        = 6,
    SHELTER_B1_UNDERGROUND_PARKING_TASK_DESCRIPTOR_COUNT     = 7,
};

/// CAP command-table slots selected by this room; their text remains resource-owned.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_01 = 1,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_02 = 2,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_03 = 3,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_04 = 4,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_05 = 5,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_06 = 6,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_07 = 7,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_08 = 8,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_09 = 9,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_0C = 12,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_0E = 14,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_0F = 15,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_10 = 16,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_17 = 23,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_1E = 30,
};

/// CAP sequence slots and the room variants that select their room-specific behavior.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_CAP_SLOT_HANDOFF       = 10,
    SHELTER_B1_UNDERGROUND_PARKING_CAP_SLOT_GARAGE_CHOICE = 11,
    SHELTER_B1_UNDERGROUND_PARKING_ROOM_CAR_SCENE         = 7,
    SHELTER_B1_UNDERGROUND_PARKING_VARIANT_SOLDIER_B      = 21,
    SHELTER_B1_UNDERGROUND_PARKING_FINAL_CHAPTER          = 6,
};

/// Room variant in which an access card can start the electric-car scene.
enum { SHELTER_B1_UNDERGROUND_PARKING_ROOM_CARD_ACCESS = 6 };

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

/// Complete selector-pattern domain; each of the four switches toggles one bit.
enum { SHELTER_B1_UNDERGROUND_PARKING_PANEL_PATTERN_COUNT = 16 };

/// Interaction IDs shared by the room's action-trigger records and handlers.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_ACTION_SOLDIER_REMARK   = 1,
    SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF      = 10,
    SHELTER_B1_UNDERGROUND_PARKING_ACTION_ROOM_CAP         = 11,
    SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE    = 12,
    SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL           = 1,
    SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2 = 2,
    SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_3 = 3,
    SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4      = 4,
    SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_7      = 7,
    SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE           = 13,
    SHELTER_B1_UNDERGROUND_PARKING_COMMAND_DIALOGUE_LOOP   = 22,
    SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4 = 48,
};

/// Access-tunnel warp: departure descriptor 1 here, arrival descriptor 3 there.
enum { SHELTER_B1_UNDERGROUND_PARKING_ACCESS_TUNNEL_WARP_SLOTS = (1 << 4) | 3 };

/// Resource key and base file for the Soldier B placement in variant 21.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_SOLDIER_B_ENTRY_ID    = 116,
    SHELTER_B1_UNDERGROUND_PARKING_SOLDIER_B_FILE_NUMBER = 615,
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

static s32 _gShopPreviewRowId;

static const u16 _gShopStandardDefaultWeaponsStock[];
static const u16 _gShopStandardDefaultAmmunitionStock[];
static const u16 _gShopStandardDefaultArmorStock[];
static const u16 _gShopStandardDefaultItemsStock[];

/// Remaining panel descriptors of the purchase confirmation.

/// The quantity and supply of the slot the charge panel is animating.
static s32                          _gShopRefillDisplayQuantityFixed;
static const EquipmentWeaponSupply* _gShopRefillSupply;

/// The scene sub-task the cutscene runner spawned, while it runs.
extern Task* gRoomCutsceneSoundTask;

static void _shelterB1UndergroundParkingCheckCaptionHandoff(Task* roomTask);
static void _shelterB1UndergroundParkingRoomInitialize(Task* roomTask);
static void _shelterB1UndergroundParkingPanelInitialize(Task* task);
static void _shelterB1UndergroundParkingPanelOpen(Task* task);
static void _shelterB1UndergroundParkingPanelScanHotspots(Task* task);
static void _shelterB1UndergroundParkingPanelOpenCommands(Task* task);
static void _shelterB1UndergroundParkingPanelHandleCommand(Task* task);
static void _shelterB1UndergroundParkingPanelCancel(Task* task);
static void _shelterB1UndergroundParkingPanelCommitSelection(Task* task);
static void _shelterB1UndergroundParkingPanelFadeOut(Task* task);

static void _shelterB1UndergroundParkingStartAmbience(void);
static void _shelterB1UndergroundParkingPanelResetSelection(void);
static void _shelterB1UndergroundParkingPanelApplySelection(void);
static void _shelterB1UndergroundParkingDrawPanelGlow(s16 screenX, s16 screenY, s16 pixelRadius, s16 packedRgb);
static void _shelterB1UndergroundParkingSetFastLightPulse(s16 fastPulse);

#include "../../shared/room/telephone.h"

#include "../../shared/room/shop.h"
#include "../../shared/room/cap_dialogue.h"

static WorldCollisionGrid         _gShelterB1UndergroundParkingRoom1CollisionGrid;
static WorldCollisionGrid         _gShelterB1UndergroundParkingRooms2And4And5CollisionGrid;
static WorldCollisionGrid         _gShelterB1UndergroundParkingRoom3CollisionGrid;
static WorldCollisionGrid         _gShelterB1UndergroundParkingRooms6And7CollisionGrid;
static WorldCollisionGrid         _gShelterB1UndergroundParkingRoom8CollisionGrid;
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRooms1Through7ViewBoundaryTriggers[8];
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRoom8ViewBoundaryTriggers[8];
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRoom1ActionTriggers[12];
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRoom2ActionTriggers[12];
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRoom3ActionTriggers[12];
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRoom4ActionTriggers[12];
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRoom5ActionTriggers[12];
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRoom6ActionTriggers[20];
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRoom7ActionTriggers[16];
static WorldCollisionTrigger      _gShelterB1UndergroundParkingRoom8ActionTriggers[11];
static WorldCoordRoomAmbientEntry _gShelterB1UndergroundParkingViewAmbientTable[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT + 1];
static WorldCoordRoomLights       _gShelterB1UndergroundParkingRoomLights;
static s32                        _shelterB1UndergroundParkingHandleDirectionAction(Task* roomTask, s32 messageId, const DirectionActionRequest* request, s32 unused);
static s32                        _shelterB1UndergroundParkingHandleCommand(Task* roomTask, s32 messageId, s32 commandId, s32 unused);
static s32                        _shelterB1UndergroundParkingUseKeyItem(Task* roomTask, s32 messageId, s32 itemId, s32 unused);
static s32                        _shelterB1UndergroundParkingResolveRoomEvent(Task* roomTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32                        _shelterB1UndergroundParkingCapSoundCue(Task* roomTask, s32 messageId, s32 soundCue, s32 unused);
static void                       _shelterB1UndergroundParkingGarageDepartureTask(Task* task);
static void                       _shelterB1UndergroundParkingAmbienceTask(Task* task);
static void                       _shelterB1UndergroundParkingPanelSessionTask(Task* task);
static void                       _shelterB1UndergroundParkingResolveRoom6ChoiceTask(Task* task);
static void                       _shelterB1UndergroundParkingStartRoom7SceneTask(Task* task);
static void                       _shelterB1UndergroundParkingResumePlayerAfterCaptionTask(Task* task);
static void                       _shelterB1UndergroundParkingAlternateCapTask(Task* task);
static void                       _shelterB1UndergroundParkingSetRoom(u8 roomNumber);
static void                       _shelterB1UndergroundParkingSetPlayerTickHold(u8 holdPlayerTick);
static void                       _shelterB1UndergroundParkingActionPromptTask(Task* task);
static void                       _shelterB1UndergroundParkingPanelTask(Task* task);

#include "../../shared/room/telephone_data.inc.c"

#include "../../shared/room/shop_data.inc.c"

ShelterB1UndergroundParkingShopSessionTaskDescStorage gShelterB1UndergroundParkingShopSessionTaskDescStorage = { { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shopSessionTask, { .value = 0 } }, { 0 } };

/// Bodyless task recipe that carries the staged departure to the 1F garage.
///
/// Spawn only after publishing `gRoomDeparture`; arguments are unused.
static TaskDesc _gShelterB1UndergroundParkingDepartureTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventDepartureTask, { .value = 0 } };

/// This carrier's bodyless scene runner and timed sound task, followed by END.
///
/// Entry SCENE borrows a RoomCutsceneRec in spawnArg2 and takes the follow-up
/// CAP command in spawnArg1; SOUND takes a packed sound request in spawnArg2.
/// Both run at priority 32. Keep callback code and the scene record loaded
/// until completion; spawning copies the recipe and retains no table pointer.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Room-event, key-item, directed-action, command and sound-cue message handlers.
///
/// Borrowed by the registered room task while this overlay is loaded. The
/// final entry makes other message IDs return zero; never dispatch its end ID.
static TaskMessageEntry _gShelterB1UndergroundParkingRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1UndergroundParkingResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1UndergroundParkingUseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1UndergroundParkingHandleDirectionAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1UndergroundParkingHandleCommand },
    { ROOM_MESSAGE_SOUND, _shelterB1UndergroundParkingCapSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless dialogue-loop recipe, with a loaded CAP command index in spawnArg1.
///
/// Command 22 starts this after holding player control. The room and CAP
/// resources must remain loaded until the loop's exit choice resumes control.
static TaskDesc _gShelterB1UndergroundParkingDialogueLoopTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _capDialogueLoopTask, { .value = 0 } };

/// Seven indexed recipes for the room's auxiliary bodyless tasks.
///
/// Slots 0..6 run the panel session, room-6 choice, electric-car scene, garage
/// departure, caption handoff, ambience and alternate CAP playback. There is
/// no terminator. Alternate playback takes command 1..5 in spawnArg1; the
/// other slots ignore spawn arguments. Callback code must remain loaded.
static TaskDesc _gShelterB1UndergroundParkingTaskDescs[SHELTER_B1_UNDERGROUND_PARKING_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1UndergroundParkingPanelSessionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1UndergroundParkingResolveRoom6ChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1UndergroundParkingStartRoom7SceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1UndergroundParkingGarageDepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1UndergroundParkingResumePlayerAfterCaptionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1UndergroundParkingAmbienceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelterB1UndergroundParkingAlternateCapTask, { .value = 0 } },
};

/// Player placement used when the accepted parking choice reveals room 6.
///
/// Position is in room game units; yaw is -1024 (4096 units per turn), with
/// zero pitch and roll. The script's placement message copies both vectors.
static ActorTransform _gShelterB1UndergroundParkingRoom6PlayerPlacement = { { 3155, 0, -247, 0 }, { 0, -1024, 0, 0 } };

/// Fades to black, places the player in room 6 and resumes player control.
///
/// Selects view 3 and the room-6 placement under the fade, waits 60 interpreter
/// updates, then returns the 30-frame fade and resumes control. The command
/// stream and placement remain borrowed until END; it has no skip path.
static EvsCommand _gShelterB1UndergroundParkingRoom6ArrivalScript[10] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = SHELTER_B1_UNDERGROUND_PARKING_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x07) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B1_UNDERGROUND_PARKING_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = SHELTER_B1_UNDERGROUND_PARKING_ROOM6_ARRIVAL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gShelterB1UndergroundParkingRoom6PlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B1_UNDERGROUND_PARKING_ROOM6_ARRIVAL_HOLD_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = SHELTER_B1_UNDERGROUND_PARKING_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B1_UNDERGROUND_PARKING_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Player clip 1 requested during the electric-car scene, without blending or collision.
///
/// EVS copies the full request and replaces source.index with the equipped weapon's
/// bank before dispatch; its stored index 1 is not the playback bank selector.
static AnimationPlayRequest _gShelterB1UndergroundParkingElectricCarPlayerAnimation = { { .index = 1 }, SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_PLAYER_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Electric-car CAP scene that switches the parking resources to room 7.
///
/// Starts CAP slot 17, releases the player tick while the display is hidden,
/// enables drawing and plays the equipped-weapon clip. Two CAP cues bracket
/// the room change before the 30-frame fade return resumes player control.
/// Commands, callbacks and the animation request stay loaded until END.
static EvsCommand _gShelterB1UndergroundParkingElectricCarSceneScript[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_CAP_SLOT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_HIDE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_DISPLAY_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _shelterB1UndergroundParkingSetPlayerTickHold }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gShelterB1UndergroundParkingElectricCarPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x03) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x09) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _shelterB1UndergroundParkingSetRoom }, { .value = SHELTER_B1_UNDERGROUND_PARKING_ROOM_CAR_SCENE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = SHELTER_B1_UNDERGROUND_PARKING_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B1_UNDERGROUND_PARKING_SCENE_FADE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path that selects room 7 and cleans up the electric-car scene under a fade.
///
/// Uses the interpreter's default primary fade, waits 8 updates and then 1,
/// selects room 7, cleans up CAP/HUD state and requests the fade's return.
/// After 8 more updates it resumes player control. The stream and callbacks
/// remain loaded until END.
static EvsCommand _gShelterB1UndergroundParkingElectricCarSceneSkipScript[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _shelterB1UndergroundParkingSetRoom }, { .value = SHELTER_B1_UNDERGROUND_PARKING_ROOM_CAR_SCENE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = SHELTER_B1_UNDERGROUND_PARKING_ELECTRIC_CAR_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Ambience pan and attenuation for logical view slots 0 through 8.
///
/// Slot 0 is retained even though normal room views are 1-based. Later views
/// use base pan and full level. Pan units are three SPU steps; attenuation is
/// halved before queuing as a signed byte (127 would be silent). Both unused
/// halfwords remain zero. The ambience task borrows this table while loaded.
static RoomAmbienceEntry _gShelterB1UndergroundParkingAmbienceMixByView[9] = {
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

/// Bodyless action-cursor child recipe used by the selector panel.
///
/// The panel passes spawnArg1 == 1 for port 0 and owns the child's lifetime.
/// Priority `TASK_PRIORITY_CONTROL` updates the cursor after the priority-32 panel each frame.
static TaskDesc _gShelterB1UndergroundParkingActionPromptTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelterB1UndergroundParkingActionPromptTask, { .value = 0 } };

/// Bodyless selector-panel recipe, spawned and polled by the panel session.
///
/// Spawn arguments are unused; the panel owns allocated work and a cursor child.
/// Its requested exit is completed by the session's poll. Keep this overlay loaded.
static TaskDesc _gShelterB1UndergroundParkingPanelTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1UndergroundParkingPanelTask, { .value = 0 } };

/// Selector-panel switches and Enter button, followed by the hotspot terminator.
///
/// Rectangles use pixels from the screen centre with inclusive far edges. Switch
/// IDs toggle the corresponding pending-pattern bit; Enter commits that pattern.
/// All five choices open the Push command. Initialization clears their hits;
/// each cursor scan updates them in place while the panel is open.
static ActionPromptHotspot _gShelterB1UndergroundParkingPanelHotspots[6] = {
    { -78, 77, 16, 16, SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_RED, SHELTER_B1_UNDERGROUND_PARKING_PANEL_PROMPT_PUSH, 0 },
    { -48, 77, 16, 16, SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_GREEN, SHELTER_B1_UNDERGROUND_PARKING_PANEL_PROMPT_PUSH, 0 },
    { -21, 77, 16, 16, SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_BLUE, SHELTER_B1_UNDERGROUND_PARKING_PANEL_PROMPT_PUSH, 0 },
    { 3, 77, 16, 16, SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_YELLOW, SHELTER_B1_UNDERGROUND_PARKING_PANEL_PROMPT_PUSH, 0 },
    { 34, 77, 40, 16, SHELTER_B1_UNDERGROUND_PARKING_PANEL_HOTSPOT_ENTER, SHELTER_B1_UNDERGROUND_PARKING_PANEL_PROMPT_PUSH, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

/// One-based room-resource variant selected by each committed switch pattern 0..15.
///
/// Values are rooms 2..5; the four switch bits index the complete table. Applying
/// a selection updates both saved and live room selectors without loading an area.
static u8 _gShelterB1UndergroundParkingPanelRoomsByPattern[SHELTER_B1_UNDERGROUND_PARKING_PANEL_PATTERN_COUNT] = {
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

/// Selector diagram's green marker positions for pending switch patterns 0..15.
///
/// Complete DVECTOR pairs in pixels relative to the GPU draw offset. The high
/// two pattern bits choose the row and the low two choose the column; the
/// pending byte contains only switch bits 8/4/2/1.
static DVECTOR _gShelterB1UndergroundParkingPanelMarkerPositions[SHELTER_B1_UNDERGROUND_PARKING_PANEL_PATTERN_COUNT] = {
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

/// World-space centre of the cyan pulsing diamond and disc, in signed game units.
static SVECTOR _gShelterB1UndergroundParkingCyanLightPosition = { -180, -1300, -5490, 0 };

/// World-space green-disc centre and six grey-beam endpoint pairs.
///
/// Coordinates use signed game units. Beams 0..4 appear in mapped views 4, 8,
/// 13 and 14; beam 5 accompanies the disc in views 2, 3, 7, 10, 11 and 12.
/// Mapped view 16 draws only the disc.
static struct {
    SVECTOR greenDiscPosition;    // World-space centre of the frame-parity green disc
    SVECTOR greyBeamPoints[6][2]; // Six beams, each with consecutive first and second world-space endpoints
} _gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints = {
    { 2020, -2290, -7580, 0 },
    {
        { { -3240, -5470, 3600, 0 }, { -2550, -5470, 2680, 0 } },
        { { -1840, -5470, 1720, 0 }, { -1150, -5470, 810, 0 } },
        { { 280, -5470, 810, 0 }, { 970, -5470, 1720, 0 } },
        { { 1710, -5470, 2680, 0 }, { 2400, -5470, 3600, 0 } },
        { { -1160, -5470, -1070, 0 }, { -1840, -5470, -1980, 0 } },
        { { 1430, -3930, -6650, 0 }, { 2580, -3930, -6650, 0 } },
    },
};

/// Two world-space grey-beam endpoint pairs for mapped views 9, 15, 22 and 24.
///
/// Coordinates use signed game units; each pair supplies consecutive beam ends.
static SVECTOR _gShelterB1UndergroundParkingGreyBeamPoints[2][2] = {
    { { 6800, -2150, -2900, 0 }, { 7960, -2150, -2900, 0 } },
    { { 6800, -2150, 2900, 0 }, { 7960, -2150, 2900, 0 } },
};

/// World-space beam ends for the red/green glow in mapped views 8 and 13.
///
/// Coordinates use signed game units; the beam uses red factor 2, green 1, blue 0.
static SVECTOR _gShelterB1UndergroundParkingRedGreenBeamPoints[2] = {
    { -7090, -3080, 1780, 0 },
    { -8250, -3080, 1780, 0 },
};

WorldCoordRoomLighting gShelterB1UndergroundParkingRoomLighting[SHELTER_B1_UNDERGROUND_PARKING_ROOM_COUNT] = {
    { &_gShelterB1UndergroundParkingRoomLights, _gShelterB1UndergroundParkingViewAmbientTable },
    { &_gShelterB1UndergroundParkingRoomLights, _gShelterB1UndergroundParkingViewAmbientTable },
    { &_gShelterB1UndergroundParkingRoomLights, _gShelterB1UndergroundParkingViewAmbientTable },
    { &_gShelterB1UndergroundParkingRoomLights, _gShelterB1UndergroundParkingViewAmbientTable },
    { &_gShelterB1UndergroundParkingRoomLights, _gShelterB1UndergroundParkingViewAmbientTable },
    { &_gShelterB1UndergroundParkingRoomLights, _gShelterB1UndergroundParkingViewAmbientTable },
    { &_gShelterB1UndergroundParkingRoomLights, _gShelterB1UndergroundParkingViewAmbientTable },
    { &_gShelterB1UndergroundParkingRoomLights, _gShelterB1UndergroundParkingViewAmbientTable },
};

WorldCollisionRoomResources gShelterB1UndergroundParkingRoomCollisionResources[SHELTER_B1_UNDERGROUND_PARKING_ROOM_COUNT] = {
    { &_gShelterB1UndergroundParkingRoom1CollisionGrid, _gShelterB1UndergroundParkingRooms1Through7ViewBoundaryTriggers, _gShelterB1UndergroundParkingRoom1ActionTriggers, NULL },
    { &_gShelterB1UndergroundParkingRooms2And4And5CollisionGrid, _gShelterB1UndergroundParkingRooms1Through7ViewBoundaryTriggers, _gShelterB1UndergroundParkingRoom2ActionTriggers, NULL },
    { &_gShelterB1UndergroundParkingRoom3CollisionGrid, _gShelterB1UndergroundParkingRooms1Through7ViewBoundaryTriggers, _gShelterB1UndergroundParkingRoom3ActionTriggers, NULL },
    { &_gShelterB1UndergroundParkingRooms2And4And5CollisionGrid, _gShelterB1UndergroundParkingRooms1Through7ViewBoundaryTriggers, _gShelterB1UndergroundParkingRoom4ActionTriggers, NULL },
    { &_gShelterB1UndergroundParkingRooms2And4And5CollisionGrid, _gShelterB1UndergroundParkingRooms1Through7ViewBoundaryTriggers, _gShelterB1UndergroundParkingRoom5ActionTriggers, NULL },
    { &_gShelterB1UndergroundParkingRooms6And7CollisionGrid, _gShelterB1UndergroundParkingRooms1Through7ViewBoundaryTriggers, _gShelterB1UndergroundParkingRoom6ActionTriggers, NULL },
    { &_gShelterB1UndergroundParkingRooms6And7CollisionGrid, _gShelterB1UndergroundParkingRooms1Through7ViewBoundaryTriggers, _gShelterB1UndergroundParkingRoom7ActionTriggers, NULL },
    { &_gShelterB1UndergroundParkingRoom8CollisionGrid, _gShelterB1UndergroundParkingRoom8ViewBoundaryTriggers, _gShelterB1UndergroundParkingRoom8ActionTriggers, NULL },
};

/// Logical-view map for underground parking room 2.
///
/// Indexed by logical view - 1 for views 1..24; entries are unsigned, 1-based
/// camera, image and sprite indices. Remaps 3 to 11, 6 to 24, 24 to 15.
/// All other views map to themselves; storage is borrowed while this overlay is loaded.
static u8 _gShelterB1UndergroundParkingRoom2ViewMap[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT] = { 1, 2, 11, 4, 5, 24, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 15 };

/// Logical-view map for underground parking room 3.
///
/// Indexed by logical view - 1 for views 1..24; entries are unsigned, 1-based
/// camera, image and sprite indices. Remaps 3 to 7, 6 to 9.
/// All other views map to themselves; storage is borrowed while this overlay is loaded.
static u8 _gShelterB1UndergroundParkingRoom3ViewMap[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT] = { 1, 2, 7, 4, 5, 9, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };

/// Logical-view map for underground parking room 4.
///
/// Indexed by logical view - 1 for views 1..24; entries are unsigned, 1-based
/// camera, image and sprite indices. Remaps 3 to 7, 6 to 22.
/// All other views map to themselves; storage is borrowed while this overlay is loaded.
static u8 _gShelterB1UndergroundParkingRoom4ViewMap[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT] = { 1, 2, 7, 4, 5, 22, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };

/// Logical-view map for underground parking room 5.
///
/// Indexed by logical view - 1 for views 1..24; entries are unsigned, 1-based
/// camera, image and sprite indices. Remaps 3 to 11, 6 to 15.
/// All other views map to themselves; storage is borrowed while this overlay is loaded.
static u8 _gShelterB1UndergroundParkingRoom5ViewMap[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT] = { 1, 2, 11, 4, 5, 15, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };

/// Logical-view map for underground parking room 6.
///
/// Indexed by logical view - 1 for views 1..24; entries are unsigned, 1-based
/// camera, image and sprite indices. Remaps 2 to 10, 3 to 12, 4 to 14.
/// All other views map to themselves; storage is borrowed while this overlay is loaded.
static u8 _gShelterB1UndergroundParkingRoom6ViewMap[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT] = { 1, 10, 12, 14, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };

/// Logical-view map for underground parking room 7.
///
/// Indexed by logical view - 1 for views 1..24; entries are unsigned, 1-based
/// camera, image and sprite indices. Remaps 2 to 10, 3 to 12, 4 to 13.
/// All other views map to themselves; storage is borrowed while this overlay is loaded.
static u8 _gShelterB1UndergroundParkingRoom7ViewMap[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT] = { 1, 10, 12, 13, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };

/// Logical-view map for underground parking room 8.
///
/// Indexed by logical view - 1 for views 1..24; entries are unsigned, 1-based
/// camera, image and sprite indices. Remaps 2 to 16, 16 to 2.
/// All other views map to themselves; storage is borrowed while this overlay is loaded.
static u8 _gShelterB1UndergroundParkingRoom8ViewMap[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT] = { 1, 16, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 2, 17, 18, 19, 20, 21, 22, 23, 24 };

u8* gShelterB1UndergroundParkingRoomViewMaps[SHELTER_B1_UNDERGROUND_PARKING_ROOM_COUNT] = {
    gViewIdentityMap,
    _gShelterB1UndergroundParkingRoom2ViewMap,
    _gShelterB1UndergroundParkingRoom3ViewMap,
    _gShelterB1UndergroundParkingRoom4ViewMap,
    _gShelterB1UndergroundParkingRoom5ViewMap,
    _gShelterB1UndergroundParkingRoom6ViewMap,
    _gShelterB1UndergroundParkingRoom7ViewMap,
    _gShelterB1UndergroundParkingRoom8ViewMap,
};

ViewCount gShelterB1UndergroundParkingRoomViewCounts[SHELTER_B1_UNDERGROUND_PARKING_ROOM_COUNT] = {
    SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT,
    SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT,
    SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT,
    SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT,
    SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT,
    SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT,
    SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT,
    SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT,
};

DirectionWarpEntry gShelterB1UndergroundParkingWarpEntries[SHELTER_B1_UNDERGROUND_PARKING_WARP_COUNT] = {
    { { { .word = 0 }, 1913, 0, -7180 }, { 0, 0, 0, 0 }, { { .word = 0 }, 1980, 0, -6660 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x02), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x01), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 401, 0, -1200 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 401, 0, -1200 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x0A), DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING },
};

/// Room-space unit normals for the room-1 collision grid (4096 per unit).
static SVECTOR _gShelterB1UndergroundParkingRoom1CollisionGridNormals[6] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_normals.inc"
};

/// Room-space vertices for the room-1 collision grid, in signed world units.
static SVECTOR _gShelterB1UndergroundParkingRoom1CollisionGridVerts[60] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_verts.inc"
};

/// Indexed quads for the room-1 collision grid.
///
/// Indices select this grid's vertex and normal pools; surface classes are 0 or 1.
static WorldCollisionGridFace _gShelterB1UndergroundParkingRoom1CollisionGridFaces[22] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_faces.inc"
};

/// Packed signed face-index lists for the room-1 collision grid's cells.
///
/// Each table pointer selects a list ending in WORLD_COLLISION_GRID_CELL_END;
/// indices before the terminator are in 0..21. The pool contains 150 halfwords.
static s16 _gShelterB1UndergroundParkingRoom1CollisionGridCells[150] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_cells.inc"
};

/// Resolves a generated halfword offset to a room-1 cell's face-index list.
///
/// Captures `_gShelterB1UndergroundParkingRoom1CollisionGridCells`, returning a
/// borrowed `s16*`. `halfwordOffset` is evaluated once; these initializers require
/// a constant list start in 0..149, with `WORLD_COLLISION_GRID_CELL_END` inside
/// the pool. The grid's `cell_face_list_macro` manifest entry selects this
/// binding for its table include. Keep the pool live while the grid is active.
#define SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1UndergroundParkingRoom1CollisionGridCells[(halfwordOffset)])

/// X-major 5-by-4 cell directory for the room-1 collision grid.
///
/// Indexed by cellX * 4 + cellZ; each entry borrows a terminated list from Cells.
static s16* _gShelterB1UndergroundParkingRoom1CollisionGridTable[20] = {
#include "assets/shelter_b1_underground_parking_collision_0A890_table.inc"
};
#undef SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and spatial index selected by underground parking room 1.
///
/// Borrows 6 normals, 60 vertices, 22 quad faces and a 5-by-4 X-major cell table.
/// Room-space X/Z are biased by 5390/8000 and divided by a 4000-world-unit cell
/// side. Activation binds the initially NULL view coordinate; the record and all
/// mesh/index storage must stay loaded while the grid is active.
static WorldCollisionGrid _gShelterB1UndergroundParkingRoom1CollisionGrid = { NULL, _gShelterB1UndergroundParkingRoom1CollisionGridNormals, _gShelterB1UndergroundParkingRoom1CollisionGridVerts, _gShelterB1UndergroundParkingRoom1CollisionGridFaces, _gShelterB1UndergroundParkingRoom1CollisionGridTable, 5390, 8000, 5, 4, 4000, ARRAY_SIZE(_gShelterB1UndergroundParkingRoom1CollisionGridFaces) };

/// Room-space unit normals for the rooms-2/4/5 collision grid (4096 per unit).
static SVECTOR _gShelterB1UndergroundParkingRooms2And4And5CollisionGridNormals[8] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_normals.inc"
};

/// Room-space vertices for the rooms-2/4/5 collision grid, in signed game units.
static SVECTOR _gShelterB1UndergroundParkingRooms2And4And5CollisionGridVerts[84] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_verts.inc"
};

/// Indexed quads for the rooms-2/4/5 collision grid.
///
/// Indices select this grid's vertex and normal pools; surface classes are 0 or 1.
static WorldCollisionGridFace _gShelterB1UndergroundParkingRooms2And4And5CollisionGridFaces[33] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_faces.inc"
};

/// Packed signed face-index lists for the rooms-2/4/5 collision grid's cells.
///
/// Each table pointer selects a list ending in WORLD_COLLISION_GRID_CELL_END;
/// indices before the terminator are in 0..32. The pool contains 210 halfwords.
static s16 _gShelterB1UndergroundParkingRooms2And4And5CollisionGridCells[210] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_cells.inc"
};

/// Resolves a generated halfword offset to a rooms-2/4/5 cell's face-index list.
///
/// Captures `_gShelterB1UndergroundParkingRooms2And4And5CollisionGridCells`,
/// returning a borrowed `s16*`. `halfwordOffset` is evaluated once; these
/// initializers require a constant list start in 0..209, with
/// `WORLD_COLLISION_GRID_CELL_END` inside the pool. The grid's
/// `cell_face_list_macro` manifest entry selects this binding for its table
/// include. Keep the pool live while the grid is active.
#define SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1UndergroundParkingRooms2And4And5CollisionGridCells[(halfwordOffset)])

/// X-major 5-by-4 cell directory for the rooms-2/4/5 collision grid.
///
/// Indexed by cellX * 4 + cellZ; each entry borrows a terminated list from Cells.
static s16* _gShelterB1UndergroundParkingRooms2And4And5CollisionGridTable[20] = {
#include "assets/shelter_b1_underground_parking_collision_0AF14_table.inc"
};
#undef SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and spatial index shared by underground parking rooms 2, 4 and 5.
///
/// Borrows 8 normals, 84 vertices, 33 quad faces and a 5-by-4 X-major cell table.
/// Room-space X/Z are biased by 5390/8000 and divided by a 4000-game-unit cell
/// side. Activation binds the initially NULL view coordinate; this record and
/// its mesh/index storage must remain loaded while the grid is active.
static WorldCollisionGrid _gShelterB1UndergroundParkingRooms2And4And5CollisionGrid = { NULL, _gShelterB1UndergroundParkingRooms2And4And5CollisionGridNormals, _gShelterB1UndergroundParkingRooms2And4And5CollisionGridVerts, _gShelterB1UndergroundParkingRooms2And4And5CollisionGridFaces, _gShelterB1UndergroundParkingRooms2And4And5CollisionGridTable, 5390, 8000, 5, 4, 4000, ARRAY_SIZE(_gShelterB1UndergroundParkingRooms2And4And5CollisionGridFaces) };

/// Room-space unit normals for the room-3 collision grid (4096 per unit).
static SVECTOR _gShelterB1UndergroundParkingRoom3CollisionGridNormals[8] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_normals.inc"
};

/// Room-space vertices for the room-3 collision grid, in signed game units.
static SVECTOR _gShelterB1UndergroundParkingRoom3CollisionGridVerts[90] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_verts.inc"
};

/// Indexed quads for the room-3 collision grid.
///
/// Indices select this grid's vertex and normal pools; surface classes are 0 or 1.
static WorldCollisionGridFace _gShelterB1UndergroundParkingRoom3CollisionGridFaces[36] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_faces.inc"
};

/// Packed signed face-index lists for the room-3 collision grid's cells.
///
/// Each table pointer selects a list ending in WORLD_COLLISION_GRID_CELL_END;
/// indices before the terminator are in 0..35. The pool contains 222 halfwords.
static s16 _gShelterB1UndergroundParkingRoom3CollisionGridCells[222] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_cells.inc"
};

/// Resolves a generated halfword offset to a room-3 cell's face-index list.
///
/// Captures `_gShelterB1UndergroundParkingRoom3CollisionGridCells`, returning a
/// borrowed `s16*`. `halfwordOffset` is evaluated once; these initializers require
/// a constant list start in 0..221, with `WORLD_COLLISION_GRID_CELL_END` inside
/// the pool. The grid's `cell_face_list_macro` manifest entry selects this
/// binding for its table include. Keep the pool live while the grid is active.
#define SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1UndergroundParkingRoom3CollisionGridCells[(halfwordOffset)])

/// X-major 5-by-4 cell directory for the room-3 collision grid.
///
/// Indexed by cellX * 4 + cellZ; each entry borrows a terminated list from Cells.
static s16* _gShelterB1UndergroundParkingRoom3CollisionGridTable[20] = {
#include "assets/shelter_b1_underground_parking_collision_0B604_table.inc"
};
#undef SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and spatial index selected by underground parking room 3.
///
/// Borrows 8 normals, 90 vertices, 36 quad faces and a 5-by-4 X-major cell table.
/// Room-space X/Z are biased by 5390/8000 and divided by a 4000-game-unit cell
/// side. Activation binds the initially NULL view coordinate; this record and
/// its mesh/index storage must remain loaded while the grid is active.
static WorldCollisionGrid _gShelterB1UndergroundParkingRoom3CollisionGrid = { NULL, _gShelterB1UndergroundParkingRoom3CollisionGridNormals, _gShelterB1UndergroundParkingRoom3CollisionGridVerts, _gShelterB1UndergroundParkingRoom3CollisionGridFaces, _gShelterB1UndergroundParkingRoom3CollisionGridTable, 5390, 8000, 5, 4, 4000, ARRAY_SIZE(_gShelterB1UndergroundParkingRoom3CollisionGridFaces) };

/// Room-space unit normals for the rooms-6/7 collision grid (4096 per unit).
static SVECTOR _gShelterB1UndergroundParkingRooms6And7CollisionGridNormals[6] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_normals.inc"
};

/// Room-space vertices for the rooms-6/7 collision grid, in signed game units.
static SVECTOR _gShelterB1UndergroundParkingRooms6And7CollisionGridVerts[68] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_verts.inc"
};

/// Indexed quads for the rooms-6/7 collision grid.
///
/// Indices select this grid's vertex and normal pools; surface classes are 0 or 1.
static WorldCollisionGridFace _gShelterB1UndergroundParkingRooms6And7CollisionGridFaces[27] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_faces.inc"
};

/// Packed signed face-index lists for the rooms-6/7 collision grid's cells.
///
/// Each table pointer selects a list ending in WORLD_COLLISION_GRID_CELL_END;
/// indices before the terminator are in 0..26. The pool contains 176 halfwords.
static s16 _gShelterB1UndergroundParkingRooms6And7CollisionGridCells[176] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_cells.inc"
};

/// Resolves a generated halfword offset to a rooms-6/7 cell's face-index list.
///
/// Captures `_gShelterB1UndergroundParkingRooms6And7CollisionGridCells`, returning
/// a borrowed `s16*`. `halfwordOffset` is evaluated once; these initializers
/// require a constant list start in 0..175, with `WORLD_COLLISION_GRID_CELL_END`
/// inside the pool. The grid's `cell_face_list_macro` manifest entry selects
/// this binding for its table include. Keep the pool live while the grid is active.
#define SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1UndergroundParkingRooms6And7CollisionGridCells[(halfwordOffset)])

/// X-major 5-by-4 cell directory for the rooms-6/7 collision grid.
///
/// Indexed by cellX * 4 + cellZ; each entry borrows a terminated list from Cells.
static s16* _gShelterB1UndergroundParkingRooms6And7CollisionGridTable[20] = {
#include "assets/shelter_b1_underground_parking_collision_0BB6C_table.inc"
};
#undef SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and spatial index shared by underground parking rooms 6 and 7.
///
/// Borrows 6 normals, 68 vertices, 27 quad faces and a 5-by-4 X-major cell table.
/// Room-space X/Z are biased by 5390/8000 and divided by a 4000-game-unit cell
/// side. Activation binds the initially NULL view coordinate; this record and
/// its mesh/index storage must remain loaded while the grid is active.
static WorldCollisionGrid _gShelterB1UndergroundParkingRooms6And7CollisionGrid = { NULL, _gShelterB1UndergroundParkingRooms6And7CollisionGridNormals, _gShelterB1UndergroundParkingRooms6And7CollisionGridVerts, _gShelterB1UndergroundParkingRooms6And7CollisionGridFaces, _gShelterB1UndergroundParkingRooms6And7CollisionGridTable, 5390, 8000, 5, 4, 4000, ARRAY_SIZE(_gShelterB1UndergroundParkingRooms6And7CollisionGridFaces) };

/// Room-space unit normals for the room-8 collision grid (4096 per unit).
static SVECTOR _gShelterB1UndergroundParkingRoom8CollisionGridNormals[8] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_normals.inc"
};

/// Room-space vertices for the room-8 collision grid, in signed game units.
static SVECTOR _gShelterB1UndergroundParkingRoom8CollisionGridVerts[83] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_verts.inc"
};

/// Indexed quads for the room-8 collision grid.
///
/// Indices select this grid's vertex and normal pools; surface classes are 0 or 1.
static WorldCollisionGridFace _gShelterB1UndergroundParkingRoom8CollisionGridFaces[30] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_faces.inc"
};

/// Packed signed face-index lists for the room-8 collision grid's cells.
///
/// Each table pointer selects a list ending in WORLD_COLLISION_GRID_CELL_END;
/// indices before the terminator are in 0..29. The pool contains 186 halfwords.
static s16 _gShelterB1UndergroundParkingRoom8CollisionGridCells[186] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_cells.inc"
};

/// Resolves a generated halfword offset to a room-8 cell's face-index list.
///
/// Captures `_gShelterB1UndergroundParkingRoom8CollisionGridCells`, returning a
/// borrowed `s16*`. `halfwordOffset` is evaluated once; these initializers require
/// a constant list start in 0..185, with `WORLD_COLLISION_GRID_CELL_END` inside
/// the pool. The grid's `cell_face_list_macro` manifest entry selects this
/// binding for its table include. Keep the pool live while the grid is active.
#define SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1UndergroundParkingRoom8CollisionGridCells[(halfwordOffset)])

/// X-major 5-by-4 cell directory for the room-8 collision grid.
///
/// Indexed by cellX * 4 + cellZ; each entry borrows a terminated list from Cells.
static s16* _gShelterB1UndergroundParkingRoom8CollisionGridTable[20] = {
#include "assets/shelter_b1_underground_parking_collision_0C194_table.inc"
};
#undef SHELTER_B1_UNDERGROUND_PARKING_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and spatial index selected by underground parking room 8.
///
/// Borrows 8 normals, 83 vertices, 30 quad faces and a 5-by-4 X-major cell table.
/// Room-space X/Z are biased by 5390/8000 and divided by a 4000-game-unit cell
/// side. Activation binds the initially NULL view coordinate; this record and
/// its mesh/index storage must remain loaded while the grid is active.
static WorldCollisionGrid _gShelterB1UndergroundParkingRoom8CollisionGrid = { NULL, _gShelterB1UndergroundParkingRoom8CollisionGridNormals, _gShelterB1UndergroundParkingRoom8CollisionGridVerts, _gShelterB1UndergroundParkingRoom8CollisionGridFaces, _gShelterB1UndergroundParkingRoom8CollisionGridTable, 5390, 8000, 5, 4, 4000, ARRAY_SIZE(_gShelterB1UndergroundParkingRoom8CollisionGridFaces) };

ViewCamera gShelterB1UndergroundParkingViewCameras[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 6700, 0x7530, 0 } }, 207 },
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
    { { { { 0, 0, -ONE }, { 0, ONE, 0 }, { ONE, 0, 0 } }, { 0x4092, 1420, 3350 } }, 6874 },
    { { { { -1166, 0, -3926 }, { -943, 3975, 280 }, { 3811, 984, -1132 } }, { 1170, 2400, -1600 } }, 257 },
    { { { { 711, 0, 4033 }, { 2858, 2890, -504 }, { -2846, 2902, 502 } }, { 1620, 1430, 1640 } }, 257 },
    { { { { -1166, 0, -3926 }, { -943, 3975, 280 }, { 3811, 984, -1132 } }, { 1170, 2400, -1600 } }, 257 },
};

/// Neutral 8-bit sprite colour modulation (unity).
enum { SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR = 128 };

/// Empty sprite batches for mapped underground parking view 1.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 2.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped underground parking views 3, 7 and 11.
///
/// The 13 sources use the 8-bit page at VRAM (960, 0) and CLUT at (0, 255).
/// XY and dimensions are pixels relative to the drawing origin; depths are
/// unscaled OT sorting values. Batches borrow these writable sources while loaded.
static SpriteSource _gShelterB1UndergroundParkingView3SpriteSources[13] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -96, -120, 1158, { .fields = { 88, 192 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -96, -80, 1514, { .fields = { 56, 72 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -128, -120, 689, { .fields = { 72, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -160, -120, 479, { .fields = { 64, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -128, -80, 823, { .fields = { 96, 56 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -160, -80, 493, { .fields = { 96, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -160, -24, 698, { .fields = { 104, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, 16, 666, { .fields = { 56, 104 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 32, 657, { .fields = { 88, 112 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -160, 72, 651, { .fields = { 48, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, 88, 636, { .fields = { 64, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -128, 88, 625, { .fields = { 72, 40 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, 104, 683, { .fields = { 96, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped underground parking views 3, 7 and 11.
///
/// The zero-count lead selects decoded background strips; the next range draws
/// all 13 sources. The final record ends the list.
static SpriteBatch _gShelterB1UndergroundParkingView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1UndergroundParkingView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped underground parking views 4 and 8.
///
/// The 32 sources use the 8-bit page at VRAM (960, 0) and CLUT at (0, 255).
/// XY and dimensions are pixels relative to the drawing origin; depths are
/// unscaled OT sorting values. Batches borrow these writable sources while loaded.
static SpriteSource _gShelterB1UndergroundParkingView4SpriteSources[32] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 0, 1327, { .fields = { 104, 136 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -152, 16, 1339, { .fields = { 112, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -152, 32, 1367, { .fields = { 104, 216 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -152, 40, 1358, { .fields = { 104, 224 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -160, 32, 1341, { .fields = { 112, 168 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 48, 1374, { .fields = { 88, 16 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 56, 1364, { .fields = { 88, 8 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 64, 1370, { .fields = { 88, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -120, 80, 1311, { .fields = { 112, 88 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -128, 96, 1247, { .fields = { 112, 104 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -128, 80, 1309, { .fields = { 112, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, 88, 1235, { .fields = { 112, 200 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, 72, 1375, { .fields = { 112, 184 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 72, 1375, { .fields = { 96, 240 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -160, 80, 1381, { .fields = { 104, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -144, 80, 1287, { .fields = { 120, 136 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, 104, 1210, { .fields = { 120, 176 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -144, 104, 1224, { .fields = { 120, 192 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -160, 88, 1307, { .fields = { 104, 248 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -160, 96, 1300, { .fields = { 112, 64 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -120, 96, 1299, { .fields = { 120, 160 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 64, -24, 2239, { .fields = { 120, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 64, 8, 2318, { .fields = { 120, 32 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, -24, 2293, { .fields = { 112, 16 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, -8, 2308, { .fields = { 112, 32 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 8, 2343, { .fields = { 112, 48 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 24, 2392, { .fields = { 112, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 8, 2391, { .fields = { 120, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 24, 2394, { .fields = { 120, 224 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 56, 24, 2358, { .fields = { 120, 240 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 0, 2337, { .fields = { 120, 112 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, -24, 2273, { .fields = { 120, 88 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped underground parking views 4 and 8.
///
/// The zero-count lead selects decoded background strips, followed by ranges
/// [0, 21) and [21, 32) and the terminator. The second range's trailing byte
/// retains its authored value 1; that byte's role is unproven.
static SpriteBatch _gShelterB1UndergroundParkingView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { 21, ARRAY_SIZE(_gShelterB1UndergroundParkingView4SpriteSources) - 21, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 5.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 6.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Retained empty sprite batch list with no installed view or other consumer.
///
/// A zero-count background-strip selector precedes SPRITE_BATCH_END. The list
/// contributes no source sprites; a former view assignment is unproven.
static SpriteBatch _gShelterB1UndergroundParkingUnusedEmptySpriteBatches1[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Retained empty sprite batch list with no installed view or other consumer.
///
/// A zero-count background-strip selector precedes SPRITE_BATCH_END. The list
/// contributes no source sprites; a former view assignment is unproven.
static SpriteBatch _gShelterB1UndergroundParkingUnusedEmptySpriteBatches2[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 9.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source sprites for mapped underground parking view 10.
///
/// 11 texture rectangles in view-centred pixels, with unscaled sorting depths.
/// Uses the 8-bit texture page at VRAM (960, 0), CLUT (0, 255), neutral RGB and
/// opaque modulation. Batches select source elements; this array has no terminator.
static SpriteSource _gShelterB1UndergroundParkingView10SpriteSources[11] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 32, 104, 780, { .fields = { 112, 192 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 48, 88, 772, { .fields = { 112, 64 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 64, 88, 700, { .fields = { 112, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 96, 80, 900, { .fields = { 112, 168 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 112, 72, 863, { .fields = { 112, 32 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 128, 80, 950, { .fields = { 112, 144 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 144, 96, 882, { .fields = { 112, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 120, 104, 808, { .fields = { 104, 224 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 96, 104, 873, { .fields = { 104, 240 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 80, 80, 786, { .fields = { 112, 96 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 80, 104, 843, { .fields = { 112, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped underground parking view 10.
///
/// The zero-count lead selects decoded background strips, then all 11 source
/// elements are drawn before the terminator.
static SpriteBatch _gShelterB1UndergroundParkingView10SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB1UndergroundParkingView10SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Retained empty sprite batch list with no installed view or other consumer.
///
/// A zero-count background-strip selector precedes SPRITE_BATCH_END. The list
/// contributes no source sprites; a former view assignment is unproven.
static SpriteBatch _gShelterB1UndergroundParkingUnusedEmptySpriteBatches3[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source sprites for mapped underground parking view 12.
///
/// 42 texture rectangles in view-centred pixels, with unscaled sorting depths.
/// Uses the 8-bit texture page at VRAM (960, 0), CLUT (0, 255), neutral RGB and
/// opaque modulation. Batches select source elements; this array has no terminator.
static SpriteSource _gShelterB1UndergroundParkingView12SpriteSources[42] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -96, -120, 1158, { .fields = { 88, 112 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -96, -80, 1514, { .fields = { 32, 72 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -128, -120, 689, { .fields = { 72, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -160, -120, 479, { .fields = { 64, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -128, -80, 823, { .fields = { 96, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -160, -80, 493, { .fields = { 96, 56 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -160, -24, 698, { .fields = { 104, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, 16, 666, { .fields = { 24, 168 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 32, 657, { .fields = { 88, 192 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -160, 72, 651, { .fields = { 16, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, 88, 636, { .fields = { 24, 48 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -128, 88, 625, { .fields = { 64, 224 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -160, 104, 683, { .fields = { 40, 184 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 16, 40, 1491, { .fields = { 72, 72 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 8, 48, 1471, { .fields = { 40, 16 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 96, 40, 1554, { .fields = { 48, 128 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 120, 24, 1538, { .fields = { 88, 40 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 96, 24, 1494, { .fields = { 32, 104 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 72, 24, 1527, { .fields = { 64, 40 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 56, 24, 1509, { .fields = { 72, 104 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 40, 24, 1510, { .fields = { 72, 192 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 16, 24, 1499, { .fields = { 40, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 8, 24, 1469, { .fields = { 112, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 0, 32, 1436, { .fields = { 120, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 0, 8, 1663, { .fields = { 88, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 24, -16, 1640, { .fields = { 32, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 48, -16, 1586, { .fields = { 32, 32 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 104, 8, 1586, { .fields = { 40, 200 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 88, 0, 1622, { .fields = { 56, 104 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 72, 0, 1614, { .fields = { 96, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, -8, 1542, { .fields = { 56, 200 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, 8, 1543, { .fields = { 56, 168 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 40, -8, 1606, { .fields = { 72, 136 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 40, 8, 1653, { .fields = { 56, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 24, 8, 1361, { .fields = { 48, 216 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 24, -8, 1391, { .fields = { 48, 32 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 8, -8, 1586, { .fields = { 48, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 8, 8, 1631, { .fields = { 48, 16 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -8, 40, 1486, { .fields = { 56, 48 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -16, 8, 1465, { .fields = { 48, 248 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -16, 16, 1517, { .fields = { 48, 64 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 24, 1527, { .fields = { 32, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped underground parking view 12.
///
/// The zero-count lead selects decoded background strips, followed by ranges
/// [0, 13) and [13, 42) and the terminator. The first range retains trailing
/// byte 1; that byte's role is unproven.
static SpriteBatch _gShelterB1UndergroundParkingView12SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, ARRAY_SIZE(_gShelterB1UndergroundParkingView12SpriteSources) - 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source sprites for mapped underground parking views 13 and 14.
///
/// 65 texture rectangles in view-centred pixels, with unscaled sorting depths.
/// Uses the 8-bit texture page at VRAM (960, 0), CLUT (0, 255), neutral RGB and
/// opaque modulation. Batches select source elements; this array has no terminator.
static SpriteSource _gShelterB1UndergroundParkingView13SpriteSources[65] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 0, 1327, { .fields = { 80, 48 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -152, 16, 1339, { .fields = { 104, 56 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -152, 32, 1367, { .fields = { 80, 200 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -152, 40, 1358, { .fields = { 80, 144 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -160, 32, 1341, { .fields = { 112, 144 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 48, 1374, { .fields = { 72, 136 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 56, 1364, { .fields = { 72, 128 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 64, 1370, { .fields = { 72, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -120, 80, 1311, { .fields = { 96, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -128, 96, 1247, { .fields = { 104, 136 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -128, 80, 1309, { .fields = { 96, 136 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, 88, 1235, { .fields = { 96, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, 72, 1375, { .fields = { 96, 192 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 72, 1375, { .fields = { 72, 160 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -160, 80, 1381, { .fields = { 80, 168 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -144, 80, 1287, { .fields = { 120, 96 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -136, 104, 1210, { .fields = { 104, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -144, 104, 1224, { .fields = { 104, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -160, 88, 1307, { .fields = { 80, 176 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -160, 96, 1300, { .fields = { 112, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -120, 96, 1299, { .fields = { 88, 88 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 64, -24, 2239, { .fields = { 120, 32 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 64, 8, 2318, { .fields = { 120, 64 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, -24, 2293, { .fields = { 96, 240 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, -8, 2308, { .fields = { 96, 224 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 8, 2343, { .fields = { 88, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 24, 2392, { .fields = { 88, 32 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 8, 2391, { .fields = { 88, 16 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 24, 2394, { .fields = { 112, 104 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 56, 24, 2358, { .fields = { 112, 88 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 0, 2337, { .fields = { 120, 144 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, -24, 2273, { .fields = { 120, 216 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 24, 72, 1465, { .fields = { 96, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -40, 56, 1587, { .fields = { 104, 240 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -32, 48, 1680, { .fields = { 80, 184 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -48, 48, 1575, { .fields = { 80, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 40, 1625, { .fields = { 72, 224 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 32, 1662, { .fields = { 72, 240 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -24, 24, 1813, { .fields = { 72, 216 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -16, 32, 1692, { .fields = { 80, 192 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 56, 1575, { .fields = { 96, 104 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 40, 1584, { .fields = { 96, 88 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, 48, 1553, { .fields = { 96, 40 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 16, 48, 1496, { .fields = { 120, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 0, 64, 1777, { .fields = { 80, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 24, 56, 1428, { .fields = { 104, 192 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 40, 56, 1383, { .fields = { 96, 24 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 48, 72, 1345, { .fields = { 104, 224 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 64, 72, 1350, { .fields = { 112, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, 48, 1394, { .fields = { 80, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 48, 1532, { .fields = { 120, 168 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, 40, 1384, { .fields = { 80, 64 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 24, 40, 1578, { .fields = { 80, 72 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 0, 32, 1595, { .fields = { 96, 176 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 0, 16, 1540, { .fields = { 112, 40 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 8, 16, 1558, { .fields = { 112, 56 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 16, 16, 1733, { .fields = { 112, 24 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 24, 16, 1659, { .fields = { 112, 160 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 24, 32, 1400, { .fields = { 96, 168 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 32, 16, 1702, { .fields = { 88, 104 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 40, 16, 1598, { .fields = { 120, 192 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 24, 1576, { .fields = { 104, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 56, 32, 1589, { .fields = { 96, 56 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, 56, 1393, { .fields = { 96, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 64, 56, 1403, { .fields = { 112, 72 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped underground parking views 13 and 14.
///
/// The zero-count lead selects decoded background strips, followed by ranges
/// [0, 21), [21, 32) and [32, 65) and the terminator. The first two ranges
/// retain trailing bytes 1 and 2; those bytes' roles are unproven.
static SpriteBatch _gShelterB1UndergroundParkingView13SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 11, 0, 0, { 2, 0 } },
    { 32, ARRAY_SIZE(_gShelterB1UndergroundParkingView13SpriteSources) - 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Retained empty sprite batch list with no installed view or other consumer.
///
/// A zero-count background-strip selector precedes SPRITE_BATCH_END. The list
/// contributes no source sprites; a former view assignment is unproven.
static SpriteBatch _gShelterB1UndergroundParkingUnusedEmptySpriteBatches4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 15.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView15SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 16.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView16SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source sprites for mapped underground parking view 17.
///
/// 28 texture rectangles in view-centred pixels, with unscaled sorting depths.
/// Uses the 8-bit texture page at VRAM (960, 0), CLUT (0, 255), neutral RGB and
/// opaque modulation. Batches select source elements; this array has no terminator.
static SpriteSource _gShelterB1UndergroundParkingView17SpriteSources[28] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 144, -32, 1061, { .fields = { 88, 184 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 144, -8, 1084, { .fields = { 88, 136 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 136, 8, 1096, { .fields = { 72, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 136, 24, 1149, { .fields = { 72, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 128, 40, 1131, { .fields = { 64, 168 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 128, 56, 1136, { .fields = { 64, 224 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 136, 64, 1122, { .fields = { 72, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 152, -80, 965, { .fields = { 120, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -120, -80, 1141, { .fields = { 96, 232 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -120, -56, 1128, { .fields = { 88, 96 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -120, -32, 1206, { .fields = { 80, 24 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -120, -8, 1169, { .fields = { 88, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -120, 16, 1237, { .fields = { 80, 48 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -128, 24, 1181, { .fields = { 104, 88 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -136, 32, 1152, { .fields = { 88, 72 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -144, 56, 1034, { .fields = { 112, 160 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -160, 64, 909, { .fields = { 112, 56 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -160, 16, 868, { .fields = { 112, 112 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -144, 32, 1173, { .fields = { 96, 112 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -144, -16, 1125, { .fields = { 104, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -160, -32, 807, { .fields = { 96, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -160, -88, 757, { .fields = { 112, 0 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -144, -56, 1125, { .fields = { 96, 48 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -144, -88, 1025, { .fields = { 96, 152 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -128, -80, 1060, { .fields = { 104, 120 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -128, 0, 1171, { .fields = { 104, 184 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -128, -24, 1107, { .fields = { 96, 208 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -128, -48, 1103, { .fields = { 96, 88 } }, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, SHELTER_B1_UNDERGROUND_PARKING_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite ranges for mapped underground parking view 17.
///
/// The zero-count lead selects decoded background strips, followed by ranges
/// [0, 8) and [8, 28) and the terminator. The first range retains trailing
/// byte 1; that byte's role is unproven.
static SpriteBatch _gShelterB1UndergroundParkingView17SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, ARRAY_SIZE(_gShelterB1UndergroundParkingView17SpriteSources) - 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 18.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView18SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 19.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView19SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 20.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView20SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 21.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView21SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 22.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView22SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 23.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView23SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batches for mapped underground parking view 24.
///
/// The zero-count lead selects decoded background strips, followed by the list
/// terminator. The source slot retains this list's address but supplies no sprites.
static SpriteBatch _gShelterB1UndergroundParkingView24SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB1UndergroundParkingSpriteViews[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT] = {
    { { .empty = _gShelterB1UndergroundParkingView1SpriteBatches }, _gShelterB1UndergroundParkingView1SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView2SpriteBatches }, _gShelterB1UndergroundParkingView2SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView3SpriteSources }, _gShelterB1UndergroundParkingView3SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView4SpriteSources }, _gShelterB1UndergroundParkingView4SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView5SpriteBatches }, _gShelterB1UndergroundParkingView5SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView6SpriteBatches }, _gShelterB1UndergroundParkingView6SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView3SpriteSources }, _gShelterB1UndergroundParkingView3SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView4SpriteSources }, _gShelterB1UndergroundParkingView4SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView9SpriteBatches }, _gShelterB1UndergroundParkingView9SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView10SpriteSources }, _gShelterB1UndergroundParkingView10SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView3SpriteSources }, _gShelterB1UndergroundParkingView3SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView12SpriteSources }, _gShelterB1UndergroundParkingView12SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView13SpriteSources }, _gShelterB1UndergroundParkingView13SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView13SpriteSources }, _gShelterB1UndergroundParkingView13SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView15SpriteBatches }, _gShelterB1UndergroundParkingView15SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView16SpriteBatches }, _gShelterB1UndergroundParkingView16SpriteBatches, NULL },
    { { .elements = _gShelterB1UndergroundParkingView17SpriteSources }, _gShelterB1UndergroundParkingView17SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView18SpriteBatches }, _gShelterB1UndergroundParkingView18SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView19SpriteBatches }, _gShelterB1UndergroundParkingView19SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView20SpriteBatches }, _gShelterB1UndergroundParkingView20SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView21SpriteBatches }, _gShelterB1UndergroundParkingView21SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView22SpriteBatches }, _gShelterB1UndergroundParkingView22SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView23SpriteBatches }, _gShelterB1UndergroundParkingView23SpriteBatches, NULL },
    { { .empty = _gShelterB1UndergroundParkingView24SpriteBatches }, _gShelterB1UndergroundParkingView24SpriteBatches, NULL },
};

/// Ten authored world-coordinate point lights shared by all underground parking rooms.
///
/// Positions and falloff radii use world units; RGB uses Q12 (ONE is unity).
/// All entries admit every logical view. Gameplay parents and composes their
/// transforms and overwrites attenuation while the room overlay stays loaded.
static WorldCoordPointLight _gShelterB1UndergroundParkingPointLights[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1570, -2500, 1829 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 2867, 2867 }, { 0, 0 } }, 2816, 4608 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2410, -2500, 2490 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2816, 4608 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2410, -2500, -2730 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2816, 4608 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1580, -2139, -2730 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2816, 4608 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2108, -1663, -6654 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2304, 2867, 2266 }, { 0, 0 } }, 1301, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7240, -2500, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2035, 2999 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x355C, -2500, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 4000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x4D1C, -2500, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 4000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7370, -2138, 2730 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 2560, 4352 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7370, -2138, -2710 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 2560, 4352 },
};

/// Shared point-light collection for underground parking rooms 1 through 8.
///
/// Borrows all ten writable point lights; there are no directional or cone lights.
/// Gameplay retains this record and its sources only while the overlay is loaded.
static WorldCoordRoomLights _gShelterB1UndergroundParkingRoomLights = { 0, NULL, ARRAY_SIZE(_gShelterB1UndergroundParkingPointLights), _gShelterB1UndergroundParkingPointLights, 0, NULL };

/// Eight view-boundary quads shared by underground parking rooms 1 through 7.
///
/// Source/destination bytes use logical views 2..6; room view maps choose the
/// images. Origins, relative corners and radii use room game units; normals
/// use 4096 per unit. Activation binds coordinates and links/enables every row,
/// including the final LAST row. Keep these mutable records loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRooms1Through7ViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { 608, -3744, -3072, 0 }, { { -4222, -4192, -311, 0 }, { 4204, -4192, 298, 0 }, { -4222, 4192, -311, 0 }, { 4204, 4192, 298, 0 } }, { 295, 0, -4093, 0 }, { 0, 0, ONE, 0 }, 5948, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 656, -3776, -3391, 0 }, { { 4534, -4192, 308, 0 }, { -4560, -4192, -330, 0 }, { 4534, 4192, 308, 0 }, { -4560, 4192, -330, 0 } }, { -287, 0, 4085, 0 }, { 0, 0, ONE, 0 }, 6186, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2384, -3648, 2848, 0 }, { { -4080, -4192, -1088, 0 }, { 4080, -4192, 1088, 0 }, { -4080, 4192, -1088, 0 }, { 4080, 4192, 1088, 0 } }, { 1056, 0, -3964, 0 }, { 0, 0, ONE, 0 }, 5948, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -448, -3616, -192, 0 }, { { 5232, -4192, 2624, 0 }, { -5232, -4192, -2624, 0 }, { 5232, 4192, 2624, 0 }, { -5232, 4192, -2624, 0 } }, { -1842, 0, 3672, 0 }, { 0, 0, ONE, 0 }, 7186, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -512, -3616, -90, 0 }, { { -5296, -4192, -2634, 0 }, { 5296, -4192, 2646, 0 }, { -5296, 4192, -2646, 0 }, { 5296, 4192, 2634, 0 } }, { 1833, -6, -3678, 0 }, { 0, 0, ONE, 0 }, 7240, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2590, -3616, 2672, 0 }, { { 3440, -4192, 944, 0 }, { -3440, -4192, -944, 0 }, { 3440, 4192, 944, 0 }, { -3440, 4192, -944, 0 } }, { -1085, 0, 3951, 0 }, { 0, 0, ONE, 0 }, 5490, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4176, -3552, -80, 0 }, { { 32, -4192, -2864, 0 }, { -32, -4192, 2864, 0 }, { 32, 4192, -2864, 0 }, { -32, 4192, 2864, 0 } }, { 4095, 0, 45, 0 }, { 0, 0, ONE, 0 }, 5068, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4288, -3520, -191, 0 }, { { -32, -4192, 2480, 0 }, { 32, -4192, -2480, 0 }, { -32, 4192, 2480, 0 }, { 32, 4192, -2480, 0 } }, { -4103, 0, -53, 0 }, { 0, 0, ONE, 0 }, 4857, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Eight view-boundary quads selected by underground parking room 8.
///
/// Directed boundaries connect logical views 2/17, 17/19 and 17/18. Origins,
/// relative corners and radii use room game units; normals use 4096 per unit.
/// Activation binds coordinates and links/enables the final LAST row as well.
/// Keep these mutable records loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRoom8ViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { 774, -3744, -5703, 0 }, { { -1271, -4192, -842, 0 }, { 1272, -4192, 842, 0 }, { -1271, 4192, -842, 0 }, { 1272, 4192, 842, 0 } }, { 2265, 0, -3423, 0 }, { 0, 0, ONE, 0 }, 4434, 0, 2, 17, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 799, -3552, -5824, 0 }, { { 1272, -4192, 842, 0 }, { -1271, -4192, -842, 0 }, { 1272, 4192, 842, 0 }, { -1271, 4192, -842, 0 } }, { -2267, 0, 3422, 0 }, { 0, 0, ONE, 0 }, 4434, 0, 17, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3198, -3648, -5760, 0 }, { { -1221, -4192, 914, 0 }, { 1222, -4192, -914, 0 }, { -1221, 4192, 914, 0 }, { 1222, 4192, -914, 0 } }, { -2461, 0, -3289, 0 }, { 0, 0, ONE, 0 }, 4434, 0, 2, 17, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3230, -3712, -5888, 0 }, { { 1222, -4192, -914, 0 }, { -1221, -4192, 914, 0 }, { 1222, 4192, -914, 0 }, { -1221, 4192, 914, 0 } }, { 2459, 0, 3287, 0 }, { 0, 0, ONE, 0 }, 4434, 0, 17, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2623, -3680, -1313, 0 }, { { -2005, -4192, 2, 0 }, { 2005, -4192, -1, 0 }, { -2005, 4192, 2, 0 }, { 2005, 4192, -1, 0 } }, { -4, 0, -4104, 0 }, { 0, 0, ONE, 0 }, 4636, 0, 17, 19, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2656, -3616, -1441, 0 }, { { 2005, -4192, -1, 0 }, { -2005, -4192, 2, 0 }, { 2005, 4192, -1, 0 }, { -2005, 4192, 2, 0 } }, { 3, 0, 4103, 0 }, { 0, 0, ONE, 0 }, 4636, 0, 19, 17, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 591, -3712, -3985, 0 }, { { -178, -4192, -2803, 0 }, { 178, -4192, 2804, 0 }, { -178, 4192, -2803, 0 }, { 178, 4192, 2804, 0 } }, { 4089, 0, -260, 0 }, { 0, 0, ONE, 0 }, 5042, 0, 17, 18, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 704, -3680, -3969, 0 }, { { 178, -4192, 2804, 0 }, { -178, -4192, -2803, 0 }, { 178, 4192, 2804, 0 }, { -178, 4192, -2803, 0 } }, { -4090, 0, 259, 0 }, { 0, 0, ONE, 0 }, 5042, 0, 18, 17, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Empty resource list for underground parking's default placement variant 1.
///
/// Contains only AREA_PLACEMENT_END; selecting this variant loads/spawns no
/// placed actors. The published variant borrows this list while the overlay is loaded.
static AreaResource _gShelterB1UndergroundParkingVariant1Resources[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Empty placement list for underground parking's default placement variant 1.
///
/// Contains only AREA_PLACEMENT_END; selecting this variant loads/spawns no
/// placed actors. The published variant borrows this list while the overlay is loaded.
static AreaPlacement _gShelterB1UndergroundParkingVariant1Placements[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Empty resource list for underground parking's placement variant 5.
///
/// The sole AREA_PLACEMENT_END record queues no actor files. The published
/// variant borrows this list while the overlay is loaded.
static AreaResource _gShelterB1UndergroundParkingVariant5Resources[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Empty placement list for underground parking's placement variant 5.
///
/// The sole AREA_PLACEMENT_END record spawns no actors. The published variant
/// borrows this list while the overlay is loaded.
static AreaPlacement _gShelterB1UndergroundParkingVariant5Placements[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Soldier B's resource list for underground parking's placement variant 21.
///
/// Entry 116 loads global-library base file 161500 and selects body descriptor 0.
/// The second record is AREA_PLACEMENT_END. Keep this list and the actor task
/// table loaded for area loading, spawning and model-flag updates.
static AreaResource _gShelterB1UndergroundParkingSoldierBResources[2] = {
    { SHELTER_B1_UNDERGROUND_PARKING_SOLDIER_B_ENTRY_ID, SHELTER_B1_UNDERGROUND_PARKING_SOLDIER_B_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor161500SoldierBTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Soldier B's placement list for underground parking's placement variant 21.
///
/// Entry 116 selects the matching resource and actor parameter row 0. The zero
/// spawn argument selects unarmed idle without the optional rifle child.
/// Position uses world game units and yaw uses 4096 units per turn. The base
/// file is relocated by two texture pages (64 VRAM words each) and four CLUT
/// rows. AREA_PLACEMENT_END ends the list; keep it live while the actor borrows it.
static AreaPlacement _gShelterB1UndergroundParkingSoldierBPlacements[2] = {
    { SHELTER_B1_UNDERGROUND_PARKING_SOLDIER_B_ENTRY_ID, 0, 0, 3700, 0, -114, 2488, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB1UndergroundParkingAreaVariants[SHELTER_B1_UNDERGROUND_PARKING_AREA_VARIANT_COUNT] = {
    { NULL, NULL },
    { _gShelterB1UndergroundParkingVariant1Placements, _gShelterB1UndergroundParkingVariant1Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { _gShelterB1UndergroundParkingVariant5Placements, _gShelterB1UndergroundParkingVariant5Resources },
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
    { _gShelterB1UndergroundParkingSoldierBPlacements, _gShelterB1UndergroundParkingSoldierBResources },
};

/// Twelve interaction quads selected by underground parking room 1.
///
/// Includes the access-tunnel warp, map handoff and selector/scene/CAP commands.
/// Origins, relative corners and radii use room game units; normals use 4096
/// per unit. All quads require facing. Activation binds coordinates and links
/// the final LAST record too; keep mutable records loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRoom1ActionTriggers[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, SHELTER_B1_UNDERGROUND_PARKING_ACCESS_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4076, 0, -401, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4960, -64, 32, 0 }, { { -1568, 0, -2928, 0 }, { 1568, 0, -2928, 0 }, { -1568, 0, 2928, 0 }, { 1568, 0, 2928, 0 } }, { 0, 4113, 0, 0 }, { -ONE, 0, 0, 0 }, 3318, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3568, -64, 4064, 0 }, { { -624, 0, -720, 0 }, { 624, 0, -720, 0 }, { -624, 0, 720, 0 }, { 624, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 799, 0, -4017, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1712, -64, -4704, 0 }, { { -656, 0, -2288, 0 }, { 656, 0, -2288, 0 }, { -656, 0, 2288, 0 }, { 656, 0, 2288, 0 } }, { 0, 4110, 0, 0 }, { ONE, 0, 0, 0 }, 2374, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -64, -2656, 0 }, { { -2080, 0, -560, 0 }, { 2080, 0, -560, 0 }, { -2080, 0, 560, 0 }, { 2080, 0, 560, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, ONE, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -160, -64, 4368, 0 }, { { -656, 0, -1376, 0 }, { 656, 0, -1376, 0 }, { -656, 0, 1376, 0 }, { 656, 0, 1376, 0 } }, { 0, 4100, 0, 0 }, { -ONE, 0, 0, 0 }, 1519, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1936, -64, 3216, 0 }, { { -2272, 0, -496, 0 }, { 2272, 0, -496, 0 }, { -2272, 0, 496, 0 }, { 2272, 0, 496, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -ONE, 0 }, 2318, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2106, 0, 3512, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3040, -64, 4688, 0 }, { { -576, 0, -992, 0 }, { 576, 0, -992, 0 }, { -576, 0, 992, 0 }, { 576, 0, 992, 0 } }, { 0, 4095, 0, 0 }, { 3920, 0, -1189, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Twelve interaction quads selected by underground parking room 2.
///
/// Includes the access-tunnel warp, map handoff and selector/scene/CAP commands.
/// Origins, relative corners and radii use room game units; normals use 4096
/// per unit. All quads require facing. Activation binds coordinates and links
/// the final LAST record too; keep mutable records loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRoom2ActionTriggers[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, SHELTER_B1_UNDERGROUND_PARKING_ACCESS_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -ONE, 0, 0, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5536, -64, 32, 0 }, { { -1376, 0, -1360, 0 }, { 1376, 0, -1360, 0 }, { -1376, 0, 1360, 0 }, { 1376, 0, 1360, 0 } }, { 0, 4101, 0, 0 }, { -ONE, 0, 0, 0 }, 1932, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -64, 3936, 0 }, { { -688, 0, -720, 0 }, { 688, 0, -720, 0 }, { -688, 0, 720, 0 }, { 688, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { 798, 0, -4017, 0 }, 995, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3296, -64, -2784, 0 }, { { -1760, 0, -480, 0 }, { 1760, 0, -480, 0 }, { -1760, 0, 480, 0 }, { 1760, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, ONE, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1760, -64, -4448, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1792, -64, 3168, 0 }, { { -2208, 0, -480, 0 }, { 2208, 0, -480, 0 }, { -2208, 0, 480, 0 }, { 2208, 0, 480, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -ONE, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -192, -64, 4304, 0 }, { { 480, 0, -1296, 0 }, { 480, 0, 1296, 0 }, { -480, 0, -1296, 0 }, { -480, 0, 1296, 0 } }, { 0, 4117, 0, 0 }, { -ONE, 0, 0, 0 }, 1378, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -1752, 0, 3703, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3024, -64, 4640, 0 }, { { -560, 0, -912, 0 }, { 560, 0, -912, 0 }, { -560, 0, 912, 0 }, { 560, 0, 912, 0 } }, { 0, ONE, 0, 0 }, { 3973, 0, -996, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction regions selected by underground parking room 3.
///
/// The large CAP region uses an unconditional quad; other records require facing.
/// Includes the access-tunnel warp and map handoff. Positions, relative corners
/// and radii use room game units; normals use 4096 per unit. The final LAST
/// record is active too. Setup binds coordinates, hit tests update these records,
/// and pending actions link them; keep the storage loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRoom3ActionTriggers[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, SHELTER_B1_UNDERGROUND_PARKING_ACCESS_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4091, 0, 201, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8000, -64, 32, 0 }, { { -3616, 0, -2928, 0 }, { 3616, 0, -2928, 0 }, { -3616, 0, 2928, 0 }, { 3616, 0, 2928, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, ONE, 0 }, 4636, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -3424, -64, 3936, 0 }, { { -640, 0, -720, 0 }, { 640, 0, -720, 0 }, { -640, 0, 720, 0 }, { 640, 0, 720, 0 } }, { 0, 4102, 0, 0 }, { 601, 0, -4052, 0 }, 962, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3424, -64, -2816, 0 }, { { -1824, 0, -480, 0 }, { 1824, 0, -480, 0 }, { -1824, 0, 480, 0 }, { 1824, 0, 480, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, ONE, 0 }, 1885, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1728, -64, -4176, 0 }, { { -480, 0, -1584, 0 }, { 480, 0, -1584, 0 }, { -480, 0, 1584, 0 }, { 480, 0, 1584, 0 } }, { 0, 4101, 0, 0 }, { ONE, 0, 0, 0 }, 1654, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1792, -64, 3232, 0 }, { { -2208, 0, -480, 0 }, { 2208, 0, -480, 0 }, { -2208, 0, 480, 0 }, { 2208, 0, 480, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -ONE, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -208, -64, 4384, 0 }, { { -496, 0, -1344, 0 }, { 496, 0, -1344, 0 }, { -496, 0, 1344, 0 }, { 496, 0, 1344, 0 } }, { 0, 4104, 0, 0 }, { -ONE, 0, 0, 0 }, 1431, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2276, 0, 3405, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2976, -64, 4720, 0 }, { { -576, 0, -896, 0 }, { 576, 0, -896, 0 }, { -576, 0, 896, 0 }, { 576, 0, 896, 0 } }, { 0, 4095, 0, 0 }, { 4052, 0, -602, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction regions selected by underground parking room 4.
///
/// Every quad requires facing, including the room-dependent CAP regions.
/// Includes the access-tunnel warp and map handoff. Positions, relative corners
/// and radii use room game units; normals use 4096 per unit. The final LAST
/// record is active too. Setup binds coordinates, hit tests update these records,
/// and pending actions link them; keep the storage loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRoom4ActionTriggers[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, SHELTER_B1_UNDERGROUND_PARKING_ACCESS_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4017, 0, -799, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4688, -64, 32, 0 }, { { -816, 0, -624, 0 }, { 816, 0, -624, 0 }, { -816, 0, 624, 0 }, { 816, 0, 624, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3424, -64, 3936, 0 }, { { -640, 0, -720, 0 }, { 640, 0, -720, 0 }, { -640, 0, 720, 0 }, { 640, 0, 720, 0 } }, { 0, 4102, 0, 0 }, { 1380, 0, -3857, 0 }, 962, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3280, -64, -2752, 0 }, { { -1744, 0, -480, 0 }, { 1744, 0, -480, 0 }, { -1744, 0, 480, 0 }, { 1744, 0, 480, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, ONE, 0 }, 1805, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1632, -64, -4240, 0 }, { { -464, 0, -1616, 0 }, { 464, 0, -1616, 0 }, { -464, 0, 1616, 0 }, { 464, 0, 1616, 0 } }, { 0, 4107, 0, 0 }, { ONE, 0, 0, 0 }, 1678, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1776, -64, 3072, 0 }, { { -2176, 0, -480, 0 }, { 2176, 0, -480, 0 }, { -2176, 0, 480, 0 }, { 2176, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -ONE, 0 }, 2217, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -160, -64, 4608, 0 }, { { -464, 0, -1616, 0 }, { 464, 0, -1616, 0 }, { -464, 0, 1616, 0 }, { 464, 0, 1616, 0 } }, { 0, 4107, 0, 0 }, { -ONE, 0, 0, 0 }, 1678, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3552, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2276, 0, 3405, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3040, -64, 4656, 0 }, { { -544, 0, -896, 0 }, { 544, 0, -896, 0 }, { -544, 0, 896, 0 }, { 544, 0, 896, 0 } }, { 0, 4098, 0, 0 }, { 3784, 0, -1568, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction regions selected by underground parking room 5.
///
/// Includes the directed room-CAP choice that can reveal room 6. All quads require facing.
/// Includes the access-tunnel warp and map handoff. Positions, relative corners
/// and radii use room game units; normals use 4096 per unit. The final LAST
/// record is active too. Setup binds coordinates, hit tests update these records,
/// and pending actions link them; keep the storage loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRoom5ActionTriggers[12] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, SHELTER_B1_UNDERGROUND_PARKING_ACCESS_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -3973, 0, -995, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -64, 3936, 0 }, { { -688, 0, -720, 0 }, { 688, 0, -720, 0 }, { -688, 0, 720, 0 }, { 688, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { 995, 0, -3973, 0 }, 995, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4896, -64, 64, 0 }, { { -596, 0, -1135, 0 }, { 588, 0, -1138, 0 }, { -589, 0, 1137, 0 }, { 595, 0, 1134, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_ROOM_CAP, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3408, -64, -2752, 0 }, { { -1840, 0, -480, 0 }, { 1840, 0, -480, 0 }, { -1840, 0, 480, 0 }, { 1840, 0, 480, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, ONE, 0 }, 1898, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1792, -64, -4352, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1680, -64, 3168, 0 }, { { -2112, 0, -480, 0 }, { 2112, 0, -480, 0 }, { -2112, 0, 480, 0 }, { 2112, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -ONE, 0 }, 2157, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -192, -64, 4432, 0 }, { { 480, 0, -1360, 0 }, { 480, 0, 1360, 0 }, { -480, 0, -1360, 0 }, { -480, 0, 1360, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 1436, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3552, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -1752, 0, 3703, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2976, -64, 4720, 0 }, { { -608, 0, -928, 0 }, { 608, 0, -928, 0 }, { -608, 0, 928, 0 }, { 608, 0, 928, 0 } }, { 0, 4099, 0, 0 }, { 4051, 0, -601, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction regions selected by underground parking room 6.
///
/// Includes garage choices, direct CAP command 14 and three access-card event regions.
/// Includes the access-tunnel warp and map handoff. Positions, relative corners
/// and radii use room game units; normals use 4096 per unit. The final LAST
/// record is active too. Setup binds coordinates, hit tests update these records,
/// and pending actions link them; keep the storage loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRoom6ActionTriggers[20] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, SHELTER_B1_UNDERGROUND_PARKING_ACCESS_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4076, 0, -401, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 112, -64, 1120, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, ONE, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3936, -64, 4832, 0 }, { { -1024, 0, -1648, 0 }, { 1728, 0, -1648, 0 }, { -1024, 0, 464, 0 }, { 1728, 0, 464, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, ONE, 0 }, 2387, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 96, -64, -1120, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2064, -64, -48, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -ONE, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2256, -64, -48, 0 }, { { -368, 0, -1392, 0 }, { 368, 0, -1392, 0 }, { -368, 0, 1392, 0 }, { 368, 0, 1392, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1436, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3808, -64, -368, 0 }, { { -368, 0, -2176, 0 }, { 368, 0, -2176, 0 }, { -368, 0, 2176, 0 }, { 368, 0, 2176, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 2202, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_0E, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3776, -64, 3776, 0 }, { { -1376, 0, -592, 0 }, { 1376, 0, -592, 0 }, { -1376, 0, 592, 0 }, { 1376, 0, 592, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -ONE, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2880, -64, 4480, 0 }, { { -672, 0, -1328, 0 }, { 672, 0, -1328, 0 }, { -672, 0, 1328, 0 }, { 672, 0, 1328, 0 } }, { 0, 4098, 0, 0 }, { ONE, 0, 0, 0 }, 1487, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3840, -64, 3696, 0 }, { { -1376, 0, -1216, 0 }, { 1376, 0, -768, 0 }, { -1376, 0, 992, 0 }, { 1376, 0, 992, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 1832, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2576, -64, 4448, 0 }, { { -1008, 0, -1328, 0 }, { 1008, 0, -1328, 0 }, { -1008, 0, 1328, 0 }, { 1008, 0, 1328, 0 } }, { 0, 4100, 0, 0 }, { ONE, 0, 0, 0 }, 1664, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3937, -64, 4832, 0 }, { { -1024, 0, -1904, 0 }, { 2368, 0, -1904, 0 }, { -1024, 0, 848, 0 }, { 2368, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, ONE, 0 }, 3029, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1760, -64, -4512, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3840, -64, -2752, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, ONE, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -256, -64, 4384, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -ONE, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2112, -64, 3232, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -1380, 0, 3856, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction regions selected by underground parking room 7.
///
/// Includes garage choices and room-command 3; no access-card event regions remain.
/// Includes the access-tunnel warp and map handoff. Positions, relative corners
/// and radii use room game units; normals use 4096 per unit. The final LAST
/// record is active too. Setup binds coordinates, hit tests update these records,
/// and pending actions link them; keep the storage loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRoom7ActionTriggers[16] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, SHELTER_B1_UNDERGROUND_PARKING_ACCESS_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -3536, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -4052, 0, -601, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 112, -64, 1120, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, ONE, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3424, -64, 3936, 0 }, { { -640, 0, -720, 0 }, { 640, 0, -720, 0 }, { -640, 0, 720, 0 }, { 640, 0, 720, 0 } }, { 0, 4102, 0, 0 }, { 1751, 0, -3703, 0 }, 962, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 96, -64, -1120, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2064, -64, -48, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -ONE, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2256, -64, -48, 0 }, { { -368, 0, -1392, 0 }, { 368, 0, -1392, 0 }, { -368, 0, 1392, 0 }, { 368, 0, 1392, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1436, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4096, -64, -2848, 0 }, { { -2416, 0, -368, 0 }, { 2416, 0, -368, 0 }, { -2416, 0, 368, 0 }, { 2416, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, ONE, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2112, -64, 3168, 0 }, { { -2432, 0, -416, 0 }, { 2432, 0, -416, 0 }, { -2432, 0, 416, 0 }, { 2432, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1728, -64, -4192, 0 }, { { -368, 0, -1392, 0 }, { 368, 0, -1392, 0 }, { -368, 0, 1392, 0 }, { 368, 0, 1392, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1436, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -224, -64, 4480, 0 }, { { -400, 0, -1424, 0 }, { 400, 0, -1424, 0 }, { -400, 0, 1424, 0 }, { 400, 0, 1424, 0 } }, { 0, 4108, 0, 0 }, { -ONE, 0, 0, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3776, -64, -3520, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { -2106, 0, 3513, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3744, -64, -192, 0 }, { { -400, 0, -2608, 0 }, { 400, 0, -2608, 0 }, { -400, 0, 2608, 0 }, { 400, 0, 2608, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 2635, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3008, -64, 4688, 0 }, { { -512, 0, -960, 0 }, { 512, 0, -960, 0 }, { -512, 0, 960, 0 }, { 512, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { 3856, 0, -1381, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction regions selected by underground parking room 8.
///
/// Includes dialogue-loop command 22, direct CAP command 23 and Soldier B remark regions.
/// Includes the access-tunnel warp and map handoff. Positions, relative corners
/// and radii use room game units; normals use 4096 per unit. The final LAST
/// record is active too. Setup binds coordinates, hit tests update these records,
/// and pending actions link them; keep the storage loaded until unlinked.
static WorldCollisionTrigger _gShelterB1UndergroundParkingRoom8ActionTriggers[11] = {
    { NULL, NULL, NULL, { 1984, -48, -7360, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, SHELTER_B1_UNDERGROUND_PARKING_ACCESS_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4448, -48, 128, 0 }, { { 480, 0, -1760, 0 }, { 480, 0, 1760, 0 }, { -480, 0, -1760, 0 }, { -480, 0, 1760, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2496, -64, -240, 0 }, { { -608, 0, -848, 0 }, { 608, 0, -848, 0 }, { -608, 0, 848, 0 }, { 608, 0, 848, 0 } }, { 0, 4094, 0, 0 }, { 4076, 0, 401, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_DIALOGUE_LOOP, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3808, -64, 3936, 0 }, { { -1024, 0, -720, 0 }, { 1024, 0, -720, 0 }, { -1024, 0, 720, 0 }, { 1024, 0, 720, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, ONE, 0 }, 1247, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -5344, 0 }, { { -1024, 0, -480, 0 }, { 1024, 0, -480, 0 }, { -1024, 0, 480, 0 }, { 1024, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1553, -64, -2208, 0 }, { { -624, 0, -928, 0 }, { 624, 0, -928, 0 }, { -624, 0, 928, 0 }, { 624, 0, 928, 0 } }, { 0, 4120, 0, 0 }, { ONE, 0, 0, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3503, -64, -688, 0 }, { { -624, 0, -432, 0 }, { 624, 0, -432, 0 }, { -624, 0, 432, 0 }, { 624, 0, 432, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -ONE, 0 }, 757, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_SOLDIER_REMARK, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1632, -64, -4320, 0 }, { { -624, 0, -928, 0 }, { 624, 0, -928, 0 }, { -624, 0, 928, 0 }, { 624, 0, 928, 0 } }, { 0, 4120, 0, 0 }, { ONE, 0, 0, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2720, -64, 864, 0 }, { { -624, 0, -832, 0 }, { 624, 0, -832, 0 }, { -624, 0, 832, 0 }, { 624, 0, 832, 0 } }, { 0, 4095, 0, 0 }, { -201, 0, -4091, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3936, -64, 416, 0 }, { { -1616, 0, -1568, 0 }, { 240, 0, -1568, 0 }, { -1616, 0, -160, 0 }, { 240, 0, -160, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_SOLDIER_REMARK, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2848, -64, -288, 0 }, { { -496, 0, -496, 0 }, { 496, 0, -496, 0 }, { -496, 0, 272, 0 }, { 496, 0, 272, 0 } }, { 0, 4097, 0, 0 }, { -ONE, 0, 0, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B1_UNDERGROUND_PARKING_ACTION_SOLDIER_REMARK, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Minimum ambient RGB levels for the underground parking's 24 logical views.
///
/// Entry zero holds the count; entries 1..24 are indexed by the logical view,
/// independently of the camera/image remapping. Channels use 16 units per 8-bit
/// colour level. View 17 has brighter minima; all others have (16, 16, 16).
/// Lighting borrows the table without modifying it while this overlay is loaded.
static WorldCoordRoomAmbientEntry _gShelterB1UndergroundParkingViewAmbientTable[SHELTER_B1_UNDERGROUND_PARKING_VIEW_COUNT + 1] = {
    { .viewCount = ARRAY_SIZE(_gShelterB1UndergroundParkingViewAmbientTable) - 1 },
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

/// Paired cue-2 bases in the loaded type-1 bank; cue 1 adds one.
enum {
    SHELTER_B1_UNDERGROUND_PARKING_FOOTSTEP_WALK_CUE2_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    SHELTER_B1_UNDERGROUND_PARKING_FOOTSTEP_RUN_CUE2_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
};

/// Footstep cue bases for underground parking surface class 1.
///
/// Walk and scripted jumps share the walking pair; running uses the running pair.
/// These requests select the currently loaded type-1 bank. Cue 1 adds one to the
/// base; companion playback adds 100. The surface record borrows this storage.
static WorldCollisionFootstepSounds _gShelterB1UndergroundParkingSurface1FootstepSounds = {
    SHELTER_B1_UNDERGROUND_PARKING_FOOTSTEP_WALK_CUE2_BASE,
    SHELTER_B1_UNDERGROUND_PARKING_FOOTSTEP_RUN_CUE2_BASE,
    SHELTER_B1_UNDERGROUND_PARKING_FOOTSTEP_WALK_CUE2_BASE,
};

/// Blocking collision policy without footstep cues for surface classes 0 and 3..7.
///
/// Allows weapon impacts and applies contact pushback. The leading serialized
/// byte stays zero; its role is unproven. Storage belongs to the loaded room overlay.
static WorldCollisionSurfaceProperties _gShelterB1UndergroundParkingDefaultSurfaceProperties = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL };

/// Blocking collision policy with footstep cues for surface class 1.
///
/// Allows weapon impacts, applies contact pushback and borrows the walking,
/// running and scripted-jump cue bases. The leading byte's role is unproven.
static WorldCollisionSurfaceProperties _gShelterB1UndergroundParkingSurface1Properties = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gShelterB1UndergroundParkingSurface1FootstepSounds };

/// Separate silent blocking policy for underground parking surface class 2.
///
/// Allows weapon impacts and applies contact pushback, with no footstep cues.
/// The leading serialized byte stays zero; its role is unproven.
static WorldCollisionSurfaceProperties _gShelterB1UndergroundParkingSurface2Properties = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL };

WorldCollisionSurfaceProperties* gShelterB1UndergroundParkingSurfacePropertiesByClass[8] = {
    &_gShelterB1UndergroundParkingDefaultSurfaceProperties,
    &_gShelterB1UndergroundParkingSurface1Properties,
    &_gShelterB1UndergroundParkingSurface2Properties,
    &_gShelterB1UndergroundParkingDefaultSurfaceProperties,
    &_gShelterB1UndergroundParkingDefaultSurfaceProperties,
    &_gShelterB1UndergroundParkingDefaultSurfaceProperties,
    &_gShelterB1UndergroundParkingDefaultSurfaceProperties,
    &_gShelterB1UndergroundParkingDefaultSurfaceProperties,
};

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
///
/// Refill commits the live weapon load to capacity immediately, then this value
/// rises from the old quantity by one quarter-unit per callback and clamps at
/// capacity. The panel uses it for both the number and its meter, through closure.
static s32 _gShopRefillDisplayQuantityFixed = 0;

/// Borrowed weapon-supply catalog row currently displayed by the refill panel.
///
/// NULL until the first carried supply is selected. Retained while the panel
/// closes; gameplay and its catalog must remain loaded throughout the session.
static const EquipmentWeaponSupply* _gShopRefillSupply = NULL;

/// Borrowed selector-panel child handle, valid from successful spawn through exit polling.
///
/// The session overwrites it on each opening and does not clear it after polling
/// releases the child. Allocation must succeed before the next poll uses it.
static Task* _gShelterB1UndergroundParkingPanelTask = NULL;

/// Fade handshake borrowed by the 30-frame fade to black before garage departure.
///
/// The departure sets subtract blend, running phase and ramp length, then lends
/// the record to the resident fade task. Storage remains live with this overlay.
static ScreenFade _gShelterB1UndergroundParkingDepartureFade = { 0 };

Task* gRoomCutsceneSoundTask = NULL;

/// Room-event latch (0 idle, 1 active), retained across selector room changes.
///
/// Set before starting the fast light pulse and ambience task; the scene command
/// clears it. While set it gates departure resolution and chooses the event's
/// CAP path. It remains set if spawning fails; it is not a task-liveness test.
static s32 _gShelterB1UndergroundParkingAmbienceEventActive = false;

/// Persistent scene record for room command 13, with eight trailing unproven bytes.
///
/// The command supplies view 20, four sound events and CAP/skip choices. The
/// asynchronous cutscene runner borrows `rec`; leave it unchanged and keep the
/// overlay loaded until the runner finishes. No access reaches the trailing bytes.
static RoomCutsceneRecStorage _gShelterB1UndergroundParkingCutsceneRecStorage = { { 0 }, { 0 } };

RoomDeparture gRoomDeparture = { 0, 0, 0, 0, 0, { 0, 0 }, 0 };

/// Committed selector-switch pattern (0..15), or 0xFF before any selection is applied.
///
/// Room initialization sets the sentinel so Enter applies even pending zero.
/// Commit copies the pending pattern; cancellation retains this byte. Its low
/// nibble indexes the pattern-to-room table. Stored for this overlay's lifetime.
static u8 _gShelterB1UndergroundParkingPanelCommittedPattern = 0;

/// Pending selector-switch pattern in 0..15 (red 8, green 4, blue 2, yellow 1).
///
/// Reset to all switches off on room initialization, toggled on confirmed switch
/// commands, and copied to the committed byte on Enter. Cancellation leaves the
/// pending pattern intact for reopening; rendering indexes the marker table
/// directly with it. Stored as one unsigned byte for the room's lifetime.
static u8 _gShelterB1UndergroundParkingPanelPendingPattern = 0;

/// Retained, unreferenced halfword after the panel-selection bytes; its role is unproven.
static u16 D_shelter_b1_underground_parking_8018D78A = 0xCCEE;

/// Cyan room-light pulse selection (0 slow, nonzero fast).
///
/// Keeps all 16 stored bits; the room's setter accepts signed halfwords.
static u16 _gShelterB1UndergroundParkingFastLightPulse = SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_SLOW;

static void _shelterB1UndergroundParkingDrawPanelIndicators(void);

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x3C\x9E"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

static void _glowDrawBeam(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);
static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

void shelterB1UndergroundParkingTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

/// Retained byte following this room's weapon-supply refill title terminator.
///
/// One-byte string-literal binding for `shop.inc.c`; label drawing does not read it.
#define SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES "\x6F"
#include "../../shared/room/shop.inc.c"
#undef SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES
#undef SHOP_REFILL_DISPLAY_QUANTITY
#undef SHOP_REFILL_SUPPLY

#include "../../shared/room/room_event_departure_task.inc.c"

#include "../../shared/room/room_cutscene_task.inc.c"

/// Holds player control for the boundary caption and queues its completion task.
///
/// Selects CAP slot 10, variant key 1, in the current display, then spawns a
/// bodyless waiter that resumes the player once CAP is idle. CAP and room task
/// resources must remain loaded. Playback/spawn results are ignored; the hold
/// is already active if either request fails.
static inline void _shelterB1UndergroundParkingStartCaptionHandoff(void)
{
    enum { SHELTER_B1_UNDERGROUND_PARKING_CAP_HANDOFF_BOUNDARY_VARIANT = 1 };

    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    capStartSequenceSlot(SHELTER_B1_UNDERGROUND_PARKING_CAP_SLOT_HANDOFF, CAP_PLAYBACK_IN_PLACE, SHELTER_B1_UNDERGROUND_PARKING_CAP_HANDOFF_BOUNDARY_VARIANT);
    taskSpawnFromTable(_gShelterB1UndergroundParkingTaskDescs, SHELTER_B1_UNDERGROUND_PARKING_TASK_RESUME_AFTER_CAPTION, 0, 0);
}

/// Checks the late-room boundary for a held-direction caption request each frame.
///
/// Requires the live player model in the room coordinate frame. With CAP idle,
/// scripted control and the attachment wheel inactive, X must be below -4710
/// and Z in [-1999, 2000). Up uses yaw 2561..3583, Down 513..1535, in 4096
/// units per turn. Both tests run independently; the receiver is unused.
static void _shelterB1UndergroundParkingCheckCaptionHandoff(Task* roomTask)
{
    // Room-coordinate limits and normalized yaw windows (4096 units per turn).
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_X_LIMIT        = -4710,
        SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_Z_MIN          = -1999,
        SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_Z_LIMIT        = 2000,
        SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_UP_YAW_FIRST   = 2561,
        SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_DOWN_YAW_FIRST = 513,
        SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_YAW_COUNT      = 1023,
    };

    Task*      playerTask;
    GameActor* player;
    GfxCoord*  rootCoord;
    s32        roomZ;
    s32        yaw;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player     = playerTask->work;
    rootCoord  = playerTask->extra.tmd->coords;
    if ((player->mode != GAME_ACTOR_MODE_SCRIPTED) && (capIsBusy() == 0) && (gGameSession->location.roomState >= SHELTER_B1_UNDERGROUND_PARKING_ROOM_CAR_SCENE) &&
        (rootCoord->coord.t[0] < SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_X_LIMIT)) {
        roomZ = rootCoord->coord.t[2];
        if (roomZ < SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_Z_LIMIT) {
            if ((roomZ >= SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_Z_MIN) && (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                yaw = (u16)player->rotation.vy & ACTOR_TRANSFORM_ANGLE_MASK;
                if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP) != 0) {
                    if ((u32)(yaw - SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_UP_YAW_FIRST) < SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_YAW_COUNT) {
                        _shelterB1UndergroundParkingStartCaptionHandoff();
                    }
                }
                if ((padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_DOWN) != 0) && ((u32)(yaw - SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_DOWN_YAW_FIRST) < SHELTER_B1_UNDERGROUND_PARKING_HANDOFF_YAW_COUNT)) {
                    _shelterB1UndergroundParkingStartCaptionHandoff();
                }
            }
        }
    }
}

/// Room-task state handlers: initialization, boundary-caption checks and teardown.
///
/// State must be 0..2. Dispatch copies all three callbacks into a local table;
/// keep the table at its source position and the overlay loaded for the task.
static const TaskFuncTable3 _gShelterB1UndergroundParkingRoomStates = {
    {
        _shelterB1UndergroundParkingRoomInitialize,
        _shelterB1UndergroundParkingCheckCaptionHandoff,
        taskKill,
    },
};

/// Handles the room trigger's directed action using a borrowed four-byte request.
///
/// Action 1 starts Soldier B's remark in variant 21; 10 handles the map handoff,
/// 11 selects room-dependent CAP commands, and 12 selects the garage choice or scene.
/// The full unsigned argument byte is used by action 10. The request is consumed
/// synchronously and never retained. Receiver, message ID and second word are
/// unused; the reply is always zero.
static s32 _shelterB1UndergroundParkingHandleDirectionAction(Task* roomTask, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    if (request->actionId == SHELTER_B1_UNDERGROUND_PARKING_ACTION_SOLDIER_REMARK && gGameSession->location.variant == SHELTER_B1_UNDERGROUND_PARKING_VARIANT_SOLDIER_B) {
        actor161500StartSoldierBRemark();
    }
    if (request->actionId == SHELTER_B1_UNDERGROUND_PARKING_ACTION_MAP_HANDOFF) {
        if (request->argument == 1 && gGameSession->location.roomState < SHELTER_B1_UNDERGROUND_PARKING_ROOM_CAR_SCENE) {
            capStartSequenceSlot(SHELTER_B1_UNDERGROUND_PARKING_CAP_SLOT_HANDOFF, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING, 2);
        }
    }
    if (request->actionId == SHELTER_B1_UNDERGROUND_PARKING_ACTION_ROOM_CAP) {
        switch (gGameSession->location.roomState) {
            case 2:
                capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_08);
                break;
            case 3:
                capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_0F);
                break;
            case 4:
                capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_09);
                break;
            case 5:
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_0C);
                taskSpawnFromTable(_gShelterB1UndergroundParkingTaskDescs, SHELTER_B1_UNDERGROUND_PARKING_TASK_ROOM6_CHOICE, 0, 0);
                break;
            case 6:
            case 7:
            case 8:
                capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_0E);
                break;
        }
    }
    if (request->actionId == SHELTER_B1_UNDERGROUND_PARKING_ACTION_GARAGE_CHOICE) {
        switch (gGameSession->location.roomState) {
            case 6:
                capStartSequenceSlot(SHELTER_B1_UNDERGROUND_PARKING_CAP_SLOT_GARAGE_CHOICE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
                break;
            case 7:
                if (_gShelterB1UndergroundParkingAmbienceEventActive != false) {
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_1E);
                } else if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) < SHELTER_B1_UNDERGROUND_PARKING_FINAL_CHAPTER) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    capStartSequenceSlot(SHELTER_B1_UNDERGROUND_PARKING_CAP_SLOT_GARAGE_CHOICE, CAP_PLAYBACK_DISPLAY_TRANSITION, 1);
                    taskSpawnFromTable(_gShelterB1UndergroundParkingTaskDescs, SHELTER_B1_UNDERGROUND_PARKING_TASK_GARAGE_DEPARTURE, 0, 0);
                } else {
                    capStartSequenceSlot(SHELTER_B1_UNDERGROUND_PARKING_CAP_SLOT_GARAGE_CHOICE, CAP_PLAYBACK_DISPLAY_TRANSITION, 2);
                }
                break;
            case 8:
                capStartSequenceSlot(SHELTER_B1_UNDERGROUND_PARKING_CAP_SLOT_GARAGE_CHOICE, CAP_PLAYBACK_DISPLAY_TRANSITION, 2);
                break;
        }
    }
    return 0;
}

/// Holds and hides the player before starting the selector panel session.
///
/// Requires a live player model and loaded room/panel resources. The bodyless
/// session task starts the room-owned selector; leaving that panel restores
/// player control and drawing. Neither request is checked, so a failed spawn
/// leaves the player held and hidden.
static inline void _shelterB1UndergroundParkingStartPanelSession(void)
{
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    taskSpawnFromTable(_gShelterB1UndergroundParkingTaskDescs, SHELTER_B1_UNDERGROUND_PARKING_TASK_PANEL_SESSION, 0, 0);
}

/// Routes CAP room-command indices to playback, panel sessions and scenes.
///
/// Handles ROOM_MESSAGE_COMMAND. The signed command ID is an index in the
/// loaded CAP resources: 1 opens the selector when progress permits, 4/7 choose
/// room-dependent playback, 2/3/5 and 48 run alternate-file commands, 13 selects the recorded
/// scene, and 22 starts the dialogue loop. Scene tasks borrow the room's
/// persistent scene record. Unknown indices do nothing; the reply is always zero.
/// The receiver, message ID and second payload word are unused.
static s32 _shelterB1UndergroundParkingHandleCommand(Task* roomTask, s32 messageId, s32 commandId, s32 unused)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_5 = 5,

        SHELTER_B1_UNDERGROUND_PARKING_SCENE_VIEW                  = 20,
        SHELTER_B1_UNDERGROUND_PARKING_SCENE_CAP_RESOURCE          = 1,
        SHELTER_B1_UNDERGROUND_PARKING_SCENE_CAP_SLOT              = 1,
        SHELTER_B1_UNDERGROUND_PARKING_EVENT_CAP_SLOT              = 31,
        SHELTER_B1_UNDERGROUND_PARKING_SCENE_AFTER_COMMAND         = 9,
        SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_COMMAND_INTRO = 1,
        SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_COMMAND_4     = 4,
    };

    RoomCutsceneRec* scene;

    switch (commandId) {
        case SHELTER_B1_UNDERGROUND_PARKING_COMMAND_PANEL:
            if (gGameSession->location.roomState < SHELTER_B1_UNDERGROUND_PARKING_ROOM_CARD_ACCESS) {
                if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) == 0) {
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_01);
                } else if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_0E7) == 0) {
                    if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_0E8) == 0) {
                        capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_02);
                    } else {
                        _shelterB1UndergroundParkingStartPanelSession();
                    }
                } else {
                    _shelterB1UndergroundParkingStartPanelSession();
                }
            } else {
                capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_10);
            }
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_7:
            switch (gGameSession->location.roomState) {
                case 1:
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_07);
                    break;
                case 2:
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_08);
                    break;
                case 3:
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_0F);
                    break;
                case 4:
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_09);
                    break;
                case 6:
                case 7:
                case 8:
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_0E);
                    break;
            }
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ROOM_CAP_4:
            switch (gGameSession->location.roomState) {
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_04);
                    break;
                case 6:
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_05);
                    break;
                case 7:
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_06);
                    break;
                case 8:
                    capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_10);
                    break;
            }
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_2:
        case SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_3:
        case SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_5:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(_gShelterB1UndergroundParkingTaskDescs, SHELTER_B1_UNDERGROUND_PARKING_TASK_ALTERNATE_CAP, commandId, 0);
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_COMMAND_SCENE:
            // Prepare persistent scene storage before handing it to an asynchronous task.
            scene                  = &_gShelterB1UndergroundParkingCutsceneRecStorage.rec;
            scene->startSound      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x0B);
            scene->endSound        = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x0E);
            scene->sceneSound      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x0C);
            scene->afterSceneSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x0D);
            scene->view            = SHELTER_B1_UNDERGROUND_PARKING_SCENE_VIEW;
            if (_gShelterB1UndergroundParkingAmbienceEventActive == false) {
                if (gameFlagGetNibble(GAME_FLAG_UNDERGROUND_PARKING_FIRST_SCENE) == 0) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    gameFlagSetNibble(GAME_FLAG_UNDERGROUND_PARKING_FIRST_SCENE, 1);
                    taskSpawnFromTable(_gShelterB1UndergroundParkingTaskDescs, SHELTER_B1_UNDERGROUND_PARKING_TASK_ALTERNATE_CAP, SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_COMMAND_INTRO, 0);
                } else {
                    scene->capSlot   = SHELTER_B1_UNDERGROUND_PARKING_SCENE_CAP_SLOT;
                    scene->capFile   = SHELTER_B1_UNDERGROUND_PARKING_SCENE_CAP_RESOURCE;
                    scene->skipScene = 0;
                    taskSpawnFromTable(_gRoomCutsceneTaskDescs, ROOM_CUTSCENE_TASK_SCENE, SHELTER_B1_UNDERGROUND_PARKING_SCENE_AFTER_COMMAND, scene);
                }
            } else {
                _gShelterB1UndergroundParkingAmbienceEventActive = false;
                scene->capSlot                                   = SHELTER_B1_UNDERGROUND_PARKING_EVENT_CAP_SLOT;
                scene->capFile                                   = 0;
                scene->skipScene                                 = 1;
                taskSpawnFromTable(_gRoomCutsceneTaskDescs, ROOM_CUTSCENE_TASK_SCENE, commandId, scene);
            }
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_COMMAND_DIALOGUE_LOOP:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&_gShelterB1UndergroundParkingDialogueLoopTaskDesc, 0, commandId, 0);
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_COMMAND_ALTERNATE_CAP_4:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(_gShelterB1UndergroundParkingTaskDescs, SHELTER_B1_UNDERGROUND_PARKING_TASK_ALTERNATE_CAP, SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_COMMAND_4, 0);
            break;
    }
    return 0;
}

#include "../../shared/room/room_variants_resolve_departure.inc.c"

/// Waits for the garage choice, fades out and publishes the 1F garage departure.
///
/// Starts bodyless at state 0. Choice key 11 starts a 30-frame subtractive fade;
/// other keys resume the player and end the task. At zero countdown it updates
/// companion/music progress, resolves the destination in place, and starts the
/// departure task. The countdown is decremented even on that final invocation.
/// Room, fade and resolver storage must remain loaded throughout.
static void _shelterB1UndergroundParkingGarageDepartureTask(Task* task)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_WAIT_CAP                 = 0,
        SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_CHECK_CHOICE             = 1,
        SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_WAIT_FADE                = 2,
        SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_CHOICE_ACCEPT            = 11,
        SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_FADE_FRAMES              = 30,
        SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_FADE_TASK_BANK           = 1,
        SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_FADE_TASK_TYPE           = 49,
        SHELTER_B1_UNDERGROUND_PARKING_COMPANION_SCHEDULE_MINE_SHELTER    = 10,
        SHELTER_B1_UNDERGROUND_PARKING_COMPANION_SCHEDULE_SHELTER_NEO_ARK = 9,
        SHELTER_B1_UNDERGROUND_PARKING_MUSIC_OVERRIDE_SUBMARINE           = 1,
        SHELTER_B1_UNDERGROUND_PARKING_MUSIC_OVERRIDE_PARKING             = 2,
        SHELTER_B1_UNDERGROUND_PARKING_SCENE_EVENT_PARKING                = 27,
    };

    RoomDeparture       departure;
    RoomVariantResolver resolve;

    switch (task->state) {
        case SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_CHECK_CHOICE:
            if (capGetVariantKey() == SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_CHOICE_ACCEPT) {
                _gShelterB1UndergroundParkingDepartureFade.blend      = SCREEN_FADE_SUBTRACT;
                _gShelterB1UndergroundParkingDepartureFade.phase      = SCREEN_FADE_RUNNING;
                _gShelterB1UndergroundParkingDepartureFade.rampFrames = SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_FADE_FRAMES;
                taskSpawn(SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_FADE_TASK_BANK, SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_FADE_TASK_TYPE, 0, &_gShelterB1UndergroundParkingDepartureFade);
                task->killCountdown = SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_FADE_FRAMES;
                task->state++;
            } else {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_DEPARTURE_WAIT_FADE:
            if (task->killCountdown == 0) {
                if (gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE) == SHELTER_B1_UNDERGROUND_PARKING_COMPANION_SCHEDULE_MINE_SHELTER) {
                    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, SHELTER_B1_UNDERGROUND_PARKING_COMPANION_SCHEDULE_SHELTER_NEO_ARK);
                }
                if (gameFlagGetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE) == SHELTER_B1_UNDERGROUND_PARKING_MUSIC_OVERRIDE_SUBMARINE) {
                    gameFlagSetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE, SHELTER_B1_UNDERGROUND_PARKING_MUSIC_OVERRIDE_PARKING);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = SHELTER_B1_UNDERGROUND_PARKING_SCENE_EVENT_PARKING;
                }
                // Resolve the garage room from progress before publishing the departure.
                resolve             = _roomVariantResolveNeoArk;
                departure.stage     = GAME_STAGE_SHELTER_NEO_ARK;
                departure.area      = GAME_AREA_SHELTER_1F_PARKING_GARAGE;
                departure.roomState = 1;
                departure.warp      = 1;
                departure.sndEvent  = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, 0x08);
                departure.facing    = ROOM_DEPARTURE_SKIP_FACING;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                _roomVariantResolveDeparture(&departure, resolve);
                gRoomDeparture = departure;
                taskSpawnFromTable(&_gShelterB1UndergroundParkingDepartureTaskDesc, 0, 0, 0);
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

    viewIndex = gGameSession->location.view;
    // Logical view slots outside the table retain base pan and full level.
    if (viewIndex < ARRAY_SIZE(_gShelterB1UndergroundParkingAmbienceMixByView)) {
        panOffset   = _gShelterB1UndergroundParkingAmbienceMixByView[viewIndex].panOffset;
        attenuation = _gShelterB1UndergroundParkingAmbienceMixByView[viewIndex].attenuation / 2;
    } else {
        panOffset   = 0;
        attenuation = 0;
    }

    switch (task->state) {
        case SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_START:
            sndEvtRequestScriptStart(SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_SOUND, (s8)panOffset, (s8)attenuation);
            task->state = task->state + 1;
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_STATE_WAIT:
            if (_gShelterB1UndergroundParkingAmbienceEventActive == false) {
                _shelterB1UndergroundParkingSetFastLightPulse(SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_SLOW);
                sndEvtRequestScriptStop(SHELTER_B1_UNDERGROUND_PARKING_AMBIENCE_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(task);
                break;
            }
            // The live save still carries the requested view during a transition.
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view != gGameSession->location.view) {
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
#include "../../shared/room/room_variants_neo_ark.inc.c"
#undef ROOM_VARIANT_RESOLVE_NEO_ARK

/// Selector-panel state handlers from initialization through cancel or closing fade.
///
/// States 0..7 allocate, open, scan, open commands, handle a command, cancel,
/// commit and fade out. Dispatch copies all eight callbacks into a local table;
/// keep this table at its source position and the overlay loaded for the panel.
static const TaskFuncTable8 _gShelterB1UndergroundParkingPanelStates = {
    {
        _shelterB1UndergroundParkingPanelInitialize,
        _shelterB1UndergroundParkingPanelOpen,
        _shelterB1UndergroundParkingPanelScanHotspots,
        _shelterB1UndergroundParkingPanelOpenCommands,
        _shelterB1UndergroundParkingPanelHandleCommand,
        _shelterB1UndergroundParkingPanelCancel,
        _shelterB1UndergroundParkingPanelCommitSelection,
        _shelterB1UndergroundParkingPanelFadeOut,
    },
};

#include "../../shared/room/room_cutscene_sound_task.inc.c"

#include "../../shared/room/room_event_trigger_hit.inc.c"

/// Starts the electric-car scene when an access card is used at its room trigger.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`: Bowman's or Yoshida's Card is accepted
/// only in room 6 with a hit on a linked room-event trigger. Leaves the cards
/// and trigger hits intact and holds the HUD and room events for the scene.
/// Returns the item menu's used-notice reply on acceptance, otherwise refusal.
/// Acceptance does not check scene-task allocation. The receiver, message ID
/// and second payload word are unused.
static s32 _shelterB1UndergroundParkingUseKeyItem(Task* roomTask, s32 messageId, s32 itemId, s32 unused)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_ITEM_YOSHIDAS_CARD = 0x122,
    };

    if (itemId == INVENTORY_COLLECTION_ID_BOWMANS_CARD || itemId == SHELTER_B1_UNDERGROUND_PARKING_ITEM_YOSHIDAS_CARD) {
        if (gGameSession->location.roomState == SHELTER_B1_UNDERGROUND_PARKING_ROOM_CARD_ACCESS) {
            if (_roomEventTriggerHit()) {
                taskSpawnFromTableOnDefaultList(_gShelterB1UndergroundParkingTaskDescs, SHELTER_B1_UNDERGROUND_PARKING_TASK_ROOM7_SCENE, 0, 0);
                gGameSession->hideHud    = 1;
                gGameSession->eventState = 1;
                return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
            }
        }
    }
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a Shelter room transition and defers departures while ambience is active.
///
/// Copies the complete eight-byte request into a writable reply before resolving
/// the room; both pointers may name the same object. Returns DIRECT when the
/// event latch is clear, otherwise HANDLED. Execute requests in the latter case
/// start CAP command 30; queries suppress it. The pointers are borrowed only
/// through dispatch. Receiver and message ID are unused.
static s32 _shelterB1UndergroundParkingResolveRoomEvent(Task* roomTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (_gShelterB1UndergroundParkingAmbienceEventActive == false) {
        return ROOM_VARIANT_TRANSITION_DIRECT;
    }
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        capRunCommandWithTransition(SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_1E);
    }
    return ROOM_VARIANT_TRANSITION_HANDLED;
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

#include "../../shared/room/cap_dialogue_loop.inc.c"

/// Starts the selector panel and waits for its child task to be removed.
///
/// Bodyless states 0/1 start and poll the room-owned panel handle. Spawning
/// must succeed: the next state requires a live, non-NULL child. Polling
/// dispatches its requested exit before this task dies; the result is discarded.
static void _shelterB1UndergroundParkingPanelSessionTask(Task* task)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_SESSION_START = 0,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_SESSION_WAIT  = 1,
    };
    s32 childResult;

    switch (task->state) {
        case SHELTER_B1_UNDERGROUND_PARKING_PANEL_SESSION_START:
            _gShelterB1UndergroundParkingPanelTask = taskSpawnFromTable(&_gShelterB1UndergroundParkingPanelTaskDesc, 0, 0, 0);
            task->state++;
            return;
        case SHELTER_B1_UNDERGROUND_PARKING_PANEL_SESSION_WAIT:
            if (taskPollKill(_gShelterB1UndergroundParkingPanelTask, &childResult) != 0) {
                taskKill(task);
            }
            return;
    }
}

/// Waits for the parking choice and reveals the electric-car key on acceptance.
///
/// State 0 waits for CAP completion; state 1 reads its retained choice key.
/// Key 11 selects room 6 in the session and live save, requests object relinking,
/// starts the arrival script and records progress 1 and key identification.
/// Other keys resume player control. The task then dies; it owns no work.
static void _shelterB1UndergroundParkingResolveRoom6ChoiceTask(Task* task)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_CHOICE_STATE_WAIT     = 0,
        SHELTER_B1_UNDERGROUND_PARKING_CHOICE_STATE_RESOLVE  = 1,
        SHELTER_B1_UNDERGROUND_PARKING_CAP_CHOICE_ROOM6      = 0xB,
        SHELTER_B1_UNDERGROUND_PARKING_PROGRESS_ROOM6        = 1,
        SHELTER_B1_UNDERGROUND_PARKING_ITEM_ELECTRIC_CAR_KEY = 0x123,
    };
    s32 state = task->state;

    switch (state) {
        case SHELTER_B1_UNDERGROUND_PARKING_CHOICE_STATE_WAIT:
            if (capIsBusy() == 0) {
                task->state += 1;
            }
            return;
        case SHELTER_B1_UNDERGROUND_PARKING_CHOICE_STATE_RESOLVE:
            if (capGetVariantKey() == SHELTER_B1_UNDERGROUND_PARKING_CAP_CHOICE_ROOM6) {
                gGameSession->location.roomState                            = SHELTER_B1_UNDERGROUND_PARKING_ROOM_CARD_ACCESS;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = SHELTER_B1_UNDERGROUND_PARKING_ROOM_CARD_ACCESS;
                gGameSession->roomObjsDirty                                 = state;
                evsStartScript(_gShelterB1UndergroundParkingRoom6ArrivalScript, EVENT_SCRIPT_HUD_KEEP);
                gameFlagSetNibble(GAME_FLAG_UNDERGROUND_PARKING_STATE, SHELTER_B1_UNDERGROUND_PARKING_PROGRESS_ROOM6);
                itemSetIdentified(SHELTER_B1_UNDERGROUND_PARKING_ITEM_ELECTRIC_CAR_KEY, 1);
            } else {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            }
            taskKill(task);
            break;
    }
}

/// Starts the skippable electric-car scene that hands the parking area to room 7.
///
/// First invocation hides the display, suspends the player tick, starts the
/// script with its skip path and records progress 2 immediately. Both scripts
/// select room 7 and resume player control; the normal script also releases
/// the tick hold. The next task invocation kills this bodyless launcher rather
/// than waiting for script completion. Requires this overlay to stay loaded.
static void _shelterB1UndergroundParkingStartRoom7SceneTask(Task* task)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_SCENE_STATE_START = 0,
        SHELTER_B1_UNDERGROUND_PARKING_PROGRESS_ROOM7    = 2,
    };

    if (task->state == SHELTER_B1_UNDERGROUND_PARKING_SCENE_STATE_START) {
        // Hide the frame before the script releases the player tick for movement.
        SetDispMask(false);
        gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
        gGameSession->hideHud     = 1;
        evsStartScriptWithSkip(_gShelterB1UndergroundParkingElectricCarSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gShelterB1UndergroundParkingElectricCarSceneSkipScript);
        gameFlagSetNibble(GAME_FLAG_UNDERGROUND_PARKING_STATE, SHELTER_B1_UNDERGROUND_PARKING_PROGRESS_ROOM7);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING, 0);
        task->state += 1;
        return;
    }
    taskKill(task);
}

/// Resumes player control and ends this bodyless task once CAP playback is idle.
///
/// The room's caption handoff starts this after holding scripted player control.
static void _shelterB1UndergroundParkingResumePlayerAfterCaptionTask(Task* task)
{
    if (capIsBusy() == 0) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        taskKill(task);
    }
}

/// Runs the spawned CAP command from this bundle's third data resource.
///
/// Starts bodyless at state 0 with command index 1..5 in spawnArg1.
/// Selects data ordinal 2 and texture origin (768, 0) in VRAM pixels, waits for
/// playback to finish, resumes the player, then restores the default CAP resource
/// and texture selection. The bundle and room overlay must remain loaded.
static void _shelterB1UndergroundParkingAlternateCapTask(Task* task)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_START    = 0,
        SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_WAIT     = 1,
        SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_RESTORE  = 2,
        SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_RESOURCE = 2,
        SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_VRAM_X   = 768,
    };

    switch (task->state) {
        case SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_START:
            gCapFile = 0;
            capSelectLoadedFile(SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_RESOURCE);
            capSetTexturePage(SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_VRAM_X, 0);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_WAIT:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B1_UNDERGROUND_PARKING_ALTERNATE_CAP_RESTORE:
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
    gGameSession->location.roomState                            = roomNumber;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = roomNumber;
    gGameSession->roomObjsDirty                                 = 1;
    gGameSession->viewDirty                                     = 1;
}

/// Sets the player actor tick hold from a room script's byte argument.
///
/// Zero resumes the actor tick; any nonzero byte suspends it. The room's scene
/// script passes zero after hiding the display so scripted movement can resume.
static void _shelterB1UndergroundParkingSetPlayerTickHold(u8 holdPlayerTick)
{
    gPlayerActorStateTickHeld = holdPlayerTick;
}

/// Registers the room message receiver and initializes the selector and ambience.
///
/// Resets the pending panel selection, enables placed actor 0 in Soldier B's
/// variant, and starts the ambience once when the final chapter has begun.
/// Advances the room task to its per-frame handler; the room tables and overlay
/// must remain loaded while the registered receiver is live.
static void _shelterB1UndergroundParkingRoomInitialize(Task* roomTask)
{
    roomTask->msgTable = _gShelterB1UndergroundParkingRoomMessages;
    gameSetTaskSlot(roomTask, GAME_TASK_SLOT_ROOM);
    _shelterB1UndergroundParkingPanelResetSelection();
    if (gGameSession->location.variant == SHELTER_B1_UNDERGROUND_PARKING_VARIANT_SOLDIER_B) {
        sceneSetPlacedActorDrawMode(0, ACTOR_MESSAGE_DRAW_SHOW);
    }
    if ((gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) >= SHELTER_B1_UNDERGROUND_PARKING_FINAL_CHAPTER) && (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_123) == 0)) {
        gameFlagSetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_123, 1);
        _shelterB1UndergroundParkingStartAmbience();
    }
    roomTask->state = roomTask->state + 1;
}

void shelterB1UndergroundParkingRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB1UndergroundParkingRoomStates;
    states.funcs[task->state](task);
}

/// Latches the room event and starts its fast light pulse and ambience task once.
///
/// A set latch suppresses further starts. The latch is set before spawning and
/// stays set if task allocation fails; the ambience task stops when it is cleared.
static void _shelterB1UndergroundParkingStartAmbience(void)
{
    if (_gShelterB1UndergroundParkingAmbienceEventActive == false) {
        _gShelterB1UndergroundParkingAmbienceEventActive = true;
        _shelterB1UndergroundParkingSetFastLightPulse(SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_FAST);
        taskSpawnFromTable(_gShelterB1UndergroundParkingTaskDescs, SHELTER_B1_UNDERGROUND_PARKING_TASK_AMBIENCE, 0, 0);
    }
}

#include "../../shared/room/action_prompt_outline_rect.inc.c"

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
        _gShelterB1UndergroundParkingPanelMarkerPositions[_gShelterB1UndergroundParkingPanelPendingPattern].vx,
        _gShelterB1UndergroundParkingPanelMarkerPositions[_gShelterB1UndergroundParkingPanelPendingPattern].vy,
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
    if (_gShelterB1UndergroundParkingPanelPendingPattern & SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_RED) {
        _shelterB1UndergroundParkingDrawPanelGlow(-0x46, 0x54, SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_RED);
    }
    if (_gShelterB1UndergroundParkingPanelPendingPattern & SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_GREEN) {
        _shelterB1UndergroundParkingDrawPanelGlow(-0x28, 0x54, SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_GREEN);
    }
    if (_gShelterB1UndergroundParkingPanelPendingPattern & SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_BLUE) {
        _shelterB1UndergroundParkingDrawPanelGlow(-0xE, 0x54, SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_BLUE);
    }
    if (_gShelterB1UndergroundParkingPanelPendingPattern & SHELTER_B1_UNDERGROUND_PARKING_PANEL_SWITCH_YELLOW) {
        _shelterB1UndergroundParkingDrawPanelGlow(0xC, 0x54, SHELTER_B1_UNDERGROUND_PARKING_PANEL_INDICATOR_RADIUS, SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_YELLOW);
    }
    _shelterB1UndergroundParkingDrawPanelMapMarkers();
}

#include "../../shared/room/action_prompt_move_cursors.inc.c"

#include "../../shared/room/action_prompt_draw_cursor.inc.c"

/// Initializes and updates the selector panel's action cursors each frame.
///
/// State 0 resets both gameplay-owned prompts; state 1 moves and draws the
/// selected ports (spawn argument 1 selects port 0, 2 port 1, otherwise both).
/// The panel spawns port 0 and owns this bodyless child's lifetime. Requires
/// loaded cursor textures, the frame packet arena and ordering table. State
/// must be 0 or 1; there is no bounds check and no task work is allocated.
static void _shelterB1UndergroundParkingActionPromptTask(Task* task)
{
    TaskFunc states[2] = { _actionPromptResetDefault, _actionPromptMoveCursorsDefault };

    states[task->state](task);
}

/// Runs the parking selector panel from setup through cancellation or commitment.
///
/// State 0 allocates owned work and an action-cursor child; states 1..7 open the
/// display, scan hotspots, open commands, handle a command, cancel, commit and
/// fade out. State must be 0..7 and the overlay must remain loaded. Cancellation
/// kills the cursor; commitment kills it before the fade. Normal task teardown
/// frees the work after the final state requests removal.
static void _shelterB1UndergroundParkingPanelTask(Task* task)
{
    TaskFuncTable8 states;

    states = _gShelterB1UndergroundParkingPanelStates;
    states.funcs[task->state](task);
}

/// Allocates the selector panel's work and acquires its cursor and display hold.
///
/// Task teardown owns the zeroed 16-byte work. Failure to allocate it kills this
/// task before acquiring any holds. A successful start spawns port 0's cursor,
/// requests view 21, clears all hotspot hits, holds room/HUD activity and enters
/// the opening state. Cursor allocation failure retains the existing behavior.
static void _shelterB1UndergroundParkingPanelInitialize(Task* task)
{
    enum { SHELTER_B1_UNDERGROUND_PARKING_PANEL_VIEW          = 21,
           SHELTER_B1_UNDERGROUND_PARKING_PANEL_CURSOR_PORT_0 = 1 };

    _ShelterB1UndergroundParkingPanelWork* work;
    ActionPromptHotspot*                   hotspot;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                = taskSpawnFromTable(&_gShelterB1UndergroundParkingActionPromptTaskDesc, 0, SHELTER_B1_UNDERGROUND_PARKING_PANEL_CURSOR_PORT_0, 0);
    task->work                                             = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = SHELTER_B1_UNDERGROUND_PARKING_PANEL_VIEW;
    task->state++;
    work->field_0 = 0;
    displayAcquireMenuHold();
    for (hotspot = _gShelterB1UndergroundParkingPanelHotspots; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
        hotspot->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

/// Draws the selector indicators and starts its opening CAP command.
///
/// Requires initialized panel work and the port-0 prompt. Resets idle mode, aim
/// speed and integer pixel position to zero; the cursor's fixed-point position
/// is left for its movement task. Flag 0xE7 selects command 2 when clear or 3
/// when set, in the current loaded CAP table, then advances to hotspot scanning.
static void _shelterB1UndergroundParkingPanelOpen(Task* task)
{
    ActionPrompt* prompt = gActionPromptSlots;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_NORMAL;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    capRunCommand(gameFlagGetNibble(GAME_FLAG_SHELTER_B1_UNDERGROUND_PARKING_0E7) == 0 ? SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_02 : SHELTER_B1_UNDERGROUND_PARKING_CAP_COMMAND_03, CAP_PLAYBACK_IN_PLACE);
    task->state++;
}

/// Latches a confirmed panel choice and stops its cursor before opening commands.
///
/// Requires live panel work, its writable prompt and a readable hit entry.
/// Copies ID 1/2/4/8 (switch bit) or 16 (Enter) and the entry's prompt kind,
/// then enters OPEN_COMMANDS. Only the copied values survive the call; no
/// hotspot pointer is retained, and the pending switch pattern is untouched.
static inline void _shelterB1UndergroundParkingPanelLatchHotspot(Task* task, ActionPrompt* prompt, _ShelterB1UndergroundParkingPanelWork* work, const ActionPromptHotspot* hotspot)
{
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    work->choice        = hotspot->id;
    work->promptKind    = hotspot->promptKind;
    task->state         = SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_OPEN_COMMANDS;
}

/// Latches the first confirmed panel hotspot, or schedules cancellation.
///
/// Runs in state 2 with live panel work, the port-0 prompt and a terminated
/// hotspot table. CAP playback hides and stops the cursor. A fresh confirm
/// copies the hotspot ID and command kind before state 3; a fresh cancel enters
/// state 5. Confirm takes priority when both arrive on a hit. Double presses
/// do not select a hotspot. Cursor coordinates and rectangles use screen pixels.
static void _shelterB1UndergroundParkingPanelScanHotspots(Task* task)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_BUTTON_CONFIRM = 0,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_BUTTON_CANCEL  = 1,
    };
    ActionPrompt*                          prompt  = gActionPromptSlots;
    ActionPromptHotspot*                   hotspot = _gShelterB1UndergroundParkingPanelHotspots;
    _ShelterB1UndergroundParkingPanelWork* work    = task->work;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    gGameSession->hideHud = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (_actionPromptHitTestDefault(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
        if (prompt->buttons.slots[SHELTER_B1_UNDERGROUND_PARKING_PANEL_BUTTON_CONFIRM].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    _shelterB1UndergroundParkingPanelLatchHotspot(task, prompt, work, hotspot);
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }
    if (prompt->buttons.slots[SHELTER_B1_UNDERGROUND_PARKING_PANEL_BUTTON_CANCEL].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_CANCEL;
    }
}

/// Opens the command menu for the latched panel hotspot and enters state 4.
///
/// Requires state-3 work containing a hotspot command kind. The menu borrows
/// the port-0 cursor's screen-pixel position; the cursor stays hidden and stopped.
static void _shelterB1UndergroundParkingPanelOpenCommands(Task* task)
{
    ActionPrompt*                          prompt = gActionPromptSlots;
    _ShelterB1UndergroundParkingPanelWork* work   = task->work;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_HANDLE_COMMAND;
}

/// Applies a confirmed switch command or schedules a changed pattern's commitment.
///
/// Runs in state 4 with a latched choice: 8, 4, 2 or 1 toggles that pending
/// pattern bit; 16 is Enter. Enter advances to state 6 only when the pending
/// 0..15 pattern differs from the committed byte (which may be 0xFF). All other
/// results return to scanning. The last menu result is read once without being
/// cleared; this handler neither waits for menu closure nor commits the pattern.
static void _shelterB1UndergroundParkingPanelHandleCommand(Task* task)
{
    enum { SHELTER_B1_UNDERGROUND_PARKING_PANEL_INITIAL_ROOM = 1 };
    ActionPrompt*                          prompt = gActionPromptSlots;
    _ShelterB1UndergroundParkingPanelWork* work   = task->work;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        if (work->choice == SHELTER_B1_UNDERGROUND_PARKING_PANEL_HOTSPOT_ENTER) {
            // Acknowledge Enter even when the pending pattern is already applied.
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_PARKING_PANEL_BUTTON, 0, 0);
            if (_gShelterB1UndergroundParkingPanelCommittedPattern != _gShelterB1UndergroundParkingPanelPendingPattern) {
                if (gGameSession->location.roomState == SHELTER_B1_UNDERGROUND_PARKING_PANEL_INITIAL_ROOM) {
                    sndEvtRequestScriptStart(SOUND_SHELTER_B1_PARKING_PANEL_APPLY_R1, 0, 0);
                } else {
                    sndEvtRequestScriptStart(SOUND_SHELTER_B1_PARKING_PANEL_APPLY, 0, 0);
                }
                task->state = SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_COMMIT;
                return;
            }
        } else {
            _gShelterB1UndergroundParkingPanelPendingPattern ^= work->choice;
            sndEvtRequestScriptStart(SOUND_SHELTER_B1_PARKING_PANEL_BUTTON, 0, 0);
            task->state = SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_SCAN;
            return;
        }
    }
    task->state = SHELTER_B1_UNDERGROUND_PARKING_PANEL_STATE_SCAN;
}

/// Restores room interaction and presentation after leaving the selector panel.
///
/// Requires the panel's acquired menu hold and a live player model. Releases
/// that hold, resumes/shows the player, clears the session's event/HUD/player
/// holds and selects live-save view 2. Ten eligible direction-input updates
/// delay manual interaction with the room trigger; automatic triggers remain
/// eligible. Leaves the panel and action-cursor tasks for the caller to retire.
static inline void _shelterB1UndergroundParkingPanelRestoreRoom(void)
{
    enum {
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_INTERACTION_DELAY_UPDATES = 10,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_RETURN_VIEW               = 2,
        SHELTER_B1_UNDERGROUND_PARKING_PANEL_EVENT_IDLE                = 0,
    };

    gDirectionInteractionRearmUpdates = SHELTER_B1_UNDERGROUND_PARKING_PANEL_INTERACTION_DELAY_UPDATES;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->eventState                               = SHELTER_B1_UNDERGROUND_PARKING_PANEL_EVENT_IDLE;
    gGameSession->hideHud                                  = false;
    gGameSession->cutsceneHold                             = false;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = SHELTER_B1_UNDERGROUND_PARKING_PANEL_RETURN_VIEW;
}

/// Cancels the panel, restoring room control and releasing its action-cursor child.
///
/// State 5 requires a live child in `spawnArg2.pointer` and the acquired menu
/// hold. The committed pattern is retained. Parent teardown releases panel work.
static void _shelterB1UndergroundParkingPanelCancel(Task* task)
{
    _shelterB1UndergroundParkingPanelRestoreRoom();
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// Commits the selector panel's pending switch pattern and starts its closing fade.
///
/// Runs in panel state 6 with a live action-prompt child in `spawnArg2.pointer`.
/// Draws the newly committed indicators, selects the room resources, clears the
/// countdown and kills that child before advancing to state 7. The parent and
/// its work remain alive to draw the fade on following frames.
static void _shelterB1UndergroundParkingPanelCommitSelection(Task* task)
{
    _gShelterB1UndergroundParkingPanelCommittedPattern = _gShelterB1UndergroundParkingPanelPendingPattern;
    _shelterB1UndergroundParkingDrawPanelIndicators();
    _shelterB1UndergroundParkingPanelApplySelection();
    task->killCountdown = 0;
    taskKill(task->spawnArg2.pointer);
    task->state++;
}

/// Darkens the committed panel to black, then restores the room and ends the task.
///
/// State 7 owns live panel work; its action-cursor child has already been killed.
/// Darkness increases by six per invocation, saturating at 255 after 43 calls
/// from zero. The subtractive overlay uses equal RGB channels. Restoration
/// balances the menu hold and task teardown releases the work.
static void _shelterB1UndergroundParkingPanelFadeOut(Task* task)
{
    enum { SHELTER_B1_UNDERGROUND_PARKING_PANEL_FADE_BLACK = 0xFF };
    _ShelterB1UndergroundParkingPanelWork* work = task->work;

    _shelterB1UndergroundParkingDrawPanelIndicators();
    work->fadeLevel += SHELTER_B1_UNDERGROUND_PARKING_PANEL_FADE_STEP;
    if (work->fadeLevel >= SHELTER_B1_UNDERGROUND_PARKING_PANEL_FADE_BLACK + 1) {
        work->fadeLevel = SHELTER_B1_UNDERGROUND_PARKING_PANEL_FADE_BLACK;
    }
    fadeDrawOverlay(work->fadeLevel, work->fadeLevel, work->fadeLevel, GPU_BLEND_SUBTRACT);
    if (work->fadeLevel == SHELTER_B1_UNDERGROUND_PARKING_PANEL_FADE_BLACK) {
        _shelterB1UndergroundParkingPanelRestoreRoom();
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

    _gShelterB1UndergroundParkingPanelCommittedPattern = SHELTER_B1_UNDERGROUND_PARKING_PANEL_SELECTION_UNCOMMITTED;
    _gShelterB1UndergroundParkingPanelPendingPattern   = 0;
}

#include "../../shared/room/action_prompt_reset.inc.c"

/// Selects the room resources mapped to the committed panel switch pattern.
///
/// The low four bits select one of sixteen entries, whose room values are
/// 2..5. Updates both the session and live save and requests object relinking;
/// this does not change the view or load another area.
static void _shelterB1UndergroundParkingPanelApplySelection(void)
{
    enum { SHELTER_B1_UNDERGROUND_PARKING_PANEL_PATTERN_MASK = 0xF };

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = _gShelterB1UndergroundParkingPanelRoomsByPattern[_gShelterB1UndergroundParkingPanelCommittedPattern & SHELTER_B1_UNDERGROUND_PARKING_PANEL_PATTERN_MASK];
    gGameSession->location.roomState                            = _gShelterB1UndergroundParkingPanelRoomsByPattern[_gShelterB1UndergroundParkingPanelCommittedPattern & SHELTER_B1_UNDERGROUND_PARKING_PANEL_PATTERN_MASK];
    gGameSession->roomObjsDirty                                 = 1;
}

#include "../../shared/room/action_prompt_hit_test.inc.c"
#undef ACTION_PROMPT_HIT_TEST

/// Queues the cyan diamond, green disc and grey beam shared by six mapped views.
///
/// Uses the room's slow/fast pulse selection for the diamond; the disc and
/// beam flicker on frame parity. Requires the current view, scratch stack,
/// packet arena and ordering table under the same contract as
/// `shelterB1UndergroundParkingDrawGlowsTask`. The beam borrows the final two
/// endpoints in `_gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints.greyBeamPoints[5]`.
static inline void _shelterB1UndergroundParkingDrawCommonViewGlows(void)
{
    if (_gShelterB1UndergroundParkingFastLightPulse != 0) {
        _glowDrawDiamond(&_gShelterB1UndergroundParkingCyanLightPosition,
                         SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_FAST,
                         SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DIAMOND_RADIUS_SCALE);
    } else {
        _glowDrawDiamond(&_gShelterB1UndergroundParkingCyanLightPosition,
                         SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_SLOW,
                         SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DIAMOND_RADIUS_SCALE);
    }
    _glowDrawBitDisc(&_gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints.greenDiscPosition,
                     SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_RADIUS_SCALE,
                     SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_COLOR_FACTORS);
    _glowDrawBeam(_gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints.greyBeamPoints[5],
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
            _glowDrawBeam(_gShelterB1UndergroundParkingRedGreenBeamPoints,
                          SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, 0,
                          SHELTER_B1_UNDERGROUND_PARKING_LIGHT_SIDE_BEAM_COLOR_FACTORS);
            // These views also show all five beams drawn by views 4 and 14.
        case 4:
        case 14:
            _glowDrawBeam(_gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints.greyBeamPoints[0], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(_gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints.greyBeamPoints[1], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(_gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints.greyBeamPoints[2], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(_gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints.greyBeamPoints[3], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(_gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints.greyBeamPoints[4], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, 0, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            break;
        case 9:
        case 15:
        case 22:
        case 24:
            _glowDrawBeam(_gShelterB1UndergroundParkingGreyBeamPoints[0], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, 0, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            _glowDrawBeam(_gShelterB1UndergroundParkingGreyBeamPoints[1], SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_BEAM_COLOR_FACTORS);
            break;
        case 16:
            _glowDrawBitDisc(&_gShelterB1UndergroundParkingGreenDiscAndGreyBeamPoints.greenDiscPosition, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_RADIUS_SCALE, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DISC_COLOR_FACTORS);
            break;
        case 18:
            if (_gShelterB1UndergroundParkingFastLightPulse != 0) {
                _glowDrawDiamond(&_gShelterB1UndergroundParkingCyanLightPosition, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_FAST, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DIAMOND_RADIUS_SCALE);
            } else {
                _glowDrawDiamond(&_gShelterB1UndergroundParkingCyanLightPosition, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_SLOW, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_DIAMOND_RADIUS_SCALE);
            }
            break;
        case 20:
            if (_gShelterB1UndergroundParkingFastLightPulse != 0) {
                _glowDrawPulsingDisc(&_gShelterB1UndergroundParkingCyanLightPosition, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_FAST, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSING_DISC_RADIUS_SCALE);
            } else {
                _glowDrawPulsingDisc(&_gShelterB1UndergroundParkingCyanLightPosition, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSE_RATE_SLOW, SHELTER_B1_UNDERGROUND_PARKING_LIGHT_PULSING_DISC_RADIUS_SCALE);
            }
            break;
    }
}

#include "../../shared/effects/glow_draw_beam.inc.c"

#include "../../shared/effects/glow_draw_bit_disc.inc.c"

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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(SHELTER_B1_UNDERGROUND_PARKING_PANEL_GLOW_DEPTH),
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

#include "../../shared/effects/glow_draw_diamond.inc.c"

#include "../../shared/effects/glow_draw_pulsing_disc.inc.c"

/// Selects the cyan room light's pulse speed (0 slow, nonzero fast).
///
/// Retains all 16 argument bits in the room's flag; callers pass 0 or 1.
static void _shelterB1UndergroundParkingSetFastLightPulse(s16 fastPulse)
{
    _gShelterB1UndergroundParkingFastLightPulse = fastPulse;
}
