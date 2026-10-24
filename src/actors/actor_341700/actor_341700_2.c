#include "actors/actor_341700.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage_types.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"
#include "../../shared/actor/actor_messages.h"

/// Values of `_Actor341700PropWork::state`.
enum {
    ACTOR_341700_PROP_STATE_HIDDEN = 0, // not drawn; entered by every dumping-hole command but 1
    ACTOR_341700_PROP_STATE_SHOWN  = 1, // drawn where it stands; the state the spawn leaves it in
    ACTOR_341700_PROP_STATE_RISE   = 2  // dropped 1800 below its origin and shaken back up, shedding the room's rising sprites; entered by dumping-hole command 1
};

/// Work block of the package's scripted prop, the model it carries beside its
/// Mad Chasers for the Shelter B3 dumping hole's event.
///
/// The prop's spawn handler allocates it zeroed and keeps it at `Task::work`;
/// its size is that allocation's. The prop has no hit points and can never be
/// locked onto. It only stands hidden, stands shown, or rises shaking from
/// below its origin, as the room's actor commands select.
typedef struct {
    s16    state;            // `ACTOR_341700_PROP_STATE_*`
    s16    prevState;        // `state` the tick last ran; -1 from spawn, so the first tick enters `state` afresh
    s16    stateEntered;     // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16    stateFrame;       // ticks `ACTOR_341700_PROP_STATE_RISE` has run, counted from 1; times its shake phases and sprite bursts
    byte   pad_8[0x10];      // never accessed
    u8     lastCommandStage; // stage tag of the last actor command received, whatever its namespace; never read
    u8     lastCommandArea;  // area tag of that command; never read
    u8     lastCommand;      // low byte of that command's selector; never read
    byte   pad_1B[0x1];      // never accessed
    MATRIX lightMtx;         // storage for the model's `TmdObject::lightMtx`
    MATRIX colorMtx;         // storage for the model's `TmdObject::colorMtx`
    byte   pad_5C[0x24];     // never accessed
} _Actor341700PropWork;
STATIC_ASSERT_SIZEOF(_Actor341700PropWork, 0x80);

/// Persistent record of this carrier's last grid-contact correction, in whole room units.
///
/// X/Z are the applied steps narrowed to s16: the arithmetic 16.16 integer
/// part plus a unit in the correction's sign when fractional bits remain.
/// Y records only the narrowed integer part and is not applied to translation.
/// No grid hit retains the previous XYZ; `pad` is untouched. Lives until unload.
static SVECTOR _gActorContactLastPushCorrection;

/// Psy-Q `RotMatrixY`.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void _actor341700PropInitialize(Enemy* enemy, Task* task);
static void _actor341700PropHiddenState(Enemy* enemy, Task* task);
static void _actor341700PropShownState(Enemy* enemy, Task* task);

static TmdSource _gActor341700Model13558;
static void      _actor341700PropTask(Task* task);

