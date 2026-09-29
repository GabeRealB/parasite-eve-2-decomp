#include "rooms/shelter_b4_reservoir.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#define RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

extern SVECTOR D_shelter_b4_reservoir_80185024[14];

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s16 D_shelter_b4_reservoir_80185020[2];

/// Working copy of one surface's extents, carved off the scratchpad stack.
typedef struct {
    s16 y;
    s16 dx;
    s16 dy;
    s16 step;
    s16 x;
    s16 z;
} _SurfaceScratch;

/// Parameters of the random effect burst the room's effect task
/// `func_shelter_b4_reservoir_8017FB84` runs every frame, set together by
/// `func_shelter_b4_reservoir_80182B04` and cleared when the task starts.
/// While `field_0` and `field_2` are both non-zero the task walks `field_0`
/// burst points, spawning an effect at each whose roll out of 100 falls below
/// `field_2`, with `field_4` added to the spawn argument.
typedef struct {
    u16 field_0;
    u16 field_2;
    u16 field_4;
} _ShelterB4ReservoirBurst;

/// Work block `func_shelter_b4_reservoir_8017FB84` reaches through its task's
/// `spawnArg2`. Only the three halves it uses are known.
typedef struct {
    byte pad_0[0x22];
    s16  field_22; // Frames counted while game flag 0xB7 is set; the splash pass is skipped while it is zero
    s16  field_24; // Radius of the last randomised burst point, scaled into the `rsin` / `rcos` offsets
    s16  field_26; // The angle of that point, then the splash chance for the frame's movement
} _ShelterB4ReservoirWork;

/// Spawn table of the screen-wave task, and the context it is spawned with.
/// The context's mode word is written through its own symbol, which is how the
/// original reached it.
extern TaskDesc       D_shelter_b4_reservoir_80184724[];
extern OverlayWaveCtx D_shelter_b4_reservoir_80187624;

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 D_shelter_b4_reservoir_8018473C;

/// The ramp and tint the wave task was spawned with.
extern OverlayWaveCtx* D_shelter_b4_reservoir_80187504;

/// Phase records of the wave's 11 column edges and 30 row edges.
extern OverlayWaveRec6 D_shelter_b4_reservoir_80187514[13];
extern OverlayWaveRec6 D_shelter_b4_reservoir_80187564[32];

extern GpMsgEntry D_shelter_b4_reservoir_801848BC[];
extern TaskDesc   D_shelter_b4_reservoir_801848EC[];
extern TaskDesc   D_shelter_b4_reservoir_80184920[];
extern Task*      D_shelter_b4_reservoir_8018492C;
extern Task*      D_shelter_b4_reservoir_80184930;
extern GpEvsCmd   D_shelter_b4_reservoir_80184948[];
extern GpEvsCmd   D_shelter_b4_reservoir_80184DC8[];
extern u8         D_shelter_b4_reservoir_80184F78;
extern u8         D_shelter_b4_reservoir_80184F79;
extern u8         D_shelter_b4_reservoir_80184F7A;
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s16 value;
    u8  retained[2];
} ShelterB4ReservoirStorage4F7C;
STATIC_ASSERT_SIZEOF(ShelterB4ReservoirStorage4F7C, 4);

extern ShelterB4ReservoirStorage4F7C D_shelter_b4_reservoir_80184F7C;

extern s16                      D_shelter_b4_reservoir_80184F82;
extern TaskDesc                 D_shelter_b4_reservoir_80184F84[];
extern RoomWaterSurface         D_shelter_b4_reservoir_80184F90[];
extern RoomWaterSurface         D_shelter_b4_reservoir_80184FA8[];
extern RoomWaterSurface         D_shelter_b4_reservoir_80184FCC[];
extern RoomWaterSurface         D_shelter_b4_reservoir_80184FE4[];
extern SVECTOR                  D_shelter_b4_reservoir_80185094;
extern SVECTOR                  D_shelter_b4_reservoir_8018509C[];
extern SVECTOR                  D_shelter_b4_reservoir_801850AC[];
extern GpAreaApplyRec           D_shelter_b4_reservoir_801874A0[];
extern GpFadeWork               D_shelter_b4_reservoir_80187500;
extern GpSaveLoc                D_shelter_b4_reservoir_80187508;
extern s32                      D_shelter_b4_reservoir_80187510;
extern u8*                      D_shelter_b4_reservoir_80187630;
extern SVECTOR                  D_shelter_b4_reservoir_80187634[];
extern _ShelterB4ReservoirBurst D_shelter_b4_reservoir_80187684;

/// Per-colour right shifts for the red, green and blue channels of the disc's
/// brightness, one row per spawn argument.
extern s16 D_shelter_b4_reservoir_801850BC[][3];

