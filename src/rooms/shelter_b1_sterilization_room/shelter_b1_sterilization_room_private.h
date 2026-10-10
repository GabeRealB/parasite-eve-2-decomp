#ifndef SRC_ROOMS_SHELTER_B1_STERILIZATION_ROOM_SHELTER_B1_STERILIZATION_ROOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_STERILIZATION_ROOM_SHELTER_B1_STERILIZATION_ROOM_PRIVATE_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room.h"

/// Number of indexed recipes in the room's event-task table; there is no END entry.
enum { SHELTER_B1_STERILIZATION_ROOM_EVENT_TASK_DESC_COUNT = 9 };

/// Bodyless priority-192 recipes for room transitions, trap tasks and CAP dialogue.
///
/// Indices 0..8 are the SHELTER_B1_STERILIZATION_ROOM_TASK_* values below.
/// Spawning copies the recipe; the callbacks and their resources must stay live.
/// Door passage takes the action-minus-three door index; dialogue tasks take
/// their route or CAP command in spawnArg1. Other recipes ignore both payloads.
extern TaskDesc gShelterB1SterilizationRoomEventTaskDescs[SHELTER_B1_STERILIZATION_ROOM_EVENT_TASK_DESC_COUNT];

/// Latches requests made through the room's two once-spawn helpers.
///
/// Descriptor indices select bits; scripts use 0/3 and the trap-damage helper
/// uses 5. Each helper sets its bit before requesting allocation, even if that
/// fails. Direct task spawns do not consult the mask. Variant-1 entry clears it
/// without stopping live tasks. The signed 32-bit shifts are retained.
extern s32 gShelterB1SterilizationRoomOnceSpawnMask;

/// Installs the room's eight-word trap extension at player-bank indices 47..54.
///
/// Index 47 becomes NULL; 48..54 borrow the seven trap clips. Copies exactly
/// eight pointer words into the equipped weapon's writable bank. Keep the room's
/// sets and track data loaded until their playback ends; dispatch itself retains
/// neither this request nor the source table.
extern AnimationBankCopyRequest gShelterB1SterilizationRoomTrapAnimationBankCopyRequest;

/// Trap-entry opening clip 48, blending over 30 frames with world collision enabled.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
extern AnimationPlayRequest gShelterB1SterilizationRoomTrapEntryBeginAnimationRequest;

/// Trap-entry clip 49 in view 11, blending over five frames with world collision enabled.
///
/// Event playback resolves the equipped weapon bank in a copy of this request.
extern AnimationPlayRequest gShelterB1SterilizationRoomTrapEntryView11AnimationRequest;

/// Trap-entry clip 50 at the first CAP cue in view 16, with a five-frame blend.
///
/// Keeps world collision enabled; event playback resolves the equipped weapon
/// bank in a copy of this request. Requires the room's installed trap extension.
extern AnimationPlayRequest gShelterB1SterilizationRoomTrapEntryCapCueAnimationRequest;

/// Trap-entry clip 51 after placing the player in view 14, resetting playback.
///
/// Keeps world collision enabled; event playback resolves the equipped weapon
/// bank in a copy of this request. Requires the room's installed trap extension.
extern AnimationPlayRequest gShelterB1SterilizationRoomTrapEntryPlacedAnimationRequest;

/// Escape opening clip 52, resetting playback and disabling world collision.
///
/// Event playback resolves the equipped weapon bank in a copy of this request;
/// the room's trap extension and clip data must remain loaded during playback.
extern AnimationPlayRequest gShelterB1SterilizationRoomTrapEscapeBeginAnimationRequest;

/// Escape clip 53 in view 18, resetting playback and disabling world collision.
///
/// Event playback resolves the equipped weapon bank in a copy of this request;
/// the room's trap extension and clip data must remain loaded during playback.
extern AnimationPlayRequest gShelterB1SterilizationRoomTrapEscapeView18AnimationRequest;

