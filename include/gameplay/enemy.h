#ifndef GAMEPLAY_ENEMY_H
#define GAMEPLAY_ENEMY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/areaplace.h"
#include "gameplay/enemy_params.h"
#include "gameplay/world_targets_types.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

/// Bit of `Enemy::reactionFlags` requesting an immediate actor stagger.
///
/// The actor consumes the request when selecting its stagger response.
#define ENEMY_REACTION_STAGGER 1

/// Bit of `Enemy::reactionFlags` requesting or retaining a buildup/stun reaction.
///
/// Starting the reaction resets its step and timer bytes. Actors choose when
/// to consume this bit; their timed hold can continue after it is cleared.
#define ENEMY_REACTION_BUILDUP 2

/// Bit of `Enemy::reactionFlags` marking active poison damage over time.
///
/// Activation restarts the pulse count and delay. Expiry requires this exact
/// bit, even though actor pulse handling tests the broader group below.
#define ENEMY_REACTION_DAMAGE_OVER_TIME 4

/// Bits of `Enemy::reactionFlags` tested together for poison pulses and tint.
///
/// Includes active poison (0x4) and bit 0x8, whose independent role is unproven.
/// Both are cleared on expiry; no recovered setter activates 0x8.
#define ENEMY_REACTION_DAMAGE_OVER_TIME_BITS 0xC

/// Positive int byte mask clearing only the stagger request.
#define ENEMY_REACTION_STAGGER_CLEAR (0xFF ^ ENEMY_REACTION_STAGGER)

/// Positive int byte mask clearing only the buildup reaction bit.
#define ENEMY_REACTION_BUILDUP_CLEAR (0xFF ^ ENEMY_REACTION_BUILDUP)

/// Positive int byte mask clearing the entire poison-consumer group (0x4 and 0x8).
#define ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR (0xFF ^ ENEMY_REACTION_DAMAGE_OVER_TIME_BITS)

/// Positive int byte mask clearing all four low reaction bits during actor-part death.
///
/// Preserves bits 4..7 without assigning them a status meaning.
#define ENEMY_REACTION_LOW_CLEAR (ENEMY_REACTION_STAGGER_CLEAR & ENEMY_REACTION_BUILDUP_CLEAR & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR)

/// Colour-mode value retaining sampled directional lighting by default.
///
/// World colour queries first apply an eligible targeting flash, otherwise a
/// poison-group reaction remaps each weighted light column to RGB (3,1,3).
/// With neither effect, directional coefficients and ambient RGB stay intact.
#define ENEMY_COLOR_DEFAULT 0

/// Colour-mode value selecting a weighted directional-light column remap.
///
/// World colour queries compute (7R+6G+3B)/33 per column and store RGB (4,2,1)
/// times that value, truncating division and narrowing to signed halfwords.
/// Ambient RGB is preserved and targeting flashes are not consumed in this
/// mode. Gallery targets instead add a sine pulse to the weighted base and
/// replicate it to RGB.
#define ENEMY_COLOR_WEIGHTED 1

/// Colour-mode value zeroing the nine directional-light RGB coefficients.
///
/// Preserves the matrix's ambient RGB translation. An eligible targeting
/// flash overrides the zeroing in world colour queries.
#define ENEMY_COLOR_BLACK 2

/// Colour-mode value filling each directional-light column with a fixed red tint.
///
/// World colour queries store Q12 RGB (0x180,0x100,0x100), preserving ambient
/// RGB, unless an eligible targeting flash overrides the fill.
#define ENEMY_COLOR_TINT 3

/// Two-bit mask selecting a current or previous mode from `Enemy::colorMode`.
///
/// Mode codes are 0..3. The setter also masks its integer input with this value.
#define ENEMY_COLOR_MODE_MASK 3

/// Shift count locating the previous colour mode in packed bits 2..3.
#define ENEMY_COLOR_PREVIOUS_SHIFT 2

/// Byte mask preserving packed colour bits 4..7 across a mode change.
///
/// Bit 7 requests the targeting flash; the roles of bits 4..6 are unproven.
/// Current and previous two-bit mode slots are replaced separately.
#define ENEMY_COLOR_KEPT_BITS (0xFF ^ (ENEMY_COLOR_MODE_MASK | (ENEMY_COLOR_MODE_MASK << ENEMY_COLOR_PREVIOUS_SHIFT)))

/// Bit of `Enemy::colorMode` requesting a green Parasite Energy targeting flash.
///
/// Preview selection sets this bit before releasing attack contacts. World
/// colour queries consume it at spawnState 0 in default, black or tint mode,
/// replacing only the directional-light coefficients. Weighted mode and the
/// gallery-specific remapper leave the request intact.
#define ENEMY_COLOR_TARGET_FLASH 0x80

/// Positive int byte mask consuming the targeting flash without changing other bits.
#define ENEMY_COLOR_TARGET_FLASH_CLEAR (0xFF ^ ENEMY_COLOR_TARGET_FLASH)

