#include "m4a1_hammer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/gtemac.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/effects.h>
#include <gameplay/hud_sprites.h>
#include <gameplay/light.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_coords.h>
#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/player_animation.h>
#include <gameplay/world_collision_types.h>

#include <engine/coord.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>
#include <engine/mc_types.h>
#include <engine/session_types.h>
#include <engine/wipsys.h>
#include <engine/wipsys_types.h>
#include <engine/sound_ids.h>

#include <weapons/weapon/sounds.h>
#include <weapons/weapon/weapon.h>
#include <weapons/weapon/weapon_index.h>

/// Signed texture-frame counter for the six-cell sprite's forward declaration.
#define SPRITE_QUAD_FRAME_T s16
#include <effects/sprite_quad/sprite_quad.h>
#include <effects/beam_strip/beam_strip.h>
#include <weapons/attack/attack.h>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 8)
PE2_OPTION_DEF(BURST_ROUNDS, int, 3)
PE2_OPTION_DEF(BURST_CANCEL_FRAMES, int, 9)
PE2_OPTION_DEF(SECONDARY_CANCEL_FRAMES, int, 0x1C)
PE2_OPTION_DEF(BURST_DELAY_FRAMES, int, 3)
PE2_OPTION_DEF(RECOVERY_COOLDOWN_FRAMES, int, 0xC)
PE2_OPTION_DEF(RIFLE_RADIUS, int, 0x100)
PE2_OPTION_DEF(DISCHARGE_RADIUS, int, 0x400)
PE2_OPTION_DEF(SECONDARY_ACTION_FRAMES, int, 0x14)
PE2_OPTION_DEF(DISCHARGE_REACH, int, 2560)
PE2_OPTION_DEF(BURST_IMPACT_FRAMES_LEFT, int, 2)
PE2_OPTION_DEF(DISCHARGE_WINDUP_FRAMES, int, 3)
PE2_OPTION_DEF(DISCHARGE_CONTACT_FRAMES_LEFT, int, 1)
/// The attachment's charge glow (`m4a1HammerGlowTask`): times in dispatch ticks, light radii in game units.
PE2_OPTION_DEF(LIGHT_REFRESH_FRAMES, int, 4)
PE2_OPTION_DEF(CHARGED_FRAMES, int, 5)
PE2_OPTION_DEF(IDLE_LIGHT_INNER, int, 0x80)
PE2_OPTION_DEF(IDLE_LIGHT_OUTER, int, 0x400)
PE2_OPTION_DEF(CHARGED_LIGHT_INNER, int, 0x400)
PE2_OPTION_DEF(CHARGED_LIGHT_OUTER, int, 0x4000)
/// Signed perspective-sizing numerator for the attachment and contact beams.
PE2_OPTION_DEF(BEAM_WIDTH_SCALE, int, 0x280)
/// Perspective-sizing numerator.
PE2_OPTION_DEF(IMPACT_SPRITE_SIZE, int, 0x400)
PE2_OPTION_DEF(IMPACT_BEAM_END_AGE, int, 8)
PE2_OPTION_DEF(IMPACT_LIFETIME, int, 25)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gM4a1HammerModel)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gM4a1HammerPlayerAnimationBank)

/// A sound of the M4A1 Hammer's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M4A1_HAMMER, (slot))

/// The M4A1 Hammer's own slots.
enum {
    SOUND_DISCHARGE = 5,
};

/// The contacts' keys and what else is wiring.
enum {
    RIFLE_KEY_BASE            = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_M4A1_HAMMER << 8),
    DISCHARGE_KEY             = WORLD_COLLISION_CONTACT_ATTACK | (WEAPON_INDEX_M4A1_HAMMER << 8) | 0x1C,
    SECONDARY_TURN_RATE_INDEX = 2,
};

/// The attachment's charge glow (`m4a1HammerGlowTask`): times in dispatch ticks, light radii in game units.
enum {
    LIGHT_SLOT     = 1,
    SPIN_TICK_MASK = 15,
};

/// Charge-flare instance of `spriteQuadDraw`.
///
/// `pos` is three cached translation words. `frame` walks the eight-cell strip,
/// `size` is the unsigned perspective numerator and `angle` is the spin in
/// 4096 units per turn.
static void spriteQuadDrawCharge(const long* pos, u16 frame, u16 size, s16 angle);

/// Eight charged sparks stored in contiguous angle, height and radius banks.
enum {
    SPARK_COUNT       = 8,
    SPARK_HEIGHT_BASE = SPARK_COUNT,
    SPARK_RADIUS_BASE = SPARK_COUNT * 2,
};

