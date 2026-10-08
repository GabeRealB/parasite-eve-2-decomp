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

/// When set, Title_BootTask spawns phase task with arg 0x80000000 (skip fade TILE).
extern u16 Title_SkipFadeFlag;

static void _titleIntroMovieTask(Task* task);

void Title_BootTask(Task* task);

void func_807246B4(void);

static void _titleShowBackgroundTask(Task* task);
static void Title_InitTask(Task* arg0);
static void Title_MenuTask(Task* task);

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
    { { { TASK_BODY_NONE, 0xC0 } }, Title_BootTask },
    { { { TASK_BODY_NONE, 0xC0 } }, _titleIntroMovieTask },
};

/// Overlay state is stored in the loaded image; the loader does not clear BSS.
s32 Title_LastRand     = 0;
u16 Title_SkipFadeFlag = 0;

/// The title task's states, which `titleScreenTask` copies and indexes by
/// `Task::state`: set-up, the flag advance, the menu in two states and the kill.
static const TaskFuncTable5 Title_PhaseTable = {
    .funcs = {
        Title_InitTask,
        _titleShowBackgroundTask,
        Title_MenuTask,
        Title_MenuTask,
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

static void Title_InitTask(Task* arg0)
{
    s32               flag;
    DisplayState*     ds;
    _TitleScreenWork* work;

    flag                          = 1;
    ds                            = &gDisplayState;
    ds->control.flags.imageSource = DISPLAY_IMAGE_NONE;
    Wip_UiHolder                  = NULL;
    if (arg0->spawnArg1.value < 0) {
        flag                   = 0;
        arg0->spawnArg1.value &= 0x7FFFFFFF;
    }
    if (arg0->spawnArg1.value > 0) {
        arg0->spawnArg1.value -= 1;
        return;
    }
    work = memCalloc(sizeof(*work), 0);
    if (work != NULL) {
        arg0->work              = work;
        work->screenFadeEnabled = flag;
        work->menuCount         = 5;
        work->selection         = TITLE_MENU_NEW_GAME;
        work->idleFrames        = 0;
        if (Wip_SysFlags.gameOver != 0) {
            work->selection = TITLE_MENU_LOAD_GAME;
        }
        Text_LoadClutImages();
        displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_KEEP_VIEW);
        ds->holdState                 = DISPLAY_HOLD_INITIAL;
        work->idleFrames              = -TITLE_SCREEN_FADE_FRAMES;
        ds->control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
        if (ds->debugMode != 0) {
            func_807246B4();
        }
        cdCmdEnqueueDisplayResource(1, 0, CD_COMMAND_DISPLAY_LOAD_MENU);
        arg0->state += 2;
        Title_MenuTask(arg0);
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

static void Title_MenuTask(Task* task)
{
    _TitleScreenWork* work = task->work;
    s32               idleFrames;
    s32               i;

    idleFrames       = work->idleFrames + 1;
    work->idleFrames = idleFrames;
    if (idleFrames > TITLE_SCREEN_IDLE_TIMEOUT) {
        if (idleFrames < TITLE_SCREEN_IDLE_TIMEOUT + TITLE_SCREEN_FADE_FRAMES) {
            if (work->screenFadeEnabled != 0) {
                TILE*     tile;
                DR_TPAGE* tpage;

                tile           = gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                setlen(tile, 3);
                setcode(tile, 0x60);
                tile->r0 = tile->g0 = tile->b0 = (idleFrames - TITLE_SCREEN_IDLE_TIMEOUT) * 16 - 1;
                tile->x0                       = -0xA0;
                tile->y0                       = -0x78;
                tile->w                        = 0x140;
                tile->h                        = 0xF0;
                setSemiTrans(tile, 1);
                addPrim(gGpuCurrentOt, tile);

                tpage          = gGpuPrimCursor;
                gGpuPrimCursor = tpage + 1;
                setlen(tpage, 1);
                tpage->code[0] = 0xE1000240;
                addPrim(gGpuCurrentOt, tpage);
            }
        } else {
            Wip_SysFlags.skipTitleIntro = 0;
            if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_1) {
                taskCallExit(task);
                gDisplayState.demoScene = gameMainGetInitializationCount() + 2;
                gDisplayState.demoScene = gDisplayState.demoScene % 3 + 1;
                printf(Title_DemoStartMsg);
                taskSpawn(0, 3, 2, 0);
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            } else {
                gDisplayState.gameMode = DISPLAY_GAME_RESTART;
            }
        }
        return;
    }

    if (work->screenFadeEnabled != 0 && idleFrames < 0) {
        TILE*     tile;
        DR_TPAGE* tpage;
        s32       color;

        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        setlen(tile, 3);
        setcode(tile, 0x62);
        color    = ~(work->idleFrames << 4);
        tile->x0 = -0xA0;
        tile->y0 = -0x78;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->b0 = color;
        tile->g0 = color;
        tile->r0 = color;
        addPrim(gGpuCurrentOt, tile);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000240;
        addPrim(gGpuCurrentOt, tpage);
    }

    if (task->state == 3) {
        if (work->menuFade < TITLE_SCREEN_FADE_FULL) {
            work->menuFade += TITLE_SCREEN_FADE_STEP;
        }
        for (i = 0; i < TITLE_CHROME_MENU_ROW_COUNT; i++) {
            _titleDrawChromeRow(i * TITLE_CHROME_MENU_ROW_STEP + TITLE_CHROME_MENU_FIRST_Y,
                                i * TITLE_CHROME_ROW_HEIGHT + TITLE_CHROME_ATLAS_V_NEW_GAME, work->menuFade);
        }
        _titleDrawChromeRow((work->selection - TITLE_MENU_NEW_GAME) * TITLE_CHROME_MENU_ROW_STEP + TITLE_CHROME_MENU_FIRST_Y,
                            TITLE_CHROME_ATLAS_V_CURSOR, work->menuFade);
        _titleDrawChromeRow(work->menuFade / 8 + TITLE_CHROME_PROMPT_Y, TITLE_CHROME_ATLAS_V_PROMPT,
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
                work->selection = -1;
            }
            if (work->selection < 0) {
                work->selection += work->menuCount;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | PAD_BUTTON_START) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            taskSpawn(0, Title_MenuSpawnIds[work->selection], 0, 0);
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            taskCallExit(task);
        }
    } else {
        if (work->promptFade < TITLE_SCREEN_FADE_FULL) {
            work->promptFade += TITLE_SCREEN_FADE_STEP;
        }
        _titleDrawChromeRow(TITLE_CHROME_PROMPT_Y - (TITLE_SCREEN_FADE_FULL - work->promptFade) / 8,
                            TITLE_CHROME_ATLAS_V_PROMPT, work->promptFade);
        _titleDrawChromeRow(TITLE_CHROME_COPYRIGHT_Y, TITLE_CHROME_ATLAS_V_COPYRIGHT, TITLE_SCREEN_FADE_FULL);
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | PAD_BUTTON_START) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            work->idleFrames = 0;
            task->state++;
        }
    }
}