/// Ordinary actor spawn kind in `Enemy::workType`: bank 9, subtype 0.
///
/// Used by area placements and dynamic targets, projectiles and summons.
/// Scene-child operations recognize the high byte as their placed-actor bank;
/// the value does not establish that an actor has a placement-table record.
#define ENEMY_WORK_ACTOR 0x900

/// Bit position of the instance's placement index in `Enemy::placeKey`.
///
/// Shifting the unsigned 16-bit key right by this count yields an index in
/// 0..15. Area spawns use the zero-based `AreaPlacement` table position;
/// dynamic spawns can assign their own instance slot. Packing an index uses
/// `index << ENEMY_PLACE_INDEX_SHIFT`; the index must fit four bits. The
/// remaining bits hold the stage (8..11) and area (0..7).
/// An extracted index must also fit any actor-specific table it selects.
#define ENEMY_PLACE_INDEX_SHIFT 12

/// Left-shift count positioning a placement's stage nibble in key bits 8..11.
///
/// Area placement keys pack a four-bit stage and eight-bit area below the
/// instance nibble. `AreaObjectPlace` spawns use this same shift for the
/// record's high key fragment: its low eight bits become key bits 8..15,
/// while the low key byte holds a flag-bank index rather than an area.
#define ENEMY_PLACE_STAGE_SHIFT 8

/// One enemy's gameplay work, hung off the task that owns it through `Task::spawnArg2`.
///
/// The actor keeps its own state elsewhere and publishes here the coordinate
/// and body position it moves through, its hit points, the placement it was
/// spawned from, and the parameter record of its kind. Gameplay finds the
/// enemy on the tracked-target list through `node`, and tells one enemy from
/// another by `placeKey`. Every child of the scene task carries one, so the
/// scene's searches select children by `placeKey` and by the bank in
/// `workType`. Leaving a room saves the pose under that key;
/// `spawnState` is the state the spawn handler resumes in, so the enemy
/// returns where it was.
///
/// A landed hit can request a reaction in `reactionFlags`. Stagger is immediate.
/// Buildup counts steps of 31 tick calls and then a random
/// countdown. Damage over time spends a fraction of `hpMax` on each pulse.
/// Lengths and that fraction come from `param`, scaled by the attack's grade.
typedef struct Enemy {
    Task*                  task;                // Owning task; the one whose `spawnArg2` is this object
    MATRIX*                field_4;             // Model-part matrix stored at spawn. Nothing reads it back; role unproven
    u16                    placeKey;            // Placement identity: area, stage, then the placement index in the high nibble (`ENEMY_PLACE_INDEX_SHIFT`)
    u16                    workType;            // Spawn kind: bank in the high byte, subtype in the low. `ENEMY_WORK_ACTOR`, or `AreaObjectPlace.kind`
    s32                    waitTicks;           // Countdown calls left before an enemy with no actor body is destroyed; starts at 120
    WorldTargetNode        node;                // Tracked-target entry. Lock-on, radar, area scans, the HP readout and damage reactions reach the enemy through it
    GfxCoord*              coord;               // Coordinate the body sits at, usually one of the actor's model parts
    VECTOR3                bodyPos;             // Body position in `coord`'s frame: the point distance and damage-chance rolls measure from
    byte                   field_28[4];         // Unread. Role unproven
    VECTOR3                playerRelPos;        // `bodyPos` in world space, relative to the player, refreshed each frame. Aim and lock-on take angle and distance from it
    byte                   field_38[4];         // Unread. Role unproven
    AreaPlacement*         place;               // Placement record the spawn parameters come from. An actor may publish a table of its own here
    s16                    hp;                  // Hit points left. Damage subtracts from it; the readout shows it against `hpMax`
    u16                    hpMax;               // Hit points the enemy is spawned with. Damage over time deals a fraction of this
    byte                   field_44[4];         // Unread. Role unproven
    u8                     field_48;            // Cleared at every spawn. Nothing reads it back; role unproven
    byte                   field_49[2];         // Unread. Role unproven
    u8                     spawnState;          // State saved with the pose and restored on the next spawn. A targeting flash is drawn only while this is 0
    u8                     reactionFlags;       // Reactions a landed hit asked for (`ENEMY_REACTION_STAGGER`, `ENEMY_REACTION_BUILDUP`, `ENEMY_REACTION_DAMAGE_OVER_TIME`)
    u8                     field_4D;            // Cleared beside `reactionFlags` on spawn. Nothing reads it back; role unproven
    u8                     colorMode;           // Colour remap: current mode in bits 0-1, previous mode in bits 2-3, `ENEMY_COLOR_TARGET_FLASH` in bit 7
    u8                     colorBlend;          // Frames left in a mode blend, counted down while the scene runs and read as a signed byte. 0 switches at once
    EnemyParams*           param;               // Parameters of this enemy's kind, shared by every enemy of that kind. `NULL` where the kind has none
    WorldCollisionContact* recs;                // This enemy's contact records. Its collision bodies point at the table, and Parasite Energy targeting claims entries in it
    u8                     buildupStep;         // Buildup steps completed, each 31 tick calls long, up to `param->buildupSteps` scaled by `buildupGrade`
    u8                     damageOverTimeDelay; // Frames until the next damage-over-time pulse, reseeded after each one
    u8                     damageOverTimePulse; // Damage-over-time pulses dealt. Expires at `param->damageOverTimeTicks` scaled by `damageOverTimeGrade`; 0 base pulses never expire
    u8                     buildupTimer;        // Frames into the current buildup step. After the last step, a countdown that ends the reaction at 0
    u8                     damageOverTimeGrade; // Row of the damage-over-time length and fraction tables. 0 when the attack has no grade bit; otherwise the ones digit of the attachment state
    u8                     buildupGrade;        // Row of the buildup length table. Same source as `damageOverTimeGrade`, and also 0 when the buildup attack's low bits are 0x31
    byte                   pad_5E[2];           // Brings the object up to its 4-byte-aligned size
} Enemy;
STATIC_ASSERT_SIZEOF(Enemy, 0x60);