/// Retained cylindrical offsets of eight sparks, reseeded at the start of a charged burst.
///
/// Bank starts are 0 for angles, `SPARK_HEIGHT_BASE` for heights
/// and `SPARK_RADIUS_BASE` for radii; each has `SPARK_COUNT`
/// entries. Angles use 4096 units per turn without normalization. Heights and
/// radii use weapon-local integer coordinates; radii stay fixed at 0..1023.
/// Every other active age tick drifts angles and raises heights, retaining
/// signed-halfword wrap on each store. Storage lasts for the loaded overlay.
static s16 _gSparkState[SPARK_COUNT * 3];

/// Borrowed live attachment glow task that owns contact flashes' teardown.
///
/// Published when the glow attaches. Contact spawning requires it to remain
/// live; releasing the glow does not clear this retained pointer.
static Task* _gGlowTask;

/// Cached attachment origin shared by charged sparks and contact beams.
///
/// Signed low halfwords of the glow's composed translation, published before
/// that update recomposes it. Uses composition-root space, including the view
/// transform; the beam drawer projects it through `GsWSMATRIX`. Storage lasts
/// for the loaded Hammer overlay; updates preserve the fourth halfword.
static SVECTOR _gAttachmentOrigin;

/// Attachment glow origin in weapon-local integer coordinates.
static SVECTOR _gGlowLocalOffset = { 0, 0x280, 0x20, 0 };

