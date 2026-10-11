#ifndef MAIN_PRIVATE_DISPLAY_H
#define MAIN_PRIVATE_DISPLAY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "types.h"

#include "main/display_types.h"
#include "main/task_types.h"
#include "display_types.h"

/// Packed GPU DMA tag with no payload and the end-of-chain address sentinel.
///
/// Written as a complete u_long tag at the start of reverse ordering tables.
/// Bits 0..23 terminate the link; the zero high byte transfers no command words.
#define GPU_DMA_CHAIN_END 0xFFFFFF

/// Two resident frame ordering tables of packed four-byte DMA tag words.
///
/// Index 0 or 1 selects 1088 tags: 32 foreground tags, 1024 depth-sorting tags
/// and 32 trailing tags. Game drawing uses tag 32 as its depth base; the game
/// descriptors submit from tag 1087. Task presentation can borrow the first
/// 1024 tags without that bias. Clear or reuse only after GPU drawing finishes.
/// SDK `GsOT_TAG*` views describe these same words without owning the storage.
extern u_long gGpuFrameOtTags[2][GPU_ORDERING_TABLE_BUFFER_ENTRIES];

/// Next free mixed-packet slot in the current system drawing arena half.
///
/// Game frames select one 0x3000-byte half of `gDisplayStaticPrimitiveArena`.
/// Caption and fade drawing borrow TILE, SPRT and DR_TPAGE slots and advance
/// by their complete typed sizes. Reservations must remain word-aligned and
/// fit the selected half; allocation has no capacity check. Packets remain
/// borrowed until GPU completion, and the cursor is reset each game frame.
extern void* gGpuSystemPrimCursor;

/// SDK VSync wait argument for paced frames (0 next blank, 2 or 3 paced blanks).
///
/// Also contributes `waitMode >> 1` to the game-loop clocks, independently of
/// animation `frameTicks`. Frame-timing selection sets it for both presenters.
extern s32 gDisplayVSyncWaitMode;

/// Frame-pacing budget in VSync(1) horizontal-line counter units.
///
/// Initially 240, then set to the fixed NTSC budgets 262, 525 or 787. Both
/// presenters compare elapsed scanlines with it; VSync presentation requires
/// the current scanlines plus prior callback time to exceed half the budget.
extern s32 gDisplayFrameBudgetLines;

/// Shared framebuffer request consumed by vertical-blank presentation.
///
/// Values 0 and 1 select prepared framebuffers. -1 means no pending request;
/// -2 marks synchronous late-frame presentation and suppresses the callback.
/// Both frame paths publish requests; VSync changes a consumed request to -1.
/// Initialized to buffer 0. Task control snapshots must be ready before the
/// callback can observe their request; GPU buffers remain live through drawing.
extern volatile s32 gDisplayPendingFlip;

/// Scanlines spent in the most recent vertical-blank callback.
///
/// Includes frame submission and driver polling. The paced paths reload this
/// interrupt-written value after a consumed flip, using its negation as the
/// next frame's scanline origin and copying it to the prior-callback allowance.
extern volatile s32 gDisplayVSyncElapsedLines;

/// Prior callback scanlines included in the next interrupt-side flip deadline.
///
/// Set from `gDisplayVSyncElapsedLines` after VSync consumes a frame request;
/// synchronous presentation clears it. The callback adds it to the current
/// low-16-bit scanline count before testing half the frame-pacing budget.
extern volatile s32 gDisplayPreviousVSyncElapsedLines;

/// Resident primitive bytes shared by system drawing and modal task presentation.
///
/// Frame indices 0 and 1 select equal 0x3000-byte halves. Complete packets must
/// fit their selected half and remain live until GPU drawing finishes. The
/// arena has program lifetime; neither frame path owns or allocates it.
extern u8 gDisplayStaticPrimitiveArena[DISPLAY_STATIC_PRIMITIVE_ARENA_BYTES];

