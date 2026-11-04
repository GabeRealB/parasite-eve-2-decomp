#include "pe2.h"

#include "title.h"

PE2_BIND(psyq, package)
#include PE2_INCLUDE(psyq, memory.h)
#include PE2_INCLUDE(psyq, rand.h)
#include PE2_INCLUDE(psyq, stdio.h)

#include PE2_INCLUDE(main, display.h)
#include PE2_INCLUDE(main, fs.h)
#include PE2_INCLUDE(main, game_debug.h)
#include PE2_INCLUDE(main, gameflag.h)
#include PE2_INCLUDE(main, gamemain.h)
#include PE2_INCLUDE(main, mc.h)
#include PE2_INCLUDE(main, mem.h)
#include PE2_INCLUDE(main, pad.h)
#include PE2_INCLUDE(main, scratch.h)
#include PE2_INCLUDE(main, session.h)
#include PE2_INCLUDE(main, sound.h)
#include PE2_INCLUDE(main, stream.h)
#include PE2_INCLUDE(main, task.h)
#include PE2_INCLUDE(main, text.h)
#include PE2_INCLUDE(main, ui.h)
#include PE2_INCLUDE(main, wipsys.h)

/// The chrome: one texture page of the rows the screen draws, a cell each.
PE2_BIND(chrome, image)

/// The attract demos the idle timeout cycles through on disc 1, in order: each a stage-zero file of the
/// demo's saved state.
PE2_BIND(demos, list)

/// The stage-zero file holding the screen's background, its palette and the chrome, loaded before the screen shows.
PE2_BIND(screen, file)

/// The stage-zero display resource the menus the title opens draw with, loaded as the screen starts: one of the group
/// main loads display resources from.
PE2_BIND(menu, file)

/// The file holding the title, whose streams it plays: the intro, one record for each disc.
PE2_BIND_SELF(file)

/// Entries of the title menu, in the order of its label and task tables.
///
/// The cursor only ever rests on New Game, Load Game and Configuration: the
/// two entries before them are stepped over, and Debug Option lies beyond the
/// count the cursor wraps at.
enum {
    MENU_SURVIVAL,
    MENU_EXTRA_GAME,
    MENU_NEW_GAME,
    MENU_LOAD_GAME,
    MENU_CONFIGURATION,
    MENU_DEBUG_OPTION,
};

PE2_EDITOR_LABEL("Timing")
/// Idle update ticks before input gives way to the black fade. The incremented count must exceed it: the tick after it
/// starts the fade, and the tick a fade's length after that starts a disc-1 attract demo or requests a disc-2 restart
/// (with the originals, ticks 901 and 916). Prompt confirmation and navigation reset the count.
PE2_OPTION_DEF(IDLE_TIMEOUT_TICKS, int, 900)
/// Brightness the prompt or menu gains each update tick as it fades in. The fade stops on reaching
/// `CHROME_FULL_BRIGHTNESS` and menu input waits for that endpoint, so it divides it (asserted beside it).
PE2_OPTION_DEF(CHROME_FADE_STEP, int, 8)

/// How fast the black fades run: each tick moves their brightness by `1 << this`, so a fade lasts `256 >> this` ticks.
/// The original 4 fades over 16 ticks; 3 takes 32 and 5 takes 8.
PE2_OPTION_DEF(BLACK_FADE_STEP_SHIFT, int, 4)

PE2_EDITOR_LABEL("Layout")
/// Draw-environment Y of the first menu row, and the distance between rows.
PE2_OPTION_DEF(MENU_FIRST_Y, int, 0x38)
PE2_OPTION_DEF(MENU_ROW_STEP, int, 14)
/// Draw-environment Y of the PRESS START BUTTON prompt and of the copyright line.
PE2_OPTION_DEF(PROMPT_Y, int, 0x40)
PE2_OPTION_DEF(COPYRIGHT_Y, int, 0x5C)
/// Brightness that moves the prompt one pixel as it slides in and out: it travels the full brightness divided by this.
PE2_OPTION_DEF(SLIDE_BRIGHTNESS_PER_PIXEL, int, 8)
PE2_EDITOR_LABEL("")

/// Spawn-payload bit that disables the title screen's subtractive black fades.
///
/// Consumed on the first initialization attempt; suppression takes effect only
/// if screen work is allocated on that attempt. A delay or allocation failure
/// loses the request. Prompt/menu brightness ramps and idle timing still run.
/// The unsigned word also selects `taskSpawn`'s unsigned argument member.
#define SCREEN_SKIP_BLACK_FADE 0x80000000U

/// Prompt and menu slots in the title-screen state dispatcher.
enum {
    SCREEN_STATE_PROMPT = 2,
    SCREEN_STATE_MENU   = 3,
};

