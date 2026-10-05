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

/// The clips the shooting gallery's session scenes add to the player's
/// animation bank, with the actor play requests stored after them.
///
/// The event scripts of the gallery's introduction and session set-up send the
/// player a copy request for this storage before they play one of these clips;
/// only the introduction's second script plays one without sending the request
/// itself. The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words from the
/// start of the storage, which is more than the clip table holds: the eleven
/// table words occupy extended ids 47-57, and the first 21 words of the play
/// requests are written into the bank after them. The player's requests select
/// ids 47-52 only, so none of those request words is played as a clip.
///
/// The play requests are not the player's. They are the first five of the run
/// of requests the scripts send to this package's own actor, the scene's second
/// placed actor, and are part of this object only because the copied span
/// reaches over them; the rest of the run follows as separate objects.
typedef union {
    struct {
        AnimationSet*        sets[11];             // Player clips for extended ids 47-57; NULL at the four ids (53-56) nothing requests, and no script sends the request for 57
        AnimationPlayRequest actorPlayRequests[5]; // Requests for animation ids 0 and 18-21 of the package's actor; nothing references the first
    } data;                                        // The records by name
    s32 words[36];                                 // The same storage as the copy reads it; the last four words lie beyond the copied span
} _Actor215100GallerySessionAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor215100GallerySessionAnimationBankExtensionStorage, 144);

extern _Actor215100GallerySessionAnimationBankExtensionStorage D_actor_215100_8014E160;

/// The clips the actor's conversation scenes add to the player's animation
/// bank, with the actor play requests stored after them.
///
/// The two captioned scenes that play these clips each send the player a copy
/// request for this storage before the first of them. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is more than the clip table holds: the twenty table words occupy
/// extended ids 47-66, and the first twelve words of the play requests are
/// written into the bank after them. The player's requests select ids 47-56
/// in the first scene and 60-66 in the second, so none of those request words
/// is played as a clip.
///
/// The play requests are not the player's. They are the first three of the run
/// of requests the scripts send to this package's own actor, the scene's second
/// placed actor, and are part of this object only because the copied span
/// reaches over them; the rest of the run follows as separate objects.
typedef union {
    struct {
        AnimationSet*        sets[20];             // Player clips for extended ids 47-66; NULL at the two ids (57-58) nothing requests, and no script sends the request for 59
        AnimationPlayRequest actorPlayRequests[3]; // Requests for animation ids 0-2 of the package's actor; nothing references the first
    } data;                                        // The records by name
    s32 words[35];                                 // The same storage as the copy reads it; the last three words lie beyond the copied span
} _Actor215100ConversationAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor215100ConversationAnimationBankExtensionStorage, 140);

extern _Actor215100ConversationAnimationBankExtensionStorage D_actor_215100_80152EAC;

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
static u8 CapCaption_Data_8015E66C[4];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void func_actor_215100_8014CA80(Enemy* enemy, Task* task);
static void func_actor_215100_8014CB04(Task* task);

/* cap captions instance: retain the original overlay symbols. */
static void func_actor_215100_8014C538(s16 arg0, s16 arg1, s16 arg2);
void        func_actor_215100_8014C5E0(s16 arg0, s16 arg1, s16 arg2);
// Exported instances: mist_shooting_gallery's modal caption calls this
// package's script selector, and its caption task this package's drawer.
#define CAP_CAPTION_SELECT_SCRIPT_LINKAGE
#define CapCaption_SelectScript actor215100CapCaptionSelectScript
#define CAP_CAPTION_DRAW_CURRENT_LINKAGE
#define CapCaption_DrawCurrent actor215100CapCaptionDrawCurrent
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

extern TaskMessageEntry D_actor_215100_8015E5A0[];
/// Glyph metrics table this overlay's caption metrics are read out of, the
/// counterpart of gameplay's `Gp_CapGlyphs`. `func_actor_215100_8014B1B0`
/// stores it and `func_actor_215100_8014C360` indexes it with a text stream's
/// `code & 0x3FF`.
static TextGlyphCell* CapCaption_Data_8015E654;

