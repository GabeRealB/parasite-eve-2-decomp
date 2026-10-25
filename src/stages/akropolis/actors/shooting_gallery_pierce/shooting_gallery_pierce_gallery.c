#include "stages/akropolis/actors/shooting_gallery_pierce.h"
#include "shooting_gallery_pierce_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/strings.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/companion_load.h"
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
#include "gameplay/player_actor.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
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

#include "stages/akropolis/rooms/mist_shooting_gallery.h"

#include "../../event_flags.h"
#include "../../rooms/mist_shooting_gallery/config.h"

#include "assets/engine/animation/player_schema.h"
#include "assets/rooms/mist/shooting_gallery/pierce_schema.h"
#include "assets/rooms/mist/shooting_gallery/player_conversation_schema.h"
#include "assets/rooms/mist/shooting_gallery/player_gallery_schema.h"
#include "../../../../shared/walk/paced_walk.h"
#include "../../../../shared/room/cap_captions_types.h"

#include "assets/rooms/mist/shooting_gallery/pierce/declarations.inc"

// Course selections are 1-based; the room controller receives selection minus one.
enum {
    GALLERY_FIRST_FREE_MOVEMENT_COURSE = 3,
    GALLERY_FIRST_SWAP_COURSE          = 4,
    GALLERY_BONUS_COURSE               = 5,
    GALLERY_EXIT_DECISION_TASK         = 0,
    GALLERY_ABORT_DECISION_TASK        = 1,
    GALLERY_END_TRAINING               = 0,
    GALLERY_CONFIRM_ROOM_EXIT          = 1,
    GALLERY_COMMIT_ROOM_EXIT           = 2,
    GALLERY_EXIT_CONFIRM_COMMAND       = 0x14,
    GALLERY_EXIT_TRANSITION_COMMAND    = 0x17,
    GALLERY_ABORT_CONFIRM_COMMAND      = 0x1D,
    GALLERY_EXIT_KEEP_ACTORS_HELD      = 1,
    GALLERY_MUSIC_FADE_AUDIO_TICKS     = 30,
    GALLERY_RETURN_VIEW                = 8,
};

static CapCaptionCaretDelayStorage _gCapCaptionCaretDelayStorage;

static void _shootingGalleryPierceSpawnPierce(Enemy* enemy, Task* task);
static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _pacedWalkExit(Task* task);

static void _shootingGalleryPierceShowTimedCaption(s16 commandIndex, s16 key, s16 durationTicks);
/// Gives this actor's caption record selector external linkage for the gallery.
#define CAP_CAPTION_SELECT_RECORD_LINKAGE
/// Binds shared record selection to this actor's exported selector.
#define CAP_CAPTION_SELECT_RECORD shootingGalleryPierceCapCaptionSelectRecord
/// Exports this actor's current-caption drawer to the shooting-gallery room.
#define CAP_CAPTION_DRAW_CURRENT_LINKAGE extern
/// Binds shared caption drawing to this actor's exported void(void) instance.
#define CAP_CAPTION_DRAW_CURRENT shootingGalleryPierceCapCaptionDrawCurrent
#include "../../../../shared/room/cap_captions.h"

static void _capCaptionRunSchedule(Task* task);
#include "../../../../shared/walk/walker.h"

static TaskDesc             _gCapCaptionTimedTaskDesc;
static TaskDesc             _gCapCaptionCancelableTaskDesc;
static const TextGlyphCell* _gCapCaptionGlyphCells;

/// Caption script table.
static const CapCommandRef*     _gCapCaptionCommandRefs;
static CapSequenceRecord*       _gCapCaptionSequence;
static s16                      _gCapCaptionBlockLeftX;
static s16                      _gCapCaptionFirstBaselineY;
static s16                      _gCapCaptionBottomBaselineY;
static s16                      _gCapCaptionRecordIndex;
static s16                      _gCapCaptionBlockHeight;
static s16                      _gCapCaptionSelectedKey;
static s32                      _gCapCaptionCaretPulseLevel;
static s32                      _gCapCaptionCaretPulseFalling;
static u16                      _gCapCaptionCaretLeftX;
static u16                      _gCapCaptionCaretTipY;
static s16                      _gCapCaptionTexturePageX;
static s16                      _gCapCaptionTexturePageY;
RoomEventMsg                    _gShootingGalleryPierceGalleryPendingExitRequest;
static CapCaptionScheduleWindow _gCapCaptionSceneClockSchedule[];

