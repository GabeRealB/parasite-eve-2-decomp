#include "types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 gMistShootingGalleryUnscaledBp;

#include "stages/akropolis/rooms/mist_shooting_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "mist_shooting_gallery_private.h"

#include "stages/akropolis/actors/shooting_gallery_targets.h"
#include "stages/akropolis/actors/shooting_gallery_pierce.h"

#include "gameplay/actor.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

/// Signed texture-frame counter for the gallery's repeating six-cell sprite strip.
#define SPRITE_QUAD_FRAME_T s16
#include "../../../../shared/effects/sprite_quad.h"
#include "../../../../shared/effects/beam_strip.h"

#define ROOM_PART declarations_mist_shooting_gallery_courses
#include "assets/rooms/mist/shooting_gallery/declarations.inc"

/// Marker values of `_MistShootingGallerySpawn::frame`, above every frame the script clock can reach.
enum {
    SPAWN_FRAME_LIMIT = 0xFFF0, // Where the script clock saturates, so it never equals a marker
    SPAWN_WAIT_CLEAR  = 0xFFF1, // Hold the script here until no spawned target is left in play
    SPAWN_END         = 0xFFFF, // End of the script; nothing further spawns
};

static void   _mistShootingGalleryExitCourse(Task* task);
static void   _mistShootingGalleryDrawCountdownClock(MistShootingGalleryWork* work);
static u16    _mistShootingGalleryTickCourseClock(MistShootingGalleryWork* work);
static void   _mistShootingGalleryShowCaption(s16 commandIndex, s16 captionKey, s16 dismissButtons);
static Enemy* _mistShootingGallerySpawnTarget(Task* controller, const _MistShootingGallerySpawn* spawn);
static void   _mistShootingGalleryDrawClockGlyph(s32 screenX, s16 screenY, s32 glyph);
static void   _mistShootingGalleryDrawRedFlash(u8 redIntensity);
static void   _mistShootingGalleryInitCourse(Task* controller);
static void   _mistShootingGalleryRunCourse0(Task* controller);
static void   _mistShootingGalleryRunCourse1(Task* controller);
static void   _mistShootingGalleryRunCourse2(Task* controller);
static void   _mistShootingGalleryRunCourse3(Task* controller);
static void   _mistShootingGalleryRunCourse4(Task* controller);
static void   _mistShootingGalleryFinishCourse(Task* controller);
static void   _mistShootingGalleryRunCourse(Task* controller);

/// Controller state handlers: 0 initialize, 1 run the course, 2 finish and show results.
static const TaskFuncTable3 _gMistShootingGalleryControllerStates = {
    { _mistShootingGalleryInitCourse, _mistShootingGalleryRunCourse, _mistShootingGalleryFinishCourse },
};

/// Course handlers indexed by the controller work's zero-based course, 0..4.
static const TaskFuncTable5 _gMistShootingGalleryCourseHandlers = {
    {
        _mistShootingGalleryRunCourse0,
        _mistShootingGalleryRunCourse1,
        _mistShootingGalleryRunCourse2,
        _mistShootingGalleryRunCourse3,
        _mistShootingGalleryRunCourse4,
    },
};

static void _mistShootingGalleryCaptionModeTask(Task* task);

static void _mistShootingGalleryCourseTask(Task* task);
static void _mistShootingGalleryRedFlashTask(Task* task);

/// Buttons that dismiss ordinary course captions; special prompts choose one button.
enum {
    CAPTION_DISMISS_BUTTONS = PAD_BUTTON_START | PAD_BUTTON_CIRCLE | PAD_BUTTON_CROSS | PAD_BUTTON_SQUARE,
};

TaskDesc gMistShootingGalleryTaskDescs[MIST_SHOOTING_GALLERY_TASK_COUNT] = {
    [MIST_SHOOTING_GALLERY_TASK_COURSE_CONTROLLER] = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistShootingGalleryCourseTask, { .value = 0 } },
    [MIST_SHOOTING_GALLERY_TASK_RED_FLASH]         = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistShootingGalleryRedFlashTask, { .value = 0 } },
};

/// Modal caption task descriptor, with no body and priority 192.
static TaskDesc _gMistShootingGalleryCaptionModeTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistShootingGalleryCaptionModeTask, { .value = 0 } };

#include "assets/rooms/mist/shooting_gallery/waves.inc"
/// One pointer slot selecting course 0's finite wave script.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse0WaveScriptRefs[1] = {
    _gMistShootingGalleryCourse0WaveScript,
};

/// Borrowed script pointer for course 1's finite target waves.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse1WaveScriptRef = _gMistShootingGalleryCourse1WaveScript;

/// Borrowed wave-script pointer for zero-based course 2's GPS tutorial.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse2WaveScriptRef = _gMistShootingGalleryCourse2WaveScript;

/// Borrowed wave-script pointer for zero-based course 3's weapon-swap tutorial.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse3WaveScriptRef = _gMistShootingGalleryCourse3WaveScript;

/// Borrowed wave-script pointer for zero-based course 4's combat tutorial.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse4WaveScriptRef = _gMistShootingGalleryCourse4WaveScript;

#include "assets/rooms/mist/shooting_gallery/creature_target_1.inc"
#include "assets/rooms/mist/shooting_gallery/creature_target_2.inc"
#include "assets/rooms/mist/shooting_gallery/creature_target_3.inc"
#include "assets/rooms/mist/shooting_gallery/creature_target_4.inc"
#include "assets/rooms/mist/shooting_gallery/creature_target_5.inc"
#include "assets/rooms/mist/shooting_gallery/creature_target_6.inc"
#include "assets/rooms/mist/shooting_gallery/creature_target_7.inc"
#include "assets/rooms/mist/shooting_gallery/creature_target_8.inc"
#include "assets/rooms/mist/shooting_gallery/creature_target_9.inc"
#include "assets/rooms/mist/shooting_gallery/creature_target_10.inc"
#include "assets/rooms/mist/shooting_gallery/mount.inc"
#include "assets/rooms/mist/shooting_gallery/half_target_1.inc"
#include "assets/rooms/mist/shooting_gallery/half_target_2.inc"
#include "assets/rooms/mist/shooting_gallery/half_target_3.inc"
#include "assets/rooms/mist/shooting_gallery/half_target_4.inc"
#include "assets/rooms/mist/shooting_gallery/bullseye_1.inc"
#include "assets/rooms/mist/shooting_gallery/bullseye_2.inc"
#include "assets/rooms/mist/shooting_gallery/bullseye_3.inc"
#include "assets/rooms/mist/shooting_gallery/demonstration_target.inc"

// Keep the read-only table at its original position among the overlay's data.

#define ROOM_PART _gMistShootingGalleryCollisionGridNormals
#include "assets/rooms/mist/shooting_gallery/geometry.inc"
#include "assets/rooms/mist/shooting_gallery/cameras.inc"
#include "assets/rooms/mist/shooting_gallery/sprites.inc"
#include "assets/rooms/mist/shooting_gallery/view_boundaries.inc"
#include "assets/rooms/mist/shooting_gallery/action_triggers.inc"
#include "assets/rooms/mist/shooting_gallery/lights.inc"
#include "assets/rooms/mist/shooting_gallery/placements.inc"
#include "assets/rooms/mist/shooting_gallery/ambient.inc"
#include "assets/rooms/mist/shooting_gallery/surfaces.inc"
s32 gMistShootingGalleryUnscaledExp = 0;

Task* gMistShootingGalleryControllerTask;

void mistShootingGalleryTracerTask(Task* task)
{
    enum {
        INIT               = 0,
        FADE               = 1,
        HELD_SIZE          = 0x600,
        RUNNING_SIZE       = 0x400,
        HORIZONTAL_MASK    = 0x3FF,
        HORIZONTAL_RADIUS  = 0x200,
        VERTICAL_OFFSET    = 0x800,
        INITIAL_BRIGHTNESS = 0x80,
        FADE_STEP          = 8,
    };

    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];
    u32         randomX;
    u32         randomZ;
    u32         randomAngle;

    /// Queues an additive blue tint at the effect's current brightness.
