/* A room task that plays a CD-streamed scene in place of the room: it blanks the
 * display, queues CD command 0x61 on the stream slot of the current location
 * with its view replaced by 0x64, shows the display once the command queue
 * signals, and runs until the CD is idle or a Start press aborts it; then it restores
 * the stream state and ends. One version ends at once, the other records the
 * abort and, when not aborted, holds 60 frames first.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_STREAMED_SCENE_H
#define SRC_SHARED_STREAMED_SCENE_H

#include "common.h"

#include "main/task_types.h"

void streamedScenePlay(Task* arg0);
void streamedScenePlayThenHold(Task* arg0);

#endif /* SRC_SHARED_STREAMED_SCENE_H */