static TmdSource _gPierceBody;
static TmdSource _gPierceGlasses;
static s32       _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 requestFlags, s32 unusedArgument);
static s32       _shootingGalleryPierceIgnoreCommand(Task* task, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);
static void      _shootingGalleryPiercePierceTask(Task* task);
static void      _pacedWalkSubModelTask(Task* task);

static AnimationSet _gMistShootingGalleryPiercePhoneListenClip;
static AnimationSet _gMistShootingGalleryPiercePhoneTalkClip;
static AnimationSet _gMistShootingGalleryPierceQuipClip;
static AnimationSet _gMistShootingGalleryPierceTeaseClip;
static AnimationSet _gMistShootingGalleryPierceWarnClip;
static AnimationSet _gMistShootingGalleryPierceStory3Clip;
static AnimationSet _gMistShootingGalleryPierceStory4Clip;
static AnimationSet _gMistShootingGalleryPierceTalkClip;
static AnimationSet _gMistShootingGalleryPierceTalkEndClip;
static AnimationSet _gMistShootingGalleryPierceCorrectHimselfClip;
static AnimationSet _gMistShootingGalleryPierceCorrectHimselfEndClip;
static AnimationSet _gSoldierBStrideWalkAnimAnim08Clip;
static AnimationSet _gSoldierBStrideWalkAnimAnim09Clip;
static AnimationSet _gSoldierBStrideWalkAnimAnim10Clip;
static AnimationSet _gPierceStory17Clip;
static AnimationSet _gMistShootingGalleryPierceGalleryIdleClip;
static AnimationSet _gMistShootingGalleryPierceGalleryAskClip;
static AnimationSet _gMistShootingGalleryPierceGalleryTalkClip;
static AnimationSet _gMistShootingGalleryPierceGalleryTeaseClip;
static AnimationSet _gMistShootingGalleryPierceGalleryReactClip;
static AnimationSet _gMistShootingGalleryPierceLevelPromptClip;
static AnimationSet _gMistShootingGalleryPierceLevelReplyClip;

static void _shootingGalleryPierceSetConversationCap(s32 enabled);

static void _shootingGalleryPierceGalleryIntroTask(Task* task);
static void _shootingGalleryPierceGalleryRedFlashLoopTask(Task* task);
static void _shootingGalleryPierceStartGalleryMovie(void);
static void _shootingGalleryPierceSpawnGalleryDemoTargetIfEnabled(s32 enabled);
static void _shootingGalleryPierceSetGalleryRedFlashLoop(s32 enabled);
static void _shootingGalleryPierceSelectGalleryRoomLights(s16 useAlternate);
static void _shootingGalleryPierceSetViewRespawnPending(s16 pending);

enum {
    CONVERSATION_BLEND_FRAMES      = 10,
    CONVERSATION_IDLE_BLEND_FRAMES = 8,
};

// CAP command slots and mapped views used by these event scripts.
enum {
    SCRIPT_PLAYER_WEAPONS               = 0,
    GALLERY_INTRO_CAP_COMMAND           = 5,
    GALLERY_TRAINING_OFFER_CAP_COMMAND  = 6,
    GALLERY_TRAINING_ACCEPT_CAP_COMMAND = 7,
    PIERCE_STORY_CAP_COMMAND            = 1,
    PIERCE_FIRST_TALK_CAP_COMMAND       = 2,
    PIERCE_SECOND_TALK_CAP_COMMAND      = 3,
    PIERCE_REPEAT_TALK_CAP_COMMAND      = 4,
    GALLERY_MENU_VIEW                   = 15,
    GALLERY_FIXED_TRAINING_VIEW         = 14,
    STORY_RETURN_VIEW                   = 2,
    GALLERY_AMBIENT_COMPONENT           = 80,
    SCRIPT_FADE_FRAMES                  = 30,
    STORY_FADE_RETURN_FRAMES            = 60,
    GALLERY_INTRO_SOUND_SCRIPT          = 0x5114000C,
    GALLERY_DEMO_SOUND_SCRIPT           = 0x5114000D,
    STORY_OPENING_SOUND_SCRIPT          = 0x5114000A,
    STORY_PIERCE_SOUND_SCRIPT           = 0x5114000B,
};