#define DRAW_TRACER_TINT(effectWork, colorBytes) \
    (colorBytes)[0] = (effectWork)->scale >> 1;  \
    (colorBytes)[1] = (effectWork)->scale >> 1;  \
    (colorBytes)[2] = (effectWork)->scale;       \
    effectDrawScreenTint((colorBytes), GPU_BLEND_ADD)

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        spriteQuadDraw(coord, work->index, HELD_SIZE, work->angle);
        _beamStripDraw(coord, &work->pos, work->index, HELD_SIZE);
        DRAW_TRACER_TINT(work, rgb);
        return;
    }

    work->age++;
    switch (task->state) {
        case INIT:
            // Attach the origin, but seed the endpoint from the existing world cache.
            coord->parent       = work->parent;
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = 0;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = FADE;

            RANDOM_LCG_ADVANCE();
            randomX      = gRandomLcgState;
            work->pos.vx = (u16)coord->workm.t[0] - ((randomX >> 16 & HORIZONTAL_MASK) - HORIZONTAL_RADIUS);
            RANDOM_LCG_ADVANCE();
            randomZ      = gRandomLcgState;
            work->pos.vy = coord->workm.t[1] - VERTICAL_OFFSET;
            RANDOM_LCG_ADVANCE();
            randomAngle  = gRandomLcgState;
            work->pos.vz = (u16)coord->workm.t[2] - ((randomZ >> 16 & HORIZONTAL_MASK) - HORIZONTAL_RADIUS);
            work->scale  = INITIAL_BRIGHTNESS;
            work->angle  = randomAngle >> 16 & ACTOR_TRANSFORM_ANGLE_MASK;
        case FADE:
            // Only odd running ticks advance the texture frame and draw the tracer.
            if (work->age & 1) {
                spriteQuadDraw(coord, ++work->index, RUNNING_SIZE, work->angle);
                _beamStripDraw(coord, &work->pos, work->index, RUNNING_SIZE);
            }
            DRAW_TRACER_TINT(work, rgb);
            work->scale -= FADE_STEP;
            if (work->scale < FADE_STEP) {
                effectKillTask(work, task);
            }
            return;
    }
#undef DRAW_TRACER_TINT
}

/// Texel width and horizontal stride of each cell in the gallery flash's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the gallery flash's texture row, repeated by its frame counter.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of the gallery flash strip, relative to its texture page.
#define SPRITE_QUAD_TOP_V 0x38
/// Inclusive bottom texel row of the gallery sprite's square cells, relative to its texture page.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
/// Perspective-sizing multiplier for the gallery effect sprite.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
/// Muzzle-flash palette: VRAM X=304 words, Y=266 scanlines.
#define SPRITE_QUAD_CLUT getClut(304, 266)
/// Uses the gallery flash's unmodified projected depth for sizing and sorting.
#define SPRITE_QUAD_INCREMENT_DEPTH 0
#include "../../../../shared/effects/sprite_quad_draw.inc.c"

/// Uses the unmodified start depth for the gallery tracer's width and sorting.
#define BEAM_STRIP_INCREMENT_DEPTH 0
#include "../../../../shared/effects/beam_strip_draw.inc.c"

/// Allocates and publishes one course's controller work and prepares player control.
static void _mistShootingGalleryInitCourse(Task* controller)
{
    enum {
        COURSE_ARGUMENT_MASK  = 0xF,
        CLOCK_INITIAL_X       = -220,
        FIRST_MOVEMENT_COURSE = 2,
        FIRST_SWAP_COURSE     = 3,
    };
    Task*                    playerTask;
    GameActor*               player;
    MistShootingGalleryWork* work;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player     = playerTask->work;

    work             = memCalloc(sizeof(*work), false);
    controller->work = work;
    if (work == NULL) {
        taskKill(controller);
        return;
    }

    gMistShootingGalleryControllerTask = controller;
    controller->exitCallback           = _mistShootingGalleryExitCourse;
    controller->state++;
    work->course = controller->spawnArg1.value & COURSE_ARGUMENT_MASK;
    work->clockX = CLOCK_INITIAL_X;

    // Extend the weapon capsule in the firing lane before entering locomotion.
    player->weaponShape.ends[0].vz =
        (player->weaponShape.ends[1].vz + gPlayerActorWeaponReaches[gPlayerStatus.weapon]) << 1;
    playerActorEnterLocomotion(playerTask, 1);

    // The early courses restrict swapping; the first two also hold movement.
    if (work->course < FIRST_SWAP_COURSE) {
        gAttachmentState.flags |= ATTACHMENT_FLAG_SWAP_LOCK;
        if (work->course < FIRST_MOVEMENT_COURSE) {
            player->movementInputDisabled = 1;
            displayAcquireMenuHold();
        }
    }
    gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
    gGameSession->battleResetPending            = 0;
    (sceneAcquireBattleRef)(0);
}

