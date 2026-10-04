#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/ending.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/dryfield_night_garage.h"
#include "../../shared/screen_wave.h"

/// Descriptor 0 of the task table this package publishes to the garage room.
///
/// The room's area-resource entry for this package names the table by this
/// address with task index 0, and its interaction handler spawns the
/// descriptor after this one from the same address. The layout that entry
/// belongs to places no actor, so this descriptor is only ever the table's
/// base and is never spawned.
extern TaskDesc D_actor_136300_8013B11C;

/// The clips the package's scenes add to the companion's animation bank, with
/// the play requests stored after them.
///
/// The captioned scene the garage room starts and the script of the package's
/// own event task each send the companion a copy request for this storage
/// before they play any of the clips. The captioned scene's skip script plays
/// one without sending the request itself. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is more than the clip table holds: the fifteen set pointers occupy
/// extended ids 47-61, and the first 17 words of the play requests are written
/// into the bank after them. The companion's requests select ids 47-61 only,
/// so none of those request words is played as a clip.
///
/// The play requests are the first four of the fifteen the package keeps for
/// the companion, one per clip in id order, and are part of this object only
/// because the copied span reaches into the fourth; the other eleven follow as
/// separate objects.
typedef union {
    struct {
        AnimationSet*        sets[15];        // Companion clips for extended ids 47-61
        AnimationPlayRequest playRequests[4]; // Requests for extended ids 47-50; the scripts play all four on the companion
    } data;                                   // The records by name
    s32 words[35];                            // The same storage as the copy reads it; the last three words lie beyond the copied span
} _Actor136300CompanionAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor136300CompanionAnimationBankExtensionStorage, 140);

extern _Actor136300CompanionAnimationBankExtensionStorage D_actor_136300_8013B140;

/// The clips the package's scenes add to the player's animation bank, with
/// the play requests stored after them.
///
/// The captioned scene the garage room starts and the script of the package's
/// own event task each send the player a copy request for this storage before
/// they play any of the clips. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is more than the clip table holds: the 26 set pointers occupy
/// extended ids 47-72, and the first six words of the play requests are
/// written into the bank after them. The player's requests select ids 47-72
/// only, so none of those request words is played as a clip.
///
/// The play requests are the first two of the 26 the package keeps for the
/// player, one per clip in id order, and are part of this object only because
/// the copied span reaches into the second; the other 24 follow as separate
/// objects.
typedef union {
    struct {
        AnimationSet*        sets[26];        // Player clips for extended ids 47-72
        AnimationPlayRequest playRequests[2]; // Requests for extended ids 47 and 48; the captioned scene opens on the first, and nothing references the second
    } data;                                   // The records by name
    s32 words[36];                            // The same storage as the copy reads it; the last four words lie beyond the copied span
} _Actor136300PlayerAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor136300PlayerAnimationBankExtensionStorage, 144);

extern _Actor136300PlayerAnimationBankExtensionStorage D_actor_136300_8013B2A8;

extern TaskDesc D_actor_136300_8013B134;

extern AnimationPlayRequest D_actor_136300_8013B208;
extern AnimationPlayRequest D_actor_136300_8013B230;

/// Script pair handed to `func_800E8614` -- the first while the ending is being
/// armed, the second when the capture event is cancelled.
extern EvsCommand D_actor_136300_8013C5C8[];
extern EvsCommand D_actor_136300_8013C6C0[];

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running context, recomputed every frame.
extern s32 gScreenWaveRamp;

/// The ramp context the running wave task was spawned with, parked at spawn
/// so the tick reads the ramp through it.
extern ScreenWaveCtx* gScreenWaveCtx;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern ScreenWaveOscillator gScreenWaveColumns[13];
extern ScreenWaveOscillator gScreenWaveRows[32];

/// The ramp context the message handler seeds and hands to the screen-wave
/// task.
extern ScreenWaveCtx D_actor_136300_8013C99C;

/// Spawn table of the screen-wave task.
extern TaskDesc D_actor_136300_80132AC4[];

static AnimationSet _gActor136300Animation00F14;
static AnimationSet _gActor136300Animation010EC;
static AnimationSet _gActor136300Animation01508;
static AnimationSet _gActor136300Animation016C4;
static AnimationSet _gActor136300Animation018AC;
static AnimationSet _gActor136300Animation01B68;
static AnimationSet _gActor136300Animation01D48;
static AnimationSet _gActor136300Animation021A0;
static AnimationSet _gActor136300Animation027B8;
static AnimationSet _gActor136300Animation03164;
static AnimationSet _gActor136300Animation034AC;
static AnimationSet _gActor136300Animation036E8;
static AnimationSet _gActor136300Animation039F8;
static AnimationSet _gActor136300Animation04074;
static AnimationSet _gActor136300Animation04658;
void                func_actor_136300_8013267C(Task*);
void                func_actor_136300_80132854(Task*);

extern AnimationPlayRequest D_actor_136300_8013B1CC;
extern AnimationPlayRequest D_actor_136300_8013B1E0;
extern AnimationPlayRequest D_actor_136300_8013B1F4;
static AnimationSet         _gActor136300Animation0495C;
static AnimationSet         _gActor136300Animation04B60;
static AnimationSet         _gActor136300Animation04E40;
static AnimationSet         _gActor136300Animation05038;
static AnimationSet         _gActor136300Animation0538C;
static AnimationSet         _gActor136300Animation05584;
static AnimationSet         _gActor136300Animation05748;
static AnimationSet         _gActor136300Animation05A08;
static AnimationSet         _gActor136300Animation05BCC;
static AnimationSet         _gActor136300Animation05F84;
static AnimationSet         _gActor136300Animation062B0;
static AnimationSet         _gActor136300Animation06568;
static AnimationSet         _gActor136300Animation066F8;
static AnimationSet         _gActor136300Animation069B8;
static AnimationSet         _gActor136300Animation06D8C;
static AnimationSet         _gActor136300Animation0717C;
static AnimationSet         _gActor136300Animation074E8;
static AnimationSet         _gActor136300Animation07BE4;
static AnimationSet         _gActor136300Animation07E5C;
static AnimationSet         _gActor136300Animation082B0;
static AnimationSet         _gActor136300Animation08544;
static AnimationSet         _gActor136300Animation088AC;
static AnimationSet         _gActor136300Animation08A84;
static AnimationSet         _gActor136300Animation08D9C;
static AnimationSet         _gActor136300Animation08F98;
static AnimationSet         _gActor136300Animation092D4;
void                        func_actor_136300_801328D4(s8);
void                        func_actor_136300_801328E0(s32);
void                        func_actor_136300_80132910(s32);
void                        func_actor_136300_80132998(void);
void                        func_actor_136300_801329EC(void);
void                        func_actor_136300_80132A4C(s32);
void                        func_actor_136300_80132A7C(s32);

