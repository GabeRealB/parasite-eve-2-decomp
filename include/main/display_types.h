#ifndef MAIN_DISPLAY_TYPES_H
#define MAIN_DISPLAY_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

/// Presentation path selected by `DisplayState::displayOwner`.
enum {
    DISPLAY_OWNER_GAME_LOOP  = 0,
    DISPLAY_OWNER_TRANSITION = 1,
    DISPLAY_OWNER_TASK       = 2,
};

/// Main-loop reset and modal-screen states, stored as a byte.
enum {
    DISPLAY_GAME_ACTIVE  = 0,
    DISPLAY_GAME_RESTART = 1,
    DISPLAY_GAME_MODAL   = 0xFF,
};

/// Presentation routine selected by the VSync callback.
enum {
    DISPLAY_VSYNC_GAME = 0,
    DISPLAY_VSYNC_TASK = 1,
};

/// Full presentation uploads strips for nonzero sources; task-only uses 2 or 3.
enum {
    DISPLAY_IMAGE_NONE              = 0,
    DISPLAY_IMAGE_STRIPS            = 1,
    DISPLAY_IMAGE_TRANSITION_STRIPS = 2,
    DISPLAY_IMAGE_ROOM_SLOT         = 3,
};

/// Low-nibble presentation modes and the flag suppressing the task ordering table.
enum {
    DISPLAY_FLIP_FULL         = 0,
    DISPLAY_FLIP_TASK_ONLY    = 1,
    DISPLAY_FLIP_HOLD         = 2,
    DISPLAY_FLIP_MODE_MASK    = 0xF,
    DISPLAY_FLIP_SKIP_TASK_OT = 0x10,
};

/// Hold bit and preserved low bits; initial setup holds with every bit set.
enum {
    DISPLAY_HOLD_ACTIVE    = 0x80,
    DISPLAY_HOLD_MODE_MASK = 0x7F,
    DISPLAY_HOLD_INITIAL   = -1,
};

/// Pending requests; high-bit sentinel requests bypass the normal display-hold gate.
enum {
    DISPLAY_MODE_NONE            = 0,
    DISPLAY_MODE_MENU_FIRST      = 0x20,
    DISPLAY_MODE_MENU_GROUP_MASK = 0xF0,
    DISPLAY_MODE_GAME_MENU_GROUP = 0x40,
    DISPLAY_MODE_MAP             = 0x43,
    DISPLAY_MODE_MENU_LIMIT      = 0x80,
    DISPLAY_MODE_DESCRIPTOR      = 0x81,
    DISPLAY_MODE_BARE_OT         = 0xFF,
};

/// CD-command latch values used to block reset while work is active.
enum {
    DISPLAY_CD_IDLE = 0,
    DISPLAY_CD_BUSY = 0xFF,
};

/// Resource and heap layout selected by the image loader.
enum {
    DISPLAY_VIDEO_NORMAL    = 0,
    DISPLAY_VIDEO_STREAMING = 1,
};

/// Demo selector values outside the numbered scene range.
enum {
    DISPLAY_DEMO_NONE         = 0,
    DISPLAY_DEMO_FIXED_REPLAY = 0x10,
};

/// Packed screen setup: width index in bits 4..7, height index in bits 0..3.
enum {
    DISPLAY_SETUP_WIDTH_MASK     = 0xF0,
    DISPLAY_SETUP_HEIGHT_MASK    = 0xF,
    DISPLAY_SETUP_INTERLACE_MASK = 0xF00,
    DISPLAY_SETUP_RGB24          = 0x2000,
    DISPLAY_SETUP_NO_CLEAR       = 0x4000,
    DISPLAY_SETUP_KEEP_VIEW      = 0x8000,
    DISPLAY_SETUP_DEFAULT        = 0x1010,
};

/// The packed room-start bytes that preserve the saved view instead of the warp's default.
enum {
    DISPLAY_ROOM_START_KEEP_VIEW_MASK = 0x00FFFF00,
};

/// Signed pixel limits accepted by the vertical screen-shake request.
enum {
    DISPLAY_SHAKE_MIN = -8,
    DISPLAY_SHAKE_MAX = 8,
};

/// Camera-depth left shifts: multiply depth before ordering-table quantization.
enum {
    DISPLAY_DEPTH_SHIFT_1X = 0,
    DISPLAY_DEPTH_SHIFT_2X = 1,
    DISPLAY_DEPTH_SHIFT_4X = 2,
    DISPLAY_DEPTH_SHIFT_8X = 3,
};

