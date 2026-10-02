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
#include "gameplay/world_targets.h"

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

/// The five round scripts of the gallery mini-game, indexed by
/// `MistShootingGalleryWork::difficulty`. `func_mist_shooting_gallery_80184A14`
/// copies the whole table onto its stack before dispatching through it, passing
/// on the controller task.
typedef struct MistShootingGalleryRounds {
    /* 0x00 */ TaskFunc rounds[5];
} MistShootingGalleryRounds;
STATIC_ASSERT_SIZEOF(MistShootingGalleryRounds, 0x14);

/// 0xC-byte spawn record for `func_mist_shooting_gallery_80184CD0`. The round
/// scripts index a table of these (reached through
/// `D_mist_shooting_gallery_80186900`) with a 12-byte stride. `idLo` / `idHi`
/// pack into the `Task_SpawnFromTable` arg2 the enemy is spawned with, and
/// `x` / `y` / `z` are written to the spawned object's
/// `GfxCoord::coord.t[0..2]`.
typedef struct MistShootingGallerySpawn {
    /* 0x0 */ u16 field_00;
    /* 0x2 */ s16 idLo;
    /* 0x4 */ s16 idHi;
    /* 0x6 */ s16 x;
    /* 0x8 */ s16 y;
    /* 0xA */ s16 z;
} MistShootingGallerySpawn;
STATIC_ASSERT_SIZEOF(MistShootingGallerySpawn, 0xC);

extern TaskDesc D_mist_shooting_gallery_801856D0;
extern TaskDesc D_80134F94;
/// The wave script the round loop walks: a run of records sharing
/// `field_00` is spawned together, `0xFFF1` waits for the current wave to
/// clear and `0xFFFF` ends the course.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186904;
/// The second course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186910;
/// The first bonus course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_8018690C;
/// The second bonus course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186908;
/// The five course wave scripts as the one array they are: element 0 is the
/// bonus course's script, and elements 1..4 are the same pointers the round
/// scripts above reach by their own addresses (`0x80186904` .. `0x80186910`).
/// The array type is what `func_mist_shooting_gallery_80182C58` needs - an
/// array element counts as a struct reference to GCC 2.8.1's alias analysis,
/// so the load is ordered against the `work->field_04` store that precedes it.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186900[];
/// Main-executable flag gating the countdown steps: while it is set the
/// gallery holds its current step instead of advancing the digit sprite.
/// Bonus-course variant selected before the round starts. It picks both the
/// banner sprite (`variant + 0xB`) and the colour it is drawn in (variant 2
/// uses 2 instead of 0x10).
/// Gameplay-side abort request. While it is 1 the bonus course tears itself
/// down: the state machine remembers where it was in `field_06` / `field_21`
/// and jumps to the state-9 shutdown banner.
extern void   func_8014A908(void);
extern void   func_8014A9A0(void);
extern void   func_8014B0D4(void);
static void   func_mist_shooting_gallery_80184A80(Task* arg0);
static void   func_mist_shooting_gallery_8018458C(MistShootingGalleryWork* work);
static u16    func_mist_shooting_gallery_80184AE0(MistShootingGalleryWork* work);
static void   func_mist_shooting_gallery_80184BB8(s16 arg0, s16 arg1, s16 arg2);
static Enemy* func_mist_shooting_gallery_80184CD0(Task* arg0, MistShootingGallerySpawn* arg1);
void          func_8014B2B8(s16 arg0, s16 arg1, s32 arg2);
static void   func_mist_shooting_gallery_801846F4(s32 arg0, s16 arg1, s32 arg2);
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

/// The five round scripts, indexed by `MistShootingGalleryWork::difficulty`.
static const MistShootingGalleryRounds D_mist_shooting_gallery_8017DB8C = {
    {
        func_mist_shooting_gallery_80182C58,
        func_mist_shooting_gallery_801831B0,
        func_mist_shooting_gallery_8018341C,
        func_mist_shooting_gallery_801838FC,
        func_mist_shooting_gallery_80183E78,
    },
};

void func_mist_shooting_gallery_80184C0C(Task*);

void func_mist_shooting_gallery_801849BC(Task*);
void func_mist_shooting_gallery_80184B10(Task*);

TaskDesc D_mist_shooting_gallery_801856B8[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_801849BC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_80184B10, { .value = 0 } },
};

TaskDesc D_mist_shooting_gallery_801856D0 = { { { TASK_BODY_NONE, 192 } }, func_mist_shooting_gallery_80184C0C, { .value = 0 } };

MistShootingGallerySpawn D_mist_shooting_gallery_801856DC[60] = {
    { 0, 10, 0, 0, 0, 3000 },
    { 0, 10, 0, 0, 0, 4500 },
    { 0, 10, 1792, 0, 0, 1500 },
    { 0, 10, 1792, 1500, 0, 3000 },
    { 0, 10, 1792, 1500, 0, 4500 },
    { 0, 10, 1792, 1500, 0, 1500 },
    { 0, 10, 1792, 3000, 0, 3000 },
    { 0, 10, 2304, 1500, 0, 6000 },
    { 0, 10, 2304, 1500, 0, 4500 },
    { 0, 10, 2304, 1500, 0, 3000 },
    { 0, 10, 2304, 1500, 0, 1500 },
    { 0, 10, 2304, 1500, 0, 0 },
    { 32, 10, 768, 1500, 0, 4500 },
    { 48, 10, 256, 0, 0, 6000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 64, 10, 768, 1500, 0, 1500 },
    { 80, 10, 256, 0, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 96, 10, 768, -1500, 0, 3000 },
    { 120, 10, 768, 0, 0, 4500 },
    { 120, 10, 768, 0, 0, 1500 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 136, 4106, 768, -1400, -3900, 3000 },
    { 160, 10, 768, 1500, 0, 6000 },
    { 160, 10, 768, 1500, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 176, 10, 1024, 0, 0, 6000 },
    { 176, 10, 1024, 0, 0, 0 },
    { 208, 4106, 1280, -1400, -3900, 5800 },
    { 208, 4106, 1280, -1400, -3900, 200 },
    { 264, 10, 768, 1500, 0, 3000 },
    { 264, 4106, 1024, 1600, -3900, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 280, 10, 512, 0, 0, 4500 },
    { 280, 10, 512, 0, 0, 1500 },
    { 360, 10, 768, -1500, 0, 6000 },
    { 360, 10, 768, -1500, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 368, 10, 512, 3000, 0, 6000 },
    { 368, 10, 512, 3000, 0, 0 },
    { 416, 4106, 1024, 1600, -3900, 3000 },
    { 440, 4106, 768, -1400, -3900, 5800 },
    { 440, 4106, 768, -1400, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 448, 10, 1024, 4500, 0, 6000 },
    { 448, 10, 1024, 4500, 0, 0 },
    { 464, 10, 1280, 3000, 0, 4500 },
    { 464, 10, 1280, 3000, 0, 1500 },
    { 496, 10, 1024, 0, 0, 3000 },
    { 536, 4106, 1024, 1600, -3900, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 552, 10, 1024, 1500, 0, 4500 },
    { 568, 10, 256, 1500, 0, 6000 },
    { 616, 10, 1024, 1500, 0, 1500 },
    { 632, 10, 256, 1500, 0, 0 },
    { 696, 4106, 1024, -1400, -3900, 5800 },
    { 696, 4106, 1024, -1400, -3900, 3000 },
    { 696, 4106, 1024, -1400, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 0xFFFF, 0, 0, 0, 0, 0 },
};

MistShootingGallerySpawn D_mist_shooting_gallery_801859AC[66] = {
    { 64, 10, 2, 6000, 0, 4500 },
    { 64, 10, 4, 6000, 0, 1500 },
    { 144, 10, 9, -1500, 0, 6000 },
    { 144, 10, 13, -1500, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 160, 11, 10, -1500, 0, 4500 },
    { 160, 11, 12, -1500, 0, 1500 },
    { 256, 10, 1, 4500, 0, 6000 },
    { 256, 10, 5, 4500, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 272, 10, 10, -1500, 0, 4500 },
    { 272, 10, 12, -1500, 0, 1500 },
    { 320, 12, 3, 6000, 0, 3000 },
    { 416, 4106, 24, 1600, -3900, 5800 },
    { 416, 4107, 34, -1400, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 432, 12, 10, -1500, 0, 4500 },
    { 432, 12, 12, -1500, 0, 1500 },
    { 464, 4106, 25, -1400, -3900, 5800 },
    { 464, 4106, 33, 1600, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 480, 10, 37, -1500, 0, 6000 },
    { 480, 10, 38, -1500, 0, 0 },
    { 496, 12, 3, 7500, 0, 3000 },
    { 544, 12, 3, 7500, 0, 3000 },
    { 656, 11, 22, -1500, 0, 6000 },
    { 656, 11, 30, 0, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 672, 10, 19, 3000, 0, 6000 },
    { 704, 11, 21, 0, 0, 6000 },
    { 704, 10, 29, 1500, 0, 0 },
    { 736, 11, 31, -1500, 0, 0 },
    { 832, 4106, 6, 4600, -3900, 5800 },
    { 832, 4106, 8, 4600, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 848, 10, 52, 3000, 0, 6000 },
    { 864, 10, 54, 0, 0, 6000 },
    { 896, 10, 59, 1500, 0, 0 },
    { 912, 10, 61, -1500, 0, 0 },
    { 976, 4106, 25, -1400, -3900, 5800 },
    { 976, 4106, 33, 1600, -3900, 200 },
    { 1088, 10, 58, 3000, 0, 0 },
    { 1104, 10, 60, 0, 0, 0 },
    { 1120, 10, 53, 1500, 0, 6000 },
    { 1136, 10, 55, -1500, 0, 6000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1152, 12, 35, -1500, 0, 6000 },
    { 1200, 12, 36, -1500, 0, 0 },
    { 1248, 11, 35, -1500, 0, 6000 },
    { 1296, 11, 36, -1500, 0, 0 },
    { 1360, 4108, 7, 4600, -3900, 3000 },
    { 1408, 4106, 6, 4600, -3900, 5800 },
    { 1408, 4106, 8, 4600, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1424, 12, 40, 0x2EE0, 0, 6000 },
    { 1424, 12, 41, 0x2EE0, 0, 0 },
    { 1472, 4106, 6, 4600, -3900, 5800 },
    { 1472, 4106, 8, 4600, -3900, 200 },
    { 1488, 10, 40, 0x2EE0, 0, 6000 },
    { 1488, 11, 41, 0x2EE0, 0, 0 },
    { 1696, 11, 10, -1500, 0, 4500 },
    { 1696, 10, 12, -1500, 0, 1500 },
    { 1792, 4106, 24, 1600, -3900, 5800 },
    { 1792, 4106, 34, -1400, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 0xFFFF, 0, 0, 0, 0, 0 },
};