/// Resident bare execution-list head for modal display-owned presentation.
///
/// Initialize with `taskInitList` before spawning or executing; empty means
/// next is NULL and prev points back to this head, which belongs to no task.
/// The display controller and spawners select it temporarily, separately from
/// the normal frame list. Reset discards links without releasing old tasks;
/// those tasks/resources must already be disposable. The head has program lifetime.
extern TaskNode gDisplayTaskList;

/// Queued task frame's image policy, retained for interrupt-side presentation.
///
/// Copies `DisplayState::control.flags.imageSource`: DISPLAY_IMAGE_* values
/// 0 none, 1 image strips, 2 transition strips, 3 captured room slot. Published
/// with the flip-control snapshot before the VSync handoff. Readers are volatile;
/// the writer uses a nonvolatile byte lvalue to preserve call-slot scheduling.
extern volatile u8 gDisplayTaskImageSourceSnapshot;

/// Queued task frame's flip control, retained for interrupt-side presentation.
///
/// Copies `DisplayState::control.flags.flipMode`. Low nibble DISPLAY_FLIP_*
/// selects 0 full, 1 task-only or 2 hold. The signed-byte comparison suppresses
/// the task OT for 0x10..0x7F, while 0x80..0xFF retain it unless holding.
/// Published with the image snapshot; volatile reads retain interrupt handoff.
extern volatile u8 gDisplayTaskFlipControlSnapshot;

/// Negative red argument that disables automatic framebuffer clearing.
enum { DISPLAY_CLEAR_DISABLED = -1 };

/// Set the automatic framebuffer-clear colour for both draw buffers.
///
/// Any negative `red` disables clearing without changing the stored colour;
/// callers use `DISPLAY_CLEAR_DISABLED`. Otherwise clearing is enabled and
/// each channel stores the low byte of its argument (normally 0..255).
/// The settings persist until changed here or by display-mode setup.
void displaySetClearColor(s32 red, s32 green, s32 blue);

/// Binds small task-owned ordering tables and the static primitive arena.
///
/// Configures both `gGpuTaskOrderingTables` slots for 64 tags and a 0x6000-byte
/// primitive arena split into two 0x3000-byte halves. Previous GPU users must
/// have finished. Does not clear tags, select the current OT or primitive cursor,
/// initialize a task list, or change display ownership; the frame path clears
/// and selects a half before drawing. Allocates and releases no storage.
void displayInitTaskBuffers(void);

/// Binds full task-owned ordering tables and clears the selected frame's tags.
///
/// Configures both `gGpuTaskOrderingTables` slots for 1024 tags in the resident
/// frame-tag buffers, whose stride includes additional foreground storage.
/// `gDisplayState.frameBuffer` must be 0 or 1; previous GPU users of the buffers
/// must have finished. Clears only that slot's 1024-tag table, terminates tag
/// zero and selects it as `gGpuCurrentOt`, without the game loop's 32-tag bias.
/// The primitive arena, task list and display ownership are unchanged.
void gpuInitTaskOrderingTables(void);

/// Executes display-list tasks and builds, paces and presents their frame.
///
/// Requires initialized task OTs, a primitive arena split into two halves,
/// live display tasks and frame-buffer index 0 or 1. Packets and borrowed image
/// storage must remain valid through GPU completion. Outside movie decoding,
/// toggles the frame buffer and waits for prior GPU work after dispatch; hold
/// mode retains the drawing buffer. Services queued CD work before pacing.
///
/// `frameStartLines` is the VSync(1) scanline-counter origin. Returns the next
/// origin, possibly negative to account for time spent in the VSync callback.
/// Within the frame budget, requests a VSync flip and presents synchronously
/// if the callback has not consumed it. Late frames present immediately.
/// Restores the caller's current OT; leaves the primitive cursor at task output.
/// `unusedOrderingTables` and `unusedOtBuffer` are retained ABI arguments and ignored.
s32 displayRunTaskFrame(GsOT* unusedOrderingTables, s32 frameStartLines, s32 unusedOtBuffer);