/// Runs zero-based course 0's lock-on tutorial and one-minute target sequence.
static void _mistShootingGalleryRunCourse0(Task* controller)
{
    enum {
        COURSE_0_INITIALIZE                    = 0,
        COURSE_0_INTRO_CAPTION                 = 1,
        COURSE_0_LAYOUT_CAPTION                = 2,
        COURSE_0_INSTRUCTIONS_CAPTION          = 3,
        COURSE_0_LAYOUT_INSTRUCTIONS           = 4,
        COURSE_0_READY_DELAY                   = 5,
        COURSE_0_FIRST_TARGET                  = 6,
        COURSE_0_WAIT_FOR_LOCK                 = 7,
        COURSE_0_SECOND_TARGET                 = 8,
        COURSE_0_TARGET_INSTRUCTIONS           = 9,
        COURSE_0_SINGLE_TARGETS                = 10,
        COURSE_0_WAIT_FOR_SINGLE_TARGETS       = 11,
        COURSE_0_SEED_TARGETS                  = 12,
        COURSE_0_WAVE_INSTRUCTIONS             = 13,
        COURSE_0_WAIT_FOR_SEED_TARGETS         = 14,
        COURSE_0_WAVES                         = 15,
        COURSE_0_AMMO_CAPTIONS                 = 16,
        COURSE_0_DURATION_FRAMES               = 1800,
        COURSE_0_LONG_DELAY_TICKS              = 60,
        COURSE_0_CAPTION_DELAY_TICKS           = 30,
        COURSE_0_TARGET_DELAY_TICKS            = 15,
        COURSE_0_RAPID_SPAWN_DELAY_TICKS       = 10,
        COURSE_0_CAPTION_COMMAND               = 17,
        COURSE_0_INTRO_KEY                     = 0,
        COURSE_0_LAYOUT_FIRST_KEY              = 1,
        COURSE_0_INSTRUCTIONS_KEY              = 4,
        COURSE_0_LAYOUT_INSTRUCTIONS_FIRST_KEY = 5,
        COURSE_0_LOCK_INSTRUCTIONS_FIRST_KEY   = 8,
        COURSE_0_TARGET_INSTRUCTIONS_FIRST_KEY = 11,
        COURSE_0_WAVE_INSTRUCTIONS_FIRST_KEY   = 14,
        COURSE_0_AMMO_FIRST_KEY                = 17,
        COURSE_0_AMMO_LAST_KEY                 = 21,
        COURSE_0_SINGLE_TARGET_END_INDEX       = 7,
        COURSE_0_SEED_TARGET_END_INDEX         = 12,
        COURSE_0_MIN_PRIMARY_LOAD              = 2,
        COURSE_0_M93R_SELECTION                = 2,
        COURSE_0_M93R_MIN_PRIMARY_LOAD         = 4,
    };
    MistShootingGalleryWork*         work;
    const _MistShootingGallerySpawn* spawn;
    s32                              minimumPrimaryLoad;
    s32                              buttonLayout;
    u16                              introFramesLeft;
    u16                              layoutFramesLeft;
    u16                              readyFramesBeforeDecrement;
    u16                              firstTargetFramesLeft;
    u16                              secondTargetFramesLeft;
    u16                              instructionsFramesLeft;
    u16                              seedFramesLeft;
    u16                              waveInstructionsFramesLeft;
    u16                              spawnFrame;
    u16                              scriptFrame;
    u16                              resumePhase;
    u8                               captionKey;

    work         = controller->work;
    buttonLayout = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_SELECT) != 0) {
        shootingGalleryPierceRequestGalleryAbort();
        return;
    }

    switch (work->phase) {
        case COURSE_0_INITIALIZE:
            work->timeLeft = COURSE_0_DURATION_FRAMES;
            work->timer    = COURSE_0_LONG_DELAY_TICKS;
            work->phase++;
        case COURSE_0_INTRO_CAPTION:
            introFramesLeft = work->timer - 1;
            work->timer     = introFramesLeft;
            if ((s32)(introFramesLeft << 16) <= 0) {
                work->timer = COURSE_0_CAPTION_DELAY_TICKS;
                work->phase++;
                _mistShootingGalleryShowCaption(COURSE_0_CAPTION_COMMAND, COURSE_0_INTRO_KEY, CAPTION_DISMISS_BUTTONS);
            }
            break;
        case COURSE_0_LAYOUT_CAPTION:
            layoutFramesLeft = work->timer - 1;
            work->timer      = layoutFramesLeft;
            if ((s32)(layoutFramesLeft << 16) <= 0) {
                work->phase++;
                _mistShootingGalleryShowCaption(COURSE_0_CAPTION_COMMAND, buttonLayout + COURSE_0_LAYOUT_FIRST_KEY, CAPTION_DISMISS_BUTTONS);
            }
            break;
        case COURSE_0_INSTRUCTIONS_CAPTION:
            work->phase++;
            _mistShootingGalleryShowCaption(COURSE_0_CAPTION_COMMAND, COURSE_0_INSTRUCTIONS_KEY, CAPTION_DISMISS_BUTTONS);
            break;
        case COURSE_0_LAYOUT_INSTRUCTIONS:
            work->timer = COURSE_0_CAPTION_DELAY_TICKS;
            work->phase++;
            _mistShootingGalleryShowCaption(COURSE_0_CAPTION_COMMAND, buttonLayout + COURSE_0_LAYOUT_INSTRUCTIONS_FIRST_KEY, CAPTION_DISMISS_BUTTONS);
            break;
        case COURSE_0_READY_DELAY:
            readyFramesBeforeDecrement = work->timer;
            work->timer                = readyFramesBeforeDecrement - 1;
            if ((s32)(readyFramesBeforeDecrement << 16) <= 0) {
                work->timer = COURSE_0_TARGET_DELAY_TICKS;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
            }
            break;
        // Seed tutorial targets before starting the timed wave script.
        case COURSE_0_FIRST_TARGET:
            firstTargetFramesLeft = work->timer - 1;
            work->timer           = firstTargetFramesLeft;
            if ((s32)(firstTargetFramesLeft << 16) <= 0) {
                work->phase++;
                spawn = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
                _mistShootingGallerySpawnTarget(controller, spawn);
                work->spawnIndex++;
            }
            break;
        case COURSE_0_WAIT_FOR_LOCK:
            if (work->targetLockMask != 0) {
                work->timer = COURSE_0_TARGET_DELAY_TICKS;
                work->phase++;
                _mistShootingGalleryShowCaption(COURSE_0_CAPTION_COMMAND, buttonLayout + COURSE_0_LOCK_INSTRUCTIONS_FIRST_KEY, CAPTION_DISMISS_BUTTONS);
            } else if (work->liveTargets == 0) {
                work->timer = COURSE_0_TARGET_DELAY_TICKS;
                work->phase++;
            }
            break;
        case COURSE_0_SECOND_TARGET:
            if (work->liveTargets == 0) {
                secondTargetFramesLeft = work->timer - 1;
                work->timer            = secondTargetFramesLeft;
                if ((s32)(secondTargetFramesLeft << 16) <= 0) {
                    work->timer = COURSE_0_LONG_DELAY_TICKS;
                    work->phase++;
                    spawn = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
                    _mistShootingGallerySpawnTarget(controller, spawn);
                    work->spawnIndex++;
                }
            }
            break;
        case COURSE_0_TARGET_INSTRUCTIONS:
            instructionsFramesLeft = work->timer - 1;
            work->timer            = instructionsFramesLeft;
            if ((s32)(instructionsFramesLeft << 16) <= 0) {
                work->phase++;
                _mistShootingGalleryShowCaption(COURSE_0_CAPTION_COMMAND, buttonLayout + COURSE_0_TARGET_INSTRUCTIONS_FIRST_KEY, CAPTION_DISMISS_BUTTONS);
            }
            break;
        case COURSE_0_SINGLE_TARGETS:
            if (work->liveTargets == 0) {
                spawn = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
                _mistShootingGallerySpawnTarget(controller, spawn);
                work->spawnIndex++;
                if (work->spawnIndex == COURSE_0_SINGLE_TARGET_END_INDEX) {
                    work->phase++;
                }
            }
            break;
        case COURSE_0_WAIT_FOR_SINGLE_TARGETS:
            if (work->liveTargets == 0) {
                work->timer = COURSE_0_CAPTION_DELAY_TICKS;
                work->phase++;
            }
            break;
        case COURSE_0_SEED_TARGETS:
            seedFramesLeft = work->timer - 1;
            work->timer    = seedFramesLeft;
            if ((s32)(seedFramesLeft << 16) <= 0) {
                spawn = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
                _mistShootingGallerySpawnTarget(controller, spawn);
                work->spawnIndex++;
                if (work->spawnIndex == COURSE_0_SEED_TARGET_END_INDEX) {
                    work->timer = COURSE_0_LONG_DELAY_TICKS;
                    work->phase++;
                } else {
                    work->timer = COURSE_0_RAPID_SPAWN_DELAY_TICKS;
                }
            }
            break;
        case COURSE_0_WAVE_INSTRUCTIONS:
            waveInstructionsFramesLeft = work->timer - 1;
            work->timer                = waveInstructionsFramesLeft;
            if ((s32)(waveInstructionsFramesLeft << 16) <= 0) {
                work->phase++;
                _mistShootingGalleryShowCaption(COURSE_0_CAPTION_COMMAND, buttonLayout + COURSE_0_WAVE_INSTRUCTIONS_FIRST_KEY, CAPTION_DISMISS_BUTTONS);
            }
            break;
        case COURSE_0_WAIT_FOR_SEED_TARGETS:
            if (work->liveTargets != 0) {
                break;
            }
            work->phase++;
            displayReleaseMenuHold();
        // Hold at clear markers and at the finite script end until timeout.
        case COURSE_0_WAVES:
            spawn      = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
            spawnFrame = spawn->frame;
            if (spawnFrame != SPAWN_END) {
                if (spawnFrame != SPAWN_WAIT_CLEAR) {
                    if (work->scriptFrame == spawnFrame) {
                        do {
                            _mistShootingGallerySpawnTarget(controller, spawn);
                            spawn++;
                            work->spawnIndex++;
                        } while (work->scriptFrame == spawn->frame);
                    }
                    scriptFrame = work->scriptFrame;
                    if (scriptFrame < SPAWN_FRAME_LIMIT) {
                        work->scriptFrame = scriptFrame + 1;
                    }
                } else if (work->liveTargets == 0) {
                    work->spawnIndex++;
                }
            }
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = COURSE_0_INITIALIZE;
                controller->state++;
            }
            break;
        case COURSE_0_AMMO_CAPTIONS:
            _mistShootingGalleryShowCaption(COURSE_0_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
            captionKey = work->captionStep;
            if (captionKey == COURSE_0_AMMO_LAST_KEY) {
                work->phase = work->resumePhase;
                displayReleaseMenuHold();
            } else {
                work->captionStep = captionKey + 1;
            }
            break;
    }

    // The ammunition warning is one-shot and retains the interrupted phase.
    if (work->interrupted == 0) {
        minimumPrimaryLoad = COURSE_0_MIN_PRIMARY_LOAD;
        if (gPlayerStatus.weapon == COURSE_0_M93R_SELECTION) {
            minimumPrimaryLoad = COURSE_0_M93R_MIN_PRIMARY_LOAD;
        }
        if (equipmentConsumeWeaponLoad(gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1), EQUIPMENT_WEAPON_LOAD_QUERY_PRIMARY) < minimumPrimaryLoad) {
            resumePhase       = work->phase;
            work->phase       = COURSE_0_AMMO_CAPTIONS;
            work->interrupted = 1;
            work->captionStep = COURSE_0_AMMO_FIRST_KEY;
            work->resumePhase = resumePhase;
        }
    }
}

