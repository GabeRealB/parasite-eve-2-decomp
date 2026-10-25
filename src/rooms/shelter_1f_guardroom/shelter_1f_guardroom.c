#include "rooms/shelter_1f_guardroom.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

static void _shelter1fGuardroomInitializeRoom(Task* task);
static void _shelter1fGuardroomRoomIdleState(Task* unusedTask);
static void _shelter1fGuardroomSetUnlockOverlayVisible(u8 visible);
static s32  _shelter1fGuardroomRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32  _shelter1fGuardroomIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Room-local selectors and rendering constants used by the resource tables.
enum {
    SHELTER_1F_GUARDROOM_ARRIVAL_VIEW           = 2,
    SHELTER_1F_GUARDROOM_BULWARK_VIEW           = 3,
    SHELTER_1F_GUARDROOM_ARRIVAL_YAW            = 2048, // Half a turn, in 4096-unit yaw angles
    SHELTER_1F_GUARDROOM_SPRITE_NEUTRAL_COLOR   = 128,
    SHELTER_1F_GUARDROOM_SPRITE_8BIT_TEXTURE    = 1,
    SHELTER_1F_GUARDROOM_AIRLOCK_WARP_SLOTS     = (1 << 4) | 2, // Departure slot 1, arrival slot 2
    SHELTER_1F_GUARDROOM_INSPECT_CAP_COMMAND    = 1,
    SHELTER_1F_GUARDROOM_UNLOCK_TRIGGER_COMMAND = 2,
    SHELTER_1F_GUARDROOM_FLOOR_WALK_SOUND       = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
    SHELTER_1F_GUARDROOM_FLOOR_RUN_SOUND        = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5B,
};

/// Room-controller handlers indexed by task state: 0 initialize, 1 idle, 2 kill.
///
/// Copied by value before dispatch; the selected callback receives the live
/// room task. No terminator or bounds check is supplied.
static const TaskFuncTable3 _gShelter1fGuardroomRoomStateHandlers = {
    {
        _shelter1fGuardroomInitializeRoom,
        _shelter1fGuardroomRoomIdleState,
        taskKill,
    },
};

static WorldCollisionGrid         _gShelter1fGuardroomCollisionGrid;
static WorldCollisionTrigger      _gShelter1fGuardroomViewBoundaryTriggers[2];
static WorldCollisionTrigger      _gShelter1fGuardroomActionTriggers[3];
static WorldCoordRoomAmbientEntry _gShelter1fGuardroomViewAmbientTable[4];
static WorldCoordRoomLights       _gShelter1fGuardroomRoomLights;
static s32                        _shelter1fGuardroomResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32                        _shelter1fGuardroomCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32                        _shelter1fGuardroomPlaySoundCueMessage(Task* task, s32 messageId, s32 cueId, s32 secondArg);
static void                       _shelter1fGuardroomUnlockBulwarkTask(Task* task);
static void                       _shelter1fGuardroomUnlockMovieTask(Task* task);

/// Room messages for transition queries, key items, actions, commands and sound cues.
///
/// Installed on the room task during initialization. The final end marker
/// makes unhandled IDs return zero; table and callbacks must remain loaded
/// while that task receives synchronous messages.
static TaskMessageEntry _gShelter1fGuardroomRoomMessageTable[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fGuardroomResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelter1fGuardroomRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fGuardroomIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fGuardroomCommandMessage },
    { ROOM_MESSAGE_SOUND, _shelter1fGuardroomPlaySoundCueMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless task recipe for the bulwark unlock dialogue and movie handoff.
///
/// Spawned at index 0 with zero payloads after player control is held. Priority
/// 32 runs before the priority-192 movie; body metadata is unused.
static TaskDesc _gShelter1fGuardroomUnlockBulwarkTaskDescriptor = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelter1fGuardroomUnlockBulwarkTask, { .value = 0 } };

/// Bodyless task recipe for the confirmed bulwark-unlock movie.
///
/// Spawned at index 0 with zero payloads. The unlock task retains the result
/// until it polls the movie's requested teardown; body metadata is unused.
static TaskDesc _gShelter1fGuardroomUnlockMovieTaskDescriptor = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelter1fGuardroomUnlockMovieTask, { .value = 0 } };

WorldCollisionRoomResources gShelter1fGuardroomRoomCollisionResources[1] = {
    { &_gShelter1fGuardroomCollisionGrid, _gShelter1fGuardroomViewBoundaryTriggers, _gShelter1fGuardroomActionTriggers, NULL },
};

