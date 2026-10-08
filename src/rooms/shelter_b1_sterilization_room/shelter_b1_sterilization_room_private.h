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

/// One bit per entry of `D_shelter_b1_sterilization_room_80188504` already
/// spawned, so each is spawned only once until the mask is cleared.
extern s32 D_shelter_b1_sterilization_room_8018C340;

extern AnimationBankCopyRequest D_shelter_b1_sterilization_room_80188590;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885AC;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885C0;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885D4;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885E8;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_801885FC;

extern AnimationPlayRequest D_shelter_b1_sterilization_room_80188610;

extern ActorTransform D_shelter_b1_sterilization_room_80188638;

extern ActorTransform D_shelter_b1_sterilization_room_80188650;

extern DamageAttack D_shelter_b1_sterilization_room_80188738;

extern EvsCommand D_shelter_b1_sterilization_room_8018873C[37];

extern EvsCommand D_shelter_b1_sterilization_room_80188AB4[20];

extern EvsCommand D_shelter_b1_sterilization_room_80188ED4[11];

extern EvsCommand D_shelter_b1_sterilization_room_80188FDC[8];

extern WorldCollisionGrid D_shelter_b1_sterilization_room_80189E44;

extern WorldCollisionTrigger D_shelter_b1_sterilization_room_8018B8A8[28];

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
