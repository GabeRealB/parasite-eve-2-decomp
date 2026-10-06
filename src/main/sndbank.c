#include "main/sound.h"

#include <psyq/sys/types.h>
#include <psyq/kernel.h>
#include <psyq/libapi.h>
#include <psyq/libspu.h>
#include <psyq/libetc.h>

#include "common.h"

#include "cdaudio.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/sound_types.h"
#include "sound_types.h"
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"

#include "actors/actor_800100.h"

#include "actors/actor_800200.h"

#include "actors/actor_800300.h"

#include "aya/aya.h"

#include "gameplay/captions.h"
#include "gameplay/model_objects.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"

#include "kyle/kyle.h"

#include "kyle/kyle_800102.h"

#include "rooms/acropolis_bridge.h"

#include "rooms/acropolis_cafeteria.h"

#include "rooms/acropolis_fountain.h"

#include "rooms/acropolis_helicopter_landing_pad.h"

#include "rooms/acropolis_security_room.h"

#include "weapons/grenade_pistol.h"

#include "weapons/hypervelocity.h"

#include "weapons/m4a1_grenade.h"

#include "weapons/tonfa_baton.h"

#include "weapons/weapon.h"

/// Header of one block in the sound heap.
///
/// The heap is one buffer of variable-length blocks chained in address order,
/// reserved and free alike. The caller's pointer addresses the payload
/// immediately after this header. A request is rounded up with the header so
/// every block stays 4-byte aligned. When the remainder cannot hold another
/// header, the reserved block keeps its existing length.
///
/// `magic` identifies a sound-heap block. Initialization stores 0xB25A on the
/// single initial block, which occupies the whole buffer. A block created by a
/// split stores 0xA52B and keeps that value if it is later reserved. Release
/// accepts either value and coalesces both the same way.
typedef struct _SndHeapBlockHeader {
    u32                         size;        // Byte length of this block, including this header
    u16                         isAllocated; // 0 free, 1 reserved
    u16                         magic;       // 0xB25A initial block, 0xA52B block created by a split
    struct _SndHeapBlockHeader* prev;        // Preceding block in address order, or NULL
    struct _SndHeapBlockHeader* next;        // Succeeding block in address order, or NULL
} _SndHeapBlockHeader;
STATIC_ASSERT_SIZEOF(_SndHeapBlockHeader, 0x10);

/// One registration in the sound driver's list of per-update polls.
///
/// The list hangs from a sentinel node, `AudioTick_List`, of which only `next`
/// is ever used: it addresses the first registration, and the sentinel's other
/// members stay cleared. Each registration is a block of the sound heap, linked
/// in ascending order of `id`, and no two in the list share an id.
typedef struct _AudioTickNode {
    AudioTickPoll          poll;     // Called once per audio update with `arg`, or NULL; returning -1 unlinks the node
    AudioTickOnRemove      onRemove; // Called once as the node is unlinked, or NULL
    u16                    id;       // Unique sort key: the list ascends by it, and a node is unlinked by matching it
    s32*                   arg;      // Registrant's pointer handed to `poll` unchanged, or NULL
    struct _AudioTickNode* prev;     // Preceding node, or the sentinel; maintained but never read
    struct _AudioTickNode* next;     // Following node, or NULL at the end of the list
} _AudioTickNode;
STATIC_ASSERT_SIZEOF(_AudioTickNode, 0x18);

// Normalized audio gain and the two directions selected during ramp setup.
enum {
    LINEAR_INTERPOLATOR_UNITY_GAIN = 65535,
    LINEAR_INTERPOLATOR_DECREASING = -1,
    LINEAR_INTERPOLATOR_INCREASING = 1
};

#define SNDHEAP_SIZE 0x3D00

#define SNDHEAP_START_MAGIC 0xB25A

#define SNDHEAP_MAGIC 0xA52B

/* Define BSS before API headers to preserve first-declaration order. */
static _SndHeapBlockHeader* SndHeap_Start;

/// Unreferenced.
static u8 D_8007A3A8[8];

static u8 SndHeap_Buffer[SNDHEAP_SIZE];

static _AudioTickNode AudioTick_List;

static u32 AudioTick_Enabled;

static u8 D_8007E0CC;

static volatile long D648E0_SpuTimerED;

void* Snd_SequenceBankBuffer;

