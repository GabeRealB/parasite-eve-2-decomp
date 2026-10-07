#include "types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 D_mist_shooting_gallery_8018E0C0;

#include "rooms/mist_shooting_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "mist_shooting_gallery_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
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

#include "mapui/map_akropolis.h"
/// Signed texture-frame counter for the gallery's repeating six-cell sprite strip.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"
#include "../../shared/beam_strip.h"

/// Marker values of `_MistShootingGallerySpawn::frame`, above every frame the
/// script clock can reach.
enum {
    MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT = 0xFFF0, // Where the script clock saturates, so it never equals a marker
    MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR  = 0xFFF1, // Hold the script here until no spawned target is left in play
    MIST_SHOOTING_GALLERY_SPAWN_END         = 0xFFFF, // End of the script; nothing further spawns
};

/// One record of a course's wave script: a gallery target to spawn once the
/// script clock reaches `frame`, or a marker that controls the script.
///
/// A course walks its script in order. Consecutive records carrying the same
/// `frame` spawn together, and since the clock only counts up, frames do not
/// decrease along a script. A course may also spawn its leading records one at
/// a time by index, ignoring `frame`. A marker record uses only `frame`; its
/// other fields are zero.
///
/// The two argument halves form the 32-bit spawn argument of the gallery
/// target actor, kept as halfwords because the word would sit unaligned:
///
/// - bits 0-3: target kind (below `MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT`),
///   which picks the target's model, hit points and score
/// - bits 12-15: behaviour of the target's mount (0 follows its path, 1 starts
///   turned half a revolution and runs the alternate path state, 2 is the
///   uncounted demonstration target, which no script requests)
/// - bits 16-23: movement path the mount follows
/// - bits 24-27: seconds the mount holds on a path whose first waypoint has no speed
/// - bit 28: the mount keeps rotating
/// - bit 29: forwarded to the target itself
/// - bit 30: spawn even when the player is within 0x400 of the position
typedef struct {
    u16 frame;      // Script frame the target spawns on, or a `MIST_SHOOTING_GALLERY_SPAWN_*` marker
    s16 spawnArgLo; // Low half of the target's spawn argument: kind and mount behaviour
    s16 spawnArgHi; // High half of the target's spawn argument: path, hold time and flags
    s16 x;          // World position the target's mount is placed at
    s16 y;
    s16 z;
} _MistShootingGallerySpawn;
STATIC_ASSERT_SIZEOF(_MistShootingGallerySpawn, 0xC);

extern TaskDesc D_mist_shooting_gallery_801856D0;
extern TaskDesc D_actor_107600_80134F94[];
/// The wave script the round loop walks: a run of records sharing
/// `frame` is spawned together, `MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR` waits
/// for the current wave to clear and `MIST_SHOOTING_GALLERY_SPAWN_END` stops
/// the script.
extern _MistShootingGallerySpawn* D_mist_shooting_gallery_80186904;
/// The second course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern _MistShootingGallerySpawn* D_mist_shooting_gallery_80186910;
/// The first bonus course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern _MistShootingGallerySpawn* D_mist_shooting_gallery_8018690C;
/// The second bonus course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern _MistShootingGallerySpawn* D_mist_shooting_gallery_80186908;
/// The five course wave scripts as the one array they are: element 0 is the
/// bonus course's script, and elements 1..4 are the same pointers the round
/// scripts above reach by their own addresses (`0x80186904` .. `0x80186910`).
/// The array type is what `func_mist_shooting_gallery_80182C58` needs - an
/// array element counts as a struct reference to GCC 2.8.1's alias analysis,
/// so the load is ordered against the `work->field_04` store that precedes it.
extern _MistShootingGallerySpawn* D_mist_shooting_gallery_80186900[];
/// Main-executable flag gating the countdown steps: while it is set the
/// gallery holds its current step instead of advancing the digit sprite.
/// Bonus-course variant selected before the round starts. It picks both the
/// banner sprite (`variant + 0xB`) and the colour it is drawn in (variant 2
/// uses 2 instead of 0x10).
/// Gameplay-side abort request. While it is 1 the bonus course tears itself
/// down: the state machine remembers where it was in `resumePhase` / `resumeCaptionStep`
/// and jumps to the state-9 shutdown banner.
extern void   func_actor_215100_8014A908(void);
extern void   func_actor_215100_8014A9A0(void);
extern void   actor215100CapCaptionDrawCurrent(void);
static void   func_mist_shooting_gallery_80184A80(Task* arg0);
static void   _mistShootingGalleryDrawCountdownClock(MistShootingGalleryWork* work);
static u16    _mistShootingGalleryTickCourseClock(MistShootingGalleryWork* work);
static void   func_mist_shooting_gallery_80184BB8(s16 arg0, s16 arg1, s16 arg2);
static Enemy* func_mist_shooting_gallery_80184CD0(Task* arg0, _MistShootingGallerySpawn* arg1);
s32           actor215100CapCaptionSelectScript(s16 arg0, s16 arg1, s32 arg2);
static void   _mistShootingGalleryDrawClockGlyph(s32 screenX, s16 screenY, s32 glyph);
static void   _mistShootingGalleryDrawRedFlash(u8 redIntensity);
static void   func_mist_shooting_gallery_80182B1C(Task* arg0);
static void   func_mist_shooting_gallery_80182C58(Task* arg0);
static void   func_mist_shooting_gallery_801831B0(Task* arg0);
static void   func_mist_shooting_gallery_8018341C(Task* arg0);
static void   func_mist_shooting_gallery_801838FC(Task* arg0);
static void   func_mist_shooting_gallery_80183E78(Task* arg0);
static void   func_mist_shooting_gallery_801842D0(Task* arg0);
static void   func_mist_shooting_gallery_80184A14(Task* arg0);

/// The gallery controller task's three-state table, run from a stack copy by
/// `func_mist_shooting_gallery_801849BC`: the setup tick
/// `func_mist_shooting_gallery_80182B1C`, the round runner
/// `func_mist_shooting_gallery_80184A14`, then
/// `func_mist_shooting_gallery_801842D0`.
static const TaskFuncTable3 D_mist_shooting_gallery_8017DB80 = {
    { func_mist_shooting_gallery_80182B1C, func_mist_shooting_gallery_80184A14, func_mist_shooting_gallery_801842D0 },
};

/// The five round scripts, indexed by `MistShootingGalleryWork::course`.
static const TaskFuncTable5 D_mist_shooting_gallery_8017DB8C = {
    {
        func_mist_shooting_gallery_80182C58,
        func_mist_shooting_gallery_801831B0,
        func_mist_shooting_gallery_8018341C,
        func_mist_shooting_gallery_801838FC,
        func_mist_shooting_gallery_80183E78,
    },
};

void func_mist_shooting_gallery_80184C0C(Task*);

void        func_mist_shooting_gallery_801849BC(Task*);
static void _mistShootingGalleryRedFlashTask(Task* task);

TaskDesc D_mist_shooting_gallery_801856B8[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_801849BC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mistShootingGalleryRedFlashTask, { .value = 0 } },
};

TaskDesc D_mist_shooting_gallery_801856D0 = { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_80184C0C, { .value = 0 } };

_MistShootingGallerySpawn D_mist_shooting_gallery_801856DC[60] = {
    { 0, 0x000A, 0x0000, 0, 0, 3000 },
    { 0, 0x000A, 0x0000, 0, 0, 4500 },
    { 0, 0x000A, 0x0700, 0, 0, 1500 },
    { 0, 0x000A, 0x0700, 1500, 0, 3000 },
    { 0, 0x000A, 0x0700, 1500, 0, 4500 },
    { 0, 0x000A, 0x0700, 1500, 0, 1500 },
    { 0, 0x000A, 0x0700, 3000, 0, 3000 },
    { 0, 0x000A, 0x0900, 1500, 0, 6000 },
    { 0, 0x000A, 0x0900, 1500, 0, 4500 },
    { 0, 0x000A, 0x0900, 1500, 0, 3000 },
    { 0, 0x000A, 0x0900, 1500, 0, 1500 },
    { 0, 0x000A, 0x0900, 1500, 0, 0 },
    { 32, 0x000A, 0x0300, 1500, 0, 4500 },
    { 48, 0x000A, 0x0100, 0, 0, 6000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 64, 0x000A, 0x0300, 1500, 0, 1500 },
    { 80, 0x000A, 0x0100, 0, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 96, 0x000A, 0x0300, -1500, 0, 3000 },
    { 120, 0x000A, 0x0300, 0, 0, 4500 },
    { 120, 0x000A, 0x0300, 0, 0, 1500 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 136, 0x100A, 0x0300, -1400, -3900, 3000 },
    { 160, 0x000A, 0x0300, 1500, 0, 6000 },
    { 160, 0x000A, 0x0300, 1500, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 176, 0x000A, 0x0400, 0, 0, 6000 },
    { 176, 0x000A, 0x0400, 0, 0, 0 },
    { 208, 0x100A, 0x0500, -1400, -3900, 5800 },
    { 208, 0x100A, 0x0500, -1400, -3900, 200 },
    { 264, 0x000A, 0x0300, 1500, 0, 3000 },
    { 264, 0x100A, 0x0400, 1600, -3900, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 280, 0x000A, 0x0200, 0, 0, 4500 },
    { 280, 0x000A, 0x0200, 0, 0, 1500 },
    { 360, 0x000A, 0x0300, -1500, 0, 6000 },
    { 360, 0x000A, 0x0300, -1500, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 368, 0x000A, 0x0200, 3000, 0, 6000 },
    { 368, 0x000A, 0x0200, 3000, 0, 0 },
    { 416, 0x100A, 0x0400, 1600, -3900, 3000 },
    { 440, 0x100A, 0x0300, -1400, -3900, 5800 },
    { 440, 0x100A, 0x0300, -1400, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 448, 0x000A, 0x0400, 4500, 0, 6000 },
    { 448, 0x000A, 0x0400, 4500, 0, 0 },
    { 464, 0x000A, 0x0500, 3000, 0, 4500 },
    { 464, 0x000A, 0x0500, 3000, 0, 1500 },
    { 496, 0x000A, 0x0400, 0, 0, 3000 },
    { 536, 0x100A, 0x0400, 1600, -3900, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 552, 0x000A, 0x0400, 1500, 0, 4500 },
    { 568, 0x000A, 0x0100, 1500, 0, 6000 },
    { 616, 0x000A, 0x0400, 1500, 0, 1500 },
    { 632, 0x000A, 0x0100, 1500, 0, 0 },
    { 696, 0x100A, 0x0400, -1400, -3900, 5800 },
    { 696, 0x100A, 0x0400, -1400, -3900, 3000 },
    { 696, 0x100A, 0x0400, -1400, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_END, 0, 0, 0, 0, 0 },
};

_MistShootingGallerySpawn D_mist_shooting_gallery_801859AC[66] = {
    { 64, 0x000A, 0x0002, 6000, 0, 4500 },
    { 64, 0x000A, 0x0004, 6000, 0, 1500 },
    { 144, 0x000A, 0x0009, -1500, 0, 6000 },
    { 144, 0x000A, 0x000D, -1500, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 160, 0x000B, 0x000A, -1500, 0, 4500 },
    { 160, 0x000B, 0x000C, -1500, 0, 1500 },
    { 256, 0x000A, 0x0001, 4500, 0, 6000 },
    { 256, 0x000A, 0x0005, 4500, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 272, 0x000A, 0x000A, -1500, 0, 4500 },
    { 272, 0x000A, 0x000C, -1500, 0, 1500 },
    { 320, 0x000C, 0x0003, 6000, 0, 3000 },
    { 416, 0x100A, 0x0018, 1600, -3900, 5800 },
    { 416, 0x100B, 0x0022, -1400, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 432, 0x000C, 0x000A, -1500, 0, 4500 },
    { 432, 0x000C, 0x000C, -1500, 0, 1500 },
    { 464, 0x100A, 0x0019, -1400, -3900, 5800 },
    { 464, 0x100A, 0x0021, 1600, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 480, 0x000A, 0x0025, -1500, 0, 6000 },
    { 480, 0x000A, 0x0026, -1500, 0, 0 },
    { 496, 0x000C, 0x0003, 7500, 0, 3000 },
    { 544, 0x000C, 0x0003, 7500, 0, 3000 },
    { 656, 0x000B, 0x0016, -1500, 0, 6000 },
    { 656, 0x000B, 0x001E, 0, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 672, 0x000A, 0x0013, 3000, 0, 6000 },
    { 704, 0x000B, 0x0015, 0, 0, 6000 },
    { 704, 0x000A, 0x001D, 1500, 0, 0 },
    { 736, 0x000B, 0x001F, -1500, 0, 0 },
    { 832, 0x100A, 0x0006, 4600, -3900, 5800 },
    { 832, 0x100A, 0x0008, 4600, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 848, 0x000A, 0x0034, 3000, 0, 6000 },
    { 864, 0x000A, 0x0036, 0, 0, 6000 },
    { 896, 0x000A, 0x003B, 1500, 0, 0 },
    { 912, 0x000A, 0x003D, -1500, 0, 0 },
    { 976, 0x100A, 0x0019, -1400, -3900, 5800 },
    { 976, 0x100A, 0x0021, 1600, -3900, 200 },
    { 1088, 0x000A, 0x003A, 3000, 0, 0 },
    { 1104, 0x000A, 0x003C, 0, 0, 0 },
    { 1120, 0x000A, 0x0035, 1500, 0, 6000 },
    { 1136, 0x000A, 0x0037, -1500, 0, 6000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1152, 0x000C, 0x0023, -1500, 0, 6000 },
    { 1200, 0x000C, 0x0024, -1500, 0, 0 },
    { 1248, 0x000B, 0x0023, -1500, 0, 6000 },
    { 1296, 0x000B, 0x0024, -1500, 0, 0 },
    { 1360, 0x100C, 0x0007, 4600, -3900, 3000 },
    { 1408, 0x100A, 0x0006, 4600, -3900, 5800 },
    { 1408, 0x100A, 0x0008, 4600, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1424, 0x000C, 0x0028, 0x2EE0, 0, 6000 },
    { 1424, 0x000C, 0x0029, 0x2EE0, 0, 0 },
    { 1472, 0x100A, 0x0006, 4600, -3900, 5800 },
    { 1472, 0x100A, 0x0008, 4600, -3900, 200 },
    { 1488, 0x000A, 0x0028, 0x2EE0, 0, 6000 },
    { 1488, 0x000B, 0x0029, 0x2EE0, 0, 0 },
    { 1696, 0x000B, 0x000A, -1500, 0, 4500 },
    { 1696, 0x000A, 0x000C, -1500, 0, 1500 },
    { 1792, 0x100A, 0x0018, 1600, -3900, 5800 },
    { 1792, 0x100A, 0x0022, -1400, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_END, 0, 0, 0, 0, 0 },
};

