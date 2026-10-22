#include "main/sound.h"

#include <psyq/sys/types.h>
#include <psyq/kernel.h>
#include <psyq/libapi.h>
#include <psyq/libspu.h>
#include <psyq/libetc.h>

#include "common.h"

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

#include "weapons/as12.h"

#include "weapons/grenade_pistol.h"

#include "weapons/hypervelocity.h"

#include "weapons/m249.h"

#include "weapons/m4a1.h"

#include "weapons/m4a1_grenade.h"

#include "weapons/m4a1_hammer.h"

#include "weapons/m4a1_javelin.h"

#include "weapons/m4a1_pyke.h"

#include "weapons/m93r.h"

#include "weapons/m950.h"

#include "weapons/p08.h"

#include "weapons/pa3.h"

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
/// The list hangs from a sentinel node, `_gAudioTickSentinel`, of which only `next`
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

/// Total byte extent of the sound allocator's arena, including block headers.
#define SOUND_HEAP_BUFFER_BYTES 0x3D00

/// Header recognizer of the arena's initial block, retained through allocation and merging.
#define SOUND_HEAP_INITIAL_BLOCK_MAGIC 0xB25A

/// Header recognizer of a block created by splitting, independent of its allocation state.
#define SOUND_HEAP_SPLIT_BLOCK_MAGIC 0xA52B

// Audio work and polling are admitted by separate gates; neither waits for a running update.
enum {
    SPU_AUDIO_WORK_HELD        = 0,
    SPU_AUDIO_WORK_AVAILABLE   = 1,
    AUDIO_TICK_POLLING_HELD    = 0,
    AUDIO_TICK_POLLING_ENABLED = 1,
    SPU_PAL_EXTRA_UPDATE_IDLE  = 0,
    SPU_PAL_EXTRA_UPDATE_ARMED = 1
};

/// First block header in the sound heap's address-ordered chain.
///
/// Reset establishes this header at the arena base. It stays the first block
/// through splitting and coalescing; all headers and payloads expire at reset.
static _SndHeapBlockHeader* _gSndHeapFirstBlock;

/// Byte arena for variable-sized sound allocations, including their block headers.
///
/// Blocks and payloads start on four-byte boundaries. This is raw storage,
/// partitioned by the allocator rather than an array of equal-sized headers.
/// Reset invalidates all allocations; callers must serialize heap operations.
static u8 _gSndHeapArena[SOUND_HEAP_BUFFER_BYTES];

/// Sentinel preceding the first registration in the ascending-id audio poll list.
///
/// Only `next` is read; reset clears the complete node. The first registration's
/// `prev` points here. Nodes belong to the sound heap and expire at heap reset.
static _AudioTickNode _gAudioTickSentinel;

/// Word gate for registered audio polls (0 skip this update, 1 permit polling).
///
/// Insert and remove temporarily clear it. Sorted insertion releases it before
/// its final link stores, so it does not serialize the complete edit.
static u32 _gAudioTickPollingEnabled;

/// PAL root-counter events remaining before the armed extra audio update.
///
/// Arming loads six; the timer interrupt decrements this byte only while armed.
/// Zero triggers one update and clears the arm. Byte wrap behavior is retained.
static u8 _gSpuPalTimerEventsRemaining;

/// BIOS event handle for the installed PAL counter-zero interrupt.
///
/// Valid only while `_gSpuPalTimerInstalled` is set; timer replacement disables
/// and closes that event before opening and enabling its successor.
static volatile long _gSpuPalTimerEvent;

/* Define BSS before API headers to preserve first-declaration order. */

void* gSndBankSequenceTableBlock;

SndBank gSndBankDescriptors[SOUND_BANK_DESCRIPTOR_COUNT];

/// Unreferenced storage; its role and element grouping are unproven.
u8 D_8007E2D8[8];

#include "sound.h"

static u8 _gSpuPalExtraAudioUpdateArmed;

static u8 _gSpuPalTimerInstalled;

/// Unreferenced initialized word; its role and representation are unproven.
static s32 D_800680A8;

static u32 _gSpuCompletedAudioUpdates;

static volatile u32 _gSpuAudioWorkAvailable;

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
    remainder->magic       = SOUND_HEAP_SPLIT_BLOCK_MAGIC;
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