MistShootingGallerySpawn D_mist_shooting_gallery_80185CC4[75] = {
    { 0, 2, 0x4000, 7500, 0, 4500 },
    { 0, 3, 0x4000, 7500, 0, 1500 },
    { 0, 1, 0x4000, 0x2EE0, 0, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 48, 3, 4098, 6000, 0, 4500 },
    { 48, 3, 4100, 6000, 0, 1500 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 80, 3, 4097, 4500, 0, 6000 },
    { 80, 1, 4101, 4500, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 112, 3, 4115, 3000, 0, 6000 },
    { 160, 3, 4117, 0, 0, 6000 },
    { 208, 3, 4125, 1500, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 240, 3, 4098, 6000, 0, 4500 },
    { 240, 3, 4100, 6000, 0, 1500 },
    { 304, 0, 4099, 6000, 0, 3000 },
    { 416, 4104, 4111, -1400, -3900, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 448, 5, 4136, 0x2EE0, 0, 6000 },
    { 448, 5, 4137, 0x2EE0, 0, 0 },
    { 512, 4104, 4111, -1400, -3900, 3000 },
    { 544, 1, 4107, -1500, 0, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 576, 4104, 4102, 4600, -3900, 5800 },
    { 576, 4104, 4104, 4600, -3900, 200 },
    { 608, 4103, 4103, 4600, -3900, 3000 },
    { 672, 9, 4099, 6000, 0, 3000 },
    { 752, 2, 4115, 3000, 0, 6000 },
    { 752, 2, 4126, 0, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 784, 1, 4138, 0x2EE0, 0, 6000 },
    { 832, 9, 4138, 0x2EE0, 0, 6000 },
    { 864, 2, 4106, -1500, 0, 4500 },
    { 864, 2, 4107, -1500, 0, 3000 },
    { 864, 2, 4108, -1500, 0, 1500 },
    { 944, 4103, 4121, -1400, -3900, 5800 },
    { 976, 4102, 4128, 4600, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1008, 3, 4123, 4500, 0, 0 },
    { 1008, 9, 4125, 1500, 0, 0 },
    { 1024, 4, 4124, 3000, 0, 0 },
    { 1120, 3, 4117, 0, 0, 6000 },
    { 1136, 3, 4118, -1500, 0, 6000 },
    { 1232, 4, 4139, 6000, 0, 4500 },
    { 1232, 5, 4140, 6000, 0, 1500 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1264, 5, 4098, 6000, 0, 4500 },
    { 1264, 5, 4100, 6000, 0, 1500 },
    { 1328, 4103, 4103, 4600, -3900, 3000 },
    { 1408, 0, 37, -1500, 0, 6000 },
    { 1408, 0, 38, -1500, 0, 0 },
    { 1472, 3, 4138, 0x2EE0, 0, 6000 },
    { 1520, 3, 4138, 0x2EE0, 0, 6000 },
    { 1520, 4103, 4121, -1400, -3900, 5800 },
    { 1520, 4102, 4128, 4600, -3900, 200 },
    { 1568, 9, 4138, 0x2EE0, 0, 6000 },
    { 1616, 3, 4138, 0x2EE0, 0, 6000 },
    { 1664, 3, 4138, 0x2EE0, 0, 6000 },
    { 1664, 1, 4127, -1500, 0, 0 },
    { 1728, 3, 4115, 3000, 0, 6000 },
    { 1744, 5, 4125, 1500, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1776, 9, 4136, 0x2EE0, 0, 6000 },
    { 1776, 9, 4137, 0x2EE0, 0, 0 },
    { 1824, 4, 4136, 0x2EE0, 0, 6000 },
    { 1824, 4, 4137, 0x2EE0, 0, 0 },
    { 1872, 2, 4136, 0x2EE0, 0, 6000 },
    { 1872, 2, 4137, 0x2EE0, 0, 0 },
    { 1920, 1, 4136, 0x2EE0, 0, 6000 },
    { 1920, 1, 4137, 0x2EE0, 0, 0 },
    { 1968, 9, 4136, 0x2EE0, 0, 6000 },
    { 1968, 9, 4137, 0x2EE0, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 0xFFFF, 0, 0, 0, 0, 0 },
};

MistShootingGallerySpawn D_mist_shooting_gallery_80186048[106] = {
    { 0, 5, 0, 7500, 0, 4500 },
    { 0, 5, 0, 7500, 0, 1500 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 32, 0, 4099, 6000, 0, 3000 },
    { 64, 2, 4136, 0x2EE0, 0, 6000 },
    { 64, 2, 4137, 0x2EE0, 0, 0 },
    { 128, 3, 4097, 4500, 0, 6000 },
    { 128, 3, 4101, 4500, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 144, 3, 4114, 4500, 0, 6000 },
    { 160, 0, 4116, 1500, 0, 6000 },
    { 176, 3, 4118, -1500, 0, 6000 },
    { 224, 9, 4124, 3000, 0, 0 },
    { 240, 1, 4126, 0, 0, 0 },
    { 320, 4104, 4111, -1400, -3900, 3000 },
    { 336, 4102, 4110, -1400, -3900, 5800 },
    { 336, 4102, 4112, -1400, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 352, 0, 4099, 6000, 0, 3000 },
    { 368, 0, 4098, 6000, 0, 4500 },
    { 368, 0, 4100, 6000, 0, 1500 },
    { 432, 0, 4136, 0x2EE0, 0, 6000 },
    { 432, 0, 4137, 0x2EE0, 0, 0 },
    { 512, 1, 4138, 0x2EE0, 0, 6000 },
    { 544, 9, 4608, 3000, 0, 4500 },
    { 544, 9, 4608, 3000, 0, 1500 },
    { 560, 1, 4138, 0x2EE0, 0, 6000 },
    { 640, 4103, 4102, 4600, -3900, 5800 },
    { 640, 4103, 4104, 4600, -3900, 200 },
    { 688, 4, 4107, -1500, 0, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 704, 9, 4123, 4500, 0, 0 },
    { 704, 2, 4125, 1500, 0, 0 },
    { 704, 3, 4127, -1500, 0, 0 },
    { 736, 2, 4115, 3000, 0, 6000 },
    { 736, 9, 4117, 0, 0, 6000 },
    { 752, 5, 4136, 0x2EE0, 0, 6000 },
    { 752, 5, 4137, 0x2EE0, 0, 0 },
    { 800, 3, 4136, 0x2EE0, 0, 6000 },
    { 800, 3, 4137, 0x2EE0, 0, 0 },
    { 848, 3, 4136, 0x2EE0, 0, 6000 },
    { 848, 3, 4137, 0x2EE0, 0, 0 },
    { 880, 9, 4608, 1500, 0, 3000 },
    { 912, 9, 4608, 0, 0, 3000 },
    { 912, 4, 5376, 0x2EE0, 0, 3000 },
    { 1024, 4104, 4119, 4600, -3900, 5800 },
    { 1024, 4104, 4121, -1400, -3900, 5800 },
    { 1040, 4103, 4129, 1600, -3900, 200 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1056, 9, 4135, -1500, 0, 6000 },
    { 1104, 3, 4135, -1500, 0, 6000 },
    { 1152, 5, 4135, -1500, 0, 6000 },
    { 1200, 5, 4135, -1500, 0, 6000 },
    { 1248, 5, 4135, -1500, 0, 6000 },
    { 1296, 1, 4135, -1500, 0, 6000 },
    { 1296, 9, 6400, 6000, 0, 4500 },
    { 1296, 9, 6400, 6000, 0, 1500 },
    { 1312, 3, 4138, 0x2EE0, 0, 6000 },
    { 1360, 3, 4138, 0x2EE0, 0, 6000 },
    { 1408, 3, 4138, 0x2EE0, 0, 6000 },
    { 1456, 3, 4138, 0x2EE0, 0, 6000 },
    { 1504, 3, 4138, 0x2EE0, 0, 6000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1520, 9, 4141, -3000, 0, 6000 },
    { 1552, 4, 4141, -3000, 0, 6000 },
    { 1584, 2, 4141, -3000, 0, 6000 },
    { 1616, 1, 4141, -3000, 0, 6000 },
    { 1648, 1, 4141, -3000, 0, 6000 },
    { 1680, 9, 4141, -3000, 0, 6000 },
    { 1712, 3, 4141, -3000, 0, 6000 },
    { 1744, 3, 4141, -3000, 0, 6000 },
    { 1776, 3, 4141, -3000, 0, 6000 },
    { 1808, 4, 4141, -3000, 0, 6000 },
    { 1840, 4, 4141, -3000, 0, 6000 },
    { 1872, 9, 4141, -3000, 0, 6000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1888, 0, 4136, 0x2EE0, 0, 6000 },
    { 1888, 0, 4137, 0x2EE0, 0, 0 },
    { 1936, 4, 4136, 0x2EE0, 0, 6000 },
    { 1936, 4, 4137, 0x2EE0, 0, 0 },
    { 1952, 4104, 4128, 4600, -3900, 200 },
    { 1984, 5, 4136, 0x2EE0, 0, 6000 },
    { 1984, 5, 4137, 0x2EE0, 0, 0 },
    { 2000, 4104, 4129, 1600, -3900, 200 },
    { 2048, 4103, 4130, -1400, -3900, 200 },
    { 2128, 1, 4106, -1500, 0, 4500 },
    { 2128, 1, 4108, -1500, 0, 1500 },
    { 2144, 4, 6144, 0x2EE0, 0, 3000 },
    { 2176, 1, 4106, -1500, 0, 4500 },
    { 2176, 1, 4108, -1500, 0, 1500 },
    { 2224, 2, 4106, -1500, 0, 4500 },
    { 2224, 2, 4108, -1500, 0, 1500 },
    { 2416, 4102, 4119, 4600, -3900, 5800 },
    { 2464, 4103, 4120, 1600, -3900, 5800 },
    { 2512, 4104, 4121, -1400, -3900, 5800 },
    { 2592, 3, 4114, 4500, 0, 6000 },
    { 2592, 1, 4124, 3000, 0, 0 },
    { 2640, 9, 4116, 1500, 0, 6000 },
    { 2640, 1, 4126, 0, 0, 0 },
    { 2688, 3, 4118, -1500, 0, 6000 },
    { 2784, 9, 4608, 4500, 0, 4500 },
    { 2784, 9, 4608, 4500, 0, 1500 },
    { 2832, 4, 4139, 6000, 0, 4500 },
    { 2832, 4, 4140, 6000, 0, 1500 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 0xFFFF, 0, 0, 0, 0, 0 },
};

