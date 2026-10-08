#include "actor_205200_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/player_state.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/neo_ark_eve_access_tunnel.h"

#include "rooms/shelter_b6_corridor.h"

#include "rooms/shelter_b6_training_room.h"
#include "../../shared/screen_wave.h"

/// Room a controller is placed in, taken from its placement's `mode` and
/// stored in `_Actor205200CtrlWork.site`.
///
/// Each site has its own number of parts, part positions and headings, room
/// sprite batches and game flags. A placement with any other mode spawns
/// nothing.
enum {
    ACTOR_205200_SITE_EVE_ACCESS_TUNNEL = 1, // Neo Ark Eve access tunnel, two parts
    ACTOR_205200_SITE_B6_CORRIDOR       = 2, // Shelter B6 corridor, three parts
    ACTOR_205200_SITE_B6_TRAINING_ROOM  = 3, // Shelter B6 training room, two parts
};

/// Stage of a controller, stored in `_Actor205200CtrlWork.state`.
enum {
    ACTOR_205200_CTRL_STARTING = 0, // Counting `startDelay` down, then starting the sustained sound
    ACTOR_205200_CTRL_PULSING  = 1, // Parts are live and the pulses repeat
    ACTOR_205200_CTRL_STOPPING = 2, // The last part is gone: release the wave and stop the sound
    ACTOR_205200_CTRL_STOPPED  = 3, // Nothing left to run
};

/// Half of the pulse cycle a controller is in, stored in
/// `_Actor205200CtrlWork.pulseState`.
enum {
    ACTOR_205200_PULSE_WAITING = 0, // Counting down to the next pulse
    ACTOR_205200_PULSE_RAISED  = 1, // The wave is up; counting down to the MP loss
};

enum {
    /// Frames a controller waits after spawning before it starts its sound.
    ACTOR_205200_START_DELAY = 5,
    /// Frames between a pulse being raised and the player losing MP to it.
    ACTOR_205200_PULSE_RAISED_FRAMES = 20,
};

/// Work block of the controller task, the enemy a placement spawns.
///
/// The controller spawns the destructible parts of its site as child enemies.
/// While any part lives it keeps one sound playing, attenuated by the distance
/// of the live part nearest the view, and pulses: each pulse raises a screen
/// wave and takes one MP from the player, and the pulses come faster the more
/// parts are live. A part takes a slot here when it spawns and gives it up
/// when it is destroyed.
typedef struct {
    GfxCoord* partCoords[3];    // Coordinate frame of each part by slot; NULL once that part is destroyed
    GfxCoord* nearestCoord;     // Live part nearest the view when last measured; NULL when no part is live
    u32       nearestDistance;  // Distance of `nearestCoord` from the view; -1 when no part is live
    s32       sustainedSoundId; // Sound script kept playing while parts live; 0 before it starts and after a requested stop
    s16       partLive[3];      // By slot (0 not spawned or destroyed, 1 live)
    s16       site;             // Room the controller is placed in (ACTOR_205200_SITE_*)
    s16       partCount;        // Live parts; while they spawn, also the slot the next part takes
    s16       pulseTimer;       // Frames left in the current half of the pulse cycle
    s16       state;            // Stage of the controller (ACTOR_205200_CTRL_*)
    s16       pulseState;       // Half of the pulse cycle (ACTOR_205200_PULSE_*)
    s16       pendingWavePhase; // SCREEN_WAVE_RAMP_FALLING while a raised wave is still owed its fall, otherwise 0
    s16       startDelay;       // Frames left before the sustained sound starts
    s16       nearestStale;     // Set to 1 when a part is destroyed, so the nearest part is measured again
    s16       stopRequested;    // Set to 1 by a nonzero actor command; stops the sound and retires the parts
} _Actor205200CtrlWork;
STATIC_ASSERT_SIZEOF(_Actor205200CtrlWork, 0x30);

/// Task state of a part, stored in its `Task::state`.
enum {
    ACTOR_205200_PART_TASK_SPAWNING = 0, // Allocating the work block and taking a controller slot
    ACTOR_205200_PART_TASK_LIVE     = 1, // A target that takes hits
    ACTOR_205200_PART_TASK_DOWN     = 2, // Out of the fight; `_Actor205200Part.downState` says how
};

/// Stage of a part that is down, stored in `_Actor205200Part.downState`.
///
/// A part goes down either because its HP ran out, which enters at DESTROYED,
/// or because the controller was told to stop, which enters at RETIRING.
enum {
    ACTOR_205200_PART_DOWN_DESTROYED   = 0, // Leave the fight, tell the controller and the room, start smouldering
    ACTOR_205200_PART_DOWN_SMOULDERING = 1, // A wreck throwing sparks and smoke at random intervals; never left
    ACTOR_205200_PART_DOWN_RETIRING    = 2, // Leave the fight without effects
    ACTOR_205200_PART_DOWN_RETIRED     = 3, // Nothing left to run
};

enum {
    /// Frames after a hit's sparks before another hit may raise them.
    ACTOR_205200_PART_HIT_EFFECT_COOLDOWN = 10,
    /// Damage at and above which a hit leaves the part sparking for the longest time.
    ACTOR_205200_PART_SPARK_DAMAGE_CAP = 200,
};

