#include "rooms/dryfield_night_trailer_coach.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "dryfield_night_trailer_coach_private.h"

#include "actors/actor_420700.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/areas.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/sound_ids.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_cutscene.h"

static void _dryfieldNightTrailerCoachDrawGlowCapsule(const SVECTOR worldPoints[2], s32 radiusScale);

static WorldCollisionGrid         _gDryfieldNightTrailerCoachCollisionGrid;
static WorldCollisionTrigger      _gDryfieldNightTrailerCoachViewBoundaryTriggers[4];
static WorldCollisionTrigger      _gDryfieldNightTrailerCoachActionTriggers[14];
static WorldCoordRoomAmbientEntry _gDryfieldNightTrailerCoachRoomAmbient[9];
static WorldCoordRoomLights       _gDryfieldNightTrailerCoachRoomLights;

/// CAP slots and presentation settings used by the room's dialogue scripts.
enum {
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_REPLY           = 11,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_STORY           = 12,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_STORY_REPEAT    = 13,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_STORY_SKIPPED   = 17,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_WARP3_ARRIVAL   = 25,
    DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_VIEW       = 3,
    DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_AMBIENT    = 100, // 8-bit RGB level; EVS converts to 16 units per level
    DRYFIELD_NIGHT_TRAILER_COACH_STORY_CLOSING_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 10),
};

EvsCommand gDryfieldNightTrailerCoachCap11ReplyScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_REPLY }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gDryfieldNightTrailerCoachStoryRepeatScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_STORY_REPEAT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gDryfieldNightTrailerCoachStoryScript[57] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_STORY }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gDryfieldNightTrailerCoachPlayerDialogueBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &gDryfieldNightTrailerCoachPlayerDialogueTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_AMBIENT }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_AMBIENT }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_AMBIENT }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryStoryLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerStoryDialogueAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryStoryReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerStoryDialogueAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryStoryReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerStoryReplyAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryStoryReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerStoryDialogueAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueFollowupAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerStoryClosingLeadInAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryStoryClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_NIGHT_TRAILER_COACH_STORY_CLOSING_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerStoryClosingAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gDryfieldNightTrailerCoachStorySkipScript[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &gDryfieldNightTrailerCoachPlayerDialogueTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_STORY_SKIPPED }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gDryfieldNightTrailerCoachWarp3ArrivalScript[24] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_CAP_WARP3_ARRIVAL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gDryfieldNightTrailerCoachPlayerDialogueBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &gDryfieldNightTrailerCoachPlayerDialogueTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_AMBIENT }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_AMBIENT }, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_AMBIENT }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueLeadInAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueHoldAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryDialogueFollowupAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerChapter4DialogueAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gDryfieldNightTrailerCoachWarp3ArrivalSkipScript[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gDryfieldNightTrailerCoachGaryTalkRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gDryfieldNightTrailerCoachPlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_TRAILER_COACH_DIALOGUE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// World-space centre of the cyan diamond in views 2/5/7 and disc in view 8.
///
/// Coordinates are signed game units, borrowed while the room is loaded.
static SVECTOR _gDryfieldNightTrailerCoachGlowMarkerPoint = { 163, -1532, -766, 0 };

/// Eight glow-strip endpoint pairs drawn in view 3; views 2/5/7 use the first four.
///
/// Signed world-coordinate game units. Each pair supplies both capsule ends;
/// all eight strips use radius scale 256. Borrowed during drawing while loaded.
static SVECTOR _gDryfieldNightTrailerCoachView3GlowStripPoints[8][2] = {
    { { 2035, -2313, -657, 0 }, { 3165, -2313, -657, 0 } },
    { { 2035, -2313, -762, 0 }, { 3165, -2313, -762, 0 } },
    { { 2035, -2313, -2482, 0 }, { 3165, -2313, -2482, 0 } },
    { { 2035, -2313, -2587, 0 }, { 3165, -2313, -2587, 0 } },
    { { 4435, -2313, -657, 0 }, { 5565, -2313, -657, 0 } },
    { { 4435, -2313, -762, 0 }, { 5565, -2313, -762, 0 } },
    { { 4435, -2313, -2482, 0 }, { 5565, -2313, -2482, 0 } },
    { { 4435, -2313, -2587, 0 }, { 5565, -2313, -2587, 0 } },
};

/// Eight glow-strip endpoint pairs drawn in views 3 and 4.
///
/// Signed world-coordinate game units. Each pair supplies both capsule ends;
/// pairs 0..3 use radius scale 256 and pairs 4..7 use 192. Borrowed while loaded.
static SVECTOR _gDryfieldNightTrailerCoachView4GlowStripPoints[8][2] = {
    { { 6835, -2313, -657, 0 }, { 7965, -2313, -657, 0 } },
    { { 6835, -2313, -762, 0 }, { 7965, -2313, -762, 0 } },
    { { 6835, -2313, -2482, 0 }, { 7965, -2313, -2482, 0 } },
    { { 6835, -2313, -2587, 0 }, { 7965, -2313, -2587, 0 } },
    { { 6020, -1883, -548, 0 }, { 6975, -1883, -548, 0 } },
    { { 6020, -1883, -656, 0 }, { 6975, -1883, -656, 0 } },
    { { 7015, -1883, -548, 0 }, { 7960, -1883, -548, 0 } },
    { { 7015, -1883, -656, 0 }, { 7960, -1883, -656, 0 } },
};

WorldCoordRoomLighting gDryfieldNightTrailerCoachRoomLighting[1] = {
    { &_gDryfieldNightTrailerCoachRoomLights, _gDryfieldNightTrailerCoachRoomAmbient },
};

WorldCollisionRoomResources gDryfieldNightTrailerCoachRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldNightTrailerCoachCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightTrailerCoachViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightTrailerCoachActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldNightTrailerCoachViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightTrailerCoachViewCounts[1] = { ARRAY_SIZE(gDryfieldNightTrailerCoachViewCameras) };

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 2),
    DRYFIELD_NIGHT_TRAILER_COACH_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH, 1),
    DRYFIELD_NIGHT_TRAILER_COACH_MAP_MARK_FLAG   = 471,
};