MistShootingGallerySpawn D_mist_shooting_gallery_80186540[80] = {
    { 128, 3, 0x3028, 0x2EE0, 0, 6000 },
    { 192, 3, 0x3029, 0x2EE0, 0, 0 },
    { 272, 0, 0x3003, 6000, 0, 3000 },
    { 352, 1, 0x3028, 0x2EE0, 0, 6000 },
    { 416, 1, 0x3029, 0x2EE0, 0, 0 },
    { 496, 0, 0x3003, 7500, 0, 3000 },
    { 576, 3, 0x300A, -3000, 0, 4500 },
    { 576, 5, 0x300C, -3000, 0, 1500 },
    { 672, 5, 0x300A, -3000, 0, 4500 },
    { 672, 0, 0x300C, -3000, 0, 1500 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 688, 5, 0x3900, 4500, 0, 6000 },
    { 704, 9, 5888, 3000, 0, 4500 },
    { 720, 4, 0x3900, 1500, 0, 3000 },
    { 736, 3, 5888, 0, 0, 1500 },
    { 752, 5, 0x3900, -1500, 0, 0 },
    { 896, 2, 0x3028, 0x2EE0, 0, 6000 },
    { 896, 0, 0x3029, 0x2EE0, 0, 0 },
    { 960, 4103, 0x3017, 4600, -3900, 5800 },
    { 976, 4103, 0x3018, 1600, -3900, 5800 },
    { 992, 4104, 0x3019, -1400, -3900, 5800 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1008, 0, 0x3025, -1500, 0, 6000 },
    { 1008, 2, 0x3026, -1500, 0, 0 },
    { 1040, 1, 4106, -1500, 0, 4500 },
    { 1040, 1, 4108, -1500, 0, 1500 },
    { 1104, 3, 0x3003, 7500, 0, 3000 },
    { 1168, 3, 0x3003, 7500, 0, 3000 },
    { 1168, 0, 0x3025, -1500, 0, 6000 },
    { 1168, 2, 0x3026, -1500, 0, 0 },
    { 1200, 9, 4106, -1500, 0, 4500 },
    { 1200, 9, 4108, -1500, 0, 1500 },
    { 1264, 3, 0x3003, 7500, 0, 3000 },
    { 1328, 3, 0x3003, 7500, 0, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1344, 1, 0x3024, -1500, 0, 0 },
    { 1408, 5, 0x3023, -1500, 0, 6000 },
    { 1472, 1, 0x3024, -1500, 0, 0 },
    { 1536, 5, 0x3023, -1500, 0, 6000 },
    { 1536, 4, 0x3F00, 7500, 0, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1552, 3, 0x302E, -3000, 0, 4500 },
    { 1600, 0, 0x302E, -3000, 0, 4500 },
    { 1648, 9, 4142, -3000, 0, 4500 },
    { 1648, 5, 0x302F, -3000, 0, 6000 },
    { 1648, 3, 0x3030, -3000, 0, 0 },
    { 1696, 0, 0x302E, -3000, 0, 4500 },
    { 1888, 4, 0x302F, -3000, 0, 6000 },
    { 1888, 1, 0x3030, -3000, 0, 0 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 1904, 4104, 4110, -1400, -3900, 5800 },
    { 1904, 4104, 4112, -1400, -3900, 200 },
    { 1936, 2, 0x3031, 0x2EE0, 0, 6000 },
    { 1984, 3, 0x302F, -3000, 0, 6000 },
    { 1984, 3, 0x3030, -3000, 0, 0 },
    { 2000, 4103, 4111, -1400, -3900, 3000 },
    { 2080, 4104, 4110, -1400, -3900, 5800 },
    { 2080, 4104, 4112, -1400, -3900, 200 },
    { 2112, 1, 4106, -3000, 0, 4500 },
    { 2112, 3, 4108, -3000, 0, 1500 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 2144, 9, 4099, 7500, 0, 3000 },
    { 2208, 1, 0x302F, -3000, 0, 6000 },
    { 2208, 1, 0x3030, -3000, 0, 0 },
    { 2208, 2, 0x3003, 7500, 0, 3000 },
    { 2208, 2, 0x3031, 0x2EE0, 0, 6000 },
    { 2272, 2, 0x3003, 7500, 0, 3000 },
    { 2336, 2, 0x3003, 7500, 0, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 2352, 3, 0x3013, 3000, 0, 6000 },
    { 2352, 3, 0x3015, 0, 0, 6000 },
    { 2352, 4, 0x301D, 1500, 0, 0 },
    { 2384, 0, 0x3002, 7500, 0, 4500 },
    { 2384, 3, 0x3003, 7500, 0, 3000 },
    { 2464, 2, 0x3003, 7500, 0, 3000 },
    { 2464, 1, 0x3004, 7500, 0, 1500 },
    { 2544, 4, 0x3002, 7500, 0, 4500 },
    { 2544, 9, 4099, 7500, 0, 3000 },
    { 0xFFF1, 0, 0, 0, 0, 0 },
    { 0xFFFF, 0, 0, 0, 0, 0 },
};

MistShootingGallerySpawn* D_mist_shooting_gallery_80186900[1] = {
    D_mist_shooting_gallery_801856DC,
};

MistShootingGallerySpawn* D_mist_shooting_gallery_80186904 = D_mist_shooting_gallery_801859AC;

MistShootingGallerySpawn* D_mist_shooting_gallery_80186908 = D_mist_shooting_gallery_80185CC4;

MistShootingGallerySpawn* D_mist_shooting_gallery_8018690C = D_mist_shooting_gallery_80186048;

MistShootingGallerySpawn* D_mist_shooting_gallery_80186910 = D_mist_shooting_gallery_80186540;

TmdBone D_mist_shooting_gallery_80186914[1] = {
#include "assets/mist_shooting_gallery_model_093FC_skeleton.inc"
};

u32 D_mist_shooting_gallery_80186938[1] = {
#include "assets/mist_shooting_gallery_model_093FC_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_8018693C[10] = {
#include "assets/mist_shooting_gallery_model_093FC_verts.inc"
};

SVECTOR D_mist_shooting_gallery_8018698C[6] = {
#include "assets/mist_shooting_gallery_model_093FC_normals.inc"
};

u32 D_mist_shooting_gallery_801869BC[73] = {
#include "assets/mist_shooting_gallery_model_093FC_stream.inc"
};

TmdSource D_mist_shooting_gallery_80186AE0 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_80186938,
    D_mist_shooting_gallery_8018693C,
    D_mist_shooting_gallery_8018698C,
    D_mist_shooting_gallery_80186914,
    D_mist_shooting_gallery_801869BC,
};

TmdBone D_mist_shooting_gallery_80186B04[1] = {
#include "assets/mist_shooting_gallery_model_095EC_skeleton.inc"
};

u32 D_mist_shooting_gallery_80186B28[1] = {
#include "assets/mist_shooting_gallery_model_095EC_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80186B2C[10] = {
#include "assets/mist_shooting_gallery_model_095EC_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80186B7C[6] = {
#include "assets/mist_shooting_gallery_model_095EC_normals.inc"
};

u32 D_mist_shooting_gallery_80186BAC[73] = {
#include "assets/mist_shooting_gallery_model_095EC_stream.inc"
};

TmdSource D_mist_shooting_gallery_80186CD0 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_80186B28,
    D_mist_shooting_gallery_80186B2C,
    D_mist_shooting_gallery_80186B7C,
    D_mist_shooting_gallery_80186B04,
    D_mist_shooting_gallery_80186BAC,
};

TmdBone D_mist_shooting_gallery_80186CF4[1] = {
#include "assets/mist_shooting_gallery_model_097DC_skeleton.inc"
};

u32 D_mist_shooting_gallery_80186D18[1] = {
#include "assets/mist_shooting_gallery_model_097DC_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80186D1C[10] = {
#include "assets/mist_shooting_gallery_model_097DC_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80186D6C[6] = {
#include "assets/mist_shooting_gallery_model_097DC_normals.inc"
};

u32 D_mist_shooting_gallery_80186D9C[73] = {
#include "assets/mist_shooting_gallery_model_097DC_stream.inc"
};

TmdSource D_mist_shooting_gallery_80186EC0 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_80186D18,
    D_mist_shooting_gallery_80186D1C,
    D_mist_shooting_gallery_80186D6C,
    D_mist_shooting_gallery_80186CF4,
    D_mist_shooting_gallery_80186D9C,
};

TmdBone D_mist_shooting_gallery_80186EE4[1] = {
#include "assets/mist_shooting_gallery_model_099CC_skeleton.inc"
};

u32 D_mist_shooting_gallery_80186F08[1] = {
#include "assets/mist_shooting_gallery_model_099CC_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80186F0C[10] = {
#include "assets/mist_shooting_gallery_model_099CC_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80186F5C[6] = {
#include "assets/mist_shooting_gallery_model_099CC_normals.inc"
};

u32 D_mist_shooting_gallery_80186F8C[73] = {
#include "assets/mist_shooting_gallery_model_099CC_stream.inc"
};