TaskDesc gShootingGalleryPierceGalleryTaskDescs[SHOOTING_GALLERY_PIERCE_GALLERY_TASK_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shootingGalleryPierceGalleryIntroTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, shootingGalleryPierceGalleryTrainingMenuTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shootingGalleryPierceGalleryRedFlashLoopTask, { .value = 0 } },
};

/// Player animation extension for the gallery's introduction and course menus: player clips 47..57 (53..56 empty).
/// Its bank-copy request reads past these eleven words into the requests that follow, so they keep their places.
#include "assets/rooms/mist/shooting_gallery/player_gallery_bank.inc"

enum { GALLERY_MENU_BLEND_FRAMES = 6 };

#define ROOM_PART _gShootingGalleryPierceGalleryPierceClip0Request
#include "assets/rooms/mist/shooting_gallery/pierce/events.inc"

#include "assets/rooms/mist/shooting_gallery/pierce_clips_2.inc"

/// Player animation extension for Pierce's story and conversation scenes.
#include "assets/rooms/mist/shooting_gallery/player_conversation_bank.inc"

enum { PIERCE_CONVERSATION_BLEND_FRAMES = 8 };

#define ROOM_PART _gShootingGalleryPierceConversationPierceClip0Request
#include "assets/rooms/mist/shooting_gallery/pierce/events.inc"

#include "../../../../shared/room/cap_captions_settings.inc.c"

/// Retained bodyless task recipe for the scene-clock caption schedule.
static TaskDesc _gCapCaptionScheduleTaskDesc = {
    { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _capCaptionRunSchedule, { .value = 0 }
};

#include "../../../../shared/room/cap_captions_schedule.inc.c"

#include "assets/characters/pierce/body.inc"

#include "assets/characters/pierce/glasses.inc"

#include "assets/rooms/mist/shooting_gallery/pierce_clips_3.inc"

/// Pierce's synchronous animation, paired-draw, placement, command and walk messages.
static TaskMessageEntry _gShootingGalleryPiercePierceMessageTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pacedWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pacedWalkSetPairModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _shootingGalleryPierceIgnoreCommand },
    { ACTOR_MESSAGE_WALK_TO, _pacedWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gShootingGalleryPiercePierceTaskDescs[SHOOTING_GALLERY_PIERCE_PIERCE_TASK_COUNT] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _shootingGalleryPiercePierceTask, { .model = &_gPierceBody } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _pacedWalkSubModelTask, { .model = &_gPierceGlasses } },
};

/// Pierce's body animation bank, indexed directly by scripted clip ID.
#include "assets/rooms/mist/shooting_gallery/pierce_bank.inc"

/// Owned handle of the gallery introduction's repeated red-flash task.
static Task* _gShootingGalleryPierceGalleryRedFlashLoopTask = NULL;

/// Borrowed relocated command-reference entries of the loaded CAP resource.
static const CapCommandRef* _gCapCaptionCommandRefs = NULL;

/// Glyph and title cells borrowed read-only from the selected loaded CAP file.
static const TextGlyphCell* _gCapCaptionGlyphCells = NULL;

/// Selected CAP sequence command and its following text records.
static CapSequenceRecord* _gCapCaptionSequence = NULL;

/// Biased left X of the selected caption's widest closed line, in screen pixels.
static s16 _gCapCaptionBlockLeftX = 0;

/// First baseline of the selected caption block, in screen pixels.
static s16 _gCapCaptionFirstBaselineY = 0;

/// Selected caption's bottom baseline in screen pixels.
static s16 _gCapCaptionBottomBaselineY = 0;

/// Selected text-record slot within the current CAP sequence.
static s16 _gCapCaptionRecordIndex = 0;

/// Height of the selected caption's closed lines, in screen pixels.
static s16 _gCapCaptionBlockHeight = 0;

/// Selected caption variant key, compared with each record's unsigned byte key.
static s16 _gCapCaptionSelectedKey = 0;

/// Continuation triangle's left X, retaining the low halfword of draw-coordinate pixels.
static u16 _gCapCaptionCaretLeftX = 0;

/// Continuation triangle's tip Y, retaining the low halfword of draw-coordinate pixels.
static u16 _gCapCaptionCaretTipY = 0;

/// Per-instance caret countdown and its uninterpreted retained bytes.
static CapCaptionCaretDelayStorage _gCapCaptionCaretDelayStorage = { 0, { 35, 192, 0 } };

s32 gShootingGalleryPierceGallerySelectedCourseLevel;

/// Owned copy of the deferred gallery room-exit request.
RoomEventMsg _gShootingGalleryPierceGalleryPendingExitRequest;