SndBank Snd_Banks[16];

/// Unreferenced.
static u8 D_8007E2D8[8];

#include "sound.h"

static u8 D_800680A4;

static u8 D58028_SpuTimerEnabled;

/// Unreferenced.
static s32 D_800680A8;

static u32 D_800680BC;

static volatile u32 D_800680C0;

void func_807257A0(Task* arg0);

static void Spu_InitSystem(s32 arg0);

static void Snd_ClearBanks(void);

static void SndHeap_Reset(void);

static long Spu_TimerCallback(void);

static s32 Spu_TimerReentryWork(void);

static void AudioTick_Reset(void);

static void AudioTick_Process(void);

static _AudioTickNode* AudioTick_Remove(_AudioTickNode* arg0);

TaskDesc D_80067828[] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_807257A0 },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x70 } }, Gp_EffAttachTask37 },
    { { { TASK_BODY_NONE, 0x80 } }, func_800E70AC },
    { { { TASK_BODY_NONE, 0xC0 } }, func_acropolis_bridge_8017F788 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_acropolis_security_room_8017ED68 },
    { { { TASK_BODY_NONE, 0xC0 } }, func_acropolis_security_room_80180294 },
    { { { TASK_BODY_NONE, 0xC0 } }, padScriptBinaryMotorHoldTask },
    { { { TASK_BODY_NONE, 0xC0 } }, padScriptVariableMotorRampTask },
    { { { TASK_BODY_NONE, 0xC0 } }, Gp_Script18Task },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisFountainClimbTask },
    { { { TASK_BODY_NONE, 0xC0 } }, func_acropolis_helicopter_landing_pad_8017EF8C },
    { { { TASK_BODY_TMD, 0x60 } }, Gp_EffAttachTask37 },
};