/// Starts the stage controller for a pending modal display request, returning 0.
///
/// Requests 0x20..0x7F queue the gameplay menu task, retaining the request byte
/// as its selector. Map requests fade to black; other menus use a dimming fade,
/// except the attachment menu (0x42), which starts no fade.
/// A demo overrides those policies with its own fade to black. Requests at or
/// above 0x80 start the controller without replacing its queued transition context;
/// smaller requests do nothing. Requires live stage/session state, loaded menu
/// code for menu requests and disposable display-task storage. Task allocation
/// failures are not reported by the constant return value.
s32 displayDispatchModeRequest(s32 modeRequest);

/// Presents a task-owned framebuffer using the queued flip and image policy.
///
/// `bufferIndex` selects buffer 0 or 1 with prepared environments and task OT.
/// The scheduling path must snapshot its flip and image controls before calling;
/// the VSync callback also uses those snapshots. Hold mode makes no GPU calls.
/// Full mode restores the background and movie when the image source is nonzero,
/// draws the current game OT, then the task OT. Other modes restore only
/// transition strips or the captured room slot before the task OT. The signed
/// flip byte suppresses the task OT for values 0x10..0x7F; values 0x80..0xFF
/// still draw it unless the low nibble selects hold.
///
/// Previous GPU users of reused storage must have finished; borrowed image,
/// movie and primitive storage must remain valid through transfer and drawing.
void displayPresentTaskFrame(s32 bufferIndex);

/// Selects the image-memory primitive reservation for task-owned drawing.
///
/// `gGpuPrimitiveBufferBase` must provide `MEMORY_PRIMITIVE_HEAP_BYTES` (0x10000)
/// writable bytes with previous GPU/heap users finished. Subsequent task frames
/// select one 0x8000-byte half. This records borrowed storage without allocating,
/// clearing it or resetting the live cursor; image-memory configuration owns
/// its lifetime. Complete packets must fit in the selected half.
void displayUseHeapTaskPrimitiveBuffer(void);

/// Selects the resident static primitive arena for task-owned drawing.
///
/// Subsequent task frames select one half of the 0x6000-byte arena (0x3000
/// bytes). Previous GPU users must have finished and packets must fit within
/// the selected half. Changes only the arena binding and byte capacity; the
/// current cursor is reset by the next frame. Allocates and releases no storage.
void displayUseStaticTaskPrimitiveBuffer(void);

/// Clears and selects a resident frame ordering table for depth-sorted drawing.
///
/// `bufferIndex` selects buffer 0 or 1; prior GPU use of that table must have
/// finished. Rebuilds all 1088 tags as a reverse DMA chain, terminates its first
/// tag and selects the depth base 32 tags later. Relative tag indices -32..1055
/// fit this buffer; ordinary depth sorting uses 0..1023. Primitive storage and
/// its cursor are unchanged. Callers borrowing the current OT must restore it
/// after their drawing when another frame owner still needs its table.
static inline void _gpuBeginOt(s32 bufferIndex)
{
    // Declare the OT pointer here so this header can precede display BSS definitions.
    extern u_long* gGpuCurrentOt;
    u_long*        firstTag;

    // Clear the reverse DMA chain before drawing tasks borrow its depth base.
    gGpuCurrentOt = gGpuFrameOtTags[bufferIndex];
    ClearOTagR(gGpuCurrentOt, GPU_ORDERING_TABLE_BUFFER_ENTRIES);
    firstTag      = gGpuCurrentOt;
    *firstTag     = GPU_DMA_CHAIN_END;
    gGpuCurrentOt = firstTag + GPU_ORDERING_TABLE_RESERVED_ENTRIES;
}

#endif // MAIN_PRIVATE_DISPLAY_H