/// Holds the training scene through its exit reply and queues the decision task.
static inline void _shootingGalleryPierceStartGalleryExitPrompt(void)
{
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
    capRunCommand(GALLERY_EXIT_CONFIRM_COMMAND, CAP_PLAYBACK_IN_PLACE);
    gCapPreserveActorControl = GALLERY_EXIT_KEEP_ACTORS_HELD;
    taskSpawnFromTable(gShootingGalleryPierceGalleryDecisionTaskDescs, GALLERY_EXIT_DECISION_TASK, GALLERY_END_TRAINING, 0);
}

void shootingGalleryPierceCheckGalleryExitInput(void)
{
    enum {
        EXIT_INPUT_COOLDOWN_TICKS = 60,
        EXIT_INPUT_VIEW           = 18,
        EXIT_ZONE_MAX_X           = -6150,
        EXIT_ZONE_MIN_Z           = 4301,
        EXIT_ZONE_END_Z           = 5700,
        EXIT_UP_YAW_START         = 0xA01,
        EXIT_DOWN_YAW_START       = 0x201,
        EXIT_YAW_WINDOW           = 0x3FF,
        YAW_MASK                  = 0xFFF,
    };
    Task*      playerTask;
    GameActor* player;
    GfxCoord*  rootCoord;
    s32        rootZ;
    s32        yaw;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player     = playerTask->work;
    rootCoord  = playerTask->extra.tmd->coords;
    if (gShootingGalleryPierceGalleryTrainingActive != 0) {
        if (gShootingGalleryPierceGallerySelectedCourseLevel >= GALLERY_FIRST_FREE_MOVEMENT_COURSE) {
            if (gGameSession->location.view == EXIT_INPUT_VIEW) {
                mistShootingGallerySetTrainingBarrierLowered(0);
                gShootingGalleryPierceGalleryExitInputBlocked = 0;
            }
            // Rearm the interaction only after the attachment action has settled.
            if ((u32)((u8)gAttachmentState.mode - ATTACHMENT_MODE_ARMED) < 2U) {
                gShootingGalleryPierceGalleryExitInputCooldownTicks = EXIT_INPUT_COOLDOWN_TICKS;
            }
            if (gShootingGalleryPierceGalleryExitInputCooldownTicks != 0) {
                gShootingGalleryPierceGalleryExitInputCooldownTicks -= 1;
                return;
            }
            if ((player->mode != GAME_ACTOR_MODE_SCRIPTED) && (capIsBusy() == 0) && (gShootingGalleryPierceGalleryExitInputBlocked == 0) &&
                (gPlayerActorStateTickHeld == 0) && (rootCoord->coord.t[0] < EXIT_ZONE_MAX_X)) {
                rootZ = rootCoord->coord.t[2];
                if (rootZ < EXIT_ZONE_END_Z) {
                    if ((rootZ >= EXIT_ZONE_MIN_Z) && (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                        yaw = (u16)player->rotation.vy & YAW_MASK;
                        if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_UP) != 0) {
                            if ((u32)(yaw - EXIT_UP_YAW_START) < (u32)EXIT_YAW_WINDOW) {
                                _shootingGalleryPierceStartGalleryExitPrompt();
                            }
                        }
                        if ((padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_DOWN) != 0) && ((u32)(yaw - EXIT_DOWN_YAW_START) < (u32)EXIT_YAW_WINDOW)) {
                            _shootingGalleryPierceStartGalleryExitPrompt();
                        }
                    }
                }
            }
        }
    }
}

