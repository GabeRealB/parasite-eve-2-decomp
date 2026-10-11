#include "weapons/m4a1_hammer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "gameplay/animation.h"
#include "types.h"
/// Signed texture-frame counter for the six-cell sprite's forward declaration.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"
#include "../../shared/beam_strip.h"

/// Charge-flare instance of `spriteQuadDraw`.
///
/// `pos` is three cached translation words. `frame` walks the eight-cell strip,
/// `size` is the unsigned perspective numerator and `angle` is the spin in
/// 4096 units per turn.
static void spriteQuadDrawCharge(const long* pos, u16 frame, u16 size, s16 angle);

/// Signed perspective-sizing numerator for the attachment and contact beams.
enum { M4A1_HAMMER_BEAM_WIDTH_SCALE = 0x280 };

/// Eight charged sparks stored in contiguous angle, height and radius banks.
enum {
    M4A1_HAMMER_SPARK_COUNT       = 8,
    M4A1_HAMMER_SPARK_HEIGHT_BASE = M4A1_HAMMER_SPARK_COUNT,
    M4A1_HAMMER_SPARK_RADIUS_BASE = M4A1_HAMMER_SPARK_COUNT * 2,
};

static s16     _gM4a1HammerSparkState[M4A1_HAMMER_SPARK_COUNT * 3];
static Task*   _gM4a1HammerGlowTask;
static SVECTOR _gM4a1HammerAttachmentOrigin;

/// Attachment glow origin in weapon-local integer coordinates.
static SVECTOR _gM4a1HammerGlowLocalOffset = { 0, 0x280, 0x20, 0 };

/// Converts a charged spark's local offset into a beam endpoint in place.
///
/// `glowCoord` points to a cached graphics coordinate with Q12 rotation.
/// `sparkPosition` is a word-aligned, readable full `SVECTOR` lvalue with writable
/// xyz in signed game-coordinate units. `attachmentOrigin` is a readable `SVECTOR`
/// lvalue in composition-root space, published before this update's composition.
/// Rotation saturates to -32768..32767; adding the origin wraps each component
/// to 16 bits. The result is in composition-root space (`GsWSMATRIX` input).
/// `glowCoord` is evaluated once; both vector arguments are evaluated repeatedly
/// and must have no side effects. The writable vector must not overlap either
/// input. Its fourth halfword is read but unchanged. Captures no caller identifiers.
/// Overwrites GTE RT, V0, MAC1..3, IR1..3 and FLAG; translation registers are unchanged.
/// Expands to several statements; invoke only within a braced block. Defined only
/// around the glow task and undefined after it; no carrier configuration is needed.
#define M4A1_HAMMER_COMPOSE_SPARK_POSITION(glowCoord, sparkPosition, attachmentOrigin) \
    gte_SetRotMatrix(&(glowCoord)->workm);                                             \
    gte_ldv0(&(sparkPosition));                                                        \
    gte_rtv0();                                                                        \
    gte_stsv(&(sparkPosition));                                                        \
    (sparkPosition).vx = (sparkPosition).vx + (u16)(attachmentOrigin).vx;              \
    (sparkPosition).vy = (sparkPosition).vy + (u16)(attachmentOrigin).vy;              \
    (sparkPosition).vz = (sparkPosition).vz + (u16)(attachmentOrigin).vz