_MistShootingGallerySpawn D_mist_shooting_gallery_80185CC4[75] = {
    { 0, 0x0002, 0x4000, 7500, 0, 4500 },
    { 0, 0x0003, 0x4000, 7500, 0, 1500 },
    { 0, 0x0001, 0x4000, 0x2EE0, 0, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 48, 0x0003, 0x1002, 6000, 0, 4500 },
    { 48, 0x0003, 0x1004, 6000, 0, 1500 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 80, 0x0003, 0x1001, 4500, 0, 6000 },
    { 80, 0x0001, 0x1005, 4500, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 112, 0x0003, 0x1013, 3000, 0, 6000 },
    { 160, 0x0003, 0x1015, 0, 0, 6000 },
    { 208, 0x0003, 0x101D, 1500, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 240, 0x0003, 0x1002, 6000, 0, 4500 },
    { 240, 0x0003, 0x1004, 6000, 0, 1500 },
    { 304, 0x0000, 0x1003, 6000, 0, 3000 },
    { 416, 0x1008, 0x100F, -1400, -3900, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 448, 0x0005, 0x1028, 0x2EE0, 0, 6000 },
    { 448, 0x0005, 0x1029, 0x2EE0, 0, 0 },
    { 512, 0x1008, 0x100F, -1400, -3900, 3000 },
    { 544, 0x0001, 0x100B, -1500, 0, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 576, 0x1008, 0x1006, 4600, -3900, 5800 },
    { 576, 0x1008, 0x1008, 4600, -3900, 200 },
    { 608, 0x1007, 0x1007, 4600, -3900, 3000 },
    { 672, 0x0009, 0x1003, 6000, 0, 3000 },
    { 752, 0x0002, 0x1013, 3000, 0, 6000 },
    { 752, 0x0002, 0x101E, 0, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 784, 0x0001, 0x102A, 0x2EE0, 0, 6000 },
    { 832, 0x0009, 0x102A, 0x2EE0, 0, 6000 },
    { 864, 0x0002, 0x100A, -1500, 0, 4500 },
    { 864, 0x0002, 0x100B, -1500, 0, 3000 },
    { 864, 0x0002, 0x100C, -1500, 0, 1500 },
    { 944, 0x1007, 0x1019, -1400, -3900, 5800 },
    { 976, 0x1006, 0x1020, 4600, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1008, 0x0003, 0x101B, 4500, 0, 0 },
    { 1008, 0x0009, 0x101D, 1500, 0, 0 },
    { 1024, 0x0004, 0x101C, 3000, 0, 0 },
    { 1120, 0x0003, 0x1015, 0, 0, 6000 },
    { 1136, 0x0003, 0x1016, -1500, 0, 6000 },
    { 1232, 0x0004, 0x102B, 6000, 0, 4500 },
    { 1232, 0x0005, 0x102C, 6000, 0, 1500 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1264, 0x0005, 0x1002, 6000, 0, 4500 },
    { 1264, 0x0005, 0x1004, 6000, 0, 1500 },
    { 1328, 0x1007, 0x1007, 4600, -3900, 3000 },
    { 1408, 0x0000, 0x0025, -1500, 0, 6000 },
    { 1408, 0x0000, 0x0026, -1500, 0, 0 },
    { 1472, 0x0003, 0x102A, 0x2EE0, 0, 6000 },
    { 1520, 0x0003, 0x102A, 0x2EE0, 0, 6000 },
    { 1520, 0x1007, 0x1019, -1400, -3900, 5800 },
    { 1520, 0x1006, 0x1020, 4600, -3900, 200 },
    { 1568, 0x0009, 0x102A, 0x2EE0, 0, 6000 },
    { 1616, 0x0003, 0x102A, 0x2EE0, 0, 6000 },
    { 1664, 0x0003, 0x102A, 0x2EE0, 0, 6000 },
    { 1664, 0x0001, 0x101F, -1500, 0, 0 },
    { 1728, 0x0003, 0x1013, 3000, 0, 6000 },
    { 1744, 0x0005, 0x101D, 1500, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1776, 0x0009, 0x1028, 0x2EE0, 0, 6000 },
    { 1776, 0x0009, 0x1029, 0x2EE0, 0, 0 },
    { 1824, 0x0004, 0x1028, 0x2EE0, 0, 6000 },
    { 1824, 0x0004, 0x1029, 0x2EE0, 0, 0 },
    { 1872, 0x0002, 0x1028, 0x2EE0, 0, 6000 },
    { 1872, 0x0002, 0x1029, 0x2EE0, 0, 0 },
    { 1920, 0x0001, 0x1028, 0x2EE0, 0, 6000 },
    { 1920, 0x0001, 0x1029, 0x2EE0, 0, 0 },
    { 1968, 0x0009, 0x1028, 0x2EE0, 0, 6000 },
    { 1968, 0x0009, 0x1029, 0x2EE0, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_END, 0, 0, 0, 0, 0 },
};

_MistShootingGallerySpawn D_mist_shooting_gallery_80186048[106] = {
    { 0, 0x0005, 0x0000, 7500, 0, 4500 },
    { 0, 0x0005, 0x0000, 7500, 0, 1500 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 32, 0x0000, 0x1003, 6000, 0, 3000 },
    { 64, 0x0002, 0x1028, 0x2EE0, 0, 6000 },
    { 64, 0x0002, 0x1029, 0x2EE0, 0, 0 },
    { 128, 0x0003, 0x1001, 4500, 0, 6000 },
    { 128, 0x0003, 0x1005, 4500, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 144, 0x0003, 0x1012, 4500, 0, 6000 },
    { 160, 0x0000, 0x1014, 1500, 0, 6000 },
    { 176, 0x0003, 0x1016, -1500, 0, 6000 },
    { 224, 0x0009, 0x101C, 3000, 0, 0 },
    { 240, 0x0001, 0x101E, 0, 0, 0 },
    { 320, 0x1008, 0x100F, -1400, -3900, 3000 },
    { 336, 0x1006, 0x100E, -1400, -3900, 5800 },
    { 336, 0x1006, 0x1010, -1400, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 352, 0x0000, 0x1003, 6000, 0, 3000 },
    { 368, 0x0000, 0x1002, 6000, 0, 4500 },
    { 368, 0x0000, 0x1004, 6000, 0, 1500 },
    { 432, 0x0000, 0x1028, 0x2EE0, 0, 6000 },
    { 432, 0x0000, 0x1029, 0x2EE0, 0, 0 },
    { 512, 0x0001, 0x102A, 0x2EE0, 0, 6000 },
    { 544, 0x0009, 0x1200, 3000, 0, 4500 },
    { 544, 0x0009, 0x1200, 3000, 0, 1500 },
    { 560, 0x0001, 0x102A, 0x2EE0, 0, 6000 },
    { 640, 0x1007, 0x1006, 4600, -3900, 5800 },
    { 640, 0x1007, 0x1008, 4600, -3900, 200 },
    { 688, 0x0004, 0x100B, -1500, 0, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 704, 0x0009, 0x101B, 4500, 0, 0 },
    { 704, 0x0002, 0x101D, 1500, 0, 0 },
    { 704, 0x0003, 0x101F, -1500, 0, 0 },
    { 736, 0x0002, 0x1013, 3000, 0, 6000 },
    { 736, 0x0009, 0x1015, 0, 0, 6000 },
    { 752, 0x0005, 0x1028, 0x2EE0, 0, 6000 },
    { 752, 0x0005, 0x1029, 0x2EE0, 0, 0 },
    { 800, 0x0003, 0x1028, 0x2EE0, 0, 6000 },
    { 800, 0x0003, 0x1029, 0x2EE0, 0, 0 },
    { 848, 0x0003, 0x1028, 0x2EE0, 0, 6000 },
    { 848, 0x0003, 0x1029, 0x2EE0, 0, 0 },
    { 880, 0x0009, 0x1200, 1500, 0, 3000 },
    { 912, 0x0009, 0x1200, 0, 0, 3000 },
    { 912, 0x0004, 0x1500, 0x2EE0, 0, 3000 },
    { 1024, 0x1008, 0x1017, 4600, -3900, 5800 },
    { 1024, 0x1008, 0x1019, -1400, -3900, 5800 },
    { 1040, 0x1007, 0x1021, 1600, -3900, 200 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1056, 0x0009, 0x1027, -1500, 0, 6000 },
    { 1104, 0x0003, 0x1027, -1500, 0, 6000 },
    { 1152, 0x0005, 0x1027, -1500, 0, 6000 },
    { 1200, 0x0005, 0x1027, -1500, 0, 6000 },
    { 1248, 0x0005, 0x1027, -1500, 0, 6000 },
    { 1296, 0x0001, 0x1027, -1500, 0, 6000 },
    { 1296, 0x0009, 0x1900, 6000, 0, 4500 },
    { 1296, 0x0009, 0x1900, 6000, 0, 1500 },
    { 1312, 0x0003, 0x102A, 0x2EE0, 0, 6000 },
    { 1360, 0x0003, 0x102A, 0x2EE0, 0, 6000 },
    { 1408, 0x0003, 0x102A, 0x2EE0, 0, 6000 },
    { 1456, 0x0003, 0x102A, 0x2EE0, 0, 6000 },
    { 1504, 0x0003, 0x102A, 0x2EE0, 0, 6000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1520, 0x0009, 0x102D, -3000, 0, 6000 },
    { 1552, 0x0004, 0x102D, -3000, 0, 6000 },
    { 1584, 0x0002, 0x102D, -3000, 0, 6000 },
    { 1616, 0x0001, 0x102D, -3000, 0, 6000 },
    { 1648, 0x0001, 0x102D, -3000, 0, 6000 },
    { 1680, 0x0009, 0x102D, -3000, 0, 6000 },
    { 1712, 0x0003, 0x102D, -3000, 0, 6000 },
    { 1744, 0x0003, 0x102D, -3000, 0, 6000 },
    { 1776, 0x0003, 0x102D, -3000, 0, 6000 },
    { 1808, 0x0004, 0x102D, -3000, 0, 6000 },
    { 1840, 0x0004, 0x102D, -3000, 0, 6000 },
    { 1872, 0x0009, 0x102D, -3000, 0, 6000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1888, 0x0000, 0x1028, 0x2EE0, 0, 6000 },
    { 1888, 0x0000, 0x1029, 0x2EE0, 0, 0 },
    { 1936, 0x0004, 0x1028, 0x2EE0, 0, 6000 },
    { 1936, 0x0004, 0x1029, 0x2EE0, 0, 0 },
    { 1952, 0x1008, 0x1020, 4600, -3900, 200 },
    { 1984, 0x0005, 0x1028, 0x2EE0, 0, 6000 },
    { 1984, 0x0005, 0x1029, 0x2EE0, 0, 0 },
    { 2000, 0x1008, 0x1021, 1600, -3900, 200 },
    { 2048, 0x1007, 0x1022, -1400, -3900, 200 },
    { 2128, 0x0001, 0x100A, -1500, 0, 4500 },
    { 2128, 0x0001, 0x100C, -1500, 0, 1500 },
    { 2144, 0x0004, 0x1800, 0x2EE0, 0, 3000 },
    { 2176, 0x0001, 0x100A, -1500, 0, 4500 },
    { 2176, 0x0001, 0x100C, -1500, 0, 1500 },
    { 2224, 0x0002, 0x100A, -1500, 0, 4500 },
    { 2224, 0x0002, 0x100C, -1500, 0, 1500 },
    { 2416, 0x1006, 0x1017, 4600, -3900, 5800 },
    { 2464, 0x1007, 0x1018, 1600, -3900, 5800 },
    { 2512, 0x1008, 0x1019, -1400, -3900, 5800 },
    { 2592, 0x0003, 0x1012, 4500, 0, 6000 },
    { 2592, 0x0001, 0x101C, 3000, 0, 0 },
    { 2640, 0x0009, 0x1014, 1500, 0, 6000 },
    { 2640, 0x0001, 0x101E, 0, 0, 0 },
    { 2688, 0x0003, 0x1016, -1500, 0, 6000 },
    { 2784, 0x0009, 0x1200, 4500, 0, 4500 },
    { 2784, 0x0009, 0x1200, 4500, 0, 1500 },
    { 2832, 0x0004, 0x102B, 6000, 0, 4500 },
    { 2832, 0x0004, 0x102C, 6000, 0, 1500 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_END, 0, 0, 0, 0, 0 },
};

_MistShootingGallerySpawn D_mist_shooting_gallery_80186540[80] = {
    { 128, 0x0003, 0x3028, 0x2EE0, 0, 6000 },
    { 192, 0x0003, 0x3029, 0x2EE0, 0, 0 },
    { 272, 0x0000, 0x3003, 6000, 0, 3000 },
    { 352, 0x0001, 0x3028, 0x2EE0, 0, 6000 },
    { 416, 0x0001, 0x3029, 0x2EE0, 0, 0 },
    { 496, 0x0000, 0x3003, 7500, 0, 3000 },
    { 576, 0x0003, 0x300A, -3000, 0, 4500 },
    { 576, 0x0005, 0x300C, -3000, 0, 1500 },
    { 672, 0x0005, 0x300A, -3000, 0, 4500 },
    { 672, 0x0000, 0x300C, -3000, 0, 1500 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 688, 0x0005, 0x3900, 4500, 0, 6000 },
    { 704, 0x0009, 0x1700, 3000, 0, 4500 },
    { 720, 0x0004, 0x3900, 1500, 0, 3000 },
    { 736, 0x0003, 0x1700, 0, 0, 1500 },
    { 752, 0x0005, 0x3900, -1500, 0, 0 },
    { 896, 0x0002, 0x3028, 0x2EE0, 0, 6000 },
    { 896, 0x0000, 0x3029, 0x2EE0, 0, 0 },
    { 960, 0x1007, 0x3017, 4600, -3900, 5800 },
    { 976, 0x1007, 0x3018, 1600, -3900, 5800 },
    { 992, 0x1008, 0x3019, -1400, -3900, 5800 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1008, 0x0000, 0x3025, -1500, 0, 6000 },
    { 1008, 0x0002, 0x3026, -1500, 0, 0 },
    { 1040, 0x0001, 0x100A, -1500, 0, 4500 },
    { 1040, 0x0001, 0x100C, -1500, 0, 1500 },
    { 1104, 0x0003, 0x3003, 7500, 0, 3000 },
    { 1168, 0x0003, 0x3003, 7500, 0, 3000 },
    { 1168, 0x0000, 0x3025, -1500, 0, 6000 },
    { 1168, 0x0002, 0x3026, -1500, 0, 0 },
    { 1200, 0x0009, 0x100A, -1500, 0, 4500 },
    { 1200, 0x0009, 0x100C, -1500, 0, 1500 },
    { 1264, 0x0003, 0x3003, 7500, 0, 3000 },
    { 1328, 0x0003, 0x3003, 7500, 0, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1344, 0x0001, 0x3024, -1500, 0, 0 },
    { 1408, 0x0005, 0x3023, -1500, 0, 6000 },
    { 1472, 0x0001, 0x3024, -1500, 0, 0 },
    { 1536, 0x0005, 0x3023, -1500, 0, 6000 },
    { 1536, 0x0004, 0x3F00, 7500, 0, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1552, 0x0003, 0x302E, -3000, 0, 4500 },
    { 1600, 0x0000, 0x302E, -3000, 0, 4500 },
    { 1648, 0x0009, 0x102E, -3000, 0, 4500 },
    { 1648, 0x0005, 0x302F, -3000, 0, 6000 },
    { 1648, 0x0003, 0x3030, -3000, 0, 0 },
    { 1696, 0x0000, 0x302E, -3000, 0, 4500 },
    { 1888, 0x0004, 0x302F, -3000, 0, 6000 },
    { 1888, 0x0001, 0x3030, -3000, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 1904, 0x1008, 0x100E, -1400, -3900, 5800 },
    { 1904, 0x1008, 0x1010, -1400, -3900, 200 },
    { 1936, 0x0002, 0x3031, 0x2EE0, 0, 6000 },
    { 1984, 0x0003, 0x302F, -3000, 0, 6000 },
    { 1984, 0x0003, 0x3030, -3000, 0, 0 },
    { 2000, 0x1007, 0x100F, -1400, -3900, 3000 },
    { 2080, 0x1008, 0x100E, -1400, -3900, 5800 },
    { 2080, 0x1008, 0x1010, -1400, -3900, 200 },
    { 2112, 0x0001, 0x100A, -3000, 0, 4500 },
    { 2112, 0x0003, 0x100C, -3000, 0, 1500 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 2144, 0x0009, 0x1003, 7500, 0, 3000 },
    { 2208, 0x0001, 0x302F, -3000, 0, 6000 },
    { 2208, 0x0001, 0x3030, -3000, 0, 0 },
    { 2208, 0x0002, 0x3003, 7500, 0, 3000 },
    { 2208, 0x0002, 0x3031, 0x2EE0, 0, 6000 },
    { 2272, 0x0002, 0x3003, 7500, 0, 3000 },
    { 2336, 0x0002, 0x3003, 7500, 0, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { 2352, 0x0003, 0x3013, 3000, 0, 6000 },
    { 2352, 0x0003, 0x3015, 0, 0, 6000 },
    { 2352, 0x0004, 0x301D, 1500, 0, 0 },
    { 2384, 0x0000, 0x3002, 7500, 0, 4500 },
    { 2384, 0x0003, 0x3003, 7500, 0, 3000 },
    { 2464, 0x0002, 0x3003, 7500, 0, 3000 },
    { 2464, 0x0001, 0x3004, 7500, 0, 1500 },
    { 2544, 0x0004, 0x3002, 7500, 0, 4500 },
    { 2544, 0x0009, 0x1003, 7500, 0, 3000 },
    { MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR, 0, 0, 0, 0, 0 },
    { MIST_SHOOTING_GALLERY_SPAWN_END, 0, 0, 0, 0, 0 },
};

_MistShootingGallerySpawn* D_mist_shooting_gallery_80186900[1] = {
    D_mist_shooting_gallery_801856DC,
};

_MistShootingGallerySpawn* D_mist_shooting_gallery_80186904 = D_mist_shooting_gallery_801859AC;

_MistShootingGallerySpawn* D_mist_shooting_gallery_80186908 = D_mist_shooting_gallery_80185CC4;

_MistShootingGallerySpawn* D_mist_shooting_gallery_8018690C = D_mist_shooting_gallery_80186048;

_MistShootingGallerySpawn* D_mist_shooting_gallery_80186910 = D_mist_shooting_gallery_80186540;

static TmdBone _gMistShootingGalleryModel093FCSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_093FC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel093FCPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_093FC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel093FCVerts[10] = {
#include "assets/mist_shooting_gallery_model_093FC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel093FCNormals[6] = {
#include "assets/mist_shooting_gallery_model_093FC_normals.inc"
};

static u32 _gMistShootingGalleryModel093FCStream[73] = {
#include "assets/mist_shooting_gallery_model_093FC_stream.inc"
};

TmdSource gMistShootingGalleryModel093FC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel093FCPartVerts,
    _gMistShootingGalleryModel093FCVerts,
    _gMistShootingGalleryModel093FCNormals,
    _gMistShootingGalleryModel093FCSkeleton,
    _gMistShootingGalleryModel093FCStream,
};