void shootingGalleryPierceGalleryExitDecisionTask(Task* task)
{
    enum {
        EXIT_WAIT_REPLY         = 0,
        EXIT_WAIT_TRANSITION    = 1,
        EXIT_REPLY_CONTINUE     = 1,
        RETURN_RESOURCE_VARIANT = 3,
        RETURN_SCENE_EVENT      = 1,
        RETURN_SPRITE_VARIANT   = 1,
        RETURN_SOUND            = MIST_SHOOTING_GALLERY_SOUND(MIST_SHOOTING_GALLERY_SOUND_DOOR_3_OPEN),
    };
    switch (task->state) {
        case EXIT_WAIT_REPLY:
            if (task->spawnArg1.value == GALLERY_COMMIT_ROOM_EXIT) {
                task->state = EXIT_WAIT_TRANSITION;
                break;
            }
            if (capIsBusy() != 0) {
                break;
            }
            if (capGetVariantKey() == EXIT_REPLY_CONTINUE) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                break;
            }
            if (task->spawnArg1.value != 0) {
                if (gameFlagGetNibble(EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT) != 0) {
                    capRunCommandWithTransition(GALLERY_EXIT_TRANSITION_COMMAND);
                }
                gGameSession->battleResetPending = 1;
                task->state                     += 1;
                break;
            }
            if (gShootingGalleryPierceGallerySelectedCourseLevel == GALLERY_FIRST_FREE_MOVEMENT_COURSE) {
                gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
            }
            gShootingGalleryPierceGalleryTrainingActive = 0;
            mistShootingGallerySetTrainingBarrierLowered(1);
            gShootingGalleryPierceGalleryExitInputBlocked = 1;
            taskCallExit(gMistShootingGalleryControllerTask);
            gGameSession->battleResetPending = 1;
            gGameSession->flowFlags         |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            sndEvtRequestMidiStop(0, GALLERY_MUSIC_FADE_AUDIO_TICKS);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            taskKill(task);
            break;
        case EXIT_WAIT_TRANSITION:
            // Restore the carried loadout before reloading the retained destination.
            if (capIsBusy() == 0) {
                gPlayerStatus.resourceVariant                       = RETURN_RESOURCE_VARIANT;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = RETURN_SCENE_EVENT;
                inventoryRestoreCarriedLoadout();
                gGameSession->hideHud = 1;
                sndEvtRequestScriptStart(RETURN_SOUND, 0, 0);
                gDisplayState.spriteVariant                                 = RETURN_SPRITE_VARIANT;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = _gShootingGalleryPierceGalleryPendingExitRequest.areaId;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = _gShootingGalleryPierceGalleryPendingExitRequest.warp;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = _gShootingGalleryPierceGalleryPendingExitRequest.roomState;
                taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
                taskKill(task);
            }
            break;
    }
}

void shootingGalleryPierceGalleryAbortDecisionTask(Task* task)
{
    enum {
        WAIT_REPLY  = 0,
        RETURN_VIEW = 1,
        FINISH      = 2,
        REPLY_STOP  = 2,
    };
    GameActor* player;

    player = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work;
    switch (task->state) {
        case WAIT_REPLY:
            if (capIsBusy() != 0) {
                break;
            }
            if (capGetVariantKey() == REPLY_STOP) {
                taskCallExit(gMistShootingGalleryControllerTask);
                task->state++;
            } else {
                taskKill(task);
            }
            break;
        case RETURN_VIEW:
            // Let the normal room update observe the saved return view on the next tick.
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = GALLERY_RETURN_VIEW;
            mistShootingGallerySelectRoomLights(0);
            task->state++;
            break;
        case FINISH:
            gGameSession->battleResetPending = 1;
            gGameSession->flowFlags         |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            sndEvtRequestMidiStop(0, GALLERY_MUSIC_FADE_AUDIO_TICKS);
            player->movementInputDisabled               = 0;
            gShootingGalleryPierceGalleryTrainingActive = 0;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
            if (gDisplayState.holdCount != 0) {
                displayReleaseMenuHold();
            }
            taskKill(task);
            break;
    }
}

void shootingGalleryPierceFinishGalleryCourse(void)
{
    gShootingGalleryPierceGalleryTrainingActive = 0;
    if (gShootingGalleryPierceGallerySelectedCourseLevel < GALLERY_FIRST_FREE_MOVEMENT_COURSE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = GALLERY_RETURN_VIEW;
        mistShootingGallerySelectRoomLights(0);
    } else {
        mistShootingGallerySetTrainingBarrierLowered(1);
        gShootingGalleryPierceGalleryExitInputBlocked = 1;
    }
    if (gShootingGalleryPierceGallerySelectedCourseLevel < GALLERY_FIRST_SWAP_COURSE) {
        gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
    }
    sndEvtRequestMidiStop(0, GALLERY_MUSIC_FADE_AUDIO_TICKS);
}

