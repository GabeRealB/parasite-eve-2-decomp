#ifndef MAIN_DISPLAY_H
#define MAIN_DISPLAY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "types.h"

#include "main/display_types.h"
#include "main/task_types.h"

/// The ordering table the frame being built is linked into.
///
/// Drawing is dispatched as tasks that take no ordering table of their own, so
/// the code building a frame publishes the table it wants filled here and puts
/// the previous value back afterwards. Primitives are linked at an index from
/// this base, which is not always the table's own start.
extern u_long* gGpuCurrentOt;

/// Resolve an aligned byte offset from depth quantization to an OT tag.
/// Callers supply a multiple of sizeof(u_long), within the current table.
#define Gpu_OtEntryAtByteOffset(byteOffset) (&gGpuCurrentOt[(byteOffset) / sizeof(u_long)])

/// Vertex `n`'s colour word: `rn`, `gn`, `bn`, then the primitive's `code` for
/// vertex 0 and a pad byte for the others. Build a constant with `PRIM_RGBC`.
#define PRIM_COLOR_WORD(p, n) (*(u32*)&(p)->r##n)

/// Vertex `n`'s position word: `xn` in the low half, `yn` in the high half, the
/// layout the GTE stores a projected point in.
#define PRIM_XY_WORD(p, n) (*(u32*)&(p)->x##n)

/// A colour word from its bytes; `code` is the primitive code for vertex 0 and
/// 0 for the other vertices.
#define PRIM_RGBC(r, g, b, code) \
    ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | ((u32)(code) << 24))

/// The one instance of the display pipeline's state.
///
/// It lives in the main executable's BSS and every overlay that draws reaches
/// it through this symbol, so it is the single place a frame's buffer, its
/// environments and its flags are recorded.
extern DisplayState gDisplayState;

extern GpuOtBuf Gpu_OtBuffers[2];

extern GsOT Gpu_OrderingTables[2];

/// The primitive buffer allocation cursor.
///
/// Drawing code writes its packets straight into the primitive buffer and
/// advances this pointer past what it took, so the cursor is the whole
/// allocation state of the buffer. Nothing is freed within a frame, and the
/// buffer comes back whole when the next frame resets the cursor to the start
/// of the half it draws from.
extern u8* gGpuPrimCursor;

void Gpu_ClearOTag(s16 tableIdx);

void Display_SetMode(s32 arg0);

void Display_ClampField126(s8 arg0);

Task* Display_SpawnWithOtSmall(s32 arg0, s32 arg1, TaskSpawnArg arg2, TaskSpawnArg arg3);

Task* Display_SpawnWithOt(TaskDesc* descriptor, s32 arg1, TaskSpawnArg arg2, TaskSpawnArg arg3);

void Display_SetDrawMode(s32 arg0);

/// Queues a mode transition; the task is spawned asynchronously, so returns NULL.
Task* Display_InitModeObj(TaskDesc* descriptor, s32 arg1, TaskSpawnArg arg2, s32 arg3);

void Gpu_ResetGraphAndOt(void);

/// LoadImage strips from Fs_ImgBuffers into the active display buffer.
void Display_LoadImageStrips(s32 arg0);

/// Mem heap reset via session (otutil.c wrapper around Display_ResetHeapFromSession).
void Display_ResetHeapWrapper(void);

void Display_AcquireRef(void);

void Display_ReleaseRef(void);

#endif // MAIN_DISPLAY_H
