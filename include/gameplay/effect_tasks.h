#ifndef GAMEPLAY_EFFECT_TASKS_H
#define GAMEPLAY_EFFECT_TASKS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/effects.h"
#include "gameplay/room_effects.h"

#include "main/coord.h"
#include "main/task_types.h"

// Effect task entry points and shared drawing data.

/// Texture layout shared by the twelve-frame effect sprite atlas.
enum {
    /// Width, height and spacing of the shared atlas's square cells, in texels.
    ///
    /// Use this count for horizontal and vertical frame-origin strides.
    /// The last texel is `EFFECT_SPRITE_ATLAS_UV_SPAN` past the origin;
    /// adjacent cells begin immediately after that texel.
    EFFECT_SPRITE_ATLAS_CELL_SIZE = 40,
};

/// Distance from the first to the last texel of an effect-atlas cell, in texels.
///
/// Add this 39-texel span to a frame's U or V origin for the inclusive right
/// or bottom UV endpoint. It covers 40 texels including both endpoints;
/// frame origins advance by `EFFECT_SPRITE_ATLAS_CELL_SIZE` instead.
/// The six-column, two-row atlas reaches U=239 and V=79, within GPU UV bytes.
///
/// Spinning billboard drawers also reuse this span as their sizing multiplier:
/// size * span / depth gives the screen-space half-diagonal before rotation,
/// in pixels. Here size is the caller's sizing numerator, not a texel count,
/// and depth is SZ3 / 4 with the drawer's depth bias already applied.
enum {
    EFFECT_SPRITE_ATLAS_UV_SPAN = EFFECT_SPRITE_ATLAS_CELL_SIZE - 1,
};

/// GPU texture-page word for drawing the shared effect sprite atlas additively.
///
/// Selects 4-bit indexed texels at VRAM X=576 words, Y=0 scanlines and
/// `GPU_BLEND_ADD`. Write this packed value directly to a textured primitive's
/// `tpage`; UV coordinates are relative texels and the CLUT is selected separately.
/// Blending also requires the primitive's semitransparency bit to be enabled;
/// only texture colours with bit 15 set blend with the framebuffer.
enum {
    EFFECT_SPRITE_ATLAS_TEXTURE_PAGE = getTPage(0, GPU_BLEND_ADD, 576, 0),
};

/// UV origins and palettes for the shared twelve-frame effect sprite atlas.
///
/// Frames 0..11 occupy six columns and two rows of 40-by-40 texel cells on
/// `EFFECT_SPRITE_ATLAS_TEXTURE_PAGE`. Gameplay effects, flying Pyke flames and
/// room panels use the stored palettes; other drawers may supply their own.
/// The read-only metadata belongs to the gameplay image and may be borrowed
/// by loaded overlays while that image is loaded. Indices must be in range;
/// the table does not wrap them.
extern const EffectSpriteTextureFrame gEffectSpriteAtlasFrames[12];

/// Unit quad corners `(-1, 1)`, `(1, 1)`, `(-1, -1)`, `(1, -1)`.
extern EffectUnitQuadCorner D_80111E38[4];

/// Draws the ground shadow under an object: a flat textured quad whose corners
/// are the unit corner table scaled by `size` and rotated by `gGfxViewCoord.workm`,
/// centred on `pos`, and drawn with subtractive blending. `shade` is the
/// vertex colour, with 0 drawing the texture unmodulated and a negative value
/// drawing nothing; nothing is drawn either once `gRoomEffectState->effectControl`
/// reaches 2.
void Gp_DrawEffGroundQuad(VECTOR3* pos, s32 size, s16 shade);

void Gp_DrawEffSprite7C(GfxCoord* arg0, s32 arg1, u32 arg2);

extern TaskDesc D_80114B34[6];

void Gp_EffAttachTask37(Task* arg0);

#endif // GAMEPLAY_EFFECT_TASKS_H
