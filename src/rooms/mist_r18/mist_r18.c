#include "rooms/mist_r18.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/starter_inventory.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"
#include "../../shared/room/backdrop_crossfade.h"

#include "assets/rooms/mist/r18/declarations.inc"

/// Packet placement and storage layout of this room's captions and captured backdrop.
enum {
    MIST_R18_CAPTION_SPRITE_OT_SLOT = 4,
    MIST_R18_CAPTION_PLATE_OT_SLOT  = 5,
    MIST_R18_TEXTURE_DEPTH_4BIT     = 0,
    MIST_R18_TEXTURE_SHADE_UNITY    = 0x80,
    MIST_R18_BACKDROP_LEFT_VRAM_X   = 832,
    MIST_R18_BACKDROP_RIGHT_VRAM_X  = 640,
    MIST_R18_BACKDROP_RIGHT_VRAM_Y  = 256,
};

/// Next briefing script to start after the current event and menu have finished.
enum {
    MIST_R18_BRIEFING_PLACE_PROP         = 1,
    MIST_R18_BRIEFING_FIRST_HELD_MODEL   = 2,
    MIST_R18_BRIEFING_OPEN_KEY_ITEMS     = 3,
    MIST_R18_BRIEFING_CHECK_DRYFIELD_MAP = 4,
    MIST_R18_BRIEFING_DEPARTING          = 5,
};

static void _modelPlacementAttachPartTask(Task* childTask);
static void _mistR18AdvanceBriefingState(Task* unusedTask);
static void _mistR18DrawFadeSprites(s32 fadeActive, s32 shade);
static void _mistR18CaptureBackdropState(Task* task);
static void _mistR18AttachedModelIdleState(Task* task);
static void _mistR18DrawTile(const PrimDrawParams* draw);
static void _mistR18DrawSprite(const PrimDrawParams* draw, u32 clutX, s32 clutY);
static void _mistR18SetTexturePage(s16 blendMode, s16 vramX, s16 vramY, s32 orderingTableSlot);
static void _mistR18WaitForCrossfadeViewState(Task* task);
static void _mistR18InitBriefingState(Task* task);

static TaskDesc _gMistR18TaskDescs[8];
/// Saved task drawing the first briefing model on the player's part 8.
///
/// NULL before spawning or after explicit teardown, including spawn failure.
/// Its attachment joins the player's teardown tree; clear this handle before
/// using it again if that tree has been killed. Room setup requires no live
/// previous instance because it resets the handle without killing a task.
static Task* _gMistR18FirstAttachedModelTask;

/// Saved task drawing the second briefing model on the player's part 8.
///
/// NULL before spawning or after explicit teardown, including spawn failure.
/// Its attachment joins the player's teardown tree; clear this handle before
/// using it again if that tree has been killed. Room setup requires no live
/// previous instance because it resets the handle without killing a task.
static Task* _gMistR18SecondAttachedModelTask;

/// Saved task drawing the briefing's separately placed prop.
///
/// Each spawn overwrites the handle, including with NULL on failure; scripts
/// tear down the previous instance before spawning again. Explicit teardown
/// kills a non-NULL task and clears the handle. Room setup only clears it,
/// so no previous instance may remain live when setup runs.
static Task* _gMistR18PlacedPropTask;

/// Next briefing phase to launch after the current event and menu finish.
///
/// Uses MIST_R18_BRIEFING_* values 1 place prop, 2 first held model, 3 open
/// key items, 4 check the Dryfield-map choice and 5 departure already started.
/// Phase 4 repeats the reminder until the map is selected; phase 5 launches
/// nothing further. This signed word is separate from the room task's state.
static s32 _gMistR18BriefingStep;
/// Keeps the key-item menu's collected-item bits and Dryfield-map choice on reminders.
///
/// Zero initializes the four collection bits and clears the map-opened latch;
/// one retains that state when reopening the menu. Written before each menu
/// script starts and forwarded as a signed word to the map overlay.
static s32 _gMistR18RetainKeyItemMenuState;

/// Attachment, idle and teardown callbacks for the room's two held models.
///
/// Indexed by task state (0 attach to the parent part, 1 retain attachment,
/// 2 kill), without a terminator or bounds check. Dispatch copies all three
/// pointers by value; the callback code must remain loaded through the call.
static const TaskFuncTable3 _gMistR18AttachedModelStates = {
    { _modelPlacementAttachPartTask, _mistR18AttachedModelIdleState, taskKill },
};

/// Initialization, script/menu progression and teardown callbacks for the briefing task.
///
/// Task states 0..2 index these three pointers without a bounds check or
/// terminator. Dispatch copies the table by value; keep this room loaded
/// through the call. These states are separate from the briefing-script cursor.
static const TaskFuncTable3 _gMistR18BriefingStates = {
    { _mistR18InitBriefingState, _mistR18AdvanceBriefingState, taskKill },
};

/// Capture, view-wait, crossfade and teardown callbacks for the frozen backdrop.
///
/// Indexed by task state (0 capture, 1 wait for the replacement view,
/// 2 crossfade to the live frame, 3 kill). Dispatch copies all four pointers
/// by value and performs no bounds check; keep their code loaded through use.
static const TaskFuncTable4 _gMistR18CrossfadeStates = {
    { _mistR18CaptureBackdropState, _mistR18WaitForCrossfadeViewState, _crossfadeOutState, taskKill },
};

/// Attached-model selectors passed by the briefing scripts and their parent part.
enum {
    MIST_R18_ATTACHMENT_MODEL_FIRST  = 0,
    MIST_R18_ATTACHMENT_MODEL_SECOND = 1,
    MIST_R18_ATTACHMENT_PLAYER_PART  = 8,
};
static void _mistR18SpawnAttachedModel(s32 modelIndex);
static void _mistR18KillAttachedModel(s32 modelIndex);
static void _mistR18SpawnBriefingCaption(void);
static void _mistR18StartBackdropCrossfade(void);
static void _mistR18SpawnPlacedProp(void);
static void _mistR18KillPlacedProp(void);
static void _mistR18ReloadParkingWithStarterLoadout(void);
static void _mistR18PrepareBriefingScene(void);
static void _mistR18StartSceneScriptPause(void);
static void _mistR18EnqueueScenePlayback(void);
static void _mistR18FinishSceneStream(void);
static void _mistR18CancelScene(void);
static void _mistR18SetOrderingDepthShift(s8 depthShift);
static void _mistR18OpenKeyItemMenu(void);