/// Resident task-bank slots used by the title startup and idle transition.
enum {
    RESIDENT_TASK_BANK      = 0,
    SCREEN_TASK_SLOT        = 2,
    START_SESSION_TASK_SLOT = 3,
    START_SESSION_DEMO_ARG  = 2,
};

/// Counter span of the title screen's subtractive black fades, in update ticks.
///
/// The idle count starts at the negative span and increments before drawing.
/// Fade-in draws from one tick after the start up to -1; fade-out draws from
/// timeout+1 to one tick short of timeout+span, and hands off at timeout+span.
/// Suppressing the tiles preserves these counter intervals.
STATIC_ASSERT(PE2_OPTION(BLACK_FADE_STEP_SHIFT) >= 0 && PE2_OPTION(BLACK_FADE_STEP_SHIFT) <= 8, black_fade_step_shift_fits_a_byte);
enum { SCREEN_BLACK_FADE_TICKS = 256 >> PE2_OPTION(BLACK_FADE_STEP_SHIFT) };

/// Unscaled RGB modulation of an additive title-chrome sprite row.
///
/// The prompt/menu ramps range from zero (invisible) to this value; the prompt
/// and copyright use its complement while the menu appears. This signed int
/// constant participates in brightness subtraction and slide calculations.
enum { CHROME_FULL_BRIGHTNESS = 0x80 };
STATIC_ASSERT(CHROME_FULL_BRIGHTNESS % PE2_OPTION(CHROME_FADE_STEP) == 0, chrome_fade_step_divides_full_brightness);

/// Page-local V origins, in texels, of the chrome's rows: its cells.
enum {
    CHROME_ATLAS_V_PROMPT    = PE2_VALUE(chrome, cell, prompt, _v),
    CHROME_ATLAS_V_COPYRIGHT = PE2_VALUE(chrome, cell, copyright, _v),
    CHROME_ATLAS_V_CURSOR    = PE2_VALUE(chrome, cell, cursor, _v),
    CHROME_ATLAS_V_NEW_GAME  = PE2_VALUE(chrome, cell, new_game, _v),
    CHROME_ATLAS_V_CONTINUE  = PE2_VALUE(chrome, cell, continue, _v),
    CHROME_ATLAS_V_OPTION    = PE2_VALUE(chrome, cell, option, _v),
    CHROME_ROW_WIDTH         = PE2_VALUE(chrome, cell, prompt, _w),
    CHROME_ROW_HEIGHT        = PE2_VALUE(chrome, cell, prompt, _h),
};

/// Every row is drawn the one size, from the page's left edge.
#define CHROME_ROW_FITS(name)                                                                           \
    (PE2_VALUE(chrome, cell, name, _u) == 0 && PE2_VALUE(chrome, cell, name, _w) == CHROME_ROW_WIDTH && \
     PE2_VALUE(chrome, cell, name, _h) == CHROME_ROW_HEIGHT)
STATIC_ASSERT(CHROME_ROW_FITS(prompt) && CHROME_ROW_FITS(copyright) && CHROME_ROW_FITS(cursor), chrome_rows_fit);
STATIC_ASSERT(CHROME_ROW_FITS(new_game) && CHROME_ROW_FITS(continue) && CHROME_ROW_FITS(option), chrome_menu_rows_fit);

/// Menu rows the chrome draws, one for each reachable entry: New Game up to Debug Option.
enum { CHROME_MENU_ROW_COUNT = MENU_DEBUG_OPTION - MENU_NEW_GAME };

/// The menu's rows are found by their index, so its labels lie one under another.
STATIC_ASSERT(CHROME_ATLAS_V_CONTINUE == CHROME_ATLAS_V_NEW_GAME + CHROME_ROW_HEIGHT &&
                  CHROME_ATLAS_V_OPTION == CHROME_ATLAS_V_CONTINUE + CHROME_ROW_HEIGHT,
              chrome_menu_rows_follow);

/// The title screen task's work: the PRESS START BUTTON prompt and the menu
/// that replaces it, with the idle count that fades the screen in and out.
typedef struct {
    s32 idleFrames;        // Idle update ticks, starting at -SCREEN_BLACK_FADE_TICKS
    s32 selection;         // Highlighted entry (a MENU_ value)
    s32 screenFadeEnabled; // Nonzero to fade the screen in from black and back out to it
    s32 promptFade;        // Brightness of the prompt as it slides in (0..CHROME_FULL_BRIGHTNESS)
    s32 menuFade;          // Brightness of the menu; the prompt and copyright fade out by its complement
    s32 menuCount;         // Entries the cursor wraps over (5, leaving Debug Option out)
} _ScreenWork;

static void _introMovieTask(Task* task);

static void _startupTask(Task* startupTask);