DirectionWarpEntry gDryfieldNightTrailerCoachWarpEntries[3] = {
    {
        .player         = { { .word = ONE / 2 }, 1579, 0, -800 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ONE / 2 }, 1579, 0, -800 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_TRAILER_COACH_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_TRAILER_COACH_MAP_MARK_FLAG,
    },
    {
        .player         = { { .word = ONE / 2 }, 1579, 0, -800 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ONE / 2 }, 1579, 0, -800 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_TRAILER_COACH_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_TRAILER_COACH_MAP_MARK_FLAG,
    },
    {
        .player         = { { .word = ONE / 2 }, 1579, 0, -800 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ONE / 2 }, 1579, 0, -800 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_NIGHT_TRAILER_COACH_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_TRAILER_COACH_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_TRAILER_COACH_MAP_MARK_FLAG,
    },
};

/// Room-axis unit normals for the coach collision mesh, with 4096 for unit length.
static SVECTOR _gDryfieldNightTrailerCoachCollisionGridNormals[10] = {
#include "assets/dryfield_night_trailer_coach_collision_0C460_normals.inc"
};

/// Collision-mesh vertices in signed room-coordinate game units.
static SVECTOR _gDryfieldNightTrailerCoachCollisionGridVerts[65] = {
#include "assets/dryfield_night_trailer_coach_collision_0C460_verts.inc"
};

/// Coach collision quads indexing the vertex/normal pools and surface classes 5/6.
static WorldCollisionGridFace _gDryfieldNightTrailerCoachCollisionGridFaces[32] = {
#include "assets/dryfield_night_trailer_coach_collision_0C460_faces.inc"
};

/// Signed face-index lists for the three cells, each ending at -1.
///
/// Lists occupy elements 0..16, 18..37 and 38..57; element 17 is retained,
/// unreferenced storage from the serialized asset.
static s16 _gDryfieldNightTrailerCoachCollisionGridCells[58] = {
#include "assets/dryfield_night_trailer_coach_collision_0C460_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightTrailerCoachCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..57 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_TRAILER_COACH_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightTrailerCoachCollisionGridCells[(halfwordOffset)])
/// X-major cell-list pointers for the coach's three-by-one collision grid.
static s16* _gDryfieldNightTrailerCoachCollisionGridTable[3] = {
#include "assets/dryfield_night_trailer_coach_collision_0C460_table.inc"
};
#undef DRYFIELD_NIGHT_TRAILER_COACH_CELL_FACE_LIST_AT_OFFSET

/// The coach's room collision mesh, partitioned into three 4000-unit XZ cells.
///
/// Origin is (-200, -3450) on X/Z. Borrows 10 normals, 65 vertices and
/// 32 quads; activation binds the view coordinate. All pools stay writable
/// and live with the loaded room. Class 6 is the y=0 floor; other quads use 5.
static WorldCollisionGrid _gDryfieldNightTrailerCoachCollisionGrid = {
    NULL,
    _gDryfieldNightTrailerCoachCollisionGridNormals,
    _gDryfieldNightTrailerCoachCollisionGridVerts,
    _gDryfieldNightTrailerCoachCollisionGridFaces,
    _gDryfieldNightTrailerCoachCollisionGridTable,
    200,
    3450,
    ARRAY_SIZE(_gDryfieldNightTrailerCoachCollisionGridTable),
    1,
    4000,
    ARRAY_SIZE(_gDryfieldNightTrailerCoachCollisionGridFaces),
};

ViewCamera gDryfieldNightTrailerCoachViewCameras[8] = {
    { .transform = { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -5000, 10000, 1625 } }, .screenDistance = 230 },
    { .transform = { { { 373, 0, 4078 }, { 877, 4000, -80 }, { -3983, 881, 364 } }, { -5900, 1690, 2010 } }, .screenDistance = 230 },
    { .transform = { { { 879, 0, -4000 }, { -892, 3992, -196 }, { 3899, 913, 857 } }, { -1154, 1800, 2260 } }, .screenDistance = 230 },
    { .transform = { { { 595, 0, -4052 }, { -1245, 3897, -183 }, { 3856, 1258, 567 } }, { -6104, 1950, 2060 } }, .screenDistance = 230 },
    { .transform = { { { -450, 0, 4071 }, { 108, 4094, 12 }, { -4069, 109, -450 } }, { -6545, 1155, 1159 } }, .screenDistance = 230 },
    { .transform = { { { 1505, 0, 3809 }, { 2177, 3360, -860 }, { -3125, 2341, 1234 } }, { -5995, 2125, 2234 } }, .screenDistance = 230 },
    { .transform = { { { -864, 0, 4003 }, { -230, 4089, -49 }, { -3997, -236, -862 } }, { -5990, 545, 1174 } }, .screenDistance = 257 },
    { .transform = { { { 1613, 0, 3764 }, { 1597, 3709, -684 }, { -3409, 1737, 1460 } }, { -525, 1650, 866 } }, .screenDistance = 230 },
};

/// Encoded GPU pages/palette for the coach's 8-bit view sprites and neutral RGB.
enum {
    DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_896      = 142,    // Page at VRAM (896, 0)
    DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960      = 143,    // Page at VRAM (960, 0)
    DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT          = 0x3FC0, // Palette at VRAM (0, 255)
    DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR = 128,
};