static TmdBone _gMistShootingGalleryModel095ECSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_095EC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel095ECPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_095EC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel095ECVerts[10] = {
#include "assets/mist_shooting_gallery_model_095EC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel095ECNormals[6] = {
#include "assets/mist_shooting_gallery_model_095EC_normals.inc"
};

static u32 _gMistShootingGalleryModel095ECStream[73] = {
#include "assets/mist_shooting_gallery_model_095EC_stream.inc"
};

TmdSource gMistShootingGalleryModel095EC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel095ECPartVerts,
    _gMistShootingGalleryModel095ECVerts,
    _gMistShootingGalleryModel095ECNormals,
    _gMistShootingGalleryModel095ECSkeleton,
    _gMistShootingGalleryModel095ECStream,
};

static TmdBone _gMistShootingGalleryModel097DCSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_097DC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel097DCPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_097DC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel097DCVerts[10] = {
#include "assets/mist_shooting_gallery_model_097DC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel097DCNormals[6] = {
#include "assets/mist_shooting_gallery_model_097DC_normals.inc"
};

static u32 _gMistShootingGalleryModel097DCStream[73] = {
#include "assets/mist_shooting_gallery_model_097DC_stream.inc"
};

TmdSource gMistShootingGalleryModel097DC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel097DCPartVerts,
    _gMistShootingGalleryModel097DCVerts,
    _gMistShootingGalleryModel097DCNormals,
    _gMistShootingGalleryModel097DCSkeleton,
    _gMistShootingGalleryModel097DCStream,
};

static TmdBone _gMistShootingGalleryModel099CCSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_099CC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel099CCPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_099CC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel099CCVerts[10] = {
#include "assets/mist_shooting_gallery_model_099CC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel099CCNormals[6] = {
#include "assets/mist_shooting_gallery_model_099CC_normals.inc"
};

static u32 _gMistShootingGalleryModel099CCStream[73] = {
#include "assets/mist_shooting_gallery_model_099CC_stream.inc"
};

TmdSource gMistShootingGalleryModel099CC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel099CCPartVerts,
    _gMistShootingGalleryModel099CCVerts,
    _gMistShootingGalleryModel099CCNormals,
    _gMistShootingGalleryModel099CCSkeleton,
    _gMistShootingGalleryModel099CCStream,
};

static TmdBone _gMistShootingGalleryModel09BBCSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_09BBC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel09BBCPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_09BBC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel09BBCVerts[10] = {
#include "assets/mist_shooting_gallery_model_09BBC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel09BBCNormals[6] = {
#include "assets/mist_shooting_gallery_model_09BBC_normals.inc"
};

static u32 _gMistShootingGalleryModel09BBCStream[73] = {
#include "assets/mist_shooting_gallery_model_09BBC_stream.inc"
};

TmdSource gMistShootingGalleryModel09BBC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel09BBCPartVerts,
    _gMistShootingGalleryModel09BBCVerts,
    _gMistShootingGalleryModel09BBCNormals,
    _gMistShootingGalleryModel09BBCSkeleton,
    _gMistShootingGalleryModel09BBCStream,
};

static TmdBone _gMistShootingGalleryModel09DACSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_09DAC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel09DACPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_09DAC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel09DACVerts[10] = {
#include "assets/mist_shooting_gallery_model_09DAC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel09DACNormals[6] = {
#include "assets/mist_shooting_gallery_model_09DAC_normals.inc"
};

static u32 _gMistShootingGalleryModel09DACStream[73] = {
#include "assets/mist_shooting_gallery_model_09DAC_stream.inc"
};

TmdSource gMistShootingGalleryModel09DAC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel09DACPartVerts,
    _gMistShootingGalleryModel09DACVerts,
    _gMistShootingGalleryModel09DACNormals,
    _gMistShootingGalleryModel09DACSkeleton,
    _gMistShootingGalleryModel09DACStream,
};

static TmdBone _gMistShootingGalleryModel09F9CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_09F9C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel09F9CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_09F9C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel09F9CVerts[10] = {
#include "assets/mist_shooting_gallery_model_09F9C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel09F9CNormals[6] = {
#include "assets/mist_shooting_gallery_model_09F9C_normals.inc"
};

static u32 _gMistShootingGalleryModel09F9CStream[73] = {
#include "assets/mist_shooting_gallery_model_09F9C_stream.inc"
};

TmdSource gMistShootingGalleryModel09F9C = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel09F9CPartVerts,
    _gMistShootingGalleryModel09F9CVerts,
    _gMistShootingGalleryModel09F9CNormals,
    _gMistShootingGalleryModel09F9CSkeleton,
    _gMistShootingGalleryModel09F9CStream,
};

static TmdBone _gMistShootingGalleryModel0A18CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0A18C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0A18CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0A18C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A18CVerts[10] = {
#include "assets/mist_shooting_gallery_model_0A18C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A18CNormals[6] = {
#include "assets/mist_shooting_gallery_model_0A18C_normals.inc"
};

static u32 _gMistShootingGalleryModel0A18CStream[73] = {
#include "assets/mist_shooting_gallery_model_0A18C_stream.inc"
};

TmdSource gMistShootingGalleryModel0A18C = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0A18CPartVerts,
    _gMistShootingGalleryModel0A18CVerts,
    _gMistShootingGalleryModel0A18CNormals,
    _gMistShootingGalleryModel0A18CSkeleton,
    _gMistShootingGalleryModel0A18CStream,
};

static TmdBone _gMistShootingGalleryModel0A37CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0A37C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0A37CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0A37C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A37CVerts[10] = {
#include "assets/mist_shooting_gallery_model_0A37C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A37CNormals[6] = {
#include "assets/mist_shooting_gallery_model_0A37C_normals.inc"
};

static u32 _gMistShootingGalleryModel0A37CStream[73] = {
#include "assets/mist_shooting_gallery_model_0A37C_stream.inc"
};

TmdSource gMistShootingGalleryModel0A37C = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0A37CPartVerts,
    _gMistShootingGalleryModel0A37CVerts,
    _gMistShootingGalleryModel0A37CNormals,
    _gMistShootingGalleryModel0A37CSkeleton,
    _gMistShootingGalleryModel0A37CStream,
};

static TmdBone _gMistShootingGalleryModel0A56CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0A56C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0A56CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0A56C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A56CVerts[10] = {
#include "assets/mist_shooting_gallery_model_0A56C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A56CNormals[6] = {
#include "assets/mist_shooting_gallery_model_0A56C_normals.inc"
};

static u32 _gMistShootingGalleryModel0A56CStream[73] = {
#include "assets/mist_shooting_gallery_model_0A56C_stream.inc"
};

TmdSource gMistShootingGalleryModel0A56C = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0A56CPartVerts,
    _gMistShootingGalleryModel0A56CVerts,
    _gMistShootingGalleryModel0A56CNormals,
    _gMistShootingGalleryModel0A56CSkeleton,
    _gMistShootingGalleryModel0A56CStream,
};

static TmdBone _gMistShootingGalleryModel0A81CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0A81C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0A81CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0A81C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A81CVerts[24] = {
#include "assets/mist_shooting_gallery_model_0A81C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A81CNormals[16] = {
#include "assets/mist_shooting_gallery_model_0A81C_normals.inc"
};

static u32 _gMistShootingGalleryModel0A81CStream[150] = {
#include "assets/mist_shooting_gallery_model_0A81C_stream.inc"
};

TmdSource gMistShootingGalleryModel0A81C = {
    0,
    1056,
    0,
    1,
    _gMistShootingGalleryModel0A81CPartVerts,
    _gMistShootingGalleryModel0A81CVerts,
    _gMistShootingGalleryModel0A81CNormals,
    _gMistShootingGalleryModel0A81CSkeleton,
    _gMistShootingGalleryModel0A81CStream,
};

static TmdBone _gMistShootingGalleryModel0AB30Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0AB30_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0AB30PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0AB30_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AB30Verts[8] = {
#include "assets/mist_shooting_gallery_model_0AB30_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AB30Normals[6] = {
#include "assets/mist_shooting_gallery_model_0AB30_normals.inc"
};

static u32 _gMistShootingGalleryModel0AB30Stream[42] = {
#include "assets/mist_shooting_gallery_model_0AB30_stream.inc"
};

TmdSource gMistShootingGalleryModel0AB30 = {
    0,
    312,
    0,
    1,
    _gMistShootingGalleryModel0AB30PartVerts,
    _gMistShootingGalleryModel0AB30Verts,
    _gMistShootingGalleryModel0AB30Normals,
    _gMistShootingGalleryModel0AB30Skeleton,
    _gMistShootingGalleryModel0AB30Stream,
};

static TmdBone _gMistShootingGalleryModel0AC94Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0AC94_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0AC94PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0AC94_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AC94Verts[8] = {
#include "assets/mist_shooting_gallery_model_0AC94_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AC94Normals[6] = {
#include "assets/mist_shooting_gallery_model_0AC94_normals.inc"
};

static u32 _gMistShootingGalleryModel0AC94Stream[42] = {
#include "assets/mist_shooting_gallery_model_0AC94_stream.inc"
};

TmdSource gMistShootingGalleryModel0AC94 = {
    0,
    312,
    0,
    1,
    _gMistShootingGalleryModel0AC94PartVerts,
    _gMistShootingGalleryModel0AC94Verts,
    _gMistShootingGalleryModel0AC94Normals,
    _gMistShootingGalleryModel0AC94Skeleton,
    _gMistShootingGalleryModel0AC94Stream,
};

static TmdBone _gMistShootingGalleryModel0ADF8Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0ADF8_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0ADF8PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0ADF8_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0ADF8Verts[8] = {
#include "assets/mist_shooting_gallery_model_0ADF8_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0ADF8Normals[6] = {
#include "assets/mist_shooting_gallery_model_0ADF8_normals.inc"
};

static u32 _gMistShootingGalleryModel0ADF8Stream[42] = {
#include "assets/mist_shooting_gallery_model_0ADF8_stream.inc"
};

TmdSource gMistShootingGalleryModel0ADF8 = {
    0,
    312,
    0,
    1,
    _gMistShootingGalleryModel0ADF8PartVerts,
    _gMistShootingGalleryModel0ADF8Verts,
    _gMistShootingGalleryModel0ADF8Normals,
    _gMistShootingGalleryModel0ADF8Skeleton,
    _gMistShootingGalleryModel0ADF8Stream,
};

static TmdBone _gMistShootingGalleryModel0AF5CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0AF5C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0AF5CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0AF5C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AF5CVerts[8] = {
#include "assets/mist_shooting_gallery_model_0AF5C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AF5CNormals[6] = {
#include "assets/mist_shooting_gallery_model_0AF5C_normals.inc"
};

static u32 _gMistShootingGalleryModel0AF5CStream[42] = {
#include "assets/mist_shooting_gallery_model_0AF5C_stream.inc"
};

TmdSource gMistShootingGalleryModel0AF5C = {
    0,
    312,
    0,
    1,
    _gMistShootingGalleryModel0AF5CPartVerts,
    _gMistShootingGalleryModel0AF5CVerts,
    _gMistShootingGalleryModel0AF5CNormals,
    _gMistShootingGalleryModel0AF5CSkeleton,
    _gMistShootingGalleryModel0AF5CStream,
};

static TmdBone _gMistShootingGalleryModel0B0D0Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0B0D0_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0B0D0PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0B0D0_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B0D0Verts[10] = {
#include "assets/mist_shooting_gallery_model_0B0D0_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B0D0Normals[6] = {
#include "assets/mist_shooting_gallery_model_0B0D0_normals.inc"
};

static u32 _gMistShootingGalleryModel0B0D0Stream[73] = {
#include "assets/mist_shooting_gallery_model_0B0D0_stream.inc"
};

TmdSource gMistShootingGalleryModel0B0D0 = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0B0D0PartVerts,
    _gMistShootingGalleryModel0B0D0Verts,
    _gMistShootingGalleryModel0B0D0Normals,
    _gMistShootingGalleryModel0B0D0Skeleton,
    _gMistShootingGalleryModel0B0D0Stream,
};

static TmdBone _gMistShootingGalleryModel0B2C0Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0B2C0_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0B2C0PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0B2C0_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B2C0Verts[10] = {
#include "assets/mist_shooting_gallery_model_0B2C0_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B2C0Normals[6] = {
#include "assets/mist_shooting_gallery_model_0B2C0_normals.inc"
};

static u32 _gMistShootingGalleryModel0B2C0Stream[73] = {
#include "assets/mist_shooting_gallery_model_0B2C0_stream.inc"
};

TmdSource gMistShootingGalleryModel0B2C0 = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0B2C0PartVerts,
    _gMistShootingGalleryModel0B2C0Verts,
    _gMistShootingGalleryModel0B2C0Normals,
    _gMistShootingGalleryModel0B2C0Skeleton,
    _gMistShootingGalleryModel0B2C0Stream,
};

static TmdBone _gMistShootingGalleryModel0B4B0Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0B4B0_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0B4B0PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0B4B0_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B4B0Verts[10] = {
#include "assets/mist_shooting_gallery_model_0B4B0_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B4B0Normals[6] = {
#include "assets/mist_shooting_gallery_model_0B4B0_normals.inc"
};

static u32 _gMistShootingGalleryModel0B4B0Stream[73] = {
#include "assets/mist_shooting_gallery_model_0B4B0_stream.inc"
};

TmdSource gMistShootingGalleryModel0B4B0 = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0B4B0PartVerts,
    _gMistShootingGalleryModel0B4B0Verts,
    _gMistShootingGalleryModel0B4B0Normals,
    _gMistShootingGalleryModel0B4B0Skeleton,
    _gMistShootingGalleryModel0B4B0Stream,
};

static TmdBone _gMistShootingGalleryModel0B6A0Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0B6A0_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0B6A0PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0B6A0_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B6A0Verts[10] = {
#include "assets/mist_shooting_gallery_model_0B6A0_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B6A0Normals[6] = {
#include "assets/mist_shooting_gallery_model_0B6A0_normals.inc"
};

static u32 _gMistShootingGalleryModel0B6A0Stream[73] = {
#include "assets/mist_shooting_gallery_model_0B6A0_stream.inc"
};