void shootingGalleryPierceRequestGalleryAbort(void)
{
    if (gShootingGalleryPierceGallerySelectedCourseLevel == GALLERY_BONUS_COURSE) {
        gShootingGalleryPierceGalleryTrainingActive = 0;
        mistShootingGallerySetTrainingBarrierLowered(1);
        gShootingGalleryPierceGalleryExitInputBlocked = 1;
        gGameSession->battleResetPending              = 1;
        sndEvtRequestMidiStop(0, GALLERY_MUSIC_FADE_AUDIO_TICKS);
        gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
    }
    if (gShootingGalleryPierceGallerySelectedCourseLevel < GALLERY_FIRST_FREE_MOVEMENT_COURSE) {
        capRunCommand(GALLERY_ABORT_CONFIRM_COMMAND, CAP_PLAYBACK_CLEAR_IF_UNSTARTED);
        taskSpawnFromTable(gShootingGalleryPierceGalleryDecisionTaskDescs, GALLERY_ABORT_DECISION_TASK, 0, 0);
    }
}

s32 shootingGalleryPierceResolveGalleryExit(const RoomEventMsg* request)
{
    if (gShootingGalleryPierceGalleryTrainingActive != 0) {
        if (request->queryOnly != ROOM_EVENT_EXECUTE) {
            return SHOOTING_GALLERY_PIERCE_GALLERY_EXIT_DEFER;
        }
        // The decision task owns this full request copy until its deferred reload.
        _gShootingGalleryPierceGalleryPendingExitRequest = *request;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        capRunCommand(GALLERY_EXIT_CONFIRM_COMMAND, CAP_PLAYBACK_IN_PLACE);
        gCapPreserveActorControl = GALLERY_EXIT_KEEP_ACTORS_HELD;
        taskSpawnFromTable(gShootingGalleryPierceGalleryDecisionTaskDescs, GALLERY_EXIT_DECISION_TASK, GALLERY_CONFIRM_ROOM_EXIT, 0);
    } else {
        // Keep the one-shot scope for the original query branch's delay-slot layout.
        do {
            if (gameFlagGetNibble(EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT) == 0) {
                return SHOOTING_GALLERY_PIERCE_GALLERY_EXIT_ALLOW;
            }
            if (request->queryOnly != ROOM_EVENT_EXECUTE) {
                break;
            }
            _gShootingGalleryPierceGalleryPendingExitRequest = *request;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(GALLERY_EXIT_TRANSITION_COMMAND);
            taskSpawnFromTable(gShootingGalleryPierceGalleryDecisionTaskDescs, GALLERY_EXIT_DECISION_TASK, GALLERY_COMMIT_ROOM_EXIT, 0);
        } while (0);
    }
    return SHOOTING_GALLERY_PIERCE_GALLERY_EXIT_DEFER;
}

void shootingGalleryPierceHandleGalleryAction(void)
{
    enum { GALLERY_ACTION_COMMAND = 0x11 };

    if (gShootingGalleryPierceGalleryTrainingActive != 0) {
        mistShootingGallerySignalAction();
        return;
    }
    capSpawnEventIfIdle(GALLERY_ACTION_COMMAND, CAP_EVENT_PAUSE_ACTORS);
}