/// Player placement shared by the normal and skipped trap-entry sequences.
///
/// Room-axis position (5540,0,8600), yaw 2047 in 4096 units per turn.
/// The placement message consumes the full transform synchronously.
extern ActorTransform gShelterB1SterilizationRoomTrapEntryPlayerPlacement;

/// Player placement shared by the normal and skipped escape-to-Dumping-Hole sequences.
///
/// Room-axis position (5876,0,10450), yaw 512 in 4096 units per turn.
/// The placement message consumes the full transform synchronously.
extern ActorTransform gShelterB1SterilizationRoomTrapEscapePlayerPlacement;

/// The repeating trap's category-4 attack: base power 25, ordinary hit reaction.
///
/// Power is scaled by the player's HP band when damage is resolved; it is not
/// a fixed 25-HP loss. Reaction zero adds no status. The damage task packs this
/// single record at index zero; it remains borrowed from the room overlay.
extern DamageAttack gShelterB1SterilizationRoomTrapAttack;

extern EvsCommand D_shelter_b1_sterilization_room_8018873C[37];

extern EvsCommand D_shelter_b1_sterilization_room_80188AB4[20];

/// First trap exchange, synchronized to scene CAP command 10 and two sound cues.
///
/// Eleven commands include END. Selects the scene CAP resource, preserves sounds
/// on skip, then ends player scripting and restores the default CAP resource.
/// The room starts it with player control held, HUD hide/restore and a separate
/// skip script. Requires live player, CAP, sound and room resources through exit.
extern EvsCommand gShelterB1SterilizationRoomTrapExchangeScript[11];

extern EvsCommand D_shelter_b1_sterilization_room_80188FDC[8];

/// Writable room collision mesh shared by all three room resource descriptors.
///
/// The 2-by-4 XZ index uses 4000-unit cells and origins X=0, Z=500. It contains
/// 68 faces, 14 Q12 normals and 124 game-unit vertices. The event actor rebuilds
/// the leading four faces/normals and eight vertices without changing cell lists.
/// Activation binds the view coordinate; all mesh storage lives with the overlay.
extern WorldCollisionGrid gShelterB1SterilizationRoomCollisionGrid;

/// Mutable room interactions and area-transition quads, including the LAST record.
///
/// Entries 0..9 dispatch room actions 1..10; entries 10/11 request the Main
/// Corridor and Pod Access Tunnel. The remaining entries select CAP or room
/// commands, with the final entry repeating action 2 at another facing. Entry
/// 22 is the event actor's weapon-assisted command 21, disabled on entry unless
/// the event progress is 1. Setup binds/enables the records; keep them loaded
/// and writable until unlinked. Positions/radii use game units, normals Q12.
extern WorldCollisionTrigger gShelterB1SterilizationRoomActionTriggers[28];

extern AreaApplyRec D_shelter_b1_sterilization_room_8018C334[2];

extern Task* gRoomCutsceneSoundTask;

extern RoomCutsceneRec D_shelter_b1_sterilization_room_8018C344;

/// Entries of the room's event-task table used by message and script callbacks.
enum {
    SHELTER_B1_STERILIZATION_ROOM_TASK_CROSSFADE          = 0,
    SHELTER_B1_STERILIZATION_ROOM_TASK_TRAP_ESCAPE_PROMPT = 1,
    SHELTER_B1_STERILIZATION_ROOM_TASK_DOOR_PASSAGE       = 2,
    SHELTER_B1_STERILIZATION_ROOM_TASK_SWITCH_ROOM        = 3,
    SHELTER_B1_STERILIZATION_ROOM_TASK_DOOR_NOTICE        = 4,
    SHELTER_B1_STERILIZATION_ROOM_TASK_TRAP_DAMAGE        = 5,
    SHELTER_B1_STERILIZATION_ROOM_TASK_RESET_SPAWN_MASK   = 6,
    SHELTER_B1_STERILIZATION_ROOM_TASK_TRAP_DIALOGUE      = 7,
    SHELTER_B1_STERILIZATION_ROOM_TASK_SCENE_DIALOGUE     = 8,
};