TmdSource gMistShootingGalleryModel0B6A0 = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0B6A0PartVerts,
    _gMistShootingGalleryModel0B6A0Verts,
    _gMistShootingGalleryModel0B6A0Normals,
    _gMistShootingGalleryModel0B6A0Skeleton,
    _gMistShootingGalleryModel0B6A0Stream,
};

static SVECTOR _gMistShootingGalleryCollision0C3A8Normals[22] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_normals.inc"
};

static SVECTOR _gMistShootingGalleryCollision0C3A8Verts[136] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_verts.inc"
};

static WorldCollisionGridFace _gMistShootingGalleryCollision0C3A8Faces[57] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_faces.inc"
};

static s16 _gMistShootingGalleryCollision0C3A8Cells[474] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_cells.inc"
};

#define GRID_CELL(i) (&_gMistShootingGalleryCollision0C3A8Cells[i])
static s16* _gMistShootingGalleryCollision0C3A8Table[28] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mist_shooting_gallery_80189968 = { NULL, _gMistShootingGalleryCollision0C3A8Normals, _gMistShootingGalleryCollision0C3A8Verts, _gMistShootingGalleryCollision0C3A8Faces, _gMistShootingGalleryCollision0C3A8Table, 0x2EE0, 7000, 7, 4, 4000, 57 };

ViewCamera D_mist_shooting_gallery_8018998C[18] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -500, 0x7530, 0 } }, 289 },
    { { { { 4063, 0, 515 }, { 11, 4094, -90 }, { -515, 91, 4062 } }, { 0x29EB, 949, -759 } }, 257 },
    { { { { -4053, 0, 589 }, { 15, 4094, 103 }, { -589, 104, -4051 } }, { 0x299F, 949, -5070 } }, 257 },
    { { { { -3856, 0, -1378 }, { -724, 3484, 2026 }, { 1173, 2152, -3281 } }, { 0x2D46, 2950, -640 } }, 257 },
    { { { { 1535, 0, 3797 }, { 2721, 2856, -1100 }, { -2648, 2935, 1070 } }, { 5780, 3030, 3820 } }, 257 },
    { { { { 1443, 0, -3833 }, { -2562, 3045, -965 }, { 2850, 2738, 1073 } }, { 8560, 2980, 3970 } }, 257 },
    { { { { -3959, 0, -1047 }, { -156, 4049, 592 }, { 1036, 613, -3915 } }, { 8580, 1450, -5400 } }, 257 },
    { { { { 3907, 0, -1227 }, { -214, 4033, -682 }, { 1208, 715, 3847 } }, { 8580, 1850, -800 } }, 257 },
    { { { { 640, 0, 4045 }, { 290, 4085, -46 }, { -4035, 294, 638 } }, { -7520, 1850, -1700 } }, 257 },
    { { { { 689, 0, -4037 }, { -312, 4083, -53 }, { 4025, 316, 687 } }, { 4980, 1850, -1500 } }, 257 },
    { { { { 179, 0, -4092 }, { 95, 4094, 4 }, { 4090, -95, 179 } }, { -2620, 1250, -2720 } }, 257 },
    { { { { 3948, 0, -1088 }, { 205, 4022, 746 }, { 1068, -774, 3877 } }, { -0x2C60, 650, 0 } }, 257 },
    { { { { -4008, 0, -841 }, { 98, 4067, -471 }, { 835, -481, -3980 } }, { -0x2D0A, 650, -4700 } }, 257 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { 8120, 1900, -3000 } }, 257 },
    { { { { 0, 0, 4096 }, { 0, 4096, 0 }, { -4096, 0, 0 } }, { 1120, 1600, -3000 } }, 680 },
    { { { { -3892, 0, 1275 }, { 126, 4075, 384 }, { -1269, 404, -3873 } }, { 9040, 1520, -8210 } }, 680 },
    { { { { 680, 0, -4039 }, { -104, 4094, -17 }, { 4037, 106, 679 } }, { 0x2A58, 1290, -2030 } }, 680 },
    { { { { 640, 0, 4045 }, { 292, 4085, -46 }, { -4035, 295, 638 } }, { -3070, 1850, -1700 } }, 257 },
};

