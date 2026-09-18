#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

#define GPU_OT_ENTRIES  0x440
#define GPU_OT_END_PRIM 0xFFFFFF

// =============================================================================
// Types — dual DISPENV/DRAWENV + system flags
// =============================================================================

/// The display pipeline: the two framebuffer/draw environment pairs, the clocks
/// the game animates from, and the flags that decide what each vblank does.
///
/// Frames alternate between two buffers, and three fields record which one is
/// current — `drawBuffer` for the game, `otBuffer` for the ordering tables and
/// `frameBuffer` for the flip. Comparing them is how the code tells whether the
/// flip has caught up with the frame it has just built.
typedef struct {
    s32     frameCount;   // Vblanks elapsed while the game owns the display; the frame timestamp gameplay reads
    s32     gameTick;     // 1/60-second game time, accumulated for the play clock; stops advancing while a CD command runs
    s32     animFrame;    // Frames rendered since the last reset; the phase effect animation advances from
    s32     vsyncCount;   // Vblanks since the display was set up, counted even while another screen owns it
    s32     field_10;     // Advanced with `gameTick` but never paused; no reader in this tree, role unproven
    s32     loopCount;    // Main-loop iterations since the last reset, counted even when no frame is rendered
    u16     width;        // Active framebuffer width in pixels
    u16     height;       // Active framebuffer height in pixels
    u8      interlace;    // 1 when the display is interlaced
    s8      holdState;    // Negative while a caller holds the display; the low bits are never written in this tree
    s8      displayOwner; // Which path renders the frame (0 the game loop, 1 a stage transition, 2 a task spawned with its own OT)
    u8      drawBuffer;   // Index of the frame buffer the game is building (0/1)
    DISPENV dispEnv[2];   // Display environments, one per buffer, selected by `drawBuffer`
    DRAWENV drawEnv[2];   // Draw environments, one per buffer, selected by `drawBuffer`
    /// The flags at 0x100. The room-start path tests all four as one word.
    union {
        u32 word;                // the four flags as one word
        struct {
            u8 imageSource;      // What the flip uploads into the current framebuffer (0 nothing, 1/2 image strips, 3 the room's stored image slot)
            u8 pendingPlayerPos; // 1: the next room setup puts the player where the save data says, not at the room's own start
            u8 unknown_102;
            u8 flipMode;         // What the next vblank flip does (0 upload and draw, 1 draw only, 2 hold the displayed frame, 3 upload the room's image slot; bit 4 suppresses the draw)
        } flags;
    } at100;
    u16         skipDraw;       // Nonzero: flip the frame without drawing the ordering table
    u16         mdecActive;     // Nonzero while an MDEC stream owns the display; such a frame is neither flipped nor synced
    volatile u8 vsyncFlag;      // (0 the VSync callback performs the standard flip, 1 it takes the flip mode's path)
    s8          vramYOffset;    // Vertical offset of both draw environments, in pixels
    u8          frameTicks;     // 1/60-second units one rendered frame advances (1, 2 or 3)
    u8          stopTaskWalk;   // A task raises this to end the active-list walk once it returns
    byte        unknown_10c;
    u8          pendingMode;    // Display mode id for the next frame (0 none, 0x20-0x7F a mode-table entry, 0xFF a bare ordering-table setup)
    u16         roomVariant;    // Variant of the stage's room tables, copied into the session when a room is set up
    u16         screenDistance; // Projection distance of the current view, loaded into GTE H
    s16         field_112;      // Guards calls into the debug module; nothing in this tree writes it, so those paths are dead. Role unproven
    s32         otBuffer;       // Index of the ordering table being built (0/1)
    s32         frameBuffer;    // Index of the frame buffer the flip is on; the flip and the alternate display paths toggle it
    byte        unknown_11c;
    u8          holdCount;      // Outstanding display holds; the hold clears when the count reaches zero
    u8          gameMode;       // (0 running, 1 start or restart requested, 0xFF a menu owns the display)
    byte        unknown_11f;
    s16         field_120;      // Set once at display init; no reader in this tree, role unproven
    u8          keepGraphics;   // Nonzero stops a display reset from reallocating the room's model and sprite buffers
    s8          skipTeardown;   // Nonzero: skip the per-type teardown when a task is freed
    u16         region;         // Video standard the sound and CD code is timed for (0 NTSC 60 Hz, 1 PAL 50 Hz)
    s8          shakeY;         // Vertical screen offset for the next frame, clamped to ±8 and copied into `vramYOffset`
    byte        unknown_127;
    u8          otDepthShift;   // Bits a depth value is shifted by to become an ordering-table entry offset
    byte        unknown_129;
    u16         videoMode;      // Display setup to use (0 the normal one, 1 the one streaming video needs)
    u16         demoScene;      // Demo being played back (0 normal play, 1-0xF a demo scene, 0x10 the attract loop)
    u8          gameRunning;    // 1 once a new game or a load has started; enables the soft-reset combo
    u8          loadBusy;       // Nonzero while a stage load is in progress; suppresses the loading screen
    u8          cdBusy;         // Nonzero while a CD command is being processed; suppresses the re-init of the game
    byte        unknown_131[7];
} DisplayState;
STATIC_ASSERT_SIZEOF(DisplayState, 0x138);

