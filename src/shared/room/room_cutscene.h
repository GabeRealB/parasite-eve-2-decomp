/* The room cutscene runner: a task a room's message handler spawns with a
 * RoomCutsceneRec as spawnArg2. It holds both characters' weapons, hides the
 * HUD, forces the scene's view and loads the CAP file, then starts the scene's
 * CAP slot with its sound task beside it; confirm or cancel skips the scene.
 * Afterwards it runs the follow-up CAP command, moves the story flags on, and
 * restores view, weapons and HUD. Each carrier defines its private spawn table and the
 * sound task's handle under the shared names below. Shared code reaches the
 * handle through `ROOM_CUTSCENE_SOUND_TASK`. A room whose symbol is the
 * `Task*` takes the default. A room whose symbol is
 * `RoomCutsceneSoundTaskStorage` binds the macro to `.task` before including
 * this header.
 *
 * Include this header in the prologue and room_cutscene_task.inc.c at the
 * task's position. The objects belong to the room, which defines them at its
 * own positions under these names:
 *
 *   static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT]
 *       ROOM_CUTSCENE_TASK_SCENE, ROOM_CUTSCENE_TASK_SOUND, then TASK_DESC_END.
 *       Scene spawns pass the follow-up CAP command as spawnArg1.value and a
 *       borrowed RoomCutsceneRec as spawnArg2.pointer. Sound spawns pass zero
 *       as spawnArg1 and a packed sound-script request as spawnArg2.value.
 *       The scene record and callback code must remain loaded until completion.
 *   Task*    gRoomCutsceneSoundTask     the running sound task, polled for its
 *                                       end and killed on a skip. A room whose
 *                                       symbol is eight bytes declares
 *                                       `RoomCutsceneSoundTaskStorage` instead
 *
 *   ROOM_CUTSCENE_SOUND_TASK  lvalue `Task*`. Default `gRoomCutsceneSoundTask`,
 *                             the pointer itself. `RoomCutsceneSoundTaskStorage`
 *                             rooms bind `.task`.
 */

#ifndef SRC_SHARED_ROOM_CUTSCENE_H
#define SRC_SHARED_ROOM_CUTSCENE_H

#include "types.h"

#include "main/task_types.h"

// The cutscene task applies the square's post-cutscene area updates when it runs in the square.
#include "stages/akropolis/rooms/square.h"

/// Indices of each carrier's bodyless cutscene tasks.
///
/// Only SCENE and SOUND can be spawned; the third descriptor terminates the list.
enum {
    ROOM_CUTSCENE_TASK_SCENE            = 0,
    ROOM_CUTSCENE_TASK_SOUND            = 1,
    ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT = 3,
};

#ifndef ROOM_CUTSCENE_SOUND_TASK
/// Running sound task the cutscene polls, and kills when the scene is skipped.
/// The room's `gRoomCutsceneSoundTask` when that symbol is the `Task*`.
#define ROOM_CUTSCENE_SOUND_TASK gRoomCutsceneSoundTask
#endif

#endif /* SRC_SHARED_ROOM_CUTSCENE_H */