/// Work block of a part task: one destructible target of a controller's site.
///
/// A live part is a sphere that weapons hit and an enemy the player can lock
/// on to. Each hit that deals damage leaves it sparking for a while, longer
/// the harder the hit. A destroyed part stays in place as a wreck. `slot` is fixed
/// at spawn and selects the part's position and heading within the site, its
/// entries in the controller's `partCoords` and `partLive`, and the game flag
/// and room sprites that record its destruction.
typedef struct {
    WorldCollisionBody    body;           // Sphere that receives weapon hits
    WorldCollisionContact contacts[3];    // Contact table of `body`, also the enemy's hit records
    EffectSpawnArg        effectArg;      // Record the part's sparks and blasts are spawned with, placed on its coordinate
    s16                   hitCooldown;    // Frames left in which hits are ignored; set from the hitting attack's cooldown
    s16                   downState;      // Stage of a part that is down (ACTOR_205200_PART_DOWN_*)
    s16                   sparkTimer;     // Live: frames of sparking left after a hit; smouldering: frames to the next spark burst
    s16                   effectCooldown; // Live: frames before a hit may raise sparks again; smouldering: frames to the next smoke puffs
    s16                   slot;           // Index of the part within its site, 0 to 2
} _Actor205200Part;
STATIC_ASSERT_SIZEOF(_Actor205200Part, 0x7C);

extern s32 gScreenWaveRamp;

extern u16      D_actor_205200_8014C9CC[];
extern s16      D_actor_205200_8014CA1C[];
extern TaskDesc D_actor_205200_8014CA60[];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_205200_8014CA78[2];
extern TaskDesc         D_actor_205200_8014CA44[];

extern EnemyParams D_actor_205200_8014C9BC;
extern SVECTOR*    D_actor_205200_8014CA24[];
extern u16*        D_actor_205200_8014CA34[];

static void _actor205200TickPulse(Task* task);
static void _actor205200MeasureNearestPart(Task* task);
static s32  _actor205200GetDistanceAttenuation(s32 distance);
static void _actor205200TickLivePart(Enemy* enemy, Task* task);
static void _actor205200TickPartSparks(Task* task);

void        func_actor_205200_8014B8C0(Task*);
static void _actor205200PartTask(Task* task);

static s32 _actor205200RequestControllerStopMsg(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArg);

/// Sound scripts for the destructible parts; bits 8..15 carry the placement instance.
enum {
    ACTOR_205200_SOUND_INSTANCE_SHIFT   = 8,
    ACTOR_205200_PART_SOUND_HIT         = 0x40340003,
    ACTOR_205200_PART_SOUND_DESTROYED   = 0x40340004,
    ACTOR_205200_PART_SOUND_WRECK_SPARK = 0x40340005,
};

/// The distance offset is saturated before conversion to signed sound attenuation units.
enum {
    ACTOR_205200_AUDIO_DISTANCE_LIMIT = 32767,
    ACTOR_205200_AUDIO_DISTANCE_SHIFT = 8, // One attenuation unit per 256 game-coordinate units
};

EnemyParams D_actor_205200_8014C9BC = { NULL, 200, 150, 0, 0, 100, 0, 0, 0 };

u16 D_actor_205200_8014C9CC[4] = {
    0,
    60,
    40,
    20,
};

SVECTOR D_actor_205200_8014C9D4[2] = {
    { -4550, -1100, 1030, 0 },
    { -1260, -1100, 7420, 0 },
};

u16 D_actor_205200_8014C9E4[2] = {
    1024,
    2048,
};

SVECTOR D_actor_205200_8014C9E8[3] = {
    { 3000, -1500, 1600, 0 },
    { 5000, -1500, -1600, 0 },
    { 7000, -1500, 1600, 0 },
};

u16 D_actor_205200_8014CA00[4] = {
    0,
    2048,
    2048,
    0,
};

SVECTOR D_actor_205200_8014CA08[2] = {
    { -150, -1200, 4000, 0 },
    { 5150, -1200, 4000, 0 },
};

u16 D_actor_205200_8014CA18[2] = {
    3072,
    1024,
};

s16 D_actor_205200_8014CA1C[4] = {
    0,
    2,
    3,
    2,
};

SVECTOR* D_actor_205200_8014CA24[4] = {
    NULL,
    D_actor_205200_8014C9D4,
    D_actor_205200_8014C9E8,
    D_actor_205200_8014CA08,
};

u16* D_actor_205200_8014CA34[4] = {
    NULL,
    D_actor_205200_8014C9E4,
    D_actor_205200_8014CA00,
    D_actor_205200_8014CA18,
};

