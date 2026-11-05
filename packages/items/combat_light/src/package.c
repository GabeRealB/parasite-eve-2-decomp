#include "combat_light.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include <math/gte.h>
#include <core/pe2.h>

#include <gameplay/attachment_state.h>
#include <gameplay/attachments.h>
#include <gameplay/display.h>
#include <gameplay/effects.h>
#include <gameplay/room_effects.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/scratch.h>
#include <engine/sound.h>
#include <engine/task_types.h>

#include <effects/flagged_quad/flagged_quad.h>

PE2_EDITOR_LABEL("Flash")
/// Frames the flash lasts, its brightness as it starts, and how fast it fades: each frame it loses its brightness and
/// size shifted down by the fade shift, times the frames gone.
PE2_OPTION_DEF(FLASH_FRAMES, int, 9)
PE2_OPTION_DEF(BRIGHTNESS, int, 0xE0)
PE2_OPTION_DEF(FADE_SHIFT, int, 4)
/// The size of the billboard at the beam's origin, plus a random part under the jitter, a power of two.
PE2_OPTION_DEF(BILLBOARD_SIZE, int, 0xA00)
PE2_OPTION_DEF(BILLBOARD_SIZE_JITTER, int, 0x400)

PE2_EDITOR_LABEL("Light")
/// The light the beam casts: its inner and outer radii, the frames it lasts, and what its inner radius loses each frame.
PE2_OPTION_DEF(LIGHT_INNER_RADIUS, int, 4000)
PE2_OPTION_DEF(LIGHT_OUTER_RADIUS, int, 4800)
PE2_OPTION_DEF(LIGHT_FRAMES, int, 6)
PE2_OPTION_DEF(LIGHT_SHRINK, int, 400)

PE2_EDITOR_LABEL("Cones")
/// The beam's six cone quads: how far out they reach, half their width in a 4096-unit turn, and their base's and tip's
/// radii as a whole one shifted down.
PE2_OPTION_DEF(CONE_LENGTH, int, 512)
PE2_OPTION_DEF(CONE_HALF_WIDTH, int, 0xC0)
PE2_OPTION_DEF(CONE_BASE_RADIUS_SHIFT, int, 4)
PE2_OPTION_DEF(CONE_TIP_RADIUS_SHIFT, int, 1)
/// How much dimmer red and green are than blue in the cones, as a shift.
PE2_OPTION_DEF(CONE_RED_GREEN_SHIFT, int, 1)
PE2_EDITOR_LABEL("")

STATIC_ASSERT(POWER_OF_TWO(PE2_OPTION(BILLBOARD_SIZE_JITTER)), billboard_size_jitter_is_a_mask);

static void _drawOriginBillboard(GfxCoord* beamCoord, s16 radiusScale, s16 rotation);
static void _drawConeQuad(GfxCoord* beamCoord, s16 azimuth, s16 brightness);

enum {
    ANGLE_TURN         = 0x1000,
    QUARTER_TURN       = 0x400,
    QUARTER_TURN_SHIFT = 10,
    TRIG_FRACTION_BITS = 12
};

/// The cones' azimuths, one per quarter turn in turn, with jitter, chosen when a beam starts.
static s16 _gConeAzimuths[6] = { 0 };

