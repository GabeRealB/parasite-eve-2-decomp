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

/// Advances tile animation and draws one frame of the sliding-tile puzzle.
///
/// Has `neoArkShrineAnimateAndDrawPuzzle`'s board, texture and GPU requirements.
/// The parameter list remains unspecified: callers supply either no argument
/// or their task, which is ignored; their argument setup is present in the binary.
void neoArkShrineDrawPuzzleFrame();

/// Restores the starting tile origins and cell-to-tile order and clears both layout latches.
///
/// Resets all sixteen drawn origins and board cells, including tile zero (the
/// gap). The next drawing frame reconstructs target origins from this board.
/// Requires the room's writable arrays and initial tables to remain loaded;
/// it neither allocates resources nor changes the saved or live room selectors.
void neoArkShrineResetPuzzle(void);

void func_neo_ark_shrine_8017D9A0(Task* task);

void func_neo_ark_shrine_8017DB10(Task* arg0);

// Callbacks referenced by the overlay's shared data tables.
/// Resets and updates the sliding-tile puzzle's action cursors.
///
/// Requires a live bodyless task with state 0 (reset both ports) or 1 (move,
/// classify presses and draw). No work is allocated. `spawnArg1.value` selects
/// port 0 with 1, port 1 with 2, and both otherwise; the puzzle spawns it with 1.
/// Requires writable gameplay prompt slots, pad state and cursor drawing resources.
/// Reset advances to state 1; updates continue until the puzzle kills this task.
void neoArkShrinePuzzleCursorTask(Task* task);

void func_neo_ark_shrine_8017EAE0(Task*);

/// Runs the first falling prop of the puzzle's enemy-release sequence.
///
/// Requires the first prop's TMD body and state 0..3: allocate and place above
/// the floor, accelerate downward to the floor, wait for the pending layout
/// restore request to clear, then kill. State 0 starts with null work; success
/// owns its drop state and model lighting matrices until teardown. Dispatch
/// copies the callback table by value and does not check the state bound.
void neoArkShrineFirstFallingPropTask(Task* task);

/// Runs the second falling prop on the first enemy-release sequence.
///
/// Requires the second prop's TMD body and state 0..2: allocate and place above
/// the floor, accelerate downward to the floor, then kill. State 0 starts with
/// null work; success owns its drop state and model lighting matrices until
/// teardown. Dispatch copies the callback table by value and does not check
/// the state bound. Unlike the first prop, it has no layout-restore wait state.
void neoArkShrineSecondFallingPropTask(Task* task);

#endif // SRC_ROOMS_NEO_ARK_SHRINE_NEO_ARK_SHRINE_PRIVATE_H