TaskDesc D_actor_205200_8014CA44[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveGridTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskDesc D_actor_205200_8014CA60[2] = {
    { { { TASK_BODY_COORD, 96 } }, func_actor_205200_8014B8C0, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, _actor205200PartTask, { .value = 0 } },
};

TaskMessageEntry D_actor_205200_8014CA78[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor205200RequestControllerStopMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static TmdBone _gActor205200EveBreaMaskedBodySkeleton[19] = {
#include "assets/eve_brea_masked_body_skeleton.inc"
};

static u32 _gActor205200EveBreaMaskedBodyPartVerts[19] = {
#include "assets/eve_brea_masked_body_partVerts.inc"
};

static SVECTOR _gActor205200EveBreaMaskedBodyVerts[312] = {
#include "assets/eve_brea_masked_body_verts.inc"
};

static SVECTOR _gActor205200EveBreaMaskedBodyNormals[338] = {
#include "assets/eve_brea_masked_body_normals.inc"
};

static u32 _gActor205200EveBreaMaskedBodyStream[3463] = {
#include "assets/eve_brea_masked_body_stream.inc"
};

TmdSource gActor205200EveBreaMaskedBody = {
    0,
    18392,
    6232,
    19,
    _gActor205200EveBreaMaskedBodyPartVerts,
    _gActor205200EveBreaMaskedBodyVerts,
    _gActor205200EveBreaMaskedBodyNormals,
    _gActor205200EveBreaMaskedBodySkeleton,
    _gActor205200EveBreaMaskedBodyStream,
};

static void _actor205200SpawnController(Enemy* enemy, Task* task);
static void _actor205200TickController(Enemy* enemy, Task* task);
static void _actor205200SpawnPart(Enemy* enemy, Task* task);
static void _actor205200ScanPartHits(Task* task, s32 unusedArg);
static void _actor205200TickDownPart(Enemy* enemy, Task* task);

#include "../../shared/screen_wave_grid.inc.c"

/// Spawns a site's destructible parts and initializes their pulse controller.
///
/// Placement modes 1..3 select the tunnel, corridor or training room. Invalid
/// modes or work-allocation failure destroy the enemy and task. The task owns
/// its zeroed work; child coordinates remain borrowed until their parts fall.
/// Successful setup resets the site's destruction markers and enters task state 1.
static void _actor205200SpawnController(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_205200_VALID_SITE_COUNT        = 3,
        ACTOR_205200_CONTROLLER_TASK_RUNNING = 1,
        ACTOR_205200_PART_DESCRIPTOR_INDEX   = 1,
        ACTOR_205200_PART_INTACT             = 0,
    };
    _Actor205200CtrlWork* work;
    u16                   site;
    s32                   partIndex;
    u16                   initialPulseFrames;

    site = enemy->place->mode;
    if ((u16)(site - ACTOR_205200_SITE_EVE_ACCESS_TUNNEL) >= ACTOR_205200_VALID_SITE_COUNT) {
        enemyDestroy(enemy, task);
        return;
    }
    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work                    = work;
    work->site                    = site;
    D_actor_205200_8015B458.state = SCREEN_WAVE_RAMP_FINISHED;
    // Each child's setup takes the next slot while partCount grows from zero.
    for (partIndex = 0; partIndex < D_actor_205200_8014CA1C[work->site]; partIndex++) {
        enemySpawnFromTable(D_actor_205200_8014CA60, ACTOR_205200_PART_DESCRIPTOR_INDEX, 0, enemy);
    }
    initialPulseFrames = D_actor_205200_8014C9CC[D_actor_205200_8014CA1C[work->site]];
    work->startDelay   = ACTOR_205200_START_DELAY;
    work->pulseTimer   = initialPulseFrames;
    // Retain the zero-site arm: it controls the original switch decision tree.
    switch (work->site) {
        case ACTOR_205200_SITE_EVE_ACCESS_TUNNEL:
            neoArkEveAccessTunnelSetPartDestroyedSprites(0, NEO_ARK_EVE_ACCESS_TUNNEL_PART_INTACT);
            neoArkEveAccessTunnelSetPartDestroyedSprites(1, NEO_ARK_EVE_ACCESS_TUNNEL_PART_INTACT);
            gameFlagSetNibble(GAME_FLAG_EVE_ACCESS_TUNNEL_PART_0_DOWN, 0);
            gameFlagSetNibble(GAME_FLAG_EVE_ACCESS_TUNNEL_PART_1_DOWN, 0);
            break;
        case ACTOR_205200_SITE_B6_CORRIDOR:
            shelterB6CorridorSetPartDestroyedSprites(0, SHELTER_B6_CORRIDOR_PART_INTACT);
            shelterB6CorridorSetPartDestroyedSprites(1, SHELTER_B6_CORRIDOR_PART_INTACT);
            shelterB6CorridorSetPartDestroyedSprites(2, SHELTER_B6_CORRIDOR_PART_INTACT);
            gameFlagSetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_0_DOWN, 0);
            gameFlagSetNibble(GAME_FLAG_B6_CORRIDOR_EVE_PART_1_DOWN, 0);
            break;
        case ACTOR_205200_SITE_B6_TRAINING_ROOM:
            shelterB6TrainingRoomSetPartDestroyedSprites(0, ACTOR_205200_PART_INTACT);
            shelterB6TrainingRoomSetPartDestroyedSprites(1, ACTOR_205200_PART_INTACT);
            gameFlagSetNibble(GAME_FLAG_153, 0);
            gameFlagSetNibble(GAME_FLAG_154, 0);
            break;
        case 0:
            break;
    }
    task->msgTable = D_actor_205200_8014CA78;
    task->state    = ACTOR_205200_CONTROLLER_TASK_RUNNING;
}

