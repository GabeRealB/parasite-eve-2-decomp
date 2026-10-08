#ifndef INCLUDE_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H
#define INCLUDE_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H

#include "types.h"
#include "overlay.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern OverlayEncounterSpot D_shelter_b3_garbage_incinerator_801874C4[16];

extern u16 D_shelter_b3_garbage_incinerator_801855DE;

extern TaskDesc D_shelter_b3_garbage_incinerator_80187150[4];

/// Mutable logical-view map shared by incinerator rooms 1 and 3.
///
/// Forty one-based camera/image/sprite indices, indexed by logical view minus
/// one. Initially identity; the encounter switches logical view 2 between
/// mapped views 2 and 17. The room overlay owns the storage while loaded.
extern u8 gShelterB3GarbageIncineratorRoom1And3ViewMap[40];

/// Logical-view byte offset and the two mappings switched by the encounter.
enum {
    SHELTER_B3_GARBAGE_INCINERATOR_ENCOUNTER_VIEW_OFFSET = 1,
    SHELTER_B3_GARBAGE_INCINERATOR_ENCOUNTER_NORMAL_VIEW = 2,
    SHELTER_B3_GARBAGE_INCINERATOR_ENCOUNTER_REVEAL_VIEW = 17,
};

extern AreaApplyRec D_shelter_b3_garbage_incinerator_8018FB6C[23];

extern u16 D_shelter_b3_garbage_incinerator_8018FBC8[2];

extern AreaVariant D_shelter_b3_garbage_incinerator_8018FA58[13];

// shelter_b3_garbage_incinerator
/// Garbage-incinerator model-lighting descriptors indexed by room minus one.
///
/// Rooms 1..3 borrow the first light collection and 4..7 the second. All seven
/// use default ambient minima (NULL tables). The overlay owns descriptors and
/// mutable light arrays; their pointers must not survive unloading it.
extern WorldCoordRoomLighting gShelterB3GarbageIncineratorRoomLighting[7];

extern WorldCollisionRoomResources D_shelter_b3_garbage_incinerator_801872B8[];

/// Logical-view map directory for incinerator rooms 1..7.
///
/// Every map has forty entries selected by logical view minus one; bytes are
/// one-based camera/image/sprite indices. Rooms 1/3 and 4/6 share their maps.
/// All maps belong to the loaded room overlay.
extern u8* gShelterB3GarbageIncineratorViewMaps[7];

extern ViewCount D_shelter_b3_garbage_incinerator_8018740C[];

extern DirectionWarpEntry D_shelter_b3_garbage_incinerator_8018741C[];

extern ViewCamera D_shelter_b3_garbage_incinerator_801883AC[];

extern SpriteView D_shelter_b3_garbage_incinerator_8018D100[];

extern WorldCollisionSurfaceProperties* D_shelter_b3_garbage_incinerator_8018FB4C[];

void func_shelter_b3_garbage_incinerator_8017DC7C(Task* task);

/// Caption selectors and task-tick duration used by the lift and exit encounter.
enum {
    SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_LIFT_ACTION    = 5,
    SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_EXIT_BLOCKED   = 22,
    SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_EXIT_ENCOUNTER = 23,
    SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_DEFAULT_KEY    = 0,
    SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_NOTICE_TICKS   = 60,
};

/// Selects an incinerator caption and starts its timed drawing task at baseline 208.
///
/// Requires the room's relocated CAP resource and font textures to remain loaded.
/// `commandIndex` selects a valid command-table entry; `key` must identify a
/// nonterminal record in 0..255. `durationTicks` counts drawing-task ticks;
/// nonpositive durations still draw once. Later selections replace the text
/// seen by every caption task. A failed spawn leaves the selection in place.
void shelterB3GarbageIncineratorShowTimedCaption(s16 commandIndex, s16 key, s16 durationTicks);

/// Installs the six low collision walls used during the incinerator exit sequence.
///
/// Requires this room's active writable `WorldCollisionGrid`: vertices 0..23,
/// faces/normals 0..5, plus vertices 24..31 and faces/normals 6..7 for variant 2.
/// The exit layout forms two separated wall groups spanning grid-local Y=-400..0
/// in game units. Surface class 1 passes probes, ignores weapon impacts and
/// applies pushback. Unit normals use 4096; existing cell lists are retained.
/// Storage is borrowed for the call and remains owned by the loaded room.
void shelterB3GarbageIncineratorSetExitCollisionWalls(void);