void m4a1HammerGlowTask(Task* task)
{
    enum {
        M4A1_HAMMER_GLOW_TASK_ATTACH     = 0,
        M4A1_HAMMER_GLOW_TASK_UPDATE     = 1,
        M4A1_HAMMER_LIGHT_SLOT           = 1,
        M4A1_HAMMER_LIGHT_REFRESH_FRAMES = 4,
        M4A1_HAMMER_CHARGED_TICKS        = 5,
        M4A1_HAMMER_ANGLE_MASK           = 0xFFF, // 4096 angle units per turn
        M4A1_HAMMER_TRIG_SHIFT           = 12,    // rsin/rcos: 4096 represents 1.0
        M4A1_HAMMER_SPIN_TICK_MASK       = 15,
        M4A1_HAMMER_IDLE_LIGHT_INNER     = 0x80,
        M4A1_HAMMER_IDLE_LIGHT_OUTER     = 0x400,
        M4A1_HAMMER_CHARGED_LIGHT_INNER  = 0x400,
        M4A1_HAMMER_CHARGED_LIGHT_OUTER  = 0x4000
    };
    EffectWork*                    work;
    GfxCoord*                      glowCoord;
    GfxCoord*                      lightCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    s32                            sparkIndex;
    s32                            heightIndex;

    work           = task->spawnArg2.pointer;
    glowCoord      = task->extra.coordBody->coord;
    transientLight = &gWorldCoordTransientPointLights[M4A1_HAMMER_LIGHT_SLOT];
    lightCoord     = &transientLight->light.head.transform.coord;
    pointLight     = &transientLight->light;

    if (((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) == 0 && gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age = work->age + 1;
        switch (task->state) {
            case M4A1_HAMMER_GLOW_TASK_ATTACH:
                // Keep the flare at the weapon's fixed local offset.
                glowCoord->parent = work->parent;
                gfxSetRotIdentity(&glowCoord->coord);
                glowCoord->coord.t[0]   = _gM4a1HammerGlowLocalOffset.vx;
                glowCoord->coord.t[1]   = _gM4a1HammerGlowLocalOffset.vy;
                glowCoord->coord.t[2]   = _gM4a1HammerGlowLocalOffset.vz;
                glowCoord->composeStamp = GRAPHICS_COORD_DIRTY;

                _gM4a1HammerGlowTask = task;
                actorRenderComposeCoord(glowCoord);
                task->state = M4A1_HAMMER_GLOW_TASK_UPDATE;
                return;
            case M4A1_HAMMER_GLOW_TASK_UPDATE:
                // Publish the existing cache before composition; contact beams share its low halves.
                _gM4a1HammerAttachmentOrigin.vx = glowCoord->workm.t[0];
                _gM4a1HammerAttachmentOrigin.vy = glowCoord->workm.t[1];
                _gM4a1HammerAttachmentOrigin.vz = glowCoord->workm.t[2];
                switch (task->spawnArg1.value) {
                    case M4A1_HAMMER_GLOW_OFF:
                        break;
                    case M4A1_HAMMER_GLOW_IDLE:
                        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                            work->age = work->age - 1;
                            if ((work->age & 1) == 0) {
                                spriteQuadDrawCharge(glowCoord->workm.t, work->age >> 1, work->period,
                                                     work->angle);
                            }
                            return;
                        }
                        actorRenderComposeCoord(glowCoord);
                        if ((work->age & M4A1_HAMMER_SPIN_TICK_MASK) == 0) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            work->angle     = (gRandomLcgState >> 16) & M4A1_HAMMER_ANGLE_MASK;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->period    = ((gRandomLcgState >> 16) & 0xFF) + 0xC0;
                        if ((work->age & 1) == 0) {
                            spriteQuadDrawCharge(glowCoord->workm.t, work->age >> 1, work->period,
                                                 work->angle);
                        }
                        transientLight->framesLeft = M4A1_HAMMER_LIGHT_REFRESH_FRAMES;
                        pointLight->inner          = M4A1_HAMMER_IDLE_LIGHT_INNER;
                        pointLight->outer          = M4A1_HAMMER_IDLE_LIGHT_OUTER;
                        gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        pointLight->head.color.b   = ((gRandomLcgState >> 16) & 0x700) + 0x400;
                        pointLight->head.color.r   = (u16)pointLight->head.color.b >> 1;
                        pointLight->head.color.g   = (u16)pointLight->head.color.b >> 1;
                        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &glowCoord->workm, &lightCoord->coord);
                        lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->index              = 0;
                        return;
                    case M4A1_HAMMER_GLOW_CHARGED:
                        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                            work->age = work->age - 1;
                            if ((work->age & 1) == 0) {
                                spriteQuadDraw(glowCoord, work->age >> 1, work->period,
                                               work->angle);
                            }
                            return;
                        }
                        actorRenderComposeCoord(glowCoord);
                        if (work->index == 0) {
                            // Three eight-entry banks hold each spark's angle, height and radius.
                            for (sparkIndex = 0; sparkIndex < M4A1_HAMMER_SPARK_COUNT; sparkIndex++) {
                                gRandomLcgState                                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                _gM4a1HammerSparkState[sparkIndex]                                 = (sparkIndex << 9) + ((gRandomLcgState >> 16) & 0x1FF);
                                gRandomLcgState                                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                _gM4a1HammerSparkState[sparkIndex + M4A1_HAMMER_SPARK_HEIGHT_BASE] = ((gRandomLcgState >> 16) & 0x7FF) + 0x200;
                                gRandomLcgState                                                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                _gM4a1HammerSparkState[sparkIndex + M4A1_HAMMER_SPARK_RADIUS_BASE] = (gRandomLcgState >> 16) & 0x3FF;
                            }
                        }
                        if ((work->age & M4A1_HAMMER_SPIN_TICK_MASK) == 0) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            work->angle     = (gRandomLcgState >> 16) & M4A1_HAMMER_ANGLE_MASK;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->period    = ((gRandomLcgState >> 16) & 0x3FF) + 0x400;
                        if ((work->age & 1) == 0) {
                            spriteQuadDraw(glowCoord, work->age >> 1, work->period, work->angle);
                            for (sparkIndex = 0; sparkIndex < M4A1_HAMMER_SPARK_COUNT; sparkIndex++) {
                                heightIndex                          = sparkIndex + M4A1_HAMMER_SPARK_HEIGHT_BASE;
                                gRandomLcgState                      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                _gM4a1HammerSparkState[sparkIndex]  -= ((gRandomLcgState >> 16) & 0x1FF) - 0x100;
                                gRandomLcgState                      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                _gM4a1HammerSparkState[heightIndex] += (gRandomLcgState >> 16) & 0xFF;
                                work->pos.vx =
                                    (_gM4a1HammerSparkState[sparkIndex + M4A1_HAMMER_SPARK_RADIUS_BASE] * rsin(_gM4a1HammerSparkState[sparkIndex])) >> M4A1_HAMMER_TRIG_SHIFT;
                                work->pos.vz =
                                    (_gM4a1HammerSparkState[sparkIndex + M4A1_HAMMER_SPARK_RADIUS_BASE] * rcos(_gM4a1HammerSparkState[sparkIndex])) >> M4A1_HAMMER_TRIG_SHIFT;
                                work->pos.vy = _gM4a1HammerSparkState[heightIndex];
                                M4A1_HAMMER_COMPOSE_SPARK_POSITION(glowCoord, work->pos, _gM4a1HammerAttachmentOrigin);
                                _beamStripDraw(glowCoord, &work->pos, work->age, M4A1_HAMMER_BEAM_WIDTH_SCALE);
                            }
                        }
                        transientLight->framesLeft = M4A1_HAMMER_LIGHT_REFRESH_FRAMES;
                        pointLight->inner          = M4A1_HAMMER_CHARGED_LIGHT_INNER;
                        pointLight->outer          = M4A1_HAMMER_CHARGED_LIGHT_OUTER;
                        gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        pointLight->head.color.b   = ((gRandomLcgState >> 16) & 0x700) + 0x800;
                        pointLight->head.color.r   = (u16)pointLight->head.color.b >> 1;
                        pointLight->head.color.g   = pointLight->head.color.b >> 1;
                        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &glowCoord->workm, &lightCoord->coord);
                        lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->index              = work->index + 1;
                        if (work->index >= M4A1_HAMMER_CHARGED_TICKS) {
                            task->spawnArg1.value = M4A1_HAMMER_GLOW_IDLE;
                        }
                        return;
                    case M4A1_HAMMER_GLOW_RELEASE:
                        effectKillTask(work, task);
                        return;
                }
                return;
        }
    }
}

#undef M4A1_HAMMER_COMPOSE_SPARK_POSITION

#undef SPRITE_QUAD_POSITION_SOURCE_TYPE
#undef SPRITE_QUAD_POS
#undef SPRITE_QUAD_FRAME_T
#undef SPRITE_QUAD_SIZE_T
/// Read-only XYZ translation words from the charge sprite's `GfxCoord::workm.t`.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const long
#define SPRITE_QUAD_POS(p, i)            ((p)[i])
/// Unsigned texture-frame counter, masked to the charge sprite's eight cells.
#define SPRITE_QUAD_FRAME_T u16
/// Unsigned 16-bit perspective size for the charge flare, matching its forward declaration.
#define SPRITE_QUAD_SIZE_T u16
/// Defines the charge-flare instance. The six-cell strip below is unbound and becomes `spriteQuadDraw`.
#define SPRITE_QUAD_FUNC spriteQuadDrawCharge
/// Packed additive charge-flare page: 4-bit indexed texels at VRAM X=512 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 512, 0)
/// Charge-flare palette: VRAM X=192 words, Y=268 scanlines.
#define SPRITE_QUAD_CLUT getClut(192, 268)
/// Texel width and horizontal stride of each cell in the charge flare's eight-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 24
#define SPRITE_QUAD_CELL_MASK  7
/// Inclusive top texel row of the charge-flare strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x88
#define SPRITE_QUAD_V1    0x9F
/// Perspective-sizing multiplier for Hammer's charge flare.
///
/// Uses the cell's inclusive 23-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"
#undef SPRITE_QUAD_POSITION_SOURCE_TYPE
#undef SPRITE_QUAD_POS
#undef SPRITE_QUAD_FRAME_T
#undef SPRITE_QUAD_SIZE_T
/// Read-only coordinate source for the six-cell sprite's cached translation.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const GfxCoord
#define SPRITE_QUAD_POS(p, i)            ((p)->workm.t[i])
/// Restore the signed counter for the repeating six-cell sprite strip.
#define SPRITE_QUAD_FRAME_T s16
/// Restore signed 16-bit perspective sizing for the six-cell sprite strip.
#define SPRITE_QUAD_SIZE_T s16

