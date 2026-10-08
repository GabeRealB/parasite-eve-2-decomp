#include "title/title.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "common.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

/// Entries of the title menu, in the order of its label and task tables.
///
/// The cursor only ever rests on New Game, Load Game and Configuration: the
/// two entries before them are stepped over, and Debug Option lies beyond the
/// count the cursor wraps at.
enum {
    TITLE_MENU_SURVIVAL,
    TITLE_MENU_EXTRA_GAME,
    TITLE_MENU_NEW_GAME,
    TITLE_MENU_LOAD_GAME,
    TITLE_MENU_CONFIGURATION,
    TITLE_MENU_DEBUG_OPTION,
};

/// Packed title-screen request: bit 31 suppresses fading on the setup tick;
/// the remaining bits count ticks to wait before allocating screen work.
#define TITLE_SCREEN_SKIP_FADE  0x80000000
#define TITLE_SCREEN_DELAY_MASK 0x7FFFFFFF

/// Prompt and menu slots in the title-screen state dispatcher.
enum {
    TITLE_SCREEN_STATE_PROMPT = 2,
    TITLE_SCREEN_STATE_MENU   = 3,
};

/// Resident task-bank slots used by the title startup and idle transition.
enum {
    TITLE_RESIDENT_TASK_BANK      = 0,
    TITLE_SCREEN_TASK_SLOT        = 2,
    TITLE_START_SESSION_TASK_SLOT = 3,
    TITLE_START_SESSION_DEMO_ARG  = 2,
};

/// Frames a black fade of the title screen lasts. The idle count starts this
/// far below zero to fade in, and runs this far past the timeout to fade out.
#define TITLE_SCREEN_FADE_FRAMES 16

/// Idle frames after which the title screen fades out and gives way to the
/// attract demo.
#define TITLE_SCREEN_IDLE_TIMEOUT 900

/// Brightness of a title-screen row that has fully faded in, and the step a
/// fading row rises by each frame.
#define TITLE_SCREEN_FADE_FULL 0x80
#define TITLE_SCREEN_FADE_STEP 8

/// Page-local V origins, in texels, of the title chrome's 256-by-16 rows.
enum {
    TITLE_CHROME_ATLAS_V_PROMPT    = 0x00,
    TITLE_CHROME_ATLAS_V_COPYRIGHT = 0x10,
    TITLE_CHROME_ATLAS_V_CURSOR    = 0x20,
    TITLE_CHROME_ATLAS_V_NEW_GAME  = 0x30,
    TITLE_CHROME_ATLAS_V_CONTINUE  = 0x40,
    TITLE_CHROME_ATLAS_V_OPTION    = 0x50,
    TITLE_CHROME_ROW_WIDTH         = 256,
    TITLE_CHROME_ROW_HEIGHT        = 16,
};

/// Chrome placement in draw-environment pixels and the visible menu-row count.
enum {
    TITLE_CHROME_MENU_FIRST_Y   = 0x38,
    TITLE_CHROME_MENU_ROW_STEP  = 14,
    TITLE_CHROME_MENU_ROW_COUNT = 3,
    TITLE_CHROME_PROMPT_Y       = 0x40,
    TITLE_CHROME_COPYRIGHT_Y    = 0x5C,
};

/// The title screen task's work: the PRESS START BUTTON prompt and the menu
/// that replaces it, with the idle count that fades the screen in and out.
typedef struct {
    s32 idleFrames;        // Frames since the last input, starting at -TITLE_SCREEN_FADE_FRAMES
    s32 selection;         // Highlighted entry (a TITLE_MENU_ value)
    s32 screenFadeEnabled; // Nonzero to fade the screen in from black and back out to it
    s32 promptFade;        // Brightness of the prompt as it slides in (0..TITLE_SCREEN_FADE_FULL)
    s32 menuFade;          // Brightness of the menu; the prompt and copyright fade out by its complement
    s32 menuCount;         // Entries the cursor wraps over (5, leaving Debug Option out)
} _TitleScreenWork;
STATIC_ASSERT_SIZEOF(_TitleScreenWork, 0x18);

/// Retained text labels for the title menu (src/title/title.c).
extern char Title_StrNewGame[];

extern char Title_StrLoadGame[];