/// Advances one tick of a finite course script, holding at its end marker.
static inline void _mistShootingGalleryStepFiniteWaves(Task* controller, MistShootingGalleryWork* work, const _MistShootingGallerySpawn* script)
{
    const _MistShootingGallerySpawn* spawn;
    u16                              spawnFrame;
    u16                              scriptFrame;

    spawn      = &script[work->spawnIndex];
    spawnFrame = spawn->frame;
    if (spawnFrame != SPAWN_END) {
        if (spawnFrame != SPAWN_WAIT_CLEAR) {
            if (work->scriptFrame == spawnFrame) {
                do {
                    _mistShootingGallerySpawnTarget(controller, spawn);
                    spawn++;
                    work->spawnIndex++;
                } while (work->scriptFrame == spawn->frame);
            }
            scriptFrame = work->scriptFrame;
            if (scriptFrame < SPAWN_FRAME_LIMIT) {
                work->scriptFrame = scriptFrame + 1;
            }
        } else if (work->liveTargets == 0) {
            work->spawnIndex++;
        }
    }
}

/// Runs zero-based course 1's introduction and two-minute finite target script.
static void _mistShootingGalleryRunCourse1(Task* controller)
{
    enum {
        COURSE_1_INITIALIZE        = 0,
        COURSE_1_INTRO_CAPTIONS    = 1,
        COURSE_1_READY_DELAY       = 2,
        COURSE_1_START_DELAY       = 3,
        COURSE_1_WAVES             = 4,
        COURSE_1_DURATION_FRAMES   = 3600,
        COURSE_1_READY_DELAY_TICKS = 60,
        COURSE_1_START_DELAY_TICKS = 10,
        COURSE_1_CAPTION_COMMAND   = 18,
        COURSE_1_INTRO_LAST_KEY    = 4,
    };
    MistShootingGalleryWork* work;
    u16                      introFramesLeft;
    u16                      readyFramesBeforeDecrement;
    u16                      startFramesBeforeDecrement;
    u8                       captionKey;

    work = controller->work;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_SELECT) != 0) {
        shootingGalleryPierceRequestGalleryAbort();
        return;
    }

    switch (work->phase) {
        case COURSE_1_INITIALIZE:
            work->timeLeft = COURSE_1_DURATION_FRAMES;
            work->timer    = COURSE_1_READY_DELAY_TICKS;
            work->phase++;
        case COURSE_1_INTRO_CAPTIONS:
            introFramesLeft = work->timer - 1;
            work->timer     = introFramesLeft;
            if ((s32)(introFramesLeft << 16) <= 0) {
                _mistShootingGalleryShowCaption(COURSE_1_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
                captionKey = work->captionStep;
                if (captionKey == COURSE_1_INTRO_LAST_KEY) {
                    work->timer = COURSE_1_READY_DELAY_TICKS;
                    work->phase++;
                    return;
                }
                work->captionStep = captionKey + 1;
                return;
            }
        default:
            return;
        case COURSE_1_READY_DELAY:
            readyFramesBeforeDecrement = work->timer;
            work->timer                = readyFramesBeforeDecrement - 1;
            if ((s32)(readyFramesBeforeDecrement << 16) <= 0) {
                work->timer = COURSE_1_START_DELAY_TICKS;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
                sceneEngageBattle(1);
                return;
            }
            break;
        case COURSE_1_START_DELAY:
            startFramesBeforeDecrement = work->timer;
            work->timer                = startFramesBeforeDecrement - 1;
            if ((s32)(startFramesBeforeDecrement << 16) > 0) {
                break;
            }
            work->phase++;
            displayReleaseMenuHold();
            /* fallthrough */
        case COURSE_1_WAVES:
            _mistShootingGalleryStepFiniteWaves(controller, work, _gMistShootingGalleryCourse1WaveScriptRef);
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = COURSE_1_INITIALIZE;
                controller->state++;
            }
            break;
    }
}

/// Advances a timed target script, repeating a selected suffix after wave clearance.
static inline void _mistShootingGalleryStepRepeatingWaves(Task* controller, MistShootingGalleryWork* work, const _MistShootingGallerySpawn* script, s32 loopRestartThreshold, s32 loopStartIndex, u16 loopStartFrame)
{
    const _MistShootingGallerySpawn* spawn;
    u16                              spawnFrame;
    u16                              scriptFrame;

    spawn      = &script[work->spawnIndex];
    spawnFrame = spawn->frame;
    if (spawnFrame != SPAWN_END) {
        if (spawnFrame != SPAWN_WAIT_CLEAR) {
            if (work->scriptFrame == spawnFrame) {
                do {
                    _mistShootingGallerySpawnTarget(controller, spawn);
                    spawn++;
                    work->spawnIndex++;
                } while (work->scriptFrame == spawn->frame);
            }
            scriptFrame = work->scriptFrame;
            if (scriptFrame < SPAWN_FRAME_LIMIT) {
                work->scriptFrame = scriptFrame + 1;
            }
        } else if (work->liveTargets == 0) {
            work->spawnIndex++;
            if (work->spawnIndex >= loopRestartThreshold) {
                work->spawnIndex  = loopStartIndex;
                work->scriptFrame = loopStartFrame;
            }
        }
    }
}

