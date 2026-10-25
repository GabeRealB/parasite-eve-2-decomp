#include "rooms/dryfield_night_main_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_00100.h"
#include "actors/actor_00300.h"
#include "actors/actor_136100.h"
#include "actors/actor_300700.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/sprite_visibility_types.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

// The latched-event symbol carries four unproven bytes after the event.
#define ROOM_EVENT_LATCHED gRoomEventLatched.event
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
/// Binds the shared puff task to this package's public void (Task*) effect callback.
/// Must precede main_street.h and remain defined through main_street_puff_task.inc.c.
#define MAIN_STREET_PUFF_TASK dryfieldNightMainStreetPuffTask
#include "../../shared/main_street.h"

static s32 _mainStreetResolveMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _mainStreetTalkMessage(Task* task, s32 messageId, s32 command, s32 unusedSecondArg);

static void _mainStreetFinishIceBagEvent(Task* task);

static s32 _mainStreetCapSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

/// Control values in this room's two-byte sprite visibility commands.
enum {
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE     = 0,
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN      = 1,
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW = 0xFF,
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END         = 0xFF,
};

/// Fixed glow anchors and the reusable puff-emission point for the night main street.
///
/// Glow positions use signed whole world-coordinate units. Puff emission uses
/// the input space of `GsWSMATRIX`, snapshots XYZ during spawning and retains
/// a borrowed pointer to the slot. GTE loads read each vector's fourth halfword.
typedef struct {
    SVECTOR shaftEndpoints[5][2]; // Five fixed shaft endpoint pairs, selected by mapped-view masks
    SVECTOR flareCenters[6];      // Fixed flare centres, selected by masks 10..15
    SVECTOR puffSpawnPoint;       // Rewritten XYZ for each puff; the fourth halfword stays zero
} _DryfieldNightMainStreetAmbientEffectAnchors;
STATIC_ASSERT_SIZEOF(_DryfieldNightMainStreetAmbientEffectAnchors, 0x88);

enum {
    DRYFIELD_NIGHT_MAIN_STREET_ROOM_STATE_INITIALIZE = 0,
    DRYFIELD_NIGHT_MAIN_STREET_ROOM_STATE_IDLE       = 1,
    DRYFIELD_NIGHT_MAIN_STREET_ROOM_STATE_RELEASE    = 2,
};

/// Advances the shared random sequence once and yields an unsigned draw in 0..65535.
///
/// Captures `gRandomLcgState`; multiplication and addition wrap modulo 2^32.
/// Each invocation consumes one draw, so callers must preserve draw order.
#define DRYFIELD_NIGHT_MAIN_STREET_RAND() ((RANDOM_LCG_ADVANCE()) >> 16)

/// The room effect mode of each view, indexed by view - 1.

/// View masks of the room task's anchors.

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The message and event the message handler latched for the room's event
/// task, and the flag it raises once it has spawned that task.
static RoomEventMsg            _gRoomEventStagedMsg;
static RoomEventStartStorage   _gMainStreetStagedSceneStartStorage;
extern RoomLatchedEventStorage gRoomEventLatched;

/// The request the event gate latched for its event task.
extern RoomEventReq gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 gRoomEventActive;

static void _dryfieldNightMainStreetInitializeRoomTask(Task* task);
static void _dryfieldNightMainStreetRoomIdle(Task* task);
static void _dryfieldNightMainStreetRestoreSectionSpriteVisibility(void);

static s32 _dryfieldNightMainStreetRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedSecondArg);
static s32 _dryfieldNightMainStreetIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static WorldCollisionGrid    _gDryfieldNightMainStreetRoom1CollisionGrid;
static WorldCollisionGrid    _gDryfieldNightMainStreetRooms2And3CollisionGrid;
static WorldCollisionTrigger _gDryfieldNightMainStreetViewBoundaryTriggers[26];
static WorldCollisionTrigger _gDryfieldNightMainStreetRoom1ActionTriggers[12];

static WorldCoordRoomAmbientEntry _gDryfieldNightMainStreetRoomAmbientTable[25];
static WorldCoordRoomLights       _gDryfieldNightMainStreetRoomLights;

/// Bodyless staged motel-scene recipe, using the room's latched event and destination.
///
/// Priority `TASK_PRIORITY_SCENE`; spawn payloads and metadata are unused. Spawn reads the recipe
/// synchronously, and the callback and singleton snapshots stay live through reload.
static TaskDesc _gMainStreetStagedSceneTaskDescriptor = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// Bodyless Ice Bag handover-completion recipe at priority 32.
///
/// Waits for CAP to become idle, then updates the saved timer and water-bag
/// possession. Spawn payloads and metadata are unused; keep callback code loaded.
static TaskDesc _gMainStreetIceBagCompletionTaskDescriptor = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _mainStreetFinishIceBagEvent, { .value = 0 } };

/// Room-task message interface, borrowed from this overlay until receiver teardown.
///
/// Transition resolution, refused key-item use, direction actions, CAP commands and sound cues.
/// The final `TASK_MESSAGE_TABLE_END` record terminates dispatch; it is not a handler.
static const TaskMessageEntry _gDryfieldNightMainStreetMessageTable[6] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mainStreetResolveMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMainStreetRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightMainStreetIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _mainStreetTalkMessage },
    { ROOM_MESSAGE_SOUND, _mainStreetCapSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Shows balcony section 3 in mapped views 14, 15 and 17 (batches 6, 3 and 5).
static SpriteVisibilityCommand _gDryfieldNightMainStreetBalconySection3VisibleSpritePatches[] = {
    { 13, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 6, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE },
    { 14, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 3, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE },
    { 16, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 5, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END },
    { 0, 0 }, // Stored after the terminator; not decoded
};

/// Hides balcony section 3 in mapped views 14, 15 and 17 (batches 6, 3 and 5).
static SpriteVisibilityCommand _gDryfieldNightMainStreetBalconySection3HiddenSpritePatches[] = {
    { 13, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 6, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN },
    { 14, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 3, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN },
    { 16, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 5, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END },
    { 0, 0 }, // Stored after the terminator; not decoded
};

/// Balcony section 3 patch lists indexed by its saved state (0 visible, 1 hidden).
static SpriteVisibilityCommand* _gDryfieldNightMainStreetBalconySection3SpritePatchLists[2] = {
    [DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE] = _gDryfieldNightMainStreetBalconySection3VisibleSpritePatches,
    [DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN]  = _gDryfieldNightMainStreetBalconySection3HiddenSpritePatches,
};

/// Shows balcony section 4 in mapped views 14, 15 and 17 (batches 7, 4 and 6).
static SpriteVisibilityCommand _gDryfieldNightMainStreetBalconySection4VisibleSpritePatches[] = {
    { 13, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 7, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE },
    { 14, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 4, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE },
    { 16, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 6, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END },
    { 0, 0 }, // Stored after the terminator; not decoded
};

/// Hides balcony section 4 in mapped views 14, 15 and 17 (batches 7, 4 and 6).
static SpriteVisibilityCommand _gDryfieldNightMainStreetBalconySection4HiddenSpritePatches[] = {
    { 13, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 7, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN },
    { 14, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 4, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN },
    { 16, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 6, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END },
    { 0, 0 }, // Stored after the terminator; not decoded
};

/// Balcony section 4 patch lists indexed by its saved state (0 visible, 1 hidden).
static SpriteVisibilityCommand* _gDryfieldNightMainStreetBalconySection4SpritePatchLists[2] = {
    [DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE] = _gDryfieldNightMainStreetBalconySection4VisibleSpritePatches,
    [DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN]  = _gDryfieldNightMainStreetBalconySection4HiddenSpritePatches,
};

/// Shows balcony section 5 in mapped views 14 and 15 (batches 8 and 5).
static SpriteVisibilityCommand _gDryfieldNightMainStreetBalconySection5VisibleSpritePatches[] = {
    { 13, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 8, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE },
    { 14, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 5, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END },
    { 0, 0 }, // Stored after the terminator; not decoded
};

/// Hides balcony section 5 in mapped views 14 and 15 (batches 8 and 5).
static SpriteVisibilityCommand _gDryfieldNightMainStreetBalconySection5HiddenSpritePatches[] = {
    { 13, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 8, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN },
    { 14, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 5, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END },
    { 0, 0 }, // Stored after the terminator; not decoded
};

/// Balcony section 5 patch lists indexed by its saved state (0 visible, 1 hidden).
static SpriteVisibilityCommand* _gDryfieldNightMainStreetBalconySection5SpritePatchLists[2] = {
    [DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE] = _gDryfieldNightMainStreetBalconySection5VisibleSpritePatches,
    [DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN]  = _gDryfieldNightMainStreetBalconySection5HiddenSpritePatches,
};

/// Shows balcony section 7 in mapped view 13 (batch 4).
static SpriteVisibilityCommand _gDryfieldNightMainStreetBalconySection7VisibleSpritePatches[] = {
    { 12, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 4, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END },
    { 0, 0 }, // Stored after the terminator; not decoded
};

/// Hides balcony section 7 in mapped view 13 (batch 4).
static SpriteVisibilityCommand _gDryfieldNightMainStreetBalconySection7HiddenSpritePatches[] = {
    { 12, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW },
    { 4, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END },
    { 0, 0 }, // Stored after the terminator; not decoded
};

/// Balcony section 7 patch lists indexed by its saved state (0 visible, 1 hidden).
static SpriteVisibilityCommand* _gDryfieldNightMainStreetBalconySection7SpritePatchLists[2] = {
    [DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_VISIBLE] = _gDryfieldNightMainStreetBalconySection7VisibleSpritePatches,
    [DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_HIDDEN]  = _gDryfieldNightMainStreetBalconySection7HiddenSpritePatches,
};

/// Stored root of balcony section 3's borrowed two-state patch directory.
static SpriteVisibilityCommand** _gDryfieldNightMainStreetBalconySection3SpritePatchTable = _gDryfieldNightMainStreetBalconySection3SpritePatchLists;

/// Stored root of balcony section 4's borrowed two-state patch directory.
static SpriteVisibilityCommand** _gDryfieldNightMainStreetBalconySection4SpritePatchTable = _gDryfieldNightMainStreetBalconySection4SpritePatchLists;

/// Stored root of balcony section 5's borrowed two-state patch directory.
static SpriteVisibilityCommand** _gDryfieldNightMainStreetBalconySection5SpritePatchTable = _gDryfieldNightMainStreetBalconySection5SpritePatchLists;

/// Stored root of balcony section 7's borrowed two-state patch directory.
static SpriteVisibilityCommand** _gDryfieldNightMainStreetBalconySection7SpritePatchTable = _gDryfieldNightMainStreetBalconySection7SpritePatchLists;

/// Ambient-effect enable state for each of the 24 mapped street views.
///
/// Indexed by mapped view minus one; the selector must be in 1..24. Entries
/// publish `ROOM_EFFECT_VIEW_ENABLED` (2) or `ROOM_EFFECT_VIEW_DISABLED` (0).
static const u16 _gDryfieldNightMainStreetAmbientEffectGateByView[24] SECTION(".data") = {
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
};

/// Authored shaft/flare positions and the next puff's reusable spawn position.
///
/// Positions use signed game-coordinate units in world space. Puff spawning
/// overwrites only its XYZ and snapshots them into each new effect; keep this
/// overlay loaded while effects retain the borrowed spawn-point address.
static _DryfieldNightMainStreetAmbientEffectAnchors _gDryfieldNightMainStreetAmbientEffectAnchors = {
    .shaftEndpoints = {
        { { -930, -2870, 10010, 0 }, { -2130, -2870, 10010, 0 } },
        { { -6010, -2870, 3050, 0 }, { -6010, -2870, 1850, 0 } },
        { { -6010, -2870, -2940, 0 }, { -6010, -2870, -4150, 0 } },
        { { -160, -3100, 8360, 0 }, { -160, -2800, 8790, 0 } },
        { { -300, -3250, -4100, 0 }, { -300, -3190, -4610, 0 } },
    },
    .flareCenters = {
        { -4140, -1700, 10930, 0 },
        { -6910, -1700, 10650, 0 },
        { -6910, -1700, 6660, 0 },
        { -6910, -1700, -3150, 0 },
        { -3150, -4850, 10920, 0 },
        { -6930, -4850, 5150, 0 },
    },
    .puffSpawnPoint = { -850, -1320, 9770, 0 },
};

/// Writable mapped-view bitsets selecting the street's shaft and flare anchors.
///
/// Bit n selects mapped view n (0..31). Entries 0..9 parallel five endpoint
/// pairs, with each even entry selecting its shaft; 10..15 select six flares.
/// The broken-lamp flag permanently clears both entries 2/3 for this load.
static u32 _gDryfieldNightMainStreetGlowViewMasks[16] = {
    (1u << 3) | (1u << 4) | (1u << 6) | (1u << 11) | (1u << 14) | (1u << 15) | (1u << 17),
    0,
    (1u << 3) | (1u << 6) | (1u << 10),
    (1u << 6),
    (1u << 2) | (1u << 7) | (1u << 10) | (1u << 13) | (1u << 18) | (1u << 24),
    (1u << 7) | (1u << 18),
    (1u << 3) | (1u << 4) | (1u << 11) | (1u << 14) | (1u << 15),
    (1u << 3) | (1u << 4) | (1u << 11) | (1u << 14) | (1u << 15),
    (1u << 2) | (1u << 7) | (1u << 13) | (1u << 18) | (1u << 21) | (1u << 22) | (1u << 23) | (1u << 24),
    (1u << 2) | (1u << 13) | (1u << 21) | (1u << 22) | (1u << 23) | (1u << 24),
    (1u << 3) | (1u << 4) | (1u << 5) | (1u << 11) | (1u << 14) | (1u << 15) | (1u << 16),
    (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 14) | (1u << 15) | (1u << 16) | (1u << 17),
    (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 14) | (1u << 15) | (1u << 16) | (1u << 17),
    (1u << 2) | (1u << 7) | (1u << 12) | (1u << 13) | (1u << 18) | (1u << 24),
    (1u << 3) | (1u << 4) | (1u << 14) | (1u << 15) | (1u << 17),
    (1u << 3) | (1u << 14),
};

/// Initializes this overlay's three halo tint rows and retained trailing halfword.
///
/// Rows 0..2 give RGB right shifts of brightness (0 full, 1 half, 2 quarter).
/// Requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` = `RoomFxHaloStorage` and an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND`. The trailing u16 is preserved
/// verbatim; its role is unproven and it is not a fourth tint row.
/// Bind around `room_visual_effects_halo_data.inc.c`, then undefine.
/// Expands to a constant braced initializer, with no arguments or side effects.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER { \
    .entries = {                                       \
        { .rShift = 0, .gShift = 1, .bShift = 2 },     \
        { .rShift = 2, .gShift = 1, .bShift = 0 },     \
        { .rShift = 0, .gShift = 2, .bShift = 1 },     \
    },                                                 \
    .alignmentFill = 0xE0DC,                           \
}
/// Selects the halo tint storage record, including its retained trailing halfword.
///
/// Expands to the `RoomFxHaloStorage` type token; requires an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` and a
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER` for `entries` and `alignmentFill`.
/// Only the three RGB-shift rows are read, at indices 0..2; the halfword's
/// role is unproven. Include `room_visual_effects.h` first, bind around
/// `room_visual_effects_halo_data.inc.c`, then undefine.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE RoomFxHaloStorage
/// Declares one halo storage record, retaining the halfword after its tint rows.
///
/// This empty suffix requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` to be
/// `RoomFxHaloStorage`. Bind around `room_visual_effects_halo_data.inc.c`.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND
#include "../../shared/room/room_visual_effects_halo_data.inc.c"

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);
static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades.entries;
}
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND

WorldCollisionRoomResources gDryfieldNightMainStreetRoomCollisionResources[3] = {
    { .grid = &_gDryfieldNightMainStreetRoom1CollisionGrid, .viewBoundaryTriggers = _gDryfieldNightMainStreetViewBoundaryTriggers, .actionTriggers = _gDryfieldNightMainStreetRoom1ActionTriggers, .occluders = NULL },
    { .grid = &_gDryfieldNightMainStreetRooms2And3CollisionGrid, .viewBoundaryTriggers = _gDryfieldNightMainStreetViewBoundaryTriggers, .actionTriggers = gDryfieldNightMainStreetRooms2And3ActionTriggers, .occluders = NULL },
    { .grid = &_gDryfieldNightMainStreetRooms2And3CollisionGrid, .viewBoundaryTriggers = _gDryfieldNightMainStreetViewBoundaryTriggers, .actionTriggers = gDryfieldNightMainStreetRooms2And3ActionTriggers, .occluders = NULL },
};

WorldCoordRoomLighting gDryfieldNightMainStreetRoomLighting[3] = {
    { .lights = &_gDryfieldNightMainStreetRoomLights, .ambientTable = _gDryfieldNightMainStreetRoomAmbientTable },
    { .lights = &_gDryfieldNightMainStreetRoomLights, .ambientTable = _gDryfieldNightMainStreetRoomAmbientTable },
    { .lights = &_gDryfieldNightMainStreetRoomLights, .ambientTable = _gDryfieldNightMainStreetRoomAmbientTable },
};

/// Logical-view mapping for main-street room selector 2.
///
/// Entries for logical views 1..24 contain 1-based camera/image/sprite indices
/// in 1..24. Duplicate indices are intentional; there is no terminator.
static u8 _gDryfieldNightMainStreetRoom2ViewMap[24] = {
    1,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
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
    22,
    21,
    23,
    24,
};

/// Logical-view mapping for main-street room selector 3.
///
/// Entries for logical views 1..24 contain 1-based camera/image/sprite indices
/// in 1..24. Duplicate indices are intentional; there is no terminator.
static u8 _gDryfieldNightMainStreetRoom3ViewMap[24] = {
    1,
    24,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
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
    23,
    21,
    23,
    24,
};

u8* gDryfieldNightMainStreetViewMaps[3] = {
    gViewIdentityMap,
    _gDryfieldNightMainStreetRoom2ViewMap,
    _gDryfieldNightMainStreetRoom3ViewMap,
};

ViewCount gDryfieldNightMainStreetViewCounts[3] = { 24, 24, 24 };

DirectionWarpEntry gDryfieldNightMainStreetWarpEntries[7] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, -450, 0, -3435 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, -450, 0, -3435 }, { 0, 0, 0, 0 }, 0x53020011, 0x53020010, DIRECTION_WARP_SOUND_NONE, 21, DIRECTION_WARP_FLAG_NONE, 488 },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6588, 0, -2500 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6588, 0, -2500 }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 7, DIRECTION_WARP_FLAG_NONE, 487 },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6673, 0, 2155 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6673, 0, 2155 }, { 0, 0, 0, 0 }, 0x53020004, 0x53020003, 0x53020007, 6, DIRECTION_WARP_FLAG_NONE, 486 },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6588, 0, 5900 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6588, 0, 5900 }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 5, DIRECTION_WARP_FLAG_NONE, 485 },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6652, 0, 0x2956 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -6652, 0, 0x2956 }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 5, DIRECTION_WARP_FLAG_NONE, 484 },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -3478, 0, 0x29B5 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -3478, 0, 0x29B5 }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 4, DIRECTION_WARP_FLAG_NONE, 483 },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, -488, 0, 8495 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, -488, 0, 8495 }, { 0, 0, 0, 0 }, 0x53020002, 0x53020001, DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, 482 },
};

/// Room-space unit normals for the room-1 collision mesh (4096 per unit).
static SVECTOR _gDryfieldNightMainStreetRoom1CollisionGridNormals[] = {
#include "assets/dryfield_night_main_street_collision_05E10_normals.inc"
};

/// Signed room-coordinate vertices for the room-1 collision mesh.
static SVECTOR _gDryfieldNightMainStreetRoom1CollisionGridVerts[] = {
#include "assets/dryfield_night_main_street_collision_05E10_verts.inc"
};

/// Indexed collision faces and room-local surface classes for room 1.
static WorldCollisionGridFace _gDryfieldNightMainStreetRoom1CollisionGridFaces[] = {
#include "assets/dryfield_night_main_street_collision_05E10_faces.inc"
};

/// Signed face-index storage for room 1; lists end at `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldNightMainStreetRoom1CollisionGridCells[] = {
#include "assets/dryfield_night_main_street_collision_05E10_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMainStreetRoom1CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..553 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MAIN_STREET_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMainStreetRoom1CollisionGridCells[(halfwordOffset)])
/// Room-1 cell-list pointers in X-major order, with seven Z cells per X row.
static s16* _gDryfieldNightMainStreetRoom1CollisionGridTable[] = {
#include "assets/dryfield_night_main_street_collision_05E10_table.inc"
};
#undef DRYFIELD_NIGHT_MAIN_STREET_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and 6-by-7 XZ cell index for main-street room selector 1.
///
/// Vertices and the 4000-unit cell size use room coordinates; normals use
/// 4096 per unit. Activation binds the view coordinate. This record and its
/// pools remain borrowed while the room overlay is loaded.
static WorldCollisionGrid _gDryfieldNightMainStreetRoom1CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMainStreetRoom1CollisionGridNormals,
    .vertices    = _gDryfieldNightMainStreetRoom1CollisionGridVerts,
    .faces       = _gDryfieldNightMainStreetRoom1CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMainStreetRoom1CollisionGridTable,
    .xBias       = 12006,
    .zBias       = 11340,
    .cellCountX  = 6,
    .cellCountZ  = 7,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMainStreetRoom1CollisionGridFaces),
};

SVECTOR gDryfieldNightMainStreetRooms2And3CollisionGridNormals[23] = {
#include "assets/dryfield_night_main_street_collision_06F80_normals.inc"
};

SVECTOR gDryfieldNightMainStreetRooms2And3CollisionGridVerts[196] = {
#include "assets/dryfield_night_main_street_collision_06F80_verts.inc"
};

WorldCollisionGridFace gDryfieldNightMainStreetRooms2And3CollisionGridFaces[89] = {
#include "assets/dryfield_night_main_street_collision_06F80_faces.inc"
};

