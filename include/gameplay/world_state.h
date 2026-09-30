#ifndef GAMEPLAY_WORLD_STATE_H
#define GAMEPLAY_WORLD_STATE_H

#include "common.h"

/// Global at `Gp_StateF0`. `Gp_InitStateF0` zeros the object, then writes
/// `field_2B` from `Mc_SaveData[0].state.gameMode` (as `u8`), or 4 when that byte is
/// 0 and `Mc_SaveData[0].state.clearCount != 0`. `Gp_IsDebugAttachRoom() == 1` forces
/// `field_2B = 0` instead. `field_0` is a state byte (1 if first set by
/// `Gp_ArmStateF0`; 2 when the last `field_6` ref is released). `field_1`
/// is an alternate-active flag (`Gp_IsStateF0Active` / `func_800A7CB0` /
/// `Gp_EnqueueSndCdIfF0` / `Gp_CdIdleIfF0Active`); last-ref release sets it to 0x3C.
/// `field_2` is a bitset (`Gp_SetStateF0Bit` sets bit `arg0 - 1` when
/// `arg0 != 0`; also written as `Gp_StateF0.field_2`). `field_3` is cleared with
/// `field_2` on last-ref release (also written as `Gp_StateF0.field_3` by
/// `Gp_SetStateF0Byte3`). `field_4` holds the scene's actors: 0 lets them
/// run, 1 freezes them so they only redraw, 2 hides them. Gameplay raises it
/// around event views and copies it into `gRoomEffectState->effectControl` /
/// `peEffectControl` every frame. `field_5` is a u8 count incremented by `Gp_ClaimSlot18`
/// when it claims a contact record. `field_6` is a u16
/// refcount incremented by `Gp_IncStateF0Ref` and decremented by
/// `Gp_ReleaseStateF0Add` / `Gp_ReleaseStateF0Clear` / `Gp_ReleaseStateF0`. Last-ref
/// release in `Gp_ReleaseStateF0Clear` also clears words at 0x8 / 0xC / 0x10.
/// `Gp_ReleaseStateF0Add` then adds the `exp` / `bp` / `mp` of the released
/// task's enemy `param` into those same words, which is how an enemy's
/// rewards reach the battle result.
/// `func_800E2C78` adds a hit's damage, capped at the enemy's remaining `hp`,
/// into `field_14` when `(arg1 & 0x7F)` is 0x19..0x1B, and the life-drain
/// effect pays that total into the player's HP.
/// The bytes from `field_18` on are flags and modes that the actors and rooms
/// of a scene raise and test between themselves; `Gp_InitStateF0` clears them.
/// `field_2B` indexes per-level scale tables: gameplay scales damage by it, and
/// several actors pick their thresholds with it.
/// Full object may still be larger than 0x2C (`Gp_PendingObj4CFlag` is a separate
/// symbol at +0x34).
typedef struct _GpStateF0 {
    union {
        struct {
            /* 0x00 */ u8 field_0;
            /* 0x01 */ u8 field_1;
            /* 0x02 */ u8 field_2;
            /* 0x03 */ u8 field_3;
        } bytes;
        u32 packed;
    } prefix;
    /* 0x04 */ u8  field_4;
    /* 0x05 */ u8  field_5;
    /* 0x06 */ u16 field_6;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s32 field_14;
    /* 0x18 */ s8  field_18;
    /* 0x19 */ u8  field_19;
    /* 0x1A */ s8  field_1A;
    /* 0x1B */ s8  field_1B;
    /* 0x1C */ s8  field_1C;
    /* 0x1D */ u8  field_1D;
    /* 0x1E */ s8  field_1E;
    /* 0x1F */ u8  field_1F;
    /* 0x20 */ s8  field_20;
    /* 0x21 */ s8  field_21;
    /* 0x22 */ s8  field_22;
    /* 0x23 */ s8  field_23;
    /* 0x24 */ s8  field_24;
    /* 0x25 */ s8  field_25;
    /* 0x26 */ s8  field_26;
    /* 0x27 */ s8  field_27;
    /* 0x28 */ s8  field_28;
    /* 0x29 */ s8  field_29;
    /* 0x2A */ u8  field_2A;
    /* 0x2B */ u8  field_2B;
} GpStateF0;
STATIC_ASSERT_SIZEOF(GpStateF0, 0x2C);

#endif // GAMEPLAY_WORLD_STATE_H