/// Runs zero-based course 2's GPS tutorial and three-minute target sequence.
static void _mistShootingGalleryRunCourse2(Task* controller)
{
    enum {
        COURSE_2_INITIALIZE               = 0,
        COURSE_2_READY_CAPTIONS           = 1,
        COURSE_2_WAIT_FOR_VIEW            = 2,
        COURSE_2_GPS_PROMPT               = 3,
        COURSE_2_CHECK_GPS                = 4,
        COURSE_2_WAIT_FOR_GPS             = 5,
        COURSE_2_PREPARE_TARGETS          = 6,
        COURSE_2_SEED_TARGETS             = 7,
        COURSE_2_INSTRUCTIONS             = 8,
        COURSE_2_START_ROUND              = 9,
        COURSE_2_WAVES                    = 10,
        COURSE_2_CAPTION_COMMAND          = 0x13,
        COURSE_2_DURATION_FRAMES          = 5400,
        COURSE_2_READY_DELAY_FRAMES       = 30,
        COURSE_2_STEP_DELAY_FRAMES        = 15,
        COURSE_2_TARGET_SETTLE_FRAMES     = 60,
        COURSE_2_HANDOFF_VIEW             = 18,
        COURSE_2_LOOP_RESTART_THRESHOLD   = ARRAY_SIZE(_gMistShootingGalleryCourse2WaveScript) - 2,
        COURSE_2_LOOP_START_INDEX         = 14,
        COURSE_2_LOOP_START_FRAME         = 240,
        COURSE_2_INITIAL_TARGET_COUNT     = 3,
        COURSE_2_READY_LAST_KEY           = 3,
        COURSE_2_LAYOUT_FIRST_KEY         = 4,
        COURSE_2_GPS_PROMPT_KEY           = 7,
        COURSE_2_GPS_CHECK_FIRST_KEY      = 8,
        COURSE_2_GPS_RETRY_KEY            = 9,
        COURSE_2_TARGET_PREP_FIRST_KEY    = 10,
        COURSE_2_TARGET_PREP_LAST_KEY     = 11,
        COURSE_2_INSTRUCTIONS_LAST_KEY    = 19,
        COURSE_2_ROUND_START_DELAY_FRAMES = 10,
    };

    enum { INVENTORY_ITEM_GPS = 0x40 };
    MistShootingGalleryWork* work;
    s32                      buttonLayout;
    s32                      hasGpsAttachment;
    u16                      timerBeforeDecrement;
    u8                       captionKey;

    work         = controller->work;
    buttonLayout = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;

    switch (work->phase) {
        case COURSE_2_INITIALIZE:
            work->timeLeft            = COURSE_2_DURATION_FRAMES;
            work->timer               = COURSE_2_READY_DELAY_FRAMES;
            gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
            work->phase++;
        case COURSE_2_READY_CAPTIONS:
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    _mistShootingGalleryShowCaption(COURSE_2_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
                    captionKey = work->captionStep;
                    if (captionKey == COURSE_2_READY_LAST_KEY) {
                        work->timer               = COURSE_2_STEP_DELAY_FRAMES;
                        gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_RUNNING;
                        work->phase++;
                    } else {
                        work->captionStep = captionKey + 1;
                    }
                }
            } else {
                work->timer--;
            }
            break;
        case COURSE_2_WAIT_FOR_VIEW:
            if (gGameSession->location.view == COURSE_2_HANDOFF_VIEW) {
                if (work->timer <= 0) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        work->timer = 1;
                        work->phase++;
                        _mistShootingGalleryShowCaption(COURSE_2_CAPTION_COMMAND, buttonLayout + COURSE_2_LAYOUT_FIRST_KEY, CAPTION_DISMISS_BUTTONS);
                    }
                } else {
                    work->timer--;
                }
            }
            break;
        case COURSE_2_GPS_PROMPT:
            if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                work->actionTriggered = 0;
                work->captionStep     = COURSE_2_GPS_CHECK_FIRST_KEY;
                work->phase++;
                _mistShootingGalleryShowCaption(COURSE_2_CAPTION_COMMAND, COURSE_2_GPS_PROMPT_KEY, CAPTION_DISMISS_BUTTONS);
            }
            break;
        case COURSE_2_CHECK_GPS:
            if (work->actionTriggered != 0) {
                hasGpsAttachment = inventoryHasAttachedItem(INVENTORY_ITEM_GPS);
                if (hasGpsAttachment != 1) {
                    _mistShootingGalleryShowCaption(COURSE_2_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
                    if (work->captionStep == COURSE_2_GPS_RETRY_KEY) {
                        work->actionTriggered = 0;
                        work->phase++;
                    }
                    work->captionStep++;
                } else {
                    work->phase       = COURSE_2_PREPARE_TARGETS;
                    work->timer       = 1;
                    work->captionStep = COURSE_2_TARGET_PREP_FIRST_KEY;
                }
            }
            break;
        case COURSE_2_WAIT_FOR_GPS:
            if (inventoryHasAttachedItem(INVENTORY_ITEM_GPS) == 1) {
                work->timer = COURSE_2_STEP_DELAY_FRAMES;
                work->phase++;
            } else if (work->actionTriggered != 0) {
                _mistShootingGalleryShowCaption(COURSE_2_CAPTION_COMMAND, COURSE_2_GPS_RETRY_KEY, CAPTION_DISMISS_BUTTONS);
                work->actionTriggered = 0;
            }
            break;
        case COURSE_2_PREPARE_TARGETS:
            if (work->timer <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    _mistShootingGalleryShowCaption(COURSE_2_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
                    if (work->captionStep == COURSE_2_TARGET_PREP_LAST_KEY) {
                        work->timer = COURSE_2_STEP_DELAY_FRAMES;
                        work->phase++;
                        sceneEngageBattle(1);
                    }
                    work->captionStep++;
                }
            } else {
                work->timer--;
            }
            break;
        case COURSE_2_SEED_TARGETS:
            work->timer--;
            if ((s32)(work->timer << 16) <= 0) {
                _mistShootingGallerySpawnTarget(controller, &_gMistShootingGalleryCourse2WaveScriptRef[work->spawnIndex]);
                work->spawnIndex++;
                if (work->spawnIndex == COURSE_2_INITIAL_TARGET_COUNT) {
                    work->timer = COURSE_2_TARGET_SETTLE_FRAMES;
                    work->phase++;
                } else {
                    work->timer = COURSE_2_STEP_DELAY_FRAMES;
                }
            }
            break;
        case COURSE_2_INSTRUCTIONS:
            if (work->timer <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    _mistShootingGalleryShowCaption(COURSE_2_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
                    if (work->captionStep == COURSE_2_INSTRUCTIONS_LAST_KEY) {
                        work->timer = COURSE_2_STEP_DELAY_FRAMES;
                        work->phase++;
                    }
                    work->captionStep++;
                }
            } else {
                work->timer--;
            }
            break;
        case COURSE_2_START_ROUND:
            timerBeforeDecrement = work->timer;
            work->timer          = timerBeforeDecrement - 1;
            if ((s32)(timerBeforeDecrement << 16) <= 0) {
                work->timer = COURSE_2_ROUND_START_DELAY_FRAMES;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
            }
            break;
        case COURSE_2_WAVES:
            _mistShootingGalleryStepRepeatingWaves(controller, work, _gMistShootingGalleryCourse2WaveScriptRef, COURSE_2_LOOP_RESTART_THRESHOLD, COURSE_2_LOOP_START_INDEX, COURSE_2_LOOP_START_FRAME);
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = COURSE_2_INITIALIZE;
                controller->state++;
            }
            break;
    }
}