/// Advances the destructible-part controller's sound and screen-wave cycle.
///
/// Requires live controller work, its matching Enemy and a coordinate-body task.
/// Running combat starts the placement-tagged sustained sound after the delay,
/// remeasures its nearest-part attenuation on view/part changes and ticks pulses.
/// Events suspend pulses and finish a pending wave fall; a stop request also
/// stops the sound. Losing every part enters stopping, including a 60-tick MIDI
/// fade in the corridor. Wave phase and controller state copies retain their
/// numeric identity when passed to the wave and compared with session/site flags.
static void _actor205200TickController(Enemy* enemy, Task* task)
{
    enum { ACTOR_205200_SUSTAINED_SOUND      = 0x40340001,
           ACTOR_205200_STOP_MIDI_FADE_TICKS = 60 };

    _Actor205200CtrlWork* work = task->work;
    s16                   controllerState;
    s32                   wavePhase;

    if (gGameSession->eventState != 0 || work->stopRequested != 0) {
        wavePhase = work->pendingWavePhase;
        if (wavePhase == SCREEN_WAVE_RAMP_FALLING) {
            D_actor_205200_8015B458.state = wavePhase;
            work->pendingWavePhase        = 0;
        }
        if (work->stopRequested != 0) {
            work->state = ACTOR_205200_CTRL_STOPPED;
            if (work->stopRequested != 0) {
                if (work->sustainedSoundId != 0) {
                    sndEvtRequestScriptStop(work->sustainedSoundId, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    work->sustainedSoundId = 0;
                }
            }
        }
    } else if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        controllerState = work->state;
        switch (controllerState) {
            case ACTOR_205200_CTRL_STARTING:
                if (--work->startDelay == 0) {
                    _actor205200MeasureNearestPart(task);
                    if (work->nearestCoord != NULL) {
                        work->sustainedSoundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_205200_SOUND_INSTANCE_SHIFT) | ACTOR_205200_SUSTAINED_SOUND;
                        sndEvtRequestScriptStart(
                            work->sustainedSoundId, 0, (s8)_actor205200GetDistanceAttenuation(work->nearestDistance));
                        work->state = ACTOR_205200_CTRL_PULSING;
                    }
                }
                break;
            case ACTOR_205200_CTRL_PULSING:
                // `controllerState` is 1 here: the view has just become ready, or a part
                // was destroyed, so the sound follows the nearest live part.
                if (gGameSession->viewReady == controllerState || work->nearestStale == controllerState) {
                    work->nearestStale = 0;
                    _actor205200MeasureNearestPart(task);
                    if (work->nearestCoord != NULL) {
                        sndEvtRequestScriptMix(
                            work->sustainedSoundId, 0, (s8)_actor205200GetDistanceAttenuation(work->nearestDistance));
                    }
                }
                _actor205200TickPulse(task);
                if (work->partCount <= 0) {
                    work->state = ACTOR_205200_CTRL_STOPPING;
                }
                break;
            case ACTOR_205200_CTRL_STOPPING:
                wavePhase = work->pendingWavePhase;
                if (wavePhase == SCREEN_WAVE_RAMP_FALLING) {
                    D_actor_205200_8015B458.state = wavePhase;
                    work->pendingWavePhase        = 0;
                }
                sndEvtRequestScriptStop(work->sustainedSoundId, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->state = ACTOR_205200_CTRL_STOPPED;
                // `controllerState` is 2 here, which is also the B6 corridor's site.
                if (work->site == controllerState) {
                    sndEvtRequestMidiStop(0, ACTOR_205200_STOP_MIDI_FADE_TICKS);
                }
                break;
        }
        task->extra.coordBody->coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Advances a controller's repeating screen-wave pulse and one-MP drain.
///
/// Requires initialized controller work, its Enemy in spawnArg2 and partCount
/// in 0..3; the caller runs one last pulse tick before noticing no parts remain.
/// A waiting timer raises a finished wave over 15 frames
/// to scale 160 and voices the placement instance. Twenty ticks later the wave
/// falls, one MP is spent and the part-count table schedules the next pulse.
/// A busy wave skips spawning/sound but retains the timer and MP-drain cycle.
static void _actor205200TickPulse(Task* task)
{
    enum {
        ACTOR_205200_PULSE_WAVE_FRAMES = 15,
        ACTOR_205200_PULSE_WAVE_SCALE  = 160,
        ACTOR_205200_PULSE_SOUND       = 0x40340002,
    };

    _Actor205200CtrlWork* work = task->work;
    Enemy*                enemy;
    s32                   pulseState = work->pulseState;

    switch (pulseState) {
        case ACTOR_205200_PULSE_WAITING:
            if (--work->pulseTimer <= 0) {
                if (D_actor_205200_8015B458.state == SCREEN_WAVE_RAMP_FINISHED) {
                    D_actor_205200_8015B458.span  = ACTOR_205200_PULSE_WAVE_FRAMES;
                    D_actor_205200_8015B458.scale = ACTOR_205200_PULSE_WAVE_SCALE;
                    taskSpawnFromTable(D_actor_205200_8014CA44, 0, 0, &D_actor_205200_8015B458);
                    sceneEngageBattle(1);
                    work->pendingWavePhase = SCREEN_WAVE_RAMP_FALLING;
                    enemy                  = task->spawnArg2.pointer;
                    sndEvtRequestScriptStart(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_205200_SOUND_INSTANCE_SHIFT) | ACTOR_205200_PULSE_SOUND, 0, 0);
                }
                work->pulseTimer = ACTOR_205200_PULSE_RAISED_FRAMES;
                work->pulseState = ACTOR_205200_PULSE_RAISED;
            }
            break;
        case ACTOR_205200_PULSE_RAISED:
            if (--work->pulseTimer <= 0) {
                // `pulseState` is 1 here, which is also SCREEN_WAVE_RAMP_FALLING.
                D_actor_205200_8015B458.state = pulseState;
                work->pulseTimer              = D_actor_205200_8014C9CC[work->partCount];
                work->pulseState              = ACTOR_205200_PULSE_WAITING;
                playerStateSpendMp(1);
                work->pendingWavePhase = 0;
            }
            break;
    }
}

/// Records the live part nearest the camera, for the controller's sustained sound.
///
/// Requires controller work and live coordinates in every occupied slot.
/// Rebuilds their transforms and measures 3D distance in game-coordinate units;
/// no live part leaves nearestCoord NULL and nearestDistance at unsigned -1.
static void _actor205200MeasureNearestPart(Task* task)
{
    enum { ACTOR_205200_NO_PART_DISTANCE = -1 }; // All bits set in the unsigned distance cache
    _Actor205200CtrlWork* work = task->work;
    const ViewCamera*     camera;
    VECTOR                cameraOffset;
    u32                   distance;
    s32                   slot;

    work->nearestCoord    = NULL;
    work->nearestDistance = ACTOR_205200_NO_PART_DISTANCE;
    camera                = viewGetMappedCamera(&gGameSession->location.loc);
    for (slot = 0; slot < ARRAY_SIZE(work->partLive); slot++) {
        if (work->partLive[slot] == true) {
            work->partCoords[slot]->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(work->partCoords[slot]);
            cameraOffset.vx = camera->transform.t[0] + work->partCoords[slot]->coord.t[0];
            cameraOffset.vy = camera->transform.t[1] + work->partCoords[slot]->coord.t[1];
            cameraOffset.vz = camera->transform.t[2] + work->partCoords[slot]->coord.t[2];
            distance        = SquareRoot0(cameraOffset.vx * cameraOffset.vx + cameraOffset.vy * cameraOffset.vy + cameraOffset.vz * cameraOffset.vz);
            if (distance < work->nearestDistance) {
                work->nearestCoord    = work->partCoords[slot];
                work->nearestDistance = distance;
            }
        }
    }
}

/// Initializes one destructible part at its site's authored position and heading.
///
/// Requires a live controller parent whose partCount is the next free slot,
/// below the site's two- or three-part count, and a coordinate body. The task
/// owns the work, while the controller borrows its coordinate. Setup acquires
/// one battle hold, links a hit sphere and target, and enters the live state;
/// allocation failure destroys the enemy and task before taking a slot.
static void _actor205200SpawnPart(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_205200_PART_BODY_KEY                   = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x34,
        ACTOR_205200_PART_RADIUS                     = 450,
        ACTOR_205200_PART_HIT_EFFECT_SCALE           = 0x400,
        ACTOR_205200_PART_HIT_EFFECT_DURATION_FACTOR = 3, // Twelve-frame spark bursts and eighteen-frame incendiary blasts
    };
    GfxCoord*             coord;
    _Actor205200CtrlWork* controllerWork;
    _Actor205200Part*     part;
    const SVECTOR*        positions;
    SVECTOR               rotation;
    MATRIX*               localMatrix;
    const u16*            headings;

    coord          = task->extra.coordBody->coord;
    controllerWork = task->parent->work;
    part           = memCalloc(sizeof(*part), false);
    if (part == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work = part;
    part->slot = controllerWork->partCount;
    controllerWork->partCount++;
    controllerWork->partCoords[part->slot] = coord;
    controllerWork->partLive[part->slot]   = true;
    headings                               = D_actor_205200_8014CA34[controllerWork->site];
    rotation.vx                            = 0;
    localMatrix                            = &coord->coord;
    rotation.vy                            = headings[part->slot];
    rotation.vz                            = 0;
    RotMatrix(&rotation, localMatrix);
    positions           = D_actor_205200_8014CA24[controllerWork->site];
    coord->coord.t[0]   = positions[part->slot].vx;
    coord->coord.t[1]   = positions[part->slot].vy;
    coord->coord.t[2]   = positions[part->slot].vz;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    enemy->field_4      = localMatrix;
    enemy->field_48     = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord      = coord;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->param      = &D_actor_205200_8014C9BC;
    enemy->recs       = part->contacts;
    enemy->hp         = D_actor_205200_8014C9BC.hpMax;
    sceneAcquireBattleRef(0);
    part->effectArg.spawnArgLo  = ACTOR_205200_PART_HIT_EFFECT_SCALE;
    part->effectArg.spawnArgHi  = ACTOR_205200_PART_HIT_EFFECT_DURATION_FACTOR;
    part->effectArg.coord       = coord;
    part->body.coord            = coord;
    part->body.context.contacts = part->contacts;
    part->body.pos.vx           = 0;
    part->body.pos.vy           = 0;
    part->body.pos.vz           = 0;
    part->body.key              = ACTOR_205200_PART_BODY_KEY;
    part->body.radius           = ACTOR_205200_PART_RADIUS;
    part->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &part->body);
    worldCollisionInitContacts(part->contacts, ARRAY_SIZE(part->contacts), 0);
    part->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    task->state       = ACTOR_205200_PART_TASK_LIVE;
}

