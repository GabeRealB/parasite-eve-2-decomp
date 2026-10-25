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

/// Base room selectors before and after the first shrine enemy reveal.
enum {
    NEO_ARK_SHRINE_LAYOUT_BASE              = 1,
    NEO_ARK_SHRINE_LAYOUT_BASE_AFTER_REVEAL = 4,
};

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

/// Shrine puzzle controller and its two falling-prop recipes, all at priority 192.
///
/// Entry 0 is bodyless; 1/2 own TMD bodies for the first/second prop and borrow
/// the room's model sources. Each spawn passes zero payloads. The three-entry
/// indexed table has no terminator; keep room resources loaded through teardown.
extern TaskDesc gNeoArkShrinePuzzleTaskDescs[3];

/// Whether the puzzle has latched its active room layout (0 base, 1 active).
///
/// Activation sets this before the timed room-selector change; restoration
/// and board reset clear it. A later tile move requests restoration while
/// it equals 1 and the activating top-row pattern no longer matches.
/// Stored as a signed halfword, independently of the pending restore request.
extern s16 gNeoArkShrinePuzzleLayoutActive;

/// Pending base-layout restore request from the shrine puzzle's enemy release.
///
/// Signed halfword (0 none, 1 pending), independent of the active-layout latch.
/// Survives puzzle-controller teardown. A later successful tile move consumes
/// exactly 1 after the activation, active-layout and solved checks, before the
/// enemy-release check. Consumption selects base layout 1, or 4 after the first
/// reveal, and requests a room-object rebuild; board reset also clears it.
/// The landed first prop waits for exactly 0 before teardown, without waiting
/// for the rebuild to finish. Requires the room overlay to remain loaded.
extern s16 gNeoArkShrinePuzzleLayoutRestorePending;

/// Extents of the 4x4 board and its corresponding per-tile position arrays.
enum {
    NEO_ARK_SHRINE_PUZZLE_CELL_COUNT = 16,
    NEO_ARK_SHRINE_PUZZLE_TILE_COUNT = NEO_ARK_SHRINE_PUZZLE_CELL_COUNT,
};

/// Board cells plus the off-board fallback and hotspot-table terminator.
enum { NEO_ARK_SHRINE_PUZZLE_HOTSPOT_COUNT = NEO_ARK_SHRINE_PUZZLE_CELL_COUNT + 2 };

/// Sliding-tile puzzle hotspots: sixteen cells, off-board fallback, then terminator.
///
/// IDs 0..15 select row-major board cells; ID 16 covers the whole screen and
/// follows the cells so a board hit takes precedence. Rectangles use centred
/// screen pixels with inclusive far edges. Every live entry offers Examine
/// (promptKind 0); hit bytes are cleared at session start and refreshed by hit
/// testing. The mutable table is borrowed throughout the loaded puzzle session.
extern ActionPromptHotspot gNeoArkShrinePuzzleHotspots[NEO_ARK_SHRINE_PUZZLE_HOTSPOT_COUNT];

/// Tile number occupying each board cell, in row-major order from the top left.
///
/// After reset this is a permutation of 0..15, with 0 the gap. Slides swap
/// adjacent cells; values index per-tile origins and textures, while indices
/// select board-cell origins and neighbours. Signed halfword storage remains
/// writable for the loaded room; reset must precede drawing or tile moves.
extern s16 gNeoArkShrinePuzzleCellTiles[NEO_ARK_SHRINE_PUZZLE_CELL_COUNT];

/// Animated screen origin of each tile, indexed by tile number 0..15.
///
/// Signed pixel coordinates are relative to the display centre. Reset seeds
/// all sixteen origins; drawing moves each halfway towards its target and
/// snaps residuals below four pixels. Tile zero animates but emits no quad.
/// Requires the room storage loaded throughout the puzzle session.
extern NeoArkShrineTileOrigin gNeoArkShrinePuzzleDrawnOrigins[NEO_ARK_SHRINE_PUZZLE_TILE_COUNT];

/// Bodyless sliding-tile puzzle cursor recipe, priority `TASK_PRIORITY_CONTROL` and zero metadata.
///
/// Exactly one descriptor, with no terminator. Entry 0 takes spawnArg1 1 for
/// controller port 0 and a NULL second argument. The puzzle session retains the
/// separate cursor task for explicit teardown; keep the room overlay loaded.
extern TaskDesc gNeoArkShrinePuzzleCursorTaskDescs[1];