/// Runs zero-based course 3's attachment-wheel tutorial and three-minute sequence.
static void _mistShootingGalleryRunCourse3(Task* controller)
{
    enum {
        COURSE_3_INITIALIZE             = 0,
        COURSE_3_READY_CAPTION          = 1,
        COURSE_3_WAIT_FOR_VIEW          = 2,
        COURSE_3_WAIT_FOR_ACTION        = 3,
        COURSE_3_SEED_TARGETS           = 4,
        COURSE_3_TURN_INSTRUCTIONS      = 5,
        COURSE_3_WAIT_FOR_TURN          = 6,
        COURSE_3_SWAP_INSTRUCTIONS      = 7,
        COURSE_3_WAVES                  = 8,
        COURSE_3_INTERRUPTION_CAPTIONS  = 9,
        COURSE_3_CAPTION_COMMAND        = 0x14,
        COURSE_3_DURATION_FRAMES        = 5400,
        COURSE_3_READY_DELAY_FRAMES     = 30,
        COURSE_3_STEP_DELAY_FRAMES      = 15,
        COURSE_3_TARGET_SETTLE_FRAMES   = 60,
        COURSE_3_HANDOFF_VIEW           = 18,
        COURSE_3_LOOP_RESTART_THRESHOLD = ARRAY_SIZE(_gMistShootingGalleryCourse3WaveScript) - 2,
        COURSE_3_LOOP_START_INDEX       = 9,
        COURSE_3_LOOP_START_FRAME       = 144,
        COURSE_3_INITIAL_TARGET_COUNT   = 2,
        COURSE_3_READY_KEY              = 0,
        COURSE_3_ACTION_PROMPT_KEY      = 7,
        COURSE_3_TURN_FIRST_KEY         = 8,
        COURSE_3_TURN_LAST_KEY          = 10,
        COURSE_3_LAYOUT_FIRST_KEY       = 11,
        COURSE_3_SWAP_FIRST_KEY         = 14,
        COURSE_3_SWAP_LAST_KEY          = 18,
        COURSE_3_INTERRUPTION_FIRST_KEY = 19,
        COURSE_3_INTERRUPTION_LAST_KEY  = 23,
        COURSE_3_TURN_YAW               = 3072,
        COURSE_3_SWAP_DELAY_FRAMES      = 4,
    };

    MistShootingGalleryWork* work;
    ActorTransform           turnRequest;
    GameActor*               player;
    s32                      dismissButtons;
    s32                      buttonLayout;
    u16                      resumePhase;
    s32                      interruptRequest;
    u8                       captionKey;
    u8                       resumeCaptionKey;

    work         = controller->work;
    buttonLayout = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;

    switch (work->phase) {
        case COURSE_3_INITIALIZE:
            work->timeLeft            = COURSE_3_DURATION_FRAMES;
            work->timer               = COURSE_3_READY_DELAY_FRAMES;
            gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
            work->phase++;
        case COURSE_3_READY_CAPTION:
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    _mistShootingGalleryShowCaption(COURSE_3_CAPTION_COMMAND, COURSE_3_READY_KEY, CAPTION_DISMISS_BUTTONS);
                    work->timer               = COURSE_3_STEP_DELAY_FRAMES;
                    gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_RUNNING;
                    work->phase++;
                }
            } else {
                work->timer--;
            }
            break;
        case COURSE_3_WAIT_FOR_VIEW:
            if (gGameSession->location.view == COURSE_3_HANDOFF_VIEW) {
                if (work->timer <= 0) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        work->timer = 1;
                        work->phase++;
                        _mistShootingGalleryShowCaption(COURSE_3_CAPTION_COMMAND, COURSE_3_ACTION_PROMPT_KEY, CAPTION_DISMISS_BUTTONS);
                    }
                } else {
                    work->timer--;
                }
            }
            break;
        case COURSE_3_WAIT_FOR_ACTION:
            if (work->actionTriggered != 0) {
                work->timer = COURSE_3_READY_DELAY_FRAMES;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
                sceneEngageBattle(1);
            }
            break;
        case COURSE_3_SEED_TARGETS:
            work->timer--;
            if ((s32)(work->timer << 16) <= 0) {
                _mistShootingGallerySpawnTarget(controller, &_gMistShootingGalleryCourse3WaveScriptRef[work->spawnIndex]);
                work->spawnIndex++;
                if (work->spawnIndex == COURSE_3_INITIAL_TARGET_COUNT) {
                    work->captionStep = COURSE_3_TURN_FIRST_KEY;
                    work->timer       = COURSE_3_TARGET_SETTLE_FRAMES;
                    work->phase++;
                } else {
                    work->timer = COURSE_3_STEP_DELAY_FRAMES;
                }
            }
            break;
        case COURSE_3_TURN_INSTRUCTIONS:
            work->timer--;
            if ((s32)(work->timer << 16) <= 0) {
                _mistShootingGalleryShowCaption(COURSE_3_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
                if (work->captionStep == COURSE_3_TURN_LAST_KEY) {
                    work->timer = COURSE_3_READY_DELAY_FRAMES;
                    work->phase++;
                    padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_ALIAS, PAD_BUTTON_R1 | PAD_BUTTON_R2);
                    turnRequest.rot.vy = COURSE_3_TURN_YAW;
                    playerActorTurnToYaw(
                        gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0, &turnRequest, 0);
                }
                work->captionStep++;
            }
            break;
        case COURSE_3_WAIT_FOR_TURN:
            player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_ALIAS, PAD_BUTTON_R1 | PAD_BUTTON_R2);
            if (player->scriptedMotionPending == 0) {
                playerActorEndScripted(
                    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0, PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET, 0);
                work->phase++;
                dismissButtons          = PAD_BUTTON_TRIANGLE;
                gAttachmentState.flags |= ATTACHMENT_FLAG_FORCE_OPEN_WHEEL;
                work->timer             = COURSE_3_SWAP_DELAY_FRAMES;
                work->captionStep       = COURSE_3_SWAP_FIRST_KEY;
                if (buttonLayout == 2) {
                    dismissButtons = PAD_BUTTON_R2;
                }
                _mistShootingGalleryShowCaption(COURSE_3_CAPTION_COMMAND, buttonLayout + COURSE_3_LAYOUT_FIRST_KEY, dismissButtons);
            }
            break;
        case COURSE_3_SWAP_INSTRUCTIONS:
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_ALIAS, PAD_BUTTON_R1 | PAD_BUTTON_R2);
            if (work->timer <= 0) {
                if ((u32)((u8)gAttachmentState.mode - ATTACHMENT_MODE_ARMED) >= 2) {
                    _mistShootingGalleryShowCaption(COURSE_3_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
                    if (work->captionStep == COURSE_3_SWAP_LAST_KEY) {
                        work->phase++;
                        padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR, PAD_BUTTON_R1 | PAD_BUTTON_R2);
                        gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
                    }
                    work->captionStep++;
                }
            } else {
                work->timer--;
            }
            break;
        case COURSE_3_WAVES:
            _mistShootingGalleryStepRepeatingWaves(controller, work, _gMistShootingGalleryCourse3WaveScriptRef, COURSE_3_LOOP_RESTART_THRESHOLD, COURSE_3_LOOP_START_INDEX, COURSE_3_LOOP_START_FRAME);
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = COURSE_3_INITIALIZE;
                controller->state++;
            }
            break;
        case COURSE_3_INTERRUPTION_CAPTIONS:
            if (work->timer <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    _mistShootingGalleryShowCaption(COURSE_3_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
                    captionKey = work->captionStep;
                    if (captionKey == COURSE_3_INTERRUPTION_LAST_KEY) {
                        work->phase       = work->resumePhase;
                        work->captionStep = work->resumeCaptionStep;
                    } else {
                        work->captionStep = captionKey + 1;
                    }
                }
            } else {
                work->timer--;
            }
            break;
    }

    if (work->interrupted == 0 && work->captionStep >= COURSE_3_INTERRUPTION_FIRST_KEY) {
        interruptRequest = gAttachmentState.effectPhase;
        if (interruptRequest == 1) {
            resumePhase             = work->phase;
            work->interrupted       = interruptRequest;
            resumeCaptionKey        = work->captionStep;
            work->phase             = COURSE_3_INTERRUPTION_CAPTIONS;
            work->timer             = COURSE_3_READY_DELAY_FRAMES;
            work->captionStep       = COURSE_3_INTERRUPTION_FIRST_KEY;
            work->resumePhase       = resumePhase;
            work->resumeCaptionStep = resumeCaptionKey;
        }
    }
}

/// Runs zero-based course 4's action tutorial and three-minute repeating target waves.
static void _mistShootingGalleryRunCourse4(Task* controller)
{
    enum {
        COURSE_4_INITIALIZE                 = 0,
        COURSE_4_INTRO_CAPTION              = 1,
        COURSE_4_WAIT_FOR_VIEW              = 2,
        COURSE_4_FIRST_ACTION_CAPTIONS      = 3,
        COURSE_4_SECOND_ACTION_CAPTIONS     = 4,
        COURSE_4_READY_DELAY                = 5,
        COURSE_4_START_DELAY                = 6,
        COURSE_4_WAVES                      = 7,
        COURSE_4_LETHAL_HIT_CAPTIONS        = 8,
        COURSE_4_LETHAL_HIT_RELEASE         = 9,
        COURSE_4_DURATION_FRAMES            = 5400,
        COURSE_4_READY_DELAY_TICKS          = 30,
        COURSE_4_START_DELAY_TICKS          = 15,
        COURSE_4_HANDOFF_VIEW               = 18,
        COURSE_4_CAPTION_COMMAND            = 21,
        COURSE_4_INTRO_KEY                  = 0,
        COURSE_4_ACTION_PROMPT_KEY          = 7,
        COURSE_4_ACTION_FIRST_KEY           = 8,
        COURSE_4_FIRST_ACTION_LAST_KEY      = 10,
        COURSE_4_SECOND_ACTION_LAST_KEY     = 16,
        COURSE_4_LETHAL_FIRST_KEY           = 17,
        COURSE_4_LETHAL_LAST_KEY            = 18,
        COURSE_4_LETHAL_CAPTION_DELAY_TICKS = 3,
        COURSE_4_LETHAL_RELEASE_DELAY_TICKS = 4,
        COURSE_4_LOOP_RESTART_THRESHOLD     = ARRAY_SIZE(_gMistShootingGalleryCourse4WaveScript) - 2,
        COURSE_4_LOOP_START_INDEX           = 11,
        COURSE_4_LOOP_START_FRAME           = 688,
    };
    MistShootingGalleryWork* work;
    u16                      framesLeft;
    u8                       captionKey;

    work = controller->work;

    switch (work->phase) {
        case COURSE_4_INITIALIZE:
            work->timeLeft            = COURSE_4_DURATION_FRAMES;
            work->timer               = COURSE_4_READY_DELAY_TICKS;
            gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
            work->phase++;
        case COURSE_4_INTRO_CAPTION:
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    _mistShootingGalleryShowCaption(COURSE_4_CAPTION_COMMAND, COURSE_4_INTRO_KEY, CAPTION_DISMISS_BUTTONS);
                    work->timer               = COURSE_4_START_DELAY_TICKS;
                    gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_RUNNING;
                    work->phase++;
                }
            } else {
                work->timer = (u16)work->timer - 1;
            }
            break;
        case COURSE_4_WAIT_FOR_VIEW:
            if (gGameSession->location.view != COURSE_4_HANDOFF_VIEW) {
                break;
            }
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    work->timer = 1;
                    work->phase++;
                    _mistShootingGalleryShowCaption(COURSE_4_CAPTION_COMMAND, COURSE_4_ACTION_PROMPT_KEY, CAPTION_DISMISS_BUTTONS);
                    work->captionStep = COURSE_4_ACTION_FIRST_KEY;
                }
            } else {
                work->timer = (u16)work->timer - 1;
            }
            break;
        case COURSE_4_FIRST_ACTION_CAPTIONS:
            if (work->actionTriggered == 0) {
                break;
            }
            _mistShootingGalleryShowCaption(COURSE_4_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
            if (work->captionStep == COURSE_4_FIRST_ACTION_LAST_KEY) {
                work->actionTriggered = 0;
                work->phase++;
            }
            work->captionStep++;
            break;
        case COURSE_4_SECOND_ACTION_CAPTIONS:
            if (work->actionTriggered == 0) {
                break;
            }
            _mistShootingGalleryShowCaption(COURSE_4_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
            if (work->captionStep == COURSE_4_SECOND_ACTION_LAST_KEY) {
                work->timer = COURSE_4_READY_DELAY_TICKS;
                work->phase++;
            }
            work->captionStep++;
            break;
        case COURSE_4_READY_DELAY:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if ((s32)(framesLeft << 16) <= 0) {
                work->timer = COURSE_4_START_DELAY_TICKS;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
                gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
                sceneEngageBattle(1);
            }
            break;
        case COURSE_4_START_DELAY:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if ((s32)(framesLeft << 16) <= 0) {
                work->phase++;
            }
            break;
        case COURSE_4_WAVES:
            // Advance the script before drawing and ticking the combat clock.
            _mistShootingGalleryStepRepeatingWaves(controller, work, _gMistShootingGalleryCourse4WaveScriptRef, COURSE_4_LOOP_RESTART_THRESHOLD, COURSE_4_LOOP_START_INDEX, COURSE_4_LOOP_START_FRAME);
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = COURSE_4_INITIALIZE;
                controller->state++;
            }
            break;
        case COURSE_4_LETHAL_HIT_CAPTIONS:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if ((s32)(framesLeft << 16) <= 0) {
                _mistShootingGalleryShowCaption(COURSE_4_CAPTION_COMMAND, work->captionStep, CAPTION_DISMISS_BUTTONS);
                captionKey = work->captionStep;
                if (captionKey == COURSE_4_LETHAL_LAST_KEY) {
                    work->timer               = COURSE_4_LETHAL_RELEASE_DELAY_TICKS;
                    gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_RUNNING;
                    work->phase++;
                    shootingGalleryPierceRequestGalleryAbort();
                } else {
                    work->captionStep = captionKey + 1;
                }
            }
            break;
        case COURSE_4_LETHAL_HIT_RELEASE:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if ((s32)(framesLeft << 16) <= 0) {
                sceneReleaseBattleRefAndClearRewards(controller, 0);
                taskKill(controller);
                return;
            }
            break;
    }

    // A lethal hit diverts both the tutorial and active waves into shutdown.
    if (work->interrupted == 0 && work->lethalHit != 0) {
        work->phase               = COURSE_4_LETHAL_HIT_CAPTIONS;
        work->interrupted         = 1;
        work->timer               = COURSE_4_LETHAL_CAPTION_DELAY_TICKS;
        work->captionStep         = COURSE_4_LETHAL_FIRST_KEY;
        gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
    }
}