static s32 _actor341700PropSetModelDrawMessage(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static s32 _actor341700PropCommandMessage(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

#include "../../shared/actor/actor_contacts.h"

static TmdBone _gActor341700Model13558Skeleton[11] = {
#include "assets/actor_341700_model_13558_skeleton.inc"
};

static u32 _gActor341700Model13558PartVerts[11] = {
#include "assets/actor_341700_model_13558_partVerts.inc"
};

static SVECTOR _gActor341700Model13558Verts[81] = {
#include "assets/actor_341700_model_13558_verts.inc"
};

static SVECTOR _gActor341700Model13558Normals[46] = {
#include "assets/actor_341700_model_13558_normals.inc"
};

static u32 _gActor341700Model13558Stream[753] = {
#include "assets/actor_341700_model_13558_stream.inc"
};

static TmdSource _gActor341700Model13558 = {
    0,
    3184,
    1720,
    11,
    _gActor341700Model13558PartVerts,
    _gActor341700Model13558Verts,
    _gActor341700Model13558Normals,
    _gActor341700Model13558Skeleton,
    _gActor341700Model13558Stream,
};

/// Model visibility, scripted command and placement messages of the rising prop.
///
/// SET_MODEL_DRAW takes a scalar mode; APPLY borrows an ActorCommand and PLACE
/// borrows an ActorTransform through synchronous dispatch. The fourth entry
/// ends lookup. Tasks borrow this table and its s32-returning callbacks while
/// the overlay is loaded; visibility returns 0, command and placement return 1.
static TaskMessageEntry _gActor341700PropMessages[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor341700PropSetModelDrawMessage },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor341700PropCommandMessage },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-space sprite offsets, in game units, for the prop's jolts and rise.
///
/// Jolt bursts select entries 35, 8, 25, 24 and 26. The frame schedule supplies
/// other indices; zero ends a schedule row. Its late-rise reads extend into
/// adjacent storage, whose interpretation as additional vectors is unproven.
static SVECTOR _gActor341700PropSpriteOffsets[48] = {
    { 0, 0, 0, 0 },
    { 0, -2500, 3000, 0 },
    { 200, -2400, 2500, 0 },
    { 0, -2500, 2000, 0 },
    { 200, -2400, 1500, 0 },
    { 0, -2500, 1000, 0 },
    { 200, -2400, 500, 0 },
    { 0, -2500, 0, 0 },
    { 200, -2400, -500, 0 },
    { 0, -2500, -1000, 0 },
    { 200, -2400, -1500, 0 },
    { 0, -2500, -2000, 0 },
    { 200, -2400, -2500, 0 },
    { 0, -2500, -3000, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 1500, -1500, 3000, 0 },
    { 1700, -1100, 2500, 0 },
    { 1500, -1500, 2000, 0 },
    { 1700, -1100, 1500, 0 },
    { 1500, -1500, 1000, 0 },
    { 1700, -1100, 500, 0 },
    { 1500, -1500, 0, 0 },
    { 1700, -1100, -500, 0 },
    { 1500, -1500, -1000, 0 },
    { 1700, -1100, -1500, 0 },
    { 1500, -1500, -2000, 0 },
    { 1700, -1100, -2500, 0 },
    { 1500, -1500, -3000, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 2500, -500, 3000, 0 },
    { 2700, -400, 2500, 0 },
    { 2500, -500, 2000, 0 },
    { 2700, -400, 1500, 0 },
    { 2500, -500, 1000, 0 },
    { 2700, -400, 500, 0 },
    { 2500, -500, 0, 0 },
    { 2700, -400, -500, 0 },
    { 2500, -500, -1000, 0 },
    { 2700, -400, -1500, 0 },
    { 2500, -500, -2000, 0 },
    { 2700, -400, -2500, 0 },
    { 2500, -500, -3000, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

/// Four sprite-offset indices per rise frame, with zero ending each row.
///
/// Row 0 corresponds to frame 120; only rows 0..120 (frames 120..240) are used.
/// All 150 rows are retained. Rows 104, 106 and 109 select 50, 58 and 52 beyond
/// the 48-vector offset table; the meaning of those adjacent reads is unproven.
static u8 _gActor341700PropSpriteSchedule[150][4] = {
#include "assets/actor_341700_motion_142DC.inc"
};

TaskDesc gActor341700PropTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_ACTOR } }, _actor341700PropTask, { .model = &_gActor341700Model13558 } };

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &_gActorContactLastPushCorrection;
}

static void _actor341700PropRiseState(Enemy* enemy, Task* task);
static void _actor341700PropUpdate(Enemy* enemy, Task* task);

#include "../../shared/actor/actor_contacts.h"

#include "../../shared/actor/actor_contacts.inc.c"