extern char Title_StrConfiguration[];

extern char Title_StrDebugOption[];

extern char Title_StrExtraGame[];

extern char Title_StrSurvival[];

/// Task spawn ids for menu selection indices.
extern s32 Title_MenuSpawnIds[];

/// Last rand() from titleScreenTask.
extern s32 Title_LastRand;

/// When set, _titleStartupTask spawns phase task with arg 0x80000000 (skip fade TILE).
extern u16 Title_SkipFadeFlag;

static void _titleIntroMovieTask(Task* task);

static void _titleStartupTask(Task* startupTask);

void func_807246B4(void);

static void _titleShowBackgroundTask(Task* task);
static void _titleInitializeScreenTask(Task* task);
static void _titleUpdateScreenTask(Task* task);

char Title_StrNewGame[]       = "New Game";
char Title_StrLoadGame[]      = "Load Game";
char Title_StrConfiguration[] = "Configuration";
char Title_StrDebugOption[]   = "Debug Option";
char Title_StrExtraGame[]     = "Extra Game";
char Title_StrSurvival[]      = "Survival";

/// Retained menu labels in selection order. The retail menu draws its labels
/// from the texture atlas instead of reading this table.
static char* Title_MenuLabels[] = {
    Title_StrSurvival,
    Title_StrExtraGame,
    Title_StrNewGame,
    Title_StrLoadGame,
    Title_StrConfiguration,
    Title_StrDebugOption,
};

s32 Title_MenuSpawnIds[] = { 6, 6, 3, 4, 5, 6 };

TaskDesc Title_TaskDescs[] = {
    { { { TASK_BODY_NONE, 0xC0 } }, _titleStartupTask },
    { { { TASK_BODY_NONE, 0xC0 } }, _titleIntroMovieTask },
};

/// Overlay state is stored in the loaded image; the loader does not clear BSS.
s32 Title_LastRand     = 0;
u16 Title_SkipFadeFlag = 0;

/// The title task's states, which `titleScreenTask` copies and indexes by
/// `Task::state`: set-up, the flag advance, the menu in two states and the kill.
static const TaskFuncTable5 Title_PhaseTable = {
    .funcs = {
        _titleInitializeScreenTask,
        _titleShowBackgroundTask,
        _titleUpdateScreenTask,
        _titleUpdateScreenTask,
        taskKill,
    },
};

/// Debug line printed when the title screen hands over to the attract demo.
static const char Title_DemoStartMsg[] = "##########DEMO START\n";

/// Debug line printed before and after a demo restores its save data, with
/// the stage and scene it names. The two bytes after the terminator are never
/// read.
static const char Title_DemoCardRestoreMsg[44] = "####DEMO_CARD_RESTORE STAGE %d, SCENE %d\n\0\x22\xE1";

static void _titleDrawChromeRow(s32 screenY, s32 atlasV, s32 brightness);

/// Initializes the title prompt after its packed spawn delay expires.
///
/// Requires state 0, a loaded title background and initialized primary heap.
/// The high no-fade bit is consumed before counting down the low 31 bits; a
/// delayed request therefore enables fades again on the next tick. Allocation
/// failure leaves state 0 for retry. Success gives the task ownership of its
/// cleared screen work and runs the first prompt update immediately.
static void _titleInitializeScreenTask(Task* task)
{
    enum {
        TITLE_SCREEN_MENU_RESOURCE_HUNDREDS = 1,
        TITLE_SCREEN_MENU_RESOURCE_INDEX    = 0,
    };
    bool              screenFadeEnabled;
    DisplayState*     display;
    _TitleScreenWork* work;

    screenFadeEnabled                  = true;
    display                            = &gDisplayState;
    display->control.flags.imageSource = DISPLAY_IMAGE_NONE;
    Wip_UiHolder                       = NULL;
    if (task->spawnArg1.value < 0) {
        screenFadeEnabled      = false;
        task->spawnArg1.value &= TITLE_SCREEN_DELAY_MASK;
    }
    if (task->spawnArg1.value > 0) {
        task->spawnArg1.value -= 1;
        return;
    }
    work = memCalloc(sizeof(*work), false);
    if (work != NULL) {
        task->work              = work;
        work->screenFadeEnabled = screenFadeEnabled;
        work->menuCount         = TITLE_MENU_DEBUG_OPTION;
        work->selection         = TITLE_MENU_NEW_GAME;
        work->idleFrames        = 0;
        if (Wip_SysFlags.gameOver != 0) {
            work->selection = TITLE_MENU_LOAD_GAME;
        }
        textUploadPalettes();
        displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_KEEP_VIEW);
        display->holdState                 = DISPLAY_HOLD_INITIAL;
        work->idleFrames                   = -TITLE_SCREEN_FADE_FRAMES;
        display->control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
        if (display->debugMode != 0) {
            func_807246B4();
        }
        cdCmdEnqueueDisplayResource(TITLE_SCREEN_MENU_RESOURCE_HUNDREDS, TITLE_SCREEN_MENU_RESOURCE_INDEX, CD_COMMAND_DISPLAY_LOAD_MENU);
        task->state += TITLE_SCREEN_STATE_PROMPT;
        _titleUpdateScreenTask(task);
    }
}