/// Signed face-index storage; each cell list ends at `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldNightMainStreetRooms2And3CollisionGridCells[720] = {
#include "assets/dryfield_night_main_street_collision_06F80_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMainStreetRooms2And3CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..719 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MAIN_STREET_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMainStreetRooms2And3CollisionGridCells[(halfwordOffset)])
/// Room-2/3 cell-list pointers in X-major order, with seven Z cells per X row.
static s16* _gDryfieldNightMainStreetRooms2And3CollisionGridTable[42] = {
#include "assets/dryfield_night_main_street_collision_06F80_table.inc"
};
#undef DRYFIELD_NIGHT_MAIN_STREET_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and 6-by-7 XZ cell index shared by room selectors 2 and 3.
///
/// Vertices and the 4000-unit cell size use room coordinates; normals use
/// 4096 per unit. Activation binds the view coordinate. The opening scene may
/// replace the first four normals/faces and sixteen vertices in the live pools.
/// This record and its pools stay borrowed while the room overlay is loaded.
static WorldCollisionGrid _gDryfieldNightMainStreetRooms2And3CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = gDryfieldNightMainStreetRooms2And3CollisionGridNormals,
    .vertices    = gDryfieldNightMainStreetRooms2And3CollisionGridVerts,
    .faces       = gDryfieldNightMainStreetRooms2And3CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMainStreetRooms2And3CollisionGridTable,
    .xBias       = 12006,
    .zBias       = 11340,
    .cellCountX  = 6,
    .cellCountZ  = 7,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(gDryfieldNightMainStreetRooms2And3CollisionGridFaces),
};

ViewCamera gDryfieldNightMainStreetViewCameras[24] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 4112, 0x6D60, -2748 } }, 289 },
    { { { { -3906, 0, 1231 }, { 39, 4093, 125 }, { -1231, 131, -3904 } }, { 1214, 1299, -3483 } }, 230 },
    { { { { 3809, 0, 1504 }, { 102, 4086, -259 }, { -1500, 279, 3800 } }, { 1205, 1259, 2882 } }, 230 },
    { { { { 3962, 0, 1037 }, { 65, 4087, -250 }, { -1035, 258, 3954 } }, { 1414, 1228, -1924 } }, 230 },
    { { { { 3997, 0, -893 }, { -382, 3702, -1708 }, { 807, 1750, 3613 } }, { 6425, 2237, -3705 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6489, 989, 2493 } }, 230 },
    { { { { -3989, 0, -928 }, { 63, 4086, -271 }, { 926, -278, -3980 } }, { 6440, 837, -1624 } }, 230 },
    { { { { 1028, 0, -3964 }, { -2496, 3181, -647 }, { 3079, 2579, 798 } }, { 2261, 1919, -9779 } }, 230 },
    { { { { 3973, 0, -993 }, { -593, 3285, -2373 }, { 796, 2446, 3187 } }, { 1213, 2874, -5701 } }, 230 },
    { { { { -3415, 0, 2261 }, { -341, 4049, -515 }, { -2235, -618, -3376 } }, { 173, 317, -8972 } }, 230 },
    { { { { 4058, 0, -551 }, { 57, 4073, 425 }, { 548, -429, 4036 } }, { 1906, 617, -5254 } }, 230 },
    { { { { -3455, 0, 2198 }, { -65, 4094, -103 }, { -2197, -122, -3454 } }, { 4039, 880, 1740 } }, 230 },
    { { { { -3906, 0, 1231 }, { 39, 4093, 125 }, { -1231, 131, -3904 } }, { 1214, 1299, -3483 } }, 230 },
    { { { { 3809, 0, 1504 }, { 102, 4086, -259 }, { -1500, 279, 3800 } }, { 1205, 1259, 2882 } }, 230 },
    { { { { 3962, 0, 1037 }, { 65, 4087, -250 }, { -1035, 258, 3954 } }, { 1414, 1228, -1924 } }, 230 },
    { { { { 3997, 0, -893 }, { -382, 3702, -1708 }, { 807, 1750, 3613 } }, { 6425, 2237, -3705 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6489, 989, 2493 } }, 230 },
    { { { { -3989, 0, -928 }, { 63, 4086, -271 }, { 926, -278, -3980 } }, { 6440, 837, -1624 } }, 230 },
    { { { { 1028, 0, -3964 }, { -2496, 3181, -647 }, { 3079, 2579, 798 } }, { 2261, 1919, -9779 } }, 230 },
    { { { { 3823, 0, -1469 }, { -867, 3305, -2257 }, { 1186, 2418, 3085 } }, { 1630, 2840, -5810 } }, 230 },
    { { { { -4089, 0, -223 }, { 23, 4073, -429 }, { 222, -430, -4067 } }, { 1450, 640, 520 } }, 230 },
    { { { { -4089, 0, -223 }, { 23, 4073, -429 }, { 222, -430, -4067 } }, { 1450, 640, 520 } }, 230 },
    { { { { -4089, 0, -223 }, { 23, 4073, -429 }, { 222, -430, -4067 } }, { 1450, 640, 520 } }, 230 },
    { { { { -3906, 0, 1231 }, { 39, 4093, 125 }, { -1231, 131, -3904 } }, { 1214, 1299, -3483 } }, 230 },
};

// Encoded 8-bit texture pages at VRAM Y 0, with neutral modulation.
enum {
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TEXTURE_8BIT     = 1,
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_832        = getTPage(DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0),
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896        = getTPage(DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960        = getTPage(DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255         = getClut(0, 255),
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256         = getClut(0, 256),
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR    = 128,
    DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED = 0,
};

/// Empty sprite-batch list retaining the decoded background in mapped view 1.
///
/// The source slot retains this list address and is never read as sprites.
static SpriteBatch _gDryfieldNightMainStreetView1SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 2.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
/// Source elements have no terminator and remain live with this room overlay.
static SpriteSource _gDryfieldNightMainStreetView2SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 104 } }, -160, -120, 1250, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 80 } }, -160, -16, 1250, { .fields = { 96, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 152 } }, -72, -120, 2350, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 96 } }, -112, -120, 2250, { .fields = { 24, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 80 } }, -160, -24, 1250, { .fields = { 120, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 80 } }, -128, -24, 1750, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -112, -24, 1875, { .fields = { 120, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -104, -24, 2350, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -96, 16, 2375, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 136, -24, 1131, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -144, -16, 2375, { .fields = { 32, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 16, -16, 2500, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 136, -16, 1133, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 136, -8, 1136, { .fields = { 64, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -104, 0, 2375, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 136, 0, 1138, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -152, 8, 2375, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 136, 8, 1091, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 136, 16, 1112, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -160, -32, 2375, { .fields = { 96, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, -136, -24, 2250, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, -128, 0, 2375, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 16 } }, -128, 16, 2375, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, -112, -24, 2375, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -96, -32, 2375, { .fields = { 88, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 32 } }, -88, 0, 2375, { .fields = { 40, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 8 } }, -80, -16, 2375, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -48, -32, 2375, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -40, 8, 2375, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 32 } }, -16, -24, 2500, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 0, -24, 2500, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 16 } }, 8, 8, 2500, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 48 } }, 32, -24, 2500, { .fields = { 72, 112 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 32, -80, 2250, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 104 } }, 16, -72, 2275, { .fields = { 48, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 40 } }, 40, -88, 2250, { .fields = { 64, 40 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 152 } }, 56, -120, 2344, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 40 } }, 40, -8, 2425, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 64, 152 } }, 96, -120, 2300, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 96, -96, 1294, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 104, -112, 1237, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 168 } }, 112, -112, 1190, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 96, 0, 1339, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 104, 0, 1311, { .fields = { 80, 56 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 176 } }, 120, -112, 1166, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 176 } }, 128, -112, 1128, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 184 } }, 136, -120, 1130, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 96 } }, 144, -120, 1094, { .fields = { 40, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 152, -120, 1006, { .fields = { 80, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 64 } }, 144, 8, 1039, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 56, 24 } }, 88, 32, 1250, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 2.
///
/// The initial zero count keeps image strips; the final record ends the list.
/// The final two serialized bytes of each range retain their unproven values.
static SpriteBatch _gDryfieldNightMainStreetView2SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 2, .field_6 = { 3, 0 } },
    { .firstSprite = 2, .spriteCount = 7 },
    { .firstSprite = 9, .spriteCount = 24, .field_6 = { 5, 0 } },
    { .firstSprite = 33, .spriteCount = 6, .field_6 = { 1, 0 } },
    { .firstSprite = 39, .spriteCount = 11, .field_6 = { 4, 0 } },
    { .firstSprite = 50, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView2SpriteSources) - 50, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 3.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
