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
#include "../../shared/room_visual_effects.h"
#include "../../shared/screen_wave.h"
#include "../../shared/glow_draw.h"
#include "../../shared/water_effects.h"

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
extern s32 gScreenWaveRamp;

/// The ramp and tint the wave task was spawned with.
extern OverlayWaveCtx* gScreenWaveCtx;

/// Phase records of the wave's 11 column edges and 30 row edges.
extern OverlayWaveRec6 gScreenWaveColumns[13];
extern OverlayWaveRec6 gScreenWaveRows[32];

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
extern ScreenFade               D_shelter_b4_reservoir_80187500;
extern RoomEventMsg             D_shelter_b4_reservoir_80187508;
extern s32                      D_shelter_b4_reservoir_80187510;
extern u8*                      D_shelter_b4_reservoir_80187630;
extern SVECTOR                  D_shelter_b4_reservoir_80187634[];
extern _ShelterB4ReservoirBurst D_shelter_b4_reservoir_80187684;

static void func_shelter_b4_reservoir_8017E7C8(Task* arg0);
static void func_shelter_b4_reservoir_8017E864(Task* task);
static void func_shelter_b4_reservoir_8017E8E4(void);
static void func_shelter_b4_reservoir_8017EA00(Task* task);
static void func_shelter_b4_reservoir_8017EE04(Task* task);
static void func_shelter_b4_reservoir_8017F23C(Task* task);
static void func_shelter_b4_reservoir_8017F674(Task* task);
static void func_shelter_b4_reservoir_8017FB44(Task* arg0);
static void func_shelter_b4_reservoir_80181668(GfxCoord* coord, u16 frame, s16 size);
static void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2);

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

s32  func_shelter_b4_reservoir_8017E25C(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b4_reservoir_8017E264(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b4_reservoir_8017E354(Task*, s32, s32, TaskMessageArg);
s32  func_shelter_b4_reservoir_8017E3C4(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b4_reservoir_8017E3CC(Task*, s32, s32, TaskMessageArg);
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
    { 0, 192, screenWaveTask, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

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
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b4_reservoir_8017E264 },
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

ActorCommand D_shelter_b4_reservoir_8018491C = { { .loc = { 4, 45 } }, 6 };

TaskDesc D_shelter_b4_reservoir_80184920[1] = {
    { (TASK_BODY_TMD | 0x100), 32, func_shelter_b4_reservoir_8017E558, { .model = &D_shelter_b4_reservoir_80184898 } },
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
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b4_reservoir_8018491C } }, { .value = 0 } },
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
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b4_reservoir_8018491C } }, { .value = 0 } },
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

#include "../../shared/room_visual_effects_disc_data.inc.c"

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