static AnimationSet _gMistR18MistR18PlayerBriefingAnim01Clip;
static AnimationSet _gMistR18MistR18PlayerBriefingAnim02Clip;
static AnimationSet _gMistR18MistR18PlayerBriefingAnim03Clip;
static AnimationSet _gMistR18MistR18PlayerBriefingAnim04Clip;
static AnimationSet _gMistR18MistR18PlayerBriefingAnim05Clip;
static AnimationSet _gMistR18MistR18PlayerBriefingAnim06Clip;
static AnimationSet _gMistR18MistR18PlayerBriefingAnim07Clip;
static AnimationSet _gMistR18MistR18PlayerBriefingAnim08Clip;
static AnimationSet _gMistR18MistR18PlayerBriefingAnim09Clip;
static AnimationSet _gMistR18MistR18PlayerBriefingAnim10Clip;
static TmdSource    _gMistR18BriefingFile;
static TmdSource    _gMistR18Laptop;
static void         _mistR18CaptionTask(Task* task);
static void         _mistR18TextureFadeTask(Task* task);
static void         _mistR18AttachedModelTask(Task* task);
static void         _mistR18FadeTileTask(Task* task);
static void         _mistR18CrossfadeTask(Task* task);
static void         _mistR18PlacePropTask(Task* task);
static void         _mistR18ReleaseScriptPauseTask(Task* task);

#include "assets/rooms/mist/r18/briefing_file.inc"

#include "assets/rooms/mist/r18/laptop.inc"

#include "assets/rooms/mist/r18/clips.inc"

/// Glyph-byte script for the briefing's two-line date and M.I.S.T. location caption.
///
/// The first 56 bytes select resident cells 0..69 or a line break, followed by
/// TEXT_STREAM_END and three retained zero bytes. The reveal's post-end probe
/// is inside this array; readable storage for its predecessor probe is unproven.
static u8 _gMistR18BriefingCaptionScript[60] = {
    18,
    30,
    41,
    45,
    30,
    38,
    27,
    30,
    43,
    69,
    56,
    65,
    69,
    69,
    53,
    61,
    61,
    61,
    69,
    69,
    52,
    62,
    55,
    55,
    0,
    12,
    TEXT_STREAM_LINE_BREAK,
    12,
    66,
    8,
    66,
    18,
    66,
    19,
    66,
    69,
    2,
    30,
    39,
    45,
    30,
    43,
    65,
    69,
    69,
    11,
    40,
    44,
    69,
    0,
    39,
    32,
    30,
    37,
    30,
    44,
    TEXT_STREAM_END,
    0,
    0,
    0,
};

/// Mutable date-and-place caption state borrowed by the briefing's single reveal task.
///
/// Draws at (-150, -90) pixels with 13-pixel line spacing. Stored plate dimensions
/// are 216 by 29; the emitter subtracts one from each dimension.
/// Uses the room font at VRAM (704, 48) and palette (16, 260). The child resets
/// cursor, reveals a script byte every two ticks and reloads 45 for the final
/// hold. Keep this stream, script, font pixels and palette live until teardown;
/// simultaneous reveals would share the cursor and are not supported.
static TextStream _gMistR18BriefingCaptionStream = {
    .x           = -150,
    .y           = -90,
    .tpageX      = 704,
    .tpageY      = 48,
    .clutX       = 16,
    .clutY       = 260,
    .charDelay   = 1,
    .cursor      = 0,
    .chars       = _gMistR18BriefingCaptionScript,
    .glyphs      = gTextCaptionGlyphCells,
    .lineHeight  = 13,
    .delayReload = 45,
    .boxWidth    = 216,
    .boxHeight   = 29,
};

/// Execution priority shared by the briefing's directly spawned child tasks.
enum { MIST_R18_BRIEFING_TASK_PRIORITY = 192 };

