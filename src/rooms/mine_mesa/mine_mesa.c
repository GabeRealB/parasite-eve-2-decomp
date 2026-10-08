#include "rooms/mine_mesa.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"
#include "../../shared/streamed_scene.h"

#define MINE_MESA_RAND() ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16)

// Script arguments shared by the room's head-aim tasks and their controllers.
enum {
    MINE_MESA_HEAD_AIM_RELEASE               = 0,
    MINE_MESA_HEAD_AIM_TRACK                 = 1,
    MINE_MESA_HEAD_AIM_MODE_COUNT            = 2,
    MINE_MESA_HEAD_AIM_CANCEL                = -1,
    MINE_MESA_HEAD_AIM_INITIALIZE            = 0,
    MINE_MESA_HEAD_AIM_UPDATE                = 1,
    MINE_MESA_HEAD_BLEND_ACTIVE              = 0,
    MINE_MESA_HEAD_AIM_YAW_LIMIT             = 768,
    MINE_MESA_PLAYER_HEAD_AIM_PITCH_LIMIT    = 512,
    MINE_MESA_COMPANION_HEAD_AIM_PITCH_LIMIT = 256,
    MINE_MESA_PLAYER_HEAD_BLEND_PITCH_LIMIT  = 16,
    MINE_MESA_HEAD_AIM_RATE_STEP             = ONE / 16,
};

enum {
    MINE_MESA_FADE_CANCEL             = -1,
    MINE_MESA_COUNTDOWN_MUSIC_ENTRY   = 2,
    MINE_MESA_SCENE_COLLISION_NORMAL  = 0,
    MINE_MESA_SCENE_COLLISION_LOWERED = 1,
};

extern WorldCollisionGrid D_mine_mesa_801864A4;

extern WorldCollisionGrid D_mine_mesa_8018700C;

extern RoomEventMsg gRoomEventStagedMsg;

extern s8 D_mine_mesa_80189B48;

extern RoomLatchedEvent gRoomEventLatched;

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_mine_mesa_801842F4[];

/// Handle of the task spawned from entry 1 or 3 of `D_mine_mesa_801842F4`, or
/// NULL while none runs.
extern Task* D_mine_mesa_80189B54;

/// Handle of the task spawned from entry 4 of `D_mine_mesa_801842F4`, or NULL
/// while none runs.
extern Task* D_mine_mesa_80189B5C;

static void _mineMesaPlayerHeadAimTask(Task* task);

extern s16 MineMesaRemaining;
extern s16 MineMesaCooldown;

extern WorldCollisionTrigger D_mine_mesa_801890A0[19];

/// A fixed place at which the room's enemy wave puts a newly spawned enemy.
///
/// The room keeps four, and the wave draws one at random from the subset the
/// current camera view allows. The enemy's root coordinate takes the position
/// as its translation and has its rotation overwritten with `yaw`.
typedef struct {
    s16 x;   // World X of the enemy's root
    s16 y;   // World Y of the enemy's root; zero in every point
    s16 z;   // World Z of the enemy's root
    s16 yaw; // Facing about the vertical axis, signed, 4096 units per turn
} _MineMesaSpawnPoint;
STATIC_ASSERT_SIZEOF(_MineMesaSpawnPoint, 8);

/// The base edge of one vertical wall the room builds into its collision grid.
///
/// Each wall is a quad standing on the edge from `start` to `end` and raised
/// by a height the room chooses when it builds them; its normal is horizontal
/// and perpendicular to the edge. The room's four walls join end to start into
/// one line.
typedef struct {
    SVECTOR start;    // Lower corner the wall starts at
    SVECTOR end;      // Lower corner the wall ends at
    SVECTOR field_10; // Never read; (0, 0, 4096) in every wall. Role and division into fields unproven
} _MineMesaWall;
STATIC_ASSERT_SIZEOF(_MineMesaWall, 0x18);

extern TaskDesc Actor00100_D1BA84;

extern TaskDesc         D_mine_mesa_801818F8;
extern TaskMessageEntry D_mine_mesa_80181904[];
extern TaskDesc         D_mine_mesa_80181990[];

/// The mesa's run: one `SVECTOR` position per frame, sent as an `ActorTransform`.
extern SVECTOR D_mine_mesa_80184184[];

extern EvsCommand D_mine_mesa_80184664[];
extern EvsCommand D_mine_mesa_80184BA4[];
extern EvsCommand D_mine_mesa_80184D9C[];
extern EvsCommand D_mine_mesa_80184FF4[];
extern EvsCommand D_mine_mesa_801850E4[];
extern EvsCommand D_mine_mesa_801854BC[];
extern EvsCommand D_mine_mesa_801856B4[];
extern EvsCommand D_mine_mesa_8018578C[];
extern EvsCommand D_mine_mesa_801861DC[];

/// The mesa's emitter placements, one `SVECTOR` per position, 8 bytes apart.
/// The runs overlap: `864F0`'s fourth and seventh positions are `864F0` itself
/// plus 0x18 and 0x30, which lie inside the `86508` run, and view 8's single
/// position is `864F0[0]` while view 4 draws the same base's 0x30.
extern SVECTOR D_mine_mesa_801864C8[];
extern SVECTOR D_mine_mesa_801864D0[];
extern SVECTOR D_mine_mesa_801864D8[];
extern SVECTOR D_mine_mesa_801864F0[];
extern SVECTOR D_mine_mesa_80186508[];

extern _MineMesaWall       D_mine_mesa_80189A9C[4];
extern _MineMesaSpawnPoint D_mine_mesa_80189AFC[];
extern TaskMessageEntry    D_mine_mesa_80189B1C[2];
extern TaskDesc            D_mine_mesa_80189B2C;
extern RoomFadeStorage     gRoomEventFade;
extern Task*               D_mine_mesa_80189B4C;
extern s32                 D_mine_mesa_80189B50;
extern Task*               D_mine_mesa_80189B58;
extern Enemy*              D_mine_mesa_80189B74[2];

static void _mineMesaUnlinkBattleTriggers(void);
static void _mineMesaResetSceneTaskHandles(void);
static void _mineMesaRebuildVariantWalls(void);

static void _mineMesaPlayerPathTask(Task* task);

static AnimationSet _gMineMesaAnimation046D0;

static AnimationSet _gMineMesaAnimation0494C;
static AnimationSet _gMineMesaAnimation04B54;
static AnimationSet _gMineMesaAnimation04ED8;
static AnimationSet _gMineMesaAnimation0515C;
static AnimationSet _gMineMesaAnimation06160;
static AnimationSet _gMineMesaAnimation0645C;
static AnimationSet _gMineMesaAnimation065FC;
static AnimationSet _gMineMesaAnimation068E4;
static AnimationSet _gMineMesaAnimation06B9C;

static void _mineMesaHoldBlackScreenTask(Task* task);
static void _mineMesaStartMovieTask(Task* task);

extern AnimationPlayRequest      D_mine_mesa_80184360;
extern AnimationPlayRequest      D_mine_mesa_80184374;
extern AnimationPlayRequest      D_mine_mesa_8018439C;
extern AnimationPlayRequest      D_mine_mesa_801844B4;
extern AnimationPlayRequest      D_mine_mesa_801844C8;
extern AnimationPlayRequest      D_mine_mesa_801844DC;
extern AnimationPlayRequest      D_mine_mesa_801844F0;
extern AnimationPlayRequest      D_mine_mesa_80184504;
extern AnimationPlayRequest      D_mine_mesa_8018452C;
extern AnimationPlayRequest      D_mine_mesa_80184540;
extern AnimationPlayRequest      D_mine_mesa_80184554;
extern ActorCommand              D_mine_mesa_80184650;
extern ActorCommand              D_mine_mesa_80184654;
extern AnimationBankCopyRequest  D_mine_mesa_80184344;
extern AnimationBankCopyRequest  D_mine_mesa_80184484;
extern PadScriptCmd              D_mine_mesa_80189A80[4];
extern PadScriptVibrationSegment D_mine_mesa_80189A90[3];
extern ActorTransform            D_mine_mesa_801843C4;
extern ActorTransform            D_mine_mesa_801843DC;
extern ActorTransform            D_mine_mesa_80184590;
extern ActorTransform            D_mine_mesa_801845A8;
extern ActorTransform            D_mine_mesa_801845C0;
static void                      _mineMesaStartEnemyWave(void);
static void                      _mineMesaStageSceneAudioStart(void);
static void                      _mineMesaEnqueueScenePlayback(void);
static void                      _mineMesaFinishScene(void);
static void                      _mineMesaStartPlayerPath(void);
static void                      _mineMesaStartPlayerHeadAim(void);
static void                      _mineMesaSetPlayerHeadAimMode(s32 mode);
static void                      _mineMesaStartCompanionHeadAim(void);
static void                      _mineMesaSetCompanionHeadAimMode(s32 mode);
static void                      _mineMesaStartPlayerHeadBlend(void);
static void                      _mineMesaSuppressAutomaticMusic(void);
static void                      _mineMesaSelectCountdownMusicEntry(u8 countdownEntry);
static void                      _mineMesaStartRunSoundCues(void);
static void                      _mineMesaDismissCompanion(void);
static void                      _mineMesaLockAttachmentsAndCancelEffects(void);
static void                      _mineMesaRequestViewRefresh(void);
static void                      _mineMesaSetSceneCollisionLowered(s32 lowered);

extern AnimationPlayRequest     D_mine_mesa_80184360;
extern AnimationPlayRequest     D_mine_mesa_801843B0;
extern AnimationPlayRequest     D_mine_mesa_801844A0;
extern AnimationPlayRequest     D_mine_mesa_80184518;
extern AnimationPlayRequest     D_mine_mesa_8018452C;
extern AnimationPlayRequest     D_mine_mesa_80184540;
extern AnimationPlayRequest     D_mine_mesa_80184568;
extern AnimationPlayRequest     D_mine_mesa_8018457C;
extern ActorCommand             D_mine_mesa_80184650;
extern AnimationBankCopyRequest D_mine_mesa_80184344;
extern AnimationBankCopyRequest D_mine_mesa_80184484;
extern EvsCommand               D_mine_mesa_8018515C[17];

extern WorldCollisionOccluder     D_mine_mesa_801899B4[2];
extern WorldCollisionTrigger      D_mine_mesa_80188E40[8];
extern WorldCoordRoomAmbientEntry D_mine_mesa_80189954[12];
extern WorldCoordRoomLights       D_mine_mesa_80188E28[1];
extern ActorTransform             D_mine_mesa_801843F4;
extern ActorTransform             D_mine_mesa_80184424;
extern ActorTransform             D_mine_mesa_8018443C;
extern ActorTransform             D_mine_mesa_801845F0;
extern ActorTransform             D_mine_mesa_80184608;
extern ActorTransform             D_mine_mesa_80184620;
extern ActorTransform             D_mine_mesa_80184638;
static void                       _mineMesaCancelScene(void);
static void                       _mineMesaStartFadeFromBlack(s32 holdTicks);
static void                       _mineMesaSetFadeFromBlackState(s32 state);
static void                       _mineMesaSpawnCompanionShotEffects(void);
static void                       _mineMesaHaltVibrationScript(void);

extern WorldCoordPointLight D_mine_mesa_801887C8[8];
extern WorldCoordSpotLight  D_mine_mesa_80188AC8[1];
static s32                  _mineMesaEnemyWaveActorEventMsg(Task* task, s32 messageId, s32 slotIndex, s32 unused);
static void                 _mineMesaEnemyWaveTask(Task* task);

static void           _mineMesaUpdateRoomEventsState(Task* task);
static __inline__ s32 _mineMesaLatchStagedEvent(const RoomEventMsg* destination, const RoomLatchedEvent* event);
static void           _mineMesaInitializeRoomState(Task* task);
static void           _mineMesaSpawnEnemyWaveState(Task* task);

static s32 _mineMesaRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);

/// The room's key-item request; zero replies leave the item unused.
enum { MINE_MESA_MESSAGE_USE_KEY_ITEM = 0x13F1 };
static s32 _mineMesaResolveRoomEventMsg(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _mineMesaRoomCommandMsg(Task* task, s32 messageId, s32 commandId, s32 unused);
static s32 _mineMesaRoomActionMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);
static s32 _mineMesaActorEventMsg(Task* task, s32 messageId, s32 slotIndex, s32 secondArg);

