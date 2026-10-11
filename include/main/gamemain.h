#ifndef MAIN_GAMEMAIN_H
#define MAIN_GAMEMAIN_H

#include "types.h"

#include "main/wipsys_types.h"

/// Established causes of a halted game loop; other bit meanings are unproven.
enum {
    GAME_MAIN_HALT_CONTROLLER_DISCONNECT = 1 << 1,
    GAME_MAIN_HALT_SCENE_ERROR           = 1 << 3,
};

/// Resident game-loop halt requests, cleared when a run starts or restarts.
///
/// GAME_MAIN_HALT_CONTROLLER_DISCONNECT holds the dimmed pause screen and
/// audio duck until the controller pause condition clears. GAME_MAIN_HALT_SCENE_ERROR
/// is held while the scene stream reports an error, overriding the CD pause
/// block. Nonzero flags stop normal task/clock progression when pause gates
/// allow it. Bit 0 is exempt from pause-screen admission; its role is unproven.
extern volatile s32 gGameMainHaltFlags;

/// Resident system state cleared at power-on and retained across soft resets.
///
/// Records the scanned disc, title-intro policy, game-over history and whether
/// movie streaming has installed its ring and decoder. Overlay consumers borrow
/// this single in-place object for the resident executable's lifetime; it is
/// separate from session and serialized player state.
extern GameMainPersistentState gGameMainPersistentState;

/// Returns the number of resident game initializations since startup.
///
/// Includes the first boot initialization (count 1) and each subsequent game
/// restart or accepted soft reset. The u32 count wraps on overflow; reading it
/// changes no state. The title uses it to rotate attract-mode demo scenes.
u32 gameMainGetInitializationCount(void);

#endif // MAIN_GAMEMAIN_H
