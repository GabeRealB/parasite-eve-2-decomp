#include "stages/akropolis/rooms/mist_r18.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/companion_load.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/starter_inventory.h"

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
#include "../../../../shared/room/backdrop_crossfade.h"

#include "assets/rooms/mist/r18/declarations.inc"

/// Packet placement and storage layout of this room's captions and captured backdrop.
enum {
    CAPTION_SPRITE_OT_SLOT = 4,
    CAPTION_PLATE_OT_SLOT  = 5,
    TEXTURE_DEPTH_4BIT     = 0,
    TEXTURE_SHADE_UNITY    = 0x80,
    BACKDROP_LEFT_VRAM_X   = 832,
    BACKDROP_RIGHT_VRAM_X  = 640,
    BACKDROP_RIGHT_VRAM_Y  = 256,
};

/// Next briefing script to start after the current event and menu have finished.
enum {
    BRIEFING_PLACE_PROP         = 1,
    BRIEFING_FIRST_HELD_MODEL   = 2,
    BRIEFING_OPEN_KEY_ITEMS     = 3,
    BRIEFING_CHECK_DRYFIELD_MAP = 4,
    BRIEFING_DEPARTING          = 5,
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
static Task* _gMistR18FirstAttachedModelTask;

/// Saved task drawing the second briefing model on the player's part 8.
static Task* _gMistR18SecondAttachedModelTask;

/// Saved task drawing the briefing's separately placed prop.
static Task* _gMistR18PlacedPropTask;

/// Next briefing phase to launch after the current event and menu finish.
static s32 _gMistR18BriefingStep;
/// Keeps the key-item menu's collected-item bits and Dryfield-map choice on reminders.
static s32 _gMistR18RetainKeyItemMenuState;

/// Attachment, idle and teardown callbacks for the room's two held models.
static const TaskFuncTable3 _gMistR18AttachedModelStates = {
    { _modelPlacementAttachPartTask, _mistR18AttachedModelIdleState, taskKill },
};

/// Initialization, script/menu progression and teardown callbacks for the briefing task.
static const TaskFuncTable3 _gMistR18BriefingStates = {
    { _mistR18InitBriefingState, _mistR18AdvanceBriefingState, taskKill },
};

/// Capture, view-wait, crossfade and teardown callbacks for the frozen backdrop.
static const TaskFuncTable4 _gMistR18CrossfadeStates = {
    { _mistR18CaptureBackdropState, _mistR18WaitForCrossfadeViewState, _crossfadeOutState, taskKill },
};

/// Attached-model selectors passed by the briefing scripts and their parent part.
enum {
    ATTACHMENT_MODEL_FIRST  = 0,
    ATTACHMENT_MODEL_SECOND = 1,
    ATTACHMENT_PLAYER_PART  = 8,
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

static AnimationSet _gMistR18PlayerBriefingAnim01Clip;
static AnimationSet _gMistR18PlayerBriefingAnim02Clip;
static AnimationSet _gMistR18PlayerBriefingAnim03Clip;
static AnimationSet _gMistR18PlayerBriefingAnim04Clip;
static AnimationSet _gMistR18PlayerBriefingAnim05Clip;
static AnimationSet _gMistR18PlayerBriefingAnim06Clip;
static AnimationSet _gMistR18PlayerBriefingAnim07Clip;
static AnimationSet _gMistR18PlayerBriefingAnim08Clip;
static AnimationSet _gMistR18PlayerBriefingAnim09Clip;
static AnimationSet _gMistR18PlayerBriefingAnim10Clip;
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

/// Spawn recipes for the briefing's held models, prop and presentation tasks.
static TaskDesc _gMistR18TaskDescs[8] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _mistR18AttachedModelTask, { .model = &_gMistR18BriefingFile } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _mistR18AttachedModelTask, { .model = &_gMistR18Laptop } },
    { { { TASK_BODY_COORD, TASK_PRIORITY_CONTROL } }, _mistR18TextureFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistR18CrossfadeTask, { .value = 0 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _mistR18PlacePropTask, { .model = &_gMistR18Laptop } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistR18CaptionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistR18FadeTileTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistR18ReleaseScriptPauseTask, { .value = 0 } },
};

/// Player-bank extension: NULL at slot 47, followed by briefing clips 48..57.
#include "assets/rooms/mist/r18/player_briefing_bank.inc"
#include "assets/rooms/mist/r18/events.inc"
#include "assets/rooms/mist/r18/header.inc"
#include "assets/rooms/mist/r18/geometry.inc"
#include "assets/rooms/mist/r18/cameras.inc"
#include "assets/rooms/mist/r18/sprites.inc"
#include "assets/rooms/mist/r18/placements.inc"
#include "assets/rooms/mist/r18/lights.inc"
#include "assets/rooms/mist/r18/surfaces.inc"

/// Encoded 8-bit sprite texture page at (960, 0), with its CLUT at (0, 255).
enum {
    SPRITE_TEXTURE_DEPTH_8BIT = 1,
};

/// Reveals a glyph-script caption over a translucent black plate during an event.
static void _mistR18CaptionTask(Task* task)
{
    enum {
        CAPTION_INITIALIZE = 0,
        CAPTION_REVEAL     = 1,
        CAPTION_FINISHED   = -1,
        FONT_PAGE_U_MASK   = 0x3F,
    };
    PrimDrawParams draw;
    TextStream*    stream;
    s32            byteIndex;

    stream = task->spawnArg2.pointer;
    if (gGameSession->eventState == 0) {
        task->state = CAPTION_FINISHED;
    }

    switch (task->state) {
        case CAPTION_INITIALIZE:
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

        case CAPTION_REVEAL:
            // Redraw the revealed prefix, then advance its byte cursor on expiry.
            draw.x         = stream->x;
            draw.y         = stream->y;
            draw.u         = stream->tpageX;
            draw.v         = stream->tpageY;
            draw.r         = TEXTURE_SHADE_UNITY;
            draw.g         = TEXTURE_SHADE_UNITY;
            draw.b         = TEXTURE_SHADE_UNITY;
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
                            draw.u = stream->glyphs[stream->chars[byteIndex]].u + (stream->tpageX & FONT_PAGE_U_MASK);
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

                _mistR18SetTexturePage(GPU_BLEND_ADD, stream->tpageX, stream->tpageY, CAPTION_SPRITE_OT_SLOT);

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
                _mistR18SetTexturePage(GPU_BLEND_AVERAGE, 0, 0, CAPTION_PLATE_OT_SLOT);
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
static inline void _mistR18StartSkippableBriefingStep(EvsCommand* script, EvsCommand* skipScript, s32 nextStep)
{
    evsStartScriptWithSkip(script, EVENT_SCRIPT_HUD_HIDE_RESTORE, skipScript);
    _gMistR18BriefingStep = nextStep;
}

/// Advances the briefing scripts and repeats the key-item menu until the Dryfield map is opened.
static void _mistR18AdvanceBriefingState(Task* unusedTask)
{
    enum { EVENT_IDLE = 0 };
    s32 briefingStep;

    // Event completion and menu teardown must finish before another script starts.
    if ((gGameSession->eventState == EVENT_IDLE) && (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
        briefingStep = _gMistR18BriefingStep;
        if (briefingStep == BRIEFING_PLACE_PROP) {
            _mistR18StartSkippableBriefingStep(_gMistR18PlacePropScript, _gMistR18SkipPlacePropScript, BRIEFING_FIRST_HELD_MODEL);
        } else if (briefingStep == BRIEFING_FIRST_HELD_MODEL) {
            evsStartScriptWithSkip(_gMistR18FirstHeldModelScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gMistR18SkipFirstHeldModelScript);
            _gMistR18RetainKeyItemMenuState = false;
            _gMistR18BriefingStep           = BRIEFING_OPEN_KEY_ITEMS;
        } else if (briefingStep == BRIEFING_OPEN_KEY_ITEMS) {
            evsStartScript(_gMistR18OpenKeyItemsScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            _gMistR18BriefingStep = BRIEFING_CHECK_DRYFIELD_MAP;
        } else if (briefingStep == BRIEFING_CHECK_DRYFIELD_MAP) {
            // Reopen the reminder menu without resetting its map-opened latch.
            if (mapAkropolisWasDryfieldMapSelected() != true) {
                evsStartScript(_gMistR18DryfieldMapReminderScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                _gMistR18RetainKeyItemMenuState = true;
                return;
            }
            _mistR18StartSkippableBriefingStep(_gMistR18DepartureScript, _gMistR18SkipDepartureScript, BRIEFING_DEPARTING);
        }
    }
}

/// Fades two fixed textured rectangles in, holds them opaque, then fades them out.
static void _mistR18TextureFadeTask(Task* task)
{
    enum {
        INITIALIZE        = 0,
        IN                = 1,
        HOLD              = 2,
        OUT               = 3,
        TEXTURE_FADE_STEP = 21,
    };
    s32 fadeActive;

    fadeActive = 1;
    switch (task->state) {
        case INITIALIZE:
            task->killCountdown = 0;
            _mistR18DrawFadeSprites(1, 0);
            task->state++;
            break;
        case IN:
            task->killCountdown += TEXTURE_FADE_STEP;
            if (task->killCountdown >= TEXTURE_SHADE_UNITY + 1) {
                fadeActive = 0;
                task->state++;
            }
            break;
        case HOLD:
            fadeActive = 0;
            task->spawnArg1.value--;
            if ((task->spawnArg1.value <= 0) || (gGameSession->evtSkipped != 0)) {
                task->state++;
            }
            break;
        case OUT:
            task->killCountdown -= TEXTURE_FADE_STEP;
            if (task->killCountdown < TEXTURE_FADE_STEP + 1) {
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
static void _mistR18DrawFadeSprites(s32 fadeActive, s32 shade)
{
    enum {
        SPRITE_RAW_OPAQUE_CODE      = 0x65,
        SPRITE_MODULATED_BLEND_CODE = 0x66,
        FADE_FIRST_CLUT             = getClut(0, 271),
        FADE_SECOND_CLUT            = getClut(16, 271),
        FADE_TEXTURE_PAGE           = getTPage(TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 704, 0),
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
        sprite->code = SPRITE_RAW_OPAQUE_CODE;
    } else {
        sprite->code = SPRITE_MODULATED_BLEND_CODE;
    }
    setXY0(sprite, screenX, screenY);
    sprite->clut = FADE_FIRST_CLUT;
    setWH(sprite, 0xCF, 0x23);
    setRGB0(sprite, shade, shade, shade);
    setUV0(sprite, 0, 0);
    addPrim(gGpuCurrentOt + CAPTION_SPRITE_OT_SLOT, sprite);

    screenX        = -0x22;
    screenY        = 0x36;
    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    setSprt(sprite);
    if (fadeActive == 0) {
        sprite->code = SPRITE_RAW_OPAQUE_CODE;
    } else {
        sprite->code = SPRITE_MODULATED_BLEND_CODE;
    }
    setXY0(sprite, screenX, screenY);
    setRGB0(sprite, shade, shade, shade);
    setUV0(sprite, 0, 0x24);
    sprite->clut = FADE_SECOND_CLUT;
    setWH(sprite, 0xB7, 0x23);
    addPrim(gGpuCurrentOt + CAPTION_SPRITE_OT_SLOT, sprite);

    pageCommand    = gGpuPrimCursor;
    gGpuPrimCursor = pageCommand + 1;
    setDrawTPage(pageCommand, true, false, FADE_TEXTURE_PAGE);
    addPrim(gGpuCurrentOt + CAPTION_SPRITE_OT_SLOT, pageCommand);
}

/// Captures the current 320x240 draw framebuffer as the crossfade's saved backdrop.
static void _mistR18CaptureBackdropState(Task* task)
{
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
        frameY = DISPLAY_BUFFER_STRIDE_ROWS;
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
    sourceRect.h   = DISPLAY_SCREEN_HEIGHT;
    SetDrawMove(copyCommand, &sourceRect, BACKDROP_LEFT_VRAM_X, 0);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, copyCommand);

    copyCommand    = gGpuPrimCursor;
    gGpuPrimCursor = copyCommand + 1;
    sourceRect.x   = frameX + CROSSFADE_BACKDROP_LEFT_WIDTH;
    sourceRect.y   = frameY;
    sourceRect.w   = DISPLAY_SCREEN_WIDTH - CROSSFADE_BACKDROP_LEFT_WIDTH;
    sourceRect.h   = DISPLAY_SCREEN_HEIGHT;
    SetDrawMove(copyCommand, &sourceRect, BACKDROP_RIGHT_VRAM_X, BACKDROP_RIGHT_VRAM_Y);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, copyCommand);

    // Head insertion executes mask-on, right copy, left copy, then mask-off.
    maskCommand    = gGpuPrimCursor;
    gGpuPrimCursor = maskCommand + 1;
    SetDrawStp(maskCommand, true);
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, maskCommand);

    task->killCountdown = 0;
    task->state++;
}

#include "../../../../shared/room/backdrop_crossfade_live.inc.c"

/// Queues an additive redraw of the captured backdrop for the room crossfade.
static void _crossfadeDrawBackdrop(s32 shade)
{
    SPRT* sprite;

    /// Reserves and initializes one semitransparent, greyscale backdrop sprite.
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
    sprite->x0   = -DISPLAY_SCREEN_WIDTH / 2;
    sprite->y0   = -DISPLAY_SCREEN_HEIGHT / 2;
    sprite->clut = 0;
    sprite->w    = CROSSFADE_BACKDROP_LEFT_WIDTH;
    sprite->h    = DISPLAY_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, sprite);
    _crossfadeSetTpage(BACKDROP_LEFT_VRAM_X, 0);

    // Each prepended texture-page command executes before its sprite.
    CROSSFADE_ALLOCATE_BACKDROP_SPRITE(sprite, shade);
    sprite->u0   = 0;
    sprite->v0   = 0;
    sprite->x0   = CROSSFADE_BACKDROP_LEFT_WIDTH - DISPLAY_SCREEN_WIDTH / 2;
    sprite->y0   = -DISPLAY_SCREEN_HEIGHT / 2;
    sprite->clut = 0;
    sprite->w    = DISPLAY_SCREEN_WIDTH - CROSSFADE_BACKDROP_LEFT_WIDTH;
    sprite->h    = DISPLAY_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt + CROSSFADE_ORDERING_TABLE_SLOT, sprite);
    _crossfadeSetTpage(BACKDROP_RIGHT_VRAM_X, BACKDROP_RIGHT_VRAM_Y);
#undef CROSSFADE_ALLOCATE_BACKDROP_SPRITE
}

/// Dispatches attachment, idle and teardown for either of the room's held models.
static void _mistR18AttachedModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistR18AttachedModelStates;
    states.funcs[task->state](task);
}

#include "../../../../shared/actor/model_placement_attach_part.inc.c"

/// Keeps an attached model unchanged while its parent controls its lifetime.
static void _mistR18AttachedModelIdleState(Task* task)
{
}

/// Draws a translucent black rectangle until its callback countdown expires.
static void _mistR18FadeTileTask(Task* task)
{
    enum { FADE_TILE_DRAW = 0 };
    PrimDrawParams draw;
    const RECT*    rect;

    rect = task->spawnArg2.pointer;

    if (task->state == FADE_TILE_DRAW) {
        draw.x         = rect->x;
        draw.y         = rect->y;
        draw.w         = rect->w;
        draw.h         = rect->h;
        draw.b         = 0;
        draw.g         = 0;
        draw.r         = 0;
        draw.semiTrans = 1;
        _mistR18DrawTile(&draw);
        _mistR18SetTexturePage(GPU_BLEND_AVERAGE, 0, 0, CAPTION_PLATE_OT_SLOT);

        if (--task->spawnArg1.value > 0) {
            return;
        }
    }
    taskKill(task);
}

/// Queues a colored tile in the caption plate OT slot.
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
    AddPrim(gGpuCurrentOt + CAPTION_PLATE_OT_SLOT, tile);
}

/// Queues a caption sprite with the palette at VRAM `clutX`/`clutY`.
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
    AddPrim(gGpuCurrentOt + CAPTION_SPRITE_OT_SLOT, sprite);
}

/// Queues a 4-bit texture-page command before sprites in the selected OT slot.
static void _mistR18SetTexturePage(s16 blendMode, s16 vramX, s16 vramY, s32 orderingTableSlot)
{
    DR_TPAGE* pageCommand;

    pageCommand    = gGpuPrimCursor;
    gGpuPrimCursor = pageCommand + 1;
    SetDrawTPage(pageCommand, true, false, GetTPage(TEXTURE_DEPTH_4BIT, blendMode, vramX, vramY));
    AddPrim(gGpuCurrentOt + orderingTableSlot, pageCommand);
}

/// Spawns either briefing model for attachment to the live player's part 8.
static void _mistR18SpawnAttachedModel(s32 modelIndex)
{
    Task** modelTaskSlot;
    Task*  modelTask;

    switch (modelIndex) {
        case ATTACHMENT_MODEL_FIRST:
            modelTaskSlot = &_gMistR18FirstAttachedModelTask;
            break;
        case ATTACHMENT_MODEL_SECOND:
            modelTaskSlot = &_gMistR18SecondAttachedModelTask;
            break;
        default:
            modelTaskSlot = NULL;
            break;
    }

    if ((modelTaskSlot != NULL) && (*modelTaskSlot == NULL)) {
        modelTask      = taskSpawnFromTable(_gMistR18TaskDescs, modelIndex, ATTACHMENT_PLAYER_PART, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        *modelTaskSlot = modelTask;
        if (modelTask != NULL) {
            modelTask->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Kills and clears the selected held-model task (0 first model, 1 second model).
static void _mistR18KillAttachedModel(s32 modelIndex)
{
    enum {
        FIRST_MODEL  = 0,
        SECOND_MODEL = 1,
    };
    if (modelIndex == FIRST_MODEL) {
        if (_gMistR18FirstAttachedModelTask != NULL) {
            taskKill(_gMistR18FirstAttachedModelTask);
        }
        _gMistR18FirstAttachedModelTask = NULL;
    } else if (modelIndex == SECOND_MODEL) {
        if (_gMistR18SecondAttachedModelTask != NULL) {
            taskKill(_gMistR18SecondAttachedModelTask);
        }
        _gMistR18SecondAttachedModelTask = NULL;
    }
}

/// Starts the briefing's glyph-script caption over its translucent plate.
static void _mistR18SpawnBriefingCaption(void)
{
    enum { BRIEFING_CAPTION_TASK_INDEX = 5 };

    taskSpawnFromTable(_gMistR18TaskDescs, BRIEFING_CAPTION_TASK_INDEX, 0, &_gMistR18BriefingCaptionStream);
}

/// Starts the briefing's captured-backdrop transition to the next view.
static void _mistR18StartBackdropCrossfade(void)
{
    enum { BACKDROP_CROSSFADE_TASK_INDEX = 3 };

    taskSpawnFromTable(_gMistR18TaskDescs, BACKDROP_CROSSFADE_TASK_INDEX, 0, NULL);
}

/// Dispatches the room's captured-backdrop crossfade.
static void _mistR18CrossfadeTask(Task* task)
{
    TaskFuncTable4 states;

    states = _gMistR18CrossfadeStates;
    states.funcs[task->state](task);
}

/// Waits for the crossfade's replacement view while advancing its saved shade.
static void _mistR18WaitForCrossfadeViewState(Task* task)
{
    enum {
        CROSSFADE_WAIT_STEP = 8,
        SHADE               = 64,
        VIEW                = 2,
    };
    u16 shade;

    shade               = (u16)task->killCountdown + CROSSFADE_WAIT_STEP;
    task->killCountdown = shade;
    if ((s16)shade >= SHADE) {
        task->killCountdown = SHADE;
    }
    if ((gGameSession->viewReady != 0) || (gGameSession->location.view != VIEW)) {
        task->killCountdown = CROSSFADE_SHADE_UNITY;
        task->state++;
    }
}

#include "../../../../shared/room/backdrop_crossfade.inc.c"

/// Spawns the briefing's separately placed prop and saves its task handle.
static void _mistR18SpawnPlacedProp(void)
{
    enum { PLACED_PROP_TASK_INDEX = 4 };

    _gMistR18PlacedPropTask = taskSpawnFromTable(_gMistR18TaskDescs, PLACED_PROP_TASK_INDEX, 0, 0);
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
static void _mistR18PlacePropTask(Task* task)
{
    enum {
        PLACE_INITIALIZE = 0,
        ORDERING_BIAS    = -8,
    };
    GfxCoord*  root;
    TmdObject* model;

    if (task->state == PLACE_INITIALIZE) {
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
        model->otOffset    = ORDERING_BIAS;
        model->flags      &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->state++;
    }
}

/// Reloads the M.I.S.T. parking area with the starter inventory after the briefing.
static void _mistR18ReloadParkingWithStarterLoadout(void)
{
    enum {
        PARKING_SPRITE_VARIANT = 1,
    };
    inventoryInitializeStarterLoadout();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_MIST_PARKING;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = MIST_R18_PARKING_ENTRY_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MIST_R18_PARKING_ENTRY_ROOM;
    gDisplayState.spriteVariant                                 = PARKING_SPRITE_VARIANT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

/// Selects the briefing's scene stream and stages its deferred audio-start request.
static void _mistR18PrepareBriefingScene(void)
{
    enum {
        GROUP     = 1,
        STREAM_ID = 30,
        SUB_ID    = 11,
    };
    gGameSession->viewDirty = true;
    cdCmdSelectScene(GROUP, STREAM_ID, SUB_ID);
    cdCmdStageSceneAudioStart();
}

/// Pauses the briefing script after starting its scene-view monitor.
static void _mistR18StartSceneScriptPause(void)
{
    enum { SCENE_SCRIPT_PAUSE_TASK_INDEX = 7 };

    if (taskSpawnFromTable(_gMistR18TaskDescs, SCENE_SCRIPT_PAUSE_TASK_INDEX, 0, NULL) != NULL) {
        gEvsPlaybackPaused = true;
    }
}

/// Queues playback of the briefing's previously selected scene/audio session.
static void _mistR18EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Ends briefing scene streaming and restores the random state saved at selection.
static void _mistR18FinishSceneStream(void)
{
    streamFinishScene();
}

/// Requests scene cancellation when the briefing's introductory event is skipped.
static void _mistR18CancelScene(void)
{
    cdCmdCancelScene();
}

/// Releases the event-script pause when the scene's view is no longer ready.
static void _mistR18ReleaseScriptPauseTask(Task* task)
{
    if (gGameSession->viewReady != 1) {
        gEvsPlaybackPaused = false;
    }
}

/// Sets the camera-depth left shift used before ordering-table quantization.
static void _mistR18SetOrderingDepthShift(s8 depthShift)
{
    gDisplayState.otDepthShift = depthShift;
}

/// Opens the briefing's key-item menu without reinitializing it on reminder passes.
static void _mistR18OpenKeyItemMenu(void)
{
    mapAkropolisOpenKeyItemMenu(0, _gMistR18RetainKeyItemMenuState);
}

/// Registers the room task and starts the introductory briefing event with its skip script.
static void _mistR18InitBriefingState(Task* task)
{
    _gMistR18FirstAttachedModelTask  = NULL;
    _gMistR18SecondAttachedModelTask = NULL;
    _gMistR18PlacedPropTask          = NULL;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    evsStartScriptWithSkip(_gMistR18IntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gMistR18SkipIntroScript);
    task->state++;
    _gMistR18BriefingStep = BRIEFING_PLACE_PROP;
}

void mistR18BriefingTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistR18BriefingStates;
    states.funcs[task->state](task);
}