TmdSource D_mist_shooting_gallery_801870B0 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_80186F08,
    D_mist_shooting_gallery_80186F0C,
    D_mist_shooting_gallery_80186F5C,
    D_mist_shooting_gallery_80186EE4,
    D_mist_shooting_gallery_80186F8C,
};

TmdBone D_mist_shooting_gallery_801870D4[1] = {
#include "assets/mist_shooting_gallery_model_09BBC_skeleton.inc"
};

u32 D_mist_shooting_gallery_801870F8[1] = {
#include "assets/mist_shooting_gallery_model_09BBC_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_801870FC[10] = {
#include "assets/mist_shooting_gallery_model_09BBC_verts.inc"
};

SVECTOR D_mist_shooting_gallery_8018714C[6] = {
#include "assets/mist_shooting_gallery_model_09BBC_normals.inc"
};

u32 D_mist_shooting_gallery_8018717C[73] = {
#include "assets/mist_shooting_gallery_model_09BBC_stream.inc"
};

TmdSource D_mist_shooting_gallery_801872A0 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_801870F8,
    D_mist_shooting_gallery_801870FC,
    D_mist_shooting_gallery_8018714C,
    D_mist_shooting_gallery_801870D4,
    D_mist_shooting_gallery_8018717C,
};

TmdBone D_mist_shooting_gallery_801872C4[1] = {
#include "assets/mist_shooting_gallery_model_09DAC_skeleton.inc"
};

u32 D_mist_shooting_gallery_801872E8[1] = {
#include "assets/mist_shooting_gallery_model_09DAC_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_801872EC[10] = {
#include "assets/mist_shooting_gallery_model_09DAC_verts.inc"
};

SVECTOR D_mist_shooting_gallery_8018733C[6] = {
#include "assets/mist_shooting_gallery_model_09DAC_normals.inc"
};

u32 D_mist_shooting_gallery_8018736C[73] = {
#include "assets/mist_shooting_gallery_model_09DAC_stream.inc"
};

TmdSource D_mist_shooting_gallery_80187490 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_801872E8,
    D_mist_shooting_gallery_801872EC,
    D_mist_shooting_gallery_8018733C,
    D_mist_shooting_gallery_801872C4,
    D_mist_shooting_gallery_8018736C,
};

TmdBone D_mist_shooting_gallery_801874B4[1] = {
#include "assets/mist_shooting_gallery_model_09F9C_skeleton.inc"
};

u32 D_mist_shooting_gallery_801874D8[1] = {
#include "assets/mist_shooting_gallery_model_09F9C_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_801874DC[10] = {
#include "assets/mist_shooting_gallery_model_09F9C_verts.inc"
};

SVECTOR D_mist_shooting_gallery_8018752C[6] = {
#include "assets/mist_shooting_gallery_model_09F9C_normals.inc"
};

u32 D_mist_shooting_gallery_8018755C[73] = {
#include "assets/mist_shooting_gallery_model_09F9C_stream.inc"
};

TmdSource D_mist_shooting_gallery_80187680 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_801874D8,
    D_mist_shooting_gallery_801874DC,
    D_mist_shooting_gallery_8018752C,
    D_mist_shooting_gallery_801874B4,
    D_mist_shooting_gallery_8018755C,
};

TmdBone D_mist_shooting_gallery_801876A4[1] = {
#include "assets/mist_shooting_gallery_model_0A18C_skeleton.inc"
};

u32 D_mist_shooting_gallery_801876C8[1] = {
#include "assets/mist_shooting_gallery_model_0A18C_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_801876CC[10] = {
#include "assets/mist_shooting_gallery_model_0A18C_verts.inc"
};

SVECTOR D_mist_shooting_gallery_8018771C[6] = {
#include "assets/mist_shooting_gallery_model_0A18C_normals.inc"
};

u32 D_mist_shooting_gallery_8018774C[73] = {
#include "assets/mist_shooting_gallery_model_0A18C_stream.inc"
};

TmdSource D_mist_shooting_gallery_80187870 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_801876C8,
    D_mist_shooting_gallery_801876CC,
    D_mist_shooting_gallery_8018771C,
    D_mist_shooting_gallery_801876A4,
    D_mist_shooting_gallery_8018774C,
};

TmdBone D_mist_shooting_gallery_80187894[1] = {
#include "assets/mist_shooting_gallery_model_0A37C_skeleton.inc"
};

u32 D_mist_shooting_gallery_801878B8[1] = {
#include "assets/mist_shooting_gallery_model_0A37C_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_801878BC[10] = {
#include "assets/mist_shooting_gallery_model_0A37C_verts.inc"
};

SVECTOR D_mist_shooting_gallery_8018790C[6] = {
#include "assets/mist_shooting_gallery_model_0A37C_normals.inc"
};

u32 D_mist_shooting_gallery_8018793C[73] = {
#include "assets/mist_shooting_gallery_model_0A37C_stream.inc"
};

TmdSource D_mist_shooting_gallery_80187A60 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_801878B8,
    D_mist_shooting_gallery_801878BC,
    D_mist_shooting_gallery_8018790C,
    D_mist_shooting_gallery_80187894,
    D_mist_shooting_gallery_8018793C,
};

TmdBone D_mist_shooting_gallery_80187A84[1] = {
#include "assets/mist_shooting_gallery_model_0A56C_skeleton.inc"
};

u32 D_mist_shooting_gallery_80187AA8[1] = {
#include "assets/mist_shooting_gallery_model_0A56C_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80187AAC[10] = {
#include "assets/mist_shooting_gallery_model_0A56C_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80187AFC[6] = {
#include "assets/mist_shooting_gallery_model_0A56C_normals.inc"
};

u32 D_mist_shooting_gallery_80187B2C[73] = {
#include "assets/mist_shooting_gallery_model_0A56C_stream.inc"
};

TmdSource D_mist_shooting_gallery_80187C50 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_80187AA8,
    D_mist_shooting_gallery_80187AAC,
    D_mist_shooting_gallery_80187AFC,
    D_mist_shooting_gallery_80187A84,
    D_mist_shooting_gallery_80187B2C,
};

TmdBone D_mist_shooting_gallery_80187C74[1] = {
#include "assets/mist_shooting_gallery_model_0A81C_skeleton.inc"
};

u32 D_mist_shooting_gallery_80187C98[1] = {
#include "assets/mist_shooting_gallery_model_0A81C_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80187C9C[24] = {
#include "assets/mist_shooting_gallery_model_0A81C_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80187D5C[16] = {
#include "assets/mist_shooting_gallery_model_0A81C_normals.inc"
};

u32 D_mist_shooting_gallery_80187DDC[150] = {
#include "assets/mist_shooting_gallery_model_0A81C_stream.inc"
};

TmdSource D_mist_shooting_gallery_80188034 = {
    0,
    1056,
    0,
    1,
    D_mist_shooting_gallery_80187C98,
    D_mist_shooting_gallery_80187C9C,
    D_mist_shooting_gallery_80187D5C,
    D_mist_shooting_gallery_80187C74,
    D_mist_shooting_gallery_80187DDC,
};

TmdBone D_mist_shooting_gallery_80188058[1] = {
#include "assets/mist_shooting_gallery_model_0AB30_skeleton.inc"
};

u32 D_mist_shooting_gallery_8018807C[1] = {
#include "assets/mist_shooting_gallery_model_0AB30_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80188080[8] = {
#include "assets/mist_shooting_gallery_model_0AB30_verts.inc"
};

SVECTOR D_mist_shooting_gallery_801880C0[6] = {
#include "assets/mist_shooting_gallery_model_0AB30_normals.inc"
};

u32 D_mist_shooting_gallery_801880F0[42] = {
#include "assets/mist_shooting_gallery_model_0AB30_stream.inc"
};

TmdSource D_mist_shooting_gallery_80188198 = {
    0,
    312,
    0,
    1,
    D_mist_shooting_gallery_8018807C,
    D_mist_shooting_gallery_80188080,
    D_mist_shooting_gallery_801880C0,
    D_mist_shooting_gallery_80188058,
    D_mist_shooting_gallery_801880F0,
};

TmdBone D_mist_shooting_gallery_801881BC[1] = {
#include "assets/mist_shooting_gallery_model_0AC94_skeleton.inc"
};

u32 D_mist_shooting_gallery_801881E0[1] = {
#include "assets/mist_shooting_gallery_model_0AC94_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_801881E4[8] = {
#include "assets/mist_shooting_gallery_model_0AC94_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80188224[6] = {
#include "assets/mist_shooting_gallery_model_0AC94_normals.inc"
};

u32 D_mist_shooting_gallery_80188254[42] = {
#include "assets/mist_shooting_gallery_model_0AC94_stream.inc"
};

TmdSource D_mist_shooting_gallery_801882FC = {
    0,
    312,
    0,
    1,
    D_mist_shooting_gallery_801881E0,
    D_mist_shooting_gallery_801881E4,
    D_mist_shooting_gallery_80188224,
    D_mist_shooting_gallery_801881BC,
    D_mist_shooting_gallery_80188254,
};

TmdBone D_mist_shooting_gallery_80188320[1] = {
#include "assets/mist_shooting_gallery_model_0ADF8_skeleton.inc"
};

u32 D_mist_shooting_gallery_80188344[1] = {
#include "assets/mist_shooting_gallery_model_0ADF8_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80188348[8] = {
#include "assets/mist_shooting_gallery_model_0ADF8_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80188388[6] = {
#include "assets/mist_shooting_gallery_model_0ADF8_normals.inc"
};

u32 D_mist_shooting_gallery_801883B8[42] = {
#include "assets/mist_shooting_gallery_model_0ADF8_stream.inc"
};

TmdSource D_mist_shooting_gallery_80188460 = {
    0,
    312,
    0,
    1,
    D_mist_shooting_gallery_80188344,
    D_mist_shooting_gallery_80188348,
    D_mist_shooting_gallery_80188388,
    D_mist_shooting_gallery_80188320,
    D_mist_shooting_gallery_801883B8,
};

TmdBone D_mist_shooting_gallery_80188484[1] = {
#include "assets/mist_shooting_gallery_model_0AF5C_skeleton.inc"
};