/// Consumes a live part's weapon contacts, applying HP loss and hit or destruction effects.
///
/// Requires initialized part work, an enemy with parameters, a live controller
/// parent and player coordinates. Attachment attacks give a zero readout and
/// end the scan. Positive weapon damage starts 30..150 frames of sparking and
/// may arm cooldowns. Zero HP removes the controller's slot immediately.
/// Clears all contacts even during cooldown. Reserves one scratch VECTOR across calls.
/// unusedArg retains the caller's unused second argument, always 1.
static void _actor205200ScanPartHits(Task* task, s32 unusedArg)
{
    enum {
        ACTOR_205200_PART_ATTACHMENT_ATTACK     = 0x8000,
        ACTOR_205200_PART_SPARK_BASE_FRAMES     = 30,
        ACTOR_205200_PART_SPARK_EXTRA_FRAMES    = 120,
        ACTOR_205200_PART_EXPLOSION_DRIFT_1     = 0x01002600, // Scale 1536, two frames per cell, drift multiplier 1
        ACTOR_205200_PART_EXPLOSION_DRIFT_2     = 0x02002600, // Same scale and animation rate, drift multiplier 2
        ACTOR_205200_PART_DESTROY_RUMBLE_FRAMES = 10,
        ACTOR_205200_PART_DESTROY_RUMBLE_START  = 255,
        ACTOR_205200_PART_DESTROY_RUMBLE_END    = 128,
    };
    VECTOR*           playerOffset;
    _Actor205200Part* part;
    Enemy*            enemy;
    GfxCoord*         coord;
    s32               damage;
    s32               contactIndex;
    s32               soundId;
    s32               hitCooldown;
    s32               sparkDamage;

    // Marks destruction now; the down handler later unlinks and releases the battle hold.
    // Arguments are evaluated repeatedly: use side-effect-free pointers and a writable s32 soundResult.
    // Captures the local effect/rumble constants above; scoped to this function and undefined below.
#define ACTOR_205200_DESTROY_PART(enemyRecord, partTask, partWork, partCoord, soundResult)                                                                      \
    {                                                                                                                                                           \
        (partTask)->state                                                               = ACTOR_205200_PART_TASK_DOWN;                                          \
        (partWork)->downState                                                           = ACTOR_205200_PART_DOWN_DESTROYED;                                     \
        ((_Actor205200CtrlWork*)(partTask)->parent->work)->partLive[(partWork)->slot]   = false;                                                                \
        ((_Actor205200CtrlWork*)(partTask)->parent->work)->partCoords[(partWork)->slot] = NULL;                                                                 \
        effectSpawn(EFFECT_EXPLOSION, (partCoord), ACTOR_205200_PART_EXPLOSION_DRIFT_1, NULL);                                                                  \
        effectSpawn(EFFECT_EXPLOSION, (partCoord), ACTOR_205200_PART_EXPLOSION_DRIFT_1, NULL);                                                                  \
        effectSpawn(EFFECT_EXPLOSION, (partCoord), ACTOR_205200_PART_EXPLOSION_DRIFT_1, NULL);                                                                  \
        effectSpawn(EFFECT_EXPLOSION, (partCoord), ACTOR_205200_PART_EXPLOSION_DRIFT_2, NULL);                                                                  \
        effectSpawn(EFFECT_EXPLOSION, (partCoord), ACTOR_205200_PART_EXPLOSION_DRIFT_2, NULL);                                                                  \
        (soundResult) = (((enemyRecord)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_205200_SOUND_INSTANCE_SHIFT) | ACTOR_205200_PART_SOUND_DESTROYED;        \
        sndEvtRequestScriptStart((soundResult), (s8)worldCoordGetOriginAudioPan((partCoord)), (s8)worldCoordGetOriginAudioDepth((partCoord)));                  \
        padScriptSpawnVariableMotorRamp(ACTOR_205200_PART_DESTROY_RUMBLE_FRAMES, ACTOR_205200_PART_DESTROY_RUMBLE_START, ACTOR_205200_PART_DESTROY_RUMBLE_END); \
    }

    playerOffset = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    coord        = task->extra.coordBody->coord;
    part         = task->work;
    enemy        = task->spawnArg2.pointer;
    if (part->hitCooldown != 0) {
        part->hitCooldown--;
        if (part->hitCooldown <= 0) {
            part->hitCooldown = 0;
        }
    }
    if (part->effectCooldown != 0) {
        part->effectCooldown--;
    }
    if (part->hitCooldown == 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(part->contacts); contactIndex++) {
            if ((part->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
                continue;
            }
            if (part->contacts[contactIndex].key.value & ACTOR_205200_PART_ATTACHMENT_ATTACK) {
                worldTargetAddReadoutAmount(&enemy->node, 0, 0);
                break;
            }
            playerOffset->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            playerOffset->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            playerOffset->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage           = damageComputePlayerAttack(part->contacts[contactIndex].key.value, SquareRoot0(playerOffset->vx * playerOffset->vx + playerOffset->vy * playerOffset->vy + playerOffset->vz * playerOffset->vz), 0, 0);
            if (damageRollCriticalHit(enemy, part->contacts[contactIndex].key.value, 0) != 0) {
                damage *= 4;
                effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, NULL);
            }
            worldTargetAddReadoutAmount(&enemy->node, damage, 0);
            enemy->hp -= damage;
            if (enemy->hp <= 0) {
                ACTOR_205200_DESTROY_PART(enemy, task, part, coord, soundId);
            } else if (damage > 0) {
                if (part->effectCooldown == 0) {
                    if ((damageGetPlayerAttackReaction(part->contacts[contactIndex].key.value) & 0xFFFF) == DAMAGE_PLAYER_REACTION_INCENDIARY) {
                        effectSpawnHit(EFFECT_HIT_KIND_BLAST, coord, NULL, &part->effectArg);
                    }
                    effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, coord, NULL, &part->effectArg);
                    part->effectCooldown = ACTOR_205200_PART_HIT_EFFECT_COOLDOWN;
                }
                if (damage <= ACTOR_205200_PART_SPARK_DAMAGE_CAP) {
                    sparkDamage = damage;
                } else {
                    sparkDamage = ACTOR_205200_PART_SPARK_DAMAGE_CAP;
                }
                part->sparkTimer = (sparkDamage * ACTOR_205200_PART_SPARK_EXTRA_FRAMES) / ACTOR_205200_PART_SPARK_DAMAGE_CAP + ACTOR_205200_PART_SPARK_BASE_FRAMES;
                hitCooldown      = damageGetPlayerAttackHitCooldown(part->contacts[contactIndex].key.value);
                if (hitCooldown > 0) {
                    part->hitCooldown = hitCooldown;
                }
                soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_205200_SOUND_INSTANCE_SHIFT) | ACTOR_205200_PART_SOUND_HIT;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
        }
    }
    worldCollisionClearContacts(part->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
#undef ACTOR_205200_DESTROY_PART
}