/// Spawn recipes for the briefing's held models, prop and presentation tasks.
///
/// Direct spawns use indices 0..7; there is no terminator. Slots 0/1 attach
/// models, 2 fades textured rectangles, 3 crossfades the backdrop, 4 places the
/// prop, 5 reveals the caption, 6 draws a fade tile and 7 monitors script pause.
/// Slots 2 and 6 have no room-local spawner or event-command reference.
/// Descriptors are read synchronously; callbacks and borrowed model sources
/// remain live with this overlay until their spawned tasks are torn down.
static TaskDesc _gMistR18TaskDescs[8] = {
    { { { TASK_BODY_TMD, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18AttachedModelTask, { .model = &_gMistR18BriefingFile } },
    { { { TASK_BODY_TMD, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18AttachedModelTask, { .model = &_gMistR18Laptop } },
    { { { TASK_BODY_COORD, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18TextureFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18CrossfadeTask, { .value = 0 } },
    { { { TASK_BODY_TMD, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18PlacePropTask, { .model = &_gMistR18Laptop } },
    { { { TASK_BODY_NONE, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18CaptionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18FadeTileTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_R18_BRIEFING_TASK_PRIORITY } }, _mistR18ReleaseScriptPauseTask, { .value = 0 } },
};

/// Player-bank extension: NULL at slot 47, followed by briefing clips 48..57.
///
/// Copied by value into the equipped bank; the ten referenced room-owned sets
/// remain borrowed during playback. The complete table is eleven pointer words.
#include "assets/rooms/mist/r18/mist_r18_player_briefing_bank.inc"
#include "assets/rooms/mist/r18/events.inc"
#include "assets/rooms/mist/r18/header.inc"
#include "assets/rooms/mist/r18/geometry.inc"
#include "assets/rooms/mist/r18/cameras.inc"
#include "assets/rooms/mist/r18/sprites.inc"
#include "assets/rooms/mist/r18/placements.inc"
#include "assets/rooms/mist/r18/lights.inc"
#include "assets/rooms/mist/r18/surfaces.inc"

/// Indexed clips and transition timing used by the briefing requests.
enum {
    MIST_R18_PLAYER_STORED_BANK_INDEX  = 1, // EVS replaces this value in its local request copy.
    MIST_R18_PLACED_ACTOR_BANK         = 0,
    MIST_R18_PLAYER_BASE_CLIP_1        = 1,
    MIST_R18_PLAYER_BRIEFING_CLIP_48   = ANIMATION_BANK_BASE_SET_COUNT + 1,
    MIST_R18_PLAYER_BRIEFING_CLIP_49   = ANIMATION_BANK_BASE_SET_COUNT + 2,
    MIST_R18_PLAYER_BRIEFING_CLIP_50   = ANIMATION_BANK_BASE_SET_COUNT + 3,
    MIST_R18_PLAYER_BRIEFING_CLIP_51   = ANIMATION_BANK_BASE_SET_COUNT + 4,
    MIST_R18_PLAYER_BRIEFING_CLIP_52   = ANIMATION_BANK_BASE_SET_COUNT + 5,
    MIST_R18_PLAYER_BRIEFING_CLIP_53   = ANIMATION_BANK_BASE_SET_COUNT + 6,
    MIST_R18_PLAYER_BRIEFING_CLIP_54   = ANIMATION_BANK_BASE_SET_COUNT + 7,
    MIST_R18_PLAYER_BRIEFING_CLIP_55   = ANIMATION_BANK_BASE_SET_COUNT + 8,
    MIST_R18_PLAYER_BRIEFING_CLIP_56   = ANIMATION_BANK_BASE_SET_COUNT + 9,
    MIST_R18_PLAYER_BRIEFING_CLIP_57   = ANIMATION_BANK_BASE_SET_COUNT + 10,
    MIST_R18_BALDWIN_CLIP_1            = 1,
    MIST_R18_BALDWIN_CLIP_2            = 2,
    MIST_R18_BALDWIN_CLIP_3            = 3,
    MIST_R18_BALDWIN_CLIP_4            = 4,
    MIST_R18_BALDWIN_CLIP_5            = 5,
    MIST_R18_BALDWIN_CLIP_6            = 6,
    MIST_R18_BALDWIN_CLIP_7            = 7,
    MIST_R18_BALDWIN_CLIP_8            = 8,
    MIST_R18_BALDWIN_CLIP_9            = 9,
    MIST_R18_BALDWIN_CLIP_10           = 10,
    MIST_R18_RUPERT_CLIP_1             = 1,
    MIST_R18_RUPERT_CLIP_2             = 2,
    MIST_R18_RUPERT_CLIP_3             = 3,
    MIST_R18_RUPERT_CLIP_4             = 4,
    MIST_R18_RUPERT_CLIP_5             = 5,
    MIST_R18_RUPERT_CLIP_6             = 6,
    MIST_R18_RUPERT_CLIP_7             = 7,
    MIST_R18_RUPERT_CLIP_8             = 8,
    MIST_R18_PLACED_ACTOR_BLEND_FRAMES = 6,
};

/// Baldwin's actor-command IDs used when the briefing reveals his held models.
enum {
    MIST_R18_BALDWIN_COMMAND_SHOW_FIRST_HELD_MODEL  = 0,
    MIST_R18_BALDWIN_COMMAND_SHOW_SECOND_HELD_MODEL = 2,
};

/// Baldwin's actor-command IDs used when the briefing hides his held models.
enum {
    MIST_R18_BALDWIN_COMMAND_HIDE_FIRST_HELD_MODEL  = 1,
    MIST_R18_BALDWIN_COMMAND_HIDE_SECOND_HELD_MODEL = 3,
};

/// Scene-child indices, CAP command slots and rendering choices used by these streams.
enum {
    MIST_R18_SCENE_ACTOR_RUPERT       = 0,
    MIST_R18_SCENE_ACTOR_BALDWIN      = 1,
    MIST_R18_CAP_COMMAND_INTRO        = 1,
    MIST_R18_CAP_COMMAND_FIRST_MODEL  = 2,
    MIST_R18_CAP_COMMAND_DEPARTURE    = 3,
    MIST_R18_CAP_COMMAND_PLACE_PROP   = 4,
    MIST_R18_CAP_COMMAND_MAP_REMINDER = 5,
    MIST_R18_ACTOR_DRAW_HIDE          = 0,
    MIST_R18_ACTOR_DRAW_SHOW          = 1,
    MIST_R18_WEAPON_ACTOR_PLAYER      = 0,
    MIST_R18_VIEW_CAPTION             = 2,
    MIST_R18_VIEW_BACKDROP            = 3,
    MIST_R18_VIEW_DIALOGUE            = 4,
    MIST_R18_VIEW_HELD_MODELS         = 5,
    MIST_R18_VIEW_PROP_INTERLUDE      = 10,
    MIST_R18_AMBIENT_COMPONENT        = 100, // EVS multiplies by 16: Q12 channel intensity 1600.
    MIST_R18_FADE_DEFAULT_FRAMES      = 0,   // Requests the interpreter's seven-frame default ramp.
    MIST_R18_FADE_RETAIN_FRAMES       = 0,   // Keeps the secondary fade's existing ramp duration on return.
};

/// Encoded 8-bit sprite texture page at (960, 0), with its CLUT at (0, 255).
enum {
    MIST_R18_SPRITE_TEXTURE_DEPTH_8BIT = 1,
    MIST_R18_SPRITE_TEXTURE_PAGE       = getTPage(MIST_R18_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    MIST_R18_SPRITE_CLUT               = getClut(0, 255),
};

/// Placement entry keys and decimal file numbers for the briefing's actor packages.
enum {
    MIST_R18_PLACEMENT_ENTRY_RUPERT  = 144,
    MIST_R18_PLACEMENT_ENTRY_BALDWIN = 105,
    MIST_R18_ACTOR_FILE_NUMBER       = 130,
};

/// Type-1 script request bases for the room's paired walking and running cues.
enum {
    MIST_R18_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 45,
    MIST_R18_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 47,
};

/// Reveals a glyph-script caption over a translucent black plate during an event.
///
/// Borrows the mutable `TextStream` in `spawnArg2.pointer`, its script and glyph
/// table until teardown. `charDelay` and `delayReload` are callback ticks;
/// a negative character delay reveals the complete caption on initialization.
/// Uses unmasked glyph bytes (the room script has no high-bit glyph indices),
/// centered draw-environment pixels and a plate inset of three pixels. The
/// compiled probe reads the byte before `chars` at cursor zero and one byte
/// after the terminator; the script storage must make both reads valid.
/// State 0 initializes, state 1 redraws and advances the cursor, and all other
/// states kill the task. Leaving the event also requests teardown.
static void _mistR18CaptionTask(Task* task)
{
    enum {
        MIST_R18_CAPTION_INITIALIZE = 0,
        MIST_R18_CAPTION_REVEAL     = 1,
        MIST_R18_CAPTION_FINISHED   = -1,
        MIST_R18_FONT_PAGE_U_MASK   = 0x3F,
    };
    PrimDrawParams draw;
    TextStream*    stream;
    s32            byteIndex;

    stream = task->spawnArg2.pointer;
    if (gGameSession->eventState == 0) {
        task->state = MIST_R18_CAPTION_FINISHED;
    }

    switch (task->state) {
        case MIST_R18_CAPTION_INITIALIZE:
            // Reset reveal timing; a negative delay exposes the complete script.
            stream->cursor = 0;
            if (stream->charDelay < 0) {
                byteIndex = 0;
                if (stream->chars[0] != TEXT_STREAM_END) {
                    do {
                        byteIndex++;
                        stream->cursor++;
                    } while (stream->chars[byteIndex] != TEXT_STREAM_END);
                }
                task->killCountdown = stream->delayReload;
            } else {
                task->killCountdown = stream->charDelay;
            }
            task->state++;
            return;

        case MIST_R18_CAPTION_REVEAL:
            // Redraw the revealed prefix, then advance its byte cursor on expiry.
            draw.x         = stream->x;
            draw.y         = stream->y;
            draw.u         = stream->tpageX;
            draw.v         = stream->tpageY;
            draw.r         = MIST_R18_TEXTURE_SHADE_UNITY;
            draw.g         = MIST_R18_TEXTURE_SHADE_UNITY;
            draw.b         = MIST_R18_TEXTURE_SHADE_UNITY;
            draw.semiTrans = 0;
            draw.unused_12 = ONE;

            if (stream->chars[stream->cursor - 1] != TEXT_STREAM_END) {
                // Glyph bytes directly index the font; line breaks reset the pen.
                {
                    for (byteIndex = 0; byteIndex < stream->cursor; byteIndex++) {
                        if (stream->chars[byteIndex] == TEXT_STREAM_LINE_BREAK) {
                            draw.x  = stream->x;
                            draw.y += stream->lineHeight;
                        } else {
                            draw.u = stream->glyphs[stream->chars[byteIndex]].u + (stream->tpageX & MIST_R18_FONT_PAGE_U_MASK);
                            draw.v = stream->glyphs[stream->chars[byteIndex]].v + (u8)stream->tpageY;
                            draw.w = stream->glyphs[stream->chars[byteIndex]].width;
                            draw.h = stream->glyphs[stream->chars[byteIndex]].height;
                            if (draw.h != 0) {
                                _mistR18DrawSprite(&draw, stream->clutX, stream->clutY);
                            }
                            draw.x += stream->glyphs[stream->chars[byteIndex]].width;
                        }
                    }
                }

                _mistR18SetTexturePage(GPU_BLEND_ADD, stream->tpageX, stream->tpageY, MIST_R18_CAPTION_SPRITE_OT_SLOT);

                if (--task->killCountdown < 0) {
                    stream->cursor++;
                    if (stream->chars[stream->cursor] == TEXT_STREAM_END) {
                        task->killCountdown = stream->delayReload;
                    } else {
                        task->killCountdown = stream->charDelay;
                    }
                }

                // The plate sits three pixels above and left of the pen.
                draw.x         = stream->x - 3;
                draw.y         = stream->y - 3;
                draw.w         = stream->boxWidth;
                draw.h         = stream->boxHeight;
                draw.b         = 0;
                draw.g         = 0;
                draw.r         = 0;
                draw.semiTrans = 1;
                _mistR18DrawTile(&draw);
                _mistR18SetTexturePage(GPU_BLEND_AVERAGE, 0, 0, MIST_R18_CAPTION_PLATE_OT_SLOT);
                return;
            }
            task->state++;
            return;

        default:
            break;
    }
    taskKill(task);
}

/// Starts a skippable briefing event and commits the cursor for its successor.
///
/// Both script arrays remain live through event playback. The interpreter marks
/// the event active before the cursor advances, preventing another launch.
/// Call only while EVS is idle. `nextStep` is the briefing cursor to run when
/// playback ends; it is independent of the room task's state.
static inline void _mistR18StartSkippableBriefingStep(EvsCommand* script, EvsCommand* skipScript, s32 nextStep)
{
    evsStartScriptWithSkip(script, EVENT_SCRIPT_HUD_HIDE_RESTORE, skipScript);
    _gMistR18BriefingStep = nextStep;
}

/// Advances the briefing scripts and repeats the key-item menu until the Dryfield map is opened.
///
/// Runs only while the event interpreter is idle, the attachment wheel is closed
/// and no display-mode transition is pending. The script cursor is independent
/// of the task's state. A repeated menu preserves its map-opened latch; departure
/// starts once the map choice is latched. `unusedTask` is required by task dispatch.
static void _mistR18AdvanceBriefingState(Task* unusedTask)
{
    enum { MIST_R18_EVENT_IDLE = 0 };
    s32 briefingStep;

    // Event completion and menu teardown must finish before another script starts.
    if ((gGameSession->eventState == MIST_R18_EVENT_IDLE) && (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
        briefingStep = _gMistR18BriefingStep;
        if (briefingStep == MIST_R18_BRIEFING_PLACE_PROP) {
            _mistR18StartSkippableBriefingStep(_gMistR18PlacePropScript, _gMistR18SkipPlacePropScript, MIST_R18_BRIEFING_FIRST_HELD_MODEL);
        } else if (briefingStep == MIST_R18_BRIEFING_FIRST_HELD_MODEL) {
            evsStartScriptWithSkip(_gMistR18FirstHeldModelScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gMistR18SkipFirstHeldModelScript);
            _gMistR18RetainKeyItemMenuState = false;
            _gMistR18BriefingStep           = MIST_R18_BRIEFING_OPEN_KEY_ITEMS;
        } else if (briefingStep == MIST_R18_BRIEFING_OPEN_KEY_ITEMS) {
            evsStartScript(_gMistR18OpenKeyItemsScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            _gMistR18BriefingStep = MIST_R18_BRIEFING_CHECK_DRYFIELD_MAP;
        } else if (briefingStep == MIST_R18_BRIEFING_CHECK_DRYFIELD_MAP) {
            // Reopen the reminder menu without resetting its map-opened latch.
            if (mapAkropolisWasDryfieldMapSelected() != true) {
                evsStartScript(_gMistR18DryfieldMapReminderScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                _gMistR18RetainKeyItemMenuState = true;
                return;
            }
            _mistR18StartSkippableBriefingStep(_gMistR18DepartureScript, _gMistR18SkipDepartureScript, MIST_R18_BRIEFING_DEPARTING);
        }
    }
}

/// Fades two fixed textured rectangles in, holds them opaque, then fades them out.
///
/// `killCountdown` holds RGB modulation, stepped by 21 per callback; the ramp
/// reaches 147 before switching to opaque raw-texture drawing. `spawnArg1.value`
/// counts hold callbacks, shortened by event skipping. State 0 queues the zero
/// level twice, states 1/3 ramp, state 2 holds, and later states kill the task.
/// This is task-table entry 2; no room-local spawner or event command uses it.
static void _mistR18TextureFadeTask(Task* task)
{
    enum {
        MIST_R18_TEXTURE_FADE_INITIALIZE = 0,
        MIST_R18_TEXTURE_FADE_IN         = 1,
        MIST_R18_TEXTURE_FADE_HOLD       = 2,
        MIST_R18_TEXTURE_FADE_OUT        = 3,
        MIST_R18_TEXTURE_FADE_STEP       = 21,
    };
    s32 fadeActive;

    fadeActive = 1;
    switch (task->state) {
        case MIST_R18_TEXTURE_FADE_INITIALIZE:
            task->killCountdown = 0;
            _mistR18DrawFadeSprites(1, 0);
            task->state++;
            break;
        case MIST_R18_TEXTURE_FADE_IN:
            task->killCountdown += MIST_R18_TEXTURE_FADE_STEP;
            if (task->killCountdown >= MIST_R18_TEXTURE_SHADE_UNITY + 1) {
                fadeActive = 0;
                task->state++;
            }
            break;
        case MIST_R18_TEXTURE_FADE_HOLD:
            fadeActive = 0;
            task->spawnArg1.value--;
            if ((task->spawnArg1.value <= 0) || (gGameSession->evtSkipped != 0)) {
                task->state++;
            }
            break;
        case MIST_R18_TEXTURE_FADE_OUT:
            task->killCountdown -= MIST_R18_TEXTURE_FADE_STEP;
            if (task->killCountdown < MIST_R18_TEXTURE_FADE_STEP + 1) {
                task->state++;
            }
            break;
        default:
            taskKill(task);
            return;
    }
    _mistR18DrawFadeSprites(fadeActive, task->killCountdown);
}

/// Queues the two fixed 4-bit texture rectangles used by the room's texture fade.
///
/// `fadeActive` selects modulated semitransparent drawing when nonzero, or
/// opaque raw-texture drawing when zero. `shade` supplies RGB modulation through
/// its low byte. Samples page (704, 0), with palettes at (0, 271)/(16, 271).
/// Reserves two `SPRT`s and one `DR_TPAGE` in the frame arena; the additive page
/// command executes first because OT insertion prepends packets. Requires that
/// texture/palette data and packet storage remain live until the GPU finishes.
static void _mistR18DrawFadeSprites(s32 fadeActive, s32 shade)
{
    enum {
        MIST_R18_SPRITE_RAW_OPAQUE_CODE      = 0x65,
        MIST_R18_SPRITE_MODULATED_BLEND_CODE = 0x66,
        MIST_R18_FADE_FIRST_CLUT             = getClut(0, 271),
        MIST_R18_FADE_SECOND_CLUT            = getClut(16, 271),
        MIST_R18_FADE_TEXTURE_PAGE           = getTPage(MIST_R18_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 704, 0),
    };
    SPRT*     sprite;
    DR_TPAGE* pageCommand;
    s16       screenX;
    s16       screenY;

    screenX        = -0x96;
    screenY        = -0x5A;
    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    setSprt(sprite);
    if (fadeActive == 0) {
        sprite->code = MIST_R18_SPRITE_RAW_OPAQUE_CODE;
    } else {
        sprite->code = MIST_R18_SPRITE_MODULATED_BLEND_CODE;
    }
    setXY0(sprite, screenX, screenY);
    sprite->clut = MIST_R18_FADE_FIRST_CLUT;
    setWH(sprite, 0xCF, 0x23);
    setRGB0(sprite, shade, shade, shade);
    setUV0(sprite, 0, 0);
    addPrim(gGpuCurrentOt + MIST_R18_CAPTION_SPRITE_OT_SLOT, sprite);

    screenX        = -0x22;
    screenY        = 0x36;
    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    setSprt(sprite);
    if (fadeActive == 0) {
        sprite->code = MIST_R18_SPRITE_RAW_OPAQUE_CODE;
    } else {
        sprite->code = MIST_R18_SPRITE_MODULATED_BLEND_CODE;
    }
    setXY0(sprite, screenX, screenY);
    setRGB0(sprite, shade, shade, shade);
    setUV0(sprite, 0, 0x24);
    sprite->clut = MIST_R18_FADE_SECOND_CLUT;
    setWH(sprite, 0xB7, 0x23);
    addPrim(gGpuCurrentOt + MIST_R18_CAPTION_SPRITE_OT_SLOT, sprite);

    pageCommand    = gGpuPrimCursor;
    gGpuPrimCursor = pageCommand + 1;
    setDrawTPage(pageCommand, true, false, MIST_R18_FADE_TEXTURE_PAGE);
    addPrim(gGpuCurrentOt + MIST_R18_CAPTION_SPRITE_OT_SLOT, pageCommand);
}

/// Captures the current 320x240 draw framebuffer as the crossfade's saved backdrop.
///
/// Splits at column 192, copying to VRAM (832, 0) and (640, 256). Buffer 0
/// starts at (0, 0), buffer 1 at (0, 272). The frame must already be rendered
/// when OT slot 8 executes. Reserves two `DR_MOVE`s and two `DR_STP`s, borrowed
/// by the GPU until frame completion. Resets the shade and advances the state.
static void _mistR18CaptureBackdropState(Task* task)
{
    enum {
        MIST_R18_FRAMEBUFFER_LOWER_Y = 272,
    };
    RECT     sourceRect;
    DR_STP*  maskCommand;
    DR_MOVE* copyCommand;
    s16      frameX;
    s16      frameY;

    if (gDisplayState.drawBuffer == 0) {
        frameX = 0;
        frameY = 0;
    } else {
        frameX = 0;
        frameY = MIST_R18_FRAMEBUFFER_LOWER_Y;
    }

    maskCommand    = gGpuPrimCursor;
    gGpuPrimCursor = maskCommand + 1;
    SetDrawStp(maskCommand, false);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, maskCommand);

    copyCommand    = gGpuPrimCursor;
    gGpuPrimCursor = copyCommand + 1;
    sourceRect.x   = frameX;
    sourceRect.y   = frameY;
    sourceRect.w   = CROSSFADE_BACKDROP_LEFT_WIDTH;
    sourceRect.h   = CROSSFADE_BACKDROP_HEIGHT;
    SetDrawMove(copyCommand, &sourceRect, MIST_R18_BACKDROP_LEFT_VRAM_X, 0);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, copyCommand);

    copyCommand    = gGpuPrimCursor;
    gGpuPrimCursor = copyCommand + 1;
    sourceRect.x   = frameX + CROSSFADE_BACKDROP_LEFT_WIDTH;
    sourceRect.y   = frameY;
    sourceRect.w   = CROSSFADE_BACKDROP_WIDTH - CROSSFADE_BACKDROP_LEFT_WIDTH;
    sourceRect.h   = CROSSFADE_BACKDROP_HEIGHT;
    SetDrawMove(copyCommand, &sourceRect, MIST_R18_BACKDROP_RIGHT_VRAM_X, MIST_R18_BACKDROP_RIGHT_VRAM_Y);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, copyCommand);

    // Head insertion executes mask-on, right copy, left copy, then mask-off.
    maskCommand    = gGpuPrimCursor;
    gGpuPrimCursor = maskCommand + 1;
    SetDrawStp(maskCommand, true);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, maskCommand);

    task->killCountdown = 0;
    task->state++;
}

#include "../../shared/room/backdrop_crossfade_live.inc.c"

/// Queues an additive redraw of the captured backdrop for the room crossfade.
///
/// Samples 192x240 pixels at VRAM (832, 0) and 128x240 at (640, 256), placing
/// them over the centered 320x240 frame. `shade` is RGB modulation (0 black,
/// 0x80 unchanged); only its low byte is stored. Requires the completed capture
/// and room for two `SPRT`s and two `DR_TPAGE`s in the current frame arena.
/// The GPU borrows those packets until completion; no capacity checks are made.
static void _crossfadeDrawBackdrop(s32 shade)
{
    SPRT* sprite;

    /// Reserves and initializes one semitransparent, greyscale backdrop sprite.
    ///
    /// `spritePacket` must be a writable SPRT pointer local and `shadeValue` a
    /// side-effect-free value, read three times. Uses the current frame arena;
    /// the caller fills geometry and linkage and retains storage for the GPU.
    /// Captures gGpuPrimCursor, advancing by sizeof(SPRT) bytes without a
    /// capacity check. RGB keeps the shade's low byte; 0x80 is unity modulation.
    /// Invoke inside a braced caller context; expansion is a compound statement.
#define CROSSFADE_ALLOCATE_BACKDROP_SPRITE(spritePacket, shadeValue) \
    {                                                                \
        (spritePacket) = gGpuPrimCursor;                             \
        gGpuPrimCursor = (spritePacket) + 1;                         \
        setSprt((spritePacket));                                     \
        setSemiTrans((spritePacket), 1);                             \
        (spritePacket)->r0 = (shadeValue);                           \
        (spritePacket)->g0 = (shadeValue);                           \
        (spritePacket)->b0 = (shadeValue);                           \
    }

    CROSSFADE_ALLOCATE_BACKDROP_SPRITE(sprite, shade);
    sprite->u0   = 0;
    sprite->v0   = 0;
    sprite->x0   = -CROSSFADE_BACKDROP_WIDTH / 2;
    sprite->y0   = -CROSSFADE_BACKDROP_HEIGHT / 2;
    sprite->clut = 0;
    sprite->w    = CROSSFADE_BACKDROP_LEFT_WIDTH;
    sprite->h    = CROSSFADE_BACKDROP_HEIGHT;
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, sprite);
    _crossfadeSetTpage(MIST_R18_BACKDROP_LEFT_VRAM_X, 0);

    // Each prepended texture-page command executes before its sprite.
    CROSSFADE_ALLOCATE_BACKDROP_SPRITE(sprite, shade);
    sprite->u0   = 0;
    sprite->v0   = 0;
    sprite->x0   = CROSSFADE_BACKDROP_LEFT_WIDTH - CROSSFADE_BACKDROP_WIDTH / 2;
    sprite->y0   = -CROSSFADE_BACKDROP_HEIGHT / 2;
    sprite->clut = 0;
    sprite->w    = CROSSFADE_BACKDROP_WIDTH - CROSSFADE_BACKDROP_LEFT_WIDTH;
    sprite->h    = CROSSFADE_BACKDROP_HEIGHT;
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, sprite);
    _crossfadeSetTpage(MIST_R18_BACKDROP_RIGHT_VRAM_X, MIST_R18_BACKDROP_RIGHT_VRAM_Y);
#undef CROSSFADE_ALLOCATE_BACKDROP_SPRITE
}

/// Dispatches attachment, idle and teardown for either of the room's held models.
///
/// Requires a live TMD task and a state in 0..2. Setup borrows the parent TMD
/// task in `spawnArg2.pointer`; `spawnArg1.value` indexes its part coordinates.
/// The parent coordinate and lighting matrices must outlive the child. Setup
/// joins the parent's teardown tree; state 1 keeps the attachment unchanged,
/// and state 2 kills it. The three callbacks are copied by value before dispatch.
static void _mistR18AttachedModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistR18AttachedModelStates;
    states.funcs[task->state](task);
}

#include "../../shared/actor/model_placement_attach_part.inc.c"

/// Keeps an attached model unchanged while its parent controls its lifetime.
static void _mistR18AttachedModelIdleState(Task* task)
{
}

/// Draws a translucent black rectangle until its callback countdown expires.
///
/// In state 0, borrows the `RECT` in `spawnArg2.pointer` on every callback and
/// covers (w - 1) by (h - 1) draw-environment pixels using average blending.
/// Decrements `spawnArg1.value` after drawing; a nonpositive result or any other
/// state kills the task. The rectangle must remain live until teardown.
/// Task-table entry 6 has no room-local spawner or event-command reference.
static void _mistR18FadeTileTask(Task* task)
{
    enum { MIST_R18_FADE_TILE_DRAW = 0 };
    PrimDrawParams draw;
    const RECT*    rect;

    rect = task->spawnArg2.pointer;

    if (task->state == MIST_R18_FADE_TILE_DRAW) {
        draw.x         = rect->x;
        draw.y         = rect->y;
        draw.w         = rect->w;
        draw.h         = rect->h;
        draw.b         = 0;
        draw.g         = 0;
        draw.r         = 0;
        draw.semiTrans = 1;
        _mistR18DrawTile(&draw);
        _mistR18SetTexturePage(GPU_BLEND_AVERAGE, 0, 0, MIST_R18_CAPTION_PLATE_OT_SLOT);

        if (--task->spawnArg1.value > 0) {
            return;
        }
    }
    taskKill(task);
}

/// Queues a coloured tile in the caption plate OT slot.
///
/// Borrows `draw` only for this call; it supplies screen X/Y, RGB and dimensions
/// whose low halfwords become (w - 1)/(h - 1). Nonzero `semiTrans` enables GPU
/// blending; the caller supplies its draw-mode command. Reserves one `TILE` in
/// the frame arena, retained until the GPU completes; capacity is not checked.
static void _mistR18DrawTile(const PrimDrawParams* draw)
{
    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    if (draw->semiTrans == 0) {
        SetShadeTex(tile, 1);
        SetSemiTrans(tile, 0);
    } else {
        SetShadeTex(tile, 0);
        SetSemiTrans(tile, 1);
    }
    tile->r0 = draw->r;
    tile->g0 = draw->g;
    tile->b0 = draw->b;
    tile->x0 = draw->x;
    tile->y0 = draw->y;
    tile->w  = draw->w - 1;
    tile->h  = draw->h - 1;
    AddPrim(gGpuCurrentOt + MIST_R18_CAPTION_PLATE_OT_SLOT, tile);
}

/// Queues a caption sprite with the palette at VRAM `clutX`/`clutY`.
///
/// Borrows `draw` for the call. X/Y are draw-environment pixels, U/V take their
/// low byte, and the dimensions become (w - 1)/(h - 1). Palette X is a pixel
/// coordinate rounded down to a 16-pixel boundary; Y is a VRAM row.
/// Nonzero `semiTrans` selects modulated blending,
/// otherwise the texture is opaque and unmodulated. The caller queues the
/// texture page separately. Reserves one `SPRT` until the GPU finishes.
static void _mistR18DrawSprite(const PrimDrawParams* draw, u32 clutX, s32 clutY)
{
    SPRT* sprite;
    u8    textureV;

    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    SetSprt(sprite);
    if (draw->semiTrans == 0) {
        SetShadeTex(sprite, 1);
        SetSemiTrans(sprite, 0);
    } else {
        SetShadeTex(sprite, 0);
        SetSemiTrans(sprite, 1);
    }
    sprite->r0   = draw->r;
    sprite->g0   = draw->g;
    sprite->b0   = draw->b;
    sprite->x0   = draw->x;
    sprite->y0   = draw->y;
    sprite->u0   = draw->u;
    textureV     = draw->v;
    sprite->clut = getClut(clutX, clutY);
    sprite->v0   = textureV;
    sprite->w    = draw->w - 1;
    sprite->h    = draw->h - 1;
    AddPrim(gGpuCurrentOt + MIST_R18_CAPTION_SPRITE_OT_SLOT, sprite);
}

/// Queues a 4-bit texture-page command before sprites in the selected OT slot.
///
/// `vramX`/`vramY` are signed 16-bit VRAM pixel coordinates; the SDK packs their
/// page origin. The low two bits of `blendMode` select a `GPU_BLEND_*` mode.
/// `orderingTableSlot` indexes the current OT and must be in bounds. Enables
/// drawing in the display area, disables dithering, and reserves one `DR_TPAGE`
/// until the GPU completes. Queue sprites first: OT insertion prepends packets.
static void _mistR18SetTexturePage(s16 blendMode, s16 vramX, s16 vramY, s32 orderingTableSlot)
{
    DR_TPAGE* pageCommand;

    pageCommand    = gGpuPrimCursor;
    gGpuPrimCursor = pageCommand + 1;
    SetDrawTPage(pageCommand, true, false, GetTPage(MIST_R18_TEXTURE_DEPTH_4BIT, blendMode, vramX, vramY));
    AddPrim(gGpuCurrentOt + orderingTableSlot, pageCommand);
}

/// Spawns either briefing model for attachment to the live player's part 8.
///
/// `modelIndex` is 0 first model or 1 second model; other values do nothing.
/// Spawns only when the selected saved handle is NULL, leaving it NULL on
/// failure. A successful model is drawable and its setup task later borrows
/// the player's part coordinate and lighting, joining the player's teardown
/// tree. The player/model resources must remain live through that attachment.
static void _mistR18SpawnAttachedModel(s32 modelIndex)
{
    Task** modelTaskSlot;
    Task*  modelTask;

    switch (modelIndex) {
        case MIST_R18_ATTACHMENT_MODEL_FIRST:
            modelTaskSlot = &_gMistR18FirstAttachedModelTask;
            break;
        case MIST_R18_ATTACHMENT_MODEL_SECOND:
            modelTaskSlot = &_gMistR18SecondAttachedModelTask;
            break;
        default:
            modelTaskSlot = NULL;
            break;
    }

    if ((modelTaskSlot != NULL) && (*modelTaskSlot == NULL)) {
        modelTask      = taskSpawnFromTable(_gMistR18TaskDescs, modelIndex, MIST_R18_ATTACHMENT_PLAYER_PART, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        *modelTaskSlot = modelTask;
        if (modelTask != NULL) {
            modelTask->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Kills and clears the selected held-model task (0 first model, 1 second model).
///
/// Other indices do nothing. The handle is cleared even when already NULL.
static void _mistR18KillAttachedModel(s32 modelIndex)
{
    enum {
        MIST_R18_ATTACHED_FIRST_MODEL  = 0,
        MIST_R18_ATTACHED_SECOND_MODEL = 1,
    };
    if (modelIndex == MIST_R18_ATTACHED_FIRST_MODEL) {
        if (_gMistR18FirstAttachedModelTask != NULL) {
            taskKill(_gMistR18FirstAttachedModelTask);
        }
        _gMistR18FirstAttachedModelTask = NULL;
    } else if (modelIndex == MIST_R18_ATTACHED_SECOND_MODEL) {
        if (_gMistR18SecondAttachedModelTask != NULL) {
            taskKill(_gMistR18SecondAttachedModelTask);
        }
        _gMistR18SecondAttachedModelTask = NULL;
    }
}

/// Starts the briefing's glyph-script caption over its translucent plate.
///
/// The child borrows and resets the room's mutable text stream. Keep that
/// stream, glyph cells and this overlay live until it finishes or the event
/// ends. The script starts one instance; spawn failure is ignored here.
static void _mistR18SpawnBriefingCaption(void)
{
    enum { MIST_R18_BRIEFING_CAPTION_TASK_INDEX = 5 };

    taskSpawnFromTable(_gMistR18TaskDescs, MIST_R18_BRIEFING_CAPTION_TASK_INDEX, 0, &_gMistR18BriefingCaptionStream);
}

/// Starts the briefing's captured-backdrop transition to the next view.
///
/// Captures the current backdrop, waits for the replacement view and fades to
/// its live frame. The script leaves view 2 after this request. Keep this
/// overlay and frame resources live through the child's teardown; its spawn
/// failure is ignored and no handle is retained here.
static void _mistR18StartBackdropCrossfade(void)
{
    enum { MIST_R18_BACKDROP_CROSSFADE_TASK_INDEX = 3 };

    taskSpawnFromTable(_gMistR18TaskDescs, MIST_R18_BACKDROP_CROSSFADE_TASK_INDEX, 0, NULL);
}

/// Dispatches the room's captured-backdrop crossfade.
///
/// Requires a live task with state 0 capture, 1 wait for the replacement view,
/// 2 fade to the live frame or 3 teardown. Handlers keep RGB modulation in
/// `killCountdown` and borrow frame packets until GPU completion. The callback
/// table is copied by value before dispatch; there is no index check.
static void _mistR18CrossfadeTask(Task* task)
{
    TaskFuncTable4 states;

    states = _gMistR18CrossfadeStates;
    states.funcs[task->state](task);
}

/// Waits for the crossfade's replacement view while advancing its saved shade.
///
/// Adds eight per callback with 16-bit wrapping, clamping the signed result at
/// 64. Once the view is ready or the view index leaves 2, sets shade 128 and
/// advances to the crossfade state. This state queues no drawing packets.
static void _mistR18WaitForCrossfadeViewState(Task* task)
{
    enum {
        MIST_R18_CROSSFADE_WAIT_STEP  = 8,
        MIST_R18_CROSSFADE_WAIT_SHADE = 64,
        MIST_R18_CROSSFADE_WAIT_VIEW  = 2,
    };
    u16 shade;

    shade               = (u16)task->killCountdown + MIST_R18_CROSSFADE_WAIT_STEP;
    task->killCountdown = shade;
    if ((s16)shade >= MIST_R18_CROSSFADE_WAIT_SHADE) {
        task->killCountdown = MIST_R18_CROSSFADE_WAIT_SHADE;
    }
    if ((gGameSession->viewReady != 0) || (gGameSession->location.view != MIST_R18_CROSSFADE_WAIT_VIEW)) {
        task->killCountdown = CROSSFADE_SHADE_UNITY;
        task->state++;
    }
}

#include "../../shared/room/backdrop_crossfade.inc.c"

/// Spawns the briefing's separately placed prop and saves its task handle.
///
/// Requires the room's prop model and task table to remain loaded. Each call
/// overwrites the saved handle, including on allocation failure; event scripts
/// pair successful spawns with teardown before spawning again.
static void _mistR18SpawnPlacedProp(void)
{
    enum { MIST_R18_PLACED_PROP_TASK_INDEX = 4 };

    _gMistR18PlacedPropTask = taskSpawnFromTable(_gMistR18TaskDescs, MIST_R18_PLACED_PROP_TASK_INDEX, 0, 0);
}

/// Kills the room's separately placed prop task and clears its saved handle.
static void _mistR18KillPlacedProp(void)
{
    if (_gMistR18PlacedPropTask != NULL) {
        taskKill(_gMistR18PlacedPropTask);
    }
    _gMistR18PlacedPropTask = NULL;
}

/// Places the room's prop model once at its fixed cutscene transform.
///
/// Requires a live TMD body with root coordinate 0. State 0 sets its translation
/// in parent-coordinate units and Euler angles in 4096 units per turn, rebuilds
/// the rotation, invalidates composition, applies an OT bias of -8 and enables
/// drawing. Later states retain the placement; task teardown owns the model.
static void _mistR18PlacePropTask(Task* task)
{
    enum {
        MIST_R18_PROP_PLACE_INITIALIZE = 0,
        MIST_R18_PROP_ORDERING_BIAS    = -8,
    };
    GfxCoord*  root;
    TmdObject* model;

    if (task->state == MIST_R18_PROP_PLACE_INITIALIZE) {
        root               = task->extra.tmd->coords;
        root->coord.t[0]   = -0x1496;
        root->coord.t[1]   = -0x2DA;
        root->coord.t[2]   = 0xB90;
        root->param.rot.vx = 0x6AA;
        root->param.rot.vy = -0xF8E;
        root->param.rot.vz = -0x333;
        RotMatrixZYX(&root->param.rot, &root->coord);
        root->composeStamp = GRAPHICS_COORD_DIRTY;
        model              = task->extra.tmd;
        model->otOffset    = MIST_R18_PROP_ORDERING_BIAS;
        model->flags      &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->state++;
    }
}

/// Reloads the M.I.S.T. parking area with the starter inventory after the briefing.
///
/// Resets the carried loadout and queues the resident reload task for Acropolis,
/// parking room 3 and arrival 3 with sprite variant 1. Stops non-ambient sound
/// scripts without fading. Used by both briefing completion paths; the saved
/// destination is consumed by the reload task after this callback returns.
static void _mistR18ReloadParkingWithStarterLoadout(void)
{
    enum {
        MIST_R18_PARKING_ENTRY_WARP     = 3,
        MIST_R18_PARKING_ENTRY_ROOM     = 3,
        MIST_R18_PARKING_SPRITE_VARIANT = 1,
    };
    inventoryInitializeStarterLoadout();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_MIST_PARKING;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = MIST_R18_PARKING_ENTRY_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MIST_R18_PARKING_ENTRY_ROOM;
    gDisplayState.spriteVariant                                 = MIST_R18_PARKING_SPRITE_VARIANT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

/// Selects the briefing's scene stream and stages its deferred audio-start request.
///
/// Marks the view dirty first. Requires the current stage's descriptor table to
/// contain scene key (1, 30, 11, 0) and no unfinished earlier scene selection.
/// Selection saves random state; the stream and buffers remain live through
/// playback. The resident CD dispatcher commits the deferred start separately.
static void _mistR18PrepareBriefingScene(void)
{
    enum {
        MIST_R18_BRIEFING_SCENE_GROUP     = 1,
        MIST_R18_BRIEFING_SCENE_STREAM_ID = 30,
        MIST_R18_BRIEFING_SCENE_SUB_ID    = 11,
    };
    gGameSession->viewDirty = true;
    cdCmdSelectScene(MIST_R18_BRIEFING_SCENE_GROUP, MIST_R18_BRIEFING_SCENE_STREAM_ID, MIST_R18_BRIEFING_SCENE_SUB_ID);
    cdCmdStageSceneAudioStart();
}

/// Pauses the briefing script after starting its scene-view monitor.
///
/// The pause latch is set only if the bodyless monitor task is allocated.
/// The monitor releases the latch when the scene view is no longer ready;
/// it stays alive until task-list teardown. Requires loaded room descriptors
/// and a live event interpreter; allocation failure leaves the latch unchanged.
static void _mistR18StartSceneScriptPause(void)
{
    enum { MIST_R18_SCENE_SCRIPT_PAUSE_TASK_INDEX = 7 };

    if (taskSpawnFromTable(_gMistR18TaskDescs, MIST_R18_SCENE_SCRIPT_PAUSE_TASK_INDEX, 0, NULL) != NULL) {
        gEvsPlaybackPaused = true;
    }
}

/// Queues playback of the briefing's previously selected scene/audio session.
///
/// Requires prepared playback buffers and space in the resident CD request ring.
/// An audio-free selection enters playing mode immediately.
static void _mistR18EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Ends briefing scene streaming and restores the random state saved at selection.
///
/// Requires a successful earlier scene selection. Buffer and task teardown remain
/// with their owners; pending resident CD requests are left in place.
static void _mistR18FinishSceneStream(void)
{
    streamFinishScene();
}

/// Requests scene cancellation when the briefing's introductory event is skipped.
///
/// Discards the deferred CD replacement and immediately finishes scene streaming,
/// restoring saved random state. Resident CD dispatch completes cancellation;
/// the skip script handles actor and display cleanup separately.
static void _mistR18CancelScene(void)
{
    cdCmdCancelScene();
}

/// Releases the event-script pause when the scene's view is no longer ready.
///
/// The spawner first sets the shared pause latch. Each callback clears it when
/// `viewReady != 1`; the task neither advances its state nor kills itself.
static void _mistR18ReleaseScriptPauseTask(Task* task)
{
    if (gGameSession->viewReady != 1) {
        gEvsPlaybackPaused = false;
    }
}

/// Sets the camera-depth left shift used before ordering-table quantization.
///
/// The departure script passes `DISPLAY_DEPTH_SHIFT_4X` while view 5 is active and
/// `DISPLAY_DEPTH_SHIFT_1X` to restore normal depth. The signed-byte EVS argument
/// is stored as a byte without validation; supported renderer shifts are 0..3.
static void _mistR18SetOrderingDepthShift(s8 depthShift)
{
    gDisplayState.otDepthShift = depthShift;
}

/// Opens the briefing's key-item menu without reinitializing it on reminder passes.
///
/// The first pass marks the four briefing items collected and clears the map
/// choice. Reminder passes skip that initialization. The map overlay and its UI
/// must stay loaded until the queued display-mode task finishes.
static void _mistR18OpenKeyItemMenu(void)
{
    mapAkropolisOpenKeyItemMenu(0, _gMistR18RetainKeyItemMenuState);
}

/// Registers the room task and starts the introductory briefing event with its skip script.
///
/// Requires fresh prop handles: initialization forgets them without killing tasks.
/// Advances to task state 1 and seeds the separate cursor for the prop-placement
/// script, which starts after the introductory event finishes or is skipped.
static void _mistR18InitBriefingState(Task* task)
{
    _gMistR18FirstAttachedModelTask  = NULL;
    _gMistR18SecondAttachedModelTask = NULL;
    _gMistR18PlacedPropTask          = NULL;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    evsStartScriptWithSkip(_gMistR18IntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gMistR18SkipIntroScript);
    task->state++;
    _gMistR18BriefingStep = MIST_R18_BRIEFING_PLACE_PROP;
}

void mistR18BriefingTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistR18BriefingStates;
    states.funcs[task->state](task);
}
