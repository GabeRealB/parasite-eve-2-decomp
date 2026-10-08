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

// Initialization lead-ins; every accepted mode then rebuilds playback state.
enum {
    SPU_INIT_HARDWARE      = 0,
    SPU_INIT_WAIT_TRANSFER = 1,
    SPU_INIT_PLAYBACK_ONLY = 2
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

static void _spuInitSystem(s32 initMode);

static void _sndBankResetDescriptors(void);

static void _sndHeapReset(void);

static long _spuTimerCallback(void);

static s32 _spuRunTimerAudioUpdate(void);

static void _audioTickReset(void);

static void _audioTickProcess(void);

static _AudioTickNode* _audioTickRemove(_AudioTickNode* node);

// A poll's completion result; every other result keeps its registration.
enum { AUDIO_TICK_POLL_FINISHED = -1 };

/// Splits a free sound-heap block into a reserved block and a free remainder.
///
/// `block` must be a live free block, and `remainder` must start `blockBytes`
/// bytes after it. `blockBytes` includes its header, is 4-byte aligned and is
/// at least one header; the bytes left in the original block must exceed one
/// header. Heap operations must be serialized.
/// The address-ordered chain is preserved, including the original block's
/// predecessor and magic. Neither resulting payload is initialized.
static inline void _sndHeapSplitBlock(_SndHeapBlockHeader* block, _SndHeapBlockHeader* remainder, size_t blockBytes)
{
    remainder->size        = block->size - blockBytes;
    remainder->magic       = SNDHEAP_MAGIC;
    remainder->isAllocated = false;
    if (block->next == NULL) {
        remainder->next = NULL;
    } else {
        block->next->prev = remainder;
        remainder->next   = block->next;
    }
    block->next        = remainder;
    remainder->prev    = block;
    block->size        = blockBytes;
    block->isAllocated = true;
}

/// Unlinks a poll registration without invoking handlers or releasing storage.
///
/// `previous` must immediately precede the live `node`; for the first
/// registration it is the list sentinel. The caller must serialize list edits
/// and disable polling. Only the neighboring links change: `node` retains its
/// links, handlers and borrowed argument pointer after it leaves the list.
static inline void _audioTickUnlinkNode(_AudioTickNode* previous, const _AudioTickNode* node)
{
    previous->next = node->next;
    if (node->next != NULL) {
        node->next->prev = previous;
    }
}

TaskDesc D_80067828[] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_807257A0 },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x70 } }, effectBurstModelPartTask },
    { { { TASK_BODY_NONE, 0x80 } }, func_800E70AC },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisBridgeKeypadTask },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisSecurityRoomMonitorTask },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisSecurityRoomPowerSupplyTask },
    { { { TASK_BODY_NONE, 0xC0 } }, padScriptBinaryMotorHoldTask },
    { { { TASK_BODY_NONE, 0xC0 } }, padScriptVariableMotorRampTask },
    { { { TASK_BODY_NONE, 0xC0 } }, padScriptTask },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisFountainClimbTask },
    { { { TASK_BODY_NONE, 0xC0 } }, func_acropolis_helicopter_landing_pad_8017EF8C },
    { { { TASK_BODY_TMD, 0x60 } }, effectBurstModelPartTask },
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
    { { { TASK_BODY_TMD, 0x50 } }, hypervelocityWeaponModelTask, { &D_hypervelocity_801202F8 } },
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
    { { { TASK_BODY_TMD, 0x52 } }, m4a1GrenadeShellTask, { &D_m4a1_grenade_8012E1FC } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x52 } }, grenadePistolShellTask, { &D_grenade_pistol_8012B5A4 } },
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
    { { { TASK_BODY_TMD, 0x50 } }, hypervelocityWeaponModelTask, { &D_hypervelocity_801205E4 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, hypervelocityWeaponModelTask, { &D_hypervelocity_80120860 } },
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
    { { { TASK_BODY_TMD, 0x50 } }, actor800100Task, { &D_kyle_800101_8016C594 } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800100Task, { &D_kyle_800102_8016CE38 } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800100Task, { &D_kyle_800103_8016C594 } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800100Task, { &D_kyle_800104_8016C594 } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800200Task, { &gActor800200FlintBody } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800300Task, { &gActor800300Model02CF4 } },
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

/// Rebuilds sound playback ownership, silent voices and boot bank reservations.
///
/// Call with audio updates held off and playback, loading and asynchronous job
/// producers quiescent. Previous sound-heap pointers, bank bindings, voice
/// references, event reservations and job handles expire without notification.
/// Separately owned images must already have been released. Resets reverb to
/// studio B with zero depth, queues all 24 voices on silence, and restores the
/// MIDI, shared and script voice ranges. Script and sequence reservations require
/// sufficient sound-heap storage; allocation failures are unchecked.
/// The caller installs the reverb warmup poll; voice changes apply at the next
/// SPU flush.
static inline void _spuResetPlaybackSystems(void)
{
    // Discard old ownership before creating new sound-heap allocations.
    _sndHeapReset();
    sndEvtReset();
    asyncCbReset();
    spuInitReverb(SPU_REV_MODE_STUDIO_B);
    spuInitVoices();
    _sndBankResetDescriptors();

    // Rebuild the poll list before reserving script and sequence banks.
    _audioTickReset();
    audioTickInitPlayback();
    sndScriptInitSystem(0);
    midiInitSystem(0);
}

/// Reinitializes sound-driver state while the caller holds off audio updates.
///
/// Mode 0 initializes SPU hardware; mode 1 waits for its DMA and resets common
/// output; mode 2 rebuilds playback state only. Other modes do nothing. Accepted
/// modes reset the sound heap, events, callbacks, voices, banks, scripts and MIDI,
/// invalidating their previous ownership. Installs a zeroed, heap-owned reverb
/// warmup counter; allocation must succeed. Replaces the optional PAL root-counter
/// interrupt and clears its extra-update arm/count. This routine does not acquire
/// a lock: its wrapper holds the audio-work gate closed for the entire call.
static inline void _spuInitSystemState(s32 initMode)
{
    enum {
        SPU_PAL_TIMER_PERIOD = 0xFFFF
    };
    s32* reverbWarmupUpdates;

    switch (initMode) {
        case SPU_INIT_HARDWARE:
            SpuInit();
            D58028_SpuTimerEnabled = false;
            D_800680BC             = 0;
            spuResetCommonOutput();
            break;
        case SPU_INIT_WAIT_TRANSFER:
            SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
            D_800680BC = 0;
            spuResetCommonOutput();
            break;
        case SPU_INIT_PLAYBACK_ONLY:
            break;
        default:
            return;
    }

    _spuResetPlaybackSystems();

    // The poll owns this counter until it finishes; storage lasts until heap reset.
    reverbWarmupUpdates  = sndHeapAlloc(sizeof(*reverbWarmupUpdates));
    *reverbWarmupUpdates = 0;

    audioTickInsert(&spuTickReverbWarmup, NULL, AUDIO_TICK_ID_REVERB_WARMUP, reverbWarmupUpdates);
    // Replace any previous PAL extra-update timer before enabling a new one.
    if (D58028_SpuTimerEnabled) {
        DisableEvent(D648E0_SpuTimerED);
        CloseEvent(D648E0_SpuTimerED);
        StopRCnt(RCntCNT0);
        D58028_SpuTimerEnabled = false;
    }

    if (gDisplayState.region == MODE_PAL) {
        D_800680A4 = 0;
        D_8007E0CC = 0;
        SetRCnt(RCntCNT0, SPU_PAL_TIMER_PERIOD, RCntMdINTR | RCntMdSC);
        ResetRCnt(RCntCNT0);
        StartRCnt(RCntCNT0);
        EnterCriticalSection();
        D648E0_SpuTimerED = OpenEvent(RCntCNT0, EvSpINT, EvMdINTR, _spuTimerCallback);
        ExitCriticalSection();
        EnableEvent(D648E0_SpuTimerED);
        D58028_SpuTimerEnabled = true;
    }
    D_800680A4 = 0;
    D_8007E0CC = 0;
}

/// Initializes the sound system while suppressing vertical-blank and PAL timer audio work.
///
/// `initMode` selects hardware initialization (SPU_INIT_HARDWARE), a completed
/// DMA transfer and common-output reset (SPU_INIT_WAIT_TRANSFER), or playback
/// rebuilding alone (SPU_INIT_PLAYBACK_ONLY). Other values leave system state
/// intact and still release the audio-work gate. Accepted modes invalidate old
/// playback ownership and install reverb warmup and optional PAL timer work.
/// Callers must serialize initialization and stop playback/loading users first.
/// The gate is zero while held and one when available; it suppresses new updates
/// without waiting for an update already in progress.
static void _spuInitSystem(s32 initMode)
{
    enum {
        SPU_AUDIO_WORK_HELD      = 0,
        SPU_AUDIO_WORK_AVAILABLE = 1
    };

    D_800680C0 = SPU_AUDIO_WORK_HELD;
    _spuInitSystemState(initMode);
    D_800680C0 = SPU_AUDIO_WORK_AVAILABLE;
}

SndBank* sndBankAllocTables(const SndBankPayload* payload)
{
    // These bank types reserve a minimum table block, in bytes, even for smaller loads.
    enum {
        SOUND_BANK_TYPE_2_MIN_TABLE_BYTES  = 0xCE,
        SOUND_BANK_TYPE_14_MIN_TABLE_BYTES = 0x78,
        SOUND_BANK_TYPE_2                  = 0x2000,
        SOUND_BANK_TYPE_4                  = 0x4000,
        SOUND_BANK_TYPE_14                 = 0xE000,
        SOUND_BANK_TYPE_4_FIRST_SLOT       = 4,
        SOUND_BANK_SLOT_UNSUPPORTED        = -1
    };
    SndBank* bank;
    s32      tableBytes;
    u8*      tableCursor;
    u16      bankType       = payload->bankId & SOUND_BANK_TYPE_MASK;
    s32      mappedSlot     = Snd_BankSlotsByType[bankType >> 12];
    s8       descriptorSlot = mappedSlot;

    if (mappedSlot == SOUND_BANK_SLOT_UNSUPPORTED) {
        return NULL;
    }

    if (bankType == SOUND_BANK_TYPE_4) {
        descriptorSlot = D_80082122 + SOUND_BANK_TYPE_4_FIRST_SLOT;
    }

    // Sequence reloads retain their table block; other slots release theirs first.
    if (bankType == SOUND_BANK_TYPE_SEQUENCE && Snd_SequenceBankBuffer != NULL) {
        bank            = &Snd_Banks[descriptorSlot];
        bank->heapBlock = Snd_SequenceBankBuffer;
    } else {
        bank = &Snd_Banks[descriptorSlot];
        sndBankFree(bank);

        tableBytes = payload->groupCount * (s32)sizeof(*bank->groups) + payload->layerCount * (s32)sizeof(*bank->layers) + payload->groupCount * (s32)sizeof(*bank->groupFirstLayer);

        switch (payload->bankId & SOUND_BANK_TYPE_MASK) {
            case SOUND_BANK_TYPE_2:
                if (tableBytes < SOUND_BANK_TYPE_2_MIN_TABLE_BYTES + 1) {
                    tableBytes = SOUND_BANK_TYPE_2_MIN_TABLE_BYTES;
                }
                break;
            case SOUND_BANK_TYPE_14:
                if (tableBytes < SOUND_BANK_TYPE_14_MIN_TABLE_BYTES + 1) {
                    tableBytes = SOUND_BANK_TYPE_14_MIN_TABLE_BYTES;
                }
                break;
            case SOUND_BANK_TYPE_SEQUENCE:
                if (tableBytes < SOUND_BANK_SEQUENCE_TABLE_BYTES + 1) {
                    tableBytes = SOUND_BANK_SEQUENCE_TABLE_BYTES;
                }
                break;
        }

        bank->heapBlock = sndHeapAlloc(tableBytes);
        if (bank->heapBlock == NULL) {
            return NULL;
        }
    }

    // Tables share one allocation: programs, sample layers, then element indices.
    tableCursor           = bank->heapBlock;
    bank->groups          = bank->heapBlock;
    tableCursor          += payload->groupCount * (s32)sizeof(*bank->groups);
    bank->layers          = (SndBankLayer*)tableCursor;
    bank->groupFirstLayer = (u16*)(tableCursor + payload->layerCount * (s32)sizeof(*bank->layers));
    return bank;
}

void spuInitSystem(void)
{
    _spuInitSystem(SPU_INIT_HARDWARE);
}

void spuResetSystem(void)
{
    _spuInitSystem(SPU_INIT_WAIT_TRANSFER);
}

/// Arms an extra audio update after six events of the installed PAL root counter.
///
/// Requires the PAL counter-zero interrupt to be installed and running. Called
/// under the audio-work guard after a VBlank update: replaces the countdown,
/// resets the timer origin, then publishes the arm. The timer callback consumes
/// it once; no sound events or audio work are processed here.
static inline void _spuArmPalExtraAudioUpdate(void)
{
    enum {
        SPU_PAL_EXTRA_UPDATE_TIMER_EVENTS = 6,
        SPU_PAL_EXTRA_UPDATE_ARMED        = 1
    };

    D_8007E0CC = SPU_PAL_EXTRA_UPDATE_TIMER_EVENTS;
    ResetRCnt(RCntCNT0);
    D_800680A4 = SPU_PAL_EXTRA_UPDATE_ARMED;
}

void spuRunVBlankAudioUpdate(void)
{
    enum {
        SPU_AUDIO_WORK_HELD      = 0,
        SPU_AUDIO_WORK_AVAILABLE = 1
    };

    if (D_800680C0 != 0) {
        D_800680C0 = SPU_AUDIO_WORK_HELD;
        // Requests are drained before polls so this update can advance new playback.
        spuTickVoices();
        sndEvtDrainQueue();
        _audioTickProcess();
        spuFlushVoiceUpdates();
        D_800680BC += 1;
        if (gDisplayState.region == MODE_PAL) {
            // Schedule the extra PAL audio update from this VBlank's timer origin.
            _spuArmPalExtraAudioUpdate();
        }
        D_800680C0 = SPU_AUDIO_WORK_AVAILABLE;
    }
}

/// Clears every bank descriptor and forgets retained sequence storage.
///
/// The sound heap has already been reset; this does not free table blocks.
static void _sndBankResetDescriptors(void)
{
    s32      wordIndex;
    s32*     words;
    s32      bankIndex;
    SndBank* bank;
    u16      freeId;

    // Clear the complete descriptor array with word stores before marking ids free.
    words     = (s32*)Snd_Banks;
    wordIndex = 0;
    do {
        *words = 0;
        wordIndex++;
        words++;
    } while ((u32)wordIndex < sizeof(Snd_Banks) / sizeof(*words));

    freeId    = SOUND_BANK_ID_FREE;
    bankIndex = ARRAY_SIZE(Snd_Banks) - 1;
    bank      = Snd_Banks;
    bank     += ARRAY_SIZE(Snd_Banks) - 1;
    do {
        bank->bankId = freeId;
        bankIndex--;
        bank--;
    } while (bankIndex >= 0);

    Snd_SequenceBankBuffer = NULL;
}

void sndBankFree(SndBank* bank)
{
    if ((bank != NULL) && ((bank->bankId & SOUND_BANK_TYPE_MASK) != SOUND_BANK_TYPE_SEQUENCE)) {
        sndHeapFree(bank->heapBlock);
        bank->heapBlock       = NULL;
        bank->groups          = NULL;
        bank->layers          = NULL;
        bank->groupFirstLayer = NULL;
        bank->bankId          = SOUND_BANK_ID_FREE;
        bank->waveBytes       = 0;
    }
}

SndBank* sndBankFind(u16 bankId)
{
    s32      bankIndex;
    SndBank* bank;
    s32      lookupId;

    if (bankId == SOUND_BANK_ID_FREE) {
        bankId = 0;
    }
    lookupId = bankId;

    for (bankIndex = 0, bank = Snd_Banks; bankIndex < ARRAY_SIZE(Snd_Banks); bankIndex++, bank++) {
        if (bank->bankId == lookupId) {
            return bank;
        }
    }
    return NULL;
}

void sndBankBuildLayerIndex(SndBank* bank)
{
    u16*          layerIndex;
    SndBankGroup* group;
    s32           groupCountdown;
    u8            groupCount;

    layerIndex = bank->groupFirstLayer;
    if (layerIndex != NULL) {
        group       = bank->groups;
        *layerIndex = 0;
        groupCount  = bank->groupCount;
        layerIndex++;
        groupCountdown = groupCount - 1;
        if (groupCountdown > 0) {
            groupCountdown = groupCount - 2;
            if (groupCountdown != -1) {
                do {
                    *layerIndex = layerIndex[-1] + group->layerCount;
                    group++;
                    groupCountdown--;
                    layerIndex++;
                } while (groupCountdown != -1);
            }
        }
    }
}

void linInterpSetup(LinInterp* ramp, s32 startLevel, s32 endLevel, s32 updateCount)
{
    enum { LINEAR_INTERPOLATOR_LEVEL_MASK = 0xFF };
    s32 levelDelta;
    s32 unityGain;

    startLevel &= LINEAR_INTERPOLATOR_LEVEL_MASK;
    endLevel   &= LINEAR_INTERPOLATOR_LEVEL_MASK;

    if (startLevel == endLevel || updateCount == 0) {
        ramp->step       = 0;
        ramp->targetGain = 0;
        ramp->gain       = 0;
        ramp->enabled    = LINEAR_INTERPOLATOR_BYPASS;
        return;
    }

    // Select normalized endpoints; selector magnitudes are not ramp gains.
    unityGain  = LINEAR_INTERPOLATOR_UNITY_GAIN;
    ramp->step = unityGain / updateCount;
    levelDelta = endLevel - startLevel;
    if (levelDelta < 0) {
        ramp->direction  = LINEAR_INTERPOLATOR_DECREASING;
        ramp->gain       = unityGain;
        ramp->targetGain = 0;
    } else {
        ramp->direction  = LINEAR_INTERPOLATOR_INCREASING;
        ramp->gain       = 0;
        ramp->targetGain = unityGain;
    }
    ramp->enabled = LINEAR_INTERPOLATOR_SCALE;
}

s32 linInterpApply(LinInterp* ramp, s32 level)
{
    if (ramp->enabled == LINEAR_INTERPOLATOR_SCALE) {
        if (ramp->gain == ramp->targetGain) {
            ramp->step = 0;
        }
        level = (s32)((level * ramp->gain) / LINEAR_INTERPOLATOR_UNITY_GAIN);
    }
    return level;
}

void linInterpStep(LinInterp* ramp)
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

void spuCalcPanVolumes(SpuVolume* volumes, s16 pan, s32 volume)
{
    enum {
        SPU_PAN_MAX                = 127,
        SPU_PAN_TABLE_LAST_INDEX   = SPU_PAN_MAX - 1,
        SPU_PAN_TABLE_CENTER_INDEX = SPU_PAN_TABLE_LAST_INDEX / 2,
        SPU_PAN_GAIN_FRACTION_BITS = 12,
        SPU_DIRECT_VOLUME_MAX      = 0x3FFF
    };
    s16 index;
    u32 left;
    u32 right;

    if (pan > 0) {
        index = pan - 1;
        if (pan > SPU_PAN_MAX) {
            index = SPU_PAN_TABLE_LAST_INDEX;
        }
    } else {
        index = 0;
    }

    left  = (u32)(volume * Snd_PanGainTable[index]) >> SPU_PAN_GAIN_FRACTION_BITS;
    right = (u32)(volume * Snd_PanGainTable[SPU_PAN_TABLE_LAST_INDEX - index]) >> SPU_PAN_GAIN_FRACTION_BITS;

    // Mono folds both gains through the same centre-pan attenuation.
    if (!sndOutputIsStereo()) {
        right = (u32)((left + right) * Snd_PanGainTable[SPU_PAN_TABLE_CENTER_INDEX]) >> SPU_PAN_GAIN_FRACTION_BITS;
        left  = right;
    }

    if (left < SPU_DIRECT_VOLUME_MAX + 1U) {
        volumes->left = (s16)left;
    } else {
        volumes->left = SPU_DIRECT_VOLUME_MAX;
    }

    if (right < SPU_DIRECT_VOLUME_MAX + 1U) {
        volumes->right = (s16)right;
    } else {
        volumes->right = SPU_DIRECT_VOLUME_MAX;
    }
}

s32 audioTickInsert(AudioTickPoll poll, AudioTickOnRemove onRemove, u16 id, s32* pollArg)
{
    _AudioTickNode* node;
    _AudioTickNode* head;
    _AudioTickNode* previous;
    _AudioTickNode* next;

    head = &AudioTick_List;
    if (head == NULL) {
        return AUDIO_TICK_NO_MEMORY;
    }
    // Disable polling during allocation and the ordered-id search.
    AudioTick_Enabled = 0;
    node              = sndHeapAlloc(sizeof(*node));
    if (node == NULL) {
        AudioTick_Enabled = 1;
        return AUDIO_TICK_NO_MEMORY;
    }
    node->poll     = poll;
    node->onRemove = onRemove;
    node->id       = id;
    node->arg      = pollArg;
    node->prev     = NULL;
    node->next     = NULL;

    previous = head;
    for (;;) {
        next = previous->next;
        if (next == NULL) {
            previous->next    = node;
            node->prev        = previous;
            node->next        = NULL;
            AudioTick_Enabled = 1;
            return AUDIO_TICK_INSERTED;
        }
        if (next->id == id) {
            sndHeapFree(node);
            AudioTick_Enabled = 1;
            return AUDIO_TICK_DUPLICATE_ID;
        }
        if (id < next->id) {
            node->next           = next;
            AudioTick_Enabled    = 1;
            previous->next->prev = node;
            previous->next       = node;
            node->prev           = previous;
            return AUDIO_TICK_INSERTED;
        }
        previous = next;
    }
}

/// Resets the sound heap to one free block occupying the complete buffer.
///
/// Invalidates every prior payload; callers and audio callbacks must be quiescent.
static void _sndHeapReset(void)
{
    SndHeap_Start              = (_SndHeapBlockHeader*)SndHeap_Buffer;
    SndHeap_Start->size        = sizeof(SndHeap_Buffer);
    SndHeap_Start->magic       = SNDHEAP_START_MAGIC;
    SndHeap_Start->isAllocated = false;
    SndHeap_Start->prev        = NULL;
    SndHeap_Start->next        = NULL;
}

void* sndHeapAlloc(size_t payloadBytes)
{
    // First fit over the address-ordered chain. Split a free block when the
    // remainder can hold another header; otherwise reserve the whole block and
    // leave its length unchanged. The returned pointer is the payload.
    enum { SOUND_HEAP_BLOCK_ALIGNMENT = 4 };
    size_t               maxFreeBlockBytes;
    size_t               newBlockSize;
    size_t               blockBytes;
    _SndHeapBlockHeader* block;
    _SndHeapBlockHeader* newBlock;

    maxFreeBlockBytes = 0;
    // Header plus payload, rounded up to the block alignment.
    blockBytes = (payloadBytes + sizeof(_SndHeapBlockHeader) + SOUND_HEAP_BLOCK_ALIGNMENT - 1) & ~(SOUND_HEAP_BLOCK_ALIGNMENT - 1);

    for (block = SndHeap_Start; block != NULL; block = block->next) {
        // A header outside the buffer is not a sound-heap block.
        if ((uintptr)block < (uintptr)SndHeap_Buffer ||
            (uintptr)(SndHeap_Buffer + sizeof(SndHeap_Buffer)) < (uintptr)block) {
            return NULL;
        }

        if (block->isAllocated) {
            continue;
        }

        // The original search retains this running maximum without using the result.
        if (maxFreeBlockBytes < block->size) {
            maxFreeBlockBytes = block->size;
        }

        if (block->size >= blockBytes) {
            newBlockSize = block->size - blockBytes;
            newBlock     = (_SndHeapBlockHeader*)((u8*)block + blockBytes);

            // Insert the remainder immediately after this block.
            if (sizeof(_SndHeapBlockHeader) < newBlockSize) {
                _sndHeapSplitBlock(block, newBlock, blockBytes);
            } else {
                block->isAllocated = true;
            }

            // The allocated data is located just after the header.
            return block + 1;
        }
    }

    return NULL;
}

void sndHeapFree(void* payload)
{
    // Release a payload pointer, or return on NULL. Coalesce with free
    // neighbors so the chain stays in address order.
    uintptr              heapStart;
    uintptr              heapEnd;
    _SndHeapBlockHeader* header;

    if (payload == NULL) {
        return;
    }

    // Compare numeric addresses because an invalid input may point outside
    // this allocation; relational C pointer comparisons would not be defined.
    // Keep the original inclusive upper-bound test. A valid input is still
    // required to be a payload returned by sndHeapAlloc.
    heapStart = (uintptr)SndHeap_Buffer;
    if ((uintptr)payload < heapStart) {
        return;
    }

    heapEnd = heapStart + sizeof(SndHeap_Buffer);
    if (heapEnd < (uintptr)payload) {
        return;
    }

    // The payload starts immediately after its header. The reserved flag is
    // cleared before the recognizer is tested; any other value returns without
    // coalescing, so the flag stays clear. Both sound-heap recognizers take
    // the same merge path.
    header              = (_SndHeapBlockHeader*)payload - 1;
    header->isAllocated = false;
    if (header->magic != SNDHEAP_MAGIC && header->magic != SNDHEAP_START_MAGIC) {
        return;
    }

    // A free predecessor absorbs this block and becomes the block to merge forward.
    if (header->prev != NULL && header->prev->isAllocated == false) {
        if (header->next != NULL) {
            header->next->prev = header->prev;
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

/// Counts PAL root-counter events until the armed extra audio update is due.
///
/// Each PAL frame arms six events. Clears the arm before calling the guarded
/// audio update; that update can be skipped during initialization/reentry.
/// Does not process queued sound events. The SDK interrupt callback returns zero.
static long _spuTimerCallback(void)
{
    if (D_800680A4 != 0) {
        D_8007E0CC--;
        if (D_8007E0CC == 0) {
            D_800680A4 = 0;
            _spuRunTimerAudioUpdate();
        }
    }
    return 0;
}

/// Runs an extra PAL timer audio update under the sound driver's reentry guard.
///
/// Advances voices and registered audio polls, then flushes SPU changes without
/// processing queued sound events. A held initialization/update guard skips
/// the update. Releases the guard before incrementing the completed-update
/// counter. Returns zero whether the update ran or was skipped.
static s32 _spuRunTimerAudioUpdate(void)
{
    if (D_800680C0 == 0) {
        return 0;
    }
    D_800680C0 = 0;
    spuTickVoices();
    _audioTickProcess();
    spuFlushVoiceUpdates();
    D_800680C0  = 1;
    D_800680BC += 1;
    return 0;
}

/// Clears the poll-list sentinel and enables polling for sound-system startup.
///
/// Existing nodes are forgotten, not freed; the sound heap was reset first.
static void _audioTickReset(void)
{
    AudioTick_List.poll     = NULL;
    AudioTick_List.onRemove = NULL;
    AudioTick_List.id       = 0;
    AudioTick_List.arg      = NULL;
    AudioTick_List.next     = NULL;
    AudioTick_List.prev     = NULL;
    AudioTick_Enabled       = 1;
}

/// Polls registrations in ascending id order once for an audio update.
///
/// Runs after voice ticks in vertical-blank and PAL timer interrupt work.
/// A disabled list skips the entire update. Completion unlinks a node and
/// resumes at its successor without releasing either node or argument storage.
static void _audioTickProcess(void)
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
                    if (poll(node->arg) == AUDIO_TICK_POLL_FINISHED) {
                        node = _audioTickRemove(node);
                        continue;
                    }
                }
                node = node->next;
            }
        }
    }
}

/// Unlinks a registered node by id and returns the next node to poll.
///
/// Runs its optional removal handler before unlinking, with polling disabled.
/// Returns NULL when the id is absent. Neither the node nor its argument is
/// returned to the heap; their storage lasts until sound-system reset.
static _AudioTickNode* _audioTickRemove(_AudioTickNode* node)
{
    AudioTickOnRemove onRemove;
    _AudioTickNode*   head;
    _AudioTickNode*   previous;
    _AudioTickNode*   current;

    head              = &AudioTick_List;
    onRemove          = node->onRemove;
    AudioTick_Enabled = 0;
    if (onRemove != NULL) {
        onRemove();
    }

    // Re-find the preceding registration by id; the node's prev link is not read.
    previous = head;
    if (previous->next != NULL) {
        do {
            current = previous->next;
            if (current->id == node->id) {
                _audioTickUnlinkNode(previous, node);
                AudioTick_Enabled = 1;
                return previous->next;
            }
            previous = current;
        } while (previous->next != NULL);
    }
    AudioTick_Enabled = 1;
    return NULL;
}
