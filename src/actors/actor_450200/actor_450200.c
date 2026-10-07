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
/// 0x3EE. Only its yaw changes, set by `func_actor_450200_8013219C`.
extern ActorTransform D_actor_450200_80137DC4;
extern Task*          D_actor_450200_801401E0;
extern Task*          D_actor_450200_801401E4;
extern u16            D_actor_450200_801401E8[256];
extern u16            D_actor_450200_801403E8[256];
extern u16            D_actor_450200_801405E8[256];
extern u16            D_actor_450200_801407E8[256];

void func_actor_450200_80132848(s32);
void func_actor_450200_80132880(s32);
void func_actor_450200_801328A0(u8);

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
void                        func_actor_450200_801320D4(s32);
void                        func_actor_450200_8013215C(void);
void                        func_actor_450200_8013217C(s32);
void                        func_actor_450200_8013219C(void);
void                        func_actor_450200_80132538(Task*);

void func_actor_450200_80131E24(Task*);
void func_actor_450200_80131FA8(Task*);

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
    { { { TASK_BODY_NONE, 32 } }, func_actor_450200_80131E24, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_actor_450200_80131FA8, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_801320D4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55070008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x4066000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_450200_80137CDC }, { .vibrationSegments = D_actor_450200_80137CE4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_450200_80137A84.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55070007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450200_8013215C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137BD8.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450200_80137D1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_80137B74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450200_8013215C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_801320D4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_801320D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450200_8013215C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_801320D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450200_8013215C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450200_8013219C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_80137DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_8013217C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_8013217C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80138A68[21] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_80137BD8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450200_8013219C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_80137DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_8013217C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_8013217C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80138C60[23] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450200_8013219C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_80137DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_8013217C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_8013217C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80138E88[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450200_80137BD8.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450200_8013219C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_80137DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_8013217C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_8013217C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

TaskDesc D_actor_450200_8013FB40 = { { { TASK_BODY_NONE, 32 } }, func_actor_450200_80132538, { .value = 0 } };

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_80132848 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450200_8013FB4C.data.playRequests[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 160 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450200_8013FC40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450200_801328A0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450200_80140078[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450200_801328A0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450200_80132880 }, { .value = 160 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

void               func_actor_450200_80132220(void);
void               func_actor_450200_801322F8(void);
static void        func_actor_450200_80132368(s32 x, s32 tpageX, s32 clutY, s32 semiTrans, s32 rgb, s32 shadeTex);
static inline void _actor450200LoadScaledClut(u16* src, u16* dst, s32 scale, s32 y);

/// Effect state machine of this actor's first sub-task: state 0 arms the
/// self-destruct countdown at 0x64 and state 2 re-arms it at 0x80, both then
/// stepping the state on; state 1 throws effect 0x60080 on every other frame,
/// state 3 splits into an odd branch that bursts 0x60080 with the countdown
/// scaled into the spawn argument while it is still positive and an even
/// branch that spawns a 0x60070 only every eighth frame -- the other two bits
/// of the odd/even split the two effects see. The part the effects hang off is
/// picked at random from the model's coordinate array: the 11-entry byte table
/// holds indices into it, which is why the load is unsigned and the stride is
/// `GfxCoord`.
void func_actor_450200_80131E24(Task* task)
{
    Task*     slot;
    GfxCoord* coord;
    s16       countdown;

    slot  = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    coord = &slot->extra.tmd->coords[D_actor_450200_8013885C[(rand() * 11) >> 15]];
    switch (task->state) {
        case 0:
            task->killCountdown = 0x64;
            task->state++;
            return;
        case 1:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if ((countdown & 1) == 0) {
                effectSpawn(EFFECT_ADDITIVE_PUFF, coord, 0x80000300, NULL);
            }
            return;
        case 2:
            task->killCountdown = 0x80;
            task->state++;
            return;
        case 3:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if (countdown & 1) {
                if (countdown > 0) {
                    effectSpawn(EFFECT_ADDITIVE_PUFF, coord, countdown * 2 + 0x80000080,
                                &D_actor_450200_80138868);
                }
            } else if (countdown >= -0x1F && (countdown & 7) == 0) {
                effectSpawn(EFFECT_SMOKE_PUFF, coord, 0xF0010100, &D_actor_450200_80138868);
            }
            return;
    }
}

/// Head-aim state of this actor's second sub-task: state 0 allocates the
/// `AnimationHeadAim` record into `Task::work` and seeds both clamps to
/// 0x100, state 1 ramps its `rate` up toward 0x1000 while `Task::spawnArg1` is
/// set and back down toward 0 while it is not, then hands the record to
/// `func_800B17D4` between the slot-3 task whose head turns and the
/// `gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)` task it turns toward. A failed allocation, and every
/// state past 1, kill the task; only the latter clears
/// `D_actor_450200_801401E0`, which is why the two `taskKill` calls are
/// distinct.
void func_actor_450200_80131FA8(Task* arg0)
{
    Task*             looker;
    AnimationHeadAim* aim;
    u16               rate;

    looker = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (arg0->state) {
        case 0:
            aim = memCalloc(sizeof(AnimationHeadAim), false);
            if (aim == NULL) {
                taskKill(arg0);
                return;
            }
            arg0->work      = aim;
            aim->yawLimit   = 0x100;
            aim->pitchLimit = 0x100;
            arg0->state++;
            /* fallthrough */
        case 1:
            aim = arg0->work;
            if (arg0->spawnArg1.value != 0) {
                rate      = aim->rate + 0x200;
                aim->rate = rate;
                if ((s16)rate > ONE) {
                    aim->rate = ONE;
                }
            } else {
                rate      = aim->rate - 0x100;
                aim->rate = rate;
                if ((s16)rate < 0) {
                    aim->rate = 0;
                }
            }
            func_800B17D4(looker, gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), aim);
            return;
        default:
            taskKill(arg0);
            D_actor_450200_801401E0 = NULL;
            return;
    }
}

/// Three-way control for the second spawned sub-task: 0 tears the live one
/// down, 1 spawns it fresh, anything else is a state write the sub-task sees.
/// Spawning is skipped when the sub-task is already running.
void func_actor_450200_801320D4(s32 arg0)
{
    if (arg0 == 0) {
        if (D_actor_450200_801401E4 != NULL) {
            taskKill(D_actor_450200_801401E4);
            D_actor_450200_801401E4 = NULL;
        }
    } else if (arg0 == 1) {
        D_actor_450200_801401E4 = taskSpawnFromTable(D_actor_450200_80137A60, 1, 0, 0);
    } else if (D_actor_450200_801401E4 != NULL) {
        D_actor_450200_801401E4->state = arg0;
    }
}

void func_actor_450200_8013215C(void)
{
    roomEffectRequestCancelAll();
}

void func_actor_450200_8013217C(s32 arg0)
{
    if (D_actor_450200_801401E0 != NULL) {
        D_actor_450200_801401E0->spawnArg1.value = arg0;
    }
}

/// Stores in the yaw of `D_actor_450200_80137DC4` the heading, as a 12-bit
/// angle, from the slot-3 task's root coordinate to the `gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)`
/// task's, refreshing both coordinates first so the X/Z offset is current.
void func_actor_450200_8013219C(void)
{
    GfxCoord* target;
    GfxCoord* looker;

    target = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
    looker = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actorRenderComposeCoord(target);
    actorRenderComposeCoord(looker);
    D_actor_450200_80137DC4.rot.vy =
        ratan2(target->coord.t[0] - looker->coord.t[0], target->coord.t[2] - looker->coord.t[2]) & 0xFFF;
}

void func_actor_450200_80132220(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT)) {
        case 0:
            func_800E8614(D_actor_450200_80138870, 0);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT, 1);
            break;
        case 1:
            func_800E8614(D_actor_450200_80138A68, 0);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT, 2);
            break;
        case 2:
            func_800E8614(D_actor_450200_80138C60, 0);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_COMPANION_TALK_COUNT, 3);
            break;
        case 3:
            func_800E8614(D_actor_450200_80138E88, 0);
            break;
    }
}