/// An entry of the external debug image, which a development console loads above the retail 2 MB and no disc
/// holds (see `taskDebugLaunchCallback`). Title-screen setup calls it once when `debugMode` is set; what it does is
/// unproven, so it keeps its address. The link script places it (`PROVIDE`).
void func_807246B4(void);

static void _showBackgroundTask(Task* task);
static void _initializeScreenTask(Task* task);
static void _updateScreenTask(Task* task);

/// Retained NUL-terminated text for New Game; the retail menu uses atlas text.
static char _gMenuNewGameLabel[] = "New Game";
/// Retained NUL-terminated text for Load Game; the retail menu uses atlas text.
static char _gMenuLoadGameLabel[] = "Load Game";
/// Retained NUL-terminated text for Configuration; the retail menu uses atlas text.
static char _gMenuConfigurationLabel[] = "Configuration";
/// Retained NUL-terminated text for the unreachable Debug Option menu entry.
static char _gMenuDebugOptionLabel[] = "Debug Option";
/// Retained NUL-terminated text for the skipped Extra Game menu entry.
static char _gMenuExtraGameLabel[] = "Extra Game";
/// Retained NUL-terminated text for the skipped Survival menu entry.
static char _gMenuSurvivalLabel[] = "Survival";

/// Retained text-label pointers indexed by MENU_ entry; there is no terminator.
///
/// All six labels remain in the loaded image, but the retail menu draws its
/// three reachable entries from the texture atlas and never reads this table.
static char* _gMenuTextLabels[MENU_DEBUG_OPTION + 1] = {
    [MENU_SURVIVAL]      = _gMenuSurvivalLabel,
    [MENU_EXTRA_GAME]    = _gMenuExtraGameLabel,
    [MENU_NEW_GAME]      = _gMenuNewGameLabel,
    [MENU_LOAD_GAME]     = _gMenuLoadGameLabel,
    [MENU_CONFIGURATION] = _gMenuConfigurationLabel,
    [MENU_DEBUG_OPTION]  = _gMenuDebugOptionLabel,
};

enum {
    MENU_LOAD_GAME_TASK_SLOT     = 4,
    MENU_CONFIGURATION_TASK_SLOT = 5,
    MENU_EXIT_TASK_SLOT          = 6,
};

/// Resident bank-0 task slots indexed by MENU_ entry, without a terminator.
///
/// Confirmation passes zero for both spawn payloads. Only New Game, Load Game
/// and Configuration are reachable; the other entries dispatch the exit task.
static s32 _gMenuTaskSlots[MENU_DEBUG_OPTION + 1] = {
    [MENU_SURVIVAL]      = MENU_EXIT_TASK_SLOT,
    [MENU_EXTRA_GAME]    = MENU_EXIT_TASK_SLOT,
    [MENU_NEW_GAME]      = START_SESSION_TASK_SLOT,
    [MENU_LOAD_GAME]     = MENU_LOAD_GAME_TASK_SLOT,
    [MENU_CONFIGURATION] = MENU_CONFIGURATION_TASK_SLOT,
    [MENU_DEBUG_OPTION]  = MENU_EXIT_TASK_SLOT,
};

TaskDesc gTitleStartupTaskDescriptors[2] = {
    { .header = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, .callback = _startupTask },
    { .header = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, .callback = _introMovieTask },
};

/// Most recent SDK rand() result sampled before a title-screen state runs.
///
/// Initialized when the overlay loads and overwritten on every screen callback;
/// no retail code reads the sample. The call advances the SDK random sequence.
static s32 _gScreenRandomSample = 0;

/// Whether the next title screen suppresses its black fade (0 fade, 1 skip).
///
/// Startup sets this before the intro; direct entry and START cancellation clear
/// it. Natural movie completion leaves it set for the screen's spawn request.
/// It is an initialized halfword in the loaded overlay, rather than cleared BSS.
static u16 _gScreenSkipFade = false;

/// Title-screen state handlers: initialize, show background, prompt, menu and release.
///
/// Copied by value before dispatch; Task::state must be in 0..4. Both prompt
/// and menu use the same updater. The release handler can destroy the task.
static const TaskFuncTable5 _gScreenStateHandlers = {
    .funcs = {
        _initializeScreenTask,
        _showBackgroundTask,
        _updateScreenTask,
        _updateScreenTask,
        taskKill,
    },
};

/// NUL-terminated diagnostic printed when a disc-1 title timeout starts an attract demo.
static const char _gAttractDemoStartMessage[] = "##########DEMO START\n";

/// Diagnostic format for the live save's stage and area before and after demo restore.
///
/// Both arguments are promoted integer location bytes; the retained text calls
/// the area a scene. The two bytes after the first NUL are not part of the
/// format and their role is unproven. The 44-byte extent retains them without
/// adding the C literal's final, implicit NUL.
static const char _gAttractDemoRestoreFormat[44] = "####DEMO_CARD_RESTORE STAGE %d, SCENE %d\n\0\x22\xE1";