/// Initial cell-to-tile permutation for the sixteen-cell sliding puzzle.
///
/// Cell indices 0..15 run across rows: tiles 1..15 then gap 0 in the last
/// cell. Reset copies all sixteen unsigned halfwords; there is no terminator.
extern u16 gNeoArkShrinePuzzleInitialCellTiles[NEO_ARK_SHRINE_PUZZLE_TILE_COUNT];

/// Initial screen origins indexed by puzzle tile number 0..15.
///
/// Signed pixels relative to the display centre, on a 32-pixel grid. Tile 0
/// starts at the bottom-right gap cell (32,32); reset copies every origin to
/// the drawn-position table. Keep the room-owned sixteen-entry table loaded.
extern NeoArkShrineTileOrigin gNeoArkShrinePuzzleInitialTileOrigins[NEO_ARK_SHRINE_PUZZLE_TILE_COUNT];

/// Live puzzle controller awaiting completion by the room's puzzle session.
///
/// NULL outside a session; successful spawning supplies the non-NULL task
/// polled while waiting. The controller retains its owned puzzle work until
/// exit dispatch, after which the session clears this handle without another
/// dereference. Supports one session at a time and requires the room loaded.
extern Task* gNeoArkShrinePuzzleControllerTask;

/// Destination screen origin of each tile, indexed by tile number 0..15.
///
/// Signed pixels relative to the display centre. Each drawing frame rebuilds
/// all targets from the cell-to-tile board before easing the drawn origins,
/// including tile zero. Reset initializes the board and drawn origins; these
/// targets become valid on the next drawing frame. Storage is room-owned.
extern NeoArkShrineTileOrigin gNeoArkShrinePuzzleTargetOrigins[NEO_ARK_SHRINE_PUZZLE_TILE_COUNT];

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

/// Restores the starting tile origins and cell-to-tile order and clears the layout latch and restore request.
///
/// Resets all sixteen drawn origins and board cells, including tile zero (the
/// gap). The next drawing frame reconstructs target origins from this board.
/// Requires the room's writable arrays and initial tables to remain loaded;
/// it neither allocates resources nor changes the saved or live room selectors.
void neoArkShrineResetPuzzle(void);

/// Draws the puzzle and routes cursor confirmation to commands or a tile move.
///
/// State 2 requires owned `NeoArkShrinePuzzleWork` and port 0's live action
/// prompt. CAP playback hides and stops the cursor and defers all input.
/// Otherwise the first hit hotspot is latched on confirm: ids 0..15 select
/// board cells and 16 selects the rest of the screen. Before examining the
/// board, or off the board, enters command state 3; an examined cell enters
/// slide state 6. Cancel enters close state 5 unless confirm already returned.
/// Requires the puzzle drawing resources and the terminated hotspot table.
void neoArkShrinePuzzleIdle(Task* task);

/// Slides the confirmed tile into an adjacent gap and selects the board's outcome.
///
/// State 6 requires owned puzzle work with selection in 0..15 and a board that
/// is a permutation of tile numbers 0..15 (zero is the gap). Draws one frame,
/// scans the selected cell's terminated neighbour list and sounds each swap.
/// Returns to idle even if no tile moved. A move may solve the puzzle once,
/// release enemies or start a layout transition. Requires the room's board,
/// drawing, CAP and sound resources to remain loaded; allocates no work.
void neoArkShrineSlidePuzzleTile(Task* task);

// Callbacks referenced by the overlay's shared data tables.
/// Resets and updates the sliding-tile puzzle's action cursors.
///
/// Requires a live bodyless task with state 0 (reset both ports) or 1 (move,
/// classify presses and draw). No work is allocated. `spawnArg1.value` selects
/// port 0 with 1, port 1 with 2, and both otherwise; the puzzle spawns it with 1.
/// Requires writable gameplay prompt slots, pad state and cursor drawing resources.
/// Reset advances to state 1; updates continue until the puzzle kills this task.
void neoArkShrinePuzzleCursorTask(Task* task);

/// Runs the shrine's sliding-tile puzzle and its timed room outcomes.
///
/// Requires a bodyless task with state 0..15; dispatch does not check bounds.
/// State 0 allocates owned work, spawns the cursor and acquires a menu hold.
/// States 1..6 handle cursor setup, input, commands, closing and tile moves;
/// 7..8 and 14..15 switch room layouts, and 9..13 release the shrine enemies.
/// Completion restores room control and requests a stop; the waiting room
/// task polls that request to release the puzzle task and its work. The code,
/// state table, board and drawing resources must remain loaded throughout.
void neoArkShrinePuzzleTask(Task* task);

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