void m4a1HammerImpactFlashTask(Task* task)
{
    enum {
        M4A1_HAMMER_IMPACT_TASK_ATTACH  = 0,
        M4A1_HAMMER_IMPACT_TASK_DRAW    = 1,
        M4A1_HAMMER_IMPACT_SPRITE_SIZE  = 0x400, // Perspective-sizing numerator
        M4A1_HAMMER_IMPACT_BEAM_END_AGE = 8,
        M4A1_HAMMER_IMPACT_LIFETIME     = 25,
        M4A1_HAMMER_IMPACT_ANGLE_MASK   = 0xFFF // 4096 angle units per turn
    };
    EffectWork* work;
    GfxCoord*   impactCoord;
    GfxCoord*   parent;

    work        = task->spawnArg2.pointer;
    impactCoord = task->extra.coordBody->coord;
    work->age++;
    switch (task->state) {
        case M4A1_HAMMER_IMPACT_TASK_ATTACH:
            // Teardown follows the attachment glow; the transform follows the contact.
            taskReparent(_gM4a1HammerGlowTask, task);
            if (task->spawnArg1.value != 0) {
                parent                    = work->parent;
                impactCoord->coord.t[0]   = 0;
                impactCoord->coord.t[1]   = 0;
                impactCoord->coord.t[2]   = 0;
                impactCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                impactCoord->parent       = parent;
                actorRenderComposeCoord(impactCoord);
                task->state = M4A1_HAMMER_IMPACT_TASK_DRAW;
            }
            work->scale     = 0x80;
            gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & M4A1_HAMMER_IMPACT_ANGLE_MASK;
            /* fallthrough */
        case M4A1_HAMMER_IMPACT_TASK_DRAW:
            if (work->age & 1) {
                spriteQuadDraw(impactCoord, ++work->index, M4A1_HAMMER_IMPACT_SPRITE_SIZE, work->angle);
                if (work->age < M4A1_HAMMER_IMPACT_BEAM_END_AGE) {
                    _beamStripDraw(impactCoord, &_gM4a1HammerAttachmentOrigin, work->index, M4A1_HAMMER_BEAM_WIDTH_SCALE);
                }
            }
            if (work->age >= M4A1_HAMMER_IMPACT_LIFETIME) {
                effectKillTask(work, task);
            }
            break;
    }
}

/// Texel width and horizontal stride of each cell in Hammer's repeating six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in Hammer's repeating sprite row, separate from its masked charge strip.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of Hammer's repeating strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
#define SPRITE_QUAD_V1    0x5F
/// Perspective-sizing multiplier for Hammer's repeating sprite strip.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
/// Repeating-strip palette: VRAM X=304 words, Y=266 scanlines.
#define SPRITE_QUAD_CLUT getClut(304, 266)
#include "../../shared/sprite_quad_draw.inc.c"

#include "../../shared/beam_strip_draw.inc.c"

/// Initial identity transform and self-parented root of the held weapon model.
///
/// Read-only asset tables retain their serialized positions in .data.
static const TmdBone _gM4a1HammerModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_model_01E10_skeleton.inc"
};

/// Vertex count of each skeletal part; the single weapon part contains all 70 vertices.
static const u32 _gM4a1HammerModelPartVerts[ARRAY_SIZE(_gM4a1HammerModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_model_01E10_partVerts.inc"
};

/// Held weapon vertices in integer coordinates local to its single part.
static SVECTOR _gM4a1HammerModelVerts[] = {
#include "assets/m4a1_hammer_model_01E10_verts.inc"
};

/// Held weapon lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gM4a1HammerModelNormals[] = {
#include "assets/m4a1_hammer_model_01E10_normals.inc"
};

/// Writable commands for two textured Gouraud triangles and 67 quads, then group and stream markers.
///
/// First model creation resolves the draw-handler slots in place.
static u32 _gM4a1HammerModelStream[] = {
#include "assets/m4a1_hammer_model_01E10_stream.inc"
};

TmdSource gM4a1HammerModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (67 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM4a1HammerModelSkeleton),
    .partVertexCounts    = _gM4a1HammerModelPartVerts,
    .verts               = _gM4a1HammerModelVerts,
    .normals             = _gM4a1HammerModelNormals,
    .skeleton            = _gM4a1HammerModelSkeleton,
    .stream              = _gM4a1HammerModelStream,
};

/// Translation and Euler keyframe poses for `_gM4a1HammerIdleAnimation`, in encoding-1 three-word entries.
static const AnimationPackedPose _gM4a1HammerIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0276C_bank1.inc"
};

/// Packed Euler keyframes for `_gM4a1HammerIdleAnimation`, in encoding-4 words with eight-unit angle steps.
static const AnimationPackedRotation _gM4a1HammerIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0276C_bank4.inc"
};

/// Keyframe durations, pose-word offsets and control records for `_gM4a1HammerIdleAnimation`.
static const AnimationRecord _gM4a1HammerIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0276C_records.inc"
};

/// Model-part track starts for `_gM4a1HammerIdleAnimation`, as absolute indices into its record array.
static const u16 _gM4a1HammerIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0276C_indices.inc"
};

/// Standing-idle pose for the Hammer player animation bank.
///
/// Selected at native-bank slots 1, 21, 22 and 41; each track repeats its held pose.
/// Records, indices and pose banks are read-only in their serialized .data
/// positions and must stay loaded while any playback slot refers to the clip.
static AnimationSet _gM4a1HammerIdleAnimation = {
    .records           = _gM4a1HammerIdleAnimationRecords,
    .trackStartIndices = _gM4a1HammerIdleAnimationIndices,
    .poseBanks         = { NULL, _gM4a1HammerIdleAnimationBank1, NULL, NULL, _gM4a1HammerIdleAnimationBank4, NULL, NULL, NULL },
};

/// Translation and Euler keyframe poses for `_gM4a1HammerUnarmedWalkAnimation`, in encoding-1 three-word entries.
static const AnimationPackedPose _gM4a1HammerUnarmedWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_02E10_bank1.inc"
};

/// Packed Euler keyframes for `_gM4a1HammerUnarmedWalkAnimation`, in encoding-4 words with eight-unit angle steps.
static const AnimationPackedRotation _gM4a1HammerUnarmedWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_02E10_bank4.inc"
};

/// Keyframe durations, pose-word offsets and control records for `_gM4a1HammerUnarmedWalkAnimation`.
static const AnimationRecord _gM4a1HammerUnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_02E10_records.inc"
};

/// Model-part track starts for `_gM4a1HammerUnarmedWalkAnimation`, as absolute indices into its record array.
static const u16 _gM4a1HammerUnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_02E10_indices.inc"
};

/// Forward walking clip while the held weapon model is absent.
///
/// Selected at native-bank slot 19; the Hammer bank still supplies the player pose.
/// Records, indices and pose banks are read-only in their serialized .data
/// positions and must stay loaded while any playback slot refers to the clip.
static AnimationSet _gM4a1HammerUnarmedWalkAnimation = {
    .records           = _gM4a1HammerUnarmedWalkAnimationRecords,
    .trackStartIndices = _gM4a1HammerUnarmedWalkAnimationIndices,
    .poseBanks         = { NULL, _gM4a1HammerUnarmedWalkAnimationBank1, NULL, NULL, _gM4a1HammerUnarmedWalkAnimationBank4, NULL, NULL, NULL },
};

/// Translation and Euler keyframe poses for `_gM4a1HammerTurnLeftAnimation`, in encoding-1 three-word entries.
static const AnimationPackedPose _gM4a1HammerTurnLeftAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_03670_bank1.inc"
};

/// Packed Euler keyframes for `_gM4a1HammerTurnLeftAnimation`, in encoding-4 words with eight-unit angle steps.
static const AnimationPackedRotation _gM4a1HammerTurnLeftAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_03670_bank4.inc"
};