/// Releases an enemy work object and begins default teardown of its owning task.
///
/// Both arguments must be non-NULL and live, with `enemy` a primary-heap
/// allocation owned by `task` through `spawnArg2.pointer`. Target tracking and
/// actor locks are detached before the enemy allocation is freed. The target
/// entry may still be unlinked when a spawn fails.
///
/// Callers must first release actor-specific list links and nested resources.
/// This calls `taskKill` directly, bypassing a replacement exit callback;
/// task and model release follow its immediate/deferred lifetime rules.
/// The enemy is invalid on return, and `spawnArg2.pointer` is left unchanged.
/// Callers must not access the enemy or task again after this call.
void enemyDestroy(Enemy* enemy, Task* task);

/// Releases a task's enemy work object and begins default task teardown.
///
/// The one-argument `TaskFunc` form of `enemyDestroy`. `task` must be non-NULL,
/// live and not already torn down, with a live primary-heap `Enemy` allocation
/// in `spawnArg2.pointer`. A failed spawn's not-yet-linked enemy is valid.
/// Release actor-specific links and nested resources before calling.
///
/// Target tracking and actor locks are detached before the enemy is freed,
/// then `taskKill` handles children, work and immediate/deferred body release.
/// Calling directly bypasses a replacement exit callback. `spawnArg2.pointer`
/// is left dangling; callers must not access the task or enemy afterwards.
void enemyTaskExit(Task* task);

/// Enemy state handler. Takes the enemy work object and the task that owns it,
/// and returns nothing.
///
/// A task body or a per-frame tick selects one from a table by `Task::state`
/// or by an actor substate and calls it with that pair. The enemy is the
/// object stored in the task's `spawnArg2.pointer`. Both pointers are live on
/// entry. A teardown handler may release them before returning; the caller
/// must not use either afterwards.
typedef void (*EnemyTaskFunc)(Enemy* enemy, Task* task);

/// Three `EnemyTaskFunc` handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles. The selector may be a task state or a
/// work substate. Dispatch requires an index in 0..2 and a non-NULL entry,
/// which receives the live enemy and the task that owns it. There is no
/// terminator or bounds check in the table. Copying it copies callback
/// pointers, not enemy or task storage; the callback code must remain loaded
/// for the call. A handler may release either argument before returning.
typedef struct {
    EnemyTaskFunc funcs[3]; // Handlers in selector order; slot meanings belong to each table
} EnemyTaskFuncTable3;
STATIC_ASSERT_SIZEOF(EnemyTaskFuncTable3, 0xC);

/// Four `EnemyTaskFunc` handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles. The selector may be a task state or a
/// work substate. Dispatch requires an index in 0..3 and a non-NULL entry,
/// which receives the live enemy and the task that owns it. There is no
/// terminator or bounds check in the table. Copying it copies callback
/// pointers, not enemy or task storage; the callback code must remain loaded
/// for the call. A handler may release either argument before returning.
typedef struct {
    EnemyTaskFunc funcs[4]; // Handlers in selector order; slot meanings belong to each table
} EnemyTaskFuncTable4;
STATIC_ASSERT_SIZEOF(EnemyTaskFuncTable4, 0x10);

/// Five `EnemyTaskFunc` handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles. The selector is the task state.
/// Dispatch requires an index in 0..4 and a non-NULL entry, which receives
/// the live enemy and the task that owns it. There is no terminator or bounds
/// check in the table. Copying it copies callback pointers, not enemy or task
/// storage; the callback code must remain loaded for the call. A handler may
/// release either argument before returning.
typedef struct {
    EnemyTaskFunc funcs[5]; // Handlers in selector order; slot meanings belong to each table
} EnemyTaskFuncTable5;
STATIC_ASSERT_SIZEOF(EnemyTaskFuncTable5, 0x14);

#endif // GAMEPLAY_ENEMY_H
