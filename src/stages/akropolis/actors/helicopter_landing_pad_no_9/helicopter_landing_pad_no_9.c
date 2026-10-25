#include "stages/akropolis/actors/helicopter_landing_pad_no_9.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "helicopter_landing_pad_no_9_private.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../../../shared/effects/sprite_quad.h"

/// The landing pad's `sparks` page the flames draw from, at VRAM (704, 0), and its cells: the golem flame's `flame_*` at
/// the top, the tongues' `tongue_*`, the flame sprite's `burning_*` over two rows and the gunfire smudges' `smudge_*`.
/// The golem flame plays on the `sparkle` palette and the smudges on `smudge`, both on row 270; the tongues and the
/// burning flames step through the fire palettes on row 271, one a frame (`tongue_*`, then `burning_*` from the eighth).
enum {
    SPARKS_PAGE_X         = 704,
    SPARKS_PALETTE_ROW    = 270,
    FIRE_PALETTE_ROW      = 271,
    SPARKS_CELL_WIDTH     = 32,
    FLAME_CELL_HEIGHT     = 40,
    TALL_CELL_HEIGHT      = 48,
    TONGUE_V              = 112,
    BURNING_V_BYTE        = 160 - 0x100, // The burning rows' 160, as the byte field wraps it
    BURNING_CELLS_PER_ROW = 8,
    BURNING_FIRST_PALETTE = 8,
    SMUDGE_V              = 208,
    SMUDGE_SIZE           = 32,
    SMUDGE_FIRST_CELL     = 4,
    SMUDGE_PALETTE_X      = 32,
};

/// Packed flame spawn options and the frame counts of the actor's particle tasks.
enum {
    PARTICLE_INITIAL          = 0,
    FLAME_SCALE_MASK          = 0xFFF,
    FLAME_RANDOM_MOTION       = 0x10000,
    FLAME_DEFAULT_SCALE       = 0x200,
    FLAME_MOTION_RANGE        = 48,
    GOLEM_FLAME_CELL_COUNT    = 6,
    GOLEM_FLAME_LAST_AGE_MASK = 0xF,
    FLAME_FADE_FRAMES         = 8,
    FLAME_FADE_STEP           = 16,
    FLAME_FULL_BRIGHTNESS     = 0x80,
    FLAME_TONGUE_FRAMES       = 8,
    FLAME_SPRITE_FRAMES       = 12,
    GUNFIRE_PARTICLE_FRAMES   = 8,
    DEBRIS_SHORT_TIER         = 1,
    DEBRIS_LONG_TIER          = 2,
    DEBRIS_GRAVITY            = 6,
    DEBRIS_FRAMES_PER_TIER    = 16,
};

/// Scratch-stack block one frame's segment of a debris streak is drawn from.
typedef struct {
    SVECTOR endpoints[2];       // World positions of the segment's two ends: before the frame's drift, then after it
    s32     depths[2];          // SZ3 / 4 of each end, stored once that end's projection is accepted; their mean is the ordering and blend depth
    s32     projectionFlags;    // Latest GTE FLAG word; bit 31 makes it negative and drops the segment
    DVECTOR screenEndpoints[2]; // Signed screen X/Y pixels, written together as one GTE word per end
} _DebrisStreakScratch;
STATIC_ASSERT_SIZEOF(_DebrisStreakScratch, 0x24);

/// VRAM coordinates of one palette for actor 510900's explosion fireball.
typedef struct {
    u16 clutX; // Palette X in VRAM words, aligned to 16 words.
    u16 clutY; // Palette Y in VRAM scanlines.
} _SpritePalette;
STATIC_ASSERT_SIZEOF(_SpritePalette, 4);

/// Twelve fireball palettes, one per frame of `gEffectSpriteAtlasFrames`: the landing pad's row 270, the effect atlas's
/// `fireball_*` variants.
static const _SpritePalette _gHelicopterLandingPadNo9FireballFramePalettes[12] __attribute__((section(".data"))) = {
    { 80, 270 },
    { 96, 270 },
    { 112, 270 },
    { 128, 270 },
    { 144, 270 },
    { 160, 270 },
    { 176, 270 },
    { 192, 270 },
    { 208, 270 },
    { 224, 270 },
    { 240, 270 },
    { 256, 270 },
};

#include "assets/enemies/golem/no_9/helipad/body_schema.h"
#include "assets/enemies/golem/no_9/sounds_akropolis.h"

#include "assets/enemies/golem/no_9/body_akropolis.inc"

#include "assets/enemies/golem/no_9/akropolis_sword.inc"

#include "assets/enemies/golem/no_9/akropolis_prop.inc"

#include "assets/enemies/golem/no_9/akropolis_chest_piece.inc"

#include "assets/enemies/golem/grenade.inc"

#include "assets/enemies/golem/no_9/transformer_cable.inc"

#include "assets/enemies/golem/no_9/package_akropolis_clips.inc"

DamageAttack gHelicopterLandingPadNo9Attacks[6] = {
    { 14, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 24, GAME_ACTOR_REACTION_BODY_BLAST },
    { 20, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 999, GAME_ACTOR_REACTION_BODY_BLAST },
    { 0, GAME_ACTOR_REACTION_PARALYSIS },
    { 1, GAME_ACTOR_REACTION_BODY_BLAST },
};

/// Chooses a flame-jet particle's local velocity and draws its subsequent size bits.
#define PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, horizontalRange, verticalStep)            \
    {                                                                                         \
        enum { FLAME_JET_MIN_X_SPEED = 128 };                                                 \
        RANDOM_LCG_ADVANCE();                                                                 \
        (effect)->move.vx = -(RANDOM_LCG_DRAW() % (horizontalRange)) - FLAME_JET_MIN_X_SPEED; \
        (effect)->move.vy = (verticalStep);                                                   \
        (effect)->move.vz = 0;                                                                \
        RANDOM_LCG_ADVANCE();                                                                 \
    }

