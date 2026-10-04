#include "rooms/neo_ark_submarine_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_submarine_tunnel_private.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"
#include "../../shared/screen_wave.h"
#include "../../shared/water_effects.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// The area-record id the event handler publishes, and the event scripts
/// `func_800E8634` / `func_800E8614` are handed.
extern EvsCommand D_80135220[];
extern EvsCommand D_actor_451100_80135FD0[];
extern EvsCommand D_80136108[];

/// Spawn table of the screen-wave task, and the context it is spawned with.
/// The context's mode word is written through its own symbol, which is how the
/// original reached it.
extern TaskDesc gScreenWaveTaskDesc[];

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 gScreenWaveRamp;

/// Message handlers this room's task answers, installed into pointer slot 7.
extern TaskMessageEntry D_neo_ark_submarine_tunnel_80181A50[];

/// The tunnel's own script blob and the byte recording which of its scenes has
/// already been staged.
extern EvsCommand D_neo_ark_submarine_tunnel_80181AF0[];

static void func_neo_ark_submarine_tunnel_8017F3BC(Task* arg0);
static void func_neo_ark_submarine_tunnel_8017F414(Task* task);

s32 func_neo_ark_submarine_tunnel_8017F064(Task*, s32, RoomEventMsg*, s32);
s32 func_neo_ark_submarine_tunnel_8017F27C(Task*, s32, s32, s32);
s32 func_neo_ark_submarine_tunnel_8017F284(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_neo_ark_submarine_tunnel_8017F2C8(Task*, s32, s32, s32);

static AnimationSet _gNeoArkSubmarineTunnelAnimation03EE0;
static AnimationSet _gNeoArkSubmarineTunnelAnimation0444C;

extern AnimationPlayRequest     D_neo_ark_submarine_tunnel_80181A88;
extern AnimationPlayRequest     D_neo_ark_submarine_tunnel_80181A9C;
extern AnimationBankCopyRequest D_neo_ark_submarine_tunnel_80181A80;
void                            func_neo_ark_submarine_tunnel_8017F398(s32);

TaskDesc D_neo_ark_submarine_tunnel_801810E4 = { { { TASK_BODY_NONE, 192 } }, waterRefractionTask, { .value = 0 } };

TaskDesc D_neo_ark_submarine_tunnel_801810F0 = { { { TASK_BODY_NONE, 192 } }, waterDistortBandTask, { .value = 0 } };

static AnimationPackedPose _gNeoArkSubmarineTunnelAnimation03EE0Bank1[6] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_bank1.inc"
};

static AnimationPackedRotation _gNeoArkSubmarineTunnelAnimation03EE0Bank4[64] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_bank4.inc"
};

static AnimationRecord _gNeoArkSubmarineTunnelAnimation03EE0Records[141] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_records.inc"
};

static u16 _gNeoArkSubmarineTunnelAnimation03EE0Indices[20] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_indices.inc"
};

static AnimationSet _gNeoArkSubmarineTunnelAnimation03EE0 = {
    _gNeoArkSubmarineTunnelAnimation03EE0Records,
    _gNeoArkSubmarineTunnelAnimation03EE0Indices,
    { NULL, _gNeoArkSubmarineTunnelAnimation03EE0Bank1, NULL, NULL, _gNeoArkSubmarineTunnelAnimation03EE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gNeoArkSubmarineTunnelAnimation0444CBank1[10] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_bank1.inc"
};

static AnimationPackedRotation _gNeoArkSubmarineTunnelAnimation0444CBank4[126] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_bank4.inc"
};

static AnimationRecord _gNeoArkSubmarineTunnelAnimation0444CRecords[171] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_records.inc"
};

static u16 _gNeoArkSubmarineTunnelAnimation0444CIndices[20] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_indices.inc"
};

static AnimationSet _gNeoArkSubmarineTunnelAnimation0444C = {
    _gNeoArkSubmarineTunnelAnimation0444CRecords,
    _gNeoArkSubmarineTunnelAnimation0444CIndices,
    { NULL, _gNeoArkSubmarineTunnelAnimation0444CBank1, NULL, NULL, _gNeoArkSubmarineTunnelAnimation0444CBank4, NULL, NULL, NULL },
};

