#include "pe/combat_light.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"
#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/task_types.h"

#include "overlay.h"

static void _combatLightDrawOriginBillboard(GfxCoord* beamCoord, s16 radiusScale, s16 rotation);
static void _combatLightDrawConeQuad(GfxCoord* beamCoord, s16 azimuth, s16 brightness);

/// Angle units and fixed-point trigonometric scale used by the beam drawers.
enum {
    COMBAT_LIGHT_ANGLE_TURN         = 0x1000,
    COMBAT_LIGHT_QUARTER_TURN       = 0x400,
    COMBAT_LIGHT_QUARTER_TURN_SHIFT = 10,
    COMBAT_LIGHT_TRIG_FRACTION_BITS = 12
};

/// Retained Combat Light cone-quad azimuths, in 4096 angle units per turn.
///
/// Startup seeds all six entries in quadrant `i & 3` with random offsets
/// 0..1023. Each drawn frame uses every entry throughout the fade. This is
/// shared package storage: starting another beam replaces the angles.
static s16 _gCombatLightConeAzimuths[6] = { 0 };

void combatLightEffectTask(Task* task)
{
    enum {
        COMBAT_LIGHT_STATE_START          = 0,
        COMBAT_LIGHT_STATE_FADE           = 1,
        COMBAT_LIGHT_LIFETIME_FRAMES      = 9,
        COMBAT_LIGHT_INITIAL_BRIGHTNESS   = 0xE0,
        COMBAT_LIGHT_INITIAL_RADIUS_SCALE = 0xA00,
        COMBAT_LIGHT_RADIUS_JITTER_MASK   = 0x3FF,
        COMBAT_LIGHT_QUADRANT_MASK        = 3,
        COMBAT_LIGHT_FADE_FRACTION_BITS   = 4,
        COMBAT_LIGHT_POINT_INNER_RADIUS   = 4000,
        COMBAT_LIGHT_POINT_OUTER_RADIUS   = 4800,
        COMBAT_LIGHT_POINT_FRAMES         = 6,
        COMBAT_LIGHT_POINT_RADIUS_STEP    = 400
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
        case COMBAT_LIGHT_STATE_START:
            // Preserve the interleaved light writes and random draws.
            pointLight->head.transform.coord.coord.t[0]        = beamCoord->coord.t[0];
            gRandomLcgState                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            initialRadiusScale                                 = ((gRandomLcgState >> 16) & COMBAT_LIGHT_RADIUS_JITTER_MASK) + COMBAT_LIGHT_INITIAL_RADIUS_SCALE;
            pointLight->head.transform.coord.coord.t[1]        = beamCoord->coord.t[1];
            gRandomLcgState                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            billboardRotation                                  = (gRandomLcgState >> 16) & (COMBAT_LIGHT_ANGLE_TURN - 1);
            originZ                                            = beamCoord->coord.t[2];
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            pointLight->head.color.r                           = ONE;
            pointLight->head.color.g                           = ONE;
            pointLight->head.color.b                           = ONE;
            pointLight->inner                                  = COMBAT_LIGHT_POINT_INNER_RADIUS;
            pointLight->outer                                  = COMBAT_LIGHT_POINT_OUTER_RADIUS;
            lightSlot->framesLeft                              = COMBAT_LIGHT_POINT_FRAMES;
            pointLight->head.transform.coord.coord.t[2]        = originZ;
            effectWork->period                                 = COMBAT_LIGHT_INITIAL_BRIGHTNESS;
            effectWork->scale                                  = initialRadiusScale;
            effectWork->angle                                  = billboardRotation;
            task->state                                        = COMBAT_LIGHT_STATE_FADE;
            for (coneIndex = 0; coneIndex < ARRAY_SIZE(_gCombatLightConeAzimuths); coneIndex++) {
                gRandomLcgState                      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                _gCombatLightConeAzimuths[coneIndex] = ((coneIndex & COMBAT_LIGHT_QUADRANT_MASK) << COMBAT_LIGHT_QUARTER_TURN_SHIFT) + ((gRandomLcgState >> 16) & (COMBAT_LIGHT_QUARTER_TURN - 1));
            }
            gAttachmentState.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            audioPan                = (s8)worldCoordGetOriginAudioPan(beamCoord);
            sndEvtRequestScriptStart(SOUND_COMBAT_LIGHT_USE, audioPan, (s8)worldCoordGetOriginAudioDepth(beamCoord));
            break;
        case COMBAT_LIGHT_STATE_FADE:
            // Scale is the billboard radius numerator; period is brightness.
            effectWork->scale  = effectWork->scale - frameAge * (effectWork->scale >> COMBAT_LIGHT_FADE_FRACTION_BITS);
            effectWork->period = effectWork->period - effectWork->age * (effectWork->period >> COMBAT_LIGHT_FADE_FRACTION_BITS);
            break;
    }
    _combatLightDrawOriginBillboard(beamCoord, effectWork->scale, effectWork->angle);
    screenTint[0] = screenTint[1] = screenTint[2] = effectWork->period;
    effectDrawScreenTint(screenTint, GPU_BLEND_ADD);
    for (coneIndex = 0; coneIndex < ARRAY_SIZE(_gCombatLightConeAzimuths); coneIndex++) {
        _combatLightDrawConeQuad(beamCoord, _gCombatLightConeAzimuths[coneIndex], effectWork->period);
    }
    if (pointLight->inner >= COMBAT_LIGHT_POINT_RADIUS_STEP + 1) {
        pointLight->inner -= COMBAT_LIGHT_POINT_RADIUS_STEP;
    }
    if (effectWork->age >= COMBAT_LIGHT_LIFETIME_FRAMES) {
        effectKillTask(effectWork, task);
    }
}