void helicopterLandingPadNo9FlameJetTask(Task* task)
{
    enum {
        LIGHT_SLOT            = 2,
        ACTIVE                = 1,
        RANDOM_SIZE_MASK      = 0xF0,
        LIGHT_RED             = ONE,
        LIGHT_GREEN           = ONE / 2,
        LIGHT_BLUE            = ONE / 4,
        LIGHT_CONTRACTION     = 400,
        LIGHT_FRAMES          = 16,
        INNER_RADIUS          = 8000,
        OUTER_RADIUS          = 10000,
        FULL_SCALE            = 256,
        SCALE_STEP            = 16,
        DYING_FLAME_FRAMES    = 30,
        DYING_PARTICLE_FRAMES = 60,
    };

    EffectWork*                    effect;
    GfxCoord*                      coord;
    WorldCoordPointLight*          pointLight;
    WorldCoordTransientPointLight* lightSlot;
    EffectWork*                    particle;
    s32                            particleIndex;
    s32                            randomBits;
    s32                            localZ;

    effect     = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    lightSlot  = &gWorldCoordTransientPointLights[LIGHT_SLOT];
    pointLight = &lightSlot->light;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
        }
        if (task->spawnArg1.value == HELICOPTER_LANDING_PAD_NO_9_FLAME_RELEASED) {
            effectKillTask(effect, task);
        }
        return;
    }
    if (task->state == PARTICLE_INITIAL) {
        coord->parent = effect->parent;
        gfxSetRotIdentity(&coord->coord);
        coord->coord.t[0]   = effect->pos.vx;
        coord->coord.t[1]   = effect->pos.vy;
        localZ              = effect->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[2]   = localZ;
        task->state         = ACTIVE;
    }
    actorRenderComposeCoord(coord);
    // The emitter also counts down slot 2 and contracts its full-strength radius.
    if (lightSlot->framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
        pointLight->head.color.r = LIGHT_RED;
        pointLight->head.color.g = LIGHT_GREEN;
        pointLight->head.color.b = LIGHT_BLUE;
        if (pointLight->inner > LIGHT_CONTRACTION) {
            pointLight->inner -= LIGHT_CONTRACTION;
        }
        lightSlot->framesLeft--;
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightSlot->light.head.transform.coord.coord);
        lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
        if (lightSlot->framesLeft == WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            task->spawnArg1.value = HELICOPTER_LANDING_PAD_NO_9_FLAME_OFF;
            effect->age           = 0;
            effect->scale         = 0;
        }
    }
    // The actor supplies intensity; each adopted particle follows this task's lifetime.
    switch (task->spawnArg1.value) {
        case HELICOPTER_LANDING_PAD_NO_9_FLAME_OFF:
            break;
        case HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING:
            effect->scale = (effect->scale < FULL_SCALE) ? effect->scale + SCALE_STEP : FULL_SCALE;
            for (particleIndex = 0; particleIndex < 2; particleIndex++) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x2C0, 0x40);
                particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + effect->scale, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            lightSlot->framesLeft = LIGHT_FRAMES;
            pointLight->inner     = INNER_RADIUS;
            pointLight->outer     = OUTER_RADIUS;
            RANDOM_LCG_ADVANCE();
            randomBits = RANDOM_LCG_DRAW();
            // Keep scale plus the motion flag grouped before adding random size.
            if (!(randomBits & 3)) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x40);
                particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + ({ effect->scale + FLAME_RANDOM_MOTION; }),
                                       &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 3)) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                particle = effectSpawn(EFFECT_NO9_FLAME_TONGUE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + ({ effect->scale + FLAME_RANDOM_MOTION; }),
                                       &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 3)) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + effect->scale, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (randomBits % 3 == 0) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x80);
                particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + (FLAME_RANDOM_MOTION | 0x80), &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 3)) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, -0x80);
                particle = effectSpawn(EFFECT_NO9_GUNFIRE_PARTICLE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + 0x180, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_FLAME_BLAST:
            lightSlot->framesLeft = LIGHT_FRAMES;
            pointLight->inner     = INNER_RADIUS;
            pointLight->outer     = OUTER_RADIUS;
            for (particleIndex = 0; particleIndex < 3; particleIndex++) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x40);
                particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) | (FLAME_RANDOM_MOTION | 0x100), &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            RANDOM_LCG_ADVANCE();
            randomBits = RANDOM_LCG_DRAW();
            if (!(randomBits & 7)) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) | 0x100, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 3)) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x80);
                particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + (FLAME_RANDOM_MOTION | 0x80), &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 7)) {
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, -0x80);
                particle = effectSpawn(EFFECT_NO9_GUNFIRE_PARTICLE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + 0x180, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_FLAME_DYING:
            RANDOM_LCG_ADVANCE();
            randomBits = RANDOM_LCG_DRAW();
            effect->age++;
            if (effect->age < DYING_FLAME_FRAMES) {
                if (!(randomBits & 7)) {
                    PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x2C0, 0x40);
                    particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + 0x80, &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                randomBits >>= 1;
                if (!(randomBits & 3)) {
                    PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x40);
                    particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + (FLAME_RANDOM_MOTION | 0x80), &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                for (particleIndex = 0; particleIndex < 2; particleIndex++) {
                    PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                    particle = effectSpawn(EFFECT_NO9_FLAME_TONGUE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) | (FLAME_RANDOM_MOTION | 0x100), &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                randomBits >>= 1;
                if (!(randomBits & 7)) {
                    PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                    particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) | 0x100, &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                randomBits >>= 1;
                if (!(randomBits & 7)) {
                    PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x80);
                    particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + (FLAME_RANDOM_MOTION | 0x80), &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                for (particleIndex = 0; particleIndex < 2; particleIndex++) {
                    PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, -0x80);
                    particle = effectSpawn(EFFECT_NO9_GUNFIRE_PARTICLE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) + 0x180, &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                lightSlot->framesLeft = LIGHT_FRAMES;
                pointLight->inner     = INNER_RADIUS;
                pointLight->outer     = OUTER_RADIUS;
            } else if (effect->age < DYING_PARTICLE_FRAMES) {
                lightSlot->framesLeft = 2;
                pointLight->inner     = LIGHT_CONTRACTION;
                pointLight->outer     = LIGHT_CONTRACTION;
                PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x80);
                particle = effectSpawn(EFFECT_NO9_GUNFIRE_PARTICLE, coord, (RANDOM_LCG_DRAW() & RANDOM_SIZE_MASK) | 0x100, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_FLAME_RELEASED:
            effectKillTask(effect, task);
            break;
    }
}

#undef PREPARE_FLAME_JET_PARTICLE_VELOCITY

void helicopterLandingPadNo9GolemFlameTask(Task* task)
{
    GfxCoord            groundCoord;
    EffectWork*         effect;
    GfxCoord*           coord;
    EffectShapeScratch* scratchTop;
    EffectShapeScratch* projectionScratch;
    EffectShapeScratch* scratch;
    POLY_FT4*           sprite;
    s16                 effectControl;
    s16                 screenEdge;
    s16                 riseSpeed;
    u8                  glowBrightness;
    u16                 worldZ;
    s32                 framesLeft;
    s32                 fadeBrightness;

/// Draws a half-scale flame glow at Y zero in the flame's parent frame.
#define DRAW_FLAME_GROUND_GLOW(flameCoord, groundCoord, flameEffect, brightness)       \
    ({                                                                                 \
        (groundCoord).parent       = (flameCoord)->parent;                             \
        (groundCoord).coord.t[0]   = (flameCoord)->coord.t[0];                         \
        (groundCoord).coord.t[1]   = 0;                                                \
        (groundCoord).coord.t[2]   = (flameCoord)->coord.t[2];                         \
        (groundCoord).composeStamp = GRAPHICS_COORD_DIRTY;                             \
        actorRenderComposeCoord(&(groundCoord));                                       \
        effectDrawGroundGlow(&(groundCoord), (flameEffect)->scale >> 1, (brightness)); \
    })

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        // Project the world centre before initializing or drawing the sprite.
        actorRenderComposeCoord(coord);
        scratchTop                               = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        projectionScratch                        = scratchTop - 1;
        projectionScratch->worldPoint.vx         = (u16)coord->workm.t[0];
        scratch                                  = projectionScratch;
        scratch->worldPoint.vy                   = (u16)coord->workm.t[1];
        worldZ                                   = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = scratch;
        scratch->worldPoint.vz                   = worldZ;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(scratchTop - 1)->screenX);
        gte_stflg(&(scratchTop - 1)->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&(scratchTop - 1)->depth);
            sprite         = gGpuPrimCursor;
            gGpuPrimCursor = sprite + 1;
            setPolyFT4(sprite);
            if (task->state == PARTICLE_INITIAL) {
                effect->scale = (u16)task->spawnArg1.value & FLAME_SCALE_MASK;
                RANDOM_LCG_ADVANCE();
                effect->angle = RANDOM_LCG_DRAW() & GOLEM_FLAME_LAST_AGE_MASK;
                if (task->spawnArg1.value & FLAME_RANDOM_MOTION) {
                    RANDOM_LCG_ADVANCE();
                    effect->period = RANDOM_LCG_DRAW() % FLAME_MOTION_RANGE;
                }
                task->state++;
            }
            // Raw texture colour gives way to modulation for the last eight frames.
            if (effect->angle - FLAME_FADE_FRAMES < effect->age) {
                framesLeft = effect->angle - effect->age + 1;
                // Fitted: the image masks the glow argument with 0xFF, so the
                // product reached glowBrightness through a join of two
                // assignments, and whatever chose between them left no
                // instruction. What was tested, and whether the two sides
                // differed in the source, is unknown; this test is a stand-in.
                if (framesLeft > 0) {
                    fadeBrightness = framesLeft * FLAME_FADE_STEP;
                } else {
                    fadeBrightness = framesLeft * FLAME_FADE_STEP;
                }
                glowBrightness = fadeBrightness;
                sprite->r0     = fadeBrightness;
                sprite->g0     = fadeBrightness;
                sprite->b0     = fadeBrightness;
            } else {
                glowBrightness = FLAME_FULL_BRIGHTNESS;
                setShadeTex(sprite, 1);
            }
            sprite->tpage = getTPage(0, GPU_BLEND_ADD, SPARKS_PAGE_X, 0);
            sprite->clut  = getClut(0, SPARKS_PALETTE_ROW);
            setSemiTrans(sprite, 1);
            sprite->u0               = (effect->age % GOLEM_FLAME_CELL_COUNT) * SPARKS_CELL_WIDTH;
            sprite->v0               = 0;
            sprite->u1               = (effect->age % GOLEM_FLAME_CELL_COUNT) * SPARKS_CELL_WIDTH + SPARKS_CELL_WIDTH - 1;
            sprite->v1               = 0;
            sprite->u2               = (effect->age % GOLEM_FLAME_CELL_COUNT) * SPARKS_CELL_WIDTH;
            sprite->v2               = FLAME_CELL_HEIGHT - 1;
            sprite->u3               = (effect->age % GOLEM_FLAME_CELL_COUNT) * SPARKS_CELL_WIDTH + SPARKS_CELL_WIDTH - 1;
            sprite->v3               = FLAME_CELL_HEIGHT - 1;
            scratch->extent.corner.x = (effect->scale * (SPARKS_CELL_WIDTH - 1)) / scratch->depth;
            scratch->extent.corner.y = (effect->scale * (FLAME_CELL_HEIGHT - 1)) / scratch->depth;
            screenEdge               = scratch->screenX - (u16)scratch->extent.corner.x;
            sprite->x2               = screenEdge;
            sprite->x0               = screenEdge;
            screenEdge               = scratch->screenX + (u16)scratch->extent.corner.x;
            sprite->x3               = screenEdge;
            sprite->x1               = screenEdge;
            screenEdge               = scratch->screenY - (u16)scratch->extent.corner.y;
            sprite->y1               = screenEdge;
            sprite->y0               = screenEdge;
            screenEdge               = scratch->screenY + (u16)scratch->extent.corner.y;
            sprite->y3               = screenEdge;
            sprite->y2               = screenEdge;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    sprite);
            // Every other frame, put the glow on Y=0 in the same parent frame.
            if (coord->coord.t[1] < 0 && (effect->age & 1)) {
                DRAW_FLAME_GROUND_GLOW(coord, groundCoord, effect, glowBrightness);
#undef DRAW_FLAME_GROUND_GLOW
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        // Paused effects remain visible; only running effects move and age.
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        riseSpeed = effect->period;
        if (riseSpeed != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  -= riseSpeed;
        }
        effect->age++;
        if (effect->angle >= effect->age) {
            return;
        }
    }
    effectKillTask(effect, task);
}

