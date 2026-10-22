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

static TmdSource _gPierceBody;
static TmdSource _gPierceGlasses;
static s32       _pacedWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32       _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 flags, s32 unusedArg);
static s32       _actor160700IgnoreCommand(Task* task, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArg);
static void      _actor160700PacedWalkerTask(Task* task);
static void      _pacedWalkSubModelTask(Task* task);

#include "assets/rooms/shelter/b1_golem_freezer_1/pierce_clips_1.inc"

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

/// Player meeting clips, bank ids 47..65, then four NULL entries; borrowed by all four Pierce meeting scripts.
///
/// The 32-entry copy reads past these 23 into the copy request and play
/// requests that follow, through the blend request's animation id; those words
/// land in ids 70..78 and are never played. Scripts play base clip 1 or meeting
/// clips 47..65 and resolve the weapon-bank selector in a temporary request.
/// The clips stay loaded during playback.
#include "assets/rooms/shelter/b1_golem_freezer_1/player_meeting_bank.inc"

/// Installs the meeting clips; reads 32 entries from a 23-entry list.
static AnimationBankCopyRequest _gActor160700PlayerMeetingBankCopy = { _gActor160700PlayerMeetingSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Restarts base clip 1; disables world-grid participation.
static AnimationPlayRequest _gActor160700PlayerMeetingResetRequest = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PLAYER_BASE_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Returns to base clip 1 with a 15-frame blend; disables world-grid participation.
static AnimationPlayRequest _gActor160700PlayerMeetingBlendRequest = { { .index = ACTOR_160700_REQUEST_BANK_INDEX }, ACTOR_160700_PLAYER_BASE_CLIP, ANIMATION_BLEND_INTERPOLATE, ACTOR_160700_PLAYER_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160700PlayerMeetingBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingResetRequest }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingResetRequest }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingResetRequest }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160700PlayerMeetingBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingResetRequest }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingResetRequest }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160700PlayerMeetingBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingBlendRequest }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160700PlayerMeetingBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160700PlayerMeetingBlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_160700_MEETING_ACTOR_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160700PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/pierce/body.inc"

#include "assets/characters/pierce/glasses.inc"

#include "assets/rooms/shelter/b1_golem_freezer_1/pierce_clips_2.inc"

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
    { { { TASK_BODY_TMD, ACTOR_160700_PIERCE_TASK_PRIORITY } }, _actor160700PacedWalkerTask, { .model = &_gPierceBody } },
    { { { TASK_BODY_TMD, ACTOR_160700_CARRIED_MODEL_PRIORITY } }, _pacedWalkSubModelTask, { .model = &_gPierceGlasses } },
};

/// Pierce's twenty-five-entry animation-set bank, indexed by clip ID.
///
/// IDs 1..23 hold loaded clips for the twenty-part rig; 0 and 24 are NULL and
/// must not be played. The animation context and its slots borrow this table
/// and the clip storage until walker teardown; keep the overlay loaded.
#include "assets/rooms/shelter/b1_golem_freezer_1/pierce_bank.inc"

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