/// Returns a saturated camera-distance offset for positional sound attenuation.
///
/// distance and the current screen projection distance use game-coordinate
/// units. Subtracts screenDistance, then clamps the signed result to -32767..32767.
/// Does not divide by 256; callers apply the attenuation shift. The subtraction
/// requires a representable signed result.
static inline s32 _actor205200ClampAudioDistance(s32 distance)
{
    s32 distanceOffset = distance - gDisplayState.screenDistance;

    if (distanceOffset >= ACTOR_205200_AUDIO_DISTANCE_LIMIT) {
        distanceOffset = ACTOR_205200_AUDIO_DISTANCE_LIMIT;
    }
    if (distanceOffset < -ACTOR_205200_AUDIO_DISTANCE_LIMIT) {
        distanceOffset = -ACTOR_205200_AUDIO_DISTANCE_LIMIT;
    }
    return distanceOffset;
}

/// Removes a downed part from combat and advances its persistent wreck effects.
///
/// Requires live part and controller work. Only running combat updates this
/// state. Destruction releases the battle hold with rewards, records the room
/// marker and keeps the coordinate for repeated sparks and smoke; retirement
/// releases without rewards or effects. Neither path frees work or the task.
static void _actor205200TickDownPart(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_205200_PART_DESTROYED               = 1,
        ACTOR_205200_PART_BATTLE_RELEASE_ARGUMENT = 0x34,
        ACTOR_205200_PART_SMOKE_ADDITIVE          = 0x32001400, // Scale 1024, one frame per cell, rising mode 2 and child puffs
        ACTOR_205200_PART_SMOKE_SUBTRACTIVE       = 0xF2001400, // Same motion and animation with subtractive blending
        ACTOR_205200_PART_WRECK_DELAY_BASE        = 30,
        ACTOR_205200_PART_WRECK_SPARK_DELAY_MASK  = 63,
        ACTOR_205200_PART_WRECK_SMOKE_DELAY_MASK  = 31,
    };
    _Actor205200Part*     part;
    GfxCoord*             coord;
    _Actor205200CtrlWork* controllerWork;
    const ViewCamera*     camera;
    VECTOR                cameraOffset;
    s32                   distance;
    s32                   soundId;
    s32                   panOffset;
    s32                   distanceOffset;

    part           = task->work;
    coord          = task->extra.coordBody->coord;
    controllerWork = task->parent->work;
    if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }
    switch (part->downState) {
        case ACTOR_205200_PART_DOWN_DESTROYED:
            // Commit destruction once; the task stays alive as a wreck.
            effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_205200_PART_SMOKE_ADDITIVE, NULL);
            effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_205200_PART_SMOKE_ADDITIVE, NULL);
            effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_205200_PART_SMOKE_SUBTRACTIVE, NULL);
            effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_205200_PART_SMOKE_SUBTRACTIVE, NULL);
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&part->body);
            sceneReleaseBattleRefWithRewards(task, ACTOR_205200_PART_BATTLE_RELEASE_ARGUMENT);
            enemy->recs                  = NULL;
            controllerWork->nearestStale = 1;
            controllerWork->partCount--;
            gSceneCombatState.pairedEnemySignals |= SCENE_COMBAT_PAIRED_CHARGE_REQUEST;
            switch (controllerWork->site) {
                case ACTOR_205200_SITE_EVE_ACCESS_TUNNEL:
                    neoArkEveAccessTunnelSetPartDestroyedSprites(part->slot, NEO_ARK_EVE_ACCESS_TUNNEL_PART_DESTROYED);
                    gameFlagSetNibble(part->slot + GAME_FLAG_EVE_ACCESS_TUNNEL_PART_0_DOWN, 1);
                    break;
                case ACTOR_205200_SITE_B6_CORRIDOR:
                    shelterB6CorridorSetPartDestroyedSprites(part->slot, SHELTER_B6_CORRIDOR_PART_DESTROYED);
                    gameFlagSetNibble(part->slot + GAME_FLAG_B6_CORRIDOR_EVE_PART_0_DOWN, 1);
                    break;
                case ACTOR_205200_SITE_B6_TRAINING_ROOM:
                    shelterB6TrainingRoomSetPartDestroyedSprites(part->slot, ACTOR_205200_PART_DESTROYED);
                    gameFlagSetNibble(part->slot + GAME_FLAG_153, 1);
                    break;
                case 0:
                    break;
            }
            part->downState      = ACTOR_205200_PART_DOWN_SMOULDERING;
            part->sparkTimer     = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_205200_PART_WRECK_SPARK_DELAY_MASK) + ACTOR_205200_PART_WRECK_DELAY_BASE;
            part->effectCooldown = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_205200_PART_WRECK_SMOKE_DELAY_MASK) + ACTOR_205200_PART_WRECK_DELAY_BASE;
            break;
        case ACTOR_205200_PART_DOWN_SMOULDERING:
            if (--part->sparkTimer <= 0) {
                gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                part->sparkTimer = ((gRandomLcgState >> 16) & ACTOR_205200_PART_WRECK_SPARK_DELAY_MASK) + ACTOR_205200_PART_WRECK_DELAY_BASE;
                effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, coord, NULL, &part->effectArg);
                effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_205200_PART_SMOKE_SUBTRACTIVE, NULL);
                camera          = viewGetMappedCamera(&gGameSession->location.loc);
                cameraOffset.vx = camera->transform.t[0] + coord->coord.t[0];
                cameraOffset.vy = camera->transform.t[1] + coord->coord.t[1];
                cameraOffset.vz = camera->transform.t[2] + coord->coord.t[2];
                distance        = SquareRoot0(cameraOffset.vx * cameraOffset.vx + cameraOffset.vy * cameraOffset.vy + cameraOffset.vz * cameraOffset.vz);
                soundId         = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_205200_SOUND_INSTANCE_SHIFT) | ACTOR_205200_PART_SOUND_WRECK_SPARK;
                panOffset       = (s8)worldCoordGetOriginAudioPan(coord);
                distanceOffset  = _actor205200ClampAudioDistance(distance);
                sndEvtRequestScriptStart(soundId, panOffset, (s16)distanceOffset >> ACTOR_205200_AUDIO_DISTANCE_SHIFT);
            }
            if (--part->effectCooldown <= 0) {
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                part->effectCooldown = ((gRandomLcgState >> 16) & ACTOR_205200_PART_WRECK_SMOKE_DELAY_MASK) + ACTOR_205200_PART_WRECK_DELAY_BASE;
                effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_205200_PART_SMOKE_SUBTRACTIVE, NULL);
                effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_205200_PART_SMOKE_SUBTRACTIVE, NULL);
            }
            break;
        case ACTOR_205200_PART_DOWN_RETIRING:
            sceneReleaseBattleRef(task, ACTOR_205200_PART_BATTLE_RELEASE_ARGUMENT);
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&part->body);
            part->downState = ACTOR_205200_PART_DOWN_RETIRED;
            break;
    }
}