void func_actor_450200_801322F8(void)
{
    if (gameFlagGetNibble(GAME_FLAG_0D7) != 0) {
        func_800E8614(D_actor_450200_80139098, 1);
    } else {
        func_neo_ark_observatory_8017FA98(0);
    }
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        D_actor_450200_801401E0 = taskSpawnFromTable(D_actor_450200_80137A60, 2, 0, 0);
    }
}

static void func_actor_450200_80132368(s32 x, s32 tpageX, s32 clutY, s32 semiTrans, s32 rgb, s32 shadeTex)
{
    SPRT*    p;
    DR_MODE* dr;
    s32      i;

    for (i = 0; i < 2; i++) {
        p              = gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setSprt(p);
        setShadeTex(p, shadeTex);
        setSemiTrans(p, semiTrans);
        p->x0   = x - 0xA0;
        p->y0   = -0x78;
        p->w    = 0x100;
        p->u0   = 0;
        p->v0   = 0;
        p->h    = 0xF0;
        p->r0   = rgb;
        p->g0   = rgb;
        p->b0   = rgb;
        p->clut = GetClut(0, clutY);
        addPrim(&gGpuCurrentOt[0x3FE], p);

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, getTPage(1, 1, tpageX, 0x100));
        addPrim(&gGpuCurrentOt[0x3FE], dr);

        tpageX += 0x80;
        x      += 0x100;
    }
}