u32 D_mist_shooting_gallery_801884A8[1] = {
#include "assets/mist_shooting_gallery_model_0AF5C_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_801884AC[8] = {
#include "assets/mist_shooting_gallery_model_0AF5C_verts.inc"
};

SVECTOR D_mist_shooting_gallery_801884EC[6] = {
#include "assets/mist_shooting_gallery_model_0AF5C_normals.inc"
};

u32 D_mist_shooting_gallery_8018851C[42] = {
#include "assets/mist_shooting_gallery_model_0AF5C_stream.inc"
};

TmdSource D_mist_shooting_gallery_801885C4 = {
    0,
    312,
    0,
    1,
    D_mist_shooting_gallery_801884A8,
    D_mist_shooting_gallery_801884AC,
    D_mist_shooting_gallery_801884EC,
    D_mist_shooting_gallery_80188484,
    D_mist_shooting_gallery_8018851C,
};

TmdBone D_mist_shooting_gallery_801885E8[1] = {
#include "assets/mist_shooting_gallery_model_0B0D0_skeleton.inc"
};

u32 D_mist_shooting_gallery_8018860C[1] = {
#include "assets/mist_shooting_gallery_model_0B0D0_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80188610[10] = {
#include "assets/mist_shooting_gallery_model_0B0D0_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80188660[6] = {
#include "assets/mist_shooting_gallery_model_0B0D0_normals.inc"
};

u32 D_mist_shooting_gallery_80188690[73] = {
#include "assets/mist_shooting_gallery_model_0B0D0_stream.inc"
};

TmdSource D_mist_shooting_gallery_801887B4 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_8018860C,
    D_mist_shooting_gallery_80188610,
    D_mist_shooting_gallery_80188660,
    D_mist_shooting_gallery_801885E8,
    D_mist_shooting_gallery_80188690,
};

TmdBone D_mist_shooting_gallery_801887D8[1] = {
#include "assets/mist_shooting_gallery_model_0B2C0_skeleton.inc"
};

u32 D_mist_shooting_gallery_801887FC[1] = {
#include "assets/mist_shooting_gallery_model_0B2C0_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80188800[10] = {
#include "assets/mist_shooting_gallery_model_0B2C0_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80188850[6] = {
#include "assets/mist_shooting_gallery_model_0B2C0_normals.inc"
};

u32 D_mist_shooting_gallery_80188880[73] = {
#include "assets/mist_shooting_gallery_model_0B2C0_stream.inc"
};

TmdSource D_mist_shooting_gallery_801889A4 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_801887FC,
    D_mist_shooting_gallery_80188800,
    D_mist_shooting_gallery_80188850,
    D_mist_shooting_gallery_801887D8,
    D_mist_shooting_gallery_80188880,
};

TmdBone D_mist_shooting_gallery_801889C8[1] = {
#include "assets/mist_shooting_gallery_model_0B4B0_skeleton.inc"
};

u32 D_mist_shooting_gallery_801889EC[1] = {
#include "assets/mist_shooting_gallery_model_0B4B0_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_801889F0[10] = {
#include "assets/mist_shooting_gallery_model_0B4B0_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80188A40[6] = {
#include "assets/mist_shooting_gallery_model_0B4B0_normals.inc"
};

u32 D_mist_shooting_gallery_80188A70[73] = {
#include "assets/mist_shooting_gallery_model_0B4B0_stream.inc"
};

TmdSource D_mist_shooting_gallery_80188B94 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_801889EC,
    D_mist_shooting_gallery_801889F0,
    D_mist_shooting_gallery_80188A40,
    D_mist_shooting_gallery_801889C8,
    D_mist_shooting_gallery_80188A70,
};

TmdBone D_mist_shooting_gallery_80188BB8[1] = {
#include "assets/mist_shooting_gallery_model_0B6A0_skeleton.inc"
};

u32 D_mist_shooting_gallery_80188BDC[1] = {
#include "assets/mist_shooting_gallery_model_0B6A0_partVerts.inc"
};

SVECTOR D_mist_shooting_gallery_80188BE0[10] = {
#include "assets/mist_shooting_gallery_model_0B6A0_verts.inc"
};

SVECTOR D_mist_shooting_gallery_80188C30[6] = {
#include "assets/mist_shooting_gallery_model_0B6A0_normals.inc"
};

u32 D_mist_shooting_gallery_80188C60[73] = {
#include "assets/mist_shooting_gallery_model_0B6A0_stream.inc"
};

TmdSource D_mist_shooting_gallery_80188D84 = {
    0,
    528,
    0,
    1,
    D_mist_shooting_gallery_80188BDC,
    D_mist_shooting_gallery_80188BE0,
    D_mist_shooting_gallery_80188C30,
    D_mist_shooting_gallery_80188BB8,
    D_mist_shooting_gallery_80188C60,
};

SVECTOR D_mist_shooting_gallery_80188DA8[22] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_normals.inc"
};

SVECTOR D_mist_shooting_gallery_80188E58[136] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_verts.inc"
};

WorldCollisionGridFace D_mist_shooting_gallery_80189298[57] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_faces.inc"
};

s16 D_mist_shooting_gallery_80189544[474] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_cells.inc"
};

#define GRID_CELL(i) (&D_mist_shooting_gallery_80189544[i])
s16* D_mist_shooting_gallery_801898F8[28] = {
#include "assets/mist_shooting_gallery_collision_0C3A8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mist_shooting_gallery_80189968 = { NULL, D_mist_shooting_gallery_80188DA8, D_mist_shooting_gallery_80188E58, D_mist_shooting_gallery_80189298, D_mist_shooting_gallery_801898F8, 0x2EE0, 7000, 7, 4, 4000, 57 };

GpViewRec D_mist_shooting_gallery_8018998C[18] = {
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
    { 76, 76, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_80134F94 },
    { 143, 151, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_8015E5D0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_mist_shooting_gallery_8018DF74[12] = {
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

static void func_mist_shooting_gallery_801847D4(u8 arg0);

/// Per-frame update for one gallery muzzle-flash / tracer effect. The task's
/// `EffectWork` holds the tracer's endpoint (`pos`), its spin angle (`angle`)
/// and its brightness ramp (`scale`); the handwritten GTE
/// routines below draw the beam and its glow from the task's own coordinate.
/// While `gRoomEffectState->effectControl` is not running the effect only redraws;
/// once control is running again it seeds a random endpoint around the coordinate's world
/// position, then fades out by 8 per frame and releases its pool block.
void func_mist_shooting_gallery_80182064(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];
    u32         rand0;
    u32         rand1;
    u32         rand2;

    work  = (EffectWork*)task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        spriteQuadDraw(coord, work->index, 0x600, work->angle);
        beamStripDraw(coord, &work->pos, work->index, 0x600);
        rgb[0] = work->scale >> 1;
        rgb[1] = work->scale >> 1;
        rgb[2] = work->scale;
        Gp_DrawFadeQuad(rgb, 1);
        return;
    }

    work->age++;
    switch (task->state) {
        case 0:
            coord->parent       = work->parent;
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = 0;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rand0           = gRandomLcgState;
            work->pos.vx    = (u16)coord->workm.t[0] - ((rand0 >> 16 & 0x3FF) - 0x200);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rand1           = gRandomLcgState;
            work->pos.vy    = coord->workm.t[1] - 0x800;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rand2           = gRandomLcgState;
            work->pos.vz    = (u16)coord->workm.t[2] - ((rand1 >> 16 & 0x3FF) - 0x200);
            work->scale     = 0x80;
            work->angle     = rand2 >> 16 & 0xFFF;
        case 1:
            if (work->age & 1) {
                spriteQuadDraw(coord, ++work->index, 0x400, work->angle);
                beamStripDraw(coord, &work->pos, work->index, 0x400);
            }
            rgb[0] = work->scale >> 1;
            rgb[1] = work->scale >> 1;
            rgb[2] = work->scale;
            Gp_DrawFadeQuad(rgb, 1);
            work->scale -= 8;
            if (work->scale < 8) {
                effectKillTask(work, task);
            }
            return;
    }
}

/// Texel width and horizontal stride of each cell in the gallery flash's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the gallery flash's texture row, repeated by its frame counter.
#define SPRITE_QUAD_CELLS_PER_ROW 6
#define SPRITE_QUAD_V0            0x38
#define SPRITE_QUAD_V1            0x5F
#define SPRITE_QUAD_SCALE         39
#define SPRITE_QUAD_CLUT          0x4293
#define SPRITE_QUAD_OTZ_BIAS      0
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

    work       = memCalloc(0x24, 0);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }

    D_mist_shooting_gallery_8018E0C4 = arg0;
    arg0->exitCallback               = func_mist_shooting_gallery_80184A80;
    arg0->state++;
    work->difficulty = arg0->spawnArg1.value & 0xF;
    work->field_0C   = -0xDC;

    actor->weaponShape.ends[0].vz =
        (actor->weaponShape.ends[1].vz + D_80112F60[gPlayerStatus.weapon]) << 1;
    func_801066DC(slot, 1);

    if (work->difficulty < 3) {
        Gp_StateC08.field_6 |= 2;
        if (work->difficulty < 2) {
            actor->movementInputDisabled = 1;
            Display_AcquireRef();
        }
    }
    gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
    gGameSession->battleResetPending            = 0;
    (Gp_IncStateF0Ref)(0);
}