/// Source elements have no terminator and remain live with this room overlay.
static SpriteSource _gDryfieldNightMainStreetView3SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 40 } }, -160, -120, 920, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 40 } }, -144, -120, 1006, { .fields = { 80, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -128, -120, 1060, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -160, -80, 931, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -152, -80, 936, { .fields = { 104, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -144, -80, 991, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -160, -8, 938, { .fields = { 120, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -152, -8, 967, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -144, -8, 1000, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -136, -8, 1088, { .fields = { 112, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -128, -8, 1117, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -120, -8, 1132, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -120, 16, 1142, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -32, -80, 2087, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -24, -72, 2128, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -16, -72, 2165, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 32 } }, -40, -8, 2002, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -24, -8, 2138, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -16, -8, 2178, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, 8, -56, 3120, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, 16, -56, 3144, { .fields = { 96, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 24, -56, 3099, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 24, -16, 3102, { .fields = { 72, 24 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 48 } }, 104, -64, 2647, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 32 } }, 96, -16, 2685, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 56 } }, 144, 48, 696, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 3.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView3SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView3SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 4.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
/// Source elements have no terminator and remain live with this room overlay.
static SpriteSource _gDryfieldNightMainStreetView4SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -160, -120, 960, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -152, -120, 998, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -144, -120, 1037, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -160, -8, 968, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -152, -8, 1071, { .fields = { 104, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -144, -8, 1089, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -136, 32, 1107, { .fields = { 96, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 80 } }, -64, -88, 1943, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -56, -80, 1928, { .fields = { 112, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -64, -8, 1842, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -56, -8, 1952, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -48, -80, 1950, { .fields = { 120, 112 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 32 } }, -40, -8, 1970, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -40, -80, 1907, { .fields = { 96, 32 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 32 } }, 80, -88, 1620, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 48 } }, 96, -56, 1623, { .fields = { 88, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 48, 40 } }, 72, -8, 1706, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 4.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView4SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView4SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 5.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
/// Source elements have no terminator and remain live with this room overlay.
static SpriteSource _gDryfieldNightMainStreetView5SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 48 } }, -8, -120, 1375, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, 0, -8, 1475, { .fields = { 104, 40 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 16, -56, 1489, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 40 } }, 24, -56, 1482, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 48 } }, 120, -120, 329, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 32 } }, 112, -72, 366, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 32 } }, 136, -72, 322, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 24 } }, 72, 24, 612, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 24 } }, 96, 24, 600, { .fields = { 48, 32 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 24 } }, 128, 24, 500, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 16 } }, 72, 48, 625, { .fields = { 48, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 16 } }, 96, 48, 600, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 16 } }, 128, 48, 500, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 16 } }, 64, 64, 650, { .fields = { 40, 16 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 16 } }, 88, 64, 600, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 16 } }, 128, 64, 500, { .fields = { 40, 200 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 56, 80, 690, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 64, 80, 675, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 40 } }, 72, 80, 675, { .fields = { 120, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 40 } }, 80, 80, 650, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 48, 40 } }, 112, 80, 650, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 32 } }, -8, -72, 1412, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 32 } }, -8, -40, 1475, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 24 } }, 96, 0, 600, { .fields = { 48, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 24 } }, 128, 0, 550, { .fields = { 48, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 40 } }, 104, -40, 550, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 40 } }, 136, -40, 500, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 5.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView5SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView5SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 6.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
/// Source elements have no terminator and remain live with this room overlay.
static SpriteSource _gDryfieldNightMainStreetView6SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 104 } }, -24, -64, 1801, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -24, -72, 1617, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -16, -80, 1450, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 8 } }, -16, -88, 1314, { .fields = { 8, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -8, -96, 1201, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 8 } }, -8, -104, 1106, { .fields = { 8, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 8 } }, 0, -112, 1026, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 48, 8 } }, 0, -120, 956, { .fields = { 0, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 72 } }, -40, -48, 2867, { .fields = { 32, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 24 } }, -24, 0, 2867, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 40, -120, 750, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 224 } }, 48, -120, 775, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, 88, 112, 550, { .fields = { 56, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 120 } }, 104, 0, 512, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 128, 0, 512, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 128, 88, 512, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 32, 0, 797, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 88 } }, 40, 0, 800, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 32, 24, 875, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 32, 48, 873, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 224 } }, 64, -120, 700, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 232 } }, 80, -120, 600, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 232 } }, 88, -120, 525, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 152, 96, 568, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 16 } }, -8, 16, 1791, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 48 } }, 120, 56, 623, { .fields = { 16, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 6.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView6SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 8, .field_6 = { 3, 0 } },
    { .firstSprite = 8, .spriteCount = 15 },
    { .firstSprite = 23, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView6SpriteSources) - 23, .field_6 = { 2, 0 } },
    { .firstSprite = ARRAY_SIZE(_gDryfieldNightMainStreetView6SpriteSources), .spriteCount = 0, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 7.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
static SpriteSource _gDryfieldNightMainStreetView7SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 16, 0, 2025, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 64 } }, -80, -24, 2025, { .fields = { 104, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 24 } }, -40, 8, 2041, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 72 } }, -24, -24, 2025, { .fields = { 16, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 64, 72 } }, 24, -24, 2025, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -136, 32, 2000, { .fields = { 64, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -136, 88, 2000, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 32 } }, -160, -80, 1875, { .fields = { 24, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 48, 16 } }, -152, -120, 2004, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 200 } }, -128, -104, 2000, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 136 } }, -160, -16, 358, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, -136, -8, 402, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 240 } }, -128, -120, 468, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 240 } }, -88, -120, 565, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 104 } }, -72, -120, 574, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -72, -16, 581, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -72, 0, 582, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -64, 0, 642, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 96 } }, -56, 0, 669, { .fields = { 64, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_832, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 128, 16 } }, -48, -120, 1053, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_832, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 120, 16 } }, -40, -104, 1206, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_832, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 104, 16 } }, -24, -88, 1221, { .fields = { 72, 16 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_832, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 96, 8 } }, -16, -72, 1400, { .fields = { 80, 56 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_832, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 88, 8 } }, -8, -64, 1678, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 8 } }, -8, -56, 1653, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 96 } }, 0, -48, 1750, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, 16, -16, 1750, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 48, 40 } }, 24, 8, 1750, { .fields = { 8, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 16 } }, 40, 48, 1750, { .fields = { 16, 32 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 120 } }, 72, -56, 1750, { .fields = { 96, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 80, 184 } }, 80, -120, 1750, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 7.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView7SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 5, .field_6 = { 1, 0 } },
    { .firstSprite = 5, .spriteCount = 5, .field_6 = { 2, 0 } },
    { .firstSprite = 10, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView7SpriteSources) - 10 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 8.