/// Draws a rotated, depth-scaled textured billboard at the beam origin.
///
/// `beamCoord->workm` must be composed. `radiusScale` is the nonnegative size
/// numerator: each corner offset has magnitude `radiusScale * 55 / (SZ3 / 4 + 1)`
/// pixels. `rotation` uses 0x1000 units per turn. Projection errors discard the
/// primitive; successful draws use additive blending and the texture's own
/// colour. Borrows 0x1C scratch bytes until return and consumes one FT4 packet
/// from the primitive cursor even on rejection; changes GTE state.
static void _combatLightDrawOriginBillboard(GfxCoord* beamCoord, s16 radiusScale, s16 rotation)
{
    enum {
        COMBAT_LIGHT_BILLBOARD_TPAGE            = 0x29,
        COMBAT_LIGHT_BILLBOARD_CLUT             = 0x428B,
        COMBAT_LIGHT_BILLBOARD_TEXTURE_U        = 0x70,
        COMBAT_LIGHT_BILLBOARD_TEXTURE_V        = 0xC8,
        COMBAT_LIGHT_BILLBOARD_TEXTURE_SPAN     = 0x37,
        COMBAT_LIGHT_BILLBOARD_RAW_TEXTURE      = 1,
        COMBAT_LIGHT_BILLBOARD_SEMI_TRANSPARENT = 2
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
        billboard->tpage = COMBAT_LIGHT_BILLBOARD_TPAGE;
        billboard->clut  = COMBAT_LIGHT_BILLBOARD_CLUT;
        setUV4(billboard,
               COMBAT_LIGHT_BILLBOARD_TEXTURE_U, COMBAT_LIGHT_BILLBOARD_TEXTURE_V,
               COMBAT_LIGHT_BILLBOARD_TEXTURE_U + COMBAT_LIGHT_BILLBOARD_TEXTURE_SPAN, COMBAT_LIGHT_BILLBOARD_TEXTURE_V,
               COMBAT_LIGHT_BILLBOARD_TEXTURE_U, COMBAT_LIGHT_BILLBOARD_TEXTURE_V + COMBAT_LIGHT_BILLBOARD_TEXTURE_SPAN,
               COMBAT_LIGHT_BILLBOARD_TEXTURE_U + COMBAT_LIGHT_BILLBOARD_TEXTURE_SPAN, COMBAT_LIGHT_BILLBOARD_TEXTURE_V + COMBAT_LIGHT_BILLBOARD_TEXTURE_SPAN);
        billboard->code     |= COMBAT_LIGHT_BILLBOARD_RAW_TEXTURE | COMBAT_LIGHT_BILLBOARD_SEMI_TRANSPARENT;
        cornerAngle          = rotation;
        block->cornerOffsetX = (((radiusScale * COMBAT_LIGHT_BILLBOARD_TEXTURE_SPAN) / block->depth) * rsin(cornerAngle)) >> COMBAT_LIGHT_TRIG_FRACTION_BITS;
        block->cornerOffsetY = (((radiusScale * COMBAT_LIGHT_BILLBOARD_TEXTURE_SPAN) / block->depth) * rcos(cornerAngle)) >> COMBAT_LIGHT_TRIG_FRACTION_BITS;
        billboard->x0        = block->screenX + (u16)block->cornerOffsetX;
        billboard->x3        = block->screenX - (u16)block->cornerOffsetX;
        billboard->y0        = block->screenY - (u16)block->cornerOffsetY;
        billboard->y3        = block->screenY + (u16)block->cornerOffsetY;
        cornerAngle          = cornerAngle + COMBAT_LIGHT_QUARTER_TURN;
        block->cornerOffsetX = (((radiusScale * COMBAT_LIGHT_BILLBOARD_TEXTURE_SPAN) / block->depth) * rsin(cornerAngle)) >> COMBAT_LIGHT_TRIG_FRACTION_BITS;
        block->cornerOffsetY = (((radiusScale * COMBAT_LIGHT_BILLBOARD_TEXTURE_SPAN) / block->depth) * rcos(cornerAngle)) >> COMBAT_LIGHT_TRIG_FRACTION_BITS;
        billboard->x1        = block->screenX + (u16)block->cornerOffsetX;
        billboard->x2        = block->screenX - (u16)block->cornerOffsetX;
        billboard->y1        = block->screenY - (u16)block->cornerOffsetY;
        billboard->y2        = block->screenY + (u16)block->cornerOffsetY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                billboard);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
}