/// Per-frame update for the gallery's bonus course. START (`0x100`) aborts the
/// whole mini-game; otherwise the seventeen states run the banner countdown
/// (`field_20` steps the sprite, `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout` picks which variant), seed the
/// course by spawning individual records of `D_mist_shooting_gallery_80186900[0]`
/// on a timer, and finally enter the wave loop of state 15. State 16 is the
/// out-of-ammo banner: it is entered from anywhere the moment the equipped
/// weapon's stock drops below the round's minimum, remembers the interrupted
/// state in `field_06` and returns to it once the banner has played out.
static void func_mist_shooting_gallery_80182C58(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    s32                       limit;
    s32                       bonus;
    u16                       t1;
    u16                       t2;
    u16                       t5;
    u16                       t6;
    u16                       t8;
    u16                       t9;
    u16                       t12;
    u16                       t13;
    u16                       key;
    u16                       wave;
    u16                       prev;
    u8                        step;

    work  = (MistShootingGalleryWork*)arg0->work;
    bonus = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
    if (Pad_CheckButtons(0, 1, 0x100) != 0) {
        func_8014A9A0();
        return;
    }

    switch (work->field_04) {
        case 0:
            work->field_02 = 0x708;
            work->field_0A = 0x3C;
            work->field_04++;
        case 1:
            t1             = work->field_0A - 1;
            work->field_0A = t1;
            if ((s32)(t1 << 16) <= 0) {
                work->field_0A = 0x1E;
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, 0, 0x8E0);
            }
            break;
        case 2:
            t2             = work->field_0A - 1;
            work->field_0A = t2;
            if ((s32)(t2 << 16) <= 0) {
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 1, 0x8E0);
            }
            break;
        case 3:
            work->field_04++;
            func_mist_shooting_gallery_80184BB8(0x11, 4, 0x8E0);
            break;
        case 4:
            work->field_0A = 0x1E;
            work->field_04++;
            func_mist_shooting_gallery_80184BB8(0x11, bonus + 5, 0x8E0);
            break;
        case 5:
            t5             = work->field_0A;
            work->field_0A = t5 - 1;
            if ((s32)(t5 << 16) <= 0) {
                work->field_0A = 0xF;
                work->field_04++;
                Task_SpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
            }
            break;
        case 6:
            t6             = work->field_0A - 1;
            work->field_0A = t6;
            if ((s32)(t6 << 16) <= 0) {
                work->field_04++;
                spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
                func_mist_shooting_gallery_80184CD0(arg0, spawn);
                work->field_08++;
            }
            break;
        case 7:
            if (work->field_1D != 0) {
                work->field_0A = 0xF;
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 8, 0x8E0);
            } else if (work->field_0E == 0) {
                work->field_0A = 0xF;
                work->field_04++;
            }
            break;
        case 8:
            if (work->field_0E == 0) {
                t8             = work->field_0A - 1;
                work->field_0A = t8;
                if ((s32)(t8 << 16) <= 0) {
                    work->field_0A = 0x3C;
                    work->field_04++;
                    spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
                    func_mist_shooting_gallery_80184CD0(arg0, spawn);
                    work->field_08++;
                }
            }
            break;
        case 9:
            t9             = work->field_0A - 1;
            work->field_0A = t9;
            if ((s32)(t9 << 16) <= 0) {
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 0xB, 0x8E0);
            }
            break;
        case 10:
            if (work->field_0E == 0) {
                spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
                func_mist_shooting_gallery_80184CD0(arg0, spawn);
                work->field_08++;
                if (work->field_08 == 7) {
                    work->field_04++;
                }
            }
            break;
        case 11:
            if (work->field_0E == 0) {
                work->field_0A = 0x1E;
                work->field_04++;
            }
            break;
        case 12:
            t12            = work->field_0A - 1;
            work->field_0A = t12;
            if ((s32)(t12 << 16) <= 0) {
                spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
                func_mist_shooting_gallery_80184CD0(arg0, spawn);
                work->field_08++;
                if (work->field_08 == 0xC) {
                    work->field_0A = 0x3C;
                    work->field_04++;
                } else {
                    work->field_0A = 0xA;
                }
            }
            break;
        case 13:
            t13            = work->field_0A - 1;
            work->field_0A = t13;
            if ((s32)(t13 << 16) <= 0) {
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x11, bonus + 0xE, 0x8E0);
            }
            break;
        case 14:
            if (work->field_0E != 0) {
                break;
            }
            work->field_04++;
            Display_ReleaseRef();
        case 15:
            spawn = &D_mist_shooting_gallery_80186900[0][work->field_08];
            key   = spawn->field_00;
            if (key != 0xFFFF) {
                if (key != 0xFFF1) {
                    if (work->field_00 == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->field_08++;
                        } while (work->field_00 == spawn->field_00);
                    }
                    wave = work->field_00;
                    if (wave <= 0xFFEF) {
                        work->field_00 = wave + 1;
                    }
                } else if (work->field_0E == 0) {
                    work->field_08++;
                }
            }
            func_mist_shooting_gallery_8018458C(work);
            if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                work->field_04 = 0;
                arg0->state++;
            }
            break;
        case 16:
            func_mist_shooting_gallery_80184BB8(0x11, work->field_20, 0x8E0);
            step = work->field_20;
            if (step == 0x15) {
                work->field_04 = work->field_06;
                Display_ReleaseRef();
            } else {
                work->field_20 = step + 1;
            }
            break;
    }

    if (work->field_1E == 0) {
        limit = 2;
        if (gPlayerStatus.weapon == 2) {
            limit = 4;
        }
        if (Gp_ConsumeSlotQty(gPlayerStatus.weapon + 0x7F, 0) < limit) {
            prev           = work->field_04;
            work->field_04 = 0x10;
            work->field_1E = 1;
            work->field_20 = 0x11;
            work->field_06 = prev;
        }
    }
}

/// Per-frame update for the gallery course itself. START (`0x100`) aborts the
/// whole mini-game; otherwise the state runs a "3, 2, 1, GO" countdown
/// (`field_20` steps the digit sprite once a second) before releasing the
/// display reference and entering the wave loop. The loop spawns every record
/// of `D_mist_shooting_gallery_80186904` that carries the current wave number,
/// draws the remaining time, and restarts the state machine once the clock
/// runs out.
static void func_mist_shooting_gallery_801831B0(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    u16                       intro;
    u16                       ready;
    u16                       start;
    u16                       key;
    u16                       wave;
    u8                        step;

    work = (MistShootingGalleryWork*)arg0->work;
    if (Pad_CheckButtons(0, 1, 0x100) != 0) {
        func_8014A9A0();
        return;
    }

    switch (work->field_04) {
        case 0:
            work->field_02 = 0xE10;
            work->field_0A = 0x3C;
            work->field_04++;
        case 1:
            intro          = work->field_0A - 1;
            work->field_0A = intro;
            if ((s32)(intro << 16) <= 0) {
                func_mist_shooting_gallery_80184BB8(0x12, work->field_20, 0x8E0);
                step = work->field_20;
                if (step == 4) {
                    work->field_0A = 0x3C;
                    work->field_04++;
                    return;
                }
                work->field_20 = step + 1;
                return;
            }
        default:
            return;
        case 2:
            ready          = work->field_0A;
            work->field_0A = ready - 1;
            if ((s32)(ready << 16) <= 0) {
                work->field_0A = 0xA;
                work->field_04++;
                Task_SpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
                Gp_ArmStateF0(1);
                return;
            }
            break;
        case 3:
            start          = work->field_0A;
            work->field_0A = start - 1;
            if ((s32)(start << 16) <= 0) {
                work->field_04++;
                Display_ReleaseRef();
                case 4:
                    spawn = &D_mist_shooting_gallery_80186904[work->field_08];
                    key   = spawn->field_00;
                    if (key != 0xFFFF) {
                        if (key != 0xFFF1) {
                            if (work->field_00 == key) {
                                do {
                                    func_mist_shooting_gallery_80184CD0(arg0, spawn);
                                    spawn++;
                                    work->field_08++;
                                } while (work->field_00 == spawn->field_00);
                            }
                            wave = work->field_00;
                            if (wave <= 0xFFEF) {
                                work->field_00 = wave + 1;
                            }
                        } else if (work->field_0E == 0) {
                            work->field_08++;
                        }
                    }
                    func_mist_shooting_gallery_8018458C(work);
                    if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                        work->field_04 = 0;
                        arg0->state++;
                    }
            }
            break;
    }
}

/// Per-frame update for the gallery's second bonus course. States 0-3 run the
/// "ready" banner and the hand-off wait on `gGameSession::location.loc.view`, gated on
/// the countdown hold `gDisplayState.pendingMode`; states 4-5 wait on the player picking up
/// item 0x40, states 6-8 count the banner up through `field_20` while
/// `gSceneCombatState.actorControl` holds, state 9 spawns the start jingle and state 10 is the
/// wave loop over `D_mist_shooting_gallery_80186908`. `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout` picks the
/// banner sprite the hand-off draws (`variant + 4`).
static void func_mist_shooting_gallery_8018341C(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    s32                       bonus;
    s32                       stocked;
    u16                       key;
    u16                       wave;
    u16                       ready;
    u8                        step;

    work  = (MistShootingGalleryWork*)arg0->work;
    bonus = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;

    switch (work->field_04) {
        case 0:
            work->field_02 = 0x1518;
            work->field_0A = 0x1E;
            D_80115768     = 1;
            work->field_04++;
        case 1:
            if (work->field_0A <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->field_20, 0x8E0);
                    step = work->field_20;
                    if (step == 3) {
                        work->field_0A = 0xF;
                        D_80115768     = 0;
                        work->field_04++;
                    } else {
                        work->field_20 = step + 1;
                    }
                }
            } else {
                work->field_0A--;
            }
            break;
        case 2:
            if (gGameSession->location.loc.view == 0x12) {
                if (work->field_0A <= 0) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        work->field_0A = 1;
                        work->field_04++;
                        func_mist_shooting_gallery_80184BB8(0x13, bonus + 4, 0x8E0);
                    }
                } else {
                    work->field_0A--;
                }
            }
            break;
        case 3:
            if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                work->field_1F = 0;
                work->field_20 = 8;
                work->field_04++;
                func_mist_shooting_gallery_80184BB8(0x13, 7, 0x8E0);
            }
            break;
        case 4:
            if (work->field_1F != 0) {
                stocked = Gp_HasStockedItem(0x40);
                if (stocked != 1) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->field_20, 0x8E0);
                    if (work->field_20 == 9) {
                        work->field_1F = 0;
                        work->field_04++;
                    }
                    work->field_20++;
                } else {
                    work->field_04 = 6;
                    work->field_0A = 1;
                    work->field_20 = 0xA;
                }
            }
            break;
        case 5:
            if (Gp_HasStockedItem(0x40) == 1) {
                work->field_0A = 0xF;
                work->field_04++;
            } else if (work->field_1F != 0) {
                func_mist_shooting_gallery_80184BB8(0x13, 9, 0x8E0);
                work->field_1F = 0;
            }
            break;
        case 6:
            if (work->field_0A <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->field_20, 0x8E0);
                    if (work->field_20 == 0xB) {
                        work->field_0A = 0xF;
                        work->field_04++;
                        Gp_ArmStateF0(1);
                    }
                    work->field_20++;
                }
            } else {
                work->field_0A--;
            }
            break;
        case 7:
            work->field_0A--;
            if ((s32)(work->field_0A << 16) <= 0) {
                func_mist_shooting_gallery_80184CD0(arg0, &D_mist_shooting_gallery_80186908[work->field_08]);
                work->field_08++;
                if (work->field_08 == 3) {
                    work->field_0A = 0x3C;
                    work->field_04++;
                } else {
                    work->field_0A = 0xF;
                }
            }
            break;
        case 8:
            if (work->field_0A <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    func_mist_shooting_gallery_80184BB8(0x13, work->field_20, 0x8E0);
                    if (work->field_20 == 0x13) {
                        work->field_0A = 0xF;
                        work->field_04++;
                    }
                    work->field_20++;
                }
            } else {
                work->field_0A--;
            }
            break;
        case 9:
            ready          = work->field_0A;
            work->field_0A = ready - 1;
            if ((s32)(ready << 16) <= 0) {
                work->field_0A = 0xA;
                work->field_04++;
                Task_SpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
            }
            break;
        case 10:
            spawn = &D_mist_shooting_gallery_80186908[work->field_08];
            key   = spawn->field_00;
            if (key != 0xFFFF) {
                if (key != 0xFFF1) {
                    if (work->field_00 == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->field_08++;
                        } while (work->field_00 == spawn->field_00);
                    }
                    wave = work->field_00;
                    if (wave <= 0xFFEF) {
                        work->field_00 = wave + 1;
                    }
                } else if (work->field_0E == 0) {
                    work->field_08++;
                    if (work->field_08 >= 0x49) {
                        work->field_08 = 0xE;
                        work->field_00 = 0xF0;
                    }
                }
            }
            func_mist_shooting_gallery_8018458C(work);
            if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                work->field_04 = 0;
                arg0->state++;
            }
            break;
    }
}

