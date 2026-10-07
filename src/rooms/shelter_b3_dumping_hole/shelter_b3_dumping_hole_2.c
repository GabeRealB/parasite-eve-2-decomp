#include "rooms/shelter_b3_dumping_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/strings.h>

#include "common.h"
#include "gte.h"

#include "shelter_b3_dumping_hole_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

/// Selects this room's billboard declaration with `s32` frame and size arguments.
///
/// Presence-only configuration for the first inclusion of `effect_sprite.h`;
/// the replacement value is unused. Leave the halfword argument flag undefined
/// and undefine this flag after the header. The drawer narrows frame to `u16`
/// and size to `s16` internally; the call signature remains word-sized.
#define EFFECT_SPRITE_BILLBOARD_WORD_ARGUMENTS
/// Binds the aimed drift fragment to this room's exported `void (Task*)` callback.
///
/// Supply a function identifier before `effect_sprite.h` and retain it through
/// the aimed drift fragment, which clears it. The two Shelter B3 carriers each
/// supply their own export; no arguments, captured locals or tokens are built.
#define EFFECT_SPRITE_DRIFT_AIMED_TASK shelterB3DumpingHoleEffectSpriteDriftTaskAimed
#include "../../shared/effect_sprite.h"
#undef EFFECT_SPRITE_BILLBOARD_WORD_ARGUMENTS

#include "../../shared/actor_messages.h"

static void _effectSpriteDrawBanked(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _effectSpriteDrawRotated(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);

#define DUMPING_HOLE_RAND() ((s32)((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16))

/// Advances a sprite frame and returns from its task after killing the closing frame.
///
/// Arguments must be side-effect-free task/work pointers and a twelve-entry
/// duration table. Expands to a compound statement in braced case arms.
/// Work fields are read and written repeatedly; the task is evaluated only
/// on completion. Uses this room's frame table `D_shelter_b3_dumping_hole_801880B8`. The frame starts in 0-11 and advances at most
/// once, advancing only after the timer exceeds its duration and testing the
/// sentinel before a later tick could read its duration.
#define SHELTER_B3_DUMPING_HOLE_ADVANCE_SPRITE_FRAME(task, work, frameDurations)                                       \
    {                                                                                                                  \
        (work)->frameTimer++;                                                                                          \
        if ((frameDurations)[(work)->frame] < (work)->frameTimer) {                                                    \
            (work)->frame++;                                                                                           \
            (work)->frameTimer = 0;                                                                                    \
            if (D_shelter_b3_dumping_hole_801880B8[(work)->frame].vramX == SHELTER_B3_DUMPING_HOLE_SPRITE_FRAME_END) { \
                taskKill(task);                                                                                        \
                return;                                                                                                \
            }                                                                                                          \
        }                                                                                                              \
    }

/// Kills and returns from a rubble task when its projected origin is outside the screen or at negative depth.
///
/// Arguments must be side-effect-free task, signed pixel coordinates and signed
/// SZ3/4 depth expressions. Tests the origin against inclusive centre-relative
/// bounds, retaining five separate exits. Expands to a compound statement;
/// completion returns from the calling void callback after task teardown.
#define SHELTER_B3_DUMPING_HOLE_CULL_DEBRIS_MODEL(task, screenX, screenY, depth) \
    {                                                                            \
        if ((screenX) < -SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_WIDTH) {            \
            taskKill((task));                                                    \
            return;                                                              \
        }                                                                        \
        if ((screenX) > SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_WIDTH) {             \
            taskKill((task));                                                    \
            return;                                                              \
        }                                                                        \
        if ((screenY) < -SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_HEIGHT) {           \
            taskKill((task));                                                    \
            return;                                                              \
        }                                                                        \
        if ((screenY) > SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_HEIGHT) {            \
            taskKill((task));                                                    \
            return;                                                              \
        }                                                                        \
        if ((depth) < 0) {                                                       \
            taskKill((task));                                                    \
            return;                                                              \
        }                                                                        \
    }

/// Centre-relative screen bounds, sprite texture units and depth sorting of this room's event sprites.
enum {
    SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_WIDTH  = 160,
    SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_HEIGHT = 120,
    SHELTER_B3_DUMPING_HOLE_SPRITE_CLUT        = getClut(0, 271),
    SHELTER_B3_DUMPING_HOLE_TEXTURE_4BIT       = 0,
    SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_WORDS = 64,
    SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_LINES = 256,
    SHELTER_B3_DUMPING_HOLE_OT_DEPTH_SHIFT     = 4,
    SHELTER_B3_DUMPING_HOLE_SPRITE_JITTER_MASK = 7,
};

/// Allocation and clear size of a `_ShelterB3DumpingHoleSpriteWork`, which extends
/// 2 bytes past its last accessed field.
enum { SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES = 0x24 };

/// Spawns one debris task and gives it a work block seeded with `seed`.
#define DUMPING_HOLE_SPAWN_DEBRIS(seed)                                                                       \
    {                                                                                                         \
        Task*                            t = taskSpawnFromTable(D_shelter_b3_dumping_hole_80188C04, 0, 0, 0); \
        _ShelterB3DumpingHoleSpriteWork* w = memMalloc(SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES, false);     \
        t->work                            = w;                                                               \
        if (w == NULL) {                                                                                      \
            taskKill(t);                                                                                      \
        } else {                                                                                              \
            memFillBytes(w, 0, SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES);                                    \
            w->seed = seed;                                                                                   \
        }                                                                                                     \
    }

extern SVECTOR D_shelter_b3_dumping_hole_8018B86C[44];

static void func_shelter_b3_dumping_hole_801830F0(s16 arg0, s16 arg1, s16 arg2);
#include "../../shared/cap_captions.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u16 D_shelter_b3_dumping_hole_8018F4D4[2];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u16 D_shelter_b3_dumping_hole_8018F4D4_value __asm__("D_shelter_b3_dumping_hole_8018F4D4");

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
static u8 CapCaption_Data_8015E66C[4];
// Scalar symbol view preserves the original byte/halfword address formation.

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u16 D_shelter_b3_dumping_hole_8018F4B0[2];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u16 D_shelter_b3_dumping_hole_8018F4B0_value __asm__("D_shelter_b3_dumping_hole_8018F4B0");

/// The clips the dumping hole's one-time arrival scene adds to the player's animation bank.
///
/// The scene's event script opens by sending `data.copy` to the player. The
/// copy takes five words from the start of this storage: the four set pointers
/// and the request's own source pointer. Those words occupy extended ids 47-51.
/// The script then plays ids 48, 49 and 50 in turn. Id 47 stays NULL, id 51
/// holds the source pointer, and the stored word count sits past the copied span.
typedef union {
    struct {
        AnimationSet*            sets[4]; // Player clips for extended ids 47-50; NULL at the id nothing plays
        AnimationBankCopyRequest copy;    // Copies the first five words of this storage
    } data;                               // The records by name
    s32 words[6];                         // The same storage as the copy reads it; the last word lies beyond the copied span
} _ShelterB3DumpingHoleAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_ShelterB3DumpingHoleAnimationBankExtensionStorage, 24);

extern _ShelterB3DumpingHoleAnimationBankExtensionStorage D_shelter_b3_dumping_hole_8018AFC8;

/// Work block of the debris event's director task, which starts the event
/// script once the player passes a set X position and stays reachable to the script's
/// callbacks and the effect tasks through the director task global.
///
/// The script posts commands for the player's animations and for the scene
/// (placements, actor commands, debris); the director handles each on its next
/// frame and clears it. The remaining fields tell the effect tasks it spawned
/// when to start and when to remove themselves.
typedef struct {
    ActorTransform pose;               // Placement-0 actor's initial position and yaw, sent back to it with `ACTOR_MESSAGE_PLACE`
    u8             pad_18[0xC];        // Never accessed; role unproven
    Task*          player;             // Player task
    Task*          placement0Actor;    // Task of the area's placement-0 actor (key: area, stage, index 0)
    Task*          placement1Actor;    // Task of the area's placement-1 actor (key: area, stage, index 1)
    u16            playerCommand;      // Pending player-animation command (0 none, 1-7), cleared once handled
    u16            playerStep;         // Progress through a multi-frame player command
    u16            playerTimer;        // Frames waited within the current player step
    u8             pad_36[0x2];
    u16            sceneCommand;       // Pending scene command (0 none, 1-7), cleared once handled
    u16            sceneStep;          // Progress through a multi-frame scene command
    u16            sceneTimer;         // Frames waited within the current scene step
    u8             pad_3E[0x2];
    u16            debrisModelSignal;  // Debris-model tasks' signal (`SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_*`)
    u16            debrisSpriteSignal; // Debris sprite rings' signal (`SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_*`)
    u16            actorSpritesStop;   // 1 removes the sprites spawned at an actor's coordinate
    u16            field_46;           // Cleared at start and set to 1 on skip like the stop flags, but never read; role unproven
    u16            playerSpritesStop;  // 1 removes the sprites spawned at the player's model coordinate
    u16            field_4A;           // Only ever cleared; role unproven
    u16            fadeStop;           // 1 removes the screen fade task
} _ShelterB3DumpingHoleDebrisEventWork;

/// `_ShelterB3DumpingHoleDebrisEventWork::debrisModelSignal` values.
enum {
    SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_WAIT   = 0, // Placed and waiting
    SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_LAUNCH = 1, // Thrown away from the screen centre
    SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_REMOVE = 2,
};

/// `_ShelterB3DumpingHoleDebrisEventWork::debrisSpriteSignal` values.
enum {
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_WAIT   = 0, // Placed and waiting
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_REMOVE = 1,
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_RISE   = 2, // Start rising and animating
};

/// One frame of the rising billboard-sprite animation the debris event spawns:
/// where the frame's 4-bit texture cell lies in VRAM and within its texture page.
///
/// A frame table is indexed by frame number and ends at an entry whose `vramX`
/// is `SHELTER_B3_DUMPING_HOLE_SPRITE_FRAME_END`.
typedef struct {
    u16 vramX; // VRAM x of the cell in 16-bit words; only its 64-word page base selects the texture page
    s16 u;     // Texture u of the cell's left edge within the page
    s16 vramY; // VRAM y of the cell; only its 256-line page base selects the texture page
    s16 v;     // Texture v of the cell's top edge within the page
    s16 w;     // Cell width in texels, also the on-screen width at unit scale
    s16 h;     // Cell height in texels, also the on-screen height at unit scale
} _ShelterB3DumpingHoleSpriteFrame;

/// `_ShelterB3DumpingHoleSpriteFrame::vramX` of the entry closing a frame table.
enum { SHELTER_B3_DUMPING_HOLE_SPRITE_FRAME_END = 0xFFFF };

/// Work block of a debris-model task: one piece of rubble the debris event
/// places, which waits there until the event signals the launch and is then
/// thrown along +X, spreading outwards from the screen centre, while it tumbles
/// and falls ever faster.
///
/// The two matrices are the storage the piece's model borrows for its
/// lighting. The motion fields are chosen once, at launch, and are zero until
/// then.
typedef struct {
    MATRIX  lightMtx; // Storage for the model's light matrix
    MATRIX  colorMtx; // Storage for the model's colour matrix
    SVECTOR rot;      // Tumble angles, rebuilt into the piece's rotation each frame as Y then X; `vz` is advanced but never applied
    SVECTOR vel;      // Per-frame translation in world axes, before `fall` is added to its `vy`
    SVECTOR spin;     // Per-frame step of `rot`, up to 127 either way per axis
    u16     fall;     // Extra downward speed, growing by `SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_GRAVITY` each frame
} _ShelterB3DumpingHoleDebrisModelWork;
STATIC_ASSERT_SIZEOF(_ShelterB3DumpingHoleDebrisModelWork, 0x5C);

/// Added to `_ShelterB3DumpingHoleDebrisModelWork::fall` every frame a launched
/// piece is in flight.
enum { SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_GRAVITY = 5 };

/// End marker in the X coordinate of the room's model placement tables.
enum { SHELTER_B3_DUMPING_HOLE_TRANSFORM_END = 0xFFFF };

/// Where the debris event places one piece of rubble, and whether a ring of
/// debris sprites is spawned around it.
///
/// The piece's task borrows `transform` as its spawn placement and reads it
/// once, when it initialises. A table of these ends at an entry whose
/// `transform.pos.vx` is `SHELTER_B3_DUMPING_HOLE_TRANSFORM_END`.
typedef struct {
    ActorTransform transform;  // Position and Euler angles the piece's model starts at
    u16            spriteRing; // Nonzero spawns five debris sprites: one at the piece and one at each YZ diagonal, `SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET` away per axis
} _ShelterB3DumpingHoleDebrisPlacement;
STATIC_ASSERT_SIZEOF(_ShelterB3DumpingHoleDebrisPlacement, 0x1C);

/// Distance along Y and along Z between a ringed piece of rubble and each of
/// the four debris sprites spawned around it, in world-coordinate units.
enum { SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET = 200 };

/// Where a billboard sprite of this room stands and how large it is drawn: the
/// head of a `_ShelterB3DumpingHoleSpriteWork`.
///
/// The debris event builds one on its stack for each debris sprite and copies
/// it whole into the sprite's freshly cleared work block.
typedef struct {
    SVECTOR pos;     // World position the sprite's coordinate starts at; `pad` is never set
    s16     scale;   // Size of the drawn quad relative to its frame's texel size (`ONE` = 1.0)
    s16     field_A; // Set to 1 by the debris event and never read; role unproven
} _ShelterB3DumpingHoleSpriteSeed;

/// Work block of a rising, animated billboard sprite, spawned in rings around
/// debris by the debris event, at its model by `actor_341700`, and at the
/// player's model by an event-script callback.
///
/// After `delay` runs out, the sprite steps through the frame table
/// `D_shelter_b3_dumping_hole_801880B8`, holding each frame for its entry in
/// the handler's duration table and drifting by `vel` each tick, and removes
/// itself at the closing frame. The allocation is
/// `SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES`; the bytes past `delay` are
/// never accessed and their role is unproven.
typedef struct {
    _ShelterB3DumpingHoleSpriteSeed seed;       // Debris sprites' world position, and the draw scale (unused by actor sprites); copied in whole by the debris event
    SVECTOR                         offset;     // Actor sprites: offset from the parent coordinate's world position
    SVECTOR                         vel;        // Per-tick drift; `vx` is cleared but never applied, and only player sprites drift in z
    s16                             frame;      // Index into the frame table and the duration table
    s16                             frameTimer; // Ticks the current frame has been shown
    u16                             delay;      // Ticks left before the animation starts
} _ShelterB3DumpingHoleSpriteWork;

/// Spawn record of the collapse event's falling shards, passed to each shard
/// task as its second spawn argument.
///
/// The record is lent, not copied: it stays in the director's work block, and a
/// shard reads it once, on its first frame, so it must outlive that frame.
/// Every shard of a burst shares one record and differs only by what it
/// randomises itself.
typedef struct {
    SVECTOR   offset;  // Added in world axes to `emitter`'s world position to give the shard's start; `pad` is never set
    SVECTOR   vel;     // Base velocity per frame, to which each shard adds up to 31 either way per axis; `pad` is never set
    GfxCoord* emitter; // Coordinate whose world placement the shard starts from; sampled once, the shard is not attached to it
    u16       radius;  // Circumradius of the shard's equilateral triangle, before each corner's random tenth is added
    u16       gravity; // Added to the shard's downward velocity every frame
} _ShelterB3DumpingHoleShardSpawn;

/// Work block of a shard task: one flat triangular splinter shed by the collapse
/// event's model, which tumbles and falls ever faster until its origin leaves
/// the screen or the event stops the shards.
///
/// The shard fills the block once, on its first frame, from the
/// `_ShelterB3DumpingHoleShardSpawn` record it was spawned with and its own
/// random draws; afterwards only `rot` and `vel.vy` change. The allocation is
/// `SHELTER_B3_DUMPING_HOLE_SHARD_WORK_BYTES`; the bytes past `gravity` are
/// never accessed and their role is unproven.
typedef struct {
    SVECTOR rot;      // Tumble angles (4096 = one turn), rebuilt into the shard's rotation each frame as Y, then X, then Z
    SVECTOR spin;     // Per-frame step of `rot`, 100 to 227 either way per axis
    SVECTOR vel;      // Per-frame translation in world axes: the spawn record's velocity plus up to 31 either way per axis, with `gravity` added to its `vy` each frame
    SVECTOR verts[3]; // The triangle's corners in the shard's own XY plane: equilateral about the origin at the spawn record's radius, each nonzero coordinate randomly pushed a tenth of the radius further out
    u16     gravity;  // Downward acceleration, added to `vel.vy` every frame; copied from the spawn record
} _ShelterB3DumpingHoleShardWork;

/// Allocation and clear size of a `_ShelterB3DumpingHoleShardWork`, which
/// extends 2 bytes past its last accessed field.
enum { SHELTER_B3_DUMPING_HOLE_SHARD_WORK_BYTES = 0x34 };

/// Work block of the collapse event's director task, which carries the event's
/// own model - three cylindrical sections chained below its root - and stays
/// reachable to the event script's callbacks through the director task global.
///
/// The script posts one command at a time; the director handles it on its next
/// frame and clears it, a multi-frame command only after its last step. The
/// collapse itself shakes the model while part 2 sheds shards, then lets part 2
/// drop away and part 1 tip over.
typedef struct {
    MATRIX                          lightMtx;         // Storage for the model's light matrix
    MATRIX                          colorMtx;         // Storage for the model's colour matrix
    ActorTransform                  pose;             // The model's placement, sent to the director itself with `ACTOR_MESSAGE_PLACE`
    _ShelterB3DumpingHoleShardSpawn shardSpawn;       // Spawn record lent to every shard task, which reads it on its first frame: shards start at part 2's coordinate
    VECTOR                          part3Scale;       // Per-axis scale of model part 3 (`ONE` = 1.0), shrunk in X and Z while part 2 sinks
    Task*                           player;           // Player task
    Task*                           placement0Actor;  // Task of the area's placement-0 actor (key: area, stage, index 0)
    Task*                           framebufferBlend; // Framebuffer-blend effect task while one runs, else NULL
    u16                             command;          // Pending command (`SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_*`), cleared once handled
    u16                             step;             // Progress through a multi-frame command
    u16                             timer;            // Frames spent in the current step
    u8                              pad_92[0x2];
    s16                             savedView;        // The session's view slot when the event started, written back to the saved location on finish
    s16                             field_96;         // Set to 1 where a command or the skip ends the collapse, but never read; role unproven
    s16                             part1Pitch;       // Angle about X: follows the model's pitch while it shakes, then restarts at 0 as part 1's tipping angle
    s16                             field_9A;         // Set to 0x80 as part 2 starts to drop, but never read; role unproven
    u16                             battleReleased;   // 1 once the event has dropped its battle reference, so the release runs once
    u8                              pad_9E[0x2];
} _ShelterB3DumpingHoleCollapseEventWork;
STATIC_ASSERT_SIZEOF(_ShelterB3DumpingHoleCollapseEventWork, 0xA0);

/// `_ShelterB3DumpingHoleCollapseEventWork::command` values, posted by the
/// event script and its skip script.
enum {
    SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_NONE         = 0,
    SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_PREPARE      = 1, // Cancel the room effects, lock attachments and pose the player
    SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_COLLAPSE     = 2, // Multi-frame: show the model in place of the player and actor and run the collapse
    SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_END_COLLAPSE = 3, // Hide the model, show the player and actor again and stop the shards
    SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_HIDE_ACTOR   = 4, // Hide the placement-0 actor
    SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_FINISH       = 5, // Restore the view, show the player and actor and end the framebuffer blend
    SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_FLICKER      = 6, // Multi-frame: flicker the view's sprites, then start the framebuffer blend
};

/// The encounter's enemy slots, in the order the controller starts them.
extern OverlayEncounterSlot D_shelter_b3_dumping_hole_8018B7BC[];

extern TaskDesc D_shelter_b3_dumping_hole_80188C04[];
extern TaskDesc D_shelter_b3_dumping_hole_80188BC8[];

extern Task*                                D_shelter_b3_dumping_hole_8018F4A8;
extern s16                                  D_shelter_b3_dumping_hole_80188154[];
extern _ShelterB3DumpingHoleSpriteFrame     D_shelter_b3_dumping_hole_801880B8[];
extern s16                                  D_shelter_b3_dumping_hole_8018816C[];
extern s16                                  D_shelter_b3_dumping_hole_80188184[];
extern s32                                  D_shelter_b3_dumping_hole_8018819C[];
extern ActorTransform                       D_shelter_b3_dumping_hole_801881CC;
extern ActorTransform                       D_shelter_b3_dumping_hole_801881E4;
extern ActorTransform                       D_shelter_b3_dumping_hole_801881FC[];
extern ActorTransform                       D_shelter_b3_dumping_hole_80188304[];
extern _ShelterB3DumpingHoleDebrisPlacement D_shelter_b3_dumping_hole_801884CC[];

extern EvsCommand            D_shelter_b3_dumping_hole_80188640[];
extern EvsCommand            D_shelter_b3_dumping_hole_80188A78[];
extern WorldCollisionTrigger D_shelter_b3_dumping_hole_8018ECA4[10];
extern Task*                 D_shelter_b3_dumping_hole_8018F4AC;
extern ActorTransform        D_shelter_b3_dumping_hole_8018966C;

extern s32 D_shelter_b3_dumping_hole_8018F4D8;

/// Collapse director's message table, installed in `Task::msgTable`.
///
/// The director sends itself only `ACTOR_MESSAGE_SET_MODEL_DRAW` and
/// `ACTOR_MESSAGE_PLACE`. The table has no `TASK_MESSAGE_TABLE_END` row, so
/// those are the only ids it may receive.
extern TaskMessageEntry         D_shelter_b3_dumping_hole_8018965C[2];
extern EvsCommand               D_shelter_b3_dumping_hole_8018968C[];
extern EvsCommand               D_shelter_b3_dumping_hole_801899A4[];
extern TaskDesc                 D_shelter_b3_dumping_hole_8018AFBC;
static CapSequenceRecord*       CapCaption_Data_8015E658;
static s16                      CapCaption_Data_8015E662;
static CapCaptionScheduleWindow CapCaption_Data_80154514[];
static TextGlyphCell*           CapCaption_Data_8015E654;
static CapCommandRef*           CapCaption_Data_8015E650;
static s16                      CapCaption_Data_8015E65C;
static s16                      CapCaption_Data_8015E65E;
static s16                      CapCaption_Data_8015E660;
static s16                      CapCaption_Data_8015E664;
static s16                      CapCaption_Data_8015E666;

static s16      CapCaption_Data_801544EC;
static s16      CapCaption_Data_801544EE;
static u16      CapCaption_Data_8015E668;
static u16      CapCaption_Data_8015E66A;
static s32      CapCaption_Data_801545E4;
static s32      CapCaption_Data_801545E8;
static TaskDesc CapCaption_Data_801544FC;
extern TaskDesc Actor04400_D107E4;
extern TaskDesc D_actor_207000_801575F0;
static TaskDesc CapCaption_Data_80154508;