/// Rotates one reserved beam-cone corner in place into the composed view frame.
///
/// `scratchHead` is the cursor immediately above the live reserved quad block,
/// and cornerIndex is 0..3. Borrows a word-aligned matrix and SVECTOR storage;
/// the GTE loads eight vector bytes and stores only the signed XYZ halfwords.
/// Replaces the GTE rotation matrix; the caller adds translation afterwards.
static inline void _combatLightRotateConeCorner(const MATRIX* localToView, OverlayFlaggedQuadScratch* scratchHead, s32 cornerIndex)
{
    gte_SetRotMatrix(localToView);
    gte_ldv0(&scratchHead[-1].corners[cornerIndex]);
    gte_rtv0();
    gte_stsv(&scratchHead[-1].corners[cornerIndex]);
}

/// Draws one additive Gouraud quad of the beam cone at a retained azimuth.
///
/// `beamCoord->workm` must be composed; `azimuth` uses 0x1000 units per turn
/// and `brightness` is 0..224 for this task. Three local corners lie on radius
/// 256 at azimuth offsets -0xC0, 0 and +0xC0. The fourth lies at radius 2048
/// and local Z -512. Only the middle base corner is lit, with half brightness
/// in red/green and full brightness in blue. Projection errors discard the
/// quad. Reserves 0x28 scratch bytes and one G4 packet; changes GTE state and
/// retains no scratch pointers.
static void _combatLightDrawConeQuad(GfxCoord* beamCoord, s16 azimuth, s16 brightness)
{
    enum {
        COMBAT_LIGHT_CONE_TIP_Z             = -512,
        COMBAT_LIGHT_CONE_HALF_ANGLE        = 0xC0,
        COMBAT_LIGHT_CONE_BASE_RADIUS_SHIFT = 4,
        COMBAT_LIGHT_CONE_TIP_RADIUS_SHIFT  = 1
    };
    OverlayFlaggedQuadScratch* scratchHead;
    OverlayFlaggedQuadScratch* block;
    s32*                       depthOutput;
    POLY_G4*                   coneQuad;
    MATRIX*                    localToView;
    s32                        cornerAzimuth;
    s32                        firstCornerAzimuth;
    s32                        tipZ;

    tipZ                                            = COMBAT_LIGHT_CONE_TIP_Z;
    scratchHead                                     = SCRATCH_STACK_CURSOR(OverlayFlaggedQuadScratch);
    block                                           = scratchHead - 1;
    SCRATCH_STACK_CURSOR(OverlayFlaggedQuadScratch) = block;
    depthOutput                                     = &block->otz;
    gte_SetTransMatrix(&GsWSMATRIX);
    cornerAzimuth = azimuth;

    // Rotate local corners into view space, then wrap each translation to 16 bits.
    firstCornerAzimuth   = cornerAzimuth - COMBAT_LIGHT_CONE_HALF_ANGLE;
    block->corners[0].vx = (u32)rsin(firstCornerAzimuth) >> COMBAT_LIGHT_CONE_BASE_RADIUS_SHIFT;
    block->corners[0].vy = (u32)rcos(firstCornerAzimuth) >> COMBAT_LIGHT_CONE_BASE_RADIUS_SHIFT;
    block->corners[0].vz = 0;
    localToView          = &beamCoord->workm;
    _combatLightRotateConeCorner(localToView, scratchHead, 0);
    block->corners[0].vx = (u16)block->corners[0].vx + (u16)beamCoord->workm.t[0];
    block->corners[0].vy = (u16)block->corners[0].vy + (u16)beamCoord->workm.t[1];
    block->corners[0].vz = (u16)block->corners[0].vz + (u16)beamCoord->workm.t[2];

    block->corners[1].vx = (u32)rsin(cornerAzimuth) >> COMBAT_LIGHT_CONE_TIP_RADIUS_SHIFT;
    block->corners[1].vy = (u32)rcos(cornerAzimuth) >> COMBAT_LIGHT_CONE_TIP_RADIUS_SHIFT;
    block->corners[1].vz = tipZ;
    _combatLightRotateConeCorner(localToView, scratchHead, 1);
    block->corners[1].vx = (u16)block->corners[1].vx + (u16)beamCoord->workm.t[0];
    block->corners[1].vy = (u16)block->corners[1].vy + (u16)beamCoord->workm.t[1];
    block->corners[1].vz = (u16)block->corners[1].vz + (u16)beamCoord->workm.t[2];

    block->corners[2].vx = (u32)rsin(cornerAzimuth) >> COMBAT_LIGHT_CONE_BASE_RADIUS_SHIFT;
    block->corners[2].vy = (u32)rcos(cornerAzimuth) >> COMBAT_LIGHT_CONE_BASE_RADIUS_SHIFT;
    block->corners[2].vz = 0;
    _combatLightRotateConeCorner(localToView, scratchHead, 2);
    block->corners[2].vx = (u16)block->corners[2].vx + (u16)beamCoord->workm.t[0];
    cornerAzimuth        = cornerAzimuth + COMBAT_LIGHT_CONE_HALF_ANGLE;
    block->corners[2].vy = (u16)block->corners[2].vy + (u16)beamCoord->workm.t[1];
    block->corners[2].vz = (u16)block->corners[2].vz + (u16)beamCoord->workm.t[2];

    block->corners[3].vx = (u32)rsin(cornerAzimuth) >> COMBAT_LIGHT_CONE_BASE_RADIUS_SHIFT;
    block->corners[3].vy = (u32)rcos(cornerAzimuth) >> COMBAT_LIGHT_CONE_BASE_RADIUS_SHIFT;
    block->corners[3].vz = 0;
    _combatLightRotateConeCorner(localToView, scratchHead, 3);
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
            setRGB2(coneQuad, brightness >> 1, brightness >> 1, brightness);
            setRGB3(coneQuad, 0, 0, 0);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratchHead[-1].otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    coneQuad);
            gpuSetPrimitiveBlendMode(coneQuad, GPU_BLEND_ADD, scratchHead[-1].otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayFlaggedQuadScratch);
}
