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

static AnimationPackedPose _gM4a1HammerAnimation05DB4Bank1[3] = {
#include "assets/m4a1_hammer_animation_05DB4_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation05DB4Bank4[22] = {
#include "assets/m4a1_hammer_animation_05DB4_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation05DB4Records[76] = {
#include "assets/m4a1_hammer_animation_05DB4_records.inc"
};

static u16 _gM4a1HammerAnimation05DB4Indices[20] = {
#include "assets/m4a1_hammer_animation_05DB4_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation05DB4 = {
    _gM4a1HammerAnimation05DB4Records,
    _gM4a1HammerAnimation05DB4Indices,
    { NULL, _gM4a1HammerAnimation05DB4Bank1, NULL, NULL, _gM4a1HammerAnimation05DB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0608CBank1[6] = {
#include "assets/m4a1_hammer_animation_0608C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0608CBank4[57] = {
#include "assets/m4a1_hammer_animation_0608C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0608CRecords[87] = {
#include "assets/m4a1_hammer_animation_0608C_records.inc"
};

static u16 _gM4a1HammerAnimation0608CIndices[20] = {
#include "assets/m4a1_hammer_animation_0608C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0608C = {
    _gM4a1HammerAnimation0608CRecords,
    _gM4a1HammerAnimation0608CIndices,
    { NULL, _gM4a1HammerAnimation0608CBank1, NULL, NULL, _gM4a1HammerAnimation0608CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06330Bank1[4] = {
#include "assets/m4a1_hammer_animation_06330_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06330Bank4[55] = {
#include "assets/m4a1_hammer_animation_06330_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06330Records[82] = {
#include "assets/m4a1_hammer_animation_06330_records.inc"
};

static u16 _gM4a1HammerAnimation06330Indices[20] = {
#include "assets/m4a1_hammer_animation_06330_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06330 = {
    _gM4a1HammerAnimation06330Records,
    _gM4a1HammerAnimation06330Indices,
    { NULL, _gM4a1HammerAnimation06330Bank1, NULL, NULL, _gM4a1HammerAnimation06330Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06530Bank1[3] = {
#include "assets/m4a1_hammer_animation_06530_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06530Bank4[23] = {
#include "assets/m4a1_hammer_animation_06530_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06530Records[76] = {
#include "assets/m4a1_hammer_animation_06530_records.inc"
};

static u16 _gM4a1HammerAnimation06530Indices[20] = {
#include "assets/m4a1_hammer_animation_06530_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06530 = {
    _gM4a1HammerAnimation06530Records,
    _gM4a1HammerAnimation06530Indices,
    { NULL, _gM4a1HammerAnimation06530Bank1, NULL, NULL, _gM4a1HammerAnimation06530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06884Bank1[8] = {
#include "assets/m4a1_hammer_animation_06884_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06884Bank4[68] = {
#include "assets/m4a1_hammer_animation_06884_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06884Records[101] = {
#include "assets/m4a1_hammer_animation_06884_records.inc"
};

static u16 _gM4a1HammerAnimation06884Indices[20] = {
#include "assets/m4a1_hammer_animation_06884_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06884 = {
    _gM4a1HammerAnimation06884Records,
    _gM4a1HammerAnimation06884Indices,
    { NULL, _gM4a1HammerAnimation06884Bank1, NULL, NULL, _gM4a1HammerAnimation06884Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06B38Bank1[5] = {
#include "assets/m4a1_hammer_animation_06B38_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06B38Bank4[55] = {
#include "assets/m4a1_hammer_animation_06B38_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06B38Records[83] = {
#include "assets/m4a1_hammer_animation_06B38_records.inc"
};

static u16 _gM4a1HammerAnimation06B38Indices[20] = {
#include "assets/m4a1_hammer_animation_06B38_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06B38 = {
    _gM4a1HammerAnimation06B38Records,
    _gM4a1HammerAnimation06B38Indices,
    { NULL, _gM4a1HammerAnimation06B38Bank1, NULL, NULL, _gM4a1HammerAnimation06B38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation06E58Bank1[6] = {
#include "assets/m4a1_hammer_animation_06E58_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation06E58Bank4[66] = {
#include "assets/m4a1_hammer_animation_06E58_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation06E58Records[96] = {
#include "assets/m4a1_hammer_animation_06E58_records.inc"
};

static u16 _gM4a1HammerAnimation06E58Indices[20] = {
#include "assets/m4a1_hammer_animation_06E58_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation06E58 = {
    _gM4a1HammerAnimation06E58Records,
    _gM4a1HammerAnimation06E58Indices,
    { NULL, _gM4a1HammerAnimation06E58Bank1, NULL, NULL, _gM4a1HammerAnimation06E58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0761CBank1[18] = {
#include "assets/m4a1_hammer_animation_0761C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0761CBank4[184] = {
#include "assets/m4a1_hammer_animation_0761C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0761CRecords[239] = {
#include "assets/m4a1_hammer_animation_0761C_records.inc"
};

static u16 _gM4a1HammerAnimation0761CIndices[20] = {
#include "assets/m4a1_hammer_animation_0761C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0761C = {
    _gM4a1HammerAnimation0761CRecords,
    _gM4a1HammerAnimation0761CIndices,
    { NULL, _gM4a1HammerAnimation0761CBank1, NULL, NULL, _gM4a1HammerAnimation0761CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation088B4Bank1[29] = {
#include "assets/m4a1_hammer_animation_088B4_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation088B4Bank4[450] = {
#include "assets/m4a1_hammer_animation_088B4_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation088B4Records[633] = {
#include "assets/m4a1_hammer_animation_088B4_records.inc"
};

static u16 _gM4a1HammerAnimation088B4Indices[20] = {
#include "assets/m4a1_hammer_animation_088B4_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation088B4 = {
    _gM4a1HammerAnimation088B4Records,
    _gM4a1HammerAnimation088B4Indices,
    { NULL, _gM4a1HammerAnimation088B4Bank1, NULL, NULL, _gM4a1HammerAnimation088B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0942CBank1[12] = {
#include "assets/m4a1_hammer_animation_0942C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0942CBank4[266] = {
#include "assets/m4a1_hammer_animation_0942C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0942CRecords[412] = {
#include "assets/m4a1_hammer_animation_0942C_records.inc"
};

static u16 _gM4a1HammerAnimation0942CIndices[20] = {
#include "assets/m4a1_hammer_animation_0942C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0942C = {
    _gM4a1HammerAnimation0942CRecords,
    _gM4a1HammerAnimation0942CIndices,
    { NULL, _gM4a1HammerAnimation0942CBank1, NULL, NULL, _gM4a1HammerAnimation0942CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation09B48Bank1[9] = {
#include "assets/m4a1_hammer_animation_09B48_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation09B48Bank4[144] = {
#include "assets/m4a1_hammer_animation_09B48_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation09B48Records[264] = {
#include "assets/m4a1_hammer_animation_09B48_records.inc"
};

static u16 _gM4a1HammerAnimation09B48Indices[20] = {
#include "assets/m4a1_hammer_animation_09B48_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation09B48 = {
    _gM4a1HammerAnimation09B48Records,
    _gM4a1HammerAnimation09B48Indices,
    { NULL, _gM4a1HammerAnimation09B48Bank1, NULL, NULL, _gM4a1HammerAnimation09B48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation09FC8Bank1[6] = {
#include "assets/m4a1_hammer_animation_09FC8_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation09FC8Bank4[107] = {
#include "assets/m4a1_hammer_animation_09FC8_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation09FC8Records[143] = {
#include "assets/m4a1_hammer_animation_09FC8_records.inc"
};

static u16 _gM4a1HammerAnimation09FC8Indices[20] = {
#include "assets/m4a1_hammer_animation_09FC8_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation09FC8 = {
    _gM4a1HammerAnimation09FC8Records,
    _gM4a1HammerAnimation09FC8Indices,
    { NULL, _gM4a1HammerAnimation09FC8Bank1, NULL, NULL, _gM4a1HammerAnimation09FC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0A1A0Bank1[3] = {
#include "assets/m4a1_hammer_animation_0A1A0_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0A1A0Bank4[32] = {
#include "assets/m4a1_hammer_animation_0A1A0_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0A1A0Records[57] = {
#include "assets/m4a1_hammer_animation_0A1A0_records.inc"
};

static u16 _gM4a1HammerAnimation0A1A0Indices[20] = {
#include "assets/m4a1_hammer_animation_0A1A0_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0A1A0 = {
    _gM4a1HammerAnimation0A1A0Records,
    _gM4a1HammerAnimation0A1A0Indices,
    { NULL, _gM4a1HammerAnimation0A1A0Bank1, NULL, NULL, _gM4a1HammerAnimation0A1A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0A704Bank1[11] = {
#include "assets/m4a1_hammer_animation_0A704_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0A704Bank4[125] = {
#include "assets/m4a1_hammer_animation_0A704_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0A704Records[167] = {
#include "assets/m4a1_hammer_animation_0A704_records.inc"
};

static u16 _gM4a1HammerAnimation0A704Indices[20] = {
#include "assets/m4a1_hammer_animation_0A704_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0A704 = {
    _gM4a1HammerAnimation0A704Records,
    _gM4a1HammerAnimation0A704Indices,
    { NULL, _gM4a1HammerAnimation0A704Bank1, NULL, NULL, _gM4a1HammerAnimation0A704Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0A8F8Bank1[3] = {
#include "assets/m4a1_hammer_animation_0A8F8_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0A8F8Bank4[20] = {
#include "assets/m4a1_hammer_animation_0A8F8_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0A8F8Records[76] = {
#include "assets/m4a1_hammer_animation_0A8F8_records.inc"
};

static u16 _gM4a1HammerAnimation0A8F8Indices[20] = {
#include "assets/m4a1_hammer_animation_0A8F8_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0A8F8 = {
    _gM4a1HammerAnimation0A8F8Records,
    _gM4a1HammerAnimation0A8F8Indices,
    { NULL, _gM4a1HammerAnimation0A8F8Bank1, NULL, NULL, _gM4a1HammerAnimation0A8F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0AD78Bank1[8] = {
#include "assets/m4a1_hammer_animation_0AD78_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0AD78Bank4[105] = {
#include "assets/m4a1_hammer_animation_0AD78_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0AD78Records[139] = {
#include "assets/m4a1_hammer_animation_0AD78_records.inc"
};

static u16 _gM4a1HammerAnimation0AD78Indices[20] = {
#include "assets/m4a1_hammer_animation_0AD78_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0AD78 = {
    _gM4a1HammerAnimation0AD78Records,
    _gM4a1HammerAnimation0AD78Indices,
    { NULL, _gM4a1HammerAnimation0AD78Bank1, NULL, NULL, _gM4a1HammerAnimation0AD78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0AF50Bank1[2] = {
#include "assets/m4a1_hammer_animation_0AF50_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0AF50Bank4[16] = {
#include "assets/m4a1_hammer_animation_0AF50_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0AF50Records[76] = {
#include "assets/m4a1_hammer_animation_0AF50_records.inc"
};

static u16 _gM4a1HammerAnimation0AF50Indices[20] = {
#include "assets/m4a1_hammer_animation_0AF50_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0AF50 = {
    _gM4a1HammerAnimation0AF50Records,
    _gM4a1HammerAnimation0AF50Indices,
    { NULL, _gM4a1HammerAnimation0AF50Bank1, NULL, NULL, _gM4a1HammerAnimation0AF50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0B790Bank1[14] = {
#include "assets/m4a1_hammer_animation_0B790_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0B790Bank4[194] = {
#include "assets/m4a1_hammer_animation_0B790_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0B790Records[272] = {
#include "assets/m4a1_hammer_animation_0B790_records.inc"
};

static u16 _gM4a1HammerAnimation0B790Indices[20] = {
#include "assets/m4a1_hammer_animation_0B790_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0B790 = {
    _gM4a1HammerAnimation0B790Records,
    _gM4a1HammerAnimation0B790Indices,
    { NULL, _gM4a1HammerAnimation0B790Bank1, NULL, NULL, _gM4a1HammerAnimation0B790Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0C238Bank1[19] = {
#include "assets/m4a1_hammer_animation_0C238_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0C238Bank4[269] = {
#include "assets/m4a1_hammer_animation_0C238_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0C238Records[336] = {
#include "assets/m4a1_hammer_animation_0C238_records.inc"
};

static u16 _gM4a1HammerAnimation0C238Indices[20] = {
#include "assets/m4a1_hammer_animation_0C238_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0C238 = {
    _gM4a1HammerAnimation0C238Records,
    _gM4a1HammerAnimation0C238Indices,
    { NULL, _gM4a1HammerAnimation0C238Bank1, NULL, NULL, _gM4a1HammerAnimation0C238Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0D0D4Bank1[24] = {
#include "assets/m4a1_hammer_animation_0D0D4_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0D0D4Bank4[390] = {
#include "assets/m4a1_hammer_animation_0D0D4_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0D0D4Records[453] = {
#include "assets/m4a1_hammer_animation_0D0D4_records.inc"
};

static u16 _gM4a1HammerAnimation0D0D4Indices[20] = {
#include "assets/m4a1_hammer_animation_0D0D4_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0D0D4 = {
    _gM4a1HammerAnimation0D0D4Records,
    _gM4a1HammerAnimation0D0D4Indices,
    { NULL, _gM4a1HammerAnimation0D0D4Bank1, NULL, NULL, _gM4a1HammerAnimation0D0D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0D578Bank1[8] = {
#include "assets/m4a1_hammer_animation_0D578_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0D578Bank4[102] = {
#include "assets/m4a1_hammer_animation_0D578_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0D578Records[151] = {
#include "assets/m4a1_hammer_animation_0D578_records.inc"
};

static u16 _gM4a1HammerAnimation0D578Indices[20] = {
#include "assets/m4a1_hammer_animation_0D578_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0D578 = {
    _gM4a1HammerAnimation0D578Records,
    _gM4a1HammerAnimation0D578Indices,
    { NULL, _gM4a1HammerAnimation0D578Bank1, NULL, NULL, _gM4a1HammerAnimation0D578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0DC5CBank1[13] = {
#include "assets/m4a1_hammer_animation_0DC5C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0DC5CBank4[170] = {
#include "assets/m4a1_hammer_animation_0DC5C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0DC5CRecords[212] = {
#include "assets/m4a1_hammer_animation_0DC5C_records.inc"
};

static u16 _gM4a1HammerAnimation0DC5CIndices[20] = {
#include "assets/m4a1_hammer_animation_0DC5C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0DC5C = {
    _gM4a1HammerAnimation0DC5CRecords,
    _gM4a1HammerAnimation0DC5CIndices,
    { NULL, _gM4a1HammerAnimation0DC5CBank1, NULL, NULL, _gM4a1HammerAnimation0DC5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0E368Bank1[13] = {
#include "assets/m4a1_hammer_animation_0E368_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0E368Bank4[175] = {
#include "assets/m4a1_hammer_animation_0E368_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0E368Records[217] = {
#include "assets/m4a1_hammer_animation_0E368_records.inc"
};

static u16 _gM4a1HammerAnimation0E368Indices[20] = {
#include "assets/m4a1_hammer_animation_0E368_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0E368 = {
    _gM4a1HammerAnimation0E368Records,
    _gM4a1HammerAnimation0E368Indices,
    { NULL, _gM4a1HammerAnimation0E368Bank1, NULL, NULL, _gM4a1HammerAnimation0E368Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0E8E0Bank1[10] = {
#include "assets/m4a1_hammer_animation_0E8E0_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0E8E0Bank4[131] = {
#include "assets/m4a1_hammer_animation_0E8E0_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0E8E0Records[169] = {
#include "assets/m4a1_hammer_animation_0E8E0_records.inc"
};

static u16 _gM4a1HammerAnimation0E8E0Indices[20] = {
#include "assets/m4a1_hammer_animation_0E8E0_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0E8E0 = {
    _gM4a1HammerAnimation0E8E0Records,
    _gM4a1HammerAnimation0E8E0Indices,
    { NULL, _gM4a1HammerAnimation0E8E0Bank1, NULL, NULL, _gM4a1HammerAnimation0E8E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0F198Bank1[18] = {
#include "assets/m4a1_hammer_animation_0F198_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0F198Bank4[201] = {
#include "assets/m4a1_hammer_animation_0F198_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0F198Records[283] = {
#include "assets/m4a1_hammer_animation_0F198_records.inc"
};

static u16 _gM4a1HammerAnimation0F198Indices[20] = {
#include "assets/m4a1_hammer_animation_0F198_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0F198 = {
    _gM4a1HammerAnimation0F198Records,
    _gM4a1HammerAnimation0F198Indices,
    { NULL, _gM4a1HammerAnimation0F198Bank1, NULL, NULL, _gM4a1HammerAnimation0F198Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation0F938Bank1[16] = {
#include "assets/m4a1_hammer_animation_0F938_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation0F938Bank4[157] = {
#include "assets/m4a1_hammer_animation_0F938_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation0F938Records[263] = {
#include "assets/m4a1_hammer_animation_0F938_records.inc"
};

static u16 _gM4a1HammerAnimation0F938Indices[20] = {
#include "assets/m4a1_hammer_animation_0F938_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation0F938 = {
    _gM4a1HammerAnimation0F938Records,
    _gM4a1HammerAnimation0F938Indices,
    { NULL, _gM4a1HammerAnimation0F938Bank1, NULL, NULL, _gM4a1HammerAnimation0F938Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1HammerAnimation1030CBank1[25] = {
#include "assets/m4a1_hammer_animation_1030C_bank1.inc"
};

static AnimationPackedRotation _gM4a1HammerAnimation1030CBank4[223] = {
#include "assets/m4a1_hammer_animation_1030C_bank4.inc"
};

static AnimationRecord _gM4a1HammerAnimation1030CRecords[311] = {
#include "assets/m4a1_hammer_animation_1030C_records.inc"
};

static u16 _gM4a1HammerAnimation1030CIndices[20] = {
#include "assets/m4a1_hammer_animation_1030C_indices.inc"
};

static AnimationSet _gM4a1HammerAnimation1030C = {
    _gM4a1HammerAnimation1030CRecords,
    _gM4a1HammerAnimation1030CIndices,
    { NULL, _gM4a1HammerAnimation1030CBank1, NULL, NULL, _gM4a1HammerAnimation1030CBank4, NULL, NULL, NULL },
};

AnimationBank D_m4a1_hammer_8012D4F4 = { { {
    NULL,
    &_gM4a1HammerIdleAnimation,
    &_gM4a1HammerAnimation0F198,
    &_gM4a1HammerAnimation0F938,
    &_gM4a1HammerAnimation1030C,
    &_gM4a1HammerTurnLeftAnimation,
    &_gM4a1HammerTurnRightAnimation,
    &_gM4a1HammerAnimation0E368,
    &_gM4a1HammerAnimation0E8E0,
    &_gM4a1HammerAnimation0AF50,
    &_gM4a1HammerAnimation0D578,
    &_gM4a1HammerAnimation0DC5C,
    &_gM4a1HammerAnimation0C238,
    &_gM4a1HammerAnimation0B790,
    &_gM4a1HammerAnimation0D0D4,
    &_gM4a1HammerAnimation0D0D4,
    &_gM4a1HammerAnimation06B38,
    &_gM4a1HammerAnimation06E58,
    &_gM4a1HammerAnimation0761C,
    &_gM4a1HammerUnarmedWalkAnimation,
    &_gM4a1HammerAnimation0D0D4,
    &_gM4a1HammerIdleAnimation,
    &_gM4a1HammerIdleAnimation,
    &_gM4a1HammerAnimation088B4,
    &_gM4a1HammerAnimation09B48,
    &_gM4a1HammerAnimation0942C,
    &_gM4a1HammerPeReleaseFamily0Animation,
    &_gM4a1HammerAnimation05DB4,
    &_gM4a1HammerAnimation0608C,
    &_gM4a1HammerAnimation06330,
    &_gM4a1HammerAnimation06530,
    &_gM4a1HammerAnimation06884,
    &_gM4a1HammerAnimation09FC8,
    &_gM4a1HammerAnimation0A1A0,
    &_gM4a1HammerAnimation09FC8,
    &_gM4a1HammerAnimation0A1A0,
    &_gM4a1HammerStairAscentAnimation,
    &_gM4a1HammerStairDescentAnimation,
    &_gM4a1HammerStairAscentFinishEvenAnimation,
    &_gM4a1HammerStairAscentFinishOddAnimation,
    &_gM4a1HammerItemUseAnimation,
    &_gM4a1HammerIdleAnimation,
    &_gM4a1HammerAnimation0A704,
    &_gM4a1HammerAnimation0A8F8,
    &_gM4a1HammerAnimation0AD78,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

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

/// Nothing reads the word after the task pointer; it keeps the offset of the
/// vector that follows.
static s32 s_unused_8012D664 = 0;

/// Cached attachment origin shared by charged sparks and contact beams.
///
/// Signed low halfwords of the glow's composed translation, published before
/// that update recomposes it. Uses composition-root space, including the view
/// transform; the beam drawer projects it through `GsWSMATRIX`. Storage lasts
/// for the loaded Hammer overlay; updates preserve the fourth halfword.
static SVECTOR _gM4a1HammerAttachmentOrigin = { 0, 0, 0, 0 };
