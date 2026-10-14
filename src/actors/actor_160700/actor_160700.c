#include "actors/actor_160700.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/walk/paced_walk.h"
#include "../../shared/walk/walker.h"

/// Conversation progress handled while the actor is in the GOLEM freezer room.
enum {
    ACTOR_160700_MEETING_UNMET       = 0,
    ACTOR_160700_MEETING_FIRST_SEEN  = 1,
    ACTOR_160700_MEETING_SECOND_SEEN = 2,
    ACTOR_160700_MEETING_THIRD_SEEN  = 3,
};

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor160700Exit(Task* task);

static TmdSource _gActor160700PierceCarradineBody;
static TmdSource _gActor160700Actor113100Model07960;
static s32       _pacedWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32       _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 flags, s32 unusedArg);
static s32       _actor160700IgnoreCommand(Task* task, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArg);
static void      _actor160700PacedWalkerTask(Task* task);
static void      _pacedWalkSubModelTask(Task* task);

static AnimationPackedPose _gActor160700Animation00C90Bank1[2] = {
#include "assets/actor_160700_animation_00C90_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation00C90Bank4[26] = {
#include "assets/actor_160700_animation_00C90_bank4.inc"
};

static AnimationRecord _gActor160700Animation00C90Records[99] = {
#include "assets/actor_160700_animation_00C90_records.inc"
};

static u16 _gActor160700Animation00C90Indices[20] = {
#include "assets/actor_160700_animation_00C90_indices.inc"
};