/// Advances the dumping-hole prop's bend by rotating nine parts about local X.
///
/// Borrows a live TMD prop task with its 11 initialized model coordinates.
/// `angleStep` is a signed increment in 4096 units per turn; each part applies
/// its own multiple. Callers use +/-1 for the initial jolt, +/-2 for the slow
/// rise and +/-3 for the fast rise and settled shake. These increments compose
/// with the existing rotations rather than setting an absolute pose.
/// Translations, parent links, root and part 7 rotations are preserved.
/// The caller must invalidate composed transforms before using them again.
static inline void _actor341700PropBendParts(const Task* propTask, s32 angleStep)
{
    gfxRotMatrixX(&propTask->extra.tmd->coords[1].coord, 4 * angleStep, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&propTask->extra.tmd->coords[2].coord, 8 * angleStep, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&propTask->extra.tmd->coords[3].coord, -8 * angleStep, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&propTask->extra.tmd->coords[4].coord, 4 * angleStep, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&propTask->extra.tmd->coords[5].coord, -2 * angleStep, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&propTask->extra.tmd->coords[6].coord, 6 * angleStep, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&propTask->extra.tmd->coords[8].coord, -2 * angleStep, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&propTask->extra.tmd->coords[9].coord, -angleStep, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&propTask->extra.tmd->coords[10].coord, -9 * angleStep, GRAPHICS_ROTATION_COMPOSE);
}

