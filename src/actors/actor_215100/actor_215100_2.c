#include "actors/actor_215100.h"
#include "actor_215100_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/strings.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/mist_shooting_gallery.h"
#include "../../shared/paced_walk.h"
#include "../../shared/cap_captions_types.h"

static CapCaptionCaretDelayStorage _gCapCaptionCaretDelayStorage;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _pacedWalkExit(Task* task);

/* cap captions instance: retain the original overlay symbols. */
static void func_actor_215100_8014C538(s16 arg0, s16 arg1, s16 arg2);
void        func_actor_215100_8014C5E0(s16 arg0, s16 arg1, s16 arg2);
// The gallery selects and draws captions using this actor's loaded CAP state.
#define CAP_CAPTION_SELECT_SCRIPT_LINKAGE
/// Binds shared record selection to this actor's exported selector.
///
/// Define before `cap_captions.h` and retain through both caption source fragments.
/// The empty linkage binding exports the s32(s16, s16, s32) instance declared in
/// `actors/actor_215100.h`. This object-like alias captures no arguments and
/// constructs no tokens.
#define CAP_CAPTION_SELECT_RECORD actor215100CapCaptionSelectRecord
#define CAP_CAPTION_DRAW_CURRENT_LINKAGE
/// Binds shared caption drawing to this actor's exported void(void) instance.
///
/// Define before cap_captions.h and retain through cap_captions.inc.c. The
/// empty linkage binding exports the copy called by mist_shooting_gallery.
/// This object-like alias captures no arguments and constructs no tokens.
#define CAP_CAPTION_DRAW_CURRENT actor215100CapCaptionDrawCurrent
#include "../../shared/cap_captions.h"
#include "../../shared/walker.h"

extern TaskDesc   D_actor_215100_8014E13C[];
extern EvsCommand D_actor_215100_8014E370[];
extern EvsCommand D_actor_215100_8014E8F8[];
extern EvsCommand D_actor_215100_8014EA90[];
extern EvsCommand D_actor_215100_8014EB08[];

static TaskDesc CapCaption_Data_801544FC;
static TaskDesc CapCaption_Data_80154508;
extern Task*    D_actor_215100_8015E64C;

extern EvsCommand    D_actor_215100_80153ED4[];
extern EvsCommand    D_actor_215100_80153FDC[];
extern EvsCommand    D_actor_215100_801543E4[];
extern TaskDesc      D_actor_215100_8015E5D0[];
extern AnimationSet* D_actor_215100_8015E5E8[25];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry     D_actor_215100_8015E5A0[];
static const TextGlyphCell* _gCapCaptionGlyphCells;

/// Caption script table.
static CapCommandRef*     CapCaption_Data_8015E650;
static CapSequenceRecord* _gCapCaptionSequence;
static s16                _gCapCaptionBlockLeftX;
static s16                _gCapCaptionFirstBaselineY;
static s16                _gCapCaptionBottomBaselineY;
static s16                _gCapCaptionRecordIndex;
static s16                _gCapCaptionBlockHeight;
static s16                CapCaption_Data_8015E666;
static s32                _gCapCaptionCaretPulseLevel;
static s32                _gCapCaptionCaretPulseFalling;
static u16                _gCapCaptionCaretLeftX;
static u16                _gCapCaptionCaretTipY;
static s16                _gCapCaptionTexturePageX;
static s16                _gCapCaptionTexturePageY;
extern RoomEventMsg       D_actor_215100_8015E678;
/// Caption schedule `func_actor_215100_8014AFAC` scans, terminated by an
/// `upper` of `CAP_CAPTION_SCHEDULE_END`.
static CapCaptionScheduleWindow CapCaption_Data_80154514[];

static TmdSource _gActor215100PierceCarradineBody;
static TmdSource _gActor215100Actor113100Model07960;
static s32       _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 requestFlags, s32 unusedArgument);
static s32       _actor215100IgnoreCommand(Task* task, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);
void             func_actor_215100_8014CA2C(Task*);
static void      _pacedWalkSubModelTask(Task* task);

static AnimationSet _gActor215100Animation10DE8;
static AnimationSet _gActor215100Animation111A0;
static AnimationSet _gActor215100Animation11364;
static AnimationSet _gActor215100Animation115F4;
static AnimationSet _gActor215100Animation11A34;
static AnimationSet _gActor215100Animation11C10;
static AnimationSet _gActor215100Animation11E24;
static AnimationSet _gActor215100Animation12330;
static AnimationSet _gActor215100Animation12720;
static AnimationSet _gActor215100Animation12ABC;
static AnimationSet _gActor215100Animation12D64;
static AnimationSet _gActor215100Animation1301C;
static AnimationSet _gActor215100Animation13310;
static AnimationSet _gActor215100Animation13500;
static AnimationSet _gActor215100Animation13818;
static AnimationSet _gActor215100Animation13A50;
static AnimationSet _gActor215100Animation13C84;
static AnimationSet _gActor215100Animation13EE4;
static AnimationSet _gActor215100Animation141A0;
static AnimationSet _gActor215100Animation14368;
static AnimationSet _gActor215100Animation14590;
static AnimationSet _gActor215100Animation14758;

static void _actor215100SetConversationCap(s32 enabled);

static void _actor215100GalleryIntroTask(Task* task);
static void _actor215100GalleryRedFlashLoopTask(Task* task);
void        func_actor_215100_8014ADD8(void);
void        func_actor_215100_8014AE08(s32);
static void _actor215100SetGalleryRedFlashLoop(s32 enabled);
void        func_actor_215100_8014AE90(s16);
static void _actor215100SetViewRespawnPending(s16 pending);

TaskDesc D_actor_215100_8014E13C[3] = {
    { { { TASK_BODY_NONE, 32 } }, _actor215100GalleryIntroTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_actor_215100_80149F2C, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _actor215100GalleryRedFlashLoopTask, { .value = 0 } },
};

/// Player clips for extended ids 47-57, added by the shooting gallery's session
/// scenes; NULL at the four ids (53-56) nothing requests, and no script sends
/// the request for 57.
///
/// The event scripts of the gallery's introduction and session set-up send the
/// player its copy request before they play one of these clips; only the
/// introduction's second script plays one without sending the request itself.
/// `D_actor_215100_8014E2B8` copies `ANIMATION_BANK_EXTENSION_CAPACITY` (32)
/// words starting here into the player's bank, which is 21 words past the end
/// of this array: the read runs on through `D_actor_215100_8014E18C`,
/// `D_actor_215100_8014E1A0`, `D_actor_215100_8014E1B4`,
/// `D_actor_215100_8014E1C8` and the first word of `D_actor_215100_8014E1DC`.
/// That overrun is the original's and is kept as it is: the request carries the
/// bank's fixed capacity, while the table was stored with only its own entries.
/// The player's requests select ids 47-52 only, so none of the words installed
/// after the table is played as a clip.
AnimationSet* D_actor_215100_8014E160[11] = { &gActor215100Animation034E4, &gActor215100Animation03754, &gActor215100Animation039AC, &gActor215100Animation03B48, &gActor215100Animation03D98, &gActor215100Animation03FF0, NULL, NULL, NULL, NULL, &gActor215100Animation042F4 };