static AnimationSet _gActor160700Animation00C90 = {
    _gActor160700Animation00C90Records,
    _gActor160700Animation00C90Indices,
    { NULL, _gActor160700Animation00C90Bank1, NULL, NULL, _gActor160700Animation00C90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation00EC0Bank1[3] = {
#include "assets/actor_160700_animation_00EC0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation00EC0Bank4[30] = {
#include "assets/actor_160700_animation_00EC0_bank4.inc"
};

static AnimationRecord _gActor160700Animation00EC0Records[81] = {
#include "assets/actor_160700_animation_00EC0_records.inc"
};

static u16 _gActor160700Animation00EC0Indices[20] = {
#include "assets/actor_160700_animation_00EC0_indices.inc"
};

static AnimationSet _gActor160700Animation00EC0 = {
    _gActor160700Animation00EC0Records,
    _gActor160700Animation00EC0Indices,
    { NULL, _gActor160700Animation00EC0Bank1, NULL, NULL, _gActor160700Animation00EC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01184Bank1[3] = {
#include "assets/actor_160700_animation_01184_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01184Bank4[36] = {
#include "assets/actor_160700_animation_01184_bank4.inc"
};

static AnimationRecord _gActor160700Animation01184Records[112] = {
#include "assets/actor_160700_animation_01184_records.inc"
};

static u16 _gActor160700Animation01184Indices[20] = {
#include "assets/actor_160700_animation_01184_indices.inc"
};

static AnimationSet _gActor160700Animation01184 = {
    _gActor160700Animation01184Records,
    _gActor160700Animation01184Indices,
    { NULL, _gActor160700Animation01184Bank1, NULL, NULL, _gActor160700Animation01184Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0135CBank1[3] = {
#include "assets/actor_160700_animation_0135C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0135CBank4[32] = {
#include "assets/actor_160700_animation_0135C_bank4.inc"
};

static AnimationRecord _gActor160700Animation0135CRecords[57] = {
#include "assets/actor_160700_animation_0135C_records.inc"
};

static u16 _gActor160700Animation0135CIndices[20] = {
#include "assets/actor_160700_animation_0135C_indices.inc"
};

static AnimationSet _gActor160700Animation0135C = {
    _gActor160700Animation0135CRecords,
    _gActor160700Animation0135CIndices,
    { NULL, _gActor160700Animation0135CBank1, NULL, NULL, _gActor160700Animation0135CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01530Bank1[3] = {
#include "assets/actor_160700_animation_01530_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01530Bank4[31] = {
#include "assets/actor_160700_animation_01530_bank4.inc"
};

static AnimationRecord _gActor160700Animation01530Records[57] = {
#include "assets/actor_160700_animation_01530_records.inc"
};

static u16 _gActor160700Animation01530Indices[20] = {
#include "assets/actor_160700_animation_01530_indices.inc"
};

static AnimationSet _gActor160700Animation01530 = {
    _gActor160700Animation01530Records,
    _gActor160700Animation01530Indices,
    { NULL, _gActor160700Animation01530Bank1, NULL, NULL, _gActor160700Animation01530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation017A0Bank1[3] = {
#include "assets/actor_160700_animation_017A0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation017A0Bank4[29] = {
#include "assets/actor_160700_animation_017A0_bank4.inc"
};

static AnimationRecord _gActor160700Animation017A0Records[98] = {
#include "assets/actor_160700_animation_017A0_records.inc"
};

static u16 _gActor160700Animation017A0Indices[20] = {
#include "assets/actor_160700_animation_017A0_indices.inc"
};

static AnimationSet _gActor160700Animation017A0 = {
    _gActor160700Animation017A0Records,
    _gActor160700Animation017A0Indices,
    { NULL, _gActor160700Animation017A0Bank1, NULL, NULL, _gActor160700Animation017A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01A4CBank1[3] = {
#include "assets/actor_160700_animation_01A4C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01A4CBank4[32] = {
#include "assets/actor_160700_animation_01A4C_bank4.inc"
};

static AnimationRecord _gActor160700Animation01A4CRecords[110] = {
#include "assets/actor_160700_animation_01A4C_records.inc"
};

static u16 _gActor160700Animation01A4CIndices[20] = {
#include "assets/actor_160700_animation_01A4C_indices.inc"
};

static AnimationSet _gActor160700Animation01A4C = {
    _gActor160700Animation01A4CRecords,
    _gActor160700Animation01A4CIndices,
    { NULL, _gActor160700Animation01A4CBank1, NULL, NULL, _gActor160700Animation01A4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01C18Bank1[3] = {
#include "assets/actor_160700_animation_01C18_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01C18Bank4[29] = {
#include "assets/actor_160700_animation_01C18_bank4.inc"
};

static AnimationRecord _gActor160700Animation01C18Records[57] = {
#include "assets/actor_160700_animation_01C18_records.inc"
};

static u16 _gActor160700Animation01C18Indices[20] = {
#include "assets/actor_160700_animation_01C18_indices.inc"
};

static AnimationSet _gActor160700Animation01C18 = {
    _gActor160700Animation01C18Records,
    _gActor160700Animation01C18Indices,
    { NULL, _gActor160700Animation01C18Bank1, NULL, NULL, _gActor160700Animation01C18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation01E54Bank1[2] = {
#include "assets/actor_160700_animation_01E54_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation01E54Bank4[35] = {
#include "assets/actor_160700_animation_01E54_bank4.inc"
};

static AnimationRecord _gActor160700Animation01E54Records[82] = {
#include "assets/actor_160700_animation_01E54_records.inc"
};

static u16 _gActor160700Animation01E54Indices[20] = {
#include "assets/actor_160700_animation_01E54_indices.inc"
};

static AnimationSet _gActor160700Animation01E54 = {
    _gActor160700Animation01E54Records,
    _gActor160700Animation01E54Indices,
    { NULL, _gActor160700Animation01E54Bank1, NULL, NULL, _gActor160700Animation01E54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02024Bank1[3] = {
#include "assets/actor_160700_animation_02024_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02024Bank4[30] = {
#include "assets/actor_160700_animation_02024_bank4.inc"
};

static AnimationRecord _gActor160700Animation02024Records[57] = {
#include "assets/actor_160700_animation_02024_records.inc"
};

static u16 _gActor160700Animation02024Indices[20] = {
#include "assets/actor_160700_animation_02024_indices.inc"
};

static AnimationSet _gActor160700Animation02024 = {
    _gActor160700Animation02024Records,
    _gActor160700Animation02024Indices,
    { NULL, _gActor160700Animation02024Bank1, NULL, NULL, _gActor160700Animation02024Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02284Bank1[3] = {
#include "assets/actor_160700_animation_02284_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02284Bank4[26] = {
#include "assets/actor_160700_animation_02284_bank4.inc"
};

static AnimationRecord _gActor160700Animation02284Records[97] = {
#include "assets/actor_160700_animation_02284_records.inc"
};

static u16 _gActor160700Animation02284Indices[20] = {
#include "assets/actor_160700_animation_02284_indices.inc"
};

static AnimationSet _gActor160700Animation02284 = {
    _gActor160700Animation02284Records,
    _gActor160700Animation02284Indices,
    { NULL, _gActor160700Animation02284Bank1, NULL, NULL, _gActor160700Animation02284Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02454Bank1[3] = {
#include "assets/actor_160700_animation_02454_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02454Bank4[30] = {
#include "assets/actor_160700_animation_02454_bank4.inc"
};

static AnimationRecord _gActor160700Animation02454Records[57] = {
#include "assets/actor_160700_animation_02454_records.inc"
};

static u16 _gActor160700Animation02454Indices[20] = {
#include "assets/actor_160700_animation_02454_indices.inc"
};

static AnimationSet _gActor160700Animation02454 = {
    _gActor160700Animation02454Records,
    _gActor160700Animation02454Indices,
    { NULL, _gActor160700Animation02454Bank1, NULL, NULL, _gActor160700Animation02454Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02714Bank1[5] = {
#include "assets/actor_160700_animation_02714_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02714Bank4[54] = {
#include "assets/actor_160700_animation_02714_bank4.inc"
};

static AnimationRecord _gActor160700Animation02714Records[87] = {
#include "assets/actor_160700_animation_02714_records.inc"
};

static u16 _gActor160700Animation02714Indices[20] = {
#include "assets/actor_160700_animation_02714_indices.inc"
};

static AnimationSet _gActor160700Animation02714 = {
    _gActor160700Animation02714Records,
    _gActor160700Animation02714Indices,
    { NULL, _gActor160700Animation02714Bank1, NULL, NULL, _gActor160700Animation02714Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation029ACBank1[3] = {
#include "assets/actor_160700_animation_029AC_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation029ACBank4[28] = {
#include "assets/actor_160700_animation_029AC_bank4.inc"
};

static AnimationRecord _gActor160700Animation029ACRecords[109] = {
#include "assets/actor_160700_animation_029AC_records.inc"
};

static u16 _gActor160700Animation029ACIndices[20] = {
#include "assets/actor_160700_animation_029AC_indices.inc"
};

static AnimationSet _gActor160700Animation029AC = {
    _gActor160700Animation029ACRecords,
    _gActor160700Animation029ACIndices,
    { NULL, _gActor160700Animation029ACBank1, NULL, NULL, _gActor160700Animation029ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02BD4Bank1[2] = {
#include "assets/actor_160700_animation_02BD4_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02BD4Bank4[23] = {
#include "assets/actor_160700_animation_02BD4_bank4.inc"
};

static AnimationRecord _gActor160700Animation02BD4Records[89] = {
#include "assets/actor_160700_animation_02BD4_records.inc"
};

static u16 _gActor160700Animation02BD4Indices[20] = {
#include "assets/actor_160700_animation_02BD4_indices.inc"
};

static AnimationSet _gActor160700Animation02BD4 = {
    _gActor160700Animation02BD4Records,
    _gActor160700Animation02BD4Indices,
    { NULL, _gActor160700Animation02BD4Bank1, NULL, NULL, _gActor160700Animation02BD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02D7CBank1[2] = {
#include "assets/actor_160700_animation_02D7C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02D7CBank4[23] = {
#include "assets/actor_160700_animation_02D7C_bank4.inc"
};

static AnimationRecord _gActor160700Animation02D7CRecords[57] = {
#include "assets/actor_160700_animation_02D7C_records.inc"
};

static u16 _gActor160700Animation02D7CIndices[20] = {
#include "assets/actor_160700_animation_02D7C_indices.inc"
};

static AnimationSet _gActor160700Animation02D7C = {
    _gActor160700Animation02D7CRecords,
    _gActor160700Animation02D7CIndices,
    { NULL, _gActor160700Animation02D7CBank1, NULL, NULL, _gActor160700Animation02D7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation02F38Bank1[3] = {
#include "assets/actor_160700_animation_02F38_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation02F38Bank4[19] = {
#include "assets/actor_160700_animation_02F38_bank4.inc"
};

static AnimationRecord _gActor160700Animation02F38Records[63] = {
#include "assets/actor_160700_animation_02F38_records.inc"
};

static u16 _gActor160700Animation02F38Indices[20] = {
#include "assets/actor_160700_animation_02F38_indices.inc"
};

static AnimationSet _gActor160700Animation02F38 = {
    _gActor160700Animation02F38Records,
    _gActor160700Animation02F38Indices,
    { NULL, _gActor160700Animation02F38Bank1, NULL, NULL, _gActor160700Animation02F38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation030F4Bank1[3] = {
#include "assets/actor_160700_animation_030F4_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation030F4Bank4[19] = {
#include "assets/actor_160700_animation_030F4_bank4.inc"
};

static AnimationRecord _gActor160700Animation030F4Records[63] = {
#include "assets/actor_160700_animation_030F4_records.inc"
};

static u16 _gActor160700Animation030F4Indices[20] = {
#include "assets/actor_160700_animation_030F4_indices.inc"
};

static AnimationSet _gActor160700Animation030F4 = {
    _gActor160700Animation030F4Records,
    _gActor160700Animation030F4Indices,
    { NULL, _gActor160700Animation030F4Bank1, NULL, NULL, _gActor160700Animation030F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation033A0Bank1[3] = {
#include "assets/actor_160700_animation_033A0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation033A0Bank4[55] = {
#include "assets/actor_160700_animation_033A0_bank4.inc"
};

static AnimationRecord _gActor160700Animation033A0Records[87] = {
#include "assets/actor_160700_animation_033A0_records.inc"
};

static u16 _gActor160700Animation033A0Indices[20] = {
#include "assets/actor_160700_animation_033A0_indices.inc"
};

static AnimationSet _gActor160700Animation033A0 = {
    _gActor160700Animation033A0Records,
    _gActor160700Animation033A0Indices,
    { NULL, _gActor160700Animation033A0Bank1, NULL, NULL, _gActor160700Animation033A0Bank4, NULL, NULL, NULL },
};

/// Meeting clip counts, serialized playback choices and task priorities.
enum {
    ACTOR_160700_PLAYER_MEETING_CLIP_COUNT = 19,
    ACTOR_160700_PLAYER_MEETING_NULL_COUNT = 4,
    ACTOR_160700_PLAYER_MEETING_SET_COUNT  = ACTOR_160700_PLAYER_MEETING_CLIP_COUNT + ACTOR_160700_PLAYER_MEETING_NULL_COUNT,
    ACTOR_160700_REQUEST_BANK_INDEX        = 1,
    ACTOR_160700_PLAYER_BASE_CLIP          = 1,
    ACTOR_160700_PLAYER_BLEND_FRAMES       = 15,
    ACTOR_160700_PIERCE_CLIP_2             = 2,
    ACTOR_160700_PIERCE_CLIP_6             = 6,
    ACTOR_160700_PIERCE_CLIP_7             = 7,
    ACTOR_160700_PIERCE_CLIP_11            = 11,
    ACTOR_160700_PIERCE_CLIP_18            = 18,
    ACTOR_160700_PIERCE_CLIP_19            = 19,
    ACTOR_160700_PIERCE_CLIP_20            = 20,
    ACTOR_160700_PIERCE_TASK_PRIORITY      = 96,
    ACTOR_160700_CARRIED_MODEL_PRIORITY    = 192,
};

/// Player meeting clips and playback requests in one bank-extension copy source.
///
/// A 32-word copy installs clips at bank IDs 47..65, four NULL entries at
/// 66..69, and request words at 70..78. Those last nine words are data, not clips:
/// they comprise `bankCopy`, `resetRequest`, and `blendRequest` through its
/// animation ID. The complete record holds 35 words, so the transfer is in bounds.
/// Scripts play base clip 1 or meeting clips 47..65 and resolve the weapon-bank
/// selector in a temporary request. Clip storage stays loaded during playback.
typedef struct {
    AnimationSet*            clips[ACTOR_160700_PLAYER_MEETING_SET_COUNT]; // Bank ids 47..65, then four NULL entries
    AnimationBankCopyRequest bankCopy;                                     // Copies the first 32 words, including this request, into the player bank
    AnimationPlayRequest     resetRequest;                                 // Restarts base clip 1; disables world-grid participation
    AnimationPlayRequest     blendRequest;                                 // Returns to base clip 1 with a 15-frame blend; disables world-grid participation
} _Actor160700PlayerMeetingAnimationData;
STATIC_ASSERT_SIZEOF(_Actor160700PlayerMeetingAnimationData, 0x8C);
STATIC_ASSERT(OFFSET_OF(_Actor160700PlayerMeetingAnimationData, blendRequest) + OFFSET_OF(AnimationPlayRequest, blend) == ANIMATION_BANK_EXTENSION_CAPACITY * sizeof(s32), Actor160700MeetingCopyEnd);

/// Player animation storage borrowed by all four Pierce meeting scripts.
///
/// The bank copy reads this record's raw representation, including its request
/// fields. Keep its field order and clip data loaded while the player uses this
/// extension; dispatch borrows the requests only through the call. The final
/// three words are outside the copied span.
static _Actor160700PlayerMeetingAnimationData _gActor160700PlayerMeetingAnimationData = {
    { &_gActor160700Animation00C90, &_gActor160700Animation00EC0, &_gActor160700Animation01184, &_gActor160700Animation0135C, &_gActor160700Animation01530, &_gActor160700Animation017A0, &_gActor160700Animation01A4C, &_gActor160700Animation01C18, &_gActor160700Animation01E54, &_gActor160700Animation02024, &_gActor160700Animation02284, &_gActor160700Animation02454, &_gActor160700Animation02714, &_gActor160700Animation029AC, &_gActor160700Animation02BD4, &_gActor160700Animation02D7C, &_gActor160700Animation02F38, &_gActor160700Animation030F4, &_gActor160700Animation033A0, NULL, NULL, NULL, NULL },
    { { .words = (const s32*)&_gActor160700PlayerMeetingAnimationData }, ANIMATION_BANK_EXTENSION_CAPACITY },
    { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PLAYER_BASE_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PLAYER_BASE_CLIP, ANIMATION_BLEND_INTERPOLATE, ACTOR_160700_PLAYER_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Clip indices selected by Pierce's meeting requests.
enum {
    ACTOR_160700_PIERCE_CLIP_3  = 3,
    ACTOR_160700_PIERCE_CLIP_4  = 4,
    ACTOR_160700_PIERCE_CLIP_5  = 5,
    ACTOR_160700_PIERCE_CLIP_8  = 8,
    ACTOR_160700_PIERCE_CLIP_9  = 9,
    ACTOR_160700_PIERCE_CLIP_10 = 10,
    ACTOR_160700_PIERCE_CLIP_12 = 12,
    ACTOR_160700_PIERCE_CLIP_13 = 13,
    ACTOR_160700_PIERCE_CLIP_14 = 14,
    ACTOR_160700_PIERCE_CLIP_15 = 15,
    ACTOR_160700_PIERCE_CLIP_16 = 16,
    ACTOR_160700_PIERCE_CLIP_17 = 17,
    ACTOR_160700_PIERCE_CLIP_21 = 21,
    ACTOR_160700_PIERCE_CLIP_22 = 22,
    ACTOR_160700_PIERCE_CLIP_23 = 23,
};

/// Unreferenced reset request for clip 47 with world-grid participation disabled.
///
/// Stores bank selector 1. Its recipient is unproven; retain this separate
/// instance even though the live player clip-47 request has identical values.
static AnimationPlayRequest _gActor160700UnusedClip47Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 47 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 0, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip47Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 48 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 1, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip48Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 49 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 2, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip49Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 50 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 3, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip50Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 51 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 4, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip51Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 52 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 5, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip52Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 53 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 6, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip53Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 54 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 7, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip54Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 55 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 8, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip55Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 56 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 9, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip56Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 57 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 10, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip57Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 58 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 11, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip58Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 59 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 12, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip59Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 60 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 13, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip60Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 61 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 14, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip61Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 62 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 15, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip62Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 63 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 16, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip63Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 64 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 17, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip64Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts player meeting clip 65 with world-grid participation disabled.
///
/// The event runtime resolves the equipped-weapon bank in a copy of this
/// request; the clip is meeting-table entry 18, borrowed from this overlay.
static AnimationPlayRequest _gActor160700PlayerClip65Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ANIMATION_BANK_BASE_SET_COUNT + 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Unreferenced reset requests for clip IDs 66..69, 0 and 1.
///
/// Every record stores bank selector 1 and disables world-grid participation.
/// Their recipient and original grouping are unproven; retain the full run.
static AnimationPlayRequest _gActor160700UnusedClipRequests[] = {
    { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, 66, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, 67, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, 68, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, 69, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Restarts Pierce's meeting clip 2 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip2Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 3 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip3Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 4 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip4Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 5 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip5Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 6, including his post-meeting restoration.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip6Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 7 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip7Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 8 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip8Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 9 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip9Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 10 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip10Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 11 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip11Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 12 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip12Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 13 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip13Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 14 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip14Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 15 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip15Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 16 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip16Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 17 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip17Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 18 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip18Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 19 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip19Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 20 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip20Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 21 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip21Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 22 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip22Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Pierce's meeting clip 23 on the placed paced walker.
///
/// The receiver uses its own clip table and ignores the bank and collision
/// words. Dispatch consumes this request synchronously; the clips stay loaded.
static AnimationPlayRequest _gActor160700PierceClip23Request = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PIERCE_CLIP_23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player placement used by the first two meetings and their common skip script.
///
/// Position (2250, 0, 790) uses whole room-coordinate units; yaw -853 uses
/// 4096 units per turn, with zero pitch and roll. The placement message borrows
/// all six meaningful components synchronously and retains no pointer here.
static ActorTransform _gActor160700PlayerMeetingTransform = { { 2250, 0, 790, 0 }, { 0, -853, 0, 0 } };

/// Player placement used midway through the second Pierce meeting.
///
/// Position (2000, 0, 750) uses whole room-coordinate units; yaw -739 uses
/// 4096 units per turn, with zero pitch and roll. The player's placement
/// message consumes the transform synchronously without retaining its address.
static ActorTransform _gActor160700PlayerSecondMeetingTransform = { { 2000, 0, 750, 0 }, { 0, -739, 0, 0 } };

/// CAP command slots, placed-actor index and restoration choices of these scripts.
enum {
    ACTOR_160700_FIRST_MEETING_CAP_COMMAND  = 3,
    ACTOR_160700_SECOND_MEETING_CAP_COMMAND = 4,
    ACTOR_160700_THIRD_MEETING_CAP_COMMAND  = 5,
    ACTOR_160700_REPEAT_MEETING_CAP_COMMAND = 6,
    ACTOR_160700_MEETING_ACTOR_INDEX        = 0,
    ACTOR_160700_POST_MEETING_VIEW          = 3,
    ACTOR_160700_USE_DEFAULT_FADE_DURATION  = 0, // Runtime selects its seven-frame ramp
};

/// First Pierce meeting choreography, synchronized to CAP command 3.
///
/// Installs the player meeting-bank extension and cues player and placed actor
/// 0's clips. Restores view 3 and normal player control at the end. The command
/// stream, payloads and clip storage stay loaded through playback and recovery.
static EvsCommand _gActor160700FirstMeetingScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_160700_FIRST_MEETING_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160700PlayerMeetingAnimationData.bankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingAnimationData.resetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160700PlayerMeetingTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip22Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip23Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_160700_POST_MEETING_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingAnimationData.resetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Shared recovery when either of the first two Pierce meetings is skipped.
///
/// Fades over scene cleanup, restores view 3 and the common player placement,
/// resets the player and Pierce to their resting clips, then returns the fade
/// and player control. Borrows the already installed meeting-bank extension;
/// this stream and its payloads stay loaded until the terminating command.
static EvsCommand _gActor160700MeetingSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_160700_USE_DEFAULT_FADE_DURATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_160700_POST_MEETING_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160700PlayerMeetingTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingAnimationData.resetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Second Pierce meeting choreography, synchronized to CAP command 4.
///
/// Installs the player meeting-bank extension, cues both actors and moves the
/// player during a secondary fade. Restores view 3 and normal player control
/// at the end. Keep commands, payloads and clip storage loaded through recovery.
static EvsCommand _gActor160700SecondMeetingScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_160700_SECOND_MEETING_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160700PlayerMeetingAnimationData.bankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingAnimationData.resetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160700PlayerMeetingTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip17Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip59Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160700PlayerSecondMeetingTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip60Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip63Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip21Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerClip65Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_160700_POST_MEETING_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingAnimationData.resetRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Third Pierce meeting choreography, synchronized to CAP command 5.
///
/// Installs the player meeting-bank extension and blends to the player's base
/// idle while cueing placed actor 0. Finishes with Pierce clip 6 and normal
/// player control. The command stream and borrowed clip storage stay loaded.
static EvsCommand _gActor160700ThirdMeetingScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_160700_THIRD_MEETING_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160700PlayerMeetingAnimationData.bankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingAnimationData.blendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Repeat Pierce conversation choreography, synchronized to CAP command 6.
///
/// Installs the player meeting-bank extension and blends to the player's base
/// idle while cueing placed actor 0. Finishes with Pierce clip 6 and normal
/// player control. The command stream and borrowed clip storage stay loaded.
static EvsCommand _gActor160700RepeatMeetingScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_160700_REPEAT_MEETING_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160700PlayerMeetingAnimationData.bankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingAnimationData.blendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor160700PierceCarradineBodySkeleton[20] = {
#include "assets/pierce_carradine_body_skeleton.inc"
};

static u32 _gActor160700PierceCarradineBodyPartVerts[20] = {
#include "assets/pierce_carradine_body_partVerts.inc"
};

static SVECTOR _gActor160700PierceCarradineBodyVerts[390] = {
#include "assets/pierce_carradine_body_verts.inc"
};

static SVECTOR _gActor160700PierceCarradineBodyNormals[407] = {
#include "assets/pierce_carradine_body_normals.inc"
};

static u32 _gActor160700PierceCarradineBodyStream[4476] = {
#include "assets/pierce_carradine_body_stream.inc"
};

static TmdSource _gActor160700PierceCarradineBody = {
    0,
    24444,
    6776,
    20,
    _gActor160700PierceCarradineBodyPartVerts,
    _gActor160700PierceCarradineBodyVerts,
    _gActor160700PierceCarradineBodyNormals,
    _gActor160700PierceCarradineBodySkeleton,
    _gActor160700PierceCarradineBodyStream,
};

static TmdBone _gActor160700Actor113100Model07960Skeleton[1] = {
#include "assets/actor_113100_model_07960_skeleton.inc"
};

static u32 _gActor160700Actor113100Model07960PartVerts[1] = {
#include "assets/actor_113100_model_07960_partVerts.inc"
};

static SVECTOR _gActor160700Actor113100Model07960Verts[14] = {
#include "assets/actor_113100_model_07960_verts.inc"
};

static SVECTOR _gActor160700Actor113100Model07960Normals[12] = {
#include "assets/actor_113100_model_07960_normals.inc"
};

static u32 _gActor160700Actor113100Model07960Stream[56] = {
#include "assets/actor_113100_model_07960_stream.inc"
};

static TmdSource _gActor160700Actor113100Model07960 = {
    0,
    340,
    0,
    1,
    _gActor160700Actor113100Model07960PartVerts,
    _gActor160700Actor113100Model07960Verts,
    _gActor160700Actor113100Model07960Normals,
    _gActor160700Actor113100Model07960Skeleton,
    _gActor160700Actor113100Model07960Stream,
};

static AnimationPackedPose _gActor160700Animation0AD1CBank1[2] = {
#include "assets/actor_160700_animation_0AD1C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0AD1CBank4[22] = {
#include "assets/actor_160700_animation_0AD1C_bank4.inc"
};

static AnimationRecord _gActor160700Animation0AD1CRecords[98] = {
#include "assets/actor_160700_animation_0AD1C_records.inc"
};

static u16 _gActor160700Animation0AD1CIndices[20] = {
#include "assets/actor_160700_animation_0AD1C_indices.inc"
};

static AnimationSet _gActor160700Animation0AD1C = {
    _gActor160700Animation0AD1CRecords,
    _gActor160700Animation0AD1CIndices,
    { NULL, _gActor160700Animation0AD1CBank1, NULL, NULL, _gActor160700Animation0AD1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0AFECBank1[2] = {
#include "assets/actor_160700_animation_0AFEC_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0AFECBank4[48] = {
#include "assets/actor_160700_animation_0AFEC_bank4.inc"
};

static AnimationRecord _gActor160700Animation0AFECRecords[106] = {
#include "assets/actor_160700_animation_0AFEC_records.inc"
};

static u16 _gActor160700Animation0AFECIndices[20] = {
#include "assets/actor_160700_animation_0AFEC_indices.inc"
};

static AnimationSet _gActor160700Animation0AFEC = {
    _gActor160700Animation0AFECRecords,
    _gActor160700Animation0AFECIndices,
    { NULL, _gActor160700Animation0AFECBank1, NULL, NULL, _gActor160700Animation0AFECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0C104Bank1[30] = {
#include "assets/actor_160700_animation_0C104_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0C104Bank4[439] = {
#include "assets/actor_160700_animation_0C104_bank4.inc"
};

static AnimationRecord _gActor160700Animation0C104Records[545] = {
#include "assets/actor_160700_animation_0C104_records.inc"
};

static u16 _gActor160700Animation0C104Indices[20] = {
#include "assets/actor_160700_animation_0C104_indices.inc"
};

static AnimationSet _gActor160700Animation0C104 = {
    _gActor160700Animation0C104Records,
    _gActor160700Animation0C104Indices,
    { NULL, _gActor160700Animation0C104Bank1, NULL, NULL, _gActor160700Animation0C104Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0C3F4Bank1[4] = {
#include "assets/actor_160700_animation_0C3F4_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0C3F4Bank4[34] = {
#include "assets/actor_160700_animation_0C3F4_bank4.inc"
};

static AnimationRecord _gActor160700Animation0C3F4Records[122] = {
#include "assets/actor_160700_animation_0C3F4_records.inc"
};

static u16 _gActor160700Animation0C3F4Indices[20] = {
#include "assets/actor_160700_animation_0C3F4_indices.inc"
};

static AnimationSet _gActor160700Animation0C3F4 = {
    _gActor160700Animation0C3F4Records,
    _gActor160700Animation0C3F4Indices,
    { NULL, _gActor160700Animation0C3F4Bank1, NULL, NULL, _gActor160700Animation0C3F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0C5D0Bank1[3] = {
#include "assets/actor_160700_animation_0C5D0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0C5D0Bank4[30] = {
#include "assets/actor_160700_animation_0C5D0_bank4.inc"
};

static AnimationRecord _gActor160700Animation0C5D0Records[60] = {
#include "assets/actor_160700_animation_0C5D0_records.inc"
};

static u16 _gActor160700Animation0C5D0Indices[20] = {
#include "assets/actor_160700_animation_0C5D0_indices.inc"
};

static AnimationSet _gActor160700Animation0C5D0 = {
    _gActor160700Animation0C5D0Records,
    _gActor160700Animation0C5D0Indices,
    { NULL, _gActor160700Animation0C5D0Bank1, NULL, NULL, _gActor160700Animation0C5D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0C858Bank1[3] = {
#include "assets/actor_160700_animation_0C858_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0C858Bank4[29] = {
#include "assets/actor_160700_animation_0C858_bank4.inc"
};

static AnimationRecord _gActor160700Animation0C858Records[104] = {
#include "assets/actor_160700_animation_0C858_records.inc"
};

static u16 _gActor160700Animation0C858Indices[20] = {
#include "assets/actor_160700_animation_0C858_indices.inc"
};

static AnimationSet _gActor160700Animation0C858 = {
    _gActor160700Animation0C858Records,
    _gActor160700Animation0C858Indices,
    { NULL, _gActor160700Animation0C858Bank1, NULL, NULL, _gActor160700Animation0C858Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0CA8CBank1[3] = {
#include "assets/actor_160700_animation_0CA8C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0CA8CBank4[36] = {
#include "assets/actor_160700_animation_0CA8C_bank4.inc"
};

static AnimationRecord _gActor160700Animation0CA8CRecords[76] = {
#include "assets/actor_160700_animation_0CA8C_records.inc"
};

static u16 _gActor160700Animation0CA8CIndices[20] = {
#include "assets/actor_160700_animation_0CA8C_indices.inc"
};

static AnimationSet _gActor160700Animation0CA8C = {
    _gActor160700Animation0CA8CRecords,
    _gActor160700Animation0CA8CIndices,
    { NULL, _gActor160700Animation0CA8CBank1, NULL, NULL, _gActor160700Animation0CA8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0CC70Bank1[3] = {
#include "assets/actor_160700_animation_0CC70_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0CC70Bank4[32] = {
#include "assets/actor_160700_animation_0CC70_bank4.inc"
};

static AnimationRecord _gActor160700Animation0CC70Records[60] = {
#include "assets/actor_160700_animation_0CC70_records.inc"
};

static u16 _gActor160700Animation0CC70Indices[20] = {
#include "assets/actor_160700_animation_0CC70_indices.inc"
};

static AnimationSet _gActor160700Animation0CC70 = {
    _gActor160700Animation0CC70Records,
    _gActor160700Animation0CC70Indices,
    { NULL, _gActor160700Animation0CC70Bank1, NULL, NULL, _gActor160700Animation0CC70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0D310Bank1[8] = {
#include "assets/actor_160700_animation_0D310_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0D310Bank4[120] = {
#include "assets/actor_160700_animation_0D310_bank4.inc"
};

static AnimationRecord _gActor160700Animation0D310Records[260] = {
#include "assets/actor_160700_animation_0D310_records.inc"
};

static u16 _gActor160700Animation0D310Indices[20] = {
#include "assets/actor_160700_animation_0D310_indices.inc"
};

static AnimationSet _gActor160700Animation0D310 = {
    _gActor160700Animation0D310Records,
    _gActor160700Animation0D310Indices,
    { NULL, _gActor160700Animation0D310Bank1, NULL, NULL, _gActor160700Animation0D310Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0D638Bank1[5] = {
#include "assets/actor_160700_animation_0D638_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0D638Bank4[70] = {
#include "assets/actor_160700_animation_0D638_bank4.inc"
};

static AnimationRecord _gActor160700Animation0D638Records[97] = {
#include "assets/actor_160700_animation_0D638_records.inc"
};

static u16 _gActor160700Animation0D638Indices[20] = {
#include "assets/actor_160700_animation_0D638_indices.inc"
};

static AnimationSet _gActor160700Animation0D638 = {
    _gActor160700Animation0D638Records,
    _gActor160700Animation0D638Indices,
    { NULL, _gActor160700Animation0D638Bank1, NULL, NULL, _gActor160700Animation0D638Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0D980Bank1[3] = {
#include "assets/actor_160700_animation_0D980_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0D980Bank4[47] = {
#include "assets/actor_160700_animation_0D980_bank4.inc"
};

static AnimationRecord _gActor160700Animation0D980Records[134] = {
#include "assets/actor_160700_animation_0D980_records.inc"
};

static u16 _gActor160700Animation0D980Indices[20] = {
#include "assets/actor_160700_animation_0D980_indices.inc"
};

static AnimationSet _gActor160700Animation0D980 = {
    _gActor160700Animation0D980Records,
    _gActor160700Animation0D980Indices,
    { NULL, _gActor160700Animation0D980Bank1, NULL, NULL, _gActor160700Animation0D980Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0DC18Bank1[3] = {
#include "assets/actor_160700_animation_0DC18_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0DC18Bank4[38] = {
#include "assets/actor_160700_animation_0DC18_bank4.inc"
};

static AnimationRecord _gActor160700Animation0DC18Records[99] = {
#include "assets/actor_160700_animation_0DC18_records.inc"
};

static u16 _gActor160700Animation0DC18Indices[20] = {
#include "assets/actor_160700_animation_0DC18_indices.inc"
};

static AnimationSet _gActor160700Animation0DC18 = {
    _gActor160700Animation0DC18Records,
    _gActor160700Animation0DC18Indices,
    { NULL, _gActor160700Animation0DC18Bank1, NULL, NULL, _gActor160700Animation0DC18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0DF38Bank1[4] = {
#include "assets/actor_160700_animation_0DF38_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0DF38Bank4[38] = {
#include "assets/actor_160700_animation_0DF38_bank4.inc"
};

static AnimationRecord _gActor160700Animation0DF38Records[130] = {
#include "assets/actor_160700_animation_0DF38_records.inc"
};

static u16 _gActor160700Animation0DF38Indices[20] = {
#include "assets/actor_160700_animation_0DF38_indices.inc"
};

static AnimationSet _gActor160700Animation0DF38 = {
    _gActor160700Animation0DF38Records,
    _gActor160700Animation0DF38Indices,
    { NULL, _gActor160700Animation0DF38Bank1, NULL, NULL, _gActor160700Animation0DF38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0E1D8Bank1[2] = {
#include "assets/actor_160700_animation_0E1D8_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0E1D8Bank4[54] = {
#include "assets/actor_160700_animation_0E1D8_bank4.inc"
};

static AnimationRecord _gActor160700Animation0E1D8Records[88] = {
#include "assets/actor_160700_animation_0E1D8_records.inc"
};

static u16 _gActor160700Animation0E1D8Indices[20] = {
#include "assets/actor_160700_animation_0E1D8_indices.inc"
};

static AnimationSet _gActor160700Animation0E1D8 = {
    _gActor160700Animation0E1D8Records,
    _gActor160700Animation0E1D8Indices,
    { NULL, _gActor160700Animation0E1D8Bank1, NULL, NULL, _gActor160700Animation0E1D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0E4E4Bank1[6] = {
#include "assets/actor_160700_animation_0E4E4_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0E4E4Bank4[61] = {
#include "assets/actor_160700_animation_0E4E4_bank4.inc"
};

static AnimationRecord _gActor160700Animation0E4E4Records[96] = {
#include "assets/actor_160700_animation_0E4E4_records.inc"
};

static u16 _gActor160700Animation0E4E4Indices[20] = {
#include "assets/actor_160700_animation_0E4E4_indices.inc"
};

static AnimationSet _gActor160700Animation0E4E4 = {
    _gActor160700Animation0E4E4Records,
    _gActor160700Animation0E4E4Indices,
    { NULL, _gActor160700Animation0E4E4Bank1, NULL, NULL, _gActor160700Animation0E4E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0E8BCBank1[4] = {
#include "assets/actor_160700_animation_0E8BC_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0E8BCBank4[60] = {
#include "assets/actor_160700_animation_0E8BC_bank4.inc"
};

static AnimationRecord _gActor160700Animation0E8BCRecords[154] = {
#include "assets/actor_160700_animation_0E8BC_records.inc"
};

static u16 _gActor160700Animation0E8BCIndices[20] = {
#include "assets/actor_160700_animation_0E8BC_indices.inc"
};

static AnimationSet _gActor160700Animation0E8BC = {
    _gActor160700Animation0E8BCRecords,
    _gActor160700Animation0E8BCIndices,
    { NULL, _gActor160700Animation0E8BCBank1, NULL, NULL, _gActor160700Animation0E8BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0EA74Bank1[2] = {
#include "assets/actor_160700_animation_0EA74_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0EA74Bank4[24] = {
#include "assets/actor_160700_animation_0EA74_bank4.inc"
};

static AnimationRecord _gActor160700Animation0EA74Records[60] = {
#include "assets/actor_160700_animation_0EA74_records.inc"
};

static u16 _gActor160700Animation0EA74Indices[20] = {
#include "assets/actor_160700_animation_0EA74_indices.inc"
};

static AnimationSet _gActor160700Animation0EA74 = {
    _gActor160700Animation0EA74Records,
    _gActor160700Animation0EA74Indices,
    { NULL, _gActor160700Animation0EA74Bank1, NULL, NULL, _gActor160700Animation0EA74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0EC58Bank1[3] = {
#include "assets/actor_160700_animation_0EC58_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0EC58Bank4[32] = {
#include "assets/actor_160700_animation_0EC58_bank4.inc"
};

static AnimationRecord _gActor160700Animation0EC58Records[60] = {
#include "assets/actor_160700_animation_0EC58_records.inc"
};

static u16 _gActor160700Animation0EC58Indices[20] = {
#include "assets/actor_160700_animation_0EC58_indices.inc"
};

static AnimationSet _gActor160700Animation0EC58 = {
    _gActor160700Animation0EC58Records,
    _gActor160700Animation0EC58Indices,
    { NULL, _gActor160700Animation0EC58Bank1, NULL, NULL, _gActor160700Animation0EC58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0EF34Bank1[3] = {
#include "assets/actor_160700_animation_0EF34_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0EF34Bank4[35] = {
#include "assets/actor_160700_animation_0EF34_bank4.inc"
};

static AnimationRecord _gActor160700Animation0EF34Records[119] = {
#include "assets/actor_160700_animation_0EF34_records.inc"
};

static u16 _gActor160700Animation0EF34Indices[20] = {
#include "assets/actor_160700_animation_0EF34_indices.inc"
};

static AnimationSet _gActor160700Animation0EF34 = {
    _gActor160700Animation0EF34Records,
    _gActor160700Animation0EF34Indices,
    { NULL, _gActor160700Animation0EF34Bank1, NULL, NULL, _gActor160700Animation0EF34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0F118Bank1[3] = {
#include "assets/actor_160700_animation_0F118_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0F118Bank4[32] = {
#include "assets/actor_160700_animation_0F118_bank4.inc"
};

static AnimationRecord _gActor160700Animation0F118Records[60] = {
#include "assets/actor_160700_animation_0F118_records.inc"
};

static u16 _gActor160700Animation0F118Indices[20] = {
#include "assets/actor_160700_animation_0F118_indices.inc"
};

static AnimationSet _gActor160700Animation0F118 = {
    _gActor160700Animation0F118Records,
    _gActor160700Animation0F118Indices,
    { NULL, _gActor160700Animation0F118Bank1, NULL, NULL, _gActor160700Animation0F118Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0F3D0Bank1[2] = {
#include "assets/actor_160700_animation_0F3D0_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0F3D0Bank4[56] = {
#include "assets/actor_160700_animation_0F3D0_bank4.inc"
};

static AnimationRecord _gActor160700Animation0F3D0Records[92] = {
#include "assets/actor_160700_animation_0F3D0_records.inc"
};

static u16 _gActor160700Animation0F3D0Indices[20] = {
#include "assets/actor_160700_animation_0F3D0_indices.inc"
};

static AnimationSet _gActor160700Animation0F3D0 = {
    _gActor160700Animation0F3D0Records,
    _gActor160700Animation0F3D0Indices,
    { NULL, _gActor160700Animation0F3D0Bank1, NULL, NULL, _gActor160700Animation0F3D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0F65CBank1[2] = {
#include "assets/actor_160700_animation_0F65C_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0F65CBank4[33] = {
#include "assets/actor_160700_animation_0F65C_bank4.inc"
};

static AnimationRecord _gActor160700Animation0F65CRecords[104] = {
#include "assets/actor_160700_animation_0F65C_records.inc"
};

static u16 _gActor160700Animation0F65CIndices[20] = {
#include "assets/actor_160700_animation_0F65C_indices.inc"
};

static AnimationSet _gActor160700Animation0F65C = {
    _gActor160700Animation0F65CRecords,
    _gActor160700Animation0F65CIndices,
    { NULL, _gActor160700Animation0F65CBank1, NULL, NULL, _gActor160700Animation0F65CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160700Animation0F830Bank1[2] = {
#include "assets/actor_160700_animation_0F830_bank1.inc"
};

static AnimationPackedRotation _gActor160700Animation0F830Bank4[23] = {
#include "assets/actor_160700_animation_0F830_bank4.inc"
};

static AnimationRecord _gActor160700Animation0F830Records[68] = {
#include "assets/actor_160700_animation_0F830_records.inc"
};

static u16 _gActor160700Animation0F830Indices[20] = {
#include "assets/actor_160700_animation_0F830_indices.inc"
};

static AnimationSet _gActor160700Animation0F830 = {
    _gActor160700Animation0F830Records,
    _gActor160700Animation0F830Indices,
    { NULL, _gActor160700Animation0F830Bank1, NULL, NULL, _gActor160700Animation0F830Bank4, NULL, NULL, NULL },
};

/// Synchronous message table for Pierce's paced walker.
///
/// Five handlers accept animation, paired draw flags, placement, ignored actor
/// commands and horizontal walk targets. The end marker makes other IDs return
/// zero. The task borrows this table and its callbacks for its whole lifetime.
static TaskMessageEntry _gActor160700PierceMessages[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pacedWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pacedWalkSetPairModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor160700IgnoreCommand },
    { ACTOR_MESSAGE_WALK_TO, _pacedWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor160700PierceCarradineTasks[ACTOR_160700_TASK_COUNT] = {
    { { { TASK_BODY_TMD, ACTOR_160700_PIERCE_TASK_PRIORITY } }, _actor160700PacedWalkerTask, { .model = &_gActor160700PierceCarradineBody } },
    { { { TASK_BODY_TMD, ACTOR_160700_CARRIED_MODEL_PRIORITY } }, _pacedWalkSubModelTask, { .model = &_gActor160700Actor113100Model07960 } },
};

/// Pierce's twenty-five-entry animation-set bank, indexed by clip ID.
///
/// IDs 1..23 hold loaded clips for the twenty-part rig; 0 and 24 are NULL and
/// must not be played. The animation context and its slots borrow this table
/// and the clip storage until walker teardown; keep the overlay loaded.
static AnimationSet* _gActor160700PierceAnimationSets[] = {
    NULL,
    &_gActor160700Animation0AD1C,
    &_gActor160700Animation0AFEC,
    &_gActor160700Animation0C104,
    &_gActor160700Animation0C3F4,
    &_gActor160700Animation0C5D0,
    &_gActor160700Animation0C858,
    &_gActor160700Animation0CA8C,
    &_gActor160700Animation0CC70,
    &_gActor160700Animation0D310,
    &_gActor160700Animation0D638,
    &_gActor160700Animation0D980,
    &_gActor160700Animation0DC18,
    &_gActor160700Animation0DF38,
    &_gActor160700Animation0E1D8,
    &_gActor160700Animation0E4E4,
    &_gActor160700Animation0E8BC,
    &_gActor160700Animation0EA74,
    &_gActor160700Animation0EC58,
    &_gActor160700Animation0EF34,
    &_gActor160700Animation0F118,
    &_gActor160700Animation0F3D0,
    &_gActor160700Animation0F65C,
    &_gActor160700Animation0F830,
    NULL,
};

static void _actor160700PacedWalkSpawn(Enemy* enemy, Task* task);

void actor160700RestoreMeetingAnimation(void)
{
    enum { ACTOR_160700_MEETING_PLACE_KEY = 0 };
    Task* actorTask;

    if (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) != ACTOR_160700_MEETING_UNMET) {
        actorTask = sceneFindPlacedActor(ACTOR_160700_MEETING_PLACE_KEY);
        if (actorTask != NULL) {
            TASK_MESSAGE_DISPATCH_POINTER(actorTask, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor160700PierceClip6Request, 0);
        }
    }
}

void actor160700StartMeetingScript(void)
{
    enum {
        ACTOR_160700_STORY_DIALOGUE_AFTER_FIRST_MEETING = 12,
    };

    // Progress advances when playback starts, including a skipped conversation.
    switch (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS)) {
        case ACTOR_160700_MEETING_UNMET:
            evsStartScriptWithSkip(_gActor160700FirstMeetingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor160700MeetingSkipScript);
            gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, ACTOR_160700_MEETING_FIRST_SEEN);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, ACTOR_160700_STORY_DIALOGUE_AFTER_FIRST_MEETING);
            break;
        case ACTOR_160700_MEETING_FIRST_SEEN:
            evsStartScriptWithSkip(_gActor160700SecondMeetingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor160700MeetingSkipScript);
            gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, ACTOR_160700_MEETING_SECOND_SEEN);
            break;
        case ACTOR_160700_MEETING_SECOND_SEEN:
            evsStartScript(_gActor160700ThirdMeetingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, ACTOR_160700_MEETING_THIRD_SEEN);
            break;
        case ACTOR_160700_MEETING_THIRD_SEEN:
            evsStartScript(_gActor160700RepeatMeetingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

/// Binds work-owned light/color matrices and samples cached root lighting.
///
/// Requires live model/work/root pointers with stable, side-effect-free
/// expressions; model, work and rootCoord occur repeatedly. sample is a
/// writable VECTOR lvalue, also repeated. The model borrows both matrices
/// until teardown. Reads workm without composition, with Y decreased by the
/// captured 800-unit sample offset, and initializes all three light rows.
/// The query consumes sample XYZ synchronously. Invoke within braces.
#define ACTOR_160700_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, sample)            \
    {                                                                                     \
        (model)->lightMtx = &(work)->light;                                               \
        (model)->colorMtx = &(work)->color;                                               \
        (sample).vx       = (rootCoord)->workm.t[0];                                      \
        (sample).vy       = (rootCoord)->workm.t[1] - ACTOR_160700_LIGHT_SAMPLE_Y_OFFSET; \
        (sample).vz       = (rootCoord)->workm.t[2];                                      \
        worldCoordSetModelLighting((model), &(sample), 0, ACTOR_160700_LIGHT_COUNT);      \
    }

/// Initializes the Pierce paced walker and creates its attachment task.
///
/// Allocates zeroed task-owned work, destroying the enemy on failure. Requires
/// a live model/Enemy and descriptor slot 1 to produce a live attachment. Parents
/// the untargetable root to the view, textures the attachment from the owner’s
/// area placement and adopts it into task teardown. The model borrows work-owned
/// lighting matrices and samples cached XYZ with Y minus 800, before composition.
/// Binds the loaded clip table, requests clip 1, installs messages/exit, updates
/// once and advances spawn state 0 to running state 1. Resources must outlive work.
static void _actor160700PacedWalkSpawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_160700_SPAWN_CLIP            = 1,
        ACTOR_160700_LIGHT_SAMPLE_Y_OFFSET = 800,
        ACTOR_160700_LIGHT_COUNT           = 3,
    };

    VECTOR         lightPosition;
    PacedWalkWork* work;
    PacedWalkWork* allocatedWork;
    GfxCoord*      rootCoord;
    TmdObject*     model;
    Enemy*         attachmentEnemy;

    model         = task->extra.tmd;
    rootCoord     = model->coords;
    allocatedWork = memCalloc(sizeof(PacedWalkWork), false);
    work          = allocatedWork;
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor160700Exit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->flags                     = 0;
    model->otOffset                  = 1;
    work->enemy                      = enemy;
    attachmentEnemy                  = enemySpawnFromTable(gActor160700PierceCarradineTasks, ACTOR_160700_TASK_CARRIED_MODEL, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(attachmentEnemy->task->extra.tmd, enemy);
    taskReparent(task, attachmentEnemy->task);
    work->pairTask  = attachmentEnemy->task;
    work->st.animId = ACTOR_160700_SPAWN_CLIP;
    ACTOR_160700_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, lightPosition);
    animationInitContext(&work->rig.anim, _gActor160700PierceAnimationSets, model,
                         work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = _gActor160700PierceMessages;
    _pacedWalkUpdate(task);
    task->state += 1;
}
#undef ACTOR_160700_INITIALIZE_MODEL_LIGHTING

#include "../../shared/walk/paced_walk_update.inc.c"

/// Dispatches Pierce's paced walker initialization or frame update.
///
/// Descriptor 0 requires a live Enemy in `spawnArg2.pointer`, a twenty-part
/// TMD body and state 0..1. State 0 allocates the paced-walk work and attachment;
/// state 1 composes, lights, moves and animates the model and draws its shadow.
/// Initialization may destroy the enemy and task on allocation failure.
/// The handler array has no bounds check; keep this overlay and clips loaded.
static void _actor160700PacedWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor160700PacedWalkSpawn,
        _actorRenderWalkerFrame,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER _pacedWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases this walker's enemy and tears down its task and adopted child model.
///
/// Requires the spawned task with its live `Enemy` in `spawnArg2.pointer`.
/// Task teardown releases the paced-walk work and applies the runtime's normal
/// or immediate body-release policy. Do not use the enemy or work afterwards.
static void _actor160700Exit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walk/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/walk/paced_walk_tick_anim.inc.c"

#include "../../shared/walk/paced_walk_reset_anim.inc.c"

#include "../../shared/walk/paced_walk_blend_anim.inc.c"

#include "../../shared/walk/paced_walk_apply_animation_request.inc.c"

/// Immediately reseeds the walker's non-root tracks from a scripted play request.
///
/// Requires a spawned TMD walker and live `PacedWalkWork` rig. The package's
/// loaded clips are IDs 1..23; its bank entries 0 and 24 are NULL. The retained
/// check rejects only IDs >=25, so callers must exclude negative and unloaded
/// IDs. The request is borrowed only through this call, while the rig's clip
/// storage must outlive playback on parts 1 through 19.
/// Nonzero `blend` captures the old poses and narrows `blendFrames` to a signed
/// halfword duration in whole normal-rate frames; 0..2047 keeps transition time
/// nonnegative. Reset ignores the duration. Travel is unchanged. Ignores the
/// request's bank selector and collision choice, message ID and second payload.
/// Returns 0 after reseeding or -1 for IDs >=25, without changing state.
static s32 _pacedWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    enum {
        PACED_WALK_ANIMATION_ID_LIMIT         = 25,
        PACED_WALK_ANIMATION_REQUEST_APPLIED  = 0,
        PACED_WALK_ANIMATION_REQUEST_REJECTED = -1,
    };
    PacedWalkWork* work;

    work = task->work;
    if (request->animationId < PACED_WALK_ANIMATION_ID_LIMIT) {
        _pacedWalkApplyAnimationRequest(work, request);
        _pacedWalkUpdate(task);
        return PACED_WALK_ANIMATION_REQUEST_APPLIED;
    }
    return PACED_WALK_ANIMATION_REQUEST_REJECTED;
}

/// Replaces the walker and its carried model's draw flags together.
///
/// Requires live TMD models on `task` and its `PacedWalkWork::pairTask`.
/// `ACTOR_MESSAGE_PAIR_SHOW` clears all flags; its absence replaces them with
/// active-draw exclusion. `ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER` adds that bit
/// to both models. Other request bits are ignored; neither allocates nor frees
/// buffers. The message ID and second payload are ignored. Returns 0.
static s32 _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 flags, s32 unusedArg)
{
    PacedWalkWork* work;
    TmdObject*     model;
    TmdObject*     pairModel;

    model     = task->extra.tmd;
    work      = task->work;
    pairModel = work->pairTask->extra.tmd;

    if (flags & ACTOR_MESSAGE_PAIR_SHOW) {
        model->flags     = 0;
        pairModel->flags = 0;
    } else {
        model->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        pairModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (flags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        model->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        pairModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/walk/paced_walk_place.inc.c"

/// Ignores actor commands and returns zero without changing the walker.
///
/// Keeps the task-message ABI for `ACTOR_COMMAND_MESSAGE_APPLY`; the receiver,
/// command pointer and other argument words are never read or retained.
static s32 _actor160700IgnoreCommand(Task* task, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArg)
{
    return 0;
}

#include "../../shared/walk/paced_walk_to.inc.c"

#include "../../shared/walk/paced_walk_attach_carried_model.inc.c"

/// Attaches the carried model to walker part 4 and refreshes its root composition.
///
/// Requires a live TMD child task parented under the spawned walker, whose
/// model has at least five coordinates and whose work is `PacedWalkWork`.
/// State 0 clears the child's model flags, borrows the parent's lighting
/// matrices and part-4 coordinate, then enters state 1. Both states mark the
/// child's root dirty; other states do nothing. The parent's model and work
/// must outlive the child, which belongs to its task teardown tree.
static void _pacedWalkSubModelTask(Task* task)
{
    enum {
        PACED_WALK_SUB_MODEL_ATTACH = 0,
        PACED_WALK_SUB_MODEL_FOLLOW = 1,
        PACED_WALK_ATTACHMENT_PART  = 4,
    };

    // Retain the original unused stack reservation.
    char           unusedStackFrame[0x10];
    Task*          parentTask      = task->parent;
    TmdObject*     model           = task->extra.tmd;
    GfxCoord*      rootCoord       = model->coords;
    GfxCoord*      attachmentCoord = &parentTask->extra.tmd->coords[PACED_WALK_ATTACHMENT_PART];
    PacedWalkWork* parentWork      = parentTask->work;

    switch (task->state) {
        case PACED_WALK_SUB_MODEL_ATTACH:
            _pacedWalkAttachCarriedModel(model, attachmentCoord, parentWork);
            task->state++;
            break;
        case PACED_WALK_SUB_MODEL_FOLLOW:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
