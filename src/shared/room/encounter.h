/* Scripted encounters: the table a room or an actor lists its enemies' entrances in, the spots they enter at, and the
 * work blocks of the tasks that spawn them. */
#ifndef SRC_SHARED_ROOM_ENCOUNTER_H
#define SRC_SHARED_ROOM_ENCOUNTER_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include "common.h"
#include "engine/task_types.h"
#include "gameplay/enemy.h"

/// Values of `EncounterSlot::status`.
enum {
    ENCOUNTER_SLOT_WAITING = 0, // not started, or its spawner could not spawn
    ENCOUNTER_SLOT_LIVE    = 1, // its spawner holds at least one enemy
    ENCOUNTER_SLOT_DONE    = 2  // its enemies are dead or were removed
};

/// Shared encounter actions carried in `ActorCommand::command`.
enum {
    /// Reveals a hidden enemy at a room entry spot.
    ///
    /// The packed word also selects a spot and, for Mad Chasers, an entry move.
    /// Slouch and Sucklerceph require the entire low byte to equal this action;
    /// their entry-move bits must therefore be zero.
    ENCOUNTER_COMMAND_APPEAR = 1,

    /// Suspends an encounter controller's feeding and completion stages.
    ///
    /// Send the exact word 4, with no packed spot or entry move. The controller
    /// keeps its task and battle hold; already started row spawners continue.
    /// When also broadcast to enemies, Mad Chasers request drop death, Slouch
    /// starts puffing, and Sucklerceph starts puffing only in active task state 1.
    ENCOUNTER_COMMAND_STOP = 4
};

/// Packs an appearance command for an encounter slot or a direct actor request.
///
/// Action 1 occupies bits 0..3, `entryMove` bits 4..7 and `spot` bits 8..11.
/// Supply nonnegative integer arguments in 0..15; each is evaluated once, in
/// unspecified order, with ordinary integer promotions and no masking or cast.
/// Constant arguments produce an integer constant expression for slot tables.
///
/// `spot` must name a loaded `EncounterSpot` row: 0..11 in the dumping
/// hole or 0..15 in the incinerator. Slouch and Sucklerceph read the full high
/// byte as the index, so bits 12..15 must stay zero. Mad Chaser entry moves are
/// 0 backflip, 1 low backward arc, and 2..15 high arc; other enemies require 0.
/// Pass spot 0 when OR-ing a separately selected, already shifted spot later.
#define ENCOUNTER_APPEAR_COMMAND(spot, entryMove) (((spot) << 8) | ((entryMove) << 4) | ENCOUNTER_COMMAND_APPEAR)

/// One row of a scripted encounter's table: a spawner task its controller
/// starts. The first three rows start together and each later one, in table
/// order, once fewer than three rows are live.
///
/// The spawner started for row `i` gets `(i << 16) + command` as its spawn
/// argument. It spawns its enemies hidden, brings each out after a delay by
/// sending it the low half of that argument as an `ActorCommand`, and writes
/// `status` back through the row index in the high half. The encounter is over
/// once every row is done.
typedef struct {
    s16  kind;       // Spawner the row starts (0 one Mad Chaser, 1 one of the Sucklerceph package's other enemy, 2 a pair of Sucklercephs)
    s16  command;    // Command its enemies are brought out with: action in bits 0..3, entry move in 4..7, spot row in 8..11
    byte field_4[2]; // Never read or written, zero in every table; role unproven
    s16  status;     // Progress of the row (`ENCOUNTER_SLOT_`); the controller resets it when it starts
} EncounterSlot;
STATIC_ASSERT_SIZEOF(EncounterSlot, 0x8);

/// A place where a scripted encounter's enemy enters its room: the position
/// its root coordinate is moved to and the yaw it comes out along.
///
/// A room keeps one table of these. The command an `EncounterSlot`
/// sends its enemy names a row by its zero-based position in bits 8..11, and
/// the enemy looks that row up in the table of the room it is in. Each enemy
/// derives its own facing from `heading`.
typedef struct {
    s16 x;       // World X the enemy's root coordinate is moved to
    s16 y;       // World Y the enemy's root coordinate is moved to
    s16 z;       // World Z the enemy's root coordinate is moved to
    u16 heading; // Yaw the enemy comes out along, 4096 units per turn, in 0..4095
} EncounterSpot;
STATIC_ASSERT_SIZEOF(EncounterSpot, 0x8);

/// Work block of a scripted encounter's controller: the task that walks the
/// encounter's `EncounterSlot` table, starting its first three rows
/// together and each later one once fewer than three rows are live.
///
/// The controller allocates the block zeroed and the task's teardown frees
/// it. An actor command whose action is `ENCOUNTER_COMMAND_STOP` ends
/// the encounter early: the controller records it in `stop` and from then on
/// neither starts a row nor finishes the encounter.
typedef struct {
    s16 frames;   // Frames counted after the first three rows are started; the encounter is armed when they reach 15, and the count stops there
    s16 nextSlot; // Table row the controller starts next; rows before it have been started
    s16 stop;     // `ENCOUNTER_COMMAND_STOP` once that command has arrived, 0 until then
} EncounterControllerWork;
STATIC_ASSERT_SIZEOF(EncounterControllerWork, 0x6);

/// Work block of a scripted encounter's one-enemy spawner: the task an
/// `EncounterSlot` row starts to spawn a single enemy hidden, bring it
/// out after a delay and watch it until the row is done.
///
/// The spawner allocates the block zeroed and the task's teardown frees it.
/// The enemy belongs to its own task and is only borrowed here.
typedef struct {
    Enemy* enemy;  // Enemy the spawner spawned hidden and then watches
    s16    frames; // Frames counted since the spawn; the enemy is brought out once they pass the spawner's delay, and the count stops there
} EncounterSingleWork;
STATIC_ASSERT_SIZEOF(EncounterSingleWork, 0x8);

/// Bits of `EncounterPairWork::goneMask`.
enum {
    ENCOUNTER_PAIR_GONE_ENEMY0 = 1 << 0, // the spawner no longer holds its first enemy
    ENCOUNTER_PAIR_GONE_ENEMY1 = 1 << 1, // the spawner no longer holds its second enemy
    ENCOUNTER_PAIR_GONE_BOTH   = ENCOUNTER_PAIR_GONE_ENEMY0 | ENCOUNTER_PAIR_GONE_ENEMY1
};

/// Work block of a scripted encounter's two-enemy spawner: the task an
/// `EncounterSlot` row starts to spawn a pair of enemies hidden, bring
/// the first out at once and the second after a delay, and watch both until
/// the row is done.
///
/// The spawner allocates the block zeroed and the task's teardown frees it.
/// Either spawn may fail, leaving that pointer null from the start; the task
/// only gives up when both do. The enemies belong to their own tasks and are
/// only borrowed here: the spawner clears a pointer once that enemy is dead or
/// has been sent away, and one tick later records it in `goneMask`.
typedef struct {
    Enemy* enemy0;   // First enemy of the pair, brought out as soon as the pair is spawned; null once it is gone or if it never spawned
    Enemy* enemy1;   // Second enemy of the pair, brought out after the spawner's delay; null once it is gone or if it never spawned
    s16    frames;   // Frames counted since the first enemy was brought out; the second follows once they pass the spawner's delay, and the count stops there
    s16    goneMask; // Which of the two the spawner has seen gone (`ENCOUNTER_PAIR_GONE_`); the row is done once both are
} EncounterPairWork;
STATIC_ASSERT_SIZEOF(EncounterPairWork, 0xC);

#endif // SRC_SHARED_ROOM_ENCOUNTER_H
