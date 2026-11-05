/* Water effects: a splash quad and spin and tile sprites drawn at a coordinate, the ripple and drift tasks built on
 * them, two tasks that redraw framebuffer scanlines displaced by a sine wave (a distortion band and a refraction
 * ripple), and the wave-crested strips of the Shelter's sewer channels (wave_strips.inc.c, configured by the includer).
 *
 * Include this header once in the room prologue and each fragment where its function sits. The spray fragment defines
 * the public void (Task*) callback named by WATER_SPRAY_TASK, declared in the carrier's room header; the ripple and
 * drift tasks are inline, and their carriers wrap them in a callback of their own.
 */

#ifndef EFFECTS_WATER_WATER_H
#define EFFECTS_WATER_WATER_H

#include <engine/areas.h>
#include <engine/coord.h>
#include <engine/display.h>
#include <engine/task_types.h>
#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <gameplay/effects.h>

/// Scratch block of the refraction drawer: the transposed view rotation, each screen row's ray and the plane-depth
/// dividend.
typedef struct {
    MATRIX  transposedView;  // Transpose of the view rotation, loaded into the GTE
    SVECTOR screenRow;       // Row ray: X 0, Y the row minus 120, Z the projection distance
    SVECTOR rotatedRow;      // screenRow rotated; its Y divides depth
    SVECTOR viewTranslation; // View translation, rotated; only Y is read
    s32     depth;           // (viewTranslation.vy + plane Y) * projection distance
    byte    field_3C[0x10];  // Never accessed
} WaterRefractionScratch;

static void _waterDrawSplash(const GfxCoord* coord, s32 halfSize, s32 brightness);
/* Define WATER_SHARED_U16_DRAWERS before this header to declare the u16 drawers of spin_u16.inc.c, which the room
 * then includes; WATER_OWN_U16_DRAWERS means the room declares and defines its own. */
#if defined(WATER_SHARED_U16_DRAWERS) && !defined(WATER_OWN_U16_DRAWERS)
static void _waterDrawSpinU16(const GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTileU16(const GfxCoord* coord, u16 textureCell, s16 radiusScale);
#endif
/// Redraws bands of the Neo Ark submarine tunnel and gallery views with their scanlines shifted vertically by a sine
/// wave, faded over each band's last sixteen rows. Packets go to the free tail of actor arena 0 in tunnel variant 3,
/// otherwise after the refraction banks in actor buffer 2.
void waterDistortBandTask(Task* task);

/// Redraws the Neo Ark water surfaces with their scanlines shifted vertically by a sine wave, in configured views of
/// the bridge, island, garden, pavilion, submarine gallery and woodland path; each row is sorted at its depth on a
/// horizontal plane. Packets go to two banks at the start of actor buffer 2, one per display buffer.
void waterRefractionTask(Task* task);

/// Sets the pixel offset from a spin sprite's center to one corner at `cornerAngle` (0 up, a quarter turn right), sized
/// by `radiusScale` over the projected depth. Y points up, so the caller subtracts it from screen Y.
static inline void _waterComputeSpriteCornerOffset(EffectShapeScratch* projection, s16 radiusScale, s32 cornerAngle)
{
    enum {
        WATER_SPRITE_CORNER_PERSPECTIVE_SCALE  = 31,
        WATER_SPRITE_CORNER_TRIG_FRACTION_BITS = 12
    };
    s32    halfDiagonalPixels;
    q19_12 angleSine;
    q19_12 angleCosine;

    angleSine                   = rsin(cornerAngle);
    halfDiagonalPixels          = (radiusScale * WATER_SPRITE_CORNER_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.x = (halfDiagonalPixels * angleSine) >> WATER_SPRITE_CORNER_TRIG_FRACTION_BITS;
    angleCosine                 = rcos(cornerAngle);
    halfDiagonalPixels          = (radiusScale * WATER_SPRITE_CORNER_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.y = (halfDiagonalPixels * angleCosine) >> WATER_SPRITE_CORNER_TRIG_FRACTION_BITS;
}

/// Sets an upright quad's corners `screenExtent` pixels to each side of its projected anchor, the anchor a quarter of
/// the height above the bottom edge; a negative extent reverses the corners.
static inline void _waterSetUprightSpriteCorners(POLY_FT4* quad, const EffectCentreScratch* projection)
{
    quad->x0 = quad->x2 = projection->screenX - projection->screenExtent;
    quad->x1 = quad->x3 = projection->screenX + projection->screenExtent;
    quad->y0 = quad->y1 = projection->screenY - projection->screenExtent - (projection->screenExtent >> 1);
    quad->y2 = quad->y3 = projection->screenY + (projection->screenExtent >> 1);
}

#endif /* EFFECTS_WATER_WATER_H */
