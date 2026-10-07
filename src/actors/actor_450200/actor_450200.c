#include "actors/actor_450200.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/neo_ark_observatory.h"

/// Commands supplied by the companion scene and its skip script.
enum {
    ACTOR_450200_PUFF_COMMAND_STOP  = 0,
    ACTOR_450200_PUFF_COMMAND_START = 1,
    ACTOR_450200_PUFF_COMMAND_FADE  = 2,
};

/// Logical room selected by both completed paths of the backdrop scene.
enum { ACTOR_450200_SCENE_ROOM_VARIANT = 2 };

/// VRAM placement and Q7 weights of the scene's two 8-bit backdrop layers.
enum {
    ACTOR_450200_BACKDROP_COMMAND_START      = 1,
    ACTOR_450200_BACKDROP_CLUT_COLORS        = 256,
    ACTOR_450200_BACKDROP_SCALE_ONE          = 128,
    ACTOR_450200_BACKDROP_TEXTURE_Y          = 256,
    ACTOR_450200_BACKDROP_FROM_TEXTURE_X     = 320,
    ACTOR_450200_BACKDROP_TO_TEXTURE_X       = 448,
    ACTOR_450200_BACKDROP_FROM_SOURCE_CLUT_Y = 247,
    ACTOR_450200_BACKDROP_TO_SOURCE_CLUT_Y   = 248,
    ACTOR_450200_BACKDROP_FROM_SCALED_CLUT_Y = 249,
    ACTOR_450200_BACKDROP_TO_SCALED_CLUT_Y   = 250,
    ACTOR_450200_BACKDROP_END_VIEW           = 8,
    ACTOR_450200_BEAM_FULL_INTENSITY         = 160,
};

/// The clips the observatory's room-variant scene adds to the player's
/// animation bank, with the play requests stored after them.
///
/// The scene is the observatory's second one-time event (direction action 2),
/// which ends by making the area's second room the current one. Its script
/// sends the player a copy request for this storage before it plays any of the
/// clips; that request is a separate object. The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words from the
/// start of the storage, which is more than the clip table holds: the four set
/// pointers occupy extended ids 47-50, and the first 28 words of the play
/// requests are written into the bank after them. The scene's requests select
/// ids 47-50 and the bank's own id 1 only, so none of those request words is
/// played as a clip.
///
/// The play requests are the first six of the seven the scene keeps for the
/// player, and are part of this object only because the copied span reaches
/// into the sixth; the request for id 50 follows as a separate object.
typedef union {
    struct {
        AnimationSet*        sets[4];         // Player clips for extended ids 47-50
        AnimationPlayRequest playRequests[6]; // Three requests for the bank's own id 1 (reset, 15-frame blend, 30-frame blend), then one each for ids 47-49; nothing references the third
    } data;                                   // The records by name
    s32 words[34];                            // The same storage as the copy reads it; the last two words lie beyond the copied span
} _Actor450200RoomVariantSceneAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor450200RoomVariantSceneAnimationBankExtensionStorage, 136);

extern _Actor450200RoomVariantSceneAnimationBankExtensionStorage D_actor_450200_8013FB4C;

/// The clips the observatory's solo scene adds to the player's animation bank,
/// with the play requests stored after them.
///
/// The scene is the form of the observatory's first one-time event (direction
/// action 1) that the player plays alone; the companion scene is the other
/// form, and a story flag chooses between them. Its script sends
/// the player a copy request for this storage before it plays any of the
/// clips; that request is a separate object. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is more than the clip table holds: the five set pointers occupy
/// extended ids 47-51, and the first 27 words of the play requests are written
/// into the bank after them. The scene's requests select ids 47-51 and the
/// bank's own id 1 only, so none of those request words is played as a clip.
///
/// The play requests are the first six of the seven the scene keeps for the
/// player, and are part of this object only because the copied span reaches
/// into the sixth; the request for id 51 follows as a separate object.
typedef union {
    struct {
        AnimationSet*        sets[5];         // Player clips for extended ids 47-51
        AnimationPlayRequest playRequests[6]; // Two requests for the bank's own id 1 (reset, 15-frame blend), then one each for ids 47-50
    } data;                                   // The records by name
    s32 words[35];                            // The same storage as the copy reads it; the last three words lie beyond the copied span
} _Actor450200SoloSceneAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor450200SoloSceneAnimationBankExtensionStorage, 140);

extern _Actor450200SoloSceneAnimationBankExtensionStorage D_actor_450200_8013C66C;

/// The clips the observatory's companion scene adds to the player's animation
/// bank, with the copy request that installs them and the play requests
/// stored after it.
///
/// The scene is the form of the observatory's first one-time event (direction
/// action 1) that the player and the companion play together; the solo scene
/// is the other form, and a story flag chooses between them. Its script sends the copy request to the player before it plays any of the
/// clips. The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words from the
/// start of the storage, which is more than the clip table holds: the eight
/// set pointers occupy extended ids 47-54, and the copy request and the first
/// 22 words of the play requests are written into the bank after them. The
/// scene's requests select ids 47-54 and the bank's own id 1 only, so none of
/// those following words is played as a clip.
///
/// The play requests open the run of requests the scene keeps for the player,
/// and are part of this object only because the copied span reaches into the
/// fifth; the rest of the run follows as separate objects.
typedef union {
    struct {
        AnimationSet*            sets[8];         // Player clips for extended ids 47-54
        AnimationBankCopyRequest copy;            // Installs the first 32 words of this storage in the player's bank extension
        AnimationPlayRequest     playRequests[5]; // Requests for extended id 47: one with a 5-frame blend, then four resets; only the first is referenced
    } data;                                       // The records by name
    s32 words[35];                                // The same storage as the copy reads it; the last three words lie beyond the copied span
} _Actor450200CompanionScenePlayerAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor450200CompanionScenePlayerAnimationBankExtensionStorage, 140);

extern _Actor450200CompanionScenePlayerAnimationBankExtensionStorage D_actor_450200_80137A84;

/// The clips the package adds to the companion's animation bank, with the copy
/// request that installs them and the play requests stored after it.
///
/// Three kinds of script send the copy request to the companion before they
/// play any of the clips: the companion scene, the script run on entering the
/// room once that scene has been seen, and three of the four companion talk
/// scripts. The fourth talk script and the alternate script the companion
/// scene is started with play clips without sending it. The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY`
/// words from the start of the storage, which is more than the clip table
/// holds: the eight set pointers occupy extended ids 47-54 of the companion's
/// bank, and the copy request and the first 22 words of the play requests are
/// written into the bank after them. The companion's requests select ids 47-54
/// and the bank's own id 1 only, so none of those following words is played as
/// a clip.
///
/// The play requests are the first five of the eight the package keeps for
/// these clips, and are part of this object only because the copied span
/// reaches into the fifth; the requests for ids 52-54 follow as separate
/// objects.
typedef union {
    struct {
        AnimationSet*            sets[8];         // Companion clips for extended ids 47-54 of the companion's bank
        AnimationBankCopyRequest copy;            // Installs the first 32 words of this storage in the companion's bank extension
        AnimationPlayRequest     playRequests[5]; // One request per clip for ids 47-51, in id order; the companion scene plays them on the companion
    } data;                                       // The records by name
    s32 words[35];                                // The same storage as the copy reads it; the last three words lie beyond the copied span
} _Actor450200CompanionAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor450200CompanionAnimationBankExtensionStorage, 140);

