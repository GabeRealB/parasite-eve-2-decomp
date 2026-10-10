#ifndef SRC_ROOMS_SHELTER_B1_STERILIZATION_ROOM_SHELTER_B1_STERILIZATION_ROOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_STERILIZATION_ROOM_SHELTER_B1_STERILIZATION_ROOM_PRIVATE_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room.h"

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_shelter_b1_sterilization_room_80188504[];

/// Latches requests made through the room's two once-spawn helpers.
///
/// Descriptor indices select bits; scripts use 0/3 and the trap-damage helper
/// uses 5. Each helper sets its bit before requesting allocation, even if that
/// fails. Direct task spawns do not consult the mask. Variant-1 entry clears it
/// without stopping live tasks. The signed 32-bit shifts are retained.
extern s32 gShelterB1SterilizationRoomOnceSpawnMask;

extern AnimationBankCopyRequest D_shelter_b1_sterilization_room_80188590;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885AC;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885C0;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885D4;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885E8;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885FC;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_80188610;

extern ActorTransform D_shelter_b1_sterilization_room_80188638;

extern ActorTransform D_shelter_b1_sterilization_room_80188650;

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