/// Keyframe durations, pose-word offsets and control records for `_gM4a1HammerTurnLeftAnimation`.
static const AnimationRecord _gM4a1HammerTurnLeftAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_03670_records.inc"
};

/// Model-part track starts for `_gM4a1HammerTurnLeftAnimation`, as absolute indices into its record array.
static const u16 _gM4a1HammerTurnLeftAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_03670_indices.inc"
};

/// Stationary negative-yaw turn clip for the Hammer player animation bank.
///
/// Selected at native-bank slot 5 for negative manual and scripted turns.
/// Records, indices and pose banks are read-only in their serialized .data
/// positions and must stay loaded while any playback slot refers to the clip.
static AnimationSet _gM4a1HammerTurnLeftAnimation = {
    .records           = _gM4a1HammerTurnLeftAnimationRecords,
    .trackStartIndices = _gM4a1HammerTurnLeftAnimationIndices,
    .poseBanks         = { NULL, _gM4a1HammerTurnLeftAnimationBank1, NULL, NULL, _gM4a1HammerTurnLeftAnimationBank4, NULL, NULL, NULL },
};

/// Translation and Euler keyframe poses for `_gM4a1HammerTurnRightAnimation`, in encoding-1 three-word entries.
static const AnimationPackedPose _gM4a1HammerTurnRightAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_03ED4_bank1.inc"
};

/// Packed Euler keyframes for `_gM4a1HammerTurnRightAnimation`, in encoding-4 words with eight-unit angle steps.
static const AnimationPackedRotation _gM4a1HammerTurnRightAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_03ED4_bank4.inc"
};

/// Keyframe durations, pose-word offsets and control records for `_gM4a1HammerTurnRightAnimation`.
static const AnimationRecord _gM4a1HammerTurnRightAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_03ED4_records.inc"
};

/// Model-part track starts for `_gM4a1HammerTurnRightAnimation`, as absolute indices into its record array.
static const u16 _gM4a1HammerTurnRightAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_03ED4_indices.inc"
};

/// Stationary positive-yaw turn clip for the Hammer player animation bank.
///
/// Selected at native-bank slot 6 for positive manual and scripted turns.
/// Records, indices and pose banks are read-only in their serialized .data
/// positions and must stay loaded while any playback slot refers to the clip.
static AnimationSet _gM4a1HammerTurnRightAnimation = {
    .records           = _gM4a1HammerTurnRightAnimationRecords,
    .trackStartIndices = _gM4a1HammerTurnRightAnimationIndices,
    .poseBanks         = { NULL, _gM4a1HammerTurnRightAnimationBank1, NULL, NULL, _gM4a1HammerTurnRightAnimationBank4, NULL, NULL, NULL },
};

/// Encoding selectors for the Hammer clips' translation/rotation and rotation-only banks.
enum {
    M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Local translations and Euler angles for `_gM4a1HammerItemUseAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerItemUseAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_041E8_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerItemUseAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_041E8_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerItemUseAnimation`; durations count frames, pose offsets count words.
static const AnimationRecord _gM4a1HammerItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_041E8_records.inc"
};

/// Absolute record indices for `_gM4a1HammerItemUseAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_041E8_indices.inc"
};

/// Player item-use presentation clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 40, played with a six-frame entry blend before returning
/// to aim locomotion; the clip does not apply the item. Tracks 0..18 use
/// read-only, word-aligned pose banks and stop controls. The overlay and all
/// clip data must remain loaded while playback refers to them.
static AnimationSet _gM4a1HammerItemUseAnimation = {
    .records           = _gM4a1HammerItemUseAnimationRecords,
    .trackStartIndices = _gM4a1HammerItemUseAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerItemUseAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerItemUseAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerStairAscentAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerStairAscentAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_04944_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerStairAscentAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_04944_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for `_gM4a1HammerStairAscentAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words, while jump offsets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_04944_records.inc"
};

/// Absolute record indices for `_gM4a1HammerStairAscentAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_04944_indices.inc"
};

/// Looping player stair-ascent clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 36. Track 1's footstep cues gate scripted stair strides;
/// the final step switches to the finish clip selected by the flight's original
/// step-count parity. Tracks 0..18 borrow read-only records, indices and
/// word-aligned pose banks, which must remain loaded throughout playback.
static AnimationSet _gM4a1HammerStairAscentAnimation = {
    .records           = _gM4a1HammerStairAscentAnimationRecords,
    .trackStartIndices = _gM4a1HammerStairAscentAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerStairAscentAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerStairAscentAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerStairDescentAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerStairDescentAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_050CC_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerStairDescentAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_050CC_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for `_gM4a1HammerStairDescentAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte
/// words, while jump offsets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_050CC_records.inc"
};

/// Absolute record indices for `_gM4a1HammerStairDescentAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_050CC_indices.inc"
};

/// Looping player stair-descent clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 37. Track 1's footstep cues gate scripted stair strides;
/// descent finishes directly in the idle clip. Tracks 0..18 borrow read-only
/// records, indices and word-aligned pose banks, which must remain loaded
/// throughout playback.
static AnimationSet _gM4a1HammerStairDescentAnimation = {
    .records           = _gM4a1HammerStairDescentAnimationRecords,
    .trackStartIndices = _gM4a1HammerStairDescentAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerStairDescentAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerStairDescentAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerStairAscentFinishOddAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_053A0_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerStairAscentFinishOddAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_053A0_bank4.inc"
};

/// Keyframes, a track-1 footstep cue and stop controls for `_gM4a1HammerStairAscentFinishOddAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_053A0_records.inc"
};

/// Absolute record indices for `_gM4a1HammerStairAscentFinishOddAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_053A0_indices.inc"
};

/// Player stair-ascent finish clip for an odd-length flight with the M4A1 Hammer.
///
/// Native-bank slot 39, selected from the original step count's odd parity and
/// entered with a three-frame blend after the last upward stride. Tracks 0..18
/// borrow read-only records, indices and word-aligned pose banks, which must
/// remain loaded throughout playback.
static AnimationSet _gM4a1HammerStairAscentFinishOddAnimation = {
    .records           = _gM4a1HammerStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gM4a1HammerStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerStairAscentFinishOddAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerStairAscentFinishOddAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerStairAscentFinishEvenAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_05730_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerStairAscentFinishEvenAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_05730_bank4.inc"
};

/// Keyframes, a track-1 footstep cue and stop controls for `_gM4a1HammerStairAscentFinishEvenAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_05730_records.inc"
};

/// Absolute record indices for `_gM4a1HammerStairAscentFinishEvenAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_05730_indices.inc"
};

/// Player stair-ascent finish clip for an even-length flight with the M4A1 Hammer.
///
/// Native-bank slot 38, selected from the original step count's even parity and
/// entered with a three-frame blend after the last upward stride. Tracks 0..18
/// borrow read-only records, indices and word-aligned pose banks, which must
/// remain loaded throughout playback.
static AnimationSet _gM4a1HammerStairAscentFinishEvenAnimation = {
    .records           = _gM4a1HammerStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gM4a1HammerStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerStairAscentFinishEvenAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerStairAscentFinishEvenAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPeReleaseFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPeReleaseFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_05BB8_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPeReleaseFamily0Animation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPeReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_05BB8_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerPeReleaseFamily0Animation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerPeReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_05BB8_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPeReleaseFamily0Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPeReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_05BB8_indices.inc"
};