void helicopterLandingPadNo9FlameTongueTask(Task* task)
{
    EffectWork*         effect;
    GfxCoord*           coord;
    EffectShapeScratch* scratch;
    POLY_FT4*           sprite;
    s16                 effectControl;
    s16                 screenEdge;
    s16                 riseSpeed;
    s32                 scale;
    s32                 frameAge;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        // Project the world centre before initializing or drawing the sprite.
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            sprite         = gGpuPrimCursor;
            gGpuPrimCursor = sprite + 1;
            setPolyFT4(sprite);
            if (task->state == PARTICLE_INITIAL) {
                if (task->spawnArg1.value & FLAME_SCALE_MASK) {
                    scale = (u16)task->spawnArg1.value & FLAME_SCALE_MASK;
                } else {
                    scale = FLAME_DEFAULT_SCALE;
                }
                effect->scale = scale;
                if (task->spawnArg1.value & FLAME_RANDOM_MOTION) {
                    RANDOM_LCG_ADVANCE();
                    effect->period = RANDOM_LCG_DRAW() % FLAME_MOTION_RANGE;
                }
                task->state++;
            }
            sprite->tpage = getTPage(0, GPU_BLEND_ADD, SPARKS_PAGE_X, 0);
            setShadeTex(sprite, 1);
            setSemiTrans(sprite, 1);
            frameAge                 = effect->age;
            sprite->clut             = (frameAge & 0x3F) | getClut(0, FIRE_PALETTE_ROW);
            frameAge                 = effect->age;
            sprite->v0               = TONGUE_V;
            sprite->u0               = frameAge * SPARKS_CELL_WIDTH;
            frameAge                 = effect->age;
            sprite->v1               = TONGUE_V;
            sprite->u1               = frameAge * SPARKS_CELL_WIDTH + SPARKS_CELL_WIDTH - 1;
            frameAge                 = effect->age;
            sprite->v2               = TONGUE_V + TALL_CELL_HEIGHT - 1;
            sprite->u2               = frameAge * SPARKS_CELL_WIDTH;
            frameAge                 = effect->age;
            sprite->v3               = TONGUE_V + TALL_CELL_HEIGHT - 1;
            sprite->u3               = frameAge * SPARKS_CELL_WIDTH + SPARKS_CELL_WIDTH - 1;
            scratch->extent.corner.x = (effect->scale * (SPARKS_CELL_WIDTH - 1)) / scratch->depth;
            scratch->extent.corner.y = (effect->scale * (TALL_CELL_HEIGHT - 1)) / scratch->depth;
            screenEdge               = scratch->screenX - (u16)scratch->extent.corner.x;
            sprite->x2               = screenEdge;
            sprite->x0               = screenEdge;
            screenEdge               = scratch->screenX + (u16)scratch->extent.corner.x;
            sprite->x3               = screenEdge;
            sprite->x1               = screenEdge;
            screenEdge               = scratch->screenY - (u16)scratch->extent.corner.y;
            sprite->y1               = screenEdge;
            sprite->y0               = screenEdge;
            screenEdge               = scratch->screenY + (u16)scratch->extent.corner.y;
            sprite->y3               = screenEdge;
            sprite->y2               = screenEdge;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    sprite);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        // Paused effects remain visible; only running effects move and age.
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        riseSpeed = effect->period;
        if (riseSpeed != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  -= riseSpeed;
        }
        effect->age++;
        if (effect->age < FLAME_TONGUE_FRAMES) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(effect, task);
}