void func_actor_136300_801328E0(s32);
void func_actor_136300_80132910(s32);

TaskDesc D_actor_136300_80132AC4[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

static AnimationPackedPose _gActor136300Animation00F14Bank1[2] = {
#include "assets/actor_136300_animation_00F14_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation00F14Bank4[32] = {
#include "assets/actor_136300_animation_00F14_bank4.inc"
};

static AnimationRecord _gActor136300Animation00F14Records[101] = {
#include "assets/actor_136300_animation_00F14_records.inc"
};

static u16 _gActor136300Animation00F14Indices[20] = {
#include "assets/actor_136300_animation_00F14_indices.inc"
};

static AnimationSet _gActor136300Animation00F14 = {
    _gActor136300Animation00F14Records,
    _gActor136300Animation00F14Indices,
    { NULL, _gActor136300Animation00F14Bank1, NULL, NULL, _gActor136300Animation00F14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation010ECBank1[3] = {
#include "assets/actor_136300_animation_010EC_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation010ECBank4[29] = {
#include "assets/actor_136300_animation_010EC_bank4.inc"
};

static AnimationRecord _gActor136300Animation010ECRecords[60] = {
#include "assets/actor_136300_animation_010EC_records.inc"
};

static u16 _gActor136300Animation010ECIndices[20] = {
#include "assets/actor_136300_animation_010EC_indices.inc"
};

static AnimationSet _gActor136300Animation010EC = {
    _gActor136300Animation010ECRecords,
    _gActor136300Animation010ECIndices,
    { NULL, _gActor136300Animation010ECBank1, NULL, NULL, _gActor136300Animation010ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation01508Bank1[3] = {
#include "assets/actor_136300_animation_01508_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation01508Bank4[80] = {
#include "assets/actor_136300_animation_01508_bank4.inc"
};

static AnimationRecord _gActor136300Animation01508Records[154] = {
#include "assets/actor_136300_animation_01508_records.inc"
};

static u16 _gActor136300Animation01508Indices[20] = {
#include "assets/actor_136300_animation_01508_indices.inc"
};

static AnimationSet _gActor136300Animation01508 = {
    _gActor136300Animation01508Records,
    _gActor136300Animation01508Indices,
    { NULL, _gActor136300Animation01508Bank1, NULL, NULL, _gActor136300Animation01508Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation016C4Bank1[2] = {
#include "assets/actor_136300_animation_016C4_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation016C4Bank4[25] = {
#include "assets/actor_136300_animation_016C4_bank4.inc"
};

static AnimationRecord _gActor136300Animation016C4Records[60] = {
#include "assets/actor_136300_animation_016C4_records.inc"
};

static u16 _gActor136300Animation016C4Indices[20] = {
#include "assets/actor_136300_animation_016C4_indices.inc"
};

static AnimationSet _gActor136300Animation016C4 = {
    _gActor136300Animation016C4Records,
    _gActor136300Animation016C4Indices,
    { NULL, _gActor136300Animation016C4Bank1, NULL, NULL, _gActor136300Animation016C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation018ACBank1[3] = {
#include "assets/actor_136300_animation_018AC_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation018ACBank4[33] = {
#include "assets/actor_136300_animation_018AC_bank4.inc"
};

static AnimationRecord _gActor136300Animation018ACRecords[60] = {
#include "assets/actor_136300_animation_018AC_records.inc"
};

static u16 _gActor136300Animation018ACIndices[20] = {
#include "assets/actor_136300_animation_018AC_indices.inc"
};

static AnimationSet _gActor136300Animation018AC = {
    _gActor136300Animation018ACRecords,
    _gActor136300Animation018ACIndices,
    { NULL, _gActor136300Animation018ACBank1, NULL, NULL, _gActor136300Animation018ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation01B68Bank1[3] = {
#include "assets/actor_136300_animation_01B68_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation01B68Bank4[39] = {
#include "assets/actor_136300_animation_01B68_bank4.inc"
};

static AnimationRecord _gActor136300Animation01B68Records[107] = {
#include "assets/actor_136300_animation_01B68_records.inc"
};

static u16 _gActor136300Animation01B68Indices[20] = {
#include "assets/actor_136300_animation_01B68_indices.inc"
};

static AnimationSet _gActor136300Animation01B68 = {
    _gActor136300Animation01B68Records,
    _gActor136300Animation01B68Indices,
    { NULL, _gActor136300Animation01B68Bank1, NULL, NULL, _gActor136300Animation01B68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation01D48Bank1[3] = {
#include "assets/actor_136300_animation_01D48_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation01D48Bank4[31] = {
#include "assets/actor_136300_animation_01D48_bank4.inc"
};

static AnimationRecord _gActor136300Animation01D48Records[60] = {
#include "assets/actor_136300_animation_01D48_records.inc"
};

static u16 _gActor136300Animation01D48Indices[20] = {
#include "assets/actor_136300_animation_01D48_indices.inc"
};

static AnimationSet _gActor136300Animation01D48 = {
    _gActor136300Animation01D48Records,
    _gActor136300Animation01D48Indices,
    { NULL, _gActor136300Animation01D48Bank1, NULL, NULL, _gActor136300Animation01D48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation021A0Bank1[7] = {
#include "assets/actor_136300_animation_021A0_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation021A0Bank4[85] = {
#include "assets/actor_136300_animation_021A0_bank4.inc"
};

static AnimationRecord _gActor136300Animation021A0Records[152] = {
#include "assets/actor_136300_animation_021A0_records.inc"
};

static u16 _gActor136300Animation021A0Indices[20] = {
#include "assets/actor_136300_animation_021A0_indices.inc"
};

static AnimationSet _gActor136300Animation021A0 = {
    _gActor136300Animation021A0Records,
    _gActor136300Animation021A0Indices,
    { NULL, _gActor136300Animation021A0Bank1, NULL, NULL, _gActor136300Animation021A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation027B8Bank1[4] = {
#include "assets/actor_136300_animation_027B8_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation027B8Bank4[147] = {
#include "assets/actor_136300_animation_027B8_bank4.inc"
};

static AnimationRecord _gActor136300Animation027B8Records[211] = {
#include "assets/actor_136300_animation_027B8_records.inc"
};

static u16 _gActor136300Animation027B8Indices[20] = {
#include "assets/actor_136300_animation_027B8_indices.inc"
};

static AnimationSet _gActor136300Animation027B8 = {
    _gActor136300Animation027B8Records,
    _gActor136300Animation027B8Indices,
    { NULL, _gActor136300Animation027B8Bank1, NULL, NULL, _gActor136300Animation027B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation03164Bank1[9] = {
#include "assets/actor_136300_animation_03164_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation03164Bank4[245] = {
#include "assets/actor_136300_animation_03164_bank4.inc"
};

static AnimationRecord _gActor136300Animation03164Records[327] = {
#include "assets/actor_136300_animation_03164_records.inc"
};

static u16 _gActor136300Animation03164Indices[20] = {
#include "assets/actor_136300_animation_03164_indices.inc"
};

static AnimationSet _gActor136300Animation03164 = {
    _gActor136300Animation03164Records,
    _gActor136300Animation03164Indices,
    { NULL, _gActor136300Animation03164Bank1, NULL, NULL, _gActor136300Animation03164Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation034ACBank1[6] = {
#include "assets/actor_136300_animation_034AC_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation034ACBank4[69] = {
#include "assets/actor_136300_animation_034AC_bank4.inc"
};

static AnimationRecord _gActor136300Animation034ACRecords[103] = {
#include "assets/actor_136300_animation_034AC_records.inc"
};

static u16 _gActor136300Animation034ACIndices[20] = {
#include "assets/actor_136300_animation_034AC_indices.inc"
};

static AnimationSet _gActor136300Animation034AC = {
    _gActor136300Animation034ACRecords,
    _gActor136300Animation034ACIndices,
    { NULL, _gActor136300Animation034ACBank1, NULL, NULL, _gActor136300Animation034ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation036E8Bank1[3] = {
#include "assets/actor_136300_animation_036E8_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation036E8Bank4[24] = {
#include "assets/actor_136300_animation_036E8_bank4.inc"
};

static AnimationRecord _gActor136300Animation036E8Records[90] = {
#include "assets/actor_136300_animation_036E8_records.inc"
};

static u16 _gActor136300Animation036E8Indices[20] = {
#include "assets/actor_136300_animation_036E8_indices.inc"
};

static AnimationSet _gActor136300Animation036E8 = {
    _gActor136300Animation036E8Records,
    _gActor136300Animation036E8Indices,
    { NULL, _gActor136300Animation036E8Bank1, NULL, NULL, _gActor136300Animation036E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation039F8Bank1[6] = {
#include "assets/actor_136300_animation_039F8_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation039F8Bank4[61] = {
#include "assets/actor_136300_animation_039F8_bank4.inc"
};

static AnimationRecord _gActor136300Animation039F8Records[97] = {
#include "assets/actor_136300_animation_039F8_records.inc"
};

static u16 _gActor136300Animation039F8Indices[20] = {
#include "assets/actor_136300_animation_039F8_indices.inc"
};

static AnimationSet _gActor136300Animation039F8 = {
    _gActor136300Animation039F8Records,
    _gActor136300Animation039F8Indices,
    { NULL, _gActor136300Animation039F8Bank1, NULL, NULL, _gActor136300Animation039F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation04074Bank1[10] = {
#include "assets/actor_136300_animation_04074_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation04074Bank4[142] = {
#include "assets/actor_136300_animation_04074_bank4.inc"
};

static AnimationRecord _gActor136300Animation04074Records[223] = {
#include "assets/actor_136300_animation_04074_records.inc"
};

static u16 _gActor136300Animation04074Indices[20] = {
#include "assets/actor_136300_animation_04074_indices.inc"
};

static AnimationSet _gActor136300Animation04074 = {
    _gActor136300Animation04074Records,
    _gActor136300Animation04074Indices,
    { NULL, _gActor136300Animation04074Bank1, NULL, NULL, _gActor136300Animation04074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation04658Bank1[2] = {
#include "assets/actor_136300_animation_04658_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation04658Bank4[152] = {
#include "assets/actor_136300_animation_04658_bank4.inc"
};

static AnimationRecord _gActor136300Animation04658Records[199] = {
#include "assets/actor_136300_animation_04658_records.inc"
};

static u16 _gActor136300Animation04658Indices[20] = {
#include "assets/actor_136300_animation_04658_indices.inc"
};

static AnimationSet _gActor136300Animation04658 = {
    _gActor136300Animation04658Records,
    _gActor136300Animation04658Indices,
    { NULL, _gActor136300Animation04658Bank1, NULL, NULL, _gActor136300Animation04658Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation0495CBank1[6] = {
#include "assets/actor_136300_animation_0495C_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation0495CBank4[46] = {
#include "assets/actor_136300_animation_0495C_bank4.inc"
};

static AnimationRecord _gActor136300Animation0495CRecords[109] = {
#include "assets/actor_136300_animation_0495C_records.inc"
};

static u16 _gActor136300Animation0495CIndices[20] = {
#include "assets/actor_136300_animation_0495C_indices.inc"
};

static AnimationSet _gActor136300Animation0495C = {
    _gActor136300Animation0495CRecords,
    _gActor136300Animation0495CIndices,
    { NULL, _gActor136300Animation0495CBank1, NULL, NULL, _gActor136300Animation0495CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation04B60Bank1[2] = {
#include "assets/actor_136300_animation_04B60_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation04B60Bank4[21] = {
#include "assets/actor_136300_animation_04B60_bank4.inc"
};

static AnimationRecord _gActor136300Animation04B60Records[82] = {
#include "assets/actor_136300_animation_04B60_records.inc"
};

static u16 _gActor136300Animation04B60Indices[20] = {
#include "assets/actor_136300_animation_04B60_indices.inc"
};

static AnimationSet _gActor136300Animation04B60 = {
    _gActor136300Animation04B60Records,
    _gActor136300Animation04B60Indices,
    { NULL, _gActor136300Animation04B60Bank1, NULL, NULL, _gActor136300Animation04B60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation04E40Bank1[5] = {
#include "assets/actor_136300_animation_04E40_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation04E40Bank4[61] = {
#include "assets/actor_136300_animation_04E40_bank4.inc"
};

static AnimationRecord _gActor136300Animation04E40Records[88] = {
#include "assets/actor_136300_animation_04E40_records.inc"
};

static u16 _gActor136300Animation04E40Indices[20] = {
#include "assets/actor_136300_animation_04E40_indices.inc"
};

static AnimationSet _gActor136300Animation04E40 = {
    _gActor136300Animation04E40Records,
    _gActor136300Animation04E40Indices,
    { NULL, _gActor136300Animation04E40Bank1, NULL, NULL, _gActor136300Animation04E40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation05038Bank1[3] = {
#include "assets/actor_136300_animation_05038_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation05038Bank4[34] = {
#include "assets/actor_136300_animation_05038_bank4.inc"
};

static AnimationRecord _gActor136300Animation05038Records[63] = {
#include "assets/actor_136300_animation_05038_records.inc"
};

static u16 _gActor136300Animation05038Indices[20] = {
#include "assets/actor_136300_animation_05038_indices.inc"
};

static AnimationSet _gActor136300Animation05038 = {
    _gActor136300Animation05038Records,
    _gActor136300Animation05038Indices,
    { NULL, _gActor136300Animation05038Bank1, NULL, NULL, _gActor136300Animation05038Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation0538CBank1[5] = {
#include "assets/actor_136300_animation_0538C_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation0538CBank4[58] = {
#include "assets/actor_136300_animation_0538C_bank4.inc"
};

static AnimationRecord _gActor136300Animation0538CRecords[120] = {
#include "assets/actor_136300_animation_0538C_records.inc"
};

static u16 _gActor136300Animation0538CIndices[20] = {
#include "assets/actor_136300_animation_0538C_indices.inc"
};

static AnimationSet _gActor136300Animation0538C = {
    _gActor136300Animation0538CRecords,
    _gActor136300Animation0538CIndices,
    { NULL, _gActor136300Animation0538CBank1, NULL, NULL, _gActor136300Animation0538CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation05584Bank1[3] = {
#include "assets/actor_136300_animation_05584_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation05584Bank4[34] = {
#include "assets/actor_136300_animation_05584_bank4.inc"
};

static AnimationRecord _gActor136300Animation05584Records[63] = {
#include "assets/actor_136300_animation_05584_records.inc"
};

static u16 _gActor136300Animation05584Indices[20] = {
#include "assets/actor_136300_animation_05584_indices.inc"
};

static AnimationSet _gActor136300Animation05584 = {
    _gActor136300Animation05584Records,
    _gActor136300Animation05584Indices,
    { NULL, _gActor136300Animation05584Bank1, NULL, NULL, _gActor136300Animation05584Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation05748Bank1[3] = {
#include "assets/actor_136300_animation_05748_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation05748Bank4[27] = {
#include "assets/actor_136300_animation_05748_bank4.inc"
};

static AnimationRecord _gActor136300Animation05748Records[57] = {
#include "assets/actor_136300_animation_05748_records.inc"
};

static u16 _gActor136300Animation05748Indices[20] = {
#include "assets/actor_136300_animation_05748_indices.inc"
};

static AnimationSet _gActor136300Animation05748 = {
    _gActor136300Animation05748Records,
    _gActor136300Animation05748Indices,
    { NULL, _gActor136300Animation05748Bank1, NULL, NULL, _gActor136300Animation05748Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation05A08Bank1[4] = {
#include "assets/actor_136300_animation_05A08_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation05A08Bank4[40] = {
#include "assets/actor_136300_animation_05A08_bank4.inc"
};

static AnimationRecord _gActor136300Animation05A08Records[104] = {
#include "assets/actor_136300_animation_05A08_records.inc"
};

static u16 _gActor136300Animation05A08Indices[20] = {
#include "assets/actor_136300_animation_05A08_indices.inc"
};

static AnimationSet _gActor136300Animation05A08 = {
    _gActor136300Animation05A08Records,
    _gActor136300Animation05A08Indices,
    { NULL, _gActor136300Animation05A08Bank1, NULL, NULL, _gActor136300Animation05A08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation05BCCBank1[3] = {
#include "assets/actor_136300_animation_05BCC_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation05BCCBank4[27] = {
#include "assets/actor_136300_animation_05BCC_bank4.inc"
};

static AnimationRecord _gActor136300Animation05BCCRecords[57] = {
#include "assets/actor_136300_animation_05BCC_records.inc"
};

static u16 _gActor136300Animation05BCCIndices[20] = {
#include "assets/actor_136300_animation_05BCC_indices.inc"
};

static AnimationSet _gActor136300Animation05BCC = {
    _gActor136300Animation05BCCRecords,
    _gActor136300Animation05BCCIndices,
    { NULL, _gActor136300Animation05BCCBank1, NULL, NULL, _gActor136300Animation05BCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation05F84Bank1[3] = {
#include "assets/actor_136300_animation_05F84_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation05F84Bank4[81] = {
#include "assets/actor_136300_animation_05F84_bank4.inc"
};

static AnimationRecord _gActor136300Animation05F84Records[128] = {
#include "assets/actor_136300_animation_05F84_records.inc"
};

static u16 _gActor136300Animation05F84Indices[20] = {
#include "assets/actor_136300_animation_05F84_indices.inc"
};

static AnimationSet _gActor136300Animation05F84 = {
    _gActor136300Animation05F84Records,
    _gActor136300Animation05F84Indices,
    { NULL, _gActor136300Animation05F84Bank1, NULL, NULL, _gActor136300Animation05F84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation062B0Bank1[7] = {
#include "assets/actor_136300_animation_062B0_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation062B0Bank4[56] = {
#include "assets/actor_136300_animation_062B0_bank4.inc"
};

static AnimationRecord _gActor136300Animation062B0Records[106] = {
#include "assets/actor_136300_animation_062B0_records.inc"
};

static u16 _gActor136300Animation062B0Indices[20] = {
#include "assets/actor_136300_animation_062B0_indices.inc"
};

static AnimationSet _gActor136300Animation062B0 = {
    _gActor136300Animation062B0Records,
    _gActor136300Animation062B0Indices,
    { NULL, _gActor136300Animation062B0Bank1, NULL, NULL, _gActor136300Animation062B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation06568Bank1[4] = {
#include "assets/actor_136300_animation_06568_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation06568Bank4[57] = {
#include "assets/actor_136300_animation_06568_bank4.inc"
};

static AnimationRecord _gActor136300Animation06568Records[85] = {
#include "assets/actor_136300_animation_06568_records.inc"
};

static u16 _gActor136300Animation06568Indices[20] = {
#include "assets/actor_136300_animation_06568_indices.inc"
};

static AnimationSet _gActor136300Animation06568 = {
    _gActor136300Animation06568Records,
    _gActor136300Animation06568Indices,
    { NULL, _gActor136300Animation06568Bank1, NULL, NULL, _gActor136300Animation06568Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation066F8Bank1[2] = {
#include "assets/actor_136300_animation_066F8_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation066F8Bank4[17] = {
#include "assets/actor_136300_animation_066F8_bank4.inc"
};

static AnimationRecord _gActor136300Animation066F8Records[57] = {
#include "assets/actor_136300_animation_066F8_records.inc"
};

static u16 _gActor136300Animation066F8Indices[20] = {
#include "assets/actor_136300_animation_066F8_indices.inc"
};

static AnimationSet _gActor136300Animation066F8 = {
    _gActor136300Animation066F8Records,
    _gActor136300Animation066F8Indices,
    { NULL, _gActor136300Animation066F8Bank1, NULL, NULL, _gActor136300Animation066F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation069B8Bank1[5] = {
#include "assets/actor_136300_animation_069B8_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation069B8Bank4[56] = {
#include "assets/actor_136300_animation_069B8_bank4.inc"
};

static AnimationRecord _gActor136300Animation069B8Records[85] = {
#include "assets/actor_136300_animation_069B8_records.inc"
};

static u16 _gActor136300Animation069B8Indices[20] = {
#include "assets/actor_136300_animation_069B8_indices.inc"
};

static AnimationSet _gActor136300Animation069B8 = {
    _gActor136300Animation069B8Records,
    _gActor136300Animation069B8Indices,
    { NULL, _gActor136300Animation069B8Bank1, NULL, NULL, _gActor136300Animation069B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation06D8CBank1[8] = {
#include "assets/actor_136300_animation_06D8C_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation06D8CBank4[84] = {
#include "assets/actor_136300_animation_06D8C_bank4.inc"
};

static AnimationRecord _gActor136300Animation06D8CRecords[117] = {
#include "assets/actor_136300_animation_06D8C_records.inc"
};

static u16 _gActor136300Animation06D8CIndices[20] = {
#include "assets/actor_136300_animation_06D8C_indices.inc"
};

static AnimationSet _gActor136300Animation06D8C = {
    _gActor136300Animation06D8CRecords,
    _gActor136300Animation06D8CIndices,
    { NULL, _gActor136300Animation06D8CBank1, NULL, NULL, _gActor136300Animation06D8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation0717CBank1[7] = {
#include "assets/actor_136300_animation_0717C_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation0717CBank4[62] = {
#include "assets/actor_136300_animation_0717C_bank4.inc"
};

static AnimationRecord _gActor136300Animation0717CRecords[149] = {
#include "assets/actor_136300_animation_0717C_records.inc"
};

static u16 _gActor136300Animation0717CIndices[20] = {
#include "assets/actor_136300_animation_0717C_indices.inc"
};

static AnimationSet _gActor136300Animation0717C = {
    _gActor136300Animation0717CRecords,
    _gActor136300Animation0717CIndices,
    { NULL, _gActor136300Animation0717CBank1, NULL, NULL, _gActor136300Animation0717CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation074E8Bank1[7] = {
#include "assets/actor_136300_animation_074E8_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation074E8Bank4[74] = {
#include "assets/actor_136300_animation_074E8_bank4.inc"
};

static AnimationRecord _gActor136300Animation074E8Records[104] = {
#include "assets/actor_136300_animation_074E8_records.inc"
};

static u16 _gActor136300Animation074E8Indices[20] = {
#include "assets/actor_136300_animation_074E8_indices.inc"
};

static AnimationSet _gActor136300Animation074E8 = {
    _gActor136300Animation074E8Records,
    _gActor136300Animation074E8Indices,
    { NULL, _gActor136300Animation074E8Bank1, NULL, NULL, _gActor136300Animation074E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation07BE4Bank1[10] = {
#include "assets/actor_136300_animation_07BE4_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation07BE4Bank4[147] = {
#include "assets/actor_136300_animation_07BE4_bank4.inc"
};

static AnimationRecord _gActor136300Animation07BE4Records[250] = {
#include "assets/actor_136300_animation_07BE4_records.inc"
};

static u16 _gActor136300Animation07BE4Indices[20] = {
#include "assets/actor_136300_animation_07BE4_indices.inc"
};

static AnimationSet _gActor136300Animation07BE4 = {
    _gActor136300Animation07BE4Records,
    _gActor136300Animation07BE4Indices,
    { NULL, _gActor136300Animation07BE4Bank1, NULL, NULL, _gActor136300Animation07BE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation07E5CBank1[4] = {
#include "assets/actor_136300_animation_07E5C_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation07E5CBank4[51] = {
#include "assets/actor_136300_animation_07E5C_bank4.inc"
};

static AnimationRecord _gActor136300Animation07E5CRecords[75] = {
#include "assets/actor_136300_animation_07E5C_records.inc"
};

static u16 _gActor136300Animation07E5CIndices[20] = {
#include "assets/actor_136300_animation_07E5C_indices.inc"
};

static AnimationSet _gActor136300Animation07E5C = {
    _gActor136300Animation07E5CRecords,
    _gActor136300Animation07E5CIndices,
    { NULL, _gActor136300Animation07E5CBank1, NULL, NULL, _gActor136300Animation07E5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation082B0Bank1[5] = {
#include "assets/actor_136300_animation_082B0_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation082B0Bank4[80] = {
#include "assets/actor_136300_animation_082B0_bank4.inc"
};

static AnimationRecord _gActor136300Animation082B0Records[162] = {
#include "assets/actor_136300_animation_082B0_records.inc"
};

static u16 _gActor136300Animation082B0Indices[20] = {
#include "assets/actor_136300_animation_082B0_indices.inc"
};

static AnimationSet _gActor136300Animation082B0 = {
    _gActor136300Animation082B0Records,
    _gActor136300Animation082B0Indices,
    { NULL, _gActor136300Animation082B0Bank1, NULL, NULL, _gActor136300Animation082B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation08544Bank1[4] = {
#include "assets/actor_136300_animation_08544_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation08544Bank4[51] = {
#include "assets/actor_136300_animation_08544_bank4.inc"
};

static AnimationRecord _gActor136300Animation08544Records[82] = {
#include "assets/actor_136300_animation_08544_records.inc"
};

static u16 _gActor136300Animation08544Indices[20] = {
#include "assets/actor_136300_animation_08544_indices.inc"
};

static AnimationSet _gActor136300Animation08544 = {
    _gActor136300Animation08544Records,
    _gActor136300Animation08544Indices,
    { NULL, _gActor136300Animation08544Bank1, NULL, NULL, _gActor136300Animation08544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation088ACBank1[5] = {
#include "assets/actor_136300_animation_088AC_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation088ACBank4[74] = {
#include "assets/actor_136300_animation_088AC_bank4.inc"
};

static AnimationRecord _gActor136300Animation088ACRecords[109] = {
#include "assets/actor_136300_animation_088AC_records.inc"
};

static u16 _gActor136300Animation088ACIndices[20] = {
#include "assets/actor_136300_animation_088AC_indices.inc"
};

static AnimationSet _gActor136300Animation088AC = {
    _gActor136300Animation088ACRecords,
    _gActor136300Animation088ACIndices,
    { NULL, _gActor136300Animation088ACBank1, NULL, NULL, _gActor136300Animation088ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation08A84Bank1[2] = {
#include "assets/actor_136300_animation_08A84_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation08A84Bank4[16] = {
#include "assets/actor_136300_animation_08A84_bank4.inc"
};

static AnimationRecord _gActor136300Animation08A84Records[76] = {
#include "assets/actor_136300_animation_08A84_records.inc"
};

static u16 _gActor136300Animation08A84Indices[20] = {
#include "assets/actor_136300_animation_08A84_indices.inc"
};

static AnimationSet _gActor136300Animation08A84 = {
    _gActor136300Animation08A84Records,
    _gActor136300Animation08A84Indices,
    { NULL, _gActor136300Animation08A84Bank1, NULL, NULL, _gActor136300Animation08A84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation08D9CBank1[5] = {
#include "assets/actor_136300_animation_08D9C_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation08D9CBank4[66] = {
#include "assets/actor_136300_animation_08D9C_bank4.inc"
};

static AnimationRecord _gActor136300Animation08D9CRecords[97] = {
#include "assets/actor_136300_animation_08D9C_records.inc"
};

static u16 _gActor136300Animation08D9CIndices[20] = {
#include "assets/actor_136300_animation_08D9C_indices.inc"
};

static AnimationSet _gActor136300Animation08D9C = {
    _gActor136300Animation08D9CRecords,
    _gActor136300Animation08D9CIndices,
    { NULL, _gActor136300Animation08D9CBank1, NULL, NULL, _gActor136300Animation08D9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation08F98Bank1[4] = {
#include "assets/actor_136300_animation_08F98_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation08F98Bank4[17] = {
#include "assets/actor_136300_animation_08F98_bank4.inc"
};

static AnimationRecord _gActor136300Animation08F98Records[78] = {
#include "assets/actor_136300_animation_08F98_records.inc"
};

static u16 _gActor136300Animation08F98Indices[20] = {
#include "assets/actor_136300_animation_08F98_indices.inc"
};

static AnimationSet _gActor136300Animation08F98 = {
    _gActor136300Animation08F98Records,
    _gActor136300Animation08F98Indices,
    { NULL, _gActor136300Animation08F98Bank1, NULL, NULL, _gActor136300Animation08F98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136300Animation092D4Bank1[8] = {
#include "assets/actor_136300_animation_092D4_bank1.inc"
};

static AnimationPackedRotation _gActor136300Animation092D4Bank4[65] = {
#include "assets/actor_136300_animation_092D4_bank4.inc"
};

static AnimationRecord _gActor136300Animation092D4Records[98] = {
#include "assets/actor_136300_animation_092D4_records.inc"
};

static u16 _gActor136300Animation092D4Indices[20] = {
#include "assets/actor_136300_animation_092D4_indices.inc"
};

static AnimationSet _gActor136300Animation092D4 = {
    _gActor136300Animation092D4Records,
    _gActor136300Animation092D4Indices,
    { NULL, _gActor136300Animation092D4Bank1, NULL, NULL, _gActor136300Animation092D4Bank4, NULL, NULL, NULL },
};

// `enemyDestroy` takes the enemy work object before its task, so it does not
// have the one-argument shape a task callback is called with.
TaskDesc D_actor_136300_8013B11C = { { { TASK_BODY_NONE, 192 } }, (TaskFunc)enemyDestroy, { .value = 0 } };

TaskDesc D_actor_136300_8013B128 = { { { TASK_BODY_NONE, 32 } }, func_actor_136300_8013267C, { .value = 0 } };

TaskDesc D_actor_136300_8013B134 = { { { TASK_BODY_NONE, 32 } }, func_actor_136300_80132854, { .value = 0 } };

_Actor136300CompanionAnimationBankExtensionStorage D_actor_136300_8013B140 = { .data = { { &_gActor136300Animation00F14, &_gActor136300Animation010EC, &_gActor136300Animation01508, &_gActor136300Animation016C4, &_gActor136300Animation018AC, &_gActor136300Animation01B68, &_gActor136300Animation01D48, &_gActor136300Animation021A0, &_gActor136300Animation027B8, &_gActor136300Animation03164, &_gActor136300Animation034AC, &_gActor136300Animation036E8, &_gActor136300Animation039F8, &_gActor136300Animation04074, &_gActor136300Animation04658 }, { { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_136300_8013B1CC = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B1E0 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B1F4 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B208 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B21C = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B230 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B244 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B258 = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B26C = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B280 = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B294 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

_Actor136300PlayerAnimationBankExtensionStorage D_actor_136300_8013B2A8 = { .data = { { &_gActor136300Animation0495C, &_gActor136300Animation04B60, &_gActor136300Animation04E40, &_gActor136300Animation05038, &_gActor136300Animation0538C, &_gActor136300Animation05584, &_gActor136300Animation05748, &_gActor136300Animation05A08, &_gActor136300Animation05BCC, &_gActor136300Animation05F84, &_gActor136300Animation062B0, &_gActor136300Animation06568, &_gActor136300Animation066F8, &_gActor136300Animation069B8, &_gActor136300Animation06D8C, &_gActor136300Animation0717C, &_gActor136300Animation074E8, &_gActor136300Animation07BE4, &_gActor136300Animation07E5C, &_gActor136300Animation082B0, &_gActor136300Animation08544, &_gActor136300Animation088AC, &_gActor136300Animation08A84, &_gActor136300Animation08D9C, &_gActor136300Animation08F98, &_gActor136300Animation092D4 }, { { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_136300_8013B338 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B34C = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B360 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B374 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B388 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B39C = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B3B0 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B3C4 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B3D8 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B3EC = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B400 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B414 = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B428 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B43C = { { .index = 1 }, 62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B450 = { { .index = 1 }, 63, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B464 = { { .index = 1 }, 64, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B478 = { { .index = 1 }, 65, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B48C = { { .index = 1 }, 66, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4A0 = { { .index = 1 }, 67, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4B4 = { { .index = 1 }, 68, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4C8 = { { .index = 1 }, 69, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4DC = { { .index = 1 }, 70, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4F0 = { { .index = 1 }, 71, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B504 = { { .index = 1 }, 72, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_actor_136300_8013B518 = { { .words = D_actor_136300_8013B2A8.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

AnimationBankCopyRequest D_actor_136300_8013B520 = { { .words = D_actor_136300_8013B140.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorTransform D_actor_136300_8013B528 = { { 2000, 0, 1940, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_136300_8013B540 = { { 2000, 0, 3940, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_136300_8013B558 = { { 2000, 0, 5240, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_136300_8013B570 = { { 2230, 0, 5240, 0 }, { 0, -1024, 0, 0 } };

GameActorMoveAnim D_actor_136300_8013B588 = { 19, 47 };

EvsCommand D_actor_136300_8013B590[149] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A7C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_136300_8013B518 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_136300_8013B520 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_136300_8013B2A8.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_136300_8013B140.data.playRequests }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132910 }, { .value = -2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136300_8013B528 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_136300_8013B540 } }, { .message = { .pointer = &D_actor_136300_8013B588 } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136300_8013B558 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B34C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B360 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B374 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B230 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3EC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B400 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B414 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B244 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B258 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B26C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B280 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B428 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B43C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B450 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A4C }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_801328E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_136300_8013B518 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_136300_8013B520 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136300_8013B540 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136300_8013B558 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4F0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B504 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B48C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B294 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1CC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1E0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B464 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B244 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B258 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B478 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B21C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3D8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_136300_801328D4 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136300_8013B570 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A7C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136300_8013C388[16] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_136300_801328D4 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136300_8013B570 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A7C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136300_8013C508[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A7C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136300_8013C5C8[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_136300_8013B518 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_136300_8013B520 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136300_80132998 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136300_801329EC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsSceneKey D_actor_136300_8013C6B8 = { 3, 65, 11 };

EvsCommand D_actor_136300_8013C6C0[8] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_801328E0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136300_8013C780[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132910 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

ScreenWaveOscillator gScreenWaveColumns[13] = { 0 };

ScreenWaveOscillator gScreenWaveRows[32] = { 0 };

ScreenWaveCtx D_actor_136300_8013C99C = { 0 };

#include "../../shared/screen_wave.inc.c"

/// State machine for the capture-event actor: arms the ending, waits for the
/// capture key, then hands control to the boot loader and spawns the drop-in
/// task. The two `func_800E8614` calls and the `arg0->state += 1` blocks are
/// written out in every arm that needs them; jump optimization merges the
/// identical tails, so one copy of the increment lands between case 3 and case
/// 6 and one copy of the call lands after case 0. Hoisting either tail into a
/// shared `goto` target compiles to a different allocation - the call's address
/// then reaches `$a0` through `$v0` instead of being built there directly.
void func_actor_136300_8013267C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gGameSession->hideHud = 1;
            Gp_MsgPlayerWeapon(0);
            func_800E8614(D_actor_136300_8013C5C8, 1);
            arg0->state += 1;
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                arg0->state += 1;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 2) {
                gGameSession->hideHud = 0;
                Gp_MsgPlayerWeapon(1);
                taskKill(arg0);
                return;
            }
            func_800E8614(D_actor_136300_8013C6C0, 1);
            arg0->state += 1;
            return;
        case 3:
            if (gGameSession->eventState != 1) {
                arg0->state += 1;
            }
            return;
        case 4:
        case 5:
            arg0->state += 1;
            return;
        case 6:
            SetDispMask(1);
            SndEvt_EnqueueType7(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_CHAPTER, 4);
            gameFlagSetNibble(GAME_FLAG_097, 0);
            gameFlagSetNibble(GAME_FLAG_098, 1);
            gameFlagSetNibble(GAME_FLAG_09A, 1);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 0xF);
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 4);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = 9;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_MINE_SHELTER;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_MINE_MESA;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
            Fs_BeginBootLoad((u8*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, 0);
            Gp_ClearCollectedBit(0x116);
            gDisplayState.spriteVariant = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            return;
    }
}

/// Runs once on spawn, then counts `spawnArg1` down; when it goes negative the
/// ending flag is set and the task kills itself. The decrement is one reused
/// local: m2c's temp plus per-arm subtract splits the value into three
/// quantities and the store lands in `$v1` instead of `$v0`.
void func_actor_136300_80132854(Task* arg0)
{
    s32 var_v0;

    if (arg0->state == 0) {
        Gp_SpawnScript18(D_80114A24, D_80114A34);
        arg0->state += 1;
    }
    var_v0 = arg0->spawnArg1.value;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1.value;
    }
    var_v0                = var_v0 - 1;
    arg0->spawnArg1.value = var_v0;
}

void func_actor_136300_801328D4(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}

void func_actor_136300_801328E0(s32 arg0)
{
    Task_SpawnFromTable(D_dryfield_night_garage_80183380, 0, arg0, 0);
}

/// Message handler driving the screen wave. A positive argument is stored in
/// `state`: `SCREEN_WAVE_RAMP_FALLING` counts the ramp back down, and
/// `SCREEN_WAVE_RAMP_FINISHED` ends the task. Otherwise `imageMdecMode` is set
/// to `MDEC_IMAGE_MODE_RGB16_MASK_BIT` and, except for the -2 message, the ramp
/// context is seeded (`span` 0x64 for 0, 5 otherwise, `scale` 0x100) and the
/// screen-wave task `D_actor_136300_80132AC4` is spawned with it.
///
/// Both halves of the context are written in *each* arm of the span test so
/// that each arm is a complete two-store address session: jump optimization
/// then merges the identical tails and the span collapses to one `li` per
/// arm, which is what puts the block's `lui` in the delay slot of the entry
/// test. Hoisting the scale store out of the arms compiles to a different
/// allocation.
void func_actor_136300_80132910(s32 arg0)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (arg0 <= 0) {
        queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        if (arg0 != -2) {
            if (arg0 == 0) {
                D_actor_136300_8013C99C.span  = 0x64;
                D_actor_136300_8013C99C.scale = 0x100;
            } else {
                D_actor_136300_8013C99C.span  = 5;
                D_actor_136300_8013C99C.scale = 0x100;
            }
            Task_SpawnFromTable(D_actor_136300_80132AC4, 0, 0, &D_actor_136300_8013C99C);
        }
    } else {
        D_actor_136300_8013C99C.state = arg0;
    }
}

void func_actor_136300_80132998(void)
{
    s32 temp_v0;

    temp_v0 = gameFlagGetNibble(GAME_FLAG_072);
    Gp_StartCapSlot((s16)(temp_v0 + 0x10), 0, 0);
    if (temp_v0 < 2) {
        gameFlagSetNibble(GAME_FLAG_072, temp_v0 + 1);
    }
}

void func_actor_136300_801329EC(void)
{
    AnimationPlayRequest* var_s0;

    if (Gp_GetCapEventKey() == 1) {
        var_s0 = &D_actor_136300_8013B208;
    } else {
        var_s0 = &D_actor_136300_8013B230;
    }
    Gp_AllyAnimId(&var_s0->source.index);
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, var_s0, 0);
}

void func_actor_136300_80132A4C(s32 arg0)
{
    Display_InitModeObj(&D_actor_136300_8013B134, arg0, 0, 0x100);
}

void func_actor_136300_80132A7C(s32 arg0)
{
    if (arg0 == 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x180, 0x100);
        return;
    }
    Gp_ResetCap();
}