/// Caption script table, and the script currently being played back with the
/// entry it is up to.
static CapCommandRef*     CapCaption_Data_8015E650;
static CapSequenceRecord* CapCaption_Data_8015E658;
static s16                CapCaption_Data_8015E65C;
static s16                CapCaption_Data_8015E65E;
static s16                CapCaption_Data_8015E660;
static s16                CapCaption_Data_8015E662;
static s16                CapCaption_Data_8015E664;
static s16                CapCaption_Data_8015E666;
/// Frames left before the caret starts drawing.
/// Caret grey level (pulses between 9 and 15) and its direction flag.
static s32 CapCaption_Data_801545E4;
static s32 CapCaption_Data_801545E8;
/// Caret position.
static u16          CapCaption_Data_8015E668;
static u16          CapCaption_Data_8015E66A;
static s16          CapCaption_Data_801544EC;
static s16          CapCaption_Data_801544EE;
extern RoomEventMsg D_actor_215100_8015E678;
/// Caption schedule `func_actor_215100_8014AFAC` scans, terminated by an
/// `upper` of `CAP_CAPTION_SCHEDULE_END`.
static CapCaptionScheduleWindow CapCaption_Data_80154514[];

static TmdSource _gActor215100PierceCarradineBody;
static TmdSource _gActor215100Actor113100Model07960;
s32              func_actor_215100_8014CCE0(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_215100_8014CD4C(Task*, s32, s32, s32);
s32              func_actor_215100_8014CE28(Task*, s32, s32, s32);
void             func_actor_215100_8014CA2C(Task*);
void             func_actor_215100_8014CEF8(Task*);

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

void func_actor_215100_8014AEC4(s32);

void func_actor_215100_8014ABAC(Task*);
void func_actor_215100_8014AD50(Task*);
void func_actor_215100_8014ADD8(void);
void func_actor_215100_8014AE08(s32);
void func_actor_215100_8014AE2C(s32);
void func_actor_215100_8014AE90(s16);
void func_actor_215100_8014AEB4(s16);

TaskDesc D_actor_215100_8014E13C[3] = {
    { { { TASK_BODY_NONE, 32 } }, func_actor_215100_8014ABAC, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_actor_215100_80149F2C, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_actor_215100_8014AD50, { .value = 0 } },
};

_Actor215100GallerySessionAnimationBankExtensionStorage D_actor_215100_8014E160 = { .data = { { &gActor215100Animation034E4, &gActor215100Animation03754, &gActor215100Animation039AC, &gActor215100Animation03B48, &gActor215100Animation03D98, &gActor215100Animation03FF0, NULL, NULL, NULL, NULL, &gActor215100Animation042F4 }, { { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

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

AnimationBankCopyRequest D_actor_215100_8014E2B8 = { { .words = D_actor_215100_8014E160.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AE2C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5114000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AE2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E268 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[3] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E27C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[4] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E1F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E290 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_8014E328 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[3] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 44 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[1] } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_215100_8014AEB4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 80 }, { .value = 80 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AE2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_8014E254 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_215100_8014E310 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[1] } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[1] } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[1] } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[1] } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[3] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_8014E160.data.actorPlayRequests[1] } }, { .value = 0 } },
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