SpriteBatch D_mist_shooting_gallery_80189C14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_shooting_gallery_80189C24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_shooting_gallery_80189C34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_80189C44[13] = {
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -160, 112, 625, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -160, 104, 625, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -160, 96, 625, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -160, 88, 650, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -160, 80, 675, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -160, 72, 700, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -160, 64, 725, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -160, 56, 750, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 48, 775, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 800, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 32, 750, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 24, 750, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, 0, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_80189D48[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_80189D60[32] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 48, 750, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 32, 750, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 750, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 0, 650, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -16, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, -32, 600, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, -48, 550, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, -64, 550, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -72, 550, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 56, 750, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 32, 750, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 16, 725, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 0, 650, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, -16, 600, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, -32, 550, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -48, 550, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 64, 750, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 48, 725, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 32, 725, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 16, 650, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 0, 600, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -16, 550, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 80, 750, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 64, 725, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 48, 725, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 32, 650, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 16, 600, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 96, 750, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 80, 700, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 64, 725, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 48, 650, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 80, 725, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_80189FE0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_shooting_gallery_80189FF8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018A008[24] = {
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -144, -104, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -136, -104, 937, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -136, -24, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -120, -104, 937, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -120, -24, 975, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -104, -104, 937, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -104, -24, 950, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -88, -96, 937, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -88, -24, 950, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -80, -24, 950, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -56, 937, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -40, 1037, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 64, -32, 875, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 56, -24, 750, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 56, -16, 775, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 56, -8, 750, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, 56, 0, 400, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, 56, 8, 375, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, 48, 24, 300, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, 48, 40, 287, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, 48, 56, 275, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, 40, 72, 187, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, 40, 88, 125, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 128, 16 } }, 32, 104, 125, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018A1E8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018A208[25] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, -8, 975, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -112, 0, 862, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -136, 8, 762, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -136, 16, 662, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -144, 24, 630, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -152, 32, 600, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -152, 40, 580, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -152, 48, 437, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -152, 56, 495, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -152, 64, 462, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -152, 72, 431, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -152, 80, 405, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -152, 88, 350, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -160, 96, 325, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -160, 104, 312, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -160, 112, 287, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 64, 0, 950, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, -64, 912, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, -80, 912, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, 0, 975, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, 0, 962, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, -80, 937, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 112, -80, 950, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, 0, 1012, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, -80, 969, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018A3FC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_shooting_gallery_8018A41C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018A42C[9] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 88, -56, 2625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 72, -48, 2812, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 80, -56, 2750, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -48, -48, 3250, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -48, 3000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -64, -48, 2875, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -8, -40, 3675, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 8, -40, 3675, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 24, -40, 3675, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018A4E0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 3, 0, 0, { 2, 0 } },
    { 6, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018A508[13] = {
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -56, -56, 1837, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -40, -56, 1837, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -24, -56, 1837, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -8, -56, 1837, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 8, -56, 1837, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 24, -56, 1837, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 40, -56, 1837, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 136, -80, 1250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 136, -8, 1250, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 144, -88, 1237, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 144, -8, 1237, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 152, -96, 1162, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 152, -8, 1162, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018A60C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 6, 0, 0, { 2, 0 } },
    { 13, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018A634[14] = {
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -160, -120, 1000, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, -160, -8, 1000, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -144, -120, 1000, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, -144, -8, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -128, -120, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, -128, -8, 1000, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -112, -8, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -112, -120, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -104, -120, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -104, -16, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -96, -16, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -96, -120, 1000, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -88, -120, 1000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -88, -48, 1000, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018A74C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018A764[12] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 88, -40, 850, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 88, -120, 850, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 96, -120, 850, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 96, -24, 850, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 104, -8, 850, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 104, -120, 850, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 112, -120, 850, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, 112, -8, 850, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 128, -120, 850, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, 128, -8, 850, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, 144, -8, 850, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 144, -120, 850, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018A854[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018A86C[29] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 88, 500, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 88, 500, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 88, 500, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 88, 500, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 88, 500, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -80, 88, 500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 88, 500, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, 80, 500, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -32, 80, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 80, 500, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, 88, 500, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 88, 500, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 88, 500, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 88, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 88, 500, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 88, 500, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 88, 500, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 88, 500, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 88, 500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 88, 500, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -16, 4500, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -16, 4500, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 8, -16, 4500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -72, -24, 3312, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -24, 3587, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, -24, 3587, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, -24, 3587, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -24, 3587, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, -24, 3312, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018AAB0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 1, 0 } },
    { 20, 3, 0, 0, { 2, 0 } },
    { 23, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018AAD8[164] = {
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, -32, 1875, { .fields = { 32, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 16, -32, 1875, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 32, -32, 1875, { .fields = { 40, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 48, -32, 1875, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 64, -32, 1875, { .fields = { 8, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, -32, 1875, { .fields = { 0, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, 96, -32, 1875, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -32, 1875, { .fields = { 24, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 128, -32, 1875, { .fields = { 16, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 144, -32, 1875, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 8 } }, -128, 64, 1194, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -120, 64, 1847, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -112, 64, 1847, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -104, 64, 1847, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -96, 64, 1847, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 64, 1847, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -80, 64, 1847, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -72, 64, 1847, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -64, 64, 1845, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -56, 64, 1847, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -48, 64, 1847, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -40, 64, 1847, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -32, 64, 1847, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -24, 64, 1847, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -16, 64, 1847, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -8, 64, 1847, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 0, 64, 1847, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 8, 64, 1847, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -128, 72, 1194, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -120, 72, 1847, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -112, 72, 1847, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -104, 72, 1847, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -96, 72, 1847, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 72, 1847, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -80, 72, 1847, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -72, 72, 1847, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -64, 72, 1845, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -56, 72, 1847, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -48, 72, 1847, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -40, 72, 1847, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -32, 72, 1847, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -24, 72, 1847, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -16, 72, 1847, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -8, 72, 1847, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 0, 72, 1847, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 8, 72, 1847, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -128, 80, 1194, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -120, 80, 1847, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -112, 80, 1847, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -104, 80, 1847, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -96, 80, 1847, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 80, 1847, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -80, 80, 1847, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -72, 80, 1847, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -64, 80, 1845, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -56, 80, 1847, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -48, 80, 1847, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -40, 80, 1846, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -32, 80, 1846, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -24, 80, 1846, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -16, 80, 1846, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -8, 80, 1846, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 0, 80, 1846, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 8, 80, 1847, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -128, 88, 1097, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -120, 88, 1098, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -112, 88, 1101, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -104, 88, 1104, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -96, 88, 1105, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 88, 1105, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -80, 88, 1105, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -72, 88, 1105, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -64, 88, 1105, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -56, 88, 1105, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -48, 88, 1105, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -40, 88, 1105, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -32, 88, 1105, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -24, 88, 1105, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -16, 88, 1105, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -8, 88, 1107, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 0, 88, 1107, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 8, 88, 1105, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -128, 96, 1117, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -120, 96, 1117, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -112, 96, 1850, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -104, 96, 1850, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -96, 96, 1850, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -88, 96, 1850, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -80, 96, 1850, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -72, 96, 1850, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -64, 96, 1845, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -56, 96, 1850, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -48, 96, 1850, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -40, 96, 1850, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -32, 96, 1850, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -24, 96, 1850, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -16, 96, 1850, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -8, 96, 1850, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 0, 96, 1850, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 8, 96, 1850, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -128, 104, 1194, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -120, 104, 1850, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -112, 104, 1850, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -104, 104, 1850, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -96, 104, 1850, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -88, 104, 1850, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -80, 104, 1850, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -72, 104, 1850, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -64, 104, 1845, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -56, 104, 1850, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -48, 104, 1850, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -40, 104, 1850, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -32, 104, 1850, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -24, 104, 1850, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -16, 104, 1850, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -8, 104, 1850, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 0, 104, 1850, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 8, 104, 1850, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -128, 112, 1194, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -120, 112, 1847, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -112, 112, 1847, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -104, 112, 1847, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -96, 112, 1847, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 112, 1847, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -80, 112, 1847, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -72, 112, 1847, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -64, 112, 1845, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -56, 112, 1846, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -48, 112, 1846, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -40, 112, 1846, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -32, 112, 1846, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -24, 112, 1846, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -16, 112, 1846, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -8, 112, 1846, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 0, 112, 1846, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 8, 112, 1846, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 104 } }, 16, 16, 1850, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, 32, 32, 1850, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, 48, 32, 1850, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, 64, 32, 1850, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, 80, 32, 1850, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 96, 40, 1850, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, 112, 32, 1850, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -104, 80, 250, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -88, 80, 250, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -72, 80, 250, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -56, 80, 250, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -40, 80, 250, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -24, 80, 250, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -8, 80, 250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 8, 80, 250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 24, 72, 250, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 40, 72, 250, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 56, 80, 250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 72, 80, 250, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 88, 80, 250, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 104, 72, 250, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 112, 104, 250, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, 120, -56, 250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 80 } }, 120, 40, 250, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 96 } }, 128, -56, 250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 128, 40, 250, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 96 } }, 144, -56, 250, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 144, 40, 250, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018B7A8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 133, 0, 0, { 2, 0 } },
    { 143, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018B7D0[58] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, -24, 1285, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -16, 1204, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, -8, 1907, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 0, 879, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -136, 8, 834, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 16, 1331, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 24, 1159, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 32, 1119, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 40, 1030, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 48, 978, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 56, 917, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 64, 925, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 72, 879, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 80, 765, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 88, 727, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 96, 693, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 104, 661, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 112, 633, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 0, 1205, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 8, 882, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 24, 833, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -136, 32, 837, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 40, 1040, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 48, 945, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 56, 878, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 64, 853, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 72, 927, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 80, 765, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 88, 727, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 96, 693, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 104, 661, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 112, 633, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 16, 830, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 24, 827, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 32, 829, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 40, 833, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 48, 838, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 56, 843, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 64, 847, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 72, 800, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 80, 764, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 88, 727, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 96, 693, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 104, 661, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 112, 633, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 48, 829, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 56, 833, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 64, 836, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 72, 815, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, 80, 773, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 88, 683, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -128, 88, 727, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 96, 699, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -112, 96, 693, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 104, 621, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, 104, 661, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 112, 594, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -104, 112, 633, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018BC58[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 58, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mist_shooting_gallery_8018BC70[6] = {
    { 143, 0x4000, { .fields = { 8, 8 } }, 88, 32, 1375, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 16 } }, -120, 16, 1375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 16 } }, -72, 0, 1375, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 16 } }, -72, 16, 1375, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 16 } }, -16, 16, 1375, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 16 } }, 40, 16, 1375, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mist_shooting_gallery_8018BCE8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mist_shooting_gallery_8018BD00[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_mist_shooting_gallery_8018BD10[18] = {
    { { .empty = D_mist_shooting_gallery_80189C14 }, D_mist_shooting_gallery_80189C14, NULL },
    { { .empty = D_mist_shooting_gallery_80189C24 }, D_mist_shooting_gallery_80189C24, NULL },
    { { .empty = D_mist_shooting_gallery_80189C34 }, D_mist_shooting_gallery_80189C34, NULL },
    { { .elements = D_mist_shooting_gallery_80189C44 }, D_mist_shooting_gallery_80189D48, NULL },
    { { .elements = D_mist_shooting_gallery_80189D60 }, D_mist_shooting_gallery_80189FE0, NULL },
    { { .empty = D_mist_shooting_gallery_80189FF8 }, D_mist_shooting_gallery_80189FF8, NULL },
    { { .elements = D_mist_shooting_gallery_8018A008 }, D_mist_shooting_gallery_8018A1E8, NULL },
    { { .elements = D_mist_shooting_gallery_8018A208 }, D_mist_shooting_gallery_8018A3FC, NULL },
    { { .empty = D_mist_shooting_gallery_8018A41C }, D_mist_shooting_gallery_8018A41C, NULL },
    { { .elements = D_mist_shooting_gallery_8018A42C }, D_mist_shooting_gallery_8018A4E0, NULL },
    { { .elements = D_mist_shooting_gallery_8018A508 }, D_mist_shooting_gallery_8018A60C, NULL },
    { { .elements = D_mist_shooting_gallery_8018A634 }, D_mist_shooting_gallery_8018A74C, NULL },
    { { .elements = D_mist_shooting_gallery_8018A764 }, D_mist_shooting_gallery_8018A854, NULL },
    { { .elements = D_mist_shooting_gallery_8018A86C }, D_mist_shooting_gallery_8018AAB0, NULL },
    { { .elements = D_mist_shooting_gallery_8018AAD8 }, D_mist_shooting_gallery_8018B7A8, NULL },
    { { .elements = D_mist_shooting_gallery_8018B7D0 }, D_mist_shooting_gallery_8018BC58, NULL },
    { { .elements = D_mist_shooting_gallery_8018BC70 }, D_mist_shooting_gallery_8018BCE8, NULL },
    { { .empty = D_mist_shooting_gallery_8018BD00 }, D_mist_shooting_gallery_8018BD00, NULL },
};

WorldCollisionTrigger D_mist_shooting_gallery_8018BDE8[28] = {
    { NULL, NULL, NULL, { -0x2B61, -2016, 3103, 0 }, { { -2935, -3232, -228, 0 }, { 2935, -3232, 229, 0 }, { -2935, 3232, -228, 0 }, { 2935, 3232, 229, 0 } }, { 318, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x2B21, -1984, 3007, 0 }, { { 2939, -3232, 183, 0 }, { -2938, -3232, -183, 0 }, { 2939, 3232, 183, 0 }, { -2938, 3232, -183, 0 } }, { -256, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x2F47, -1696, -1767, 0 }, { { 2806, -3232, 891, 0 }, { -2806, -3232, -891, 0 }, { 2806, 3232, 891, 0 }, { -2806, 3232, -891, 0 } }, { -1243, 0, 3911, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x3086, -1696, -1639, 0 }, { { -2806, -3232, -891, 0 }, { 2806, -3232, 891, 0 }, { -2806, 3232, -891, 0 }, { 2806, 3232, 891, 0 } }, { 1241, 0, -3912, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7361, -1696, -3842, 0 }, { { -1091, -3232, -2734, 0 }, { 1091, -3232, 2734, 0 }, { -1091, 3232, -2734, 0 }, { 1091, 3232, 2734, 0 } }, { 3811, 0, -1522, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7169, -1760, -3906, 0 }, { { 1091, -3232, 2734, 0 }, { -1091, -3232, -2734, 0 }, { 1091, 3232, 2734, 0 }, { -1091, 3232, -2734, 0 } }, { -3812, 0, 1520, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9297, -1632, -3409, 0 }, { { 6, -3232, -1901, 0 }, { -6, -3232, 1902, 0 }, { 6, 3232, -1901, 0 }, { -6, 3232, 1902, 0 } }, { 4100, 0, 12, 0 }, { 0, 0, 4096, 0 }, 3744, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9152, -1760, -3457, 0 }, { { -6, -3232, 1902, 0 }, { 6, -3232, -1901, 0 }, { -6, 3232, 1902, 0 }, { 6, 3232, -1901, 0 } }, { -4102, 0, -13, 0 }, { 0, 0, 4096, 0 }, 3744, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7713, -1472, 2848, 0 }, { { -2111, -3232, -7, 0 }, { 2112, -3232, 7, 0 }, { -2111, 3232, -7, 0 }, { 2112, 3232, 7, 0 } }, { 13, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7649, -1664, 2719, 0 }, { { 2112, -3232, 1, 0 }, { -2112, -3232, 0, 0 }, { 2112, 3232, 1, 0 }, { -2112, 3232, 0, 0 } }, { -2, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5858, -1856, 5503, 0 }, { { 0, -3232, 2112, 0 }, { 1, -3232, -2112, 0 }, { 0, 3232, 2112, 0 }, { 1, 3232, -2112, 0 } }, { -4102, 0, -2, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 8, 18, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6082, -1856, 5472, 0 }, { { 1, -3232, -2112, 0 }, { 0, -3232, 2112, 0 }, { 1, 3232, -2112, 0 }, { 0, 3232, 2112, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 18, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1007, -1792, 2783, 0 }, { { -16, -3232, -5824, 0 }, { 15, -3232, 5824, 0 }, { -16, 3232, -5824, 0 }, { 15, 3232, 5824, 0 } }, { 4109, 0, -11, 0 }, { 0, 0, 4096, 0 }, 6656, 0, 10, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1152, -1824, 2814, 0 }, { { 15, -3232, 5824, 0 }, { -16, -3232, -5824, 0 }, { 15, 3232, 5824, 0 }, { -16, 3232, -5824, 0 } }, { -4110, 0, 10, 0 }, { 0, 0, 4096, 0 }, 6656, 0, 9, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7487, -2017, 2782, 0 }, { { -15, -3232, -5824, 0 }, { 16, -3232, 5824, 0 }, { -15, 3232, -5824, 0 }, { 16, 3232, 5824, 0 } }, { 4109, 0, -11, 0 }, { 0, 0, 4096, 0 }, 6656, 0, 11, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7678, -1889, 2910, 0 }, { { -3, -3232, 5825, 0 }, { 3, -3232, -5824, 0 }, { -3, 3232, 5825, 0 }, { 3, 3232, -5824, 0 } }, { -4111, 0, -3, 0 }, { 0, 0, 4096, 0 }, 6656, 0, 10, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2D1F, -1856, 6127, 0 }, { { 817, -3232, 2301, 0 }, { -828, -3232, -2311, 0 }, { 817, 3232, 2301, 0 }, { -828, 3232, -2311, 0 } }, { -3864, 0, 1377, 0 }, { 0, 0, 4096, 0 }, 4047, 0, 11, 12, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2C3F, -1921, 6143, 0 }, { { -815, -3232, -2312, 0 }, { 808, -3232, 2306, 0 }, { -815, 3232, -2312, 0 }, { 808, 3232, 2306, 0 } }, { 3867, 0, -1360, 0 }, { 0, 0, 4096, 0 }, 4047, 0, 12, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2D5F, -1921, -481, 0 }, { { 826, -3232, -2319, 0 }, { -849, -3232, 2278, 0 }, { 826, 3232, -2319, 0 }, { -849, 3232, 2278, 0 } }, { 3850, 0, 1402, 0 }, { 0, 0, 4096, 0 }, 4047, 0, 13, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2E7F, -1952, -384, 0 }, { { -843, -3232, 2294, 0 }, { 833, -3232, -2305, 0 }, { -843, 3232, 2294, 0 }, { 833, 3232, -2305, 0 } }, { -3853, 0, -1404, 0 }, { 0, 0, 4096, 0 }, 4047, 0, 11, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x306E, -1696, 3168, 0 }, { { -1696, -3232, 0, 0 }, { 1697, -3232, 0, 0 }, { -1696, 3232, 0, 0 }, { 1697, 3232, 0, 0 } }, { 0, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 3647, 0, 13, 12, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x305F, -2080, 3007, 0 }, { { 1697, -3232, 0, 0 }, { -1697, -3232, 0, 0 }, { 1697, 3232, 0, 0 }, { -1697, 3232, 0, 0 } }, { 0, 0, 4103, 0 }, { 0, 0, 4096, 0 }, 3647, 0, 12, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7200, -1568, -1280, 0 }, { { 2112, -3232, 1, 0 }, { -2112, -3232, 0, 0 }, { 2112, 3232, 1, 0 }, { -2112, 3232, 0, 0 } }, { -2, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7264, -1601, -1057, 0 }, { { -2112, -3232, 0, 0 }, { 2112, -3232, 1, 0 }, { -2112, 3232, 0, 0 }, { 2112, 3232, 1, 0 } }, { 0, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2785, -1664, 287, 0 }, { { 1396, -3232, 3354, 0 }, { -1396, -3232, -3354, 0 }, { 1396, 3232, 3354, 0 }, { -1396, 3232, -3354, 0 } }, { -3787, 0, 1575, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 18, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3074, -1536, 224, 0 }, { { -1312, -3232, -3128, 0 }, { 1312, -3232, 3127, 0 }, { -1312, 3232, -3128, 0 }, { 1312, 3232, 3127, 0 } }, { 3781, 0, -1587, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 9, 18, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3043, -1536, 6237, 0 }, { { 1284, -3232, -3139, 0 }, { -1283, -3232, 3140, 0 }, { 1284, 3232, -3139, 0 }, { -1283, 3232, 3140, 0 } }, { 3795, 0, 1551, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 9, 18, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2787, -1632, 6272, 0 }, { { -1285, -3232, 3138, 0 }, { 1282, -3232, -3141, 0 }, { -1285, 3232, 3138, 0 }, { 1282, 3232, -3141, 0 } }, { -3797, 0, -1553, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 18, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_mist_shooting_gallery_8018C638[21] = {
    { NULL, NULL, NULL, { -0x2E7B, -64, 5421, 0 }, { { -444, 0, -595, 0 }, { 453, 0, -597, 0 }, { -453, 0, 598, 0 }, { 445, 0, 596, 0 } }, { 0, 4111, 0, 0 }, { 4093, 0, 86, 0 }, 749, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9697, -48, -2929, 0 }, { { -419, 0, -538, 0 }, { 414, 0, -528, 0 }, { -413, 0, 528, 0 }, { 421, 0, 538, 0 } }, { 0, 4097, 0, 0 }, { -4093, 0, -86, 0 }, 680, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 36, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7955, -48, -1633, 0 }, { { -759, 0, 356, 0 }, { -759, 0, -352, 0 }, { 755, 0, 350, 0 }, { 765, 0, -351, 0 } }, { 0, 4111, 0, 0 }, { -72, 0, -4095, 0 }, 839, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 86, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7920, -48, -784, 0 }, { { -698, 0, 388, 0 }, { -688, 0, -381, 0 }, { 688, 0, 382, 0 }, { 698, 0, -388, 0 } }, { 0, 4096, 0, 0 }, { 315, 0, 4082, 0 }, 796, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 101, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8784, -48, -2912, 0 }, { { -467, 0, -778, 0 }, { 462, 0, -768, 0 }, { -461, 0, 768, 0 }, { 469, 0, 778, 0 } }, { 0, 4105, 0, 0 }, { 4092, 0, -115, 0 }, 907, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 66, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6146, -64, 3023, 0 }, { { -1586, 0, -1011, 0 }, { -337, 0, -1013, 0 }, { -1579, 0, 1014, 0 }, { -336, 0, 1012, 0 } }, { 0, 4116, 0, 0 }, { -4093, 0, 114, 0 }, 1876, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x27F2, -64, 3999, 0 }, { { -680, 0, -675, 0 }, { 671, 0, -677, 0 }, { -667, 0, 678, 0 }, { 677, 0, 676, 0 } }, { 0, 4097, 0, 0 }, { 86, 0, -4093, 0 }, 957, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9424, -64, 2944, 0 }, { { -680, 0, -1059, 0 }, { 671, 0, -1060, 0 }, { -667, 0, 1062, 0 }, { 677, 0, 1060, 0 } }, { 0, 4099, 0, 0 }, { -4093, 0, -86, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x2AB0, -64, -1, 0 }, { { -1540, 0, -867, 0 }, { 1501, 0, -869, 0 }, { -1501, 0, 870, 0 }, { 1541, 0, 868, 0 } }, { 0, 4103, 0, 0 }, { 4093, 0, 86, 0 }, 1764, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 4, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -8192, -64, 3056, 0 }, { { -440, 0, -2931, 0 }, { 431, 0, -2933, 0 }, { -427, 0, 2934, 0 }, { 437, 0, 2932, 0 } }, { 0, 4114, 0, 0 }, { 4093, 0, 86, 0 }, 2963, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7888, -64, 5824, 0 }, { { -755, 0, -458, 0 }, { 750, 0, -448, 0 }, { -749, 0, 448, 0 }, { 757, 0, 458, 0 } }, { 0, 4107, 0, 0 }, { -115, 0, -4092, 0 }, 884, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5184, -64, -3216, 0 }, { { -490, 0, 1220, 0 }, { -480, 0, -1213, 0 }, { 480, 0, 1214, 0 }, { 490, 0, -1220, 0 } }, { 0, 4095, 0, 0 }, { -4093, 0, 115, 0 }, 1311, WORLD_COLLISION_TRIGGER_ACTION_CAP, 34, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x2980, -64, -3232, 0 }, { { -970, 0, 420, 0 }, { -960, 0, -413, 0 }, { 960, 0, 414, 0 }, { 970, 0, -420, 0 } }, { 0, 4096, 0, 0 }, { -86, 0, 4093, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_CAP, 33, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5968, -64, -2368, 0 }, { { -1082, 0, 340, 0 }, { -1072, 0, -333, 0 }, { 1072, 0, 334, 0 }, { 1082, 0, -340, 0 } }, { 0, 4106, 0, 0 }, { 86, 0, -4094, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 26, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6432, -64, -4192, 0 }, { { -1402, 0, 340, 0 }, { -1392, 0, -333, 0 }, { 1392, 0, 334, 0 }, { 1402, 0, -340, 0 } }, { 0, 4102, 0, 0 }, { -87, 0, 4093, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_CAP, 27, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x2B10, -64, 5760, 0 }, { { -1004, 0, -355, 0 }, { 1013, 0, -357, 0 }, { -1013, 0, 358, 0 }, { 1005, 0, 356, 0 } }, { 0, 4100, 0, 0 }, { 86, 0, -4093, 0 }, 1070, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x2DC0, -64, -992, 0 }, { { -352, 0, -2275, 0 }, { 359, 0, -2277, 0 }, { -355, 0, 2278, 0 }, { 351, 0, 2276, 0 } }, { 0, 4099, 0, 0 }, { 4093, 0, 86, 0 }, 2304, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x2DC0, -64, 2688, 0 }, { { -352, 0, -1251, 0 }, { 359, 0, -1253, 0 }, { -355, 0, 1254, 0 }, { 351, 0, 1252, 0 } }, { 0, 4106, 0, 0 }, { 4093, 0, 86, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x29C0, -64, 4128, 0 }, { { -680, 0, -675, 0 }, { 671, 0, -677, 0 }, { -667, 0, 678, 0 }, { 677, 0, 676, 0 } }, { 0, 4097, 0, 0 }, { -3826, 0, -1460, 0 }, 957, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x2860, -64, 4096, 0 }, { { -352, 0, -1987, 0 }, { 359, 0, -1989, 0 }, { -355, 0, 1990, 0 }, { 351, 0, 1988, 0 } }, { 0, 4099, 0, 0 }, { -4093, 0, -86, 0 }, 2019, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7104, -64, 3040, 0 }, { { -626, 0, -1011, 0 }, { 623, 0, -1013, 0 }, { -619, 0, 1014, 0 }, { 624, 0, 1012, 0 } }, { 0, 4116, 0, 0 }, { -4085, 0, -288, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Point lights for the gallery's default room lighting, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits (`ONE` is full strength). The loaded room overlay owns
/// these mutable records: coordinate updates rebuild transforms and shading
/// queries overwrite attenuation.
static WorldCoordPointLight _gMistShootingGalleryDefaultPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2936, -1770, -2280 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 4014, 3440 },
        },
        .inner = 0,
        .outer = 2800,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2710, -1770, 750 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 0,
        .outer = 3300,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7000, -1770, -3110 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 0,
        .outer = 2800,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7750, -1770, 3195 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 0,
        .outer = 4941,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1078, -1770, 6131 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 621,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2138, -1770, 716 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1041,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4639, -1770, 356 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 581,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4420, -1369, 6050 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 821,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2F6C, -1770, -57 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 641,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3070, -1770, 5845 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 801,
        .outer = 3500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3264, -1270, 2965 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 581,
        .outer = 2400,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 906, -1939, 3030 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2000, 2000, 2000 },
        },
        .inner = 441,
        .outer = 8005,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8500, -2494, 3030 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2000, 2000, 2000 },
        },
        .inner = 421,
        .outer = 8842,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2A59, -1770, 4361 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 243,
        .outer = 2622,
    },
};

WorldCoordRoomLights gMistShootingGalleryDefaultRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gMistShootingGalleryDefaultPointLights),
    .pointLights           = _gMistShootingGalleryDefaultPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCoordLight D_mist_shooting_gallery_8018D1CC[1] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5655, -10, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } },
};