TaskDesc D_800678F4[] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, Gp_PlayerWorkTask, { &D_aya_10400_8011B078 } },
    { { { TASK_BODY_TMD, 0x50 } }, Gp_PlayerWorkTask, { &D_aya_10300_8011ACE8 } },
    { { { TASK_BODY_TMD, 0x50 } }, Gp_PlayerWorkTask, { &D_aya_10200_8011C210 } },
    { { { TASK_BODY_TMD, 0x50 } }, Gp_PlayerWorkTask, { &D_aya_10500_8011C2E4 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10400_8011B4CC } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10400_8011B9BC } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10400_8011B4CC } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10400_8011BE10 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10300_8011B128 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10300_8011BA58 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10300_8011B568 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10300_8011BE98 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10200_80115B90 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10200_801164C0 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10200_80115FD0 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10200_80116900 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10500_80115B90 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10500_801164C0 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10500_80115FD0 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_aya_10500_80116900 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_p08_8011D924 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m93r_8011DA74 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m950_8011DA9C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_p08_8011D924 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_p229_8011E5E4 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_unused_85_8011D53C } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_mongoose_8011D934 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_grenade_pistol_8011E28C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_mm1_8011E494 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_pa3_8011DA04 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_sp12_8011DB44 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_as12_8011DCF0 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m4a1_8011DEC4 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m249_8011DE74 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, tonfaBatonModelTask, { &D_tonfa_baton_8011E460 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m4a1_8011DEC4 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m4a1_8011DEC4 } },
    { { { TASK_BODY_TMD, 0x50 } }, func_hypervelocity_8011F6C0, { &D_hypervelocity_801202F8 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_gunblade_8011EEB0 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m4a1_hammer_8011F778 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m4a1_bayonet_8011E9FC } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m4a1_grenade_8011EA2C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m4a1_pyke_8011F56C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_m4a1_javelin_8012071C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_mp5a5_8011EAFC } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_mp5a5_8011EAFC } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_mp5a5_8011EAFC } },
    { { { TASK_BODY_TMD, 0x52 } }, func_m4a1_grenade_8011DE68, { &D_m4a1_grenade_8012E1FC } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x52 } }, func_grenade_pistol_8011DBD0, { &D_grenade_pistol_8012B5A4 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x52 } }, func_mm1_8011DBD0, { &D_mm1_8012D444 } },
    { { { TASK_BODY_TMD, 0x52 } }, func_kyle_800102_801682B4, { &D_kyle_800102_801775A8 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, tonfaBatonModelTask, { &D_tonfa_baton_8011E5EC } },
    { { { TASK_BODY_TMD, 0x50 } }, func_hypervelocity_8011F6C0, { &D_hypervelocity_801205E4 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, func_hypervelocity_8011F6C0, { &D_hypervelocity_80120860 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, func_actor_800100_80163CF0, { &D_kyle_800101_8016C594 } },
    { { { TASK_BODY_TMD, 0x50 } }, func_actor_800100_80163CF0, { &D_kyle_800102_8016CE38 } },
    { { { TASK_BODY_TMD, 0x50 } }, func_actor_800100_80163CF0, { &D_kyle_800103_8016C594 } },
    { { { TASK_BODY_TMD, 0x50 } }, func_actor_800100_80163CF0, { &D_kyle_800104_8016C594 } },
    { { { TASK_BODY_TMD, 0x50 } }, func_actor_800200_801626EC, { &gActor800200FlintBody } },
    { { { TASK_BODY_TMD, 0x50 } }, func_actor_800300_801625F4, { &gActor800300Model02CF4 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800101_8016C9E8 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800101_8016D32C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800101_8016CED8 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800101_8016D81C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800102_8016D28C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800102_8016DBD0 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800102_8016D77C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800102_8016E0C0 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800103_8016C9E8 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800103_8016D32C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800103_8016CED8 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800103_8016D81C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800104_8016C9E8 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800104_8016D32C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800104_8016CED8 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800104_8016D81C } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800101_8016DC60 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800102_8016E93C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800103_8016DE3C } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &D_kyle_800104_8016E568 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, acropolisCafeteriaLoosePropTask, { &gAcropolisCafeteriaModel07CA8 } },
    { { { TASK_BODY_TMD, 0x50 } }, acropolisCafeteriaLoosePropTask, { &gAcropolisCafeteriaModel08638 } },
    { { { TASK_BODY_TMD, 0x50 } }, acropolisCafeteriaLoosePropTask, { &gAcropolisCafeteriaModel09090 } },
    { { { TASK_BODY_TMD, 0x50 } }, acropolisCafeteriaLoosePropTask, { &gAcropolisCafeteriaModel09A20 } },
};

static u8 D_800680A4             = 0;
static u8 D58028_SpuTimerEnabled = 0;
/// Unreferenced.
static s32          D_800680A8            = 0;
s8                  Snd_BankSlotsByType[] = { 0, 1, 2, 3, 4, 7, 0xC, 0xD, -1, -1, -1, -1, -1, -1, 8, 0xA };
static u32          D_800680BC            = 0;
static volatile u32 D_800680C0            = 0;

static inline void Spu_InitSystemLocked(s32 arg0)
{
    s32* temp_v0;

    switch (arg0) {
        case 0:
            SpuInit();
            D58028_SpuTimerEnabled = false;
            D_800680BC             = 0;
            Spu_ResetCommonAttr();
            break;
        case 1:
            SpuIsTransferCompleted(1);
            D_800680BC = 0;
            Spu_ResetCommonAttr();
            break;
        case 2:
            break;
        default:
            return;
    }

    SndHeap_Reset();
    sndEvtReset();
    AsyncCb_Reset();
    Spu_ConfigReverb(3);
    Spu_InitVoices();
    Snd_ClearBanks();
    AudioTick_Reset();
    Snd_RegisterTickCallbacks();
    Snd_InitBanks(0);
    Midi_InitSystem(0);

    temp_v0  = SndHeap_Malloc(4);
    *temp_v0 = 0;

    AudioTick_Insert(&Snd_ReverbWarmupCb, NULL, 0x8801, temp_v0);
    if (D58028_SpuTimerEnabled) {
        DisableEvent(D648E0_SpuTimerED);
        CloseEvent(D648E0_SpuTimerED);
        StopRCnt(RCntCNT0);
        D58028_SpuTimerEnabled = false;
    }

    if (gDisplayState.region == MODE_PAL) {
        D_800680A4 = 0;
        D_8007E0CC = 0;
        SetRCnt(RCntCNT0, 0xffff, RCntMdINTR | RCntMdSC);
        ResetRCnt(RCntCNT0);
        StartRCnt(RCntCNT0);
        EnterCriticalSection();
        D648E0_SpuTimerED = OpenEvent(RCntCNT0, EvSpINT, EvMdINTR, Spu_TimerCallback);
        ExitCriticalSection();
        EnableEvent(D648E0_SpuTimerED);
        D58028_SpuTimerEnabled = true;
    }
    D_800680A4 = 0;
    D_8007E0CC = 0;
}

/// Runs the initialisation with the audio frame work held off (`D_800680C0`
/// is the flag `Audio_IrqFrameWork` tests).
static void Spu_InitSystem(s32 arg0)
{
    D_800680C0 = 0;
    Spu_InitSystemLocked(arg0);
    D_800680C0 = 1;
}

SndBank* Snd_AllocBank(SndBankPayload* payload)
{
    // These bank types reserve a minimum table block, in bytes, even for smaller loads.
    enum {
        SOUND_BANK_TYPE_2_MIN_TABLE_BYTES  = 0xCE,
        SOUND_BANK_TYPE_14_MIN_TABLE_BYTES = 0x78
    };
    SndBank* bank;
    s32      size;
    u8*      heap;
    u16      type  = payload->bankId & SOUND_BANK_TYPE_MASK;
    s32      entry = Snd_BankSlotsByType[type >> 12];
    s8       slot  = entry;

    if (entry == -1) {
        return NULL;
    }

    if (type == 0x4000) {
        slot = D_80082122 + 4;
    }

    if (type == SOUND_BANK_TYPE_SEQUENCE && Snd_SequenceBankBuffer != 0) {
        bank            = &Snd_Banks[slot];
        bank->heapBlock = Snd_SequenceBankBuffer;
    } else {
        bank = &Snd_Banks[slot];
        Snd_FreeBank(bank);

        size = (payload->layerCount * (s32)(sizeof(*bank->layers) / sizeof(u32)) + payload->groupCount * (s32)(sizeof(*bank->groups) / sizeof(u32))) * (s32)sizeof(u32) + payload->groupCount * (s32)sizeof(*bank->groupFirstLayer);

        switch (payload->bankId & SOUND_BANK_TYPE_MASK) {
            case 0x2000:
                if (size < SOUND_BANK_TYPE_2_MIN_TABLE_BYTES + 1) {
                    size = SOUND_BANK_TYPE_2_MIN_TABLE_BYTES;
                }
                break;
            case 0xE000:
                if (size < SOUND_BANK_TYPE_14_MIN_TABLE_BYTES + 1) {
                    size = SOUND_BANK_TYPE_14_MIN_TABLE_BYTES;
                }
                break;
            case SOUND_BANK_TYPE_SEQUENCE:
                if (size < SOUND_BANK_SEQUENCE_TABLE_BYTES + 1) {
                    size = SOUND_BANK_SEQUENCE_TABLE_BYTES;
                }
                break;
        }

        bank->heapBlock = SndHeap_Malloc(size);
        if (bank->heapBlock == NULL) {
            return NULL;
        }
    }

    heap                  = bank->heapBlock;
    bank->groups          = bank->heapBlock;
    heap                 += payload->groupCount * (s32)sizeof(*bank->groups);
    bank->layers          = (SndBankLayer*)heap;
    bank->groupFirstLayer = (u16*)(heap + payload->layerCount * (s32)sizeof(*bank->layers));
    return bank;
}

void Spu_Init(void)
{
    Spu_InitSystem(0);
}

void Spu_WaitDma(void)
{
    Spu_InitSystem(1);
}

void Audio_IrqFrameWork(void)
{
    if (D_800680C0 != 0) {
        D_800680C0 = 0;
        Spu_TickVoices();
        SndEvt_Process();
        AudioTick_Process();
        Spu_FlushVoiceUpdates();
        D_800680BC += 1;
        if (gDisplayState.region == MODE_PAL) {
            D_8007E0CC = 6;
            ResetRCnt(RCntCNT0);
            D_800680A4 = 1;
        }
        D_800680C0 = 1;
    }
}

static void Snd_ClearBanks(void)
{
    s32      i;
    s32*     p;
    SndBank* ptr;
    u16      flag;

    p = (s32*)Snd_Banks;
    i = 0;
    do {
        *p = 0;
        i++;
        p++;
    } while ((u32)i < sizeof(Snd_Banks) / sizeof(*p));

    flag = SOUND_BANK_ID_FREE;
    i    = ARRAY_SIZE(Snd_Banks) - 1;
    ptr  = Snd_Banks;
    ptr += ARRAY_SIZE(Snd_Banks) - 1;
    do {
        ptr->bankId = flag;
        i--;
        ptr--;
    } while (i >= 0);

    Snd_SequenceBankBuffer = 0;
}

void Snd_FreeBank(SndBank* bank)
{
    if ((bank != NULL) && ((bank->bankId & SOUND_BANK_TYPE_MASK) != SOUND_BANK_TYPE_SEQUENCE)) {
        SndHeap_Free(bank->heapBlock);
        bank->heapBlock       = NULL;
        bank->groups          = NULL;
        bank->layers          = NULL;
        bank->groupFirstLayer = NULL;
        bank->bankId          = SOUND_BANK_ID_FREE;
        bank->waveBytes       = 0;
    }
}

SndBank* Snd_FindBank(u16 bankId)
{
    s32      i;
    SndBank* ptr;
    s32      id;

    if (bankId == SOUND_BANK_ID_FREE) {
        bankId = 0;
    }
    id = bankId;

    for (i = 0, ptr = Snd_Banks; i < ARRAY_SIZE(Snd_Banks); i++, ptr++) {
        if (ptr->bankId == id) {
            return ptr;
        }
    }
    return NULL;
}

void Snd_BuildGroupIndex(SndBank* bank)
{
    u16*          table;
    SndBankGroup* group;
    s32           i;
    u8            count;

    table = bank->groupFirstLayer;
    if (table != NULL) {
        group  = bank->groups;
        *table = 0;
        count  = bank->groupCount;
        table++;
        i = count - 1;
        if (i > 0) {
            i = count - 2;
            if (i != -1) {
                do {
                    *table = table[-1] + group->layerCount;
                    group++;
                    i--;
                    table++;
                } while (i != -1);
            }
        }
    }
}

void LinInterp_Setup(LinInterp* ramp, s32 arg1, s32 arg2, s32 arg3)
{
    s32 temp;
    s32 limit;

    arg1 &= 0xFF;
    arg2 &= 0xFF;

    if (arg1 == arg2 || arg3 == 0) {
        ramp->step       = 0;
        ramp->targetGain = 0;
        ramp->gain       = 0;
        ramp->enabled    = LINEAR_INTERPOLATOR_BYPASS;
        return;
    }

    limit      = LINEAR_INTERPOLATOR_UNITY_GAIN;
    ramp->step = limit / arg3;
    temp       = arg2 - arg1;
    if (temp < 0) {
        ramp->direction  = LINEAR_INTERPOLATOR_DECREASING;
        ramp->gain       = limit;
        ramp->targetGain = 0;
    } else {
        ramp->direction  = LINEAR_INTERPOLATOR_INCREASING;
        ramp->gain       = 0;
        ramp->targetGain = limit;
    }
    ramp->enabled = LINEAR_INTERPOLATOR_SCALE;
}

s32 LinInterp_Apply(LinInterp* ramp, s32 arg1)
{
    s32 var_a1;

    var_a1 = arg1;
    if (ramp->enabled == LINEAR_INTERPOLATOR_SCALE) {
        if (ramp->gain == ramp->targetGain) {
            ramp->step = 0;
        }
        var_a1 = (s32)((var_a1 * ramp->gain) / LINEAR_INTERPOLATOR_UNITY_GAIN);
    }
    return var_a1;
}

void LinInterp_Step(LinInterp* ramp)
{
    s32 step = ramp->step;

    if (step) {
        if (ramp->direction < 0) {
            if (ramp->targetGain + step >= ramp->gain) {
                ramp->gain = ramp->targetGain;
            } else {
                ramp->gain = ramp->gain - step;
            }
        } else {
            ramp->gain = ramp->gain + step;
            if (ramp->gain >= ramp->targetGain) {
                ramp->gain = ramp->targetGain;
            }
        }
    }
}

void Spu_ApplyPanVolume(s16* arg0, s16 arg1, s32 arg2)
{
    s16 index;
    u32 left;
    u32 right;

    if (arg1 > 0) {
        index = arg1 - 1;
        if (arg1 >= 0x80) {
            index = 0x7E;
        }
    } else {
        index = 0;
    }

    left  = (u32)(arg2 * Snd_PanGainTable[index]) >> 0xC;
    right = (u32)(arg2 * Snd_PanGainTable[0x7E - index]) >> 0xC;

    if (!sndOutputIsStereo()) {
        right = (u32)((left + right) * Snd_PanGainTable[0x3F]) >> 0xC;
        left  = right;
    }

    if (left < 0x4000U) {
        arg0[0] = (s16)left;
    } else {
        arg0[0] = 0x3FFF;
    }

    if (right < 0x4000U) {
        arg0[1] = (s16)right;
    } else {
        arg0[1] = 0x3FFF;
    }
}

s32 AudioTick_Insert(AudioTickPoll poll, AudioTickOnRemove onRemove, u16 id, s32* arg)
{
    _AudioTickNode* node;
    _AudioTickNode* head;
    _AudioTickNode* p;
    _AudioTickNode* next;
    u16             id16;
    u16             key;

    id16 = id;
    key  = id16;
    head = &AudioTick_List;
    if (head == NULL) {
        return -1;
    }
    AudioTick_Enabled = 0;
    node              = SndHeap_Malloc(sizeof(_AudioTickNode));
    if (node == NULL) {
        AudioTick_Enabled = 1;
        return -1;
    }
    node->poll     = poll;
    node->onRemove = onRemove;
    node->id       = id16;
    node->arg      = arg;
    node->prev     = NULL;
    node->next     = NULL;

    p = head;
    for (;;) {
        next = p->next;
        if (next == NULL) {
            p->next           = node;
            node->prev        = p;
            node->next        = NULL;
            AudioTick_Enabled = 1;
            return 0;
        }
        if (next->id == key) {
            SndHeap_Free(node);
            AudioTick_Enabled = 1;
            return -2;
        }
        if (key < next->id) {
            node->next        = next;
            AudioTick_Enabled = 1;
            p->next->prev     = node;
            p->next           = node;
            node->prev        = p;
            return 0;
        }
        p = next;
    }
}

static void SndHeap_Reset(void)
{
    SndHeap_Start              = (_SndHeapBlockHeader*)SndHeap_Buffer;
    SndHeap_Start->size        = SNDHEAP_SIZE;
    SndHeap_Start->magic       = SNDHEAP_START_MAGIC;
    SndHeap_Start->isAllocated = false;
    SndHeap_Start->prev        = NULL;
    SndHeap_Start->next        = NULL;
}

void* SndHeap_Malloc(size_t size)
{
    // First fit over the address-ordered chain. Split a free block when the
    // remainder can hold another header; otherwise reserve the whole block and
    // leave its length unchanged. The returned pointer is the payload.
    size_t               maxBlockSize;
    size_t               newBlockSize;
    size_t               allocSize;
    _SndHeapBlockHeader* block;
    _SndHeapBlockHeader* newBlock;

    maxBlockSize = 0;

    // Header plus payload, rounded up to the block alignment.
    allocSize = (size + sizeof(_SndHeapBlockHeader) + 3) & ~3;

    for (block = SndHeap_Start; block != NULL; block = block->next) {
        // A header outside the buffer is not a sound-heap block.
        if (block < (_SndHeapBlockHeader*)SndHeap_Buffer ||
            (_SndHeapBlockHeader*)&SndHeap_Buffer[SNDHEAP_SIZE] < block) {
            return NULL;
        }

        if (block->isAllocated) {
            continue;
        }

        // Retained search step. The running maximum is not read again.
        if (maxBlockSize < block->size) {
            maxBlockSize = block->size;
        }

        if (block->size >= allocSize) {
            newBlockSize = block->size - allocSize;
            newBlock     = (_SndHeapBlockHeader*)((u8*)block + allocSize);

            // Insert the remainder immediately after this block.
            if (sizeof(_SndHeapBlockHeader) < newBlockSize) {
                newBlock->size        = newBlockSize;
                newBlock->magic       = SNDHEAP_MAGIC;
                newBlock->isAllocated = false;

                if (block->next == NULL) {
                    newBlock->next = NULL;
                } else {
                    block->next->prev = newBlock;
                    newBlock->next    = block->next;
                }
                block->next        = newBlock;
                newBlock->prev     = block;
                block->size        = allocSize;
                block->isAllocated = true;
            } else {
                block->isAllocated = true;
            }

            // The allocated data is located just after the header.
            return (u8*)(block + 1);
        }
    }

    return NULL;
}

void SndHeap_Free(void* ptr)
{
    // Release a payload pointer, or return on NULL. Coalesce with free
    // neighbors so the chain stays in address order.
    uintptr              heapStart;
    uintptr              heapEnd;
    _SndHeapBlockHeader* header;

    if (ptr == NULL) {
        return;
    }

    // Compare numeric addresses because an invalid input may point outside
    // this allocation; relational C pointer comparisons would not be defined.
    // Keep the original inclusive upper-bound test. A valid input is still
    // required to be a payload returned by SndHeap_Malloc.
    heapStart = (uintptr)SndHeap_Buffer;
    if ((uintptr)ptr < heapStart) {
        return;
    }

    heapEnd = heapStart + SNDHEAP_SIZE;
    if (heapEnd < (uintptr)ptr) {
        return;
    }

    // The payload starts immediately after its header. The reserved flag is
    // cleared before the recognizer is tested; any other value returns without
    // coalescing, so the flag stays clear. Both sound-heap recognizers take
    // the same merge path.
    header              = (_SndHeapBlockHeader*)ptr - 1;
    header->isAllocated = false;
    if (header->magic != SNDHEAP_MAGIC && header->magic != SNDHEAP_START_MAGIC) {
        return;
    }

    // A free predecessor absorbs this block and becomes the block to merge forward.
    if (header->prev != NULL && header->prev->isAllocated == false) {
        if (header->next != NULL) {
            header->next->prev = header->prev;
            header->prev       = header->prev; // Retained; the value does not change.
        }
        header->prev->next  = header->next;
        header->prev->size += header->size;
        header              = header->prev;
    }

    // A free successor is absorbed the same way.
    if (header->next != NULL && header->next->isAllocated == false) {
        if (header->next->next != NULL) {
            header->next->next->prev = header;
        }
        header->size += header->next->size;
        header->next  = header->next->next;
    }

    // The surviving block is already free. This store is part of the release.
    header->isAllocated = false;
}

static long Spu_TimerCallback(void)
{
    if (D_800680A4 != 0) {
        D_8007E0CC--;
        if (D_8007E0CC == 0) {
            D_800680A4 = 0;
            Spu_TimerReentryWork();
        }
    }
    return 0;
}

static s32 Spu_TimerReentryWork(void)
{
    if (D_800680C0 == 0) {
        return 0;
    }
    D_800680C0 = 0;
    Spu_TickVoices();
    AudioTick_Process();
    Spu_FlushVoiceUpdates();
    D_800680C0  = 1;
    D_800680BC += 1;
    return 0;
}

static void AudioTick_Reset(void)
{
    AudioTick_List.poll     = NULL;
    AudioTick_List.onRemove = NULL;
    AudioTick_List.id       = 0;
    AudioTick_List.arg      = NULL;
    AudioTick_List.next     = NULL;
    AudioTick_List.prev     = NULL;
    AudioTick_Enabled       = 1;
}

static void AudioTick_Process(void)
{
    _AudioTickNode* head;
    _AudioTickNode* node;
    AudioTickPoll   poll;

    head = &AudioTick_List;
    if (AudioTick_Enabled != 0) {
        if (head != NULL) {
            node = head->next;
            while (1) {
                if (node == NULL) {
                    break;
                }
                poll = node->poll;
                if (poll != NULL) {
                    if (poll(node->arg) == -1) {
                        node = AudioTick_Remove(node);
                        continue;
                    }
                }
                node = node->next;
            }
        }
    }
}

static _AudioTickNode* AudioTick_Remove(_AudioTickNode* arg0)
{
    AudioTickOnRemove onRemove;
    _AudioTickNode*   head;
    _AudioTickNode*   prev;
    _AudioTickNode*   curr;

    head              = &AudioTick_List;
    onRemove          = arg0->onRemove;
    AudioTick_Enabled = 0;
    if (onRemove != NULL) {
        onRemove();
    }

    prev = head;
    if (prev->next != NULL) {
        do {
            curr = prev->next;
            if (curr->id == arg0->id) {
                prev->next = arg0->next;
                if (arg0->next != NULL) {
                    arg0->next->prev = prev;
                }
                AudioTick_Enabled = 1;
                return prev->next;
            }
            prev = curr;
        } while (prev->next != NULL);
    }
    AudioTick_Enabled = 1;
    return NULL;
}
