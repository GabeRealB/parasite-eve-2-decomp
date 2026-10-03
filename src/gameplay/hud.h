#ifndef GAMEPLAY_PRIVATE_HUD_H
#define GAMEPLAY_PRIVATE_HUD_H

#include "common.h"

struct Enemy;

/// Placement of the locked-on enemy's HP readout, kept between frames.
///
/// The readout is anchored at the right of the screen: near the top, or lower,
/// clear of the radar, when the player has one. A newly locked enemy's readout
/// is drawn at the anchor. While the same enemy stays locked, each frame moves
/// it one eighth of the remaining distance, rounded down, from the stored
/// position toward the anchor, so it slides when the anchor moves. A zero-filled
/// record is the valid initial state.
typedef struct {
    struct Enemy* enemy; // Enemy shown on the last drawn frame. Only compared, never dereferenced; `NULL` before the first
    s16           x;     // Horizontal position as last drawn, pixels from the screen center
    s16           y;     // Vertical position as last drawn, pixels from the screen center, increasing downward
} HudTargetHpReadout;
STATIC_ASSERT_SIZEOF(HudTargetHpReadout, 8);

/// Seven `u16` masks tested against `PlayerStatus.statusFlags` by the party HP/MP
/// HUD (`func_800A57B0`); each set bit draws one 14x14 status icon.
typedef struct GpHudStatusBits {
    u16 bits[7];
} GpHudStatusBits;
STATIC_ASSERT_SIZEOF(GpHudStatusBits, 0xE);

/// Displayed player HP and MP.
///
/// Signed widened copies of `PlayerStatus.hp` and `PlayerStatus.mp`, so a
/// negative hit-point total stays negative. While the in-game HUD is drawn,
/// each copy steps one point toward the live stat, either way. Menu drawings
/// of the HP/MP block step a copy upward only. The live stat is copied in
/// whole on a HUD reset, after equipment recalculates the maxima, when a
/// heal or boost panel or the menu HP/MP display opens, when an attachment
/// heal spends MP, when a Parasite Energy level fills MP, and when the heal
/// panel closes. HUD bars and the menu numbers and bars read the copies; the
/// HUD's numeric labels read the live stats.
typedef struct {
    s32 hp; // Displayed hit points, in the same points as `PlayerStatus.hp`
    s32 mp; // Displayed Parasite Energy, in the same points as `PlayerStatus.mp`
} HudHpMp;
STATIC_ASSERT_SIZEOF(HudHpMp, 0x8);

#endif // GAMEPLAY_PRIVATE_HUD_H