/// Spawn-argument choices of the trap dialogue's two progress-dependent routes.
enum {
    SHELTER_B1_STERILIZATION_ROOM_DIALOGUE_PRIMARY   = 0,
    SHELTER_B1_STERILIZATION_ROOM_DIALOGUE_ALTERNATE = 1,
};

/// Latches and requests an event-table entry once until the room's spawn mask is reset.
///
/// `descriptorIndex` must name an entry in 0..8; scripts use CROSSFADE and
/// SWITCH_ROOM. Both spawn payloads are zero. The bit is committed before
/// allocation, so a failed spawn also remains latched. Keep the overlay loaded.
void shelterB1SterilizationRoomSpawnTaskOnce(s32 descriptorIndex);

/// Offers the trap escape choice and starts the Dumping Hole scene when accepted.
///
/// Starts in state 0, holds player control and the session event gate, and
/// requests CAP 8. After the queued display transition, state 2 reads choice
/// key 1 as acceptance; it marks the trap stopped and clears its restart mode.
/// Decline clears the gate and resumes control. Other states clear the gate
/// and kill the task. Spawn arguments are unused; requires live CAP, player,
/// session and room resources until the script or decline path takes over.
void shelterB1SterilizationRoomTrapEscapePromptTask(Task* task);

/// Selects room 2 after requesting a view refresh on the preceding task tick.
///
/// Updates both the live save and session room selectors, requests room-object
/// relinking, then releases the task on its next tick. Spawn arguments are unused.
void shelterB1SterilizationRoomSwitchRoomTask(Task* task);

/// Holds player control for the notice shared by door actions 3, 6, 7 and 10.
///
/// State 0 requests CAP 9; the next callback resumes control and kills the
/// task. The spawner supplies a door index, but both payloads are unused:
/// this task never changes the view or places the player. Keep the overlay,
/// player and loaded CAP resources live through the queued display transition.
void shelterB1SterilizationRoomDoorNoticeTask(Task* task);

/// Rearms the room's once-per-entry event tasks and releases this reset task.
///
/// Clears the complete task-spawn latch mask; it does not stop tasks already
/// running. Room variant 1 spawns this one-shot callback on entry. All task
/// state and spawn arguments are unused; the borrowed live task is killed.
void shelterB1SterilizationRoomResetSpawnMaskTask(Task* task);

/// Runs one of two trap-dialogue routes, choosing its CAP command from trap progress.
///
/// State 0 resets CAP and selects loaded data-resource ordinal 1 at VRAM
/// (704,256). Zero `spawnArg1.value` selects commands 6/9 before/after escape;
/// nonzero selects commands 7/8 and records that route as started. The primary
/// post-escape route also records its dialogue and action-4 scene milestones.
/// State 1 waits for idle CAP, restores the default resource and kills the task.
/// `spawnArg2` is unused. Requires writable loaded CAP resources, their texture,
/// and the room overlay through completion; it borrows no extra task work.
void shelterB1SterilizationRoomTrapDialogueTask(Task* task);

/// Runs a selected scene CAP command and restores the default resource when it ends.
///
/// State 0 resets CAP, binds loaded data-resource ordinal 1 at VRAM (704,256)
/// and starts `spawnArg1.value`, an unchecked valid command-table index.
/// It tests for idle CAP in the same tick and on subsequent state-1 ticks,
/// then resets the resource and kills the task. `spawnArg2` is unused.
/// Keep the overlay and fully loaded, writable CAP resource and texture live.
void shelterB1SterilizationRoomSceneDialogueTask(Task* task);

#endif // SRC_ROOMS_SHELTER_B1_STERILIZATION_ROOM_SHELTER_B1_STERILIZATION_ROOM_PRIVATE_H