void helicopterLandingPadNo9FlameSpriteTask(Task* task)
{
    EffectWork*         effect;
    GfxCoord*           coord;
    EffectShapeScratch* scratch;
    POLY_FT4*           sprite;
    s16                 effectControl;
    s16                 frameAge;
    s16                 screenEdge;
    s32                 scale;
    s32                 paletteAge;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        // Project the world centre before initializing or drawing the sprite.
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            sprite         = gGpuPrimCursor;
            gGpuPrimCursor = sprite + 1;
            setPolyFT4(sprite);
            if (task->state == PARTICLE_INITIAL) {
                if (task->spawnArg1.value & FLAME_SCALE_MASK) {
                    scale = (u16)task->spawnArg1.value & FLAME_SCALE_MASK;
                } else {
                    scale = FLAME_DEFAULT_SCALE;
                }
                effect->scale = scale;
                if (effect->period = (u16)((u32)task->spawnArg1.value >> 16) & (FLAME_RANDOM_MOTION >> 16)) {
                    RANDOM_LCG_ADVANCE();
                    effect->period = RANDOM_LCG_DRAW() % FLAME_MOTION_RANGE;
                }
                task->state++;
            }
            sprite->tpage = getTPage(0, GPU_BLEND_ADD, SPARKS_PAGE_X, 0);
            setShadeTex(sprite, 1);
            setSemiTrans(sprite, 1);
            paletteAge               = effect->age;
            sprite->clut             = ((paletteAge + BURNING_FIRST_PALETTE) & 0x3F) | getClut(0, FIRE_PALETTE_ROW);
            frameAge                 = effect->age;
            sprite->u0               = (s16)(frameAge % BURNING_CELLS_PER_ROW) * SPARKS_CELL_WIDTH;
            frameAge                 = effect->age;
            sprite->v0               = (frameAge / BURNING_CELLS_PER_ROW) * TALL_CELL_HEIGHT + BURNING_V_BYTE;
            frameAge                 = effect->age;
            sprite->u1               = (s16)(frameAge % BURNING_CELLS_PER_ROW) * SPARKS_CELL_WIDTH + SPARKS_CELL_WIDTH - 1;
            frameAge                 = effect->age;
            sprite->v1               = (frameAge / BURNING_CELLS_PER_ROW) * TALL_CELL_HEIGHT + BURNING_V_BYTE;
            frameAge                 = effect->age;
            sprite->u2               = (s16)(frameAge % BURNING_CELLS_PER_ROW) * SPARKS_CELL_WIDTH;
            frameAge                 = effect->age;
            sprite->v2               = (frameAge / BURNING_CELLS_PER_ROW) * TALL_CELL_HEIGHT + BURNING_V_BYTE + TALL_CELL_HEIGHT - 1;
            frameAge                 = effect->age;
            sprite->u3               = (s16)(frameAge % BURNING_CELLS_PER_ROW) * SPARKS_CELL_WIDTH + SPARKS_CELL_WIDTH - 1;
            frameAge                 = effect->age;
            sprite->v3               = (frameAge / BURNING_CELLS_PER_ROW) * TALL_CELL_HEIGHT + BURNING_V_BYTE + TALL_CELL_HEIGHT - 1;
            scratch->extent.corner.x = (effect->scale * (SPARKS_CELL_WIDTH - 1)) / scratch->depth;
            scratch->extent.corner.y = (effect->scale * (TALL_CELL_HEIGHT - 1)) / scratch->depth;
            // The target shift uses period's low five bits; nonzero period also enables falling.
            scratch->extent.corner.x >>= effect->period;
            screenEdge                 = scratch->screenX - (u16)scratch->extent.corner.x;
            sprite->x2                 = screenEdge;
            sprite->x0                 = screenEdge;
            screenEdge                 = scratch->screenX + (u16)scratch->extent.corner.x;
            sprite->x3                 = screenEdge;
            sprite->x1                 = screenEdge;
            screenEdge                 = scratch->screenY - (u16)scratch->extent.corner.y;
            sprite->y1                 = screenEdge;
            sprite->y0                 = screenEdge;
            screenEdge                 = scratch->screenY + (u16)scratch->extent.corner.y;
            sprite->y3                 = screenEdge;
            sprite->y2                 = screenEdge;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    sprite);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        // Paused effects remain visible; only running effects move and age.
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (effect->period != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  += 0x38;
        }
        effect->age++;
        if (effect->age < FLAME_SPRITE_FRAMES) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(effect, task);
}