static void _drawChromeRow(s32 screenY, s32 atlasV, s32 brightness);

/// Initializes the title prompt after its packed spawn delay expires.
///
/// Requires state 0, a loaded title background and initialized primary heap.
/// The high no-fade bit is consumed before the delay and allocation; a delayed
/// request or failed allocation therefore enables fades again on retry.
/// Allocation failure leaves state 0. Success gives the task ownership of its
/// cleared screen work and runs the first prompt update immediately.
static void _initializeScreenTask(Task* task)
{
    enum { MENU_FILE_ID = PE2_VALUE(menu, _id) };
    STATIC_ASSERT(FS_FILE_GROUP(MENU_FILE_ID) == CD_COMMAND_DISPLAY_FILE_GROUP, menu_file_is_a_display_resource);
    /// Low-31-bit mask for the spawn payload's initialization-tick countdown.
    ///
    /// A delay in 0..0x7FFFFFFF defers allocation for that many attempts. Clearing
    /// the high flag leaves a nonnegative signed value for the countdown tests.
    enum { SCREEN_DELAY_MASK = 0x7FFFFFFF };
    bool          screenFadeEnabled;
    DisplayState* display;
    _ScreenWork*  work;

    screenFadeEnabled                  = true;
    display                            = &gDisplayState;
    display->control.flags.imageSource = DISPLAY_IMAGE_NONE;
    gUiPromptObject                    = NULL;
    if ((task->spawnArg1.unsignedValue & SCREEN_SKIP_BLACK_FADE) != 0) {
        screenFadeEnabled      = false;
        task->spawnArg1.value &= SCREEN_DELAY_MASK;
    }
    if (task->spawnArg1.value > 0) {
        task->spawnArg1.value -= 1;
        return;
    }
    work = memCalloc(sizeof(*work), false);
    if (work != NULL) {
        task->work              = work;
        work->screenFadeEnabled = screenFadeEnabled;
        work->menuCount         = MENU_DEBUG_OPTION;
        work->selection         = MENU_NEW_GAME;
        work->idleFrames        = 0;
        if (gGameMainPersistentState.gameOver != 0) {
            work->selection = MENU_LOAD_GAME;
        }
        textUploadPalettes();
        displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_KEEP_VIEW);
        display->holdState                 = DISPLAY_HOLD_INITIAL;
        work->idleFrames                   = -SCREEN_BLACK_FADE_TICKS;
        display->control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
        if (display->debugMode != 0) {
            func_807246B4();
        }
        cdCmdEnqueueDisplayResource(FS_FILE_HUNDREDS(MENU_FILE_ID), FS_FILE_INDEX(MENU_FILE_ID), CD_COMMAND_DISPLAY_LOAD_MENU);
        task->state += SCREEN_STATE_PROMPT;
        _updateScreenTask(task);
    }
}

/// Prepends the GPU draw mode used by the title chrome's sprites.
///
/// Selects the chrome's texture page at its depth, with additive blending and
/// dithering, and drawing into the displayed area disabled.
/// Requires a current ordering tag and word-aligned space for one DR_TPAGE;
/// its storage must remain live until the GPU consumes the ordering table.
/// The prepend makes this mode execute before primitives already at the tag.
static inline void _prependChromeDrawMode(void)
{
    DR_TPAGE* drawMode;

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(PE2_VALUE(chrome, _depth), GPU_BLEND_ADD, PE2_VALUE(chrome, _page_x), PE2_VALUE(chrome, _page_y)));
    addPrim(gGpuCurrentOt, drawMode);
}