extern _Actor450200CompanionAnimationBankExtensionStorage D_actor_450200_80137BD8;

extern u8         D_actor_450200_8013885C[];
extern SVECTOR    D_actor_450200_80138868;
extern EvsCommand D_actor_450200_80138870[];
extern EvsCommand D_actor_450200_80138A68[];
extern EvsCommand D_actor_450200_80138C60[];
extern EvsCommand D_actor_450200_80138E88[];
extern EvsCommand D_actor_450200_80139098[];
extern TaskDesc   D_actor_450200_80137A60[];
extern TaskDesc   D_actor_450200_8013FB40;

/// Placement payload that four of the overlay's data records pair with message
/// 0x3EE. Only its yaw changes, set by `_actor450200UpdateTalkFacing`.
extern ActorTransform D_actor_450200_80137DC4;
extern Task*          D_actor_450200_801401E0;
extern Task*          D_actor_450200_801401E4;
extern u16            D_actor_450200_801401E8[256];
extern u16            D_actor_450200_801403E8[256];
extern u16            D_actor_450200_801405E8[256];
extern u16            D_actor_450200_801407E8[256];

static void _actor450200StartBackdropCrossFade(s32 command);
static void _actor450200SetLightBeamIntensity(s32 intensity);
static void _actor450200SetRoomVariant(u8 roomVariant);

extern AnimationPlayRequest D_actor_450200_80137B38;
extern AnimationPlayRequest D_actor_450200_80137B4C;
extern AnimationPlayRequest D_actor_450200_80137B60;
extern AnimationPlayRequest D_actor_450200_80137B74;
static AnimationSet         _gActor450200Animation01A7C;
static AnimationSet         _gActor450200Animation02DCC;
static AnimationSet         _gActor450200Animation031A8;
static AnimationSet         _gActor450200Animation033DC;
static AnimationSet         _gActor450200Animation03818;
static AnimationSet         _gActor450200Animation039B8;
static AnimationSet         _gActor450200Animation03C30;
static AnimationSet         _gActor450200Animation03E04;
static void                 _actor450200ControlCompanionPuffs(s32 command);
static void                 _actor450200CancelRoomEffects(void);
static void                 _actor450200SetHeadAimEnabled(s32 enabled);
static void                 _actor450200UpdateTalkFacing(void);
static void                 _actor450200BackdropCrossFadeTask(Task* task);

static void _actor450200CompanionPuffTask(Task* task);
static void _actor450200HeadAimTask(Task* task);

static AnimationPackedPose _gActor450200Animation01A7CBank1[62] = {
#include "assets/actor_450200_animation_01A7C_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation01A7CBank4[225] = {
#include "assets/actor_450200_animation_01A7C_bank4.inc"
};

static AnimationRecord _gActor450200Animation01A7CRecords[595] = {
#include "assets/actor_450200_animation_01A7C_records.inc"
};

static u16 _gActor450200Animation01A7CIndices[20] = {
#include "assets/actor_450200_animation_01A7C_indices.inc"
};

