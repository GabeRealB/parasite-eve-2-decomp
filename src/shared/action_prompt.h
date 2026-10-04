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

/// Rectangle the action prompt's outline drawer takes: a left and top edge
/// with a width and height, in the center-origin screen pixels of an
/// `ActionPromptHotspot`.
///
/// It is the unsigned reading of a hotspot's first four fields, and callers
/// pass a hotspot entry through it. The drawer only adds the fields and stores
/// the low 16 bits on a line's signed vertices, so a negative hotspot
/// coordinate arrives at the line unchanged. The outline spans (`x`, `y`) to
/// (`x + w`, `y + h`).
typedef struct {
    u16 x; // Left edge, pixels from the screen center; the low 16 bits of a signed coordinate
    u16 y; // Top edge, pixels from the screen center, increasing downward; also the low 16 bits
    u16 w; // Width in pixels
    u16 h; // Height in pixels
} ActionPromptRect;
STATIC_ASSERT_SIZEOF(ActionPromptRect, 0x8);

void actionPromptReset(Task* task);
void actionPromptMoveCursors(Task* task);
void actionPromptDrawCursor(s32 x, s32 y, s32 variant);
s32  actionPromptHitTest(ActionPromptHotspot* table, s16 x, s16 y);
void actionPromptOutlineRect(ActionPromptRect* rect, u8 r, u8 g, u8 b);
void actionPromptEventEnd(Task* task);

#endif /* SRC_SHARED_ACTION_PROMPT_H */
