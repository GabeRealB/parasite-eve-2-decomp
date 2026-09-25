#ifndef GAMEPLAY_LIGHT_H
#define GAMEPLAY_LIGHT_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// Sparse overlay of a light source: an object whose head is a
/// `GsCOORDINATE2`, so its `workm` is the light's world matrix and its light
/// fields follow at 0x50. `Gp_GetObjLuma`
/// treats `field_44` as a room-id filter against `gGameSession->at4.loc.view`
/// (0 = any room), writes `0x1000` (GTE ONE) to `field_4A`, and returns a
/// weighted `field_50/52/54` luminance. `func_800D9794` casts to
/// `GsCOORDINATE2` for `workm.t` as a `VECTOR*`, loads `field_4A` into GTE
/// IR0, and `gte_ldsv`s the three halfwords at 0x50. `func_800D98C4` /
/// `func_800D9A30` subtract `workm.t` from a world `VECTOR` and write
/// the negated normalized direction.
/// `Gp_LightFalloff` halves `field_18` as XYZ, compares distance² against
/// inner `field_58` and outer `field_5C` (each squared then `>> 2`), and
/// writes the attenuated luminance to `field_38.vx` (same word as
/// `GsCOORDINATE2.workm.t[0]`) plus the 12.4 scale to `field_4A`.
/// `Gp_LightPoint` instead subtracts a world `VECTOR3` from `field_38`
/// (same words as `GsCOORDINATE2.workm.t`),
/// writes the scale to `field_4A`, and returns the luminance.
/// `Gp_LightPointRoom` is that same subtract, plus the `field_44` room-id
/// filter and an `|dx|` / `|dz|` reject against `field_5C / 2` before
/// the squared-radius test.
/// `Gp_LightCone` is the cone-light variant (`GpObj68`): same room-id
/// filter and halved `field_24.t -` world `VECTOR3`, but outer/inner
/// radii are `field_64` / `field_60`, and the normalized direction is
/// dotted with `field_24` column 2 against `rcos(field_68 >> 1)`.
/// `func_800D759C` overlays `GsCOORDINATE2` at offset 0: `field_18` is
/// `coord.t`, `field_4C` is `sub`. It normalizes `-field_18`, rotates that
/// direction by `Transpose(Gfx_ViewWorldMtx) * sub->workm`, then writes the
/// negated row into `arg3->field_1C` and the IR0-scaled `field_50` color
/// into `arg3->field_20` (same matrix slots as `func_800D9794`).
typedef struct _GpObj44 {
    /* 0x00 */ byte           pad_0[0x18];
    /* 0x18 */ VECTOR3        field_18;
    /* 0x24 */ byte           pad_24[0x14];
    /* 0x38 */ VECTOR3        field_38;
    /* 0x44 */ s16            field_44;
    /* 0x46 */ byte           pad_46[4];
    /* 0x4A */ s16            field_4A;
    /* 0x4C */ GsCOORDINATE2* field_4C;
    /* 0x50 */ s16            field_50;
    /* 0x52 */ s16            field_52;
    /* 0x54 */ s16            field_54;
    /* 0x56 */ byte           pad_56[2];
    /* 0x58 */ s32            field_58;
    /* 0x5C */ s32            field_5C;
} GpObj44;
STATIC_ASSERT_SIZEOF(GpObj44, 0x60);

/// One of the eight transient point lights gameplay keeps on top of a room's
/// own lights, which effects, weapons, parasite energies, actors and rooms
/// switch on for as long as something of theirs glows.
///
/// A slot is lit while `framesLeft` is non-zero. The per-frame gameplay tick
/// counts it down outside events, so an owner keeps its light on by re-arming
/// the count every frame it draws. While a slot is lit, gameplay re-evaluates
/// `data.coord` against the view each frame and ranks the slot with the room's
/// point lights whenever a model is lit, reading it as `data.light`. Nothing
/// allocates the slots: each kind of owner writes indices of its own.
typedef struct _GpCoord64 {
    s32 framesLeft;          // frames the light stays lit; 0 leaves the slot dark
    union {
        GsCOORDINATE2 coord; // where the light is, parented to the view
        GpObj44       light; // the same object read as a light: colour and falloff radii
    } data;
} GpCoord64;
STATIC_ASSERT_SIZEOF(GpCoord64, 0x64);

extern GpCoord64 Gp_RoomCoords[8];

#endif // GAMEPLAY_LIGHT_H
