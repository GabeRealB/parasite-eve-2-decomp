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
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/random.h"
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

static void _waterDrawSpin(const GfxCoord* coord, s16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTile(const GfxCoord* coord, s16 frameIndex, s16 radiusScale);

#define RAND() ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16)

extern SVECTOR D_shelter_b4_reservoir_80185024[14];

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s16 D_shelter_b4_reservoir_80185020[2];

enum {
    SHELTER_B4_RESERVOIR_BURST_CHANCE_SCALE     = 100,
    SHELTER_B4_RESERVOIR_BURST_BASE_HALF_EXTENT = 320,
    // Packed sprite argument: movement step 80 and a two-frame animation period.
    SHELTER_B4_RESERVOIR_BURST_MOTION_ARGS = (80 << 16) | (2 << 12),
    // Adds 0..255 to the half-extent and 0..1 to the animation period.
    SHELTER_B4_RESERVOIR_BURST_VARIATION_MASK = (1 << 12) | 0xFF,
};

/// Configuration of the reservoir's per-frame burst-sprite emitter.
///
/// Cleared when the room effect task starts and retained until reconfigured.
/// A zero point count or spawn chance disables emission. The point count must
/// fit the ten-point position table; it is not clamped. The base half-extent
/// plus 0..255 random size variation must fit the spawn argument's low 12 bits
/// so it does not carry into the animation period.
typedef struct {
    u16 pointCount;         // Active emission points (0..10)
    u16 spawnChancePercent; // Per-point, per-frame roll threshold out of 100 (0 disabled, >=100 always)
    u16 baseHalfExtent;     // Base sprite half-extent in world units, before random size variation
} _ShelterB4ReservoirBurstConfig;
STATIC_ASSERT_SIZEOF(_ShelterB4ReservoirBurstConfig, 6);

/// Spawn table of the screen-wave task, and the context it is spawned with.
/// The context's mode word is written through its own symbol, which is how the
/// original reached it.
extern TaskDesc      gScreenWaveTaskDesc[];
extern ScreenWaveCtx gScreenWaveSpawnCtx;

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 gScreenWaveRamp;

/// The ramp and tint the wave task was spawned with.
extern ScreenWaveCtx* gScreenWaveCtx;

/// Phase records of the wave's 11 column edges and 30 row edges.
extern ScreenWaveOscillator gScreenWaveColumns[13];
extern ScreenWaveOscillator gScreenWaveRows[32];

extern TaskMessageEntry D_shelter_b4_reservoir_801848BC[];
extern TaskDesc         D_shelter_b4_reservoir_801848EC[];
extern TaskDesc         D_shelter_b4_reservoir_80184920[];
extern Task*            D_shelter_b4_reservoir_8018492C;
extern Task*            D_shelter_b4_reservoir_80184930;
extern EvsCommand       D_shelter_b4_reservoir_80184948[];
extern EvsCommand       D_shelter_b4_reservoir_80184DC8[];
extern u8               D_shelter_b4_reservoir_80184F78;
extern u8               D_shelter_b4_reservoir_80184F79;
extern u8               D_shelter_b4_reservoir_80184F7A;
/// Half-extent of a water-drift sprite, plus two trailing bytes.
///
/// Each frame the room task ORs `halfExtent` into the low half of a spawn
/// argument and fills the other fields from the three bytes that precede this
/// object. `waterDriftTask` reads that layout: bits 0..11 are this half-extent,
/// bits 12..15 the frame period, bits 16..23 the velocity level and bits 24..31
/// the velocity kind. The initial half-extent is 640, so it occupies only the
/// low 12 bits. The trailing bytes are zero; their role is unproven.
typedef struct {
    s16 halfExtent; // Sprite half-extent in world units, bits 0..11 of the spawn argument
    u8  field_2[2]; // Zero bytes with no accesses; role unproven
} _ShelterB4ReservoirWaterDriftHalfExtent;
STATIC_ASSERT_SIZEOF(_ShelterB4ReservoirWaterDriftHalfExtent, 4);

extern _ShelterB4ReservoirWaterDriftHalfExtent D_shelter_b4_reservoir_80184F7C;

extern s16                            D_shelter_b4_reservoir_80184F82;
extern TaskDesc                       D_shelter_b4_reservoir_80184F84[];
extern RoomWaterSurface               D_shelter_b4_reservoir_80184F90[];
extern RoomWaterSurface               D_shelter_b4_reservoir_80184FA8[];
extern RoomWaterSurface               D_shelter_b4_reservoir_80184FCC[];
extern RoomWaterSurface               D_shelter_b4_reservoir_80184FE4[];
extern SVECTOR                        D_shelter_b4_reservoir_80185094;
extern SVECTOR                        D_shelter_b4_reservoir_8018509C[];
extern SVECTOR                        D_shelter_b4_reservoir_801850AC[];
extern AreaApplyRec                   D_shelter_b4_reservoir_801874A0[];
extern ScreenFade                     D_shelter_b4_reservoir_80187500;
extern RoomEventMsg                   D_shelter_b4_reservoir_80187508;
extern s32                            D_shelter_b4_reservoir_80187510;
extern u8*                            D_shelter_b4_reservoir_80187630;
extern SVECTOR                        D_shelter_b4_reservoir_80187634[];
extern _ShelterB4ReservoirBurstConfig D_shelter_b4_reservoir_80187684;

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

extern WorldCollisionGrid     D_shelter_b4_reservoir_80185AB8[1];
extern WorldCollisionOccluder D_shelter_b4_reservoir_801873B0[2];
extern WorldCollisionTrigger  D_shelter_b4_reservoir_80186AC0[8];
extern WorldCollisionTrigger  D_shelter_b4_reservoir_80186D20[7];
extern WorldCollisionTrigger  D_shelter_b4_reservoir_80186F34[9];
extern WorldCoordRoomLights   D_shelter_b4_reservoir_80186AA8[1];
extern TaskDesc               Actor04400_D107E4;
extern TaskDesc               D_actor_100400_80147E48;
extern TaskDesc               D_actor_207000_801575F0;

s32  func_shelter_b4_reservoir_8017E25C(Task*, s32, s32, s32);
s32  func_shelter_b4_reservoir_8017E264(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b4_reservoir_8017E354(Task*, s32, s32, s32);
s32  func_shelter_b4_reservoir_8017E3C4(Task*, s32, s32, s32);
s32  func_shelter_b4_reservoir_8017E3CC(Task*, s32, s32, s32);
void func_shelter_b4_reservoir_8017DE8C(Task*);
void func_shelter_b4_reservoir_8017E0AC(Task*);
void func_shelter_b4_reservoir_8017E400(Task*);
void func_shelter_b4_reservoir_8017E4B0(Task*);
void func_shelter_b4_reservoir_8017E558(Task*);
void func_shelter_b4_reservoir_8017E690(s32);
void func_shelter_b4_reservoir_8017E770(s32);
void func_shelter_b4_reservoir_8017E780(s32);
void func_shelter_b4_reservoir_8017E7A8(void);

TaskDesc gScreenWaveTaskDesc[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

static TmdBone _gShelterB4ReservoirModel07220Skeleton[1] = {
#include "assets/shelter_b4_reservoir_model_07220_skeleton.inc"
};

static u32 _gShelterB4ReservoirModel07220PartVerts[1] = {
#include "assets/shelter_b4_reservoir_model_07220_partVerts.inc"
};

static SVECTOR _gShelterB4ReservoirModel07220Verts[15] = {
#include "assets/shelter_b4_reservoir_model_07220_verts.inc"
};

static u32 _gShelterB4ReservoirModel07220Stream[46] = {
#include "assets/shelter_b4_reservoir_model_07220_stream.inc"
};

static TmdSource _gShelterB4ReservoirModel07220 = {
    0,
    320,
    0,
    1,
    _gShelterB4ReservoirModel07220PartVerts,
    _gShelterB4ReservoirModel07220Verts,
    &_gShelterB4ReservoirModel07220Verts[15],
    _gShelterB4ReservoirModel07220Skeleton,
    _gShelterB4ReservoirModel07220Stream,
};

TaskMessageEntry D_shelter_b4_reservoir_801848BC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b4_reservoir_8017E264 },
    { 5105, func_shelter_b4_reservoir_8017E25C },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b4_reservoir_8017E3C4 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b4_reservoir_8017E354 },
    { ROOM_MESSAGE_SOUND, func_shelter_b4_reservoir_8017E3CC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b4_reservoir_801848EC[4] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b4_reservoir_8017DE8C, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b4_reservoir_8017E400, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b4_reservoir_8017E4B0, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b4_reservoir_8017E0AC, { .value = 0 } },
};