/// Encounter controller's message table, installed in `Task::msgTable`.
///
/// `ACTOR_COMMAND_MESSAGE_APPLY` stops the controller when the borrowed
/// `ActorCommand::command` is 4. Any other id reaches `TASK_MESSAGE_TABLE_END`
/// and returns zero.
extern TaskMessageEntry D_shelter_b3_dumping_hole_8018B7AC[2];

/// Player commands posted by the debris event's scripts, including its skip script.
enum {
    SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_NONE                          = 0,
    SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PREPARE_ANIMATION_47          = 1, // Place/move, wait six ticks, then play clip 47
    SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PLAY_ANIMATION_50             = 2,
    SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_HIDE_MODEL                    = 3,
    SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_RESTORE_PLAYER                = 4, // Show, place at the final pose and play clip 9
    SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PLAY_ANIMATION_48             = 5, // Broadcast actor command 1 and wait sixteen ticks first
    SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PLAY_ANIMATION_51_DOUBLE_RATE = 6, // Play clip 51 at twice the normal playback rate
    SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PLAY_ANIMATION_49             = 7,
};

/// Scene commands posted by the debris event's scripts; value 3 has no handled action.
enum {
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_NONE           = 0,
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_LAUNCH_DEBRIS  = 1, // Launch effects, then show actor 0 and broadcast command 2
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_RESTORE_ACTOR0 = 2, // Restore the initial pose and broadcast command 3
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_RESUME_BATTLE  = 4,
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_PLACE_ACTOR1   = 5,
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_CREATE_DEBRIS  = 6, // Stop actor sprites and spawn rubble and sprite rings
    SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_REMOVE_DEBRIS  = 7,
};

/// Glutton model modes used by the debris event's placement-0 callback.
enum {
    SHELTER_B3_DUMPING_HOLE_ACTOR0_DRAW_HIDE_RESET_ALLOCATE = 0,
    SHELTER_B3_DUMPING_HOLE_ACTOR0_DRAW_HIDE_RESET          = 2,
};

/// Actor command resuming the dumping-hole battle in the active stage/area namespace.
enum { SHELTER_B3_DUMPING_HOLE_ACTOR_COMMAND_RESUME_BATTLE = 5 };

/// Selector enabling the arrival event's player-sprite spawn and stop callbacks.
enum { SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITES_SELECT = 0 };

static void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0);

static void func_shelter_b3_dumping_hole_8017F1B0(Task* arg0);
static void func_shelter_b3_dumping_hole_80183218(u8 arg0);
static void _shelterB3DumpingHoleSpawnPlayerSpriteBurst(GfxCoord* sourceCoord, s16 selector);

static void func_shelter_b3_dumping_hole_801833EC(Task* arg0);
static void func_shelter_b3_dumping_hole_80183E6C(s16 arg0, s16 arg1, s16 arg2);

static void func_shelter_b3_dumping_hole_80183298(Task* arg0);
static void func_shelter_b3_dumping_hole_801836E0(Task* arg0);
static void func_shelter_b3_dumping_hole_8018378C(Task* arg0);
static void func_shelter_b3_dumping_hole_80183824(Task* arg0);
static void func_shelter_b3_dumping_hole_801838A0(Task* arg0);
static void func_shelter_b3_dumping_hole_80183950(Task* arg0);
static void func_shelter_b3_dumping_hole_80183A00(Task* arg0);
static void func_shelter_b3_dumping_hole_80183A98(Task* arg0);
static void func_shelter_b3_dumping_hole_80183AEC(Task* arg0);
static void func_shelter_b3_dumping_hole_80183B9C(Task* arg0);
static void func_shelter_b3_dumping_hole_80183C38(Task* arg0);
static void func_shelter_b3_dumping_hole_80183C8C(Task* arg0);
static void func_shelter_b3_dumping_hole_80183CA0(Task* arg0);
static void func_shelter_b3_dumping_hole_80183D34(Task* arg0);
static void func_shelter_b3_dumping_hole_80183E08(Task* arg0);
static void _madChaserWavePairDropDead(Task* task);

static void _shelterB3DumpingHoleDebrisSpriteTask(Task* task);
static void _shelterB3DumpingHoleActorSpriteTask(Task* task);
static void _shelterB3DumpingHolePlayerSpriteTask(Task* task);
static void _shelterB3DumpingHoleDebrisModelTask(Task* task);
void        func_shelter_b3_dumping_hole_8017F820(Task*);
static void _shelterB3DumpingHoleFadeFromBlackTask(Task* task);
static void _shelterB3DumpingHoleBroadcastActorCommand(s16 command);
void        func_shelter_b3_dumping_hole_8017FE34(void);
static void _shelterB3DumpingHoleDebrisEventSetActor0DrawMode(s32 drawMode);
static void _shelterB3DumpingHoleDebrisEventSetActor1DrawMode(s32 drawMode);
static void _shelterB3DumpingHoleDebrisEventPostPlayerCommand(s16 command);
static void _shelterB3DumpingHoleDebrisEventPostSceneCommand(s16 command);
void        func_shelter_b3_dumping_hole_8017FF14(void);
static void _shelterB3DumpingHoleDebrisEventStageAudioStart(void);
static void _shelterB3DumpingHoleDebrisEventEnqueuePlayback(void);
void        func_shelter_b3_dumping_hole_80180034(void);

static void _shelterB3DumpingHoleShardTask(Task* task);
void        func_shelter_b3_dumping_hole_80181430(void);
void        func_shelter_b3_dumping_hole_80181560(Task*);
void        func_shelter_b3_dumping_hole_801818E0(void);
static void _shelterB3DumpingHoleCollapseEventSetPlayerDrawMode(s32 drawMode);
static void _shelterB3DumpingHoleCollapseEventPostCommand(s16 command);
static void _shelterB3DumpingHoleCollapseEventStageAudioStart(void);
static void _shelterB3DumpingHoleCollapseEventEnqueuePlayback(void);
void        func_shelter_b3_dumping_hole_801819F0(void);

static void _shelterB3DumpingHoleShakeTask(Task* task);

static AnimationSet _gShelterB3DumpingHoleAnimation0C810;
static AnimationSet _gShelterB3DumpingHoleAnimation0CCB4;
static AnimationSet _gShelterB3DumpingHoleAnimation0D9C4;

extern AnimationPlayRequest      D_shelter_b3_dumping_hole_8018AFF4;
extern AnimationPlayRequest      D_shelter_b3_dumping_hole_8018B008;
extern AnimationPlayRequest      D_shelter_b3_dumping_hole_8018B01C;
extern ActorCommand              D_shelter_b3_dumping_hole_8018B078;
extern PadScriptCmd              D_shelter_b3_dumping_hole_8018AFAC[2];
extern PadScriptVibrationSegment D_shelter_b3_dumping_hole_8018AFB4[2];
extern ActorTransform            D_shelter_b3_dumping_hole_8018B030;
extern ActorTransform            D_shelter_b3_dumping_hole_8018B048;
extern ActorTransform            D_shelter_b3_dumping_hole_8018B060;
static void                      _shelterB3DumpingHoleSpawnShake(void);
static void                      _shelterB3DumpingHoleSpawnPlayerSprites(s16 selector);
static void                      _shelterB3DumpingHoleRequestPlayerSpriteStop(s32 selector);

extern WorldCollisionGrid    D_shelter_b3_dumping_hole_8018C3EC[1];
extern WorldCollisionTrigger D_shelter_b3_dumping_hole_8018E88C[8];
extern WorldCollisionTrigger D_shelter_b3_dumping_hole_8018EF9C[8];

s32  _shelterB3DumpingHoleEncounterStopMessage(Task* task, s32 messageId, ActorCommand* request, s32 unused);
void func_shelter_b3_dumping_hole_80183550(Task*);
void func_shelter_b3_dumping_hole_801835C8(Task*);
void func_shelter_b3_dumping_hole_80183620(Task*);
void func_shelter_b3_dumping_hole_80183678(Task*);