/// Queues one centered title-chrome atlas row with additive brightness.
///
/// `screenY` is the top edge in draw-environment pixels; `atlasV` is a
/// CHROME_ATLAS_V_ origin in page-local texels. `brightness` is normally
/// 0..CHROME_FULL_BRIGHTNESS (0 invisible, 0x80 unscaled); packing keeps its
/// low byte. Coordinates retain the sprite's signed-halfword/unsigned-byte
/// truncation. The chrome texture and palette must already be loaded.
/// Requires a current ordering tag and word-aligned space for one SPRT plus
/// one DR_TPAGE; their storage must remain live until GPU consumption.
static void _drawChromeRow(s32 screenY, s32 atlasV, s32 brightness)
{
    /// Modulated free-size sprite command with semi-transparency enabled.
    enum { CHROME_SPRITE_COMMAND = 0x66 };
    SPRT* sprite;
    u8    brightnessByte;

    brightnessByte                      = brightness;
    sprite                              = gGpuPrimCursor;
    gGpuPrimCursor                      = sprite + 1;
    sprite->x0                          = -CHROME_ROW_WIDTH / 2;
    sprite->w                           = CHROME_ROW_WIDTH;
    sprite->h                           = CHROME_ROW_HEIGHT;
    sprite->clut                        = getClut(PE2_VALUE(chrome, _palette_x), PE2_VALUE(chrome, _palette_y));
    GPU_PRIMITIVE_COLOR_WORD(sprite, 0) = (brightnessByte << 16) | (brightnessByte << 8) | brightnessByte;
    setlen(sprite, (sizeof(*sprite) - sizeof(sprite->tag)) / sizeof(u_long));
    sprite->u0 = 0;
    sprite->v0 = atlasV;
    setcode(sprite, CHROME_SPRITE_COMMAND);
    sprite->y0 = screenY;
    addPrim(gGpuCurrentOt, sprite);

    // Prepending after the sprite makes its texture mode execute first.
    _prependChromeDrawMode();
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
static inline void _prependScreenFadeDrawMode(void)
{
    enum {
        SCREEN_FADE_TEXTURE_DEPTH_4BIT = 0,
        SCREEN_FADE_DRAW_MODE_COMMAND =
            _get_mode(false, true, getTPage(SCREEN_FADE_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0)),
    };
    DR_TPAGE* drawMode;

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, ARRAY_SIZE(drawMode->code));
    drawMode->code[0] = SCREEN_FADE_DRAW_MODE_COMMAND;
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
static void _updateScreenTask(Task* task)
{
    enum {
        SCREEN_FADE_BRIGHTNESS_STEP = 1 << PE2_OPTION(BLACK_FADE_STEP_SHIFT),
        SCREEN_TILE_SEMITRANSPARENT = 0x62,
        MENU_BEFORE_FIRST           = -1,
    };
    _ScreenWork* work = task->work;
    s32          idleFrames;
    s32          menuRow;

    // Let the idle fade finish before replacing the title presentation.
    idleFrames       = work->idleFrames + 1;
    work->idleFrames = idleFrames;
    if (idleFrames > PE2_OPTION(IDLE_TIMEOUT_TICKS)) {
        if (idleFrames < PE2_OPTION(IDLE_TIMEOUT_TICKS) + SCREEN_BLACK_FADE_TICKS) {
            if (work->screenFadeEnabled != 0) {
                TILE* tile;

                tile           = gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                setTile(tile);
                tile->r0 = tile->g0 = tile->b0 = (idleFrames - PE2_OPTION(IDLE_TIMEOUT_TICKS)) * SCREEN_FADE_BRIGHTNESS_STEP - 1;
                tile->x0                       = -DISPLAY_SCREEN_WIDTH / 2;
                tile->y0                       = -DISPLAY_SCREEN_HEIGHT / 2;
                tile->w                        = DISPLAY_SCREEN_WIDTH;
                tile->h                        = DISPLAY_SCREEN_HEIGHT;
                setSemiTrans(tile, 1);
                addPrim(gGpuCurrentOt, tile);

                _prependScreenFadeDrawMode();
            }
        } else {
            gGameMainPersistentState.skipTitleIntro = 0;
            if (gGameMainPersistentState.discNumber == GAME_MAIN_DISC_1) {
                taskCallExit(task);
                gDisplayState.demoScene = gameMainGetInitializationCount() + 2;
                gDisplayState.demoScene = gDisplayState.demoScene % PE2_VALUE(demos, _count) + 1;
                printf(_gAttractDemoStartMessage);
                taskSpawn(RESIDENT_TASK_BANK, START_SESSION_TASK_SLOT, START_SESSION_DEMO_ARG, 0);
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
        setcode(tile, SCREEN_TILE_SEMITRANSPARENT);
        fadeBrightness = ~(work->idleFrames << PE2_OPTION(BLACK_FADE_STEP_SHIFT));
        tile->x0       = -DISPLAY_SCREEN_WIDTH / 2;
        tile->y0       = -DISPLAY_SCREEN_HEIGHT / 2;
        tile->w        = DISPLAY_SCREEN_WIDTH;
        tile->h        = DISPLAY_SCREEN_HEIGHT;
        tile->b0       = fadeBrightness;
        tile->g0       = fadeBrightness;
        tile->r0       = fadeBrightness;
        addPrim(gGpuCurrentOt, tile);

        _prependScreenFadeDrawMode();
    }

    // Fade the menu in over the prompt, then accept navigation and selection.
    if (task->state == SCREEN_STATE_MENU) {
        if (work->menuFade < CHROME_FULL_BRIGHTNESS) {
            work->menuFade += PE2_OPTION(CHROME_FADE_STEP);
        }
        for (menuRow = 0; menuRow < CHROME_MENU_ROW_COUNT; menuRow++) {
            _drawChromeRow(menuRow * PE2_OPTION(MENU_ROW_STEP) + PE2_OPTION(MENU_FIRST_Y),
                           menuRow * CHROME_ROW_HEIGHT + CHROME_ATLAS_V_NEW_GAME, work->menuFade);
        }
        _drawChromeRow((work->selection - MENU_NEW_GAME) * PE2_OPTION(MENU_ROW_STEP) + PE2_OPTION(MENU_FIRST_Y),
                       CHROME_ATLAS_V_CURSOR, work->menuFade);
        _drawChromeRow(work->menuFade / PE2_OPTION(SLIDE_BRIGHTNESS_PER_PIXEL) + PE2_OPTION(PROMPT_Y), CHROME_ATLAS_V_PROMPT,
                       CHROME_FULL_BRIGHTNESS - work->menuFade);
        _drawChromeRow(PE2_OPTION(COPYRIGHT_Y), CHROME_ATLAS_V_COPYRIGHT, CHROME_FULL_BRIGHTNESS - work->menuFade);
        if (work->menuFade < CHROME_FULL_BRIGHTNESS) {
            return;
        }

        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_DOWN) != 0) {
            work->idleFrames = 0;
            work->selection++;
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            if (work->selection >= work->menuCount) {
                work->selection -= work->menuCount;
            }
            if (work->selection == MENU_SURVIVAL) {
                work->selection = MENU_EXTRA_GAME;
            }
            if (work->selection == MENU_EXTRA_GAME) {
                work->selection = MENU_NEW_GAME;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_UP) != 0) {
            work->idleFrames = 0;
            work->selection--;
            sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
            if (work->selection == MENU_EXTRA_GAME) {
                work->selection = MENU_SURVIVAL;
            }
            if (work->selection == MENU_SURVIVAL) {
                work->selection = MENU_BEFORE_FIRST;
            }
            if (work->selection < 0) {
                work->selection += work->menuCount;
            }
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | PAD_BUTTON_START) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            taskSpawn(RESIDENT_TASK_BANK, _gMenuTaskSlots[work->selection], 0, 0);
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            taskCallExit(task);
        }
    } else {
        // Slide the prompt into place; confirmation opens the menu immediately.
        if (work->promptFade < CHROME_FULL_BRIGHTNESS) {
            work->promptFade += PE2_OPTION(CHROME_FADE_STEP);
        }
        _drawChromeRow(PE2_OPTION(PROMPT_Y) - (CHROME_FULL_BRIGHTNESS - work->promptFade) / PE2_OPTION(SLIDE_BRIGHTNESS_PER_PIXEL),
                       CHROME_ATLAS_V_PROMPT, work->promptFade);
        _drawChromeRow(PE2_OPTION(COPYRIGHT_Y), CHROME_ATLAS_V_COPYRIGHT, CHROME_FULL_BRIGHTNESS);
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | PAD_BUTTON_START) != 0) {
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
    u8*       mineShelterBankBytes;

    stateBytes     = gFsActorBuffer2Base;
    bank           = GAME_FLAG_NIBBLE_BANK_LIVE;
    savedDemoScene = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene;
    savedVibration = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration;
    if (gDisplayState.demoScene == DISPLAY_DEMO_FIXED_REPLAY) {
        stateBytes = GAME_DEBUG_FIXED_REPLAY_BASE;
    }
    printf(_gAttractDemoRestoreFormat, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area);

    // Consume one live record of each kind in the demo file's serialized order.
    memcpy(&gMcSaveData[MEMORY_CARD_SAVE_LIVE], stateBytes, sizeof(gMcSaveData[MEMORY_CARD_SAVE_LIVE]));
    stateBytes += sizeof(gMcSaveData[MEMORY_CARD_SAVE_LIVE]);

    // Restore the live player image; the serialized backup stays intact.
    memcpy(((u8(*)[PLAYER_STATUS_SAVE_RECORD_BYTES]) & gPlayerStatus)[bank], stateBytes, PLAYER_STATUS_SAVE_RECORD_BYTES);
    stateBytes += PLAYER_STATUS_SAVE_RECORD_BYTES;

    memcpy(&gGameFlagAcropolisBanks[bank], stateBytes, sizeof(gGameFlagAcropolisBanks[bank]));
    stateBytes += sizeof(gGameFlagAcropolisBanks[bank]);

    memcpy(gGameFlagDryfieldBanks, stateBytes, sizeof(gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE]));
    stateBytes += sizeof(gGameFlagDryfieldBanks[GAME_FLAG_STAGE_BANK_LIVE]);

    memcpy(gGameFlagDryfieldNightBanks, stateBytes, sizeof(gGameFlagDryfieldNightBanks[GAME_FLAG_STAGE_BANK_LIVE]));
    stateBytes += sizeof(gGameFlagDryfieldNightBanks[GAME_FLAG_STAGE_BANK_LIVE]);

    // Keep the byte stride's first shift before the bank-base address load.
    bankTimesEight       = bank * 8;
    mineShelterBankBytes = (u8*)gGameFlagMineShelterBanks;
    memcpy(mineShelterBankBytes + ((bankTimesEight - bank) * 8 + bank) * 4, stateBytes, sizeof(gGameFlagMineShelterBanks[bank]));
    stateBytes += sizeof(gGameFlagMineShelterBanks[bank]);

    memcpy(gGameFlagNeoArkBanks, stateBytes, sizeof(gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE]));
    stateBytes += sizeof(gGameFlagNeoArkBanks[GAME_FLAG_STAGE_BANK_LIVE]);

    memcpy(&gGameFlagNibbleBanks[bank], stateBytes, sizeof(gGameFlagNibbleBanks[bank]));

    // Keep the active replay selection and the user's vibration setting.
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene = savedDemoScene;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration = savedVibration;
    if (fsIsStageCdfAvailable(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage) != true) {
        gDisplayState.gameMode = DISPLAY_GAME_RESTART;
    }
    printf(_gAttractDemoRestoreFormat, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area);
}

