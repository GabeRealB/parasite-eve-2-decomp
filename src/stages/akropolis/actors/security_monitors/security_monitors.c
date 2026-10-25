#include "stages/akropolis/actors/security_monitors.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/loading.h"

#include "main/coord.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/tmd_types.h"

#include "../../event_flags.h"

/// Steps of the palette greying, kept in `_Work::clutGreyStep`.
enum {
    CLUT_GREY_READ    = 0, // Read the figure's two CLUT rows back from VRAM
    CLUT_GREY_CONVERT = 1, // Raise the three channels of every entry to its brightest one
    CLUT_GREY_UPLOAD  = 2, // Write the two rows back to VRAM
    CLUT_GREY_DONE    = 3, // Greyed; the tick leaves the rows alone
};

/// Ticks the advancing figure moves forward for, counted in `_Work::advanceFrames`, before its scene is flagged done
/// and its task moves on to its teardown.
enum { ADVANCE_FRAMES = 0x5A };

/// The mapped views of the two figures' monitors.
enum {
    RUPERT_VIEW = 0xA,
    SWAT_VIEW   = 0xB,
};

/// Both figures play the only populated entry of their animation set tables.
enum { ANIMATION_SET = 1 };

/// Work block of either figure the package shows, allocated zeroed at its full size by the figure's spawn state and
/// kept at `Task::work` for the task's life.
typedef struct {
    ActorAnimRig20 rig;           // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    s16            animState;     // Step of the animation (0 none, else `ACTOR_ENEMY_ANIM_BLEND`, `_RESET` or `_TICK`)
    s16            appliedAnimId; // Clip the slots were last seeded with; recorded, never read
    s16            animId;        // Clip the next reseed selects, an index into the figure's animation set table
    u16            animFrames;    // Updates since the last reseed; counted, never read
    u8             animRate;      // Rate given every slot ahead of a reseed, in sixteenths of a frame; nothing sets it, so it is 0, which a reseed from the clip's start replaces with `ANIMATION_RATE_ONE`
    byte           pad_47D[0x7];  // Never accessed
    MATRIX         light;         // Light-direction matrix lent to the model object
    MATRIX         color;         // Light-colour matrix lent to the model object
    s16            advanceFrames; // Ticks the figure has moved forward for; reaching `ADVANCE_FRAMES` ends its scene
    s16            advancing;     // Set once the figure's view has been shown, never cleared (0 standing where it was placed, 1 moving forward every tick)
    u8             clutGreyStep;  // Step of the palette greying (`CLUT_GREY_*`)
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0x4CC);

static void _securityMonitorsInitRupert(Enemy* enemy, Task* task);
static void _securityMonitorsUpdateRupert(Enemy* enemy, Task* task);
static void _securityMonitorsInitSwat(Enemy* enemy, Task* task);
static void _securityMonitorsUpdateSwat(Enemy* enemy, Task* task);
static s32  _actorMovementStepForwardNonzero(GfxCoord* coord, s16 stepDistance);
static void _securityMonitorsInitRupertLighting(Task* task);
static void _securityMonitorsInitSwatLighting(Task* task);
static void _securityMonitorsTickPaletteGreying(Task* task, s32 clutRowOffset, s16 workspaceRow);
static void _securityMonitorsUpdateAnimation(Task* task);

static TmdSource _gRupertBody;
static TmdSource _gSwatMember2Body;
static void      _securityMonitorsRupertTask(Task* task);
static void      _securityMonitorsSwatTask(Task* task);

#include "assets/characters/rupert/body.inc"

#include "assets/characters/swat_member_2/body.inc"

#include "assets/rooms/akropolis/security_room/security_monitors_clips.inc"

/// Rupert's animation-set table for the security-monitor scene.
#include "assets/rooms/akropolis/security_room/rupert_bank.inc"

/// The SWAT figure's animation-set table for the security-monitor scene.
#include "assets/rooms/akropolis/security_room/swat_bank.inc"

TaskDesc gSecurityMonitorsSwatTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_ACTOR } },
    .callback = _securityMonitorsSwatTask,
    .data     = { .model = &_gSwatMember2Body },
};

TaskDesc gSecurityMonitorsRupertTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_ACTOR } },
    .callback = _securityMonitorsRupertTask,
    .data     = { .model = &_gRupertBody },
};

/// Palette read-back and greying workspace: rows 0-1 Rupert's, 2-3 the SWAT member's.
static u16 _gSecurityMonitorsGreyPaletteRows[4][256];

/// Greys a figure's two palette rows over three calls, through two rows of the shared workspace.
static void _securityMonitorsTickPaletteGreying(Task* task, s32 clutRowOffset, s16 workspaceRow)
{
    enum {
        FIRST_ACTOR_PALETTE_ROW = 0xF5, // Row 245, before the placement's row offset
        CHANNEL_MASK            = 0x1F,
        GREEN_SHIFT             = 5,
        BLUE_SHIFT              = 10,
        CLUT_STP                = 0x8000,
    };
    RECT   rect;
    _Work* work;
    s32    entryIndex;

    /// Raises one RGB555 palette entry's channels to their maximum and sets STP.
#define GREY_CLUT_ENTRY(entry)                                                    \
    do {                                                                          \
        u16 red;                                                                  \
        u16 green;                                                                \
        u16 blue;                                                                 \
        red   = (entry) & CHANNEL_MASK;                                           \
        green = ((entry) >> GREEN_SHIFT) & CHANNEL_MASK;                          \
        blue  = ((entry) >> BLUE_SHIFT) & CHANNEL_MASK;                           \
        if (red < green) {                                                        \
            red = green;                                                          \
        } else {                                                                  \
            green = red;                                                          \
        }                                                                         \
        if (green < blue) {                                                       \
            green = blue;                                                         \
        } else {                                                                  \
            blue = green;                                                         \
        }                                                                         \
        if (blue < red) {                                                         \
            blue = red;                                                           \
        } else {                                                                  \
            red = blue;                                                           \
        }                                                                         \
        (entry) = red | (green << GREEN_SHIFT) | (blue << BLUE_SHIFT) | CLUT_STP; \
    } while (0)

    work = task->work;
    // Separate GPU read-back, CPU conversion and upload across successive ticks.
    if (work->clutGreyStep == CLUT_GREY_READ) {
        rect.x = 0;
        rect.y = clutRowOffset + FIRST_ACTOR_PALETTE_ROW;
        rect.w = ARRAY_SIZE(_gSecurityMonitorsGreyPaletteRows[0]);
        rect.h = 1;
        StoreImage2(&rect, (u_long*)_gSecurityMonitorsGreyPaletteRows[workspaceRow]);
        rect.x = 0;
        rect.y = clutRowOffset + FIRST_ACTOR_PALETTE_ROW + 1;
        rect.w = ARRAY_SIZE(_gSecurityMonitorsGreyPaletteRows[0]);
        rect.h = 1;
        StoreImage2(&rect, (u_long*)_gSecurityMonitorsGreyPaletteRows[workspaceRow + 1]);
        work->clutGreyStep = CLUT_GREY_CONVERT;
    } else if (work->clutGreyStep == CLUT_GREY_CONVERT) {
        for (entryIndex = 0; entryIndex < ARRAY_SIZE(_gSecurityMonitorsGreyPaletteRows[0]); entryIndex++) {
            GREY_CLUT_ENTRY(_gSecurityMonitorsGreyPaletteRows[workspaceRow][entryIndex]);
        }
        for (entryIndex = 0; entryIndex < ARRAY_SIZE(_gSecurityMonitorsGreyPaletteRows[0]); entryIndex++) {
            GREY_CLUT_ENTRY(_gSecurityMonitorsGreyPaletteRows[workspaceRow + 1][entryIndex]);
        }
#undef GREY_CLUT_ENTRY
        work->clutGreyStep = CLUT_GREY_UPLOAD;
    } else if (work->clutGreyStep == CLUT_GREY_UPLOAD) {
        rect.x = 0;
        rect.y = clutRowOffset + FIRST_ACTOR_PALETTE_ROW;
        rect.w = ARRAY_SIZE(_gSecurityMonitorsGreyPaletteRows[0]);
        rect.h = 1;
        LoadImage2(&rect, (u_long*)_gSecurityMonitorsGreyPaletteRows[workspaceRow]);
        rect.x = 0;
        rect.y = clutRowOffset + FIRST_ACTOR_PALETTE_ROW + 1;
        rect.w = ARRAY_SIZE(_gSecurityMonitorsGreyPaletteRows[0]);
        rect.h = 1;
        LoadImage2(&rect, (u_long*)_gSecurityMonitorsGreyPaletteRows[workspaceRow + 1]);
        work->clutGreyStep = CLUT_GREY_DONE;
    }
}