TaskDesc gTaskBank2Descs[TASK_BANK2_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_807257A0 },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x70 } }, effectBurstModelPartTask },
    { { { TASK_BODY_NONE, 0x80 } }, capPlaybackTask },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisBridgeKeypadTask },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisSecurityRoomMonitorTask },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisSecurityRoomPowerSupplyTask },
    { { { TASK_BODY_NONE, 0xC0 } }, padScriptBinaryMotorHoldTask },
    { { { TASK_BODY_NONE, 0xC0 } }, padScriptVariableMotorRampTask },
    { { { TASK_BODY_NONE, 0xC0 } }, padScriptTask },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisFountainClimbTask },
    { { { TASK_BODY_NONE, 0xC0 } }, acropolisHelicopterLandingPadExitTask },
    { { { TASK_BODY_TMD, 0x60 } }, effectBurstModelPartTask },
};

TaskDesc gTaskBank7Descs[TASK_BANK7_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, playerActorWorkTask, { &gAya10400BodyModel } },
    { { { TASK_BODY_TMD, 0x50 } }, playerActorWorkTask, { &gAya10300BodyModel } },
    { { { TASK_BODY_TMD, 0x50 } }, playerActorWorkTask, { &gAya10200BodyModel } },
    { { { TASK_BODY_TMD, 0x50 } }, playerActorWorkTask, { &gAya10500BodyModel } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10400Hand0Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10400FirstPairHand1Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10400Hand0Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10400SecondPairHand1Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10300FirstPairHand0Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10300FirstPairHand1Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10300SecondPairHand0Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10300SecondPairHand1Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10200FirstPairHand0Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10200FirstPairHand1Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10200SecondPairHand0Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10200SecondPairHand1Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10500FirstPairHand0Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10500FirstPairHand1Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10500SecondPairHand0Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAya10500SecondPairHand1Model } },
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
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gP08WeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM93rWeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM950WeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gP08WeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gP229WeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gUnused85HeldWeaponModel } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gMongooseModel } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gGrenadePistolWeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gMm1WeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gPa3WeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gSp12Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gAs12Model } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM4a1RifleModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM249WeaponModel } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, tonfaBatonModelTask, { &gTonfaBatonWeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM4a1RifleModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM4a1RifleModel } },
    { { { TASK_BODY_TMD, 0x50 } }, hypervelocityWeaponModelTask, { &gHypervelocityRootModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gGunbladeModel } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM4a1HammerModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM4a1BayonetWeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM4a1GrenadeWeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM4a1PykeWeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gM4a1JavelinWeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gMp5a5WeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gMp5a5WeaponModel } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gMp5a5WeaponModel } },
    { { { TASK_BODY_TMD, 0x52 } }, m4a1GrenadeShellTask, { &gM4a1GrenadeProjectileModel } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x52 } }, grenadePistolShellTask, { .model = &gGrenadePistolShellModel } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x52 } }, mm1ShellTask, { .model = &gMm1ShellModel } },
    { { { TASK_BODY_TMD, 0x52 } }, kyle800102GrenadeShellTask, { .model = &gKyleMm1GrenadeShell } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, tonfaBatonModelTask, { &gTonfaBatonShaftModel } },
    { { { TASK_BODY_TMD, 0x50 } }, hypervelocityWeaponModelTask, { &gHypervelocitySlideModel } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, hypervelocityWeaponModelTask, { &gHypervelocityHingeModel } },
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
    { { { TASK_BODY_TMD, 0x50 } }, actor800100Task, { &gKyle800101Body } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800100Task, { &gKyle800102Body } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800100Task, { &gKyle800103Body } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800100Task, { &gKyle800104Body } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800200Task, { &gFlintBody } },
    { { { TASK_BODY_TMD, 0x50 } }, actor800300Task, { &gEveCompanionBody } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gKyle800101HandRight } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800101HandLeft2 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800101HandRight3 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800101HandLeft } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gKyle800102HandRight } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800102HandLeft2 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800102HandRight3 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800102HandLeft } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gKyle800103HandRight } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800103HandLeft2 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800103HandRight3 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800103HandLeft } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gKyle800104HandRight } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800104HandLeft2 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800104HandRight3 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyle800104HandLeft } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyleP229 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyleMm1 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { .model = &gKyleM950 } },
    { { { TASK_BODY_TMD, 0x50 } }, modelObjectChildTask, { &gKyleM4a1 } },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x50 } }, acropolisCafeteriaLoosePropTask, { &gAcropolisCafeteriaModel07CA8 } },
    { { { TASK_BODY_TMD, 0x50 } }, acropolisCafeteriaLoosePropTask, { &gAcropolisCafeteriaModel08638 } },
    { { { TASK_BODY_TMD, 0x50 } }, acropolisCafeteriaLoosePropTask, { &gAcropolisCafeteriaModel09090 } },
    { { { TASK_BODY_TMD, 0x50 } }, acropolisCafeteriaLoosePropTask, { &gAcropolisCafeteriaModel09A20 } },
};