TaskDesc D_mine_mesa_801818F8 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_mine_mesa_80181904[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mineMesaResolveRoomEventMsg },
    { MINE_MESA_MESSAGE_USE_KEY_ITEM, _mineMesaRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mineMesaRoomActionMsg },
    { ROOM_MESSAGE_COMMAND, _mineMesaRoomCommandMsg },
    { ROOM_MESSAGE_ACTOR_EVENT, _mineMesaActorEventMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_mine_mesa_80181934[11] = {
    1,
    1,
    0,
    0,
    0,
    4,
    1,
    0,
    0,
    0,
    0,
};

EvsCommand D_mine_mesa_80181960[2] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_mine_mesa_80181990[2] = {
    { { { TASK_BODY_NONE, 192 } }, _mineMesaStartMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, streamedScenePlay, { .value = 0 } },
};

TaskDesc D_mine_mesa_801819A8 = { { { TASK_BODY_NONE, 192 } }, _mineMesaHoldBlackScreenTask, { .value = 0 } };

static AnimationPackedPose _gMineMesaAnimation046D0Bank1[6] = {
#include "assets/mine_mesa_animation_046D0_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation046D0Bank4[46] = {
#include "assets/mine_mesa_animation_046D0_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation046D0Records[109] = {
#include "assets/mine_mesa_animation_046D0_records.inc"
};

static u16 _gMineMesaAnimation046D0Indices[20] = {
#include "assets/mine_mesa_animation_046D0_indices.inc"
};

static AnimationSet _gMineMesaAnimation046D0 = {
    _gMineMesaAnimation046D0Records,
    _gMineMesaAnimation046D0Indices,
    { NULL, _gMineMesaAnimation046D0Bank1, NULL, NULL, _gMineMesaAnimation046D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineMesaAnimation0494CBank1[2] = {
#include "assets/mine_mesa_animation_0494C_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation0494CBank4[32] = {
#include "assets/mine_mesa_animation_0494C_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation0494CRecords[101] = {
#include "assets/mine_mesa_animation_0494C_records.inc"
};

static u16 _gMineMesaAnimation0494CIndices[20] = {
#include "assets/mine_mesa_animation_0494C_indices.inc"
};

static AnimationSet _gMineMesaAnimation0494C = {
    _gMineMesaAnimation0494CRecords,
    _gMineMesaAnimation0494CIndices,
    { NULL, _gMineMesaAnimation0494CBank1, NULL, NULL, _gMineMesaAnimation0494CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineMesaAnimation04B54Bank1[4] = {
#include "assets/mine_mesa_animation_04B54_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation04B54Bank4[34] = {
#include "assets/mine_mesa_animation_04B54_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation04B54Records[64] = {
#include "assets/mine_mesa_animation_04B54_records.inc"
};

static u16 _gMineMesaAnimation04B54Indices[20] = {
#include "assets/mine_mesa_animation_04B54_indices.inc"
};

static AnimationSet _gMineMesaAnimation04B54 = {
    _gMineMesaAnimation04B54Records,
    _gMineMesaAnimation04B54Indices,
    { NULL, _gMineMesaAnimation04B54Bank1, NULL, NULL, _gMineMesaAnimation04B54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineMesaAnimation04ED8Bank1[6] = {
#include "assets/mine_mesa_animation_04ED8_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation04ED8Bank4[71] = {
#include "assets/mine_mesa_animation_04ED8_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation04ED8Records[116] = {
#include "assets/mine_mesa_animation_04ED8_records.inc"
};

static u16 _gMineMesaAnimation04ED8Indices[20] = {
#include "assets/mine_mesa_animation_04ED8_indices.inc"
};

static AnimationSet _gMineMesaAnimation04ED8 = {
    _gMineMesaAnimation04ED8Records,
    _gMineMesaAnimation04ED8Indices,
    { NULL, _gMineMesaAnimation04ED8Bank1, NULL, NULL, _gMineMesaAnimation04ED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineMesaAnimation0515CBank1[3] = {
#include "assets/mine_mesa_animation_0515C_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation0515CBank4[34] = {
#include "assets/mine_mesa_animation_0515C_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation0515CRecords[98] = {
#include "assets/mine_mesa_animation_0515C_records.inc"
};

static u16 _gMineMesaAnimation0515CIndices[20] = {
#include "assets/mine_mesa_animation_0515C_indices.inc"
};

static AnimationSet _gMineMesaAnimation0515C = {
    _gMineMesaAnimation0515CRecords,
    _gMineMesaAnimation0515CIndices,
    { NULL, _gMineMesaAnimation0515CBank1, NULL, NULL, _gMineMesaAnimation0515CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineMesaAnimation06160Bank1[28] = {
#include "assets/mine_mesa_animation_06160_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation06160Bank4[423] = {
#include "assets/mine_mesa_animation_06160_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation06160Records[498] = {
#include "assets/mine_mesa_animation_06160_records.inc"
};

static u16 _gMineMesaAnimation06160Indices[20] = {
#include "assets/mine_mesa_animation_06160_indices.inc"
};

static AnimationSet _gMineMesaAnimation06160 = {
    _gMineMesaAnimation06160Records,
    _gMineMesaAnimation06160Indices,
    { NULL, _gMineMesaAnimation06160Bank1, NULL, NULL, _gMineMesaAnimation06160Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineMesaAnimation0645CBank1[4] = {
#include "assets/mine_mesa_animation_0645C_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation0645CBank4[56] = {
#include "assets/mine_mesa_animation_0645C_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation0645CRecords[103] = {
#include "assets/mine_mesa_animation_0645C_records.inc"
};

static u16 _gMineMesaAnimation0645CIndices[20] = {
#include "assets/mine_mesa_animation_0645C_indices.inc"
};

static AnimationSet _gMineMesaAnimation0645C = {
    _gMineMesaAnimation0645CRecords,
    _gMineMesaAnimation0645CIndices,
    { NULL, _gMineMesaAnimation0645CBank1, NULL, NULL, _gMineMesaAnimation0645CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineMesaAnimation065FCBank1[2] = {
#include "assets/mine_mesa_animation_065FC_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation065FCBank4[18] = {
#include "assets/mine_mesa_animation_065FC_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation065FCRecords[60] = {
#include "assets/mine_mesa_animation_065FC_records.inc"
};

static u16 _gMineMesaAnimation065FCIndices[20] = {
#include "assets/mine_mesa_animation_065FC_indices.inc"
};

static AnimationSet _gMineMesaAnimation065FC = {
    _gMineMesaAnimation065FCRecords,
    _gMineMesaAnimation065FCIndices,
    { NULL, _gMineMesaAnimation065FCBank1, NULL, NULL, _gMineMesaAnimation065FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineMesaAnimation068E4Bank1[4] = {
#include "assets/mine_mesa_animation_068E4_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation068E4Bank4[45] = {
#include "assets/mine_mesa_animation_068E4_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation068E4Records[109] = {
#include "assets/mine_mesa_animation_068E4_records.inc"
};

static u16 _gMineMesaAnimation068E4Indices[20] = {
#include "assets/mine_mesa_animation_068E4_indices.inc"
};

static AnimationSet _gMineMesaAnimation068E4 = {
    _gMineMesaAnimation068E4Records,
    _gMineMesaAnimation068E4Indices,
    { NULL, _gMineMesaAnimation068E4Bank1, NULL, NULL, _gMineMesaAnimation068E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineMesaAnimation06B9CBank1[2] = {
#include "assets/mine_mesa_animation_06B9C_bank1.inc"
};

static AnimationPackedRotation _gMineMesaAnimation06B9CBank4[39] = {
#include "assets/mine_mesa_animation_06B9C_bank4.inc"
};

static AnimationRecord _gMineMesaAnimation06B9CRecords[109] = {
#include "assets/mine_mesa_animation_06B9C_records.inc"
};

static u16 _gMineMesaAnimation06B9CIndices[20] = {
#include "assets/mine_mesa_animation_06B9C_indices.inc"

};

static AnimationSet _gMineMesaAnimation06B9C = {
    _gMineMesaAnimation06B9CRecords,
    _gMineMesaAnimation06B9CIndices,
    { NULL, _gMineMesaAnimation06B9CBank1, NULL, NULL, _gMineMesaAnimation06B9CBank4, NULL, NULL, NULL },
};

SVECTOR D_mine_mesa_80184184[46] = {
#include "assets/mine_mesa_motion_06BC4.inc"
};

static void _mineMesaCompanionHeadAimTask(Task* task);
static void _mineMesaFadeFromBlackTask(Task* task);
static void _mineMesaPlayerHeadBlendTask(Task* task);
static void _mineMesaRunSoundCuesTask(Task* task);

TaskDesc D_mine_mesa_801842F4[6] = {
    { { { TASK_BODY_NONE, 192 } }, _mineMesaPlayerPathTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mineMesaPlayerHeadAimTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mineMesaCompanionHeadAimTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mineMesaPlayerHeadBlendTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mineMesaFadeFromBlackTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mineMesaRunSoundCuesTask, { .value = 0 } },
};

AnimationSet* D_mine_mesa_8018433C[2] = {
    NULL,
    &_gMineMesaAnimation046D0,
};

AnimationBankCopyRequest D_mine_mesa_80184344 = { { .sets = D_mine_mesa_8018433C }, ARRAY_SIZE(D_mine_mesa_8018433C) };

// Retained data: Same five-field layout as the following animation arguments; retained unreferenced entry.
AnimationPlayRequest D_mine_mesa_8018434C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_80184360 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_80184374 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_mine_mesa_80184388 = { { .index = 1 }, 3, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_8018439C = { { .index = 1 }, 13, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_801843B0 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_mine_mesa_801843C4 = { { 8780, 0, 2700, 0 }, { 0, 785, 0, 0 } };

ActorTransform D_mine_mesa_801843DC = { { 7670, 0, 2200, 0 }, { 0, 785, 0, 0 } };

ActorTransform D_mine_mesa_801843F4 = { { 9550, 0, 1700, 0 }, { 0, -774, 0, 0 } };

// Retained parameter record; layout follows the adjacent script arguments.
ActorTransform D_mine_mesa_8018440C = { { 7770, 0, 2000, 0 }, { 0, 685, 0, 0 } };

ActorTransform D_mine_mesa_80184424 = { { 6280, 0, 5370, 0 }, { 0, 1420, 0, 0 } };

ActorTransform D_mine_mesa_8018443C = { { 9630, 0, 1570, 0 }, { 0, 1024, 0, 0 } };

AnimationSet* D_mine_mesa_80184454[12] = {
    NULL,
    &_gMineMesaAnimation0494C,
    &_gMineMesaAnimation04B54,
    &_gMineMesaAnimation04ED8,
    &_gMineMesaAnimation0515C,
    &_gMineMesaAnimation06160,
    &_gMineMesaAnimation0645C,
    NULL,
    &_gMineMesaAnimation068E4,
    &_gMineMesaAnimation06B9C,
    &_gMineMesaAnimation065FC,
    &_gMineMesaAnimation068E4,
};

AnimationBankCopyRequest D_mine_mesa_80184484 = { { .sets = D_mine_mesa_80184454 }, ARRAY_SIZE(D_mine_mesa_80184454) };

AnimationPlayRequest D_mine_mesa_8018448C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_801844A0 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mine_mesa_801844B4 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_801844C8 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_801844DC = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_801844F0 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_80184504 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_80184518 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_8018452C = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mine_mesa_80184540 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mine_mesa_80184554 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_mesa_80184568 = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mine_mesa_8018457C = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_mine_mesa_80184590 = { { 6320, 0, 1770, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_mine_mesa_801845A8 = { { 6320, 0, 1770, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_mine_mesa_801845C0 = { { 6320, 0, 1770, 0 }, { 0, -1024, 0, 0 } };

// Retained parameter record; layout follows the adjacent script arguments.
ActorTransform D_mine_mesa_801845D8 = { { 4600, 0, 1770, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_mine_mesa_801845F0 = { { 6172, 0, 2800, 0 }, { 0, 2047, 0, 0 } };

ActorTransform D_mine_mesa_80184608 = { { 4820, 0, 2500, 0 }, { 0, 1054, 0, 0 } };

ActorTransform D_mine_mesa_80184620 = { { 6172, 0, 2800, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_mine_mesa_80184638 = { { 6172, 0, 2800, 0 }, { 0, 2048, 0, 0 } };

ActorCommand D_mine_mesa_80184650 = { { .loc = { 4, 1 } }, 0 };

ActorCommand D_mine_mesa_80184654 = { { .loc = { 4, 1 } }, 1 };

ActorCommand D_mine_mesa_80184658 = { { .loc = { 4, 1 } }, 2 };

EvsSceneKey D_mine_mesa_8018465C = { 4, 1, 11 };

EvsCommand D_mine_mesa_80184664[56] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaLockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 50 }, { .value = 50 }, { .value = 50 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801843C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_80184590 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184344 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184484 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018439C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184554 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStartPlayerPath }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStartRunSoundCues }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184374 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStartPlayerHeadBlend }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_TRACK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_801844B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_801844C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801845A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184504 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_801844DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801845C0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_801844F0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54010003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184374 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetSceneCollisionLowered }, { .value = MINE_MESA_SCENE_COLLISION_LOWERED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStartEnemyWave }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaDismissCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_mesa_80184BA4[21] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaLockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801843DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801845A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184344 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184374 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetSceneCollisionLowered }, { .value = MINE_MESA_SCENE_COLLISION_LOWERED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStartEnemyWave }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaDismissCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_mesa_80184D9C[25] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184344 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184484 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184360 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184540 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStartPlayerHeadAim }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStartCompanionHeadAim }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetCompanionHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_TRACK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetCompanionHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018452C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetCompanionHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_mesa_80184FF4[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetCompanionHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_mesa_801850E4[5] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_mine_mesa_80184654 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneAcquireBattleRef }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_mesa_8018515C[17] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_mine_mesa_8018465C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSuppressAutomaticMusic }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_mine_mesa_80184650 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaRequestViewRefresh }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaEnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_mine_mesa_80189A80 }, { .vibrationSegments = D_mine_mesa_80189A90 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaFinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _mineMesaSelectCountdownMusicEntry }, { .value = MINE_MESA_COUNTDOWN_MUSIC_ENTRY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_mine_mesa_801852F4[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetFadeFromBlackState }, { .value = MINE_MESA_FADE_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaCancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184344 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184484 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801843F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801845F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184360 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184568 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _mineMesaSelectCountdownMusicEntry }, { .value = MINE_MESA_COUNTDOWN_MUSIC_ENTRY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetSceneCollisionLowered }, { .value = MINE_MESA_SCENE_COLLISION_NORMAL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_mesa_801854BC[21] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184344 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184484 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184360 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184540 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStartPlayerHeadAim }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_TRACK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaStartCompanionHeadAim }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetCompanionHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_TRACK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetCompanionHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018452C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetCompanionHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_mesa_801856B4[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetPlayerHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetCompanionHeadAimMode }, { .value = MINE_MESA_HEAD_AIM_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_mesa_8018578C[110] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaStartFadeFromBlack }, { .value = 900 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184344 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184484 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184360 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_801844A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_mine_mesa_8018515C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetFadeFromBlackState }, { .value = MINE_MESA_FADE_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_80184424 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_80184608 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184344 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184484 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_801843B0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184518 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_mine_mesa_80184650 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaStartFadeFromBlack }, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54010006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetFadeFromBlackState }, { .value = MINE_MESA_FADE_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaStartFadeFromBlack }, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x40010012 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetFadeFromBlackState }, { .value = MINE_MESA_FADE_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_8018443C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_80184620 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184518 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaSpawnCompanionShotEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_8018457C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaStartFadeFromBlack }, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetFadeFromBlackState }, { .value = MINE_MESA_FADE_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_80184638 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184568 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaStartFadeFromBlack }, { .value = 1110 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetFadeFromBlackState }, { .value = MINE_MESA_FADE_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801843F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801845F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184360 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetSceneCollisionLowered }, { .value = MINE_MESA_SCENE_COLLISION_NORMAL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _mineMesaSelectCountdownMusicEntry }, { .value = MINE_MESA_COUNTDOWN_MUSIC_ENTRY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_mesa_801861DC[24] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaCancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineMesaHaltVibrationScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetFadeFromBlackState }, { .value = MINE_MESA_FADE_CANCEL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801843F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_mesa_801845F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184344 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_mesa_80184484 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184360 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_mesa_80184568 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mineMesaSetSceneCollisionLowered }, { .value = MINE_MESA_SCENE_COLLISION_NORMAL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _mineMesaSelectCountdownMusicEntry }, { .value = MINE_MESA_COUNTDOWN_MUSIC_ENTRY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static SVECTOR _gMineMesaCollision08EE4Normals[3] = {
#include "assets/mine_mesa_collision_08EE4_normals.inc"
};

static SVECTOR _gMineMesaCollision08EE4Verts[8] = {
#include "assets/mine_mesa_collision_08EE4_verts.inc"
};

static WorldCollisionGridFace _gMineMesaCollision08EE4Faces[3] = {
#include "assets/mine_mesa_collision_08EE4_faces.inc"
};

static s16 _gMineMesaCollision08EE4Cells[4] = {
#include "assets/mine_mesa_collision_08EE4_cells.inc"
};

#define GRID_CELL(i) (&_gMineMesaCollision08EE4Cells[i])
static s16* _gMineMesaCollision08EE4Table[1] = {
#include "assets/mine_mesa_collision_08EE4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mine_mesa_801864A4 = { NULL, _gMineMesaCollision08EE4Normals, _gMineMesaCollision08EE4Verts, _gMineMesaCollision08EE4Faces, _gMineMesaCollision08EE4Table, 250, 150, 1, 1, 4000, 3 };

SVECTOR D_mine_mesa_801864C8[1] = {
    { 4000, -650, 4780, 0 },
};

SVECTOR D_mine_mesa_801864D0[1] = {
    { 4000, -650, 3270, 0 },
};

SVECTOR D_mine_mesa_801864D8[3] = {
    { 0x71DE, -3310, 0x3C82, 0 },
    { 0x3A7A, -3350, 2270, 0 },
    { 0x2C4C, -3320, 6720, 0 },
};

SVECTOR D_mine_mesa_801864F0[3] = {
    { 6170, -1710, 7860, 0 },
    { 2550, -3190, 6380, 0 },
    { 5050, -3350, 540, 0 },
};

SVECTOR D_mine_mesa_80186508[4] = {
    { 560, -1930, 380, 0 },
    { -2210, -1890, 5620, 0 },
    { -6860, -1920, 5390, 0 },
    { -4060, -1920, 2820, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_mine_mesa_80186538[1] = {
    { &D_mine_mesa_8018700C, D_mine_mesa_80188E40, D_mine_mesa_801890A0, D_mine_mesa_801899B4 },
};

u8* D_mine_mesa_80186548[1] = {
    gViewIdentityMap,
};

WorldCoordRoomLighting D_mine_mesa_8018654C[1] = {
    { D_mine_mesa_80188E28, D_mine_mesa_80189954 },
};

ViewCount D_mine_mesa_80186554[1] = { 11 };

DirectionWarpEntry D_mine_mesa_80186558[2] = {
    { { { .word = 2048 }, 7060, 0, 2570 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 7060, 0, 2570 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 315, 0, 3562 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 315, 0, 3562 }, { 0, 0, 0, 0 }, 0x54010002, 0x54010001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gMineMesaCollision09A4CNormals[37] = {
#include "assets/mine_mesa_collision_09A4C_normals.inc"
};

static SVECTOR _gMineMesaCollision09A4CVerts[117] = {
#include "assets/mine_mesa_collision_09A4C_verts.inc"
};

static WorldCollisionGridFace _gMineMesaCollision09A4CFaces[53] = {
#include "assets/mine_mesa_collision_09A4C_faces.inc"
};

static s16 _gMineMesaCollision09A4CCells[348] = {
#include "assets/mine_mesa_collision_09A4C_cells.inc"
};

#define GRID_CELL(i) (&_gMineMesaCollision09A4CCells[i])
static s16* _gMineMesaCollision09A4CTable[16] = {
#include "assets/mine_mesa_collision_09A4C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mine_mesa_8018700C = { NULL, _gMineMesaCollision09A4CNormals, _gMineMesaCollision09A4CVerts, _gMineMesaCollision09A4CFaces, _gMineMesaCollision09A4CTable, 800, 4070, 4, 4, 4000, 53 };

ViewCamera D_mine_mesa_80187030[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -6000, 0x34BC, -4000 } }, 257 },
    { { { { 1615, 0, -3764 }, { -1626, 3693, -698 }, { 3394, 1769, 1456 } }, { -1831, 3462, 352 } }, 230 },
    { { { { 4051, 0, -602 }, { -488, 2401, -3281 }, { 353, 3317, 2375 } }, { -0x286F, 4490, -1950 } }, 230 },
    { { { { -1260, 0, 3897 }, { 246, 4087, 79 }, { -3889, 258, -1258 } }, { -0x2E41, 1458, -7495 } }, 230 },
    { { { { 3993, 0, 908 }, { 440, 3582, -1936 }, { -794, 1985, 3493 } }, { -3173, 3846, 2544 } }, 230 },
    { { { { 2243, 0, 3426 }, { 258, 4084, -169 }, { -3416, 308, 2237 } }, { -9512, 1162, -648 } }, 257 },
    { { { { 1893, 0, -3632 }, { -1039, 3924, -541 }, { 3480, 1171, 1814 } }, { 1379, 3414, 1712 } }, 911 },
    { { { { 4007, 0, -846 }, { -480, 3371, -2275 }, { 696, 2325, 3299 } }, { -8537, 4380, 3541 } }, 269 },
    { { { { -802, 0, 4016 }, { -5, 4095, -1 }, { -4016, -6, -802 } }, { -0x3297, 1019, -5195 } }, 541 },
    { { { { 3671, 0, -1816 }, { 548, 3904, 1108 }, { 1731, -1237, 3499 } }, { -0x2B85, 434, -1934 } }, 269 },
    { { { { 639, 0, 4045 }, { 392, 4076, -61 }, { -4026, 396, 636 } }, { -0x33D2, 1931, -1371 } }, 230 },
};

SpriteBatch D_mine_mesa_801871BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_mesa_801871CC[48] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -24, -48, 2094, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, -48, 1954, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, -48, 1938, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, -24, 1755, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -56, -24, 1693, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -72, -24, 1435, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -104, -40, 1393, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -136, -40, 1409, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -88, -16, 1320, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, -16, 1317, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -152, -16, 1427, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -136, 0, 1239, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 0, 1254, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 16, 1118, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -136, 16, 1152, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 32, 1075, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -144, 32, 1100, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 32, 1122, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 48, 1239, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, 48, 1103, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -16, 1274, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -104, 24, 1200, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, 24, 1065, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 40, 1137, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 48, 1064, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 88, 890, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, 48, 987, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 96, 907, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 48, 1002, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, 72, 900, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 80, 825, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, 80, 750, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 104, 675, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, -80, 3082, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, -72, 2993, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -24, -72, 2834, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 0, -72, 2711, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 2604, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, -64, 2488, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 32, 1445, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 32, 24, 1388, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 32, 1383, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 64, 96, 1019, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 104, 96, 1011, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 104, 1006, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, 80, 1099, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 64, 1122, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 48, 1125, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_mesa_8018758C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 0, 0 } },
    { 22, 11, 0, 0, { 3, 0 } },
    { 33, 6, 0, 0, { 2, 0 } },
    { 39, 3, 0, 0, { 4, 0 } },
    { 42, 6, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_mesa_801875C4[39] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 8, 1183, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 96, 908, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 80, 1085, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 80, 931, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 64, 811, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 48, 864, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 32, 902, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 16, 954, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 0, 998, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, -16, 1024, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -16, 1028, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 0, 1008, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 16, 983, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 32, 1002, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 48, 979, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 64, 957, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -96, 1690, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, -120, 1440, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, -80, 1510, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -112, 1210, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -72, 1224, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -24, 1398, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -48, 1507, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -72, 1068, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, -40, 1065, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 8, 1119, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -40, 959, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -8, 943, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 40, 1009, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 40, 1191, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -8, 864, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 24, 846, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 890, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 24, 777, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 56, 713, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 665, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 96, 695, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, 64, 776, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 88, 1051, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_mesa_801878D0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 23, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_mesa_801878F0[18] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 0, 2026, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, -8, 1828, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -24, 1574, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -24, 1686, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -24, 1640, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -24, 1605, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -56, 32, 1235, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 32, 1149, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 32, 1155, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 32, 963, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -8, 971, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, -8, 944, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, -8, 894, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -8, 904, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, -8, 998, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -8, 1101, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -8, 1175, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -8, 1289, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_mesa_80187A58[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_mesa_80187A70[66] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -40, 1440, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -16, 1426, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 16, 1505, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, -24, 1755, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 72, 8, 1591, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 8, 1554, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 120, 8, 1534, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, -8, 1562, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 96, -8, 1514, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 120, -8, 1450, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 72, -32, 1718, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, -40, 1696, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 120, -40, 1519, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 8, 1437, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 0, 1446, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 16, 1363, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, 32, 1401, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 0, 1435, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 1370, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, 32, 1408, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 16, 1434, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, -56, 1229, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -24, 1393, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 8, 1410, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, 24, 1486, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, -64, 1256, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, -72, 1287, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, -80, 1313, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, -56, 1032, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -160, -16, 1075, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, 56, 1075, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -128, -16, 1127, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -128, 40, 1212, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, -48, 1116, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -24, 1315, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 24, 1370, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -160, -88, 1030, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 48, 827, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 152, 104, 697, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 128, 96, 749, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 80, 752, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, 104, 803, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 72, 785, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 104, 842, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 80, 760, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 80, 104, 800, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 88, 866, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -8, 104, 888, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -8, 80, 814, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -8, 56, 797, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, 64, 832, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 80, 894, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 80, 900, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -64, 787, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -16, 781, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -152, 8, 796, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -144, 32, 916, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 32, 908, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -64, 56, 843, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -64, 80, 935, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 72, 980, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 56, 865, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -104, 48, 869, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -104, 72, 966, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, 72, 950, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 832, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_mesa_80187F98[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 8, 0, 0, { 0, 0 } },
    { 21, 16, 0, 0, { 2, 0 } },
    { 37, 29, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_mesa_80187FC8[23] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -8, 1420, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -8, 1273, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -24, 1200, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -8, 1156, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -48, 1191, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 8, -8, 1097, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -56, 1074, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -48, 1152, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, -48, 1236, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 32, -24, 959, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 32, 16, 958, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, -24, 832, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, 0, 850, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, 40, 775, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, 0, 785, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 80, 40, 702, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 0, 653, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 681, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 120, 0, 634, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, 0, 640, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -32, 754, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 104, -32, 691, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, -32, 630, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_mesa_80188194[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_mesa_801881AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_mesa_801881BC[42] = {
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, -72, 2064, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -48, 1903, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -32, 1974, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -88, -72, 2051, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, -64, 1947, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -72, 1814, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, -48, 1843, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, -48, 1873, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -96, -32, 1924, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, -32, 1902, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, -16, 1861, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -32, 1765, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, -32, 1975, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -56, 1779, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -152, -56, 1940, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 48, 1349, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 48, 1381, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 56, 1369, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 56, 1362, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 64, 1375, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 64, 1370, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 64, 1323, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 64, 1346, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 64, 1328, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, -112, 2958, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 32, -112, 2687, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, -104, 2468, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -96, 2317, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -88, 2171, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -80, 2050, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 72, -72, 1939, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 80, -64, 1832, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, -64, 1758, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 96, -56, 1690, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 104, -48, 1590, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -48, 1501, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 8, 1537, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, -24, 1345, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, -40, 1429, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 16, 1494, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 0, 1415, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, -32, 1359, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_mesa_80188504[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, 9, 0, 0, { 2, 0 } },
    { 24, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_mesa_8018852C[24] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 64, 1327, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -120, 72, 1340, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -64, 72, 1580, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 0, 72, 1022, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 112, 1218, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 104 } }, -152, -32, 1064, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, -88, -24, 1043, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 96 } }, -48, -24, 1023, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 104 } }, 16, -32, 1003, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -48, -48, 1778, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -16, -56, 1757, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 16, -56, 1734, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 32, 72, 1329, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -56, 2000, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 64, 96, 1292, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 64, 1296, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 64, 1246, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 56, 16, 1244, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 16, 1436, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 64, 8, 1282, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -16, 1066, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 96, 8, 1966, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -16, 1878, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, -16, 1538, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_mesa_8018870C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_mesa_80188724[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_mesa_80188734[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_mine_mesa_80188744[11] = {
    { { .empty = D_mine_mesa_801871BC }, D_mine_mesa_801871BC, NULL },
    { { .elements = D_mine_mesa_801871CC }, D_mine_mesa_8018758C, NULL },
    { { .elements = D_mine_mesa_801875C4 }, D_mine_mesa_801878D0, NULL },
    { { .elements = D_mine_mesa_801878F0 }, D_mine_mesa_80187A58, NULL },
    { { .elements = D_mine_mesa_80187A70 }, D_mine_mesa_80187F98, NULL },
    { { .elements = D_mine_mesa_80187FC8 }, D_mine_mesa_80188194, NULL },
    { { .empty = D_mine_mesa_801881AC }, D_mine_mesa_801881AC, NULL },
    { { .elements = D_mine_mesa_801881BC }, D_mine_mesa_80188504, NULL },
    { { .elements = D_mine_mesa_8018852C }, D_mine_mesa_8018870C, NULL },
    { { .empty = D_mine_mesa_80188724 }, D_mine_mesa_80188724, NULL },
    { { .empty = D_mine_mesa_80188734 }, D_mine_mesa_80188734, NULL },
};

WorldCoordPointLight D_mine_mesa_801887C8[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7697, -4012, 1391 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1173, 1575, 1726 }, { 0, 0 } }, 1258, 8401 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -812, -1993, 3861 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3194, 2867, 2129 }, { 0, 0 } }, 0, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6812, -1806, 8035 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4014, 3522, 2703 }, { 0, 0 } }, 600, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2536, -1993, 6425 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3768, 3112 }, { 0, 0 } }, 1000, 3600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 930, -1993, 94 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3932, 3440, 2621 }, { 0, 0 } }, 600, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4276, -2312, 970 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3768, 2949 }, { 0, 0 } }, 359, 4501 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2842, -1832, -687 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 3276, 2867 }, { 0, 0 } }, 0, 4702 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x28E1, -1993, 5842 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3850, 3276 }, { 0, 0 } }, 1000, 3200 },
};

WorldCoordSpotLight D_mine_mesa_80188AC8[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -540, 0, -4069 }, { -4075, 0, 534 }, { 0, 4105, 0 } }, { 7565, -1101, 4051 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2 * ONE, 2 * ONE, 2 * ONE }, { 0, 0 } }, { -4061, 533, 0, 0 }, 12000, 12000, 318 },
};