/// Completes a clip reseed and schedules playback with a fresh tick count.
static inline void _securityMonitorsCommitAnimationRequest(_Work* playbackWork, _Work* reseedWork)
{
    reseedWork->appliedAnimId = reseedWork->animId;
    playbackWork->animState   = ACTOR_ENEMY_ANIM_TICK;
    playbackWork->animFrames  = 0;
}

/// Applies an animation request or advances the figure's nineteen animated parts.
static void _securityMonitorsUpdateAnimation(Task* task)
{
    _Work* work;
    _Work* reseedWork;
    _Work* playbackWork;
    s32    blendSlot;
    s32    resetSlot;
    s32    tickSlot;

    work = task->work;
    // Reseeding leaves the root untouched and schedules playback for the next update.
    if (work->animState == ACTOR_ENEMY_ANIM_BLEND) {
        reseedWork = task->work;
        for (blendSlot = 1; blendSlot < ARRAY_SIZE(reseedWork->rig.slots); blendSlot++) {
            reseedWork->rig.slots[blendSlot].rate = reseedWork->animRate;
            animationSeekSlotWithBlend(&reseedWork->rig.anim, blendSlot, reseedWork->animId, 0, 0);
        }
        _securityMonitorsCommitAnimationRequest(work, reseedWork);
        return;
    }
    if (work->animState == ACTOR_ENEMY_ANIM_RESET) {
        reseedWork = task->work;
        for (resetSlot = 1; resetSlot < ARRAY_SIZE(reseedWork->rig.slots); resetSlot++) {
            reseedWork->rig.slots[resetSlot].rate = reseedWork->animRate;
            animationResetSlot(&reseedWork->rig.anim, resetSlot, reseedWork->animId);
        }
        _securityMonitorsCommitAnimationRequest(work, reseedWork);
        return;
    }
    if (work->animState == ACTOR_ENEMY_ANIM_TICK) {
        work->animFrames++;
        playbackWork = task->work;
        for (tickSlot = 1; tickSlot < ARRAY_SIZE(playbackWork->rig.slots); tickSlot++) {
            animationTickSlot(&playbackWork->rig.anim, tickSlot);
        }
    }
}

enum {
    RUPERT_TASK_INITIALIZE = 0,
    RUPERT_TASK_UPDATE     = 1,
    RUPERT_TASK_DESTROY    = 2,
};

/// Rupert security-monitor lifecycle handlers: 0 initialize, 1 walk/playback, 2 destroy.
static const EnemyTaskFuncTable3 _gSecurityMonitorsRupertTaskStates = {
    .funcs = {
        [RUPERT_TASK_INITIALIZE] = _securityMonitorsInitRupert,
        [RUPERT_TASK_UPDATE]     = _securityMonitorsUpdateRupert,
        [RUPERT_TASK_DESTROY]    = enemyDestroy,
    },
};

enum {
    SWAT_TASK_INITIALIZE = 0,
    SWAT_TASK_UPDATE     = 1,
    SWAT_TASK_DESTROY    = 2,
};

/// SWAT security-monitor lifecycle handlers: 0 initialize, 1 playback, 2 destroy.
static const EnemyTaskFuncTable3 _gSecurityMonitorsSwatTaskStates = {
    .funcs = {
        [SWAT_TASK_INITIALIZE] = _securityMonitorsInitSwat,
        [SWAT_TASK_UPDATE]     = _securityMonitorsUpdateSwat,
        [SWAT_TASK_DESTROY]    = enemyDestroy,
    },
};