/// Family-0 Parasite Energy release clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 26, entered with a six-frame blend before the family-0
/// charge and cast clips. The attachment ID selects the family; this is the
/// release phase rather than the cast. Tracks 0..18 borrow read-only records,
/// indices and word-aligned pose banks, which must remain loaded throughout playback.
static AnimationSet _gM4a1HammerPeReleaseFamily0Animation = {
    .records           = _gM4a1HammerPeReleaseFamily0AnimationRecords,
    .trackStartIndices = _gM4a1HammerPeReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPeReleaseFamily0AnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPeReleaseFamily0AnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPeChargeFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPeChargeFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_05DB4_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPeChargeFamily0Animation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPeChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_05DB4_bank4.inc"
};

/// Keyframes and loop/stop controls for `_gM4a1HammerPeChargeFamily0Animation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
/// Loop targets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerPeChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_05DB4_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPeChargeFamily0Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPeChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_05DB4_indices.inc"
};

/// Family-0 Parasite Energy charge loop for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 27, restarted while the attachment countdown runs and
/// replaced by the family-0 cast clip when the countdown reaches zero.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerPeChargeFamily0Animation = {
    .records           = _gM4a1HammerPeChargeFamily0AnimationRecords,
    .trackStartIndices = _gM4a1HammerPeChargeFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPeChargeFamily0AnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPeChargeFamily0AnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPeCastFamily0Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPeCastFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0608C_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPeCastFamily0Animation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPeCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0608C_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerPeCastFamily0Animation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerPeCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0608C_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPeCastFamily0Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPeCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0608C_indices.inc"
};

/// Family-0 Parasite Energy cast clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 28, restarted when the attachment countdown reaches zero.
/// The player waits for clip completion before resuming locomotion or aim.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerPeCastFamily0Animation = {
    .records           = _gM4a1HammerPeCastFamily0AnimationRecords,
    .trackStartIndices = _gM4a1HammerPeCastFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPeCastFamily0AnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPeCastFamily0AnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPeReleaseFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPeReleaseFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_06330_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPeReleaseFamily1Animation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPeReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06330_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerPeReleaseFamily1Animation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerPeReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06330_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPeReleaseFamily1Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPeReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06330_indices.inc"
};

/// Family-1 Parasite Energy release clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 29, entered with a six-frame blend as attachment release
/// starts the countdown, before the family-1 charge and cast phases.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerPeReleaseFamily1Animation = {
    .records           = _gM4a1HammerPeReleaseFamily1AnimationRecords,
    .trackStartIndices = _gM4a1HammerPeReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPeReleaseFamily1AnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPeReleaseFamily1AnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPeChargeFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPeChargeFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_06530_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPeChargeFamily1Animation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPeChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06530_bank4.inc"
};

/// Keyframes and loop/stop controls for `_gM4a1HammerPeChargeFamily1Animation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
/// Loop targets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerPeChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06530_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPeChargeFamily1Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPeChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06530_indices.inc"
};

/// Family-1 Parasite Energy charge loop for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 30, restarted while the attachment countdown runs and
/// replaced by the family-1 cast clip when the countdown reaches zero.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerPeChargeFamily1Animation = {
    .records           = _gM4a1HammerPeChargeFamily1AnimationRecords,
    .trackStartIndices = _gM4a1HammerPeChargeFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPeChargeFamily1AnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPeChargeFamily1AnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPeCastFamily1Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPeCastFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_06884_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPeCastFamily1Animation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPeCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06884_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerPeCastFamily1Animation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerPeCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06884_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPeCastFamily1Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPeCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06884_indices.inc"
};

/// Family-1 Parasite Energy cast clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 31, restarted when the attachment countdown reaches zero.
/// The player waits for clip completion before resuming locomotion or aim.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerPeCastFamily1Animation = {
    .records           = _gM4a1HammerPeCastFamily1AnimationRecords,
    .trackStartIndices = _gM4a1HammerPeCastFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPeCastFamily1AnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPeCastFamily1AnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPart4HitReactionAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPart4HitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_06B38_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPart4HitReactionAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPart4HitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06B38_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerPart4HitReactionAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerPart4HitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06B38_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPart4HitReactionAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPart4HitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06B38_indices.inc"
};

/// Player hit-reaction clip for the collision body attached to model part 4.
///
/// Native-bank slot 16 in the equipped M4A1 Hammer bank. A recorded hit region
/// of 1 selects it with a three-frame entry blend on child slots 1..18; the
/// root slot is unchanged. Clip completion permits the damage state to recover.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerPart4HitReactionAnimation = {
    .records           = _gM4a1HammerPart4HitReactionAnimationRecords,
    .trackStartIndices = _gM4a1HammerPart4HitReactionAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPart4HitReactionAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPart4HitReactionAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerOtherBodyHitReactionAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerOtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_06E58_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerOtherBodyHitReactionAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerOtherBodyHitReactionAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06E58_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerOtherBodyHitReactionAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerOtherBodyHitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06E58_records.inc"
};

/// Absolute record indices for `_gM4a1HammerOtherBodyHitReactionAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerOtherBodyHitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_06E58_indices.inc"
};

/// Player hit-reaction clip for collision bodies other than model part 4.
///
/// Native-bank slot 17 in the equipped M4A1 Hammer bank. A recorded hit region
/// other than 1 selects it with a three-frame entry blend on child slots 1..18;
/// slot 0 is unchanged. Stop controls hold the final pose for damage recovery.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerOtherBodyHitReactionAnimation = {
    .records           = _gM4a1HammerOtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gM4a1HammerOtherBodyHitReactionAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerOtherBodyHitReactionAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerOtherBodyHitReactionAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerStoppedPoseAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerStoppedPoseAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0761C_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerStoppedPoseAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerStoppedPoseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0761C_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerStoppedPoseAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerStoppedPoseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0761C_records.inc"
};

/// Absolute record indices for `_gM4a1HammerStoppedPoseAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerStoppedPoseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0761C_indices.inc"
};

/// Stopped player pose used when HP reaches zero with M4A1 Hammer equipped.
///
/// Native-bank slot 18. Stopping disables movement and turning and resets child
/// slots 1..18, or blends them for the requested number of normal-rate frames.
/// Slot 0 is unchanged; stop controls hold the final pose.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerStoppedPoseAnimation = {
    .records           = _gM4a1HammerStoppedPoseAnimationRecords,
    .trackStartIndices = _gM4a1HammerStoppedPoseAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerStoppedPoseAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerStoppedPoseAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerIdleHighHpAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerIdleHighHpAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_088B4_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerIdleHighHpAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerIdleHighHpAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_088B4_bank4.inc"
};

/// Keyframes and loop and stop controls for `_gM4a1HammerIdleHighHpAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerIdleHighHpAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_088B4_records.inc"
};

/// Absolute record indices for `_gM4a1HammerIdleHighHpAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerIdleHighHpAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_088B4_indices.inc"
};