///
/// The source slot retains this list address and is never read as sprites.
static SpriteBatch _gDryfieldNightMainStreetView8SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 9.
///
/// The source slot retains this list address and is never read as sprites.
static SpriteBatch _gDryfieldNightMainStreetView9SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 10.
///
/// The source slot retains this list address and is never read as sprites.
static SpriteBatch _gDryfieldNightMainStreetView10SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 11.
///
/// The source slot retains this list address and is never read as sprites.
static SpriteBatch _gDryfieldNightMainStreetView11SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 12.
///
/// The source slot retains this list address and is never read as sprites.
static SpriteBatch _gDryfieldNightMainStreetView12SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped views 13 and 24.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
/// Both views borrow this same source array and batch list.
static SpriteSource _gDryfieldNightMainStreetViews13And24SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, 16, -72, 2325, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, 24, -72, 2300, { .fields = { 88, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 40, -72, 1915, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, 32, -72, 2315, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 32, 0, 2205, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 24 } }, 32, -64, 2575, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 40 } }, 48, -80, 2575, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 56, -120, 2575, { .fields = { 48, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 80 } }, 64, -120, 2575, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 32, 0, 2575, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 32 } }, 40, -8, 2575, { .fields = { 56, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 56, -32, 2576, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 64 } }, 64, -40, 2575, { .fields = { 64, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 96, -96, 1294, { .fields = { 48, 24 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 104, -104, 1237, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 160 } }, 112, -104, 1190, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 80 } }, 104, -24, 1310, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 96, 0, 1339, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 88, 32, 1499, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 168 } }, 120, -112, 1165, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 168 } }, 128, -112, 1128, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 176 } }, 136, -112, 1128, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 144, 8, 1068, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 152, 8, 1019, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, 144, -120, 1043, { .fields = { 80, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 152, -120, 1006, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, 16, -72, 2379, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, 24, -72, 2196, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, 32, -80, 2033, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, 40, -80, 1895, { .fields = { 56, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, 48, -88, 1773, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, 56, -88, 1666, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, 64, -96, 1571, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 32 } }, 72, -104, 1487, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, 80, -104, 1411, { .fields = { 56, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped views 13 and 24.
///
/// The initial zero count keeps image strips; the final record ends the list.
/// Both views share visibility changes to these mutable ranges.
static SpriteBatch _gDryfieldNightMainStreetViews13And24SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 5, .field_6 = { 3, 0 } },
    { .firstSprite = 5, .spriteCount = 8 },
    { .firstSprite = 13, .spriteCount = 13, .field_6 = { 2, 0 } },
    { .firstSprite = 26, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetViews13And24SpriteSources) - 26, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 14.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
static SpriteSource _gDryfieldNightMainStreetView14SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -104, -104, 1184, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 184 } }, -160, -120, 932, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 184 } }, -152, -120, 939, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 40 } }, -144, -120, 988, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -136, -120, 1024, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 40 } }, -128, -120, 1163, { .fields = { 88, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -120, -120, 1378, { .fields = { 88, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -112, -112, 1532, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -104, -88, 1824, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -144, -40, 1001, { .fields = { 104, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -136, -8, 1088, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -128, -8, 1117, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 56 } }, -120, -8, 1137, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -40, -72, 2467, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -40, -40, 2814, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -48, 8, 2075, { .fields = { 72, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -40, -8, 2075, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -32, -80, 2119, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 96 } }, -24, -72, 2138, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 96 } }, -16, -72, 2174, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -8, 8, 1987, { .fields = { 72, 112 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 8 } }, -56, -72, 2130, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -64, -64, 2524, { .fields = { 40, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 8 } }, -56, -56, 2626, { .fields = { 48, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, -56, -48, 2564, { .fields = { 56, 16 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 0, -56, 3385, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, 8, -56, 3124, { .fields = { 112, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 72 } }, 16, -56, 3148, { .fields = { 120, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 24, -16, 3103, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 24, -56, 3099, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 32, -56, 3470, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -40, 16, 1830, { .fields = { 64, 8 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -32, 16, 1972, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -24, 16, 1979, { .fields = { 64, 24 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -80, 24, 1506, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -72, 24, 1506, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -64, 24, 1544, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -56, 24, 1591, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -48, 24, 1556, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -40, 24, 1625, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -32, 24, 1625, { .fields = { 64, 112 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -24, 24, 1625, { .fields = { 64, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -16, 24, 1625, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -104, 32, 1286, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -96, 32, 1335, { .fields = { 72, 200 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -88, 32, 1341, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -80, 32, 1379, { .fields = { 72, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -72, 32, 1379, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -64, 32, 1379, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -56, 32, 1405, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -48, 32, 1433, { .fields = { 72, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -112, 40, 1155, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -104, 40, 1149, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -96, 40, 1148, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -88, 40, 1207, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 56, 24 } }, 64, -72, 2716, { .fields = { 32, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 48, 16 } }, 16, -64, 2943, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 16 } }, -16, -80, 2182, { .fields = { 72, 16 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, -8, -80, 2365, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 16 } }, 0, -72, 2582, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, 8, -72, 2842, { .fields = { 80, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 16 } }, 16, -64, 2995, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 14.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView14SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 13, .field_6 = { 6, 0 } },
    { .firstSprite = 13, .spriteCount = 8, .field_6 = { 1, 0 } },
    { .firstSprite = 21, .spriteCount = 4, .field_6 = { 4, 0 } },
    { .firstSprite = 25, .spriteCount = 6 },
    { .firstSprite = 31, .spriteCount = 24, .field_6 = { 5, 0 } },
    { .firstSprite = 55, .spriteCount = 1, .field_6 = { 3, 0 } },
    { .firstSprite = 56, .spriteCount = 1, .field_6 = { 7, 0 } },
    { .firstSprite = 57, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView14SpriteSources) - 57, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 15.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
static SpriteSource _gDryfieldNightMainStreetView15SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 184 } }, -160, -120, 1000, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -152, -8, 1071, { .fields = { 104, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -144, -8, 1089, { .fields = { 120, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -136, 32, 1151, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -152, -120, 998, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -144, -120, 1037, { .fields = { 88, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -32, -8, 1956, { .fields = { 88, 56 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -80, -88, 1563, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -72, -88, 1633, { .fields = { 96, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -64, -88, 1952, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -56, -80, 1936, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -48, -80, 2002, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -40, -80, 1907, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -32, -80, 1882, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -24, -80, 1842, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -40, -8, 1958, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -32, 8, 1975, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 64, 24 } }, 48, -104, 1634, { .fields = { 40, 112 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 24, 16 } }, 24, -96, 1713, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 64, 24 } }, -40, -96, 1799, { .fields = { 40, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 8 } }, -136, -120, 1084, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 8 } }, -128, -120, 1125, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 16 } }, -120, -120, 1180, { .fields = { 88, 40 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 16 } }, -112, -120, 1219, { .fields = { 88, 24 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, -104, -120, 1279, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, -96, -120, 1346, { .fields = { 96, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 32 } }, -88, -120, 1420, { .fields = { 96, 32 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, -80, -112, 1503, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, -72, -104, 1598, { .fields = { 96, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 32 } }, -64, -104, 1704, { .fields = { 104, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 24 } }, -56, -96, 1825, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 15.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView15SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 6 },
    { .firstSprite = 6, .spriteCount = 11, .field_6 = { 3, 0 } },
    { .firstSprite = 17, .spriteCount = 2, .field_6 = { 2, 0 } },
    { .firstSprite = 19, .spriteCount = 1, .field_6 = { 4, 0 } },
    { .firstSprite = 20, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView15SpriteSources) - 20, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 16.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
static SpriteSource _gDryfieldNightMainStreetView16SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 48 } }, -8, -120, 1375, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, 0, -8, 1475, { .fields = { 104, 40 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 16, -56, 1489, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 40 } }, 24, -56, 1482, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 48 } }, 120, -120, 329, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 32 } }, 112, -72, 366, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 32 } }, 136, -72, 322, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 24 } }, 72, 24, 612, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 24 } }, 96, 24, 600, { .fields = { 48, 32 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 24 } }, 128, 24, 500, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 16 } }, 72, 48, 625, { .fields = { 48, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 16 } }, 96, 48, 600, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 16 } }, 128, 48, 500, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 16 } }, 64, 64, 650, { .fields = { 40, 16 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 16 } }, 88, 64, 600, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 16 } }, 128, 64, 500, { .fields = { 40, 200 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 56, 80, 690, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 64, 80, 675, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 40 } }, 72, 80, 675, { .fields = { 120, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 40 } }, 80, 80, 650, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 48, 40 } }, 112, 80, 650, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 32 } }, -8, -72, 1412, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 32 } }, -8, -40, 1475, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 24 } }, 96, 0, 600, { .fields = { 48, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 24 } }, 128, 0, 550, { .fields = { 48, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 40 } }, 104, -40, 550, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 40 } }, 136, -40, 500, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 16.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView16SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView16SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 17.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
static SpriteSource _gDryfieldNightMainStreetView17SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -40, -40, 2877, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 80 } }, -32, -48, 2877, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -24, 0, 2877, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -24, -48, 2904, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -24, -72, 1800, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -32, 24, 1786, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -16, -72, 1801, { .fields = { 40, 112 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -8, -72, 1810, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 0, -8, 1800, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 8, 24, 1861, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 16, 24, 1774, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 40, -120, 667, { .fields = { 40, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 16, 64, 821, { .fields = { 72, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 144, 72, 591, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 24, 8, 873, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 24, 56, 873, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 88 } }, 32, 0, 800, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 120 } }, 40, -24, 756, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 216 } }, 48, -120, 775, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 224 } }, 56, -120, 750, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 232 } }, 64, -120, 725, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 240 } }, 72, -120, 700, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 240 } }, 80, -120, 650, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 240 } }, 88, -120, 600, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 240 } }, 96, -120, 491, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 168 } }, 104, -48, 462, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, 112, -8, 464, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 120 } }, 120, 0, 467, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 128, 8, 464, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 128, 72, 462, { .fields = { 32, 192 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 136, 72, 594, { .fields = { 24, 56 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 16 } }, -72, -120, 1063, { .fields = { 16, 24 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 24 } }, -56, -120, 1080, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -32, -120, 1061, { .fields = { 0, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 8 } }, -32, -112, 1112, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 8 } }, -32, -104, 1228, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, -32, -96, 1333, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, -32, -88, 1356, { .fields = { 16, 112 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 8 } }, -88, -120, 1043, { .fields = { 16, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 8, 8 } }, 16, -56, 3564, { .fields = { 24, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 24, 16 } }, 16, -48, 3056, { .fields = { 8, 40 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 24, 16 } }, 40, -48, 3135, { .fields = { 16, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_256, { .fields = { 32, 24 } }, -8, -56, 2979, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 17.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView17SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 4, .field_6 = { 3, 0 } },
    { .firstSprite = 4, .spriteCount = 7 },
    { .firstSprite = 11, .spriteCount = 20, .field_6 = { 5, 0 } },
    { .firstSprite = 31, .spriteCount = 8, .field_6 = { 1, 0 } },
    { .firstSprite = 39, .spriteCount = 3, .field_6 = { 4, 0 } },
    { .firstSprite = 42, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView17SpriteSources) - 42, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 18.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
static SpriteSource _gDryfieldNightMainStreetView18SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 136 } }, -160, -16, 358, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, -136, -8, 402, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 240 } }, -128, -120, 468, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 240 } }, -88, -120, 565, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 112 } }, -72, -120, 574, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, -72, -8, 582, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -64, -8, 592, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -56, -8, 669, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -48, 48, 669, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -40, 64, 706, { .fields = { 32, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -32, 64, 706, { .fields = { 32, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 18.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView18SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView18SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 19.
///
/// The source slot retains this list address and is never read as sprites.
static SpriteBatch _gDryfieldNightMainStreetView19SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list retaining the decoded background in mapped view 20.
///
/// The source slot retains this list address and is never read as sprites.
static SpriteBatch _gDryfieldNightMainStreetView20SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 21.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
static SpriteSource _gDryfieldNightMainStreetView21SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 184 } }, -160, -72, 689, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 184 } }, -144, -72, 705, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 176 } }, -136, -72, 731, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 160 } }, -128, -64, 766, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 144 } }, -120, -48, 785, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 144 } }, -112, -48, 835, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, -104, -40, 888, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, -96, -40, 969, { .fields = { 32, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 120 } }, -88, -40, 1029, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -80, -32, 1116, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -72, -32, 1151, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -64, -32, 1214, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 88 } }, -56, -24, 1269, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 184 } }, -48, -120, 1625, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -56, -120, 1625, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -56, -8, 1625, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 184 } }, 8, -120, 1625, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -32, -40, 1675, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -128, 48, 1675, { .fields = { 120, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 40, 48, 1675, { .fields = { 0, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 64 } }, -160, -24, 1675, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 88 } }, -120, -32, 1675, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 56, 72 } }, -96, -32, 1675, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 88 } }, -40, -32, 1675, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 16 } }, 8, -32, 1675, { .fields = { 32, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 48, 8 } }, 0, -8, 1675, { .fields = { 16, 24 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 32 } }, 8, 8, 1675, { .fields = { 32, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 96 } }, 48, -40, 1675, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 64, 72 } }, 64, -32, 1675, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 88 } }, 128, -32, 1675, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 16 } }, -72, 40, 1675, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 24 } }, -24, 32, 1675, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 32 } }, 8, 24, 1675, { .fields = { 48, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 24 } }, 56, 32, 1675, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 152, 40, 1675, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -88, 64, 687, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 40 } }, -112, 48, 687, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 56 } }, -144, 40, 687, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 48 } }, -160, 48, 687, { .fields = { 72, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 21.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView21SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 13 },
    { .firstSprite = 13, .spriteCount = 4, .field_6 = { 3, 0 } },
    { .firstSprite = 17, .spriteCount = 13, .field_6 = { 2, 0 } },
    { .firstSprite = 30, .spriteCount = 5, .field_6 = { 4, 0 } },
    { .firstSprite = 35, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView21SpriteSources) - 35, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 22.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
static SpriteSource _gDryfieldNightMainStreetView22SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 184 } }, -160, -72, 589, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 184 } }, -144, -72, 605, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 176 } }, -136, -72, 631, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 160 } }, -128, -64, 666, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 144 } }, -120, -48, 685, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 136 } }, -112, -48, 736, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, -104, -40, 763, { .fields = { 48, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, -96, -40, 794, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 120 } }, -88, -40, 854, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -80, -32, 916, { .fields = { 40, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -72, -32, 951, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -64, -32, 1064, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 88 } }, -56, -24, 1169, { .fields = { 24, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 184 } }, -48, -120, 1637, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -56, -120, 1625, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -56, -8, 1625, { .fields = { 104, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -16, -120, 1625, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -8, -120, 1625, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 120 } }, 0, -80, 1625, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 88 } }, 8, -32, 1625, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 16, 24, 1625, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -104, -16, 1675, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 0, -8, 1700, { .fields = { 32, 216 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 152, 0, 1700, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -48, 24, 1675, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -48, 40, 1675, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -128, 48, 1675, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -104, 48, 1675, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 8 } }, -160, -24, 1675, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 16 } }, -160, -8, 1675, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -160, 16, 1675, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 8 } }, -160, 32, 1675, { .fields = { 80, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 88 } }, -120, -32, 1675, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -128, 8, 1675, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 56, 16 } }, -104, -32, 1675, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 56, 16 } }, -104, -8, 1675, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 16 } }, -104, 8, 1675, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -72, 16, 1675, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 64, 8 } }, -104, 32, 1675, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 88 } }, -40, -24, 1675, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 64 } }, -16, -16, 1675, { .fields = { 112, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 48 } }, 8, 0, 1700, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 40 } }, 24, 8, 1700, { .fields = { 120, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 48, 32 } }, 40, 24, 1700, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 48 } }, 88, 8, 1700, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 40 } }, 104, 8, 1700, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 48 } }, 128, 8, 1700, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 96, 16, 1700, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 80, 88, 1700, { .fields = { 72, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 104, 88, 1700, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 80, 32 } }, -24, 24, 1700, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 80, 64 } }, 56, 24, 1700, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 32 } }, 136, 24, 1700, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 22.
///
/// The initial zero count keeps image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView22SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 13 },
    { .firstSprite = 13, .spriteCount = 8, .field_6 = { 3, 0 } },
    { .firstSprite = 21, .spriteCount = 26, .field_6 = { 2, 0 } },
    { .firstSprite = 47, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView22SpriteSources) - 47, .field_6 = { 4, 0 } },
    { .firstSprite = ARRAY_SIZE(_gDryfieldNightMainStreetView22SpriteSources), .spriteCount = 0, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture rectangles and sorting depths for mapped view 23.
///
/// Dimensions and positions are pixels; depths are unscaled OT sorting values.
/// All 57 source elements are selected by its batches, without a terminator.
/// The source and cached packets remain valid while this overlay is loaded.
static SpriteSource _gDryfieldNightMainStreetView23SpriteSources[] = {
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 184 } }, -160, -72, 639, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 184 } }, -144, -72, 655, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 176 } }, -136, -72, 681, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 160 } }, -128, -64, 716, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 144 } }, -120, -48, 785, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 136 } }, -112, -48, 836, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, -104, -40, 888, { .fields = { 48, 128 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 128 } }, -96, -40, 969, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 120 } }, -88, -40, 1004, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -80, -32, 1041, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -72, -32, 1076, { .fields = { 40, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -64, -32, 1139, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 88 } }, -56, -24, 1219, { .fields = { 0, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -56, -120, 1625, { .fields = { 120, 152 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 184 } }, -48, -120, 1625, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -56, -8, 1625, { .fields = { 120, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -16, -120, 1625, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -8, -120, 1625, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 120 } }, 0, -80, 1625, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 88 } }, 8, -32, 1625, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 16, 24, 1625, { .fields = { 80, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -104, -16, 1700, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 0, -8, 1700, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 152, 0, 1700, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -128, 8, 1700, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -48, 24, 1700, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -48, 40, 1700, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -128, 48, 1700, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -104, 48, 1700, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 8 } }, -160, -24, 1700, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 16 } }, -160, -8, 1700, { .fields = { 32, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 8 } }, -160, 16, 1700, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 8 } }, -160, 32, 1700, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 88 } }, -120, -32, 1700, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 56, 16 } }, -104, -32, 1700, { .fields = { 16, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 56, 16 } }, -104, -8, 1700, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 16 } }, -104, 8, 1700, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 64, 8 } }, -104, 32, 1700, { .fields = { 8, 136 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -72, 16, 1700, { .fields = { 40, 80 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 88 } }, -40, -24, 1700, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 64 } }, -16, -16, 1700, { .fields = { 112, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 48 } }, 8, 0, 1700, { .fields = { 104, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 40 } }, 24, 8, 1700, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 48, 32 } }, 40, 24, 1700, { .fields = { 32, 88 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 72, 48 } }, 88, 8, 1700, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 96, 16, 1675, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 40, 32 } }, -24, 24, 1675, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 16, 32, 1675, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 32 } }, 24, 24, 1675, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 72, 64 } }, 56, 24, 1675, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 40 } }, 128, 24, 1675, { .fields = { 72, 120 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -160, -32, 1675, { .fields = { 72, 248 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 64 } }, -160, -24, 1675, { .fields = { 80, 184 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 24, 88 } }, -128, -24, 1675, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 64 } }, -104, 0, 1675, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 32, 56 } }, -88, 0, 1675, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_NIGHT_MAIN_STREET_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_CLUT_255, { .fields = { 16, 32 } }, -56, 8, 1675, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MAIN_STREET_SPRITE_OPAQUE_MODULATED },
};