/// Unreferenced bytes following the room's cone light in the overlay image.
///
/// Their original purpose and internal boundaries are unproven. Preserve the
/// representation, including embedded address values, without treating it as lights.
static u8 _gMineMesaUnreferencedData[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x1C,
    0xBC,
    0x18,
    0x80,
    0x01,
    0x00,
    0x00,
    0x00,
    0x1C,
    0xBF,
    0x00,
    0x00,
    0x1C,
    0xC1,
    0x18,
    0x80,
    0x24,
    0xBA,
    0x11,
    0x80,
    0xD8,
    0x17,
    0x07,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x50,
    0x01,
    0x00,
    0x00,
    0xE0,
    0x04,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x05,
    0x00,
    0x00,
    0x03,
    0x21,
    0x62,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x30,
    0x05,
    0xC0,
    0xFF,
    0x90,
    0x0E,
    0x00,
    0x00,
    0x40,
    0xFD,
    0x00,
    0x00,
    0x50,
    0xF0,
    0x00,
    0x00,
    0xC0,
    0x02,
    0x00,
    0x00,
    0x50,
    0xF0,
    0x00,
    0x00,
    0x40,
    0xFD,
    0x00,
    0x00,
    0xB0,
    0x0F,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x60,
    0x2C,
    0xC0,
    0xFF,
    0xE0,
    0x0E,
    0x00,
    0x00,
    0x40,
    0xFD,
    0x00,
    0x00,
    0x50,
    0xF0,
    0x00,
    0x00,
    0xC0,
    0x02,
    0x00,
    0x00,
    0x50,
    0xF0,
    0x00,
    0x00,
    0x40,
    0xFD,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xC0,
    0x02,
    0x00,
    0x00,
    0xB0,
    0x0F,
    0x00,
    0x00,
    0x00,
    0x00,
    0x05,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xE7,
    0x0F,
    0x05,
    0x80,
    0x01,
    0x00,
    0x03,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xF0,
    0xFB,
    0x00,
    0x00,
    0x60,
    0xEF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xA0,
    0x10,
    0x00,
    0x00,
    0x10,
    0x04,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x16,
    0x11,
    0x05,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xD8,
    0x17,
    0x07,
    0x80,
    0xA0,
    0x18,
    0xC0,
    0xFF,
    0xA0,
    0x1E,
    0x00,
    0x00,
    0x60,
    0xEF,
    0x00,
    0x00,
    0xF0,
    0xFB,
    0x00,
    0x00,
    0xA0,
    0x10,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x16,
    0x11,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x68,
    0xC1,
    0x18,
    0x80,
    0xA0,
    0xBF,
    0x00,
    0x00,
    0xD8,
    0x17,
    0x07,
    0x80,
    0x10,
    0x18,
    0x00,
    0x00,
    0x90,
    0x09,
    0x00,
    0x00,
    0x5C,
    0xFB,
    0x00,
    0x00,
    0xF3,
    0xFB,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xEC,
    0xFB,
    0x00,
    0x00,
    0x63,
    0xFB,
    0x00,
    0x00,
    0x13,
    0x04,
    0x00,
    0x00,
    0xA3,
    0x04,
    0x00,
    0x00,
    0x0C,
    0x04,
    0x00,
    0x00,
    0x00,
    0x00,
    0x04,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xB0,
    0xFE,
    0x00,
    0x00,
    0xD0,
    0xFA,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xD0,
    0xFA,
    0x00,
    0x00,
    0xB0,
    0xFE,
    0x00,
    0x00,
    0x30,
    0x05,
    0x00,
    0x00,
    0x50,
    0x01,
    0x00,
    0x00,
    0x30,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x07,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x56,
    0x05,
    0x02,
    0x40,
    0x01,
    0x00,
    0x62,
    0x00,
    0x00,
    0xC2,
    0x18,
    0x80,
    0x68,
    0xC1,
    0x00,
    0x00,
    0xD8,
    0x17,
    0x07,
    0x80,
    0x50,
    0x20,
    0xC0,
    0xFF,
    0x10,
    0x13,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x14,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x0F,
    0x06,
    0x00,
    0x00,
    0x01,
    0x00,
    0x62,
    0x00,
    0x4C,
    0xC2,
    0x00,
    0x00,
    0xB4,
    0xC1,
    0x18,
    0x80,
    0xD8,
    0x17,
    0x07,
    0x80,
    0x40,
    0x20,
    0xC0,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0xC0,
    0xFA,
    0x00,
    0x00,
    0xF0,
    0xFC,
    0x00,
    0x00,
    0x40,
    0x05,
    0x00,
    0x00,
    0xF0,
    0xFC,
    0x00,
    0x00,
    0xC0,
    0xFA,
    0x00,
    0x00,
    0x10,
    0x03,
    0x00,
    0x00,
    0x40,
    0x05,
    0x00,
    0x00,
    0x10,
    0x03,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xD8,
    0x17,
    0x07,
    0x80,
    0x40,
    0x03,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xB0,
    0xFE,
    0x00,
    0x00,
    0xE0,
    0xFD,
    0x00,
    0x00,
    0x50,
    0x01,
    0x00,
    0x00,
    0xE0,
    0xFD,
    0x00,
    0x00,
    0xB0,
    0xFE,
    0x00,
    0x00,
    0x20,
    0x02,
    0x00,
    0x00,
    0x50,
    0x01,
    0x00,
    0x00,
};