WorldCoordPointLight D_mist_shooting_gallery_8018D224[18] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2AF8, -1500, 5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1400, 2400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9400, -1950, 1850 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9400, -1950, 3070 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9400, -1950, 4235 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9400, -1950, 5470 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8000, -1500, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6215, -1500, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7505, -2000, 295 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -2000, 5500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2AF8, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1400, 2400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -2000, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2AF8, -1500, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1400, 2400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x290E, -1500, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1400, 2400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7505, -1500, 495 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7505, -1500, 2740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7505, -1500, 5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9400, -1950, -545 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9400, -1950, 650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 1000 },
};

WorldCoordSpotLight D_mist_shooting_gallery_8018D8E4[15] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2D05, -2750, 590 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2867, 2457 }, { 0, 0 } }, { -2247, 3424, 53, 0 }, 3000, 4000, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -455, -2715, -770 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, { 1297, 3504, 1677, 0 }, 3000, 4000, 682 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4980, -2715, -195 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, { -349, 3719, 1678, 0 }, 3000, 4000, 682 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7835, -2715, 260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, { 211, 3966, 1000, 0 }, 3000, 4000, 682 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7835, -2715, 5720 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, { 183, 3830, -1439, 0 }, 3000, 4000, 682 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4870, -2715, 6185 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, { -567, 3666, -1735, 0 }, 3000, 4000, 682 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2D05, -2750, -465 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2867, 2457 }, { 0, 0 } }, { -2247, 3424, 53, 0 }, 3000, 4000, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -350, -2715, 6725 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, { 1164, 3572, -1631, 0 }, 3000, 4000, 682 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x311A, -2715, -375 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, { -1170, 3874, 627, 0 }, 3000, 4000, 568 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x311A, -2715, 6370 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, { -1164, 3856, -738, 0 }, 3000, 4000, 568 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x31EC, -2715, 2995 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, { -691, 4037, 0, 0 }, 3000, 4000, 455 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2D05, -2750, -1495 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2867, 2457 }, { 0, 0 } }, { -2247, 3424, 53, 0 }, 3000, 4000, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2D05, -2750, -2880 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2867, 2457 }, { 0, 0 } }, { -2247, 3424, 53, 0 }, 3000, 4000, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2BD9, -2750, -3470 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2867, 2457 }, { 0, 0 } }, { 380, 2861, -2906, 0 }, 3000, 4000, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9910, -2750, -3470 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 2867, 2457 }, { 0, 0 } }, { -939, 2797, -2840, 0 }, 3000, 4000, 341 },
};

WorldCoordRoomLights D_mist_shooting_gallery_8018DF38 = { ARRAY_SIZE(D_mist_shooting_gallery_8018D1CC), D_mist_shooting_gallery_8018D1CC, ARRAY_SIZE(D_mist_shooting_gallery_8018D224), D_mist_shooting_gallery_8018D224, ARRAY_SIZE(D_mist_shooting_gallery_8018D8E4), D_mist_shooting_gallery_8018D8E4 };

AreaResource D_mist_shooting_gallery_8018DF50[3] = {
    { 76, 76, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_107600_80134F94 },
    { 143, 151, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_215100_8015E5D0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_mist_shooting_gallery_8018DF74[12] = {
    { NULL, NULL },
    { D_map_akropolis_8017BDEC, D_mist_shooting_gallery_8018DF50 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

// Keep the read-only table at its original position among the overlay's data.
const WorldCoordRoomAmbientEntry gMistShootingGalleryViewAmbientTable[MIST_SHOOTING_GALLERY_AMBIENT_VIEW_COUNT + 1] SECTION(".data") = {
    [0]  = { .viewCount = ARRAY_SIZE(gMistShootingGalleryViewAmbientTable) - 1 },
    [1]  = { .color = { 16, 16, 16, 16 } },
    [2]  = { .color = { 16, 16, 16, 16 } },
    [3]  = { .color = { 16, 16, 16, 16 } },
    [4]  = { .color = { 16, 16, 16, 16 } },
    [5]  = { .color = { 16, 16, 16, 16 } },
    [6]  = { .color = { 16, 16, 16, 16 } },
    [7]  = { .color = { 340, 340, 350, 341 } },
    [8]  = { .color = { 800, 820, 820, 812 } },
    [9]  = { .color = { 600, 600, 600, 600 } },
    [10] = { .color = { 500, 500, 500, 500 } },
    [11] = { .color = { 500, 500, 500, 500 } },
    [12] = { .color = { 16, 16, 16, 16 } },
    [13] = { .color = { 16, 16, 16, 16 } },
    [14] = { .color = { 16, 16, 16, 16 } },
    [15] = { .color = { 16, 16, 16, 16 } },
    [16] = { .color = { 16, 16, 16, 16 } },
    [17] = { .color = { 16, 16, 16, 16 } },
    [18] = { .color = { 600, 600, 600, 600 } },
};

WorldCollisionFootstepSounds D_mist_shooting_gallery_8018E06C = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

WorldCollisionFootstepSounds D_mist_shooting_gallery_8018E078 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_mist_shooting_gallery_8018E084[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mist_shooting_gallery_8018E08C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mist_shooting_gallery_8018E06C },
};

WorldCollisionSurfaceProperties D_mist_shooting_gallery_8018E094[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mist_shooting_gallery_8018E078 },
};

WorldCollisionSurfaceProperties* D_mist_shooting_gallery_8018E09C[8] = {
    D_mist_shooting_gallery_8018E084,
    D_mist_shooting_gallery_8018E08C,
    D_mist_shooting_gallery_8018E094,
    D_mist_shooting_gallery_8018E084,
    D_mist_shooting_gallery_8018E084,
    D_mist_shooting_gallery_8018E084,
    D_mist_shooting_gallery_8018E084,
    D_mist_shooting_gallery_8018E084,
};

s32 D_mist_shooting_gallery_8018E0BC = 0;

Task* D_mist_shooting_gallery_8018E0C4;

void mistShootingGalleryTracerTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_TRACER_INIT               = 0,
        MIST_SHOOTING_GALLERY_TRACER_FADE               = 1,
        MIST_SHOOTING_GALLERY_TRACER_HELD_SIZE          = 0x600,
        MIST_SHOOTING_GALLERY_TRACER_RUNNING_SIZE       = 0x400,
        MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_MASK    = 0x3FF,
        MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_RADIUS  = 0x200,
        MIST_SHOOTING_GALLERY_TRACER_VERTICAL_OFFSET    = 0x800,
        MIST_SHOOTING_GALLERY_TRACER_INITIAL_BRIGHTNESS = 0x80,
        MIST_SHOOTING_GALLERY_TRACER_FADE_STEP          = 8,
    };

    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];
    u32         randomX;
    u32         randomZ;
    u32         randomAngle;

    /// Queues the additive blue tint from this task's work and three-byte rgb buffer.
    ///
    /// Expands to four statements; invoke only inside an explicit brace-delimited block.
    /// Each component narrows to a GPU byte. The work's scale is read three times.
#define MIST_SHOOTING_GALLERY_DRAW_TRACER_TINT() \
    rgb[0] = work->scale >> 1;                   \
    rgb[1] = work->scale >> 1;                   \
    rgb[2] = work->scale;                        \
    effectDrawScreenTint(rgb, GPU_BLEND_ADD)

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        spriteQuadDraw(coord, work->index, MIST_SHOOTING_GALLERY_TRACER_HELD_SIZE, work->angle);
        _beamStripDraw(coord, &work->pos, work->index, MIST_SHOOTING_GALLERY_TRACER_HELD_SIZE);
        MIST_SHOOTING_GALLERY_DRAW_TRACER_TINT();
        return;
    }

    work->age++;
    switch (task->state) {
        case MIST_SHOOTING_GALLERY_TRACER_INIT:
            // Attach the origin, but seed the endpoint from the existing world cache.
            coord->parent       = work->parent;
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = 0;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = MIST_SHOOTING_GALLERY_TRACER_FADE;

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomX         = gRandomLcgState;
            work->pos.vx    = (u16)coord->workm.t[0] - ((randomX >> 16 & MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_MASK) - MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_RADIUS);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomZ         = gRandomLcgState;
            work->pos.vy    = coord->workm.t[1] - MIST_SHOOTING_GALLERY_TRACER_VERTICAL_OFFSET;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomAngle     = gRandomLcgState;
            work->pos.vz    = (u16)coord->workm.t[2] - ((randomZ >> 16 & MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_MASK) - MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_RADIUS);
            work->scale     = MIST_SHOOTING_GALLERY_TRACER_INITIAL_BRIGHTNESS;
            work->angle     = randomAngle >> 16 & ACTOR_TRANSFORM_ANGLE_MASK;
        case MIST_SHOOTING_GALLERY_TRACER_FADE:
            // Only odd running ticks advance the texture frame and draw the tracer.
            if (work->age & 1) {
                spriteQuadDraw(coord, ++work->index, MIST_SHOOTING_GALLERY_TRACER_RUNNING_SIZE, work->angle);
                _beamStripDraw(coord, &work->pos, work->index, MIST_SHOOTING_GALLERY_TRACER_RUNNING_SIZE);
            }
            MIST_SHOOTING_GALLERY_DRAW_TRACER_TINT();
            work->scale -= MIST_SHOOTING_GALLERY_TRACER_FADE_STEP;
            if (work->scale < MIST_SHOOTING_GALLERY_TRACER_FADE_STEP) {
                effectKillTask(work, task);
            }
            return;
    }
#undef MIST_SHOOTING_GALLERY_DRAW_TRACER_TINT
}

/// Texel width and horizontal stride of each cell in the gallery flash's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the gallery flash's texture row, repeated by its frame counter.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of the gallery flash strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
#define SPRITE_QUAD_V1    0x5F
/// Perspective-sizing multiplier for the gallery effect sprite.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
/// Muzzle-flash palette: VRAM X=304 words, Y=266 scanlines.
#define SPRITE_QUAD_CLUT     getClut(304, 266)
#define SPRITE_QUAD_OTZ_BIAS 0
#include "../../shared/sprite_quad_draw.inc.c"

#define BEAM_STRIP_OTZ_BIAS 0
#include "../../shared/beam_strip_draw.inc.c"

static void func_mist_shooting_gallery_80182B1C(Task* arg0)
{
    Task*                    slot;
    GameActor*               actor;
    MistShootingGalleryWork* work;

    slot  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor = slot->work;

    work       = memCalloc(sizeof(MistShootingGalleryWork), 0);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }

    D_mist_shooting_gallery_8018E0C4 = arg0;
    arg0->exitCallback               = func_mist_shooting_gallery_80184A80;
    arg0->state++;
    work->course = arg0->spawnArg1.value & 0xF;
    work->clockX = -0xDC;

    actor->weaponShape.ends[0].vz =
        (actor->weaponShape.ends[1].vz + D_80112F60[gPlayerStatus.weapon]) << 1;
    playerActorEnterLocomotion(slot, 1);

    if (work->course < 3) {
        Gp_StateC08.flags |= ATTACHMENT_FLAG_SWAP_LOCK;
        if (work->course < 2) {
            actor->movementInputDisabled = 1;
            displayAcquireMenuHold();
        }
    }
    gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
    gGameSession->battleResetPending            = 0;
    (sceneAcquireBattleRef)(0);
}

/// Per-frame update for the gallery's bonus course. START (`0x100`) aborts the
/// whole mini-game; otherwise the seventeen states run the banner countdown
/// (`captionStep` steps the sprite, `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout` picks which variant), seed the
/// course by spawning individual records of `D_mist_shooting_gallery_80186900[0]`
/// on a timer, and finally enter the wave loop of state 15. State 16 is the
/// out-of-ammo banner: it is entered from anywhere the moment the equipped
/// weapon's stock drops below the round's minimum, remembers the interrupted
/// state in `resumePhase` and returns to it once the banner has played out.
static void func_mist_shooting_gallery_80182C58(Task* arg0)
{
    MistShootingGalleryWork*   work;
    _MistShootingGallerySpawn* spawn;
    s32                        limit;
    s32                        bonus;
    u16                        t1;
    u16                        t2;
    u16                        t5;
    u16                        t6;
    u16                        t8;
    u16                        t9;
    u16                        t12;
    u16                        t13;
    u16                        key;
    u16                        wave;
    u16                        prev;
    u8                         step;

    work  = arg0->work;
    bonus = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_SELECT) != 0) {
        func_actor_215100_8014A9A0();
        return;
    }

    switch (work->phase) {
        case 0:
            work->timeLeft = 0x708;
            work->timer    = 0x3C;
            work->phase++;
        case 1:
            t1          = work->timer - 1;
            work->timer = t1;
            if ((s32)(t1 << 16) <= 0) {
                work->timer = 0x1E;
                work->phase++;
                func_mist_shooting_gallery_80184BB8(0x11, 0, 0x8E0);
            }
            break;
        case 2:
            t2          = work->timer - 1;
            work->timer = t2;
            if ((s32)(t2 << 16) <= 0) {
                work->phase++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 1, 0x8E0);
            }
            break;
        case 3:
            work->phase++;
            func_mist_shooting_gallery_80184BB8(0x11, 4, 0x8E0);
            break;
        case 4:
            work->timer = 0x1E;
            work->phase++;
            func_mist_shooting_gallery_80184BB8(0x11, bonus + 5, 0x8E0);
            break;
        case 5:
            t5          = work->timer;
            work->timer = t5 - 1;
            if ((s32)(t5 << 16) <= 0) {
                work->timer = 0xF;
                work->phase++;
                taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
            }
            break;
        case 6:
            t6          = work->timer - 1;
            work->timer = t6;
            if ((s32)(t6 << 16) <= 0) {
                work->phase++;
                spawn = &D_mist_shooting_gallery_80186900[0][work->spawnIndex];
                func_mist_shooting_gallery_80184CD0(arg0, spawn);
                work->spawnIndex++;
            }
            break;
        case 7:
            if (work->targetLockMask != 0) {
                work->timer = 0xF;
                work->phase++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 8, 0x8E0);
            } else if (work->liveTargets == 0) {
                work->timer = 0xF;
                work->phase++;
            }
            break;
        case 8:
            if (work->liveTargets == 0) {
                t8          = work->timer - 1;
                work->timer = t8;
                if ((s32)(t8 << 16) <= 0) {
                    work->timer = 0x3C;
                    work->phase++;
                    spawn = &D_mist_shooting_gallery_80186900[0][work->spawnIndex];
                    func_mist_shooting_gallery_80184CD0(arg0, spawn);
                    work->spawnIndex++;
                }
            }
            break;
        case 9:
            t9          = work->timer - 1;
            work->timer = t9;
            if ((s32)(t9 << 16) <= 0) {
                work->phase++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 0xB, 0x8E0);
            }
            break;
        case 10:
            if (work->liveTargets == 0) {
                spawn = &D_mist_shooting_gallery_80186900[0][work->spawnIndex];
                func_mist_shooting_gallery_80184CD0(arg0, spawn);
                work->spawnIndex++;
                if (work->spawnIndex == 7) {
                    work->phase++;
                }
            }
            break;
        case 11:
            if (work->liveTargets == 0) {
                work->timer = 0x1E;
                work->phase++;
            }
            break;
        case 12:
            t12         = work->timer - 1;
            work->timer = t12;
            if ((s32)(t12 << 16) <= 0) {
                spawn = &D_mist_shooting_gallery_80186900[0][work->spawnIndex];
                func_mist_shooting_gallery_80184CD0(arg0, spawn);
                work->spawnIndex++;
                if (work->spawnIndex == 0xC) {
                    work->timer = 0x3C;
                    work->phase++;
                } else {
                    work->timer = 0xA;
                }
            }
            break;
        case 13:
            t13         = work->timer - 1;
            work->timer = t13;
            if ((s32)(t13 << 16) <= 0) {
                work->phase++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 0xE, 0x8E0);
            }
            break;
        case 14:
            if (work->liveTargets != 0) {
                break;
            }
            work->phase++;
            displayReleaseMenuHold();
        case 15:
            spawn = &D_mist_shooting_gallery_80186900[0][work->spawnIndex];
            key   = spawn->frame;
            if (key != MIST_SHOOTING_GALLERY_SPAWN_END) {
                if (key != MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR) {
                    if (work->scriptFrame == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->spawnIndex++;
                        } while (work->scriptFrame == spawn->frame);
                    }
                    wave = work->scriptFrame;
                    if (wave < MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT) {
                        work->scriptFrame = wave + 1;
                    }
                } else if (work->liveTargets == 0) {
                    work->spawnIndex++;
                }
            }
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = 0;
                arg0->state++;
            }
            break;
        case 16:
            func_mist_shooting_gallery_80184BB8(0x11, work->captionStep, 0x8E0);
            step = work->captionStep;
            if (step == 0x15) {
                work->phase = work->resumePhase;
                displayReleaseMenuHold();
            } else {
                work->captionStep = step + 1;
            }
            break;
    }

    if (work->interrupted == 0) {
        limit = 2;
        if (gPlayerStatus.weapon == 2) {
            limit = 4;
        }
        if (equipmentConsumeWeaponLoad(gPlayerStatus.weapon + 0x7F, EQUIPMENT_WEAPON_LOAD_QUERY_PRIMARY) < limit) {
            prev              = work->phase;
            work->phase       = 0x10;
            work->interrupted = 1;
            work->captionStep = 0x11;
            work->resumePhase = prev;
        }
    }
}