/// Raises the dumping-hole prop with timed jolts, part rotations and sprite bursts.
///
/// Requires the prop's 11-coordinate model and initialized work. Entry starts
/// the root at Y=1800 in its parent's frame; each running tick advances the
/// signed 16-bit frame counter. At or below Y=200 it keeps shaking without
/// selecting another state. Sprite rows are used for frames 120..240, with
/// zero ending a row. The room's sprite director must remain live; spawned
/// sprites borrow the prop root through their first update. `enemy` is unused.
static void _actor341700PropRiseState(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_341700_PROP_RISE_START_Y       = 1800,
        ACTOR_341700_PROP_RISE_SETTLED_Y     = 200,
        ACTOR_341700_PROP_JOLT_FIRST_FRAME   = 11,
        ACTOR_341700_PROP_JOLT_END_FRAME     = 18,
        ACTOR_341700_PROP_SLOW_SHAKE_FRAME   = 121,
        ACTOR_341700_PROP_FAST_SHAKE_FRAME   = 166,
        ACTOR_341700_PROP_SPRITE_FIRST_FRAME = 120,
        ACTOR_341700_PROP_SPRITE_LAST_FRAME  = 240,
        ACTOR_341700_PROP_SPRITE_ROW_END     = 0,
    };
    SVECTOR               spriteOffset;
    _Actor341700PropWork* work = task->work;
    s16                   spriteSlot;
    s16                   jitter;
    u32                   randomByte;
    u32                   nextRandomState;

    if (work->stateEntered != 0) {
        work->stateFrame                    = 0;
        task->extra.tmd->coords->coord.t[1] = ACTOR_341700_PROP_RISE_START_Y;
    }
    work->stateFrame++;
    // Four-tick vertical cycles rise gradually; six-tick part bends reverse.
    if (task->extra.tmd->coords->coord.t[1] > ACTOR_341700_PROP_RISE_SETTLED_Y) {
        if (work->stateFrame >= ACTOR_341700_PROP_FAST_SHAKE_FRAME) {
            if (work->stateFrame % 4 < 2) {
                task->extra.tmd->coords->coord.t[1] += 90;
            } else {
                task->extra.tmd->coords->coord.t[1] -= 100;
            }
            if (work->stateFrame % 6 < 3) {
                _actor341700PropBendParts(task, 3);
            } else {
                _actor341700PropBendParts(task, -3);
            }
        } else if (work->stateFrame >= ACTOR_341700_PROP_SLOW_SHAKE_FRAME) {
            if (work->stateFrame % 4 < 2) {
                task->extra.tmd->coords->coord.t[1] += 50;
            } else {
                task->extra.tmd->coords->coord.t[1] -= 58;
            }
            if (work->stateFrame % 6 < 3) {
                _actor341700PropBendParts(task, 2);
            } else {
                _actor341700PropBendParts(task, -2);
            }
        } else if (work->stateFrame >= ACTOR_341700_PROP_JOLT_FIRST_FRAME && work->stateFrame < ACTOR_341700_PROP_JOLT_END_FRAME) {
            if (work->stateFrame % 4 < 2) {
                task->extra.tmd->coords->coord.t[1] += 30;
            } else {
                task->extra.tmd->coords->coord.t[1] -= 33;
            }
            if (work->stateFrame % 6 < 3) {
                _actor341700PropBendParts(task, 1);
            } else {
                _actor341700PropBendParts(task, -1);
            }
            switch ((work->stateFrame - ACTOR_341700_PROP_JOLT_FIRST_FRAME) % 8) {
                case 0:
                    spriteOffset = _gActor341700PropSpriteOffsets[35];
                    shelterB3DumpingHoleSpawnActorSprite(task->extra.tmd->coords, &spriteOffset);
                    break;
                case 1:
                    spriteOffset = _gActor341700PropSpriteOffsets[8];
                    shelterB3DumpingHoleSpawnActorSprite(task->extra.tmd->coords, &spriteOffset);
                    break;
                case 2:
                    spriteOffset = _gActor341700PropSpriteOffsets[25];
                    shelterB3DumpingHoleSpawnActorSprite(task->extra.tmd->coords, &spriteOffset);
                    break;
                case 4:
                    spriteOffset = _gActor341700PropSpriteOffsets[24];
                    shelterB3DumpingHoleSpawnActorSprite(task->extra.tmd->coords, &spriteOffset);
                    break;
                case 5:
                    spriteOffset = _gActor341700PropSpriteOffsets[26];
                    shelterB3DumpingHoleSpawnActorSprite(task->extra.tmd->coords, &spriteOffset);
                    break;
            }
        }
    } else {
        if (work->stateFrame % 4 < 2) {
            task->extra.tmd->coords->coord.t[1] += 100;
        } else {
            task->extra.tmd->coords->coord.t[1] -= 100;
        }
        if (work->stateFrame % 6 < 3) {
            _actor341700PropBendParts(task, 3);
        } else {
            _actor341700PropBendParts(task, -3);
        }
    }
    // Rotations and root movement invalidate every part's composed transform.
    task->extra.tmd->coords[0].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[1].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[2].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[3].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[4].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[5].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[6].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[7].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[8].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[9].composeStamp  = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
    // Each row selects world-space offsets; jitter uses the shared LCG stream.
    for (spriteSlot = 0; spriteSlot < (s32)ARRAY_SIZE(_gActor341700PropSpriteSchedule[0]); spriteSlot++) {
        if (work->stateFrame > ACTOR_341700_PROP_SPRITE_LAST_FRAME) {
            return;
        }
        if (work->stateFrame < ACTOR_341700_PROP_SPRITE_FIRST_FRAME) {
            return;
        }
        if (_gActor341700PropSpriteSchedule[work->stateFrame - ACTOR_341700_PROP_SPRITE_FIRST_FRAME][spriteSlot] == ACTOR_341700_PROP_SPRITE_ROW_END) {
            return;
        }
        spriteOffset    = _gActor341700PropSpriteOffsets[_gActor341700PropSpriteSchedule[work->stateFrame - ACTOR_341700_PROP_SPRITE_FIRST_FRAME][spriteSlot]];
        nextRandomState = RANDOM_LCG_NEXT(gRandomLcgState);
        randomByte      = (nextRandomState >> 16) & 0xFF;
        jitter          = randomByte;
        gRandomLcgState = nextRandomState;
        if (jitter % 2) {
            jitter = -randomByte;
        }
        spriteOffset.vx += jitter / 2;
        spriteOffset.vy -= jitter;
        spriteOffset.vz += jitter;
        shelterB3DumpingHoleSpawnActorSprite(task->extra.tmd->coords, &spriteOffset);
    }
}

/// Hidden, shown and rise behaviors selected by the prop's work state.
///
/// `ACTOR_341700_PROP_STATE_*` selects 0 hidden, 1 shown, 2 rise. The running
/// update copies all three non-NULL void(Enemy*, Task*) callbacks before dispatch.
/// Keep the index in range and the overlay loaded; enemy/model/work belong to the task.
static const EnemyTaskFuncTable3 _gActor341700PropStateHandlers = { {
    _actor341700PropHiddenState,
    _actor341700PropShownState,
    _actor341700PropRiseState,
} };