// The run of requests the scripts send to this package's own actor, the scene's second placed actor, starts here. This one is not referenced.
AnimationPlayRequest D_actor_215100_8014E18C = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E1A0 = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E1B4 = { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E1C8 = { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E1DC = { { .index = 1 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E1F0 = { { .index = 1 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E204[3] = {
    { { .index = 1 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_215100_8014E240 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E254 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E268 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E27C = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E290 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E2A4 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_actor_215100_8014E2B8 = { { .sets = D_actor_215100_8014E160 }, ANIMATION_BANK_EXTENSION_CAPACITY };

AnimationPlayRequest D_actor_215100_8014E2C0 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E2D4 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E2E8 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8014E2FC = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_215100_8014E310 = { { -0x27CE, 0, 4100, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_215100_8014E328 = { { -0x279C, 0, 4720, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_215100_8014E340 = { { -6550, 0, 2950, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_215100_8014E358 = { { -6800, 0, 2950, 0 }, { 0, 1024, 0, 0 } };

EvsCommand D_actor_215100_8014E370[59] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_215100_8014E2B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_215100_8014ADD8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_8014E340 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_8014E310 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5114000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AE08 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetGalleryRedFlashLoop }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5114000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetGalleryRedFlashLoop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E268 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E27C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E290 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_8014E328 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_8014E310 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014E8F8[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor215100SetViewRespawnPending }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetGalleryRedFlashLoop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_8014E310 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_8014E340 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014EA90[5] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014EB08[6] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_215100_8014E2B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E240 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014EB98[3] = {
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014EBE0[18] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_215100_8014E2B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2A4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2D4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_8014E358 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014ED90[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_8014E340 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_8014E310 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014EE68[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_215100_8014E2B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_8014E340 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_8014E310 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014EFA0[8] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2D4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_8014E358 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_215100_8014AE90 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014F060[9] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E2D4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_8014E358 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_8014F138[6] = {
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gActor215100Animation05738Bank1[3] = {
#include "assets/actor_215100_animation_05738_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation05738Bank4[81] = {
#include "assets/actor_215100_animation_05738_bank4.inc"
};

static AnimationRecord _gActor215100Animation05738Records[128] = {
#include "assets/actor_215100_animation_05738_records.inc"
};

static u16 _gActor215100Animation05738Indices[20] = {
#include "assets/actor_215100_animation_05738_indices.inc"
};

static AnimationSet _gActor215100Animation05738 = {
    _gActor215100Animation05738Records,
    _gActor215100Animation05738Indices,
    { NULL, _gActor215100Animation05738Bank1, NULL, NULL, _gActor215100Animation05738Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation05B0CBank1[8] = {
#include "assets/actor_215100_animation_05B0C_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation05B0CBank4[84] = {
#include "assets/actor_215100_animation_05B0C_bank4.inc"
};

static AnimationRecord _gActor215100Animation05B0CRecords[117] = {
#include "assets/actor_215100_animation_05B0C_records.inc"
};

static u16 _gActor215100Animation05B0CIndices[20] = {
#include "assets/actor_215100_animation_05B0C_indices.inc"
};

static AnimationSet _gActor215100Animation05B0C = {
    _gActor215100Animation05B0CRecords,
    _gActor215100Animation05B0CIndices,
    { NULL, _gActor215100Animation05B0CBank1, NULL, NULL, _gActor215100Animation05B0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation05EFCBank1[7] = {
#include "assets/actor_215100_animation_05EFC_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation05EFCBank4[62] = {
#include "assets/actor_215100_animation_05EFC_bank4.inc"
};

static AnimationRecord _gActor215100Animation05EFCRecords[149] = {
#include "assets/actor_215100_animation_05EFC_records.inc"
};

static u16 _gActor215100Animation05EFCIndices[20] = {
#include "assets/actor_215100_animation_05EFC_indices.inc"
};

static AnimationSet _gActor215100Animation05EFC = {
    _gActor215100Animation05EFCRecords,
    _gActor215100Animation05EFCIndices,
    { NULL, _gActor215100Animation05EFCBank1, NULL, NULL, _gActor215100Animation05EFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation06268Bank1[7] = {
#include "assets/actor_215100_animation_06268_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation06268Bank4[74] = {
#include "assets/actor_215100_animation_06268_bank4.inc"
};

static AnimationRecord _gActor215100Animation06268Records[104] = {
#include "assets/actor_215100_animation_06268_records.inc"
};

static u16 _gActor215100Animation06268Indices[20] = {
#include "assets/actor_215100_animation_06268_indices.inc"
};

static AnimationSet _gActor215100Animation06268 = {
    _gActor215100Animation06268Records,
    _gActor215100Animation06268Indices,
    { NULL, _gActor215100Animation06268Bank1, NULL, NULL, _gActor215100Animation06268Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation066B4Bank1[6] = {
#include "assets/actor_215100_animation_066B4_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation066B4Bank4[78] = {
#include "assets/actor_215100_animation_066B4_bank4.inc"
};

static AnimationRecord _gActor215100Animation066B4Records[159] = {
#include "assets/actor_215100_animation_066B4_records.inc"
};

static u16 _gActor215100Animation066B4Indices[20] = {
#include "assets/actor_215100_animation_066B4_indices.inc"
};

static AnimationSet _gActor215100Animation066B4 = {
    _gActor215100Animation066B4Records,
    _gActor215100Animation066B4Indices,
    { NULL, _gActor215100Animation066B4Bank1, NULL, NULL, _gActor215100Animation066B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation069DCBank1[5] = {
#include "assets/actor_215100_animation_069DC_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation069DCBank4[69] = {
#include "assets/actor_215100_animation_069DC_bank4.inc"
};

static AnimationRecord _gActor215100Animation069DCRecords[98] = {
#include "assets/actor_215100_animation_069DC_records.inc"
};

static u16 _gActor215100Animation069DCIndices[20] = {
#include "assets/actor_215100_animation_069DC_indices.inc"
};

static AnimationSet _gActor215100Animation069DC = {
    _gActor215100Animation069DCRecords,
    _gActor215100Animation069DCIndices,
    { NULL, _gActor215100Animation069DCBank1, NULL, NULL, _gActor215100Animation069DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation06F5CBank1[10] = {
#include "assets/actor_215100_animation_06F5C_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation06F5CBank4[104] = {
#include "assets/actor_215100_animation_06F5C_bank4.inc"
};

static AnimationRecord _gActor215100Animation06F5CRecords[198] = {
#include "assets/actor_215100_animation_06F5C_records.inc"
};

static u16 _gActor215100Animation06F5CIndices[20] = {
#include "assets/actor_215100_animation_06F5C_indices.inc"
};

static AnimationSet _gActor215100Animation06F5C = {
    _gActor215100Animation06F5CRecords,
    _gActor215100Animation06F5CIndices,
    { NULL, _gActor215100Animation06F5CBank1, NULL, NULL, _gActor215100Animation06F5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation0733CBank1[8] = {
#include "assets/actor_215100_animation_0733C_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation0733CBank4[85] = {
#include "assets/actor_215100_animation_0733C_bank4.inc"
};

static AnimationRecord _gActor215100Animation0733CRecords[119] = {
#include "assets/actor_215100_animation_0733C_records.inc"
};

static u16 _gActor215100Animation0733CIndices[20] = {
#include "assets/actor_215100_animation_0733C_indices.inc"
};

static AnimationSet _gActor215100Animation0733C = {
    _gActor215100Animation0733CRecords,
    _gActor215100Animation0733CIndices,
    { NULL, _gActor215100Animation0733CBank1, NULL, NULL, _gActor215100Animation0733CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation075B4Bank1[3] = {
#include "assets/actor_215100_animation_075B4_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation075B4Bank4[27] = {
#include "assets/actor_215100_animation_075B4_bank4.inc"
};

static AnimationRecord _gActor215100Animation075B4Records[102] = {
#include "assets/actor_215100_animation_075B4_records.inc"
};

static u16 _gActor215100Animation075B4Indices[20] = {
#include "assets/actor_215100_animation_075B4_indices.inc"
};

static AnimationSet _gActor215100Animation075B4 = {
    _gActor215100Animation075B4Records,
    _gActor215100Animation075B4Indices,
    { NULL, _gActor215100Animation075B4Bank1, NULL, NULL, _gActor215100Animation075B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation07794Bank1[2] = {
#include "assets/actor_215100_animation_07794_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation07794Bank4[25] = {
#include "assets/actor_215100_animation_07794_bank4.inc"
};

static AnimationRecord _gActor215100Animation07794Records[69] = {
#include "assets/actor_215100_animation_07794_records.inc"
};

static u16 _gActor215100Animation07794Indices[20] = {
#include "assets/actor_215100_animation_07794_indices.inc"
};

static AnimationSet _gActor215100Animation07794 = {
    _gActor215100Animation07794Records,
    _gActor215100Animation07794Indices,
    { NULL, _gActor215100Animation07794Bank1, NULL, NULL, _gActor215100Animation07794Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation07960Bank1[2] = {
#include "assets/actor_215100_animation_07960_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation07960Bank4[22] = {
#include "assets/actor_215100_animation_07960_bank4.inc"
};

static AnimationRecord _gActor215100Animation07960Records[67] = {
#include "assets/actor_215100_animation_07960_records.inc"
};

static u16 _gActor215100Animation07960Indices[20] = {
#include "assets/actor_215100_animation_07960_indices.inc"
};

static AnimationSet _gActor215100Animation07960 = {
    _gActor215100Animation07960Records,
    _gActor215100Animation07960Indices,
    { NULL, _gActor215100Animation07960Bank1, NULL, NULL, _gActor215100Animation07960Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation07E28Bank1[4] = {
#include "assets/actor_215100_animation_07E28_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation07E28Bank4[97] = {
#include "assets/actor_215100_animation_07E28_bank4.inc"
};

static AnimationRecord _gActor215100Animation07E28Records[177] = {
#include "assets/actor_215100_animation_07E28_records.inc"
};

static u16 _gActor215100Animation07E28Indices[20] = {
#include "assets/actor_215100_animation_07E28_indices.inc"
};

static AnimationSet _gActor215100Animation07E28 = {
    _gActor215100Animation07E28Records,
    _gActor215100Animation07E28Indices,
    { NULL, _gActor215100Animation07E28Bank1, NULL, NULL, _gActor215100Animation07E28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation07FFCBank1[2] = {
#include "assets/actor_215100_animation_07FFC_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation07FFCBank4[22] = {
#include "assets/actor_215100_animation_07FFC_bank4.inc"
};

static AnimationRecord _gActor215100Animation07FFCRecords[69] = {
#include "assets/actor_215100_animation_07FFC_records.inc"
};

static u16 _gActor215100Animation07FFCIndices[20] = {
#include "assets/actor_215100_animation_07FFC_indices.inc"
};

static AnimationSet _gActor215100Animation07FFC = {
    _gActor215100Animation07FFCRecords,
    _gActor215100Animation07FFCIndices,
    { NULL, _gActor215100Animation07FFCBank1, NULL, NULL, _gActor215100Animation07FFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation08400Bank1[5] = {
#include "assets/actor_215100_animation_08400_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation08400Bank4[94] = {
#include "assets/actor_215100_animation_08400_bank4.inc"
};

static AnimationRecord _gActor215100Animation08400Records[128] = {
#include "assets/actor_215100_animation_08400_records.inc"
};

static u16 _gActor215100Animation08400Indices[20] = {
#include "assets/actor_215100_animation_08400_indices.inc"
};

static AnimationSet _gActor215100Animation08400 = {
    _gActor215100Animation08400Records,
    _gActor215100Animation08400Indices,
    { NULL, _gActor215100Animation08400Bank1, NULL, NULL, _gActor215100Animation08400Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation08788Bank1[4] = {
#include "assets/actor_215100_animation_08788_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation08788Bank4[84] = {
#include "assets/actor_215100_animation_08788_bank4.inc"
};

static AnimationRecord _gActor215100Animation08788Records[110] = {
#include "assets/actor_215100_animation_08788_records.inc"
};

static u16 _gActor215100Animation08788Indices[20] = {
#include "assets/actor_215100_animation_08788_indices.inc"
};

static AnimationSet _gActor215100Animation08788 = {
    _gActor215100Animation08788Records,
    _gActor215100Animation08788Indices,
    { NULL, _gActor215100Animation08788Bank1, NULL, NULL, _gActor215100Animation08788Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation08BE0Bank1[5] = {
#include "assets/actor_215100_animation_08BE0_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation08BE0Bank4[108] = {
#include "assets/actor_215100_animation_08BE0_bank4.inc"
};

static AnimationRecord _gActor215100Animation08BE0Records[135] = {
#include "assets/actor_215100_animation_08BE0_records.inc"
};

static u16 _gActor215100Animation08BE0Indices[20] = {
#include "assets/actor_215100_animation_08BE0_indices.inc"
};

static AnimationSet _gActor215100Animation08BE0 = {
    _gActor215100Animation08BE0Records,
    _gActor215100Animation08BE0Indices,
    { NULL, _gActor215100Animation08BE0Bank1, NULL, NULL, _gActor215100Animation08BE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation08EBCBank1[2] = {
#include "assets/actor_215100_animation_08EBC_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation08EBCBank4[32] = {
#include "assets/actor_215100_animation_08EBC_bank4.inc"
};

static AnimationRecord _gActor215100Animation08EBCRecords[125] = {
#include "assets/actor_215100_animation_08EBC_records.inc"
};

static u16 _gActor215100Animation08EBCIndices[20] = {
#include "assets/actor_215100_animation_08EBC_indices.inc"
};

static AnimationSet _gActor215100Animation08EBC = {
    _gActor215100Animation08EBCRecords,
    _gActor215100Animation08EBCIndices,
    { NULL, _gActor215100Animation08EBCBank1, NULL, NULL, _gActor215100Animation08EBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation09064Bank1[2] = {
#include "assets/actor_215100_animation_09064_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation09064Bank4[23] = {
#include "assets/actor_215100_animation_09064_bank4.inc"
};

static AnimationRecord _gActor215100Animation09064Records[57] = {
#include "assets/actor_215100_animation_09064_records.inc"
};

static u16 _gActor215100Animation09064Indices[20] = {
#include "assets/actor_215100_animation_09064_indices.inc"
};

static AnimationSet _gActor215100Animation09064 = {
    _gActor215100Animation09064Records,
    _gActor215100Animation09064Indices,
    { NULL, _gActor215100Animation09064Bank1, NULL, NULL, _gActor215100Animation09064Bank4, NULL, NULL, NULL },
};

/// Player clips for extended ids 47-66, added by the actor's conversation
/// scenes; NULL at the two ids (57-58) nothing requests, and no script sends
/// the request for 59.
///
/// The two captioned scenes that play these clips each send the player its copy
/// request before the first of them. `D_actor_215100_801531CC` copies
/// `ANIMATION_BANK_EXTENSION_CAPACITY` (32) words starting here into the
/// player's bank, which is twelve words past the end of this array: the read
/// runs on through `D_actor_215100_80152EFC`, `D_actor_215100_80152F10` and the
/// first two words of `D_actor_215100_80152F24`. That overrun is the original's
/// and is kept as it is: the request carries the bank's fixed capacity, while
/// the table was stored with only its own entries. The player's requests select
/// ids 47-56 in the first scene and 60-66 in the second, so none of the words
/// installed after the table is played as a clip.
AnimationSet* D_actor_215100_80152EAC[20] = { &_gActor215100Animation075B4, &_gActor215100Animation07794, &_gActor215100Animation07960, &_gActor215100Animation07E28, &_gActor215100Animation07FFC, &_gActor215100Animation08400, &_gActor215100Animation08788, &_gActor215100Animation08BE0, &_gActor215100Animation08EBC, &_gActor215100Animation09064, NULL, NULL, &_gActor215100Animation05738, &_gActor215100Animation05B0C, &_gActor215100Animation05EFC, &_gActor215100Animation06268, &_gActor215100Animation066B4, &_gActor215100Animation069DC, &_gActor215100Animation06F5C, &_gActor215100Animation0733C };

// The run of requests the scripts send to this package's own actor, the scene's second placed actor, starts here. This one is not referenced.
AnimationPlayRequest D_actor_215100_80152EFC = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152F10 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152F24 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152F38 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152F4C = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152F60 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152F74 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152F88 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152F9C = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152FB0 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152FC4 = { { .index = 1 }, 10, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152FD8 = { { .index = 1 }, 11, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80152FEC = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153000 = { { .index = 1 }, 12, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153014 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153028 = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8015303C = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153050 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153064 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153078 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8015308C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_801530A0 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_801530B4 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_801530C8 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_801530DC = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_801530F0 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153104 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153118 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8015312C = { { .index = 1 }, 59, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153140 = { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153154 = { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153168 = { { .index = 1 }, 62, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_8015317C = { { .index = 1 }, 63, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153190 = { { .index = 1 }, 64, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_801531A4 = { { .index = 1 }, 65, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_801531B8 = { { .index = 1 }, 66, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_actor_215100_801531CC = { { .sets = D_actor_215100_80152EAC }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorTransform D_actor_215100_801531D4 = { { -0x27F6, 0, 4640, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_215100_801531EC = { { -0x2BC0, 0, 3000, 0 }, { 0, -2218, 0, 0 } };

ActorTransform D_actor_215100_80153204 = { { -0x27F6, 0, 5000, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_215100_8015321C = { { -0x295E, 0, 2100, 0 }, { 0, -56, 0, 0 } };

ActorTransform D_actor_215100_80153234 = { { -0x29FA, 0, 3972, 0 }, { 0, 0, 0, 0 } };

AnimationPlayRequest D_actor_215100_8015324C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153260 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_actor_215100_80153274[117] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_215100_801531CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_801531D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5114000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 112 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5114000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_801531EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_8015321C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015308C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153078 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F9C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F9C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153104 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153118 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153000 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8015303C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153000 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015308C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153104 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153118 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F9C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F9C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801530F0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F60 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_80153204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015324C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_80153234 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_80153D6C[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015324C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_80153204 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_215100_80153234 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_80153ED4[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153014 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153028 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_80153FDC[43] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_215100_801531CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153014 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153028 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152F38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153140 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153154 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153014 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153028 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153168 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8015317C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153190 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153014 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153028 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801531A4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_801531B8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_801543E4[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153014 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153028 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor215100SetConversationCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "../../shared/cap_captions_settings.inc.c"

static void CapCaption_RunSchedule(Task* task);

static TaskDesc D_actor_215100_801544F0[1] = {
    { { { TASK_BODY_NONE, 32 } }, CapCaption_RunSchedule, { .value = 0 } }
};

#include "../../shared/cap_captions_schedule.inc.c"

static TmdBone _gActor215100PierceCarradineBodySkeleton[20] = {
#include "assets/pierce_carradine_body_skeleton.inc"
};

static u32 _gActor215100PierceCarradineBodyPartVerts[20] = {
#include "assets/pierce_carradine_body_partVerts.inc"
};

static SVECTOR _gActor215100PierceCarradineBodyVerts[390] = {
#include "assets/pierce_carradine_body_verts.inc"
};

static SVECTOR _gActor215100PierceCarradineBodyNormals[407] = {
#include "assets/pierce_carradine_body_normals.inc"
};

static u32 _gActor215100PierceCarradineBodyStream[4476] = {
#include "assets/pierce_carradine_body_stream.inc"
};

static TmdSource _gActor215100PierceCarradineBody = {
    0,
    24444,
    6776,
    20,
    _gActor215100PierceCarradineBodyPartVerts,
    _gActor215100PierceCarradineBodyVerts,
    _gActor215100PierceCarradineBodyNormals,
    _gActor215100PierceCarradineBodySkeleton,
    _gActor215100PierceCarradineBodyStream,
};

static TmdBone _gActor215100Actor113100Model07960Skeleton[1] = {
#include "assets/actor_113100_model_07960_skeleton.inc"
};

static u32 _gActor215100Actor113100Model07960PartVerts[1] = {
#include "assets/actor_113100_model_07960_partVerts.inc"
};

static SVECTOR _gActor215100Actor113100Model07960Verts[14] = {
#include "assets/actor_113100_model_07960_verts.inc"
};

static SVECTOR _gActor215100Actor113100Model07960Normals[12] = {
#include "assets/actor_113100_model_07960_normals.inc"
};

static u32 _gActor215100Actor113100Model07960Stream[56] = {
#include "assets/actor_113100_model_07960_stream.inc"
};

static TmdSource _gActor215100Actor113100Model07960 = {
    0,
    340,
    0,
    1,
    _gActor215100Actor113100Model07960PartVerts,
    _gActor215100Actor113100Model07960Verts,
    _gActor215100Actor113100Model07960Normals,
    _gActor215100Actor113100Model07960Skeleton,
    _gActor215100Actor113100Model07960Stream,
};

static AnimationPackedPose _gActor215100Animation10DE8Bank1[2] = {
#include "assets/actor_215100_animation_10DE8_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation10DE8Bank4[25] = {
#include "assets/actor_215100_animation_10DE8_bank4.inc"
};

static AnimationRecord _gActor215100Animation10DE8Records[88] = {
#include "assets/actor_215100_animation_10DE8_records.inc"
};

static u16 _gActor215100Animation10DE8Indices[20] = {
#include "assets/actor_215100_animation_10DE8_indices.inc"
};

static AnimationSet _gActor215100Animation10DE8 = {
    _gActor215100Animation10DE8Records,
    _gActor215100Animation10DE8Indices,
    { NULL, _gActor215100Animation10DE8Bank1, NULL, NULL, _gActor215100Animation10DE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation111A0Bank1[2] = {
#include "assets/actor_215100_animation_111A0_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation111A0Bank4[77] = {
#include "assets/actor_215100_animation_111A0_bank4.inc"
};

static AnimationRecord _gActor215100Animation111A0Records[135] = {
#include "assets/actor_215100_animation_111A0_records.inc"
};

static u16 _gActor215100Animation111A0Indices[20] = {
#include "assets/actor_215100_animation_111A0_indices.inc"
};

static AnimationSet _gActor215100Animation111A0 = {
    _gActor215100Animation111A0Records,
    _gActor215100Animation111A0Indices,
    { NULL, _gActor215100Animation111A0Bank1, NULL, NULL, _gActor215100Animation111A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation11364Bank1[2] = {
#include "assets/actor_215100_animation_11364_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation11364Bank4[21] = {
#include "assets/actor_215100_animation_11364_bank4.inc"
};

static AnimationRecord _gActor215100Animation11364Records[66] = {
#include "assets/actor_215100_animation_11364_records.inc"
};

static u16 _gActor215100Animation11364Indices[20] = {
#include "assets/actor_215100_animation_11364_indices.inc"
};

static AnimationSet _gActor215100Animation11364 = {
    _gActor215100Animation11364Records,
    _gActor215100Animation11364Indices,
    { NULL, _gActor215100Animation11364Bank1, NULL, NULL, _gActor215100Animation11364Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation115F4Bank1[2] = {
#include "assets/actor_215100_animation_115F4_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation115F4Bank4[32] = {
#include "assets/actor_215100_animation_115F4_bank4.inc"
};

static AnimationRecord _gActor215100Animation115F4Records[106] = {
#include "assets/actor_215100_animation_115F4_records.inc"
};

static u16 _gActor215100Animation115F4Indices[20] = {
#include "assets/actor_215100_animation_115F4_indices.inc"
};

static AnimationSet _gActor215100Animation115F4 = {
    _gActor215100Animation115F4Records,
    _gActor215100Animation115F4Indices,
    { NULL, _gActor215100Animation115F4Bank1, NULL, NULL, _gActor215100Animation115F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation11A34Bank1[4] = {
#include "assets/actor_215100_animation_11A34_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation11A34Bank4[92] = {
#include "assets/actor_215100_animation_11A34_bank4.inc"
};

static AnimationRecord _gActor215100Animation11A34Records[148] = {
#include "assets/actor_215100_animation_11A34_records.inc"
};

static u16 _gActor215100Animation11A34Indices[20] = {
#include "assets/actor_215100_animation_11A34_indices.inc"
};

static AnimationSet _gActor215100Animation11A34 = {
    _gActor215100Animation11A34Records,
    _gActor215100Animation11A34Indices,
    { NULL, _gActor215100Animation11A34Bank1, NULL, NULL, _gActor215100Animation11A34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation11C10Bank1[2] = {
#include "assets/actor_215100_animation_11C10_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation11C10Bank4[27] = {
#include "assets/actor_215100_animation_11C10_bank4.inc"
};

static AnimationRecord _gActor215100Animation11C10Records[66] = {
#include "assets/actor_215100_animation_11C10_records.inc"
};

static u16 _gActor215100Animation11C10Indices[20] = {
#include "assets/actor_215100_animation_11C10_indices.inc"
};

static AnimationSet _gActor215100Animation11C10 = {
    _gActor215100Animation11C10Records,
    _gActor215100Animation11C10Indices,
    { NULL, _gActor215100Animation11C10Bank1, NULL, NULL, _gActor215100Animation11C10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation11E24Bank1[3] = {
#include "assets/actor_215100_animation_11E24_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation11E24Bank4[32] = {
#include "assets/actor_215100_animation_11E24_bank4.inc"
};

static AnimationRecord _gActor215100Animation11E24Records[72] = {
#include "assets/actor_215100_animation_11E24_records.inc"
};

static u16 _gActor215100Animation11E24Indices[20] = {
#include "assets/actor_215100_animation_11E24_indices.inc"
};

static AnimationSet _gActor215100Animation11E24 = {
    _gActor215100Animation11E24Records,
    _gActor215100Animation11E24Indices,
    { NULL, _gActor215100Animation11E24Bank1, NULL, NULL, _gActor215100Animation11E24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation12330Bank1[4] = {
#include "assets/actor_215100_animation_12330_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation12330Bank4[99] = {
#include "assets/actor_215100_animation_12330_bank4.inc"
};

static AnimationRecord _gActor215100Animation12330Records[192] = {
#include "assets/actor_215100_animation_12330_records.inc"
};

static u16 _gActor215100Animation12330Indices[20] = {
#include "assets/actor_215100_animation_12330_indices.inc"
};

static AnimationSet _gActor215100Animation12330 = {
    _gActor215100Animation12330Records,
    _gActor215100Animation12330Indices,
    { NULL, _gActor215100Animation12330Bank1, NULL, NULL, _gActor215100Animation12330Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation12720Bank1[5] = {
#include "assets/actor_215100_animation_12720_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation12720Bank4[91] = {
#include "assets/actor_215100_animation_12720_bank4.inc"
};

static AnimationRecord _gActor215100Animation12720Records[126] = {
#include "assets/actor_215100_animation_12720_records.inc"
};

static u16 _gActor215100Animation12720Indices[20] = {
#include "assets/actor_215100_animation_12720_indices.inc"
};

static AnimationSet _gActor215100Animation12720 = {
    _gActor215100Animation12720Records,
    _gActor215100Animation12720Indices,
    { NULL, _gActor215100Animation12720Bank1, NULL, NULL, _gActor215100Animation12720Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation12ABCBank1[6] = {
#include "assets/actor_215100_animation_12ABC_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation12ABCBank4[74] = {
#include "assets/actor_215100_animation_12ABC_bank4.inc"
};

static AnimationRecord _gActor215100Animation12ABCRecords[119] = {
#include "assets/actor_215100_animation_12ABC_records.inc"
};

static u16 _gActor215100Animation12ABCIndices[20] = {
#include "assets/actor_215100_animation_12ABC_indices.inc"
};

static AnimationSet _gActor215100Animation12ABC = {
    _gActor215100Animation12ABCRecords,
    _gActor215100Animation12ABCIndices,
    { NULL, _gActor215100Animation12ABCBank1, NULL, NULL, _gActor215100Animation12ABCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation12D64Bank1[2] = {
#include "assets/actor_215100_animation_12D64_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation12D64Bank4[33] = {
#include "assets/actor_215100_animation_12D64_bank4.inc"
};

static AnimationRecord _gActor215100Animation12D64Records[111] = {
#include "assets/actor_215100_animation_12D64_records.inc"
};

static u16 _gActor215100Animation12D64Indices[20] = {
#include "assets/actor_215100_animation_12D64_indices.inc"
};

static AnimationSet _gActor215100Animation12D64 = {
    _gActor215100Animation12D64Records,
    _gActor215100Animation12D64Indices,
    { NULL, _gActor215100Animation12D64Bank1, NULL, NULL, _gActor215100Animation12D64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation1301CBank1[3] = {
#include "assets/actor_215100_animation_1301C_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation1301CBank4[29] = {
#include "assets/actor_215100_animation_1301C_bank4.inc"
};

static AnimationRecord _gActor215100Animation1301CRecords[116] = {
#include "assets/actor_215100_animation_1301C_records.inc"
};

static u16 _gActor215100Animation1301CIndices[20] = {
#include "assets/actor_215100_animation_1301C_indices.inc"
};

static AnimationSet _gActor215100Animation1301C = {
    _gActor215100Animation1301CRecords,
    _gActor215100Animation1301CIndices,
    { NULL, _gActor215100Animation1301CBank1, NULL, NULL, _gActor215100Animation1301CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation13310Bank1[2] = {
#include "assets/actor_215100_animation_13310_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation13310Bank4[43] = {
#include "assets/actor_215100_animation_13310_bank4.inc"
};

static AnimationRecord _gActor215100Animation13310Records[120] = {
#include "assets/actor_215100_animation_13310_records.inc"
};

static u16 _gActor215100Animation13310Indices[20] = {
#include "assets/actor_215100_animation_13310_indices.inc"
};

static AnimationSet _gActor215100Animation13310 = {
    _gActor215100Animation13310Records,
    _gActor215100Animation13310Indices,
    { NULL, _gActor215100Animation13310Bank1, NULL, NULL, _gActor215100Animation13310Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation13500Bank1[2] = {
#include "assets/actor_215100_animation_13500_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation13500Bank4[30] = {
#include "assets/actor_215100_animation_13500_bank4.inc"
};

static AnimationRecord _gActor215100Animation13500Records[68] = {
#include "assets/actor_215100_animation_13500_records.inc"
};

static u16 _gActor215100Animation13500Indices[20] = {
#include "assets/actor_215100_animation_13500_indices.inc"
};

static AnimationSet _gActor215100Animation13500 = {
    _gActor215100Animation13500Records,
    _gActor215100Animation13500Indices,
    { NULL, _gActor215100Animation13500Bank1, NULL, NULL, _gActor215100Animation13500Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation13818Bank1[2] = {
#include "assets/actor_215100_animation_13818_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation13818Bank4[56] = {
#include "assets/actor_215100_animation_13818_bank4.inc"
};

static AnimationRecord _gActor215100Animation13818Records[116] = {
#include "assets/actor_215100_animation_13818_records.inc"
};

static u16 _gActor215100Animation13818Indices[20] = {
#include "assets/actor_215100_animation_13818_indices.inc"
};

static AnimationSet _gActor215100Animation13818 = {
    _gActor215100Animation13818Records,
    _gActor215100Animation13818Indices,
    { NULL, _gActor215100Animation13818Bank1, NULL, NULL, _gActor215100Animation13818Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation13A50Bank1[2] = {
#include "assets/actor_215100_animation_13A50_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation13A50Bank4[27] = {
#include "assets/actor_215100_animation_13A50_bank4.inc"
};

static AnimationRecord _gActor215100Animation13A50Records[89] = {
#include "assets/actor_215100_animation_13A50_records.inc"
};

static u16 _gActor215100Animation13A50Indices[20] = {
#include "assets/actor_215100_animation_13A50_indices.inc"
};

static AnimationSet _gActor215100Animation13A50 = {
    _gActor215100Animation13A50Records,
    _gActor215100Animation13A50Indices,
    { NULL, _gActor215100Animation13A50Bank1, NULL, NULL, _gActor215100Animation13A50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation13C84Bank1[3] = {
#include "assets/actor_215100_animation_13C84_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation13C84Bank4[30] = {
#include "assets/actor_215100_animation_13C84_bank4.inc"
};

static AnimationRecord _gActor215100Animation13C84Records[82] = {
#include "assets/actor_215100_animation_13C84_records.inc"
};

static u16 _gActor215100Animation13C84Indices[20] = {
#include "assets/actor_215100_animation_13C84_indices.inc"
};

static AnimationSet _gActor215100Animation13C84 = {
    _gActor215100Animation13C84Records,
    _gActor215100Animation13C84Indices,
    { NULL, _gActor215100Animation13C84Bank1, NULL, NULL, _gActor215100Animation13C84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation13EE4Bank1[2] = {
#include "assets/actor_215100_animation_13EE4_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation13EE4Bank4[34] = {
#include "assets/actor_215100_animation_13EE4_bank4.inc"
};

static AnimationRecord _gActor215100Animation13EE4Records[92] = {
#include "assets/actor_215100_animation_13EE4_records.inc"
};

static u16 _gActor215100Animation13EE4Indices[20] = {
#include "assets/actor_215100_animation_13EE4_indices.inc"
};

static AnimationSet _gActor215100Animation13EE4 = {
    _gActor215100Animation13EE4Records,
    _gActor215100Animation13EE4Indices,
    { NULL, _gActor215100Animation13EE4Bank1, NULL, NULL, _gActor215100Animation13EE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation141A0Bank1[2] = {
#include "assets/actor_215100_animation_141A0_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation141A0Bank4[41] = {
#include "assets/actor_215100_animation_141A0_bank4.inc"
};

static AnimationRecord _gActor215100Animation141A0Records[108] = {
#include "assets/actor_215100_animation_141A0_records.inc"
};

static u16 _gActor215100Animation141A0Indices[20] = {
#include "assets/actor_215100_animation_141A0_indices.inc"
};

static AnimationSet _gActor215100Animation141A0 = {
    _gActor215100Animation141A0Records,
    _gActor215100Animation141A0Indices,
    { NULL, _gActor215100Animation141A0Bank1, NULL, NULL, _gActor215100Animation141A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation14368Bank1[2] = {
#include "assets/actor_215100_animation_14368_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation14368Bank4[28] = {
#include "assets/actor_215100_animation_14368_bank4.inc"
};

static AnimationRecord _gActor215100Animation14368Records[60] = {
#include "assets/actor_215100_animation_14368_records.inc"
};

static u16 _gActor215100Animation14368Indices[20] = {
#include "assets/actor_215100_animation_14368_indices.inc"
};

static AnimationSet _gActor215100Animation14368 = {
    _gActor215100Animation14368Records,
    _gActor215100Animation14368Indices,
    { NULL, _gActor215100Animation14368Bank1, NULL, NULL, _gActor215100Animation14368Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation14590Bank1[2] = {
#include "assets/actor_215100_animation_14590_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation14590Bank4[36] = {
#include "assets/actor_215100_animation_14590_bank4.inc"
};

static AnimationRecord _gActor215100Animation14590Records[76] = {
#include "assets/actor_215100_animation_14590_records.inc"
};

static u16 _gActor215100Animation14590Indices[20] = {
#include "assets/actor_215100_animation_14590_indices.inc"
};

static AnimationSet _gActor215100Animation14590 = {
    _gActor215100Animation14590Records,
    _gActor215100Animation14590Indices,
    { NULL, _gActor215100Animation14590Bank1, NULL, NULL, _gActor215100Animation14590Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor215100Animation14758Bank1[2] = {
#include "assets/actor_215100_animation_14758_bank1.inc"
};

static AnimationPackedRotation _gActor215100Animation14758Bank4[28] = {
#include "assets/actor_215100_animation_14758_bank4.inc"
};

static AnimationRecord _gActor215100Animation14758Records[60] = {
#include "assets/actor_215100_animation_14758_records.inc"
};

static u16 _gActor215100Animation14758Indices[20] = {
#include "assets/actor_215100_animation_14758_indices.inc"
};

static AnimationSet _gActor215100Animation14758 = {
    _gActor215100Animation14758Records,
    _gActor215100Animation14758Indices,
    { NULL, _gActor215100Animation14758Bank1, NULL, NULL, _gActor215100Animation14758Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_actor_215100_8015E5A0[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pacedWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pacedWalkSetPairModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor215100IgnoreCommand },
    { ACTOR_MESSAGE_WALK_TO, _pacedWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_215100_8015E5D0[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_215100_8014CA2C, { .model = &_gActor215100PierceCarradineBody } },
    { { { TASK_BODY_TMD, 192 } }, _pacedWalkSubModelTask, { .model = &_gActor215100Actor113100Model07960 } },
};

AnimationSet* D_actor_215100_8015E5E8[25] = {
    NULL,
    &_gActor215100Animation10DE8,
    &_gActor215100Animation111A0,
    &_gActor215100Animation11364,
    &_gActor215100Animation115F4,
    &_gActor215100Animation11A34,
    &_gActor215100Animation11C10,
    &_gActor215100Animation11E24,
    &_gActor215100Animation12330,
    &_gActor215100Animation12720,
    &_gActor215100Animation12ABC,
    &_gActor215100Animation12D64,
    &_gActor215100Animation1301C,
    &_gActor215100Animation13310,
    &_gActor215100Animation13500,
    &_gActor215100Animation13818,
    NULL,
    NULL,
    &_gActor215100Animation13A50,
    &_gActor215100Animation13C84,
    &_gActor215100Animation13EE4,
    &_gActor215100Animation141A0,
    &_gActor215100Animation14368,
    &_gActor215100Animation14590,
    &_gActor215100Animation14758,
};

Task* D_actor_215100_8015E64C = NULL;

static CapCommandRef* CapCaption_Data_8015E650 = NULL;

/// Glyph and title cells borrowed read-only from the selected loaded CAP file.
///
/// Text uses low-ten-bit indices; titles use low-byte selectors minus one.
/// Every used index must exist in the file, which must remain loaded while
/// captions are measured or drawn. Relocation republishes this pointer.
static const TextGlyphCell* _gCapCaptionGlyphCells = NULL;

/// Selected CAP sequence command and its following text records.
///
/// Borrowed from the loaded CAP file, which must remain live through selection
/// and drawing. Slot zero is the sequence command; text records start at one
/// and end at a record whose text reference is `CAP_TEXT_REF_END`. NULL hides
/// captions. Selection requires a matching nonterminal key for measurement.
static CapSequenceRecord* _gCapCaptionSequence = NULL;

/// Biased left X of the selected caption's widest closed line, in screen pixels.
///
/// Cached as a signed halfword from (320 - width) / 2 - 5. Drawing uses its low
/// halfword for packet coordinates; long lines may place it left of the screen.
static s16 _gCapCaptionBlockLeftX = 0;

/// First baseline of the selected caption block, in screen pixels.
///
/// Cached as a signed halfword from the bottom baseline minus subsequent
/// closed-line heights. Drawing subtracts the caption draw-origin Y.
static s16 _gCapCaptionFirstBaselineY = 0;

/// Selected caption's bottom baseline in screen pixels.
///
/// Selection narrows the caller's word to a signed halfword. This anchors the
/// first-baseline calculation and the caption box's bottom edge.
static s16 _gCapCaptionBottomBaselineY = 0;

/// Selected text-record slot within the current CAP sequence.
///
/// A signed-halfword index counting twelve-byte records; slot zero is the
/// command, so selected text starts at one. The selected key must exist before
/// the terminal slot and its index must fit in 1..32767 for metric calculation.
static s16 _gCapCaptionRecordIndex = 0;

/// Height of the selected caption's closed lines, in screen pixels.
///
/// Each line break commits its tallest nonnegative glyph height plus two,
/// or two pixels for an empty line. The cached sum narrows to a signed halfword;
/// an unfinished last line contributes nothing. Used to size the caption box.
static s16 _gCapCaptionBlockHeight = 0;

static s16 CapCaption_Data_8015E666 = 0;

/// Continuation triangle's left X, retaining the low halfword of draw-coordinate pixels.
///
/// Updated at each text line break to the preceding pen X plus four. The
/// triangle spans seven pixels to its right; no position is updated without a break.
static u16 _gCapCaptionCaretLeftX = 0;

/// Continuation triangle's tip Y, retaining the low halfword of draw-coordinate pixels.
///
/// Updated at each text line break to the preceding baseline minus two.
/// Its upper edge is seven pixels above this tip; no VRAM Y offset is applied.
static u16 _gCapCaptionCaretTipY = 0;

/// Per-instance caret countdown and its uninterpreted retained bytes.
///
/// Selection resets `drawsLeft` to `CAP_CAPTION_CARET_DELAY_DRAWS`. Only eligible
/// caret calls consume it; all initializer bytes remain stored verbatim.
static CapCaptionCaretDelayStorage _gCapCaptionCaretDelayStorage = { 0, { 35, 192, 0 } };

s32 D_actor_215100_8015E670;

RoomEventMsg D_actor_215100_8015E678;

void func_actor_215100_8014A398(void);
void func_actor_215100_8014A908(void);
void func_actor_215100_8014A9A0(void);
s32  func_actor_215100_8014AA54(RoomEventMsg* arg0);
void func_actor_215100_8014AB6C(void);

static void func_actor_215100_8014C660(Enemy* enemy, Task* task);

/// Arms the weapon pickup at this actor's spot while the event flag
/// `D_actor_215100_8014D038` is up and the story step has reached 3. A session
/// leave (`gGameSession->location.loc.view == 0x12`) restores the training barrier with
/// `mistShootingGallerySetTrainingBarrierLowered(0)` and clears
/// `D_actor_215100_8014D03C`; sub-states 2 and 3 of
/// `Gp_StateC08.mode` start the 0x3C-frame cooldown in
/// `D_actor_215100_8014D044`, and while that cooldown runs the function only
/// ticks it down.
///
/// Otherwise the player has to be standing in the zone — its model root's X
/// below -0x1806 and its Z inside [0x10CD, 0x1644) — not aiming
/// (`GameActor.mode != 2`), with the caption system idle, `D_80115768`
/// and `gDisplayState.pendingMode` clear, its yaw inside one of the two 0x3FF-wide windows
/// opening at 0x201 and 0xA01, and one of the 0x1000 / 0x4000 pad masks held.
/// Either mask runs the handoff `func_actor_215100_8014AA54` uses: the weapon
/// message, caption command 0x14 and the scene task `D_actor_215100_8014CF6C`.
void func_actor_215100_8014A398(void)
{
    Task*      task;
    GameActor* actor;
    GfxCoord*  coord;
    s32        z;
    s32        facing;

    task  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor = (GameActor*)task->work;
    coord = task->extra.tmd->coords;
    if (D_actor_215100_8014D038 != 0) {
        if (D_actor_215100_8015E670 >= 3) {
            if (gGameSession->location.loc.view == 0x12) {
                mistShootingGallerySetTrainingBarrierLowered(0);
                D_actor_215100_8014D03C = 0;
            }
            if ((u32)((u8)Gp_StateC08.mode - ATTACHMENT_MODE_ARMED) < 2U) {
                D_actor_215100_8014D044 = 0x3C;
            }
            if (D_actor_215100_8014D044 != 0) {
                D_actor_215100_8014D044 -= 1;
                return;
            }
            if ((actor->mode != GAME_ACTOR_MODE_SCRIPTED) && (capIsBusy() == 0) && (D_actor_215100_8014D03C == 0) &&
                (D_80115768 == 0) && (coord->coord.t[0] < -0x1806)) {
                z = coord->coord.t[2];
                if (z < 0x1644) {
                    if ((z >= 0x10CD) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                        facing = (u16)actor->rotation.vy & 0xFFF;
                        if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP) != 0) {
                            if ((u32)(facing - 0xA01) < 0x3FFU) {
                                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
                                capRunCommand(0x14, CAP_PLAYBACK_IN_PLACE);
                                D_80115690 = 1;
                                taskSpawnFromTable(D_actor_215100_8014CF6C, 0, 0, 0);
                            }
                        }
                        if ((padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_DOWN) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
                            capRunCommand(0x14, CAP_PLAYBACK_IN_PLACE);
                            D_80115690 = 1;
                            taskSpawnFromTable(D_actor_215100_8014CF6C, 0, 0, 0);
                        }
                    }
                }
            }
        }
    }
}

/// Watches the caption system while the actor waits to be talked to.
///
/// State 0 first honours the spawn argument: `spawnArg1 == 2` means the actor
/// was placed already committed, so it just steps to state 1, and only
/// `spawnArg1 == 0` is the interactive case. Otherwise it waits for
/// `capIsBusy` to drop and switches on the key `capGetVariantKey` returns.
/// Key 1 is the plain "talk to me" — it takes the player's weapon away and
/// clears `gSceneCombatState.actorControl`; every other key ends the encounter, and which ending
/// depends on `spawnArg1`: non-zero plays caption command 0x17 behind story
/// flag 0xED and steps to state 1, while zero starts the full ending from here
/// (the caption system is stopped, the scene task `D_mist_shooting_gallery_8018E0C4` gets its exit,
/// the sound plays and the weapon is taken). All of those finish by killing
/// this task.
///
/// State 1 commits the deferred room transition once the caption system is
/// idle again: it copies the area, warp and room of the request held in
/// `D_actor_215100_8015E678` into the live save's location, clears the
/// inventory, then spawns task 0x11 and kills itself.
void func_actor_215100_8014A5C0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (arg0->spawnArg1.value == 2) {
                arg0->state = 1;
                break;
            }
            if (capIsBusy() != 0) {
                break;
            }
            if (capGetVariantKey() == 1) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(arg0);
                break;
            }
            if (arg0->spawnArg1.value != 0) {
                if (gameFlagGetNibble(GAME_FLAG_0ED) != 0) {
                    capRunCommandWithTransition(0x17);
                }
                gGameSession->battleResetPending = 1;
                arg0->state                     += 1;
                break;
            }
            if (D_actor_215100_8015E670 == 3) {
                Gp_StateC08.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
            }
            D_actor_215100_8014D038 = 0;
            mistShootingGallerySetTrainingBarrierLowered(1);
            D_actor_215100_8014D03C = 1;
            taskCallExit(D_mist_shooting_gallery_8018E0C4);
            gGameSession->battleResetPending = 1;
            gGameSession->flowFlags         |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            sndEvtRequestMidiStop(0, 0x1E);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            taskKill(arg0);
            break;
        case 1:
            if (capIsBusy() == 0) {
                gPlayerStatus.resourceVariant                       = 3;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 1;
                inventoryRestoreCarriedLoadout();
                gGameSession->hideHud = 1;
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 5), 0, 0);
                gDisplayState.spriteVariant                                = 1;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_actor_215100_8015E678.areaId;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_actor_215100_8015E678.warp;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = D_actor_215100_8015E678.room;
                taskSpawn(0, 0x11, 0, 0);
                taskKill(arg0);
            }
            break;
    }
}

/// Watches the caption system while the actor waits to be talked to: state 0
/// polls `capIsBusy` / `capGetVariantKey`, and on key 2 hands the scene task
/// `D_mist_shooting_gallery_8018E0C4` its exit and steps to state 1, while any other key kills the
/// task outright. State 1 starts the caption playback and steps to state 2,
/// which commits the ending: it flags the save-slot session, plays the sound,
/// clears the actor's own 0x97B, drops the story flag the sibling
/// `func_actor_215100_8014A908` sets, and releases the menu hold.
void func_actor_215100_8014A7C4(Task* arg0)
{
    GameActor* actor;

    actor = (GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work;
    switch (arg0->state) {
        case 0:
            if (capIsBusy() != 0) {
                break;
            }
            if (capGetVariantKey() == 2) {
                taskCallExit(D_mist_shooting_gallery_8018E0C4);
                arg0->state++;
            } else {
                taskKill(arg0);
            }
            break;
        case 1:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
            mistShootingGallerySelectRoomLights(0);
            arg0->state++;
            break;
        case 2:
            gGameSession->battleResetPending = 1;
            gGameSession->flowFlags         |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            sndEvtRequestMidiStop(0, 0x1E);
            actor->movementInputDisabled = 0;
            D_actor_215100_8014D038      = 0;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            Gp_StateC08.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
            if (gDisplayState.holdCount != 0) {
                displayReleaseMenuHold();
            }
            taskKill(arg0);
            break;
    }
}

void func_actor_215100_8014A908(void)
{
    D_actor_215100_8014D038 = 0;
    if (D_actor_215100_8015E670 < 3) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
        mistShootingGallerySelectRoomLights(0);
    } else {
        mistShootingGallerySetTrainingBarrierLowered(1);
        D_actor_215100_8014D03C = 1;
    }
    if (D_actor_215100_8015E670 < 4) {
        Gp_StateC08.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
    }
    sndEvtRequestMidiStop(0, 0x1E);
}

void func_actor_215100_8014A9A0(void)
{
    if (D_actor_215100_8015E670 == 5) {
        D_actor_215100_8014D038 = 0;
        mistShootingGallerySetTrainingBarrierLowered(1);
        D_actor_215100_8014D03C          = 1;
        gGameSession->battleResetPending = 1;
        sndEvtRequestMidiStop(0, 0x1E);
        gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
    }
    if (D_actor_215100_8015E670 < 3) {
        capRunCommand(0x1D, CAP_PLAYBACK_CLEAR_IF_UNSTARTED);
        taskSpawnFromTable(D_actor_215100_8014CF6C, 1, 0, 0);
    }
}

/// Hands the actor off to its caption script, or starts one, depending on
/// whether the script for the current story flag has already run.
///
/// The `else` arm is a `do { } while (0)` whose `break` is the "already
/// committed" exit. It is not vestigial: the loop notes it emits make `reorg`
/// mark that branch's label as leaving a loop, so the delay-slot pass predicts
/// it not-taken and fills its slot from the fall-through rather than from the
/// shared `return 2` tail. Without the loop the branch reaches the same label
/// by a copied `li v0,2`, one instruction longer.
s32 func_actor_215100_8014AA54(RoomEventMsg* arg0)
{
    if (D_actor_215100_8014D038 != 0) {
        if (arg0->queryOnly != ROOM_EVENT_EXECUTE) {
            return 2;
        }
        D_actor_215100_8015E678 = *arg0;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        capRunCommand(0x14, CAP_PLAYBACK_IN_PLACE);
        D_80115690 = 1;
        taskSpawnFromTable(D_actor_215100_8014CF6C, 0, 1, 0);
    } else {
        do {
            if (gameFlagGetNibble(GAME_FLAG_0ED) == 0) {
                return 1;
            }
            if (arg0->queryOnly != ROOM_EVENT_EXECUTE) {
                break;
            }
            D_actor_215100_8015E678 = *arg0;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(0x17);
            taskSpawnFromTable(D_actor_215100_8014CF6C, 0, 2, 0);
        } while (0);
    }
    return 2;
}

void func_actor_215100_8014AB6C(void)
{
    if (D_actor_215100_8014D038 != 0) {
        func_mist_shooting_gallery_80184954();
        return;
    }
    capSpawnEventIfIdle(0x11, CAP_EVENT_PAUSE_ACTORS);
}

/// Runs the first-visit gallery introduction and hands control to its menu or exit scene.
///
/// Spawned on gallery entry with a live room overlay. Hides the HUD immediately;
/// an already-seen introduction goes straight to the menu. Otherwise the task
/// waits for each event script, uses the CAP reply to choose the menu or exit,
/// and kills itself after the handoff. Event scripts own HUD/control restoration.
static void _actor215100GalleryIntroTask(Task* task)
{
    enum {
        ACTOR_215100_GALLERY_INTRO_START         = 0,
        ACTOR_215100_GALLERY_INTRO_WAIT          = 1,
        ACTOR_215100_GALLERY_REPLY_START         = 2,
        ACTOR_215100_GALLERY_REPLY_WAIT          = 3,
        ACTOR_215100_GALLERY_EXIT_START          = 4,
        ACTOR_215100_GALLERY_MENU_REPLY_START    = 10,
        ACTOR_215100_GALLERY_MENU_REPLY_WAIT     = 11,
        ACTOR_215100_GALLERY_MENU_TASK           = 1,
        ACTOR_215100_GALLERY_MENU_REPLY_SEQUENCE = 7,
        ACTOR_215100_GALLERY_EXIT_REPLY_SEQUENCE = 8,
        ACTOR_215100_GALLERY_INTRO_OBJECTIVE     = 0x3A,
    };

    switch (task->state) {
        case ACTOR_215100_GALLERY_INTRO_START:
            // Mark the introduction before playback, so skipping still consumes it.
            gGameSession->hideHud = 1;
            if (gameFlagGetNibble(GAME_FLAG_SHOOTING_GALLERY_INTRO_SEEN) == 0) {
                gameFlagSetNibble(GAME_FLAG_SHOOTING_GALLERY_INTRO_SEEN, 1);
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACTOR_215100_GALLERY_INTRO_OBJECTIVE);
                evsStartScriptWithSkip(D_actor_215100_8014E370, EVENT_SCRIPT_HUD_KEEP, D_actor_215100_8014E8F8);
                task->state++;
            } else {
                taskSpawnFromTable(D_actor_215100_8014E13C, ACTOR_215100_GALLERY_MENU_TASK, 0, 0);
                taskKill(task);
            }
            break;
        case ACTOR_215100_GALLERY_INTRO_WAIT:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case ACTOR_215100_GALLERY_REPLY_START:
            evsStartScript(D_actor_215100_8014EA90, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR_215100_GALLERY_REPLY_WAIT:
            if (gGameSession->eventState == 0) {
                if (capGetVariantKey() != 0) {
                    task->state = ACTOR_215100_GALLERY_MENU_REPLY_START;
                } else {
                    task->state++;
                }
            }
            break;
        case ACTOR_215100_GALLERY_EXIT_START:
            capStartSequenceSlot(ACTOR_215100_GALLERY_EXIT_REPLY_SEQUENCE, 0, 0);
            evsStartScript(D_actor_215100_8014EBE0, EVENT_SCRIPT_HUD_KEEP);
            taskKill(task);
            break;
        case ACTOR_215100_GALLERY_MENU_REPLY_START:
            capStartSequenceSlot(ACTOR_215100_GALLERY_MENU_REPLY_SEQUENCE, 0, 0);
            evsStartScript(D_actor_215100_8014EB08, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR_215100_GALLERY_MENU_REPLY_WAIT:
            if (gGameSession->eventState == 0) {
                taskSpawnFromTable(D_actor_215100_8014E13C, ACTOR_215100_GALLERY_MENU_TASK, 0, 0);
                taskKill(task);
            }
            break;
    }
}

/// Spawns a gallery red flash whenever its elapsed counter is divisible by 48.
///
/// Includes the first tick. Uses the task's zero-initialized signed-halfword
/// counter as elapsed ticks;
/// increments retain halfword wrapping. Requires the gallery flash table to
/// remain loaded. No work is allocated; the event callback kills this task.
static void _actor215100GalleryRedFlashLoopTask(Task* task)
{
    enum {
        ACTOR_215100_GALLERY_RED_FLASH_PERIOD_TICKS = 48,
        ACTOR_215100_GALLERY_RED_FLASH_TASK         = 1,
    };

    if (task->killCountdown % ACTOR_215100_GALLERY_RED_FLASH_PERIOD_TICKS == 0) {
        taskSpawnFromTable(D_mist_shooting_gallery_801856B8, ACTOR_215100_GALLERY_RED_FLASH_TASK, 0, 0);
    }
    task->killCountdown = task->killCountdown + 1;
}

void func_actor_215100_8014ADD8(void)
{
    taskSpawnFromTable(D_mist_shooting_gallery_80185384, 0, 0, 0);
}

void func_actor_215100_8014AE08(s32 arg0)
{
    if (arg0 != 0) {
        func_mist_shooting_gallery_801848B4();
    }
}

/// Starts or stops the repeated red flashes used during the gallery introduction.
///
/// Nonzero spawns and publishes a loop task; zero kills the published task and
/// clears its handle. The script must stop an existing loop before starting
/// another: a repeated start overwrites the handle without killing its task.
/// Both actor and gallery overlays must remain loaded until the loop is stopped.
static void _actor215100SetGalleryRedFlashLoop(s32 enabled)
{
    enum { ACTOR_215100_GALLERY_RED_FLASH_LOOP_TASK = 2 };

    if (enabled != 0) {
        D_actor_215100_8015E64C = taskSpawnFromTable(D_actor_215100_8014E13C, ACTOR_215100_GALLERY_RED_FLASH_LOOP_TASK, 0, 0);
        return;
    }
    if (D_actor_215100_8015E64C != NULL) {
        taskKill(D_actor_215100_8015E64C);
        D_actor_215100_8015E64C = NULL;
    }
}

void func_actor_215100_8014AE90(s16 arg0)
{
    mistShootingGallerySelectRoomLights(arg0);
}

/// Sets the deferred view-respawn request when the introduction is skipped.
///
/// Stores the signed halfword unchanged; nonzero asks gameplay to respawn the
/// saved view when scene loading permits, and zero clears the request.
static void _actor215100SetViewRespawnPending(s16 pending)
{
    gGameSession->viewDirty = pending;
}

/// Selects the CAP resource and texture page for Pierce's conversation scenes.
///
/// Nonzero selects data-resource ordinal 1 and VRAM origin (768, 0); zero
/// restores the bundle's default CAP selection. Requires those resources loaded.
static void _actor215100SetConversationCap(s32 enabled)
{
    enum {
        ACTOR_215100_CONVERSATION_CAP_RESOURCE_ORDINAL = 1,
        ACTOR_215100_CONVERSATION_CAP_VRAM_X           = 768,
        ACTOR_215100_CONVERSATION_CAP_VRAM_Y           = 0,
    };

    if (enabled != 0) {
        Gp_CapFile = NULL;
        capSelectLoadedFile(ACTOR_215100_CONVERSATION_CAP_RESOURCE_ORDINAL);
        capSetTexturePage(ACTOR_215100_CONVERSATION_CAP_VRAM_X, ACTOR_215100_CONVERSATION_CAP_VRAM_Y);
        return;
    }
    capReset();
}

void actor215100StartPierceConversation(void)
{
    enum {
        ACTOR_215100_PIERCE_TALK_FIRST  = 0,
        ACTOR_215100_PIERCE_TALK_SECOND = 1,
        ACTOR_215100_PIERCE_TALK_REPEAT = 2,
    };

    switch (gameFlagGetNibble(GAME_FLAG_PIERCE_TALK_PROGRESS)) {
        case ACTOR_215100_PIERCE_TALK_FIRST:
            gameFlagSetNibble(GAME_FLAG_PIERCE_TALK_PROGRESS, ACTOR_215100_PIERCE_TALK_SECOND);
            evsStartScript(D_actor_215100_80153ED4, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
        case ACTOR_215100_PIERCE_TALK_SECOND:
            evsStartScript(D_actor_215100_80153FDC, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_PIERCE_TALK_PROGRESS, ACTOR_215100_PIERCE_TALK_REPEAT);
            break;
        case ACTOR_215100_PIERCE_TALK_REPEAT:
            evsStartScript(D_actor_215100_801543E4, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

#include "../../shared/cap_captions.inc.c"

static void func_actor_215100_8014C538(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_ShowTimed(arg0, arg1, arg2);
}

#include "../../shared/cap_captions_resource.inc.c"

void func_actor_215100_8014C5E0(s16 arg0, s16 arg1, s16 arg2)
{
    _capCaptionLoadResource(arg0, arg1, arg2);
}

/// State-0 handler of the actor's dispatcher: allocates the work block, spawns
/// the sub-model and adopts it as a child, takes the model's texture page and
/// CLUT from the area placement the enemy's `placeKey` selects, sets up the
/// animation context on clip 0xC, installs the message table whose handlers
/// are the actor's script opcodes, and starts the animation.
static void func_actor_215100_8014C660(Enemy* enemy, Task* task)
{
    VECTOR         vec;
    PacedWalkWork* work;
    PacedWalkWork* mem;
    GfxCoord*      coord;
    TmdObject*     obj;
    Enemy*         spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = memCalloc(sizeof(PacedWalkWork), false);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _pacedWalkExit;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                       = 0;
    obj->otOffset                    = 1;
    work->enemy                      = enemy;
    spawned                          = enemySpawnFromTable(D_actor_215100_8015E5D0, 1, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(spawned->task->extra.tmd, enemy);
    taskReparent(task, spawned->task);
    work->pairTask  = spawned->task;
    work->st.animId = 0xC;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, D_actor_215100_8015E5E8, obj,
                         work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = D_actor_215100_8015E5A0;
    _pacedWalkUpdate(task);
    task->state++;
}

#include "../../shared/paced_walk_update.inc.c"

/// Two-state dispatcher, its handler table built on the stack: state 0 spawns
/// the actor, state 1 runs it. Both handlers take the task's `Enemy` as
/// well as the task.
void func_actor_215100_8014CA2C(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_215100_8014C660,
        _actorRenderWalkerFrame,
    };

    fns[task->state](task->spawnArg2.pointer, task);
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
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the walker's enemy record and tears down its task tree.
///
/// Installed after the walker allocates its work. `spawnArg2.pointer` must
/// retain the live owning enemy through this callback; task teardown releases
/// the work, model and carried child. Neither pointer survives teardown.
static void _pacedWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

#include "../../shared/paced_walk_blend_anim.inc.c"

/// Records the requested clip and next reseed without changing heading or travel.
///
/// Borrows both pointers for the call. Clip and blend duration narrow to signed
/// halfwords. A nonzero blend selects interpolation; reset leaves the previous
/// duration untouched. Clears `st.field_6`, whose role is unproven.
static inline void _pacedWalkApplyAnimationRequest(PacedWalkWork* work, const AnimationPlayRequest* request)
{
    work->st.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
        work->blendFrames = request->blendFrames;
    } else {
        work->st.state = ACTOR_ENEMY_ANIM_RESET;
    }
    work->st.field_6 = 0;
}

/// Immediately reseeds the walker's non-root tracks from a scripted play request.
///
/// Requires a spawned TMD walker with live `PacedWalkWork`. The bank has loaded
/// clips 1..15 and 18..24; entries 0, 16 and 17 are NULL. The signed check rejects
/// only IDs >=25, so callers must exclude negative and unloaded IDs. The request
/// is borrowed through dispatch; clip storage must outlive playback on parts 1..19.
/// Nonzero blend captures old poses and narrows the duration to a signed halfword
/// in normal-rate frames; 0..2047 keeps blend time nonnegative. Reset ignores it.
/// Ignores the bank selector, collision choice, message ID and second payload.
/// Returns 0 after reseeding or -1 for a rejected ID without changing playback.
static s32 _pacedWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum {
        PACED_WALK_ANIMATION_ID_LIMIT         = ARRAY_SIZE(D_actor_215100_8015E5E8),
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

/// Replaces both model flags from paired-draw request bits, in receiver-first order.
///
/// Both TMD objects must be live and writable; pointers may alias. SHOW clears
/// all flags, its absence excludes active drawing, and SKIP_AUTO_BUFFER adds
/// automatic-buffer suppression. Other bits are ignored; no buffers are changed.
static inline void _pacedWalkReplacePairDrawFlags(TmdObject* walkerModel, TmdObject* pairModel, s32 requestFlags)
{
    enum { PACED_WALK_PAIR_MODEL_NO_FLAGS = 0 };

    if (requestFlags & ACTOR_MESSAGE_PAIR_SHOW) {
        walkerModel->flags = PACED_WALK_PAIR_MODEL_NO_FLAGS;
        pairModel->flags   = PACED_WALK_PAIR_MODEL_NO_FLAGS;
    } else {
        walkerModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        pairModel->flags   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (requestFlags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        walkerModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        pairModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
}

/// Replaces the walker and its carried model's draw flags together.
///
/// Requires live TMD models on the receiver and its `PacedWalkWork::pairTask`,
/// regardless of spawn arguments. Uses `ACTOR_MESSAGE_PAIR_*` request bits:
/// SHOW clears all flags; without it, both exclude active drawing. The separate
/// SKIP_AUTO_BUFFER bit adds that object flag. Other bits are ignored. Does not
/// allocate or release buffers. Ignores the message ID and second payload; returns 0.
static s32 _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 requestFlags, s32 unusedArgument)
{
    PacedWalkWork* work;
    TmdObject*     walkerModel;
    TmdObject*     pairModel;

    walkerModel = task->extra.tmd;
    work        = task->work;
    pairModel   = work->pairTask->extra.tmd;
    _pacedWalkReplacePairDrawFlags(walkerModel, pairModel, requestFlags);
    return 0;
}

#include "../../shared/paced_walk_place.inc.c"

/// Ignores actor commands without changing the walker.
///
/// Retains the four-word ABI of `ACTOR_COMMAND_MESSAGE_APPLY`. The receiver,
/// borrowed command and other argument words are never read or retained; returns 0.
static s32 _actor215100IgnoreCommand(Task* task, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument)
{
    return 0;
}

#include "../../shared/paced_walk_to.inc.c"

/// Links a carried model to a walker part and borrows the walker's lighting.
///
/// Both the part coordinate and work matrices must outlive the child. Keeps its
/// root's local transform, invalidates composition and clears all model flags.
/// Requires live writable storage and an acyclic coordinate ancestry.
static inline void _pacedWalkAttachCarriedModel(TmdObject* carriedModel, GfxCoord* attachmentCoord, PacedWalkWork* parentWork)
{
    enum { PACED_WALK_CARRIED_MODEL_NO_FLAGS = 0 };
    GfxCoord* rootCoord = carriedModel->coords;

    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    carriedModel->lightMtx  = &parentWork->light;
    carriedModel->flags     = PACED_WALK_CARRIED_MODEL_NO_FLAGS;
    carriedModel->colorMtx  = &parentWork->color;
    rootCoord->parent       = attachmentCoord;
}

/// Attaches the carried model to walker part 4 and refreshes its root composition.
///
/// Requires a live TMD child task under a spawned walker with `PacedWalkWork`
/// and at least five model coordinates. State 0 borrows part 4 and the parent's
/// light matrices, clears all flags and enters state 1. Both states mark the
/// root dirty; other states do nothing. The parent's model and work must outlive
/// this child, which belongs to the parent's task teardown tree.
static void _pacedWalkSubModelTask(Task* task)
{
    enum {
        PACED_WALK_SUB_MODEL_ATTACH = 0,
        PACED_WALK_SUB_MODEL_FOLLOW = 1,
        PACED_WALK_ATTACHMENT_PART  = 4,
    };
    // The original unused reservation retains the shared return epilogue.
    char           unusedStackFrame[0x10];
    Task*          parentTask      = task->parent;
    TmdObject*     carriedModel    = task->extra.tmd;
    GfxCoord*      rootCoord       = carriedModel->coords;
    GfxCoord*      attachmentCoord = &parentTask->extra.tmd->coords[PACED_WALK_ATTACHMENT_PART];
    PacedWalkWork* parentWork      = parentTask->work;

    switch (task->state) {
        case PACED_WALK_SUB_MODEL_ATTACH:
            _pacedWalkAttachCarriedModel(carriedModel, attachmentCoord, parentWork);
            task->state++;
            break;
        case PACED_WALK_SUB_MODEL_FOLLOW:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