/// Per-frame update for the gallery course itself. START (`0x100`) aborts the
/// whole mini-game; otherwise the state runs a "3, 2, 1, GO" countdown
/// (`captionStep` steps the digit sprite once a second) before releasing the
/// menu hold and entering the wave loop. The loop spawns every record
/// of `D_mist_shooting_gallery_80186904` that carries the current script frame,
/// draws the remaining time, and restarts the state machine once the clock
/// runs out.
static void func_mist_shooting_gallery_801831B0(Task* arg0)
{
    MistShootingGalleryWork*   work;
    _MistShootingGallerySpawn* spawn;
    u16                        intro;
    u16                        ready;
    u16                        start;
    u16                        key;
    u16                        wave;
    u8                         step;

    work = arg0->work;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_SELECT) != 0) {
        func_actor_215100_8014A9A0();
        return;
    }

    switch (work->phase) {
        case 0:
            work->timeLeft = 0xE10;
            work->timer    = 0x3C;
            work->phase++;
        case 1:
            intro       = work->timer - 1;
            work->timer = intro;
            if ((s32)(intro << 16) <= 0) {
                func_mist_shooting_gallery_80184BB8(0x12, work->captionStep, 0x8E0);
                step = work->captionStep;
                if (step == 4) {
                    work->timer = 0x3C;
                    work->phase++;
                    return;
                }
                work->captionStep = step + 1;
                return;
            }
        default:
            return;
        case 2:
            ready       = work->timer;
            work->timer = ready - 1;
            if ((s32)(ready << 16) <= 0) {
                work->timer = 0xA;
                work->phase++;
                taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
                sceneEngageBattle(1);
                return;
            }
            break;
        case 3:
            start       = work->timer;
            work->timer = start - 1;
            if ((s32)(start << 16) <= 0) {
                work->phase++;
                displayReleaseMenuHold();
                case 4:
                    spawn = &D_mist_shooting_gallery_80186904[work->spawnIndex];
                    key   = spawn->frame;
                    if (key != MIST_SHOOTING_GALLERY_SPAWN_END) {
                        if (key != MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR) {
                            if (work->scriptFrame == key) {
                                do {
                                    func_mist_shooting_gallery_80184CD0(arg0, spawn);
                                    spawn++;
                                    work->spawnIndex++;
                                } while (work->scriptFrame == spawn->frame);
                            }
                            wave = work->scriptFrame;
                            if (wave < MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT) {
                                work->scriptFrame = wave + 1;
                            }
                        } else if (work->liveTargets == 0) {
                            work->spawnIndex++;
                        }
                    }
                    _mistShootingGalleryDrawCountdownClock(work);
                    if (_mistShootingGalleryTickCourseClock(work) == 0) {
                        work->phase = 0;
                        arg0->state++;
                    }
            }
            break;
    }
}

/// Per-frame update for the gallery's second bonus course. States 0-3 run the
/// "ready" banner and the hand-off wait on `gGameSession::location.loc.view`, gated on
/// the countdown hold `gDisplayState.pendingMode`; states 4-5 wait on the player picking up
/// item 0x40, states 6-8 count the banner up through `captionStep` while
/// `gSceneCombatState.actorControl` holds, state 9 spawns the start jingle and state 10 is the
/// wave loop over `D_mist_shooting_gallery_80186908`. `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout` picks the
/// banner sprite the hand-off draws (`variant + 4`).
static void func_mist_shooting_gallery_8018341C(Task* arg0)
{
    MistShootingGalleryWork*   work;
    _MistShootingGallerySpawn* spawn;
    s32                        bonus;
    s32                        stocked;
    u16                        key;
    u16                        wave;
    u16                        ready;
    u8                         step;

    work  = arg0->work;
    bonus = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;

    switch (work->phase) {
        case 0:
            work->timeLeft = 0x1518;
            work->timer    = 0x1E;
            D_80115768     = 1;
            work->phase++;
        case 1:
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->captionStep, 0x8E0);
                    step = work->captionStep;
                    if (step == 3) {
                        work->timer = 0xF;
                        D_80115768  = 0;
                        work->phase++;
                    } else {
                        work->captionStep = step + 1;
                    }
                }
            } else {
                work->timer--;
            }
            break;
        case 2:
            if (gGameSession->location.loc.view == 0x12) {
                if (work->timer <= 0) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        work->timer = 1;
                        work->phase++;
                        func_mist_shooting_gallery_80184BB8(0x13, bonus + 4, 0x8E0);
                    }
                } else {
                    work->timer--;
                }
            }
            break;
        case 3:
            if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                work->actionTriggered = 0;
                work->captionStep     = 8;
                work->phase++;
                func_mist_shooting_gallery_80184BB8(0x13, 7, 0x8E0);
            }
            break;
        case 4:
            if (work->actionTriggered != 0) {
                stocked = Gp_HasStockedItem(0x40);
                if (stocked != 1) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->captionStep, 0x8E0);
                    if (work->captionStep == 9) {
                        work->actionTriggered = 0;
                        work->phase++;
                    }
                    work->captionStep++;
                } else {
                    work->phase       = 6;
                    work->timer       = 1;
                    work->captionStep = 0xA;
                }
            }
            break;
        case 5:
            if (Gp_HasStockedItem(0x40) == 1) {
                work->timer = 0xF;
                work->phase++;
            } else if (work->actionTriggered != 0) {
                func_mist_shooting_gallery_80184BB8(0x13, 9, 0x8E0);
                work->actionTriggered = 0;
            }
            break;
        case 6:
            if (work->timer <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->captionStep, 0x8E0);
                    if (work->captionStep == 0xB) {
                        work->timer = 0xF;
                        work->phase++;
                        sceneEngageBattle(1);
                    }
                    work->captionStep++;
                }
            } else {
                work->timer--;
            }
            break;
        case 7:
            work->timer--;
            if ((s32)(work->timer << 16) <= 0) {
                func_mist_shooting_gallery_80184CD0(arg0, &D_mist_shooting_gallery_80186908[work->spawnIndex]);
                work->spawnIndex++;
                if (work->spawnIndex == 3) {
                    work->timer = 0x3C;
                    work->phase++;
                } else {
                    work->timer = 0xF;
                }
            }
            break;
        case 8:
            if (work->timer <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->captionStep, 0x8E0);
                    if (work->captionStep == 0x13) {
                        work->timer = 0xF;
                        work->phase++;
                    }
                    work->captionStep++;
                }
            } else {
                work->timer--;
            }
            break;
        case 9:
            ready       = work->timer;
            work->timer = ready - 1;
            if ((s32)(ready << 16) <= 0) {
                work->timer = 0xA;
                work->phase++;
                taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
            }
            break;
        case 10:
            spawn = &D_mist_shooting_gallery_80186908[work->spawnIndex];
            key   = spawn->frame;
            if (key != MIST_SHOOTING_GALLERY_SPAWN_END) {
                if (key != MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR) {
                    if (work->scriptFrame == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->spawnIndex++;
                        } while (work->scriptFrame == spawn->frame);
                    }
                    wave = work->scriptFrame;
                    if (wave < MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT) {
                        work->scriptFrame = wave + 1;
                    }
                } else if (work->liveTargets == 0) {
                    work->spawnIndex++;
                    if (work->spawnIndex >= 0x49) {
                        work->spawnIndex  = 0xE;
                        work->scriptFrame = 0xF0;
                    }
                }
            }
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = 0;
                arg0->state++;
            }
            break;
    }
}

/// Per-frame update for the gallery's first bonus course. States 0-3 run the
/// "ready" banner and the hand-off wait on `gGameSession::location.loc.view`, state 4
/// seeds the first two records of `D_mist_shooting_gallery_8018690C`, states
/// 5-7 hand the player over to actor mode 2 while the banner counts up through
/// `captionStep`, and state 8 is the wave loop proper. `Gp_StateC08.effectPhase` is the abort
/// request: once it is raised the machine saves its place in `resumePhase` /
/// `resumeCaptionStep` and jumps to the state-9 shutdown banner, which restores them.
static void func_mist_shooting_gallery_801838FC(Task* arg0)
{
    MistShootingGalleryWork*   work;
    _MistShootingGallerySpawn* spawn;
    ActorTransform             xform;
    GameActor*                 actor;
    s32                        mode;
    s32                        bonus;
    u16                        key;
    u16                        wave;
    u16                        prev;
    s32                        abort;
    u8                         step;
    u8                         hold;

    work  = arg0->work;
    bonus = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;

    switch (work->phase) {
        case 0:
            work->timeLeft = 0x1518;
            work->timer    = 0x1E;
            D_80115768     = 1;
            work->phase++;
        case 1:
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    func_mist_shooting_gallery_80184BB8(0x14, 0, 0x8E0);
                    work->timer = 0xF;
                    D_80115768  = 0;
                    work->phase++;
                }
            } else {
                work->timer--;
            }
            break;
        case 2:
            if (gGameSession->location.loc.view == 0x12) {
                if (work->timer <= 0) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        work->timer = 1;
                        work->phase++;
                        func_mist_shooting_gallery_80184BB8(0x14, 7, 0x8E0);
                    }
                } else {
                    work->timer--;
                }
            }
            break;
        case 3:
            if (work->actionTriggered != 0) {
                work->timer = 0x1E;
                work->phase++;
                taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
                sceneEngageBattle(1);
            }
            break;
        case 4:
            work->timer--;
            if ((s32)(work->timer << 16) <= 0) {
                func_mist_shooting_gallery_80184CD0(arg0, &D_mist_shooting_gallery_8018690C[work->spawnIndex]);
                work->spawnIndex++;
                if (work->spawnIndex == 2) {
                    work->captionStep = 8;
                    work->timer       = 0x3C;
                    work->phase++;
                } else {
                    work->timer = 0xF;
                }
            }
            break;
        case 5:
            work->timer--;
            if ((s32)(work->timer << 16) <= 0) {
                func_mist_shooting_gallery_80184BB8(0x14, work->captionStep, 0x8E0);
                if (work->captionStep == 0xA) {
                    work->timer = 0x1E;
                    work->phase++;
                    func_800E9BDC(5, 0xA);
                    xform.rot.vy = 0xC00;
                    playerActorTurnToYaw(
                        gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0, &xform, 0);
                }
                work->captionStep++;
            }
            break;
        case 6:
            actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
            func_800E9BDC(5, 0xA);
            if (actor->scriptedMotionPending == 0) {
                Gp_EnterActorMode2(
                    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0, 2, 0);
                work->phase++;
                mode               = 0x10;
                Gp_StateC08.flags |= ATTACHMENT_FLAG_OPEN_WHEEL;
                work->timer        = 4;
                work->captionStep  = 0xE;
                if (bonus == 2) {
                    mode = 2;
                }
                func_mist_shooting_gallery_80184BB8(0x14, bonus + 0xB, mode);
            }
            break;
        case 7:
            func_800E9BDC(5, 0xA);
            if (work->timer <= 0) {
                if ((u32)((u8)Gp_StateC08.mode - ATTACHMENT_MODE_ARMED) >= 2) {
                    func_mist_shooting_gallery_80184BB8(0x14, work->captionStep, 0x8E0);
                    if (work->captionStep == 0x12) {
                        work->phase++;
                        func_800E9BDC(0, 0xA);
                        Gp_StateC08.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
                    }
                    work->captionStep++;
                }
            } else {
                work->timer--;
            }
            break;
        case 8:
            spawn = &D_mist_shooting_gallery_8018690C[work->spawnIndex];
            key   = spawn->frame;
            if (key != MIST_SHOOTING_GALLERY_SPAWN_END) {
                if (key != MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR) {
                    if (work->scriptFrame == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->spawnIndex++;
                        } while (work->scriptFrame == spawn->frame);
                    }
                    wave = work->scriptFrame;
                    if (wave < MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT) {
                        work->scriptFrame = wave + 1;
                    }
                } else if (work->liveTargets == 0) {
                    work->spawnIndex++;
                    if (work->spawnIndex >= 0x68) {
                        work->spawnIndex  = 9;
                        work->scriptFrame = 0x90;
                    }
                }
            }
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = 0;
                arg0->state++;
            }
            break;
        case 9:
            if (work->timer <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    func_mist_shooting_gallery_80184BB8(0x14, work->captionStep, 0x8E0);
                    step = work->captionStep;
                    if (step == 0x17) {
                        work->phase       = work->resumePhase;
                        work->captionStep = work->resumeCaptionStep;
                    } else {
                        work->captionStep = step + 1;
                    }
                }
            } else {
                work->timer--;
            }
            break;
    }

    if (work->interrupted == 0 && work->captionStep >= 0x13) {
        abort = Gp_StateC08.effectPhase;
        if (abort == 1) {
            prev                    = work->phase;
            work->interrupted       = abort;
            hold                    = work->captionStep;
            work->phase             = 9;
            work->timer             = 0x1E;
            work->captionStep       = 0x13;
            work->resumePhase       = prev;
            work->resumeCaptionStep = hold;
        }
    }
}