/// Updates prop lighting and runs its hidden, shown or rise state while actors run.
///
/// Requires initialized prop work and its live enemy/model. Lighting samples
/// coordinate 1's composed translation even while paused or hidden. Hidden control
/// sets draw exclusion; paused control leaves state timing untouched. Running
/// control dispatches work state 0..2 and marks state entry before the call.
static void _actor341700PropUpdate(Enemy* enemy, Task* task)
{
    VECTOR                lightingPosition;
    _Actor341700PropWork* work          = task->work;
    EnemyTaskFuncTable3   stateHandlers = _gActor341700PropStateHandlers;

    // Sample the moving part's translation after composing the full view chain.
    task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[1]);
    lightingPosition.vx = task->extra.tmd->coords[1].workm.t[0];
    lightingPosition.vy = task->extra.tmd->coords[1].workm.t[1];
    lightingPosition.vz = task->extra.tmd->coords[1].workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            // Only running updates consume a state transition or advance its time.
            if (work->prevState != work->state) {
                work->stateEntered = true;
            } else {
                work->stateEntered = false;
            }
            work->prevState = work->state;
            stateHandlers.funcs[work->state](enemy, task);
            if (gGameSession->viewReady != 0) {
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            return;
    }
}

/// Lifecycle callbacks of the rising prop's enemy-backed task.
///
/// `Task::state` selects 0 initialize, 1 update, 2 destroy. Dispatch copies
/// all three non-NULL void(Enemy*, Task*) callbacks and passes the enemy from
/// the second spawn argument. Destruction releases the task and its enemy;
/// both objects and the callback code must remain live until the selected call.
static const EnemyTaskFuncTable3 _gActor341700PropTaskHandlers = { {
    _actor341700PropInitialize,
    _actor341700PropUpdate,
    enemyDestroy,
} };