/// Dispatches Rupert's security-monitor task through initialization, playback and teardown.
static void _securityMonitorsRupertTask(Task* task)
{
    Enemy*              enemy;
    EnemyTaskFuncTable3 stateHandlers;

    enemy         = task->spawnArg2.pointer;
    stateHandlers = _gSecurityMonitorsRupertTaskStates;
    stateHandlers.funcs[task->state](enemy, task);
}

/// Initializes Rupert's animated model for his one-shot security-monitor scene.
static void _securityMonitorsInitRupert(Enemy* enemy, Task* task)
{
    _Work*     work;
    GfxCoord*  rootCoord;
    TmdObject* model;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_SCENES_SEEN) & AKROPOLIS_MONITOR_RUPERT_SCENE_SEEN) ||
        (work = memCalloc(sizeof(_Work), 0), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }
    _securityMonitorsInitRupertLighting(task);
    enemy->field_4  = &rootCoord->coord;
    enemy->field_48 = 0;
    model->flags    = 0;
    animationInitContext(&work->rig.anim, _gSecurityMonitorsRupertAnimationSets, model, work->rig.poses,
                         work->rig.slots);
    rootCoord->parent   = &gGfxViewCoord;
    work->animState     = ACTOR_ENEMY_ANIM_RESET;
    work->animId        = ANIMATION_SET;
    work->advanceFrames = 0;
    work->advancing     = 0;
    if ((viewGetMappedIndex() & 0xFF) == RUPERT_VIEW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    _securityMonitorsUpdateAnimation(task);
    task->state += 1;
}

/// Advances Rupert's security-monitor scene and completes it after ninety walking updates.
static void _securityMonitorsUpdateRupert(Enemy* enemy, Task* task)
{
    enum {
        ADVANCE_DISTANCE = 0x24, // Parent-coordinate units per tick
        CLUT_ROW_OFFSET  = 2,
        WORKSPACE_ROW    = 0,
    };
    _Work*     work;
    GfxCoord*  rootCoord;
    TmdObject* model;

    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    _securityMonitorsTickPaletteGreying(task, CLUT_ROW_OFFSET, WORKSPACE_ROW);
    if ((viewGetMappedIndex() & 0xFF) == RUPERT_VIEW) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_SCENES_SEEN, gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_SCENES_SEEN) | AKROPOLIS_MONITOR_RUPERT_SCENE_SEEN);
        model->flags    = 0;
        work->advancing = 1;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // Once seen, the walk continues even while another camera hides the model.
    if (work->advancing == 1) {
        work->advanceFrames++;
        _actorMovementStepForwardNonzero(rootCoord, ADVANCE_DISTANCE);
    }
    _securityMonitorsUpdateAnimation(task);
    if (work->advanceFrames >= ADVANCE_FRAMES) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_RUPERT_SCENE_SEEN, 1);
        task->state++;
    }
}

/// Dispatches the SWAT figure's security-monitor task through initialization, playback and teardown.
static void _securityMonitorsSwatTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = _gSecurityMonitorsSwatTaskStates;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Initializes the stationary SWAT figure shown on its security-monitor camera.
static void _securityMonitorsInitSwat(Enemy* enemy, Task* task)
{
    _Work*     work;
    GfxCoord*  rootCoord;
    TmdObject* model;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS) >= AKROPOLIS_SANCTUARY_ROUTE_OBSERVATORY_ENTERED ||
        (work = memCalloc(sizeof(_Work), 0), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }
    _securityMonitorsInitSwatLighting(task);
    enemy->field_4  = &rootCoord->coord;
    enemy->field_48 = 0;
    model->flags    = 0;
    animationInitContext(&work->rig.anim, _gSecurityMonitorsSwatAnimationSets, model, work->rig.poses,
                         work->rig.slots);
    rootCoord->parent = &gGfxViewCoord;
    work->animState   = ACTOR_ENEMY_ANIM_RESET;
    work->animId      = ANIMATION_SET;
    _securityMonitorsUpdateAnimation(task);
    task->state += 1;
}