/// Prepends the GPU draw mode used by the title chrome's sprites.
///
/// Selects an 8-bit texture page at VRAM word X=768, row Y=256, additive
/// blending and dithering, with drawing into the displayed area disabled.
/// Requires a current ordering tag and word-aligned space for one DR_TPAGE;
/// its storage must remain live until the GPU consumes the ordering table.
/// The prepend makes this mode execute before primitives already at the tag.
static inline void _titlePrependChromeDrawMode(void)
{
    enum { TITLE_CHROME_TEXTURE_8_BIT = 1 };
    DR_TPAGE* drawMode;

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(TITLE_CHROME_TEXTURE_8_BIT, GPU_BLEND_ADD, 768, 256));
    addPrim(gGpuCurrentOt, drawMode);
}

/// Queues one centered title-chrome atlas row with additive brightness.
///
/// `screenY` is the top edge in draw-environment pixels; `atlasV` is a
/// TITLE_CHROME_ATLAS_V_ origin in page-local texels. `brightness` is normally
/// 0..TITLE_SCREEN_FADE_FULL (0 invisible, 0x80 unscaled); packing keeps its
/// low byte. Coordinates retain the sprite's signed-halfword/unsigned-byte
/// truncation. The chrome texture and palette must already be loaded.
/// Requires a current ordering tag and word-aligned space for one SPRT plus
/// one DR_TPAGE; their storage must remain live until GPU consumption.
static void _titleDrawChromeRow(s32 screenY, s32 atlasV, s32 brightness)
{
    /// Modulated free-size sprite command with semi-transparency enabled.
    enum { TITLE_CHROME_SPRITE_COMMAND = 0x66 };
    SPRT* sprite;
    u8    brightnessByte;

    brightnessByte                      = brightness;
    sprite                              = gGpuPrimCursor;
    gGpuPrimCursor                      = sprite + 1;
    sprite->x0                          = -TITLE_CHROME_ROW_WIDTH / 2;
    sprite->w                           = TITLE_CHROME_ROW_WIDTH;
    sprite->h                           = TITLE_CHROME_ROW_HEIGHT;
    sprite->clut                        = getClut(0, 255);
    GPU_PRIMITIVE_COLOR_WORD(sprite, 0) = (brightnessByte << 16) | (brightnessByte << 8) | brightnessByte;
    setlen(sprite, (sizeof(*sprite) - sizeof(sprite->tag)) / sizeof(u_long));
    sprite->u0 = 0;
    sprite->v0 = atlasV;
    setcode(sprite, TITLE_CHROME_SPRITE_COMMAND);
    sprite->y0 = screenY;
    addPrim(gGpuCurrentOt, sprite);

    // Prepending after the sprite makes its texture mode execute first.
    _titlePrependChromeDrawMode();
}

