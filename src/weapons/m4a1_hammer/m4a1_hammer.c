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
#include "types.h"
/// Signed texture-frame counter for the six-cell sprite's forward declaration.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/effects/sprite_quad.h"
#include "../../shared/effects/beam_strip.h"

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

/// Rotates a charged spark offset and adds the cached attachment origin in place.
///
/// `glowCoord` borrows a composed GfxCoord with Q12 rotation. `sparkPosition`
/// points to a writable word-aligned SVECTOR in local game-coordinate units;
/// `attachmentOrigin` borrows a disjoint const SVECTOR in composition-root space.
/// Rotation saturates XYZ to signed 16 bits; origin addition then wraps to
/// 16 bits. The origin is the cache published before this update's composition.
/// The result is a beam endpoint in GsWSMATRIX input space. The position's fourth
/// halfword is loaded but unchanged. Changes GTE rotation, V0, MAC/IR and FLAG;
/// translation registers stay unchanged. Inputs are borrowed only for this call.
/// The coordinate argument is evaluated once; the vector pointers repeat and
/// must have no side effects. Captures no caller locals. Expands to seven
/// statements; invoke only inside a braced control body.
#define M4A1_HAMMER_COMPOSE_SPARK_POSITION(glowCoord, sparkPosition, attachmentOrigin) \
    gte_SetRotMatrix(&(glowCoord)->workm);                                             \
    gte_ldv0((sparkPosition));                                                         \
    gte_rtv0();                                                                        \
    gte_stsv((sparkPosition));                                                         \
    (sparkPosition)->vx = (sparkPosition)->vx + (u16)(attachmentOrigin)->vx;           \
    (sparkPosition)->vy = (sparkPosition)->vy + (u16)(attachmentOrigin)->vy;           \
    (sparkPosition)->vz = (sparkPosition)->vz + (u16)(attachmentOrigin)->vz

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
                                M4A1_HAMMER_COMPOSE_SPARK_POSITION(glowCoord, &work->pos, &_gM4a1HammerAttachmentOrigin);
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
#undef SPRITE_QUAD_TRANSLATION_COMPONENT
#undef SPRITE_QUAD_FRAME_T
#undef SPRITE_QUAD_SIZE_T
/// Read-only XYZ translation words from the charge sprite's `GfxCoord::workm.t`.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const long
/// Reads one cached XYZ translation word for the charge-flare drawer.
///
/// `source` borrows three readable signed 32-bit words from `GfxCoord::workm.t`;
/// `componentIndex` is 0 (X), 1 (Y) or 2 (Z), in elements. Values are integer
/// game coordinates in `GsWSMATRIX` input space; the drawer projects their low
/// 16 bits. Each argument is evaluated once, no caller identifiers are captured,
/// and the source is neither modified nor retained. This binding persists
/// through the charge drawer inclusion and is rebound for the six-cell drawer.
#define SPRITE_QUAD_TRANSLATION_COMPONENT(source, componentIndex) ((source)[(componentIndex)])
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
/// Low three frame bits repeating the charge flare's eight horizontal columns.
///
/// The unsigned frame argument takes the halved effect age and selects
/// U=0..168 in 24-texel steps, with V=0x88..0x9F. The next drawer inclusion
/// undefines this signed integer constant before the six-column strip is
/// configured. See `sprite_quad.h` for the column-mask contract.
#define SPRITE_QUAD_COLUMN_MASK 7
/// Inclusive top texel row of the charge-flare strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x88
/// Inclusive bottom texel row of the charge-flare strip's square cells, relative to its texture page.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
/// Perspective-sizing multiplier for Hammer's charge flare.
///
/// Uses the cell's inclusive 23-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/effects/sprite_quad_draw.inc.c"
#undef SPRITE_QUAD_POSITION_SOURCE_TYPE
#undef SPRITE_QUAD_TRANSLATION_COMPONENT
#undef SPRITE_QUAD_FRAME_T
#undef SPRITE_QUAD_SIZE_T
/// Read-only coordinate source for the six-cell sprite's cached translation.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const GfxCoord
/// Reads one cached translation component for Hammer's six-cell sprite drawer.
///
/// `source` borrows a live `const GfxCoord*` with an already composed cache;
/// `componentIndex` is 0 (X), 1 (Y) or 2 (Z), in elements. The signed 32-bit
/// word is in integer game-coordinate units and `GsWSMATRIX` input space;
/// the drawer projects its low 16 bits. Each argument is evaluated once,
/// no caller identifiers are captured, and the source is neither modified
/// nor retained. This restores the header's accessor alongside its source type
/// and remains bound through the six-cell drawer inclusion.
#define SPRITE_QUAD_TRANSLATION_COMPONENT(source, componentIndex) ((source)->workm.t[(componentIndex)])
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
/// Inclusive bottom texel row of the repeating strip's square cells, relative to its texture page.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
/// Perspective-sizing multiplier for Hammer's repeating sprite strip.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
/// Repeating-strip palette: VRAM X=304 words, Y=266 scanlines.
#define SPRITE_QUAD_CLUT getClut(304, 266)
#include "../../shared/effects/sprite_quad_draw.inc.c"

#include "../../shared/effects/beam_strip_draw.inc.c"

/// The model `gM4a1HammerModel`: generated from its asset (weapons/m4a1_hammer/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/m4a1_hammer/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/m4a1_hammer/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/m4a1_hammer/clips.inc"
#include "assets/weapons/m4a1_hammer/bank.inc"

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
