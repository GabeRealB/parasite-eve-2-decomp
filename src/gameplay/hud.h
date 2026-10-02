#ifndef GAMEPLAY_PRIVATE_HUD_H
#define GAMEPLAY_PRIVATE_HUD_H

#include "common.h"

struct Enemy;

/// 8-byte follow state passed to `Gp_HudTrackEnemy` / `Gp_HudTrackSlot0`.
/// `field_0` is the last `Enemy` drawn; `field_4` / `field_6` are the
/// previous screen X/Y that `Gp_HudTrackEnemy` lerps toward 0x6A, -0x35
/// (or -0x64 when `func_800B9D80(0x100000)` is 0).
typedef struct _GpHudTrack {
    /* 0x0 */ struct Enemy* field_0;
    /* 0x4 */ s16           field_4;
    /* 0x6 */ s16           field_6;
} GpHudTrack;
STATIC_ASSERT_SIZEOF(GpHudTrack, 8);

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
