/* The room cutscene runner: a task a room's message handler spawns with a
 * RoomCutsceneRec as spawnArg2. It holds both characters' weapons, hides the
 * HUD, forces the scene's view and loads the CAP file, then starts the scene's
 * CAP slot with its sound task beside it; confirm or cancel skips the scene.
 * Afterwards it runs the follow-up CAP command, moves the story flags on, and
 * restores view, weapons and HUD. The room defines the spawn table and the
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
 *   TaskDesc gRoomCutsceneTaskDescs[3]  the cutscene task, its sound task and
 *                                       the terminator
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

#ifndef ROOM_CUTSCENE_SOUND_TASK
/// Running sound task the cutscene polls, and kills when the scene is skipped.
/// The room's `gRoomCutsceneSoundTask` when that symbol is the `Task*`.
#define ROOM_CUTSCENE_SOUND_TASK gRoomCutsceneSoundTask
#endif

#endif /* SRC_SHARED_ROOM_CUTSCENE_H */