WorldCoordRoomLighting gShelter1fGuardroomRoomLighting[1] = {
    { &_gShelter1fGuardroomRoomLights, _gShelter1fGuardroomViewAmbientTable },
};

u8* gShelter1fGuardroomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelter1fGuardroomRoomViewCounts[1] = { ARRAY_SIZE(gShelter1fGuardroomViewCameras) };

DirectionWarpEntry gShelter1fGuardroomWarpEntries[1] = {
    { { { .word = SHELTER_1F_GUARDROOM_ARRIVAL_YAW }, -9000, 0, -3620 }, { 0, 0, 0, 0 }, { { .word = SHELTER_1F_GUARDROOM_ARRIVAL_YAW }, -9000, 0, -3620 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_GUARDROOM, 2), SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_GUARDROOM, 1), DIRECTION_WARP_SOUND_NONE, SHELTER_1F_GUARDROOM_ARRIVAL_VIEW, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Collision-grid normals in room space, with `ONE` per unit length.
static SVECTOR _gShelter1fGuardroomCollisionGridNormals[] = {
#include "assets/shelter_1f_guardroom_collision_00630_normals.inc"
};

/// Collision-grid vertices in signed room-space game coordinates.
static SVECTOR _gShelter1fGuardroomCollisionGridVerts[] = {
#include "assets/shelter_1f_guardroom_collision_00630_verts.inc"
};

/// Eight indexed grid quads: class 1 floor, class 0 walls and ceiling.
static WorldCollisionGridFace _gShelter1fGuardroomCollisionGridFaces[] = {
#include "assets/shelter_1f_guardroom_collision_00630_faces.inc"
};

/// Serialized cell-list words; the live list names faces 0..7 and ends at -1.
///
/// The asset retains one further zero word beyond the terminated list.
static s16 _gShelter1fGuardroomCollisionGridCells[] = {
#include "assets/shelter_1f_guardroom_collision_00630_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelter1fGuardroomCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..9,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_1F_GUARDROOM_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelter1fGuardroomCollisionGridCells[(halfwordOffset)])
/// The grid's single XZ cell, referencing its terminated face-index list.
static s16* _gShelter1fGuardroomCollisionGridTable[] = {
#include "assets/shelter_1f_guardroom_collision_00630_table.inc"
};
#undef SHELTER_1F_GUARDROOM_CELL_FACE_LIST_AT_OFFSET

/// The guardroom collision mesh and its one-cell XZ face index.
///
/// Positions use room-space game units. Cell coordinates add X/Z biases
/// 10500/4500 before division by the 4000-unit cell side. Room setup binds
/// the initially NULL view coordinate; the grid and pools stay writable.
static WorldCollisionGrid _gShelter1fGuardroomCollisionGrid = {
    NULL,
    _gShelter1fGuardroomCollisionGridNormals,
    _gShelter1fGuardroomCollisionGridVerts,
    _gShelter1fGuardroomCollisionGridFaces,
    _gShelter1fGuardroomCollisionGridTable,
    10500,
    4500,
    1,
    1,
    4000,
    ARRAY_SIZE(_gShelter1fGuardroomCollisionGridFaces),
};

ViewCamera gShelter1fGuardroomViewCameras[3] = {
    { { { { 4095, 0, 0 }, { 0, 0, -ONE }, { 0, 4095, 0 } }, { 0, 21300, 0 } }, 207 },
    { { { { 597, 0, 4052 }, { 2175, 3455, -320 }, { -3418, 2198, 504 } }, { 6020, 2440, 4040 } }, 207 },
    { { { { 756, 0, -4025 }, { -1977, 3567, -371 }, { 3506, 2012, 658 } }, { 9590, 2480, 4320 } }, 257 },
};

/// View 1's empty sprite range and end marker, retaining decoded background strips.
static SpriteBatch _gShelter1fGuardroomView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite range and end marker, retaining decoded background strips.
static SpriteBatch _gShelter1fGuardroomView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Two adjacent texture rectangles showing the unlocked bulwark in view 3.
///
/// Positions and dimensions are pixels relative to the view centre. Both use
/// 8-bit texture pages, palette row 255, neutral colour and opaque emission;
/// depth 2500 is the unscaled sprite sorting value.
static SpriteSource _gShelter1fGuardroomUnlockOverlaySpriteSources[] = {
    { getTPage(SHELTER_1F_GUARDROOM_SPRITE_8BIT_TEXTURE, 0, 896, 0), getClut(0, 255), { .fields = { 72, 160 } }, -72, -120, 2500, { .fields = { 104, 0 } }, SHELTER_1F_GUARDROOM_SPRITE_NEUTRAL_COLOR, SHELTER_1F_GUARDROOM_SPRITE_NEUTRAL_COLOR, SHELTER_1F_GUARDROOM_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(SHELTER_1F_GUARDROOM_SPRITE_8BIT_TEXTURE, 0, 960, 0), getClut(0, 255), { .fields = { 80, 160 } }, 0, -120, 2500, { .fields = { 48, 0 } }, SHELTER_1F_GUARDROOM_SPRITE_NEUTRAL_COLOR, SHELTER_1F_GUARDROOM_SPRITE_NEUTRAL_COLOR, SHELTER_1F_GUARDROOM_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 3's background selector, unlocked-bulwark sprite range and end marker.
///
/// The zero-count first batch retains background strips. Batch 1 spans both
/// source sprites; its hidden byte follows unlock progress and movie playback.
static SpriteBatch _gShelter1fGuardroomView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelter1fGuardroomUnlockOverlaySpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelter1fGuardroomSpriteViews[3] = {
    { { .empty = _gShelter1fGuardroomView1SpriteBatches }, _gShelter1fGuardroomView1SpriteBatches, NULL },
    { { .empty = _gShelter1fGuardroomView2SpriteBatches }, _gShelter1fGuardroomView2SpriteBatches, NULL },
    { { .elements = _gShelter1fGuardroomUnlockOverlaySpriteSources }, _gShelter1fGuardroomView3SpriteBatches, NULL },
};

/// Two warm point lights and one red point light contributing in every view.
///
/// Transforms and RGB intensities use `ONE` per unit; translations and falloff
/// radii use game coordinates. Runtime lighting binds parents, composes the
/// transforms and overwrites attenuation, so the records remain writable.
static WorldCoordPointLight _gShelter1fGuardroomPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9420, -2000, -3790 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2867, 2457 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7690, -2000, -3790 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2867, 2457 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9000, -2110, -3400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 0, 0 }, { 0, 0 } }, 1000, 1500 },
};