extern SpriteDrawArea D_shelter_b3_dumping_hole_8018D3E0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018C944[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018C954[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018C964[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CAA0[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CAB8[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CAC8[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CAD8[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC28[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC40[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC50[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC60[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018CC70[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018D3B0[6];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018D7B4[4];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018D964[4];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018D984[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DB24[4];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DBE4[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DBFC[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DC0C[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD34[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD4C[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD5C[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD80[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DD98[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DF88[3];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFA0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFB0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFC0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFD0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFE0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018DFF0[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018E000[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018E010[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018E020[2];
extern SpriteBatch    D_shelter_b3_dumping_hole_8018E030[2];
extern SpriteSource   D_shelter_b3_dumping_hole_8018C974[15];
extern SpriteSource   D_shelter_b3_dumping_hole_8018CAE8[16];
extern SpriteSource   D_shelter_b3_dumping_hole_8018CC80[92];
extern SpriteSource   D_shelter_b3_dumping_hole_8018D3F4[48];
extern SpriteSource   D_shelter_b3_dumping_hole_8018D7D4[20];
extern SpriteSource   D_shelter_b3_dumping_hole_8018D994[20];
extern SpriteSource   D_shelter_b3_dumping_hole_8018DB44[8];
extern SpriteSource   D_shelter_b3_dumping_hole_8018DC1C[14];
extern SpriteSource   D_shelter_b3_dumping_hole_8018DD6C[1];
extern SpriteSource   D_shelter_b3_dumping_hole_8018DDA8[24];
extern TaskDesc       D_actor_342100_80164B78[];
extern TaskDesc       D_actor_341700_80174D58;
extern TaskDesc       D_shelter_b3_dumping_hole_80188BC8[5];

_ShelterB3DumpingHoleSpriteFrame D_shelter_b3_dumping_hole_801880B8[13] = {
    { 704, 0, 112, 112, 48, 48 },
    { 716, 48, 112, 112, 48, 48 },
    { 728, 96, 112, 112, 48, 48 },
    { 740, 144, 112, 112, 48, 48 },
    { 752, 192, 112, 112, 48, 48 },
    { 704, 0, 160, 160, 48, 48 },
    { 716, 48, 160, 160, 48, 48 },
    { 728, 96, 160, 160, 48, 48 },
    { 740, 144, 160, 160, 48, 48 },
    { 752, 192, 160, 160, 48, 48 },
    { 704, 0, 208, 208, 48, 48 },
    { 716, 48, 208, 208, 48, 48 },
    { SHELTER_B3_DUMPING_HOLE_SPRITE_FRAME_END, 0, 0, 0, 0, 0 },
};

s16 D_shelter_b3_dumping_hole_80188154[12] = {
    2,
    2,
    2,
    2,
    2,
    60,
    2,
    2,
    2,
    2,
    2,
    2,
};

s16 D_shelter_b3_dumping_hole_8018816C[12] = {
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

s16 D_shelter_b3_dumping_hole_80188184[12] = {
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

s32 D_shelter_b3_dumping_hole_8018819C[12] = {
    8000,
    0,
    -6000,
    0,
    0x4000000,
    0,
    0x290E,
    0,
    -6000,
    0,
    0x4000000,
    0,
};

ActorTransform D_shelter_b3_dumping_hole_801881CC = { { 0x290E, 0, -6000, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_shelter_b3_dumping_hole_801881E4 = { { 5000, 1800, -6000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_shelter_b3_dumping_hole_801881FC[11] = {
    { { 9000, -1000, -1500, 0 }, { 0, 0, 0, 0 } },
    { { 8900, -1600, -5100, 0 }, { 0, 0, 0, 0 } },
    { { 8200, -2300, -5300, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1800, -5800, 0 }, { 0, 0, 0, 0 } },
    { { 8400, -1800, -6900, 0 }, { 0, 0, 0, 0 } },
    { { 9100, -1200, -5800, 0 }, { 0, 0, 0, 0 } },
    { { 9000, -1200, -7000, 0 }, { 0, 0, 0, 0 } },
    { { 9000, -800, -5700, 0 }, { 0, 0, 0, 0 } },
    { { 8400, -2100, -6000, 0 }, { 0, 0, 0, 0 } },
    { { 7900, -2900, -5500, 0 }, { 0, 0, 0, 0 } },
    { { SHELTER_B3_DUMPING_HOLE_TRANSFORM_END, 0, 0, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_shelter_b3_dumping_hole_80188304[19] = {
    { { 8800, -1000, -5900, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -2400, -6200, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1400, -7200, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1500, -6700, 0 }, { 0, 0, 0, 0 } },
    { { 9100, -1100, -6100, 0 }, { 0, 0, 0, 0 } },
    { { 9000, -1400, -5800, 0 }, { 0, 0, 0, 0 } },
    { { 8900, -900, -5400, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1100, -4900, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1300, -4400, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1700, -4700, 0 }, { 0, 0, 0, 0 } },
    { { 8900, -2000, -5100, 0 }, { 0, 0, 0, 0 } },
    { { 8600, -2300, -4400, 0 }, { 0, 0, 0, 0 } },
    { { 8400, -2900, -5000, 0 }, { 0, 0, 0, 0 } },
    { { 8400, -2000, -7000, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -2200, -7600, 0 }, { 0, 0, 0, 0 } },
    { { 8800, -1300, -7600, 0 }, { 0, 0, 0, 0 } },
    { { 9100, -800, -7200, 0 }, { 0, 0, 0, 0 } },
    { { 9000, -700, -4600, 0 }, { 0, 0, 0, 0 } },
    { { SHELTER_B3_DUMPING_HOLE_TRANSFORM_END, 0, 0, 0 }, { 0, 0, 0, 0 } },
};

_ShelterB3DumpingHoleDebrisPlacement D_shelter_b3_dumping_hole_801884CC[13] = {
    { { { 8300, -2900, -5600, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 9000, -1200, -6700, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 8600, -2700, -7000, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 8600, -2300, -6700, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 8600, -1800, -5500, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 8800, -1500, -5400, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 8900, -1000, -7700, 0 }, { 0, 0, 0, 0 } }, 0 },
    { { { 8800, -1900, -6300, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 8300, -2300, -4800, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 8700, -1700, -4400, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 8500, -1800, -7700, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { 8700, -2300, -5600, 0 }, { 0, 0, 0, 0 } }, 1 },
    { { { SHELTER_B3_DUMPING_HOLE_TRANSFORM_END, 0, 0, 0 }, { 0, 0, 0, 0 } }, 0 },
};

EvsSceneKey D_shelter_b3_dumping_hole_80188638 = { 4, 17, 11 };

EvsCommand D_shelter_b3_dumping_hole_80188640[45] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_8017FE34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostPlayerCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PREPARE_ANIMATION_47 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostSceneCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_PLACE_ACTOR1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3DumpingHoleDebrisEventSetActor0DrawMode }, { .value = SHELTER_B3_DUMPING_HOLE_ACTOR0_DRAW_HIDE_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3DumpingHoleDebrisEventSetActor1DrawMode }, { .value = ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_shelter_b3_dumping_hole_80188638 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB3DumpingHoleDebrisEventStageAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB3DumpingHoleDebrisEventEnqueuePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3DumpingHoleDebrisEventSetActor1DrawMode }, { .value = ACTOR_MESSAGE_VISIBILITY_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostPlayerCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PLAY_ANIMATION_48 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostPlayerCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PLAY_ANIMATION_51_DOUBLE_RATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostPlayerCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PLAY_ANIMATION_49 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostPlayerCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_PLAY_ANIMATION_50 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostPlayerCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_HIDE_MODEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3DumpingHoleDebrisEventSetActor0DrawMode }, { .value = SHELTER_B3_DUMPING_HOLE_ACTOR0_DRAW_HIDE_RESET_ALLOCATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3DumpingHoleDebrisEventSetActor1DrawMode }, { .value = ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostSceneCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_CREATE_DEBRIS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostSceneCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_LAUNCH_DEBRIS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostPlayerCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_RESTORE_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostSceneCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_REMOVE_DEBRIS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostSceneCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_RESTORE_ACTOR0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_80180034 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostSceneCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_RESUME_BATTLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostPlayerCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b3_dumping_hole_80188A78[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostPlayerCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleDebrisEventPostSceneCommand }, { .value = SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_8017FF14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleBroadcastActorCommand }, { .value = SHELTER_B3_DUMPING_HOLE_ACTOR_COMMAND_RESUME_BATTLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b3_dumping_hole_80188BC8[5] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b3_dumping_hole_8017F820, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _shelterB3DumpingHoleFadeFromBlackTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _shelterB3DumpingHoleDebrisModelTask, { .model = &gShelterB3DumpingHoleModel0A0CC } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _shelterB3DumpingHoleDebrisModelTask, { .model = &gShelterB3DumpingHoleModel0A348 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _shelterB3DumpingHoleDebrisModelTask, { .model = &gShelterB3DumpingHoleModel0A5EC } },
};

TaskDesc D_shelter_b3_dumping_hole_80188C04[4] = {
    { { { TASK_BODY_COORD, 192 } }, _shelterB3DumpingHoleDebrisSpriteTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, _shelterB3DumpingHoleActorSpriteTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, _shelterB3DumpingHolePlayerSpriteTask, { .value = 0 } },
};

static TmdBone _gShelterB3DumpingHoleModel0BAC8Skeleton[4] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_skeleton.inc"
};

static u32 _gShelterB3DumpingHoleModel0BAC8PartVerts[4] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_partVerts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0BAC8Verts[100] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_verts.inc"
};

static SVECTOR _gShelterB3DumpingHoleModel0BAC8Normals[18] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_normals.inc"
};

static u32 _gShelterB3DumpingHoleModel0BAC8Stream[365] = {
#include "assets/shelter_b3_dumping_hole_model_0BAC8_stream.inc"
};

static TmdSource _gShelterB3DumpingHoleModel0BAC8 = {
    0,
    2600,
    0,
    4,
    _gShelterB3DumpingHoleModel0BAC8PartVerts,
    _gShelterB3DumpingHoleModel0BAC8Verts,
    _gShelterB3DumpingHoleModel0BAC8Normals,
    _gShelterB3DumpingHoleModel0BAC8Skeleton,
    _gShelterB3DumpingHoleModel0BAC8Stream,
};

TaskMessageEntry D_shelter_b3_dumping_hole_8018965C[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

ActorTransform D_shelter_b3_dumping_hole_8018966C = { { 4500, -0x2CEC, -5450, 0 }, { 341, 0, 0, 0 } };

EvsSceneKey D_shelter_b3_dumping_hole_80189684 = { 4, 18, 11 };

EvsCommand D_shelter_b3_dumping_hole_8018968C[33] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleCollapseEventPostCommand }, { .value = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_PREPARE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_shelter_b3_dumping_hole_80189684 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB3DumpingHoleCollapseEventStageAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB3DumpingHoleCollapseEventEnqueuePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleCollapseEventPostCommand }, { .value = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_COLLAPSE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleCollapseEventPostCommand }, { .value = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_END_COLLAPSE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleCollapseEventPostCommand }, { .value = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_HIDE_ACTOR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3DumpingHoleCollapseEventSetPlayerDrawMode }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleCollapseEventPostCommand }, { .value = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_FLICKER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_801819F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleCollapseEventPostCommand }, { .value = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_FINISH }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3DumpingHoleCollapseEventSetPlayerDrawMode }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleCollapseEventPostCommand }, { .value = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_801818E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b3_dumping_hole_801899A4[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleCollapseEventPostCommand }, { .value = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_80181430 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b3_dumping_hole_801818E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_shelter_b3_dumping_hole_80189ADC[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_shelter_b3_dumping_hole_80181560, { .model = &_gShelterB3DumpingHoleModel0BAC8 } },
    { { { TASK_BODY_COORD, 192 } }, _shelterB3DumpingHoleShardTask, { .value = 0 } },
};

static AnimationPackedPose _gShelterB3DumpingHoleAnimation0C810Bank1[6] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_bank1.inc"
};

static AnimationPackedRotation _gShelterB3DumpingHoleAnimation0C810Bank4[46] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_bank4.inc"
};

static AnimationRecord _gShelterB3DumpingHoleAnimation0C810Records[109] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_records.inc"
};

static u16 _gShelterB3DumpingHoleAnimation0C810Indices[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0C810_indices.inc"
};

static AnimationSet _gShelterB3DumpingHoleAnimation0C810 = {
    _gShelterB3DumpingHoleAnimation0C810Records,
    _gShelterB3DumpingHoleAnimation0C810Indices,
    { NULL, _gShelterB3DumpingHoleAnimation0C810Bank1, NULL, NULL, _gShelterB3DumpingHoleAnimation0C810Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB3DumpingHoleAnimation0CCB4Bank1[9] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_bank1.inc"
};

static AnimationPackedRotation _gShelterB3DumpingHoleAnimation0CCB4Bank4[97] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_bank4.inc"
};

static AnimationRecord _gShelterB3DumpingHoleAnimation0CCB4Records[153] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_records.inc"
};

static u16 _gShelterB3DumpingHoleAnimation0CCB4Indices[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0CCB4_indices.inc"
};

static AnimationSet _gShelterB3DumpingHoleAnimation0CCB4 = {
    _gShelterB3DumpingHoleAnimation0CCB4Records,
    _gShelterB3DumpingHoleAnimation0CCB4Indices,
    { NULL, _gShelterB3DumpingHoleAnimation0CCB4Bank1, NULL, NULL, _gShelterB3DumpingHoleAnimation0CCB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gShelterB3DumpingHoleAnimation0D9C4Bank1[28] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_bank1.inc"
};

static AnimationPackedRotation _gShelterB3DumpingHoleAnimation0D9C4Bank4[232] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_bank4.inc"
};

static AnimationRecord _gShelterB3DumpingHoleAnimation0D9C4Records[500] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_records.inc"
};

static u16 _gShelterB3DumpingHoleAnimation0D9C4Indices[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0D9C4_indices.inc"
};

static AnimationSet _gShelterB3DumpingHoleAnimation0D9C4 = {
    _gShelterB3DumpingHoleAnimation0D9C4Records,
    _gShelterB3DumpingHoleAnimation0D9C4Indices,
    { NULL, _gShelterB3DumpingHoleAnimation0D9C4Bank1, NULL, NULL, _gShelterB3DumpingHoleAnimation0D9C4Bank4, NULL, NULL, NULL },
};

PadScriptCmd D_shelter_b3_dumping_hole_8018AFAC[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_shelter_b3_dumping_hole_8018AFB4[2] = {
    { 255, 250, 9, 1 },
    { 0, 0, 9, 0 },
};

TaskDesc D_shelter_b3_dumping_hole_8018AFBC = { { { TASK_BODY_NONE, 192 } }, _shelterB3DumpingHoleShakeTask, { .value = 0 } };

_ShelterB3DumpingHoleAnimationBankExtensionStorage D_shelter_b3_dumping_hole_8018AFC8 = { .data = { { NULL, &_gShelterB3DumpingHoleAnimation0C810, &_gShelterB3DumpingHoleAnimation0CCB4, &_gShelterB3DumpingHoleAnimation0D9C4 }, { { .words = D_shelter_b3_dumping_hole_8018AFC8.words }, 5 } } };

AnimationPlayRequest D_shelter_b3_dumping_hole_8018AFE0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b3_dumping_hole_8018AFF4 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_shelter_b3_dumping_hole_8018B008 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b3_dumping_hole_8018B01C = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_shelter_b3_dumping_hole_8018B030 = { { 0x2904, 0, -3300, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_shelter_b3_dumping_hole_8018B048 = { { 0x2904, -100, -3300, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_shelter_b3_dumping_hole_8018B060 = { { 8800, 0, -6000, 0 }, { 0, 1024, 0, 0 } };

ActorCommand D_shelter_b3_dumping_hole_8018B078 = { { .loc = { 4, 39 } }, 0 };

ActorCommand D_shelter_b3_dumping_hole_8018B07C = { { .loc = { 4, 39 } }, 1 };

EvsCommand D_shelter_b3_dumping_hole_8018B080[39] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &D_shelter_b3_dumping_hole_8018AFC8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b3_dumping_hole_8018B078 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b3_dumping_hole_8018AFF4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54270001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 130 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b3_dumping_hole_8018B008 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b3_dumping_hole_8018B030 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54270002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB3DumpingHoleSpawnShake }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_shelter_b3_dumping_hole_8018AFAC }, { .vibrationSegments = D_shelter_b3_dumping_hole_8018AFB4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _shelterB3DumpingHoleSpawnPlayerSprites }, { .value = SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITES_SELECT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3DumpingHoleRequestPlayerSpriteStop }, { .value = SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITES_SELECT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b3_dumping_hole_8018B01C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b3_dumping_hole_8018B048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b3_dumping_hole_8018B060 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b3_dumping_hole_8018B428[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b3_dumping_hole_8018B078 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x54270001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x54270002 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_shelter_b3_dumping_hole_8018B060 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _shelterB3DumpingHoleRequestPlayerSpriteStop }, { .value = SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITES_SELECT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/cap_captions_settings.inc.c"

static void CapCaption_RunSchedule(Task* task);

TaskDesc D_shelter_b3_dumping_hole_8018B57C[1] = {
    { { { TASK_BODY_NONE, 32 } }, CapCaption_RunSchedule, { .value = 0 } }
};

#include "../../shared/cap_captions_schedule.inc.c"

WorldCollisionRoomResources D_shelter_b3_dumping_hole_8018B678[2] = {
    { D_shelter_b3_dumping_hole_8018C3EC, NULL, D_shelter_b3_dumping_hole_8018ECA4, NULL },
    { D_shelter_b3_dumping_hole_8018C3EC, D_shelter_b3_dumping_hole_8018E88C, D_shelter_b3_dumping_hole_8018EF9C, NULL },
};

u8* D_shelter_b3_dumping_hole_8018B698[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount D_shelter_b3_dumping_hole_8018B6A0[2] = { 37, 37 };

DirectionWarpEntry D_shelter_b3_dumping_hole_8018B6A4[3] = {
    { { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, 0x54270004, 0x54270003, DIRECTION_WARP_SOUND_NONE, 30, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 30, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x4C2C, 0, -4450 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 15, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

OverlayEncounterSpot D_shelter_b3_dumping_hole_8018B74C[12] = {
    { 1500, -3950, -550, 2048 },
    { 4500, -3950, -550, 2048 },
    { 7500, -3950, -550, 2048 },
    { 10500, -3950, -550, 2048 },
    { 13500, -3950, -550, 2048 },
    { 16500, -3950, -550, 2048 },
    { 1500, -3950, -12450, 0 },
    { 4500, -3950, -12450, 0 },
    { 7500, -3950, -12450, 0 },
    { 10500, -3950, -12450, 0 },
    { 13500, -3950, -12450, 0 },
    { 16500, -3950, -12450, 0 },
};

TaskMessageEntry D_shelter_b3_dumping_hole_8018B7AC[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _shelterB3DumpingHoleEncounterStopMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

OverlayEncounterSlot D_shelter_b3_dumping_hole_8018B7BC[16] = {
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(3, 2), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(9, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(2, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(4, 1), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(8, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(9, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(3, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(10, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(4, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(10, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(4, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(10, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(5, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(11, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(11, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(3, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
};

TaskDesc D_shelter_b3_dumping_hole_8018B83C[4] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b3_dumping_hole_80183550, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_shelter_b3_dumping_hole_801835C8, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_shelter_b3_dumping_hole_80183620, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_shelter_b3_dumping_hole_80183678, { .value = 0 } },
};

// Lighting task indexes a shared pool through entry 40; interior bases also address entries in the same pool. The final existing eight-point view ends at entry 43.
SVECTOR D_shelter_b3_dumping_hole_8018B86C[44] = {
    { 1170, -7750, 150, 0 },
    { 1830, -7750, 150, 0 },
    { 4170, -7750, 150, 0 },
    { 4830, -7750, 150, 0 },
    { 7170, -7750, 150, 0 },
    { 7830, -7750, 150, 0 },
    { 10170, -7750, 150, 0 },
    { 10830, -7750, 150, 0 },
    { 13170, -7750, 150, 0 },
    { 13830, -7750, 150, 0 },
    { 16170, -7750, 150, 0 },
    { 16830, -7750, 150, 0 },
    { 1170, -7750, -12150, 0 },
    { 1830, -7750, -12150, 0 },
    { 4170, -7750, -12150, 0 },
    { 4830, -7750, -12150, 0 },
    { 7170, -7750, -12150, 0 },
    { 7830, -7750, -12150, 0 },
    { 10170, -7750, -12150, 0 },
    { 10830, -7750, -12150, 0 },
    { 13170, -7750, -12150, 0 },
    { 13830, -7750, -12150, 0 },
    { 16170, -7750, -12150, 0 },
    { 16830, -7750, -12150, 0 },
    { 1500, -10440, -3500, 0 },
    { 4500, -10440, -3500, 0 },
    { 1500, -10440, -8500, 0 },
    { 4500, -10440, -8500, 0 },
    { 19090, -5050, -4610, 0 },
    { 19090, -5050, -7300, 0 },
    { 19090, -1044, -3620, 0 },
    { 20150, -2050, -4400, 0 },
    { 1500, -4380, 550, 0 },
    { 4500, -4380, 550, 0 },
    { 7500, -4380, 550, 0 },
    { 10500, -4380, 550, 0 },
    { 13500, -4380, 550, 0 },
    { 16500, -4380, 550, 0 },
    { 1500, -4380, -12550, 0 },
    { 4500, -4380, -12550, 0 },
    { 7500, -4380, -12550, 0 },
    { 10500, -4380, -12550, 0 },
    { 13500, -4380, -12550, 0 },
    { 16500, -4380, -12550, 0 },
};

static SVECTOR _gShelterB3DumpingHoleCollision0EE2CNormals[35] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_normals.inc"
};

static SVECTOR _gShelterB3DumpingHoleCollision0EE2CVerts[90] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_verts.inc"
};

static WorldCollisionGridFace _gShelterB3DumpingHoleCollision0EE2CFaces[56] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_faces.inc"
};

static s16 _gShelterB3DumpingHoleCollision0EE2CCells[412] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB3DumpingHoleCollision0EE2CCells[i])
static s16* _gShelterB3DumpingHoleCollision0EE2CTable[24] = {
#include "assets/shelter_b3_dumping_hole_collision_0EE2C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b3_dumping_hole_8018C3EC[1] = {
    { NULL, _gShelterB3DumpingHoleCollision0EE2CNormals, _gShelterB3DumpingHoleCollision0EE2CVerts, _gShelterB3DumpingHoleCollision0EE2CFaces, _gShelterB3DumpingHoleCollision0EE2CTable, 100, 0x2F44, 6, 4, 4000, 56 },
};

ViewCamera D_shelter_b3_dumping_hole_8018C410[37] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2710, 0x61A8, 6000 } }, 257 },
    { { { { 3054, 0, 2728 }, { 510, 4023, -571 }, { -2680, 766, 3000 } }, { -0x452F, 2991, 0x2DB7 } }, 257 },
    { { { { 3126, 0, 2645 }, { 600, 3989, -709 }, { -2576, 929, 3045 } }, { -0x3779, 3121, 0x2A83 } }, 257 },
    { { { { 2261, 0, -3415 }, { -290, 4081, -192 }, { 3403, 348, 2252 } }, { -0x2919, 2411, 9983 } }, 257 },
    { { { { 4028, 0, -739 }, { -727, 736, -3963 }, { 132, 4029, 724 } }, { -8741, 9891, 7423 } }, 289 },
    { { { { 4028, 0, -739 }, { -727, 736, -3963 }, { 132, 4029, 724 } }, { -0x3CD3, 9891, 7423 } }, 289 },
    { { { { 4095, 0, 19 }, { 2, 4057, -556 }, { -19, 556, 4057 } }, { -0x33FE, 2784, 0x3158 } }, 230 },
    { { { { 2223, 0, -3439 }, { -1144, 3862, -739 }, { 3243, 1362, 2096 } }, { -0x31C5, 2794, 9228 } }, 257 },
    { { { { 1013, 0, -3968 }, { -3267, 2324, -834 }, { 2252, 3372, 574 } }, { -8292, 7543, 6554 } }, 289 },
    { { { { 1061, 0, -3956 }, { -2152, 3436, -577 }, { 3319, 2228, 890 } }, { -0x333C, 4161, 7069 } }, 289 },
    { { { { 4028, 0, -739 }, { -723, 834, -3944 }, { 150, 4010, 821 } }, { -0x2E19, 8861, 7423 } }, 289 },
    { { { { 4037, 0, -690 }, { -663, 1141, -3877 }, { 192, 3933, 1125 } }, { -0x41F1, 7141, 7933 } }, 257 },
    { { { { 618, 0, 4049 }, { -574, 4054, 87 }, { -4008, -580, 611 } }, { -0x511E, 690, 6915 } }, 257 },
    { { { { 2533, 0, -3218 }, { -725, 3990, -570 }, { 3135, 922, 2468 } }, { -0x410B, 1800, 6163 } }, 329 },
    { { { { 2538, 0, 3214 }, { -729, 3989, 575 }, { -3130, -929, 2472 } }, { -0x3A3F, 581, 4683 } }, 289 },
    { { { { 3865, 0, -1353 }, { 0, 4096, 0 }, { 1353, 0, 3865 } }, { -9755, 1115, 5205 } }, 447 },
    { { { { 3700, 0, -1756 }, { 511, 3918, 1077 }, { 1680, -1193, 3539 } }, { -0x2FE5, 2981, 1883 } }, 257 },
    { { { { 2435, 0, -3293 }, { 228, 4086, 168 }, { 3285, -283, 2429 } }, { -6561, 901, 8903 } }, 257 },
    { { { { 547, 0, 4059 }, { -348, 4080, 47 }, { -4044, -351, 545 } }, { -0x3071, 1141, 6693 } }, 257 },
    { { { { 2366, 0, 3342 }, { 2987, 1837, -2115 }, { -1499, 3660, 1061 } }, { -0x28F1, 5771, 7663 } }, 289 },
    { { { { 2828, 0, 2962 }, { -1276, 3696, 1218 }, { -2673, -1764, 2552 } }, { -6451, 9662, 7109 } }, 257 },
    { { { { 2533, 0, -3218 }, { -725, 3990, -570 }, { 3135, 922, 2468 } }, { -0x410B, 1800, 6163 } }, 329 },
    { { { { 449, 0, 4071 }, { -401, 4075, 44 }, { -4051, -404, 447 } }, { -0x2B7A, 1770, 6430 } }, 230 },
    { { { { 3412, 0, 2265 }, { 1845, 2373, -2781 }, { -1312, 3337, 1977 } }, { -0x3C29, 0x2879, 0x2BFF } }, 275 },
    { { { { 449, 0, -4071 }, { 286, 4085, 31 }, { 4061, -287, 448 } }, { -0x376A, 1090, 6535 } }, 230 },
    { { { { 3508, 0, -2113 }, { -876, 3727, -1454 }, { 1923, 1697, 3192 } }, { -0x31C5, 4261, 0x297F } }, 230 },
    { { { { 941, 0, -3986 }, { -3234, 2393, -764 }, { 2329, 3323, 550 } }, { -0x2AB4, 7743, 6554 } }, 289 },
    { { { { 3954, 0, -1065 }, { -1039, 900, -3858 }, { 234, 3995, 869 } }, { -0x3CA1, 8931, 7643 } }, 257 },
    { { { { 473, 0, -4068 }, { 308, 4084, 35 }, { 4056, -310, 472 } }, { -8800, 1401, 6883 } }, 257 },
    { { { { 698, 0, -4035 }, { 449, 4070, 77 }, { 4010, -456, 694 } }, { -0x2F6C, 1401, 6883 } }, 257 },
    { { { { 354, 0, 4080 }, { -290, 4085, 25 }, { -4070, -291, 353 } }, { -0x4589, 1641, 7013 } }, 230 },
    { { { { 627, 0, -4047 }, { -2564, 3169, -397 }, { 3131, 2594, 485 } }, { -3752, 7543, 6554 } }, 289 },
    { { { { 639, 0, 4045 }, { 1942, 3592, -307 }, { -3548, 1967, 560 } }, { -0x40D0, 5263, 7144 } }, 289 },
    { { { { 3310, 0, 2412 }, { 400, 4039, -549 }, { -2378, 680, 3264 } }, { -0x452F, 2991, 0x2DB7 } }, 257 },
    { { { { 344, 0, 4081 }, { 160, 4092, -13 }, { -4078, 160, 344 } }, { -0x3517, 1731, 6823 } }, 230 },
    { { { { 1289, 0, 3887 }, { 2700, 2946, -895 }, { -2797, 2844, 927 } }, { -7481, 2971, 7013 } }, 257 },
    { { { { 618, 0, 4049 }, { -574, 4054, 87 }, { -4008, -580, 611 } }, { -0x511E, 690, 6915 } }, 257 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018C944[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018C954[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018C964[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018C974[15] = {
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, 32, 1637, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 80, 0, 1612, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 40, 1575, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, 32, 1587, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -16, 1587, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 216 } }, 112, -120, 1537, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 120, -120, 1500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 128, -120, 1462, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 136, -120, 1425, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 144, -120, 1412, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 152, -120, 1375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 104, -120, 1575, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 104, -8, 1500, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, -16, 1600, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 96, -80, 1575, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CAA0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CAB8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CAC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CAD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018CAE8[16] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 16, 1025, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, 32, 1075, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 104, -40, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 152 } }, 112, -40, 975, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, -80, 950, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, 120, -120, 950, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 240 } }, 136, -120, 900, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, 24, 1050, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -40, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 96, 0, 1025, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -24, 1000, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -8, 986, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 32, 1087, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 72, 1125, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, 24, 1075, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC28[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018CC70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018CC80[92] = {
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, 16, 950, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, -64, 987, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -128, -64, 987, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -128, 16, 950, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -136, -64, 987, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -136, 16, 950, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, -64, 1012, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -144, 16, 937, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -152, -64, 1025, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -152, 16, 937, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, -64, 1025, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, 16, 937, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -88, 48, 2700, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -24, 48, 2700, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 48, 2700, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -104, 56, 2700, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -40, 56, 2700, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 24, 56, 2700, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 56, 2700, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 64, 2700, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 64, 2700, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 64, 2700, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 64, 2700, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 64, 2700, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 2700, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 64, 2700, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 64, 2700, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 56, 2125, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 48, 2125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -104, 56, 2125, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -32, 56, 2125, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 32, 56, 2125, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -120, 64, 2125, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, 64, 2125, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 8, 64, 2125, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 72, 64, 2125, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 80, 2125, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 80, 2125, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 80, 2125, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 80, 2125, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 80, 2125, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 80, 2125, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 80, 2125, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 80, 2125, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 80, 2125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 80, 2125, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, 72, 2125, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 72, 2125, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 72, 2125, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 72, 2125, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 72, 2125, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 72, 2125, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 72, 2125, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 72, 2125, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 72, 2125, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 72, 2125, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 72, 2125, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 72, 2125, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 72, 2125, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 72, 2125, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 72, 2125, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 72, 2125, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 72, 2125, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 72, 2125, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 56, 1400, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 64, 56, 1425, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 72, 64, 1425, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 120, 72, 1400, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 80, 1400, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 80, 1400, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 80, 1400, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 80, 1400, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 80, 1400, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 80, 1400, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 80, 1400, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 80, 1400, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 80, 1400, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 80, 1400, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -136, 72, 1400, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 72, 1400, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 72, 1400, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 72, 1400, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 72, 1400, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 72, 1400, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 72, 1400, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, 72, 1400, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 64, 1400, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 64, 1400, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 64, 1400, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 64, 1400, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 64, 1400, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 1400, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018D3B0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 15, 0, 0, { 0, 0 } },
    { 27, 37, 0, 0, { 2, 0 } },
    { 64, 28, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_dumping_hole_8018D3E0[2] = {
    { { 1, 0, 318, 196 }, 1400 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_dumping_hole_8018D3F4[48] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 0, -120, 1050, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, -120, 1050, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -120, 1050, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -120, 1050, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -120, 1050, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -120, 1000, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -120, 1000, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, -120, 912, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -80, 1025, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, -80, 975, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -80, 900, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 32, -80, 1075, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, -80, 1050, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, -120, 887, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, -120, 887, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, -120, 875, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -120, 925, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -120, 925, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -120, 950, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -120, 950, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -120, 1000, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -120, 1000, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -96, 750, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -56, 750, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -16, 750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -96, 800, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -56, 800, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -16, 800, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -96, 875, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -56, 875, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -16, 900, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -96, 925, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -56, 925, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, -16, 837, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -96, 950, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -56, 975, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -16, 875, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 120 } }, -48, -96, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -96, 1000, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -56, 1000, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -16, 900, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -48, 24, 1125, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 1025, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 24, 775, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 24, 800, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 24, 912, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 24, 962, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 24, 962, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018D7B4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 1, 0 } },
    { 22, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018D7D4[20] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 625, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 8, 625, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 0, 32, 625, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -104, 48, 625, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -128, 72, 625, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -88, 72, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -48, 72, 625, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -8, 72, 625, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 32, 72, 625, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 72, 625, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 112, 72, 625, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 72, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 80, 32, 625, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 80, -16, 625, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 128, -40, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 32, 625, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, -64, 875, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 56, -112, 875, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 56, -88, 875, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 56, -64, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018D964[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018D984[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018D994[20] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -56, 768, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -56, 769, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, -56, 769, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -56, 789, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -56, -80, 595, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -56, 0, 573, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 40, 691, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -80, 634, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -40, 598, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, 0, 590, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 40, 622, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, -80, 490, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, -40, 465, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, 0, 456, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -136, 40, 450, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, -80, 392, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, -40, 383, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, 0, 371, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 40, 359, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -56, -40, 568, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DB24[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018DB44[8] = {
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -160, 24, 375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -136, 32, 375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -112, 40, 375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -88, 48, 375, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, 56, 375, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -40, 64, 375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, 72, 375, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 8, 96, 375, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DBE4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DBFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DC0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018DC1C[14] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 375, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 0, -120, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 40, -120, 375, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -24, -104, 375, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 32, -104, 375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -40, -88, 375, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 24, -88, 375, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, -88, 375, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -48, -72, 375, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 16, -72, 375, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, -72, 375, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -56, -56, 375, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 8, -56, 375, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -40, -40, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD34[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD4C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018DD6C[1] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 8, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD80[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DD98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_dumping_hole_8018DDA8[24] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 562, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 88, 1300, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 72, 1250, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 80, 1200, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 112, 16, 750, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 104, 24, 712, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 112, 32, 675, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 40, 637, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 48, 600, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 88, 1150, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 96, 1100, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 104, 1050, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 32, 1200, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 32, 1150, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 40, 1100, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, 40, 1050, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, 48, 1000, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 136, 48, 950, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 56, 900, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, 0, 750, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -24, 750, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 48, 1225, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 64, 1300, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, 64, 850, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DF88[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFD0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018DFF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E000[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E010[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E020[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E030[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_dumping_hole_8018E040[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b3_dumping_hole_8018E050[37] = {
    { { .empty = D_shelter_b3_dumping_hole_8018C944 }, D_shelter_b3_dumping_hole_8018C944, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018C954 }, D_shelter_b3_dumping_hole_8018C954, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018C964 }, D_shelter_b3_dumping_hole_8018C964, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018C974 }, D_shelter_b3_dumping_hole_8018CAA0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CAB8 }, D_shelter_b3_dumping_hole_8018CAB8, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CAC8 }, D_shelter_b3_dumping_hole_8018CAC8, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CAD8 }, D_shelter_b3_dumping_hole_8018CAD8, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018CAE8 }, D_shelter_b3_dumping_hole_8018CC28, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CC40 }, D_shelter_b3_dumping_hole_8018CC40, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CC50 }, D_shelter_b3_dumping_hole_8018CC50, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CC60 }, D_shelter_b3_dumping_hole_8018CC60, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018CC70 }, D_shelter_b3_dumping_hole_8018CC70, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018CC80 }, D_shelter_b3_dumping_hole_8018D3B0, D_shelter_b3_dumping_hole_8018D3E0 },
    { { .elements = D_shelter_b3_dumping_hole_8018D3F4 }, D_shelter_b3_dumping_hole_8018D7B4, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018D7D4 }, D_shelter_b3_dumping_hole_8018D964, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018D984 }, D_shelter_b3_dumping_hole_8018D984, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018D994 }, D_shelter_b3_dumping_hole_8018DB24, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018DB44 }, D_shelter_b3_dumping_hole_8018DBE4, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DBFC }, D_shelter_b3_dumping_hole_8018DBFC, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DC0C }, D_shelter_b3_dumping_hole_8018DC0C, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018DC1C }, D_shelter_b3_dumping_hole_8018DD34, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DD4C }, D_shelter_b3_dumping_hole_8018DD4C, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DD5C }, D_shelter_b3_dumping_hole_8018DD5C, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018DD6C }, D_shelter_b3_dumping_hole_8018DD80, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DD98 }, D_shelter_b3_dumping_hole_8018DD98, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018DDA8 }, D_shelter_b3_dumping_hole_8018DF88, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFA0 }, D_shelter_b3_dumping_hole_8018DFA0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFB0 }, D_shelter_b3_dumping_hole_8018DFB0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFC0 }, D_shelter_b3_dumping_hole_8018DFC0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFD0 }, D_shelter_b3_dumping_hole_8018DFD0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFE0 }, D_shelter_b3_dumping_hole_8018DFE0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018DFF0 }, D_shelter_b3_dumping_hole_8018DFF0, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018E000 }, D_shelter_b3_dumping_hole_8018E000, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018E010 }, D_shelter_b3_dumping_hole_8018E010, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018E020 }, D_shelter_b3_dumping_hole_8018E020, NULL },
    { { .empty = D_shelter_b3_dumping_hole_8018E030 }, D_shelter_b3_dumping_hole_8018E030, NULL },
    { { .elements = D_shelter_b3_dumping_hole_8018CC80 }, D_shelter_b3_dumping_hole_8018D3B0, NULL },
};

WorldCoordLight D_shelter_b3_dumping_hole_8018E20C[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9988, -0x2B02, -2510 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2871, 2582, 2295 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9632, -0x2B02, 1990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 925, 820 }, { 0, 0 } },
};

WorldCoordPointLight D_shelter_b3_dumping_hole_8018E2BC[3] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A38, -3000, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 819, 737, 655 }, { 0, 0 } }, 3561, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2903, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4914, 409, 0 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BB, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4914, 409, 0 }, { 0, 0 } }, 1500, 2000 },
};

WorldCoordRoomLights D_shelter_b3_dumping_hole_8018E3DC = { ARRAY_SIZE(D_shelter_b3_dumping_hole_8018E20C), D_shelter_b3_dumping_hole_8018E20C, ARRAY_SIZE(D_shelter_b3_dumping_hole_8018E2BC), D_shelter_b3_dumping_hole_8018E2BC, 0, NULL };

WorldCoordPointLight D_shelter_b3_dumping_hole_8018E3F4[12] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -3980, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1230, 1110, 985 }, { 0, 0 } }, 4202, 7241 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3A98, -3980, -9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1229, 1106, 987 }, { 0, 0 } }, 4339, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -3980, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1230, 1109, 984 }, { 0, 0 } }, 4124, 7086 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3A98, -3980, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1229, 1109, 987 }, { 0, 0 } }, 4239, 6981 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -3980, -9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1229, 1106, 986 }, { 0, 0 } }, 4180, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -3980, -9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1232, 1106, 985 }, { 0, 0 } }, 4275, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A38, -3000, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 1844, 1640 }, { 0, 0 } }, 3561, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2903, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4915, 409, 0 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BB, -4372, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4915, 409, 0 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4DF6, -2052, -4394 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 310, 3279, 1229 }, { 0, 0 } }, 1341, 1738 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A91, -2181, -3367 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2460, 3690, 4096 }, { 0, 0 } }, 1400, 2022 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6000, -9400, -3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2460, 2215, 1970 }, { 0, 0 } }, 3000, 4000 },
};