/// Enables the title background strips and advances to the prompt state.
static void _showBackgroundTask(Task* task)
{
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    task->state++;
}

void titleScreenTask(Task* task)
{
    TaskFuncTable5 phases;

    phases               = _gScreenStateHandlers;
    _gScreenRandomSample = rand();
    phases.funcs[task->state](task);
}

void titleExitTask(Task* task)
{
    taskCallExit(task);
}

/// Queues the title background, palette and texture: the screen's file.
///
/// Uses normal loading with no image displacement. Requires the stage-zero
/// file table, available image workspace and space in the CD request ring.
/// The queue copies the selected key bytes and all four argument bytes during
/// this call; loading finishes asynchronously.
static inline void _enqueueBackgroundLoad(void)
{
    enum { SCREEN_FILE_ID = PE2_VALUE(screen, _id) };
    u8 fileKey[4];
    u8 fileArgs[sizeof(gCdCmdQueue.entries[0].args)];

    // Selector byte 1 is ignored; the hundreds component is argument byte 0.
    fileKey[3]  = 0;
    fileKey[2]  = FS_FILE_GROUP(SCREEN_FILE_ID);
    fileKey[0]  = FS_FILE_INDEX(SCREEN_FILE_ID);
    fileArgs[0] = FS_FILE_HUNDREDS(SCREEN_FILE_ID);
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
static void _introMovieTask(Task* task)
{
    enum {
        INTRO_PREPARE,
        INTRO_QUEUE_MOVIE,
        INTRO_WAIT_READY,
        INTRO_PLAYING,
        INTRO_QUEUE_BACKGROUND,
        INTRO_WAIT_BACKGROUND,
        INTRO_UPLOAD_BACKGROUND,
        INTRO_RESTORE_MEMORY,
    };
    u8              movieArgs[sizeof(gCdCmdQueue.entries[0].args)];
    GameLocationKey movieLocation;
    CdCmdQueue*     queue = &gCdCmdQueue;

    switch (task->state) {
        case INTRO_PREPARE:
            memCopyBytes(gFsStage0StreamSlots, gStreamSlots, sizeof(gFsStage0StreamSlots));
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state++;
            break;
        case INTRO_QUEUE_MOVIE:
            movieLocation = gGameSession->location;
            if (gGameMainPersistentState.discNumber == GAME_MAIN_DISC_2) {
                movieLocation.view = PE2_VALUE(file, streams, intro_disc_2, _id);
            } else {
                movieLocation.view = PE2_VALUE(file, streams, intro_disc_1, _id);
            }
            // Playback uses only the slot byte; the queue copies all four bytes.
            movieArgs[0] = streamFindMovieSlot(&movieLocation, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, movieArgs);
            task->state++;
            break;
        case INTRO_WAIT_READY:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case INTRO_PLAYING:
            if (cdCmdIsIdle()) {
                task->state++;
            } else if (padIsStartPressed()) {
                _gScreenSkipFade = false;
                SetDispMask(0);
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        // Wait for playback or cancellation to drain before reusing its storage.
        case INTRO_QUEUE_BACKGROUND:
            if (cdCmdIsIdle()) {
                gCdCmdQueue.preserveDisplayAfterDecode = 1;
                _enqueueBackgroundLoad();
                task->state++;
            }
            break;
        case INTRO_WAIT_BACKGROUND:
            if (cdCmdIsIdle()) {
                displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                task->state++;
            }
            break;
        case INTRO_UPLOAD_BACKGROUND:
            streamResetGameRestore();
            displayUploadBackgroundImage(gDisplayState.drawBuffer);
            displayUploadBackgroundImage(gDisplayState.drawBuffer ^ 1);
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
            task->state++;
            break;
        case INTRO_RESTORE_MEMORY:
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
static void _startupTask(Task* startupTask)
{
    enum {
        STARTUP_PREPARE,
        STARTUP_WAIT_FIRST_FRAME,
        STARTUP_WAIT_SECOND_FRAME,
        STARTUP_SPAWN_SCREEN,
        STARTUP_WAIT_SCREEN_FRAME,
        STARTUP_FINISH,
        STARTUP_QUEUE_BACKGROUND,
        STARTUP_WAIT_BACKGROUND,
        STARTUP_INTRO_TASK_INDEX = 1,
    };
    s32   nextState;
    Task* task;

    task = startupTask;
    switch (task->state) {
        case STARTUP_PREPARE:
            // Normal boot gives the movie exclusive presentation ownership.
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            _gScreenSkipFade                        = true;
            if ((gDisplayState.debugMode < 0) || (gGameMainPersistentState.skipTitleIntro != 0)) {
                nextState        = STARTUP_QUEUE_BACKGROUND;
                _gScreenSkipFade = false;
            } else {
                displaySpawnTaskFromTable(gTitleStartupTaskDescriptors, STARTUP_INTRO_TASK_INDEX, 0, 0);
                gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
                nextState                            = task->state + 1;
            }
            task->state = nextState;
            return;
        case STARTUP_WAIT_FIRST_FRAME:
        case STARTUP_WAIT_SECOND_FRAME:
            task->state = task->state + 1;
            return;
        case STARTUP_SPAWN_SCREEN:
            if (_gScreenSkipFade != 0) {
                taskSpawn(RESIDENT_TASK_BANK, SCREEN_TASK_SLOT, SCREEN_SKIP_BLACK_FADE, 0);
            } else {
                taskSpawn(RESIDENT_TASK_BANK, SCREEN_TASK_SLOT, 0, 0);
            }
            /* fallthrough */
        case STARTUP_WAIT_SCREEN_FRAME:
            task->state = task->state + 1;
            return;
        case STARTUP_FINISH:
            SetDispMask(1);
            gGameMainPersistentState.skipTitleIntro = 1;
            taskKill(task);
            return;
        case STARTUP_QUEUE_BACKGROUND:
            // Skipping the movie still requires its background and chrome load.
            _enqueueBackgroundLoad();
            task->state = task->state + 1;
            /* fallthrough */
        case STARTUP_WAIT_BACKGROUND:
            if (cdCmdIsIdle()) {
                task->state = STARTUP_SPAWN_SCREEN;
            }
            return;
    }
}

/// The first demo's file, from which the others are found by their index.
enum { DEMO_FIRST_FILE_ID = PE2_VALUE(demos, items, 0, _id) };

/// Main loads a numbered demo for scenes 1 up to its fixed replay, and the title numbers them from 1.
STATIC_ASSERT(PE2_VALUE(demos, _count) > 0 && PE2_VALUE(demos, _count) < DISPLAY_DEMO_FIXED_REPLAY, demo_count_fits_main);
/// A demo's file is the first's, as many hundreds on as its index: each at index 0, one group holding them all.
STATIC_ASSERT(PE2_VALUE(demos, _count) < 2 || PE2_VALUE(demos, _step) == 100, demos_are_a_hundred_apart);
STATIC_ASSERT(FS_FILE_INDEX(DEMO_FIRST_FILE_ID) == 0 && FS_FILE_HUNDREDS(DEMO_FIRST_FILE_ID) + PE2_VALUE(demos, _count) <= 100,
              demos_share_a_group);

void titleEnqueueAttractDemoFile(s32 demoIndex)
{
    enum { DEMO_FILE_KEY_SCRATCH_BYTES = 8 };
    u8  loadOptions[sizeof(gCdCmdQueue.entries[0].args)];
    u8* fileKey;

    fileKey                = SCRATCH_STACK_RESERVE_BYTES(DEMO_FILE_KEY_SCRATCH_BYTES);
    gGameSession->field_80 = 0;
    fileKey[3]             = 0;
    fileKey[2]             = FS_FILE_GROUP(DEMO_FIRST_FILE_ID);
    fileKey[0]             = 0;
    loadOptions[0]         = demoIndex + FS_FILE_HUNDREDS(DEMO_FIRST_FILE_ID);
    loadOptions[3]         = 0;
    loadOptions[2]         = 0;
    loadOptions[1]         = CD_COMMAND_LOAD_DEFAULT;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, loadOptions);
    // The queue owns a copy before this scratch reservation is released.
    SCRATCH_STACK_RELEASE_BYTES(DEMO_FILE_KEY_SCRATCH_BYTES);
}
