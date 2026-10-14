/* The action prompt: the point-and-click cursors rooms and actors use at
 * puzzles - the prompt task's first two states, the cursor's drawing, a
 * hotspot hit test and a rectangle outline - and the exit state of the
 * room event task that spawns the prompt.
 *
 * Include this header in the prologue and each fragment at the position of
 * that function: action_prompt_reset, _move_cursors, _draw_cursor, _hit_test
 * _outline_rect and _event_end (each with the .inc.c suffix). A package includes only the
 * fragments it carries. The cursor drawer has a static instance in each
 * carrier, as does the event-end state (declared in its carrier's prologue);
 * reset and cursor-motion instances are also static. Hotspot tests are static
 * except the shrine and night motel lobby copies called from another file;
 * every outline instance is static. Declare reset instances in the
 * carrier's prologue; reset and motion bindings can select additional private
 * instances there.
 */

#ifndef SRC_SHARED_ACTION_PROMPT_H
#define SRC_SHARED_ACTION_PROMPT_H

#include "main/task_types.h"

#include "gameplay/action_prompt.h"

/// Unsigned screen rectangle used by the retained room-specific prompt drawer.
///
/// Left and top are center-origin screen pixels in the same space as an
/// `ActionPromptHotspot`. Vertex stores retain the low 16 bits, preserving
/// signed coordinate bit patterns. The outline spans (`x`, `y`) to
/// (`x + w`, `y + h`); the shared hotspot drawer takes the hotspot itself.
typedef struct {
    u16 x; // Left edge, pixels from the screen center; the low 16 bits of a signed coordinate
    u16 y; // Top edge, pixels from the screen center, increasing downward; also the low 16 bits
    u16 w; // Width in pixels
    u16 h; // Height in pixels
} ActionPromptRect;
STATIC_ASSERT_SIZEOF(ActionPromptRect, 0x8);

/// Selects the task callback defined by the reset fragment.
///
/// Bind to a translation-unit-local function identifier with signature
/// `void(Task* task)`. The default is `_actionPromptResetDefault`. Declare each
/// selected instance static in its carrier's prologue before its callers; this
/// header supplies only the binding, not a function declaration. A binding
/// supplied before this header overrides the default.
///
/// Acropolis security and Shelter R47 bind an additional `_actionPromptReset`
/// instance around a second reset-fragment inclusion, then restore the default.
/// This object-like alias captures no arguments, repeats no evaluation and
/// uses neither stringification nor token pasting.
#ifndef ACTION_PROMPT_RESET_TASK
#define ACTION_PROMPT_RESET_TASK _actionPromptResetDefault
#endif

/// Selects the task callback defined by the cursor-motion fragment.
///
/// Bind to a translation-unit-local function identifier with signature
/// `void(Task* task)`. The default is `_actionPromptMoveCursorsDefault`.
/// Acropolis security and Shelter R47 bind an additional static instance,
/// `_actionPromptMoveCursors`, declared in each carrier's prologue before its
/// callers. Rebind around its move fragment and restore the default afterwards;
/// `ACTION_PROMPT_DRAW_CURSOR` selects the drawer called by that instance.
/// A binding supplied before this header also selects its static prototype.
/// This object-like alias captures no arguments, repeats no evaluation and
/// uses neither stringification nor token pasting.
#ifndef ACTION_PROMPT_MOVE_CURSORS_TASK
#define ACTION_PROMPT_MOVE_CURSORS_TASK _actionPromptMoveCursorsDefault
#endif
static void ACTION_PROMPT_MOVE_CURSORS_TASK(Task* task);

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

/// Selects the function identifier declared here and defined by the hotspot-test fragment.
///
/// The signature is `s32(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY)`;
/// the default is `actionPromptHitTest`, with external linkage for the shrine
/// and night motel lobby's same-package callers in other translation units.
/// Other ordinary carriers bind `_actionPromptHitTestDefault` before this
/// header and declare it static in their prologues. The binding selects this
/// header's prototype as well as the fragment's definition; keep it through
/// every caller that spells the binding, including the factory panel fragment.
///
/// Acropolis security and Shelter R47 bind an additional `_actionPromptHitTest`,
/// declared static in each carrier's prologue, around its fragment inclusion.
/// Restore the carrier's original binding afterwards. This object-like alias
/// substitutes only the identifier: it captures no arguments, repeats no
/// evaluation and uses neither stringification nor token pasting.
#ifndef ACTION_PROMPT_HIT_TEST
#define ACTION_PROMPT_HIT_TEST actionPromptHitTest
#endif

/// Refreshes every hotspot's hit flag and reports whether any contains the point.
///
/// Coordinates are signed center-origin screen pixels, with Y increasing
/// downward. Rectangle edges, including `x + w` and `y + h`, are inside;
/// the sums use signed 32-bit arithmetic without narrowing to 16 bits.
/// Overlapping rectangles are all marked, and zero-sized dimensions can hit
/// their edge. Each preceding entry's `hit` becomes 1 or 0; the return is 1
/// for any hit, otherwise 0, independently of the entries' choice ids.
///
/// `hotspots` must provide writable entries followed by a readable entry whose
/// `id` is `ACTION_PROMPT_HOTSPOT_END`. The sentinel's other fields are untouched;
/// an empty table returns 0. Storage remains owned by the caller.
s32 ACTION_PROMPT_HIT_TEST(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);

/// Selects the function declared here and defined by the outline fragment.
///
/// Bind to a translation-unit-local function identifier with signature
/// `void(const ActionPromptHotspot* hotspot, u8 red, u8 green, u8 blue)`.
/// The default is `_actionPromptOutlineRectDefault`. The header and fragment
/// declare each selected instance static; a binding supplied before this header
/// also selects its prototype. Keep that binding through the fragment and any
/// callers that spell it.
///
/// Shelter R47 selects an additional `_actionPromptOutlineRect`, declared static
/// in its prologue, around its fragment inclusion, then restores the default.
/// This object-like alias only substitutes an identifier: it captures no
/// arguments, repeats no evaluation and uses neither stringification nor
/// token pasting.
#ifndef ACTION_PROMPT_OUTLINE_RECT
#define ACTION_PROMPT_OUTLINE_RECT _actionPromptOutlineRectDefault
#endif
static void ACTION_PROMPT_OUTLINE_RECT(const ActionPromptHotspot* hotspot, u8 red, u8 green, u8 blue);

#endif /* SRC_SHARED_ACTION_PROMPT_H */
