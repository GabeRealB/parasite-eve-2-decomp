/* The action prompt: the point-and-click cursors rooms and actors use at
 * puzzles - the prompt task's first two states, the cursor's drawing, a
 * hotspot hit test and a rectangle outline - and the exit state of the
 * key-item event task that spawns the prompt.
 *
 * Include this header in the prologue and each fragment at the position of
 * that function: action_prompt_reset, _move_cursors, _draw_cursor, _hit_test
 * _outline_rect and _event_end (each with the .inc.c suffix). A package includes only the
 * fragments it carries. The functions have external linkage, since some
 * packages call them from another of their files.
 */

#ifndef SRC_SHARED_ACTION_PROMPT_H
#define SRC_SHARED_ACTION_PROMPT_H

#include "main/task_types.h"

#include "gameplay/action_prompt.h"

#include "rooms/room_common.h"

void actionPromptReset(Task* task);
void actionPromptMoveCursors(Task* task);
void actionPromptDrawCursor(s32 x, s32 y, s32 variant);
s32  actionPromptHitTest(ActionPromptHotspot* table, s16 x, s16 y);
void actionPromptOutlineRect(RoomRect* rect, u8 r, u8 g, u8 b);
void actionPromptEventEnd(Task* task);

#endif /* SRC_SHARED_ACTION_PROMPT_H */