/// Runs the first-visit gallery introduction and hands control to its menu or exit scene.
static void _shootingGalleryPierceGalleryIntroTask(Task* task)
{
    enum {
        INTRO_START         = 0,
        INTRO_WAIT          = 1,
        REPLY_START         = 2,
        REPLY_WAIT          = 3,
        EXIT_START          = 4,
        MENU_REPLY_START    = 10,
        MENU_REPLY_WAIT     = 11,
        MENU_REPLY_SEQUENCE = 7,
        EXIT_REPLY_SEQUENCE = 8,
    };

    switch (task->state) {
        case INTRO_START:
            // Mark the introduction before playback, so skipping still consumes it.
            gGameSession->hideHud = 1;
            if (gameFlagGetNibble(EVENT_FLAG_MIST_SHOOTING_GALLERY_INTRO_SEEN) == 0) {
                gameFlagSetNibble(EVENT_FLAG_MIST_SHOOTING_GALLERY_INTRO_SEEN, 1);
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, EVENT_FLAG_OBJECTIVE_MIST_SHOOTING_GALLERY_START);
                evsStartScriptWithSkip(_gShootingGalleryPierceGalleryIntroScript, EVENT_SCRIPT_HUD_KEEP, _gShootingGalleryPierceGalleryIntroSkipScript);
                task->state++;
            } else {
                taskSpawnFromTable(gShootingGalleryPierceGalleryTaskDescs, SHOOTING_GALLERY_PIERCE_GALLERY_MENU_TASK, 0, 0);
                taskKill(task);
            }
            break;
        case INTRO_WAIT:
            if (gGameSession->eventState == 0) {
                task->state++;
            }
            break;
        case REPLY_START:
            evsStartScript(_gShootingGalleryPierceGalleryTrainingOfferScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case REPLY_WAIT:
            if (gGameSession->eventState == 0) {
                if (capGetVariantKey() != 0) {
                    task->state = MENU_REPLY_START;
                } else {
                    task->state++;
                }
            }
            break;
        case EXIT_START:
            capStartSequenceSlot(EXIT_REPLY_SEQUENCE, 0, 0);
            evsStartScript(gShootingGalleryPierceGalleryDeclineTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            taskKill(task);
            break;
        case MENU_REPLY_START:
            capStartSequenceSlot(MENU_REPLY_SEQUENCE, 0, 0);
            evsStartScript(_gShootingGalleryPierceGalleryIntroAcceptTrainingScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case MENU_REPLY_WAIT:
            if (gGameSession->eventState == 0) {
                taskSpawnFromTable(gShootingGalleryPierceGalleryTaskDescs, SHOOTING_GALLERY_PIERCE_GALLERY_MENU_TASK, 0, 0);
                taskKill(task);
            }
            break;
    }
}

/// Spawns a gallery red flash whenever its elapsed counter is divisible by 48.
static void _shootingGalleryPierceGalleryRedFlashLoopTask(Task* task)
{
    enum {
        GALLERY_RED_FLASH_PERIOD_TICKS = 48,
    };

    if (task->killCountdown % GALLERY_RED_FLASH_PERIOD_TICKS == 0) {
        taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
    }
    task->killCountdown = task->killCountdown + 1;
}

/// Starts the gallery introduction's movie session, including the cleared-run splash.
static void _shootingGalleryPierceStartGalleryMovie(void)
{
    enum { GALLERY_MOVIE_SESSION_TASK = 0 };

    taskSpawnFromTable(gMistShootingGalleryMovieTaskDescs, GALLERY_MOVIE_SESSION_TASK, 0, 0);
}

/// Spawns the introduction's demonstration target for a nonzero enable word.
static void _shootingGalleryPierceSpawnGalleryDemoTargetIfEnabled(s32 enabled)
{
    if (enabled != 0) {
        mistShootingGallerySpawnDemoTarget();
    }
}

/// Starts or stops the repeated red flashes used during the gallery introduction.
static void _shootingGalleryPierceSetGalleryRedFlashLoop(s32 enabled)
{
    if (enabled != 0) {
        _gShootingGalleryPierceGalleryRedFlashLoopTask = taskSpawnFromTable(gShootingGalleryPierceGalleryTaskDescs, SHOOTING_GALLERY_PIERCE_GALLERY_RED_FLASH_LOOP_TASK, 0, 0);
        return;
    }
    if (_gShootingGalleryPierceGalleryRedFlashLoopTask != NULL) {
        taskKill(_gShootingGalleryPierceGalleryRedFlashLoopTask);
        _gShootingGalleryPierceGalleryRedFlashLoopTask = NULL;
    }
}

/// Selects default room lights for zero, or alternate lights for a nonzero halfword.
static void _shootingGalleryPierceSelectGalleryRoomLights(s16 useAlternate)
{
    mistShootingGallerySelectRoomLights(useAlternate);
}

/// Sets the deferred view-respawn request when the introduction is skipped.
static void _shootingGalleryPierceSetViewRespawnPending(s16 pending)
{
    gGameSession->viewDirty = pending;
}

/// Selects the CAP resource and texture page for Pierce's conversation scenes.
static void _shootingGalleryPierceSetConversationCap(s32 enabled)
{
    enum {
        RESOURCE_ORDINAL = 1,
        VRAM_X           = 768,
        VRAM_Y           = 0,
    };

    if (enabled != 0) {
        gCapFile = NULL;
        capSelectLoadedFile(RESOURCE_ORDINAL);
        capSetTexturePage(VRAM_X, VRAM_Y);
        return;
    }
    capReset();
}

void shootingGalleryPierceStartPierceConversation(void)
{
    switch (gameFlagGetNibble(EVENT_FLAG_MIST_PIERCE_TALK_PROGRESS)) {
        case MIST_PIERCE_TALK_FIRST:
            gameFlagSetNibble(EVENT_FLAG_MIST_PIERCE_TALK_PROGRESS, MIST_PIERCE_TALK_SECOND);
            evsStartScript(_gShootingGalleryPiercePierceFirstTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
        case MIST_PIERCE_TALK_SECOND:
            evsStartScript(_gShootingGalleryPiercePierceSecondTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(EVENT_FLAG_MIST_PIERCE_TALK_PROGRESS, MIST_PIERCE_TALK_REPEAT);
            break;
        case MIST_PIERCE_TALK_REPEAT:
            evsStartScript(_gShootingGalleryPiercePierceRepeatTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

#include "../../../../shared/room/cap_captions.inc.c"

/// Selects a keyed caption and spawns its drawing task for signed tick duration.
static void _shootingGalleryPierceShowTimedCaption(s16 commandIndex, s16 key, s16 durationTicks)
{
    _capCaptionShowTimed(commandIndex, key, durationTicks);
}

#include "../../../../shared/room/cap_captions_resource.inc.c"

void shootingGalleryPierceSelectCaptionResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex)
{
    _capCaptionLoadResource(texturePageX, texturePageY, dataResourceIndex);
}

/// Spawns and adopts Pierce's paired model with the parent's placement textures.
static inline Task* _shootingGalleryPierceSpawnPiercePair(Enemy* enemy, Task* task)
{
    Enemy* pairedEnemy;

    pairedEnemy = enemySpawnFromTable(gShootingGalleryPiercePierceTaskDescs, SHOOTING_GALLERY_PIERCE_PIERCE_PAIR_TASK, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(pairedEnemy->task->extra.tmd, enemy);
    taskReparent(task, pairedEnemy->task);
    return pairedEnemy->task;
}

/// Allocates Pierce's paced-walk state, adopts his paired model and starts clip 12.
static void _shootingGalleryPierceSpawnPierce(Enemy* enemy, Task* task)
{
    enum {
        INITIAL_CLIP = 12,
        LIGHT_HEIGHT = 800,
        LIGHT_COUNT  = 3,
    };
    VECTOR         lightingPosition;
    PacedWalkWork* work;
    GfxCoord*      rootCoord;
    TmdObject*     model;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if ((task->work = work = memCalloc(sizeof(*work), false)) == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _pacedWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->flags                     = 0;
    model->otOffset                  = 1;
    work->enemy                      = enemy;
    // The paired model becomes a child released with the parent task tree.
    work->pairTask  = _shootingGalleryPierceSpawnPiercePair(enemy, task);
    work->st.animId = INITIAL_CLIP;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    // Sample lighting 800 world units above the root, then seed the actor rig.
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1] - LIGHT_HEIGHT;
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingPosition, 0, LIGHT_COUNT);
    animationInitContext(&work->rig.anim, _gShootingGalleryPiercePierceAnimationSets, model,
                         work->rig.poses, work->rig.slots);
    work->st.state = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable = _gShootingGalleryPiercePierceMessageTable;
    _pacedWalkUpdate(task);
    task->state++;
}

#include "../../../../shared/walk/paced_walk_update.inc.c"

/// Dispatches Pierce's spawn state or paced movement, animation, lighting and shadow.
static void _shootingGalleryPiercePierceTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _shootingGalleryPierceSpawnPierce,
        _actorRenderWalkerFrame,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
#define ACTOR_RENDER_UPDATE_WALKER _pacedWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the walker's enemy record and tears down its task tree.
static void _pacedWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../../../shared/walk/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../../../shared/walk/paced_walk_tick_anim.inc.c"

#include "../../../../shared/walk/paced_walk_reset_anim.inc.c"

#include "../../../../shared/walk/paced_walk_blend_anim.inc.c"

#include "../../../../shared/walk/paced_walk_apply_animation_request.inc.c"

/// Immediately reseeds the walker's non-root tracks from a scripted play request.
static s32 _pacedWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum {
        PACED_WALK_ANIMATION_ID_LIMIT         = ARRAY_SIZE(_gShootingGalleryPiercePierceAnimationSets),
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

#include "../../../../shared/walk/paced_walk_place.inc.c"

/// Ignores actor commands without changing the walker.
static s32 _shootingGalleryPierceIgnoreCommand(Task* task, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument)
{
    return 0;
}

#include "../../../../shared/walk/paced_walk_to.inc.c"

#include "../../../../shared/walk/paced_walk_attach_carried_model.inc.c"

/// Attaches the carried model to walker part 4 and refreshes its root composition.
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
