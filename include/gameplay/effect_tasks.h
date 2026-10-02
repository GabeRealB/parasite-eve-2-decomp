#ifndef GAMEPLAY_EFFECT_TASKS_H
#define GAMEPLAY_EFFECT_TASKS_H

#include "types.h"

#include "gameplay/effects.h"

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
    EFFECT_SPRITE_ATLAS_CELL_SIZE    = 40,
    EFFECT_SPRITE_ATLAS_UV_SPAN      = EFFECT_SPRITE_ATLAS_CELL_SIZE - 1, // Inclusive texel distance between edges.
    EFFECT_SPRITE_ATLAS_TEXTURE_PAGE = 0x29,                              // 4-bit VRAM (576, 0); additive blending.
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
extern GpQuadCorner D_80111E38[4];

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