/// Arm for one PAL timer audio update (0 idle, 1 count root-counter events).
///
/// Published after loading the countdown and resetting the counter; consumed
/// before attempting the guarded update, even if that update is skipped.
static u8 _gSpuPalExtraAudioUpdateArmed = SPU_PAL_EXTRA_UPDATE_IDLE;
/// Whether the optional PAL counter-zero BIOS event is installed (0 no, 1 yes).
///
/// Initialization uses this byte to decide whether to disable and close the
/// previous event. Hardware initialization clears it before rebuilding state.
static u8 _gSpuPalTimerInstalled = false;
/// Unreferenced initialized word; its role and representation are unproven.
static s32 D_800680A8                                 = 0;
s8         gSndBankSlotsByType[SOUND_BANK_TYPE_COUNT] = {
    0, 1, 2, 3, 4, 7, 12, 13,
    SOUND_BANK_SLOT_UNSUPPORTED, SOUND_BANK_SLOT_UNSUPPORTED,
    SOUND_BANK_SLOT_UNSUPPORTED, SOUND_BANK_SLOT_UNSUPPORTED,
    SOUND_BANK_SLOT_UNSUPPORTED, SOUND_BANK_SLOT_UNSUPPORTED,
    8, 10
};
/// Wrapping count of executed VBlank and extra PAL timer audio updates.
///
/// Hardware and wait-transfer initialization reset it. Playback-only rebuilding
/// retains it; a skipped update does not increment it. No reader is known.
static u32 _gSpuCompletedAudioUpdates = 0;
/// Interrupt-visible audio-work gate (0 held, 1 available), initially held.
///
/// Initialization and both update paths clear it while working. It suppresses
/// new work without waiting for work already running. VBlank increments its
/// counter before releasing this gate; timer work releases before incrementing.
static volatile u32 _gSpuAudioWorkAvailable = SPU_AUDIO_WORK_HELD;

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
            _gSpuPalTimerInstalled     = false;
            _gSpuCompletedAudioUpdates = 0;
            spuResetCommonOutput();
            break;
        case SPU_INIT_WAIT_TRANSFER:
            SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
            _gSpuCompletedAudioUpdates = 0;
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
    if (_gSpuPalTimerInstalled) {
        DisableEvent(_gSpuPalTimerEvent);
        CloseEvent(_gSpuPalTimerEvent);
        StopRCnt(RCntCNT0);
        _gSpuPalTimerInstalled = false;
    }

    if (gDisplayState.region == MODE_PAL) {
        _gSpuPalExtraAudioUpdateArmed = SPU_PAL_EXTRA_UPDATE_IDLE;
        _gSpuPalTimerEventsRemaining  = 0;
        SetRCnt(RCntCNT0, SPU_PAL_TIMER_PERIOD, RCntMdINTR | RCntMdSC);
        ResetRCnt(RCntCNT0);
        StartRCnt(RCntCNT0);
        EnterCriticalSection();
        _gSpuPalTimerEvent = OpenEvent(RCntCNT0, EvSpINT, EvMdINTR, _spuTimerCallback);
        ExitCriticalSection();
        EnableEvent(_gSpuPalTimerEvent);
        _gSpuPalTimerInstalled = true;
    }
    _gSpuPalExtraAudioUpdateArmed = SPU_PAL_EXTRA_UPDATE_IDLE;
    _gSpuPalTimerEventsRemaining  = 0;
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
    _gSpuAudioWorkAvailable = SPU_AUDIO_WORK_HELD;
    _spuInitSystemState(initMode);
    _gSpuAudioWorkAvailable = SPU_AUDIO_WORK_AVAILABLE;
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
        SOUND_BANK_TYPE_4_FIRST_SLOT       = 4
    };
    SndBank* bank;
    s32      tableBytes;
    u8*      tableCursor;
    u16      bankType       = payload->bankId & SOUND_BANK_TYPE_MASK;
    s32      mappedSlot     = gSndBankSlotsByType[bankType >> 12];
    s8       descriptorSlot = mappedSlot;

    if (mappedSlot == SOUND_BANK_SLOT_UNSUPPORTED) {
        return NULL;
    }

    if (bankType == SOUND_BANK_TYPE_4) {
        descriptorSlot = gSndLoadCharacterUploadOrdinal + SOUND_BANK_TYPE_4_FIRST_SLOT;
    }

    // Sequence reloads retain their table block; other slots release theirs first.
    if (bankType == SOUND_BANK_TYPE_SEQUENCE && gSndBankSequenceTableBlock != NULL) {
        bank            = &gSndBankDescriptors[descriptorSlot];
        bank->heapBlock = gSndBankSequenceTableBlock;
    } else {
        bank = &gSndBankDescriptors[descriptorSlot];
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
        SPU_PAL_EXTRA_UPDATE_TIMER_EVENTS = 6
    };

    _gSpuPalTimerEventsRemaining = SPU_PAL_EXTRA_UPDATE_TIMER_EVENTS;
    ResetRCnt(RCntCNT0);
    _gSpuPalExtraAudioUpdateArmed = SPU_PAL_EXTRA_UPDATE_ARMED;
}