/// Sets the prop's model draw and primitive-buffer policy without changing its state.
///
/// Requires a live TMD task. Mode 0 replaces flags with draw exclusion and
/// mode 1 clears flags; both allocate a buffer if absent. Mode 2 adds automatic
/// buffer suppression, and mode 3 replaces all flags with that bit. Neither
/// releases an existing buffer. Other modes change nothing. The message ID
/// and second payload are ignored; returns 0 even if allocation fails.
static s32 _actor341700PropSetModelDrawMessage(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    TmdObject* model = task->extra.tmd;

    switch (mode) {
        case ACTOR_MESSAGE_VISIBILITY_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            break;
        case ACTOR_MESSAGE_VISIBILITY_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            break;
        case ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_VISIBILITY_CLEAR_FLAGS_SKIP_AUTO_BUFFER:
            model->flags = TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Applies the dumping-hole prop's hide, rise or hide-and-resume command.
///
/// Requires initialized prop work and a readable command borrowed through this
/// call. Every namespace's stage, area and low command byte are recorded.
/// In the Shelter B3 dumping-hole namespace, command 0 selects hidden, command
/// 1 selects rise and invalidates the root transform, and every other command
/// selects hidden and task update state 1. State entry is deferred until the
/// next running update; repeating a state does not restart it. Other namespaces
/// change no state. The message ID and second payload are ignored; returns 1.
static s32 _actor341700PropCommandMessage(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_341700_PROP_COMMAND_CONTEXT         = GAME_STAGE_MINE_SHELTER | (GAME_AREA_SHELTER_B3_DUMPING_HOLE << 8),
        ACTOR_341700_PROP_COMMAND_HIDE            = 0,
        ACTOR_341700_PROP_COMMAND_RISE            = 1,
        ACTOR_341700_PROP_COMMAND_HIDE_AND_RESUME = 2,
        ACTOR_341700_PROP_TASK_UPDATE             = 1,
    };
    _Actor341700PropWork* work = task->work;

    work->lastCommandStage = command->context.loc.stage;
    work->lastCommandArea  = command->context.loc.area;
    work->lastCommand      = (u8)command->command;

    if (command->context.key == ACTOR_341700_PROP_COMMAND_CONTEXT) {
        switch (command->command) {
            case ACTOR_341700_PROP_COMMAND_HIDE:
                work->state = ACTOR_341700_PROP_STATE_HIDDEN;
                return 1;
            case ACTOR_341700_PROP_COMMAND_RISE:
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->state                           = ACTOR_341700_PROP_STATE_RISE;
                break;
            case ACTOR_341700_PROP_COMMAND_HIDE_AND_RESUME:
            default:
                work->state = ACTOR_341700_PROP_STATE_HIDDEN;
                task->state = ACTOR_341700_PROP_TASK_UPDATE;
                break;
        }
    }
    return 1;
}

#include "../../shared/actor/actor_messages_place.inc.c"

#include "../../shared/actor/coord_math_yaw_scale.inc.c"

/// Initializes the prop's task-owned work, lighting and untargetable enemy record.
///
/// Requires the descriptor's 11-coordinate model and live enemy owned by this
/// task. The root starts at (5000, 0, -6000) beneath the view coordinate; placement
/// messages may replace that transform. Work supplies the model's light/color
/// matrices until task teardown. Allocation failure destroys the enemy/task;
/// success selects shown and advances task state 0 to update state 1.
static void _actor341700PropInitialize(Enemy* enemy, Task* task)
{
    enum { ACTOR_341700_PROP_NO_PREVIOUS_STATE = -1 };
    _Actor341700PropWork* work;
    TmdObject*            model;
    GfxCoord*             rootCoord;
    VECTOR                lightingPosition;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    task->work = work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Join the view chain; the enemy has no combat or lock-on role.
    rootCoord->parent                     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[1]   = 0;
    task->extra.tmd->coords->coord.t[0]   = 5000;
    task->extra.tmd->coords->coord.t[2]   = -6000;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    task->msgTable                        = _gActor341700PropMessages;
    enemy->field_4                        = &rootCoord->coord;
    enemy->field_48                       = 0;
    enemy->bodyPos.vx                     = 0;
    enemy->bodyPos.vy                     = 0;
    enemy->bodyPos.vz                     = 0;
    enemy->coord                          = &task->extra.tmd->coords[2];
    enemy->node.state.parts.flags         = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags                  = 0;
    enemy->hpMax                          = 0;
    enemy->hp                             = 0;
    // Bind task-owned lighting storage before evaluating the initial position.
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1];
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
    work->prevState = ACTOR_341700_PROP_NO_PREVIOUS_STATE;
    work->state     = ACTOR_341700_PROP_STATE_SHOWN;
    task->state    += 1;
}

/// On hidden-state entry, excludes the prop from drawing and automatic buffer allocation.
///
/// Requires initialized prop work and its live enemy/model. Existing buffers
/// are retained, and the enemy remains untargetable. Later ticks do nothing.
static void _actor341700PropHiddenState(Enemy* enemy, Task* task)
{
    _Actor341700PropWork* work = task->work;
    TmdObject*            model;

    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    }
}

/// On shown-state entry, clears model flags and allocates a missing primitive buffer.
///
/// Requires initialized prop work and its live enemy/model. The enemy remains
/// untargetable. Allocation failure is left to automatic buffer recovery;
/// later ticks do nothing.
static void _actor341700PropShownState(Enemy* enemy, Task* task)
{
    _Actor341700PropWork* work = task->work;
    TmdObject*            model;

    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
    }
}

/// Dispatches the scripted prop's task lifecycle with its owned enemy record.
///
/// Requires a live enemy in `spawnArg2.pointer` and task state 0..2:
/// 0 initializes, 1 updates, and 2 destroys. The selected handler may release
/// both objects; this dispatcher accesses neither after the call.
static void _actor341700PropTask(Task* task)
{
    EnemyTaskFuncTable3 taskHandlers;

    taskHandlers = _gActor341700PropTaskHandlers;
    taskHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