/// Prepends the GPU draw mode for the title screen's subtractive black fade.
///
/// Queue the semitransparent, untextured fade tile at `gGpuCurrentOt` first:
/// prepending this command makes the GPU subtract the tile's RGB from the
/// framebuffer when drawing it. The mode persists until another draw-mode
/// command replaces it. The dithering bit is enabled and drawing into the
/// displayed area is disabled; the selected 4-bit texture page at VRAM (0, 0)
/// is unused by the tile.
///
/// Requires a writable current ordering tag and `sizeof(DR_TPAGE)` writable,
/// word-aligned bytes at `gGpuPrimCursor`; advances the cursor by that extent.
/// The packet and ordering-table storage are borrowed until GPU drawing ends.
static inline void _titlePrependScreenFadeDrawMode(void)
{
    enum {
        TITLE_SCREEN_FADE_TEXTURE_DEPTH_4BIT = 0,
        TITLE_SCREEN_FADE_DRAW_MODE_COMMAND =
            _get_mode(false, true, getTPage(TITLE_SCREEN_FADE_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0)),
    };
    DR_TPAGE* drawMode;

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, ARRAY_SIZE(drawMode->code));
    drawMode->code[0] = TITLE_SCREEN_FADE_DRAW_MODE_COMMAND;
    addPrim(gGpuCurrentOt, drawMode);
}