void spuRunVBlankAudioUpdate(void)
{
    if (_gSpuAudioWorkAvailable != SPU_AUDIO_WORK_HELD) {
        _gSpuAudioWorkAvailable = SPU_AUDIO_WORK_HELD;
        // Requests are drained before polls so this update can advance new playback.
        spuTickVoices();
        sndEvtDrainQueue();
        _audioTickProcess();
        spuFlushVoiceUpdates();
        _gSpuCompletedAudioUpdates += 1;
        if (gDisplayState.region == MODE_PAL) {
            // Schedule the extra PAL audio update from this VBlank's timer origin.
            _spuArmPalExtraAudioUpdate();
        }
        _gSpuAudioWorkAvailable = SPU_AUDIO_WORK_AVAILABLE;
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
    words     = (s32*)gSndBankDescriptors;
    wordIndex = 0;
    do {
        *words = 0;
        wordIndex++;
        words++;
    } while ((u32)wordIndex < sizeof(gSndBankDescriptors) / sizeof(*words));

    freeId    = SOUND_BANK_ID_FREE;
    bankIndex = ARRAY_SIZE(gSndBankDescriptors) - 1;
    bank      = gSndBankDescriptors;
    bank     += ARRAY_SIZE(gSndBankDescriptors) - 1;
    do {
        bank->bankId = freeId;
        bankIndex--;
        bank--;
    } while (bankIndex >= 0);

    gSndBankSequenceTableBlock = NULL;
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

    for (bankIndex = 0, bank = gSndBankDescriptors; bankIndex < ARRAY_SIZE(gSndBankDescriptors); bankIndex++, bank++) {
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

    left  = (u32)(volume * gSpuPanGainTable[index]) >> SPU_PAN_GAIN_FRACTION_BITS;
    right = (u32)(volume * gSpuPanGainTable[SPU_PAN_TABLE_LAST_INDEX - index]) >> SPU_PAN_GAIN_FRACTION_BITS;

    // Mono folds both gains through the same centre-pan attenuation.
    if (!sndOutputIsStereo()) {
        right = (u32)((left + right) * gSpuPanGainTable[SPU_PAN_TABLE_CENTER_INDEX]) >> SPU_PAN_GAIN_FRACTION_BITS;
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

    head = &_gAudioTickSentinel;
    if (head == NULL) {
        return AUDIO_TICK_NO_MEMORY;
    }
    // Disable polling during allocation and the ordered-id search.
    _gAudioTickPollingEnabled = AUDIO_TICK_POLLING_HELD;
    node                      = sndHeapAlloc(sizeof(*node));
    if (node == NULL) {
        _gAudioTickPollingEnabled = AUDIO_TICK_POLLING_ENABLED;
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
            previous->next            = node;
            node->prev                = previous;
            node->next                = NULL;
            _gAudioTickPollingEnabled = AUDIO_TICK_POLLING_ENABLED;
            return AUDIO_TICK_INSERTED;
        }
        if (next->id == id) {
            sndHeapFree(node);
            _gAudioTickPollingEnabled = AUDIO_TICK_POLLING_ENABLED;
            return AUDIO_TICK_DUPLICATE_ID;
        }
        if (id < next->id) {
            node->next                = next;
            _gAudioTickPollingEnabled = AUDIO_TICK_POLLING_ENABLED;
            previous->next->prev      = node;
            previous->next            = node;
            node->prev                = previous;
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
    _gSndHeapFirstBlock              = (_SndHeapBlockHeader*)_gSndHeapArena;
    _gSndHeapFirstBlock->size        = sizeof(_gSndHeapArena);
    _gSndHeapFirstBlock->magic       = SOUND_HEAP_INITIAL_BLOCK_MAGIC;
    _gSndHeapFirstBlock->isAllocated = false;
    _gSndHeapFirstBlock->prev        = NULL;
    _gSndHeapFirstBlock->next        = NULL;
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

    for (block = _gSndHeapFirstBlock; block != NULL; block = block->next) {
        // A header outside the buffer is not a sound-heap block.
        if ((uintptr)block < (uintptr)_gSndHeapArena ||
            (uintptr)(_gSndHeapArena + sizeof(_gSndHeapArena)) < (uintptr)block) {
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
    heapStart = (uintptr)_gSndHeapArena;
    if ((uintptr)payload < heapStart) {
        return;
    }

    heapEnd = heapStart + sizeof(_gSndHeapArena);
    if (heapEnd < (uintptr)payload) {
        return;
    }

    // The payload starts immediately after its header. The reserved flag is
    // cleared before the recognizer is tested; any other value returns without
    // coalescing, so the flag stays clear. Both sound-heap recognizers take
    // the same merge path.
    header              = (_SndHeapBlockHeader*)payload - 1;
    header->isAllocated = false;
    if (header->magic != SOUND_HEAP_SPLIT_BLOCK_MAGIC && header->magic != SOUND_HEAP_INITIAL_BLOCK_MAGIC) {
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
    if (_gSpuPalExtraAudioUpdateArmed != SPU_PAL_EXTRA_UPDATE_IDLE) {
        _gSpuPalTimerEventsRemaining--;
        if (_gSpuPalTimerEventsRemaining == 0) {
            _gSpuPalExtraAudioUpdateArmed = SPU_PAL_EXTRA_UPDATE_IDLE;
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
    if (_gSpuAudioWorkAvailable == SPU_AUDIO_WORK_HELD) {
        return 0;
    }
    _gSpuAudioWorkAvailable = SPU_AUDIO_WORK_HELD;
    spuTickVoices();
    _audioTickProcess();
    spuFlushVoiceUpdates();
    _gSpuAudioWorkAvailable     = SPU_AUDIO_WORK_AVAILABLE;
    _gSpuCompletedAudioUpdates += 1;
    return 0;
}

/// Clears the poll-list sentinel and enables polling for sound-system startup.
///
/// Existing nodes are forgotten, not freed; the sound heap was reset first.
static void _audioTickReset(void)
{
    _gAudioTickSentinel.poll     = NULL;
    _gAudioTickSentinel.onRemove = NULL;
    _gAudioTickSentinel.id       = 0;
    _gAudioTickSentinel.arg      = NULL;
    _gAudioTickSentinel.next     = NULL;
    _gAudioTickSentinel.prev     = NULL;
    _gAudioTickPollingEnabled    = AUDIO_TICK_POLLING_ENABLED;
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

    head = &_gAudioTickSentinel;
    if (_gAudioTickPollingEnabled != AUDIO_TICK_POLLING_HELD) {
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

    head                      = &_gAudioTickSentinel;
    onRemove                  = node->onRemove;
    _gAudioTickPollingEnabled = AUDIO_TICK_POLLING_HELD;
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
                _gAudioTickPollingEnabled = AUDIO_TICK_POLLING_ENABLED;
                return previous->next;
            }
            previous = current;
        } while (previous->next != NULL);
    }
    _gAudioTickPollingEnabled = AUDIO_TICK_POLLING_ENABLED;
    return NULL;
}
