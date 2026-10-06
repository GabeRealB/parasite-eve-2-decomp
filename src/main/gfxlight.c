#include "main/gfx.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgs.h>

#include "types.h"

#include "gfx.h"
#include "main/scratch.h"

/// Scratch reservation for one flat light's normalized direction.
///
/// `direction` is the last eight bytes of a 24-byte reservation, eight bytes
/// below the scratch cursor from before the reservation. The leading bytes
/// are never accessed; their extent is what places `direction` there while
/// the stack moves by the whole reservation. Released before the caller
/// returns. Normalization borrows a further reservation beneath this one
/// and leaves the direction's final halfword unchanged.
typedef struct {
    u8      pad[0x10]; // Unused
    SVECTOR direction; // Normalized xyz, length about ONE (4096); final halfword unused
} _GfxFlatLightScratch;
STATIC_ASSERT_SIZEOF(_GfxFlatLightScratch, 0x18);

MATRIX D_80074080;

/// Shift from RGB byte levels (256 for 1.0) to Q12 light-colour coefficients.
enum { GRAPHICS_FLAT_LIGHT_COLOR_SHIFT = 4 };

/// Stores a normalized flat light's direction row and RGB coefficient column.
///
/// `lightIndex` is 0..2; `normalizedDirection` supplies an SVECTOR's readable
/// xyz scaled by `ONE`, and `light` supplies RGB bytes, shifted left by four.
/// Writes three halfwords in each writable matrix, direction first, retaining
/// all other entries.
/// Evaluates the index six times and every other argument three times.
/// Arguments must be stable expressions without side effects; inputs must
/// remain readable through the stores. Captures no caller identifiers.
/// Expands to a compound statement; invoke without a trailing semicolon.
#define GRAPHICS_STORE_FLAT_LIGHT_MATRICES(lightIndex, light, normalizedDirection, directionMatrix, colorMatrix) \
    {                                                                                                            \
        (directionMatrix)->m[(lightIndex)][0] = -(normalizedDirection).vx;                                       \
        (directionMatrix)->m[(lightIndex)][1] = -(normalizedDirection).vy;                                       \
        (directionMatrix)->m[(lightIndex)][2] = -(normalizedDirection).vz;                                       \
        (colorMatrix)->m[0][(lightIndex)]     = (light)->r << GRAPHICS_FLAT_LIGHT_COLOR_SHIFT;                   \
        (colorMatrix)->m[1][(lightIndex)]     = (light)->g << GRAPHICS_FLAT_LIGHT_COLOR_SHIFT;                   \
        (colorMatrix)->m[2][(lightIndex)]     = (light)->b << GRAPHICS_FLAT_LIGHT_COLOR_SHIFT;                   \
    }

/// Installs one flat directional light in a matrix pair for the default lights.
///
/// Uses the index, input and scratch-stack contract of `gfxSetFlatLight`.
static __inline__ void _gfxWriteFlatLightMatrices(s32 lightIndex, const GsF_LIGHT* light, MATRIX* directionMatrix, MATRIX* colorMatrix)
{
    _GfxFlatLightScratch* scratch;

    // Inline expansion preserves the default callers' scratch-cursor accesses.
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxFlatLightScratch);
    gfxNormalizeLightDirection(light, &scratch->direction);

    GRAPHICS_STORE_FLAT_LIGHT_MATRICES(lightIndex, light, scratch->direction, directionMatrix, colorMatrix)

    SCRATCH_STACK_RELEASE_BLOCK(_GfxFlatLightScratch);
}

void gfxResetDefaultLights(void)
{
    // Directional colours use RGB bytes; ambient uses raw GTE register units.
    enum {
        GRAPHICS_DEFAULT_LIGHT_0_COLOR = 0xD0,
        GRAPHICS_DEFAULT_LIGHT_1_COLOR = 0x80,
        GRAPHICS_DEFAULT_LIGHT_2_COLOR = 0x60,
        GRAPHICS_DEFAULT_AMBIENT_COLOR = 0x40,
    };
    GsF_LIGHT lights[3];

    lights[0].vx = 100;
    lights[0].vy = 100;
    lights[0].vz = 100;
    lights[0].r  = GRAPHICS_DEFAULT_LIGHT_0_COLOR;
    lights[0].g  = GRAPHICS_DEFAULT_LIGHT_0_COLOR;
    lights[0].b  = GRAPHICS_DEFAULT_LIGHT_0_COLOR;
    _gfxWriteFlatLightMatrices(0, &lights[0], &GsLIGHTWSMATRIX, &D_80074080);

    lights[1].vy = -50;
    lights[1].vz = -100;
    lights[1].vx = 20;
    lights[1].r  = GRAPHICS_DEFAULT_LIGHT_1_COLOR;
    lights[1].g  = GRAPHICS_DEFAULT_LIGHT_1_COLOR;
    lights[1].b  = GRAPHICS_DEFAULT_LIGHT_1_COLOR;
    _gfxWriteFlatLightMatrices(1, &lights[1], &GsLIGHTWSMATRIX, &D_80074080);

    lights[2].vx = -20;
    lights[2].vy = 20;
    lights[2].vz = 100;
    lights[2].r  = GRAPHICS_DEFAULT_LIGHT_2_COLOR;
    lights[2].g  = GRAPHICS_DEFAULT_LIGHT_2_COLOR;
    lights[2].b  = GRAPHICS_DEFAULT_LIGHT_2_COLOR;
    _gfxWriteFlatLightMatrices(2, &lights[2], &GsLIGHTWSMATRIX, &D_80074080);

    D_80074080.t[0] = GRAPHICS_DEFAULT_AMBIENT_COLOR;
    D_80074080.t[1] = GRAPHICS_DEFAULT_AMBIENT_COLOR;
    D_80074080.t[2] = GRAPHICS_DEFAULT_AMBIENT_COLOR;
}

void gfxSetFlatLight(s32 lightIndex, const GsF_LIGHT* light, MATRIX* directionMatrix, MATRIX* colorMatrix)
{
    _GfxFlatLightScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxFlatLightScratch);
    gfxNormalizeLightDirection(light, &scratch->direction);

    GRAPHICS_STORE_FLAT_LIGHT_MATRICES(lightIndex, light, scratch->direction, directionMatrix, colorMatrix)

    SCRATCH_STACK_RELEASE_BLOCK(_GfxFlatLightScratch);
}

/// Replaces one flat directional light in the resident default matrix pair.
///
/// `lightIndex` is 0..2; input direction, colour and scratch requirements are
/// those of `gfxSetFlatLight`. Writes `GsLIGHTWSMATRIX`'s selected direction
/// row and `D_80074080`'s RGB column; ambient colour is retained.
static void _gfxSetDefaultFlatLight(s32 lightIndex, const GsF_LIGHT* light)
{
    _gfxWriteFlatLightMatrices(lightIndex, light, &GsLIGHTWSMATRIX, &D_80074080);
}

/// Sets the resident default ambient RGB in raw GTE background-register units.
///
/// Values use 16 units per RGB unit and are stored unchanged in `D_80074080.t`.
/// There is no scaling or clamping. The TMD renderer loads them when drawing;
/// this setter leaves directional coefficients and current GTE state intact.
static void _gfxSetDefaultAmbientColor(long ambientRed, long ambientGreen, long ambientBlue)
{
    D_80074080.t[0] = ambientRed;
    D_80074080.t[1] = ambientGreen;
    D_80074080.t[2] = ambientBlue;
}