void m4a1HammerGlowTask(Task* task)
{
    enum {
        GLOW_TASK_ATTACH = 0,
        GLOW_TASK_UPDATE = 1,
        ANGLE_MASK       = 0xFFF, // 4096 angle units per turn
        TRIG_SHIFT       = 12,    // rsin/rcos: 4096 represents 1.0
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
    transientLight = &gWorldCoordTransientPointLights[LIGHT_SLOT];
    lightCoord     = &transientLight->light.head.transform.coord;
    pointLight     = &transientLight->light;

    if (((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) == 0 && gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age = work->age + 1;
        switch (task->state) {
            case GLOW_TASK_ATTACH:
                // Keep the flare at the weapon's fixed local offset.
                glowCoord->parent = work->parent;
                gfxSetRotIdentity(&glowCoord->coord);
                glowCoord->coord.t[0]   = _gGlowLocalOffset.vx;
                glowCoord->coord.t[1]   = _gGlowLocalOffset.vy;
                glowCoord->coord.t[2]   = _gGlowLocalOffset.vz;
                glowCoord->composeStamp = GRAPHICS_COORD_DIRTY;

                _gGlowTask = task;
                actorRenderComposeCoord(glowCoord);
                task->state = GLOW_TASK_UPDATE;
                return;
            case GLOW_TASK_UPDATE:
                // Publish the existing cache before composition; contact beams share its low halves.
                _gAttachmentOrigin.vx = glowCoord->workm.t[0];
                _gAttachmentOrigin.vy = glowCoord->workm.t[1];
                _gAttachmentOrigin.vz = glowCoord->workm.t[2];
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
                        if ((work->age & SPIN_TICK_MASK) == 0) {
                            RANDOM_LCG_ADVANCE();
                            work->angle = RANDOM_LCG_DRAW() & ANGLE_MASK;
                        }
                        RANDOM_LCG_ADVANCE();
                        work->period = (RANDOM_LCG_DRAW() & 0xFF) + 0xC0;
                        if ((work->age & 1) == 0) {
                            spriteQuadDrawCharge(glowCoord->workm.t, work->age >> 1, work->period,
                                                 work->angle);
                        }
                        transientLight->framesLeft = PE2_OPTION(LIGHT_REFRESH_FRAMES);
                        pointLight->inner          = PE2_OPTION(IDLE_LIGHT_INNER);
                        pointLight->outer          = PE2_OPTION(IDLE_LIGHT_OUTER);
                        RANDOM_LCG_ADVANCE();
                        pointLight->head.color.b = (RANDOM_LCG_DRAW() & 0x700) + 0x400;
                        pointLight->head.color.r = (u16)pointLight->head.color.b >> 1;
                        pointLight->head.color.g = (u16)pointLight->head.color.b >> 1;
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
                            for (sparkIndex = 0; sparkIndex < SPARK_COUNT; sparkIndex++) {
                                RANDOM_LCG_ADVANCE();
                                _gSparkState[sparkIndex] = (sparkIndex << 9) + (RANDOM_LCG_DRAW() & 0x1FF);
                                RANDOM_LCG_ADVANCE();
                                _gSparkState[sparkIndex + SPARK_HEIGHT_BASE] = (RANDOM_LCG_DRAW() & 0x7FF) + 0x200;
                                RANDOM_LCG_ADVANCE();
                                _gSparkState[sparkIndex + SPARK_RADIUS_BASE] = RANDOM_LCG_DRAW() & 0x3FF;
                            }
                        }
                        if ((work->age & SPIN_TICK_MASK) == 0) {
                            RANDOM_LCG_ADVANCE();
                            work->angle = RANDOM_LCG_DRAW() & ANGLE_MASK;
                        }
                        RANDOM_LCG_ADVANCE();
                        work->period = (RANDOM_LCG_DRAW() & 0x3FF) + 0x400;
                        if ((work->age & 1) == 0) {
                            spriteQuadDraw(glowCoord, work->age >> 1, work->period, work->angle);
                            for (sparkIndex = 0; sparkIndex < SPARK_COUNT; sparkIndex++) {
                                heightIndex = sparkIndex + SPARK_HEIGHT_BASE;
                                RANDOM_LCG_ADVANCE();
                                _gSparkState[sparkIndex] -= (RANDOM_LCG_DRAW() & 0x1FF) - 0x100;
                                RANDOM_LCG_ADVANCE();
                                _gSparkState[heightIndex] += RANDOM_LCG_DRAW() & 0xFF;
                                work->pos.vx =
                                    (_gSparkState[sparkIndex + SPARK_RADIUS_BASE] * rsin(_gSparkState[sparkIndex])) >> TRIG_SHIFT;
                                work->pos.vz =
                                    (_gSparkState[sparkIndex + SPARK_RADIUS_BASE] * rcos(_gSparkState[sparkIndex])) >> TRIG_SHIFT;
                                work->pos.vy = _gSparkState[heightIndex];
                                // Rotates a charged spark offset and adds the cached attachment origin in place.
                                gte_ApplyMatrixSV(&glowCoord->workm, &work->pos, &work->pos);
                                work->pos.vx = work->pos.vx + (u16)_gAttachmentOrigin.vx;
                                work->pos.vy = work->pos.vy + (u16)_gAttachmentOrigin.vy;
                                work->pos.vz = work->pos.vz + (u16)_gAttachmentOrigin.vz;
                                _beamStripDraw(glowCoord, &work->pos, work->age, PE2_OPTION(BEAM_WIDTH_SCALE));
                            }
                        }
                        transientLight->framesLeft = PE2_OPTION(LIGHT_REFRESH_FRAMES);
                        pointLight->inner          = PE2_OPTION(CHARGED_LIGHT_INNER);
                        pointLight->outer          = PE2_OPTION(CHARGED_LIGHT_OUTER);
                        RANDOM_LCG_ADVANCE();
                        pointLight->head.color.b = (RANDOM_LCG_DRAW() & 0x700) + 0x800;
                        pointLight->head.color.r = (u16)pointLight->head.color.b >> 1;
                        pointLight->head.color.g = pointLight->head.color.b >> 1;
                        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &glowCoord->workm, &lightCoord->coord);
                        lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->index              = work->index + 1;
                        if (work->index >= PE2_OPTION(CHARGED_FRAMES)) {
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
#include <effects/sprite_quad/draw.inc.c>
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
        IMPACT_TASK_ATTACH = 0,
        IMPACT_TASK_DRAW   = 1,
        IMPACT_ANGLE_MASK  = 0xFFF // 4096 angle units per turn
    };
    EffectWork* work;
    GfxCoord*   impactCoord;
    GfxCoord*   parent;

    work        = task->spawnArg2.pointer;
    impactCoord = task->extra.coordBody->coord;
    work->age++;
    switch (task->state) {
        case IMPACT_TASK_ATTACH:
            // Teardown follows the attachment glow; the transform follows the contact.
            taskReparent(_gGlowTask, task);
            if (task->spawnArg1.value != 0) {
                parent                    = work->parent;
                impactCoord->coord.t[0]   = 0;
                impactCoord->coord.t[1]   = 0;
                impactCoord->coord.t[2]   = 0;
                impactCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                impactCoord->parent       = parent;
                actorRenderComposeCoord(impactCoord);
                task->state = IMPACT_TASK_DRAW;
            }
            work->scale = 0x80;
            RANDOM_LCG_ADVANCE();
            work->angle = RANDOM_LCG_DRAW() & IMPACT_ANGLE_MASK;
            /* fallthrough */
        case IMPACT_TASK_DRAW:
            if (work->age & 1) {
                spriteQuadDraw(impactCoord, ++work->index, PE2_OPTION(IMPACT_SPRITE_SIZE), work->angle);
                if (work->age < PE2_OPTION(IMPACT_BEAM_END_AGE)) {
                    _beamStripDraw(impactCoord, &_gAttachmentOrigin, work->index, PE2_OPTION(BEAM_WIDTH_SCALE));
                }
            }
            if (work->age >= PE2_OPTION(IMPACT_LIFETIME)) {
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
#include <effects/sprite_quad/draw.inc.c>

#include <effects/beam_strip/draw.inc.c>

/// The model and the player's clips: here, as the package stores them after the data above.
PE2_VALUE(model)
PE2_VALUE(bank)

void m4a1HammerAttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE       = 0,
        PHASE_WAIT_READY    = 1,
        PHASE_SELECT_ATTACK = 2,
        PHASE_BURST         = 3,
        PHASE_IMPACT        = 4,
        PHASE_DISCHARGE     = 5,
        PHASE_RECOVER       = 6,
    };

    GameActor*             actor;
    GfxCoord*              rootCoord;
    GfxCoord*              impactCoord;
    WorldCollisionCapsule* weaponShape;
    Task*                  glowTask;
    s32                    readyBlendFrames;
    s32                    phaseDelay;
    u16                    collisionFlags;

    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    weaponShape = &actor->weaponShape;
    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            readyBlendFrames                                      = PE2_OPTION(READY_BLEND_FRAMES);
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += readyBlendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = PE2_OPTION(MOVING_READY_BLEND_FRAMES);
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PHASE_SELECT_ATTACK:
            actor->rumblePosted = 0;
            if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase                                     = PHASE_BURST;
                actor->turnRateIndex                                  = 0;
                actor->stateTimer                                     = 0;
                actor->attackCancelTicks                              = PE2_OPTION(BURST_CANCEL_FRAMES);
                actor->actionValue                                    = PE2_OPTION(BURST_ROUNDS);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | RIFLE_KEY_BASE;
                weaponShape->end0Radius                               = PE2_OPTION(RIFLE_RADIUS);
                weaponShape->end1Radius                               = PE2_OPTION(RIFLE_RADIUS);
                weaponShape->ends[0].vz                               = weaponShape->ends[1].vz + gPlayerActorWeaponReaches[0x19];
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            } else if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase                                     = PHASE_DISCHARGE;
                actor->turnRateIndex                                  = SECONDARY_TURN_RATE_INDEX;
                actor->attackCancelTicks                              = PE2_OPTION(SECONDARY_CANCEL_FRAMES);
                actor->actionValue                                    = PE2_OPTION(SECONDARY_ACTION_FRAMES);
                actor->stateTimer                                     = PE2_OPTION(DISCHARGE_WINDUP_FRAMES);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = DISCHARGE_KEY;
                weaponShape->end0Radius                               = PE2_OPTION(DISCHARGE_RADIUS);
                weaponShape->end1Radius                               = PE2_OPTION(DISCHARGE_RADIUS);
                weaponShape->ends[0].vz                               = weaponShape->ends[1].vz + PE2_OPTION(DISCHARGE_REACH);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                glowTask = actor->weaponEffectTask;
                if (glowTask != NULL) {
                    glowTask->spawnArg1.value = M4A1_HAMMER_GLOW_CHARGED;
                }
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_HAMMER), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_DISCHARGE), 1);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE, 0, 2);
                break;
            }
            /* fallthrough */
        case PHASE_BURST:
            if (actor->actionValue != 0) {
                phaseDelay = actor->stateTimer;
                if (phaseDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = PE2_OPTION(BURST_DELAY_FRAMES);
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_HAMMER), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_INDEX_M4A1_HAMMER, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 2);
                    break;
                }
                actor->stateTimer = phaseDelay - 1;
                if (phaseDelay - 1 == PE2_OPTION(BURST_IMPACT_FRAMES_LEFT)) {
                    _weaponResolveShotImpact(actor, rootCoord, impactCoord);
                }
                break;
            }
            /* fallthrough */
        case PHASE_IMPACT:
            actor->statePhase = PHASE_RECOVER;
            _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            break;
        case PHASE_DISCHARGE:
            // The discharge enables contacts for one tick before recovery.
            phaseDelay        = actor->stateTimer - 1;
            actor->stateTimer = phaseDelay;
            if (phaseDelay == PE2_OPTION(DISCHARGE_CONTACT_FRAMES_LEFT)) {
                collisionFlags                                       = actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags = collisionFlags;
            } else if (phaseDelay == 0) {
                actor->statePhase = PHASE_RECOVER;
                if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_SECONDARY) == 0) {
                    actor->weaponEffectTask->spawnArg1.value = M4A1_HAMMER_GLOW_OFF;
                }
                collisionFlags                                       = actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags = collisionFlags;
            }
            /* fallthrough */
        case PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = PE2_OPTION(RECOVERY_COOLDOWN_FRAMES);
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