/// Per-frame update for the gallery's first bonus course. States 0-3 run the
/// "ready" banner and the hand-off wait on `gGameSession::location.loc.view`, state 4
/// seeds the first two records of `D_mist_shooting_gallery_8018690C`, states
/// 5-7 hand the player over to actor mode 2 while the banner counts up through
/// `field_20`, and state 8 is the wave loop proper. `Gp_StateC08.field_3` is the abort
/// request: once it is raised the machine saves its place in `field_06` /
/// `field_21` and jumps to the state-9 shutdown banner, which restores them.
static void func_mist_shooting_gallery_801838FC(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    ActorTransform            xform;
    GameActor*                actor;
    s32                       mode;
    s32                       bonus;
    u16                       key;
    u16                       wave;
    u16                       prev;
    s32                       abort;
    u8                        step;
    u8                        hold;

    work  = (MistShootingGalleryWork*)arg0->work;
    bonus = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;

    switch (work->field_04) {
        case 0:
            work->field_02 = 0x1518;
            work->field_0A = 0x1E;
            D_80115768     = 1;
            work->field_04++;
        case 1:
            if (work->field_0A <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    func_mist_shooting_gallery_80184BB8(0x14, 0, 0x8E0);
                    work->field_0A = 0xF;
                    D_80115768     = 0;
                    work->field_04++;
                }
            } else {
                work->field_0A--;
            }
            break;
        case 2:
            if (gGameSession->location.loc.view == 0x12) {
                if (work->field_0A <= 0) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        work->field_0A = 1;
                        work->field_04++;
                        func_mist_shooting_gallery_80184BB8(0x14, 7, 0x8E0);
                    }
                } else {
                    work->field_0A--;
                }
            }
            break;
        case 3:
            if (work->field_1F != 0) {
                work->field_0A = 0x1E;
                work->field_04++;
                Task_SpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
                Gp_ArmStateF0(1);
            }
            break;
        case 4:
            work->field_0A--;
            if ((s32)(work->field_0A << 16) <= 0) {
                func_mist_shooting_gallery_80184CD0(arg0, &D_mist_shooting_gallery_8018690C[work->field_08]);
                work->field_08++;
                if (work->field_08 == 2) {
                    work->field_20 = 8;
                    work->field_0A = 0x3C;
                    work->field_04++;
                } else {
                    work->field_0A = 0xF;
                }
            }
            break;
        case 5:
            work->field_0A--;
            if ((s32)(work->field_0A << 16) <= 0) {
                func_mist_shooting_gallery_80184BB8(0x14, work->field_20, 0x8E0);
                if (work->field_20 == 0xA) {
                    work->field_0A = 0x1E;
                    work->field_04++;
                    func_800E9BDC(5, 0xA);
                    xform.rot.vy = 0xC00;
                    func_80104E00(
                        gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0, &xform, 0);
                }
                work->field_20++;
            }
            break;
        case 6:
            actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
            func_800E9BDC(5, 0xA);
            if (actor->scriptedMotionPending == 0) {
                Gp_EnterActorMode2(
                    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0, 2, 0);
                work->field_04++;
                mode                 = 0x10;
                Gp_StateC08.field_6 |= 0x10;
                work->field_0A       = 4;
                work->field_20       = 0xE;
                if (bonus == 2) {
                    mode = 2;
                }
                func_mist_shooting_gallery_80184BB8(0x14, bonus + 0xB, mode);
            }
            break;
        case 7:
            func_800E9BDC(5, 0xA);
            if (work->field_0A <= 0) {
                if ((u32)((u8)Gp_StateC08.field_A - 2) >= 2) {
                    func_mist_shooting_gallery_80184BB8(0x14, work->field_20, 0x8E0);
                    if (work->field_20 == 0x12) {
                        work->field_04++;
                        func_800E9BDC(0, 0xA);
                        Gp_StateC08.field_6 &= 0xFD;
                    }
                    work->field_20++;
                }
            } else {
                work->field_0A--;
            }
            break;
        case 8:
            spawn = &D_mist_shooting_gallery_8018690C[work->field_08];
            key   = spawn->field_00;
            if (key != 0xFFFF) {
                if (key != 0xFFF1) {
                    if (work->field_00 == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->field_08++;
                        } while (work->field_00 == spawn->field_00);
                    }
                    wave = work->field_00;
                    if (wave <= 0xFFEF) {
                        work->field_00 = wave + 1;
                    }
                } else if (work->field_0E == 0) {
                    work->field_08++;
                    if (work->field_08 >= 0x68) {
                        work->field_08 = 9;
                        work->field_00 = 0x90;
                    }
                }
            }
            func_mist_shooting_gallery_8018458C(work);
            if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                work->field_04 = 0;
                arg0->state++;
            }
            break;
        case 9:
            if (work->field_0A <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    func_mist_shooting_gallery_80184BB8(0x14, work->field_20, 0x8E0);
                    step = work->field_20;
                    if (step == 0x17) {
                        work->field_04 = work->field_06;
                        work->field_20 = work->field_21;
                    } else {
                        work->field_20 = step + 1;
                    }
                }
            } else {
                work->field_0A--;
            }
            break;
    }

    if (work->field_1E == 0 && work->field_20 >= 0x13) {
        abort = Gp_StateC08.field_3;
        if (abort == 1) {
            prev           = work->field_04;
            work->field_1E = abort;
            hold           = work->field_20;
            work->field_04 = 9;
            work->field_0A = 0x1E;
            work->field_20 = 0x13;
            work->field_06 = prev;
            work->field_21 = hold;
        }
    }
}

/// Per-frame update for the gallery's second course. Same shape as
/// `func_mist_shooting_gallery_801831B0`: a countdown that steps the digit
/// sprite through `field_20` (gated on `gDisplayState.pendingMode`), a hand-off wait on
/// `gGameSession::location.loc.view`, then the wave loop over
/// `D_mist_shooting_gallery_80186910`. `field_22` is the abort request - once
/// it is raised the state machine jumps to the 8 -> 9 shutdown, which releases
/// the `gSceneCombatState` reference and kills the task.
static void func_mist_shooting_gallery_80183E78(Task* arg0)
{
    MistShootingGalleryWork*  work;
    MistShootingGallerySpawn* spawn;
    u16                       timer;
    u16                       wave;
    u16                       key;
    u8                        step;

    work = (MistShootingGalleryWork*)arg0->work;

    switch (work->field_04) {
        case 0:
            work->field_02 = 0x1518;
            work->field_0A = 0x1E;
            D_80115768     = 1;
            work->field_04++;
        case 1:
            if (work->field_0A <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    func_mist_shooting_gallery_80184BB8(0x15, 0, 0x8E0);
                    work->field_0A = 0xF;
                    D_80115768     = 0;
                    work->field_04++;
                }
            } else {
                work->field_0A = (u16)work->field_0A - 1;
            }
            break;
        case 2:
            if (gGameSession->location.loc.view != 0x12) {
                break;
            }
            if (work->field_0A <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    work->field_0A = 1;
                    work->field_04++;
                    func_mist_shooting_gallery_80184BB8(0x15, 7, 0x8E0);
                    work->field_20 = 8;
                }
            } else {
                work->field_0A = (u16)work->field_0A - 1;
            }
            break;
        case 3:
            if (work->field_1F == 0) {
                break;
            }
            func_mist_shooting_gallery_80184BB8(0x15, work->field_20, 0x8E0);
            if (work->field_20 == 0xA) {
                work->field_1F = 0;
                work->field_04++;
            }
            work->field_20++;
            break;
        case 4:
            if (work->field_1F == 0) {
                break;
            }
            func_mist_shooting_gallery_80184BB8(0x15, work->field_20, 0x8E0);
            if (work->field_20 == 0x10) {
                work->field_0A = 0x1E;
                work->field_04++;
            }
            work->field_20++;
            break;
        case 5:
            timer          = work->field_0A - 1;
            work->field_0A = timer;
            if ((s32)(timer << 16) <= 0) {
                work->field_0A = 0xF;
                work->field_04++;
                Task_SpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
                SndEvt_EnqueueType6(0x5114000F, 0, 0);
                Gp_StateC08.field_6 &= 0xFD;
                Gp_ArmStateF0(1);
            }
            break;
        case 6:
            timer          = work->field_0A - 1;
            work->field_0A = timer;
            if ((s32)(timer << 16) <= 0) {
                work->field_04++;
            }
            break;
        case 7:
            spawn = &D_mist_shooting_gallery_80186910[work->field_08];
            key   = spawn->field_00;
            if (key != 0xFFFF) {
                if (key != 0xFFF1) {
                    if (work->field_00 == key) {
                        do {
                            func_mist_shooting_gallery_80184CD0(arg0, spawn);
                            spawn++;
                            work->field_08++;
                        } while (work->field_00 == spawn->field_00);
                    }
                    wave = work->field_00;
                    if (wave <= 0xFFEF) {
                        work->field_00 = wave + 1;
                    }
                } else if (work->field_0E == 0) {
                    work->field_08++;
                    if (work->field_08 >= 0x4E) {
                        work->field_08 = 0xB;
                        work->field_00 = 0x2B0;
                    }
                }
            }
            func_mist_shooting_gallery_8018458C(work);
            if (func_mist_shooting_gallery_80184AE0(work) == 0) {
                work->field_04 = 0;
                arg0->state++;
            }
            break;
        case 8:
            timer          = work->field_0A - 1;
            work->field_0A = timer;
            if ((s32)(timer << 16) <= 0) {
                func_mist_shooting_gallery_80184BB8(0x15, work->field_20, 0x8E0);
                step = work->field_20;
                if (step == 0x12) {
                    work->field_0A = 4;
                    D_80115768     = 0;
                    work->field_04++;
                    func_8014A9A0();
                } else {
                    work->field_20 = step + 1;
                }
            }
            break;
        case 9:
            timer          = work->field_0A - 1;
            work->field_0A = timer;
            if ((s32)(timer << 16) <= 0) {
                Gp_ReleaseStateF0Clear(arg0, 0);
                taskKill(arg0);
                return;
            }
            break;
    }

    if (work->field_1E == 0 && work->field_22 != 0) {
        work->field_04 = 8;
        work->field_1E = 1;
        work->field_0A = 3;
        work->field_20 = 0x11;
        D_80115768     = 1;
    }
}