void combatLightEffectTask(Task* task)
{
    enum {
        STATE_START   = 0,
        STATE_FADE    = 1,
        QUADRANT_MASK = 3
    };
    EffectWork*                    effectWork;
    GfxCoord*                      beamCoord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          pointLight;
    s32                            coneIndex;
    s32                            frameAge;
    s32                            originZ;
    s32                            initialRadiusScale;
    s32                            billboardRotation;
    s32                            audioPan;
    u8                             screenTint[3];

    lightSlot  = gWorldCoordTransientPointLights;
    pointLight = &lightSlot->light;
    effectWork = task->spawnArg2.pointer;
    beamCoord  = task->extra.coordBody->coord;
    if ((gAttachmentState.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING)) {
        sndEvtRequestScriptStop(SOUND_COMBAT_LIGHT_USE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        effectKillTask(effectWork, task);
        return;
    }
    frameAge        = (u16)effectWork->age + 1;
    effectWork->age = frameAge;
    switch (task->state) {
        case STATE_START:
            // Preserve the interleaved light writes and random draws.
            pointLight->head.transform.coord.coord.t[0] = beamCoord->coord.t[0];
            RANDOM_LCG_ADVANCE();
            initialRadiusScale                          = (RANDOM_LCG_DRAW() & (PE2_OPTION(BILLBOARD_SIZE_JITTER) - 1)) + PE2_OPTION(BILLBOARD_SIZE);
            pointLight->head.transform.coord.coord.t[1] = beamCoord->coord.t[1];
            RANDOM_LCG_ADVANCE();
            billboardRotation                                  = RANDOM_LCG_DRAW() & (ANGLE_TURN - 1);
            originZ                                            = beamCoord->coord.t[2];
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            pointLight->head.color.r                           = ONE;
            pointLight->head.color.g                           = ONE;
            pointLight->head.color.b                           = ONE;
            pointLight->inner                                  = PE2_OPTION(LIGHT_INNER_RADIUS);
            pointLight->outer                                  = PE2_OPTION(LIGHT_OUTER_RADIUS);
            lightSlot->framesLeft                              = PE2_OPTION(LIGHT_FRAMES);
            pointLight->head.transform.coord.coord.t[2]        = originZ;
            effectWork->period                                 = PE2_OPTION(BRIGHTNESS);
            effectWork->scale                                  = initialRadiusScale;
            effectWork->angle                                  = billboardRotation;
            task->state                                        = STATE_FADE;
            for (coneIndex = 0; coneIndex < ARRAY_SIZE(_gConeAzimuths); coneIndex++) {
                RANDOM_LCG_ADVANCE();
                _gConeAzimuths[coneIndex] = ((coneIndex & QUADRANT_MASK) << QUARTER_TURN_SHIFT) + (RANDOM_LCG_DRAW() & (QUARTER_TURN - 1));
            }
            gAttachmentState.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            audioPan                = (s8)worldCoordGetOriginAudioPan(beamCoord);
            sndEvtRequestScriptStart(SOUND_COMBAT_LIGHT_USE, audioPan, (s8)worldCoordGetOriginAudioDepth(beamCoord));
            break;
        case STATE_FADE:
            // Scale is the billboard radius numerator; period is brightness.
            effectWork->scale  = effectWork->scale - frameAge * (effectWork->scale >> PE2_OPTION(FADE_SHIFT));
            effectWork->period = effectWork->period - effectWork->age * (effectWork->period >> PE2_OPTION(FADE_SHIFT));
            break;
    }
    _drawOriginBillboard(beamCoord, effectWork->scale, effectWork->angle);
    screenTint[0] = screenTint[1] = screenTint[2] = effectWork->period;
    effectDrawScreenTint(screenTint, GPU_BLEND_ADD);
    for (coneIndex = 0; coneIndex < ARRAY_SIZE(_gConeAzimuths); coneIndex++) {
        _drawConeQuad(beamCoord, _gConeAzimuths[coneIndex], effectWork->period);
    }
    if (pointLight->inner >= PE2_OPTION(LIGHT_SHRINK) + 1) {
        pointLight->inner -= PE2_OPTION(LIGHT_SHRINK);
    }
    if (effectWork->age >= PE2_OPTION(FLASH_FRAMES)) {
        effectKillTask(effectWork, task);
    }
}

/// Draws the additive billboard at the beam's origin, turned by `rotation` and sized by `radiusScale` in perspective;
/// a point failing to project draws nothing.
static void _drawOriginBillboard(GfxCoord* beamCoord, s16 radiusScale, s16 rotation)
{
    enum {
        BILLBOARD_TPAGE            = 0x29,
        BILLBOARD_CLUT             = 0x428B,
        BILLBOARD_TEXTURE_U        = 0x70,
        BILLBOARD_TEXTURE_V        = 0xC8,
        BILLBOARD_TEXTURE_SPAN     = 0x37,
        BILLBOARD_RAW_TEXTURE      = 1,
        BILLBOARD_SEMI_TRANSPARENT = 2
    };
    EffectBillboardScratch* scratchHead;
    EffectBillboardScratch* block;
    s32*                    depthOutput;
    POLY_FT4*               billboard;
    s32                     cornerAngle;

    // Narrow the composed origin to the GTE's signed 16-bit vertex encoding.
    scratchHead                                  = SCRATCH_STACK_CURSOR(EffectBillboardScratch);
    block                                        = scratchHead - 1;
    depthOutput                                  = &block->depth;
    block->worldPoint.vx                         = (u16)beamCoord->workm.t[0];
    block->worldPoint.vy                         = (u16)beamCoord->workm.t[1];
    block->worldPoint.vz                         = (u16)beamCoord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectBillboardScratch) = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    billboard      = gGpuPrimCursor;
    gGpuPrimCursor = billboard + 1;
    setPolyFT4(billboard);
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(depthOutput);
        block->depth++;
        billboard->tpage = BILLBOARD_TPAGE;
        billboard->clut  = BILLBOARD_CLUT;
        setUV4(billboard,
               BILLBOARD_TEXTURE_U, BILLBOARD_TEXTURE_V,
               BILLBOARD_TEXTURE_U + BILLBOARD_TEXTURE_SPAN, BILLBOARD_TEXTURE_V,
               BILLBOARD_TEXTURE_U, BILLBOARD_TEXTURE_V + BILLBOARD_TEXTURE_SPAN,
               BILLBOARD_TEXTURE_U + BILLBOARD_TEXTURE_SPAN, BILLBOARD_TEXTURE_V + BILLBOARD_TEXTURE_SPAN);
        billboard->code     |= BILLBOARD_RAW_TEXTURE | BILLBOARD_SEMI_TRANSPARENT;
        cornerAngle          = rotation;
        block->cornerOffsetX = (((radiusScale * BILLBOARD_TEXTURE_SPAN) / block->depth) * rsin(cornerAngle)) >> TRIG_FRACTION_BITS;
        block->cornerOffsetY = (((radiusScale * BILLBOARD_TEXTURE_SPAN) / block->depth) * rcos(cornerAngle)) >> TRIG_FRACTION_BITS;
        billboard->x0        = block->screenX + (u16)block->cornerOffsetX;
        billboard->x3        = block->screenX - (u16)block->cornerOffsetX;
        billboard->y0        = block->screenY - (u16)block->cornerOffsetY;
        billboard->y3        = block->screenY + (u16)block->cornerOffsetY;
        cornerAngle          = cornerAngle + QUARTER_TURN;
        block->cornerOffsetX = (((radiusScale * BILLBOARD_TEXTURE_SPAN) / block->depth) * rsin(cornerAngle)) >> TRIG_FRACTION_BITS;
        block->cornerOffsetY = (((radiusScale * BILLBOARD_TEXTURE_SPAN) / block->depth) * rcos(cornerAngle)) >> TRIG_FRACTION_BITS;
        billboard->x1        = block->screenX + (u16)block->cornerOffsetX;
        billboard->x2        = block->screenX - (u16)block->cornerOffsetX;
        billboard->y1        = block->screenY - (u16)block->cornerOffsetY;
        billboard->y2        = block->screenY + (u16)block->cornerOffsetY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                billboard);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
}

