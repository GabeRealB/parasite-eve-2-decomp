/* The jukebox menu: a UI panel titled SELECT that lists music tracks and plays
 * the chosen one. Each row is a `JukeboxTrack` (sequence id and drawn label)
 * in include/rooms/rooms_shared_8018055c.h. It offers one of ten track lists,
 * chosen by the save's game mode (list 4 before the first clear) plus 5 outside
 * the shooting gallery's alternate PE training mode.
 * Confirming a new row plays the select sound, fades out the current music and
 * hands the sequence id to the panel's menu task, which loads it from CD. A host
 * task opens the panel, holds the prim buffer and frame timing while it is up,
 * and ends the stage ten ticks after it closes.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_JUKEBOX_H
#define SRC_SHARED_JUKEBOX_H

#include "types.h"

#include "gameplay/attachments.h"

#include "main/task_types.h"

static void _jukeboxHostTask(Task* task);

static void _jukeboxDrawRow(UiList* list, UiObject* object);

#endif /* SRC_SHARED_JUKEBOX_H */