static AnimationSet _gActor450200Animation01A7C = {
    _gActor450200Animation01A7CRecords,
    _gActor450200Animation01A7CIndices,
    { NULL, _gActor450200Animation01A7CBank1, NULL, NULL, _gActor450200Animation01A7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation02DCCBank1[43] = {
#include "assets/actor_450200_animation_02DCC_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation02DCCBank4[468] = {
#include "assets/actor_450200_animation_02DCC_bank4.inc"
};

static AnimationRecord _gActor450200Animation02DCCRecords[619] = {
#include "assets/actor_450200_animation_02DCC_records.inc"
};

static u16 _gActor450200Animation02DCCIndices[20] = {
#include "assets/actor_450200_animation_02DCC_indices.inc"
};

static AnimationSet _gActor450200Animation02DCC = {
    _gActor450200Animation02DCCRecords,
    _gActor450200Animation02DCCIndices,
    { NULL, _gActor450200Animation02DCCBank1, NULL, NULL, _gActor450200Animation02DCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation031A8Bank1[2] = {
#include "assets/actor_450200_animation_031A8_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation031A8Bank4[91] = {
#include "assets/actor_450200_animation_031A8_bank4.inc"
};

static AnimationRecord _gActor450200Animation031A8Records[130] = {
#include "assets/actor_450200_animation_031A8_records.inc"
};

static u16 _gActor450200Animation031A8Indices[20] = {
#include "assets/actor_450200_animation_031A8_indices.inc"
};

static AnimationSet _gActor450200Animation031A8 = {
    _gActor450200Animation031A8Records,
    _gActor450200Animation031A8Indices,
    { NULL, _gActor450200Animation031A8Bank1, NULL, NULL, _gActor450200Animation031A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation033DCBank1[2] = {
#include "assets/actor_450200_animation_033DC_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation033DCBank4[33] = {
#include "assets/actor_450200_animation_033DC_bank4.inc"
};

static AnimationRecord _gActor450200Animation033DCRecords[82] = {
#include "assets/actor_450200_animation_033DC_records.inc"
};

static u16 _gActor450200Animation033DCIndices[20] = {
#include "assets/actor_450200_animation_033DC_indices.inc"
};

static AnimationSet _gActor450200Animation033DC = {
    _gActor450200Animation033DCRecords,
    _gActor450200Animation033DCIndices,
    { NULL, _gActor450200Animation033DCBank1, NULL, NULL, _gActor450200Animation033DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation03818Bank1[2] = {
#include "assets/actor_450200_animation_03818_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation03818Bank4[104] = {
#include "assets/actor_450200_animation_03818_bank4.inc"
};

static AnimationRecord _gActor450200Animation03818Records[141] = {
#include "assets/actor_450200_animation_03818_records.inc"
};

static u16 _gActor450200Animation03818Indices[20] = {
#include "assets/actor_450200_animation_03818_indices.inc"
};

static AnimationSet _gActor450200Animation03818 = {
    _gActor450200Animation03818Records,
    _gActor450200Animation03818Indices,
    { NULL, _gActor450200Animation03818Bank1, NULL, NULL, _gActor450200Animation03818Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation039B8Bank1[2] = {
#include "assets/actor_450200_animation_039B8_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation039B8Bank4[18] = {
#include "assets/actor_450200_animation_039B8_bank4.inc"
};

static AnimationRecord _gActor450200Animation039B8Records[60] = {
#include "assets/actor_450200_animation_039B8_records.inc"
};

static u16 _gActor450200Animation039B8Indices[20] = {
#include "assets/actor_450200_animation_039B8_indices.inc"
};

static AnimationSet _gActor450200Animation039B8 = {
    _gActor450200Animation039B8Records,
    _gActor450200Animation039B8Indices,
    { NULL, _gActor450200Animation039B8Bank1, NULL, NULL, _gActor450200Animation039B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation03C30Bank1[2] = {
#include "assets/actor_450200_animation_03C30_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation03C30Bank4[28] = {
#include "assets/actor_450200_animation_03C30_bank4.inc"
};

static AnimationRecord _gActor450200Animation03C30Records[104] = {
#include "assets/actor_450200_animation_03C30_records.inc"
};

static u16 _gActor450200Animation03C30Indices[20] = {
#include "assets/actor_450200_animation_03C30_indices.inc"
};

static AnimationSet _gActor450200Animation03C30 = {
    _gActor450200Animation03C30Records,
    _gActor450200Animation03C30Indices,
    { NULL, _gActor450200Animation03C30Bank1, NULL, NULL, _gActor450200Animation03C30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation03E04Bank1[2] = {
#include "assets/actor_450200_animation_03E04_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation03E04Bank4[23] = {
#include "assets/actor_450200_animation_03E04_bank4.inc"
};

static AnimationRecord _gActor450200Animation03E04Records[68] = {
#include "assets/actor_450200_animation_03E04_records.inc"
};

static u16 _gActor450200Animation03E04Indices[20] = {
#include "assets/actor_450200_animation_03E04_indices.inc"
};

static AnimationSet _gActor450200Animation03E04 = {
    _gActor450200Animation03E04Records,
    _gActor450200Animation03E04Indices,
    { NULL, _gActor450200Animation03E04Bank1, NULL, NULL, _gActor450200Animation03E04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation04168Bank1[8] = {
#include "assets/actor_450200_animation_04168_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation04168Bank4[68] = {
#include "assets/actor_450200_animation_04168_bank4.inc"
};

static AnimationRecord _gActor450200Animation04168Records[105] = {
#include "assets/actor_450200_animation_04168_records.inc"
};

static u16 _gActor450200Animation04168Indices[20] = {
#include "assets/actor_450200_animation_04168_indices.inc"
};

static AnimationSet _gActor450200Animation04168 = {
    _gActor450200Animation04168Records,
    _gActor450200Animation04168Indices,
    { NULL, _gActor450200Animation04168Bank1, NULL, NULL, _gActor450200Animation04168Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation043F0Bank1[2] = {
#include "assets/actor_450200_animation_043F0_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation043F0Bank4[51] = {
#include "assets/actor_450200_animation_043F0_bank4.inc"
};

static AnimationRecord _gActor450200Animation043F0Records[85] = {
#include "assets/actor_450200_animation_043F0_records.inc"
};

static u16 _gActor450200Animation043F0Indices[20] = {
#include "assets/actor_450200_animation_043F0_indices.inc"
};

static AnimationSet _gActor450200Animation043F0 = {
    _gActor450200Animation043F0Records,
    _gActor450200Animation043F0Indices,
    { NULL, _gActor450200Animation043F0Bank1, NULL, NULL, _gActor450200Animation043F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation04890Bank1[10] = {
#include "assets/actor_450200_animation_04890_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation04890Bank4[96] = {
#include "assets/actor_450200_animation_04890_bank4.inc"
};

static AnimationRecord _gActor450200Animation04890Records[150] = {
#include "assets/actor_450200_animation_04890_records.inc"
};

static u16 _gActor450200Animation04890Indices[20] = {
#include "assets/actor_450200_animation_04890_indices.inc"
};

static AnimationSet _gActor450200Animation04890 = {
    _gActor450200Animation04890Records,
    _gActor450200Animation04890Indices,
    { NULL, _gActor450200Animation04890Bank1, NULL, NULL, _gActor450200Animation04890Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation04DB0Bank1[6] = {
#include "assets/actor_450200_animation_04DB0_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation04DB0Bank4[124] = {
#include "assets/actor_450200_animation_04DB0_bank4.inc"
};

static AnimationRecord _gActor450200Animation04DB0Records[166] = {
#include "assets/actor_450200_animation_04DB0_records.inc"
};

static u16 _gActor450200Animation04DB0Indices[20] = {
#include "assets/actor_450200_animation_04DB0_indices.inc"
};

static AnimationSet _gActor450200Animation04DB0 = {
    _gActor450200Animation04DB0Records,
    _gActor450200Animation04DB0Indices,
    { NULL, _gActor450200Animation04DB0Bank1, NULL, NULL, _gActor450200Animation04DB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation05088Bank1[2] = {
#include "assets/actor_450200_animation_05088_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation05088Bank4[43] = {
#include "assets/actor_450200_animation_05088_bank4.inc"
};

static AnimationRecord _gActor450200Animation05088Records[113] = {
#include "assets/actor_450200_animation_05088_records.inc"
};

static u16 _gActor450200Animation05088Indices[20] = {
#include "assets/actor_450200_animation_05088_indices.inc"
};

static AnimationSet _gActor450200Animation05088 = {
    _gActor450200Animation05088Records,
    _gActor450200Animation05088Indices,
    { NULL, _gActor450200Animation05088Bank1, NULL, NULL, _gActor450200Animation05088Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation05350Bank1[3] = {
#include "assets/actor_450200_animation_05350_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation05350Bank4[57] = {
#include "assets/actor_450200_animation_05350_bank4.inc"
};

static AnimationRecord _gActor450200Animation05350Records[92] = {
#include "assets/actor_450200_animation_05350_records.inc"
};

static u16 _gActor450200Animation05350Indices[20] = {
#include "assets/actor_450200_animation_05350_indices.inc"
};

static AnimationSet _gActor450200Animation05350 = {
    _gActor450200Animation05350Records,
    _gActor450200Animation05350Indices,
    { NULL, _gActor450200Animation05350Bank1, NULL, NULL, _gActor450200Animation05350Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation05870Bank1[6] = {
#include "assets/actor_450200_animation_05870_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation05870Bank4[124] = {
#include "assets/actor_450200_animation_05870_bank4.inc"
};

static AnimationRecord _gActor450200Animation05870Records[166] = {
#include "assets/actor_450200_animation_05870_records.inc"
};

static u16 _gActor450200Animation05870Indices[20] = {
#include "assets/actor_450200_animation_05870_indices.inc"
};

static AnimationSet _gActor450200Animation05870 = {
    _gActor450200Animation05870Records,
    _gActor450200Animation05870Indices,
    { NULL, _gActor450200Animation05870Bank1, NULL, NULL, _gActor450200Animation05870Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation05C18Bank1[7] = {
#include "assets/actor_450200_animation_05C18_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation05C18Bank4[81] = {
#include "assets/actor_450200_animation_05C18_bank4.inc"
};

static AnimationRecord _gActor450200Animation05C18Records[112] = {
#include "assets/actor_450200_animation_05C18_records.inc"
};

static u16 _gActor450200Animation05C18Indices[20] = {
#include "assets/actor_450200_animation_05C18_indices.inc"
};

static AnimationSet _gActor450200Animation05C18 = {
    _gActor450200Animation05C18Records,
    _gActor450200Animation05C18Indices,
    { NULL, _gActor450200Animation05C18Bank1, NULL, NULL, _gActor450200Animation05C18Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_450200_80137A60[3] = {
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _actor450200CompanionPuffTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, _actor450200HeadAimTask, { .value = 0 } },
};

_Actor450200CompanionScenePlayerAnimationBankExtensionStorage D_actor_450200_80137A84 = { .data = { { &_gActor450200Animation04168, &_gActor450200Animation043F0, &_gActor450200Animation04890, &_gActor450200Animation04DB0, &_gActor450200Animation05088, &_gActor450200Animation05350, &_gActor450200Animation05870, &_gActor450200Animation05C18 }, { { .words = D_actor_450200_80137A84.words }, ANIMATION_BANK_EXTENSION_CAPACITY }, { { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE } } } };

AnimationPlayRequest D_actor_450200_80137B10[2] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_actor_450200_80137B38 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B4C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B60 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B74 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B88 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137B9C = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137BB0 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137BC4 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

_Actor450200CompanionAnimationBankExtensionStorage D_actor_450200_80137BD8 = { .data = { { &_gActor450200Animation01A7C, &_gActor450200Animation02DCC, &_gActor450200Animation031A8, &_gActor450200Animation033DC, &_gActor450200Animation03818, &_gActor450200Animation039B8, &_gActor450200Animation03C30, &_gActor450200Animation03E04 }, { { .words = D_actor_450200_80137BD8.words }, ANIMATION_BANK_EXTENSION_CAPACITY }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_450200_80137C64 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450200_80137C78 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450200_80137C8C = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450200_80137CA0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137CB4 = { { .index = 1 }, 32, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450200_80137CC8 = { { .index = 1 }, 33, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

PadScriptCmd D_actor_450200_80137CDC[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_450200_80137CE4[2] = {
    { 253, 67, 12, 1 },
    { 0, 0, 13, 0 },
};

ActorTransform D_actor_450200_80137CEC = { { 0x36B0, 0, 0x2D50, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D04 = { { 8000, 0, 0x2D50, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D1C = { { 0x2904, 0, 0x2FA8, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D34 = { { 8000, 0, 0x2FA8, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D4C = { { 8000, 0, 9000, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D64 = { { 0x2AF8, 0, 0x3070, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450200_80137D7C = { { 0x2AF8, 0, 0x2C24, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_450200_80137D94 = { { 9750, 0, 0x2C24, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_450200_80137DAC = { { 8000, 0, 9700, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_450200_80137DC4 = { { 0, 0, 0, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_450200_80137DDC[4] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B9C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_450200_80137E3C[7] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B4C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 30 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_450200_80137EE4[82] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_80137A84.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_80137BD8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137CEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137D64 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_450200_80137BD8.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_450200_80137D04 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200ControlCompanionPuffs }, { .value = ACTOR_450200_PUFF_COMMAND_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55070008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x4066000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_450200_80137CDC }, { .vibrationSegments = D_actor_450200_80137CE4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_450200_80137A84.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55070007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137D1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200ControlCompanionPuffs }, { .value = ACTOR_450200_PUFF_COMMAND_FADE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_450200_80137DDC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_450200_80137E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_450200_80137DDC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_450200_80137E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_450200_80137DDC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_450200_80137E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BB0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C64 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137D7C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_450200_80137D34 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_450200_80137D4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.playRequests[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200ControlCompanionPuffs }, { .value = ACTOR_450200_PUFF_COMMAND_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137D94 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137DAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_neo_ark_observatory_8017FA98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80138694[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200ControlCompanionPuffs }, { .value = ACTOR_450200_PUFF_COMMAND_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137D94 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137DAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_neo_ark_observatory_8017FA98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

u8 D_actor_450200_8013885C[12] = {
    1,
    3,
    5,
    6,
    9,
    14,
    15,
    16,
    17,
    18,
    19,
    0,
};

SVECTOR D_actor_450200_80138868 = { 0, -128, 0, 0 };

EvsCommand D_actor_450200_80138870[21] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_80137BD8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200UpdateTalkFacing }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_80137DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80138A68[21] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_80137BD8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200UpdateTalkFacing }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_80137DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80138C60[23] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200UpdateTalkFacing }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_80137DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80138E88[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_80137BD8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450200UpdateTalkFacing }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_80137DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137CC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetHeadAimEnabled }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80139098[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137D94 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_80137BD8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137C78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_neo_ark_observatory_8017FA98 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gActor450200Animation0865CBank1[61] = {
#include "assets/actor_450200_animation_0865C_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation0865CBank4[363] = {
#include "assets/actor_450200_animation_0865C_bank4.inc"
};

static AnimationRecord _gActor450200Animation0865CRecords[669] = {
#include "assets/actor_450200_animation_0865C_records.inc"
};

static u16 _gActor450200Animation0865CIndices[20] = {
#include "assets/actor_450200_animation_0865C_indices.inc"
};

static AnimationSet _gActor450200Animation0865C = {
    _gActor450200Animation0865CRecords,
    _gActor450200Animation0865CIndices,
    { NULL, _gActor450200Animation0865CBank1, NULL, NULL, _gActor450200Animation0865CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation08B88Bank1[12] = {
#include "assets/actor_450200_animation_08B88_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation08B88Bank4[116] = {
#include "assets/actor_450200_animation_08B88_bank4.inc"
};

static AnimationRecord _gActor450200Animation08B88Records[159] = {
#include "assets/actor_450200_animation_08B88_records.inc"
};

static u16 _gActor450200Animation08B88Indices[20] = {
#include "assets/actor_450200_animation_08B88_indices.inc"
};

static AnimationSet _gActor450200Animation08B88 = {
    _gActor450200Animation08B88Records,
    _gActor450200Animation08B88Indices,
    { NULL, _gActor450200Animation08B88Bank1, NULL, NULL, _gActor450200Animation08B88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation08E94Bank1[2] = {
#include "assets/actor_450200_animation_08E94_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation08E94Bank4[46] = {
#include "assets/actor_450200_animation_08E94_bank4.inc"
};

static AnimationRecord _gActor450200Animation08E94Records[123] = {
#include "assets/actor_450200_animation_08E94_records.inc"
};

static u16 _gActor450200Animation08E94Indices[20] = {
#include "assets/actor_450200_animation_08E94_indices.inc"
};

static AnimationSet _gActor450200Animation08E94 = {
    _gActor450200Animation08E94Records,
    _gActor450200Animation08E94Indices,
    { NULL, _gActor450200Animation08E94Bank1, NULL, NULL, _gActor450200Animation08E94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation09E70Bank1[34] = {
#include "assets/actor_450200_animation_09E70_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation09E70Bank4[398] = {
#include "assets/actor_450200_animation_09E70_bank4.inc"
};

static AnimationRecord _gActor450200Animation09E70Records[495] = {
#include "assets/actor_450200_animation_09E70_records.inc"
};

static u16 _gActor450200Animation09E70Indices[20] = {
#include "assets/actor_450200_animation_09E70_indices.inc"
};

static AnimationSet _gActor450200Animation09E70 = {
    _gActor450200Animation09E70Records,
    _gActor450200Animation09E70Indices,
    { NULL, _gActor450200Animation09E70Bank1, NULL, NULL, _gActor450200Animation09E70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation0A824Bank1[21] = {
#include "assets/actor_450200_animation_0A824_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation0A824Bank4[243] = {
#include "assets/actor_450200_animation_0A824_bank4.inc"
};

static AnimationRecord _gActor450200Animation0A824Records[295] = {
#include "assets/actor_450200_animation_0A824_records.inc"
};

static u16 _gActor450200Animation0A824Indices[20] = {
#include "assets/actor_450200_animation_0A824_indices.inc"
};

static AnimationSet _gActor450200Animation0A824 = {
    _gActor450200Animation0A824Records,
    _gActor450200Animation0A824Indices,
    { NULL, _gActor450200Animation0A824Bank1, NULL, NULL, _gActor450200Animation0A824Bank4, NULL, NULL, NULL },
};

_Actor450200SoloSceneAnimationBankExtensionStorage D_actor_450200_8013C66C = { .data = { { &_gActor450200Animation0865C, &_gActor450200Animation08B88, &_gActor450200Animation08E94, &_gActor450200Animation09E70, &_gActor450200Animation0A824 }, { { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_450200_8013C6F8 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_actor_450200_8013C70C = { { .words = D_actor_450200_8013C66C.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorTransform D_actor_450200_8013C714 = { { 0x2710, 0, 0x2EE0, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_450200_8013C72C[40] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_8013C70C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_8013C714 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55070001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.playRequests[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.playRequests[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C6F8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013C66C.data.playRequests[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55070002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_8013C714 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_450200_8013C66C.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_8013CAEC[12] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_8013C714 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_450200_8013C66C.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gActor450200Animation0B190Bank1[6] = {
#include "assets/actor_450200_animation_0B190_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation0B190Bank4[64] = {
#include "assets/actor_450200_animation_0B190_bank4.inc"
};

static AnimationRecord _gActor450200Animation0B190Records[141] = {
#include "assets/actor_450200_animation_0B190_records.inc"
};

static u16 _gActor450200Animation0B190Indices[20] = {
#include "assets/actor_450200_animation_0B190_indices.inc"
};

static AnimationSet _gActor450200Animation0B190 = {
    _gActor450200Animation0B190Records,
    _gActor450200Animation0B190Indices,
    { NULL, _gActor450200Animation0B190Bank1, NULL, NULL, _gActor450200Animation0B190Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation0D678Bank1[84] = {
#include "assets/actor_450200_animation_0D678_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation0D678Bank4[907] = {
#include "assets/actor_450200_animation_0D678_bank4.inc"
};

static AnimationRecord _gActor450200Animation0D678Records[1183] = {
#include "assets/actor_450200_animation_0D678_records.inc"
};

static u16 _gActor450200Animation0D678Indices[20] = {
#include "assets/actor_450200_animation_0D678_indices.inc"
};

static AnimationSet _gActor450200Animation0D678 = {
    _gActor450200Animation0D678Records,
    _gActor450200Animation0D678Indices,
    { NULL, _gActor450200Animation0D678Bank1, NULL, NULL, _gActor450200Animation0D678Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation0D9E0Bank1[5] = {
#include "assets/actor_450200_animation_0D9E0_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation0D9E0Bank4[74] = {
#include "assets/actor_450200_animation_0D9E0_bank4.inc"
};

static AnimationRecord _gActor450200Animation0D9E0Records[109] = {
#include "assets/actor_450200_animation_0D9E0_records.inc"
};

static u16 _gActor450200Animation0D9E0Indices[20] = {
#include "assets/actor_450200_animation_0D9E0_indices.inc"
};

static AnimationSet _gActor450200Animation0D9E0 = {
    _gActor450200Animation0D9E0Records,
    _gActor450200Animation0D9E0Indices,
    { NULL, _gActor450200Animation0D9E0Bank1, NULL, NULL, _gActor450200Animation0D9E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450200Animation0DCF8Bank1[5] = {
#include "assets/actor_450200_animation_0DCF8_bank1.inc"
};

static AnimationPackedRotation _gActor450200Animation0DCF8Bank4[66] = {
#include "assets/actor_450200_animation_0DCF8_bank4.inc"
};

static AnimationRecord _gActor450200Animation0DCF8Records[97] = {
#include "assets/actor_450200_animation_0DCF8_records.inc"
};

static u16 _gActor450200Animation0DCF8Indices[20] = {
#include "assets/actor_450200_animation_0DCF8_indices.inc"
};

static AnimationSet _gActor450200Animation0DCF8 = {
    _gActor450200Animation0DCF8Records,
    _gActor450200Animation0DCF8Indices,
    { NULL, _gActor450200Animation0DCF8Bank1, NULL, NULL, _gActor450200Animation0DCF8Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_450200_8013FB40 = { { { TASK_BODY_NONE, 32 } }, _actor450200BackdropCrossFadeTask, { .value = 0 } };

_Actor450200RoomVariantSceneAnimationBankExtensionStorage D_actor_450200_8013FB4C = { .data = { { &_gActor450200Animation0B190, &_gActor450200Animation0D678, &_gActor450200Animation0D9E0, &_gActor450200Animation0DCF8 }, { { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_450200_8013FBD4 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_actor_450200_8013FBE8 = { { .words = D_actor_450200_8013FB4C.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

GameActorMoveAnim D_actor_450200_8013FBF0 = { 19, 1 };

ActorTransform D_actor_450200_8013FBF8 = { { 6271, 0, 3306, 0 }, { 0, -682, 0, 0 } };

ActorTransform D_actor_450200_8013FC10 = { { 3096, 0, 4774, 0 }, { 0, -1080, 0, 0 } };

ActorTransform D_actor_450200_8013FC28 = { { 880, 0, 6890, 0 }, { 0, 3015, 0, 0 } };

ActorTransform D_actor_450200_8013FC40 = { { 880, 0, 6890, 0 }, { 0, 967, 0, 0 } };

EvsCommand D_actor_450200_8013FC58[44] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_8013FBE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_8013FBF8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_450200_8013FC10 } }, { .message = { .pointer = &D_actor_450200_8013FBF0 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55070004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 68 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_450200_8013FC28 } }, { .message = { .pointer = &D_actor_450200_8013FBF0 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_8013FC28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.playRequests[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FBD4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55070003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200StartBackdropCrossFade }, { .value = ACTOR_450200_BACKDROP_COMMAND_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.playRequests[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 160 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_8013FC40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450200SetRoomVariant }, { .value = ACTOR_450200_SCENE_ROOM_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80140078[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450200SetRoomVariant }, { .value = ACTOR_450200_SCENE_ROOM_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450200SetLightBeamIntensity }, { .value = ACTOR_450200_BEAM_FULL_INTENSITY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_8013FC40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

Task* D_actor_450200_801401E0;

Task* D_actor_450200_801401E4;

u16 D_actor_450200_801401E8[256];

u16 D_actor_450200_801403E8[256];

u16 D_actor_450200_801405E8[256];

u16 D_actor_450200_801407E8[256];

void               func_actor_450200_801322F8(void);
static void        _actor450200DrawBackdropLayer(s32 screenX, s32 textureX, s32 paletteY, s32 semiTransparent, s32 brightness, s32 rawTexture);
static inline void _actor450200LoadScaledClut(const u16* sourceColors, u16* scaledColors, s32 scale, s32 vramY);

/// Emits additive puffs and smoke from randomly chosen companion model parts.
///
/// States 0/1 initialize/run steady emission; 2/3 initialize/run the shrinking
/// puffs and smoke tail. The scene owns teardown: the signed halfword counter
/// wraps and never kills this task. Requires a live companion TMD with at least
/// twenty coordinates throughout emission and while spawned effects borrow
/// their anchors and the shared offset vector.
static void _actor450200CompanionPuffTask(Task* task)
{
    enum {
        ACTOR_450200_PUFF_INIT            = 0,
        ACTOR_450200_PUFF_STEADY          = 1,
        ACTOR_450200_PUFF_FADE_INIT       = 2,
        ACTOR_450200_PUFF_FADE            = 3,
        ACTOR_450200_PUFF_ANCHOR_COUNT    = 11,
        ACTOR_450200_PUFF_STEADY_SEED     = 100,
        ACTOR_450200_PUFF_FADE_TICKS      = 128,
        ACTOR_450200_PUFF_SMOKE_TAIL_MIN  = -31,
        ACTOR_450200_PUFF_STEADY_SPAWN    = 0x80000300,
        ACTOR_450200_PUFF_FADE_SPAWN_BASE = 0x80000080,
        ACTOR_450200_PUFF_SMOKE_SPAWN     = 0xF0010100,
    };
    Task*     companion;
    GfxCoord* anchor;
    s16       countdown;

    // The twelfth table byte is excluded; every tick advances rand before dispatch.
    companion = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    anchor    = &companion->extra.tmd->coords[D_actor_450200_8013885C[(rand() * ACTOR_450200_PUFF_ANCHOR_COUNT) >> 15]];
    switch (task->state) {
        case ACTOR_450200_PUFF_INIT:
            task->killCountdown = ACTOR_450200_PUFF_STEADY_SEED;
            task->state++;
            return;
        case ACTOR_450200_PUFF_STEADY:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if ((countdown & 1) == 0) {
                effectSpawn(EFFECT_ADDITIVE_PUFF, anchor, ACTOR_450200_PUFF_STEADY_SPAWN, NULL);
            }
            return;
        case ACTOR_450200_PUFF_FADE_INIT:
            task->killCountdown = ACTOR_450200_PUFF_FADE_TICKS;
            task->state++;
            return;
        case ACTOR_450200_PUFF_FADE:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if (countdown & 1) {
                if (countdown > 0) {
                    effectSpawn(EFFECT_ADDITIVE_PUFF, anchor, countdown * 2 + ACTOR_450200_PUFF_FADE_SPAWN_BASE,
                                &D_actor_450200_80138868);
                }
            } else if (countdown >= ACTOR_450200_PUFF_SMOKE_TAIL_MIN && (countdown & 7) == 0) {
                effectSpawn(EFFECT_SMOKE_PUFF, anchor, ACTOR_450200_PUFF_SMOKE_SPAWN, &D_actor_450200_80138868);
            }
            return;
    }
}

/// Fades the scene's head-turn blend weight toward full aiming or the current pose.
///
/// A full-range fade takes eight calls with `enabled` nonzero or sixteen with
/// it zero. Requires writable `aim`; only its Q12 `rate` changes, where zero
/// holds the current pose and `ONE` applies the aim. Updates wrap to a signed
/// halfword before endpoint checks, including outside the normal 0..ONE range.
static inline void _actor450200RampHeadAimWeight(AnimationHeadAim* aim, s32 enabled)
{
    enum {
        ACTOR_450200_HEAD_AIM_FADE_IN_TICKS  = 8,
        ACTOR_450200_HEAD_AIM_FADE_OUT_TICKS = 16,
    };
    s16 nextWeight;

    if (enabled != 0) {
        nextWeight = aim->rate + ONE / ACTOR_450200_HEAD_AIM_FADE_IN_TICKS;
        aim->rate  = nextWeight;
        if (nextWeight > ONE) {
            aim->rate = ONE;
        }
    } else {
        nextWeight = aim->rate - ONE / ACTOR_450200_HEAD_AIM_FADE_OUT_TICKS;
        aim->rate  = nextWeight;
        if (nextWeight < 0) {
            aim->rate = 0;
        }
    }
}

/// Fades the player's head turn toward the companion during talk scenes.
///
/// State 0 owns a zeroed `AnimationHeadAim` in `Task::work` and immediately runs
/// state 1. `spawnArg1` enables aiming when nonzero. Both actors must retain live
/// TMD models with at least five coordinates. Other states kill this task and
/// clear its tracked handle; allocation failure kills without clearing it.
static void _actor450200HeadAimTask(Task* task)
{
    enum {
        ACTOR_450200_HEAD_AIM_INIT  = 0,
        ACTOR_450200_HEAD_AIM_RUN   = 1,
        ACTOR_450200_HEAD_AIM_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 16,
    };
    Task*             looker;
    AnimationHeadAim* aim;

    looker = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (task->state) {
        case ACTOR_450200_HEAD_AIM_INIT:
            aim = memCalloc(sizeof(AnimationHeadAim), false);
            if (aim == NULL) {
                taskKill(task);
                return;
            }
            task->work      = aim;
            aim->yawLimit   = ACTOR_450200_HEAD_AIM_LIMIT;
            aim->pitchLimit = ACTOR_450200_HEAD_AIM_LIMIT;
            task->state++;
            /* fallthrough */
        case ACTOR_450200_HEAD_AIM_RUN:
            aim = task->work;
            _actor450200RampHeadAimWeight(aim, task->spawnArg1.value);
            animationAimHeadAt(looker, gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), aim);
            return;
        default:
            taskKill(task);
            D_actor_450200_801401E0 = NULL;
            return;
    }
}

/// Controls the companion scene's tracked puff emitter.
///
/// Commands are 0 stop, 1 start and 2 begin fading. Start always creates a new
/// task and replaces the tracked handle, even when one is already live; failed
/// spawning stores NULL. Other values write the live task's state unchanged.
static void _actor450200ControlCompanionPuffs(s32 command)
{
    enum { ACTOR_450200_PUFF_TASK_TABLE_INDEX = 1 };

    if (command == ACTOR_450200_PUFF_COMMAND_STOP) {
        if (D_actor_450200_801401E4 != NULL) {
            taskKill(D_actor_450200_801401E4);
            D_actor_450200_801401E4 = NULL;
        }
    } else if (command == ACTOR_450200_PUFF_COMMAND_START) {
        D_actor_450200_801401E4 = taskSpawnFromTable(D_actor_450200_80137A60, ACTOR_450200_PUFF_TASK_TABLE_INDEX, 0, 0);
    } else if (D_actor_450200_801401E4 != NULL) {
        D_actor_450200_801401E4->state = command;
    }
}

/// Requests cancellation of all room effects at the scene's cleanup points.
static void _actor450200CancelRoomEffects(void)
{
    roomEffectRequestCancelAll();
}

/// Sets the live player head-aim task's enable value (zero off, nonzero on).
///
/// Stores the full event argument; a missing task makes this a no-op.
static void _actor450200SetHeadAimEnabled(s32 enabled)
{
    if (D_actor_450200_801401E0 != NULL) {
        D_actor_450200_801401E0->spawnArg1.value = enabled;
    }
}

/// Updates the talk placement's yaw so the player faces the companion.
///
/// Both tasks must own live root coordinates in the same parent frame. Stores
/// a heading in 4096 units per turn; only yaw changes. The talk script applies
/// this payload to the player after this callback returns.
static void _actor450200UpdateTalkFacing(void)
{
    GfxCoord* target;
    GfxCoord* looker;

    target = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    looker = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    actorRenderComposeCoord(target);
    actorRenderComposeCoord(looker);
    D_actor_450200_80137DC4.rot.vy =
        ratan2(target->coord.t[0] - looker->coord.t[0], target->coord.t[2] - looker->coord.t[2]) & ACTOR_TRANSFORM_ANGLE_MASK;
}

void actor450200StartCompanionTalk(void)
{
    enum {
        ACTOR_450200_TALK_FIRST  = 0,
        ACTOR_450200_TALK_SECOND = 1,
        ACTOR_450200_TALK_THIRD  = 2,
        ACTOR_450200_TALK_REPEAT = 3,
    };
    switch (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT)) {
        case ACTOR_450200_TALK_FIRST:
            evsStartScript(D_actor_450200_80138870, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT, ACTOR_450200_TALK_SECOND);
            break;
        case ACTOR_450200_TALK_SECOND:
            evsStartScript(D_actor_450200_80138A68, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT, ACTOR_450200_TALK_THIRD);
            break;
        case ACTOR_450200_TALK_THIRD:
            evsStartScript(D_actor_450200_80138C60, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT, ACTOR_450200_TALK_REPEAT);
            break;
        case ACTOR_450200_TALK_REPEAT:
            evsStartScript(D_actor_450200_80138E88, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

void func_actor_450200_801322F8(void)
{
    if (gameFlagGetNibble(GAME_FLAG_0D7) != 0) {
        evsStartScript(D_actor_450200_80139098, EVENT_SCRIPT_HUD_KEEP);
    } else {
        func_neo_ark_observatory_8017FA98(0);
    }
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        D_actor_450200_801401E0 = taskSpawnFromTable(D_actor_450200_80137A60, 2, 0, 0);
    }
}

/// Draws one backdrop layer as two adjacent 256-by-240 8-bit sprites.
///
/// `screenX` is the left edge in pixels before subtracting the 160-pixel screen
/// centre. `textureX` is a VRAM word X and `paletteY` selects the CLUT row; the
/// texture starts at row 256 and the 256-colour CLUT at X=0. Nonzero `rawTexture` ignores RGB
/// modulation; otherwise `brightness` supplies its low byte to all channels.
/// Nonzero `semiTransparent` enables additive blending of bit-15 texels.
/// Requires space for two SPRT/DR_MODE pairs in the current frame's packet arena
/// and at least 1023 ordering-table entries. Packets remain live through drawing.
static void _actor450200DrawBackdropLayer(s32 screenX, s32 textureX, s32 paletteY, s32 semiTransparent, s32 brightness, s32 rawTexture)
{
    enum {
        ACTOR_450200_BACKDROP_SPRITES       = 2,
        ACTOR_450200_BACKDROP_SPRITE_WIDTH  = 256,
        ACTOR_450200_BACKDROP_HEIGHT        = 240,
        ACTOR_450200_BACKDROP_SCREEN_HALF_X = 160,
        ACTOR_450200_BACKDROP_SCREEN_HALF_Y = 120,
        ACTOR_450200_BACKDROP_PAGE_WORDS    = 128,
        ACTOR_450200_BACKDROP_TEXTURE_8_BIT = 1,
        ACTOR_450200_BACKDROP_SORTING_TAG   = 1022,
    };
    SPRT*    sprite;
    DR_MODE* drawMode;
    s32      tileIndex;

    for (tileIndex = 0; tileIndex < ACTOR_450200_BACKDROP_SPRITES; tileIndex++) {
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setSprt(sprite);
        setShadeTex(sprite, rawTexture);
        setSemiTrans(sprite, semiTransparent);
        sprite->x0   = screenX - ACTOR_450200_BACKDROP_SCREEN_HALF_X;
        sprite->y0   = -ACTOR_450200_BACKDROP_SCREEN_HALF_Y;
        sprite->w    = ACTOR_450200_BACKDROP_SPRITE_WIDTH;
        sprite->u0   = 0;
        sprite->v0   = 0;
        sprite->h    = ACTOR_450200_BACKDROP_HEIGHT;
        sprite->r0   = brightness;
        sprite->g0   = brightness;
        sprite->b0   = brightness;
        sprite->clut = GetClut(0, paletteY);
        addPrim(&gGpuCurrentOt[ACTOR_450200_BACKDROP_SORTING_TAG], sprite);

        // Prepending the draw mode makes it execute before its sprite.
        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setDrawTPage(drawMode, 0, 1, getTPage(ACTOR_450200_BACKDROP_TEXTURE_8_BIT, GPU_BLEND_ADD, textureX, ACTOR_450200_BACKDROP_TEXTURE_Y));
        addPrim(&gGpuCurrentOt[ACTOR_450200_BACKDROP_SORTING_TAG], drawMode);

        textureX += ACTOR_450200_BACKDROP_PAGE_WORDS;
        screenX  += ACTOR_450200_BACKDROP_SPRITE_WIDTH;
    }
}

/// Scales and uploads a 256-entry RGB555 backdrop palette with bit 15 set.
///
/// `scale` is a Q7 weight, normally 0..128. Both arrays contain 256 halfwords;
/// they may be identical, but otherwise must not overlap. `scaledColors` must
/// be word-aligned and remain live until the GPU transfer completes. Uploads
/// one row at VRAM word coordinates (0, `vramY`); retains no source pointer.
static inline void _actor450200LoadScaledClut(const u16* sourceColors, u16* scaledColors, s32 scale, s32 vramY)
{
    enum {
        ACTOR_450200_CLUT_CHANNEL_MASK     = 31,
        ACTOR_450200_CLUT_COLOR_MASK       = 0x7FFF,
        ACTOR_450200_CLUT_SCALE_SHIFT      = 7,
        ACTOR_450200_CLUT_SCALED_BYTE_MASK = 255,
    };
    RECT rect;
    s32  colorIndex;
    u32  channelValue;
    u32  green;
    u32  red;
    u32  packedColor;

    for (colorIndex = 0; colorIndex < ACTOR_450200_BACKDROP_CLUT_COLORS; colorIndex++) {
        channelValue             = ((sourceColors[colorIndex] >> 10) & ACTOR_450200_CLUT_CHANNEL_MASK) * scale;
        green                    = ((sourceColors[colorIndex] >> 5) & ACTOR_450200_CLUT_CHANNEL_MASK) * scale;
        red                      = ((u8)sourceColors[colorIndex] & ACTOR_450200_CLUT_CHANNEL_MASK) * scale;
        packedColor              = channelValue >> ACTOR_450200_CLUT_SCALE_SHIFT;
        green                  >>= ACTOR_450200_CLUT_SCALE_SHIFT;
        packedColor             &= ACTOR_450200_CLUT_SCALED_BYTE_MASK;
        packedColor            <<= 10;
        packedColor             |= ~ACTOR_450200_CLUT_COLOR_MASK;
        green                   &= ACTOR_450200_CLUT_SCALED_BYTE_MASK;
        green                  <<= 5;
        packedColor             |= green;
        channelValue             = red >> ACTOR_450200_CLUT_SCALE_SHIFT;
        channelValue            &= ACTOR_450200_CLUT_SCALED_BYTE_MASK;
        channelValue            |= packedColor;
        scaledColors[colorIndex] = channelValue;
    }
    setRECT(&rect, 0, vramY, ACTOR_450200_BACKDROP_CLUT_COLORS, 1);
    LoadImage(&rect, (u_long*)scaledColors);
}

/// Cross-fades the scene's two backdrop layers while raising beam intensity.
///
/// State 0 captures two 256-colour source palettes; state 1 lowers the outgoing
/// Q7 weight by four per tick and draws the destination opaquely after the fade.
/// View 8 kills the task without restoring palettes. Requires the observatory
/// overlay, backdrop textures and frame drawing resources. Palette buffers are
/// shared, so the scene must run only one of these tasks at a time.
static void _actor450200BackdropCrossFadeTask(Task* task)
{
    enum {
        ACTOR_450200_BACKDROP_CAPTURE            = 0,
        ACTOR_450200_BACKDROP_DRAW               = 1,
        ACTOR_450200_BACKDROP_TO_SCREEN_X        = -64,
        ACTOR_450200_BACKDROP_WEIGHT_STEP        = 4,
        ACTOR_450200_BACKDROP_NEUTRAL_BRIGHTNESS = 128,
    };
    RECT rect;
    s32  beamIntensity;

    if (gGameSession->location.loc.view == ACTOR_450200_BACKDROP_END_VIEW) {
        taskKill(task);
        return;
    }

    switch (task->state) {
        case ACTOR_450200_BACKDROP_CAPTURE:
            task->killCountdown = ACTOR_450200_BACKDROP_SCALE_ONE;
            task->state        += 1;
            setRECT(&rect, 0, ACTOR_450200_BACKDROP_FROM_SOURCE_CLUT_Y, ACTOR_450200_BACKDROP_CLUT_COLORS, 1);
            StoreImage(&rect, (u_long*)D_actor_450200_801401E8);
            rect.y = ACTOR_450200_BACKDROP_TO_SOURCE_CLUT_Y;
            StoreImage(&rect, (u_long*)D_actor_450200_801403E8);
            break;

        case ACTOR_450200_BACKDROP_DRAW:
            // Add the incoming palette over the outgoing one using complementary weights.
            if (task->killCountdown >= 0) {
                _actor450200DrawBackdropLayer(ACTOR_450200_BACKDROP_TO_SCREEN_X, ACTOR_450200_BACKDROP_TO_TEXTURE_X, ACTOR_450200_BACKDROP_TO_SCALED_CLUT_Y, true, ACTOR_450200_BACKDROP_NEUTRAL_BRIGHTNESS, true);
                _actor450200DrawBackdropLayer(0, ACTOR_450200_BACKDROP_FROM_TEXTURE_X, ACTOR_450200_BACKDROP_FROM_SCALED_CLUT_Y, false, ACTOR_450200_BACKDROP_NEUTRAL_BRIGHTNESS, true);
                _actor450200LoadScaledClut(D_actor_450200_801401E8, D_actor_450200_801405E8, task->killCountdown, ACTOR_450200_BACKDROP_FROM_SCALED_CLUT_Y);
                _actor450200LoadScaledClut(D_actor_450200_801403E8, D_actor_450200_801407E8, ACTOR_450200_BACKDROP_SCALE_ONE - task->killCountdown, ACTOR_450200_BACKDROP_TO_SCALED_CLUT_Y);
            } else {
                _actor450200DrawBackdropLayer(ACTOR_450200_BACKDROP_TO_SCREEN_X, ACTOR_450200_BACKDROP_TO_TEXTURE_X, ACTOR_450200_BACKDROP_TO_SCALED_CLUT_Y, false, ACTOR_450200_BACKDROP_NEUTRAL_BRIGHTNESS, true);
            }

            // Clamp after low-halfword wrapping, before passing the beam level to the room.
            beamIntensity = ACTOR_450200_BEAM_FULL_INTENSITY - (u16)task->killCountdown;
            if ((u32)(beamIntensity & 0xFFFF) >= (u32)ACTOR_450200_BEAM_FULL_INTENSITY) {
                beamIntensity = ACTOR_450200_BEAM_FULL_INTENSITY;
            }
            neoArkObservatorySetLightBeamIntensity(beamIntensity & 0xFFFF);
            task->killCountdown = (u16)task->killCountdown - ACTOR_450200_BACKDROP_WEIGHT_STEP;
            break;
    }
}

/// Starts a backdrop cross-fade for command 1; other commands do nothing.
///
/// The scene must avoid overlapping starts; the task uses shared palette buffers.
/// Spawn failure is ignored. View 8 terminates the task after the scene finishes.
static void _actor450200StartBackdropCrossFade(s32 command)
{
    if (command == ACTOR_450200_BACKDROP_COMMAND_START) {
        taskSpawnFromTable(&D_actor_450200_8013FB40, 0, 0, 0);
    }
}

/// Supplies an event argument's low halfword to the observatory beam setter.
///
/// The room overlay must be loaded. This adapter zero-extends the argument;
/// the setter stores it as a signed halfword without clamping. Scenes use 160.
static void _actor450200SetLightBeamIntensity(s32 intensity)
{
    neoArkObservatorySetLightBeamIntensity(intensity & 0xFFFF);
}

/// Updates the current area's room selector in the session and live save state.
///
/// The selector must be valid for the current area; both scene paths supply 2.
/// Performs no room loading or view change and leaves the other save slots alone.
static void _actor450200SetRoomVariant(u8 roomVariant)
{
    gGameSession->location.loc.room                            = roomVariant;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = roomVariant;
}