/// The guardroom's three point lights, with no directional or cone lights.
static WorldCoordRoomLights _gShelter1fGuardroomRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gShelter1fGuardroomPointLights),
    _gShelter1fGuardroomPointLights,
    0,
    NULL,
};

/// Directed camera boundaries from view 3 to 2 and from view 2 to 3.
///
/// The final included quad carries LAST. Room setup binds and links both
/// mutable records; positions/radii use game units and normals use `ONE`.
static WorldCollisionTrigger _gShelter1fGuardroomViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { -7645, -1408, -3616, 0 }, { { 75, -1872, -1898, 0 }, { -87, -1872, 1885, 0 }, { 75, 1872, -1898, 0 }, { -87, 1872, 1885, 0 } }, { 4098, 0, 175, 0 }, { 0, 0, ONE, 0 }, 2660, 0, SHELTER_1F_GUARDROOM_BULWARK_VIEW, SHELTER_1F_GUARDROOM_ARRIVAL_VIEW, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7472, -1440, -3600, 0 }, { { -78, -1840, 1851, 0 }, { 62, -1840, -1863, 0 }, { -78, 1840, 1851, 0 }, { 62, 1840, -1863, 0 } }, { -4095, 0, -155, 0 }, { 0, 0, ONE, 0 }, 2610, 0, SHELTER_1F_GUARDROOM_ARRIVAL_VIEW, SHELTER_1F_GUARDROOM_BULWARK_VIEW, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Facing-gated interactions for the airlock exit, CAP dialogue and bulwark control.
///
/// The warp leaves through descriptor 1 and arrives at airlock descriptor 2.
/// The next quad runs CAP command 1; the last sends room command 2 for the
/// unlock interaction. LAST includes that final record in setup. Room setup
/// binds and links the writable quads; positions/radii use game units.
static WorldCollisionTrigger _gShelter1fGuardroomActionTriggers[3] = {
    { NULL, NULL, NULL, { -9360, -48, -3520, 0 }, { { -656, 0, -224, 0 }, { 656, 0, -224, 0 }, { -656, 0, 224, 0 }, { 656, 0, 224, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -ONE, 0 }, 692, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, SHELTER_1F_GUARDROOM_AIRLOCK_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7488, -64, -3136, 0 }, { { -656, 0, -224, 0 }, { 656, 0, -224, 0 }, { -656, 0, 224, 0 }, { 656, 0, 224, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -ONE, 0 }, 692, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_GUARDROOM_INSPECT_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6960, -64, -3728, 0 }, { { -288, 0, -816, 0 }, { 288, 0, -816, 0 }, { -288, 0, 816, 0 }, { 288, 0, 816, 0 } }, { 0, 4104, 0, 0 }, { -ONE, 0, 0, 0 }, 863, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_GUARDROOM_UNLOCK_TRIGGER_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Ambient minima for views 1..3, preceded by the count in entry zero.
///
/// RGB uses 16 units per 8-bit colour level: view 1 has minima 16 and views
/// 2/3 have minima 300. The fourth component retains their authored brightness.
static WorldCoordRoomAmbientEntry _gShelter1fGuardroomViewAmbientTable[4] = {
    { .viewCount = ARRAY_SIZE(_gShelter1fGuardroomViewAmbientTable) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 300, 300, 300, 300 } },
};

/// Type-1 footstep script bases for the guardroom floor.
///
/// Walk and scripted jump use entry 0x59; run uses 0x5B. Cue 1 adds one to
/// select the paired foot, while cue 2 uses the base. Requests resolve against
/// the loaded type-1 bank; this record owns no sound resource.
static WorldCollisionFootstepSounds _gShelter1fGuardroomFloorFootstepSounds = {
    SHELTER_1F_GUARDROOM_FLOOR_WALK_SOUND,
    SHELTER_1F_GUARDROOM_FLOOR_RUN_SOUND,
    SHELTER_1F_GUARDROOM_FLOOR_WALK_SOUND,
};

/// Solid, impact-enabled collision policy without footstep sounds.
static WorldCollisionSurfaceProperties _gShelter1fGuardroomSilentSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Solid, impact-enabled floor policy with walking, running and scripted-jump cues.
static WorldCollisionSurfaceProperties _gShelter1fGuardroomFloorSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gShelter1fGuardroomFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelter1fGuardroomSurfaceProperties[8] = {
    &_gShelter1fGuardroomSilentSurfaceProperties,
    &_gShelter1fGuardroomFloorSurfaceProperties,
    &_gShelter1fGuardroomSilentSurfaceProperties,
    &_gShelter1fGuardroomSilentSurfaceProperties,
    &_gShelter1fGuardroomSilentSurfaceProperties,
    &_gShelter1fGuardroomSilentSurfaceProperties,
    &_gShelter1fGuardroomSilentSurfaceProperties,
    &_gShelter1fGuardroomSilentSurfaceProperties,
};

/// Movie task retained by the unlock sequence until its teardown is polled.
///
/// Valid only between a successful movie spawn and the poll that runs its exit.
/// Teardown releases the task without clearing this slot; never dereference
/// the retained pointer after that poll or after the guardroom is unloaded.
static Task* _gShelter1fGuardroomUnlockMovieTask = NULL;

/// Records the Bulwark unlock and returns the guardroom to player control.
///
/// Call after the confirmed unlock movie task has exited. Shows the HUD and
/// unlocked overlay before setting the persistent flag and releasing scripted
/// player control. Requires live room presentation, player and session resources.
/// Kills the bodyless unlock task last; its pointer is invalid afterwards.
static inline void _shelter1fGuardroomFinishBulwarkUnlock(Task* task)
{
    enum { SHELTER_1F_GUARDROOM_BULWARK_UNLOCKED = 1 };

    gGameSession->hideHud = false;
    _shelter1fGuardroomSetUnlockOverlayVisible(true);
    gameFlagSetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED, SHELTER_1F_GUARDROOM_BULWARK_UNLOCKED);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    taskKill(task);
}