static void func_mist_shooting_gallery_801842D0(Task* arg0)
{
    MistShootingGalleryWork* work;
    GameActor*               actor;

    work  = (MistShootingGalleryWork*)arg0->work;
    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    switch (work->field_04) {
        case 0:
            if (work->field_0E != 0) {
                return;
            }
            work->field_0A = 0x1E;
            work->field_04++;
            Display_AcquireRef();
        case 1:
            if ((s16)work->field_0A-- > 0) {
                return;
            }
            work->field_0A = 0x5A;
            work->field_04++;
            Task_SpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
            SndEvt_EnqueueType6(0x5114000F, 0, 0);
            return;
        case 2:
            if ((s16)--work->field_0A > 0) {
                return;
            }
            work->field_04++;
            actor->movementInputDisabled                        = 0;
            actor->pendingCollisionUpdates                      = 7;
            actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
            Gp_ReleaseStateF0Clear(arg0, 0);
            func_8014A908();
            return;
        case 3:
            if (gGameSession->battleResetPending == 0) {
                work->field_04++;
            }
            return;
        case 4:
            if (gGameSession->battleResetPending == 1) {
                Display_ReleaseRef();
                taskKill(arg0);
            }
            return;
    }
}

s32 func_mist_shooting_gallery_80184470(s32 score)
{
    s32 bonus = 0;

    switch (((MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work)->difficulty) {
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

static void func_mist_shooting_gallery_8018458C(MistShootingGalleryWork* work)
{
    s32 digit0;
    s32 digit1;
    s32 digit2;
    s32 digit3;
    s32 frames;

    frames = work->field_02;
    if (work->field_0C < -0x78) {
        work->field_0C += 0xA;
    }

    digit0 = frames / 18000;
    if (digit0 != 0) {
        frames %= 18000;
    }
    func_mist_shooting_gallery_801846F4(work->field_0C, 0x46, digit0);

    digit1 = frames / 1800;
    if (digit1 != 0) {
        frames %= 1800;
    }
    func_mist_shooting_gallery_801846F4(work->field_0C + 0xC, 0x46, digit1);

    digit2 = frames / 300;
    if (digit2 != 0) {
        frames %= 300;
    }
    func_mist_shooting_gallery_801846F4(work->field_0C + 0x24, 0x46, digit2);

    digit3 = frames / 30;
    if (digit3 != 0) {
        frames %= 30;
    }
    func_mist_shooting_gallery_801846F4(work->field_0C + 0x30, 0x46, digit3);

    func_mist_shooting_gallery_801846F4(work->field_0C + 0x18, 0x46, 0xA);
}

static void func_mist_shooting_gallery_801846F4(s32 arg0, s16 arg1, s32 arg2)
{
    SPRT*     p;
    DR_TPAGE* dr;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    p->w           = 0xF;
    p->h           = 0x13;
    p->clut        = 0x4140;
    setlen(p, 4);
    p->y0 = arg1;
    p->u0 = arg2 * 16;
    p->v0 = 0;
    setcode(p, 0x65);
    p->x0 = arg0;
    addPrim(gGpuCurrentOt, p);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000215;
    addPrim(gGpuCurrentOt, dr);
}

static void func_mist_shooting_gallery_801847D4(u8 arg0)
{
    TILE*     p;
    DR_TPAGE* dr;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    p->x0          = -0xA8;
    p->y0          = -0x7C;
    p->w           = 0x180;
    p->h           = 0x100;
    setlen(p, 3);
    p->r0 = arg0;
    p->g0 = 0;
    p->b0 = 0;
    setcode(p, 0x62);
    addPrim(gGpuCurrentOt, p);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000235;
    addPrim(gGpuCurrentOt, dr);
}

void func_mist_shooting_gallery_801848B4(void)
{
    Enemy*     enemy;
    TmdObject* obj;
    GfxCoord*  coord;

    enemy = Gp_SpawnEnemyFromTable(&D_80134F94, 0, 0x200D, NULL);
    if (enemy != NULL) {
        obj                    = enemy->task->extra.tmd;
        obj->texturePageOffset = 0;
        obj->clutRowOffset     = 2;
        tmdProcessStream(obj);
        tmdProcessStream(obj);
        coord             = enemy->task->extra.tmd->coords;
        coord->coord.t[0] = 0x1770;
        coord->coord.t[2] = 0xBB8;
        coord->coord.t[1] = 0;
        enemy->workType   = ENEMY_WORK_PLAIN;
    }
}

void func_mist_shooting_gallery_80184954(void)
{
    MistShootingGalleryWork* work = (MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work;

    work->field_1F = 1;
}

s32 func_mist_shooting_gallery_80184970(s32 arg0)
{
    MistShootingGalleryWork* work = (MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work;
    s32                      ret  = 0;

    if (work->difficulty < 3) {
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
    MistShootingGalleryWork*  work   = (MistShootingGalleryWork*)arg0->work;
    MistShootingGalleryRounds rounds = D_mist_shooting_gallery_8017DB8C;

    rounds.rounds[work->difficulty](arg0);
}

static void func_mist_shooting_gallery_80184A80(Task* arg0)
{
    GameActor* actor;

    actor                                               = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    actor->movementInputDisabled                        = 0;
    actor->pendingCollisionUpdates                      = 7;
    actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
    Display_ReleaseRef();
    Gp_ReleaseStateF0Clear(arg0, 0);
    taskKill(arg0);
}

static u16 func_mist_shooting_gallery_80184AE0(MistShootingGalleryWork* work)
{
    u16 temp = work->field_02;

    if ((temp != 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING)) {
        work->field_02 = temp - 1;
    }
    return work->field_02;
}

void func_mist_shooting_gallery_80184B10(Task* arg0)
{
    s16 count;

    switch (arg0->state) {
        case 0:
            arg0->state           = 1;
            arg0->killCountdown   = 0x28;
            arg0->spawnArg1.value = 0xFF;
        case 1:
            count = --arg0->killCountdown;
            if (count <= 0) {
                taskKill(arg0);
                return;
            }
            if (count < 0x1F) {
                if (arg0->spawnArg1.value >= 9) {
                    arg0->spawnArg1.value -= 8;
                }
                func_mist_shooting_gallery_801847D4((u8)arg0->spawnArg1.value);
            }
            return;
    }
}

static void func_mist_shooting_gallery_80184BB8(s16 arg0, s16 arg1, s16 arg2)
{
    func_8014B2B8(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_mist_shooting_gallery_801856D0, arg2, 0, 0);
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
                if (Pad_CheckButtons(0, 1, arg0->spawnArg1.value) != 0) {
                    arg0->state = arg0->state + 1;
                } else {
                    func_8014B0D4();
                }
                break;
            }
            func_8014B0D4();
            break;
        case 2:
            taskKill(arg0);
            Stage_SetEndingFlag();
            break;
    }
}

static Enemy* func_mist_shooting_gallery_80184CD0(Task* arg0, MistShootingGallerySpawn* arg1)
{
    MistShootingGalleryWork* work;
    Enemy*                   enemy;
    TmdObject*               obj;
    GfxCoord*                coord;

    work  = (MistShootingGalleryWork*)arg0->work;
    enemy = Gp_SpawnEnemyFromTable(&D_80134F94, 0, arg1->idLo | (arg1->idHi << 16), NULL);
    if (enemy != NULL) {
        enemy->task->parent = arg0;
        taskReparent(arg0, enemy->task);
        obj                    = enemy->task->extra.tmd;
        obj->texturePageOffset = 0;
        obj->clutRowOffset     = 2;
        tmdProcessStream(obj);
        tmdProcessStream(obj);
        coord             = enemy->task->extra.tmd->coords;
        coord->coord.t[0] = arg1->x;
        coord->coord.t[1] = arg1->y;
        coord->coord.t[2] = arg1->z;
        enemy->workType   = ENEMY_WORK_PLAIN;
        work->field_0E++;
    }
    return enemy;
}