SpriteBatch D_shelter_b4_reservoir_80185C44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_reservoir_80185C54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_reservoir_80185C64[11] = {
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

SpriteBatch D_shelter_b4_reservoir_80185D40[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_reservoir_80185D58[4] = {
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -64, 2000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 8, -8, 1500, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 16, -32, 1525, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, -64, 1750, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_reservoir_80185DA8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_reservoir_80185DC0[58] = {
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

SpriteBatch D_shelter_b4_reservoir_80186248[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 45, 0, 0, { 1, 0 } },
    { 45, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_reservoir_80186268[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_reservoir_80186278[9] = {
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

SpriteBatch D_shelter_b4_reservoir_8018632C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_reservoir_80186344[36] = {
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

SpriteBatch D_shelter_b4_reservoir_80186614[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 36, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_reservoir_8018662C[11] = {
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

SpriteBatch D_shelter_b4_reservoir_80186708[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_reservoir_80186720[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
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

WorldCoordPointLight D_shelter_b4_reservoir_801867A8[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -4450, 5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -4450, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2520, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2520, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2581, 4061 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2520, 4000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -4450, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 5367, 6365 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -3000, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1228, 1228, 1228 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4500, -2000, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1500, 2500 },
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
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_reservoir_801871F8[4] = {
    { 44, 44, 0, 0, { 0, 0 }, &D_80142604 },
    { 72, 72, 1, 0, { 0, 0 }, D_80153EC8 },
    { 73, 73, 1, 0, { 0, 0 }, D_8014E7A4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_reservoir_80187228[4] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 70, 70, 1, 0, { 0, 0 }, &D_801575F0 },
    { 71, 71, 1, 0, { 0, 0 }, &D_80151E60 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_reservoir_80187258[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
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

ScreenFade D_shelter_b4_reservoir_80187500 = { 0 };

OverlayWaveCtx* gScreenWaveCtx = NULL;

RoomEventMsg D_shelter_b4_reservoir_80187508 = { 0 };

s32 D_shelter_b4_reservoir_80187510 = 0;

OverlayWaveRec6 gScreenWaveColumns[13] = { 0 };

OverlayWaveRec6 gScreenWaveRows[32] = { 0 };

OverlayWaveCtx D_shelter_b4_reservoir_80187624 = { 0 };

u8* D_shelter_b4_reservoir_80187630 = NULL;

SVECTOR D_shelter_b4_reservoir_80187634[10] = { 0 };

_ShelterB4ReservoirBurst D_shelter_b4_reservoir_80187684 = { 0, 0, 0 };

static void func_shelter_b4_reservoir_8017E068(void);
static void func_shelter_b4_reservoir_8017E8EC(Task* task);

#include "../../shared/screen_wave.inc.c"

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
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 5;
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
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
                gGameSession->location.loc.room                            = 2;
                gGameSession->roomObjsDirty                                = 1;
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
            D_shelter_b4_reservoir_80187500.blend      = SCREEN_FADE_SUBTRACT;
            D_shelter_b4_reservoir_80187500.phase      = SCREEN_FADE_RUNNING;
            D_shelter_b4_reservoir_80187500.rampFrames = 0x1E;
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
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_shelter_b4_reservoir_80187508.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_shelter_b4_reservoir_80187508.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_shelter_b4_reservoir_80187508.areaId)[1];
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_reservoir_8017E25C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E264(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    func_map_shelter_80179A04(src, dst);
    if (src->areaId == 0x2C) {
        if (GameFlag_GetNibble(0xB7) == 1) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(src->flagId, 2);
                Gp_RunCapCmd1(2);
            }
        } else {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                D_shelter_b4_reservoir_80187508.warp              = (u8)dst->areaId;
                D_shelter_b4_reservoir_80187508.field_4           = dst->warp;
                ((u8*)&D_shelter_b4_reservoir_80187508.areaId)[1] = dst->room;
                Task_SpawnFromTable(D_shelter_b4_reservoir_801848EC, 3, 0xB, 0);
            }
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_reservoir_8017E354(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    if (arg2 == 3) {
        Gp_MsgPlayer3F3(0);
        Gp_MsgAlly3F3(0);
        Gp_MsgPlayerWeapon(0);
        Gp_MsgAllyWeapon(0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
        Gp_StateF0.field_4                                         = 2;
        Task_SpawnFromTable(D_shelter_b4_reservoir_801848EC, 0, 0, 0);
    }
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3C4(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3CC(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
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
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != 8) {
        obj->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
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
    CdCmdQueue* queue = &gCdCmdQueue;

    if (arg0 <= 0) {
        queue->imageMdecMode                  = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
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

/// Callback the room's event tables name: requests all-effect cancellation
/// (`ROOM_EFFECT_CANCEL_ALL`) on `gRoomEffectState`.
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

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        D_shelter_b4_reservoir_80187630 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_reservoir_80187630 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    if (gGameSession->location.loc.view != 0xA) {
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
    head                       = SCRATCH_STACK_CURSOR(u8);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_CURSOR(u8)   = head - 0xC;
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
    SCRATCH_STACK_RELEASE_BYTES(0xC);
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
    head                       = SCRATCH_STACK_CURSOR(u8);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_CURSOR(u8)   = head - 0xC;
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
    SCRATCH_STACK_RELEASE_BYTES(0xC);
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
    head                       = SCRATCH_STACK_CURSOR(u8);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_CURSOR(u8)   = head - 0xC;
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
    SCRATCH_STACK_RELEASE_BYTES(0xC);
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
    head                       = SCRATCH_STACK_CURSOR(u8);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_CURSOR(u8)   = head - 0xC;
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
    SCRATCH_STACK_RELEASE_BYTES(0xC);
}

void func_shelter_b4_reservoir_8017FADC(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_reservoir_8017FB44, func_shelter_b4_reservoir_8017E8EC };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_reservoir_80184F80;
}

/// First state of the water task: clears the session counter the current
/// display mode selects (`field_80` when `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` is zero, `field_7E`
/// otherwise) and moves on to the per-frame state.
static void func_shelter_b4_reservoir_8017FB44(Task* arg0)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
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
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
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
            glowDrawCapsule(&D_shelter_b4_reservoir_80185024[10], 0x200, 0x444);
            break;
        case 4:
            glowDrawCapsule(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b4_reservoir_80185024[6], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b4_reservoir_80185024[8], 0x200, 0x333);
            if (GameFlag_GetNibble(0xB7) != 0) {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 5:
            glowDrawCapsule(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x444);
            if (GameFlag_GetNibble(0xB7) != 0) {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 6:
            if (GameFlag_GetNibble(0xB7) != 0) {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 7:
            glowDrawCapsule(&D_shelter_b4_reservoir_80185024[2], 0x200, 0x444);
            break;
        case 3:
        case 9:
            glowDrawCapsule(&D_shelter_b4_reservoir_80185024[4], 0x200, 0x444);
            break;
    }
}

#include "../../shared/water_ripple_task.inc.c"

void func_shelter_b4_reservoir_801803DC(Task* task)
{
    waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task.inc.c"

void func_shelter_b4_reservoir_80180864(Task* task)
{
    waterDriftTask(task);
}

#include "../../shared/water_spin.inc.c"

#include "../../shared/water_tile.inc.c"

void func_shelter_b4_reservoir_801813F0(Task* task)
{
    GpEffWork* work  = task->spawnArg2.pointer;
    GfxCoord*  coord = task->extra.coordBody->coord;
    s16        f2a;
    u32        rng;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        func_shelter_b4_reservoir_80181668(coord, work->index, work->scale);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
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

    scratch                                 = SCRATCH_STACK_CURSOR_SLOT;
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
        prim           = gGpuPrimCursor;
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

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_tinted_disc_no_bias.inc.c"

static void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2)
{
    D_shelter_b4_reservoir_80187684.field_0 = arg0;
    D_shelter_b4_reservoir_80187684.field_2 = arg1;
    D_shelter_b4_reservoir_80187684.field_4 = arg2;
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_shelter_b4_reservoir_80182B1C(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b4_reservoir_80183074(Task* task)
{
    RoomFx_FlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b4_reservoir_80183CD4(Task* arg0)
{
    RoomFx_OrangeBurst2Task(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