ActorCommand D_shelter_b4_reservoir_8018491C = { { .loc = { 4, 45 } }, 6 };

TaskDesc D_shelter_b4_reservoir_80184920[1] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 32 } }, func_shelter_b4_reservoir_8017E558, { .model = &_gShelterB4ReservoirModel07220 } },
};

Task* D_shelter_b4_reservoir_8018492C = 0;

Task* D_shelter_b4_reservoir_80184930 = NULL;

AnimationPlayRequest D_shelter_b4_reservoir_80184934 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

EvsCommand D_shelter_b4_reservoir_80184948[48] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b4_reservoir_8017E7A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = screenWaveRun }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E770 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E770 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x542D0005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b4_reservoir_8017E7A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = screenWaveRun }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542D0005 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x542D0006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b4_reservoir_8017E7A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FADE_SOUND_ATTENUATION, { .value = 0x542D0006 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x542D0007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b4_reservoir_8018491C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542D0006 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_shelter_b4_reservoir_80184DC8[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = screenWaveRun }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E690 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b4_reservoir_8017E780 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b4_reservoir_8018491C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b4_reservoir_8017E7A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

u8 D_shelter_b4_reservoir_80184F78 = 4;

u8 D_shelter_b4_reservoir_80184F79 = 8;

u8 D_shelter_b4_reservoir_80184F7A = 3;

_ShelterB4ReservoirWaterDriftHalfExtent D_shelter_b4_reservoir_80184F7C = { 640, { 0, 0 } };

s16 D_shelter_b4_reservoir_80184F80 = -2000;

s16 D_shelter_b4_reservoir_80184F82 = 0;

TaskDesc D_shelter_b4_reservoir_80184F84[1] = {
    { { { TASK_BODY_NONE, 96 } }, func_shelter_b4_reservoir_8017FADC, { .value = 0 } },
};

RoomWaterSurface D_shelter_b4_reservoir_80184F90[2] = {
    { -980, -5900, 2023, 9900, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

RoomWaterSurface D_shelter_b4_reservoir_80184FA8[3] = {
    { 1037, -5900, 950, 0x2E18, 64 },
    { 1987, -5900, 950, 0x2E18, 64 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

RoomWaterSurface D_shelter_b4_reservoir_80184FCC[2] = {
    { 2940, -3900, 2900, 1800, 16 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

RoomWaterSurface D_shelter_b4_reservoir_80184FE4[5] = {
    { 1400, 3100, 1000, 4800, 64 },
    { -1028, 3100, 2428, 4800, 64 },
    { -1028, -5900, 2428, 9000, 1 },
    { 788, -5900, 1500, 3800, 1 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
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

WorldCoordRoomLighting D_shelter_b4_reservoir_801850E8[2] = {
    { D_shelter_b4_reservoir_80186AA8, NULL },
    { D_shelter_b4_reservoir_80186AA8, NULL },
};

WorldCollisionRoomResources D_shelter_b4_reservoir_801850F8[2] = {
    { D_shelter_b4_reservoir_80185AB8, D_shelter_b4_reservoir_80186AC0, D_shelter_b4_reservoir_80186D20, D_shelter_b4_reservoir_801873B0 },
    { D_shelter_b4_reservoir_80185AB8, D_shelter_b4_reservoir_80186AC0, D_shelter_b4_reservoir_80186F34, D_shelter_b4_reservoir_801873B0 },
};

u8* D_shelter_b4_reservoir_80185118[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount D_shelter_b4_reservoir_80185120[2] = { 10, 10 };

DirectionWarpEntry D_shelter_b4_reservoir_80185124[2] = {
    { { { .word = 3072 }, 5500, 0, -3000 }, { 0, 0, 0, 0 }, { { .word = 3584 }, 5000, 0, -3650 }, { 0, 0, 0, 0 }, 0x542D0004, 0x542D0003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -2527, -2000, -2825 }, { 0, 0, 0, 0 }, { { .word = 512 }, -2700, -2000, -2000 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_FADE_DEPARTURE, GAME_FLAG_MAP_MARK_RESERVOIR_1BE },
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

static SVECTOR _gShelterB4ReservoirCollision084F8Normals[22] = {
#include "assets/shelter_b4_reservoir_collision_084F8_normals.inc"
};

static SVECTOR _gShelterB4ReservoirCollision084F8Verts[98] = {
#include "assets/shelter_b4_reservoir_collision_084F8_verts.inc"
};

static WorldCollisionGridFace _gShelterB4ReservoirCollision084F8Faces[45] = {
#include "assets/shelter_b4_reservoir_collision_084F8_faces.inc"
};

static s16 _gShelterB4ReservoirCollision084F8Cells[310] = {
#include "assets/shelter_b4_reservoir_collision_084F8_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB4ReservoirCollision084F8Cells[i])
static s16* _gShelterB4ReservoirCollision084F8Table[25] = {
#include "assets/shelter_b4_reservoir_collision_084F8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b4_reservoir_80185AB8[1] = {
    { NULL, _gShelterB4ReservoirCollision084F8Normals, _gShelterB4ReservoirCollision084F8Verts, _gShelterB4ReservoirCollision084F8Faces, _gShelterB4ReservoirCollision084F8Table, 0x2AC6, 6000, 5, 5, 4000, 45 },
};

ViewCamera D_shelter_b4_reservoir_80185ADC[10] = {
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

SpriteView D_shelter_b4_reservoir_80186730[10] = {
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

WorldCoordRoomLights D_shelter_b4_reservoir_80186AA8[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_b4_reservoir_801867A8), D_shelter_b4_reservoir_801867A8, 0, NULL },
};

WorldCollisionTrigger D_shelter_b4_reservoir_80186AC0[8] = {
    { NULL, NULL, NULL, { -3072, -1856, -1137, 0 }, { { -2192, -4304, 0, 0 }, { 2192, -4304, 0, 0 }, { -2192, 4304, 0, 0 }, { 2192, 4304, 0, 0 } }, { 0, 0, -4120, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3008, -1729, -1264, 0 }, { { 2128, -4304, 0, 0 }, { -2128, -4304, 0, 0 }, { 2128, 4304, 0, 0 }, { -2128, 4304, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -32, -1856, 3200, 0 }, { { 1008, -4304, 0, 0 }, { -1008, -4304, 0, 0 }, { 1008, 4304, 0, 0 }, { -1008, 4304, 0, 0 } }, { 0, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -16, -1889, 3328, 0 }, { { -1088, -4304, 0, 0 }, { 1088, -4304, 0, 0 }, { -1088, 4304, 0, 0 }, { 1088, 4304, 0, 0 } }, { 0, 0, -4116, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1184, -1952, -1632, 0 }, { { 2188, -4304, 204, 0 }, { -2191, -4304, -207, 0 }, { 2188, 4304, 204, 0 }, { -2191, 4304, -207, 0 } }, { -384, 0, 4088, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1168, -1888, -1504, 0 }, { { -2276, -4304, -229, 0 }, { 2270, -4304, 222, 0 }, { -2276, 4304, -229, 0 }, { 2270, 4304, 222, 0 } }, { 406, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3008, -2016, -2992, 0 }, { { 0, -4304, -1392, 0 }, { 0, -4304, 1392, 0 }, { 0, 4304, -1392, 0 }, { 0, 4304, 1392, 0 } }, { 4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3136, -1984, -2976, 0 }, { { 0, -4304, 1264, 0 }, { 0, -4304, -1264, 0 }, { 0, 4304, 1264, 0 }, { 0, 4304, -1264, 0 } }, { -4108, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b4_reservoir_80186D20[7] = {
    { NULL, NULL, NULL, { 0, -48, 864, 0 }, { { -1024, 0, -160, 0 }, { 1024, 0, -160, 0 }, { -1024, 0, 160, 0 }, { 1024, 0, 160, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_FACING, 202, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 224, -1888, 3888, 0 }, { { -1024, 0, -320, 0 }, { 1024, 0, -320, 0 }, { -1024, 0, 320, 0 }, { 1024, 0, 320, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1070, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5661, -48, -3072, 0 }, { { 448, 0, -1024, 0 }, { 448, 0, 1024, 0 }, { -448, 0, -1024, 0 }, { -448, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_WARP, 46, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2736, -2096, -3552, 0 }, { { 240, 0, -1024, 0 }, { 240, 0, 1024, 0 }, { -240, 0, -1024, 0 }, { -240, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_WARP, 44, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1968, -1888, 5152, 0 }, { { 576, 0, -1024, 0 }, { 576, 0, 1024, 0 }, { -576, 0, -1024, 0 }, { -576, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2817, -2080, -5648, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2720, -2080, -816, 0 }, { { -448, 0, -1216, 0 }, { 448, 0, -1216, 0 }, { -448, 0, 1216, 0 }, { 448, 0, 1216, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 1292, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b4_reservoir_80186F34[9] = {
    { NULL, NULL, NULL, { -512, -48, 864, 0 }, { { -1024, 0, -160, 0 }, { 1024, 0, -160, 0 }, { -1024, 0, 160, 0 }, { 1024, 0, 160, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_FACING, 202, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 384, -1888, 3776, 0 }, { { -1024, 0, -208, 0 }, { 1024, 0, -208, 0 }, { -1024, 0, 208, 0 }, { 1024, 0, 208, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 202, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5872, -48, -3072, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 46, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2736, -2096, -3552, 0 }, { { 240, 0, -1024, 0 }, { 240, 0, 1024, 0 }, { -240, 0, -1024, 0 }, { -240, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2144, -2080, 5232, 0 }, { { 848, 0, -816, 0 }, { 848, 0, 816, 0 }, { -848, 0, -816, 0 }, { -848, 0, 816, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1056, -64, -5728, 0 }, { { 848, 0, -816, 0 }, { 848, 0, 816, 0 }, { -848, 0, -816, 0 }, { -848, 0, 816, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2080, -64, 5616, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2816, -2064, -5648, 0 }, { { -1024, 0, -352, 0 }, { 1024, 0, -352, 0 }, { -1024, 0, 352, 0 }, { 1024, 0, 352, 0 } }, { 0, 4094, 0, 0 }, { 0, 0, 4096, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2768, -2080, -944, 0 }, { { -496, 0, -1200, 0 }, { 496, 0, -1200, 0 }, { -496, 0, 1200, 0 }, { 496, 0, 1200, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 1292, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b4_reservoir_801871E0[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_reservoir_801871F8[4] = {
    { 44, 44, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor04400_D107E4 },
    { 72, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_80153EC8 },
    { 73, 73, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_8014E7A4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_reservoir_80187228[4] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { 70, 70, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207000_801575F0 },
    { 71, 71, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207000_80151E60 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_reservoir_80187258[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
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

AreaVariant D_shelter_b4_reservoir_80187350[12] = {
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

WorldCollisionOccluder D_shelter_b4_reservoir_801873B0[2] = {
    { NULL, NULL, { 4544, -2432, -1024, 0 }, { { -1408, 4896, -768, 0 }, { 1408, 4896, 768, 0 }, { -1408, -4896, -768, 0 }, { 1408, -4896, 768, 0 } }, { -1963, 0, 3597, 0 }, 5145, 1, 0 },
    { NULL, NULL, { 4447, 0, -4993, 0 }, { { -1455, 4896, 834, 0 }, { 1456, 4896, -833, 0 }, { -1455, -4896, 834, 0 }, { 1456, -4896, -833, 0 } }, { 2036, 0, 3555, 0 }, 5170, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

s32 D_shelter_b4_reservoir_80187428[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_shelter_b4_reservoir_80187434 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionFootstepSounds D_shelter_b4_reservoir_80187440 = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionFootstepSounds D_shelter_b4_reservoir_8018744C = {
    0x10000025,
    0x10000027,
    0x10000029,
};

WorldCollisionSurfaceProperties D_shelter_b4_reservoir_80187458[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b4_reservoir_80187460[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b4_reservoir_80187468[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_reservoir_8018744C },
};

WorldCollisionSurfaceProperties D_shelter_b4_reservoir_80187470[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_reservoir_80187440 },
};

WorldCollisionSurfaceProperties D_shelter_b4_reservoir_80187478[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_reservoir_80187434 },
};

WorldCollisionSurfaceProperties* D_shelter_b4_reservoir_80187480[8] = {
    D_shelter_b4_reservoir_80187458,
    D_shelter_b4_reservoir_80187460,
    D_shelter_b4_reservoir_80187468,
    D_shelter_b4_reservoir_80187470,
    D_shelter_b4_reservoir_80187458,
    D_shelter_b4_reservoir_80187478,
    D_shelter_b4_reservoir_80187458,
    D_shelter_b4_reservoir_80187458,
};

AreaApplyRec D_shelter_b4_reservoir_801874A0[24] = {
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

ScreenWaveCtx* gScreenWaveCtx = NULL;

RoomEventMsg D_shelter_b4_reservoir_80187508 = { 0 };

s32 D_shelter_b4_reservoir_80187510 = 0;

ScreenWaveOscillator gScreenWaveColumns[13] = { 0 };

ScreenWaveOscillator gScreenWaveRows[32] = { 0 };

ScreenWaveCtx gScreenWaveSpawnCtx = { 0 };

u8* D_shelter_b4_reservoir_80187630 = NULL;

SVECTOR D_shelter_b4_reservoir_80187634[10] = { 0 };

_ShelterB4ReservoirBurstConfig D_shelter_b4_reservoir_80187684 = { 0, 0, 0 };

static void func_shelter_b4_reservoir_8017E068(void);
static void func_shelter_b4_reservoir_8017E8EC(Task* task);

#include "../../shared/screen_wave.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

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
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
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
                gameFlagSetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE, 1);
                gameFlagSetNibble(GAME_FLAG_MAP_MARK_RESERVOIR_1BF, 2);
                gameFlagSetNibble(GAME_FLAG_INCINERATOR_CONTROL_ROOM_STATE, 1);
                gameFlagSetNibble(GAME_FLAG_MAP_MARK_RESERVOIR_1BE, 2);
                Gp_ApplyAreaRecs(D_shelter_b4_reservoir_801874A0);
                D_80114D08 = 0xA;
                taskKill(task);
            }
            break;
    }
}

static void func_shelter_b4_reservoir_8017E068(void)
{
    // Water-drift spawn argument: half-extent, then period, velocity level and kind.
    D_shelter_b4_reservoir_80187510 = (D_shelter_b4_reservoir_80184F78 << 0x18) | (D_shelter_b4_reservoir_80184F7A << 0xC) | (D_shelter_b4_reservoir_80184F79 << 0x10) | D_shelter_b4_reservoir_80184F7C.halfExtent;
}

void func_shelter_b4_reservoir_8017E0AC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
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
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                D_80114D08                     = 0xA;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
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
                sndEvtRequestScriptStart(SOUND_SHELTER_B4_RESERVOIR_EXIT_TRANSIT, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(SOUND_SHELTER_B4_RESERVOIR_EXIT_TRANSIT) == 0) {
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

s32 func_shelter_b4_reservoir_8017E25C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E264(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    func_map_shelter_80179A04(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_B4_UPPER_SEWER) {
        if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == 1) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(src->flagId, 2);
                Gp_RunCapCmd1(2);
            }
        } else {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                D_shelter_b4_reservoir_80187508.warp              = (u8)dst->areaId;
                D_shelter_b4_reservoir_80187508.field_4           = dst->warp;
                ((u8*)&D_shelter_b4_reservoir_80187508.areaId)[1] = dst->room;
                taskSpawnFromTable(D_shelter_b4_reservoir_801848EC, 3, 0xB, 0);
            }
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_reservoir_8017E354(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        Gp_MsgPlayer3F3(0);
        Gp_MsgAlly3F3(0);
        Gp_MsgPlayerWeapon(0);
        Gp_MsgAllyWeapon(0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
        gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_HIDDEN;
        taskSpawnFromTable(D_shelter_b4_reservoir_801848EC, 0, 0, 0);
    }
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3C4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3CC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        sndEvtRequestScriptStart(0x542D0000 | 2, 0, 0);
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

#include "../../shared/screen_wave_run.inc.c"

void func_shelter_b4_reservoir_8017E690(s32 arg0)
{
    switch (arg0) {
        case 0:
            D_shelter_b4_reservoir_8018492C = taskSpawnFromTable(D_shelter_b4_reservoir_801848EC, 1, 0x96, 0);
            break;
        case 1:
            if (D_shelter_b4_reservoir_8018492C != 0) {
                taskKill(D_shelter_b4_reservoir_8018492C);
                D_shelter_b4_reservoir_8018492C = 0;
            }
            D_shelter_b4_reservoir_80184F80 = -0x1F4;
            break;
        case 2:
            D_shelter_b4_reservoir_8018492C = taskSpawnFromTable(D_shelter_b4_reservoir_801848EC, 2, 0x96, 0);
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
    func_shelter_b4_reservoir_80182B04(ARRAY_SIZE(D_shelter_b4_reservoir_80187634), arg0,
                                       SHELTER_B4_RESERVOIR_BURST_BASE_HALF_EXTENT);
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
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_shelter_b4_reservoir_80184F84, 0, 0, 0);
    if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
        D_shelter_b4_reservoir_80184F80 = -0x1F4;
    } else {
        D_shelter_b4_reservoir_80184F80 = -0x7D0;
    }
    D_shelter_b4_reservoir_80184930 = taskSpawnFromTable(D_shelter_b4_reservoir_80184920, 0, 0, 0);
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
    RoomWaterSurface* surface = D_shelter_b4_reservoir_80184F90;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        D_shelter_b4_reservoir_80187630 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_reservoir_80187630 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    if (gGameSession->location.loc.view != 0xA) {
        surface->depth = 0x2328 - (((D_shelter_b4_reservoir_80184F80 + 0x7D0) * 0x31) >> 5);
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
    WaterQuadScratch* scratchEnd;
    WaterQuadScratch* scratch;
    RoomWaterSurface* surface;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    surface                    = D_shelter_b4_reservoir_80184F90;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = D_shelter_b4_reservoir_80184F80;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width;
        scratch->dz = surface->depth / 32;
        scratch->x  = surface->x + D_shelter_b4_reservoir_80185020[0];
        scratch->z  = surface->z;
        for (i = 0; i < 32; i++) {
            v0.vx            = scratch->x;
            v0.vy            = scratch->y;
            v0.vz            = scratch->z + scratch->dz * i;
            v1.vx            = scratch->x;
            v1.vy            = scratch->y;
            v1.vz            = scratch->z + scratch->dz * (i + 1);
            scratch->yOffset = 0;
            v2.vx            = scratch->x + scratch->dx;
            v2.vy            = scratch->y + scratch->yOffset;
            v2.vz            = scratch->z + scratch->dz * i;
            scratch->yOffset = 0;
            v3.vx            = scratch->x + scratch->dx;
            v3.vy            = scratch->y + scratch->yOffset;
            v3.vz            = scratch->z + scratch->dz * (i + 1);
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0x80;
                poly->g0                       = 0;
                poly->b0                       = 0;
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
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

/// Same strip renderer as `func_shelter_b4_reservoir_8017EA00`, driven by
/// `D_shelter_b4_reservoir_80184FA8`: each surface's `segmentCount` gives its quad
/// count, and its X is used as stored rather than offset. `task` is unused.
static void func_shelter_b4_reservoir_8017EE04(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    WaterQuadScratch* scratchEnd;
    WaterQuadScratch* scratch;
    RoomWaterSurface* surface;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    surface                    = D_shelter_b4_reservoir_80184FA8;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = D_shelter_b4_reservoir_80184F80;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width;
        scratch->dz = surface->depth / surface->segmentCount;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (i = 0; i < surface->segmentCount; i++) {
            v0.vx            = scratch->x;
            v0.vy            = scratch->y;
            v0.vz            = scratch->z + scratch->dz * i;
            v1.vx            = scratch->x;
            v1.vy            = scratch->y;
            v1.vz            = scratch->z + scratch->dz * (i + 1);
            scratch->yOffset = 0;
            v2.vx            = scratch->x + scratch->dx;
            v2.vy            = scratch->y + scratch->yOffset;
            v2.vz            = scratch->z + scratch->dz * i;
            scratch->yOffset = 0;
            v3.vx            = scratch->x + scratch->dx;
            v3.vy            = scratch->y + scratch->yOffset;
            v3.vz            = scratch->z + scratch->dz * (i + 1);
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0x80;
                poly->g0                       = 0;
                poly->b0                       = 0;
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
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
}

/// Strip renderer like `func_shelter_b4_reservoir_8017EE04`, driven by
/// `D_shelter_b4_reservoir_80184FCC`, but laid along X instead of Z: each
/// surface's `width` is divided into `segmentCount` quads, and `depth` is the
/// extent along Z. The scratch fields `dx` and `dz` therefore hold the X step
/// and the Z extent here. `task` is unused.
static void func_shelter_b4_reservoir_8017F23C(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    WaterQuadScratch* scratchEnd;
    WaterQuadScratch* scratch;
    RoomWaterSurface* surface;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    surface                    = D_shelter_b4_reservoir_80184FCC;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = D_shelter_b4_reservoir_80184F80;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width / surface->segmentCount;
        scratch->dz = surface->depth;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (i = 0; i < surface->segmentCount; i++) {
            v0.vx            = scratch->x + scratch->dx * i;
            v0.vy            = scratch->y;
            v0.vz            = scratch->z;
            v1.vx            = scratch->x + scratch->dx * (i + 1);
            v1.vy            = scratch->y;
            v1.vz            = scratch->z;
            scratch->yOffset = 0;
            v2.vx            = scratch->x + scratch->dx * i;
            v2.vy            = scratch->y + scratch->yOffset;
            v2.vz            = scratch->z + scratch->dz;
            scratch->yOffset = 0;
            v3.vx            = scratch->x + scratch->dx * (i + 1);
            v3.vy            = scratch->y + scratch->yOffset;
            v3.vz            = scratch->z + scratch->dz;
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0x80;
                poly->g0                       = 0;
                poly->b0                       = 0;
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
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
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
    WaterQuadScratch* scratchEnd;
    WaterQuadScratch* scratch;
    RoomWaterSurface* surface;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;
    u8                c;

    surface                    = D_shelter_b4_reservoir_80184FE4;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    (scratchEnd - 1)->y = D_shelter_b4_reservoir_80184F82;
    c                   = -(D_shelter_b4_reservoir_80184F82 * 16) / 225;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        scratch->dx = surface->width;
        scratch->dz = surface->depth / surface->segmentCount;
        scratch->x  = surface->x;
        scratch->z  = surface->z;
        for (i = 0; i < surface->segmentCount; i++) {
            v0.vx            = scratch->x;
            v0.vy            = scratch->y;
            v0.vz            = scratch->z + scratch->dz * i;
            v1.vx            = scratch->x;
            v1.vy            = scratch->y;
            v1.vz            = scratch->z + scratch->dz * (i + 1);
            scratch->yOffset = 0;
            v2.vx            = scratch->x + scratch->dx;
            v2.vy            = scratch->y + scratch->yOffset;
            v2.vz            = scratch->z + scratch->dz * i;
            scratch->yOffset = 0;
            v3.vx            = scratch->x + scratch->dx;
            v3.vy            = scratch->y + scratch->yOffset;
            v3.vz            = scratch->z + scratch->dz * (i + 1);
            otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0;
                poly->g0                       = c >> 2;
                poly->b0                       = c;
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
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
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
    EffectWork* work;
    Task*       player;
    GfxCoord*   root;
    GfxCoord*   c;
    GfxCoord    coord;
    s32         i;
    s32         baseHalfExtent;
    s32         randomizedSpawnArgs;

    work   = task->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    root   = player->extra.tmd->coords;
    if (task->state == 0) {
        gRoomEffectWaterRippleId  = EFFECT_SHELTER_B4_RESERVOIR_WATER_RIPPLE;
        gRoomEffectWaterSprayId   = EFFECT_SHELTER_B4_RESERVOIR_WATER_SPRAY;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B4_RESERVOIR_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B4_RESERVOIR_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B4_RESERVOIR_ORANGE_BURST_2;
        task->state               = 1;
        // Scatter the burst points around the anchor. The work's `scale` and
        // `angle` hold the polar radius and angle of the point being placed.
        for (i = 0; i < 10; i++) {
            work->scale                           = RAND() & 0x1C0;
            work->angle                           = (RAND() & 0x1FF) + (i << 9);
            D_shelter_b4_reservoir_80187634[i].vx = D_shelter_b4_reservoir_80185094.vx + 0x800;
            D_shelter_b4_reservoir_80187634[i].vy =
                D_shelter_b4_reservoir_80185094.vy + ((work->scale * rsin(work->angle)) >> 12);
            D_shelter_b4_reservoir_80187634[i].vz =
                D_shelter_b4_reservoir_80185094.vz + ((work->scale * rcos(work->angle)) >> 12);
        }
        D_shelter_b4_reservoir_80187684.pointCount         = 0;
        D_shelter_b4_reservoir_80187684.spawnChancePercent = 0;
        D_shelter_b4_reservoir_80187684.baseHalfExtent     = 0;
        for (i = 0; i < 2; i++) {
            c                                     = &player->extra.tmd->coords[i * 3 + 14];
            D_shelter_b4_reservoir_801850AC[i].vx = c->workm.t[0];
            D_shelter_b4_reservoir_801850AC[i].vy = c->workm.t[1];
            D_shelter_b4_reservoir_801850AC[i].vz = c->workm.t[2];
        }
    }
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
            // `age` counts the frames run with the event done; the splash pass waits for the second.
            if (gGameSession->waterY < root->coord.t[1] && work->age != 0) {
                for (i = 0; i < 2; i++) {
                    c = &player->extra.tmd->coords[i * 3 + 14];
                    actorRenderComposeCoord(c);
                    // `angle` is reused as the splash chance out of 512: the distance
                    // this coordinate moved since the previous frame, plus a ripple bias.
                    work->angle = ABS(D_shelter_b4_reservoir_801850AC[i].vx - c->workm.t[0]) +
                                  ABS(D_shelter_b4_reservoir_801850AC[i].vy - c->workm.t[1]) +
                                  ABS(D_shelter_b4_reservoir_801850AC[i].vz - c->workm.t[2]) + 0x20;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &c->workm, &coord.coord);
                    coord.parent       = &gGfxViewCoord;
                    coord.coord.t[1]   = gGameSession->waterY;
                    coord.composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(&coord);
                    if ((s32)(RAND() & 0x1FF) < work->angle) {
                        Gp_SpawnEff(gRoomEffectWaterRippleId, &coord, 0x40, NULL);
                    }
                    work->angle -= 0x20;
                    if ((s32)(RAND() & 0x1FF) < work->angle) {
                        Gp_SpawnEff(gRoomEffectWaterSprayId, &coord, 0x1202180, NULL);
                    }
                    D_shelter_b4_reservoir_801850AC[i].vx = c->workm.t[0];
                    D_shelter_b4_reservoir_801850AC[i].vy = c->workm.t[1];
                    D_shelter_b4_reservoir_801850AC[i].vz = c->workm.t[2];
                }
            }
            work->age++;
        }
        // Emit independently at each active point, occasionally moving its position.
        if (D_shelter_b4_reservoir_80187684.pointCount != 0 && D_shelter_b4_reservoir_80187684.spawnChancePercent != 0) {
            for (i = 0; i < D_shelter_b4_reservoir_80187684.pointCount; i++) {
                if ((RAND() & 0x1F) == 0) {
                    work->scale                           = RAND() & 0x1C0;
                    work->angle                           = (RAND() & 0x1FF) + (i << 9);
                    D_shelter_b4_reservoir_80187634[i].vx = D_shelter_b4_reservoir_80185094.vx + 0x800;
                    D_shelter_b4_reservoir_80187634[i].vy =
                        D_shelter_b4_reservoir_80185094.vy + ((work->scale * rsin(work->angle)) >> 12);
                    D_shelter_b4_reservoir_80187634[i].vz =
                        D_shelter_b4_reservoir_80185094.vz + ((work->scale * rcos(work->angle)) >> 12);
                }
                if ((u16)(RAND() % SHELTER_B4_RESERVOIR_BURST_CHANCE_SCALE) < D_shelter_b4_reservoir_80187684.spawnChancePercent) {
                    baseHalfExtent      = D_shelter_b4_reservoir_80187684.baseHalfExtent;
                    randomizedSpawnArgs = (RAND() & SHELTER_B4_RESERVOIR_BURST_VARIATION_MASK) + SHELTER_B4_RESERVOIR_BURST_MOTION_ARGS;
                    Gp_SpawnEff(EFFECT_SHELTER_B4_RESERVOIR_BURST_SPRITE, NULL, baseHalfExtent + randomizedSpawnArgs,
                                &D_shelter_b4_reservoir_80187634[i]);
                }
            }
        }
    }
    if ((u8)viewGetMappedIndex() == 10) {
        D_shelter_b4_reservoir_8018509C[1].vy = D_shelter_b4_reservoir_80184F82;
        if ((RAND() & 1) == 0) {
            Gp_SpawnEff(gRoomEffectWaterSprayId, NULL, (RAND() & 0x1000) + 0x4A03600, &D_shelter_b4_reservoir_8018509C[0]);
        }
        if ((RAND() & 1) == 0) {
            Gp_SpawnEff(gRoomEffectWaterSprayId, NULL, (RAND() & 0x10FF) | 0x11602300, &D_shelter_b4_reservoir_8018509C[1]);
        }
        if ((RAND() & 3) == 0) {
            Gp_SpawnEff(gRoomEffectWaterRippleId, NULL, (RAND() & 0x7F) | 0x80, &D_shelter_b4_reservoir_8018509C[1]);
        }
    }
    switch ((u8)viewGetMappedIndex()) {
        case 2:
            _glowDrawCapsule(&D_shelter_b4_reservoir_80185024[10], 0x200, 0x444);
            break;
        case 4:
            _glowDrawCapsule(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x222);
            _glowDrawCapsule(&D_shelter_b4_reservoir_80185024[6], 0x200, 0x444);
            _glowDrawCapsule(&D_shelter_b4_reservoir_80185024[8], 0x200, 0x333);
            if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 5:
            _glowDrawCapsule(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x444);
            if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 6:
            if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                glowDrawTintedDiscNoBias(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 7:
            _glowDrawCapsule(&D_shelter_b4_reservoir_80185024[2], 0x200, 0x444);
            break;
        case 3:
        case 9:
            _glowDrawCapsule(&D_shelter_b4_reservoir_80185024[4], 0x200, 0x444);
            break;
    }
}

#include "../../shared/water_ripple_task.inc.c"

void func_shelter_b4_reservoir_801803DC(Task* task)
{
    _waterRippleTask(task);
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
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s16         f2a;
    u32         rng;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        func_shelter_b4_reservoir_80181668(coord, work->index, work->scale);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
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

            work->step      = f2a;
            work->move.vy   = 0;
            work->move.vz   = 0;
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng;
            work->move.vx   = -((rng >> 16) & 0x3F) - 0x40;
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
                    effectKillTask(work, task);
                }
            }
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x4393). `frame` selects one of six 32-texel
/// UV columns at u = `(frame % 6) * 32 + 0x40`, v = 0x40..0x5F. `size` is a
/// half-extent; the on-screen radius is `size * 31 / depth`, and the quad is
/// axis-aligned about the projected point.
static void func_shelter_b4_reservoir_80181668(GfxCoord* coord, u16 frame, s16 size)
{
    void**               scratch;
    u8*                  head;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    DisplayState*        ds;
    s32                  col;
    s16                  xy;
    u16                  vz;

    scratch                                                                     = SCRATCH_STACK_CURSOR_SLOT;
    head                                                                        = *scratch;
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = (u16)coord->workm.t[0];
    block                                                                       = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    block->worldPoint.vy                                                        = (u16)coord->workm.t[1];
    vz                                                                          = (u16)coord->workm.t[2];
    *scratch                                                                    = block;
    block->worldPoint.vz                                                        = vz;
    vec                                                                         = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage         = 0x2B;
        prim->clut          = 0x4393;
        prim->v0            = 0x40;
        prim->v1            = 0x40;
        prim->v2            = 0x5F;
        prim->v3            = 0x5F;
        col                 = (u16)(frame % 6) << 5;
        prim->u0            = col + 0x40;
        prim->u2            = col + 0x40;
        prim->u1            = col + 0x5F;
        prim->u3            = col + 0x5F;
        block->screenExtent = (size * 31) / block->depth;
        xy                  = block->screenX - (u16)block->screenExtent;
        prim->x2            = xy;
        prim->x0            = xy;
        xy                  = block->screenX + (u16)block->screenExtent;
        prim->x3            = xy;
        prim->x1            = xy;
        xy                  = block->screenY - (u16)block->screenExtent;
        prim->y1            = xy;
        prim->y0            = xy;
        xy                  = block->screenY + (u16)block->screenExtent;
        prim->y3            = xy;
        prim->y2            = xy;
        ds                  = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(EffectCentreScratch));
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_tinted_disc_no_bias.inc.c"

static void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2)
{
    D_shelter_b4_reservoir_80187684.pointCount         = arg0;
    D_shelter_b4_reservoir_80187684.spawnChancePercent = arg1;
    D_shelter_b4_reservoir_80187684.baseHalfExtent     = arg2;
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_shelter_b4_reservoir_80182B1C(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b4_reservoir_80183074(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b4_reservoir_80183CD4(Task* arg0)
{
    _roomVisualEffectsFlyingOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