/// Update of the actor's controller task: dispatches on its state to the
/// setup handler `_actor205200SpawnController` (state 0) or the per-frame
/// handler `_actor205200TickController` (state 1), passing the task's enemy
/// record along with the task.
void func_actor_205200_8014B8C0(Task* task)
{
    EnemyTaskFunc fns[2] = {
        _actor205200SpawnController,
        _actor205200TickController,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Converts camera distance to a signed sound attenuation offset in -128..127.
///
/// distance is in game-coordinate units. Subtracts the projection distance,
/// saturates at +/-32767, then divides by 256, rounding negative values down.
static s32 _actor205200GetDistanceAttenuation(s32 distance)
{
    return _actor205200ClampAudioDistance(distance) >> ACTOR_205200_AUDIO_DISTANCE_SHIFT;
}

/// Latches a controller stop request from a nonzero actor command, returning zero.
///
/// Handles ACTOR_COMMAND_MESSAGE_APPLY after controller setup. request is a
/// borrowed readable command; only its command halfword is used. A zero command
/// leaves the latch unchanged. messageId and unusedArg are ignored.
static s32 _actor205200RequestControllerStopMsg(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArg)
{
    _Actor205200CtrlWork* work;

    work = task->work;
    if (request->command != 0 && work->stopRequested == 0) {
        work->stopRequested = true;
    }
    return 0;
}

/// State handlers of a part task - spawn, per-frame tick and teardown - that
/// `_actor205200PartTask` dispatches through by state.
static const EnemyTaskFuncTable3 D_actor_205200_80149E24 = {
    _actor205200SpawnPart,
    _actor205200TickLivePart,
    _actor205200TickDownPart,
};

/// Dispatches a destructible part's spawn, live or down handler.
///
/// task->state must be ACTOR_205200_PART_TASK_SPAWNING, LIVE or DOWN (0..2);
/// dispatch is unchecked. spawnArg2.pointer is the live owning Enemy, and after
/// spawn the parent controller and its work must outlive this part.
static void _actor205200PartTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = D_actor_205200_80149E24;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Updates a live part's hits and sparks, and retires it when its controller stops.
///
/// Requires initialized part and controller work. Paused combat returns;
/// hidden combat makes the target un-lockable and returns. Running combat hides
/// its HP then updates it; other control values also update. A controller stop
/// overrides a hit's destruction state with retirement in the same frame.
static void _actor205200TickLivePart(Enemy* enemy, Task* task)
{
    _Actor205200Part*     part;
    _Actor205200CtrlWork* controllerWork;
    s32                   actorControl;
    s32                   notLockableFlag;

    part            = task->work;
    controllerWork  = task->parent->work;
    actorControl    = gSceneCombatState.actorControl;
    notLockableFlag = WORLD_TARGET_NOT_LOCKABLE;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            enemy->node.state.parts.flags = notLockableFlag;
            return;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
    }
    _actor205200ScanPartHits(task, notLockableFlag);
    if (part->sparkTimer != 0) {
        _actor205200TickPartSparks(task);
    }
    if (controllerWork->stopRequested == true) {
        task->state     = ACTOR_205200_PART_TASK_DOWN;
        part->downState = ACTOR_205200_PART_DOWN_RETIRING;
    }
}

/// Advances a live part's remaining spark duration and emits a burst every 64 frames.
///
/// Requires initialized part work with a positive sparkTimer. Decrements before
/// testing and also emits at zero; the caller stops ticking when the timer ends.
static void _actor205200TickPartSparks(Task* task)
{
    enum { ACTOR_205200_PART_SPARK_INTERVAL_MASK = 64 - 1 };
    _Actor205200Part* part = task->work;

    if (!(--part->sparkTimer & ACTOR_205200_PART_SPARK_INTERVAL_MASK)) {
        effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, task->extra.coordBody->coord, NULL, &part->effectArg);
    }
}