/// Runs the dialogue and confirmed movie sequence that unlocks the bulwark.
///
/// Starts at state 0 with scripted player control already held. CAP command 2
/// must finish before its variant key is tested; key 11 starts the movie while
/// other keys release control and exit. Movie teardown precedes restoring the
/// HUD, showing the unlocked overlay and setting the unlock flag. Requires the
/// guardroom resources, CAP slot 2 and session to remain live through completion.
static void _shelter1fGuardroomUnlockBulwarkTask(Task* task)
{
    enum {
        SHELTER_1F_GUARDROOM_UNLOCK_START_DIALOGUE = 0,
        SHELTER_1F_GUARDROOM_UNLOCK_WAIT_DIALOGUE  = 1,
        SHELTER_1F_GUARDROOM_UNLOCK_CHECK_CHOICE   = 2,
        SHELTER_1F_GUARDROOM_UNLOCK_WAIT_MOVIE     = 3,
        SHELTER_1F_GUARDROOM_UNLOCK_FINISH         = 4,
        SHELTER_1F_GUARDROOM_UNLOCK_CAP_COMMAND    = 2,
        SHELTER_1F_GUARDROOM_UNLOCK_CONFIRMED_KEY  = 0xB
    };
    s32 movieKillStatus;

    switch (task->state) {
        case SHELTER_1F_GUARDROOM_UNLOCK_START_DIALOGUE:
            capRunCommandWithTransition(SHELTER_1F_GUARDROOM_UNLOCK_CAP_COMMAND);
            task->state++;
            break;
        case SHELTER_1F_GUARDROOM_UNLOCK_WAIT_DIALOGUE:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_1F_GUARDROOM_UNLOCK_CHECK_CHOICE:
            gCapCommandEntries[SHELTER_1F_GUARDROOM_UNLOCK_CAP_COMMAND].command->counter = 1;
            if (capGetVariantKey() != SHELTER_1F_GUARDROOM_UNLOCK_CONFIRMED_KEY) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                break;
            }
            // Keep the HUD hidden until the independent movie task has exited.
            gGameSession->hideHud               = 1;
            _gShelter1fGuardroomUnlockMovieTask = taskSpawnFromTable(&_gShelter1fGuardroomUnlockMovieTaskDescriptor, 0, 0, 0);
            task->state++;
            break;
        case SHELTER_1F_GUARDROOM_UNLOCK_WAIT_MOVIE:
            if (taskPollKill(_gShelter1fGuardroomUnlockMovieTask, &movieKillStatus) == 0) {
                break;
            }
            task->state++;
            break;
        case SHELTER_1F_GUARDROOM_UNLOCK_FINISH:
            _shelter1fGuardroomFinishBulwarkUnlock(task);
            break;
    }
}