/// Sprite ranges over the decoded background for mapped view 23.
///
/// The initial zero count retains image strips; the final record ends the list.
static SpriteBatch _gDryfieldNightMainStreetView23SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 13 },
    { .firstSprite = 13, .spriteCount = 8, .field_6 = { 3, 0 } },
    { .firstSprite = 21, .spriteCount = 24, .field_6 = { 2, 0 } },
    { .firstSprite = 45, .spriteCount = 6, .field_6 = { 4, 0 } },
    { .firstSprite = 51, .spriteCount = ARRAY_SIZE(_gDryfieldNightMainStreetView23SpriteSources) - 51, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Unselected empty sprite list retained after the view-23 resources.
///
/// Contains a decoded-background header and an end marker, with no source
/// ranges. View 24 instead shares view 13's populated batch list.
static SpriteBatch _gDryfieldNightMainStreetUnusedEmptySpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gDryfieldNightMainStreetSpriteViews[24] = {
    { { .empty = _gDryfieldNightMainStreetView1SpriteBatches }, _gDryfieldNightMainStreetView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView2SpriteSources }, _gDryfieldNightMainStreetView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView3SpriteSources }, _gDryfieldNightMainStreetView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView4SpriteSources }, _gDryfieldNightMainStreetView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView5SpriteSources }, _gDryfieldNightMainStreetView5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView6SpriteSources }, _gDryfieldNightMainStreetView6SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView7SpriteSources }, _gDryfieldNightMainStreetView7SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMainStreetView8SpriteBatches }, _gDryfieldNightMainStreetView8SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMainStreetView9SpriteBatches }, _gDryfieldNightMainStreetView9SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMainStreetView10SpriteBatches }, _gDryfieldNightMainStreetView10SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMainStreetView11SpriteBatches }, _gDryfieldNightMainStreetView11SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMainStreetView12SpriteBatches }, _gDryfieldNightMainStreetView12SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetViews13And24SpriteSources }, _gDryfieldNightMainStreetViews13And24SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView14SpriteSources }, _gDryfieldNightMainStreetView14SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView15SpriteSources }, _gDryfieldNightMainStreetView15SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView16SpriteSources }, _gDryfieldNightMainStreetView16SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView17SpriteSources }, _gDryfieldNightMainStreetView17SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView18SpriteSources }, _gDryfieldNightMainStreetView18SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMainStreetView19SpriteBatches }, _gDryfieldNightMainStreetView19SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMainStreetView20SpriteBatches }, _gDryfieldNightMainStreetView20SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView21SpriteSources }, _gDryfieldNightMainStreetView21SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView22SpriteSources }, _gDryfieldNightMainStreetView22SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetView23SpriteSources }, _gDryfieldNightMainStreetView23SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMainStreetViews13And24SpriteSources }, _gDryfieldNightMainStreetViews13And24SpriteBatches, NULL },
};

/// Directed logical-view boundaries shared by room selectors 1..3.
///
/// Positions/radii use game coordinates and normals use 4096 per unit. Source
/// and destination selectors are logical views; the final LAST record is live.
/// Room activation binds and mutates these nodes; keep their storage loaded.
static WorldCollisionTrigger _gDryfieldNightMainStreetViewBoundaryTriggers[26] = {
    { NULL, NULL, NULL, { -2385, -3392, 351, 0 }, { { -2863, -4416, -165, 0 }, { 2863, -4416, 165, 0 }, { -2863, 4416, -165, 0 }, { 2863, 4416, 165, 0 } }, { 236, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 5246, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2337, -3376, 239, 0 }, { { 2868, -4400, 137, 0 }, { -2867, -4400, -136, 0 }, { 2868, 4400, 137, 0 }, { -2867, 4400, -136, 0 } }, { -195, 0, 4092, 0 }, { 0, 0, 4096, 0 }, 5246, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3665, -3232, 5071, 0 }, { { -1383, -4256, 519, 0 }, { 1381, -4256, -521, 0 }, { -1383, 4256, 519, 0 }, { 1381, 4256, -521, 0 } }, { -1447, 0, -3843, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3764, -3216, 4924, 0 }, { { 1447, -4240, -514, 0 }, { -1449, -4240, 512, 0 }, { 1447, 4240, -514, 0 }, { -1449, 4240, 512, 0 } }, { 1368, 0, 3863, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6177, -3200, 5327, 0 }, { { 980, -4224, 41, 0 }, { -979, -4224, -40, 0 }, { 980, 4224, 41, 0 }, { -979, 4224, -40, 0 } }, { -171, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6210, -3280, 5598, 0 }, { { -979, -4304, 7, 0 }, { 980, -4304, -7, 0 }, { -979, 4304, 7, 0 }, { 980, 4304, -7, 0 } }, { -30, 0, -4117, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5988, -3552, -292, 0 }, { { -959, -4576, 198, 0 }, { 960, -4576, -198, 0 }, { -959, 4576, 198, 0 }, { 960, 4576, -198, 0 } }, { -829, 0, -4015, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6051, -3536, -452, 0 }, { { 949, -4560, -244, 0 }, { -949, -4560, 245, 0 }, { 949, 4560, -244, 0 }, { -949, 4560, 245, 0 } }, { 1025, 0, 3983, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -3488, -2833, 0 }, { { -8, -4224, -2308, 0 }, { 8, -4224, 2308, 0 }, { -8, 4224, -2308, 0 }, { 8, 4224, 2308, 0 } }, { 4105, 0, -15, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 2, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5072, -3488, -2960, 0 }, { { 8, -4224, 2292, 0 }, { -8, -4224, -2292, 0 }, { 8, 4224, 2292, 0 }, { -8, 4224, -2292, 0 } }, { -4102, 0, 14, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 7, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5312, -2848, 0x28E0, 0 }, { { 235, -4224, -1330, 0 }, { -240, -4224, 1325, 0 }, { 235, 4224, -1330, 0 }, { -240, 4224, 1325, 0 } }, { 4031, 0, 720, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -3008, 0x2940, 0 }, { { -239, -4224, 1326, 0 }, { 236, -4224, -1329, 0 }, { -239, 4224, 1326, 0 }, { 236, 4224, -1329, 0 } }, { -4033, 0, -722, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1314, -3296, 4926, 0 }, { { 1314, -4256, 675, 0 }, { -1313, -4256, -675, 0 }, { 1314, 4256, 675, 0 }, { -1313, 4256, -675, 0 } }, { -1878, 0, 3651, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1076, -3264, 5229, 0 }, { { -1235, -4256, -651, 0 }, { 1236, -4256, 651, 0 }, { -1235, 4256, -651, 0 }, { 1236, 4256, 651, 0 } }, { 1912, 0, -3632, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5056, -3232, 2256, 0 }, { { 24, -4224, 2036, 0 }, { -24, -4224, -2036, 0 }, { 24, 4224, 2036, 0 }, { -24, 4224, -2036, 0 } }, { -4105, 0, 48, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -2944, 2272, 0 }, { { -24, -4224, -2100, 0 }, { 24, -4224, 2100, 0 }, { -24, 4224, -2100, 0 }, { 24, 4224, 2100, 0 } }, { 4111, 0, -47, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5248, -2912, 6816, 0 }, { { -24, -4224, -1844, 0 }, { 24, -4224, 1844, 0 }, { -24, 4224, -1844, 0 }, { 24, 4224, 1844, 0 } }, { 4101, 0, -54, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5040, -3008, 6912, 0 }, { { -24, -4224, 1844, 0 }, { 24, -4224, -1844, 0 }, { -24, 4224, 1844, 0 }, { 24, 4224, -1844, 0 } }, { -4103, 0, -54, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -384, -2624, 7537, 0 }, { { -1800, -4416, 722, 0 }, { 1793, -4416, -730, 0 }, { -1800, 4416, 722, 0 }, { 1793, 4416, -730, 0 } }, { -1542, 0, -3815, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -448, -2816, 7328, 0 }, { { 1841, -4416, -751, 0 }, { -1852, -4416, 741, 0 }, { 1841, 4416, -751, 0 }, { -1852, 4416, 741, 0 } }, { 1537, 0, 3806, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2465, -2944, 9983, 0 }, { { 211, -4416, -1982, 0 }, { -215, -4416, 1978, 0 }, { 211, 4416, -1982, 0 }, { -215, 4416, 1978, 0 } }, { 4081, 0, 438, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2370, -2976, 0x27BE, 0 }, { { -217, -4416, 1976, 0 }, { 209, -4416, -1985, 0 }, { -217, 4416, 1976, 0 }, { 209, 4416, -1985, 0 } }, { -4083, 0, -440, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -400, -3008, -2064, 0 }, { { -1759, -4416, -117, 0 }, { 1759, -4416, 117, 0 }, { -1759, 4416, -117, 0 }, { 1759, 4416, 117, 0 } }, { 271, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 4748, 0, 21, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -224, -3296, -2176, 0 }, { { 1759, -4416, 117, 0 }, { -1759, -4416, -117, 0 }, { 1759, 4416, 117, 0 }, { -1759, 4416, -117, 0 } }, { -273, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 4748, 0, 2, 21, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1889, -3232, -4353, 0 }, { { 265, -4416, -2218, 0 }, { -265, -4416, 2218, 0 }, { 265, 4416, -2218, 0 }, { -265, 4416, 2218, 0 } }, { 4078, 0, 486, 0 }, { 0, 0, 4096, 0 }, 4937, 0, 21, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1696, -3136, -4480, 0 }, { { -265, -4416, 2218, 0 }, { 265, -4416, -2218, 0 }, { -265, 4416, 2218, 0 }, { 265, 4416, -2218, 0 } }, { -4079, 0, -488, 0 }, { 0, 0, 4096, 0 }, 4937, 0, 2, 21, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction and warp regions for main-street room selector 1.