void helicopterLandingPadNo9GunfireParticleTask(Task* task)
{
    EffectWork*         effect;
    GfxCoord*           coord;
    EffectShapeScratch* scratch;
    POLY_FT4*           sprite;
    s16                 effectControl;
    s16                 frameAge;
    s16                 screenEdge;
    s16                 riseSpeed;
    s32                 scale;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        // Project the world centre before initializing or drawing the sprite.
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            sprite         = gGpuPrimCursor;
            gGpuPrimCursor = sprite + 1;
            setPolyFT4(sprite);
            if (task->state == PARTICLE_INITIAL) {
                if (task->spawnArg1.value & FLAME_SCALE_MASK) {
                    scale = (u16)task->spawnArg1.value & FLAME_SCALE_MASK;
                } else {
                    scale = FLAME_DEFAULT_SCALE;
                }
                effect->scale = scale;
                RANDOM_LCG_ADVANCE();
                effect->period = RANDOM_LCG_DRAW() % FLAME_MOTION_RANGE;
                task->state++;
            }
            sprite->tpage = getTPage(0, GPU_BLEND_SUBTRACT, SPARKS_PAGE_X, 0);
            setShadeTex(sprite, 1);
            setSemiTrans(sprite, 1);
            sprite->clut             = getClut(SMUDGE_PALETTE_X, SPARKS_PALETTE_ROW);
            frameAge                 = effect->age;
            sprite->v0               = SMUDGE_V;
            sprite->u0               = (frameAge / 2 + SMUDGE_FIRST_CELL) * SPARKS_CELL_WIDTH;
            frameAge                 = effect->age;
            sprite->v1               = SMUDGE_V;
            sprite->u1               = (frameAge / 2 + SMUDGE_FIRST_CELL) * SPARKS_CELL_WIDTH + SPARKS_CELL_WIDTH - 1;
            frameAge                 = effect->age;
            sprite->v2               = SMUDGE_V + SMUDGE_SIZE - 1;
            sprite->u2               = (frameAge / 2 + SMUDGE_FIRST_CELL) * SPARKS_CELL_WIDTH;
            frameAge                 = effect->age;
            sprite->v3               = SMUDGE_V + SMUDGE_SIZE - 1;
            sprite->u3               = (frameAge / 2 + SMUDGE_FIRST_CELL) * SPARKS_CELL_WIDTH + SPARKS_CELL_WIDTH - 1;
            scratch->extent.corner.x = (effect->scale * (SMUDGE_SIZE - 1)) / scratch->depth;
            scratch->extent.corner.y = (effect->scale * (SMUDGE_SIZE - 1)) / scratch->depth;
            screenEdge               = scratch->screenX - (u16)scratch->extent.corner.x;
            sprite->x2               = screenEdge;
            sprite->x0               = screenEdge;
            screenEdge               = scratch->screenX + (u16)scratch->extent.corner.x;
            sprite->x3               = screenEdge;
            sprite->x1               = screenEdge;
            screenEdge               = scratch->screenY - (u16)scratch->extent.corner.y;
            sprite->y1               = screenEdge;
            sprite->y0               = screenEdge;
            screenEdge               = scratch->screenY + (u16)scratch->extent.corner.y;
            sprite->y3               = screenEdge;
            sprite->y2               = screenEdge;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth),
                    sprite);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        // Paused effects remain visible; only running effects move and age.
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        riseSpeed = effect->period;
        if (riseSpeed != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  -= riseSpeed;
        }
        effect->age++;
        if (effect->age < GUNFIRE_PARTICLE_FRAMES) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(effect, task);
}

void helicopterLandingPadNo9MuzzleFlashTask(Task* task)
{
    enum {
        LIGHT_SLOT         = 3,
        LIGHT_RED          = 3 * ONE / 4,
        LIGHT_GREEN        = ONE / 2,
        LIGHT_BLUE         = ONE / 4,
        LIGHT_FRAMES       = 4,
        LIGHT_INNER_RADIUS = 4000,
        LIGHT_OUTER_RADIUS = 4800,
    };

    WorldCoordTransientPointLight* lightSlot;
    GfxCoord*                      lightCoord;
    WorldCoordPointLight*          pointLight;
    EffectWork*                    effect;
    GfxCoord*                      coord;
    s32                            particleIndex;

    lightSlot  = &gWorldCoordTransientPointLights[LIGHT_SLOT];
    lightCoord = &lightSlot->light.head.transform.coord;
    effect     = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    pointLight = &lightSlot->light;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        effectKillTask(effect, task);
        return;
    }
    coord->parent = effect->parent;
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[0]   = effect->pos.vx;
    coord->coord.t[1]   = effect->pos.vy;
    coord->coord.t[2]   = effect->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    // Finish placement before spawning the burst and placing its view-relative light.
    actorRenderComposeCoord(coord);
    effect->move.vx = -0x200;
    effect->move.vy = 0x40;
    effect->move.vz = 0;
    effectSpawn(EFFECT_IMPACT_SPARK, coord, 0x180, &effect->move);
    for (particleIndex = 0; particleIndex < 6; particleIndex++) {
        effectSpawn(EFFECT_NO9_GOLEM_DEBRIS_STREAK, coord, 0, &effect->move);
        effectSpawn(EFFECT_PIXEL_SPARK, coord, 1, NULL);
    }
    lightSlot->framesLeft    = LIGHT_FRAMES;
    pointLight->inner        = LIGHT_INNER_RADIUS;
    pointLight->outer        = LIGHT_OUTER_RADIUS;
    pointLight->head.color.r = LIGHT_RED;
    pointLight->head.color.g = LIGHT_GREEN;
    pointLight->head.color.b = LIGHT_BLUE;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightCoord->coord);
    lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    effectKillTask(effect, task);
}