_Actor215100ConversationAnimationBankExtensionStorage D_actor_215100_80152EAC = { .data = { { &_gActor215100Animation075B4, &_gActor215100Animation07794, &_gActor215100Animation07960, &_gActor215100Animation07E28, &_gActor215100Animation07FFC, &_gActor215100Animation08400, &_gActor215100Animation08788, &_gActor215100Animation08BE0, &_gActor215100Animation08EBC, &_gActor215100Animation09064, NULL, NULL, &_gActor215100Animation05738, &_gActor215100Animation05B0C, &_gActor215100Animation05EFC, &_gActor215100Animation06268, &_gActor215100Animation066B4, &_gActor215100Animation069DC, &_gActor215100Animation06F5C, &_gActor215100Animation0733C }, { { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

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

AnimationBankCopyRequest D_actor_215100_801531CC = { { .words = D_actor_215100_80152EAC.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorTransform D_actor_215100_801531D4 = { { -0x27F6, 0, 4640, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_215100_801531EC = { { -0x2BC0, 0, 3000, 0 }, { 0, -2218, 0, 0 } };

ActorTransform D_actor_215100_80153204 = { { -0x27F6, 0, 5000, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_215100_8015321C = { { -0x295E, 0, 2100, 0 }, { 0, -56, 0, 0 } };

ActorTransform D_actor_215100_80153234 = { { -0x29FA, 0, 3972, 0 }, { 0, 0, 0, 0 } };

AnimationPlayRequest D_actor_215100_8015324C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_215100_80153260 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_actor_215100_80153274[117] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AEC4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152EAC.data.actorPlayRequests[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5114000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152EAC.data.actorPlayRequests[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 112 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152EAC.data.actorPlayRequests[1] } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_80153ED4[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AEC4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153014 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153028 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_80153FDC[43] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AEC4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_215100_801543E4[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AEC4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_215100_80153260 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153014 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80153028 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_215100_80152FEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_215100_8014AEC4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_215100_8014CCE0 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_215100_8014CD4C },
    { ACTOR_MESSAGE_PLACE, pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_215100_8014CE28 },
    { ACTOR_MESSAGE_WALK_TO, pacedWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_215100_8015E5D0[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_215100_8014CA2C, { .model = &_gActor215100PierceCarradineBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_215100_8014CEF8, { .model = &_gActor215100Actor113100Model07960 } },
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

static TextGlyphCell* CapCaption_Data_8015E654 = NULL;

static CapSequenceRecord* CapCaption_Data_8015E658 = NULL;

static s16 CapCaption_Data_8015E65C = 0;

static s16 CapCaption_Data_8015E65E = 0;

static s16 CapCaption_Data_8015E660 = 0;

static s16 CapCaption_Data_8015E662 = 0;

static s16 CapCaption_Data_8015E664 = 0;

static s16 CapCaption_Data_8015E666 = 0;

static u16 CapCaption_Data_8015E668 = 0;

static u16 CapCaption_Data_8015E66A = 0;

static u8 CapCaption_Data_8015E66C[4] = {
    0,
    35,
    192,
    0,
};

s32 D_actor_215100_8015E670;

RoomEventMsg D_actor_215100_8015E678;

void func_actor_215100_8014A398(void);
void func_actor_215100_8014A908(void);
void func_actor_215100_8014A9A0(void);
s32  func_actor_215100_8014AA54(RoomEventMsg* arg0);
void func_actor_215100_8014AB6C(void);
void func_actor_215100_8014AF0C(void);

static void func_actor_215100_8014C660(Enemy* enemy, Task* task);

/// Arms the weapon pickup at this actor's spot while the event flag
/// `D_actor_215100_8014D038` is up and the story step has reached 3. A session
/// leave (`gGameSession->location.loc.view == 0x12`) drops the `func_mist_shooting_gallery_80180390` hold and
/// `D_actor_215100_8014D03C` with it, sub-states 2 and 3 of
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
                func_mist_shooting_gallery_80180390(0);
                D_actor_215100_8014D03C = 0;
            }
            if ((u32)((u8)Gp_StateC08.mode - ATTACHMENT_MODE_ARMED) < 2U) {
                D_actor_215100_8014D044 = 0x3C;
            }
            if (D_actor_215100_8014D044 != 0) {
                D_actor_215100_8014D044 -= 1;
                return;
            }
            if ((actor->mode != GAME_ACTOR_MODE_SCRIPTED) && (Gp_CapBusy() == 0) && (D_actor_215100_8014D03C == 0) &&
                (D_80115768 == 0) && (coord->coord.t[0] < -0x1806)) {
                z = coord->coord.t[2];
                if (z < 0x1644) {
                    if ((z >= 0x10CD) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                        facing = (u16)actor->rotation.vy & 0xFFF;
                        if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP) != 0) {
                            if ((u32)(facing - 0xA01) < 0x3FFU) {
                                Gp_MsgPlayerWeapon(0);
                                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
                                Gp_RunCapCmd(0x14, 0);
                                D_80115690 = 1;
                                taskSpawnFromTable(D_actor_215100_8014CF6C, 0, 0, 0);
                            }
                        }
                        if ((padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_DOWN) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                            Gp_MsgPlayerWeapon(0);
                            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
                            Gp_RunCapCmd(0x14, 0);
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
/// `Gp_CapBusy` to drop and switches on the key `Gp_GetCapEventKey` returns.
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
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_GetCapEventKey() == 1) {
                Gp_MsgPlayerWeapon(1);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(arg0);
                break;
            }
            if (arg0->spawnArg1.value != 0) {
                if (gameFlagGetNibble(GAME_FLAG_0ED) != 0) {
                    Gp_RunCapCmd1(0x17);
                }
                gGameSession->battleResetPending = 1;
                arg0->state                     += 1;
                break;
            }
            if (D_actor_215100_8015E670 == 3) {
                Gp_StateC08.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
            }
            D_actor_215100_8014D038 = 0;
            func_mist_shooting_gallery_80180390(1);
            D_actor_215100_8014D03C = 1;
            taskCallExit(D_mist_shooting_gallery_8018E0C4);
            gGameSession->battleResetPending = 1;
            gGameSession->flowFlags         |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            SndEvt_EnqueueType2(0, 0x1E);
            Gp_MsgPlayerWeapon(1);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            taskKill(arg0);
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                gPlayerStatus.resourceVariant                       = 3;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 1;
                Gp_ClearInventory();
                gGameSession->hideHud = 1;
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 5), 0, 0);
                gDisplayState.spriteVariant                                = 1;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_actor_215100_8015E678.areaId;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_actor_215100_8015E678.warp;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = D_actor_215100_8015E678.room;
                Task_Spawn(0, 0x11, 0, 0);
                taskKill(arg0);
            }
            break;
    }
}

/// Watches the caption system while the actor waits to be talked to: state 0
/// polls `Gp_CapBusy` / `Gp_GetCapEventKey`, and on key 2 hands the scene task
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
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_GetCapEventKey() == 2) {
                taskCallExit(D_mist_shooting_gallery_8018E0C4);
                arg0->state++;
            } else {
                taskKill(arg0);
            }
            break;
        case 1:
            Gp_MsgPlayerWeapon(0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
            func_mist_shooting_gallery_801811C0(0);
            arg0->state++;
            break;
        case 2:
            gGameSession->battleResetPending = 1;
            gGameSession->flowFlags         |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            SndEvt_EnqueueType2(0, 0x1E);
            actor->movementInputDisabled = 0;
            D_actor_215100_8014D038      = 0;
            Gp_MsgPlayerWeapon(1);
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
        func_mist_shooting_gallery_801811C0(0);
    } else {
        func_mist_shooting_gallery_80180390(1);
        D_actor_215100_8014D03C = 1;
    }
    if (D_actor_215100_8015E670 < 4) {
        Gp_StateC08.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
    }
    SndEvt_EnqueueType2(0, 0x1E);
}

void func_actor_215100_8014A9A0(void)
{
    if (D_actor_215100_8015E670 == 5) {
        D_actor_215100_8014D038 = 0;
        func_mist_shooting_gallery_80180390(1);
        D_actor_215100_8014D03C          = 1;
        gGameSession->battleResetPending = 1;
        SndEvt_EnqueueType2(0, 0x1E);
        gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
    }
    if (D_actor_215100_8015E670 < 3) {
        Gp_RunCapCmd(0x1D, 3);
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
        Gp_MsgPlayerWeapon(0);
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        Gp_RunCapCmd(0x14, 0);
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
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(0x17);
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
    Gp_SpawnIfCapIdle(0x11, 1);
}

void func_actor_215100_8014ABAC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gGameSession->hideHud = 1;
            if (gameFlagGetNibble(GAME_FLAG_SHOOTING_GALLERY_INTRO_SEEN) == 0) {
                gameFlagSetNibble(GAME_FLAG_SHOOTING_GALLERY_INTRO_SEEN, 1);
                func_800E3FAC(0xA2, 0x3A);
                func_800E8634(D_actor_215100_8014E370, 1, D_actor_215100_8014E8F8);
                arg0->state++;
            } else {
                taskSpawnFromTable(D_actor_215100_8014E13C, 1, 0, 0);
                taskKill(arg0);
            }
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                arg0->state++;
            }
            break;
        case 2:
            func_800E8614(D_actor_215100_8014EA90, 1);
            arg0->state++;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                if (Gp_GetCapEventKey() != 0) {
                    arg0->state = 10;
                } else {
                    arg0->state++;
                }
            }
            break;
        case 4:
            Gp_StartCapSlot(8, 0, 0);
            func_800E8614(D_actor_215100_8014EBE0, 1);
            taskKill(arg0);
            break;
        case 10:
            Gp_StartCapSlot(7, 0, 0);
            func_800E8614(D_actor_215100_8014EB08, 1);
            arg0->state++;
            break;
        case 11:
            if (gGameSession->eventState == 0) {
                taskSpawnFromTable(D_actor_215100_8014E13C, 1, 0, 0);
                taskKill(arg0);
            }
            break;
    }
}

void func_actor_215100_8014AD50(Task* arg0)
{
    if (arg0->killCountdown % 48 == 0) {
        taskSpawnFromTable(D_mist_shooting_gallery_801856B8, 1, 0, 0);
    }
    arg0->killCountdown = arg0->killCountdown + 1;
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

void func_actor_215100_8014AE2C(s32 arg0)
{
    if (arg0 != 0) {
        D_actor_215100_8015E64C = taskSpawnFromTable(D_actor_215100_8014E13C, 2, 0, 0);
        return;
    }
    if (D_actor_215100_8015E64C != NULL) {
        taskKill(D_actor_215100_8015E64C);
        D_actor_215100_8015E64C = NULL;
    }
}

void func_actor_215100_8014AE90(s16 arg0)
{
    func_mist_shooting_gallery_801811C0(arg0);
}

void func_actor_215100_8014AEB4(s16 arg0)
{
    gGameSession->viewDirty = arg0;
}

void func_actor_215100_8014AEC4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x300, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_215100_8014AF0C(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_PIERCE_TALK_PROGRESS)) {
        case 0:
            gameFlagSetNibble(GAME_FLAG_PIERCE_TALK_PROGRESS, 1);
            func_800E8614(D_actor_215100_80153ED4, 0);
            break;
        case 1:
            func_800E8614(D_actor_215100_80153FDC, 0);
            gameFlagSetNibble(GAME_FLAG_PIERCE_TALK_PROGRESS, 2);
            break;
        case 2:
            func_800E8614(D_actor_215100_801543E4, 0);
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
    CapCaption_LoadResource(arg0, arg1, arg2);
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
    task->exitCallback               = func_actor_215100_8014CB04;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                       = 0;
    obj->otOffset                    = 1;
    work->enemy                      = enemy;
    spawned                          = Gp_SpawnEnemyFromTable(D_actor_215100_8015E5D0, 1, 0, enemy);
    actorTintModel(spawned->task->extra.tmd, enemy);
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
    pacedWalkUpdate(task);
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
        func_actor_215100_8014CA80,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_215100_8014CA80
#define walkerUpdate     pacedWalkUpdate
#define walkerDrawShadow walkerDrawShadow
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback: hands the task's `Enemy` back to `enemyDestroy`.
static void func_actor_215100_8014CB04(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/walker_shadow.inc.c"

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

#include "../../shared/paced_walk_blend_anim.inc.c"

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0x19 and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_215100_8014CCE0(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    PacedWalkWork* work;

    work = task->work;
    if (args->animationId < 0x19) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = args->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        pacedWalkUpdate(task);
        return 0;
    }
    return -1;
}

/// Script opcode: sets the visibility flags of the actor's model and of the
/// model of the enemy spawned alongside it. `flags` bit 0 shows both
/// (`TmdObject::flags` 0) and its absence hides them with
/// `TMD_OBJECT_SKIP_ACTIVE_DRAW`. Bit 1 also sets `TMD_OBJECT_SKIP_AUTO_BUFFER`.
/// The middle argument is the one every opcode of the table receives.
s32 func_actor_215100_8014CD4C(Task* task, s32 arg1, s32 flags, s32 arg3)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((PacedWalkWork*)task->work)->pairTask->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        other->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (flags & 2) {
        self->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        other->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/paced_walk_place.inc.c"

/// Script opcode that does nothing.
s32 func_actor_215100_8014CE28(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

#include "../../shared/paced_walk_to.inc.c"

/// Handler of the sub-model the actor spawns and adopts as its child. On the
/// first frame it points the sub-model's light and colour matrices at the
/// parent's, makes it visible with `flags` 0 and parents its root coordinate
/// to part 4 of the parent's model; every frame it clears the coordinate's
/// `composeStamp` so it is recomputed from that part.
void func_actor_215100_8014CEF8(Task* task)
{
    char           pad[0x10];
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra.tmd;
    GfxCoord*      coord  = obj->coords;
    GfxCoord*      sub    = &parent->extra.tmd->coords[4];
    PacedWalkWork* work   = parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = &work->light;
            obj->flags          = 0;
            obj->colorMtx       = &work->color;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