/// Rotates a cone corner into the beam's view frame, in place.
static inline void _rotateConeCorner(const MATRIX* localToView, FlaggedQuadScratch* scratchHead, s32 cornerIndex)
{
    gte_SetRotMatrix(localToView);
    gte_ldv0(&scratchHead[-1].corners[cornerIndex]);
    gte_rtv0();
    gte_stsv(&scratchHead[-1].corners[cornerIndex]);
}

/// Draws a cone quad at `azimuth`: additive, lit blue at its base's middle corner and black at the others; a corner
/// failing to project drops it.
static void _drawConeQuad(GfxCoord* beamCoord, s16 azimuth, s16 brightness)
{
    FlaggedQuadScratch* scratchHead;
    FlaggedQuadScratch* block;
    s32*                depthOutput;
    POLY_G4*            coneQuad;
    MATRIX*             localToView;
    s32                 cornerAzimuth;
    s32                 firstCornerAzimuth;
    s32                 tipZ;

    tipZ                                     = -PE2_OPTION(CONE_LENGTH);
    scratchHead                              = SCRATCH_STACK_CURSOR(FlaggedQuadScratch);
    block                                    = scratchHead - 1;
    SCRATCH_STACK_CURSOR(FlaggedQuadScratch) = block;
    depthOutput                              = &block->otz;
    gte_SetTransMatrix(&GsWSMATRIX);
    cornerAzimuth = azimuth;

    // Rotate local corners into view space, then wrap each translation to 16 bits.
    firstCornerAzimuth   = cornerAzimuth - PE2_OPTION(CONE_HALF_WIDTH);
    block->corners[0].vx = (u32)rsin(firstCornerAzimuth) >> PE2_OPTION(CONE_BASE_RADIUS_SHIFT);
    block->corners[0].vy = (u32)rcos(firstCornerAzimuth) >> PE2_OPTION(CONE_BASE_RADIUS_SHIFT);
    block->corners[0].vz = 0;
    localToView          = &beamCoord->workm;
    _rotateConeCorner(localToView, scratchHead, 0);
    block->corners[0].vx = (u16)block->corners[0].vx + (u16)beamCoord->workm.t[0];
    block->corners[0].vy = (u16)block->corners[0].vy + (u16)beamCoord->workm.t[1];
    block->corners[0].vz = (u16)block->corners[0].vz + (u16)beamCoord->workm.t[2];

    block->corners[1].vx = (u32)rsin(cornerAzimuth) >> PE2_OPTION(CONE_TIP_RADIUS_SHIFT);
    block->corners[1].vy = (u32)rcos(cornerAzimuth) >> PE2_OPTION(CONE_TIP_RADIUS_SHIFT);
    block->corners[1].vz = tipZ;
    _rotateConeCorner(localToView, scratchHead, 1);
    block->corners[1].vx = (u16)block->corners[1].vx + (u16)beamCoord->workm.t[0];
    block->corners[1].vy = (u16)block->corners[1].vy + (u16)beamCoord->workm.t[1];
    block->corners[1].vz = (u16)block->corners[1].vz + (u16)beamCoord->workm.t[2];

    block->corners[2].vx = (u32)rsin(cornerAzimuth) >> PE2_OPTION(CONE_BASE_RADIUS_SHIFT);
    block->corners[2].vy = (u32)rcos(cornerAzimuth) >> PE2_OPTION(CONE_BASE_RADIUS_SHIFT);
    block->corners[2].vz = 0;
    _rotateConeCorner(localToView, scratchHead, 2);
    block->corners[2].vx = (u16)block->corners[2].vx + (u16)beamCoord->workm.t[0];
    cornerAzimuth        = cornerAzimuth + PE2_OPTION(CONE_HALF_WIDTH);
    block->corners[2].vy = (u16)block->corners[2].vy + (u16)beamCoord->workm.t[1];
    block->corners[2].vz = (u16)block->corners[2].vz + (u16)beamCoord->workm.t[2];

    block->corners[3].vx = (u32)rsin(cornerAzimuth) >> PE2_OPTION(CONE_BASE_RADIUS_SHIFT);
    block->corners[3].vy = (u32)rcos(cornerAzimuth) >> PE2_OPTION(CONE_BASE_RADIUS_SHIFT);
    block->corners[3].vz = 0;
    _rotateConeCorner(localToView, scratchHead, 3);
    block->corners[3].vx = (u16)block->corners[3].vx + (u16)beamCoord->workm.t[0];
    block->corners[3].vy = (u16)block->corners[3].vy + (u16)beamCoord->workm.t[1];
    block->corners[3].vz = (u16)block->corners[3].vz + (u16)beamCoord->workm.t[2];

    // Reject errors from either the single-corner or three-corner projection.
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratchHead[-1].corners[0]);
    gte_rtps();
    coneQuad       = gGpuPrimCursor;
    gGpuPrimCursor = coneQuad + 1;
    setPolyG4(coneQuad);
    gte_stsxy(&coneQuad->x0);
    gte_stflg(&scratchHead[-1].flag);
    if (block->flag >= 0) {
        gte_ldv3(&scratchHead[-1].corners[1], &scratchHead[-1].corners[2], &scratchHead[-1].corners[3]);
        gte_rtpt();
        gte_stsxy3(&coneQuad->x1, &coneQuad->x2, &coneQuad->x3);
        gte_stflg(&scratchHead[-1].flag);
        if (block->flag >= 0) {
            gte_stszotz(depthOutput);
            scratchHead[-1].otz++;
            setRGB0(coneQuad, 0, 0, 0);
            setRGB1(coneQuad, 0, 0, 0);
            setRGB2(coneQuad, brightness >> PE2_OPTION(CONE_RED_GREEN_SHIFT), brightness >> PE2_OPTION(CONE_RED_GREEN_SHIFT), brightness);
            setRGB3(coneQuad, 0, 0, 0);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratchHead[-1].otz),
                    coneQuad);
            gpuSetPrimitiveBlendMode(coneQuad, GPU_BLEND_ADD, scratchHead[-1].otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(FlaggedQuadScratch);
}