/// Empty sprite list for mapped view 1, retaining the decoded background.
///
/// The zero-count record is followed by the terminal record; its address
/// also occupies the unused source slot. Borrowed while the room is loaded.
static SpriteBatch _gDryfieldNightTrailerCoachView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The 101 depth-sorted texture rectangles used in mapped view 2.
///
/// Positions and sizes are pixels relative to the view centre; depth is
/// the unscaled sorting value. Writable sources live with this room overlay.
static SpriteSource _gDryfieldNightTrailerCoachView2SpriteSources[101] = {
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 48, 40 } }, -72, 24, 750, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 72, 40 } }, -72, 64, 750, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 64, 16 } }, -64, 104, 750, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 16 } }, -24, 48, 750, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 48, 585, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 48, 591, { .fields = { 48, 8 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 48, 547, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 48, 591, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 48, 591, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 48, 591, { .fields = { 48, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 48, 591, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -88, 48, 582, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -80, 48, 591, { .fields = { 48, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, 48, 551, { .fields = { 48, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 56, 468, { .fields = { 48, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 56, 492, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 56, 549, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 56, 522, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 56, 549, { .fields = { 56, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 56, 549, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 56, 549, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 56, 549, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -88, 56, 549, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -80, 56, 562, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, 56, 554, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 64, 443, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 64, 461, { .fields = { 48, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 64, 484, { .fields = { 48, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 64, 512, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 64, 512, { .fields = { 48, 184 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 64, 512, { .fields = { 48, 192 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 64, 512, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 64, 512, { .fields = { 40, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 64, 512, { .fields = { 40, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -88, 64, 512, { .fields = { 40, 8 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -80, 64, 533, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 72, 480, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 72, 453, { .fields = { 48, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 72, 476, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 72, 480, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 72, 480, { .fields = { 48, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 72, 479, { .fields = { 48, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 72, 479, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 72, 479, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 72, 479, { .fields = { 48, 176 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -88, 72, 490, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 80, 401, { .fields = { 48, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 80, 401, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 80, 409, { .fields = { 80, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 80, 451, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 80, 451, { .fields = { 80, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 80, 451, { .fields = { 80, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 80, 452, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 80, 452, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 80, 452, { .fields = { 72, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -88, 80, 467, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 88, 373, { .fields = { 72, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 88, 373, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 88, 379, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 88, 386, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 88, 407, { .fields = { 96, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 88, 407, { .fields = { 104, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 88, 426, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 88, 426, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 88, 434, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 96, 353, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 96, 359, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 96, 366, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 96, 382, { .fields = { 88, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 96, 385, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 96, 403, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 96, 404, { .fields = { 64, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 96, 404, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 96, 416, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 104, 347, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 104, 364, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 104, 365, { .fields = { 56, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 104, 366, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 104, 384, { .fields = { 56, 8 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 104, 384, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 104, 384, { .fields = { 64, 8 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 104, 390, { .fields = { 64, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 112, 347, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 112, 353, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 112, 351, { .fields = { 72, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 112, 365, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 112, 365, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 112, 365, { .fields = { 64, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 112, 365, { .fields = { 64, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 112, 375, { .fields = { 64, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 64, 40 } }, -120, -24, 700, { .fields = { 64, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_896, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 120, 16 } }, -120, 16, 700, { .fields = { 104, 152 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 24 } }, 0, 16, 725, { .fields = { 96, 152 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 48 } }, 16, 40, 675, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 40 } }, 8, 40, 725, { .fields = { 120, 152 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 24 } }, 0, 40, 750, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_896, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 136, 16 } }, -136, 32, 625, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 64, 16 } }, -64, 48, 625, { .fields = { 56, 176 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 64 } }, 64, -8, 1150, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 32 } }, 72, -8, 1150, { .fields = { 96, 72 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 32, 32 } }, 72, 24, 1100, { .fields = { 88, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 2's ranges [0,4), [4,98) and [98,101), then the end marker.
///
/// The leading zero-count record keeps the decoded background. All three
/// ranges are visible and included in cached packets while this room is loaded.
static SpriteBatch _gDryfieldNightTrailerCoachView2SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 94, 0, 0, { 2, 0 } },
    { 98, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The 160 depth-sorted texture rectangles used in mapped view 3.
///
/// Positions and sizes are pixels relative to the view centre; depth is
/// the unscaled sorting value. Writable sources live with this room overlay.
static SpriteSource _gDryfieldNightTrailerCoachView3SpriteSources[160] = {
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 32, 48 } }, 40, -24, 1562, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 0, 465, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 0, 481, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 0, 489, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 0, 490, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 0, 493, { .fields = { 64, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 0, 499, { .fields = { 64, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 0, 502, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 64, 8, 988, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, 8, 990, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 8, 451, { .fields = { 56, 72 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 8, 440, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 8, 446, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 8, 453, { .fields = { 56, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 8, 459, { .fields = { 56, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 8, 466, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 8, 473, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 8, 475, { .fields = { 64, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 64, 16, 924, { .fields = { 64, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 72, 16, 921, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 16, 453, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 16, 442, { .fields = { 64, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 16, 435, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 16, 427, { .fields = { 72, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 16, 431, { .fields = { 64, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 16, 437, { .fields = { 64, 8 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 16, 443, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 16, 445, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 16, 462, { .fields = { 64, 184 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 16, 449, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 24, 456, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 24, 445, { .fields = { 64, 72 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 24, 438, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 24, 430, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 24, 423, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 24, 413, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 24, 401, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 24, 400, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 24, 404, { .fields = { 48, 192 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 24, 403, { .fields = { 48, 184 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 32, 458, { .fields = { 48, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 32, 448, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 32, 440, { .fields = { 48, 72 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 32, 433, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 32, 425, { .fields = { 48, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 32, 419, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 32, 402, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 32, 390, { .fields = { 40, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 32, 380, { .fields = { 40, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 32, 379, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 40, 461, { .fields = { 48, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 40, 451, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 40, 443, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 40, 436, { .fields = { 48, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 40, 428, { .fields = { 48, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 40, 416, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 40, 404, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 40, 394, { .fields = { 56, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 40, 382, { .fields = { 56, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 40, 379, { .fields = { 56, 184 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 48, 464, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 48, 454, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 48, 446, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 48, 438, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 48, 431, { .fields = { 48, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 48, 419, { .fields = { 48, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 48, 405, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 48, 394, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 48, 383, { .fields = { 48, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 48, 381, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 56, 467, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 56, 457, { .fields = { 48, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 56, 449, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 56, 441, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 56, 433, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 56, 420, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 56, 407, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 56, 396, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 56, 384, { .fields = { 96, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 56, 382, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 64, 495, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 64, 499, { .fields = { 96, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 64, 484, { .fields = { 96, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 64, 479, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 64, 465, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 64, 423, { .fields = { 88, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 64, 413, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 64, 397, { .fields = { 88, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 64, 386, { .fields = { 88, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 64, 384, { .fields = { 88, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 72, 484, { .fields = { 88, 8 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 72, 468, { .fields = { 88, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 72, 467, { .fields = { 88, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 72, 455, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 72, 452, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 72, 451, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 72, 451, { .fields = { 112, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 72, 399, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 72, 392, { .fields = { 120, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 72, 468, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 80, 482, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 80, 467, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 80, 450, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 80, 441, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 80, 441, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 80, 436, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 80, 434, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 80, 438, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 80, 445, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 80, 473, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 88, 466, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 88, 466, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 88, 454, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 88, 439, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 88, 423, { .fields = { 72, 72 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 88, 417, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 88, 417, { .fields = { 72, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 88, 438, { .fields = { 72, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 88, 461, { .fields = { 72, 8 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 88, 450, { .fields = { 72, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 96, 442, { .fields = { 72, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 96, 442, { .fields = { 72, 80 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 96, 442, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 96, 440, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 96, 425, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 96, 412, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 96, 425, { .fields = { 72, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 96, 442, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 96, 442, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 96, 442, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 104, 420, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 104, 420, { .fields = { 80, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 104, 420, { .fields = { 80, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 104, 420, { .fields = { 80, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 104, 420, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 104, 413, { .fields = { 80, 8 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 104, 420, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 104, 420, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 104, 420, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 104, 420, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 112, 433, { .fields = { 80, 184 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 88, 112, 430, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, 112, 426, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 104, 112, 423, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 112, 112, 409, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 112, 406, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 112, 403, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 112, 400, { .fields = { 80, 192 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 144, 112, 400, { .fields = { 80, 216 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 112, 400, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 8 } }, -24, 48, 450, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 40, 16 } }, -40, 56, 450, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 56, 24 } }, -56, 72, 400, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 48, 24 } }, -48, 96, 375, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 80, 16 } }, 0, 104, 375, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 80, 24 } }, 0, 80, 400, { .fields = { 48, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 80, 16 } }, 0, 64, 450, { .fields = { 48, 152 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_896, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 80, 8 } }, 0, 56, 500, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 8 } }, 8, 48, 650, { .fields = { 40, 8 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 48, 32 } }, 32, 24, 600, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 3's ranges [0,1) and [1,160), then the end marker.
///
/// The leading zero-count record keeps the decoded background. Both ranges
/// are visible and included in cached packets while this room is loaded.
static SpriteBatch _gDryfieldNightTrailerCoachView3SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 159, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The 39 depth-sorted texture rectangles used in mapped view 4.
///
/// Positions and sizes are pixels relative to the view centre; depth is
/// the unscaled sorting value. Writable sources live with this room overlay.
static SpriteSource _gDryfieldNightTrailerCoachView4SpriteSources[39] = {
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 64, 450, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 88, 362, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 88, 361, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, 88, 375, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, 88, 394, { .fields = { 56, 152 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 96, 673, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 96, 343, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 96, 360, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, 96, 382, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 56, 96, 402, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 104, 332, { .fields = { 64, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 104, 334, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 104, 366, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, 104, 389, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 16, 112, 610, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 112, 317, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 112, 340, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 112, 373, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, 112, 397, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 40, 40 } }, -160, -96, 450, { .fields = { 72, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 32 } }, -160, -8, 450, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, 24, 450, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 56, 8 } }, -160, 48, 450, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 72 } }, -104, 48, 587, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 48, 8 } }, -160, 56, 450, { .fields = { 16, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 64 } }, -112, 56, 575, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 48 } }, -120, 72, 575, { .fields = { 104, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 40, 16 } }, -160, 64, 450, { .fields = { 40, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 40 } }, -136, 80, 550, { .fields = { 96, 152 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, 80, 487, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, 96, 475, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 72 } }, 128, 8, 450, { .fields = { 112, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 96 } }, 144, 8, 400, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 80, 24 } }, 64, 80, 400, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 48, 24 } }, 80, 56, 450, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 40, 32 } }, 40, 48, 550, { .fields = { 64, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 8 } }, 40, 80, 550, { .fields = { 40, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_896, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 104, 16 } }, 56, 104, 375, { .fields = { 120, 176 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 56, 40 } }, 16, 80, 400, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 4's ranges [0,38) and [38,39), then the end marker.
///
/// The leading zero-count record keeps the decoded background. Both ranges
/// are visible and included in cached packets while this room is loaded.
static SpriteBatch _gDryfieldNightTrailerCoachView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 38, 0, 0, { 1, 0 } },
    { 38, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Five vertical texture rectangles along mapped view 5's right edge.
///
/// Each is 8 by 112 pixels at y=8, with individual sorting depths. Positions
/// are relative to the view centre; writable sources live with this room.
static SpriteSource _gDryfieldNightTrailerCoachView5SpriteSources[5] = {
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 112 } }, 120, 8, 325, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 112 } }, 128, 8, 312, { .fields = { 120, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 112 } }, 136, 8, 300, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 112 } }, 144, 8, 287, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 112 } }, 152, 8, 275, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 5's complete five-source range, followed by the end marker.
///
/// The leading zero-count record keeps the decoded background. The range
/// is visible and included in cached packets while this room is loaded.
static SpriteBatch _gDryfieldNightTrailerCoachView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightTrailerCoachView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The 22 depth-sorted texture rectangles used in mapped view 6.
///
/// Positions and sizes are pixels relative to the view centre; depth is
/// the unscaled sorting value. Writable sources live with this room overlay.
static SpriteSource _gDryfieldNightTrailerCoachView6SpriteSources[22] = {
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 112, 363, { .fields = { 120, 16 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -152, 112, 363, { .fields = { 120, 24 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -144, 112, 363, { .fields = { 120, 48 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 112, 353, { .fields = { 120, 80 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -128, 112, 373, { .fields = { 120, 88 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -120, 112, 373, { .fields = { 120, 72 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -112, 112, 373, { .fields = { 120, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, 112, 375, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 32, 8 } }, -64, -48, 875, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 48, 8 } }, -112, -40, 875, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 40, 8 } }, -64, -40, 875, { .fields = { 88, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, -32, 875, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 56, 8 } }, -112, -32, 875, { .fields = { 72, 104 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 32, 8 } }, -48, -24, 875, { .fields = { 96, 136 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 64, 8 } }, -112, -24, 875, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 8 } }, -40, -16, 875, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 72, 8 } }, -112, -16, 875, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 64, 8 } }, -96, -8, 875, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 8 } }, -112, -8, 875, { .fields = { 112, 152 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 8 } }, -32, -8, 875, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 16 } }, -32, 0, 875, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 24, 8 } }, -112, 0, 875, { .fields = { 104, 40 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 6's complete 22-source range, followed by the end marker.
///
/// The leading zero-count record keeps the decoded background. The range
/// is visible and included in cached packets while this room is loaded.
static SpriteBatch _gDryfieldNightTrailerCoachView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightTrailerCoachView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Seven adjacent vertical texture rectangles used in mapped view 7.
///
/// Positions are pixels relative to the view centre; all use sorting depth
/// 800. Writable sources live with this room overlay.
static SpriteSource _gDryfieldNightTrailerCoachView7SpriteSources[7] = {
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 72 } }, 16, 0, 800, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 56 } }, 0, 0, 800, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 56 } }, -16, 0, 800, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 56 } }, -32, 0, 800, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 56 } }, -48, 0, 800, { .fields = { 112, 72 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 56 } }, -64, 0, 800, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_PAGE_960, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_CLUT, { .fields = { 16, 56 } }, -80, 0, 800, { .fields = { 112, 184 } }, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_TRAILER_COACH_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 7's complete seven-source range, followed by the end marker.
///
/// The leading zero-count record keeps the decoded background. The range
/// is visible and included in cached packets while this room is loaded.
static SpriteBatch _gDryfieldNightTrailerCoachView7SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightTrailerCoachView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 8, retaining the decoded background.
///
/// The zero-count record is followed by the terminal record; its address
/// also occupies the unused source slot. Borrowed while the room is loaded.
static SpriteBatch _gDryfieldNightTrailerCoachView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightTrailerCoachSpriteViews[8] = {
    { { .empty = _gDryfieldNightTrailerCoachView1SpriteBatches }, _gDryfieldNightTrailerCoachView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightTrailerCoachView2SpriteSources }, _gDryfieldNightTrailerCoachView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightTrailerCoachView3SpriteSources }, _gDryfieldNightTrailerCoachView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightTrailerCoachView4SpriteSources }, _gDryfieldNightTrailerCoachView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightTrailerCoachView5SpriteSources }, _gDryfieldNightTrailerCoachView5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightTrailerCoachView6SpriteSources }, _gDryfieldNightTrailerCoachView6SpriteBatches, NULL },
    { { .elements = _gDryfieldNightTrailerCoachView7SpriteSources }, _gDryfieldNightTrailerCoachView7SpriteBatches, NULL },
    { { .empty = _gDryfieldNightTrailerCoachView8SpriteBatches }, _gDryfieldNightTrailerCoachView8SpriteBatches, NULL },
};

/// Thirteen authored point lights contributing in every coach view.
///
/// Positions and inner/outer falloff radii use room-coordinate game units;
/// RGB intensities use Q12. Room lighting parents and composes their writable
/// coordinates and replaces attenuation while this room overlay is loaded.
static WorldCoordPointLight _gDryfieldNightTrailerCoachPointLights[13] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9699, -1003, -2182 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3277, 3277, 2458 }, { 0, 0 } }, 1063, 1624 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2600, -2000, -710 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3075, 3075, 3075 }, { 0, 0 } }, 1658, 2444 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2600, -2000, -2540 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2868, 2868, 2868 }, { 0, 0 } }, 1765, 2482 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9699, -1003, -941 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 1043, 1642 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 490, -1975, -700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3278 }, { 0, 0 } }, 1304, 1599 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 490, -1975, -1625 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3276 }, { 0, 0 } }, 1285, 1620 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 490, -1975, -2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3276 }, { 0, 0 } }, 1287, 1555 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -2000, -2540 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2868, 2868, 2868 }, { 0, 0 } }, 1664, 2386 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -2000, -710 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2868, 2868, 2868 }, { 0, 0 } }, 1698, 2456 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7400, -2000, -710 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3280, 3280, 3280 }, { 0, 0 } }, 1403, 2026 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7400, -2000, -2540 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3278, 3280, 3280 }, { 0, 0 } }, 1380, 1902 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7000, -1600, -400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2868, 2868, 2868 }, { 0, 0 } }, 1240, 1802 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7200, -1000, -2700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2458, 2050, 1642 }, { 0, 0 } }, 838, 1280 },
};

/// The coach's thirteen point lights, with no directional or cone lights.
///
/// Borrows writable transforms and attenuation storage for the loaded room.
static WorldCoordRoomLights _gDryfieldNightTrailerCoachRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gDryfieldNightTrailerCoachPointLights),
    _gDryfieldNightTrailerCoachPointLights,
    0,
    NULL,
};

/// View boundaries linking 2 to 3 and 3 to 4 in both directions.
///
/// Four writable room-space quads, including the final LAST record. Setup
/// binds their coordinate and list links; storage stays live until unlinked.
static WorldCollisionTrigger _gDryfieldNightTrailerCoachViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { 3070, -1168, -738, 0 }, { { 14, -1904, -1569, 0 }, { -13, -1904, 1570, 0 }, { 14, 1904, -1569, 0 }, { -13, 1904, 1570, 0 } }, { 4110, 0, 35, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3246, -1120, -801, 0 }, { { 16, -1888, 1619, 0 }, { -16, -1888, -1619, 0 }, { 16, 1888, 1619, 0 }, { -16, 1888, -1619, 0 } }, { -4096, 0, 39, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7918, -1376, -1538, 0 }, { { -2, -2112, 1038, 0 }, { -1, -2112, -1043, 0 }, { -2, 2112, 1038, 0 }, { -1, 2112, -1043, 0 } }, { -4106, 0, -4, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7806, -1376, -1538, 0 }, { { -7, -2112, -1048, 0 }, { -8, -2112, 1031, 0 }, { -7, 2112, -1048, 0 }, { -8, 2112, 1031, 0 } }, { 4097, 0, 1, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Minimum ambient RGB levels for logical views 1..8, preceded by their count.
///
/// Levels use 16 units per 8-bit channel; the fourth halfword retains weighted
/// brightness. Borrowed read-only by model lighting while the room is loaded.
static WorldCoordRoomAmbientEntry _gDryfieldNightTrailerCoachRoomAmbient[9] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightTrailerCoachRoomAmbient) - 1 },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 820, 820, 820, 820 } },
    { .color = { 820, 820, 820, 820 } },
    { .color = { 1230, 1230, 1230, 1230 } },
    { .color = { 820, 820, 820, 820 } },
    { .color = { 820, 820, 820, 820 } },
    { .color = { 820, 820, 820, 820 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Command slots and packed warp selectors used by the action-trigger list.
enum {
    DRYFIELD_NIGHT_TRAILER_COACH_EXIT_WARP_SLOTS       = (1 << 4) | 2,
    DRYFIELD_NIGHT_TRAILER_COACH_ROOM_TALK_COMMAND     = 3,
    DRYFIELD_NIGHT_TRAILER_COACH_ROOM_CUTSCENE_COMMAND = 14,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_2         = 2,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_15        = 15,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_18        = 18,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_19        = 19,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_20        = 20,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_21        = 21,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_22        = 22,
    DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_23        = 23,
};

/// Fourteen interaction quads: the junk-yard exit, talk/cutscene commands and CAPs.
///
/// Includes the final LAST record. Setup borrows and mutates their coordinate,
/// list links and hit latches; the room must remain loaded until unlinked.
static WorldCollisionTrigger _gDryfieldNightTrailerCoachActionTriggers[14] = {
    { NULL, NULL, NULL, { 1536, -48, -784, 0 }, { { -416, 0, -272, 0 }, { 416, 0, -272, 0 }, { -416, 0, 272, 0 }, { 416, 0, 272, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 496, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD, DRYFIELD_NIGHT_TRAILER_COACH_EXIT_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3631, -64, -2321, 0 }, { { -656, 0, 1390, 0 }, { -482, 0, -3258, 0 }, { 323, 0, 1403, 0 }, { 1393, 0, 339, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 3288, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_ROOM_TALK_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 160, -64, -656, 0 }, { { -416, 0, -448, 0 }, { 416, 0, -448, 0 }, { -416, 0, 448, 0 }, { 416, 0, 448, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 610, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_ROOM_CUTSCENE_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 128, -64, -1664, 0 }, { { -416, 0, -544, 0 }, { 416, 0, -544, 0 }, { -416, 0, 544, 0 }, { 416, 0, 544, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 683, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_15, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1856, -64, -3040, 0 }, { { -768, 0, -272, 0 }, { 768, 0, -272, 0 }, { -768, 0, 272, 0 }, { 768, 0, 272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_18, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9248, -64, -480, 0 }, { { -416, 0, -544, 0 }, { 416, 0, -544, 0 }, { -416, 0, 544, 0 }, { 416, 0, 544, 0 } }, { 0, 4104, 0, 0 }, { -4091, 0, 201, 0 }, 683, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_19, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9184, -64, -2224, 0 }, { { -416, 0, -1136, 0 }, { 416, 0, -1136, 0 }, { -416, 0, 1136, 0 }, { 416, 0, 1136, 0 } }, { 0, 4100, 0, 0 }, { -4091, 0, 201, 0 }, 1207, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_20, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6640, -64, -1136, 0 }, { { -1456, 0, -320, 0 }, { 1456, 0, -320, 0 }, { -1456, 0, 320, 0 }, { 1456, 0, 320, 0 } }, { 0, 4096, 0, 0 }, { -201, 0, -4091, 0 }, 1487, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_21, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7040, -64, -2080, 0 }, { { -1840, 0, -320, 0 }, { 1840, 0, -320, 0 }, { -1840, 0, 320, 0 }, { 1840, 0, 320, 0 } }, { 0, 4113, 0, 0 }, { 201, 0, 4091, 0 }, 1863, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_22, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4352, -64, 0, 0 }, { { -1200, 0, -1344, 0 }, { 1200, 0, -1344, 0 }, { -1200, 0, -320, 0 }, { 1200, 0, -320, 0 } }, { 0, 4116, 0, 0 }, { 0, 0, -4096, 0 }, 1801, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_23, CAP_EVENT_PAUSE_ACTORS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2880, -64, -1168, 0 }, { { -912, 0, -336, 0 }, { 912, 0, -336, 0 }, { -912, 0, 336, 0 }, { 912, 0, 336, 0 } }, { 0, 4103, 0, 0 }, { 201, 0, 4091, 0 }, 970, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_2, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2320, -64, -2320, 0 }, { { -352, 0, -960, 0 }, { 352, 0, -960, 0 }, { -352, 0, 960, 0 }, { 352, 0, 960, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 1021, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_2, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4224, -64, -832, 0 }, { { -656, 0, -512, 0 }, { 656, 0, -512, 0 }, { -656, 0, 512, 0 }, { 656, 0, 512, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 832, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_23, CAP_EVENT_PAUSE_ACTORS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8560, -64, -2576, 0 }, { { -384, 0, -784, 0 }, { 384, 0, -784, 0 }, { -384, 0, 784, 0 }, { 384, 0, 784, 0 } }, { 0, 4105, 0, 0 }, { 4090, 0, 200, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_TRAILER_COACH_CAP_COMMAND_22, CAP_EVENT_NO_FLAGS, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_RESOURCE_ENTRY  = 106,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_FILE_NUMBER     = 207,
    DRYFIELD_NIGHT_TRAILER_COACH_GARY_BODY_DESCRIPTOR = 0,
};

/// Gary Douglas's loading and spawn recipe for placement variant 1.
///
/// Entry 106 selects global-library file 420700 and descriptor 0. The single
/// resource and its terminator are borrowed during loading and spawning;
/// keep the room table and Gary's actor descriptors loaded through their use.
static AreaResource _gDryfieldNightTrailerCoachGaryResources[2] = {
    {
        .entryId        = DRYFIELD_NIGHT_TRAILER_COACH_GARY_RESOURCE_ENTRY,
        .fileNumber     = DRYFIELD_NIGHT_TRAILER_COACH_GARY_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = DRYFIELD_NIGHT_TRAILER_COACH_GARY_BODY_DESCRIPTOR,
        .unknown_6      = { 0, 0 },
        .taskTable      = gActor420700GaryDouglasTaskDescriptors,
    },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightTrailerCoachAreaVariants[DRYFIELD_NIGHT_TRAILER_COACH_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldFullTrailerCoachVariant1Placements, .resources = _gDryfieldNightTrailerCoachGaryResources },
};

/// Type-1 cue-2 requests; cue 1 uses the following entry in the loaded bank.
enum {
    DRYFIELD_NIGHT_TRAILER_COACH_FLOOR_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 45,
    DRYFIELD_NIGHT_TRAILER_COACH_FLOOR_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 47,
};

/// Paired footstep script bases for the coach's class-6 floor.
///
/// Walking and scripted jumps share entries 45/46; running uses 47/48 of
/// the loaded type-1 bank. Cue 2 uses the base and cue 1 the next entry.
static WorldCollisionFootstepSounds _gDryfieldNightTrailerCoachFloorFootstepSounds = {
    .walk         = DRYFIELD_NIGHT_TRAILER_COACH_FLOOR_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_TRAILER_COACH_FLOOR_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_TRAILER_COACH_FLOOR_WALK_SOUND,
};

/// Silent blocking policy shared by surface classes 0..4 and 7.
static WorldCollisionSurfaceProperties _gDryfieldNightTrailerCoachDefaultSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Separate silent blocking policy for the mesh's class-5 walls and raised surfaces.
static WorldCollisionSurfaceProperties _gDryfieldNightTrailerCoachClass5SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Blocking policy and footstep sounds for the mesh's class-6 floor at y=0.
static WorldCollisionSurfaceProperties _gDryfieldNightTrailerCoachFloorSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldNightTrailerCoachFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightTrailerCoachSurfacePropertiesByClass[8] = {
    &_gDryfieldNightTrailerCoachDefaultSurfaceProperties,
    &_gDryfieldNightTrailerCoachDefaultSurfaceProperties,
    &_gDryfieldNightTrailerCoachDefaultSurfaceProperties,
    &_gDryfieldNightTrailerCoachDefaultSurfaceProperties,
    &_gDryfieldNightTrailerCoachDefaultSurfaceProperties,
    &_gDryfieldNightTrailerCoachClass5SurfaceProperties,
    &_gDryfieldNightTrailerCoachFloorSurfaceProperties,
    &_gDryfieldNightTrailerCoachDefaultSurfaceProperties,
};

enum {
    DRYFIELD_NIGHT_TRAILER_COACH_STORY_GARAGE_VARIANT = 4,
    DRYFIELD_NIGHT_TRAILER_COACH_STORY_SET_MAP_MARK   = 1,
};

AreaApplyRec gDryfieldNightTrailerCoachStoryAreaUpdates[2] = {
    {
        .stage   = GAME_STAGE_DRYFIELD_NIGHT,
        .area    = GAME_AREA_DRYFIELD_NIGHT_GARAGE,
        .variant = DRYFIELD_NIGHT_TRAILER_COACH_STORY_GARAGE_VARIANT,
        .policy  = AREA_APPLY_MODE_ALWAYS | DRYFIELD_NIGHT_TRAILER_COACH_STORY_SET_MAP_MARK,
    },
    { .stage = AREA_APPLY_END },
};

s32 gShopRefillDisplayQuantityFixed = 0;

const EquipmentWeaponSupply* gShopRefillSupply = NULL;

Task* gRoomCutsceneSoundTask = NULL;

RoomCutsceneRec gDryfieldNightTrailerCoachCutscene = { 0 };

static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

void dryfieldNightTrailerCoachDrawGlowsTask(Task* unusedTask)
{
    enum {
        DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS       = 0x100,
        DRYFIELD_NIGHT_TRAILER_COACH_SMALL_GLOW_RADIUS = 0xC0,
        DRYFIELD_NIGHT_TRAILER_COACH_MARKER_PULSE_RATE = 0x60,
        DRYFIELD_NIGHT_TRAILER_COACH_DIAMOND_RADIUS    = 0xA0,
        DRYFIELD_NIGHT_TRAILER_COACH_DISC_RADIUS       = 0x30,
    };

    const SVECTOR(*firstStripPoints)[2];
    const SVECTOR(*secondStripPoints)[2];
    const SVECTOR* markerPoint;
    u8             view;

    view = gGameSession->location.view;
    switch (view) {
        case 3:
            firstStripPoints = _gDryfieldNightTrailerCoachView3GlowStripPoints;
            _dryfieldNightTrailerCoachDrawGlowCapsule(firstStripPoints[0], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(firstStripPoints[1], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(firstStripPoints[2], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(firstStripPoints[3], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(firstStripPoints[4], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(firstStripPoints[5], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(firstStripPoints[6], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(firstStripPoints[7], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            // View 3 also draws the strips visible in view 4.
            /* fallthrough */
        case 4:
            secondStripPoints = _gDryfieldNightTrailerCoachView4GlowStripPoints;
            _dryfieldNightTrailerCoachDrawGlowCapsule(secondStripPoints[0], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(secondStripPoints[1], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(secondStripPoints[2], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(secondStripPoints[3], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(secondStripPoints[4], DRYFIELD_NIGHT_TRAILER_COACH_SMALL_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(secondStripPoints[5], DRYFIELD_NIGHT_TRAILER_COACH_SMALL_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(secondStripPoints[6], DRYFIELD_NIGHT_TRAILER_COACH_SMALL_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(secondStripPoints[7], DRYFIELD_NIGHT_TRAILER_COACH_SMALL_GLOW_RADIUS);
            break;
        case 2:
        case 5:
        case 7:
            markerPoint = &_gDryfieldNightTrailerCoachGlowMarkerPoint;
            _glowDrawDiamond(markerPoint, DRYFIELD_NIGHT_TRAILER_COACH_MARKER_PULSE_RATE, DRYFIELD_NIGHT_TRAILER_COACH_DIAMOND_RADIUS);
            // The binary reuses the marker base for the following strip pairs.
            _dryfieldNightTrailerCoachDrawGlowCapsule(&markerPoint[1], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(&markerPoint[3], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(&markerPoint[5], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            _dryfieldNightTrailerCoachDrawGlowCapsule(&markerPoint[7], DRYFIELD_NIGHT_TRAILER_COACH_GLOW_RADIUS);
            break;
        case 8:
            _glowDrawPulsingDisc(&_gDryfieldNightTrailerCoachGlowMarkerPoint, DRYFIELD_NIGHT_TRAILER_COACH_MARKER_PULSE_RATE, DRYFIELD_NIGHT_TRAILER_COACH_DISC_RADIUS);
            break;
    }
}

#include "../../shared/effects/glow_draw_diamond.inc.c"

#include "../../shared/effects/glow_draw_pulsing_disc.inc.c"

/// Initializes a grey-to-black Gouraud wedge for a capsule end cap.
///
/// Borrows one writable `POLY_G4`. Vertex 2 receives `centreIntensity`
/// (0..255) in all three RGB channels; rim vertices 0, 1 and 3 are black.
/// Sets the packet length and opaque, untextured Gouraud-quad command.
/// The caller supplies screen coordinates, the ordering-table link and
/// semitransparency with its blend draw mode before GPU submission.
static inline void _dryfieldNightTrailerCoachInitGreyCapsuleWedge(POLY_G4* wedge, u8 centreIntensity)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, centreIntensity, centreIntensity, centreIntensity);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws a flickering grey capsule aligned with two projected world points.
///
/// `worldPoints` supplies two consecutive points, borrowed for this call.
/// The signed low halfword of `radiusScale` gives each end a pixel radius of
/// `radiusScale * 64 / depth`, where depth is camera Z / 4 and must be nonzero.
/// Negative GTE flags at either end reject the capsule. Grey centre intensity
/// alternates between 16 and 24; the rim is black. Queues six Gouraud quads
/// and additive blend commands in the current frame's packet arena. The two
/// joining quads sort at the mean depth; end caps sort at their own depths.
/// Requires composed view matrices, an initialized scratch stack with one
/// `OverlayPointPairScratch` available, and room in the current packet arena.
static void _dryfieldNightTrailerCoachDrawGlowCapsule(const SVECTOR worldPoints[2], s32 radiusScale)
{
    enum { DRYFIELD_NIGHT_TRAILER_COACH_CAPSULE_BASE_INTENSITY = 16 };

    const SVECTOR*           secondPoint;
    OverlayPointPairScratch* block;
    POLY_G4*                 quad;
    const DisplayState*      display;
    s32                      bearing;
    s32                      sweepAngle;
    s32                      halfTurnEnd;
    s32                      sweepLimit;
    s32                      baseAngle;
    s32                      rimAngle;
    s32                      nextSweepAngle;
    s32                      farCapAngle;
    s32                      sideAngle;
    s32                      scaledRadius;
    s32                      intensity;

    secondPoint = worldPoints + 1;
    block       = SCRATCH_STACK_RESERVE_BLOCK(OverlayPointPairScratch);

    // Project both endpoints before deriving radii and the screen-space bearing.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoints);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(secondPoint);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaledRadius   = (s16)radiusScale * GLOW_RADIUS_SCALE;
            block->radius0 = scaledRadius / block->otz0;
            block->radius1 = scaledRadius / block->otz1;
            bearing        = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            display        = &gDisplayState;
            sweepAngle     = (s16)bearing;
            intensity      = (((u8)display->animFrame & 1) * GLOW_FLICKER_INTENSITY_STEP) | DRYFIELD_NIGHT_TRAILER_COACH_CAPSULE_BASE_INTENSITY;
            halfTurnEnd    = sweepAngle + GLOW_HALF_TURN;
            if (sweepAngle < halfTurnEnd) {
                baseAngle  = sweepAngle;
                sweepLimit = halfTurnEnd;
                // Two quarter-turn sweeps fill the end caps and both sides of the band.
                do {
                    quad           = gGpuPrimCursor;
                    gGpuPrimCursor = quad + 1;
                    _dryfieldNightTrailerCoachInitGreyCapsuleWedge(quad, intensity);
                    quad->x0       = block->sx0 + ((block->radius0 * rsin(sweepAngle)) >> GLOW_TRIG_SHIFT);
                    rimAngle       = sweepAngle + GLOW_EIGHTH_TURN;
                    quad->y0       = block->sy0 + ((block->radius0 * rcos(sweepAngle)) >> GLOW_TRIG_SHIFT);
                    quad->x1       = block->sx0 + ((block->radius0 * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
                    quad->y1       = block->sy0 + ((block->radius0 * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
                    nextSweepAngle = sweepAngle + GLOW_QUARTER_TURN;
                    quad->x2       = block->sx0;
                    quad->y2       = block->sy0;
                    quad->x3       = block->sx0 + ((block->radius0 * rsin(nextSweepAngle)) >> GLOW_TRIG_SHIFT);
                    quad->y3       = block->sy0 + ((block->radius0 * rcos(nextSweepAngle)) >> GLOW_TRIG_SHIFT);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz0),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, block->otz0);

                    sideAngle      = baseAngle + ((sweepAngle - baseAngle) * 2);
                    quad           = gGpuPrimCursor;
                    gGpuPrimCursor = quad + 1;
                    setPolyG4(quad);
                    setRGB0(quad, 0, 0, 0);
                    setRGB1(quad, 0, 0, 0);
                    setRGB2(quad, intensity, intensity, intensity);
                    setRGB3(quad, intensity, intensity, intensity);
                    quad->x0 = block->sx0 + ((block->radius0 * rsin(sideAngle)) >> GLOW_TRIG_SHIFT);
                    quad->y0 = block->sy0 + ((block->radius0 * rcos(sideAngle)) >> GLOW_TRIG_SHIFT);
                    quad->x1 = block->sx1 + ((block->radius1 * rsin(sideAngle)) >> GLOW_TRIG_SHIFT);
                    quad->y1 = block->sy1 + ((block->radius1 * rcos(sideAngle)) >> GLOW_TRIG_SHIFT);
                    quad->x2 = block->sx0;
                    quad->y2 = block->sy0;
                    quad->x3 = block->sx1;
                    quad->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH((block->otz1 + block->otz0) / 2),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, (block->otz1 + block->otz0) / 2);

                    quad           = gGpuPrimCursor;
                    farCapAngle    = sweepAngle + GLOW_HALF_TURN;
                    rimAngle       = farCapAngle;
                    gGpuPrimCursor = quad + 1;
                    _dryfieldNightTrailerCoachInitGreyCapsuleWedge(quad, intensity);
                    quad->x0 = block->sx1 + ((block->radius1 * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
                    quad->y0 = block->sy1 + ((block->radius1 * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
                    rimAngle = sweepAngle + (GLOW_HALF_TURN + GLOW_EIGHTH_TURN);
                    quad->x1 = block->sx1 + ((block->radius1 * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
                    quad->y1 = block->sy1 + ((block->radius1 * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
                    rimAngle = sweepAngle + (GLOW_HALF_TURN + GLOW_QUARTER_TURN);
                    quad->x2 = block->sx1;
                    quad->y2 = block->sy1;
                    quad->x3 = block->sx1 + ((block->radius1 * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
                    quad->y3 = block->sy1 + ((block->radius1 * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz1),
                            quad);
                    gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, block->otz1);
                    sweepAngle = nextSweepAngle;
                } while (sweepAngle < sweepLimit);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayPointPairScratch);
}