TaskDesc gScreenWaveTaskDesc[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskMessageEntry D_neo_ark_submarine_tunnel_80181A50[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_submarine_tunnel_8017F284 },
    { 5105, func_neo_ark_submarine_tunnel_8017F27C },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_submarine_tunnel_8017F064 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_submarine_tunnel_8017F2C8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_neo_ark_submarine_tunnel_80181A78[2] = {
    &_gNeoArkSubmarineTunnelAnimation0444C,
    &_gNeoArkSubmarineTunnelAnimation03EE0,
};

AnimationBankCopyRequest D_neo_ark_submarine_tunnel_80181A80 = { { .sets = D_neo_ark_submarine_tunnel_80181A78 }, ARRAY_SIZE(D_neo_ark_submarine_tunnel_80181A78) };

AnimationPlayRequest D_neo_ark_submarine_tunnel_80181A88 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_submarine_tunnel_80181A9C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

ActorCommand D_neo_ark_submarine_tunnel_80181AB0 = { { .loc = { 5, 12 } }, 0 };

ActorCommand D_neo_ark_submarine_tunnel_80181AB4 = { { .loc = { 5, 12 } }, 1 };

ActorCommand D_neo_ark_submarine_tunnel_80181AB8 = { { .loc = { 5, 12 } }, 2 };

AnimationPlayRequest D_neo_ark_submarine_tunnel_80181ABC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_neo_ark_submarine_tunnel_80181AD0 = { { 4544, 3001, 0, 0 }, { 0, -1024, 0, 0 } };

EvsSceneKey D_neo_ark_submarine_tunnel_80181AE8 = { 5, 60, 11 };

EvsCommand D_neo_ark_submarine_tunnel_80181AF0[32] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_neo_ark_submarine_tunnel_80181AE8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_neo_ark_submarine_tunnel_80181A80 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181ABC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_neo_ark_submarine_tunnel_80181AD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = screenWaveRun }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181A9C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_neo_ark_submarine_tunnel_80181AB4 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181A88 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_neo_ark_submarine_tunnel_80181AB8 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181ABC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = screenWaveRun }, { .value = SCREEN_WAVE_RAMP_FINISHED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_neo_ark_submarine_tunnel_8017F398 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

/// State handlers of the room task `func_neo_ark_submarine_tunnel_8017F434`
/// runs: `func_neo_ark_submarine_tunnel_8017F3BC` sets it up,
/// `func_neo_ark_submarine_tunnel_8017F414` runs every later tick, and
/// `taskKill` ends it.
static const TaskFuncTable3 D_neo_ark_submarine_tunnel_8017D614 = {
    { func_neo_ark_submarine_tunnel_8017F3BC, func_neo_ark_submarine_tunnel_8017F414, taskKill }
};

#include "../../shared/screen_wave.inc.c"

s32 func_neo_ark_submarine_tunnel_8017F064(Task* arg0, s32 arg1, RoomEventMsg* arg2, s32 arg3)
{
    u8 temp_s0;
    u8 temp_s0_2;
    u8 temp_s0_3;
    u8 temp_s0_4;

    temp_s0 = arg2->warp;
    if ((temp_s0 == 1) && (GameFlag_GetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS) == temp_s0) && (gGameSession->location.loc.variant == 3)) {
        func_800E3FAC(0xA2, 0x35);
        GameFlag_SetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS, 2);
        GameFlag_SetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE, 1);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x1A;
        func_800E8634(D_80135220, 0, D_actor_451100_80135FD0);
    }
    if ((arg2->warp == 2) && (GameFlag_GetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN) == 0)) {
        temp_s0_2 = gGameSession->location.loc.variant;
        if (temp_s0_2 == 1) {
            func_800E8614(D_neo_ark_submarine_tunnel_80181AF0, 0);
            D_neo_ark_submarine_tunnel_80181DF0 = temp_s0_2;
        }
    }
    temp_s0_3 = arg2->warp;
    if ((temp_s0_3 == 3) && (D_neo_ark_submarine_tunnel_80181DF0 == 0) && (gGameSession->location.loc.warp == 2) && (GameFlag_GetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS) == 0) && (gGameSession->location.loc.variant == temp_s0_3)) {
        GameFlag_SetNibble(GAME_FLAG_SUBMARINE_TUNNEL_PROGRESS, 1);
        func_800E8614(D_80136108, 0);
        D_neo_ark_submarine_tunnel_80181DF0 = 1;
    }
    if ((arg2->warp == 2) && (D_neo_ark_submarine_tunnel_80181DF0 == 0)) {
        temp_s0_4 = gGameSession->location.loc.warp;
        if (temp_s0_4 == 1) {
            Gp_MsgPlayerWeapon(1);
            D_neo_ark_submarine_tunnel_80181DF0 = temp_s0_4;
        }
    }
    if ((arg2->warp == 3) && (D_neo_ark_submarine_tunnel_80181DF0 == 0) && (gGameSession->location.loc.warp == 2)) {
        Gp_MsgPlayerWeapon(1);
        D_neo_ark_submarine_tunnel_80181DF0 = 1;
    }
    return 0;
}

/// Answers 0 unconditionally.
s32 func_neo_ark_submarine_tunnel_8017F27C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Save-location message handler: copies the incoming `RoomEventMsg` onto the
/// outgoing one, forwards both to `func_map_neo_ark_80179B14` and answers 1.
s32 func_neo_ark_submarine_tunnel_8017F284(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

/// Message 0x13F0 handler: for an `arg2` of 4 or 5, and only while the
/// session's place is 1, passes it to `Gp_SpawnIfCapIdle`. Answers 0.
s32 func_neo_ark_submarine_tunnel_8017F2C8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 < 6) {
        if (arg2 >= 4) {
            if (gGameSession->location.loc.variant == 1) {
                Gp_SpawnIfCapIdle(arg2, 0);
            }
        }
    }
    return 0;
}

#include "../../shared/screen_wave_run.inc.c"

void func_neo_ark_submarine_tunnel_8017F398(s32 arg0)
{
    GameFlag_SetNibble(GAME_FLAG_SUBMARINE_TUNNEL_EVENT_SEEN, arg0);
}

/// First state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7, plays sound event 0x550C0003 and advances.
static void func_neo_ark_submarine_tunnel_8017F3BC(Task* arg0)
{
    arg0->msgTable = D_neo_ark_submarine_tunnel_80181A50;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    SndEvt_EnqueueType6(SOUND_NEO_ARK_SUBMARINE_TUNNEL_AMBIENCE, 0, 0);
    arg0->state = arg0->state + 1;
}

/// Later states of the room task: reads pointer slot 3 and discards it.
static void func_neo_ark_submarine_tunnel_8017F414(Task* task)
{
    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
}

/// Room task tick: copies the three-entry state table
/// `D_neo_ark_submarine_tunnel_8017D614` to the stack and calls the entry for
/// the task's state.
void func_neo_ark_submarine_tunnel_8017F434(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_submarine_tunnel_8017D614;
    sp.funcs[task->state](task);
}