static void func_shelter_b4_reservoir_8017E7C8(Task* arg0);
static void func_shelter_b4_reservoir_8017E864(Task* task);
static void func_shelter_b4_reservoir_8017E8E4(void);
static void func_shelter_b4_reservoir_8017EA00(Task* task);
static void func_shelter_b4_reservoir_8017EE04(Task* task);
static void func_shelter_b4_reservoir_8017F23C(Task* task);
static void func_shelter_b4_reservoir_8017F674(Task* task);
static void func_shelter_b4_reservoir_8017FB44(Task* arg0);
static void func_shelter_b4_reservoir_80180530(GfxCoord* arg0, s32 arg1, s32 arg2);
static void func_shelter_b4_reservoir_80180D20(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b4_reservoir_8018110C(GfxCoord* arg0, s16 arg1, s16 arg2);
static void func_shelter_b4_reservoir_80181668(GfxCoord* coord, u16 frame, s16 size);
static void func_shelter_b4_reservoir_801818F0(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b4_reservoir_80182134(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2);
static void func_shelter_b4_reservoir_80183298(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b4_reservoir_8018351C(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b4_reservoir_80183940(GfxCoord* arg0, s32 arg1, u8* rgb);
static void func_shelter_b4_reservoir_80183E80(GfxCoord* coord, s16 size);
static void func_shelter_b4_reservoir_801843AC(GfxCoord* arg0, s32 arg1);

/// State handlers of the room task `func_shelter_b4_reservoir_8017E88C` runs,
/// which copies the table to the stack and calls the entry for the task's
/// state: the room's setup, the per-frame state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b4_reservoir_8017D5C4 = {
    { func_shelter_b4_reservoir_8017E7C8, func_shelter_b4_reservoir_8017E864, taskKill }
};

void func_shelter_b4_reservoir_8017FADC(Task*);

extern GpGridParams   D_shelter_b4_reservoir_80185AB8[1];
extern GpObj3A        D_shelter_b4_reservoir_801873B0[2];
extern GpObj4C        D_shelter_b4_reservoir_80186AC0[8];
extern GpObj4C        D_shelter_b4_reservoir_80186D20[7];
extern GpObj4C        D_shelter_b4_reservoir_80186F34[9];
extern GpRoomCoordSet D_shelter_b4_reservoir_80186AA8[1];
extern TaskDesc       D_80142604;
extern TaskDesc       D_80147E48;
extern TaskDesc       D_801575F0;

s32  func_shelter_b4_reservoir_8017E25C(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b4_reservoir_8017E264(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32  func_shelter_b4_reservoir_8017E354(Task*, s32, s32, GpMessageArg);
s32  func_shelter_b4_reservoir_8017E3C4(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b4_reservoir_8017E3CC(Task*, s32, s32, GpMessageArg);
void func_shelter_b4_reservoir_8017D650(Task*);
void func_shelter_b4_reservoir_8017DE8C(Task*);
void func_shelter_b4_reservoir_8017E0AC(Task*);
void func_shelter_b4_reservoir_8017E400(Task*);
void func_shelter_b4_reservoir_8017E4B0(Task*);
void func_shelter_b4_reservoir_8017E558(Task*);
void func_shelter_b4_reservoir_8017E610(s32);
void func_shelter_b4_reservoir_8017E690(s32);
void func_shelter_b4_reservoir_8017E770(s32);
void func_shelter_b4_reservoir_8017E780(s32);
void func_shelter_b4_reservoir_8017E7A8(void);

TaskDesc D_shelter_b4_reservoir_80184724[2] = {
    { 0, 192, func_shelter_b4_reservoir_8017D650, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 D_shelter_b4_reservoir_8018473C = 256;

TmdBone D_shelter_b4_reservoir_80184740[1] = {
#include "assets/shelter_b4_reservoir_model_072D8_skeleton.inc"
};

u32 D_shelter_b4_reservoir_80184764[1] = {
#include "assets/shelter_b4_reservoir_model_072D8_partVerts.inc"
};

SVECTOR D_shelter_b4_reservoir_80184768[15] = {
#include "assets/shelter_b4_reservoir_model_072D8_verts.inc"
};

u32 D_shelter_b4_reservoir_801847E0[46] = {
#include "assets/shelter_b4_reservoir_model_072D8_stream.inc"
};

TmdSource D_shelter_b4_reservoir_80184898 = {
    0,
    320,
    0,
    1,
    D_shelter_b4_reservoir_80184764,
    D_shelter_b4_reservoir_80184768,
    &D_shelter_b4_reservoir_80184768[15],
    D_shelter_b4_reservoir_80184740,
    D_shelter_b4_reservoir_801847E0,
};

GpMsgEntry D_shelter_b4_reservoir_801848BC[6] = {
    { 5102, func_shelter_b4_reservoir_8017E264 },
    { 5105, func_shelter_b4_reservoir_8017E25C },
    { 5103, func_shelter_b4_reservoir_8017E3C4 },
    { 5104, func_shelter_b4_reservoir_8017E354 },
    { 5106, func_shelter_b4_reservoir_8017E3CC },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_b4_reservoir_801848EC[4] = {
    { 0, 32, func_shelter_b4_reservoir_8017DE8C, { .model = NULL } },
    { 0, 32, func_shelter_b4_reservoir_8017E400, { .model = NULL } },
    { 0, 32, func_shelter_b4_reservoir_8017E4B0, { .model = NULL } },
    { 0, 32, func_shelter_b4_reservoir_8017E0AC, { .model = NULL } },
};

GpCmdArg D_shelter_b4_reservoir_8018491C = { { .loc = { 4, 45 } }, 6 };

TaskDesc D_shelter_b4_reservoir_80184920[1] = {
    { 257, 32, func_shelter_b4_reservoir_8017E558, { .model = &D_shelter_b4_reservoir_80184898 } },
};

Task* D_shelter_b4_reservoir_8018492C = 0;

Task* D_shelter_b4_reservoir_80184930 = NULL;

AnimationPlayRequest D_shelter_b4_reservoir_80184934 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

GpEvsCmd D_shelter_b4_reservoir_80184948[48] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b4_reservoir_8017E7A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E610 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E770 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E770 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x542D0005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b4_reservoir_8017E7A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E610 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x542D0005 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x542D0006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b4_reservoir_8017E7A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 27, { .value = 0x542D0006 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x542D0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_shelter_b4_reservoir_8018491C }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x542D0006 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_shelter_b4_reservoir_80184DC8[18] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E610 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_shelter_b4_reservoir_8018491C }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b4_reservoir_8017E7A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

u8 D_shelter_b4_reservoir_80184F78 = 4;

u8 D_shelter_b4_reservoir_80184F79 = 8;

u8 D_shelter_b4_reservoir_80184F7A = 3;

ShelterB4ReservoirStorage4F7C D_shelter_b4_reservoir_80184F7C = { 640, { 0, 0 } };

s16 D_shelter_b4_reservoir_80184F80 = -2000;

s16 D_shelter_b4_reservoir_80184F82 = 0;

TaskDesc D_shelter_b4_reservoir_80184F84[1] = {
    { 0, 96, func_shelter_b4_reservoir_8017FADC, { .model = NULL } },
};

RoomWaterSurface D_shelter_b4_reservoir_80184F90[2] = {
    { -980, -5900, 2023, 9900, 0 },
    { 0, 0, 0, 0, -1 },
};

RoomWaterSurface D_shelter_b4_reservoir_80184FA8[3] = {
    { 1037, -5900, 950, 0x2E18, 64 },
    { 1987, -5900, 950, 0x2E18, 64 },
    { 0, 0, 0, 0, -1 },
};

RoomWaterSurface D_shelter_b4_reservoir_80184FCC[2] = {
    { 2940, -3900, 2900, 1800, 16 },
    { 0, 0, 0, 0, -1 },
};

RoomWaterSurface D_shelter_b4_reservoir_80184FE4[5] = {
    { 1400, 3100, 1000, 4800, 64 },
    { -1028, 3100, 2428, 4800, 64 },
    { -1028, -5900, 2428, 9000, 1 },
    { 788, -5900, 1500, 3800, 1 },
    { 0, 0, 0, 0, -1 },
};

s16 D_shelter_b4_reservoir_80185020[2] = {
    0,
    -0x693D,
};

// Lighting task uses entries 0,6,8,12; two-point capsule renderer also consumes the second point of each pair.
SVECTOR D_shelter_b4_reservoir_80185024[14] = {
    { -1580, -4460, 5900, 0 },
    { -420, -4460, 5900, 0 },
    { -1580, -4460, -5900, 0 },
    { -420, -4460, -5900, 0 },
    { 2900, -2520, -4440, 0 },
    { 2900, -2520, -5600, 0 },
    { 2900, -2520, 590, 0 },
    { 2900, -2520, -580, 0 },
    { 2900, -2520, 4600, 0 },
    { 2900, -2520, 3420, 0 },
    { 3920, -2390, -3000, 0 },
    { 5080, -2390, -3000, 0 },
    { -2640, -2980, 4900, 0 },
    { -2640, -2980, 4800, 0 },
};

SVECTOR D_shelter_b4_reservoir_80185094 = { -1000, -1000, -5000, 0 };

SVECTOR D_shelter_b4_reservoir_8018509C[2] = {
    { 0, -3210, -5000, 0 },
    { 0, 0, -5000, 0 },
};

SVECTOR D_shelter_b4_reservoir_801850AC[2] = { 0 };

// RGB shifts selected by Actor02400's (place->mode & 1) variant.
s16 D_shelter_b4_reservoir_801850BC[2][3] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};

// Gameplay direction-facing rows, selected by Gp_DirFlags & 0x100.
// Each row reserves 16 byte positions for the encoded direction index.
// The final zero bytes may also have served as alignment in the original C.
u8 D_shelter_b4_reservoir_801850C8[16] = {
    2,
    2,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    5,
    5,
    5,
    5,
    5,
    5,
    0,
};
u8 D_shelter_b4_reservoir_801850D8[16] = {
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    0,
    0,
    0,
};

GpRoomCoordRec D_shelter_b4_reservoir_801850E8[2] = {
    { D_shelter_b4_reservoir_80186AA8, NULL },
    { D_shelter_b4_reservoir_80186AA8, NULL },
};

GpRoomObjRec D_shelter_b4_reservoir_801850F8[2] = {
    { D_shelter_b4_reservoir_80185AB8, D_shelter_b4_reservoir_80186AC0, D_shelter_b4_reservoir_80186D20, D_shelter_b4_reservoir_801873B0 },
    { D_shelter_b4_reservoir_80185AB8, D_shelter_b4_reservoir_80186AC0, D_shelter_b4_reservoir_80186F34, D_shelter_b4_reservoir_801873B0 },
};

u8* D_shelter_b4_reservoir_80185118[2] = {
    D_8010CAF8,
    D_8010CAF8,
};

GpViewCountRec D_shelter_b4_reservoir_80185120[2] = {
    { { .bytes = { 10, 0 } } },
    { { .bytes = { 10, 0 } } },
};

GpWarpRec D_shelter_b4_reservoir_80185124[2] = {
    { { .words = { 3072, 5500, 0, -3000 } }, { 0, 0, 0, 0 }, { .words = { 3584, 5000, 0, -3650 } }, { 0, 0, 0, 0 }, 0x542D0004, 0x542D0003, 0, 2, 0, 0 },
    { { .words = { 1024, -2527, -2000, -2825 } }, { 0, 0, 0, 0 }, { .words = { 512, -2700, -2000, -2000 } }, { 0, 0, 0, 0 }, 0, 0, 0, 7, 2, 446 },
};

SVECTOR D_shelter_b4_reservoir_80185194[8] = {
    { 400, 1000, -2400, 0 },
    { 2000, 1000, 2100, 0 },
    { 2000, 1000, 5000, 0 },
    { 1500, 1000, -4000, 0 },
    { 0, 1000, -600, 0 },
    { 200, 1000, -3900, 0 },
    { 2000, 1000, 400, 0 },
    { 1000, 1000, -4500, 0 },
};

SVECTOR* D_shelter_b4_reservoir_801851D4[4] = {
    D_shelter_b4_reservoir_80185194,
    D_shelter_b4_reservoir_80185194,
    D_shelter_b4_reservoir_80185194,
    D_shelter_b4_reservoir_80185194,
};

SVECTOR D_shelter_b4_reservoir_801851E4[5] = {
    { 0, 0, 0, 0 },
    { 0, 1000, -3000, 0 },
    { 0, 1000, 0, 0 },
    { 2000, 1000, 3800, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_shelter_b4_reservoir_8018520C[22] = {
#include "assets/shelter_b4_reservoir_collision_084F8_normals.inc"
};

SVECTOR D_shelter_b4_reservoir_801852BC[98] = {
#include "assets/shelter_b4_reservoir_collision_084F8_verts.inc"
};

GpGridFace D_shelter_b4_reservoir_801855CC[45] = {
#include "assets/shelter_b4_reservoir_collision_084F8_faces.inc"
};

s16 D_shelter_b4_reservoir_801857E8[310] = {
#include "assets/shelter_b4_reservoir_collision_084F8_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b4_reservoir_801857E8[i])
s16* D_shelter_b4_reservoir_80185A54[25] = {
#include "assets/shelter_b4_reservoir_collision_084F8_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b4_reservoir_80185AB8[1] = {
    { NULL, D_shelter_b4_reservoir_8018520C, D_shelter_b4_reservoir_801852BC, D_shelter_b4_reservoir_801855CC, D_shelter_b4_reservoir_80185A54, 0x2AC6, 6000, 5, 5, 4000, 45 },
};

GpViewRec D_shelter_b4_reservoir_80185ADC[10] = {
    { { { { 4095, 0, -20 }, { -20, 35, -4095 }, { 0, 4095, 35 } }, { 151, 0x7530, -115 } }, 447 },
    { { { { -1356, 0, -3864 }, { 134, 4093, -47 }, { 3862, -142, -1355 } }, { -836, 880, 2121 } }, 230 },
    { { { { -4037, 0, 688 }, { 118, 4034, 695 }, { -678, 705, -3977 } }, { -1560, 2040, -2150 } }, 257 },
    { { { { 3928, 0, 1160 }, { 175, 4049, -593 }, { -1147, 618, 3883 } }, { -2310, 2260, 4900 } }, 257 },
    { { { { 4066, 0, 488 }, { 137, 3928, -1149 }, { -468, 1158, 3900 } }, { 1360, 4030, 3480 } }, 257 },
    { { { { 1937, 0, 3608 }, { 1304, 3818, -700 }, { -3364, 1480, 1806 } }, { 730, 3443, -4061 } }, 358 },
    { { { { -3980, 0, 967 }, { -85, 4080, -351 }, { -963, -361, -3964 } }, { 1190, 2910, -1600 } }, 257 },
    { { { { -592, 0, 4052 }, { 353, 4080, 51 }, { -4037, 357, -590 } }, { -2060, 1280, 4640 } }, 380 },
    { { { { -2610, 0, -3155 }, { -2761, 1983, 2284 }, { 1528, 3583, -1264 } }, { 560, 5430, 2400 } }, 230 },
    { { { { -3991, 0, -921 }, { -208, 3989, 902 }, { 897, 925, -3887 } }, { 360, 2120, -7690 } }, 257 },
};

GpSprtCmd D_shelter_b4_reservoir_80185C44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_reservoir_80185C54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_reservoir_80185C64[11] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -96, 1050, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, -104, 1000, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, -112, 975, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -120, 925, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -56, 925, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -56, 975, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, 8, 925, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 72, 925, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -144, 8, 975, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -144, 56, 975, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, 40, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b4_reservoir_80185D40[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_reservoir_80185D58[4] = {
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -64, 2000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 8, -8, 1500, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 16, -32, 1525, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, -64, 1750, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b4_reservoir_80185DA8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_reservoir_80185DC0[58] = {
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 80, 112, 1338, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 80, 104, 1347, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 96, 1384, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 88, 1405, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 80, 1438, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 72, 1740, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 64, 1504, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 56, 1630, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 48, 1578, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 40, 1500, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 32, 1627, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 24, 1693, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 16, 1749, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 8, 1795, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 0, 1811, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -8, 1996, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -16, 2195, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -24, 2325, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, -40, 1875, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, -40, 1675, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -32, 1625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 0, 1500, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, 0, 1375, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 24, 1300, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 40, 1375, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 72, 1375, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 40, 1300, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 72, 1300, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, 56, 1250, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 80, 1225, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 0, 1875, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 8, 1795, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 16, 1685, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 24, 1693, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 32, 1625, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 40, 1600, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 48, 1578, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 56, 1550, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 64, 1504, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 72, 1462, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 80, 1438, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 88, 1405, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 96, 1384, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 104, 1347, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 112, 1338, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, 16, 1375, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, 24, 1350, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, 32, 1325, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 40, 1250, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 48, 1125, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 56, 1075, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 64, 1000, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 72, 925, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 80, 875, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 88, 850, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -8, 96, 825, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -8, 104, 775, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -8, 112, 750, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b4_reservoir_80186248[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 45, 0, 0, { 1, 0 } },
    { 45, 13, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_reservoir_80186268[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_reservoir_80186278[9] = {
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -72, 48, 1912, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -72, 56, 1908, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 64, 1570, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 72, 1443, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 80, 1373, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 88, 1241, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -88, 96, 1022, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -88, 104, 963, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -88, 112, 925, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b4_reservoir_8018632C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_reservoir_80186344[36] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 750, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -112, -120, 750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -112, -48, 750, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -112, 24, 750, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -104, 72, 750, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, -56, 32, 750, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 48 } }, -56, 72, 750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, 48, 750, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, 8, 72, 750, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, 64, 72, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 120, 72, 750, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 8, 32, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 16, 40, 750, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, 64, 32, 750, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 750, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 56, -40, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -56, 16, 750, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, 16, 750, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, 24, 750, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -56, 8, 750, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, 0, 750, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 0, 0, 750, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -56, -24, 750, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 0, -24, 750, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, -32, 750, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, -32, 750, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -56, -48, 750, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 0, -48, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 104, -40, 750, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 104, -80, 750, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, 104, -120, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 56, -80, 750, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 56, -120, 750, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 0, -120, 750, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -56, -120, 750, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -56, -8, 750, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b4_reservoir_80186614[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 36, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_reservoir_8018662C[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, -72, 1274, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 825, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, -80, 942, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -48, 1031, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 909, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -120, 1060, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, -80, 1298, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, -80, 986, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -80, 1030, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, -72, 1009, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -64, 1013, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b4_reservoir_80186708[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_reservoir_80186720[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b4_reservoir_80186730[10] = {
    { { .empty = D_shelter_b4_reservoir_80185C44 }, D_shelter_b4_reservoir_80185C44, NULL },
    { { .empty = D_shelter_b4_reservoir_80185C54 }, D_shelter_b4_reservoir_80185C54, NULL },
    { { .elements = D_shelter_b4_reservoir_80185C64 }, D_shelter_b4_reservoir_80185D40, NULL },
    { { .elements = D_shelter_b4_reservoir_80185D58 }, D_shelter_b4_reservoir_80185DA8, NULL },
    { { .elements = D_shelter_b4_reservoir_80185DC0 }, D_shelter_b4_reservoir_80186248, NULL },
    { { .empty = D_shelter_b4_reservoir_80186268 }, D_shelter_b4_reservoir_80186268, NULL },
    { { .elements = D_shelter_b4_reservoir_80186278 }, D_shelter_b4_reservoir_8018632C, NULL },
    { { .elements = D_shelter_b4_reservoir_80186344 }, D_shelter_b4_reservoir_80186614, NULL },
    { { .elements = D_shelter_b4_reservoir_8018662C }, D_shelter_b4_reservoir_80186708, NULL },
    { { .empty = D_shelter_b4_reservoir_80186720 }, D_shelter_b4_reservoir_80186720, NULL },
};

GpPointLight D_shelter_b4_reservoir_801867A8[8] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -4450, 5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 3000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -4450, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 3000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2520, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2520, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 2581, 4061 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2520, 4000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -4450, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 5367, 6365 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -3000, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1228, 1228, 1228, { 0, 0 } }, 5000, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4500, -2000, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 1500, 2500 },
};

GpRoomCoordSet D_shelter_b4_reservoir_80186AA8[1] = {
    { 0, NULL, 8, D_shelter_b4_reservoir_801867A8, 0, NULL },
};

GpObj4C D_shelter_b4_reservoir_80186AC0[8] = {
    { NULL, NULL, NULL, { -3072, -1856, -1137, 0 }, { { -2192, -4304, 0, 0 }, { 2192, -4304, 0, 0 }, { -2192, 4304, 0, 0 }, { 2192, 4304, 0, 0 } }, { 0, 0, -4120, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 7, 5, 1, 0 },
    { NULL, NULL, NULL, { -3008, -1729, -1264, 0 }, { { 2128, -4304, 0, 0 }, { -2128, -4304, 0, 0 }, { 2128, 4304, 0, 0 }, { -2128, 4304, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 5, 7, 1, 0 },
    { NULL, NULL, NULL, { -32, -1856, 3200, 0 }, { { 1008, -4304, 0, 0 }, { -1008, -4304, 0, 0 }, { 1008, 4304, 0, 0 }, { -1008, 4304, 0, 0 } }, { 0, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -16, -1889, 3328, 0 }, { { -1088, -4304, 0, 0 }, { 1088, -4304, 0, 0 }, { -1088, 4304, 0, 0 }, { 1088, 4304, 0, 0 } }, { 0, 0, -4116, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 1184, -1952, -1632, 0 }, { { 2188, -4304, 204, 0 }, { -2191, -4304, -207, 0 }, { 2188, 4304, 204, 0 }, { -2191, 4304, -207, 0 } }, { -384, 0, 4088, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 1168, -1888, -1504, 0 }, { { -2276, -4304, -229, 0 }, { 2270, -4304, 222, 0 }, { -2276, 4304, -229, 0 }, { 2270, 4304, 222, 0 } }, { 406, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 3008, -2016, -2992, 0 }, { { 0, -4304, -1392, 0 }, { 0, -4304, 1392, 0 }, { 0, 4304, -1392, 0 }, { 0, 4304, 1392, 0 } }, { 4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 3136, -1984, -2976, 0 }, { { 0, -4304, 1264, 0 }, { 0, -4304, -1264, 0 }, { 0, 4304, 1264, 0 }, { 0, 4304, -1264, 0 } }, { -4108, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 3, 2, 129, 0 },
};

GpObj4C D_shelter_b4_reservoir_80186D20[7] = {
    { NULL, NULL, NULL, { 0, -48, 864, 0 }, { { -1024, 0, -160, 0 }, { 1024, 0, -160, 0 }, { -1024, 0, 160, 0 }, { 1024, 0, 160, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1031, 1, 202, 0, 2, 0 },
    { NULL, NULL, NULL, { 224, -1888, 3888, 0 }, { { -1024, 0, -320, 0 }, { 1024, 0, -320, 0 }, { -1024, 0, 320, 0 }, { 1024, 0, 320, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1070, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 5661, -48, -3072, 0 }, { { 448, 0, -1024, 0 }, { 448, 0, 1024, 0 }, { -448, 0, -1024, 0 }, { -448, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1115, 0, 46, 18, 2, 0 },
    { NULL, NULL, NULL, { -2736, -2096, -3552, 0 }, { { 240, 0, -1024, 0 }, { 240, 0, 1024, 0 }, { -240, 0, -1024, 0 }, { -240, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1047, 0, 44, 34, 2, 0 },
    { NULL, NULL, NULL, { -1968, -1888, 5152, 0 }, { { 576, 0, -1024, 0 }, { 576, 0, 1024, 0 }, { -576, 0, -1024, 0 }, { -576, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1173, 2, 3, 255, 2, 0 },
    { NULL, NULL, NULL, { -2817, -2080, -5648, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1108, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { -2720, -2080, -816, 0 }, { { -448, 0, -1216, 0 }, { 448, 0, -1216, 0 }, { -448, 0, 1216, 0 }, { 448, 0, 1216, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 1292, 2, 7, 0, 130, 0 },
};

GpObj4C D_shelter_b4_reservoir_80186F34[9] = {
    { NULL, NULL, NULL, { -512, -48, 864, 0 }, { { -1024, 0, -160, 0 }, { 1024, 0, -160, 0 }, { -1024, 0, 160, 0 }, { 1024, 0, 160, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1031, 1, 202, 0, 2, 0 },
    { NULL, NULL, NULL, { 384, -1888, 3776, 0 }, { { -1024, 0, -208, 0 }, { 1024, 0, -208, 0 }, { -1024, 0, 208, 0 }, { 1024, 0, 208, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1039, 257, 202, 128, 2, 0 },
    { NULL, NULL, NULL, { 5872, -48, -3072, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, 0, 46, 18, 2, 0 },
    { NULL, NULL, NULL, { -2736, -2096, -3552, 0 }, { { 240, 0, -1024, 0 }, { 240, 0, 1024, 0 }, { -240, 0, -1024, 0 }, { -240, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1047, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { -2144, -2080, 5232, 0 }, { { 848, 0, -816, 0 }, { 848, 0, 816, 0 }, { -848, 0, -816, 0 }, { -848, 0, 816, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1173, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -1056, -64, -5728, 0 }, { { 848, 0, -816, 0 }, { 848, 0, 816, 0 }, { -848, 0, -816, 0 }, { -848, 0, 816, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1173, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 2080, -64, 5616, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1123, 2, 9, 0, 2, 0 },
    { NULL, NULL, NULL, { -2816, -2064, -5648, 0 }, { { -1024, 0, -352, 0 }, { 1024, 0, -352, 0 }, { -1024, 0, 352, 0 }, { 1024, 0, 352, 0 } }, { 0, 4094, 0, 0 }, { 0, 0, 4096, 0 }, 1078, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { -2768, -2080, -944, 0 }, { { -496, 0, -1200, 0 }, { 496, 0, -1200, 0 }, { -496, 0, 1200, 0 }, { 496, 0, 1200, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 1292, 2, 10, 0, 130, 0 },
};

GpAreaTmdRec D_shelter_b4_reservoir_801871E0[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_reservoir_801871F8[4] = {
    { 44, 44, 0, 0, { 0, 0 }, &D_80142604 },
    { 72, 72, 1, 0, { 0, 0 }, D_80153EC8 },
    { 73, 73, 1, 0, { 0, 0 }, D_8014E7A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_reservoir_80187228[4] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 70, 70, 1, 0, { 0, 0 }, &D_801575F0 },
    { 71, 71, 1, 0, { 0, 0 }, &D_80151E60 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_reservoir_80187258[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b4_reservoir_80187270[2] = {
    { 4, 0, 1, 900, 1000, -2400, 3072, 0, 0, 2, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_reservoir_80187290[5] = {
    { 44, 0, 0, 2950, 0, -2800, 1200, 0, 0, 2, 0 },
    { 44, 0, 0, 1650, 0, -400, 2100, 0, 0, 2, 0 },
    { 72, 0, 0, -1800, -2000, -2150, 200, 0, 2, 4, 0 },
    { 72, 0, 0, -2350, -2000, -4350, 400, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_reservoir_801872E0[4] = {
    { 24, 0, 0, -2100, -2000, 780, 2048, 0, 0, 2, 0 },
    { 70, 0, 0, 780, 0, -4300, 3584, 0, 2, 4, 0 },
    { 71, 0, 1, 1750, 0, 3450, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_reservoir_80187320[3] = {
    { 4, 0, 0, 850, 0, -1000, 3200, 0, 0, 2, 0 },
    { 4, 0, 0, 350, 0, -4600, 300, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b4_reservoir_80187350[12] = {
    { NULL, NULL },
    { D_shelter_b4_reservoir_80187270, D_shelter_b4_reservoir_801871E0 },
    { D_shelter_b4_reservoir_80187290, D_shelter_b4_reservoir_801871F8 },
    { D_shelter_b4_reservoir_801872E0, D_shelter_b4_reservoir_80187228 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b4_reservoir_80187320, D_shelter_b4_reservoir_80187258 },
};

GpObj3A D_shelter_b4_reservoir_801873B0[2] = {
    { NULL, NULL, { 4544, -2432, -1024, 0 }, { { -1408, 4896, -768, 0 }, { 1408, 4896, 768, 0 }, { -1408, -4896, -768, 0 }, { 1408, -4896, 768, 0 } }, { -1963, 0, 3597, 0 }, { 25, 20 }, 1, 0 },
    { NULL, NULL, { 4447, 0, -4993, 0 }, { { -1455, 4896, 834, 0 }, { 1456, 4896, -833, 0 }, { -1455, -4896, 834, 0 }, { 1456, -4896, -833, 0 } }, { 2036, 0, 3555, 0 }, { 50, 20 }, 129, 0 },
};

s32 D_shelter_b4_reservoir_80187428[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

s32 D_shelter_b4_reservoir_80187434[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

s32 D_shelter_b4_reservoir_80187440[3] = {
    0x10000015,
    0x10000017,
    0x10000015,
};

s32 D_shelter_b4_reservoir_8018744C[3] = {
    0x10000025,
    0x10000027,
    0x10000029,
};

GpRoomParamRec D_shelter_b4_reservoir_80187458[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b4_reservoir_80187460[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_shelter_b4_reservoir_80187468[1] = {
    { 0, 0, 1, 0, D_shelter_b4_reservoir_8018744C },
};

GpRoomParamRec D_shelter_b4_reservoir_80187470[1] = {
    { 0, 0, 1, 0, D_shelter_b4_reservoir_80187440 },
};

GpRoomParamRec D_shelter_b4_reservoir_80187478[1] = {
    { 0, 0, 1, 0, D_shelter_b4_reservoir_80187434 },
};

GpRoomParamRec* D_shelter_b4_reservoir_80187480[8] = {
    D_shelter_b4_reservoir_80187458,
    D_shelter_b4_reservoir_80187460,
    D_shelter_b4_reservoir_80187468,
    D_shelter_b4_reservoir_80187470,
    D_shelter_b4_reservoir_80187458,
    D_shelter_b4_reservoir_80187478,
    D_shelter_b4_reservoir_80187458,
    D_shelter_b4_reservoir_80187458,
};

GpAreaApplyRec D_shelter_b4_reservoir_801874A0[24] = {
    { 4, 42, 3, 0 },
    { 4, 43, 2, 17 },
    { 4, 43, 8, 33 },
    { 4, 44, 2, 17 },
    { 4, 44, 8, 33 },
    { 3, 1, 11, 1 },
    { 3, 2, 1, 1 },
    { 3, 6, 11, 1 },
    { 3, 7, 11, 1 },
    { 3, 13, 1, 1 },
    { 3, 16, 11, 1 },
    { 3, 19, 11, 1 },
    { 3, 20, 11, 1 },
    { 3, 22, 11, 0 },
    { 3, 24, 4, 1 },
    { 3, 25, 10, 0 },
    { 3, 28, 1, 1 },
    { 3, 29, 11, 1 },
    { 3, 31, 11, 1 },
    { 3, 32, 10, 0 },
    { 3, 34, 11, 1 },
    { 3, 38, 11, 17 },
    { 3, 38, 1, 33 },
    { 255, 0, 0, 0 },
};

GpFadeWork D_shelter_b4_reservoir_80187500 = { 0 };

OverlayWaveCtx* D_shelter_b4_reservoir_80187504 = NULL;

GpSaveLoc D_shelter_b4_reservoir_80187508 = { 0 };

s32 D_shelter_b4_reservoir_80187510 = 0;

OverlayWaveRec6 D_shelter_b4_reservoir_80187514[13] = { 0 };

OverlayWaveRec6 D_shelter_b4_reservoir_80187564[32] = { 0 };

OverlayWaveCtx D_shelter_b4_reservoir_80187624 = { 0 };

u8* D_shelter_b4_reservoir_80187630 = NULL;

SVECTOR D_shelter_b4_reservoir_80187634[10] = { 0 };

_ShelterB4ReservoirBurst D_shelter_b4_reservoir_80187684 = { 0, 0, 0 };

static void func_shelter_b4_reservoir_8017E068(void);
static void func_shelter_b4_reservoir_8017E8EC(Task* task);

/// Task that ripples the whole screen: it redraws the frame just rendered as a
/// 10 by 30 grid of textured quads whose corners are pushed around by sine
/// waves. The first frame gives every column and row edge a random phase
/// offset and speed, takes its context from `spawnArg2` and passes -8 to
/// `displaySetShakeY`. Afterwards the context's mode ramps the strength
/// up to its limit (mode 0), back down to zero and on to mode 2 (mode 1), or
/// ends the task and passes 0 back (mode 2); the displacement is the ramp's
/// share of the context's peak. A non-zero tint flag shades the quads with the context's
/// colour instead of drawing them unlit. The grid is bracketed by draw-mode
/// packets that switch mask-bit setting on at the back of the order table and
/// off again at the front.
void func_shelter_b4_reservoir_8017D650(Task* arg0)
{
    OverlayWaveCtx* ctx;
    POLY_FT4*       p;
    DR_STP*         stp;
    s32             i, j, k;
    s32             drawY;
    s32             tpage0, tpage1;
    s32             u0, u1, v0, v1;
    s32             waveX0, waveY0, waveX1, waveY1;
    s32             waveX2, waveY2, waveX3, waveY3;
    s32*            state;

    CdCmd_Queue.field_22A = 2;
    /* Through a pointer rather than as `arg0->state`: a member load is struct
       memory, which the scheduler lets rise above the store before it, and the
       original keeps the two in source order. */
    state = &arg0->state;
    switch (*state) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_shelter_b4_reservoir_80187514[i].phase  = 0;
                D_shelter_b4_reservoir_80187514[i].offset = (u32)rand() >> 3;
                D_shelter_b4_reservoir_80187514[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_shelter_b4_reservoir_80187564[i].phase  = 0;
                D_shelter_b4_reservoir_80187564[i].offset = (u32)rand() >> 3;
                D_shelter_b4_reservoir_80187564[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_shelter_b4_reservoir_8018473C        = 0;
            D_shelter_b4_reservoir_80187504        = arg0->spawnArg2.pointer;
            D_shelter_b4_reservoir_80187504->frame = 0;
            D_shelter_b4_reservoir_80187504->state = 0;
            displaySetShakeY(DISPLAY_SHAKE_MIN);
            arg0->state++;
            break;
        case 1:
            ctx = D_shelter_b4_reservoir_80187504;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        ctx->frame--;
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    displaySetShakeY(0);
                    break;
            }
            D_shelter_b4_reservoir_8018473C = D_shelter_b4_reservoir_80187504->frame * D_shelter_b4_reservoir_80187504->scale / D_shelter_b4_reservoir_80187504->span;
            for (i = 0; i < 11; i++) {
                D_shelter_b4_reservoir_80187514[i].phase += D_shelter_b4_reservoir_80187514[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_shelter_b4_reservoir_80187564[i].phase += D_shelter_b4_reservoir_80187564[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_shelter_b4_reservoir_80187504->blend == ANIMATION_BLEND_RESET) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_shelter_b4_reservoir_80187504->r;
                        p->g0 = D_shelter_b4_reservoir_80187504->g;
                        p->b0 = D_shelter_b4_reservoir_80187504->b;
                    }
                    u0 = k * 32;
                    u1 = (k + 1) * 32;
                    if (u1 == 320)
                        u1 = 319;
                    if (u0 < 128) {
                        p->tpage = tpage0;
                    } else {
                        p->tpage = tpage1;
                        u0      -= 128;
                        u1      -= 128;
                    }
                    if (j != -1) {
                        v1     = (j + 1) * 8 + gDisplayState.drawBuffer * 16;
                        v0     = j * 8 + gDisplayState.drawBuffer * 16;
                        waveX0 = D_shelter_b4_reservoir_8018473C * (rsin((j << 9) + D_shelter_b4_reservoir_80187514[k].phase + D_shelter_b4_reservoir_80187514[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_shelter_b4_reservoir_8018473C * (rsin((k << 10) + D_shelter_b4_reservoir_80187564[j].phase + D_shelter_b4_reservoir_80187564[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_shelter_b4_reservoir_8018473C * (rsin((j << 9) + D_shelter_b4_reservoir_80187514[k + 1].phase + D_shelter_b4_reservoir_80187514[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_shelter_b4_reservoir_8018473C * (rsin(((k + 1) << 10) + D_shelter_b4_reservoir_80187564[j].phase + D_shelter_b4_reservoir_80187564[j].offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        drawY = gDisplayState.drawBuffer * 16;
                        p->x0 = k * 32 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 32 - 160;
                        p->y1 = -112;
                        v0    = drawY + 8;
                        v1    = drawY;
                    }
                    {

                        waveX2 = D_shelter_b4_reservoir_8018473C * (rsin(((j + 1) << 9) + D_shelter_b4_reservoir_80187514[k].phase + D_shelter_b4_reservoir_80187514[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_shelter_b4_reservoir_8018473C * (rsin((k << 10) + D_shelter_b4_reservoir_80187564[j + 1].phase + D_shelter_b4_reservoir_80187564[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_shelter_b4_reservoir_8018473C * (rsin(((j + 1) << 9) + D_shelter_b4_reservoir_80187514[k + 1].phase + D_shelter_b4_reservoir_80187514[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_shelter_b4_reservoir_8018473C * (rsin(((k + 1) << 10) + D_shelter_b4_reservoir_80187564[j + 1].phase + D_shelter_b4_reservoir_80187564[j + 1].offset) << 3);
                        p->y3  = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    p->u0 = u0;
                    p->v0 = v0;
                    p->u1 = u1;
                    p->v1 = v0;
                    p->u2 = u0;
                    p->v2 = v1;
                    p->u3 = u1;
                    p->v3 = v1;
                    addPrim(&gGpuCurrentOt[3], p);
                }
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
}

void func_shelter_b4_reservoir_8017DE8C(Task* task)
{
    switch (task->state) {
        case 0:
            gGameSession->eventState = 1;
            gGameSession->hideHud    = 1;
            task->state++;
            break;
        case 1:
            Gp_RunCapCmd(3, 0);
            D_80115690 = 1;
            D_80115680 = 5;
            task->state++;
            break;
        case 2:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 3:
            Gp_StateF0.field_4 = 0;
            if (Gp_GetCapEventKey() == 0xC) {
                taskKill(task);
                Mc_SaveData[0].state.at4.loc.view = 5;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Gp_MsgAllyWeapon(1);
                Gp_MsgAlly3F3(1);
                gGameSession->eventState = 0;
                gGameSession->hideHud    = 0;
                D_80114D08               = 0xA;
                break;
            }
            Gp_MsgSlot4Chain(0, 0);
            func_800E8634(D_shelter_b4_reservoir_80184948, 0, D_shelter_b4_reservoir_80184DC8);
            task->state++;
            break;
        case 4:
            if (gGameSession->eventState == 0) {
                Mc_SaveData[0].state.at4.loc.room = 2;
                gGameSession->at4.loc.room        = 2;
                gGameSession->roomObjsDirty       = 1;
                GameFlag_SetNibble(0xB7, 1);
                GameFlag_SetNibble(0x1BF, 2);
                GameFlag_SetNibble(0xB6, 1);
                GameFlag_SetNibble(0x1BE, 2);
                Gp_ApplyAreaRecs(D_shelter_b4_reservoir_801874A0);
                D_80114D08 = 0xA;
                taskKill(task);
            }
            break;
    }
}

static void func_shelter_b4_reservoir_8017E068(void)
{
    D_shelter_b4_reservoir_80187510 = (D_shelter_b4_reservoir_80184F78 << 0x18) | (D_shelter_b4_reservoir_80184F7A << 0xC) | (D_shelter_b4_reservoir_80184F79 << 0x10) | D_shelter_b4_reservoir_80184F7C.value;
}

void func_shelter_b4_reservoir_8017E0AC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(arg0->spawnArg1.value, 0);
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                Gp_StateF0.field_4 = 0;
                D_80114D08         = 0xA;
                break;
            }
            Gp_StateF0.field_4 = 1;
            Gp_TriggerPeIfArmed();
            D_shelter_b4_reservoir_80187500.field_0 = 0;
            D_shelter_b4_reservoir_80187500.field_1 = 0;
            D_shelter_b4_reservoir_80187500.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, &D_shelter_b4_reservoir_80187500);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                SndEvt_EnqueueType6(0x542D0001, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(0x542D0001) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b4_reservoir_80187508.field_2;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b4_reservoir_80187508.field_4;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_b4_reservoir_80187508.prefix.bytes.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_reservoir_8017E25C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E264(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_map_shelter_80179A04(src, dst);
    if (*(u16*)src == 0x2C) {
        if (GameFlag_GetNibble(0xB7) == 1) {
            if (src->field_5 == 0) {
                Gp_SetNibbleIf(src->field_6, 2);
                Gp_RunCapCmd1(2);
            }
        } else {
            if (src->field_5 == 0) {
                D_shelter_b4_reservoir_80187508.field_2              = dst->prefix.bytes.field_0;
                D_shelter_b4_reservoir_80187508.field_4              = dst->field_2;
                D_shelter_b4_reservoir_80187508.prefix.bytes.field_1 = dst->field_3;
                Task_SpawnFromTable(D_shelter_b4_reservoir_801848EC, 3, 0xB, 0);
            }
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_reservoir_8017E354(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 3) {
        Gp_MsgPlayer3F3(0);
        Gp_MsgAlly3F3(0);
        Gp_MsgPlayerWeapon(0);
        Gp_MsgAllyWeapon(0);
        Mc_SaveData[0].state.at4.loc.view = 6;
        Gp_StateF0.field_4                = 2;
        Task_SpawnFromTable(D_shelter_b4_reservoir_801848EC, 0, 0, 0);
    }
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3C4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3CC(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 2) {
        SndEvt_EnqueueType6(0x542D0000 | 2, 0, 0);
    }
    return 0;
}

void func_shelter_b4_reservoir_8017E400(Task* arg0)
{
    s16 temp_v1;
    s32 temp_v0;

    temp_v0                         = (s32)(arg0->killCountdown * 0x5DC) / (s32)arg0->spawnArg1.value;
    temp_v1                         = (u16)arg0->killCountdown + 1;
    arg0->killCountdown             = temp_v1;
    D_shelter_b4_reservoir_80184F80 = temp_v0 - 0x7D0;
    if (arg0->spawnArg1.value < temp_v1) {
        taskKill(arg0);
        D_shelter_b4_reservoir_8018492C = 0;
    }
}

void func_shelter_b4_reservoir_8017E4B0(Task* arg0)
{
    s16 temp_v1;
    s32 temp_v0;

    temp_v0                         = (s32) - (arg0->killCountdown * 0x708) / (s32)arg0->spawnArg1.value;
    temp_v1                         = (u16)arg0->killCountdown + 1;
    arg0->killCountdown             = temp_v1;
    D_shelter_b4_reservoir_80184F82 = (s16)temp_v0;
    if (arg0->spawnArg1.value < temp_v1) {
        taskKill(arg0);
        D_shelter_b4_reservoir_8018492C = 0;
    }
}

void func_shelter_b4_reservoir_8017E558(Task* arg0)
{
    TmdObject* obj   = arg0->extra.tmd;
    GfxCoord*  coord = obj->coords;

    if (arg0->state == 0) {
        coord->coord.t[0]   = -1000;
        coord->coord.t[1]   = -1000;
        coord->coord.t[2]   = -5000;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->state++;
    }
    if (arg0->state == 2) {
        coord->coord.t[0]   = -1000;
        coord->coord.t[1]   = -1000;
        coord->coord.t[2]   = -5000;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->state++;
    }
    if (arg0->state == 3) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[1]  += 4;
    }
    if (Mc_SaveData[0].state.at4.loc.view != 8) {
        obj->flags = 0x84;
    } else {
        obj->flags    = 0;
        obj->otOffset = 0;
    }
}

/// Event callback that runs the screen wave. Called with zero or less, it sets
/// the MDEC decode mode to 2, fills the wave's context (peak 0x60 reached in
/// one step, tinted 0x40/0x80/0x80) and spawns the wave task with it; called
/// with a positive value, it stores that value as the running wave's mode, so
/// 1 fades it out and 2 ends it.
void func_shelter_b4_reservoir_8017E610(s32 arg0)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    if (arg0 <= 0) {
        queue->field_22A                      = 2;
        D_shelter_b4_reservoir_80187624.span  = 1;
        D_shelter_b4_reservoir_80187624.scale = 0x60;
        D_shelter_b4_reservoir_80187624.r     = 0x40;
        D_shelter_b4_reservoir_80187624.blend = ANIMATION_BLEND_INTERPOLATE;
        D_shelter_b4_reservoir_80187624.g     = 0x80;
        D_shelter_b4_reservoir_80187624.b     = 0x80;
        Task_SpawnFromTable(D_shelter_b4_reservoir_80184724, 0, 0, &D_shelter_b4_reservoir_80187624);
        return;
    }
    D_shelter_b4_reservoir_80187624.state = arg0;
}

void func_shelter_b4_reservoir_8017E690(s32 arg0)
{
    switch (arg0) {
        case 0:
            D_shelter_b4_reservoir_8018492C = Task_SpawnFromTable(D_shelter_b4_reservoir_801848EC, 1, 0x96, 0);
            break;
        case 1:
            if (D_shelter_b4_reservoir_8018492C != 0) {
                taskKill(D_shelter_b4_reservoir_8018492C);
                D_shelter_b4_reservoir_8018492C = 0;
            }
            D_shelter_b4_reservoir_80184F80 = -0x1F4;
            break;
        case 2:
            D_shelter_b4_reservoir_8018492C = Task_SpawnFromTable(D_shelter_b4_reservoir_801848EC, 2, 0x96, 0);
            break;
        case 3:
            if (D_shelter_b4_reservoir_8018492C != 0) {
                taskKill(D_shelter_b4_reservoir_8018492C);
                D_shelter_b4_reservoir_8018492C = 0;
            }
            D_shelter_b4_reservoir_80184F82 = 0;
            break;
    }
}

void func_shelter_b4_reservoir_8017E770(s32 arg0)
{
    D_shelter_b4_reservoir_80184930->state = arg0;
}

void func_shelter_b4_reservoir_8017E780(s32 arg0)
{
    func_shelter_b4_reservoir_80182B04(10, arg0, 0x140);
}

/// Callback the room's event tables name: requests the 0x100 pulse from
/// `Gp_State1C`.
void func_shelter_b4_reservoir_8017E7A8(void)
{
    Gp_PulseState1C();
}

static void func_shelter_b4_reservoir_8017E7C8(Task* arg0)
{
    arg0->msgTable = D_shelter_b4_reservoir_801848BC;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b4_reservoir_80184F84, 0, 0, 0);
    if (GameFlag_GetNibble(0xB7) != 0) {
        D_shelter_b4_reservoir_80184F80 = -0x1F4;
    } else {
        D_shelter_b4_reservoir_80184F80 = -0x7D0;
    }
    D_shelter_b4_reservoir_80184930 = Task_SpawnFromTable(D_shelter_b4_reservoir_80184920, 0, 0, 0);
    arg0->state                     = (s32)(arg0->state + 1);
}

static void func_shelter_b4_reservoir_8017E864(Task* task)
{
    func_shelter_b4_reservoir_8017E068();
    func_shelter_b4_reservoir_8017E8E4();
}

/// Runs a task through the room's three-entry state table
/// `D_shelter_b4_reservoir_8017D5C4`, copied onto the stack first.
void func_shelter_b4_reservoir_8017E88C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_reservoir_8017D5C4;
    sp.funcs[task->state](task);
}

static void func_shelter_b4_reservoir_8017E8E4(void)
{
}

static void func_shelter_b4_reservoir_8017E8EC(Task* task)
{
    RoomWaterSurface* p = D_shelter_b4_reservoir_80184F90;

    if (Mc_SaveData[0].state.companionType == 0) {
        D_shelter_b4_reservoir_80187630 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_reservoir_80187630 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    if (gGameSession->at4.loc.view != 0xA) {
        p->depth = 0x2328 - (((D_shelter_b4_reservoir_80184F80 + 0x7D0) * 0x31) >> 5);
        func_shelter_b4_reservoir_8017EA00(task);
        func_shelter_b4_reservoir_8017EE04(task);
        func_shelter_b4_reservoir_8017F23C(task);
        return;
    }
    if (D_shelter_b4_reservoir_80184F82 < -0x708) {
        D_shelter_b4_reservoir_80184F82 = -0x708;
    } else if (D_shelter_b4_reservoir_80184F82 > 0) {
        D_shelter_b4_reservoir_80184F82 = 0;
    }
    func_shelter_b4_reservoir_8017F674(task);
}

/// Draws each surface in `D_shelter_b4_reservoir_80184F90` as a strip of 32
/// flat semi-transparent quads laid along Z, projected through the view
/// matrix, each followed by a draw-mode packet selecting blend mode 2. Quads
/// the projection flags as invalid are skipped. `task` is unused.
static void func_shelter_b4_reservoir_8017EA00(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    u8*               head;
    _SurfaceScratch*  s;
    RoomWaterSurface* e;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    e                          = D_shelter_b4_reservoir_80184F90;
    head                       = SCRATCH_HEAD(u8);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_HEAD(u8)           = head - 0xC;
    s                          = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_reservoir_80184F80;
    for (; e->count != -1; e++) {
        s->dx   = e->width;
        s->step = e->depth / 32;
        s->x    = e->x + D_shelter_b4_reservoir_80185020[0];
        s->z    = e->z;
        for (i = 0; i < 32; i++) {
            v0.vx = s->x;
            v0.vy = s->y;
            v0.vz = s->z + s->step * i;
            v1.vx = s->x;
            v1.vy = s->y;
            v1.vz = s->z + s->step * (i + 1);
            s->dy = 0;
            v2.vx = s->x + s->dx;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->step * i;
            s->dy = 0;
            v3.vx = s->x + s->dx;
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->step * (i + 1);
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r0              = 0x80;
                poly->g0              = 0;
                poly->b0              = 0;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                              = (DR_MODE*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// Same strip renderer as `func_shelter_b4_reservoir_8017EA00`, driven by
/// `D_shelter_b4_reservoir_80184FA8`: each surface's `field_8` gives its quad
/// count, and its X is used as stored rather than offset. `task` is unused.
static void func_shelter_b4_reservoir_8017EE04(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    u8*               head;
    _SurfaceScratch*  s;
    RoomWaterSurface* e;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    e                          = D_shelter_b4_reservoir_80184FA8;
    head                       = SCRATCH_HEAD(u8);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_HEAD(u8)           = head - 0xC;
    s                          = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_reservoir_80184F80;
    for (; e->count != -1; e++) {
        s->dx   = e->width;
        s->step = e->depth / e->count;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < e->count; i++) {
            v0.vx = s->x;
            v0.vy = s->y;
            v0.vz = s->z + s->step * i;
            v1.vx = s->x;
            v1.vy = s->y;
            v1.vz = s->z + s->step * (i + 1);
            s->dy = 0;
            v2.vx = s->x + s->dx;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->step * i;
            s->dy = 0;
            v3.vx = s->x + s->dx;
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->step * (i + 1);
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r0              = 0x80;
                poly->g0              = 0;
                poly->b0              = 0;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                              = (DR_MODE*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// Strip renderer like `func_shelter_b4_reservoir_8017EE04`, driven by
/// `D_shelter_b4_reservoir_80184FCC`, but laid along X instead of Z: each
/// surface's `field_4` is divided into `field_8` quads, and `field_6` is the
/// extent along Z. The scratch fields `dx` and `step` therefore hold the X step
/// and the Z extent here. `task` is unused.
static void func_shelter_b4_reservoir_8017F23C(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    u8*               head;
    _SurfaceScratch*  s;
    RoomWaterSurface* e;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    e                          = D_shelter_b4_reservoir_80184FCC;
    head                       = SCRATCH_HEAD(u8);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_HEAD(u8)           = head - 0xC;
    s                          = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_reservoir_80184F80;
    for (; e->count != -1; e++) {
        s->dx   = e->width / e->count;
        s->step = e->depth;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < e->count; i++) {
            v0.vx = s->x + s->dx * i;
            v0.vy = s->y;
            v0.vz = s->z;
            v1.vx = s->x + s->dx * (i + 1);
            v1.vy = s->y;
            v1.vz = s->z;
            s->dy = 0;
            v2.vx = s->x + s->dx * i;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->step;
            s->dy = 0;
            v3.vx = s->x + s->dx * (i + 1);
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->step;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r0              = 0x80;
                poly->g0              = 0;
                poly->b0              = 0;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                              = (DR_MODE*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// Strip renderer like `func_shelter_b4_reservoir_8017EE04`, driven by
/// `D_shelter_b4_reservoir_80184FE4` and drawn at height
/// `D_shelter_b4_reservoir_80184F82` instead of `D_shelter_b4_reservoir_80184F80`.
/// The quads are tinted by that height: blue is `-height * 16 / 225` and green
/// a quarter of it, so they brighten as the level sinks. `task` is unused.
static void func_shelter_b4_reservoir_8017F674(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    u8*               head;
    _SurfaceScratch*  s;
    RoomWaterSurface* e;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;
    u8                c;

    e                          = D_shelter_b4_reservoir_80184FE4;
    head                       = SCRATCH_HEAD(u8);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_HEAD(u8)           = head - 0xC;
    s                          = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_reservoir_80184F82;
    c                                   = -(D_shelter_b4_reservoir_80184F82 * 16) / 225;
    for (; e->count != -1; e++) {
        s->dx   = e->width;
        s->step = e->depth / e->count;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < e->count; i++) {
            v0.vx = s->x;
            v0.vy = s->y;
            v0.vz = s->z + s->step * i;
            v1.vx = s->x;
            v1.vy = s->y;
            v1.vz = s->z + s->step * (i + 1);
            s->dy = 0;
            v2.vx = s->x + s->dx;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->step * i;
            s->dy = 0;
            v3.vx = s->x + s->dx;
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->step * (i + 1);
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r0              = 0;
                poly->g0              = c >> 2;
                poly->b0              = c;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                              = (DR_MODE*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

void func_shelter_b4_reservoir_8017FADC(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_reservoir_8017FB44, func_shelter_b4_reservoir_8017E8EC };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_reservoir_80184F80;
}

/// First state of the water task: clears the session counter the current
/// display mode selects (`field_80` when `Mc_SaveData[0].state.companionType` is zero, `field_7E`
/// otherwise) and moves on to the per-frame state.
static void func_shelter_b4_reservoir_8017FB44(Task* arg0)
{
    if (Mc_SaveData[0].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b4_reservoir_8017FB84(Task* task)
{
    _ShelterB4ReservoirWork* work;
    Task*                    player;
    GfxCoord*                root;
    GfxCoord*                c;
    GfxCoord                 coord;
    s32                      i;
    s32                      offset;
    s32                      roll;

    work   = task->spawnArg2.pointer;
    player = gameGetPtrSlot(3);
    root   = player->extra.tmd->coords;
    if (task->state == 0) {
        D_8011574C  = 0x60172;
        D_80115738  = 0x60173;
        D_80115734  = 0x60225;
        D_80115730  = 0x60230;
        D_80115754  = 0x6023B;
        task->state = 1;
        for (i = 0; i < 10; i++) {
            work->field_24                        = RAND() & 0x1C0;
            work->field_26                        = (RAND() & 0x1FF) + (i << 9);
            D_shelter_b4_reservoir_80187634[i].vx = D_shelter_b4_reservoir_80185094.vx + 0x800;
            D_shelter_b4_reservoir_80187634[i].vy =
                D_shelter_b4_reservoir_80185094.vy + ((work->field_24 * rsin(work->field_26)) >> 12);
            D_shelter_b4_reservoir_80187634[i].vz =
                D_shelter_b4_reservoir_80185094.vz + ((work->field_24 * rcos(work->field_26)) >> 12);
        }
        D_shelter_b4_reservoir_80187684.field_0 = 0;
        D_shelter_b4_reservoir_80187684.field_2 = 0;
        D_shelter_b4_reservoir_80187684.field_4 = 0;
        for (i = 0; i < 2; i++) {
            c                                     = &player->extra.tmd->coords[i * 3 + 14];
            D_shelter_b4_reservoir_801850AC[i].vx = c->workm.t[0];
            D_shelter_b4_reservoir_801850AC[i].vy = c->workm.t[1];
            D_shelter_b4_reservoir_801850AC[i].vz = c->workm.t[2];
        }
    }
    if (Gp_State1C->eventState == 0) {
        if (GameFlag_GetNibble(0xB7) != 0) {
            if (gGameSession->waterY < root->coord.t[1] && work->field_22 != 0) {
                for (i = 0; i < 2; i++) {
                    c = &player->extra.tmd->coords[i * 3 + 14];
                    Gp_UpdateCoord(c);
                    work->field_26 = ABS(D_shelter_b4_reservoir_801850AC[i].vx - c->workm.t[0]) +
                                     ABS(D_shelter_b4_reservoir_801850AC[i].vy - c->workm.t[1]) +
                                     ABS(D_shelter_b4_reservoir_801850AC[i].vz - c->workm.t[2]) + 0x20;
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &c->workm, &coord.coord);
                    coord.parent       = &gGfxViewCoord;
                    coord.coord.t[1]   = gGameSession->waterY;
                    coord.composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(&coord);
                    if ((s32)(RAND() & 0x1FF) < work->field_26) {
                        Gp_SpawnEff(D_8011574C, &coord, 0x40, NULL);
                    }
                    work->field_26 -= 0x20;
                    if ((s32)(RAND() & 0x1FF) < work->field_26) {
                        Gp_SpawnEff(D_80115738, &coord, 0x1202180, NULL);
                    }
                    D_shelter_b4_reservoir_801850AC[i].vx = c->workm.t[0];
                    D_shelter_b4_reservoir_801850AC[i].vy = c->workm.t[1];
                    D_shelter_b4_reservoir_801850AC[i].vz = c->workm.t[2];
                }
            }
            work->field_22++;
        }
        if (D_shelter_b4_reservoir_80187684.field_0 != 0 && D_shelter_b4_reservoir_80187684.field_2 != 0) {
            for (i = 0; i < D_shelter_b4_reservoir_80187684.field_0; i++) {
                if ((RAND() & 0x1F) == 0) {
                    work->field_24                        = RAND() & 0x1C0;
                    work->field_26                        = (RAND() & 0x1FF) + (i << 9);
                    D_shelter_b4_reservoir_80187634[i].vx = D_shelter_b4_reservoir_80185094.vx + 0x800;
                    D_shelter_b4_reservoir_80187634[i].vy =
                        D_shelter_b4_reservoir_80185094.vy + ((work->field_24 * rsin(work->field_26)) >> 12);
                    D_shelter_b4_reservoir_80187634[i].vz =
                        D_shelter_b4_reservoir_80185094.vz + ((work->field_24 * rcos(work->field_26)) >> 12);
                }
                if ((u16)(RAND() % 100) < D_shelter_b4_reservoir_80187684.field_2) {
                    offset = D_shelter_b4_reservoir_80187684.field_4;
                    roll   = (RAND() & 0x10FF) + 0x502000;
                    Gp_SpawnEff(0x600AA, NULL, offset + roll, &D_shelter_b4_reservoir_80187634[i]);
                }
            }
        }
    }
    if ((u8)Gp_GetViewIndex() == 10) {
        D_shelter_b4_reservoir_8018509C[1].vy = D_shelter_b4_reservoir_80184F82;
        if ((RAND() & 1) == 0) {
            Gp_SpawnEff(D_80115738, NULL, (RAND() & 0x1000) + 0x4A03600, &D_shelter_b4_reservoir_8018509C[0]);
        }
        if ((RAND() & 1) == 0) {
            Gp_SpawnEff(D_80115738, NULL, (RAND() & 0x10FF) | 0x11602300, &D_shelter_b4_reservoir_8018509C[1]);
        }
        if ((RAND() & 3) == 0) {
            Gp_SpawnEff(D_8011574C, NULL, (RAND() & 0x7F) | 0x80, &D_shelter_b4_reservoir_8018509C[1]);
        }
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 2:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[10], 0x200, 0x444);
            break;
        case 4:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x222);
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[6], 0x200, 0x444);
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[8], 0x200, 0x333);
            if (GameFlag_GetNibble(0xB7) != 0) {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 5:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x444);
            if (GameFlag_GetNibble(0xB7) != 0) {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 6:
            if (GameFlag_GetNibble(0xB7) != 0) {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 7:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[2], 0x200, 0x444);
            break;
        case 3:
        case 9:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[4], 0x200, 0x444);
            break;
    }
}

/// Effect task drawing a flat quad through `func_shelter_b4_reservoir_80180530`
/// that spreads and fades. Its first frame sets the size (`angle`) from the
/// low 12 bits of the spawn argument and the brightness (`scale`) to 0x40,
/// and turns the coordinate to a random angle about Y. Every frame after that
/// grows the size by 0x20 and dims the brightness by 2, releasing the effect
/// once the brightness drops under 2. While `Gp_State1C->eventState` is
/// non-zero it only redraws at the current values, releasing from state 4.
void func_shelter_b4_reservoir_801803DC(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_reservoir_80180530(coord, work->angle, work->scale);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        if (task->state == 0) {
            work->scale = 0x40;
            work->angle = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
        }
        work->angle += 0x20;
        func_shelter_b4_reservoir_80180530(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat textured quad at `arg0`: the four corners of the unit quad
/// `D_80111E38`, scaled by `arg1`, are rotated by the coordinate's world
/// matrix and offset by its translation, then projected through `GsWSMATRIX`.
/// If the projection is valid, one semi-transparent `POLY_FT4` (tpage 0x2B,
/// clut 0x43D1, UV 0,0x38 to 0x37,0x6F) is queued with all three colour
/// channels set to `arg2`. The work block lives on the scratchpad stack.
static void func_shelter_b4_reservoir_80180530(GfxCoord* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        tbl   = &D_80111E38[i];
        v     = &block->vec[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D1;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        setRGB0(prim, arg2, arg2, arg2);
        prim->u0 = 0;
        prim->u1 = 0x37;
        prim->u2 = 0;
        prim->v2 = 0x6F;
        prim->u3 = 0x37;
        prim->v3 = 0x6F;
        setSemiTrans(prim, 1);
        prim->x0 = block->sxy0.vx;
        prim->y0 = block->sxy0.vy;
        prim->x1 = block->sxy1.vx;
        prim->y1 = block->sxy1.vy;
        prim->x2 = block->sxy2.vx;
        prim->y2 = block->sxy2.vy;
        prim->x3 = block->sxy3.vx;
        prim->y3 = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Particle effect task, drawn as the spinning sprite of
/// `func_shelter_b4_reservoir_80180D20` or, when the spawn argument's top
/// nibble is set, the standing sprite of `func_shelter_b4_reservoir_8018110C`.
/// Its first frame takes the size from the argument's low 12 bits, a random
/// angle, and the ticks per animation frame from bits 12-15. Unless the work
/// already carries a velocity it picks one by the kind in bits 24-27 - none,
/// a random upward burst, a random spray, a narrow upward jet, or the work's
/// stored direction - scaled to the speed in bits 16-23 (0x40 when zero).
/// Every later tick draws, moves the coordinate by the velocity with gravity
/// pulling it down, and releases the effect after animation frame 7. While
/// `Gp_State1C->eventState` is non-zero it only draws, releasing from state 4.
void func_shelter_b4_reservoir_80180864(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b4_reservoir_80180D20(coord, work->index, work->scale, work->angle);
            } else {
                func_shelter_b4_reservoir_8018110C(coord, work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1.value & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = task->spawnArg1.signedBytes[3];
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_shelter_b4_reservoir_80180D20(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b4_reservoir_8018110C(coord, work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning sprite at the coordinate's world position: one
/// semi-transparent textured quad (tpage 0x2B, clut 0x43D3) centred on the
/// projected point, unless the projection flags an error. `arg1` picks the
/// 32-texel-wide frame at U `arg1 * 32` in the strip at V 0xE0..0xFF, `arg2` is
/// the size (a screen half-extent of `arg2 * 31 / otz`) and `arg3` the angle
/// the corners are turned by.
static void func_shelter_b4_reservoir_80180D20(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (s16)arg1 << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws a sprite at the coordinate's world position: one semi-transparent
/// textured quad (tpage 0x2B, clut 0x43D2) around the projected point, unless
/// the projection flags an error. `arg1` picks one of eight 56-texel frames,
/// four across and two down from V 0x70. `arg2` is the size, a screen
/// half-extent of `arg2 * 55 / otz`; the quad stands on the point, reaching one
/// and a half extents above it and half an extent below.
static void func_shelter_b4_reservoir_8018110C(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpRingScratch* block;
    POLY_FT4*      prim;

    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        setUVWH(prim, (arg1 % 4) * 0x38, (arg1 % 8) / 4 * 0x38 + 0x70, 0x37, 0x37);
        block->step = (arg2 * 0x37) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->step;
        prim->x1 = prim->x3 = block->sx + block->step;
        prim->y0 = prim->y1 = block->sy - block->step - (block->step >> 1);
        prim->y2 = prim->y3 = block->sy + (block->step >> 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

void func_shelter_b4_reservoir_801813F0(Task* task)
{
    GpEffWork* work  = task->spawnArg2.pointer;
    GfxCoord*  coord = task->extra.tmd->coords;
    s16        f2a;
    u32        rng;

    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_reservoir_80181668(coord, work->index, work->scale);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.value & 0xFFF;

            if (task->spawnArg1.value & 0xF000) {
                work->period = (task->spawnArg1.value >> 12) & 0x7;
            } else {
                work->period = 1;
            }

            work->age   = 0;
            task->state = 1;

            if (task->spawnArg1.value & 0xFF0000) {
                f2a = (task->spawnArg1.value >> 16) & 0xFF;
            } else {
                f2a = 0x40;
            }

            work->step    = f2a;
            work->move.vy = 0;
            work->move.vz = 0;
            rng           = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState   = rng;
            work->move.vx = -((rng >> 16) & 0x3F) - 0x40;
            VectorNormalSS(&work->move, &work->move);

            gte_lddp(work->step);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&work->move);
            break;
        case 1:
            func_shelter_b4_reservoir_80181668(coord, work->index, work->scale);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 6) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x4393). `frame` selects one of six 32-texel
/// UV columns at u = `(frame % 6) * 32 + 0x40`, v = 0x40..0x5F. `size` is a
/// half-extent; the on-screen radius is `size * 31 / otz`, and the quad is
/// axis-aligned about the projected point.
static void func_shelter_b4_reservoir_80181668(GfxCoord* coord, u16 frame, s16 size)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            col;
    s16            xy;
    u16            vz;

    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)coord->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)coord->workm.t[1];
    vz                                      = (u16)coord->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x4393;
        prim->v0    = 0x40;
        prim->v1    = 0x40;
        prim->v2    = 0x5F;
        prim->v3    = 0x5F;
        col         = (u16)(frame % 6) << 5;
        prim->u0    = col + 0x40;
        prim->u2    = col + 0x40;
        prim->u1    = col + 0x5F;
        prim->u3    = col + 0x5F;
        block->step = (size * 31) / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = (u16)block->sy - (u16)block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (u16)block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn unless both project.
/// Each end is a half-disc of screen radius `arg1 * 64 / otz` and the two are
/// joined by a band, built from gouraud quads lit at the centre line and black
/// at the rim, in two 0x400 steps around the angle between the projected
/// points. `arg2` is the colour as three 4-bit channels (0xRGB), brightened
/// slightly on odd frames.
static void func_shelter_b4_reservoir_801818F0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      conn;
    u8                       r;
    u8                       g;
    u8                       b;

    p1       = arg0 + 1;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / block->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a star-shaped glow at the world point `arg0`, projected through
/// `gGfxViewCoord.workm`, unless the projection flags an error. A disc of screen
/// radius `arg1 * 64 / otz` is built from gouraud wedges lit at the centre and
/// black at the rim, alternating half brightness at full radius with full
/// brightness at half radius; four spikes at half brightness reach out
/// between them. `arg2` is the colour as three 4-bit channels (0xRGB), and its
/// top nibble is the shift of a small brightening applied on odd frames.
static void func_shelter_b4_reservoir_80182134(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                ua;
    s32                ub;
    s32                uc;
    s32                frame;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;
    s32                outer;
    s32                inner;
    s32                hr;
    s32                hg;
    s32                hb;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = SCRATCH_PUSH_BYTES_AT(scratch, 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1        <<= 16;
        arg1        >>= 16;
        outer         = (arg1 * 64) / block->otz;
        frame         = gDisplayState.animFrame;
        block->rOuter = outer;
        inner         = (arg1 * 8) / block->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (frame & 1) << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = inner;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            hr = (u8)r >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            hg = (u8)g >> 1;
            hb = (u8)b >> 1;
            setRGB2(prim, hr, hg, hb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        r   = (u8)hr;
        g   = (u8)hg;
        b   = (u8)hb;
        do {
            ua             = ang - 0x400;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            ang      = uc;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}

static void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2)
{
    D_shelter_b4_reservoir_80187684.field_0 = arg0;
    D_shelter_b4_reservoir_80187684.field_2 = arg1;
    D_shelter_b4_reservoir_80187684.field_4 = arg2;
}

/// Effect task of a glowing disc attached to its parent at the work's
/// position. State 1 grows the disc and, every fourth tick, spawns effect
/// `D_80115730` at a random joint of the player's model; state 2 adds a
/// flickering second disc at half brightness; state 3 drifts the disc away,
/// shrinking and fading it inside an expanding ring, then releases the effect.
/// The spawn argument picks the disc's colour shifts from
/// `D_shelter_b4_reservoir_801850BC`.
void func_shelter_b4_reservoir_80182B1C(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->parent                    = mem->parent;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = mem->pos.vx;
            coord->coord.t[1]                = mem->pos.vy;
            coord->coord.t[2]                = mem->pos.vz;
            coord->composeStamp              = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &player->extra.tmd->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1.value][0];
            col[1] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1.value][1];
            col[2] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1.value][2];
            func_shelter_b4_reservoir_80183940(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1.value][0];
            col[1] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1.value][1];
            col[2] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1.value][2];
            func_shelter_b4_reservoir_80183940(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b4_reservoir_80183940(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1.value][0];
            col[1] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1.value][1];
            col[2] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1.value][2];
            func_shelter_b4_reservoir_80183940(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_shelter_b4_reservoir_8018351C(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Effect task that drifts toward the coordinate in `spawnArg1`. Its first
/// frame turns the world-space offset to that coordinate into the effect's
/// own frame and keeps 0xCC/0x1000 of it as the per-frame step. Every frame
/// after that moves by the step and, on odd ticks, draws the next frame of
/// `func_shelter_b4_reservoir_80183298`, releasing the effect at tick 20. While
/// `Gp_State1C->eventState` is non-zero it does nothing but release from state
/// 4.
void func_shelter_b4_reservoir_80183074(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    GfxCoord*  target;
    VECTOR     delta;

    work   = task->spawnArg2.pointer;
    coord  = task->extra.tmd->coords;
    target = task->spawnArg1.pointer;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0]  += work->pos.vx;
                coord->coord.t[1]  += work->pos.vy;
                coord->coord.t[2]  += work->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_shelter_b4_reservoir_80183298(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
static void func_shelter_b4_reservoir_80183298(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex                                     = arg1;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = arg0->workm.t[1];
    vz                                      = arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        setRGB0(prim, arg3, arg3, arg3);
        setUVWH(prim, t + 0x60, 0, 0x17, 0x17);
        sarg        = (s16)arg2;
        t           = sarg * 24;
        block->step = (t - sarg) / block->otz;
        xy          = block->sx - block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = block->sx + block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = block->sy - block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = block->sy + block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Draws a glowing ring around the coordinate's projected position, unless the
/// projection flags an error: sixteen gouraud quads, black at screen radius
/// `arg1 * 64 / (otz + 1)` and coloured `rgb` at
/// `(arg1 + arg2) * 64 / (otz + 1)`.
static void func_shelter_b4_reservoir_8018351C(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           outer;

    block         = SCRATCH_PUSH(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpArcScratch);
}

/// Draws a glowing disc at the coordinate's world position, unless the
/// projection flags an error: eight gouraud wedges coloured `rgb` at the
/// projected centre and black at the rim, of screen radius
/// `arg1 * 64 / (otz + 1)`.
static void func_shelter_b4_reservoir_80183940(GfxCoord* arg0, s32 arg1, u8* rgb)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;

    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->step * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->step * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->step * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->step * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Burst effect task: each frame draws a disc and the glow of
/// `func_shelter_b4_reservoir_80183E80` at a growing size, and while its echo
/// brightness lasts a widening ring fading out behind them. Once the echo is
/// spent the main brightness runs down, and the effect is released when it
/// falls under 0x18.
void func_shelter_b4_reservoir_80183CD4(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_shelter_b4_reservoir_80183940(coord, (s16)(step * 2), rgb);
        func_shelter_b4_reservoir_80183E80(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b4_reservoir_8018351C(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus a flat quad on the ground beneath it.
/// It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void func_shelter_b4_reservoir_80183E80(GfxCoord* coord, s16 size)
{
    GfxCoord       ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;

    slot                                  = &Gp_RoomCoords[2];
    slot->framesLeft                      = 2;
    light                                 = &slot->light;
    light->inner                          = 0x300;
    light->outer                          = 0x3000;
    random                                = (Gp_LcgState * 5) + 0x71357911;
    intensity                             = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r                         = intensity;
    shifted                               = intensity << 0x10;
    light->head.g                         = shifted >> 0x11;
    light->head.b                         = shifted >> 0x12;
    light->head.u.at.local.t[0]           = coord->coord.t[0];
    light->head.u.at.local.t[1]           = coord->coord.t[1];
    light->head.u.at.local.t[2]           = coord->coord.t[2];
    slot->light.head.u.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_LcgState                           = random;
    block                                 = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx                         = coord->workm.t[0];
    block->vec.vy                         = coord->workm.t[1];
    block->vec.vz                         = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->step = size * 0x37 / block->otz;
        left        = block->sx - block->step;
        prim->x2    = left;
        prim->x0    = left;
        right       = block->sx + block->step;
        prim->x3    = right;
        prim->x1    = right;
        top         = block->sy - block->step;
        prim->y1    = top;
        prim->y0    = top;
        bottom      = block->sy + block->step;
        prim->y3    = bottom;
        prim->y2    = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)(size * 3 / 2);
        block->step = outerSize * 0x37 / block->otz;
        outerLeft   = block->sx - block->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = block->sx + block->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = block->sy - block->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = block->sy + block->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b4_reservoir_801843AC(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b4_reservoir_801843AC(GfxCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        v     = &block->vec[i];
        tbl   = &D_80111E38[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}