/// Resident frame presentation, clocks and controls shared by all overlays.
///
/// Buffer indices are 0 or 1. Game rendering builds `otBuffer`; tasks that own
/// presentation and the movie decoder advance `frameBuffer`. `drawBuffer`
/// selects the primitive half and environments exposed to drawing consumers.
/// The clocks reset at initialization and when deterministic replay begins.
/// Unknown storage retains its observed extent without asserting a role.
typedef struct {
    s32     frameCount;                  // Game-owned vblanks; timestamps continue during a main-loop halt
    s32     gameTick;                    // Nominal 60-Hz play-clock ticks; CD file loads pause this counter
    u32     animFrame;                   // Game frames begun; animation phase wraps as an unsigned counter
    s32     vsyncCount;                  // All vblanks since the clocks were reset, including modal presentation
    s32     loopTicks;                   // One tick per non-halted loop plus game-frame pacing ticks; CD work does not pause it
    s32     loopCount;                   // Non-halted main-loop iterations, including task-owned presentation
    u16     width;                       // Configured framebuffer width in pixels
    s16     height;                      // Configured framebuffer height in pixels
    u8      interlace;                   // Requested interlace, including the saved override (0 off, 1 on)
    s8      holdState;                   // Bit 7 blocks menu requests; low bits are preserved and their role is unproven
    s8      displayOwner;                // Presentation path (0 game loop, 1 transition, 2 independent task list)
    u8      drawBuffer;                  // Buffer exposed to drawing consumers (0/1); held frames can retain the old value
    DISPENV dispEnv[2];                  // Display areas paired with the opposite draw area in non-interlaced double buffering
    DRAWENV drawEnv[2];                  // Drawing areas and coordinate origins for each buffer
    union {
        u32 word;                        // Packed byte view; room setup tests the two middle bytes together
        struct {
            u8 imageSource;              // Background source (0 none, 1 strips, 2 transition strips, 3 room image slot)
            u8 pendingPlayerPos;         // Restore the saved player transform at the next room start (0 no, 1 yes)
            u8 unknown_102;              // Included in the room-start view-preservation test; role unproven
            s8 flipMode;                 // Low nibble: 0 both OTs, 1 task OT, 2 hold; 0x10 suppresses the task OT
        } flags;
    } control;                           // Presentation and room-start controls with byte and packed-word accesses
    u16         skipDraw;                // Nonzero suppresses the game OT during normal presentation
    u16         mdecActive;              // Nonzero lets the decoder advance buffers and skips the task-loop GPU wait
    volatile u8 vsyncFlag;               // VSync presentation path (0 game, 1 task-owned)
    s8          vramYOffset;             // Applied vertical screen shake in pixels; image uploads crop to this offset
    u8          frameTicks;              // Nominal 60-Hz animation step per game frame (1, 2 or 3); separate from gameTick
    u8          stopTaskWalk;            // Task-list request (0 continue, 1 stop after the current callback)
    byte        unknown_10c;             // Role unproven; no direct access
    u8          pendingMode;             // Request (0 none, 0x20-0x7F menu, 0x81 descriptor task, 0xFF bare OT)
    u16         spriteVariant;           // Next session's 1-based sprite-table/CD resource variant (0 unset; USA selects 1)
    u16         screenDistance;          // Low 16 bits of the view's projection distance, the value held in GTE H
    s16         debugMode;               // 0 disables external debug hooks; nonzero enables them; negative-mode meaning unproven
    s32         otBuffer;                // Game ordering-table and primitive-buffer half being built (0/1)
    s32         frameBuffer;             // Task-owned or movie framebuffer selector (0/1); compared with game buffer selectors
    byte        unknown_11c;             // Role unproven; no direct access
    u8          holdCount;               // Outstanding display holds; balanced acquire/release clears bit 7 at zero
    u8          gameMode;                // Main-loop state (0 active, 1 reinitialize, 0xFF modal screen)
    byte        unknown_11f;             // Role unproven; no direct access
    s16         field_120;               // Initialized to 1; no reader, role and signedness unproven
    s8          keepGraphics;            // Preserve resident room graphics during modal display and decode paths
    s8          immediateTaskFree;       // Task release (0 normal deferred collection, nonzero free body and task immediately)
    u16         region;                  // Timing standard for audio and CD (0 NTSC/60 Hz, 1 PAL/50 Hz)
    s8          shakeY;                  // Requested vertical screen shake in pixels, clamped to [-8, 8]
    byte        unknown_127;             // Role unproven; no direct access
    u8          otDepthShift;            // Camera-depth left shift before OT quantization (0/1/2/3 scales depth by 1/2/4/8)
    byte        unknown_129;             // Role unproven; no direct access
    u16         videoMode;               // Image/heap setup selector (0 normal, 1 streaming-video setup)
    u16         demoScene;               // Replay source (0 live play, 1-0xF numbered scene, 0x10 fixed development-memory replay)
    u8          gameRunning;             // Nonzero enables soft reset and disconnected-controller pause checks
    u8          suppressDisconnectPause; // Nonzero blocks the disconnected-controller pause screen during loads or HUD effects
    u8          cdBusy;                  // CD-command processing latch (0 idle, 0xFF busy); blocks main-loop reinitialization
    byte        unknown_131[7];          // Role and subdivision unproven; retained within the full-state clear
} DisplayState;
STATIC_ASSERT_SIZEOF(DisplayState, 0x138);

/// Per-buffer OT context (Gpu_OtBuffers[2]). Indexed by display buffer (stride 0x14).
/// field_4 is OT start; field_10 is the last tag (passed to DrawOTag).
typedef struct _GpuOtBuf {
    /* 0x00 */ s32     depth;
    /* 0x04 */ u_long* ot;
    /* 0x08 */ u8      unknown_08[0x8];
    /* 0x10 */ u_long* lastTag;
} GpuOtBuf;
STATIC_ASSERT_SIZEOF(GpuOtBuf, 0x14);

#endif // MAIN_DISPLAY_TYPES_H