/// Refuses every key-item use request in the guardroom without consuming the item.
///
/// `itemId` is the inventory item ID; all arguments are ignored. The zero
/// result makes the item menu report that the item cannot be used here.
static s32 _shelter1fGuardroomRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition after resolving its Neo Ark destination variant.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Borrows a complete eight-byte request
/// and writable reply during synchronous dispatch; they may alias. Copies the
/// request before resolving the reply room. Query mode preserves the selectors.
/// Neither pointer is retained; the map overlay must remain loaded. Always
/// returns 1 (passage allowed); the receiving task and message ID are unused.
static s32 _shelter1fGuardroomResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_1F_GUARDROOM_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return SHELTER_1F_GUARDROOM_TRANSITION_ALLOWED;
}

/// Handles the bulwark unlock command from the guardroom's room-message table.
///
/// `ROOM_MESSAGE_COMMAND` payload 2 holds player control and starts the unlock
/// sequence while the flag is clear, or runs CAP command 3 once unlocked. Other
/// payloads do nothing. Always returns zero; receiver, message ID and second
/// payload are unused. The room and CAP resources must remain loaded.
static s32 _shelter1fGuardroomCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        SHELTER_1F_GUARDROOM_COMMAND_UNLOCK_BULWARK = 2,
        SHELTER_1F_GUARDROOM_CAP_ALREADY_UNLOCKED   = 3,
        SHELTER_1F_GUARDROOM_COMMAND_HANDLED        = 0
    };

    if (commandId == SHELTER_1F_GUARDROOM_COMMAND_UNLOCK_BULWARK) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED) == 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&_gShelter1fGuardroomUnlockBulwarkTaskDescriptor, 0, 0, 0);
        } else {
            capRunCommandWithTransition(SHELTER_1F_GUARDROOM_CAP_ALREADY_UNLOCKED);
        }
    }
    return SHELTER_1F_GUARDROOM_COMMAND_HANDLED;
}