/// Updates the title prompt or menu and hands an idle screen to the attract demo.
///
/// Requires state 2 (prompt) or 3 (menu), live initialized screen work, loaded
/// chrome textures/palettes and a current GPU ordering tag with room for this
/// tick's packets. The cursor ranges over New Game, Load Game and Configuration.
/// Counters measure task ticks; fades pack brightness into unsigned bytes.
/// Confirming a menu entry or starting a disc-1 demo invokes the task's exit
/// handler, which may release it. Disc 2 requests a game restart after timeout.
static void _titleUpdateScreenTask(Task* task)
{
    enum {
        TITLE_SCREEN_WIDTH_PIXELS          = 320,
        TITLE_SCREEN_HEIGHT_PIXELS         = 240,
        TITLE_SCREEN_FADE_BRIGHTNESS_STEP  = 16,
        TITLE_SCREEN_FADE_BRIGHTNESS_SHIFT = 4,
        TITLE_SCREEN_BRIGHTNESS_PER_PIXEL  = 8,
        TITLE_SCREEN_TILE_SEMITRANSPARENT  = 0x62,
        TITLE_ATTRACT_DEMO_COUNT           = 3,
        TITLE_MENU_BEFORE_FIRST            = -1,
    };
    _TitleScreenWork* work = task->work;
    s32               idleFrames;
    s32               menuRow;

    // Let the idle fade finish before replacing the title presentation.
    idleFrames       = work->idleFrames + 1;
    work->idleFrames = idleFrames;
    if (idleFrames > TITLE_SCREEN_IDLE_TIMEOUT) {
        if (idleFrames < TITLE_SCREEN_IDLE_TIMEOUT + TITLE_SCREEN_FADE_FRAMES) {
            if (work->screenFadeEnabled != 0) {
                TILE* tile;

                tile           = gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                setTile(tile);
                tile->r0 = tile->g0 = tile->b0 = (idleFrames - TITLE_SCREEN_IDLE_TIMEOUT) * TITLE_SCREEN_FADE_BRIGHTNESS_STEP - 1;
                tile->x0                       = -TITLE_SCREEN_WIDTH_PIXELS / 2;
                tile->y0                       = -TITLE_SCREEN_HEIGHT_PIXELS / 2;
                tile->w                        = TITLE_SCREEN_WIDTH_PIXELS;
                tile->h                        = TITLE_SCREEN_HEIGHT_PIXELS;
                setSemiTrans(tile, 1);
                addPrim(gGpuCurrentOt, tile);

                _titlePrependScreenFadeDrawMode();
            }
        } else {
            Wip_SysFlags.skipTitleIntro = 0;
            if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_1) {
                taskCallExit(task);
                gDisplayState.demoScene = gameMainGetInitializationCount() + 2;
                gDisplayState.demoScene = gDisplayState.demoScene % TITLE_ATTRACT_DEMO_COUNT + 1;
                printf(Title_DemoStartMsg);
                taskSpawn(TITLE_RESIDENT_TASK_BANK, TITLE_START_SESSION_TASK_SLOT, TITLE_START_SESSION_DEMO_ARG, 0);
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            } else {
                gDisplayState.gameMode = DISPLAY_GAME_RESTART;
            }
        }
        return;
    }

    // The negative idle count ramps the newly initialized screen in from black.
    if (work->screenFadeEnabled != 0 && idleFrames < 0) {
        TILE* tile;
        s32   fadeBrightness;

        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        setlen(tile, (sizeof(*tile) - sizeof(tile->tag)) / sizeof(u_long));
        setcode(tile, TITLE_SCREEN_TILE_SEMITRANSPARENT);
        fadeBrightness = ~(work->idleFrames << TITLE_SCREEN_FADE_BRIGHTNESS_SHIFT);
        tile->x0       = -TITLE_SCREEN_WIDTH_PIXELS / 2;
        tile->y0       = -TITLE_SCREEN_HEIGHT_PIXELS / 2;
        tile->w        = TITLE_SCREEN_WIDTH_PIXELS;
        tile->h        = TITLE_SCREEN_HEIGHT_PIXELS;
        tile->b0       = fadeBrightness;
        tile->g0       = fadeBrightness;
        tile->r0       = fadeBrightness;
        addPrim(gGpuCurrentOt, tile);

        _titlePrependScreenFadeDrawMode();
    }

    // Fade the menu in over the prompt, then accept navigation and selection.
    if (task->state == TITLE_SCREEN_STATE_MENU) {
        if (work->menuFade < TITLE_SCREEN_FADE_FULL) {
            work->menuFade += TITLE_SCREEN_FADE_STEP;
        }
        for (menuRow = 0; menuRow < TITLE_CHROME_MENU_ROW_COUNT; menuRow++) {
            _titleDrawChromeRow(menuRow * TITLE_CHROME_MENU_ROW_STEP + TITLE_CHROME_MENU_FIRST_Y,
                                menuRow * TITLE_CHROME_ROW_HEIGHT + TITLE_CHROME_ATLAS_V_NEW_GAME, work->menuFade);
        }
        _titleDrawChromeRow((work->selection - TITLE_MENU_NEW_GAME) * TITLE_CHROME_MENU_ROW_STEP + TITLE_CHROME_MENU_FIRST_Y,
                            TITLE_CHROME_ATLAS_V_CURSOR, work->menuFade);
        _titleDrawChromeRow(work->menuFade / TITLE_SCREEN_BRIGHTNESS_PER_PIXEL + TITLE_CHROME_PROMPT_Y, TITLE_CHROME_ATLAS_V_PROMPT,
                            TITLE_SCREEN_FADE_FULL - work->menuFade);
        _titleDrawChromeRow(TITLE_CHROME_COPYRIGHT_Y, TITLE_CHROME_ATLAS_V_COPYRIGHT, TITLE_SCREEN_FADE_FULL - work->menuFade);
        if (work->menuFade < TITLE_SCREEN_FADE_FULL) {
            return;
        }

        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
            work->idleFrames = 0;
            work->selection++;
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            if (work->selection >= work->menuCount) {
                work->selection -= work->menuCount;
            }
            if (work->selection == TITLE_MENU_SURVIVAL) {
                work->selection = TITLE_MENU_EXTRA_GAME;
            }
            if (work->selection == TITLE_MENU_EXTRA_GAME) {
                work->selection = TITLE_MENU_NEW_GAME;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
            work->idleFrames = 0;
            work->selection--;
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            if (work->selection == TITLE_MENU_EXTRA_GAME) {
                work->selection = TITLE_MENU_SURVIVAL;
            }
            if (work->selection == TITLE_MENU_SURVIVAL) {
                work->selection = TITLE_MENU_BEFORE_FIRST;
            }
            if (work->selection < 0) {
                work->selection += work->menuCount;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | PAD_BUTTON_START) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            taskSpawn(TITLE_RESIDENT_TASK_BANK, Title_MenuSpawnIds[work->selection], 0, 0);
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            taskCallExit(task);
        }
    } else {
        // Slide the prompt into place; confirmation opens the menu immediately.
        if (work->promptFade < TITLE_SCREEN_FADE_FULL) {
            work->promptFade += TITLE_SCREEN_FADE_STEP;
        }
        _titleDrawChromeRow(TITLE_CHROME_PROMPT_Y - (TITLE_SCREEN_FADE_FULL - work->promptFade) / TITLE_SCREEN_BRIGHTNESS_PER_PIXEL,
                            TITLE_CHROME_ATLAS_V_PROMPT, work->promptFade);
        _titleDrawChromeRow(TITLE_CHROME_COPYRIGHT_Y, TITLE_CHROME_ATLAS_V_COPYRIGHT, TITLE_SCREEN_FADE_FULL);
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | PAD_BUTTON_START) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            work->idleFrames = 0;
            task->state++;
        }
    }
}