WorldCoordRoomLights D_shelter_b3_dumping_hole_8018E874 = { 0, NULL, ARRAY_SIZE(D_shelter_b3_dumping_hole_8018E3F4), D_shelter_b3_dumping_hole_8018E3F4, 0, NULL };

WorldCollisionTrigger D_shelter_b3_dumping_hole_8018E88C[8] = {
    { NULL, NULL, NULL, { 0x3F30, -5696, -5696, 0 }, { { 0, -6336, -4624, 0 }, { 0, -6336, 4624, 0 }, { 0, 6336, -4624, 0 }, { 0, 6336, 4624, 0 } }, { 4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 30, 29, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3FE0, -5664, -5728, 0 }, { { 0, -6336, 4624, 0 }, { 0, -6336, -4624, 0 }, { 0, 6336, 4624, 0 }, { 0, 6336, -4624, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 29, 30, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x327F, -5280, -5793, 0 }, { { -452, -6336, 4602, 0 }, { 453, -6336, -4601, 0 }, { -452, 6336, 4602, 0 }, { 453, 6336, -4601, 0 } }, { -4077, 0, -401, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 31, 29, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x31DE, -5345, -5858, 0 }, { { 453, -6336, -4601, 0 }, { -452, -6336, 4602, 0 }, { 453, 6336, -4601, 0 }, { -452, 6336, 4602, 0 } }, { 4076, 0, 400, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 29, 31, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9152, -5376, -6017, 0 }, { { 453, -6336, -4601, 0 }, { -452, -6336, 4602, 0 }, { 453, 6336, -4601, 0 }, { -452, 6336, 4602, 0 } }, { 4076, 0, 400, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 31, 35, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9311, -5344, -5921, 0 }, { { -452, -6336, 4602, 0 }, { 453, -6336, -4601, 0 }, { -452, 6336, 4602, 0 }, { 453, 6336, -4601, 0 } }, { -4077, 0, -401, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 35, 31, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6016, -4993, -6048, 0 }, { { 0, -6336, -4623, 0 }, { 0, -6336, 4623, 0 }, { 0, 6336, -4623, 0 }, { 0, 6336, 4623, 0 } }, { 4095, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 35, 36, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6176, -5120, -6112, 0 }, { { 0, -6336, 4623, 0 }, { 0, -6336, -4623, 0 }, { 0, 6336, 4623, 0 }, { 0, 6336, -4623, 0 } }, { -4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 7832, 0, 36, 35, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaPlacement D_shelter_b3_dumping_hole_8018EAEC[5] = {
    { 32, 0, 0, 4276, 1, -6109, 1024, 0, 0, 2, 0 },
    { 252, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 103, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 44, 1, 0, 0x38C0, 0, -5888, 2048, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_shelter_b3_dumping_hole_8018EB3C[5] = {
    { 32, 32, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_403200_8015F8D0 },
    { 103, 417, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_shelter_b3_dumping_hole_80188BC8 },
    { 252, 417, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &D_actor_341700_80176354 },
    { 44, 44, AREA_RESOURCE_FILE_GROUP_BASE_60, 2, { 0, 0 }, &D_actor_341700_80174D58 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b3_dumping_hole_8018EB78[2] = {
    { 32, 32, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_403200_8015F8D0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_dumping_hole_8018EB90[2] = {
    { 32, 0, 0, 4276, 1, -6109, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_shelter_b3_dumping_hole_8018EBB0[5] = {
    { 44, 44, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 1, { 0, 0 }, &Actor04400_D107E4 },
    { 70, 70, AREA_RESOURCE_FILE_GROUP_BASE_20, 2, { 0, 0 }, &D_actor_207000_801575F0 },
    { 71, 71, AREA_RESOURCE_FILE_GROUP_BASE_20, 1, { 0, 0 }, &D_actor_207000_80151E60 },
    { 103, 421, AREA_RESOURCE_FILE_GROUP_BASE_30, 1, { 0, 0 }, D_actor_342100_80164B78 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_dumping_hole_8018EBEC[5] = {
    { 44, 1, 0, 0x38C0, 4000, -5888, 2048, 0, 0, 2, 0 },
    { 70, 1, 0, 0x2B5C, 0, -8200, 2048, 0, 2, 4, 0 },
    { 71, 1, 1, 5000, 0, -5900, 2048, 0, 3, 5, 0 },
    { 103, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b3_dumping_hole_8018EC3C[13] = {
    { NULL, NULL },
    { D_shelter_b3_dumping_hole_8018EAEC, D_shelter_b3_dumping_hole_8018EB3C },
    { D_shelter_b3_dumping_hole_8018EBEC, D_shelter_b3_dumping_hole_8018EBB0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b3_dumping_hole_8018EB90, D_shelter_b3_dumping_hole_8018EB78 },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionTrigger D_shelter_b3_dumping_hole_8018ECA4[10] = {
    { NULL, NULL, NULL, { 0x4BD0, -48, -4688, 0 }, { { -688, 0, -624, 0 }, { 688, 0, -624, 0 }, { -688, 0, 624, 0 }, { 688, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 927, WORLD_COLLISION_TRIGGER_ACTION_WARP, 40, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3820, -64, -0x3A40, 0 }, { { -688, 0, -2576, 0 }, { 688, 0, -2576, 0 }, { -688, 0, 2576, 0 }, { 688, 0, 2576, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2660, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4B61, -64, -3760, 0 }, { { -1008, 0, -832, 0 }, { 1008, 0, -832, 0 }, { -1008, 0, 832, 0 }, { 1008, 0, 832, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4224, -64, -5808, 0 }, { { -464, 0, -1184, 0 }, { 464, 0, -1184, 0 }, { -464, 0, 1184, 0 }, { 464, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { 4075, 0, -402, 0 }, 1267, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8816, -64, -3984, 0 }, { { -800, 0, -1008, 0 }, { 800, 0, -432, 0 }, { -800, 0, 432, 0 }, { 800, 0, 1008, 0 } }, { 0, 4098, 0, 0 }, { 399, 0, -4075, 0 }, 1286, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8816, -64, -7824, 0 }, { { -816, 0, -224, 0 }, { 816, 0, -1216, 0 }, { -816, 0, 1216, 0 }, { 816, 0, 224, 0 } }, { 0, 4099, 0, 0 }, { 14, 0, 4094, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2D70, -64, -3808, 0 }, { { -2080, 0, -512, 0 }, { 2080, 0, -512, 0 }, { -2080, 0, 512, 0 }, { 2080, 0, 512, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2DB0, -64, -8272, 0 }, { { -2000, 0, -368, 0 }, { 2000, 0, -368, 0 }, { -2000, 0, 368, 0 }, { 2000, 0, 368, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 2031, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8608, -64, -5888, 0 }, { { -688, 0, -2576, 0 }, { 688, 0, -2576, 0 }, { -688, 0, 2576, 0 }, { 688, 0, 2576, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 2660, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 20, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -496, -64, -6080, 0 }, { { -1504, 0, -4336, 0 }, { 1504, 0, -4336, 0 }, { -1504, 0, 4336, 0 }, { 1504, 0, 4336, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 4579, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b3_dumping_hole_8018EF9C[8] = {
    { NULL, NULL, NULL, { 0x4BD0, -48, -4688, 0 }, { { -688, 0, -624, 0 }, { 688, 0, -624, 0 }, { -688, 0, 624, 0 }, { 688, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 927, WORLD_COLLISION_TRIGGER_ACTION_WARP, 40, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6334, -64, -5984, 0 }, { { -144, 0, -2576, 0 }, { 144, 0, -2576, 0 }, { -144, 0, 2576, 0 }, { 144, 0, 2576, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 2572, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4B61, -64, -3760, 0 }, { { -1008, 0, -832, 0 }, { 1008, 0, -832, 0 }, { -1008, 0, 832, 0 }, { 1008, 0, 832, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4224, -64, -5808, 0 }, { { -464, 0, -1184, 0 }, { 464, 0, -1184, 0 }, { -464, 0, 1184, 0 }, { 464, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { 4075, 0, -402, 0 }, 1267, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8816, -64, -3984, 0 }, { { -800, 0, -1008, 0 }, { 800, 0, -432, 0 }, { -800, 0, 432, 0 }, { 800, 0, 1008, 0 } }, { 0, 4098, 0, 0 }, { 399, 0, -4075, 0 }, 1286, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8816, -64, -7824, 0 }, { { -816, 0, -224, 0 }, { 816, 0, -1216, 0 }, { -816, 0, 1216, 0 }, { 816, 0, 224, 0 } }, { 0, 4099, 0, 0 }, { 14, 0, 4094, 0 }, 1459, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2D70, -64, -3808, 0 }, { { -2080, 0, -512, 0 }, { 2080, 0, -512, 0 }, { -2080, 0, 512, 0 }, { 2080, 0, 512, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2DB0, -64, -8272, 0 }, { { -2000, 0, -368, 0 }, { 2000, 0, -368, 0 }, { -2000, 0, 368, 0 }, { 2000, 0, 368, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 2031, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_b3_dumping_hole_8018F1FC[38] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b3_dumping_hole_8018F1FC) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 208, 208, 208, 208 } },
    { .color = { 208, 208, 208, 208 } },
    { .color = { 308, 311, 308, 309 } },
    { .color = { 208, 208, 208, 208 } },
    { .color = { 208, 208, 208, 208 } },
    { .color = { 310, 310, 310, 310 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 537, 453, 359, 472 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 516, 512, 516, 514 } },
    { .color = { 246, 249, 246, 247 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 25, 288, 287, 189 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 205, 205, 205, 205 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCoordRoomAmbientEntry D_shelter_b3_dumping_hole_8018F32C[38] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b3_dumping_hole_8018F32C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 616, 618, 617, 617 } },
    { .color = { 617, 618, 618, 617 } },
    { .color = { 616, 617, 617, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 615, 615, 615, 615 } },
    { .color = { 615, 615, 615, 615 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b3_dumping_hole_8018F45C = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b3_dumping_hole_8018F468[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b3_dumping_hole_8018F470[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b3_dumping_hole_8018F478[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b3_dumping_hole_8018F45C },
};

WorldCollisionSurfaceProperties* D_shelter_b3_dumping_hole_8018F480[9] = {
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F470,
    D_shelter_b3_dumping_hole_8018F478,
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F468,
    D_shelter_b3_dumping_hole_8018F468,
    NULL,
};

u8 D_shelter_b3_dumping_hole_8018F4A4[4] = {
    0,
    74,
    201,
    8,
};

Task* D_shelter_b3_dumping_hole_8018F4A8 = NULL;

Task* D_shelter_b3_dumping_hole_8018F4AC = NULL;

u16 D_shelter_b3_dumping_hole_8018F4B0[2] = {
    0,
    0xDF0D,
};

static CapCommandRef* CapCaption_Data_8015E650 = NULL;

static TextGlyphCell* CapCaption_Data_8015E654 = NULL;

static CapSequenceRecord* CapCaption_Data_8015E658 = NULL;

static s16 CapCaption_Data_8015E65C = 0;

static s16 CapCaption_Data_8015E65E = 0;

static s16 CapCaption_Data_8015E660 = 0;

static s16 CapCaption_Data_8015E662 = 0;

static s16 CapCaption_Data_8015E664 = 0;

static s16 CapCaption_Data_8015E666 = 0;

static u16 CapCaption_Data_8015E668 = 0;

static u16 CapCaption_Data_8015E66A = 0;

static u8 CapCaption_Data_8015E66C[4] = {
    0,
    19,
    111,
    0,
};

u16 D_shelter_b3_dumping_hole_8018F4D4[2] = {
    0,
    0xD086,
};

s32 D_shelter_b3_dumping_hole_8018F4D8;

static void func_shelter_b3_dumping_hole_8018098C(Task* task);

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Draws a signed rubble tumble step in -127..127, in 4096 units per turn.
///
/// Consumes two LCG draws in order: magnitude first, then bit 15 for the sign.
static inline s32 _shelterB3DumpingHoleRandomDebrisSpin(void)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_SPIN_MAGNITUDE_MASK = 0x7F,
        SHELTER_B3_DUMPING_HOLE_SPIN_SIGN_BIT       = 0x8000,
    };
    s32 spin = DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SPIN_MAGNITUDE_MASK;
    if (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SPIN_SIGN_BIT) {
        spin = -spin;
    }
    return spin;
}

/// Tests a centre-relative pixel position against the inclusive 320x240 screen bounds.
///
/// Returns 1 outside [-160, 160] by [-120, 120], otherwise 0; tests the
/// point alone, irrespective of a primitive's extent.
static inline u16 _shelterB3DumpingHoleIsOffscreen(s16 screenX, s16 screenY)
{
    if (screenX < -SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_WIDTH) {
        return 1;
    }
    if (screenX > SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_WIDTH) {
        return 1;
    }
    if (screenY < -SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_HEIGHT) {
        return 1;
    }
    if (screenY > SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_HEIGHT) {
        return 1;
    }
    return 0;
}

/// Queues a raw-texture, additive billboard at a coordinate origin, or returns 1 if culled.
///
/// `width` and `height` are texels and unit-scale pixels; `scale` is Q12
/// (`ONE` = 1.0). UV endpoints narrow to GPU bytes. `vramX` is in VRAM
/// words and `vramY` in lines; their page bases select a 4-bit texture.
/// `clut` is a packed GPU palette selector. A nonzero `depthOverride` replaces
/// SZ3/4 for both sorting and the negative-depth test; zero uses projection.
/// Returns 0 after queuing in the current packet arena. Culls the origin
/// against the centre-relative screen bounds, without testing GTE flags.
/// The coordinate is composed here; the caller marks later movement dirty.
static u16 _shelterB3DumpingHoleDrawSprite(GfxCoord* coord, s16 width, s16 height, s16 u,
                                           s16 v, s16 vramX, s16 vramY, s16 scale,
                                           s16 clut, s32 depthOverride)
{
    SVECTOR   origin;
    s32       packedScreenXY;
    s32       depth;
    s32       cullDepth;
    u16       culled;
    POLY_FT4* prim;
    u16       widthPixels;
    u16       heightPixels;
    s16       screenX;
    s16       screenY;

    actorRenderComposeCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    origin.vx = origin.vy = origin.vz = 0;
    gte_ldv0(&origin);
    gte_rtps();
    gte_stsxy(&packedScreenXY);
    gte_stszotz(&depth);
    screenY = packedScreenXY >> 16;
    screenX = packedScreenXY;
    if (depthOverride != 0) {
        depth = depthOverride;
    }
    cullDepth = depth;
    if (screenX < -SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_WIDTH) {
        culled = 1;
    } else if (screenX > SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_WIDTH || screenY < -SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_HEIGHT || screenY > SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_HEIGHT || cullDepth < 0) {
        culled = 1;
    } else {
        culled = 0;
    }
    if (culled) {
        return 1;
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    setSemiTrans(prim, 1);
    setShadeTex(prim, 1);
    widthPixels  = width * scale / ONE;
    heightPixels = height * scale / ONE;
    setXY4(prim, screenX - widthPixels / 2, screenY - heightPixels / 2, screenX + widthPixels / 2, screenY - heightPixels / 2, screenX - widthPixels / 2, screenY + heightPixels / 2,
           screenX + widthPixels / 2, screenY + heightPixels / 2);
    setUV4(prim, u, v, u + width - 1, v, u, v + height - 1, u + width - 1, v + height - 1);
    prim->clut  = clut;
    prim->tpage = getTPage(SHELTER_B3_DUMPING_HOLE_TEXTURE_4BIT, GPU_BLEND_ADD, vramX / SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_WORDS * SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_WORDS, vramY / SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_LINES * SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_LINES);
    addPrim(gGpuCurrentOt + (depth >> SHELTER_B3_DUMPING_HOLE_OT_DEPTH_SHIFT), prim);
    return 0;
}

/// Runs a debris-ring sprite from its seeded world position until its animation ends.
///
/// Waits for the debris director's rise signal, starts at frame 5 after a
/// 0-7 tick delay, and moves upward 10-17 world units per animation tick.
/// Uses the debris duration table and the seed's Q12 scale. Removal signals,
/// frame-table completion and drawing cull each kill the task. The spawner
/// provides the task-owned work block; the debris director must stay live.
static void _shelterB3DumpingHoleDebrisSpriteTask(Task* task)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_SETUP       = 0,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_WAIT_SIGNAL = 1,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_WAIT_DELAY  = 2,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_ANIMATE     = 3,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_FIRST_FRAME = 5,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_RISE_SPEED  = 10,
    };
    _ShelterB3DumpingHoleSpriteWork*      work      = task->work;
    GfxCoord*                             coord     = task->extra.coordBody->coord;
    _ShelterB3DumpingHoleDebrisEventWork* eventWork = D_shelter_b3_dumping_hole_8018F4A8->work;

    if (eventWork->debrisSpriteSignal == SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_REMOVE) {
        taskKill(task);
        return;
    }

    switch (task->state) {
        case SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_SETUP:
            coord->parent     = &gGfxViewCoord;
            coord->coord.t[0] = work->seed.pos.vx;
            coord->coord.t[1] = work->seed.pos.vy;
            coord->coord.t[2] = work->seed.pos.vz;
            task->state++;
            return;
        case SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_WAIT_SIGNAL:
            if (eventWork->debrisSpriteSignal != SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_RISE) {
                return;
            }
            work->frame     = SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_FIRST_FRAME;
            work->vel.vx    = 0;
            work->vel.vz    = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->vel.vy    = -SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_RISE_SPEED - ((gRandomLcgState >> 16) & SHELTER_B3_DUMPING_HOLE_SPRITE_JITTER_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->delay     = (gRandomLcgState >> 16) & SHELTER_B3_DUMPING_HOLE_SPRITE_JITTER_MASK;
            task->state++;
            return;
        case SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_WAIT_DELAY:
            if (work->delay == 0) {
                task->state = SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_ANIMATE;
            } else {
                work->delay--;
            }
            return;
        case SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITE_ANIMATE: {
            SHELTER_B3_DUMPING_HOLE_ADVANCE_SPRITE_FRAME(task, work, D_shelter_b3_dumping_hole_80188154);
            break;
        }
        default:
            return;
    }

    coord->coord.t[1] += work->vel.vy;
    if (_shelterB3DumpingHoleDrawSprite(
            coord,
            D_shelter_b3_dumping_hole_801880B8[work->frame].w,
            D_shelter_b3_dumping_hole_801880B8[work->frame].h,
            D_shelter_b3_dumping_hole_801880B8[work->frame].u,
            D_shelter_b3_dumping_hole_801880B8[work->frame].v,
            D_shelter_b3_dumping_hole_801880B8[work->frame].vramX,
            D_shelter_b3_dumping_hole_801880B8[work->frame].vramY,
            work->seed.scale, SHELTER_B3_DUMPING_HOLE_SPRITE_CLUT, 0) != 0) {
        taskKill(task);
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Runs a dark rising sprite sampled once from an actor coordinate plus a world offset.
///
/// The source coordinate in spawn argument 2 must live through setup. After
/// 20-27 delay ticks, frames 0-11 rise 10-17 world units per tick at unit
/// scale and fixed sorting depth 1000/16. The actor duration table controls
/// frame holds. The director's stop flag, final frame and offscreen origin
/// kill the task. Unlike the raw-texture event sprites, this quad modulates
/// its texture with intensity 32 and does not use projected depth.
static void _shelterB3DumpingHoleActorSpriteTask(Task* task)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_SETUP      = 0,
        SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_WAIT_DELAY = 1,
        SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_ANIMATE    = 2,
        SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_DELAY      = 20,
        SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_RISE_SPEED = 10,
        SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_SORT_DEPTH = 1000,
        SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_INTENSITY  = 0x20,
    };
    _ShelterB3DumpingHoleSpriteWork* work  = task->work;
    GfxCoord*                        coord = task->extra.coordBody->coord;
    SVECTOR                          sourceWorldPos;
    SVECTOR                          origin;
    DVECTOR                          screenPos;
    POLY_FT4*                        prim;
    u16                              offscreen;
    u16                              widthPixels;
    u16                              heightPixels;
    s16                              screenX;
    s16                              screenY;
    s32                              projectedY;
    s16                              width;
    s16                              height;
    s16                              u;
    s16                              v;
    s16                              vramX;
    s16                              vramY;
    s16                              scale        = ONE;
    s32                              sortingDepth = SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_SORT_DEPTH;

    if (((_ShelterB3DumpingHoleDebrisEventWork*)D_shelter_b3_dumping_hole_8018F4A8->work)->actorSpritesStop == true) {
        taskKill(task);
        return;
    }

    switch (task->state) {
        case SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_SETUP:
            coord->parent = &gGfxViewCoord;
            gfxComposeNodeWorldTransform(task->spawnArg2.pointer, &coord->coord, &sourceWorldPos);
            coord->coord.t[0] = sourceWorldPos.vx + work->offset.vx;
            coord->coord.t[1] = sourceWorldPos.vy + work->offset.vy;
            coord->coord.t[2] = sourceWorldPos.vz + work->offset.vz;
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->vel.vy      = -SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_RISE_SPEED - ((gRandomLcgState >> 16) & SHELTER_B3_DUMPING_HOLE_SPRITE_JITTER_MASK);
            work->vel.vx      = 0;
            work->vel.vz      = 0;
            work->frame       = 0;
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->delay       = ((gRandomLcgState >> 16) & SHELTER_B3_DUMPING_HOLE_SPRITE_JITTER_MASK) + SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_DELAY;
            task->state++;
            return;
        case SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_WAIT_DELAY:
            if (work->delay == 0) {
                task->state = SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_ANIMATE;
            } else {
                work->delay--;
            }
            return;
        case SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_ANIMATE: {
            SHELTER_B3_DUMPING_HOLE_ADVANCE_SPRITE_FRAME(task, work, D_shelter_b3_dumping_hole_8018816C);
            break;
        }
        default:
            return;
    }

    coord->coord.t[1] += work->vel.vy;
    width              = D_shelter_b3_dumping_hole_801880B8[work->frame].w;
    height             = D_shelter_b3_dumping_hole_801880B8[work->frame].h;
    u                  = D_shelter_b3_dumping_hole_801880B8[work->frame].u;
    v                  = D_shelter_b3_dumping_hole_801880B8[work->frame].v;
    vramX              = D_shelter_b3_dumping_hole_801880B8[work->frame].vramX;
    vramY              = D_shelter_b3_dumping_hole_801880B8[work->frame].vramY;
    // Project the origin, then draw a modulated quad at the fixed ordering depth.
    actorRenderComposeCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    origin.vx = origin.vy = origin.vz = 0;
    gte_ldv0(&origin);
    gte_rtps();
    gte_stsxy(&screenPos);
    screenX    = screenPos.vx;
    projectedY = screenPos.vy;
    screenY    = projectedY;
    offscreen  = _shelterB3DumpingHoleIsOffscreen(screenX, projectedY);
    if (offscreen) {
        taskKill(task);
        return;
    }
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    setSemiTrans(prim, 1);
    prim->r0 = prim->g0 = prim->b0 = SHELTER_B3_DUMPING_HOLE_ACTOR_SPRITE_INTENSITY;
    widthPixels                    = width * scale / ONE;
    heightPixels                   = height * scale / ONE;
    setXY4(prim, screenX - widthPixels / 2, screenY - heightPixels / 2, screenX + widthPixels / 2, screenY - heightPixels / 2, screenX - widthPixels / 2, screenY + heightPixels / 2, screenX + widthPixels / 2, screenY + heightPixels / 2);
    setUV4(prim, u, v, u + width - 1, v, u, v + height - 1, u + width - 1, v + height - 1);
    prim->clut  = SHELTER_B3_DUMPING_HOLE_SPRITE_CLUT;
    prim->tpage = getTPage(SHELTER_B3_DUMPING_HOLE_TEXTURE_4BIT, GPU_BLEND_ADD, (vramX / SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_WORDS) * SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_WORDS, (vramY / SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_LINES) * SHELTER_B3_DUMPING_HOLE_TEXTURE_PAGE_LINES);
    addPrim(&gGpuCurrentOt[sortingDepth >> SHELTER_B3_DUMPING_HOLE_OT_DEPTH_SHIFT], prim);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Runs a rising sprite sampled once from the player model, with a selected Z drift.
///
/// Allocates its task-owned work at setup and samples the coordinate lent in
/// spawn argument 2. Spawn argument 1 supplies signed world units per tick
/// for Z drift, jittered by 0 or 1 away from zero; zero chooses -1, 0 or 1.
/// After 0-7 delay ticks it rises 15-22 units per tick through frames 0-11
/// at unit Q12 scale. The player duration table ends the animation; the
/// director's stop flag also removes it. Drawing cull alone does not kill it.
static void _shelterB3DumpingHolePlayerSpriteTask(Task* task)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_SETUP      = 0,
        SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_WAIT_DELAY = 1,
        SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_ANIMATE    = 2,
        SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_RISE_SPEED = 15,
    };
    _ShelterB3DumpingHoleSpriteWork* work  = task->work;
    GfxCoord*                        coord = task->extra.coordBody->coord;
    SVECTOR                          sourceWorldPos;
    s32                              baseDriftZ;
    u32                              riseRandomState;
    u32                              directionRandomState;
    s32                              currentDriftZ;
    s16                              randomDriftZ;
    s16                              biasedDriftZ;

    if (((_ShelterB3DumpingHoleDebrisEventWork*)D_shelter_b3_dumping_hole_8018F4A8->work)->playerSpritesStop == true) {
        taskKill(task);
        return;
    }

    switch (task->state) {
        case SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_SETUP:
            coord->parent = &gGfxViewCoord;
            gfxComposeNodeWorldTransform(task->spawnArg2.pointer, &coord->coord, &sourceWorldPos);
            coord->coord.t[0] = sourceWorldPos.vx;
            coord->coord.t[1] = sourceWorldPos.vy;
            coord->coord.t[2] = sourceWorldPos.vz;
            task->work        = memMalloc(SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES, false);
            if (task->work == NULL) {
                taskKill(task);
                return;
            }
            work = task->work;
            memFillBytes(work, 0, SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES);
            work->vel.vy     = -0xA;
            work->vel.vx     = 0;
            work->vel.vz     = 0;
            work->seed.scale = ONE;
            work->vel.vx     = 0;
            riseRandomState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->vel.vy     = -SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_RISE_SPEED - ((riseRandomState >> 16) & SHELTER_B3_DUMPING_HOLE_SPRITE_JITTER_MASK);
            baseDriftZ       = task->spawnArg1.value;
            gRandomLcgState  = riseRandomState;
            // Retain the halfword wrap and the original random-draw order for Z drift.
            if (baseDriftZ == 0) {
                directionRandomState = riseRandomState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                currentDriftZ        = work->vel.vz;
                gRandomLcgState      = directionRandomState;
                if ((directionRandomState >> 16) & 1) {
                    gRandomLcgState = directionRandomState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    randomDriftZ    = currentDriftZ + ((gRandomLcgState >> 16) & 1);
                } else {
                    gRandomLcgState = directionRandomState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    randomDriftZ    = currentDriftZ - ((gRandomLcgState >> 16) & 1);
                }
                work->vel.vz = randomDriftZ;
            } else {
                if (baseDriftZ < 0) {
                    gRandomLcgState = riseRandomState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    biasedDriftZ    = (u16)work->vel.vz + ((u16)task->spawnArg1.value - ((gRandomLcgState >> 16) & 1));
                } else {
                    gRandomLcgState = riseRandomState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    biasedDriftZ    = (u16)work->vel.vz + ((u16)task->spawnArg1.value + ((gRandomLcgState >> 16) & 1));
                }
                work->vel.vz = biasedDriftZ;
            }
            work->frame     = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->delay     = (gRandomLcgState >> 16) & SHELTER_B3_DUMPING_HOLE_SPRITE_JITTER_MASK;
            task->state++;
            return;
        case SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_WAIT_DELAY:
            if (work->delay == 0) {
                task->state = SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_ANIMATE;
            } else {
                work->delay--;
            }
            return;
        case SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_ANIMATE: {
            SHELTER_B3_DUMPING_HOLE_ADVANCE_SPRITE_FRAME(task, work, D_shelter_b3_dumping_hole_80188184);
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            _shelterB3DumpingHoleDrawSprite(
                coord,
                D_shelter_b3_dumping_hole_801880B8[work->frame].w,
                D_shelter_b3_dumping_hole_801880B8[work->frame].h,
                D_shelter_b3_dumping_hole_801880B8[work->frame].u,
                D_shelter_b3_dumping_hole_801880B8[work->frame].v,
                D_shelter_b3_dumping_hole_801880B8[work->frame].vramX,
                D_shelter_b3_dumping_hole_801880B8[work->frame].vramY,
                work->seed.scale, SHELTER_B3_DUMPING_HOLE_SPRITE_CLUT, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        }
    }
}

#undef SHELTER_B3_DUMPING_HOLE_ADVANCE_SPRITE_FRAME

/// Places and lights one rubble model as a child of the debris event director.
///
/// Borrows the ActorTransform in spawn argument 2 for this setup call.
/// The model borrows lighting matrices from a new task-owned work block;
/// its primitive buffer is allocated here because the descriptor skips it.
/// Allocation failure kills the task and returns without setting up the model.
static void _shelterB3DumpingHoleInitDebrisModel(Task* task)
{
    _ShelterB3DumpingHoleDebrisModelWork* work;
    TmdObject*                            model;
    GfxCoord*                             coord;
    ActorTransform*                       placement;
    VECTOR                                worldPos;
    TmdObject*                            lightingModel;

    model      = task->extra.tmd;
    placement  = task->spawnArg2.pointer;
    coord      = model->coords;
    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    memFillBytes(work, 0, sizeof(*work));
    coord->parent          = &gGfxViewCoord;
    task->extra.tmd->flags = 0;
    tmdAllocPrimitiveBuffer(model);
    model->lightMtx   = &work->lightMtx;
    model->colorMtx   = &work->colorMtx;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(&coord->coord, placement->rot.vy, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixX(&coord->coord, placement->rot.vx, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&coord->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    taskReparent(D_shelter_b3_dumping_hole_8018F4A8, task);
    actorRenderComposeCoord(coord);
    // Reload the model after reparenting and composing its coordinate.
    lightingModel = task->extra.tmd;
    worldPos.vx   = lightingModel->coords->workm.t[0];
    worldPos.vy   = task->extra.tmd->coords->workm.t[1];
    worldPos.vz   = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(lightingModel, &worldPos, 0, 3);
}

/// Waits for the debris signal, launches a rubble model away from screen centre, then tumbles it.
///
/// Spawn argument 1 selects the +X speed tier (0 fast, 1 medium, 2 slow);
/// argument 2 lends its ActorTransform through setup. Launch projects the
/// world origin, removes out-of-bounds or negative-depth pieces, and uses
/// the screen angle for YZ spread. Velocities are world units per tick and
/// spin uses 4096 units per turn. In flight downward speed grows by five
/// per tick; X and Y rotations are applied, while Z is advanced only.
/// The director's remove signal kills the task in any state.
static void _shelterB3DumpingHoleDebrisModelTask(Task* task)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_SETUP        = 0,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_WAIT         = 1,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_LAUNCH       = 2,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_FLY          = 3,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_FAST               = 0,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_MEDIUM             = 1,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SLOW               = 2,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_Z_SPREAD           = 17,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_Y_SPREAD           = 5,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_FAST_X_SPEED       = 50,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_MEDIUM_X_SPEED     = 40,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SLOW_X_SPEED       = 30,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SPEED_JITTER_MASK  = 0x1F,
        SHELTER_B3_DUMPING_HOLE_DEBRIS_SPREAD_JITTER_MASK = 7,
    };
    _ShelterB3DumpingHoleDebrisModelWork* work   = task->work;
    u16                                   signal = ((_ShelterB3DumpingHoleDebrisEventWork*)D_shelter_b3_dumping_hole_8018F4A8->work)->debrisModelSignal;
    GfxCoord*                             coord  = task->extra.tmd->coords;
    GfxCoord*                             movingCoord;
    SVECTOR                               worldPos;
    s32                                   packedScreenXY;
    s32                                   depth;
    s16                                   screenX;
    s16                                   screenY;
    s16                                   spreadAngle;
    s32                                   spinX;
    s32                                   spinY;
    s32                                   spinZ;

    if (signal == SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_REMOVE) {
        taskKill(task);
        return;
    }
    switch (task->state) {
        case SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_SETUP:
            _shelterB3DumpingHoleInitDebrisModel(task);
            task->state++;
            return;
        case SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_WAIT:
            if (signal == SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_LAUNCH) {
                task->state = SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_LAUNCH;
            }
            return;
        case SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_LAUNCH:
            worldPos.vx = coord->workm.t[0];
            worldPos.vy = coord->workm.t[1];
            worldPos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&worldPos);
            gte_rtps();
            gte_stsxy(&packedScreenXY);
            gte_stszotz(&depth);
            screenX = packedScreenXY;
            screenY = packedScreenXY >> 16;
            SHELTER_B3_DUMPING_HOLE_CULL_DEBRIS_MODEL(task, screenX, screenY, depth);
            // Spread in YZ from the projected centre while the tier drives motion along +X.
            spreadAngle  = ratan2(screenY, screenX);
            work->vel.vz = rcos(spreadAngle) * ((DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_DEBRIS_SPREAD_JITTER_MASK) + SHELTER_B3_DUMPING_HOLE_DEBRIS_Z_SPREAD) / ONE;
            work->vel.vy = rsin(spreadAngle) * ((DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_DEBRIS_SPREAD_JITTER_MASK) + SHELTER_B3_DUMPING_HOLE_DEBRIS_Y_SPREAD) / ONE;
            switch (task->spawnArg1.value) {
                case SHELTER_B3_DUMPING_HOLE_DEBRIS_FAST:
                    work->vel.vx = (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_DEBRIS_SPEED_JITTER_MASK) + SHELTER_B3_DUMPING_HOLE_DEBRIS_FAST_X_SPEED;
                    break;
                case SHELTER_B3_DUMPING_HOLE_DEBRIS_MEDIUM:
                    work->vel.vx = (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_DEBRIS_SPEED_JITTER_MASK) + SHELTER_B3_DUMPING_HOLE_DEBRIS_MEDIUM_X_SPEED;
                    break;
                case SHELTER_B3_DUMPING_HOLE_DEBRIS_SLOW:
                    work->vel.vx = (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_DEBRIS_SPEED_JITTER_MASK) + SHELTER_B3_DUMPING_HOLE_DEBRIS_SLOW_X_SPEED;
                    break;
            }
            spinX         = _shelterB3DumpingHoleRandomDebrisSpin();
            work->spin.vx = spinX;
            spinY         = _shelterB3DumpingHoleRandomDebrisSpin();
            work->spin.vy = spinY;
            spinZ         = _shelterB3DumpingHoleRandomDebrisSpin();
            work->spin.vz = spinZ;
            work->fall    = 0;
            task->state++;
            return;
        case SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_FLY:
            work->rot.vx            += work->spin.vx;
            work->rot.vy            += work->spin.vy;
            work->rot.vz            += work->spin.vz;
            work->fall              += SHELTER_B3_DUMPING_HOLE_DEBRIS_MODEL_GRAVITY;
            movingCoord              = task->extra.tmd->coords;
            movingCoord->parent      = &gGfxViewCoord;
            movingCoord->coord.t[0] += work->vel.vx;
            movingCoord->coord.t[1] += work->vel.vy + work->fall;
            movingCoord->coord.t[2] += work->vel.vz;
            gfxRotMatrixY(&movingCoord->coord, work->rot.vy, GRAPHICS_ROTATION_REPLACE);
            gfxRotMatrixX(&movingCoord->coord, work->rot.vx, GRAPHICS_ROTATION_COMPOSE);
            movingCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
    }
}

#undef SHELTER_B3_DUMPING_HOLE_CULL_DEBRIS_MODEL

static void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0)
{
    _ShelterB3DumpingHoleDebrisEventWork* work = arg0->work;
    union {
        AnimationPlayRequest anim;
        ActorCommand         loc;
    } msg;
    u8 area;

    if (work->player != NULL) {
        taskMessageDispatch(work->player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0);
    }
    switch (work->playerCommand) {
        case 0:
            break;
        case 1:
            switch (work->playerStep) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(work->player, 0x3E9, D_shelter_b3_dumping_hole_8018819C, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->player, 0x3F2, &D_shelter_b3_dumping_hole_8018819C[6], 0);
                    work->playerStep++;
                    return;
                case 1:
                    if (taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->playerTimer = 0;
                        work->playerStep++;
                    }
                    return;
                case 2:
                    if (++work->playerTimer < 6) {
                        return;
                    }
                    {
                        _ShelterB3DumpingHoleDebrisEventWork* w2       = arg0->work;
                        s32                                   weaponId = gPlayerStatus.weapon;
                        msg.anim.source.index                          = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                        msg.anim.animationId                           = 0x2F;
                        msg.anim.blend                                 = ANIMATION_BLEND_INTERPOLATE;
                        msg.anim.blendFrames                           = 0xA;
                        msg.anim.enableWorldCollision                  = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(w2->player, ANIMATION_MESSAGE_PLAY, &msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 2: {
            _ShelterB3DumpingHoleDebrisEventWork* w2       = arg0->work;
            s32                                   weaponId = gPlayerStatus.weapon;
            msg.anim.source.index                          = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.animationId                           = 0x32;
            msg.anim.blend                                 = ANIMATION_BLEND_RESET;
            msg.anim.blendFrames                           = 0;
            msg.anim.enableWorldCollision                  = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(w2->player, ANIMATION_MESSAGE_PLAY, &msg, 0);
        } break;
        case 3:
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
        case 4:
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->player, 0x3E9, &D_shelter_b3_dumping_hole_801881CC, 0);
            {
                _ShelterB3DumpingHoleDebrisEventWork* w2       = arg0->work;
                s32                                   weaponId = gPlayerStatus.weapon;
                msg.anim.source.index                          = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                msg.anim.animationId                           = 9;
                msg.anim.blend                                 = ANIMATION_BLEND_RESET;
                msg.anim.blendFrames                           = 0;
                msg.anim.enableWorldCollision                  = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(w2->player, ANIMATION_MESSAGE_PLAY, &msg, 0);
            }
            break;
        case 5:
            switch (work->playerStep) {
                case 0:
                    msg.loc.context.loc.stage = gGameSession->location.loc.stage;
                    area                      = gGameSession->location.loc.area;
                    msg.loc.command           = 1;
                    msg.loc.context.loc.area  = area;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
                    work->playerTimer = 0;
                    work->playerStep++;
                    return;
                case 1:
                    if (++work->playerTimer < 0x10) {
                        return;
                    }
                    {
                        _ShelterB3DumpingHoleDebrisEventWork* w2       = arg0->work;
                        s32                                   weaponId = gPlayerStatus.weapon;
                        msg.anim.source.index                          = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                        msg.anim.animationId                           = 0x30;
                        msg.anim.blend                                 = ANIMATION_BLEND_INTERPOLATE;
                        msg.anim.blendFrames                           = 0xA;
                        msg.anim.enableWorldCollision                  = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(w2->player, ANIMATION_MESSAGE_PLAY, &msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 6: {
            _ShelterB3DumpingHoleDebrisEventWork* w2       = arg0->work;
            s32                                   weaponId = gPlayerStatus.weapon;
            msg.anim.source.index                          = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.animationId                           = 0x33;
            msg.anim.blend                                 = ANIMATION_BLEND_INTERPOLATE;
            msg.anim.blendFrames                           = 0xA;
            msg.anim.enableWorldCollision                  = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(w2->player, ANIMATION_MESSAGE_PLAY, &msg, 0);
        }
            taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, 0x20, 0);
            break;
        case 7: {
            _ShelterB3DumpingHoleDebrisEventWork* w2       = arg0->work;
            s32                                   weaponId = gPlayerStatus.weapon;
            msg.anim.source.index                          = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.animationId                           = 0x31;
            msg.anim.blend                                 = ANIMATION_BLEND_INTERPOLATE;
            msg.anim.blendFrames                           = 0xA;
            msg.anim.enableWorldCollision                  = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(w2->player, ANIMATION_MESSAGE_PLAY, &msg, 0);
        } break;
    }
    work->playerCommand = 0;
}

static void func_shelter_b3_dumping_hole_8017F1B0(Task* arg0)
{
    _ShelterB3DumpingHoleDebrisEventWork* work = arg0->work;
    ActorCommand                          msg;
    _ShelterB3DumpingHoleSpriteSeed       seed;
    _ShelterB3DumpingHoleDebrisPlacement* placement;
    _ShelterB3DumpingHoleDebrisPlacement* ringed;
    u16                                   i;
    u8                                    area;

    switch (work->sceneCommand) {
        case 0:
            break;
        case 1:
            switch (work->sceneStep) {
                case 0:
                    work->debrisModelSignal  = SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_LAUNCH;
                    work->debrisSpriteSignal = SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_RISE;
                    work->sceneTimer         = 0;
                    work->sceneStep++;
                    return;
                case 1:
                    if (++work->sceneTimer < 3) {
                        return;
                    }
                    taskMessageDispatch(((_ShelterB3DumpingHoleDebrisEventWork*)D_shelter_b3_dumping_hole_8018F4A8->work)->placement0Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    msg.context.loc.stage = gGameSession->location.loc.stage;
                    area                  = gGameSession->location.loc.area;
                    msg.command           = 2;
                    msg.context.loc.area  = area;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
                    break;
                default:
                    return;
            }
            break;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(work->placement0Actor, ACTOR_MESSAGE_PLACE, &work->pose, 0);
            msg.context.loc.stage = gGameSession->location.loc.stage;
            area                  = gGameSession->location.loc.area;
            msg.command           = 3;
            msg.context.loc.area  = area;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            break;
        case 4:
            msg.context.loc.stage = gGameSession->location.loc.stage;
            area                  = gGameSession->location.loc.area;
            msg.command           = 5;
            msg.context.loc.area  = area;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            break;
        case 5:
            TASK_MESSAGE_DISPATCH_POINTER(work->placement1Actor, ACTOR_MESSAGE_PLACE, &D_shelter_b3_dumping_hole_801881E4, 0);
            break;
        case 6:
            switch (work->sceneStep) {
                case 0:
                    work->actorSpritesStop = 1;
                    work->sceneStep++;
                    return;
                case 1:
                    for (i = 0; D_shelter_b3_dumping_hole_801881FC[i].pos.vx != SHELTER_B3_DUMPING_HOLE_TRANSFORM_END; i++) {
                        taskSpawnFromTable(D_shelter_b3_dumping_hole_80188BC8, 2, 0, &D_shelter_b3_dumping_hole_801881FC[i]);
                    }
                    for (i = 0; D_shelter_b3_dumping_hole_80188304[i].pos.vx != SHELTER_B3_DUMPING_HOLE_TRANSFORM_END; i++) {
                        taskSpawnFromTable(D_shelter_b3_dumping_hole_80188BC8, 3, 1, &D_shelter_b3_dumping_hole_80188304[i]);
                    }
                    for (i = 0; D_shelter_b3_dumping_hole_801884CC[i].transform.pos.vx != SHELTER_B3_DUMPING_HOLE_TRANSFORM_END; i++) {
                        placement = &D_shelter_b3_dumping_hole_801884CC[i];
                        taskSpawnFromTable(D_shelter_b3_dumping_hole_80188BC8, 4, 2, &placement->transform);
                        if (placement->spriteRing != 0) {
                            // One sprite at the piece, then one at each diagonal around it in the YZ plane.
                            ringed       = placement;
                            seed.pos.vx  = ringed->transform.pos.vx;
                            seed.pos.vy  = ringed->transform.pos.vy;
                            seed.pos.vz  = ringed->transform.pos.vz;
                            seed.scale   = ONE;
                            seed.field_A = 1;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.pos.vy = ringed->transform.pos.vy + SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET;
                            seed.pos.vz = ringed->transform.pos.vz + SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.pos.vy = ringed->transform.pos.vy + SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET;
                            seed.pos.vz = ringed->transform.pos.vz - SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.pos.vy = ringed->transform.pos.vy - SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET;
                            seed.pos.vz = ringed->transform.pos.vz + SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.pos.vy = ringed->transform.pos.vy - SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET;
                            seed.pos.vz = ringed->transform.pos.vz - SHELTER_B3_DUMPING_HOLE_SPRITE_RING_OFFSET;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                        }
                    }
                    break;
                default:
                    return;
            }
            break;
        case 7:
            work->debrisSpriteSignal = SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_REMOVE;
            work->debrisModelSignal  = SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_REMOVE;
            break;
    }
    work->sceneCommand = 0;
}

void func_shelter_b3_dumping_hole_8017F820(Task* arg0)
{
    _ShelterB3DumpingHoleDebrisEventWork* work;
    _ShelterB3DumpingHoleDebrisEventWork* w;
    Task*                                 t;
    _ShelterB3DumpingHoleDebrisEventWork* w2;
    AnimationBankCopyRequest              msg;
    AnimationPlayRequest                  anim;
    AnimationPlayRequest*                 p;
    s32                                   n;
    s32                                   weaponId;

    switch (arg0->state) {
        case 0:
            work       = memMalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->player                       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_shelter_b3_dumping_hole_8018F4A8 = arg0;
                work->placement0Actor              = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT))->task;
                work->placement1Actor              = sceneFindEnemyByPlaceKey((gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | (u16)(gGameSession->location.loc.area | (1 << ENEMY_PLACE_INDEX_SHIFT)))->task;
                work->debrisSpriteSignal           = SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_WAIT;
                work->debrisModelSignal            = SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_WAIT;
                work->field_4A                     = 0;
                work->playerSpritesStop            = 0;
                work->field_46                     = 0;
            }
            // Keep the placement-0 actor's starting pose (a quarter-turn yaw)
            // for the scene command that places it back.
            w                                                   = arg0->work;
            w->pose.pos.vx                                      = w->placement0Actor->extra.tmd->coords->coord.t[0];
            t                                                   = w->placement0Actor;
            w->pose.pos.vy                                      = t->extra.tmd->coords->coord.t[1];
            w->pose.pos.vz                                      = t->extra.tmd->coords->coord.t[2];
            w->pose.rot.vy                                      = 0x400;
            w->pose.rot.vx                                      = 0;
            w->pose.rot.vz                                      = 0;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xC;
            gStageSceneMusicEntry                               = 3;
            arg0->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                break;
            }
            if (capIsBusy() != 0) {
                break;
            }
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE || gPlayerStatus.coordMtx->t[0] < 0x36B1) {
                break;
            }
            w2 = arg0->work;
            n  = 0;
            while (D_shelter_b3_dumping_hole_801880A0[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.source.sets = &D_shelter_b3_dumping_hole_801880A0[0];
            msg.wordCount   = n & 0xFFFF;
            TASK_MESSAGE_DISPATCH_POINTER(w2->player, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &msg, 0);
            weaponId                  = gPlayerStatus.weapon;
            p                         = &anim;
            anim.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            p->animationId            = 1;
            p->blend                  = ANIMATION_BLEND_INTERPOLATE;
            p->blendFrames            = 0xA;
            anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &anim, 0);
            arg0->state++;
            break;
        case 2:
            func_800E8634(D_shelter_b3_dumping_hole_80188640, 0, D_shelter_b3_dumping_hole_80188A78);
            arg0->state++;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                WorldCollisionTrigger* collision = &D_shelter_b3_dumping_hole_8018ECA4[8];
                collision->flags                &= ~WORLD_COLLISION_TRIGGER_ENABLED;
                taskKill(arg0);
                return;
            }
            func_shelter_b3_dumping_hole_8017EDB8(arg0);
            func_shelter_b3_dumping_hole_8017F1B0(arg0);
            break;
    }
}

s16 func_shelter_b3_dumping_hole_8017FB70(void)
{
    if (gGameSession->location.loc.room == 2) {
        return 0;
    }
    return D_shelter_b3_dumping_hole_8018809C;
}

/// Fades the debris event's subtractive black overlay away at its spawn-selected rate.
///
/// Spawn argument 1's low halfword is subtracted from each signed channel
/// every tick after drawing; the low channel bytes are drawn, with red
/// also supplying blue. Setup allocates task-owned work and draws that
/// same tick. Negative red or the director's stop flag kills the task.
static void _shelterB3DumpingHoleFadeFromBlackTask(Task* task)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_FADE_SETUP          = 0,
        SHELTER_B3_DUMPING_HOLE_FADE_RUN            = 1,
        SHELTER_B3_DUMPING_HOLE_FADE_FULL_INTENSITY = 0xFF,
    };
    ScreenFadeWork*                       fade;
    ScreenFadeWork*                       allocatedFade;
    _ShelterB3DumpingHoleDebrisEventWork* eventWork;

    eventWork = D_shelter_b3_dumping_hole_8018F4A8->work;
    fade      = task->work;
    if (eventWork->fadeStop == true) {
        taskKill(task);
        return;
    }
    switch (task->state) {
        case SHELTER_B3_DUMPING_HOLE_FADE_SETUP:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = SHELTER_B3_DUMPING_HOLE_FADE_FULL_INTENSITY;
            fade->g      = SHELTER_B3_DUMPING_HOLE_FADE_FULL_INTENSITY;
            fade->r      = SHELTER_B3_DUMPING_HOLE_FADE_FULL_INTENSITY;
            task->state += 1;
            /* fallthrough */
        case SHELTER_B3_DUMPING_HOLE_FADE_RUN:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            fade->r -= (u16)task->spawnArg1.value;
            fade->g -= (u16)task->spawnArg1.value;
            fade->b -= (u16)task->spawnArg1.value;
            if (fade->r < 0) {
                taskKill(task);
            }
            break;
    }
}

/// Broadcasts an actor command in the active session's stage/area namespace.
///
/// The command uses its low 16 bits. The scene manager forwards the complete
/// stack record synchronously to its placed actors, so no pointer is retained.
static void _shelterB3DumpingHoleBroadcastActorCommand(s16 command)
{
    ActorCommand request;

    request.context.loc.stage = gGameSession->location.loc.stage;
    request.context.loc.area  = gGameSession->location.loc.area;
    request.command           = command;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &request, ACTOR_COMMAND_MESSAGE_APPLY);
}

void shelterB3DumpingHoleSpawnActorSprite(GfxCoord* sourceCoord, const SVECTOR* worldOffset)
{
    Task*                            task;
    _ShelterB3DumpingHoleSpriteWork* work;

    task       = taskSpawnFromTable(D_shelter_b3_dumping_hole_80188C04, 1, 0, sourceCoord);
    work       = memMalloc(SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES, false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    memFillBytes(work, 0, SHELTER_B3_DUMPING_HOLE_SPRITE_WORK_BYTES);
    work->offset.vx = worldOffset->vx;
    work->offset.vy = worldOffset->vy;
    work->offset.vz = worldOffset->vz;
}

/// Spawns three rising player sprites from a borrowed coordinate when `selector` is zero.
///
/// Other selectors do nothing. Base Z velocities are 0, -10 and +10 world
/// units per tick, before each sprite's jitter. Keep `sourceCoord` live through
/// each sprite's first tick and the debris director live until the sprites end.
static void _shelterB3DumpingHoleSpawnPlayerSpriteBurst(GfxCoord* sourceCoord, s16 selector)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_TASK_INDEX   = 3,
        SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_BASE_Z_SPEED = 10,
    };
    if (selector == SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITES_SELECT) {
        taskSpawnFromTable(D_shelter_b3_dumping_hole_80188C04, SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_TASK_INDEX, 0, sourceCoord);
        taskSpawnFromTable(D_shelter_b3_dumping_hole_80188C04, SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_TASK_INDEX, -SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_BASE_Z_SPEED, sourceCoord);
        taskSpawnFromTable(D_shelter_b3_dumping_hole_80188C04, SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_TASK_INDEX, SHELTER_B3_DUMPING_HOLE_PLAYER_SPRITE_BASE_Z_SPEED, sourceCoord);
    }
}

/// Asks the debris director to stop its player-model sprites when `selector` is zero.
static void _shelterB3DumpingHoleStopPlayerSprites(s32 selector)
{
    _ShelterB3DumpingHoleDebrisEventWork* eventWork = D_shelter_b3_dumping_hole_8018F4A8->work;
    if (selector == 0) {
        eventWork->playerSpritesStop = true;
    }
}

void func_shelter_b3_dumping_hole_8017FE34(void)
{
    taskSpawnFromTable(D_shelter_b3_dumping_hole_80188BC8, 1, 9, 0);
}

/// Sets the debris event's placement-0 actor model draw/reset mode.
///
/// Requires a live debris director and actor. Mode 0 hides/resets and allocates
/// model/escort buffers; 1 shows and allocates; 2 hides/resets without allocation;
/// 3 hides without resetting the actor state.
static void _shelterB3DumpingHoleDebrisEventSetActor0DrawMode(s32 drawMode)
{
    _ShelterB3DumpingHoleDebrisEventWork* eventWork = D_shelter_b3_dumping_hole_8018F4A8->work;
    taskMessageDispatch(eventWork->placement0Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Sets the debris event's placement-1 prop model draw/buffer mode.
///
/// Uses `ACTOR_MESSAGE_VISIBILITY_*` without changing the prop's behavior state.
/// Requires a live debris director and prop. Mode 0 hides and allocates;
/// 1 shows and allocates; 2 disables automatic buffer allocation while retaining
/// visibility; 3 shows with automatic allocation disabled.
static void _shelterB3DumpingHoleDebrisEventSetActor1DrawMode(s32 drawMode)
{
    _ShelterB3DumpingHoleDebrisEventWork* eventWork = D_shelter_b3_dumping_hole_8018F4A8->work;
    taskMessageDispatch(eventWork->placement1Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Replaces the debris director's pending player command and restarts its progress.
///
/// Requires the live debris director. Use a
/// `SHELTER_B3_DUMPING_HOLE_DEBRIS_PLAYER_COMMAND_*` value; NONE cancels the
/// pending command. Restarts at step zero without clearing the command timer.
static void _shelterB3DumpingHoleDebrisEventPostPlayerCommand(s16 command)
{
    _ShelterB3DumpingHoleDebrisEventWork* eventWork = D_shelter_b3_dumping_hole_8018F4A8->work;
    eventWork->playerCommand                        = command;
    eventWork->playerStep                           = 0;
}

/// Replaces the debris director's pending scene command and restarts its progress.
///
/// Requires the live debris director. Use a
/// `SHELTER_B3_DUMPING_HOLE_DEBRIS_SCENE_COMMAND_*` value; NONE cancels the
/// pending command. Restarts at step zero without clearing the command timer.
static void _shelterB3DumpingHoleDebrisEventPostSceneCommand(s16 command)
{
    _ShelterB3DumpingHoleDebrisEventWork* eventWork = D_shelter_b3_dumping_hole_8018F4A8->work;
    eventWork->sceneCommand                         = command;
    eventWork->sceneStep                            = 0;
}

void func_shelter_b3_dumping_hole_8017FF14(void)
{
    Task*                                 st  = D_shelter_b3_dumping_hole_8018F4A8;
    _ShelterB3DumpingHoleDebrisEventWork* ent = st->work;
    _ShelterB3DumpingHoleDebrisEventWork* ent2;
    s32                                   desc[5];

    taskMessageDispatch(ent->placement1Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
    taskMessageDispatch(ent->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    TASK_MESSAGE_DISPATCH_POINTER(ent->player, 0x3E9, &D_shelter_b3_dumping_hole_801881CC, 0);
    ent2    = st->work;
    desc[0] = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
    desc[1] = 9;
    desc[2] = 0;
    desc[3] = 0;
    desc[4] = 0;
    TASK_MESSAGE_DISPATCH_POINTER(ent2->player, ANIMATION_MESSAGE_PLAY, desc, 0);
    ent->debrisModelSignal  = SHELTER_B3_DUMPING_HOLE_DEBRIS_MODELS_REMOVE;
    ent->field_46           = 1;
    ent->debrisSpriteSignal = SHELTER_B3_DUMPING_HOLE_DEBRIS_SPRITES_REMOVE;
    ent->fadeStop           = 1;
    ent->actorSpritesStop   = 1;
    CdCmd_CancelReplaceAndActivate();
}

/// Stages deferred audio start for the debris event's selected scene.
///
/// Keep the selected scene and playback buffers live through request consumption.
/// With no selected slot, the previous deferred request remains intact.
static void _shelterB3DumpingHoleDebrisEventStageAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Requests playback of the debris event's selected scene/audio session.
///
/// Keep the selected scene and prepared playback buffers live through the request.
/// Without a selected slot, the scene enters playing mode immediately.
static void _shelterB3DumpingHoleDebrisEventEnqueuePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

void func_shelter_b3_dumping_hole_80180034(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}

/// Draws a tumbling triangular shard sampled from the collapse model's emitter.
///
/// Spawn argument 2 lends a shard-spawn record through the first tick.
/// Setup allocates task-owned work, samples the emitter world position and
/// randomizes velocity, spin and three local XY corners about its radius.
/// Later ticks move under the record's downward acceleration, cull the
/// origin, draw a grey Gouraud triangle, then advance rotation for the next
/// tick. A cleared collapse-shards flag removes it. Angles use 4096 units
/// per turn; positions and velocities use world units and ticks.
static void _shelterB3DumpingHoleShardTask(Task* task)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_SHARD_SETUP                = 0,
        SHELTER_B3_DUMPING_HOLE_SHARD_FLY                  = 1,
        SHELTER_B3_DUMPING_HOLE_SHARD_VELOCITY_JITTER_MASK = 0x1F,
        SHELTER_B3_DUMPING_HOLE_SHARD_SPIN_JITTER_MASK     = 0x7F,
        SHELTER_B3_DUMPING_HOLE_SHARD_MIN_SPIN             = 100,
        SHELTER_B3_DUMPING_HOLE_SHARD_ANGLE_60             = 0x2AA,
        SHELTER_B3_DUMPING_HOLE_SHARD_ANGLE_30             = 0x155,
        SHELTER_B3_DUMPING_HOLE_SHARD_DARK_INTENSITY       = 0x10,
        SHELTER_B3_DUMPING_HOLE_SHARD_MID_INTENSITY        = 0x40,
        SHELTER_B3_DUMPING_HOLE_SHARD_LIGHT_INTENSITY      = 0x80,
    };
    _ShelterB3DumpingHoleShardWork*  work;
    GfxCoord*                        coord;
    _ShelterB3DumpingHoleShardSpawn* spawn;
    POLY_G3*                         prim;
    SVECTOR                          emitterWorldPos;
    s16                              vertexScreenX[3];
    s16                              vertexScreenY[3];
    SVECTOR                          origin;
    s32                              packedScreenXY;
    s32                              depth;
    s16                              vertexIndex;
    s16                              originScreenX;
    s16                              originScreenY;

    work  = task->work;
    coord = task->extra.coordBody->coord;
    spawn = task->spawnArg2.pointer;
    if (D_shelter_b3_dumping_hole_8018F4B0_value == 0) {
        taskKill(task);
        return;
    }
    switch (task->state) {
        case SHELTER_B3_DUMPING_HOLE_SHARD_SETUP:
            task->work = memCalloc(SHELTER_B3_DUMPING_HOLE_SHARD_WORK_BYTES, false);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            work          = task->work;
            coord->parent = &gGfxViewCoord;
            memFillBytes(task->work, 0, SHELTER_B3_DUMPING_HOLE_SHARD_WORK_BYTES);
            gfxComposeNodeWorldTransform(spawn->emitter, &coord->coord, &emitterWorldPos);
            coord->coord.t[0] = emitterWorldPos.vx + spawn->offset.vx;
            coord->coord.t[1] = emitterWorldPos.vy + spawn->offset.vy;
            coord->coord.t[2] = emitterWorldPos.vz + spawn->offset.vz;
            work->vel.vx      = spawn->vel.vx + ((DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_VELOCITY_JITTER_MASK) : -(DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_VELOCITY_JITTER_MASK));
            work->vel.vy      = spawn->vel.vy + ((DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_VELOCITY_JITTER_MASK) : -(DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_VELOCITY_JITTER_MASK));
            work->vel.vz      = spawn->vel.vz + ((DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_VELOCITY_JITTER_MASK) : -(DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_VELOCITY_JITTER_MASK));
            work->gravity     = spawn->gravity;
            work->spin.vx     = (DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_SPIN_JITTER_MASK) : -(DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_SPIN_JITTER_MASK);
            work->spin.vy     = (DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_SPIN_JITTER_MASK) : -(DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_SPIN_JITTER_MASK);
            work->spin.vz     = (DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_SPIN_JITTER_MASK) : -(DUMPING_HOLE_RAND() & SHELTER_B3_DUMPING_HOLE_SHARD_SPIN_JITTER_MASK);
            if (work->spin.vx > 0) {
                work->spin.vx += SHELTER_B3_DUMPING_HOLE_SHARD_MIN_SPIN;
            } else {
                work->spin.vx -= SHELTER_B3_DUMPING_HOLE_SHARD_MIN_SPIN;
            }
            if (work->spin.vy > 0) {
                work->spin.vy += SHELTER_B3_DUMPING_HOLE_SHARD_MIN_SPIN;
            } else {
                work->spin.vy -= SHELTER_B3_DUMPING_HOLE_SHARD_MIN_SPIN;
            }
            if (work->spin.vz > 0) {
                work->spin.vz += SHELTER_B3_DUMPING_HOLE_SHARD_MIN_SPIN;
            } else {
                work->spin.vz -= SHELTER_B3_DUMPING_HOLE_SHARD_MIN_SPIN;
            }
            // Equilateral corners receive outward jitter of one tenth of the radius.
            work->verts[0].vx = 0;
            work->verts[0].vy = spawn->radius + ((DUMPING_HOLE_RAND() & 1) ? spawn->radius / 10 : 0);
            work->verts[0].vz = 0;
            work->verts[1].vx = spawn->radius * rsin(SHELTER_B3_DUMPING_HOLE_SHARD_ANGLE_60) / ONE + ((DUMPING_HOLE_RAND() & 1) ? spawn->radius / 10 : 0);
            work->verts[1].vy = -(spawn->radius * rsin(SHELTER_B3_DUMPING_HOLE_SHARD_ANGLE_30) / ONE) - ((DUMPING_HOLE_RAND() & 1) ? spawn->radius / 10 : 0);
            work->verts[1].vz = 0;
            work->verts[2].vx = -(spawn->radius * rsin(SHELTER_B3_DUMPING_HOLE_SHARD_ANGLE_60) / ONE) - ((DUMPING_HOLE_RAND() & 1) ? spawn->radius / 10 : 0);
            work->verts[2].vy = -(spawn->radius * rsin(SHELTER_B3_DUMPING_HOLE_SHARD_ANGLE_30) / ONE) - ((DUMPING_HOLE_RAND() & 1) ? spawn->radius / 10 : 0);
            work->verts[2].vz = 0;
            task->state++;
            break;
        case SHELTER_B3_DUMPING_HOLE_SHARD_FLY:
            work->vel.vy      += work->gravity;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            actorRenderComposeCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            origin.vz = 0;
            origin.vy = 0;
            origin.vx = 0;
            gte_ldv0(&origin);
            gte_rtps();
            gte_stsxy(&packedScreenXY);
            gte_stszotz(&depth);
            originScreenY = packedScreenXY >> 16;
            originScreenX = packedScreenXY;
            // Cull the origin before projecting and queuing the three triangle corners.
            if (originScreenX < -SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_WIDTH || (originScreenX > SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_WIDTH || originScreenY < -SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_HEIGHT || originScreenY > SHELTER_B3_DUMPING_HOLE_SCREEN_HALF_HEIGHT || depth < 0)) {
                taskKill(task);
                break;
            }
            for (vertexIndex = 0; vertexIndex < (s16)ARRAY_SIZE(work->verts); vertexIndex++) {
                gte_ldv0(&work->verts[vertexIndex]);
                gte_rtps();
                gte_stsxy(&packedScreenXY);
                gte_stszotz(&depth);
                vertexScreenX[vertexIndex] = packedScreenXY;
                vertexScreenY[vertexIndex] = packedScreenXY >> 16;
            }
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG3(prim);
            setRGB0(prim, SHELTER_B3_DUMPING_HOLE_SHARD_DARK_INTENSITY, SHELTER_B3_DUMPING_HOLE_SHARD_DARK_INTENSITY, SHELTER_B3_DUMPING_HOLE_SHARD_DARK_INTENSITY);
            setRGB1(prim, SHELTER_B3_DUMPING_HOLE_SHARD_MID_INTENSITY, SHELTER_B3_DUMPING_HOLE_SHARD_MID_INTENSITY, SHELTER_B3_DUMPING_HOLE_SHARD_MID_INTENSITY);
            setRGB2(prim, SHELTER_B3_DUMPING_HOLE_SHARD_LIGHT_INTENSITY, SHELTER_B3_DUMPING_HOLE_SHARD_LIGHT_INTENSITY, SHELTER_B3_DUMPING_HOLE_SHARD_LIGHT_INTENSITY);
            prim->x0 = vertexScreenX[0];
            prim->y0 = vertexScreenY[0];
            prim->x1 = vertexScreenX[1];
            prim->y1 = vertexScreenY[1];
            prim->x2 = vertexScreenX[2];
            prim->y2 = vertexScreenY[2];
            addPrim(&gGpuCurrentOt[depth >> SHELTER_B3_DUMPING_HOLE_OT_DEPTH_SHIFT], prim);
            work->rot.vx += work->spin.vx;
            work->rot.vy += work->spin.vy;
            work->rot.vz += work->spin.vz;
            gfxRotMatrixY(&coord->coord, work->rot.vy, GRAPHICS_ROTATION_REPLACE);
            gfxRotMatrixX(&coord->coord, work->rot.vx, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&coord->coord, work->rot.vz, GRAPHICS_ROTATION_COMPOSE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}

/// Per-frame handler of the collapse event's director task: runs the pending
/// `_ShelterB3DumpingHoleCollapseEventWork::command`, which is cleared once
/// handled unless it is a multi-frame sequence.
///
/// - `PREPARE` sends the slot-3 task its animation and the slot-4 task message
///   0x7DA for the current stage and area.
/// - `COLLAPSE` is a sequence stepped by `step`. Step 0 sends the opening
///   messages, overrides the view vector and places the task at its starting
///   pose. Step 1 spawns a burst of shard tasks every 16 frames and moves model
///   part 2; from frame 0x3D it also shrinks part 3 along X and Z, rebuilding
///   its rotation from identity each frame, and at frame 0x5B it spawns the
///   closing effects and advances. Until then the pose rotation alternates
///   either side of its starting value each frame. Step 2 keeps moving part 2
///   and, after six frames, tips part 1 about X. Every frame of the sequence
///   also shakes the screen by one unit.
/// - `END_COLLAPSE` and `FINISH` undo the sequence's overrides and send the
///   slot-4 task message 0x7DA; `FINISH` also restores the saved view, resets
///   the slot-3 animation and ends the framebuffer-blend task.
/// - `HIDE_ACTOR` sends the placement-0 actor message 0x7D5.
/// - `FLICKER` is a second sequence that alternates
///   `func_shelter_b3_dumping_hole_80183218` calls, then spawns the
///   framebuffer-blend task.
///
/// The message buffers are unions because the cases share their stack slots.
static void func_shelter_b3_dumping_hole_8018098C(Task* task)
{
    _ShelterB3DumpingHoleCollapseEventWork* work;
    GfxMatrix*                              ident;
    GfxMatrix*                              ident2;
    u16                                     i;
    ActorCommand*                           command3;
    ActorCommand*                           command5;
    s32*                                    p;
    union {
        s32          words[5];
        ActorCommand loc;
        SVECTOR      vec[4];
    } buf;
    union {
        SVECTOR      vec;
        ActorCommand loc;
    } buf2;
    s32 words[5];

    work = task->work;
    switch (work->command) {
        case SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_PREPARE:
            Gp_PulseState1C();
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            buf.words[0]       = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
            buf.words[1]       = 9;
            buf.words[2]       = 1;
            buf.words[3]       = 0xA;
            buf.words[4]       = 0;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, buf.words, 0);
            buf.loc.context.loc.stage = gGameSession->location.loc.stage;
            buf.loc.context.loc.area  = gGameSession->location.loc.area;
            buf.loc.command           = 0xA;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &buf.loc, ACTOR_COMMAND_MESSAGE_APPLY);
            work->command = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_NONE;
            return;
        case SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_COLLAPSE:
            switch (work->step) {
                case 0:
                    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
                    buf.vec[0].vx = 0x800;
                    buf.vec[0].vy = 0x800;
                    buf.vec[0].vz = 0x800;
                    worldCoordSetAmbientColorOverride(&buf.vec[0]);
                    taskMessageDispatch(work->placement0Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
                    taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    work->pose.pos.vx = D_shelter_b3_dumping_hole_8018966C.pos.vx;
                    work->pose.pos.vy = D_shelter_b3_dumping_hole_8018966C.pos.vy;
                    work->pose.pos.vz = D_shelter_b3_dumping_hole_8018966C.pos.vz;
                    work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx;
                    work->pose.rot.vy = D_shelter_b3_dumping_hole_8018966C.rot.vy;
                    work->pose.rot.vz = D_shelter_b3_dumping_hole_8018966C.rot.vz;
                    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &work->pose, 0);
                    work->shardSpawn.offset.vx               = 0;
                    work->shardSpawn.offset.vy               = 0;
                    work->shardSpawn.offset.vz               = 0;
                    D_shelter_b3_dumping_hole_8018F4B0_value = 1;
                    work->shardSpawn.emitter                 = &task->extra.tmd->coords[2];
                    work->shardSpawn.radius                  = 0x14;
                    work->part3Scale.vx                      = 0x1000;
                    work->part3Scale.vy                      = 0x1000;
                    work->part3Scale.vz                      = 0x1000;
                    work->timer                              = 0;
                    work->step++;
                    break;
                case 1:
                    work->timer++;
                    if (!(gDisplayState.animFrame & 0xF)) {
                        work->shardSpawn.vel.vx  = 0;
                        work->shardSpawn.vel.vy  = 0;
                        work->shardSpawn.vel.vz  = 0;
                        work->shardSpawn.gravity = 4;
                        for (i = 0; i < 10; i++) {
                            taskSpawnFromTable(D_shelter_b3_dumping_hole_80189ADC, 1, 0, &work->shardSpawn);
                        }
                    }
                    if (work->timer >= 0x5B) {
                        buf.vec[2].vx = 0xC8;
                        buf.vec[2].vy = 0x190;
                        buf.vec[2].vz = -0xC8;
                        ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[2], &buf.vec[3]);
                        Gp_SpawnEff(EFFECT_SMOKE_PUFF, task->extra.tmd->coords, 0x608, &buf.vec[3]);
                        buf.vec[2].vx = 0xC8;
                        buf.vec[2].vy = 0x190;
                        buf.vec[2].vz = -0x320;
                        ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[2], &buf.vec[3]);
                        Gp_SpawnEff(EFFECT_SMOKE_PUFF, task->extra.tmd->coords, 0x608, &buf.vec[3]);
                        work->timer      = 0;
                        work->part1Pitch = 0;
                        work->field_9A   = 0x80;
                        work->step++;
                        break;
                    }
                    if (work->timer >= 0x1F) {
                        if (work->timer >= 0x3D) {
                            task->extra.tmd->coords[2].coord.t[1] += 8;
                            work->part3Scale.vx                   -= 10;
                            work->part3Scale.vz                   -= 10;
                            ident                                  = (GfxMatrix*)&task->extra.tmd->coords[3].coord;

                            ident->rotationWords.m00M01 = ONE;
                            ident->rotationWords.m02M10 = 0;
                            ident->rotationWords.m11M12 = ONE;
                            ident->rotationWords.m20M21 = 0;
                            ident->rotationWords.m22    = ONE;

                            gfxScaleMatrixColumns(&task->extra.tmd->coords[3].coord, &work->part3Scale);
                        } else if (work->timer < 0x20) {
                            task->extra.tmd->coords[2].coord.t[1] += 0x20;
                            ident2                                 = (GfxMatrix*)&task->extra.tmd->coords[3].coord;

                            ident2->rotationWords.m00M01 = ONE;
                            ident2->rotationWords.m02M10 = 0;
                            ident2->rotationWords.m11M12 = ONE;
                            ident2->rotationWords.m20M21 = 0;
                            ident2->rotationWords.m22    = ONE;
                        }
                        if (!(gDisplayState.animFrame & 0xF)) {
                            buf.vec[1].vx = 0xC8;
                            buf.vec[1].vy = 0x190;
                            buf.vec[1].vz = -0xC8;
                            ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[1], &buf2.vec);
                            Gp_SpawnEff(EFFECT_FLASH_BURST, task->extra.tmd->coords, 0x200, &buf2.vec);
                            buf.vec[1].vx = 0xC8;
                            buf.vec[1].vy = 0x190;
                            buf.vec[1].vz = -0x320;
                            ApplyMatrixSV(&task->extra.tmd->coords->coord, &buf.vec[1], &buf2.vec);
                            Gp_SpawnEff(EFFECT_FLASH_BURST, task->extra.tmd->coords, 0x200, &buf2.vec);
                        }
                    }
                    if (gDisplayState.animFrame & 1) {
                        work->part1Pitch = work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx + 0x10;
                    } else {
                        work->part1Pitch = work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx - 0x10;
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &work->pose, 0);
                    break;
                case 2:
                    if (!(gDisplayState.animFrame & 0xF)) {
                        buf2.vec.vx = -0x190;
                        buf2.vec.vy = 0x190;
                        buf2.vec.vz = 0x190;
                        Gp_SpawnEff(EFFECT_FLASH_BURST, &task->extra.tmd->coords[1], 0x200, &buf2.vec);
                    }
                    if (++work->timer >= 6) {
                        if (work->part1Pitch < -0x154) {
                            work->part1Pitch -= 1;
                        } else {
                            work->part1Pitch -= 8;
                        }
                        gfxRotMatrixX(&task->extra.tmd->coords[1].coord, work->part1Pitch, GRAPHICS_ROTATION_REPLACE);
                    }
                    task->extra.tmd->coords[2].coord.t[1] += 0x190;
                    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &work->pose, 0);
                    break;
            }
            if (gDisplayState.animFrame & 1) {
                displaySetShakeY(1);
            } else {
                displaySetShakeY(-1);
            }
            return;
        case SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_END_COLLAPSE:
            worldCoordSetAmbientColorOverride(NULL);
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskMessageDispatch(work->placement0Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            D_shelter_b3_dumping_hole_8018F4B0_value = 0;
            work->field_96                           = 1;
            Gp_PulseState1C();
            buf2.loc.context.loc.stage = gGameSession->location.loc.stage;
            buf2.loc.context.loc.area  = gGameSession->location.loc.area;
            command3                   = &buf2.loc;
            command3->command          = 0xB;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, command3, ACTOR_COMMAND_MESSAGE_APPLY);
            displaySetShakeY(0);
            work->command = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_NONE;
            return;
        case SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_HIDE_ACTOR:
            taskMessageDispatch(work->placement0Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
        case SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_FINISH:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = work->savedView;
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskMessageDispatch(work->placement0Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            work->field_96 = 1;
            Gp_PulseState1C();
            buf2.loc.context.loc.stage = gGameSession->location.loc.stage;
            buf2.loc.context.loc.area  = gGameSession->location.loc.area;
            command5                   = &buf2.loc;
            command5->command          = 0xC;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, command5, ACTOR_COMMAND_MESSAGE_APPLY);
            p        = words;
            words[0] = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
            p[1]     = 1;
            p[2]     = 1;
            p[3]     = 0xA;
            words[4] = 0;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, words, 0);
            if (work->framebufferBlend != NULL) {
                taskCallExit(work->framebufferBlend);
                work->framebufferBlend = NULL;
            }
            break;
        case SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_NONE:
            break;
        case SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_FLICKER:
            switch (work->step) {
                case 1:
                case 3:
                case 5:
                    func_shelter_b3_dumping_hole_80183218(0);
                    work->timer = 0;
                    work->step++;
                    return;
                case 0:
                case 2:
                case 4:
                    func_shelter_b3_dumping_hole_80183218(1);
                    work->timer = 0;
                    work->step++;
                    return;
                case 6:
                    if (++work->timer >= 0xB) {
                        work->framebufferBlend = taskSpawn(1, 0x2D, 0x10, 0);
                        work->timer            = 0;
                        work->step++;
                    }
                    return;
                case 7:
                    if (++work->timer >= 6) {
                        func_shelter_b3_dumping_hole_80183218(1);
                        func_shelter_b3_dumping_hole_80183218(2);
                        break;
                    }
                    return;
                default:
                    return;
            }
            break;
    }
    work->command = SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_NONE;
}

void func_shelter_b3_dumping_hole_80181430(void)
{
    _ShelterB3DumpingHoleCollapseEventWork* work;
    ActorCommand                            request;
    s32                                     desc3[5];
    s32*                                    p3;

    work = D_shelter_b3_dumping_hole_8018F4AC->work;
    worldCoordSetAmbientColorOverride(NULL);
    if (work->framebufferBlend != NULL) {
        taskCallExit(work->framebufferBlend);
        work->framebufferBlend = NULL;
    }
    work->field_96 = 1;
    Gp_PulseState1C();

    D_shelter_b3_dumping_hole_8018F4B0_value = 0;
    request.context.loc.stage                = gGameSession->location.loc.stage;
    request.context.loc.area                 = gGameSession->location.loc.area;
    request.command                          = 0x13;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &request, ACTOR_COMMAND_MESSAGE_APPLY);

    displaySetShakeY(0);
    taskMessageDispatch(work->placement0Actor, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);

    p3       = desc3;
    desc3[0] = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
    p3[1]    = 1;
    desc3[2] = 0;
    desc3[3] = 0;
    desc3[4] = 0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, desc3, 0);
    CdCmd_CancelReplaceAndActivate();
}

void func_shelter_b3_dumping_hole_80181560(Task* task)
{
    s32                                     desc[5];
    TmdObject*                              obj;
    TmdObject*                              tail;
    _ShelterB3DumpingHoleCollapseEventWork* work;

    switch (task->state) {
        case 0:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            obj        = task->extra.tmd;
            task->work = memCalloc(sizeof(_ShelterB3DumpingHoleCollapseEventWork), false);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                task->extra.tmd->coords->parent = &gGfxViewCoord;
                work                            = task->work;
                memFillBytes(work, 0, sizeof(*work));
                work->player                       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_shelter_b3_dumping_hole_8018F4AC = task;
                work->placement0Actor              = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8))->task;
                obj->lightMtx                      = &work->lightMtx;
                obj->colorMtx                      = &work->colorMtx;
                task->msgTable                     = D_shelter_b3_dumping_hole_8018965C;
                func_shelter_b3_dumping_hole_80183218(0);
            }
            D_shelter_b3_dumping_hole_8018F4D8                               = 0;
            ((_ShelterB3DumpingHoleCollapseEventWork*)task->work)->savedView = gGameSession->location.loc.view;
            Gp_MsgPlayerWeapon(0);
            desc[0] = gPlayerStatus.weapon + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1 ? 1 : 0x22);
            desc[1] = 9;
            desc[2] = 1;
            desc[3] = 0xA;
            desc[4] = 0;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, desc, 0);
            task->state++;
            break;
        case 1:
            D_shelter_b3_dumping_hole_8018809C = 0;
            func_800E8634(D_shelter_b3_dumping_hole_8018968C, 0, D_shelter_b3_dumping_hole_801899A4);
            task->state++;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                gameFlagSetNibble(GAME_FLAG_11D, 1);
                taskKill(task);
            } else {
                func_shelter_b3_dumping_hole_8018098C(task);
            }
            break;
    }
    tail    = task->extra.tmd;
    desc[0] = tail->coords->workm.t[0];
    desc[1] = task->extra.tmd->coords->workm.t[1];
    desc[2] = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(tail, desc, 0, 3);
}

#include "../../shared/actor_messages_draw_mode.inc.c"

#include "../../shared/actor_messages_place_ypr.inc.c"

void func_shelter_b3_dumping_hole_801818E0(void)
{
    _ShelterB3DumpingHoleCollapseEventWork* work = D_shelter_b3_dumping_hole_8018F4AC->work;
    if (work->battleReleased == 0) {
        Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x20);
        gSceneCombatState.battleRefs = 0;
        gGameSession->flowFlags     |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        work->battleReleased         = 1;
    }
}

/// Sets the collapse event's player model draw/buffer mode.
///
/// Requires a live collapse director and player; `drawMode` is a
/// `PLAYER_ACTOR_MODEL_DRAW_*` value forwarded without narrowing.
static void _shelterB3DumpingHoleCollapseEventSetPlayerDrawMode(s32 drawMode)
{
    _ShelterB3DumpingHoleCollapseEventWork* work = D_shelter_b3_dumping_hole_8018F4AC->work;
    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Replaces the collapse director's pending command and restarts its progress.
///
/// Requires the live collapse director. Use a
/// `SHELTER_B3_DUMPING_HOLE_COLLAPSE_COMMAND_*` value; NONE cancels the pending
/// command. Restarts at step zero without clearing the command timer.
static void _shelterB3DumpingHoleCollapseEventPostCommand(s16 command)
{
    _ShelterB3DumpingHoleCollapseEventWork* work = D_shelter_b3_dumping_hole_8018F4AC->work;
    work->command                                = command;
    work->step                                   = 0;
}

/// Stages deferred audio start for the collapse event's selected scene.
///
/// Keep the selected scene and playback buffers live through request consumption.
/// With no selected slot, the previous deferred request remains intact.
static void _shelterB3DumpingHoleCollapseEventStageAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Requests playback of the collapse event's selected scene/audio session.
///
/// Keep the selected scene and prepared playback buffers live through the request.
/// Without a selected slot, the scene enters playing mode immediately.
static void _shelterB3DumpingHoleCollapseEventEnqueuePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

void func_shelter_b3_dumping_hole_801819F0(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}

/// Spawns a vertical three-pixel shake alternating for nine updates, then clearing.
static void _shelterB3DumpingHoleSpawnShake(void)
{
    taskSpawnFromTable(&D_shelter_b3_dumping_hole_8018AFBC, 0, 0, NULL);
}

/// Alternates the display's vertical shake by three pixels for nine updates, then clears it.
///
/// Setup reuses spawn argument 1 as the signed amplitude and killCountdown
/// as an eight-to-minus-one timer. The final shaking update advances the
/// state; the following update clears the global displacement and kills it.
static void _shelterB3DumpingHoleShakeTask(Task* task)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_SHAKE_SETUP     = 0,
        SHELTER_B3_DUMPING_HOLE_SHAKE_RUN       = 1,
        SHELTER_B3_DUMPING_HOLE_SHAKE_AMPLITUDE = 3,
        SHELTER_B3_DUMPING_HOLE_SHAKE_COUNTDOWN = 8,
    };
    switch (task->state) {
        case SHELTER_B3_DUMPING_HOLE_SHAKE_SETUP:
            task->spawnArg1.value = SHELTER_B3_DUMPING_HOLE_SHAKE_AMPLITUDE;
            task->killCountdown   = SHELTER_B3_DUMPING_HOLE_SHAKE_COUNTDOWN;
            task->state          += 1;
            break;
        case SHELTER_B3_DUMPING_HOLE_SHAKE_RUN:
            if (--task->killCountdown < 0) {
                task->state += 1;
            }
            displaySetShakeY(task->spawnArg1.value);
            task->spawnArg1.value = -task->spawnArg1.value;
            break;
        default:
            displaySetShakeY(0);
            taskKill(task);
            break;
    }
}

/// Spawns the arrival event's player-sprite burst from player model coordinate 1.
///
/// Zero selects the burst; other selectors do nothing. Requires that player
/// coordinate through each sprite's first tick and the live debris director
/// until the sprites end.
static void _shelterB3DumpingHoleSpawnPlayerSprites(s16 selector)
{
    _shelterB3DumpingHoleSpawnPlayerSpriteBurst(
        &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[1], selector);
}

/// Requests removal of the player sprites when the complete selector word is zero.
///
/// Requires the live debris director. Other selectors do nothing; each sprite
/// observes the stop flag and releases itself on its next tick.
static void _shelterB3DumpingHoleRequestPlayerSpriteStop(s32 selector)
{
    _shelterB3DumpingHoleStopPlayerSprites(selector);
}

#include "../../shared/cap_captions.inc.c"

static void func_shelter_b3_dumping_hole_801830F0(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_ShowTimed(arg0, arg1, arg2);
}

#include "../../shared/cap_captions_resource.inc.c"

void func_shelter_b3_dumping_hole_80183198(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_LoadResource(arg0, arg1, arg2);
}

/// Hides or shows sprite commands 1 and 2 of the area's view 13 through their
/// `SpriteBatch::hidden`: 0 hides both, 1 shows command 2 and 2 shows command 1.
static void func_shelter_b3_dumping_hole_80183218(u8 arg0)
{
    GameLocationKey* g4      = &gGameSession->location.loc;
    SpriteBatch*     batches = Gp_SprtTables[g4->stage - 1]->areaViews[g4->area - 1][13].batches;

    if (arg0 == 0) {
        batches[1].hidden = 1;
        batches[2].hidden = 1;
    } else if (arg0 == 1) {
        batches[2].hidden = 0;
    } else if (arg0 == 2) {
        batches[1].hidden = 0;
    }
}

/// Spawns the two enemies of one slot from the `D_actor_207000_80151E60` table, numbering
/// them from the spawn counter, and marks the slot live. Actor 342400 carries
/// the same body.
static void func_shelter_b3_dumping_hole_80183298(Task* arg0)
{
    OverlayEncounterPairWork* work;
    Enemy*                    enemy;
    Task*                     task;
    TmdObject*                obj;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work   = work;
    work->enemy0 = Gp_SpawnEnemyFromTable(&D_actor_207000_80151E60, 1, 1, 0);
    work->enemy1 = Gp_SpawnEnemyFromTable(&D_actor_207000_80151E60, 1, 1, 0);
    if (work->enemy0 == NULL && work->enemy1 == NULL) {
        taskKill(arg0);
        return;
    }
    if (work->enemy0 != NULL) {
        enemy           = work->enemy0;
        enemy->placeKey = D_shelter_b3_dumping_hole_8018F4D4_value << ENEMY_PLACE_INDEX_SHIFT;
        D_shelter_b3_dumping_hole_8018F4D4_value++;
        task                   = enemy->task;
        obj                    = task->extra.tmd;
        obj->texturePageOffset = 3;
        obj->clutRowOffset     = 5;
        enemy->hp              = 1;
    }
    if (work->enemy1 != NULL) {
        enemy           = work->enemy1;
        enemy->placeKey = D_shelter_b3_dumping_hole_8018F4D4_value << ENEMY_PLACE_INDEX_SHIFT;
        D_shelter_b3_dumping_hole_8018F4D4_value++;
        task                   = enemy->task;
        obj                    = task->extra.tmd;
        obj->texturePageOffset = 3;
        obj->clutRowOffset     = 5;
        enemy->hp              = 1;
    }
    D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
    arg0->state++;
}

static void func_shelter_b3_dumping_hole_801833EC(Task* arg0)
{
    OverlayEncounterControllerWork* work = arg0->work;
    s16                             count;
    s16                             i;
    s16                             idx;
    s16                             type;
    s16                             arg;

    count = 0;
    for (i = 0; i < 16; i++) {
        if (D_shelter_b3_dumping_hole_8018B7BC[i].status == OVERLAY_ENCOUNTER_SLOT_LIVE) {
            count++;
        }
    }
    if (count < 3) {
        idx = work->nextSlot;
        if (idx < 16 && gGameSession->sceneClock >= 0x3D) {
            type = D_shelter_b3_dumping_hole_8018B7BC[idx].kind;
            arg  = D_shelter_b3_dumping_hole_8018B7BC[idx].command;
            switch (type) {
                case 0:
                    taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 1, (idx << 16) + arg, 0);
                    break;
                case 1:
                    taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 2, (idx << 16) + arg, 0);
                    break;
                case 2:
                    taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 3, (idx << 16) + arg, 0);
                    break;
            }
            work->nextSlot++;
        }
    }
}

/// Latches the encounter stop command.
///
/// Installed for `ACTOR_COMMAND_MESSAGE_APPLY`; borrows `request` for this
/// call. Other commands do not change the controller. It is declared to return
/// a value, as message handlers are, but has no return statement.
/// `messageId` and the second payload word are unused.
static s32 _shelterB3DumpingHoleEncounterStopMessage(Task* task, s32 messageId, ActorCommand* request, s32 unused)
{
    OverlayEncounterControllerWork* work = task->work;

    if (request->command == OVERLAY_ENCOUNTER_COMMAND_STOP) {
        work->stop = request->command;
    }
    // No return statement: the handler falls off its end, as the binary does.
    // What the dispatcher then reads is whatever the comparison left behind.
}

/// States of the task that works through the room's 16 enemy slots: set-up,
/// the first three slot spawns, a 15-frame wait before `sceneEngageBattle`, and a
/// loop that starts the next slot while fewer than three are live and ends
/// once all 16 have been cleared.
static const TaskFuncTable4 D_shelter_b3_dumping_hole_8017D654 = { {
    func_shelter_b3_dumping_hole_801836E0,
    func_shelter_b3_dumping_hole_8018378C,
    func_shelter_b3_dumping_hole_80183824,
    func_shelter_b3_dumping_hole_801838A0,
} };

/// States of a slot task holding one enemy from `Actor04400_D107E4`: spawn it, after
/// a delay switch its palette and send it message 0x7DB, then wait for its hit
/// points to run out.
static const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D664 = { {
    func_shelter_b3_dumping_hole_80183950,
    func_shelter_b3_dumping_hole_80183A00,
    func_shelter_b3_dumping_hole_80183A98,
} };

/// States of a slot task holding one enemy from `D_actor_207000_801575F0`: spawn it, after
/// a delay switch its palette and send it message 0x7DB, then wait for its hit
/// points to run out.
static const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D670 = { {
    func_shelter_b3_dumping_hole_80183AEC,
    func_shelter_b3_dumping_hole_80183B9C,
    func_shelter_b3_dumping_hole_80183C38,
} };

/// States of a slot task holding a pair of enemies from `D_actor_207000_80151E60`: spawn
/// them, one idle frame, send the first message 0x7DB, send the second the
/// same after a delay, then wait until both are gone.
static const TaskFuncTable5 D_shelter_b3_dumping_hole_8017D67C = { {
    func_shelter_b3_dumping_hole_80183298,
    func_shelter_b3_dumping_hole_80183C8C,
    func_shelter_b3_dumping_hole_80183CA0,
    func_shelter_b3_dumping_hole_80183D34,
    func_shelter_b3_dumping_hole_80183E08,
} };

void func_shelter_b3_dumping_hole_80183550(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_shelter_b3_dumping_hole_8017D654;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[task->state](task);
    }
}

void func_shelter_b3_dumping_hole_801835C8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D664;
    sp.funcs[task->state](task);
}

void func_shelter_b3_dumping_hole_80183620(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D670;
    sp.funcs[task->state](task);
}

void func_shelter_b3_dumping_hole_80183678(Task* task)
{
    TaskFuncTable5 sp;

    sp = D_shelter_b3_dumping_hole_8017D67C;
    sp.funcs[task->state](task);
}

static void func_shelter_b3_dumping_hole_801836E0(Task* arg0)
{
    OverlayEncounterControllerWork* work;
    s32                             i;

    if (gGameSession->spawnPhase[0] == GAME_SESSION_SPAWN_COMPLETE) {
        taskKill(arg0);
        return;
    }
    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    for (i = 15; i >= 0; i--) {
        D_shelter_b3_dumping_hole_8018B7BC[i].status = OVERLAY_ENCOUNTER_SLOT_WAITING;
    }
    D_shelter_b3_dumping_hole_8018F4D4_value = 0;
    arg0->work                               = work;
    arg0->msgTable                           = D_shelter_b3_dumping_hole_8018B7AC;
    arg0->state                             += 1;
}

static void func_shelter_b3_dumping_hole_8018378C(Task* arg0)
{
    OverlayEncounterControllerWork* work = arg0->work;
    s32                             i;

    for (i = 0; i < 3; i++) {
        s16 idx = work->nextSlot;
        func_shelter_b3_dumping_hole_80183E6C(idx, D_shelter_b3_dumping_hole_8018B7BC[idx].kind,
                                              D_shelter_b3_dumping_hole_8018B7BC[idx].command);
        work->nextSlot += 1;
    }
    arg0->state += 1;
}

static void func_shelter_b3_dumping_hole_80183824(Task* arg0)
{
    OverlayEncounterControllerWork* work = arg0->work;

    if (++work->frames == 15) {
        (sceneAcquireBattleRef)(0);
        gGameSession->spawnPhase[0] = GAME_SESSION_SPAWN_ARMED;
        sceneEngageBattle(1);
        arg0->state += 1;
    }
}

static void func_shelter_b3_dumping_hole_801838A0(Task* arg0)
{
    OverlayEncounterControllerWork* work = arg0->work;
    s16                             count;
    s32                             i;

    count = 0;
    if (work->stop != OVERLAY_ENCOUNTER_COMMAND_STOP) {
        func_shelter_b3_dumping_hole_801833EC(arg0);
        for (i = 0; i < 0x10; i++) {
            if (D_shelter_b3_dumping_hole_8018B7BC[i].status == OVERLAY_ENCOUNTER_SLOT_DONE) {
                count++;
            }
        }
        if (count == 0x10) {
            Gp_ReleaseStateF0Clear(arg0, 0);
            gGameSession->spawnPhase[0] = GAME_SESSION_SPAWN_COMPLETE;
            taskKill(arg0);
        }
    }
}

static void func_shelter_b3_dumping_hole_80183950(Task* arg0)
{
    OverlayEncounterSingleWork* work = memCalloc(sizeof(*work), 0);
    if (work != NULL) {
        Enemy* enemy;
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&Actor04400_D107E4, 1, 0, NULL);
        if (enemy != NULL) {
            u16 idx;
            D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
            idx                                                                           = D_shelter_b3_dumping_hole_8018F4D4_value;
            work->enemy                                                                   = enemy;
            enemy->placeKey                                                               = idx << ENEMY_PLACE_INDEX_SHIFT;
            D_shelter_b3_dumping_hole_8018F4D4_value                                      = idx + 1;
            arg0->state                                                                  += 1;
            return;
        }
    }
    taskKill(arg0);
}

static void func_shelter_b3_dumping_hole_80183A00(Task* arg0)
{
    ActorCommand                request;
    OverlayEncounterSingleWork* work = arg0->work;
    Enemy*                      t0   = work->enemy;
    Task*                       t00  = t0->task;

    if (++work->frames > 45) {
        TmdObject* p              = t00->extra.tmd;
        p->clutRowOffset          = 2;
        p->texturePageOffset      = 0;
        t0->workType              = ENEMY_WORK_PLAIN;
        request.context.loc.stage = 0;
        request.context.loc.area  = 0x2C;
        request.command           = arg0->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(t00, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        arg0->state += 1;
    }
}

static void func_shelter_b3_dumping_hole_80183A98(Task* arg0)
{
    OverlayEncounterSingleWork* work = arg0->work;

    if (work->enemy->hp <= 0) {
        D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(arg0);
    }
}

static void func_shelter_b3_dumping_hole_80183AEC(Task* arg0)
{
    OverlayEncounterSingleWork* work = memCalloc(sizeof(*work), 0);
    if (work != NULL) {
        Enemy* enemy;
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&D_actor_207000_801575F0, 2, 0, NULL);
        if (enemy != NULL) {
            u16 idx;
            D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
            idx                                                                           = D_shelter_b3_dumping_hole_8018F4D4_value;
            work->enemy                                                                   = enemy;
            enemy->placeKey                                                               = idx << ENEMY_PLACE_INDEX_SHIFT;
            D_shelter_b3_dumping_hole_8018F4D4_value                                      = idx + 1;
            arg0->state                                                                  += 1;
            return;
        }
    }
    taskKill(arg0);
}

static void func_shelter_b3_dumping_hole_80183B9C(Task* arg0)
{
    ActorCommand                request;
    OverlayEncounterSingleWork* work = arg0->work;
    Enemy*                      t0   = work->enemy;
    Task*                       t00  = t0->task;

    if (++work->frames > 60) {
        TmdObject* p              = t00->extra.tmd;
        p->texturePageOffset      = 2;
        p->clutRowOffset          = 4;
        t0->workType              = ENEMY_WORK_PLAIN;
        request.context.loc.stage = 0;
        request.context.loc.area  = 0x2A;
        request.command           = arg0->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(t00, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        arg0->state += 1;
    }
}

static void func_shelter_b3_dumping_hole_80183C38(Task* arg0)
{
    OverlayEncounterSingleWork* work = arg0->work;

    if (work->enemy->hp <= 0) {
        D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(arg0);
    }
}

/// Advances the task to its next state.
static void func_shelter_b3_dumping_hole_80183C8C(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

static void func_shelter_b3_dumping_hole_80183CA0(Task* arg0)
{
    ActorCommand              request;
    OverlayEncounterPairWork* work = arg0->work;
    Enemy*                    t0   = work->enemy0;

    if (t0 != NULL) {
        Task*      t00            = t0->task;
        TmdObject* p              = t00->extra.tmd;
        p->texturePageOffset      = 3;
        p->clutRowOffset          = 5;
        t0->workType              = ENEMY_WORK_PLAIN;
        request.context.loc.stage = 0;
        request.context.loc.area  = 0x2E;
        request.command           = arg0->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(t00, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
    }
    work->frames = 0;
    arg0->state += 1;
}

static void func_shelter_b3_dumping_hole_80183D34(Task* arg0)
{
    OverlayEncounterPairWork* work = arg0->work;
    Enemy*                    t    = work->enemy1;

    _madChaserWavePairDropDead(arg0);
    if (work->enemy1 != NULL) {
        if (++work->frames <= 60) {
            return;
        }
        {
            Task*        t00 = work->enemy1->task;
            TmdObject*   p   = t00->extra.tmd;
            ActorCommand request;
            p->texturePageOffset      = 3;
            p->clutRowOffset          = 5;
            t->workType               = ENEMY_WORK_PLAIN;
            request.context.loc.stage = 0;
            request.context.loc.area  = 0x2E;
            request.command           = arg0->spawnArg1.value;
            TASK_MESSAGE_DISPATCH_POINTER(t00, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        }
    }
    work->frames = 0;
    arg0->state += 1;
}

static void func_shelter_b3_dumping_hole_80183E08(Task* arg0)
{
    OverlayEncounterPairWork* work = arg0->work;

    _madChaserWavePairDropDead(arg0);
    if (work->goneMask == OVERLAY_ENCOUNTER_PAIR_GONE_BOTH) {
        D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(arg0);
    }
}

static void func_shelter_b3_dumping_hole_80183E6C(s16 arg0, s16 arg1, s16 arg2)
{
    switch (arg1) {
        case 0:
            taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 1, (arg0 << 16) + arg2, 0);
            break;
        case 1:
            taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 2, (arg0 << 16) + arg2, 0);
            break;
        case 2:
            taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 3, (arg0 << 16) + arg2, 0);
            break;
    }
}

/// Forgets dead encounter-pair enemies and marks their slots gone on the next check.
///
/// Only HP at or below zero clears a non-NULL borrowed enemy pointer. A
/// pointer already NULL sets its goneMask bit; clearing and marking occur
/// on successive calls. Enemy tasks remain owned by the actor system.
static void _madChaserWavePairDropDead(Task* task)
{
    OverlayEncounterPairWork* work = task->work;

    if (work->enemy0 != NULL) {
        if (work->enemy0->hp <= 0) {
            work->enemy0 = NULL;
        }
    } else {
        work->goneMask |= OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY0;
    }
    if (work->enemy1 != NULL) {
        if (work->enemy1->hp <= 0) {
            work->enemy1 = NULL;
        }
    } else {
        work->goneMask |= OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY1;
    }
}

void shelterB3DumpingHoleDrawViewGlowsTask(Task* task)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE    = 0x200,
        SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE = 0x280,
        SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE       = 0x300,
        SHELTER_B3_DUMPING_HOLE_GLOW_LARGE_DISC_RADIUS_SCALE = 0x400,
        SHELTER_B3_DUMPING_HOLE_GLOW_DIM_RED                 = 0x100,
        SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED              = 0x200,
        SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED              = 0x300,
        SHELTER_B3_DUMPING_HOLE_GLOW_RED                     = 0x400,
        SHELTER_B3_DUMPING_HOLE_GLOW_GREY                    = 0x444,
        SHELTER_B3_DUMPING_HOLE_GLOW_CYAN                    = 0x44,
        SHELTER_B3_DUMPING_HOLE_GLOW_GREEN                   = 0x40,
    };
    u8 view;

    if (task->state == 0) {
        D_shelter_b3_dumping_hole_8018F4D8 = 0;
        task->state                        = 1;
    }

    // Each mapped view selects only the visible subset of the room's world points.
    view = viewGetMappedIndex();
    switch (view) {
        case 2:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_DIM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[34], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[35], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[36], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 3:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[34], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[35], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 4:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[28], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[29], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[37], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 7:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[34], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[35], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[36], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[37], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 14:
            if (D_shelter_b3_dumping_hole_8018F4D8 != 0) {
                glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[30], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_CYAN);
                glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[31], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREEN);
            }
            break;
        case 15:
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[0], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[2], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_DIM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[34], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[35], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 17:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[36], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 18:
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[10], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[28], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[29], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[35], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[36], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[37], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            break;
        case 19:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 21:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[24], SHELTER_B3_DUMPING_HOLE_GLOW_LARGE_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[25], SHELTER_B3_DUMPING_HOLE_GLOW_LARGE_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            break;
        case 22:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[30], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_CYAN);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[31], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREEN);
            break;
        case 23:
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[0], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 26:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[26], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[27], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 29:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[28], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[29], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[30], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_CYAN);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[31], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREEN);
            break;
        case 30:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[28], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[29], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[30], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_CYAN);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[31], SHELTER_B3_DUMPING_HOLE_GLOW_SMALL_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREEN);
            break;
        case 31:
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[0], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[2], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[4], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[12], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[14], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[24], SHELTER_B3_DUMPING_HOLE_GLOW_LARGE_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[26], SHELTER_B3_DUMPING_HOLE_GLOW_LARGE_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[34], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[38], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[39], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[40], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 34:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[34], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[35], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[36], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[37], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 35:
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[38], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[39], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            break;
        case 13:
        case 37:
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[0], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[2], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[4], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[6], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[12], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[14], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            _glowDrawCapsule(&D_shelter_b3_dumping_hole_8018B86C[16], SHELTER_B3_DUMPING_HOLE_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[24], SHELTER_B3_DUMPING_HOLE_GLOW_LARGE_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[25], SHELTER_B3_DUMPING_HOLE_GLOW_LARGE_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[26], SHELTER_B3_DUMPING_HOLE_GLOW_LARGE_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[27], SHELTER_B3_DUMPING_HOLE_GLOW_LARGE_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_GREY);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[32], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_DIM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[33], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[34], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[35], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[38], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_DIM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[39], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_MEDIUM_RED);
            glowDrawDisc(&D_shelter_b3_dumping_hole_8018B86C[40], SHELTER_B3_DUMPING_HOLE_GLOW_DISC_RADIUS_SCALE, SHELTER_B3_DUMPING_HOLE_GLOW_BRIGHT_RED);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"

#include "../../shared/effect_sprite_drift_aimed.inc.c"

#include "../../shared/effect_sprite_draw_banked.inc.c"

#include "../../shared/effect_sprite_draw_rotated.inc.c"

void shelterB3DumpingHoleGluttonRainParticleTask(Task* task)
{
    enum {
        SHELTER_B3_DUMPING_HOLE_RAIN_SETUP                = 0,
        SHELTER_B3_DUMPING_HOLE_RAIN_CHIP                 = 1,
        SHELTER_B3_DUMPING_HOLE_RAIN_BILLBOARD            = 2,
        SHELTER_B3_DUMPING_HOLE_RAIN_SIZE_MASK            = 0xFFF,
        SHELTER_B3_DUMPING_HOLE_RAIN_ANGLE_MASK           = 0xFFF,
        SHELTER_B3_DUMPING_HOLE_RAIN_PERIOD_BITS          = 0xF000,
        SHELTER_B3_DUMPING_HOLE_RAIN_PERIOD_SHIFT         = 12,
        SHELTER_B3_DUMPING_HOLE_RAIN_NIBBLE_MASK          = 0xF,
        SHELTER_B3_DUMPING_HOLE_RAIN_DRAWER_BITS          = 0xF0000000,
        SHELTER_B3_DUMPING_HOLE_RAIN_SPEED_BITS           = 0xFF0000,
        SHELTER_B3_DUMPING_HOLE_RAIN_SPEED_SHIFT          = 16,
        SHELTER_B3_DUMPING_HOLE_RAIN_SPEED_MASK           = 0xFF,
        SHELTER_B3_DUMPING_HOLE_RAIN_DEFAULT_SPEED        = 64,
        SHELTER_B3_DUMPING_HOLE_RAIN_STILL                = 0,
        SHELTER_B3_DUMPING_HOLE_RAIN_RANDOM_UPWARD        = 1,
        SHELTER_B3_DUMPING_HOLE_RAIN_RANDOM_ALL_AXES      = 2,
        SHELTER_B3_DUMPING_HOLE_RAIN_RANDOM_NARROW_UPWARD = 3,
        SHELTER_B3_DUMPING_HOLE_RAIN_OFFSET_DIRECTION     = 5,
        SHELTER_B3_DUMPING_HOLE_RAIN_GRAVITY              = 6,
        SHELTER_B3_DUMPING_HOLE_RAIN_FRAME_COUNT          = 8,
        SHELTER_B3_DUMPING_HOLE_RAIN_UPWARD_Y_BITS        = 0xFFC0,
    };
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    velocity;
    s32         velocityKind;
    s32         framePeriod;
    s32         drawState;
    s32         speed;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    // Suspended effects redraw without advancing motion or the animation cursor.
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Suspended effects redraw without advancing motion or the animation cursor.
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state < SHELTER_B3_DUMPING_HOLE_RAIN_BILLBOARD) {
                _effectSpriteDrawChip(coord, work->index, work->scale, work->angle);
            } else {
                _effectSpriteDrawBillboard(coord, (u16)work->index, work->scale);
            }
            return;
        }
        effectKillTask(work, task);
        return;
    }
    work->age++;
    switch (task->state) {
        case SHELTER_B3_DUMPING_HOLE_RAIN_SETUP:
            work->scale     = task->spawnArg1.halves.low & SHELTER_B3_DUMPING_HOLE_RAIN_SIZE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & SHELTER_B3_DUMPING_HOLE_RAIN_ANGLE_MASK;
            if (task->spawnArg1.value & SHELTER_B3_DUMPING_HOLE_RAIN_PERIOD_BITS) {
                framePeriod = (task->spawnArg1.value >> SHELTER_B3_DUMPING_HOLE_RAIN_PERIOD_SHIFT) & 0xF;
            } else {
                framePeriod = 1;
            }
            work->period = framePeriod;
            work->age    = 0;
            drawState    = SHELTER_B3_DUMPING_HOLE_RAIN_CHIP;
            if (task->spawnArg1.value & SHELTER_B3_DUMPING_HOLE_RAIN_DRAWER_BITS) {
                drawState = SHELTER_B3_DUMPING_HOLE_RAIN_BILLBOARD;
            }
            task->state = drawState;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1.value & SHELTER_B3_DUMPING_HOLE_RAIN_SPEED_BITS) {
                    speed = (task->spawnArg1.value >> SHELTER_B3_DUMPING_HOLE_RAIN_SPEED_SHIFT) & SHELTER_B3_DUMPING_HOLE_RAIN_SPEED_MASK;
                } else {
                    speed = SHELTER_B3_DUMPING_HOLE_RAIN_DEFAULT_SPEED;
                }
                work->step   = speed;
                velocityKind = task->spawnArg1.signedBytes[3];
                switch (velocityKind & SHELTER_B3_DUMPING_HOLE_RAIN_NIBBLE_MASK) {
                    case SHELTER_B3_DUMPING_HOLE_RAIN_STILL:
                        work->step = 0;
                        break;
                    case SHELTER_B3_DUMPING_HOLE_RAIN_RANDOM_UPWARD:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = SHELTER_B3_DUMPING_HOLE_RAIN_UPWARD_Y_BITS - ((gRandomLcgState >> 16) & 0x7F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case SHELTER_B3_DUMPING_HOLE_RAIN_RANDOM_ALL_AXES:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case SHELTER_B3_DUMPING_HOLE_RAIN_RANDOM_NARROW_UPWARD:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        break;
                    case SHELTER_B3_DUMPING_HOLE_RAIN_OFFSET_DIRECTION:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                // Normalize the selected direction and scale it to units per running tick.
                velocity = &work->move;
                VectorNormalSS(velocity, velocity);
                gte_lddp(work->step);
                gte_ldsv(velocity);
                gte_gpf12();
                gte_stsv(velocity);
            } else {
                work->step = SHELTER_B3_DUMPING_HOLE_RAIN_DEFAULT_SPEED;
            }
            return;
        case SHELTER_B3_DUMPING_HOLE_RAIN_CHIP:
            _effectSpriteDrawChip(coord, work->index, work->scale, work->angle);
            break;
        case SHELTER_B3_DUMPING_HOLE_RAIN_BILLBOARD:
            _effectSpriteDrawBillboard(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += SHELTER_B3_DUMPING_HOLE_RAIN_GRAVITY;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= SHELTER_B3_DUMPING_HOLE_RAIN_FRAME_COUNT) {
            effectKillTask(work, task);
        }
    }
}

