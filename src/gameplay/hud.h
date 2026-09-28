#ifndef GAMEPLAY_PRIVATE_HUD_H
#define GAMEPLAY_PRIVATE_HUD_H

#include "common.h"

struct GpEnemy;

/// 8-byte follow state passed to `Gp_HudTrackEnemy` / `Gp_HudTrackSlot0`.
/// `field_0` is the last `GpEnemy` drawn; `field_4` / `field_6` are the
/// previous screen X/Y that `Gp_HudTrackEnemy` lerps toward 0x6A, -0x35
/// (or -0x64 when `func_800B9D80(0x100000)` is 0).
typedef struct _GpHudTrack {
    /* 0x0 */ struct GpEnemy* field_0;
    /* 0x4 */ s16             field_4;
    /* 0x6 */ s16             field_6;
} GpHudTrack;
STATIC_ASSERT_SIZEOF(GpHudTrack, 8);

/// Seven `u16` masks tested against `PlayerStatus.field_25` by the party HP/MP
/// HUD (`func_800A57B0`); each set bit draws one 14x14 status icon.
typedef struct GpHudStatusBits {
    u16 bits[7];
} GpHudStatusBits;
STATIC_ASSERT_SIZEOF(GpHudStatusBits, 0xE);

#endif // GAMEPLAY_PRIVATE_HUD_H