void helicopterLandingPadNo9DebrisStreakTask(Task* task)
{
    _DebrisStreakScratch* scratch;
    EffectWork*           effect;
    GfxCoord*             coord;
    LINE_F2*              line;
    s16                   effectControl;
    s16                   lifetimeTier;
    s32                   randomState;
    s16                   brightness;
    s16                   nextAge;

    // The reservation precedes the control check, including early-return paths.
    scratch       = SCRATCH_STACK_RESERVE_BLOCK(_DebrisStreakScratch);
    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(effect, task);
        }
        return;
    }
    actorRenderComposeCoord(coord);
    if (task->state == PARTICLE_INITIAL) {
        RANDOM_LCG_ADVANCE();
        effect->move.vx = 0x20 - (RANDOM_LCG_DRAW() & 0x3F);
        RANDOM_LCG_ADVANCE();
        effect->move.vy = -(RANDOM_LCG_DRAW() & 0x3F) - 0x10;
        RANDOM_LCG_ADVANCE();
        effect->move.vz = 0x20 - (RANDOM_LCG_DRAW() & 0x3F);
        RANDOM_LCG_ADVANCE();
        lifetimeTier = DEBRIS_LONG_TIER;
        if ((RANDOM_LCG_DRAW() & 3) != 0) {
            lifetimeTier = DEBRIS_SHORT_TIER;
        }
        randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
        effect->scale   = lifetimeTier;
        effect->angle   = (((u32)randomState >> 16) & 1) + 1;
        gRandomLcgState = randomState;
        task->state++;
    }
    // Project the segment traced by this frame's local-coordinate drift.
    scratch->endpoints[0].vx = coord->workm.t[0];
    scratch->endpoints[0].vy = coord->workm.t[1];
    scratch->endpoints[0].vz = coord->workm.t[2];
    coord->coord.t[0]       += effect->move.vx;
    coord->coord.t[1]       += effect->move.vy;
    coord->coord.t[2]       += effect->move.vz;
    coord->composeStamp      = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    scratch->endpoints[1].vx = coord->workm.t[0];
    scratch->endpoints[1].vy = coord->workm.t[1];
    scratch->endpoints[1].vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->endpoints[0]);
    gte_rtps();
    gte_stsxy(&scratch->screenEndpoints[0]);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depths[0]);
        gte_ldv0(&scratch->endpoints[1]);
        gte_rtps();
        gte_stsxy(&scratch->screenEndpoints[1]);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depths[1]);
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineF2(line);
            brightness = 0xFF - (effect->age << (5 - effect->scale));
            line->r0   = brightness;
            line->g0   = brightness >> effect->angle;
            line->b0   = brightness >> 3;
            line->x0   = scratch->screenEndpoints[0].vx;
            line->y0   = scratch->screenEndpoints[0].vy;
            line->x1   = scratch->screenEndpoints[1].vx;
            line->y1   = scratch->screenEndpoints[1].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH((scratch->depths[0] + scratch->depths[1]) >> 1),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, (scratch->depths[0] + scratch->depths[1]) >> 1);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_DebrisStreakScratch);
    effect->move.vy += DEBRIS_GRAVITY;
    nextAge          = effect->age + 1;
    effect->age      = nextAge;
    if (nextAge > effect->scale * DEBRIS_FRAMES_PER_TIER - 1) {
        effectKillTask(effect, task);
    }
}

void helicopterLandingPadNo9ImpactBurstTask(Task* task)
{
    enum {
        FIREBALL           = 0,
        WAIT_SMOKE         = 1,
        SMOKE              = 2,
        TAIL_SMOKE         = 3,
        FINISHED           = 4,
        SMOKE_START        = 9,
        TAIL_START         = 51,
        END                = 61,
        MAIN_SMOKE_OPTIONS = 0x82004400,
        TAIL_SMOKE_OPTIONS = 0xD2004400,
    };

    EffectWork* effect;
    GfxCoord*   coord;
    s16         effectControl;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN || task->state == FINISHED) {
            effectKillTask(effect, task);
        }
        return;
    }
    effect->age++;
    switch (task->state) {
        case FIREBALL:
            // Adopt the fireball; later calls reload the controller work.
            effect = effectSpawn(EFFECT_NO9_EXPLOSION_FIREBALL, coord, 0x480, NULL);
            if (effect != NULL) {
                taskReparent(task, effect->task);
            }
            task->state++;
            break;
        case WAIT_SMOKE:
            if (effect->age >= SMOKE_START) {
                task->state++;
            }
            break;
        case SMOKE:
            effectSpawn(EFFECT_SMOKE_PUFF, coord, MAIN_SMOKE_OPTIONS, NULL);
            if (effect->age >= TAIL_START) {
                task->state++;
            }
            break;
        case TAIL_SMOKE:
            effectSpawn(EFFECT_SMOKE_PUFF, coord, TAIL_SMOKE_OPTIONS, NULL);
            if (effect->age >= END) {
                task->state++;
            }
            break;
        case FINISHED:
            effectKillTask(effect, task);
            break;
    }
}

/// Builds the fireball's random drift and rotates it from the borrowed parent basis.
#define MAKE_FIREBALL_DRIFT(effect)                              \
    {                                                            \
        enum { CENTRE = 16,                                      \
               MASK   = 31 };                                      \
        RANDOM_LCG_ADVANCE();                                    \
        (effect)->move.vx = CENTRE - (RANDOM_LCG_DRAW() & MASK); \
        RANDOM_LCG_ADVANCE();                                    \
        (effect)->move.vy = CENTRE - (RANDOM_LCG_DRAW() & MASK); \
        RANDOM_LCG_ADVANCE();                                    \
        (effect)->move.vz = CENTRE - (RANDOM_LCG_DRAW() & MASK); \
        gte_lddp((effect)->scale << DRIFT_SCALE_SHIFT);          \
        gte_ldsv(&(effect)->move);                               \
        gte_gpf12();                                             \
        gte_stsv(&(effect)->move);                               \
        gte_lddp((effect)->index << ROTATION_FRACTION_BITS);     \
        gte_ldsv(&(effect)->move);                               \
        gte_gpf12();                                             \
        gte_stsv(&(effect)->move);                               \
        gte_SetRotMatrix(&(effect)->parent->coord);              \
        gte_ldv0(&(effect)->move);                               \
        gte_rtv0();                                              \
        gte_stsv(&(effect)->move);                               \
    }

