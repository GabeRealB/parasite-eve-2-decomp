/* The action prompt: the point-and-click cursors rooms and actors use at
 * puzzles - the prompt task's first two states, the cursor's drawing, a
 * hotspot hit test and a rectangle outline - and the exit state of the
 * key-item event task that spawns the prompt.
 *
 * Include this header in the prologue and each fragment at the position of
 * that function: action_prompt_reset, _move_cursors, _draw_cursor, _hit_test
 * _outline_rect and _event_end (each with the .inc.c suffix). A package includes only the
 * fragments it carries. The cursor drawer has a static instance in each
 * carrier; the other functions have external linkage, since some packages
 * call them from another of their files.
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

/// Selects the cursor drawer declared and called by the included prompt fragments.
///
/// Bind to a translation-unit-local function identifier with signature
/// `void(s32 cursorX, s32 cursorY, s32 cursorMode)`. Coordinates are center-origin
/// screen pixels and the mode is an `ACTION_PROMPT_MODE_*` value. The default is
/// `_actionPromptDrawCursorDefault`. Acropolis security and Shelter R47 select
/// `_actionPromptDrawCursor` for an additional prompt.
///
/// Keep the same binding around the move and draw fragments. Declare an
/// additional instance static in the carrier's prologue before its callers,
/// then restore the default after its draw fragment. A binding supplied before
/// this header also selects its static prototype. This object-like alias only
/// substitutes the identifier: it captures no arguments, repeats no evaluation
/// and uses neither stringification nor token pasting.
#ifndef ACTION_PROMPT_DRAW_CURSOR
#define ACTION_PROMPT_DRAW_CURSOR _actionPromptDrawCursorDefault
#endif
static void ACTION_PROMPT_DRAW_CURSOR(s32 cursorX, s32 cursorY, s32 cursorMode);

s32  actionPromptHitTest(ActionPromptHotspot* table, s16 x, s16 y);
void actionPromptOutlineRect(ActionPromptRect* rect, u8 r, u8 g, u8 b);
void actionPromptEventEnd(Task* task);

#endif /* SRC_SHARED_ACTION_PROMPT_H */