void titleRestoreAttractDemoState(void)
{
    const u8* stateBytes;
    s32       savedDemoScene;
    s32       savedVibration;
    s32       bank;
    s32       bankTimesEight;
    u8*       shelterBankBytes;

    stateBytes     = Fs_ActorLoadBase2;
    bank           = GAME_FLAG_NIBBLE_BANK_LIVE;
    savedDemoScene = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene;
    savedVibration = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration;
    if (gDisplayState.demoScene == DISPLAY_DEMO_FIXED_REPLAY) {
        stateBytes = FILE_SYSTEM_FIXED_REPLAY_BASE;
    }
    printf(Title_DemoCardRestoreMsg, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area);

    // Consume one live record of each kind in the demo file's serialized order.
    memcpy(&gMcSaveData[MEMORY_CARD_SAVE_LIVE], stateBytes, sizeof(gMcSaveData[MEMORY_CARD_SAVE_LIVE]));
    stateBytes += sizeof(gMcSaveData[MEMORY_CARD_SAVE_LIVE]);

    // Restore the live player image; the serialized backup stays intact.
    memcpy(((u8(*)[PLAYER_STATUS_SAVE_RECORD_BYTES]) & gPlayerStatus)[bank], stateBytes, PLAYER_STATUS_SAVE_RECORD_BYTES);
    stateBytes += PLAYER_STATUS_SAVE_RECORD_BYTES;

    memcpy(&GameFlag_AcropolisBanks[bank], stateBytes, sizeof(GameFlag_AcropolisBanks[bank]));
    stateBytes += sizeof(GameFlag_AcropolisBanks[bank]);

    memcpy(GameFlag_DryfieldBanks, stateBytes, sizeof(GameFlag_DryfieldBanks[0]));
    stateBytes += sizeof(GameFlag_DryfieldBanks[0]);

    memcpy(GameFlag_DryfieldFullBanks, stateBytes, sizeof(GameFlag_DryfieldFullBanks[0]));
    stateBytes += sizeof(GameFlag_DryfieldFullBanks[0]);

    // &GameFlag_ShelterBanks[bank], with bank * 0xE4 spelled out: the typed
    // index loads the array address before the first shift, the target after.
    bankTimesEight   = bank * 8;
    shelterBankBytes = (u8*)GameFlag_ShelterBanks;
    memcpy(shelterBankBytes + ((bankTimesEight - bank) * 8 + bank) * 4, stateBytes, sizeof(GameFlag_ShelterBanks[bank]));
    stateBytes += sizeof(GameFlag_ShelterBanks[bank]);

    memcpy(GameFlag_NeoArkBanks, stateBytes, sizeof(GameFlag_NeoArkBanks[0]));
    stateBytes += sizeof(GameFlag_NeoArkBanks[0]);

    memcpy(&gGameFlagNibbleBanks[bank], stateBytes, sizeof(gGameFlagNibbleBanks[bank]));

    // Keep the active replay selection and the user's vibration setting.
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene = savedDemoScene;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration = savedVibration;
    if (fsIsStageCdfAvailable(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage) != true) {
        gDisplayState.gameMode = DISPLAY_GAME_RESTART;
    }
    printf(Title_DemoCardRestoreMsg, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area);
}

/// Enables the title background strips and advances to the prompt state.
static void _titleShowBackgroundTask(Task* task)
{
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    task->state++;
}

void titleScreenTask(Task* task)
{
    TaskFuncTable5 phases;

    phases         = Title_PhaseTable;
    Title_LastRand = rand();
    phases.funcs[task->state](task);
}

void titleExitTask(Task* task)
{
    taskCallExit(task);
}