/// Per-frame update for the gallery's second course. Same shape as
/// `func_mist_shooting_gallery_801831B0`: a countdown that steps the digit
/// sprite through `captionStep` (gated on `gDisplayState.pendingMode`), a hand-off wait on
/// `gGameSession::location.loc.view`, then the wave loop over
/// `D_mist_shooting_gallery_80186910`. `lethalHit` is the abort request - once
/// it is raised the state machine jumps to the 8 -> 9 shutdown, which releases
/// the `gSceneCombatState` reference and kills the task.
static void func_mist_shooting_gallery_80183E78(Task* arg0)
{
    MistShootingGalleryWork*   work;
    _MistShootingGallerySpawn* spawn;
    u16                        timer;
    u16                        wave;
    u16                        key;
    u8                         step;

    work = arg0->work;

    switch (work->phase) {
        case 0:
            work->timeLeft = 0x1518;
            work->timer    = 0x1E;
            D_80115768     = 1;
            work->phase++;
        case 1:
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    func_mist_shooting_gallery_80184BB8(0x15, 0, 0x8E0);
                    work->timer = 0xF;
                    D_80115768  = 0;
                    work->phase++;
                }
            } else {
                work->timer = (u16)work->timer - 1;
            }
            break;
        case 2:
            if (gGameSession->location.loc.view != 0x12) {
                break;
            }
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    work->timer = 1;
                    work->phase++;
                    func_mist_shooting_gallery_80184BB8(0x15, 7, 0x8E0);
                    work->captionStep = 8;
                }
            } else {
                work->timer = (u16)work->timer - 1;
            }
            break;
        case 3:
            if (work->actionTriggered == 0) {
                break;
            }
            func_mist_shooting_gallery_80184BB8(0x15, work->captionStep, 0x8E0);
            if (work->captionStep == 0xA) {
                work->actionTriggered = 0;
                work->phase++;
            }
            work->captionStep++;
            break;
        case 4:
            if (work->actionTriggered == 0) {
                break;
            }
            func_mist_shooting_gallery_80184BB8(0x15, work->captionStep, 0x8E0);
            if (work->captionStep == 0x10) {
                work->timer = 0x1E;
                work->phase++;
            }
            work->captionStep++;
            break;
        case 5:
            timer       = work->timer - 1;
            work->timer = timer;
            if ((s32)(timer << 16) <= 0) {
                work->timer = 0xF;
                work->phase++;
                taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
                Gp_StateC08.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
                sceneEngageBattle(1);
            }
            break;
        case 6:
            timer       = work->timer - 1;
            work->timer = timer;
            if ((s32)(timer << 16) <= 0) {
                work->phase++;
            }
            break;
        case 7:
            spawn = &D_mist_shooting_gallery_80186910[work->spawnIndex];
            key   = spawn->frame;
            if (key != MIST_SHOOTING_GALLERY_SPAWN_END) {
                if (key != MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR) {
                    if (work->scriptFrame == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->spawnIndex++;
                        } while (work->scriptFrame == spawn->frame);
                    }
                    wave = work->scriptFrame;
                    if (wave < MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT) {
                        work->scriptFrame = wave + 1;
                    }
                } else if (work->liveTargets == 0) {
                    work->spawnIndex++;
                    if (work->spawnIndex >= 0x4E) {
                        work->spawnIndex  = 0xB;
                        work->scriptFrame = 0x2B0;
                    }
                }
            }
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = 0;
                arg0->state++;
            }
            break;
        case 8:
            timer       = work->timer - 1;
            work->timer = timer;
            if ((s32)(timer << 16) <= 0) {
                func_mist_shooting_gallery_80184BB8(0x15, work->captionStep, 0x8E0);
                step = work->captionStep;
                if (step == 0x12) {
                    work->timer = 4;
                    D_80115768  = 0;
                    work->phase++;
                    func_actor_215100_8014A9A0();
                } else {
                    work->captionStep = step + 1;
                }
            }
            break;
        case 9:
            timer       = work->timer - 1;
            work->timer = timer;
            if ((s32)(timer << 16) <= 0) {
                sceneReleaseBattleRefAndClearRewards(arg0, 0);
                taskKill(arg0);
                return;
            }
            break;
    }

    if (work->interrupted == 0 && work->lethalHit != 0) {
        work->phase       = 8;
        work->interrupted = 1;
        work->timer       = 3;
        work->captionStep = 0x11;
        D_80115768        = 1;
    }
}

static void func_mist_shooting_gallery_801842D0(Task* arg0)
{
    MistShootingGalleryWork* work;
    GameActor*               actor;

    work  = arg0->work;
    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    switch (work->phase) {
        case 0:
            if (work->liveTargets != 0) {
                return;
            }
            work->timer = 0x1E;
            work->phase++;
            displayAcquireMenuHold();
        case 1:
            if ((s16)work->timer-- > 0) {
                return;
            }
            work->timer = 0x5A;
            work->phase++;
            taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
            sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
            return;
        case 2:
            if ((s16)--work->timer > 0) {
                return;
            }
            work->phase++;
            actor->movementInputDisabled                        = 0;
            actor->pendingCollisionUpdates                      = 7;
            actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
            sceneReleaseBattleRefAndClearRewards(arg0, 0);
            func_actor_215100_8014A908();
            return;
        case 3:
            if (gGameSession->battleResetPending == 0) {
                work->phase++;
            }
            return;
        case 4:
            if (gGameSession->battleResetPending == 1) {
                displayReleaseMenuHold();
                taskKill(arg0);
            }
            return;
    }
}

s32 func_mist_shooting_gallery_80184470(s32 score)
{
    s32 bonus = 0;

    switch (((MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work)->course) {
        case 0:
            if (score >= 0x2710) {
                bonus = 0x12C;
            } else if (score >= 0x2328) {
                bonus = 0xC8;
            } else if (score >= 0x1F40) {
                bonus = 0x64;
            }
            break;
        case 1:
            if (score >= 0x43F8) {
                bonus = 0x12C;
            } else if (score >= 0x41A0) {
                bonus = 0xC8;
            } else if (score >= 0x3E80) {
                bonus = 0x64;
            }
            break;
        case 2:
            if (score > 0xC34F) {
                bonus = 0x12C;
            } else if (score > 0xB3AF) {
                bonus = 0xC8;
            } else if (score > 0x9857) {
                bonus = 0x64;
            }
            break;
        case 3:
            if (score > 0xEA5F) {
                bonus = 0x12C;
            } else if (score > 0xDABF) {
                bonus = 0xC8;
            } else if (score > 0xCB1F) {
                bonus = 0x64;
            }
            break;
        case 4:
            if (score > 0xD6D7) {
                bonus = 0x12C;
            } else if (score > 0xCF07) {
                bonus = 0xC8;
            } else if (score > 0xC34F) {
                bonus = 0x64;
            }
            break;
    }
    return bonus;
}

/// Slides the course countdown into view and draws its remaining time as MM:SS.
///
/// `timeLeft` is measured at 30 frames per second; subsecond frames are omitted.
/// The courses start at 1800, 3600 or 5400 frames, keeping all digits in 0..9.
/// Screen positions are pixels relative to the display's drawing origin.
static void _mistShootingGalleryDrawCountdownClock(MistShootingGalleryWork* work)
{
    enum {
        MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_SECOND = 30,
        MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_MINUTE = 60 * MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_SECOND,
        MIST_SHOOTING_GALLERY_CLOCK_COLON             = 10,
        MIST_SHOOTING_GALLERY_CLOCK_Y                 = 70,
    };

    s32 minuteTens;
    s32 minuteOnes;
    s32 secondTens;
    s32 secondOnes;
    s32 remainingFrames;

    /// Extracts and draws one decimal place of the remaining frame count.
    ///
    /// Captures work, remainingFrames and the clock Y constant. Pass a digit local,
    /// a positive constant framesPerPlace and a constant pixel offset. The first two
    /// arguments occur repeatedly; the quotient must be in 0..9. Expands to several
    /// statements and must be invoked inside an explicit brace-delimited block.
#define MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(digit, framesPerPlace, xOffset) \
    (digit) = remainingFrames / (framesPerPlace);                              \
    if ((digit) != 0) {                                                        \
        remainingFrames %= (framesPerPlace);                                   \
    }                                                                          \
    _mistShootingGalleryDrawClockGlyph(work->clockX + (xOffset), MIST_SHOOTING_GALLERY_CLOCK_Y, (digit))

    remainingFrames = work->timeLeft;
    if (work->clockX < -0x78) {
        work->clockX += 0xA;
    }

    // Peel off each decimal place before submitting its glyph.
    MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(minuteTens, 10 * MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_MINUTE, 0);
    MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(minuteOnes, MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_MINUTE, 0xC);
    MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(secondTens, 10 * MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_SECOND, 0x24);
    MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(secondOnes, MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_SECOND, 0x30);

    _mistShootingGalleryDrawClockGlyph(work->clockX + 0x18, MIST_SHOOTING_GALLERY_CLOCK_Y, MIST_SHOOTING_GALLERY_CLOCK_COLON);
#undef MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE
}

/// Queues one unmodulated 15 by 19 pixel clock glyph and its texture-page command.
///
/// `glyph` is 0..9 for a digit or 10 for the colon; cells begin every 16 texels.
/// X and Y are pixels relative to the drawing origin and narrow to signed 16 bits.
/// The current primitive buffer must hold an SPRT and a DR_TPAGE. The page command
/// is linked last so it executes before the sprite in the prepend-only table.
static void _mistShootingGalleryDrawClockGlyph(s32 screenX, s16 screenY, s32 glyph)
{
    enum {
        MIST_SHOOTING_GALLERY_CLOCK_GLYPH_STRIDE    = 16,
        MIST_SHOOTING_GALLERY_CLOCK_RAW_SPRITE_CODE = 0x65,
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
    sprite->u0 = glyph * MIST_SHOOTING_GALLERY_CLOCK_GLYPH_STRIDE;
    sprite->v0 = 0;
    setcode(sprite, MIST_SHOOTING_GALLERY_CLOCK_RAW_SPRITE_CODE);
    sprite->x0 = screenX;
    addPrim(gGpuCurrentOt, sprite);

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(0, GPU_BLEND_AVERAGE, 320, 256));
    addPrim(gGpuCurrentOt, drawMode);
}

/// Queues an additive red tile covering the viewport, with intensity in 0..255.
///
/// The 384 by 256 pixel tile overhangs the centred viewport. The current primitive
/// buffer must hold a TILE and a DR_TPAGE; the draw-mode command is linked last
/// so additive blending is selected before the tile executes.
static void _mistShootingGalleryDrawRedFlash(u8 redIntensity)
{
    enum { MIST_SHOOTING_GALLERY_RED_FLASH_TILE_CODE = 0x62 };

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
    setcode(tile, MIST_SHOOTING_GALLERY_RED_FLASH_TILE_CODE);
    addPrim(gGpuCurrentOt, tile);

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(0, GPU_BLEND_ADD, 320, 256));
    addPrim(gGpuCurrentOt, drawMode);
}

void func_mist_shooting_gallery_801848B4(void)
{
    Enemy*     enemy;
    TmdObject* obj;
    GfxCoord*  coord;

    enemy = enemySpawnFromTable(D_actor_107600_80134F94, 0, 0x200D, NULL);
    if (enemy != NULL) {
        obj                    = enemy->task->extra.tmd;
        obj->texturePageOffset = 0;
        obj->clutRowOffset     = 2;
        tmdBuildBufferHalf(obj);
        tmdBuildBufferHalf(obj);
        coord             = enemy->task->extra.tmd->coords;
        coord->coord.t[0] = 0x1770;
        coord->coord.t[2] = 0xBB8;
        coord->coord.t[1] = 0;
        enemy->workType   = ENEMY_WORK_PLAIN;
    }
}

void func_mist_shooting_gallery_80184954(void)
{
    MistShootingGalleryWork* work = D_mist_shooting_gallery_8018E0C4->work;

    work->actionTriggered = 1;
}

s32 func_mist_shooting_gallery_80184970(s32 arg0)
{
    MistShootingGalleryWork* work = D_mist_shooting_gallery_8018E0C4->work;
    s32                      ret  = 0;

    if (work->course < 3) {
        ret = arg0 >= 0xC8;
    } else if (arg0 >= 0x12C) {
        ret = 1;
    }
    return ret;
}

/// The gallery controller task: copies the three-state table
/// `D_mist_shooting_gallery_8017DB80` onto the stack and runs the entry for the
/// task's current state - the setup tick `func_mist_shooting_gallery_80182B1C`,
/// the round runner `func_mist_shooting_gallery_80184A14`, then
/// `func_mist_shooting_gallery_801842D0`.
void func_mist_shooting_gallery_801849BC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_shooting_gallery_8017DB80;
    sp.funcs[task->state](task);
}

static void func_mist_shooting_gallery_80184A14(Task* arg0)
{
    MistShootingGalleryWork* work   = arg0->work;
    TaskFuncTable5           rounds = D_mist_shooting_gallery_8017DB8C;

    rounds.funcs[work->course](arg0);
}

static void func_mist_shooting_gallery_80184A80(Task* arg0)
{
    GameActor* actor;

    actor                                               = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    actor->movementInputDisabled                        = 0;
    actor->pendingCollisionUpdates                      = 7;
    actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
    displayReleaseMenuHold();
    sceneReleaseBattleRefAndClearRewards(arg0, 0);
    taskKill(arg0);
}

/// Decrements the course's 30 Hz clock while combat actors run; returns frames left.
///
/// A zero clock stays zero. Each course tests the result after drawing the clock
/// and enters its closing state on the tick that returns zero.
static u16 _mistShootingGalleryTickCourseClock(MistShootingGalleryWork* work)
{
    u16 framesLeft = work->timeLeft;

    if ((framesLeft != 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING)) {
        work->timeLeft = framesLeft - 1;
    }
    return work->timeLeft;
}

/// Runs a forty-tick red screen flash, drawing and fading on ticks 10..39.
///
/// No work block is allocated. The task owns a frame countdown in `killCountdown`
/// and an 8-bit colour intensity stored in `spawnArg1.value`. Initialization
/// replaces the spawn payload with 255; each visible tick reduces it by 8 when
/// above 8, then draws. The task kills itself before drawing at countdown zero.
static void _mistShootingGalleryRedFlashTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_RED_FLASH_INIT              = 0,
        MIST_SHOOTING_GALLERY_RED_FLASH_FADE              = 1,
        MIST_SHOOTING_GALLERY_RED_FLASH_LIFETIME          = 40,
        MIST_SHOOTING_GALLERY_RED_FLASH_DRAW_BELOW        = 31,
        MIST_SHOOTING_GALLERY_RED_FLASH_INITIAL_INTENSITY = 255,
        MIST_SHOOTING_GALLERY_RED_FLASH_FADE_STEP         = 8,
    };

    s16 framesLeft;

    switch (task->state) {
        case MIST_SHOOTING_GALLERY_RED_FLASH_INIT:
            task->state           = MIST_SHOOTING_GALLERY_RED_FLASH_FADE;
            task->killCountdown   = MIST_SHOOTING_GALLERY_RED_FLASH_LIFETIME;
            task->spawnArg1.value = MIST_SHOOTING_GALLERY_RED_FLASH_INITIAL_INTENSITY;
        case MIST_SHOOTING_GALLERY_RED_FLASH_FADE:
            framesLeft = --task->killCountdown;
            if (framesLeft <= 0) {
                taskKill(task);
                return;
            }
            // Leave the opening ticks clear, then fade the red overlay.
            if (framesLeft < MIST_SHOOTING_GALLERY_RED_FLASH_DRAW_BELOW) {
                if (task->spawnArg1.value >= MIST_SHOOTING_GALLERY_RED_FLASH_FADE_STEP + 1) {
                    task->spawnArg1.value -= MIST_SHOOTING_GALLERY_RED_FLASH_FADE_STEP;
                }
                _mistShootingGalleryDrawRedFlash((u8)task->spawnArg1.value);
            }
            return;
    }
}

static void func_mist_shooting_gallery_80184BB8(s16 arg0, s16 arg1, s16 arg2)
{
    actor215100CapCaptionSelectScript(arg0, arg1, 0xD0);
    displayQueueModeTask(&D_mist_shooting_gallery_801856D0, arg2, 0, STAGE_ENTRY_RELOAD);
}

void func_mist_shooting_gallery_80184C0C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->state         = 1;
            arg0->killCountdown = 0x10;
        case 1:
            if (arg0->killCountdown != 0) {
                arg0->killCountdown--;
            } else {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, arg0->spawnArg1.value) != 0) {
                    arg0->state = arg0->state + 1;
                } else {
                    actor215100CapCaptionDrawCurrent();
                }
                break;
            }
            actor215100CapCaptionDrawCurrent();
            break;
        case 2:
            taskKill(arg0);
            stageRequestModeTaskExit();
            break;
    }
}

static Enemy* func_mist_shooting_gallery_80184CD0(Task* arg0, _MistShootingGallerySpawn* arg1)
{
    MistShootingGalleryWork* work;
    Enemy*                   enemy;
    TmdObject*               obj;
    GfxCoord*                coord;

    work  = arg0->work;
    enemy = enemySpawnFromTable(D_actor_107600_80134F94, 0, arg1->spawnArgLo | (arg1->spawnArgHi << 16), NULL);
    if (enemy != NULL) {
        enemy->task->parent = arg0;
        taskReparent(arg0, enemy->task);
        obj                    = enemy->task->extra.tmd;
        obj->texturePageOffset = 0;
        obj->clutRowOffset     = 2;
        tmdBuildBufferHalf(obj);
        tmdBuildBufferHalf(obj);
        coord             = enemy->task->extra.tmd->coords;
        coord->coord.t[0] = arg1->x;
        coord->coord.t[1] = arg1->y;
        coord->coord.t[2] = arg1->z;
        enemy->workType   = ENEMY_WORK_PLAIN;
        work->liveTargets++;
    }
    return enemy;
}