// Types — ordering tables

/// Per-buffer OT context (Gpu_OtBuffers[2]). Indexed by display buffer (stride 0x14).
/// field_4 is OT start; field_10 is the last tag (passed to DrawOTag).
typedef struct _GpuOtBuf {
    /* 0x00 */ s32     depth;
    /* 0x04 */ u_long* ot;
    /* 0x08 */ u8      unknown_08[0x8];
    /* 0x10 */ u_long* lastTag;
} GpuOtBuf;
STATIC_ASSERT_SIZEOF(GpuOtBuf, 0x14);

// =============================================================================
// Globals
// =============================================================================

/// The one instance of the display pipeline's state.
///
/// It lives in the main executable's BSS and every overlay that draws reaches
/// it through this symbol, so it is the single place a frame's buffer, its
/// environments and its flags are recorded.
extern DisplayState gDisplayState;

extern const u16 Display_WidthTable[];
extern const u16 Display_HeightTable[];

extern u_long       Gpu_OtTags[2 * GPU_OT_ENTRIES];
extern GpuOtBuf     Gpu_OtBuffers[2];
extern GsOT         Gpu_OrderingTables[2];
extern u_long*      Gpu_CurrentOt;
extern u8*          Gpu_SysPrimCursor;         // primitive buffer cursor
extern DR_TPAGE*    Gpu_PrimCursor;            // primitive buffer cursor
extern u8           Gpu_PrimBufStatic[0x6000]; // 2 x 0x3000, base of Gpu_SysPrimCursor
extern void*        Gpu_PrimBufBase;           // base Gpu_PrimCursor is reset from
extern volatile u8  D_8006EC30;                // the flip's copy of at100.flags.imageSource, read from the VSync callback
extern volatile u8  D_80070E38;                // the flip's copy of at100.flags.flipMode, read from the VSync callback
extern volatile s32 D_80070F64;                // VSync countdown

// --- APIs ---
void  Gpu_ClearOTag(s16 tableIdx);
void  Display_SetMode(s32 arg0);
void  Display_SetAutoClear(s32 arg0, s32 arg1, s32 arg2);
void  Display_ClampField126(s8 arg0);
void  Gpu_InitOtSmall(void);
void  Gpu_InitOt(void);
Task* Display_SpawnFromMode(void);
Task* Display_SpawnWithOtSmall(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
Task* Display_SpawnWithOt(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
void  Display_FlipOtAndDispatch(s32 arg0);
void  Display_InvertFramebufferGray(void);
void  Display_SetDrawMode(s32 arg0);
s32   Display_InitModeObj(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
void  Gpu_ResetGraphAndOt(void);
/// arg2 is unused; GameMain_Loop passes gDisplayState.otBuffer for match.
s32 Display_FrameFlipDraw(s32 arg0, s32 arg1, s32 arg2);
s32 Display_DispatchModeId(s32 arg0);

/// Put draw/disp env and optionally transfer framebuffer strips (gamemain.c).
void Display_FlipDraw(s32 arg0);
/// VSync callback: timed flip / strip load / audio tick (gamemain.c).
void Display_VSyncCallback(void);
/// LoadImage strips from Fs_ImgBuffers into the active display buffer.
void Display_LoadImageStrips(s32 arg0);
/// Mem heap reset via session (otutil.c wrapper around Display_ResetHeapFromSession).
void Display_ResetHeapWrapper(void);
void Display_AcquireRef(void);
void Display_ReleaseRef(void);
void Display_TransitionTask(Task* task);
void Display_TaskLoadStep(Task* task);

#endif // DISPLAY_H