/// Greys and animates the stationary SWAT figure, drawing it only on its mapped camera.
static void _securityMonitorsUpdateSwat(Enemy* enemy, Task* task)
{
    enum {
        CLUT_ROW_OFFSET = 4,
        WORKSPACE_ROW   = 2,
    };
    TmdObject* model;

    model = task->extra.tmd;
    _securityMonitorsTickPaletteGreying(task, CLUT_ROW_OFFSET, WORKSPACE_ROW);
    if ((viewGetMappedIndex() & 0xFF) == SWAT_VIEW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    _securityMonitorsUpdateAnimation(task);
}

/// Translates a coordinate along its normalized local Z axis for a nonzero signed step.
static s32 _actorMovementStepForwardNonzero(GfxCoord* coord, s16 stepDistance)
{
    enum { ACTOR_MOVEMENT_FROZEN = 1 };
    SVECTOR* scratchEnd;
    SVECTOR* displacement;
    SVECTOR* gteDisplacement;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == ACTOR_MOVEMENT_FROZEN) {
        return 0;
    }
    scratchEnd                    = SCRATCH_STACK_CURSOR(SVECTOR);
    displacement                  = scratchEnd - 1;
    gteDisplacement               = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = displacement;
    if (stepDistance != 0) {
        // The local axis and translation share the coordinate's parent space.
        gfxReadMatrixZAxis(&coord->coord, displacement);
        VectorNormalSS(displacement, displacement);
        gte_lddp(stepDistance);
        gte_ldsv(gteDisplacement);
        gte_gpf12();
        gte_stsv(gteDisplacement);
        coord->coord.t[0]  += scratchEnd[-1].vx;
        coord->coord.t[1]  += displacement->vy;
        coord->coord.t[2]  += displacement->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return stepDistance;
}

/// Installs Rupert's fixed monochrome lighting matrices on his monitor model.
static void _securityMonitorsInitRupertLighting(Task* task)
{
    TmdObject* model;
    _Work*     work;

    work  = task->work;
    model = task->extra.tmd;

    // Seed through the packed view, then install the figure's coefficients.
    gfxSetRotIdentity(&work->light);
    gfxSetRotIdentity(&work->color);

    model->lightMtx = &work->light;

    work->color.m[0][0] = ONE;
    work->color.m[0][1] = ONE;
    work->color.m[0][2] = ONE;
    work->color.m[1][0] = ONE;
    work->color.m[1][1] = ONE;
    work->color.m[1][2] = ONE;
    work->color.m[2][0] = ONE;
    work->color.m[2][1] = ONE;
    work->color.m[2][2] = ONE;

    work->light.m[0][0] = ONE;
    work->light.m[0][1] = ONE;
    work->light.m[0][2] = ONE;
    work->light.m[1][0] = 0;
    work->light.m[1][1] = ONE;
    work->light.m[1][2] = ONE;
    work->light.m[2][0] = ONE;
    work->light.m[2][1] = ONE;
    work->light.m[2][2] = 0;

    model->colorMtx = &work->color;
}

/// Installs the SWAT figure's fixed monochrome lighting matrices on its monitor model.
static void _securityMonitorsInitSwatLighting(Task* task)
{
    TmdObject* model;
    _Work*     work;

    work  = task->work;
    model = task->extra.tmd;

    // Seed through the packed view, then install the figure's coefficients.
    gfxSetRotIdentity(&work->light);
    gfxSetRotIdentity(&work->color);

    model->lightMtx = &work->light;

    work->color.m[0][0] = ONE;
    work->color.m[0][1] = ONE;
    work->color.m[0][2] = ONE;
    work->color.m[1][0] = ONE;
    work->color.m[1][1] = ONE;
    work->color.m[1][2] = ONE;
    work->color.m[2][0] = ONE;
    work->color.m[2][1] = ONE;
    work->color.m[2][2] = ONE;

    work->light.m[0][0] = -ONE;
    work->light.m[0][1] = ONE;
    work->light.m[0][2] = ONE;
    work->light.m[1][0] = ONE;
    work->light.m[1][1] = ONE;
    work->light.m[1][2] = ONE;
    work->light.m[2][0] = ONE;
    work->light.m[2][1] = ONE;
    work->light.m[2][2] = ONE;

    model->colorMtx = &work->color;
}