#include "../../shared/effect_sprite_draw_chip.inc.c"

#include "../../shared/effect_sprite_draw_billboard.inc.c"

void func_shelter_b3_dumping_hole_80186D4C(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    MATRIX*     m;
    s32         i;

    mem   = (EffectWork*)arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _effectSpriteDrawBillboard(coord, (mem->age / 2) & 0xFFFF, 0x380);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                                = &coord->coord;
        coord->parent                    = mem->parent;
        MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
        MATRIX_PAIR(m, 0, 2)             = 0;
        MATRIX_PAIR(m, 1, 1)             = 0x1000;
        MATRIX_PAIR(m, 2, 0)             = 0;
        m->m[2][2]                       = 0x1000;
        coord->coord.t[2]                = 0;
        coord->coord.t[1]                = 0;
        coord->coord.t[0]                = 0;
        coord->composeStamp              = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        arg0->state = 1;
    }
    mem->age += 1;
    switch (arg0->spawnArg1.value) {
        case 0:
            Gp_SpawnEff(EFFECT_GLUTTON_RAIN_PARTICLE, coord, 0x14002400, NULL);
            arg0->spawnArg1.value = 1;
            return;
        case 1:
            _effectSpriteDrawBillboard(coord, (mem->age / 2) & 0xFFFF, 0x380);
            if (!(mem->age & 1)) {
                Gp_SpawnEff(EFFECT_GLUTTON_RAIN_PARTICLE, coord, 0x1001400, NULL);
            }
            mem->age += 1;
            return;
        case 2:
            Gp_SpawnEff(EFFECT_GLUTTON_RAIN_PARTICLE, coord, 0x10002380, NULL);
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(EFFECT_GLUTTON_RAIN_PARTICLE, coord, 0x2002400, NULL);
                Gp_SpawnEff(EFFECT_SHELTER_B3_DUMPING_HOLE_DRIFT_SPRITE, coord, 0x2202300, NULL);
            }
            arg0->spawnArg1.value = 3;
            return;
        case 3:
            effectKillTask(mem, arg0);
            return;
    }
}