/// Restore demo card / save banks from Fs_ActorLoadBase2 (or 0x80600100 when
/// gDisplayState.demoScene == DISPLAY_DEMO_FIXED_REPLAY).
/// Preserves gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration / field_23 across the bulk copy.
void Title_RestoreDemoCard(void)
{
    u8* src;
    s32 saveField23;
    s32 saveField21;
    s32 bank;
    s32 t;
    u8* base;

    src         = (u8*)Fs_ActorLoadBase2;
    bank        = GAME_FLAG_NIBBLE_BANK_LIVE;
    saveField23 = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene;
    saveField21 = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration;
    if (gDisplayState.demoScene == DISPLAY_DEMO_FIXED_REPLAY) {
        src = FILE_SYSTEM_FIXED_REPLAY_BASE;
    }
    printf(Title_DemoCardRestoreMsg, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area);

    memcpy(&gMcSaveData[MEMORY_CARD_SAVE_LIVE], src, sizeof(McSaveData));
    src += sizeof(McSaveData);

    // Restore the live player image; the serialized backup stays intact.
    memcpy((u8*)&gPlayerStatus + bank * PLAYER_STATUS_SAVE_RECORD_BYTES, src, PLAYER_STATUS_SAVE_RECORD_BYTES);
    src += PLAYER_STATUS_SAVE_RECORD_BYTES;

    memcpy(&GameFlag_AcropolisBanks[bank], src, GAME_FLAG_ACROPOLIS_BANK_BYTES);
    src += GAME_FLAG_ACROPOLIS_BANK_BYTES;

    memcpy(GameFlag_DryfieldBanks, src, GAME_FLAG_DRYFIELD_BANK_BYTES);
    src += GAME_FLAG_DRYFIELD_BANK_BYTES;

    memcpy(GameFlag_DryfieldFullBanks, src, GAME_FLAG_DRYFIELD_NIGHT_BANK_BYTES);
    src += GAME_FLAG_DRYFIELD_NIGHT_BANK_BYTES;

    // &GameFlag_ShelterBanks[bank], with bank * 0xE4 spelled out: the typed
    // index loads the array address before the first shift, the target after.
    t    = bank * 8;
    base = (u8*)GameFlag_ShelterBanks;
    memcpy(base + ((t - bank) * 8 + bank) * 4, src, GAME_FLAG_MINE_SHELTER_BANK_BYTES);
    src += GAME_FLAG_MINE_SHELTER_BANK_BYTES;

    memcpy(GameFlag_NeoArkBanks, src, GAME_FLAG_NEO_ARK_BANK_BYTES);
    src += GAME_FLAG_NEO_ARK_BANK_BYTES;

    memcpy(&gGameFlagNibbleBanks[bank], src, sizeof(gGameFlagNibbleBanks[bank]));

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene = saveField23;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration = saveField21;
    if (Fs_StageCdfIsAvailable(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage) != 1) {
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

/// Queues stage-zero file 2, containing the title background and textures.
///
/// Both borrowed byte buffers provide four bytes; the enqueue copies them
/// immediately. The file selector's byte 1 is unused by the CD request API.
static inline void _titleEnqueueBackgroundLoad(u8* fileKey, u8* loadOptions)
{
    enum { TITLE_BACKGROUND_FILE_INDEX = 2 };

    fileKey[3]     = 0;
    fileKey[2]     = 0;
    fileKey[0]     = TITLE_BACKGROUND_FILE_INDEX;
    loadOptions[0] = 0;
    loadOptions[1] = CD_COMMAND_LOAD_DEFAULT;
    loadOptions[2] = 0;
    loadOptions[3] = 0;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadOptions);
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
    u8          fileKey[4];
    u8          loadOptions[sizeof(gCdCmdQueue.entries[0].args)];
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
                _titleEnqueueBackgroundLoad(fileKey, loadOptions);
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

void Title_BootTask(Task* arg0)
{
    u8    param1[4];
    u8    param2[4];
    s32   next;
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            Title_SkipFadeFlag                      = 1;
            if ((gDisplayState.debugMode < 0) || (Wip_SysFlags.skipTitleIntro != 0)) {
                next               = 6;
                Title_SkipFadeFlag = 0;
            } else {
                displaySpawnTaskFromTable(Title_TaskDescs, 1, 0, 0);
                gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
                next                                 = task->state + 1;
            }
            task->state = next;
            return;
        case 1:
        case 2:
            task->state = task->state + 1;
            return;
        case 3:
            if (Title_SkipFadeFlag != 0) {
                taskSpawn(0, 2, 0x80000000, 0);
            } else {
                taskSpawn(0, 2, 0, 0);
            }
            /* fallthrough */
        case 4:
            task->state = task->state + 1;
            return;
        case 5:
            SetDispMask(1);
            Wip_SysFlags.skipTitleIntro = 1;
            taskKill(task);
            return;
        case 6:
            param1[3] = 0;
            param1[2] = 0;
            param1[0] = 2;
            param2[0] = 0;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            task->state = task->state + 1;
            /* fallthrough */
        case 7:
            if (cdCmdIsIdle() & 0xFFFF) {
                task->state = 3;
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