/// Scales the 256-entry 15-bit CLUT `src` by `scale`/0x80 per channel into
/// `dst`, setting the semi-transparency bit on every entry, and uploads it to
/// VRAM row `y`.
static inline void _actor450200LoadScaledClut(u16* src, u16* dst, s32 scale, s32 y)
{
    RECT rect;
    s32  i;
    u32  r;
    u32  g;
    u32  b;
    u32  col;

    for (i = 0; i < 0x100; i++) {
        r      = ((src[i] >> 10) & 0x1F) * scale;
        g      = ((src[i] >> 5) & 0x1F) * scale;
        b      = ((u8)src[i] & 0x1F) * scale;
        col    = r >> 7;
        g    >>= 7;
        col   &= 0xFF;
        col  <<= 10;
        col   |= ~0x7FFF;
        g     &= 0xFF;
        g    <<= 5;
        col   |= g;
        r      = b >> 7;
        r     &= 0xFF;
        r     |= col;
        dst[i] = r;
    }
    setRECT(&rect, 0, y, 0x100, 1);
    LoadImage(&rect, (u_long*)dst);
}

void func_actor_450200_80132538(Task* task)
{
    RECT rect;
    s32  state;
    s32  level;

    if (gGameSession->location.loc.view == 8) {
        taskKill(task);
        return;
    }

    state = task->state;
    switch (state) {
        case 0:
            task->killCountdown = 0x80;
            task->state        += 1;
            setRECT(&rect, 0, 0xF7, 0x100, 1);
            StoreImage(&rect, (u_long*)D_actor_450200_801401E8);
            rect.y = 0xF8;
            StoreImage(&rect, (u_long*)D_actor_450200_801403E8);
            break;

        case 1:
            if (task->killCountdown >= 0) {
                func_actor_450200_80132368(-0x40, 0x1C0, 0xFA, 1, 0x80, state);
                func_actor_450200_80132368(0, 0x140, 0xF9, 0, 0x80, state);
                _actor450200LoadScaledClut(D_actor_450200_801401E8, D_actor_450200_801405E8, task->killCountdown, 0xF9);
                _actor450200LoadScaledClut(D_actor_450200_801403E8, D_actor_450200_801407E8, 0x80 - task->killCountdown, 0xFA);
            } else {
                func_actor_450200_80132368(-0x40, 0x1C0, 0xFA, 0, 0x80, state);
            }

            level = 0xA0 - (u16)task->killCountdown;
            if ((u32)(level & 0xFFFF) >= 0xA0U) {
                level = 0xA0;
            }
            neoArkObservatorySetLightBeamIntensity(level & 0xFFFF);
            task->killCountdown = (u16)task->killCountdown - 4;
            break;
    }
}

void func_actor_450200_80132848(s32 arg0)
{
    if (arg0 == 1) {
        taskSpawnFromTable(&D_actor_450200_8013FB40, 0, 0, 0);
    }
}

void func_actor_450200_80132880(s32 arg0)
{
    neoArkObservatorySetLightBeamIntensity(arg0 & 0xFFFF);
}

void func_actor_450200_801328A0(u8 arg0)
{
    gGameSession->location.loc.room                            = arg0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = arg0;
}