void helicopterLandingPadNo9ExplosionFireballTask(Task* task)
{
    enum {
        DEFAULT_SCALE          = 768,
        SIZE_MASK              = 0xFFF,
        ACTIVE                 = 1,
        DEFAULT_PERIOD         = 2,
        PERIOD_MASK            = 0xF000,
        PERIOD_SHIFT           = 12,
        MAX_PERIOD             = 15,
        MOTION_MASK            = 15,
        SUPPRESS_CHILDREN      = 0xF0000000,
        FAST_CHILD_OPTIONS     = (2 << 24) | (1 << 12),
        SLOW_CHILD_OPTIONS     = (1 << 24) | (2 << 12),
        ROTATION_FRACTION_BITS = 12,
        DRIFT_SCALE_SHIFT      = 3,
    };

    EffectWork* effect;
    EffectWork* childEffect;
    GfxCoord*   coord;
    s16         effectControl;
    s16         scale;
    s16         framePeriod;
    s32         optionsByte;
    s32         childIndex;
    s32         childCount;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(effect, task);
        }
        return;
    }
    effect->age++;
    if (task->state == PARTICLE_INITIAL) {
        scale = DEFAULT_SCALE;
        if (task->spawnArg1.value & SIZE_MASK) {
            scale = task->spawnArg1.halves.low & SIZE_MASK;
        }
        effect->scale = scale;
        RANDOM_LCG_ADVANCE();
        effect->angle = RANDOM_LCG_DRAW() & ACTOR_TRANSFORM_ANGLE_MASK;
        if (task->spawnArg1.value & PERIOD_MASK) {
            framePeriod = (task->spawnArg1.value >> PERIOD_SHIFT) & MAX_PERIOD;
        } else {
            framePeriod = DEFAULT_PERIOD;
        }
        effect->period = framePeriod;
        effect->step   = (s32)((u16)effect->scale << 16) >> 23;
        optionsByte    = task->spawnArg1.signedBytes[3];
        effect->index  = optionsByte & MOTION_MASK;
        // Nonzero motion selects drift in the borrowed parent's local basis.
        if (effect->index != 0) {
            MAKE_FIREBALL_DRIFT(effect);
        } else if (!(task->spawnArg1.value & SUPPRESS_CHILDREN)) {
            childCount = gDisplayState.animFrame & 3;
            childIndex = 0;
            if (childCount != 0) {
                do {
                    childEffect = effectSpawn(EFFECT_NO9_EXPLOSION_FIREBALL, coord, ((s32)((u16)effect->scale << 16) >> 17) | FAST_CHILD_OPTIONS, NULL);
                    if (childEffect != NULL) {
                        taskReparent(task, childEffect->task);
                    }
                    childIndex += 1;
                } while (childIndex < childCount);
            }
            childCount = gDisplayState.animFrame & 1;
            childIndex = 0;
            if (childIndex < childCount) {
                do {
                    childEffect = effectSpawn(EFFECT_NO9_EXPLOSION_FIREBALL, coord, ((s32)((u16)effect->scale << 16) >> 17) | SLOW_CHILD_OPTIONS, NULL);
                    if (childEffect != NULL) {
                        taskReparent(task, childEffect->task);
                    }
                    childIndex += 1;
                } while (childIndex < childCount);
            }
        }
        effect->age--;
        task->state = ACTIVE;
    }
    // Draw before advancing translation and the signed-halfword size.
    spriteQuadDraw(coord, effect->age / effect->period, effect->scale, effect->angle);
    coord->coord.t[0]  += effect->move.vx;
    coord->coord.t[1]  += effect->move.vy;
    coord->coord.t[2]  += effect->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    effect->scale      += effect->step;
    if (effect->age > effect->period * (ARRAY_SIZE(gEffectSpriteAtlasFrames) - 1) - 1) {
        effectKillTask(effect, task);
    }
}

#undef MAKE_FIREBALL_DRIFT

/// Packed additive texture page for the shared effect atlas, with this actor's palettes.
#define SPRITE_QUAD_TEXTURE_PAGE EFFECT_SPRITE_ATLAS_TEXTURE_PAGE
/// Fireball palette for atlas frame `frame`, packed as a GPU CLUT word.
#define SPRITE_QUAD_CLUT getClut(_gHelicopterLandingPadNo9FireballFramePalettes[frame].clutX, _gHelicopterLandingPadNo9FireballFramePalettes[frame].clutY)
/// UV-origin table for the next included sprite-quad drawer.
#define SPRITE_QUAD_UV_TABLE gEffectSpriteAtlasFrames
STATIC_ASSERT(ARRAY_SIZE(SPRITE_QUAD_UV_TABLE) == ARRAY_SIZE(_gHelicopterLandingPadNo9FireballFramePalettes), sprite_quad_uv_palette_frame_counts_match);

/// Texel width and height of the shared atlas cells used with this actor's palettes.
#define SPRITE_QUAD_CELL_WIDTH EFFECT_SPRITE_ATLAS_CELL_SIZE
/// Perspective-sizing multiplier for the shared atlas sprite.
#define SPRITE_QUAD_SCALE EFFECT_SPRITE_ATLAS_UV_SPAN
#include "../../../../shared/effects/sprite_quad_draw.inc.c"