WorldCoordRoomLights D_mine_mesa_80188E28[1] = {
    { 0, NULL, ARRAY_SIZE(D_mine_mesa_801887C8), D_mine_mesa_801887C8, ARRAY_SIZE(D_mine_mesa_80188AC8), D_mine_mesa_80188AC8 },
};

WorldCollisionTrigger D_mine_mesa_80188E40[8] = {
    { NULL, NULL, NULL, { 4507, -976, 1532, 0 }, { { -43, -1616, -2266, 0 }, { 31, -1616, 2256, 0 }, { -42, 1616, -2267, 0 }, { 31, 1616, 2255, 0 } }, { 4097, -3, -68, 0 }, { 0, 0, 4096, 0 }, 2769, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4685, -992, 1484, 0 }, { { 33, -1616, 2236, 0 }, { -55, -1616, -2261, 0 }, { 36, 1616, 2236, 0 }, { -54, 1616, -2262, 0 } }, { -4097, 3, 79, 0 }, { 0, 0, 4096, 0 }, 2769, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3859, -960, 6795, 0 }, { { 659, -1616, -2165, 0 }, { -669, -1616, 2158, 0 }, { 658, 1616, -2166, 0 }, { -669, 1616, 2157, 0 } }, { 3917, 1, 1202, 0 }, { 0, 0, 4096, 0 }, 2769, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4019, -960, 6734, 0 }, { { -747, -1616, 2261, 0 }, { 724, -1616, -2275, 0 }, { -747, 1616, 2263, 0 }, { 723, 1616, -2275, 0 } }, { -3897, 0, -1264, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7116, -960, 845, 0 }, { { 6, -1616, -2645, 0 }, { -6, -1616, 2645, 0 }, { 6, 1616, -2644, 0 }, { -5, 1616, 2647, 0 } }, { 4109, -1, 8, 0 }, { 0, 0, 4096, 0 }, 3093, 0, 8, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7247, -928, 947, 0 }, { { 1, -1616, 2546, 0 }, { 1, -1616, -2544, 0 }, { 0, 1616, 2545, 0 }, { 0, 1616, -2544, 0 } }, { -4098, -3, 0, 0 }, { 0, 0, 4096, 0 }, 3007, 0, 2, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9024, -960, 6432, 0 }, { { 334, -1616, 2171, 0 }, { -334, -1616, -2170, 0 }, { 334, 1616, 2170, 0 }, { -334, 1616, -2171, 0 } }, { -4052, 0, 623, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 4, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8880, -896, 6528, 0 }, { { -312, -1616, -2184, 0 }, { 313, -1616, 2186, 0 }, { -311, 1616, -2185, 0 }, { 313, 1616, 2185, 0 } }, { 4055, -3, -582, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 8, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_mine_mesa_801890A0[19] = {
    { NULL, NULL, NULL, { 336, -48, 3872, 0 }, { { -336, 0, -1248, 0 }, { 336, 0, -1248, 0 }, { -336, 0, 1248, 0 }, { 336, 0, 1248, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 1286, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1328, -64, 3728, 0 }, { { -704, 0, -4016, 0 }, { 704, 0, -4016, 0 }, { -704, 0, 4016, 0 }, { 704, 0, 4016, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 4071, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2C60, -64, 3808, 0 }, { { -704, 0, -4016, 0 }, { 704, 0, -4016, 0 }, { -704, 0, 4016, 0 }, { 704, 0, 4016, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 4071, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 6304, -64, 224, 0 }, { { -4256, 0, -1040, 0 }, { 4256, 0, -1040, 0 }, { -4256, 0, 1040, 0 }, { 4256, 0, 1040, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 4374, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 6304, -64, 7840, 0 }, { { -4256, 0, -1040, 0 }, { 4256, 0, -1040, 0 }, { -4256, 0, 1040, 0 }, { 4256, 0, 1040, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 4374, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 6159, -64, 2959, 0 }, { { -1187, 0, -1676, 0 }, { 1181, 0, -1683, 0 }, { -1180, 0, -12, 0 }, { 1188, 0, -19, 0 } }, { 0, 4103, 0, 0 }, { 4096, 0, 0, 0 }, 2048, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9248, -64, 4016, 0 }, { { -336, 0, -1328, 0 }, { 336, 0, -1328, 0 }, { -336, 0, 1328, 0 }, { 336, 0, 1328, 0 } }, { 0, 4103, 0, 0 }, { 4096, 0, 0, 0 }, 1366, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8272, -64, 4880, 0 }, { { -1344, 0, -784, 0 }, { 1344, 0, -784, 0 }, { -1344, 0, 784, 0 }, { 1344, 0, 784, 0 } }, { 0, 4116, 0, 0 }, { 0, 0, 4096, 0 }, 1551, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8256, -64, 3072, 0 }, { { -1344, 0, -784, 0 }, { 1344, 0, -784, 0 }, { -1344, 0, 784, 0 }, { 1344, 0, 784, 0 } }, { 0, 4116, 0, 0 }, { 0, 0, -4096, 0 }, 1551, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 832, -64, 416, 0 }, { { -336, 0, -544, 0 }, { 336, 0, -544, 0 }, { -336, 0, 544, 0 }, { 336, 0, 544, 0 } }, { 0, 4107, 0, 0 }, { 4096, 0, 0, 0 }, 636, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -16, -64, 1560, 0 }, { { -128, 0, -1240, 0 }, { 1184, 0, -1240, 0 }, { 96, 0, 1640, 0 }, { 1888, 0, 264, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1902, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1567, -64, 6527, 0 }, { { -1599, 0, -23, 0 }, { -869, 0, -1111, 0 }, { 870, 0, 1112, 0 }, { 1600, 0, 24, 0 } }, { 0, 4103, 0, 0 }, { 1380, 0, -3857, 0 }, 1598, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5008, -64, 3984, 0 }, { { -1952, 0, -1680, 0 }, { 1952, 0, -1680, 0 }, { -1952, 0, 1680, 0 }, { 1952, 0, 1680, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 2572, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8440, -64, 360, 0 }, { { -2424, 0, 520, 0 }, { -3640, 0, -952, 0 }, { 840, 0, 808, 0 }, { 5224, 0, -376, 0 } }, { 0, 4098, 0, 0 }, { -799, 0, 4017, 0 }, 5221, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2D70, -64, 3456, 0 }, { { -464, 0, -3728, 0 }, { 464, 0, -3728, 0 }, { -464, 0, 3728, 0 }, { 464, 0, 3728, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 3753, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6111, -64, 7503, 0 }, { { -803, 0, -444, 0 }, { 797, 0, -451, 0 }, { -796, 0, 452, 0 }, { 804, 0, 445, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 918, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4992, 0, 5376, 0 }, { { -1952, 0, -272, 0 }, { 1952, 0, -272, 0 }, { -1952, 0, 272, 0 }, { 1952, 0, 272, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, 4096, 0 }, 1970, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4992, 0, 2592, 0 }, { { -1952, 0, -272, 0 }, { 1952, 0, -272, 0 }, { -1952, 0, 272, 0 }, { 1952, 0, 272, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 1970, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3328, 0, 4032, 0 }, { { -272, 0, 1440, 0 }, { -272, 0, -1440, 0 }, { 272, 0, 1440, 0 }, { 272, 0, -1440, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1465, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_mine_mesa_80189644[2] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_mesa_8018965C[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_mesa_80189674[3] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_mesa_80189698[3] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_mesa_801896BC[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_mesa_801896E0[3] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { 37, 37, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_203700_80151DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_mine_mesa_80189704[2] = {
    { 1, 1, 0, 2000, 0, 3960, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_mesa_80189724[5] = {
    { 16, 0, 0, 5900, -1670, 3600, 400, 0, 0, 2, 0 },
    { 16, 0, 0, 6700, -1300, 4600, 1200, 0, 0, 2, 0 },
    { 16, 0, 0, 7800, -1300, 3550, 2600, 0, 0, 2, 0 },
    { 16, 0, 0, 9000, 0, 4200, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_mesa_80189774[7] = {
    { 25, 0, 2, 9200, 0, 6800, 2800, 0, 0, 2, 2 },
    { 25, 0, 2, 9700, 0, 1200, 3300, 0, 0, 2, 2 },
    { 15, 0, 2, 2200, -3000, 7800, 3950, 0, 2, 4, 0 },
    { 15, 0, 2, 0x2EE0, -2500, 7800, 500, 0, 2, 4, 0 },
    { 15, 0, 2, 6300, -1800, 8350, -200, 0, 2, 4, 0 },
    { 15, 0, 2, 0x2EE0, -1500, 200, 700, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_mesa_801897E4[7] = {
    { 25, 0, 1, 0x2968, 0, 1500, 3072, 0, 0, 2, 0 },
    { 25, 0, 1, 8500, 0, 2500, 3072, 0, 0, 2, 0 },
    { 25, 0, 1, 8000, 0, 6000, 3072, 0, 0, 2, 0 },
    { 15, 0, 2, 8700, -2000, 8000, 0, 0, 2, 4, 0 },
    { 15, 0, 2, 5000, -3000, 0, 2048, 0, 2, 4, 0 },
    { 15, 0, 2, 0x2EE0, -2000, 500, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_mesa_80189854[3] = {
    { 20, 3, 1, 7000, 0, 6500, 1024, 0, 0, 2, 0 },
    { 57, 4, 1, 6000, 0, 2000, 1024, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_mesa_80189884[7] = {
    { 25, 0, 1, 0x2968, 0, 1500, 3072, 0, 0, 2, 0 },
    { 25, 0, 1, 8500, 0, 2500, 3072, 0, 0, 2, 0 },
    { 25, 0, 1, 8000, 0, 6000, 3072, 0, 0, 2, 0 },
    { 37, 0, 0, 1600, -1800, 3500, 0, 0, 2, 4, 0 },
    { 37, 0, 0, 2800, -1800, 1400, 0, 0, 2, 4, 0 },
    { 37, 0, 0, 5000, -1800, 6500, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_mine_mesa_801898F4[12] = {
    { NULL, NULL },
    { D_mine_mesa_80189704, D_mine_mesa_80189644 },
    { D_mine_mesa_80189724, D_mine_mesa_8018965C },
    { D_mine_mesa_80189774, D_mine_mesa_80189674 },
    { D_mine_mesa_801897E4, D_mine_mesa_80189698 },
    { NULL, NULL },
    { NULL, NULL },
    { D_mine_mesa_80189854, D_mine_mesa_801896BC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_mine_mesa_80189884, D_mine_mesa_801896E0 },
};

WorldCoordRoomAmbientEntry D_mine_mesa_80189954[12] = {
    { .viewCount = ARRAY_SIZE(D_mine_mesa_80189954) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 700, 900, 850, 818 } },
    { .color = { 200, 300, 320, 265 } },
    { .color = { 460, 460, 650, 483 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionOccluder D_mine_mesa_801899B4[2] = {
    { NULL, NULL, { 6128, -144, 4048, 0 }, { { -2224, 1168, 912, 0 }, { 2224, 1168, -912, 0 }, { -2224, -1167, 912, 0 }, { 2224, -1167, -912, 0 } }, { 1556, 0, 3798, 0 }, 2660, 1, 0 },
    { NULL, NULL, { 6207, -136, 4015, 0 }, { { -2297, 1160, -913, 0 }, { 2298, 1160, 914, 0 }, { -2297, -1160, -913, 0 }, { 2298, -1160, 914, 0 } }, { -1518, 0, 3815, 0 }, 2721, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_mine_mesa_80189A2C = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

WorldCollisionSurfaceProperties D_mine_mesa_80189A38[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mine_mesa_80189A40[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mine_mesa_80189A48[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mine_mesa_80189A2C },
};

WorldCollisionSurfaceProperties D_mine_mesa_80189A50[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mine_mesa_80189A2C },
};

WorldCollisionSurfaceProperties D_mine_mesa_80189A58[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_mine_mesa_80189A60[8] = {
    D_mine_mesa_80189A38,
    D_mine_mesa_80189A40,
    D_mine_mesa_80189A48,
    D_mine_mesa_80189A50,
    D_mine_mesa_80189A58,
    D_mine_mesa_80189A38,
    D_mine_mesa_80189A38,
    D_mine_mesa_80189A38,
};

PadScriptCmd D_mine_mesa_80189A80[4] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_mine_mesa_80189A90[3] = {
    { 0, 0, 1, 0 },
    { 255, 66, 20, 1 },
    { 255, 255, 10, 1 },
};

_MineMesaWall D_mine_mesa_80189A9C[4] = {
    { { 5440, 200, -830, 0 }, { 6620, 200, 340, 0 }, { 0, 0, 4096, 0 } },
    { { 6620, 200, 340, 0 }, { 8680, 200, 460, 0 }, { 0, 0, 4096, 0 } },
    { { 8680, 200, 460, 0 }, { 10040, 200, -230, 0 }, { 0, 0, 4096, 0 } },
    { { 10040, 200, -230, 0 }, { 15000, 200, 610, 0 }, { 0, 0, 4096, 0 } },
};

_MineMesaSpawnPoint D_mine_mesa_80189AFC[4] = {
    { 2247, 0, -7235, 0 },
    { 1247, 0, -6535, 256 },
    { 19000, 0, 4300, -800 },
    { 18700, 0, 5300, -1200 },
};

TaskMessageEntry D_mine_mesa_80189B1C[2] = {
    { ROOM_MESSAGE_ACTOR_EVENT, _mineMesaEnemyWaveActorEventMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_mine_mesa_80189B2C = { { { TASK_BODY_NONE, 32 } }, _mineMesaEnemyWaveTask, { .value = 0 } };

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

s8 D_mine_mesa_80189B48 = 0;

Task* D_mine_mesa_80189B4C = NULL;

s32 D_mine_mesa_80189B50 = 0;

Task* D_mine_mesa_80189B54 = NULL;

Task* D_mine_mesa_80189B58 = NULL;

Task* D_mine_mesa_80189B5C = NULL;

RoomLatchedEvent gRoomEventLatched = { 0 };

s16 MineMesaRemaining = 0;

s16 MineMesaCooldown = 0;

/// Not referenced by the package; its width and role are unproven.
s16 D_mine_mesa_80189B70 = 0;

Enemy* D_mine_mesa_80189B74[2] = {
    NULL,
    NULL,
};

static void _mineMesaInitializeEnemyWaveState(Task* task);
static void _mineMesaFinishEnemyWaveState(Task* task);

#include "../../shared/room_event_staged_task.inc.c"

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Starts the first-arrival or armed companion scene while variant 1 is idle.
///
/// Wheel attachment mode and an active event suspend the checks. First arrival
/// updates its objective and seen flag even without a companion; later ticks
/// consume the armed companion-scene latch. Requires live room/session storage.
/// The room task is unused; this state neither advances nor owns work.
static void _mineMesaUpdateRoomEventsState(Task* task)
{
    enum { EVENT_IDLE              = 0,
           COMPANION_SCENE_VARIANT = 1,
           ARRIVAL_UNSEEN          = 0,
           ARRIVAL_SEEN            = 1,
           OBJECTIVE_MESA_ARRIVAL  = 0x1B,
           COMPANION_SCENE_PLAYED  = 2 };
    u8  variant;
    s32 companionSceneState;

    if ((gGameSession->eventState == EVENT_IDLE) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (variant = gGameSession->location.loc.variant, variant == COMPANION_SCENE_VARIANT)) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_MESA_ARRIVAL_SEEN) == ARRIVAL_UNSEEN) {
            if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
                evsStartScriptWithSkip(D_mine_mesa_8018578C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_mine_mesa_801861DC);
            }
            gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, OBJECTIVE_MESA_ARRIVAL);
            gameFlagSetNibble(GAME_FLAG_MINE_MESA_ARRIVAL_SEEN, ARRIVAL_SEEN);
            return;
        }
        companionSceneState = gameFlagGetNibble(GAME_FLAG_MINE_MESA_0CD);
        if ((companionSceneState == variant) && (D_mine_mesa_80189B50 == companionSceneState)) {
            evsStartScriptWithSkip(D_mine_mesa_80184664, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_mine_mesa_80184BA4);
            D_mine_mesa_80189B50 = COMPANION_SCENE_PLAYED;
        }
    }
}

/// Refuses every key-item request without changing the room or consuming the item.
///
/// The key-item menu interprets the zero reply as unavailable; all arguments
/// are ignored, including the requested inventory item ID.
static s32 _mineMesaRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return 0;
}

/// Tests a staged transition and latches its records for an executing request.
///
/// Borrows the complete destination and event records for this call. Flag zero
/// is always eligible; another flag must name an unseen nibble. Returns 2 when
/// eligible and 1 when already seen. Queries clear the start marker but retain
/// the latched records. Execution copies both records before spawning the staged
/// task and marks the event seen even if spawning fails. Requires room storage
/// and the staged-task resources to remain live through the deferred transition.
static __inline__ s32 _mineMesaLatchStagedEvent(const RoomEventMsg* destination, const RoomLatchedEvent* event)
{
    enum { EVENT_NO_FLAG      = 0,
           EVENT_UNSEEN       = 0,
           EVENT_SEEN         = 1,
           EVENT_NOT_STARTED  = 0,
           EVENT_STARTED      = 1,
           EVENT_ALREADY_SEEN = 1,
           EVENT_ELIGIBLE     = 2 };
    D_mine_mesa_80189B48 = EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == EVENT_UNSEEN || event->flagId == EVENT_NO_FLAG) {
        if (destination->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *destination;
            gRoomEventLatched   = *event;
            if (event->flagId != EVENT_NO_FLAG) {
                gameFlagSetNibble(event->flagId, EVENT_SEEN);
            }
            taskSpawnFromTable(&D_mine_mesa_801818F8, 0, 0, 0);
            D_mine_mesa_80189B48 = EVENT_STARTED;
        }
        return EVENT_ELIGIBLE;
    }
    return EVENT_ALREADY_SEEN;
}

/// Resolves a room destination and gates the staged tunnel-entrance departure.
///
/// Borrows an eight-byte request and writable reply synchronously; they may
/// alias. Copies the request before the Shelter map resolves its variant.
/// Other areas return 1. The tunnel entrance returns 0 during an engaged
/// variant-1 battle, otherwise 2 when the departure scene is eligible or 1
/// when already seen. Query requests resolve and test without starting the
/// scene; execution retains copies, never these borrowed addresses.
/// The receiver and message ID are unused.
static s32 _mineMesaResolveRoomEventMsg(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { EVENT_BLOCKED           = 0,
           EVENT_RESOLVED          = 1,
           COMPANION_SCENE_VARIANT = 1,
           CAP_TUNNEL_DEPARTURE    = 0xE,
           TUNNEL_DEPARTURE_SOUND  = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_MESA, 1),
           EVENT_SKIP_FADE         = 0 };
    RoomLatchedEvent event;
    u8               variant;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId != GAME_AREA_MINE_TUNNEL_ENTRANCE) {
        return EVENT_RESOLVED;
    }
    variant = gGameSession->location.loc.variant;
    if (variant == COMPANION_SCENE_VARIANT && gSceneCombatState.signals.bytes.battlePhase == variant) {
        return EVENT_BLOCKED;
    }
    event.capCmd   = CAP_TUNNEL_DEPARTURE;
    event.stageSnd = TUNNEL_DEPARTURE_SOUND;
    event.flagId   = GAME_FLAG_MESA_TO_TUNNEL_ENTRANCE_SCENE;
    event.fade     = EVENT_SKIP_FADE;
    return _mineMesaLatchStagedEvent(reply, &event);
}

/// Selects the secret-passage dialogue for room command 13.
///
/// Progress below 2 runs CAP command 12; later progress runs command 13,
/// both with transition presentation. All other commands are ignored.
/// The receiver, message ID and second payload are unused. Always returns zero.
static s32 _mineMesaRoomCommandMsg(Task* task, s32 messageId, s32 commandId, s32 unused)
{
    enum { COMMAND_SECRET_PASSAGE_DIALOGUE = 0xD,
           SECRET_PASSAGE_READY            = 2,
           CAP_PASSAGE_PENDING             = 0xC,
           CAP_PASSAGE_READY               = 0xD };
    if (commandId == COMMAND_SECRET_PASSAGE_DIALOGUE) {
        capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_PROGRESS) >= SECRET_PASSAGE_READY ? CAP_PASSAGE_READY : CAP_PASSAGE_PENDING);
    }
    return 0;
}

/// Handles the mesa's battle-boundary and companion-talk actions.
///
/// Borrows a non-NULL `DirectionActionRequest` for synchronous dispatch. Action
/// 1 latches the battle event, selects its objective and removes its automatic
/// triggers; action 2 selects the first or repeat companion conversation until
/// that event is latched. Script starts require a live companion, but the flag
/// changes do not. The request's argument byte is unused. Always returns zero;
/// the receiver, message ID and second payload are unused.
static s32 _mineMesaRoomActionMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    enum { ACTION_BEGIN_BATTLE   = 1,
           ACTION_COMPANION_TALK = 2,
           OBJECTIVE_MESA_BATTLE = 0x1C,
           EVENT_UNSEEN          = 0,
           EVENT_SEEN            = 1 };

    switch (request->actionId) {
        case ACTION_BEGIN_BATTLE:
            if (gameFlagGetNibble(GAME_FLAG_MINE_MESA_TRIGGER_1_SEEN) == EVENT_UNSEEN) {
                if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
                    evsStartScript(D_mine_mesa_801850E4, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                }
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, OBJECTIVE_MESA_BATTLE);
                gameFlagSetNibble(GAME_FLAG_MINE_MESA_TRIGGER_1_SEEN, EVENT_SEEN);
                _mineMesaUnlinkBattleTriggers();
            }
            break;
        case ACTION_COMPANION_TALK:
            if (gameFlagGetNibble(GAME_FLAG_MINE_MESA_TRIGGER_1_SEEN) <= EVENT_UNSEEN) {
                if (gameFlagGetNibble(GAME_FLAG_MINE_MESA_COMPANION_TALK_SEEN) == EVENT_UNSEEN) {
                    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
                        evsStartScriptWithSkip(D_mine_mesa_80184D9C, EVENT_SCRIPT_HUD_KEEP, D_mine_mesa_80184FF4);
                    }
                    gameFlagSetNibble(GAME_FLAG_MINE_MESA_COMPANION_TALK_SEEN, EVENT_SEEN);
                } else if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
                    evsStartScriptWithSkip(D_mine_mesa_801854BC, EVENT_SCRIPT_HUD_KEEP, D_mine_mesa_801856B4);
                }
            }
            break;
    }
    return 0;
}

/// Arms the variant-1 companion scene or forwards an enemy event to the wave.
///
/// On the first event with a companion present, locks attachments, cancels
/// room effects and latches the scene for the room tick. Later events are
/// forwarded synchronously to a live wave controller when present. Its first
/// payload is the enemy's placement slot (0 or 1); the second word is forwarded
/// unchanged (the spawned enemies send zero). Other variants ignore events.
/// The receiver and message ID are unused; always returns zero.
static s32 _mineMesaActorEventMsg(Task* task, s32 messageId, s32 slotIndex, s32 secondArg)
{
    enum { COMPANION_SCENE_VARIANT = 1,
           COMPANION_SCENE_UNSEEN  = 0,
           COMPANION_SCENE_ARMED   = 1 };
    u8 variant;

    variant = gGameSession->location.loc.variant;
    if (variant == COMPANION_SCENE_VARIANT) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_MESA_0CD) == COMPANION_SCENE_UNSEEN) {
            if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
                Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
                roomEffectRequestCancelAll();
                D_mine_mesa_80189B50 = variant;
                gameFlagSetNibble(GAME_FLAG_MINE_MESA_0CD, COMPANION_SCENE_ARMED);
            }
        } else if (D_mine_mesa_80189B4C != NULL) {
            taskMessageDispatch(D_mine_mesa_80189B4C, ROOM_MESSAGE_ACTOR_EVENT, slotIndex, secondArg);
        }
    }
    return 0;
}

/// Registers the room and prepares arrival presentation and variant collision.
///
/// State 0 installs the message table and room slot. An unseen arrival seeds
/// companion HP and starts the movie launcher when a companion exists, and
/// hides the water map marker. A seen arrival unlinks the battle triggers.
/// Selects scene music, resets borrowed scene/wave handles, rebuilds the walls
/// and advances to event checks. Previous room tasks must already be retired;
/// room resources and the active collision grid must remain live.
static void _mineMesaInitializeRoomState(Task* task)
{
    enum { ARRIVAL_UNSEEN          = 0,
           ARRIVAL_COMPANION_HP    = 5,
           MOVIE_LAUNCH_ENTRY      = 0,
           MAP_MARK_HIDDEN         = 0,
           SCENE_MUSIC_ENTRY       = 1,
           COMPANION_SCENE_UNARMED = 0 };
    task->msgTable = D_mine_mesa_80181904;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_MINE_MESA_ARRIVAL_SEEN) == ARRIVAL_UNSEEN) {
        if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp = ARRIVAL_COMPANION_HP;
            taskSpawnFromTable(D_mine_mesa_80181990, MOVIE_LAUNCH_ENTRY, 0, 0);
        }
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_WATER, MAP_MARK_HIDDEN);
    } else {
        _mineMesaUnlinkBattleTriggers();
    }
    gStageSceneMusicEntry = SCENE_MUSIC_ENTRY;
    _mineMesaResetSceneTaskHandles();
    D_mine_mesa_80189B4C = NULL;
    _mineMesaRebuildVariantWalls();
    task->state          = task->state + 1;
    D_mine_mesa_80189B50 = COMPANION_SCENE_UNARMED;
}

/// Removes the four automatic battle-action triggers from the live room lists.
///
/// Requires Mine Mesa's trigger storage to remain live. Entries 1..4 are the
/// automatic action-1 quads; the warp at entry 0 and later interactions remain
/// linked. Removal is repeatable and retains each trigger's geometry.
static void _mineMesaUnlinkBattleTriggers(void)
{
    enum { FIRST_BATTLE_TRIGGER = 1 };

    worldCollisionUnlinkTrigger(0, &D_mine_mesa_801890A0[FIRST_BATTLE_TRIGGER]);
    worldCollisionUnlinkTrigger(0, &D_mine_mesa_801890A0[FIRST_BATTLE_TRIGGER + 1]);
    worldCollisionUnlinkTrigger(0, &D_mine_mesa_801890A0[FIRST_BATTLE_TRIGGER + 2]);
    worldCollisionUnlinkTrigger(0, &D_mine_mesa_801890A0[FIRST_BATTLE_TRIGGER + 3]);
}

/// State handlers of the room task `mineMesaRoomTask` drives: the
/// set-up tick, the per-frame tick and `taskKill`.
static const TaskFuncTable3 D_mine_mesa_8017D5D8 = {
    { _mineMesaInitializeRoomState, _mineMesaUpdateRoomEventsState, taskKill },
};

void mineMesaRoomTask(Task* task)
{
    TaskFuncTable3 states = D_mine_mesa_8017D5D8;

    states.funcs[task->state](task);
}

/// Starts the ten-kill wave once while the room's controller handle is NULL.
///
/// Borrows the spawned bodyless controller until room exit; allocation failure
/// leaves NULL so a later call can retry. Requires the mesa actor resources.
/// The retained handle is not cleared at wave teardown: call only during this
/// room entry, before the completed controller can be collected or reused.
static void _mineMesaStartEnemyWave(void)
{
    if (D_mine_mesa_80189B4C == NULL) {
        D_mine_mesa_80189B4C = taskSpawnFromTable(&D_mine_mesa_80189B2C, 0, 0, 0);
    }
}

#include "../../shared/streamed_scene_play.inc.c"

/// Keeps the screen black until its signed halfword timer reaches 256.
///
/// Advances the timer by four per tick (64 ticks from zero), drawing on the
/// final tick before releasing the task. This task needs no body or work.
static void _mineMesaHoldBlackScreenTask(Task* task)
{
    enum { BLACK_LEVEL       = 255,
           BLACK_TIMER_STEP  = 4,
           BLACK_TIMER_LIMIT = 256 };

    u16 timerBits;

    fadeDrawOverlay(BLACK_LEVEL, BLACK_LEVEL, BLACK_LEVEL, GPU_BLEND_SUBTRACT);
    timerBits           = task->killCountdown + BLACK_TIMER_STEP;
    task->killCountdown = timerBits;
    if ((s16)timerBits >= BLACK_TIMER_LIMIT) {
        taskKill(task);
    }
}

/// Hands display presentation to the room's streamed movie and releases the launcher.
///
/// The current room's movie slot and game-resource restore state must be ready.
/// Entry 1 of the movie table runs playback on the display task list. Queues
/// the current camera packets and selects task-only flipping even if spawning
/// fails; this one-shot launcher owns no body or work.
static void _mineMesaStartMovieTask(Task* task)
{
    enum { MOVIE_PLAYBACK_ENTRY = 1 };

    displaySpawnTaskFromTable(D_mine_mesa_80181990, MOVIE_PLAYBACK_ENTRY, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

/// Places the player at successive samples of the room's scripted path.
///
/// `killCountdown` is a nonnegative sample index, initially zero. Each tick
/// sends a borrowed `ActorTransform` synchronously, offset by (-100, 0, 200)
/// game-coordinate units and facing yaw 785 (4096 units per turn). Exhausting
/// the 46 samples or skipping the event releases the task. The player must be live.
static void _mineMesaPlayerPathTask(Task* task)
{
    enum { PATH_X_OFFSET = -100,
           PATH_Z_OFFSET = 200,
           PATH_YAW      = 785 };
    ActorTransform placement;

    if (task->killCountdown >= (s32)ARRAY_SIZE(D_mine_mesa_80184184) || gGameSession->evtSkipped != 0) {
        taskKill(task);
        return;
    }
    placement.pos.vx  = D_mine_mesa_80184184[task->killCountdown].vx;
    placement.pos.vy  = D_mine_mesa_80184184[task->killCountdown].vy;
    placement.pos.vz  = D_mine_mesa_80184184[task->killCountdown].vz;
    placement.pos.vx += PATH_X_OFFSET;
    placement.pos.vz += PATH_Z_OFFSET;
    placement.rot.vx  = 0;
    placement.rot.vy  = PATH_YAW;
    placement.rot.vz  = 0;
    task->killCountdown++;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &placement, 0);
}

/// Fades retained head aiming in or out by 256 Q12 units per call.
///
/// Borrows writable `aim`; its rate normally lies in 0..ONE (4096). Nonzero
/// `tracking` selects ONE, zero selects zero. The rate is the fraction of the
/// current-to-target head rotation applied on an active tick. Signed halfword
/// narrowing precedes endpoint clamping; the other aim fields stay intact.
static inline void _mineMesaRampHeadAimRate(AnimationHeadAim* aim, s32 tracking)
{
    s16 nextRate;

    if (tracking != 0) {
        nextRate  = aim->rate + MINE_MESA_HEAD_AIM_RATE_STEP;
        aim->rate = nextRate;
        if (nextRate > ONE) {
            aim->rate = ONE;
        }
    } else {
        nextRate  = aim->rate - MINE_MESA_HEAD_AIM_RATE_STEP;
        aim->rate = nextRate;
        if (nextRate < 0) {
            aim->rate = 0;
        }
    }
}

/// Turns the player's head toward the companion with retained pitch-wrap history.
///
/// Both tasks need live TMD models with five root-to-head coordinates. The
/// script freeze gate suspends updates. Initialization owns zeroed
/// `AnimationHeadAim` work, sets yaw/pitch limits to 768/512 angle units
/// (4096 per turn), and updates immediately. Nonzero `spawnArg1.value` ramps
/// the Q12 rate toward ONE; zero ramps toward zero, by ONE/16 per active tick.
/// Missing tasks, allocation failure or other states release the task and
/// clear its tracked handle; task teardown frees the work.
static void _mineMesaPlayerHeadAimTask(Task* task)
{
    Task*             playerTask;
    Task*             companionTask;
    AnimationHeadAim* aim;
    s32               state;

    playerTask    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (D_801156F9 == 0) {
        if ((playerTask == NULL) || (companionTask == NULL)) {
            task->state = MINE_MESA_HEAD_AIM_CANCEL;
        }
        state = task->state;
        switch (state) {
            case MINE_MESA_HEAD_AIM_INITIALIZE:
                aim = memCalloc(sizeof(AnimationHeadAim), false);
                if (aim != NULL) {
                    task->work      = aim;
                    aim->yawLimit   = MINE_MESA_HEAD_AIM_YAW_LIMIT;
                    aim->pitchLimit = MINE_MESA_PLAYER_HEAD_AIM_PITCH_LIMIT;
                    task->state++;
                        /* fallthrough */
                    case MINE_MESA_HEAD_AIM_UPDATE:
                        aim = task->work;
                        _mineMesaRampHeadAimRate(aim, task->spawnArg1.value);
                        animationAimHeadAt(playerTask, companionTask, aim);
                        return;
                }
                /* fallthrough */
            default:
                taskKill(task);
                D_mine_mesa_80189B54 = NULL;
                break;
        }
    }
}

/// Turns the companion's head toward the player with retained pitch-wrap history.
///
/// The script freeze gate suspends updates. Initialization owns zeroed
/// `AnimationHeadAim` work, sets yaw/pitch limits to 768/256 angle units
/// (4096 per turn), and updates immediately. Nonzero `spawnArg1.value` ramps
/// the Q12 rate toward ONE; zero ramps toward zero, by ONE/16 per active tick.
/// Both models need five root-to-head coordinates and the player must be live:
/// only the companion is checked. A missing companion, allocation failure or
/// other state releases the task, frees its work and clears its tracked handle.
static void _mineMesaCompanionHeadAimTask(Task* task)
{
    Task*             companionTask;
    AnimationHeadAim* aim;

    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (D_801156F9 == 0) {
        if (companionTask == NULL) {
            task->state = MINE_MESA_HEAD_AIM_CANCEL;
        }
        switch (task->state) {
            case MINE_MESA_HEAD_AIM_INITIALIZE:
                aim = memCalloc(sizeof(AnimationHeadAim), false);
                if (aim != NULL) {
                    task->work      = aim;
                    aim->yawLimit   = MINE_MESA_HEAD_AIM_YAW_LIMIT;
                    aim->pitchLimit = MINE_MESA_COMPANION_HEAD_AIM_PITCH_LIMIT;
                    task->state++;
                        /* fallthrough */
                    case MINE_MESA_HEAD_AIM_UPDATE:
                        aim = task->work;
                        _mineMesaRampHeadAimRate(aim, task->spawnArg1.value);
                        animationAimHeadAt(companionTask, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), aim);
                        return;
                }
                /* fallthrough */
            default:
                taskKill(task);
                D_mine_mesa_80189B58 = NULL;
                break;
        }
    }
}

/// Queues a centred 320x240 subtractive tile and its blend command at OT slot 3.
///
/// Byte RGB values are channel amounts subtracted from the frame, with 255 on
/// all channels producing black. Requires an initialized frame arena with room
/// for a TILE and a DR_TPAGE; both packets remain live until GPU drawing ends.
static inline void _mineMesaDrawBlackOverlay(u8 red, u8 green, u8 blue)
{
    enum { OVERLAY_WIDTH        = 320,
           OVERLAY_HEIGHT       = 240,
           OVERLAY_OT_INDEX     = 3,
           OVERLAY_BLEND_PAGE_X = 320 };
    TILE*     tile;
    DR_TPAGE* drawMode;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    SetSemiTrans(tile, 1);
    tile->x0 = -OVERLAY_WIDTH / 2;
    tile->y0 = -OVERLAY_HEIGHT / 2;
    tile->w  = OVERLAY_WIDTH;
    tile->h  = OVERLAY_HEIGHT;
    setRGB0(tile, red, green, blue);
    addPrim(gGpuCurrentOt + OVERLAY_OT_INDEX, tile);
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 1, 0, getTPage(0, GPU_BLEND_SUBTRACT, OVERLAY_BLEND_PAGE_X, 0));
    addPrim(gGpuCurrentOt + OVERLAY_OT_INDEX, drawMode);
}

/// Holds a black screen for the spawn delay, then reveals it eight shade levels per tick.
///
/// State 0 seeds the shade to 255; state 1 consumes `spawnArg1.value` until
/// it is negative; state 2 reduces the signed halfword shade and kills the
/// task when it becomes negative. Other states cancel it. Every tick queues
/// the shade sampled before the state update, including initialization and
/// teardown ticks. Requires the current frame arena and OT slot 3.
static void _mineMesaFadeFromBlackTask(Task* task)
{
    enum { FADE_INITIALIZE,
           FADE_HOLD,
           FADE_REVEAL,
           FADE_BLACK_LEVEL = 255,
           FADE_SHADE_STEP  = 8 };

    u8 red, green, blue;

    // Sample before updating: the final decrement still draws the preceding shade.
    red = green = blue = task->killCountdown;
    switch (task->state) {
        case FADE_INITIALIZE:
            task->killCountdown = FADE_BLACK_LEVEL;
            task->state++;
            break;
        case FADE_HOLD:
            if (--task->spawnArg1.value < 0) {
                task->state++;
            }
            break;
        case FADE_REVEAL:
            task->killCountdown -= FADE_SHADE_STEP;
            if (task->killCountdown < 0) {
                taskKill(task);
            }
            break;
        default:
            taskKill(task);
            break;
    }
    _mineMesaDrawBlackOverlay(red, green, blue);
}

/// Stages the selected scene's deferred audio-start request for the room script.
///
/// Scene selection and prepared buffers must survive request consumption.
/// A later CD replacement commit enqueues it; this callback does not start playback.
static void _mineMesaStageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Enqueues playback of the room script's selected scene/audio session.
///
/// Requires prepared playback storage to remain live through the queued request.
static void _mineMesaEnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes scene streaming and restores the random state saved at scene selection.
///
/// Requires a successfully selected scene. Playback buffers and tasks retain their
/// owners; this callback clears streaming state without releasing that storage.
static void _mineMesaFinishScene(void)
{
    streamFinishScene();
}

/// Cancels the room script's selected scene and pending CD request.
///
/// Discards the deferred replacement and restores scene/random state immediately;
/// CD cancellation completes on later dispatches. Requires prior scene selection.
static void _mineMesaCancelScene(void)
{
    cdCmdCancelScene();
}

/// Starts the scene's 46-tick player path at its first sample.
///
/// Requires the player task and mesa path data to remain live until completion
/// or event skip. The bodyless path task owns no work; no handle is retained.
static void _mineMesaStartPlayerPath(void)
{
    enum { PLAYER_PATH_ENTRY = 0 };
    taskSpawnFromTable(D_mine_mesa_801842F4, PLAYER_PATH_ENTRY, 0, 0);
}

/// Starts and tracks the player's retained head aim toward the companion.
///
/// The bodyless task begins with tracking disabled and allocates its own aim
/// work. Both actor models must have live head coordinates before an active
/// tick. Call with no previous player-head task running; this replaces the
/// borrowed handle without releasing an earlier task.
static void _mineMesaStartPlayerHeadAim(void)
{
    enum { PLAYER_HEAD_AIM_ENTRY = 1 };
    D_mine_mesa_80189B54 = taskSpawnFromTable(D_mine_mesa_801842F4, PLAYER_HEAD_AIM_ENTRY, 0, 0);
}

/// Controls the tracked player head-aim task, if one is live.
///
/// `mode` 0 releases the blend and 1 enables tracking; every other value kills
/// the task and clears its handle. The player's handle can also hold the direct
/// head-blend task. Killing a retained-aim task releases its owned work.
static void _mineMesaSetPlayerHeadAimMode(s32 mode)
{
    Task* headAimTask = D_mine_mesa_80189B54;

    if (headAimTask == NULL) {
        return;
    }
    if (mode < MINE_MESA_HEAD_AIM_MODE_COUNT) {
        if (mode >= MINE_MESA_HEAD_AIM_RELEASE) {
            headAimTask->spawnArg1.value = mode;
            return;
        }
    }
    taskKill(D_mine_mesa_80189B54);
    D_mine_mesa_80189B54 = NULL;
}

/// Starts and tracks the companion's retained head aim toward the player.
///
/// The bodyless task begins with tracking disabled and owns its aim work.
/// Both actor models must have live head coordinates before an active tick.
/// Call with no previous companion-head task running; this replaces the
/// borrowed handle without releasing an earlier task.
static void _mineMesaStartCompanionHeadAim(void)
{
    enum { COMPANION_HEAD_AIM_ENTRY = 2 };
    D_mine_mesa_80189B58 = taskSpawnFromTable(D_mine_mesa_801842F4, COMPANION_HEAD_AIM_ENTRY, 0, 0);
}

/// Controls the tracked companion head-aim task, if one is live.
///
/// `mode` 0 releases the blend and 1 enables tracking; every other value kills
/// the task and clears its handle. Killing a retained-aim task releases its owned work.
static void _mineMesaSetCompanionHeadAimMode(s32 mode)
{
    if (D_mine_mesa_80189B58 != NULL) {
        if (mode < MINE_MESA_HEAD_AIM_MODE_COUNT) {
            if (mode >= MINE_MESA_HEAD_AIM_RELEASE) {
                D_mine_mesa_80189B58->spawnArg1.value = mode;
                return;
            }
        }
        taskKill(D_mine_mesa_80189B58);
        D_mine_mesa_80189B58 = NULL;
    }
}

/// Replaces the tracked player-head task with direct companion-facing blend.
///
/// Releases any prior player-head task and its work before retaining the new
/// bodyless task. Blend starts at zero with tracking disabled. Both actor
/// models must have live head coordinates; the new task owns no work.
static void _mineMesaStartPlayerHeadBlend(void)
{
    enum { PLAYER_HEAD_BLEND_ENTRY = 3 };
    if (D_mine_mesa_80189B54 != NULL) {
        taskKill(D_mine_mesa_80189B54);
    }
    D_mine_mesa_80189B54 = taskSpawnFromTable(D_mine_mesa_801842F4, PLAYER_HEAD_BLEND_ENTRY, 0, 0);
}

/// Fades direct head aiming in or out by 256 Q12 units per call.
///
/// Borrows a live head-blend `task`, whose signed halfword `killCountdown`
/// stores a Q12 weight normally in 0..ONE (4096). Nonzero `tracking` selects
/// ONE, zero selects zero. This storage is a blend fraction, not elapsed time;
/// signed halfword narrowing precedes endpoint clamping.
static inline void _mineMesaRampHeadBlendWeight(Task* task, s32 tracking)
{
    s16 nextWeight;

    if (tracking != 0) {
        nextWeight          = task->killCountdown + MINE_MESA_HEAD_AIM_RATE_STEP;
        task->killCountdown = nextWeight;
        if (nextWeight > ONE) {
            task->killCountdown = ONE;
        }
    } else {
        nextWeight          = task->killCountdown - MINE_MESA_HEAD_AIM_RATE_STEP;
        task->killCountdown = nextWeight;
        if (nextWeight < 0) {
            task->killCountdown = 0;
        }
    }
}

/// Blends the player's head toward the companion without retained pitch history.
///
/// Both tasks need live TMD models with five root-to-head coordinates. The
/// script freeze gate suspends updates. State 0 uses `killCountdown` as a Q12
/// weight, normally 0..ONE: nonzero `spawnArg1.value` ramps up by ONE/16;
/// zero ramps down by ONE/16. Halfword truncation precedes signed clamping.
/// Yaw/pitch limits are 768/16 angle units (4096 per turn), widened to preserve
/// the existing pose. Missing tasks or other states kill and clear the player
/// head-aim handle. This task owns no work block.
static void _mineMesaPlayerHeadBlendTask(Task* task)
{
    Task* playerTask;
    Task* companionTask;

    playerTask    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (D_801156F9 == 0) {
        if ((playerTask == NULL) || (companionTask == NULL)) {
            task->state = MINE_MESA_HEAD_AIM_CANCEL;
        }
        if (task->state == MINE_MESA_HEAD_BLEND_ACTIVE) {
            _mineMesaRampHeadBlendWeight(task, task->spawnArg1.value);
            animationAimHeadAtTask(playerTask, companionTask, MINE_MESA_HEAD_AIM_YAW_LIMIT, MINE_MESA_PLAYER_HEAD_BLEND_PITCH_LIMIT, task->killCountdown);
            return;
        }
        taskKill(task);
        D_mine_mesa_80189B54 = NULL;
    }
}

/// Covers the screen immediately and starts the tracked black-screen fade.
///
/// `holdTicks` is the signed delay consumed after initialization, one per hold
/// tick until negative; scripts use 300, 900 or 1110. The subsequent reveal
/// decreases shade by eight per tick. Requires the frame arena and no earlier
/// tracked fade running; replacing the handle does not release that fade.
/// The bodyless task owns no work; allocation failure still draws this overlay.
static void _mineMesaStartFadeFromBlack(s32 holdTicks)
{
    enum { FADE_FROM_BLACK_ENTRY = 4,
           BLACK_LEVEL           = 255 };
    D_mine_mesa_80189B5C = taskSpawnFromTable(D_mine_mesa_801842F4, FADE_FROM_BLACK_ENTRY, holdTicks, 0);
    fadeDrawOverlay(BLACK_LEVEL, BLACK_LEVEL, BLACK_LEVEL, GPU_BLEND_SUBTRACT);
}

/// Sets the tracked black-screen fade task's state when its handle is non-NULL.
///
/// States 0/1/2 initialize, hold and reveal; any other value requests task
/// release on its next tick. The full signed state word is stored unchanged;
/// room scripts pass -1 to cancel the overlay.
/// Requires a live task for a non-NULL handle; fade teardown leaves it unchanged.
static void _mineMesaSetFadeFromBlackState(s32 state)
{
    if (D_mine_mesa_80189B5C != NULL) {
        D_mine_mesa_80189B5C->state = state;
    }
}

/// Keeps automatic area and countdown music requests from replacing scene music.
///
/// Sets both session skip flags for subsequent requests; neither starts nor
/// stops sound. Requires the current session to remain live.
static void _mineMesaSuppressAutomaticMusic(void)
{
    gGameSession->flowFlags |= (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
}

/// Selects the room script's countdown-music entry for a later music request.
///
/// `countdownEntry` must fit the active stage's countdown-music table. This byte setter
/// neither loads nor starts music; the room scripts select entry 2.
static void _mineMesaSelectCountdownMusicEntry(u8 countdownEntry)
{
    gStageSceneMusicEntry = countdownEntry;
}

/// Starts the run scene's timed sound cues at elapsed tick zero.
///
/// Plays the loaded type-1 bank's cues on ticks 47 and 57; event completion can
/// end it early. The bodyless task owns no work and no handle is retained.
static void _mineMesaStartRunSoundCues(void)
{
    enum { RUN_SOUND_CUES_ENTRY = 5 };
    taskSpawnFromTable(D_mine_mesa_801842F4, RUN_SOUND_CUES_ENTRY, 0, 0);
}

/// Plays the run scene's two sound cues on ticks 47 and 57, then releases the task.
///
/// `killCountdown` starts at zero and advances as a signed halfword timer.
/// Ending the scripted event releases the task early, after that tick's cue
/// check. Requests use the loaded type-1 sound bank with no pan or attenuation.
static void _mineMesaRunSoundCuesTask(Task* task)
{
    enum { RUN_FIRST_CUE_TICK = 47,
           RUN_LAST_CUE_TICK  = 57,
           RUN_FIRST_SOUND_ID = 0x10000039,
           RUN_LAST_SOUND_ID  = 0x1000003A };

    u16 elapsedTickBits;

    elapsedTickBits     = task->killCountdown + 1;
    task->killCountdown = elapsedTickBits;
    switch ((s16)elapsedTickBits) {
        case RUN_FIRST_CUE_TICK:
            sndEvtRequestScriptStart(RUN_FIRST_SOUND_ID, 0, 0);
            break;
        case RUN_LAST_CUE_TICK:
            sndEvtRequestScriptStart(RUN_LAST_SOUND_ID, 0, 0);
            break;
    }
    if ((gGameSession->eventState == 0) || ((s16)task->killCountdown >= RUN_LAST_CUE_TICK)) {
        taskKill(task);
    }
}

/// Removes the scheduled companion after the mesa's scripted departure.
///
/// A nonzero family-1 schedule requires a live companion task and exit callback.
/// Clears that schedule and the saved companion selection, calls the task's
/// exit callback, then clears its session slot. A zero schedule does nothing.
static void _mineMesaDismissCompanion(void)
{
    enum { COMPANION_SCHEDULE_NONE = 0,
           COMPANION_TYPE_NONE     = 0 };

    if (gameFlagGetNibble(GAME_FLAG_COMPANION_1_SCHEDULE) != COMPANION_SCHEDULE_NONE) {
        gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_SCHEDULE_NONE);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType = COMPANION_TYPE_NONE;
        taskCallExit(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION));
        gameSetTaskSlot(NULL, GAME_TASK_SLOT_COMPANION);
    }
}

/// Locks attachment selection and requests cancellation of every active room effect.
///
/// Used at scripted scene entry; requires the live room-effect controller.
/// Cancellation is serviced by effect tasks on later ticks.
static void _mineMesaLockAttachmentsAndCancelEffects(void)
{
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    roomEffectRequestCancelAll();
}

/// Requests a deferred respawn of view tasks using the saved camera view.
static void _mineMesaRequestViewRefresh(void)
{
    gGameSession->viewDirty = 1;
}

/// Adds the companion's handgun muzzle flash and attack sound to a scene shot.
///
/// A missing companion does nothing. A live companion must have a loaded TMD
/// with coordinate 8 valid for the flash's lifetime and the attack sound bank
/// loaded. The flash is an independent counted effect; this retains no handle.
static void _mineMesaSpawnCompanionShotEffects(void)
{
    enum { MUZZLE_COORD_INDEX       = 8,
           NPC_PISTOL_FLASH_PROFILE = 33 };
    Task* companionTask;

    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (companionTask != NULL) {
        effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, &companionTask->extra.tmd->coords[MUZZLE_COORD_INDEX], NPC_PISTOL_FLASH_PROFILE, NULL);
        sndEvtRequestScriptStart(SOUND_ACTOR_800100_ATTACK, 0, 0);
    }
}

/// Stops vibration requests and asks the scene's vibration tasks to release themselves.
///
/// Script and motor teardown occurs on eligible callbacks, which actor freeze
/// can delay. Used by the arrival scene's skip path.
static void _mineMesaHaltVibrationScript(void)
{
    padScriptHalt();
}

/// Clears the player-head, companion-head and black-screen task handles on room entry.
///
/// This resets tracking only; it does not terminate tasks. Previous room tasks
/// must already be retired before entry initialization.
static void _mineMesaResetSceneTaskHandles(void)
{
    D_mine_mesa_80189B54 = NULL;
    D_mine_mesa_80189B58 = NULL;
    D_mine_mesa_80189B5C = NULL;
}

/// Restores the live grid's reserved three-face mesh from its template.
///
/// Both grids must cover three normals/faces and eight vertices. Copy XYZ
/// only for vectors, retaining destination pad words; faces copy completely.
/// Pointer arguments must be stable, side-effect-free `WorldCollisionGrid*`
/// expressions, with disjoint source and destination pools. `elementIndex`
/// is a writable s32 lvalue evaluated repeatedly and left at eight. The fixed
/// template array extents select the copy lengths. Use as a complete statement.
#define MINE_MESA_RESTORE_SCENE_COLLISION(liveGrid, templateGrid, elementIndex)                                       \
    {                                                                                                                 \
        for ((elementIndex) = 0; (elementIndex) < (s32)ARRAY_SIZE(_gMineMesaCollision08EE4Faces); (elementIndex)++) { \
            (liveGrid)->normals[(elementIndex)].vx = (templateGrid)->normals[(elementIndex)].vx;                      \
            (liveGrid)->normals[(elementIndex)].vy = (templateGrid)->normals[(elementIndex)].vy;                      \
            (liveGrid)->normals[(elementIndex)].vz = (templateGrid)->normals[(elementIndex)].vz;                      \
            (liveGrid)->faces[(elementIndex)]      = (templateGrid)->faces[(elementIndex)];                           \
        }                                                                                                             \
        for ((elementIndex) = 0; (elementIndex) < (s32)ARRAY_SIZE(_gMineMesaCollision08EE4Verts); (elementIndex)++) { \
            (liveGrid)->vertices[(elementIndex)].vx = (templateGrid)->vertices[(elementIndex)].vx;                    \
            (liveGrid)->vertices[(elementIndex)].vy = (templateGrid)->vertices[(elementIndex)].vy;                    \
            (liveGrid)->vertices[(elementIndex)].vz = (templateGrid)->vertices[(elementIndex)].vz;                    \
        }                                                                                                             \
    }

/// Restores and places the room's three-face scene collision mesh.
///
/// Copies three normals/faces and eight XYZ vertices from the template into the
/// live grid's reserved leading entries, then translates them by (6200, -180,
/// 2550) game units for zero `lowered`, or (6200, 2000, 2550) for nonzero.
/// Positive grid Y points down. Other faces, vertex pad words and cell lists
/// remain intact; repeated calls place from the template rather than accumulate.
static void _mineMesaSetSceneCollisionLowered(s32 lowered)
{
    enum { SCENE_COLLISION_X         = 6200,
           SCENE_COLLISION_Z         = 2550,
           SCENE_COLLISION_NORMAL_Y  = -180,
           SCENE_COLLISION_LOWERED_Y = 2000 };
    WorldCollisionGrid* liveGrid     = &D_mine_mesa_8018700C;
    WorldCollisionGrid* templateGrid = &D_mine_mesa_801864A4;
    SVECTOR             translation;
    s32                 elementIndex;

    // Restore the reserved mesh before applying its room-space placement.
    MINE_MESA_RESTORE_SCENE_COLLISION(liveGrid, templateGrid, elementIndex);
    if (lowered == MINE_MESA_SCENE_COLLISION_NORMAL) {
        translation.vx = SCENE_COLLISION_X;
        translation.vy = SCENE_COLLISION_NORMAL_Y;
    } else {
        translation.vx = SCENE_COLLISION_X;
        translation.vy = SCENE_COLLISION_LOWERED_Y;
    }
    translation.vz = SCENE_COLLISION_Z;
    for (elementIndex = 0; elementIndex < (s32)ARRAY_SIZE(_gMineMesaCollision08EE4Verts); elementIndex++) {
        liveGrid->vertices[elementIndex].vx += translation.vx;
        liveGrid->vertices[elementIndex].vy += translation.vy;
        liveGrid->vertices[elementIndex].vz += translation.vz;
    }
}

#undef MINE_MESA_RESTORE_SCENE_COLLISION

void mineMesaDrawViewFlaresTask(Task* task)
{
    enum { FLARES_INITIALIZE,
           FLARES_DRAW,
           FLARE_LARGE_TEXTURE_COLUMN = 0,
           FLARE_SMALL_TEXTURE_COLUMN = 1,
           FLARE_LARGE_RADIUS_SCALE   = 0x300,
           FLARE_SMALL_RADIUS_SCALE   = 0x200 };

    // Install this loaded room's callbacks before actors can spawn their effects.
    if (task->state == FLARES_INITIALIZE) {
        gRoomEffectFlashId               = EFFECT_MINE_MESA_FLASH;
        gRoomEffectTwinTrailId           = EFFECT_MINE_MESA_TWIN_TRAIL;
        gRoomEffectSparkBurstId          = EFFECT_MINE_MESA_SPARK_BURST;
        gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
        task->state                      = FLARES_DRAW;
    }

    // Camera subsets share overlapping runs through the consecutive position records.
    switch (viewGetMappedIndex() & 0xFF) {
        case 2: {
            const SVECTOR* positions = D_mine_mesa_801864D0;
            _glowDrawFlare(&positions[0], FLARE_LARGE_TEXTURE_COLUMN, FLARE_LARGE_RADIUS_SCALE);
            _glowDrawFlare(&positions[1], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[2], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[3], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            break;
        }
        case 4: {
            const SVECTOR* positions = D_mine_mesa_801864F0;
            _glowDrawFlare(&positions[0], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[1], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[2], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[3], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[6], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            break;
        }
        case 5: {
            const SVECTOR* positions = D_mine_mesa_801864C8;
            _glowDrawFlare(&positions[0], FLARE_LARGE_TEXTURE_COLUMN, FLARE_LARGE_RADIUS_SCALE);
            _glowDrawFlare(&positions[1], FLARE_LARGE_TEXTURE_COLUMN, FLARE_LARGE_RADIUS_SCALE);
            _glowDrawFlare(&positions[5], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[6], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[8], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[9], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            break;
        }
        case 6: {
            const SVECTOR* positions = D_mine_mesa_801864F0;
            _glowDrawFlare(&positions[0], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[1], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[4], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[5], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            break;
        }
        case 8: {
            const SVECTOR* positions = D_mine_mesa_801864F0;
            _glowDrawFlare(&positions[0], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            break;
        }
        case 9: {
            const SVECTOR* positions = D_mine_mesa_80186508;
            _glowDrawFlare(&positions[0], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[1], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[2], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            break;
        }
        case 10: {
            const SVECTOR* positions = D_mine_mesa_801864D8;
            _glowDrawFlare(&positions[0], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[2], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            break;
        }
        case 11: {
            const SVECTOR* positions = D_mine_mesa_801864F0;
            _glowDrawFlare(&positions[0], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[1], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[2], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[3], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[4], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[5], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            _glowDrawFlare(&positions[6], FLARE_SMALL_TEXTURE_COLUMN, FLARE_SMALL_RADIUS_SCALE);
            break;
        }
    }
}

#include "../../shared/glow_draw_flare.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void mineMesaRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void mineMesaRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void mineMesaRoomVisualEffectsSparkBurstTask(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"

/// Builds one vertical collision quad from the room's base-edge table.
///
/// `wallIndex` is 0..3. Writable normals/faces cover index `wallIndex + 3`, and
/// vertices cover its four corners at `4 * (wallIndex + 3)` through +3. Borrows
/// these pools for the call. `height` is in game units, subtracted from upper
/// corners' Y and narrowed to s16. Writes a horizontal Q12 unit normal and
/// surface class 3; vector pad words and grid cell lists stay intact.
static inline void _mineMesaBuildWallFace(SVECTOR* normals, SVECTOR* vertices,
                                          WorldCollisionGridFace* faces, s16 wallIndex, s32 height)
{
    enum { WALL_FIRST_FACE        = 3,
           WALL_VERTICES_PER_FACE = 4,
           WALL_SURFACE_CLASS     = 3 };
    s16 faceIndex;
    s32 firstVertexIndex;

    faceIndex = wallIndex + WALL_FIRST_FACE;

    // Duplicate the lower edge, then raise the second pair in the grid's Y-down frame.
    vertices[faceIndex * WALL_VERTICES_PER_FACE].vx = vertices[faceIndex * WALL_VERTICES_PER_FACE + 2].vx = D_mine_mesa_80189A9C[wallIndex].start.vx;
    vertices[faceIndex * WALL_VERTICES_PER_FACE].vy = vertices[faceIndex * WALL_VERTICES_PER_FACE + 2].vy = D_mine_mesa_80189A9C[wallIndex].start.vy;
    vertices[faceIndex * WALL_VERTICES_PER_FACE].vz = vertices[faceIndex * WALL_VERTICES_PER_FACE + 2].vz = D_mine_mesa_80189A9C[wallIndex].start.vz;
    vertices[faceIndex * WALL_VERTICES_PER_FACE + 1].vx = vertices[faceIndex * WALL_VERTICES_PER_FACE + 3].vx = D_mine_mesa_80189A9C[wallIndex].end.vx;
    vertices[faceIndex * WALL_VERTICES_PER_FACE + 1].vy = vertices[faceIndex * WALL_VERTICES_PER_FACE + 3].vy = D_mine_mesa_80189A9C[wallIndex].end.vy;
    vertices[faceIndex * WALL_VERTICES_PER_FACE + 1].vz = vertices[faceIndex * WALL_VERTICES_PER_FACE + 3].vz = D_mine_mesa_80189A9C[wallIndex].end.vz;
    vertices[faceIndex * WALL_VERTICES_PER_FACE + 2].vy                                                      -= height;
    vertices[faceIndex * WALL_VERTICES_PER_FACE + 3].vy                                                      -= height;

    // Keep connectivity and the edge's horizontal collision normal in the reserved face.
    faces[faceIndex].vertexIndices[1] = faceIndex * WALL_VERTICES_PER_FACE + 1;
    firstVertexIndex                  = faceIndex * WALL_VERTICES_PER_FACE;
    faces[faceIndex].vertexIndices[0] = firstVertexIndex;
    faces[faceIndex].vertexIndices[2] = firstVertexIndex + 2;
    faces[faceIndex].vertexIndices[3] = firstVertexIndex + 3;
    faces[faceIndex].surfaceClass     = WALL_SURFACE_CLASS;
    faces[faceIndex].normalIndex      = faceIndex;
    normals[faceIndex].vx             = D_mine_mesa_80189A9C[wallIndex].start.vz - D_mine_mesa_80189A9C[wallIndex].end.vz;
    normals[faceIndex].vy             = 0;
    normals[faceIndex].vz             = D_mine_mesa_80189A9C[wallIndex].end.vx - D_mine_mesa_80189A9C[wallIndex].start.vx;
    VectorNormalSS(&normals[faceIndex], &normals[faceIndex]);
}

void mineMesaBuildWalls(s32 height)
{
    SVECTOR*                normals;
    SVECTOR*                vertices;
    WorldCollisionGridFace* faces;
    s16                     wallIndex;

    normals  = Gp_GridParams->normals;
    vertices = Gp_GridParams->vertices;
    faces    = Gp_GridParams->faces;
    // Extend each base edge upward; preserve the grid's existing cell lists.
    for (wallIndex = 0; wallIndex < (s32)ARRAY_SIZE(D_mine_mesa_80189A9C); wallIndex++) {
        _mineMesaBuildWallFace(normals, vertices, faces, wallIndex, height);
    }
}

/// Fills the wave's two enemy slots until ten deaths have been accounted for.
///
/// Borrows slots whose death messages clear them; spawned enemies belong to
/// the scene manager. At one kill remaining both slots must be empty before
/// spawning. The signed cooldown decrements once per eligible tick and is
/// seeded to 80 after a spawn; slot-0 spawning can consume its first tick in
/// the same call. Views select spawn-point subsets {1,2,3}, {2,3}, {0,1,2},
/// {0,1}, {2,3} for mapped views 2,3,4,5,8; other views allow all four.
/// Requires live actor resources, spawn points and the area's first placement.
/// At zero remaining kills, clears the saved companion selection, forces one
/// final battle release without crediting rewards, and advances to teardown.
static void _mineMesaSpawnEnemyWaveState(Task* task)
{
    enum { MESA_LEAP_TUNING_ZERO       = (3 << 16) | 2,
           SPAWN_COOLDOWN_TICKS        = 80,
           TEXTURE_DEBUG_DEMO          = 10,
           COMPANION_NONE              = 0,
           POST_WAVE_MUSIC_ENTRY       = 1,
           VIEW_SPAWN_POINTS_123       = 2,
           VIEW_FIRST_SPAWN_POINTS_23  = 3,
           VIEW_SPAWN_POINTS_012       = 4,
           VIEW_SPAWN_POINTS_01        = 5,
           VIEW_SECOND_SPAWN_POINTS_23 = 8 };
    GameLocationKey            placementKey;
    Enemy                      unusedEnemy;
    s32                        slotIndex;
    s32                        spawnPointIndex;
    u32                        secondRandomBit;
    const _MineMesaSpawnPoint* spawnPoints;
    const _MineMesaSpawnPoint* spawnPoint;
    TmdObject*                 model;
    GameLocationKey*           currentLocation;
    AreaPlacement*             placement;
    GfxCoord*                  rootCoord;
    Enemy*                     spawnedEnemy;

/// Binds the current area's first placement texture offsets and rebuilds both buffers.
///
/// Arguments must be stable lvalues: a live TmdObject*, writable GameLocationKey,
/// GameLocationKey* temporary and AreaPlacement* temporary, respectively. They are
/// evaluated repeatedly; the two pointer temporaries are assigned by this block.
/// Uses the current session/save, loaded area resources and TEXTURE_DEBUG_DEMO
/// constant in this function. Resolving the variant may initialize saved state.
/// Expands to a braced block; invoke inside a braced block.
#define MINE_MESA_APPLY_WAVE_TEXTURE_OFFSETS(model, placementKey, currentLocation, placement)                     \
    {                                                                                                             \
        (currentLocation)    = &gGameSession->location.loc;                                                       \
        (placementKey).stage = (currentLocation)->stage;                                                          \
        (placementKey).area  = (currentLocation)->area;                                                           \
        (placementKey).room  = (currentLocation)->room;                                                           \
        (placementKey).view  = gGameSession->location.loc.view;                                                   \
        areaSyncLocationVariant(&(placementKey));                                                                 \
        (placement)                = areaGetVariant(&(placementKey))->placements;                                 \
        (model)->texturePageOffset = (placement)->texturePageOffset;                                              \
        (model)->clutRowOffset     = (placement)->clutRowOffset;                                                  \
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == TEXTURE_DEBUG_DEMO) {                           \
            printf("tpage=%x, clut=%x, eno=%x\n", (placement)->texturePageOffset, (placement)->clutRowOffset, 0); \
        }                                                                                                         \
        if ((model)->buffer != NULL) {                                                                            \
            tmdBuildBufferHalf((model));                                                                          \
            tmdBuildBufferHalf((model));                                                                          \
        }                                                                                                         \
    }

    // Respect the shared cooldown and leave the last kill to a lone enemy.
    for (slotIndex = 0; slotIndex < (s32)ARRAY_SIZE(D_mine_mesa_80189B74); slotIndex++) {
        if (MineMesaCooldown > 0) {
            MineMesaCooldown--;
            break;
        }
        if (MineMesaRemaining == 0) {
            break;
        }
        if (MineMesaRemaining == 1 &&
            (D_mine_mesa_80189B74[0] != NULL || D_mine_mesa_80189B74[1] != NULL)) {
            break;
        }
        if (D_mine_mesa_80189B74[slotIndex] != NULL) {
            continue;
        }
        spawnedEnemy                    = enemySpawnFromTable(&Actor00100_D1BA84, 0, MESA_LEAP_TUNING_ZERO, NULL);
        D_mine_mesa_80189B74[slotIndex] = spawnedEnemy;
        if (spawnedEnemy == NULL) {
            break;
        }
        spawnedEnemy->workType                     = ENEMY_WORK_PLAIN;
        D_mine_mesa_80189B74[slotIndex]->placeKey |= slotIndex << ENEMY_PLACE_INDEX_SHIFT;
        // Keep the same LCG draws, including the two draws used by view 4.
        switch (viewGetMappedIndex() & 0xFF) {
            case VIEW_SPAWN_POINTS_123:
                spawnPointIndex = MINE_MESA_RAND() % 3 + 1;
                break;
            case VIEW_FIRST_SPAWN_POINTS_23:
                spawnPointIndex = (MINE_MESA_RAND() & 1) | 2;
                break;
            case VIEW_SPAWN_POINTS_012:
                spawnPointIndex = ((MINE_MESA_RAND() & 1) == 0) * 2;
                secondRandomBit = MINE_MESA_RAND() & 1;
                if (secondRandomBit == 1) {
                    spawnPointIndex = secondRandomBit;
                }
                break;
            case VIEW_SPAWN_POINTS_01:
                spawnPointIndex = MINE_MESA_RAND() & 1;
                break;
            case VIEW_SECOND_SPAWN_POINTS_23:
                spawnPointIndex = (MINE_MESA_RAND() & 1) | 2;
                break;
            default:
                spawnPointIndex = MINE_MESA_RAND() & 3;
                break;
        }
        spawnPoints                                                          = D_mine_mesa_80189AFC;
        spawnPoint                                                           = &spawnPoints[(s16)spawnPointIndex];
        D_mine_mesa_80189B74[slotIndex]->task->extra.tmd->coords->coord.t[0] = spawnPoint->x;
        D_mine_mesa_80189B74[slotIndex]->task->extra.tmd->coords->coord.t[1] = spawnPoint->y;
        D_mine_mesa_80189B74[slotIndex]->task->extra.tmd->coords->coord.t[2] = spawnPoint->z;
        // Bind textures from placement zero for either enemy slot.
        model = D_mine_mesa_80189B74[slotIndex]->task->extra.tmd;
        MINE_MESA_APPLY_WAVE_TEXTURE_OFFSETS(model, placementKey, currentLocation, placement);
#undef MINE_MESA_APPLY_WAVE_TEXTURE_OFFSETS
        gfxRotMatrixY(&D_mine_mesa_80189B74[slotIndex]->task->extra.tmd->coords->coord,
                      spawnPoint->yaw, GRAPHICS_ROTATION_REPLACE);
        rootCoord               = D_mine_mesa_80189B74[slotIndex]->task->extra.tmd->coords;
        MineMesaCooldown        = SPAWN_COOLDOWN_TICKS;
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (MineMesaRemaining > 0) {
        return;
    }
    // End the scripted battle without adding an enemy reward.
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType = COMPANION_NONE;
    // This stack payload is published by the binary but never read by release or teardown.
    task->spawnArg2.pointer      = &unusedEnemy;
    unusedEnemy.param            = NULL;
    gSceneCombatState.battleRefs = 1;
    sceneReleaseBattleRef(task, 0);
    gStageSceneMusicEntry = POST_WAVE_MUSIC_ENTRY;
    task->state++;
}

/// Rebuilds the mesa's collision walls at the current room variant's height.
///
/// Variants 1 and 7 use 2000 game units; all others use 400. Requires the
/// loaded mesa wall table and active grid described by `mineMesaBuildWalls`.
static void _mineMesaRebuildVariantWalls(void)
{
    enum { WALL_HEIGHT_TALL = 2000,
           WALL_HEIGHT_LOW  = 400 };
    s32 height;

    if (gGameSession->location.loc.variant == 1 || gGameSession->location.loc.variant == 7) {
        height = WALL_HEIGHT_TALL;
    } else {
        height = WALL_HEIGHT_LOW;
    }
    mineMesaBuildWalls(height);
}

/// Consumes an enemy-wave actor event, releasing a dead enemy's occupied slot.
///
/// `slotIndex` is the integer first payload, 0 or 1, supplied from the spawned
/// enemy's placement-index nibble. The other payload and receiver are unused.
/// A nonpositive HP clears the slot and decrements the signed halfword kill
/// counter; the unsigned view preserves its low-halfword subtraction. Empty or
/// still-live slots leave the wave unchanged. Returns 1 for every event.
static s32 _mineMesaEnemyWaveActorEventMsg(Task* task, s32 messageId, s32 slotIndex, s32 unused)
{
    enum { ACTOR_EVENT_CONSUMED = 1 };

    if (D_mine_mesa_80189B74[slotIndex] != NULL && D_mine_mesa_80189B74[slotIndex]->hp <= 0) {
        D_mine_mesa_80189B74[slotIndex] = NULL;
        MineMesaRemaining               = (u16)MineMesaRemaining - 1;
    }
    return ACTOR_EVENT_CONSUMED;
}

/// Initializes the ten-kill enemy wave and enables its actor-event replies.
///
/// State 0 requires the previous wave's enemies to be retired. Clears both
/// borrowed enemy slots, seeds the signed kill count, installs the wave's
/// message table and advances to spawning state 1. Does not reset spawn
/// cooldown; a previous delay can carry into this wave. Owns no task work.
static void _mineMesaInitializeEnemyWaveState(Task* task)
{
    enum { WAVE_KILL_COUNT = 10 };

    MineMesaRemaining       = WAVE_KILL_COUNT;
    D_mine_mesa_80189B74[1] = NULL;
    D_mine_mesa_80189B74[0] = NULL;
    task->msgTable          = D_mine_mesa_80189B1C;
    task->state++;
}

/// Advances the completed enemy wave to its teardown state on the following tick.
///
/// Called in state 2 after the spawner has released the battle hold; state 3
/// kills the controller. This intervening tick preserves that teardown delay.
static void _mineMesaFinishEnemyWaveState(Task* task)
{
    task->state = task->state + 1;
}

/// State handlers of the enemy-wave task `_mineMesaEnemyWaveTask` drives: the
/// set-up tick, the spawner, a step past the wave and `taskKill`.
static const TaskFuncTable4 D_mine_mesa_8017D660 = {
    { _mineMesaInitializeEnemyWaveState, _mineMesaSpawnEnemyWaveState, _mineMesaFinishEnemyWaveState, taskKill },
};

/// Runs the room's ten-kill wave through spawning, completion delay and teardown.
///
/// Requires the mesa actor resources and two enemy slots to remain live.
/// `state` must be 0..3: initialize, spawn/update, finish, release. A fresh
/// bodyless task starts at 0. The spawner releases the battle hold before state
/// 2, and state 3 releases this controller on the following tick. Owns no work.
static void _mineMesaEnemyWaveTask(Task* task)
{
    TaskFuncTable4 states = D_mine_mesa_8017D660;

    states.funcs[task->state](task);
}