///
/// Seven regions select warp descriptors; the others request CAP or room actions.
/// The final LAST record is included. Positions/radii use game coordinates and
/// normals use 4096 per unit. Binding mutates the borrowed nodes and their links.
static WorldCollisionTrigger _gDryfieldNightMainStreetRoom1ActionTriggers[12] = {
    { NULL, NULL, NULL, { -384, 0, -3712, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, -2816, 0 }, { { -416, 0, -864, 0 }, { 416, 0, -864, 0 }, { -416, 0, 864, 0 }, { 416, 0, 864, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 957, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6880, -48, 2016, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6848, -48, 6000, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 942, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, 0x27E0, 0 }, { { -416, 0, -800, 0 }, { 416, 0, -800, 0 }, { -416, 0, 800, 0 }, { 416, 0, 800, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 900, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3584, -48, 0x2A00, 0 }, { { 736, 0, -416, 0 }, { 736, 0, 416, 0 }, { -736, 0, -416, 0 }, { -736, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 844, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 97, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -736, -64, 0x2842, 0 }, { { 1024, 0, -960, 0 }, { 1024, 0, 960, 0 }, { -1024, 0, -960, 0 }, { -1024, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1402, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -225, -48, 8767, 0 }, { { -415, 0, -415, 0 }, { 417, 0, -415, 0 }, { -416, 0, 416, 0 }, { 416, 0, 416, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 586, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 115, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5600, -64, -3600, 0 }, { { -1600, 0, -1872, 0 }, { 1600, 0, -1872, 0 }, { -1600, 0, 1873, 0 }, { 1600, 0, 1873, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 16, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -2305, -64, 8447, 0 }, { { -2192, 0, -2576, 0 }, { 2193, 0, -2576, 0 }, { -2192, 0, 2576, 0 }, { 2193, 0, 2576, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 3376, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 17, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5920, -64, -4064, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2962, -64, 7199, 0 }, { { -624, 0, -512, 0 }, { 625, 0, -512, 0 }, { -624, 0, 513, 0 }, { 625, 0, 513, 0 } }, { 0, 4096, 0, 0 }, { -2598, 0, -3166, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 19, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger gDryfieldNightMainStreetRooms2And3ActionTriggers[12] = {
    { NULL, NULL, NULL, { -384, 0, -3712, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, -2816, 0 }, { { -416, 0, -864, 0 }, { 416, 0, -864, 0 }, { -416, 0, 864, 0 }, { 416, 0, 864, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 957, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6880, -48, 2016, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6848, -48, 6000, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 942, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, 0x27E0, 0 }, { { -416, 0, -800, 0 }, { 416, 0, -800, 0 }, { -416, 0, 800, 0 }, { 416, 0, 800, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 900, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3584, -48, 0x2A00, 0 }, { { 736, 0, -416, 0 }, { 736, 0, 416, 0 }, { -736, 0, -416, 0 }, { -736, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 844, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 97, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -736, -64, 0x2842, 0 }, { { 1024, 0, -960, 0 }, { 1024, 0, 960, 0 }, { -1024, 0, -960, 0 }, { -1024, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1402, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -929, -48, 8303, 0 }, { { -415, 0, -687, 0 }, { 417, 0, -687, 0 }, { -416, 0, 688, 0 }, { 416, 0, 688, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 115, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5600, -64, -3600, 0 }, { { -1600, 0, -1872, 0 }, { 1600, 0, -1872, 0 }, { -1600, 0, 1873, 0 }, { 1600, 0, 1873, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 16, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -2305, -64, 8447, 0 }, { { -2192, 0, -2576, 0 }, { 2193, 0, -2576, 0 }, { -2192, 0, 2576, 0 }, { 2193, 0, 2576, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 3376, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 17, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5920, -64, -4064, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2962, -64, 7199, 0 }, { { -624, 0, -512, 0 }, { 625, 0, -512, 0 }, { -624, 0, 513, 0 }, { 625, 0, 513, 0 } }, { 0, 4096, 0, 0 }, { -2598, 0, -3166, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 19, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Ten mutable point lights shared by all three main-street room selectors.
///
/// Positions and falloff radii use room/world coordinate units; RGB is Q12.
/// All lights admit every logical view. Activation parents them to the view;
/// composition and shading overwrite their caches and attenuation.
static WorldCoordPointLight _gDryfieldNightMainStreetPointLights[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4417, -2000, 2032 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4417, -2000, -1229 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -2000, -2607 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, ONE, 3686 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -2000, 7297 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, ONE, 3686 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4253, -2000, 0x2A75 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 9652 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 6623 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1664, -2000, 9386 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, -3140 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3404 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 316 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 500, 2000 },
};

/// The shared authored light collection: ten point lights, no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightMainStreetRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightMainStreetPointLights),
    .pointLights           = _gDryfieldNightMainStreetPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Resource keys matched by the map overlay's main-street placements.
enum {
    DRYFIELD_NIGHT_MAIN_STREET_RESOURCE_DESERT_CHASER = 1,
    DRYFIELD_NIGHT_MAIN_STREET_RESOURCE_BRAIN_STINGER = 3,
    DRYFIELD_NIGHT_MAIN_STREET_RESOURCE_MOTH          = 8,
    DRYFIELD_NIGHT_MAIN_STREET_RESOURCE_GARY_DOUGLAS  = 106,
};

/// Desert Chaser and Moth file/task recipes for placement variant 1.
///
/// Entry IDs match the map's placements. The terminal entry ends the resource
/// scan; these records and their borrowed task descriptors live with the overlays.
static AreaResource _gDryfieldNightMainStreetVariant1Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_MAIN_STREET_RESOURCE_DESERT_CHASER, .fileNumber = 1, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00100DesertChaserTaskDesc },
    { .entryId = DRYFIELD_NIGHT_MAIN_STREET_RESOURCE_MOTH, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor300700MothTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Gary Douglas's opening-scene file/task recipe for placement variant 2.
///
/// The map's entry 106 selects descriptor zero. The terminal entry ends the
/// scan; keep the room, placement and actor descriptor storage loaded while used.
static AreaResource _gDryfieldNightMainStreetVariant2Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_MAIN_STREET_RESOURCE_GARY_DOUGLAS, .fileNumber = 361, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor136100SceneTaskDescriptors },
    { .entryId = AREA_PLACEMENT_END },
};

/// Brain Stinger file/task recipe for the two placements in variant 7.
///
/// Both map placements use entry 3 and descriptor zero. The terminal entry ends
/// the scan; the recipe borrows its task table from the loaded actor overlay.
static AreaResource _gDryfieldNightMainStreetVariant7Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_MAIN_STREET_RESOURCE_BRAIN_STINGER, .fileNumber = 3, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor100300Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightMainStreetAreaVariants[13] = {
    [1] = { .placements = gMapDryfieldFullMainStreetVariant1Placements, .resources = _gDryfieldNightMainStreetVariant1Resources },
    [2] = { .placements = gMapDryfieldFullMainStreetVariant2Placements, .resources = _gDryfieldNightMainStreetVariant2Resources },
    [7] = { .placements = gMapDryfieldFullMainStreetVariant7Placements, .resources = _gDryfieldNightMainStreetVariant7Resources },
};

/// Minimum ambient RGB and stored brightness for logical views 1..24.
///
/// Entry zero holds the view count. Colours use 16 units per 8-bit level;
/// the same table is borrowed by all three room selectors, without view mapping.
static WorldCoordRoomAmbientEntry _gDryfieldNightMainStreetRoomAmbientTable[25] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightMainStreetRoomAmbientTable) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 615, 617, 618, 616 } },
    { .color = { 616, 615, 615, 615 } },
    { .color = { 617, 618, 617, 617 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 617, 615, 616, 615 } },
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
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

enum {
    DRYFIELD_NIGHT_MAIN_STREET_SURFACE_4_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    DRYFIELD_NIGHT_MAIN_STREET_SURFACE_4_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B,
    DRYFIELD_NIGHT_MAIN_STREET_SURFACE_2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    DRYFIELD_NIGHT_MAIN_STREET_SURFACE_2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
    DRYFIELD_NIGHT_MAIN_STREET_SURFACE_3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2D,
    DRYFIELD_NIGHT_MAIN_STREET_SURFACE_3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2F,
};

/// Surface class 4's footstep cue bases; scripted jumps reuse the walking pair.
///
/// These type-1 requests borrow the loaded room bank; animation cue 1 adds one
/// to the selected base, and companion playback adds 100.
static WorldCollisionFootstepSounds _gDryfieldNightMainStreetSurface4FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_MAIN_STREET_SURFACE_4_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_MAIN_STREET_SURFACE_4_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_MAIN_STREET_SURFACE_4_WALK_SOUND,
};

/// Surface class 2's footstep cue bases; scripted jumps reuse the walking pair.
///
/// These type-1 requests borrow the loaded room bank; animation cue 1 adds one
/// to the selected base, and companion playback adds 100.
static WorldCollisionFootstepSounds _gDryfieldNightMainStreetSurface2FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_MAIN_STREET_SURFACE_2_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_MAIN_STREET_SURFACE_2_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_MAIN_STREET_SURFACE_2_WALK_SOUND,
};

/// Surface class 3's footstep cue bases; scripted jumps reuse the walking pair.
///
/// These type-1 requests borrow the loaded room bank; animation cue 1 adds one
/// to the selected base, and companion playback adds 100.
static WorldCollisionFootstepSounds _gDryfieldNightMainStreetSurface3FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_MAIN_STREET_SURFACE_3_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_MAIN_STREET_SURFACE_3_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_MAIN_STREET_SURFACE_3_WALK_SOUND,
};

/// Blocking policy without footstep cues, shared by surface classes 0 and 5..7.
///
/// Applies pushback. The first byte's role is unproven; its authored zero is retained.
static WorldCollisionSurfaceProperties _gDryfieldNightMainStreetBlockingSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 4 blocks probes and allows weapon impacts, with its own footstep pair.
///
/// Applies pushback. The first byte's role is unproven; its authored zero is retained.
static WorldCollisionSurfaceProperties _gDryfieldNightMainStreetSurface4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMainStreetSurface4FootstepSounds,
};

/// Surface class 2 blocks probes and allows weapon impacts, with its own footstep pair.
///
/// Applies pushback. The first byte's role is unproven; its authored zero is retained.
static WorldCollisionSurfaceProperties _gDryfieldNightMainStreetSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMainStreetSurface2FootstepSounds,
};

/// Surface class 3 blocks probes and allows weapon impacts, with its own footstep pair.
///
/// Applies pushback. The first byte's role is unproven; its authored zero is retained.
static WorldCollisionSurfaceProperties _gDryfieldNightMainStreetSurface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMainStreetSurface3FootstepSounds,
};

/// Surface class 1 passes probes and ignores weapon impacts, without footstep cues.
///
/// Applies pushback. The first byte's role is unproven; its authored zero is retained.
static WorldCollisionSurfaceProperties _gDryfieldNightMainStreetPassThroughSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldNightMainStreetSurfacePropertiesByClass[8] = {
    &_gDryfieldNightMainStreetBlockingSilentSurface,
    &_gDryfieldNightMainStreetPassThroughSilentSurface,
    &_gDryfieldNightMainStreetSurface2Properties,
    &_gDryfieldNightMainStreetSurface3Properties,
    &_gDryfieldNightMainStreetSurface4Properties,
    &_gDryfieldNightMainStreetBlockingSilentSurface,
    &_gDryfieldNightMainStreetBlockingSilentSurface,
    &_gDryfieldNightMainStreetBlockingSilentSurface,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Latest staged motel-scene start indication (0 no accepted start, 1 requested).
///
/// Each eligible resolver branch clears it, including queries, then sets it
/// after requesting a task even if spawn fails. It is not a live-task handle.
/// Retains seven zero bytes whose role and grouping remain unproven.
static RoomEventStartStorage _gMainStreetStagedSceneStartStorage = { 0 };

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

u8 gRoomEventActive = 0;

RoomLatchedEventStorage gRoomEventLatched;

RoomEventReq gRoomEventReq;

#include "../../shared/room/room_event_staged_task.inc.c"

#include "../../shared/room/room_event.inc.c"

/// State-dispatch template for the room receiver: initialize, idle, release.
///
/// The dispatcher copies all three callbacks before indexing by Task::state.
static const TaskFuncTable3 _gDryfieldNightMainStreetRoomTaskStates = {
    .funcs = {
        [DRYFIELD_NIGHT_MAIN_STREET_ROOM_STATE_INITIALIZE] = _dryfieldNightMainStreetInitializeRoomTask,
        [DRYFIELD_NIGHT_MAIN_STREET_ROOM_STATE_IDLE]       = _dryfieldNightMainStreetRoomIdle,
        [DRYFIELD_NIGHT_MAIN_STREET_ROOM_STATE_RELEASE]    = taskKill,
    },
};

#include "../../shared/main_street_resolve_msg.inc.c"

#include "../../shared/main_street_play_time_task.inc.c"

#include "../../shared/main_street_cap_sound_cue.inc.c"

#include "../../shared/main_street_talk_msg.inc.c"

/// Refuses every key-item use in the nighttime main street.
///
/// Handles message 0x13F1 with the collected item ID and a zero second payload.
/// Reads no argument and returns 0 to select the item menu's cannot-use notice,
/// without consuming an item or changing the room.
static s32 _dryfieldNightMainStreetRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Ignores room-specific trigger actions in the nighttime main street.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`, borrowing a `DirectionActionRequest`
/// for synchronous dispatch with a zero second payload. Reads no argument,
/// retains no request and leaves the room unchanged. Returns 0, which the
/// trigger dispatcher ignores.
static s32 _dryfieldNightMainStreetIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the night main street's room receiver and restores saved background visibility.
///
/// State 0 requires the loaded room message table and writable sprite batches.
/// Publishes a borrowed task in `GAME_TASK_SLOT_ROOM`, advances to idle state 1
/// and enables CAP completion sound cues. Stop messaging before task teardown.
static void _dryfieldNightMainStreetInitializeRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightMainStreetMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _dryfieldNightMainStreetRestoreSectionSpriteVisibility();
    task->state                     = task->state + 1;
    gCapRoomCompletionSoundsEnabled = CAP_ROOM_COMPLETION_SOUNDS_ENABLED;
}

/// Keeps the initialized room entry task alive without changing its state.
static void _dryfieldNightMainStreetRoomIdle(Task* task)
{
}

void dryfieldNightMainStreetRoomTask(Task* task)
{
    TaskFuncTable3 roomStates;

    roomStates = _gDryfieldNightMainStreetRoomTaskStates;
    roomStates.funcs[task->state](task);
}

/// Restores a balcony section's sprite visibility from its saved state.
///
/// `stateFlag` must yield 0 or 1 for the two-list directory. Borrows the address
/// of its stored root so the directory is loaded after reading the saved flag.
/// Uses the live session and stage's first sprite variant; slots start at zero.
/// Each stream begins with (view slot, 255), immediately followed by a batch
/// write (batch slot, hidden byte). A selector's following write is consumed
/// without another selector/end check; consecutive selectors are unsupported.
/// Slot 255 ends the stream, but the first view lookup precedes that check, so
/// a terminator-only stream is invalid. All selected batches must be writable.
/// Neither the directory address nor any command pointer is retained.
static inline void _dryfieldNightMainStreetApplySectionSpriteVisibility(SpriteVisibilityCommand** const* commandListsAddress, s32 stateFlag)
{
    GameLocationKey*          location;
    SpriteView*               areaViews;
    SpriteBatch*              batches;
    SpriteVisibilityCommand*  command;
    s16                       savedState;
    SpriteVisibilityCommand** directory;
    savedState = gameFlagGetNibble(stateFlag);
    directory  = *commandListsAddress;
    command    = directory[savedState];
    location   = &gGameSession->location;
    areaViews  = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    batches    = areaViews[command->slotIndex].batches;
    if (command->slotIndex != DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END) {
        do {
            if (command->operation == DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_SELECT_VIEW) {
                batches = areaViews[command->slotIndex].batches;
                command++;
            }
            batches[command->slotIndex].hidden = command->operation;
            command++;
        } while (command->slotIndex != DRYFIELD_NIGHT_MAIN_STREET_SPRITE_PATCH_END);
    }
}

/// Restores main-street background visibility from four saved balcony-section states.
///
/// Requires loaded writable sprite batches and saved states for sections 3,
/// 4, 5 and 7 in 0..1, each indexing its two-list table. The byte streams select view
/// slots and write batch hidden flags, ending at 0xFF; no packets are freed.
/// The room entry task calls this after installing its message receiver.
static void _dryfieldNightMainStreetRestoreSectionSpriteVisibility(void)
{
    _dryfieldNightMainStreetApplySectionSpriteVisibility(&_gDryfieldNightMainStreetBalconySection3SpritePatchTable, GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_3_STATE);
    _dryfieldNightMainStreetApplySectionSpriteVisibility(&_gDryfieldNightMainStreetBalconySection4SpritePatchTable, GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_4_STATE);
    _dryfieldNightMainStreetApplySectionSpriteVisibility(&_gDryfieldNightMainStreetBalconySection5SpritePatchTable, GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_5_STATE);
    _dryfieldNightMainStreetApplySectionSpriteVisibility(&_gDryfieldNightMainStreetBalconySection7SpritePatchTable, GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_7_STATE);
}

/// Chooses the next puff's spawn point with three ordered LCG draws.
///
/// Mutates the room's dedicated spawn-point slot in `GsWSMATRIX`'s input space.
/// The fourth halfword is retained;
/// X is -1185..-886, Y is -1255..-656 and Z is 9836..10535 coordinate units.
static inline void _dryfieldNightMainStreetChoosePuffSpawnPoint(void)
{
    _gDryfieldNightMainStreetAmbientEffectAnchors.puffSpawnPoint.vx = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 300 - 0x4A1;
    _gDryfieldNightMainStreetAmbientEffectAnchors.puffSpawnPoint.vy = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 600 - 0x4E7;
    _gDryfieldNightMainStreetAmbientEffectAnchors.puffSpawnPoint.vz = 0x2927 - DRYFIELD_NIGHT_MAIN_STREET_RAND() % 700;
}

void dryfieldNightMainStreetAmbientEffectsTask(Task* task)
{
    enum {
        NEW                         = 0,
        RUNNING                     = 1,
        SHAFT_RADIUS_SCALE          = 0x180,
        FLARE_RADIUS_SCALE          = 0x380,
        FLARE_TEXTURE               = 1,
        FIRST_FLARE                 = 10,
        FLARE_END                   = ARRAY_SIZE(_gDryfieldNightMainStreetGlowViewMasks),
        PUFF_VIEW_FIRST             = 8,
        PUFF_VIEW_SECOND            = 19,
        PUFF_ENTRY_COUNT            = 48,
        PUFF_RANDOM_SIZE_AND_PERIOD = 0x10FF,
        PUFF_ENTRY_ARGS             = 0x103100, // Size base 256, period base 3, speed 16
        PUFF_CONTINUING_ARGS        = 0x82100,  // Size base 256, period base 2, speed 8
    };
    s32 viewMask;
    s32 effectIndex;

    viewMask = 1 << (viewGetMappedIndex() & 0xFF);
    if (task->state == NEW) {
        gRoomEffectMoteId         = EFFECT_DRYFIELD_NIGHT_MAIN_STREET_MOTE;
        gRoomEffectHaloId         = EFFECT_DRYFIELD_NIGHT_MAIN_STREET_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_DRYFIELD_NIGHT_MAIN_STREET_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_DRYFIELD_NIGHT_MAIN_STREET_SPARK_EMITTER;
        task->state               = RUNNING;
    }
    // A broken balcony lamp no longer contributes its distant shaft.
    if (gameFlagGetNibble(GAME_FLAG_07F) != 0) {
        _gDryfieldNightMainStreetGlowViewMasks[3] = 0;
        _gDryfieldNightMainStreetGlowViewMasks[2] = 0;
    }
    if (viewMask & _gDryfieldNightMainStreetGlowViewMasks[0]) {
        _glowDrawShaft(_gDryfieldNightMainStreetAmbientEffectAnchors.shaftEndpoints[0], SHAFT_RADIUS_SCALE);
    }
    if (viewMask & _gDryfieldNightMainStreetGlowViewMasks[2]) {
        _glowDrawShaft(_gDryfieldNightMainStreetAmbientEffectAnchors.shaftEndpoints[1], SHAFT_RADIUS_SCALE);
    }
    if (viewMask & _gDryfieldNightMainStreetGlowViewMasks[4]) {
        _glowDrawShaft(_gDryfieldNightMainStreetAmbientEffectAnchors.shaftEndpoints[2], SHAFT_RADIUS_SCALE);
    }
    if (viewMask & _gDryfieldNightMainStreetGlowViewMasks[6]) {
        _glowDrawShaft(_gDryfieldNightMainStreetAmbientEffectAnchors.shaftEndpoints[3], SHAFT_RADIUS_SCALE);
    }
    if (viewMask & _gDryfieldNightMainStreetGlowViewMasks[8]) {
        _glowDrawShaft(_gDryfieldNightMainStreetAmbientEffectAnchors.shaftEndpoints[4], SHAFT_RADIUS_SCALE);
    }
    for (effectIndex = FIRST_FLARE; effectIndex < FLARE_END; effectIndex++) {
        if (viewMask & _gDryfieldNightMainStreetGlowViewMasks[effectIndex]) {
            _glowDrawFlare(&_gDryfieldNightMainStreetAmbientEffectAnchors.flareCenters[effectIndex - FIRST_FLARE], FLARE_TEXTURE, FLARE_RADIUS_SCALE);
        }
    }
    gRoomEffectState->roomEffectMode = _gDryfieldNightMainStreetAmbientEffectGateByView[(viewGetMappedIndex() & 0xFF) - 1];
    // Seed the view with puffs on entry, then replenish on alternate animation frames.
    if ((viewGetMappedIndex() & 0xFF) == PUFF_VIEW_FIRST || (viewGetMappedIndex() & 0xFF) == PUFF_VIEW_SECOND) {
        if (task->spawnArg1.value != (viewGetMappedIndex() & 0xFF)) {
            for (effectIndex = 0; effectIndex < PUFF_ENTRY_COUNT; effectIndex++) {
                _dryfieldNightMainStreetChoosePuffSpawnPoint();
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MAIN_STREET_PUFF, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & PUFF_RANDOM_SIZE_AND_PERIOD) + PUFF_ENTRY_ARGS,
                            &_gDryfieldNightMainStreetAmbientEffectAnchors.puffSpawnPoint);
            }
        } else if (gDisplayState.animFrame & 1) {
            _dryfieldNightMainStreetChoosePuffSpawnPoint();
            effectSpawn(EFFECT_DRYFIELD_NIGHT_MAIN_STREET_PUFF, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & PUFF_RANDOM_SIZE_AND_PERIOD) | PUFF_CONTINUING_ARGS,
                        &_gDryfieldNightMainStreetAmbientEffectAnchors.puffSpawnPoint);
        }
    }
    task->spawnArg1.value = viewGetMappedIndex() & 0xFF;
}

#undef DRYFIELD_NIGHT_MAIN_STREET_RAND

#include "../../shared/effects/glow_draw_shaft.inc.c"

#include "../../shared/effects/glow_draw_flare.inc.c"

#include "../../shared/main_street_puff_task.inc.c"

#include "../../shared/main_street_draw_puff.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void dryfieldNightMainStreetRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void dryfieldNightMainStreetRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void dryfieldNightMainStreetRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void dryfieldNightMainStreetRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}
