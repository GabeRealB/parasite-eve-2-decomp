#ifndef SRC_ROOMS_NEO_ARK_SHRINE_NEO_ARK_SHRINE_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_SHRINE_NEO_ARK_SHRINE_PRIVATE_H

#include "common.h"

#include "main/task_types.h"

#include "gameplay/action_prompt.h"

/// Top-left corner of one square tile of the shrine's sliding-tile puzzle.
///
/// The same pair serves two spaces: a screen position relative to the centre
/// of the display, where the 4x4 board spans -64 to 63 on each axis, and a
/// texel position within the puzzle's texture page. Some tables of it are
/// indexed by tile number and others by board cell.
typedef struct {
    s16 x; // horizontal position, in pixels or texels
    s16 y; // vertical position, in pixels or texels
} NeoArkShrineTileOrigin;

/// `ActionPromptHotspot::id` of the hotspot covering the whole screen, which
/// follows the sixteen board cells (ids 0..15) in the puzzle's hotspot table
/// and so is the one confirmed when the cursor is off the board.
#define NEO_ARK_SHRINE_HOTSPOT_OFF_BOARD 16

/// Work block of the task that runs the shrine's sliding-tile puzzle screen,
/// allocated by its first state and kept at `Task::work`.
///
/// The puzzle's idle state latches the hotspot the player confirms. A
/// confirmed board cell slides its tile into the gap when the gap is next to
/// it, but only once the player has examined the board; until then, and always
/// for the rest of the screen, the confirm opens the Examine command at the
/// cursor instead. The steps that play out a finished row or column time
/// themselves with `timer`.
typedef struct {
    u8  unknown_0[8];  // Never read or written by the room; role unproven
    u16 timer;         // Frames the current timed step has run; each such step is entered with it at 0
    u8  unknown_A[2];  // Never read or written by the room; role unproven
    s16 selection;     // `ActionPromptHotspot::id` of the confirmed hotspot (0..15 board cell, 16 off the board)
    s8  promptKind;    // `ActionPromptHotspot::promptKind` of that hotspot, forwarded when its command prompt opens
    s8  boardExamined; // Whether the Examine command was accepted on a tile (0 a tile confirm offers Examine, 1 it slides the tile)
} NeoArkShrinePuzzleWork;
STATIC_ASSERT_SIZEOF(NeoArkShrinePuzzleWork, 0x10);

/// Hotspot table of the shrine's cap script, terminated by `ACTION_PROMPT_HOTSPOT_END`.
extern ActionPromptHotspot D_neo_ark_shrine_80182430[];

extern TaskDesc D_neo_ark_shrine_80182508[];

/// Set by one of the cap script's two pad-lerp steps and cleared by the other;
/// while it is set, the puzzle's step check reports kind 4.
extern s16 D_neo_ark_shrine_80186868;

/// Set by the cap script step that ends the prompt task. The puzzle's step
/// check consumes it to switch the room's layout, and the first falling prop
/// waits for it to clear.
extern s16 D_neo_ark_shrine_8018686A;

/// Current order index of each of the 16 slots. Read back through a `u16`
/// pointer where the puzzle swaps two of them, which is why those accesses are
/// unsigned while the rest are `s16`.
extern s16 D_neo_ark_shrine_8018686C[16];

/// Drawn position of each puzzle tile, eased towards its target every frame.
extern NeoArkShrineTileOrigin D_neo_ark_shrine_8018688C[16];

extern TaskDesc D_neo_ark_shrine_80182404[1];

extern u16 D_neo_ark_shrine_80182410[16];

extern NeoArkShrineTileOrigin D_neo_ark_shrine_8018256C[16];

extern Task* D_neo_ark_shrine_80186864;

extern NeoArkShrineTileOrigin D_neo_ark_shrine_801868CC[16];

/// Advances the shrine's tile animation by one frame and draws the puzzle.
///
/// The board must be a permutation of tile numbers 0..15, with zero the gap.
/// Screen origins are in pixels relative to the display centre; texture
/// origins are texels, and each visible tile is drawn unscaled at 32x32.
/// Drawn positions halve their remaining displacement with signed shifts,
/// then snap when both residuals are less than four pixels. The gap moves but
/// emits no packet. Requires the board and drawn positions initialized, a
/// current ordering table, the tile texture loaded, and packet-arena space
/// for fifteen `POLY_FT4` packets. Packets remain borrowed until GPU completion.
void neoArkShrineAnimateAndDrawPuzzle(void);

/// Per-frame helper of the cap script, declared without a parameter list
/// because some callers pass it their `task`: the extra argument setup is what
/// their generated code needs, and the helper ignores it.
void func_neo_ark_shrine_8017EAC0();

void func_neo_ark_shrine_8017F448(void);

void func_neo_ark_shrine_8017D9A0(Task* task);

void func_neo_ark_shrine_8017DB10(Task* arg0);

// Callbacks referenced by the overlay's shared data tables.
void func_neo_ark_shrine_8017EA70(Task*);

void func_neo_ark_shrine_8017EAE0(Task*);

void func_neo_ark_shrine_8017EB54(Task*);

void func_neo_ark_shrine_8017EBB8(Task*);

#endif // SRC_ROOMS_NEO_ARK_SHRINE_NEO_ARK_SHRINE_PRIVATE_H