/// Restores movement and queues the player's collision passes and view triggers.
static inline void _mistShootingGalleryRestorePlayerControl(GameActor* player)
{
    player->movementInputDisabled                        = 0;
    player->pendingCollisionUpdates                      = GAME_ACTOR_COLLISION_REQUEST_MASK;
    player->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
}

/// Closes a completed gallery course after the remaining targets leave play.
static void _mistShootingGalleryFinishCourse(Task* controller)
{
    enum {
        WAIT_FOR_TARGETS     = 0,
        PAUSE                = 1,
        FLASH                = 2,
        WAIT_FOR_RESET_CLEAR = 3,
        WAIT_FOR_RESET_SET   = 4,
        PAUSE_TICKS          = 30,
        FLASH_TICKS          = 90,
    };
    MistShootingGalleryWork* work;
    GameActor*               player;

    work   = controller->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    switch (work->phase) {
        case WAIT_FOR_TARGETS:
            if (work->liveTargets != 0) {
                return;
            }
            work->timer = PAUSE_TICKS;
            work->phase++;
            displayAcquireMenuHold();
        case PAUSE:
            if ((s16)work->timer-- > 0) {
                return;
            }
            work->timer = FLASH_TICKS;
            work->phase++;
            taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
            sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
            return;
        case FLASH:
            if ((s16)--work->timer > 0) {
                return;
            }
            work->phase++;
            // Restore control before releasing combat ownership and actor course state.
            _mistShootingGalleryRestorePlayerControl(player);
            sceneReleaseBattleRefAndClearRewards(controller, 0);
            shootingGalleryPierceFinishGalleryCourse();
            return;
        case WAIT_FOR_RESET_CLEAR:
            if (gGameSession->battleResetPending == 0) {
                work->phase++;
            }
            return;
        case WAIT_FOR_RESET_SET:
            if (gGameSession->battleResetPending == 1) {
                displayReleaseMenuHold();
                taskKill(controller);
            }
            return;
    }
}

s32 mistShootingGalleryGetBonusBp(s32 score)
{
    enum {
        NONE   = 0,
        LOW    = 100,
        MEDIUM = 200,
        HIGH   = 300,
    };

    const MistShootingGalleryWork* work    = gMistShootingGalleryControllerTask->work;
    s32                            bonusBp = NONE;

    switch (work->course) {
        case 0:
            if (score >= 10000) {
                bonusBp = HIGH;
            } else if (score >= 9000) {
                bonusBp = MEDIUM;
            } else if (score >= 8000) {
                bonusBp = LOW;
            }
            break;
        case 1:
            if (score >= 17400) {
                bonusBp = HIGH;
            } else if (score >= 16800) {
                bonusBp = MEDIUM;
            } else if (score >= 16000) {
                bonusBp = LOW;
            }
            break;
        case 2:
            if (score > 49999) {
                bonusBp = HIGH;
            } else if (score > 45999) {
                bonusBp = MEDIUM;
            } else if (score > 38999) {
                bonusBp = LOW;
            }
            break;
        case 3:
            if (score > 59999) {
                bonusBp = HIGH;
            } else if (score > 55999) {
                bonusBp = MEDIUM;
            } else if (score > 51999) {
                bonusBp = LOW;
            }
            break;
        case 4:
            if (score > 54999) {
                bonusBp = HIGH;
            } else if (score > 52999) {
                bonusBp = MEDIUM;
            } else if (score > 49999) {
                bonusBp = LOW;
            }
            break;
    }
    return bonusBp;
}

/// Slides the course countdown into view and draws its remaining time as MM:SS.
static void _mistShootingGalleryDrawCountdownClock(MistShootingGalleryWork* work)
{
    enum {
        FRAMES_PER_SECOND = 30,
        FRAMES_PER_MINUTE = 60 * FRAMES_PER_SECOND,
        COLON             = 10,
        CLOCK_Y           = 70,
    };

    s32 minuteTens;
    s32 minuteOnes;
    s32 secondTens;
    s32 secondOnes;
    s32 remainingFrames;

    /// Extracts and draws one decimal place of the remaining frame count.
#define DRAW_CLOCK_PLACE(digit, framesPerPlace, xOffset) \
    (digit) = remainingFrames / (framesPerPlace);        \
    if ((digit) != 0) {                                  \
        remainingFrames %= (framesPerPlace);             \
    }                                                    \
    _mistShootingGalleryDrawClockGlyph(work->clockX + (xOffset), CLOCK_Y, (digit))

    remainingFrames = work->timeLeft;
    if (work->clockX < -0x78) {
        work->clockX += 0xA;
    }

    // Peel off each decimal place before submitting its glyph.
    DRAW_CLOCK_PLACE(minuteTens, 10 * FRAMES_PER_MINUTE, 0);
    DRAW_CLOCK_PLACE(minuteOnes, FRAMES_PER_MINUTE, 0xC);
    DRAW_CLOCK_PLACE(secondTens, 10 * FRAMES_PER_SECOND, 0x24);
    DRAW_CLOCK_PLACE(secondOnes, FRAMES_PER_SECOND, 0x30);

    _mistShootingGalleryDrawClockGlyph(work->clockX + 0x18, CLOCK_Y, COLON);
#undef DRAW_CLOCK_PLACE
}

/// Queues one unmodulated 15 by 19 pixel clock glyph and its texture-page command.
static void _mistShootingGalleryDrawClockGlyph(s32 screenX, s16 screenY, s32 glyph)
{
    enum {
        GLYPH_STRIDE    = 16,
        RAW_SPRITE_CODE = 0x65,
    };

    SPRT*     sprite;
    DR_TPAGE* drawMode;

    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    sprite->w      = 0xF;
    sprite->h      = 0x13;
    sprite->clut   = getClut(0, 261);
    setlen(sprite, 4);
    sprite->y0 = screenY;
    sprite->u0 = glyph * GLYPH_STRIDE;
    sprite->v0 = 0;
    setcode(sprite, RAW_SPRITE_CODE);
    sprite->x0 = screenX;
    addPrim(gGpuCurrentOt, sprite);

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(0, GPU_BLEND_AVERAGE, 320, 256));
    addPrim(gGpuCurrentOt, drawMode);
}