/// Installs the six low collision walls used before the lift's first move.
///
/// Requires this room's active writable `WorldCollisionGrid`: vertices 0..23,
/// faces/normals 0..5, plus vertices 24..31 and faces/normals 6..7 for variant 2.
/// Walls span grid-local Y=-400..0 in game units and use surface class 1
/// (passes probes, ignores weapon impacts, applies pushback). Unit normals use
/// 4096; existing cell lists are retained. Also restores this layout for the
/// boss reset and the lift's second move. Storage remains owned by the room.
void shelterB3GarbageIncineratorSetLiftCollisionWalls(void);

/// Draws the current view's incinerator lamps and pulsing red glows each frame.
///
/// Gameplay effect slot 0x144 supplies a live `EffectWork` in `spawnArg2.pointer`.
/// Its signed halfword `scale` stores a packed RGB-nibble warning colour:
/// orange before the lift's first move, alternating orange/blue every two
/// animation frames during that move, then blue. Enables room-effect view mode;
/// the current mapped camera index selects the lamps, with an extra rotating
/// glow during the exit warp. The task neither advances state nor frees work;
/// the counted effect's exit callback owns cleanup. View matrices, scratch
/// stack and the current frame's packet arena must be ready for glow drawing.
void shelterB3GarbageIncineratorDrawLightsTask(Task* task);

/// Animates a drifting sprite with an optional parent-oriented launch.
///
/// Gameplay effect slot 0x196 supplies counted, task-owned `EffectWork` in
/// `spawnArg2.pointer` and a single-coordinate body. Start at state 0, cell 0;
/// initialization consumes one running update without drawing or moving.
/// The coordinate's `workm` must be composed before drawing; GTE, scratch-stack
/// and packet-arena state must be ready.
///
/// `spawnArg1` bits 0..11 set perspective size; bits 12..15 select the frame
/// period (zero means 1, otherwise bits 12..14 must be 1..7). Bits 16..23
/// select launch speed in coordinate-parent units per update (zero means 64).
/// Bits 24..27 select direction: 0 stationary, 1 random upward, 2 all axes,
/// 3 narrow upward, 5 from `work->pos`, 6 planar, 7 parent-rotated forward direction.
/// Other kinds retain the zero direction for SDK normalization. Mode 7 borrows
/// `work->parent->coord`'s rotation, which must map into the effect's parent
/// space. A nonzero initial `move` bypasses generation and speed scaling.
///
/// Bit 31 selects the ten-cell alternate sheet; clear selects the twelve-cell
/// banked sheet. Bits 28..30 become palette bits in `pos.vx`, replacing offset X
/// before mode 5 copies its direction. Spin is randomized in 4096 units per turn.
/// Running updates draw, move, then accelerate Y: mode 7 adds age/10; other
/// modes subtract 1 (alternate) or 2 (banked). Zero `step` skips movement and
/// acceleration. Animation advances once per period and releases after the
/// final cell. Suspension redraws without advancing; cancellation redraws
/// once before releasing the counted work and task through `effectKillTask`.
void shelterB3GarbageIncineratorEffectSpriteDriftTaskAimed(Task* task);

/// Animates one eight-cell debris sprite from the incinerator's effect task slot.
///
/// Requires a live counted effect with owned `EffectWork` in `spawnArg2.pointer`
/// and a single-coordinate body. The coordinate's `workm` must be composed for
/// drawing. Start at state 0 and cell `index` 0..7; states 1/2 draw chip/billboard.
/// Initialization takes one running update without drawing, moving or advancing.
///
/// `spawnArg1` bits 0..11 set perspective size (0..4095); bits 12..15 give
/// running updates per cell (0 means 1); bits 16..23 give launch speed in parent
/// coordinate units per update (0 means 64). Bits 24..27 choose direction:
/// 0 stationary, 1 random upward, 2 random on all axes, 3 narrow upward, 5 from
/// `work->pos`. Other kinds leave the zero direction for SDK normalization.
/// Any set bit in 28..31 selects the billboard. Spin is randomized once in
/// 4096 units per turn. A nonzero initial `move` is retained as velocity and
/// enables movement regardless of the packed speed or direction.
///
/// Running updates draw, move by signed halfword velocity, then add 6 to Y
/// velocity and advance the cell every period. Stationary sprites skip both
/// movement and gravity. Suspension redraws a chip without advancing; cancellation
/// redraws that chip once and releases the counted work and task. Finishing cell
/// 7 also releases them. Drawers require initialized GTE, scratch and packet state.
void shelterB3GarbageIncineratorEffectSpriteDebrisTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H