/// Ignores trigger action requests in the guardroom and returns zero.
///
/// `request` borrows the direction system's action record during synchronous
/// dispatch. No argument is read, no action starts, and no storage is retained.
static s32 _shelter1fGuardroomIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Queues the guardroom sound script selected by CAP cue 3.
///
/// Handles `ROOM_MESSAGE_SOUND`: cue 3 starts local area-bank script 3; other
/// cues do nothing. Requires the current stage sound bank and sound event queue.
/// Always returns zero; the receiving task, message ID and second payload are unused.
static s32 _shelter1fGuardroomPlaySoundCueMessage(Task* task, s32 messageId, s32 cueId, s32 secondArg)
{
    enum { SHELTER_1F_GUARDROOM_SOUND_CUE_SCRIPT_3 = 3 };

    if (cueId == SHELTER_1F_GUARDROOM_SOUND_CUE_SCRIPT_3) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_GUARDROOM, 3), 0, 0);
    }
    return 0;
}

/// Registers the room receiver and restores the unlocked-bulwark overlay.
///
/// State 0 installs the message table and publishes the borrowed task in the
/// room slot. The unlock flag selects overlay visibility before state 1 begins.
/// Requires the guardroom's loaded sprite tables and current session.
static void _shelter1fGuardroomInitializeRoom(Task* task)
{
    task->msgTable = _gShelter1fGuardroomRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _shelter1fGuardroomSetUnlockOverlayVisible(gameFlagGetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED));
    task->state++;
}

/// Keeps the initialized room event task available for messages in state 1.
///
/// The frame callback leaves the task, its message table and its state intact;
/// teardown remains the responsibility of the room's task owner.
static void _shelter1fGuardroomRoomIdleState(Task* unusedTask)
{
}

void shelter1fGuardroomRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelter1fGuardroomRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Plays the current room's bulwark-unlock movie with its overlay hidden.
///
/// State 0 selects movie sub-ID 0 and one-based frame 1. State 1 waits for
/// `movieReady` (started or skipped); state 2 waits for the CD queue to become
/// idle before requesting teardown. The surrounding unlock task restores the
/// overlay and HUD after this task exits. Requires a loaded guardroom movie.
/// The queue copies the four-byte stack arguments synchronously; the three
/// opcode-unused bytes remain unwritten, and no stack pointer is retained.
static void _shelter1fGuardroomUnlockMovieTask(Task* task)
{
    enum {
        SHELTER_1F_GUARDROOM_MOVIE_START       = 0,
        SHELTER_1F_GUARDROOM_MOVIE_WAIT_READY  = 1,
        SHELTER_1F_GUARDROOM_MOVIE_WAIT_IDLE   = 2,
        SHELTER_1F_GUARDROOM_MOVIE_FIRST_FRAME = 1,
        SHELTER_1F_GUARDROOM_MOVIE_SUB_ID      = 0
    };
    u8          streamArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case SHELTER_1F_GUARDROOM_MOVIE_START:
            _shelter1fGuardroomSetUnlockOverlayVisible(0);
            queue->movieFrame = SHELTER_1F_GUARDROOM_MOVIE_FIRST_FRAME;
            streamArgs[0]     = streamFindMovieSlot(&gGameSession->location, SHELTER_1F_GUARDROOM_MOVIE_SUB_ID, 0);
            // The queue copies four bytes; this opcode uses only the slot byte.
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state++;
            break;
        case SHELTER_1F_GUARDROOM_MOVIE_WAIT_READY:
            if (queue->movieReady != 0) {
                task->state = SHELTER_1F_GUARDROOM_MOVIE_WAIT_IDLE;
            }
            break;
        case SHELTER_1F_GUARDROOM_MOVIE_WAIT_IDLE:
            if (cdCmdIsIdle()) {
                taskRequestKill(task, 0);
            }
            break;
    }
}

/// Shows or hides the two-sprite overlay for the unlocked bulwark in guardroom view 3.
///
/// `visible` is a byte (0 hidden, nonzero visible). Requires the current session
/// to select the guardroom and its map and room resources to remain loaded.
/// The overlay is hidden during the unlock movie and restored when it completes.
static void _shelter1fGuardroomSetUnlockOverlayVisible(u8 visible)
{
    enum {
        SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_VIEW_INDEX  = 2,
        SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_BATCH_INDEX = 1,
    };
    const GameLocationKey* location = &gGameSession->location;
    SpriteBatch*           batches;

    batches = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1][SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_VIEW_INDEX].batches;
    if (visible == 0) {
        batches[SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_BATCH_INDEX].hidden = 1;
    } else {
        batches[SHELTER_1F_GUARDROOM_UNLOCK_OVERLAY_BATCH_INDEX].hidden = 0;
    }
}

void shelter1fGuardroomEffectNoopTask(Task* unusedTask)
{
}