/// Looping player idle clip for HP above half maximum with M4A1 Hammer equipped.
///
/// Native-bank slot 23, selected after 300 direction-free locomotion ticks with
/// a five-frame entry blend on child slots 1..18. The half-maximum threshold
/// truncates down; equality selects the lower HP band. Slot 0 is unchanged.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerIdleHighHpAnimation = {
    .records           = _gM4a1HammerIdleHighHpAnimationRecords,
    .trackStartIndices = _gM4a1HammerIdleHighHpAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerIdleHighHpAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerIdleHighHpAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerIdleLowHpAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerIdleLowHpAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0942C_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerIdleLowHpAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerIdleLowHpAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0942C_bank4.inc"
};

/// Keyframes and loop and stop controls for `_gM4a1HammerIdleLowHpAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerIdleLowHpAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0942C_records.inc"
};

/// Absolute record indices for `_gM4a1HammerIdleLowHpAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerIdleLowHpAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0942C_indices.inc"
};

/// Looping player idle clip for HP at or below a quarter maximum with M4A1 Hammer equipped.
///
/// Native-bank slot 25, selected after 300 direction-free locomotion ticks with
/// a five-frame entry blend. The quarter-maximum threshold truncates down.
/// Paralysis also selects this clip with a six-frame blend regardless of HP.
/// Both uses play child slots 1..18 and leave slot 0 unchanged.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerIdleLowHpAnimation = {
    .records           = _gM4a1HammerIdleLowHpAnimationRecords,
    .trackStartIndices = _gM4a1HammerIdleLowHpAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerIdleLowHpAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerIdleLowHpAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerIdleMidHpAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerIdleMidHpAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_09B48_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerIdleMidHpAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerIdleMidHpAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_09B48_bank4.inc"
};

/// Keyframes and loop and stop controls for `_gM4a1HammerIdleMidHpAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerIdleMidHpAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_09B48_records.inc"
};

/// Absolute record indices for `_gM4a1HammerIdleMidHpAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerIdleMidHpAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_09B48_indices.inc"
};

/// Looping player idle clip for HP between a quarter and half maximum with M4A1 Hammer equipped.
///
/// Native-bank slot 24, selected after 300 direction-free locomotion ticks with
/// a five-frame entry blend on child slots 1..18. HP must exceed the truncated
/// quarter-maximum threshold and be at most the truncated half-maximum threshold.
/// Slot 0 is unchanged.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerIdleMidHpAnimation = {
    .records           = _gM4a1HammerIdleMidHpAnimationRecords,
    .trackStartIndices = _gM4a1HammerIdleMidHpAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerIdleMidHpAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerIdleMidHpAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerEventEntryAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerEventEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_09FC8_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerEventEntryAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerEventEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_09FC8_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerEventEntryAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerEventEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_09FC8_records.inc"
};

/// Absolute record indices for `_gM4a1HammerEventEntryAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerEventEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_09FC8_indices.inc"
};

/// Player event-entry presentation clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slots 32 and 34 share this clip. A zero presentation selector
/// starts slot 32 on child slots 1..18 before an event command; the event waits
/// for playback to finish. Slot 0 is unchanged; stop controls hold the last pose.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerEventEntryAnimation = {
    .records           = _gM4a1HammerEventEntryAnimationRecords,
    .trackStartIndices = _gM4a1HammerEventEntryAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerEventEntryAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerEventEntryAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerEventExitAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerEventExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0A1A0_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerEventExitAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerEventExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0A1A0_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerEventExitAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerEventExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0A1A0_records.inc"
};

/// Absolute record indices for `_gM4a1HammerEventExitAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerEventExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0A1A0_indices.inc"
};

/// Player event-exit presentation clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slots 33 and 35 share this clip. A nonzero presentation selector
/// starts slot 33 on child slots 1..18 after an event command; the event waits
/// for playback to finish. Slot 0 is unchanged; stop controls hold the last pose.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerEventExitAnimation = {
    .records           = _gM4a1HammerEventExitAnimationRecords,
    .trackStartIndices = _gM4a1HammerEventExitAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerEventExitAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerEventExitAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPeReleaseFamily2Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPeReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0A704_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPeReleaseFamily2Animation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPeReleaseFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0A704_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerPeReleaseFamily2Animation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerPeReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0A704_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPeReleaseFamily2Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPeReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0A704_indices.inc"
};

/// Family-2 Parasite Energy release clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 42, entered with a six-frame blend before the family-2
/// charge and cast clips. Non-item attachment IDs with decimal tens digit 3
/// select this family. Stop controls hold the final pose.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerPeReleaseFamily2Animation = {
    .records           = _gM4a1HammerPeReleaseFamily2AnimationRecords,
    .trackStartIndices = _gM4a1HammerPeReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPeReleaseFamily2AnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPeReleaseFamily2AnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPeChargeFamily2Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPeChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0A8F8_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPeChargeFamily2Animation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPeChargeFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0A8F8_bank4.inc"
};

/// Keyframes and loop/stop controls for `_gM4a1HammerPeChargeFamily2Animation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
/// Loop targets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerPeChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0A8F8_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPeChargeFamily2Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPeChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0A8F8_indices.inc"
};

/// Family-2 Parasite Energy charge hold loop for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 43, restarted while the attachment countdown runs and
/// replaced by the family-2 cast clip when the countdown reaches zero.
/// Non-item attachment IDs with decimal tens digit 3 select this family.
/// Each track loops back to its start to retain the charge pose.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerPeChargeFamily2Animation = {
    .records           = _gM4a1HammerPeChargeFamily2AnimationRecords,
    .trackStartIndices = _gM4a1HammerPeChargeFamily2AnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPeChargeFamily2AnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPeChargeFamily2AnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerPeCastFamily2Animation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerPeCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0AD78_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerPeCastFamily2Animation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerPeCastFamily2AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0AD78_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerPeCastFamily2Animation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerPeCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0AD78_records.inc"
};

/// Absolute record indices for `_gM4a1HammerPeCastFamily2Animation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerPeCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0AD78_indices.inc"
};

/// Family-2 Parasite Energy cast clip for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 44, restarted after the charge countdown reaches zero.
/// Non-item attachment IDs with decimal tens digit 3 select this family.
/// Playback completion resumes the previous player state; stop controls
/// hold the final pose. Tracks 0..18 borrow read-only records, indices and
/// word-aligned pose banks. Keep all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerPeCastFamily2Animation = {
    .records           = _gM4a1HammerPeCastFamily2AnimationRecords,
    .trackStartIndices = _gM4a1HammerPeCastFamily2AnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerPeCastFamily2AnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerPeCastFamily2AnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerAimIdleAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerAimIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0AF50_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerAimIdleAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerAimIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0AF50_bank4.inc"
};

/// Keyframes and loop/stop controls for `_gM4a1HammerAimIdleAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
/// Loop targets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerAimIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0AF50_records.inc"
};

/// Absolute record indices for `_gM4a1HammerAimIdleAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerAimIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0AF50_indices.inc"
};

/// Player aim-idle hold loop for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 9, selected while aiming without movement or turning.
/// Also supplies the ready pose before either weapon attack. Child slots
/// 1..18 retain their poses through track-start loops; slot 0 is unchanged.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded while playback or another
/// animation table retains this clip.
static AnimationSet _gM4a1HammerAimIdleAnimation = {
    .records           = _gM4a1HammerAimIdleAnimationRecords,
    .trackStartIndices = _gM4a1HammerAimIdleAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerAimIdleAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerAimIdleAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerAimBackwardOrTurnAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerAimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0B790_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerAimBackwardOrTurnAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerAimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0B790_bank4.inc"
};

