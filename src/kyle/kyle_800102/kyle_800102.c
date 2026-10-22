#include "kyle/kyle_800102.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "kyle/kyle.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"

#include "weapons/grenade_pistol.h"
#include "weapons/weapon.h"
#include "../../shared/projectiles/launcher_grenade.h"

static SVECTOR _gLauncherGrenadeMuzzleOffsets[2];
static u16     _gLauncherGrenadeBlastRadii[4];
static u8      _gLauncherGrenadeInitialFlightDivisors[4];

static void _launcherGrenadeExit(Task* task);
static void _launcherGrenadeSpawn(Task* task);

#include "../../shared/projectiles/launcher_grenade_flight.inc.c"

#include "../../shared/projectiles/launcher_grenade.inc.c"
void kyle800102GrenadeShellTask(Task* projectileTask)
{
    const TaskFunc stateHandlers[] = {
        _launcherGrenadeSpawn,
        _launcherGrenadeFly,
        _launcherGrenadeBlast,
        _launcherGrenadeExit,
    };

    stateHandlers[projectileTask->state](projectileTask);
}

/// Initial part-local pose and parent indices copied into each body model.
/// Vertex counts for the twenty consecutive part-local groups, totalling 300.
/// Body vertices in integer part-local coordinates, ordered by skeletal part.
/// Body lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands: twenty skeletal groups and a final pre-transformed group.
///
/// First model creation resolves handler slots in place; the last word is
/// `TMD_STREAM_END`. Projection commands fill the first primitive-buffer region.
#include "assets/characters/kyle/body.inc"

/// Identity rest transform for the hand's single root, copied into each model.
/// Vertex count for the single part-local hand group (23 vertices).
/// Right-hand vertices in integer coordinates local to the hand root.
/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/hand_right.inc"

/// Identity rest transform for the right hand's single root, copied into each model.
/// Vertex count for the single part-local right-hand group (27 vertices).
/// Right-hand vertices in integer coordinates local to the hand root.
/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/hand_right_3.inc"

/// Identity rest transform for the left hand's single root, copied into each model.
/// Vertex count for the left hand's single part (23 vertices).
/// Left-hand vertices in integer coordinates local to the hand root.
/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for four textured Gouraud triangles and nineteen quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/hand_left_2.inc"

/// Identity rest transform for the left hand's single root, copied into each model.
/// Vertex count for the single part-local left-hand group (27 vertices).
/// Left-hand vertices in integer coordinates local to the hand root.
/// Hand lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for two textured Gouraud triangles and twenty-four quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/hand_left.inc"

/// Identity rest transform for the launcher's single root, copied into each model.
/// Vertex count for the single part-local launcher group (52 vertices).
/// Launcher vertices in integer coordinates local to its root.
/// Launcher lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for four textured Gouraud triangles and forty-one quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/mm1.inc"

/// Encoding-indexed pose banks used by this package's body clips.
enum {
    KYLE_800102_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    KYLE_800102_ANIMATION_PACKED_ROTATION_BANK      = 4
};

#include "assets/characters/kyle/companion_2_clips.inc"

#include "assets/characters/kyle/companion_2_bank.inc"

/// Grenade Pistol/MM1 muzzle offsets in integer launcher-local coordinates.
///
/// Rows 0 and 1 select the Grenade Pistol and MM1 respectively, from bits 16..19
/// of the shot's spawn argument. Kyle's MM1 uses row 1. Only XYZ is consumed.
static SVECTOR _gLauncherGrenadeMuzzleOffsets[2] = {
    { 0, 480, 128, 0 },
    { 0, 544, 128, 0 },
};

/// Blast-sphere radii in game-coordinate units for the three grenade rounds.
///
/// Indexed by the loaded round minus `GRENADE_ROUND_FIRST`: fragmentation 500,
/// airburst 1200 and riot 2000. The fourth stored halfword is zero; its role is
/// unproven and no supported round selects it.
static u16 _gLauncherGrenadeBlastRadii[4] = {
    [GRENADE_ROUND_FRAGMENTATION - GRENADE_ROUND_FIRST] = 500,
    [GRENADE_ROUND_AIRBURST -
        GRENADE_ROUND_FIRST]                            = 1200,
    [GRENADE_ROUND_RIOT -
        GRENADE_ROUND_FIRST]                            = 2000,
    0,
};

/// Initial positive flight divisors for launcher rows 0 Grenade Pistol and 1 MM1.
///
/// The byte value is shifted left 16 into the Q16.16 flight clock. Flight divides
/// the Q12 direction by its integer half, then increases the divisor each tick.
/// The two trailing stored zero bytes have unproven roles and are never selected
/// by supported launchers; they are not valid initial divisors.
static u8 _gLauncherGrenadeInitialFlightDivisors[4] = { 12, 8, 0, 0 };

/// Identity rest transform for the shell's single root, copied into each model.
/// Vertex count for the single part-local shell group (8 vertices).
/// Shell vertices in integer coordinates local to its root.
/// Shell lighting normals with twelve fractional bits, indexed independently of vertices.
/// Writable draw commands for six textured Gouraud quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// First model creation resolves the command handler slots in place.
#include "assets/characters/kyle/mm1_grenade_shell.inc"

/// The names the resident companion table gives this package's copies.
DEFINE_ALIAS(gKyleBody, gKyle800102Body);
DEFINE_ALIAS(gKyleHandRight, gKyle800102HandRight);
DEFINE_ALIAS(gKyleHandLeft2, gKyle800102HandLeft2);
DEFINE_ALIAS(gKyleHandLeft, gKyle800102HandLeft);
DEFINE_ALIAS(gKyleHandRight3, gKyle800102HandRight3);