void helicopterLandingPadNo9InitBody(Enemy* enemy, Task* task)
{
    enum {
        PROP_DESCRIPTOR          = 1,
        WEAPON_DESCRIPTOR        = 2,
        CHEST_DESCRIPTOR         = 3,
        TRANSFORMER_DESCRIPTOR   = 5,
        BLAST_SOURCE_DESCRIPTOR  = 6,
        BODY_INITIAL_ANIMATION   = 1,
        BODY_TASK_RUNNING        = 1,
        HIT_EFFECT_ARGUMENT_LOW  = 1024,
        HIT_EFFECT_ARGUMENT_HIGH = 2,
        BODY_CONTACT_KEY         = WORLD_COLLISION_CONTACT_ENEMY_BODY | 27,
        BODY_RADIUS              = 450,
        WEAPON_ATTACK_OFFSET_X   = -320,
        WEAPON_ATTACK_OFFSET_Y   = 128,
        ATTACK_RADIUS            = 400,
    };
    TmdObject*                   bodyModel;
    GfxCoord*                    rootCoord;
    HelicopterLandingPadNo9Work* work;
    Enemy*                       childEnemy;
    EffectWork*                  flameEffect;
    WorldCollisionContact*       bodyContacts;
    WorldCollisionContact*       attackContacts;
    s32                          slotIndex;

    bodyModel = task->extra.tmd;
    rootCoord = bodyModel->coords;
    work      = memCalloc(sizeof(HelicopterLandingPadNo9Work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    bodyModel->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    bodyModel->lightMtx     = &work->light;
    bodyModel->colorMtx     = &work->color;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[CHEST_PART];
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &gHelicopterLandingPadNo9EnemyParams;
    enemy->recs                   = work->bodyContacts;
    enemy->hp                     = gHelicopterLandingPadNo9EnemyParams.hpMax;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[CHEST_PART];
    work->hitEffectArg.spawnArgLo = HIT_EFFECT_ARGUMENT_LOW;
    work->hitEffectArg.spawnArgHi = HIT_EFFECT_ARGUMENT_HIGH;
    animationInitContext(&work->rig.anim, gHelicopterLandingPadNo9BodyAnimationSets, bodyModel,
                         work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, BODY_INITIAL_ANIMATION);
    }
    work->present = 1;
    enemySpawnFromTable(gHelicopterLandingPadNo9TaskDescriptors, PROP_DESCRIPTOR, 0, enemy);
    childEnemy = enemySpawnFromTable(gHelicopterLandingPadNo9TaskDescriptors, WEAPON_DESCRIPTOR, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(childEnemy->task->extra.tmd, enemy);
    work->weaponTask = childEnemy->task;
    flameEffect      = effectSpawn((EFFECT_HELICOPTER_LANDING_PAD_NO_9_FLAME_JET | EFFECT_SPAWN_UNLIMITED), childEnemy->task->extra.tmd->coords, 0, NULL);
    if (flameEffect != NULL) {
        work->flameJetTask = flameEffect->task;
        taskReparent(task, flameEffect->task);
    }
    if (work->flameJetTask != NULL) {
        work->flameJetTask->spawnArg1.value = HELICOPTER_LANDING_PAD_NO_9_FLAME_OFF;
    }
    childEnemy = enemySpawnFromTable(gHelicopterLandingPadNo9TaskDescriptors, CHEST_DESCRIPTOR, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(childEnemy->task->extra.tmd, enemy);
    work->chestModelTask = childEnemy->task;
    enemySpawnFromTable(gHelicopterLandingPadNo9TaskDescriptors, TRANSFORMER_DESCRIPTOR, 0, enemy);
    enemySpawnFromTable(gHelicopterLandingPadNo9TaskDescriptors, TRANSFORMER_DESCRIPTOR, 1, enemy);
    enemySpawnFromTable(gHelicopterLandingPadNo9TaskDescriptors, TRANSFORMER_DESCRIPTOR, 2, enemy);
    enemySpawnFromTable(gHelicopterLandingPadNo9TaskDescriptors, BLAST_SOURCE_DESCRIPTOR, 0, enemy);
    work->body.coord            = &task->extra.tmd->coords[CHEST_PART];
    bodyContacts                = work->bodyContacts;
    work->body.context.contacts = bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = BODY_CONTACT_KEY;
    work->body.radius           = BODY_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    attackContacts                      = work->attackContacts;
    work->body.flags                   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->weaponAttack.coord            = work->weaponTask->extra.tmd->coords;
    work->weaponAttack.context.contacts = attackContacts;
    work->weaponAttack.pos.vx           = WEAPON_ATTACK_OFFSET_X;
    work->weaponAttack.pos.vy           = WEAPON_ATTACK_OFFSET_Y;
    work->weaponAttack.pos.vz           = 0;
    work->weaponAttack.key              = 0;
    work->weaponAttack.radius           = ATTACK_RADIUS;
    work->weaponAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->weaponAttack);
    worldCollisionInitContacts(attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->weaponAttack.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->forearmAttack.coord            = &task->extra.tmd->coords[RIGHT_FOREARM_PART];
    work->forearmAttack.context.contacts = attackContacts;
    work->forearmAttack.pos.vx           = 0;
    work->forearmAttack.pos.vy           = 0;
    work->forearmAttack.pos.vz           = 0;
    work->forearmAttack.key              = 0;
    work->forearmAttack.radius           = ATTACK_RADIUS;
    work->forearmAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->forearmAttack);
    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->msgTable             = gHelicopterLandingPadNo9BodyMessages;
    task->exitCallback         = helicopterLandingPadNo9ExitBody;
    helicopterLandingPadNo9RestoreGridFaces(task);
    helicopterLandingPadNo9SetExtraGridFace(true);
    task->state = BODY_TASK_RUNNING;
}

void helicopterLandingPadNo9TickEvent(Enemy* enemy, Task* task)
{
    enum {
        FRAME       = 210,
        FRAMES      = 255,
        START_SOUND = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_FIRE),
        LOOP_SOUND  = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_EVENT_FLAME_LOOP),
    };
    GfxCoord*                    rootCoord;
    HelicopterLandingPadNo9Work* work;
    s32                          startSound;
    s32                          startPan;
    s32                          loopPan;
    s32                          slotIndex;

    work                          = task->work;
    rootCoord                     = task->extra.tmd->coords;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->animationId == BODY_ANIMATION_EVENT_5 && work->animationFrame == FRAME) {
        work->flameMode   = HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING;
        work->flameFrames = FRAMES;
        startSound        = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | START_SOUND;
        startPan          = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(startSound, startPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        work->eventFlameSound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | LOOP_SOUND;
        loopPan               = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(work->eventFlameSound, loopPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
    work->animationFrame++;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    helicopterLandingPadNo9UpdateLighting(task, rootCoord);
    // Publish changed flame modes to the adopted jet after updating the body pose.
    if (work->flameMode != work->sentFlameMode) {
        if (work->flameJetTask != NULL) {
            work->flameJetTask->spawnArg1.value = work->flameMode;
        }
        work->sentFlameMode = work->flameMode;
    }
}