/// Queues an additive red tile covering the viewport, with intensity in 0..255.
static void _mistShootingGalleryDrawRedFlash(u8 redIntensity)
{
    enum { RED_FLASH_TILE_CODE = 0x62 };

    TILE*     tile;
    DR_TPAGE* drawMode;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    tile->x0       = -0xA8;
    tile->y0       = -0x7C;
    tile->w        = 0x180;
    tile->h        = 0x100;
    setlen(tile, 3);
    tile->r0 = redIntensity;
    tile->g0 = 0;
    tile->b0 = 0;
    setcode(tile, RED_FLASH_TILE_CODE);
    addPrim(gGpuCurrentOt, tile);

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(0, GPU_BLEND_ADD, 320, 256));
    addPrim(gGpuCurrentOt, drawMode);
}

void mistShootingGallerySpawnDemoTarget(void)
{
    enum {
        TARGET_KIND     = 13,
        MOUNT_BEHAVIOUR = 2,
        BEHAVIOUR_SHIFT = 12,
        CLUT_ROW        = 2,
    };
    Enemy*     enemy;
    TmdObject* model;
    GfxCoord*  rootCoord;

    enemy = enemySpawnFromTable(gShootingGalleryTargetsTaskDescriptors, SHOOTING_GALLERY_TARGETS_TASK_MOUNT, TARGET_KIND | (MOUNT_BEHAVIOUR << BEHAVIOUR_SHIFT), NULL);
    if (enemy != NULL) {
        model                    = enemy->task->extra.tmd;
        model->texturePageOffset = 0;
        model->clutRowOffset     = CLUT_ROW;
        // Both primitive-buffer halves must use the gallery palette.
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
        rootCoord             = enemy->task->extra.tmd->coords;
        rootCoord->coord.t[0] = 6000;
        rootCoord->coord.t[2] = 3000;
        rootCoord->coord.t[1] = 0;
        enemy->workType       = ENEMY_WORK_ACTOR;
    }
}

void mistShootingGallerySignalAction(void)
{
    MistShootingGalleryWork* work = gMistShootingGalleryControllerTask->work;

    work->actionTriggered = 1;
}

s32 mistShootingGalleryQualifiesForPrize(s32 bonusBp)
{
    enum {
        FIRST_ADVANCED_PRIZE_COURSE = 3,
        BASIC_PRIZE_BP              = 200,
        ADVANCED_PRIZE_BP           = 300,
    };
    MistShootingGalleryWork* work      = gMistShootingGalleryControllerTask->work;
    s32                      qualifies = 0;

    if (work->course < FIRST_ADVANCED_PRIZE_COURSE) {
        qualifies = bonusBp >= BASIC_PRIZE_BP;
    } else if (bonusBp >= ADVANCED_PRIZE_BP) {
        qualifies = 1;
    }
    return qualifies;
}

/// Dispatches initialization, course playback and closing for the gallery controller.
static void _mistShootingGalleryCourseTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistShootingGalleryControllerStates;
    states.funcs[task->state](task);
}

/// Dispatches the active controller's selected course script for this tick.
static void _mistShootingGalleryRunCourse(Task* controller)
{
    MistShootingGalleryWork* work   = controller->work;
    TaskFuncTable5           rounds = _gMistShootingGalleryCourseHandlers;

    rounds.funcs[work->course](controller);
}

/// Restores player control and tears down the gallery controller.
static void _mistShootingGalleryExitCourse(Task* task)
{
    GameActor* player;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    _mistShootingGalleryRestorePlayerControl(player);
    displayReleaseMenuHold();
    sceneReleaseBattleRefAndClearRewards(task, 0);
    taskKill(task);
}

/// Decrements the course's 30 Hz clock while combat actors run; returns frames left.
static u16 _mistShootingGalleryTickCourseClock(MistShootingGalleryWork* work)
{
    u16 framesLeft = work->timeLeft;

    if ((framesLeft != 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING)) {
        work->timeLeft = framesLeft - 1;
    }
    return work->timeLeft;
}

/// Runs a forty-tick red screen flash, drawing and fading on ticks 10..39.
static void _mistShootingGalleryRedFlashTask(Task* task)
{
    enum {
        INIT              = 0,
        FADE              = 1,
        LIFETIME          = 40,
        DRAW_BELOW        = 31,
        INITIAL_INTENSITY = 255,
        FADE_STEP         = 8,
    };

    s16 framesLeft;

    switch (task->state) {
        case INIT:
            task->state           = FADE;
            task->killCountdown   = LIFETIME;
            task->spawnArg1.value = INITIAL_INTENSITY;
        case FADE:
            framesLeft = --task->killCountdown;
            if (framesLeft <= 0) {
                taskKill(task);
                return;
            }
            // Leave the opening ticks clear, then fade the red overlay.
            if (framesLeft < DRAW_BELOW) {
                if (task->spawnArg1.value >= FADE_STEP + 1) {
                    task->spawnArg1.value -= FADE_STEP;
                }
                _mistShootingGalleryDrawRedFlash((u8)task->spawnArg1.value);
            }
            return;
    }
}

/// Selects a keyed CAP caption and opens its modal button-dismissed display.
static void _mistShootingGalleryShowCaption(s16 commandIndex, s16 captionKey, s16 dismissButtons)
{
    enum { CAPTION_BOTTOM_BASELINE_Y = 208 };

    shootingGalleryPierceCapCaptionSelectRecord(commandIndex, captionKey, CAPTION_BOTTOM_BASELINE_Y);
    displayQueueModeTask(&_gMistShootingGalleryCaptionModeTaskDesc, dismissButtons, 0, STAGE_ENTRY_RELOAD);
}

/// Displays the selected caption until a button press closes its display mode.
static void _mistShootingGalleryCaptionModeTask(Task* task)
{
    enum { INITIALIZE      = 0,
           WAIT_FOR_BUTTON = 1,
           CLOSE           = 2,
           INPUT_DELAY     = 16 };

    switch (task->state) {
        case INITIALIZE:
            task->state         = WAIT_FOR_BUTTON;
            task->killCountdown = INPUT_DELAY;
            // Draw on the initialization tick while consuming the first delay tick.
        case WAIT_FOR_BUTTON:
            if (task->killCountdown != 0) {
                task->killCountdown--;
            } else {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, task->spawnArg1.value) != 0) {
                    task->state = task->state + 1;
                } else {
                    shootingGalleryPierceCapCaptionDrawCurrent();
                }
                break;
            }
            shootingGalleryPierceCapCaptionDrawCurrent();
            break;
        case CLOSE:
            taskKill(task);
            stageRequestModeTaskExit();
            break;
    }
}

/// Spawns, places and counts one scripted target mount for the active course.
static Enemy* _mistShootingGallerySpawnTarget(Task* controller, const _MistShootingGallerySpawn* spawn)
{
    enum {
        SPAWN_HIGH_HALF_SHIFT = 16,
        TARGET_CLUT_ROW       = 2,
    };
    MistShootingGalleryWork* work;
    Enemy*                   enemy;
    TmdObject*               model;
    GfxCoord*                rootCoord;

    work  = controller->work;
    enemy = enemySpawnFromTable(gShootingGalleryTargetsTaskDescriptors, SHOOTING_GALLERY_TARGETS_TASK_MOUNT, spawn->spawnArgLo | (spawn->spawnArgHi << SPAWN_HIGH_HALF_SHIFT), NULL);
    if (enemy != NULL) {
        // Retain the parent overwrite before transfer; source-ring ownership is unproven.
        enemy->task->parent = controller;
        taskReparent(controller, enemy->task);
        model                    = enemy->task->extra.tmd;
        model->texturePageOffset = 0;
        model->clutRowOffset     = TARGET_CLUT_ROW;
        // Both primitive-buffer halves must use the gallery palette.
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
        rootCoord             = enemy->task->extra.tmd->coords;
        rootCoord->coord.t[0] = spawn->x;
        rootCoord->coord.t[1] = spawn->y;
        rootCoord->coord.t[2] = spawn->z;
        enemy->workType       = ENEMY_WORK_ACTOR;
        work->liveTargets++;
    }
    return enemy;
}