/// Queues the title background, palette and texture from stage-zero file 2.
///
/// Uses normal loading with no image displacement. Requires the stage-zero
/// file table, available image workspace and space in the CD request ring.
/// The queue copies the selected key bytes and all four argument bytes during
/// this call; loading finishes asynchronously.
static inline void _titleEnqueueBackgroundLoad(void)
{
    enum { TITLE_BACKGROUND_FILE_INDEX = 2 };
    u8 fileKey[4];
    u8 fileArgs[sizeof(gCdCmdQueue.entries[0].args)];

    // Selector byte 1 is ignored; the hundreds component is argument byte 0.
    fileKey[3]  = 0;
    fileKey[2]  = 0;
    fileKey[0]  = TITLE_BACKGROUND_FILE_INDEX;
    fileArgs[0] = 0;
    fileArgs[1] = CD_COMMAND_LOAD_DEFAULT;
    fileArgs[2] = 0;
    fileArgs[3] = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, fileArgs);
}

/// Plays the current disc's title intro, then restores the title display.
///
/// Starts at state 0 and requires loaded global stream descriptors, a matching
/// disc movie and exclusive display/CD ownership. START cancels playback and
/// enables the subsequent title fade. The task releases itself after restoring
/// image memory and both background buffers, then resumes normal presentation.
static void _titleIntroMovieTask(Task* task)
{
    enum {
        TITLE_INTRO_PREPARE,
        TITLE_INTRO_QUEUE_MOVIE,
        TITLE_INTRO_WAIT_READY,
        TITLE_INTRO_PLAYING,
        TITLE_INTRO_QUEUE_BACKGROUND,
        TITLE_INTRO_WAIT_BACKGROUND,
        TITLE_INTRO_UPLOAD_BACKGROUND,
        TITLE_INTRO_RESTORE_MEMORY,
        TITLE_INTRO_DISC_1_MOVIE_ID = 0x64,
        TITLE_INTRO_DISC_2_MOVIE_ID = 0x65,
    };
    u8          movieArgs[sizeof(gCdCmdQueue.entries[0].args)];
    GameLoc     movieLocation;
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case TITLE_INTRO_PREPARE:
            memCopyBytes(Fs_Streams, Stream_Slots, sizeof(Fs_Streams));
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state++;
            break;
        case TITLE_INTRO_QUEUE_MOVIE:
            movieLocation = gGameSession->location;
            if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_2) {
                movieLocation.loc.view = TITLE_INTRO_DISC_2_MOVIE_ID;
            } else {
                movieLocation.loc.view = TITLE_INTRO_DISC_1_MOVIE_ID;
            }
            // Playback uses only the slot byte; the queue copies all four bytes.
            movieArgs[0] = streamFindMovieSlot(&movieLocation.loc, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, movieArgs);
            task->state++;
            break;
        case TITLE_INTRO_WAIT_READY:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case TITLE_INTRO_PLAYING:
            if (cdCmdIsIdle()) {
                task->state++;
            } else if (padIsStartPressed()) {
                Title_SkipFadeFlag = 0;
                SetDispMask(0);
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        // Wait for playback or cancellation to drain before reusing its storage.
        case TITLE_INTRO_QUEUE_BACKGROUND:
            if (cdCmdIsIdle()) {
                gCdCmdQueue.preserveDisplayAfterDecode = 1;
                _titleEnqueueBackgroundLoad();
                task->state++;
            }
            break;
        case TITLE_INTRO_WAIT_BACKGROUND:
            if (cdCmdIsIdle()) {
                displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                task->state++;
            }
            break;
        case TITLE_INTRO_UPLOAD_BACKGROUND:
            streamResetGameRestore();
            displayUploadBackgroundImage(gDisplayState.drawBuffer);
            displayUploadBackgroundImage(gDisplayState.drawBuffer ^ 1);
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
            task->state++;
            break;
        case TITLE_INTRO_RESTORE_MEMORY:
            if (streamPollGameRestore(0, 0)) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

/// Coordinates the title intro or direct background load, then starts the screen.
///
/// Starts at state 0 with the title overlay and stage-zero file table loaded.
/// The movie uses the display-owned task list; this coordinator resumes after
/// playback restores the game loop. Direct entry waits for its background load.
/// The movie route waits two ticks before the screen spawn; both routes wait
/// one tick after it, then enable display output, latch the subsequent intro-skip
/// request and release this task.
static void _titleStartupTask(Task* startupTask)
{
    enum {
        TITLE_STARTUP_PREPARE,
        TITLE_STARTUP_WAIT_FIRST_FRAME,
        TITLE_STARTUP_WAIT_SECOND_FRAME,
        TITLE_STARTUP_SPAWN_SCREEN,
        TITLE_STARTUP_WAIT_SCREEN_FRAME,
        TITLE_STARTUP_FINISH,
        TITLE_STARTUP_QUEUE_BACKGROUND,
        TITLE_STARTUP_WAIT_BACKGROUND,
        TITLE_STARTUP_INTRO_TASK_INDEX = 1,
    };
    s32   nextState;
    Task* task;

    task = startupTask;
    switch (task->state) {
        case TITLE_STARTUP_PREPARE:
            // Normal boot gives the movie exclusive presentation ownership.
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            Title_SkipFadeFlag                      = 1;
            if ((gDisplayState.debugMode < 0) || (Wip_SysFlags.skipTitleIntro != 0)) {
                nextState          = TITLE_STARTUP_QUEUE_BACKGROUND;
                Title_SkipFadeFlag = 0;
            } else {
                displaySpawnTaskFromTable(Title_TaskDescs, TITLE_STARTUP_INTRO_TASK_INDEX, 0, 0);
                gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
                nextState                            = task->state + 1;
            }
            task->state = nextState;
            return;
        case TITLE_STARTUP_WAIT_FIRST_FRAME:
        case TITLE_STARTUP_WAIT_SECOND_FRAME:
            task->state = task->state + 1;
            return;
        case TITLE_STARTUP_SPAWN_SCREEN:
            if (Title_SkipFadeFlag != 0) {
                taskSpawn(TITLE_RESIDENT_TASK_BANK, TITLE_SCREEN_TASK_SLOT, TITLE_SCREEN_SKIP_FADE, 0);
            } else {
                taskSpawn(TITLE_RESIDENT_TASK_BANK, TITLE_SCREEN_TASK_SLOT, 0, 0);
            }
            /* fallthrough */
        case TITLE_STARTUP_WAIT_SCREEN_FRAME:
            task->state = task->state + 1;
            return;
        case TITLE_STARTUP_FINISH:
            SetDispMask(1);
            Wip_SysFlags.skipTitleIntro = 1;
            taskKill(task);
            return;
        case TITLE_STARTUP_QUEUE_BACKGROUND:
            // Skipping the movie still requires its background and chrome load.
            _titleEnqueueBackgroundLoad();
            task->state = task->state + 1;
            /* fallthrough */
        case TITLE_STARTUP_WAIT_BACKGROUND:
            if (cdCmdIsIdle()) {
                task->state = TITLE_STARTUP_SPAWN_SCREEN;
            }
            return;
    }
}

void titleEnqueueAttractDemoFile(s32 demoIndex)
{
    enum {
        TITLE_DEMO_FILE_GROUP             = 0x50,
        TITLE_DEMO_FIRST_FILE_ID_HUNDREDS = 0xA,
        TITLE_DEMO_FILE_KEY_SCRATCH_BYTES = 8,
    };
    u8  loadOptions[sizeof(gCdCmdQueue.entries[0].args)];
    u8* fileKey;

    fileKey                = SCRATCH_STACK_RESERVE_BYTES(TITLE_DEMO_FILE_KEY_SCRATCH_BYTES);
    gGameSession->field_80 = 0;
    fileKey[3]             = 0;
    fileKey[2]             = TITLE_DEMO_FILE_GROUP;
    fileKey[0]             = 0;
    loadOptions[0]         = demoIndex + TITLE_DEMO_FIRST_FILE_ID_HUNDREDS;
    loadOptions[3]         = 0;
    loadOptions[2]         = 0;
    loadOptions[1]         = CD_COMMAND_LOAD_DEFAULT;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadOptions);
    // The queue owns a copy before this scratch reservation is released.
    SCRATCH_STACK_RELEASE_BYTES(TITLE_DEMO_FILE_KEY_SCRATCH_BYTES);
}