/// Keyframes and loop/stop controls for `_gM4a1HammerAimBackwardOrTurnAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
/// Loop targets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerAimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0B790_records.inc"
};

/// Absolute record indices for `_gM4a1HammerAimBackwardOrTurnAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerAimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0B790_indices.inc"
};

/// Player aim-backward and stationary-turn loop for the equipped M4A1 Hammer bank.
///
/// Native-bank slot 13, selected for backward movement or a stationary turn
/// while aiming. Child slots 1..18 loop through their tracks; slot 0 is unchanged.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerAimBackwardOrTurnAnimation = {
    .records           = _gM4a1HammerAimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gM4a1HammerAimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerAimBackwardOrTurnAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerAimBackwardOrTurnAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerAimForwardAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerAimForwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0C238_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerAimForwardAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerAimForwardAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0C238_bank4.inc"
};

/// Keyframes, footstep cues and loop/stop controls for `_gM4a1HammerAimForwardAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
/// Loop targets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerAimForwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0C238_records.inc"
};

/// Absolute record indices for `_gM4a1HammerAimForwardAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerAimForwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0C238_indices.inc"
};

/// Player forward-walk loop while aiming with the M4A1 Hammer.
///
/// Native-bank slot 12. Child tracks 1..18 enter repeating cycles; slot 1
/// supplies alternating footstep cues. Normal playback leaves slot 0 unchanged.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerAimForwardAnimation = {
    .records           = _gM4a1HammerAimForwardAnimationRecords,
    .trackStartIndices = _gM4a1HammerAimForwardAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerAimForwardAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerAimForwardAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerReloadAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0D0D4_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerReloadAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerReloadAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0D0D4_bank4.inc"
};

/// Keyframes, reload cues and stop controls for `_gM4a1HammerReloadAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
/// Slot 1's combined cue bits request load application and its reload sound.
static const AnimationRecord _gM4a1HammerReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0D0D4_records.inc"
};

/// Absolute record indices for `_gM4a1HammerReloadAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0D0D4_indices.inc"
};

/// Player reload clip shared by both M4A1 Hammer loads and battle-end reloading.
///
/// Native-bank slots 14, 15 and 20 share this set. Every track ends in a pose hold;
/// normal playback selects child slots 1..18, leaving slot 0 unchanged.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerReloadAnimation = {
    .records           = _gM4a1HammerReloadAnimationRecords,
    .trackStartIndices = _gM4a1HammerReloadAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerReloadAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerReloadAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerRifleFireAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerRifleFireAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0D578_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerRifleFireAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerRifleFireAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0D578_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerRifleFireAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerRifleFireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0D578_records.inc"
};

/// Absolute record indices for `_gM4a1HammerRifleFireAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerRifleFireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0D578_indices.inc"
};

/// Player rifle-shot clip for the primary M4A1 Hammer attack.
///
/// Native-bank slot 10, restarted for each burst round with a two-frame pose blend.
/// The attack controller times the shots; every track ends in a pose hold.
/// Normal playback selects child slots 1..18, leaving slot 0 unchanged.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerRifleFireAnimation = {
    .records           = _gM4a1HammerRifleFireAnimationRecords,
    .trackStartIndices = _gM4a1HammerRifleFireAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerRifleFireAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerRifleFireAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerDischargeAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerDischargeAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0DC5C_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerDischargeAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerDischargeAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0DC5C_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerDischargeAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerDischargeAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0DC5C_records.inc"
};

/// Absolute record indices for `_gM4a1HammerDischargeAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerDischargeAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0DC5C_indices.inc"
};

/// Player discharge clip for the M4A1 Hammer attachment's secondary attack.
///
/// Native-bank slot 11, started with a two-frame pose blend when charging begins.
/// The attack controller times the contact window; every track ends in a pose hold.
/// Normal playback selects child slots 1..18, leaving slot 0 unchanged.
/// Tracks 0..18 borrow read-only records, indices and word-aligned pose banks.
/// Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerDischargeAnimation = {
    .records           = _gM4a1HammerDischargeAnimationRecords,
    .trackStartIndices = _gM4a1HammerDischargeAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerDischargeAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerDischargeAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerAimEntryAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerAimEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0E368_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerAimEntryAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerAimEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0E368_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerAimEntryAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerAimEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0E368_records.inc"
};

/// Absolute record indices for `_gM4a1HammerAimEntryAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerAimEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0E368_indices.inc"
};

/// Player transition into the M4A1 Hammer aiming pose.
///
/// Native-bank slot 7, also borrowed as the player's recovery clip after enemy grabs.
/// Every track ends in a pose hold. Normal playback selects child slots 1..18,
/// leaving slot 0 unchanged. Tracks 0..18 borrow read-only records, indices and
/// word-aligned pose banks. Keep the overlay and all clip data loaded while
/// playback or another animation table retains this clip.
static AnimationSet _gM4a1HammerAimEntryAnimation = {
    .records           = _gM4a1HammerAimEntryAnimationRecords,
    .trackStartIndices = _gM4a1HammerAimEntryAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerAimEntryAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerAimEntryAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerAimExitAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerAimExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0E8E0_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerAimExitAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerAimExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0E8E0_bank4.inc"
};

/// Keyframes and stop controls for `_gM4a1HammerAimExitAnimation`.
///
/// Keyframe durations count normal-rate frames; pose offsets count four-byte words.
static const AnimationRecord _gM4a1HammerAimExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0E8E0_records.inc"
};

/// Absolute record indices for `_gM4a1HammerAimExitAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerAimExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0E8E0_indices.inc"
};

/// Player transition out of the M4A1 Hammer aiming pose.
///
/// Native-bank slot 8, including the exit from scripted firing to normal locomotion.
/// Every track ends in a pose hold. Normal playback selects child slots 1..18,
/// leaving slot 0 unchanged. Tracks 0..18 borrow read-only records, indices and
/// word-aligned pose banks. Keep the overlay and all clip data loaded throughout playback.
static AnimationSet _gM4a1HammerAimExitAnimation = {
    .records           = _gM4a1HammerAimExitAnimationRecords,
    .trackStartIndices = _gM4a1HammerAimExitAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerAimExitAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerAimExitAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0F198_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerWalkAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0F198_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for `_gM4a1HammerWalkAnimation`.
///
/// Durations count normal-rate frames, pose offsets count four-byte words,
/// and jump targets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0F198_records.inc"
};

/// Absolute record indices for `_gM4a1HammerWalkAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0F198_indices.inc"
};

/// Forward-walk clip with the M4A1 Hammer weapon model present.
///
/// Native-bank slot 2; walking without the weapon model selects slot 19 instead.
/// All 19 tracks loop; track 1 supplies the paired footstep cues. Normal
/// playback selects child slots 1..18, leaving slot 0 unchanged. Records,
/// indices and word-aligned pose banks are read-only and borrowed for playback.
/// Keep the overlay and all clip data loaded while any slot refers to them.
static AnimationSet _gM4a1HammerWalkAnimation = {
    .records           = _gM4a1HammerWalkAnimationRecords,
    .trackStartIndices = _gM4a1HammerWalkAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerWalkAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerWalkAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerBackwardWalkAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerBackwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_0F938_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerBackwardWalkAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerBackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0F938_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for `_gM4a1HammerBackwardWalkAnimation`.
///
/// Durations count normal-rate frames, pose offsets count four-byte words,
/// and jump targets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerBackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0F938_records.inc"
};

