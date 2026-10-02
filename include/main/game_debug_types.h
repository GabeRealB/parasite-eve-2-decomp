#ifndef MAIN_GAME_DEBUG_TYPES_H
#define MAIN_GAME_DEBUG_TYPES_H

#include "common.h"

// Established diagnostic selection, input override modes and loading states.
enum {
    GAME_DEBUG_DIAGNOSTIC_LIGHT_PROBE  = 0x13,
    GAME_DEBUG_INPUT_OVERRIDE_REPLAY   = -1,
    GAME_DEBUG_INPUT_OVERRIDE_NONE     = 0,
    GAME_DEBUG_INPUT_OVERRIDE_EXTERNAL = 1,
    GAME_DEBUG_LOADING_IDLE            = 0,
    GAME_DEBUG_LOADING_ACTIVE          = 1,
};

/// Resident diagnostic controls, input override mode and session-loading status.
///
/// The resident instance is cleared at main-loop initialization and survives
/// overlay loads. Input overrides run during game-loop-owned input updates and
/// zero all four analog axes before processing the button sample. Replay ends
/// by restoring live input. Unknown storage retains its full cleared extent;
/// neither its contents nor its internal field boundaries are established.
typedef struct {
    byte unknown_0[0x1];    // Cleared at initialization; role unproven
    u8   diagnosticMode;    // Diagnostic selection (0x13 light probe); other values unproven
    byte unknown_2[0x1];    // Cleared at initialization; role unproven
    u8   loadingActive;     // Session load status (0 idle/finished, 1 waiting/loading)
    byte unknown_4[0x4];    // Cleared at initialization; contents unproven
    s8   inputOverrideMode; // Input source (0 live, -1 replay, 1 external hook); other nonzero values also replay
    s8   field_9;           // 1 calls an external hook at non-demo startup; role unproven
    s8   hideHud;           // HUD suppression (0 draw, nonzero skip status/ammo/target display)
    byte unknown_B[0x11];   // Cleared at initialization; contents unproven
} GameDebugState;
STATIC_ASSERT_SIZEOF(GameDebugState, 0x1C);

#endif // MAIN_GAME_DEBUG_TYPES_H