/// Absolute record indices for `_gM4a1HammerBackwardWalkAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerBackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_0F938_indices.inc"
};

/// Backward-walk clip for the equipped M4A1 Hammer player bank.
///
/// Native-bank slot 3, selected for backward ordinary locomotion.
/// All 19 tracks loop; track 1 supplies the paired footstep cues. Normal
/// playback selects child slots 1..18, leaving slot 0 unchanged. Records,
/// indices and word-aligned pose banks are read-only and borrowed for playback.
/// Keep the overlay and all clip data loaded while any slot refers to them.
static AnimationSet _gM4a1HammerBackwardWalkAnimation = {
    .records           = _gM4a1HammerBackwardWalkAnimationRecords,
    .trackStartIndices = _gM4a1HammerBackwardWalkAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerBackwardWalkAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerBackwardWalkAnimationBank4,
    },
};

/// Local translations and Euler angles for `_gM4a1HammerRunAnimation`, in model units and 1/4096 turns.
static const AnimationPackedPose _gM4a1HammerRunAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_hammer_animation_1030C_bank1.inc"
};

/// Packed Euler angles for `_gM4a1HammerRunAnimation`, in eight-unit steps of a 4096-unit turn.
static const AnimationPackedRotation _gM4a1HammerRunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_1030C_bank4.inc"
};

/// Keyframes, footstep cues and loop controls for `_gM4a1HammerRunAnimation`.
///
/// Durations count normal-rate frames, pose offsets count four-byte words,
/// and jump targets are absolute indices in this record array.
static const AnimationRecord _gM4a1HammerRunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_1030C_records.inc"
};

/// Absolute record indices for `_gM4a1HammerRunAnimation`'s model-part tracks 0..18.
///
/// The final serialized halfword is retained but is not a twentieth track.
static const u16 _gM4a1HammerRunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_hammer_animation_1030C_indices.inc"
};

/// Running clip for the equipped M4A1 Hammer player bank.
///
/// Native-bank slot 4, shared by ordinary running and default scripted run-to.
/// All 19 tracks loop; track 1 supplies the paired footstep cues. Normal
/// playback selects child slots 1..18, leaving slot 0 unchanged. Records,
/// indices and word-aligned pose banks are read-only and borrowed for playback.
/// Keep the overlay and all clip data loaded while any slot refers to them.
static AnimationSet _gM4a1HammerRunAnimation = {
    .records           = _gM4a1HammerRunAnimationRecords,
    .trackStartIndices = _gM4a1HammerRunAnimationIndices,
    .poseBanks         = {
        [M4A1_HAMMER_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1HammerRunAnimationBank1,
        [M4A1_HAMMER_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1HammerRunAnimationBank4,
    },
};

AnimationBank gM4a1HammerPlayerAnimationBank = {
    .table = {
        .sets = {
            NULL,
            &_gM4a1HammerIdleAnimation,
            &_gM4a1HammerWalkAnimation,
            &_gM4a1HammerBackwardWalkAnimation,
            &_gM4a1HammerRunAnimation,
            &_gM4a1HammerTurnLeftAnimation,
            &_gM4a1HammerTurnRightAnimation,
            &_gM4a1HammerAimEntryAnimation,
            &_gM4a1HammerAimExitAnimation,
            &_gM4a1HammerAimIdleAnimation,
            &_gM4a1HammerRifleFireAnimation,
            &_gM4a1HammerDischargeAnimation,
            &_gM4a1HammerAimForwardAnimation,
            &_gM4a1HammerAimBackwardOrTurnAnimation,
            &_gM4a1HammerReloadAnimation,
            &_gM4a1HammerReloadAnimation,
            &_gM4a1HammerPart4HitReactionAnimation,
            &_gM4a1HammerOtherBodyHitReactionAnimation,
            &_gM4a1HammerStoppedPoseAnimation,
            &_gM4a1HammerUnarmedWalkAnimation,
            &_gM4a1HammerReloadAnimation,
            &_gM4a1HammerIdleAnimation,
            &_gM4a1HammerIdleAnimation,
            &_gM4a1HammerIdleHighHpAnimation,
            &_gM4a1HammerIdleMidHpAnimation,
            &_gM4a1HammerIdleLowHpAnimation,
            &_gM4a1HammerPeReleaseFamily0Animation,
            &_gM4a1HammerPeChargeFamily0Animation,
            &_gM4a1HammerPeCastFamily0Animation,
            &_gM4a1HammerPeReleaseFamily1Animation,
            &_gM4a1HammerPeChargeFamily1Animation,
            &_gM4a1HammerPeCastFamily1Animation,
            &_gM4a1HammerEventEntryAnimation,
            &_gM4a1HammerEventExitAnimation,
            &_gM4a1HammerEventEntryAnimation,
            &_gM4a1HammerEventExitAnimation,
            &_gM4a1HammerStairAscentAnimation,
            &_gM4a1HammerStairDescentAnimation,
            &_gM4a1HammerStairAscentFinishEvenAnimation,
            &_gM4a1HammerStairAscentFinishOddAnimation,
            &_gM4a1HammerItemUseAnimation,
            &_gM4a1HammerIdleAnimation,
            &_gM4a1HammerPeReleaseFamily2Animation,
            &_gM4a1HammerPeChargeFamily2Animation,
            &_gM4a1HammerPeCastFamily2Animation,
            NULL,
            NULL,
            // The writable extension is initially zero.
        },
    },
};

/// Retained cylindrical offsets of eight sparks, reseeded at the start of a charged burst.
///
/// Bank starts are 0 for angles, `M4A1_HAMMER_SPARK_HEIGHT_BASE` for heights
/// and `M4A1_HAMMER_SPARK_RADIUS_BASE` for radii; each has `M4A1_HAMMER_SPARK_COUNT`
/// entries. Angles use 4096 units per turn without normalization. Heights and
/// radii use weapon-local integer coordinates; radii stay fixed at 0..1023.
/// Every other active age tick drifts angles and raises heights, retaining
/// signed-halfword wrap on each store. Storage lasts for the loaded overlay.
static s16 _gM4a1HammerSparkState[M4A1_HAMMER_SPARK_COUNT * 3] = { 0 };

/// Borrowed live attachment glow task that owns contact flashes' teardown.
///
/// Published when the glow attaches. Contact spawning requires it to remain
/// live; releasing the glow does not clear this retained pointer.
static Task* _gM4a1HammerGlowTask = NULL;

/// Retained zero-initialized word with no known accesses; its role is unproven.
static s32 s_unused_8012D664 = 0;

/// Cached attachment origin shared by charged sparks and contact beams.
///
/// Signed low halfwords of the glow's composed translation, published before
/// that update recomposes it. Uses composition-root space, including the view
/// transform; the beam drawer projects it through `GsWSMATRIX`. Storage lasts
/// for the loaded Hammer overlay; updates preserve the fourth halfword.
static SVECTOR _gM4a1HammerAttachmentOrigin = { 0, 0, 0, 0 };
