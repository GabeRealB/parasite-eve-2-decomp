#include "actor_143000_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/strings.h>

#include "common.h"

#include "gameplay/action_prompt.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/item_menu.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "../../shared/action_prompt.h"

extern u8 D_actor_143000_80135C0C;

/// Most characters the keypad's entry line holds.
#define ACTOR_143000_KEYPAD_CODE_CAPACITY 20

/// Banner stored in `_Actor143000KeypadWork::resultBanner`.
#define ACTOR_143000_KEYPAD_BANNER_NONE     0
#define ACTOR_143000_KEYPAD_BANNER_ACCEPTED 1
#define ACTOR_143000_KEYPAD_BANNER_REJECTED 2

/// States selected by the keypad task's eleven-entry callback table.
enum {
    ACTOR_143000_KEYPAD_STATE_INITIALIZE     = 0,
    ACTOR_143000_KEYPAD_STATE_ARM_CURSOR     = 1,
    ACTOR_143000_KEYPAD_STATE_INPUT          = 2,
    ACTOR_143000_KEYPAD_STATE_OPEN_COMMANDS  = 3,
    ACTOR_143000_KEYPAD_STATE_HANDLE_COMMAND = 4,
    ACTOR_143000_KEYPAD_STATE_CLOSE          = 5,
    ACTOR_143000_KEYPAD_STATE_RESUME_INPUT   = 6,
    ACTOR_143000_KEYPAD_STATE_CHECK_CODE     = 7,
    ACTOR_143000_KEYPAD_STATE_TYPE_KEY       = 8,
    ACTOR_143000_KEYPAD_STATE_AUTO_TYPE_CODE = 9,
    ACTOR_143000_KEYPAD_STATE_WAIT_FADE      = 10,
};

/// Choices in the keypad's hotspot table; several surround rectangles share one id.
enum {
    ACTOR_143000_KEYPAD_HOTSPOT_NONE     = 0,
    ACTOR_143000_KEYPAD_HOTSPOT_STATUS   = 1,
    ACTOR_143000_KEYPAD_HOTSPOT_SURROUND = 2,
    ACTOR_143000_KEYPAD_HOTSPOT_ENTER    = 3,
    ACTOR_143000_KEYPAD_HOTSPOT_ENTRY    = 4,
    ACTOR_143000_KEYPAD_HOTSPOT_KEYS     = 5,
};

/// Keypad atlas, screen grid and dog motion units, private to this drawer and input.
enum {
    ACTOR_143000_KEYPAD_TEXTURE_PAGE                 = 0x16,
    ACTOR_143000_KEYPAD_OT_SLOT                      = 0x3FE,
    ACTOR_143000_KEYPAD_CLUT_STATUS                  = 0x3DC0,
    ACTOR_143000_KEYPAD_CLUT_HIGHLIGHT               = 0x3DC1,
    ACTOR_143000_KEYPAD_CLUT_ACCEPTED                = 0x3DC3,
    ACTOR_143000_KEYPAD_CLUT_REJECTED                = 0x3DC4,
    ACTOR_143000_KEYPAD_CLUT_ENTRY                   = 0x3DC5,
    ACTOR_143000_KEYPAD_CLUT_CARET                   = 0x3DC6,
    ACTOR_143000_KEYPAD_CLUT_DOG                     = 0x3DC7,
    ACTOR_143000_KEYPAD_GRID_LEFT                    = -128,
    ACTOR_143000_KEYPAD_GRID_TOP                     = 32,
    ACTOR_143000_KEYPAD_KEY_PIXELS                   = 16,
    ACTOR_143000_KEYPAD_COLUMNS                      = 13,
    ACTOR_143000_KEYPAD_GRID_TEXTURE_TOP             = 0x70,
    ACTOR_143000_KEYPAD_ENTER_TEXTURE_LEFT           = 0x30,
    ACTOR_143000_KEYPAD_ENTER_TEXTURE_TOP            = 0xB8,
    ACTOR_143000_KEYPAD_ENTRY_TEXTURE_LEFT           = 0x58,
    ACTOR_143000_KEYPAD_CARET_TEXTURE_LEFT           = 0x60,
    ACTOR_143000_KEYPAD_ENTRY_TEXTURE_TOP            = 0xB8,
    ACTOR_143000_KEYPAD_BANNER_ACCEPTED_TEXTURE_LEFT = 0x70,
    ACTOR_143000_KEYPAD_BANNER_REJECTED_TEXTURE_LEFT = 0x30,
    ACTOR_143000_KEYPAD_BANNER_TEXTURE_TOP           = 0xA0,
    ACTOR_143000_KEYPAD_BANNER_WIDTH                 = 64,
    ACTOR_143000_KEYPAD_BANNER_HEIGHT                = 24,
    ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS           = 8,
    ACTOR_143000_KEYPAD_CARET_BLINK_BIT              = 0x10,
    ACTOR_143000_KEYPAD_STATUS_WIDTH                 = 254,
    ACTOR_143000_KEYPAD_STATUS_HEIGHT                = 16,
    ACTOR_143000_KEYPAD_DOG_FRACTION_BITS            = 4,
    ACTOR_143000_KEYPAD_DOG_FRAME_UNITS              = 16,
    ACTOR_143000_KEYPAD_DOG_TEXTURE_TOP              = 0xA0,
    ACTOR_143000_KEYPAD_DOG_WIDTH                    = 48,
    ACTOR_143000_KEYPAD_DOG_HEIGHT                   = 24,
    ACTOR_143000_KEYPAD_DOG_START_X                  = 160 << ACTOR_143000_KEYPAD_DOG_FRACTION_BITS,
    ACTOR_143000_KEYPAD_DOG_WRAP_X                   = -208 * (1 << ACTOR_143000_KEYPAD_DOG_FRACTION_BITS),
    ACTOR_143000_KEYPAD_DOG_IDLE_SPEED               = 1 << ACTOR_143000_KEYPAD_DOG_FRACTION_BITS,
    ACTOR_143000_KEYPAD_DOG_KEY_SPEED                = 3 << ACTOR_143000_KEYPAD_DOG_FRACTION_BITS,
    ACTOR_143000_KEYPAD_DOG_SPEED_DECAY              = 4,
    ACTOR_143000_KEYPAD_DEBUG_DEMO                   = 9,
};

/// Source origin of the stored 320-pixel image, in VRAM halfword coordinates.
enum {
    ACTOR_143000_CAPTURE_VRAM_X = 448,
    ACTOR_143000_CAPTURE_VRAM_Y = 256,
};

/// Reserves and initializes one opaque textured quad in the frame arena.
///
/// quad must be a side-effect-free, writable POLY_FT4* lvalue; it is evaluated
/// repeatedly. Captures gGpuPrimCursor, requiring word-aligned capacity for one
/// packet, and advances it without checking. Expands to standalone statements;
/// the caller still supplies geometry, UVs, CLUT, texture page and OT linkage.
#define ACTOR_143000_RESERVE_KEYPAD_QUAD(quad) \
    (quad)         = gGpuPrimCursor;           \
    gGpuPrimCursor = (quad) + 1;               \
    SetPolyFT4(quad);

/// Work block of the Shelter B2 laboratory's code keypad task.
///
/// The task shows the keypad full screen and lets the action cursor confirm
/// its hotspots. A confirmed hotspot is latched and offers its command at the
/// cursor; accepting that command on the key grid makes the grid type from
/// then on. Typed characters fill the entry line, and the Enter hotspot checks
/// it: `statusLine`, `statusWidth` and `resultBanner` step through the answer,
/// and an accepted code ends the task and spawns the one that follows.
typedef struct {
    byte                  field_0[2];      // Never read or written; role unproven
    u16                   selectedHotspot; // `ActionPromptHotspot::id` confirmed this frame, kept while its command prompt is open (0 none)
    s16                   field_4;         // Sends the task through a state that clears it when the command prompt closes unaccepted; nothing sets it, role unproven
    s8                    promptKind;      // `ActionPromptHotspot::promptKind` of that hotspot, forwarded when its command prompt opens
    s8                    keypadExamined;  // Whether the command was accepted on the key grid (0 a confirm there offers the command, 1 it types the key)
    ActionPromptCursorPos keyPress;        // Cursor position latched with a confirm on the key grid; selects the key typed
    s32                   codeAccepted;    // Result of the last check of the entry (0 rejected, 1 accepted); passed with the task's kill request when it ends
    s16                   codeLength;      // Characters on the entry line, at most ACTOR_143000_KEYPAD_CODE_CAPACITY
    s8                    statusLine;      // Status text drawn near the top of the screen, as its 1-based 16-pixel row in the texture (0 none)
    s8                    resultBanner;    // Banner drawn at the screen center (0 none, 1 code accepted, 2 code rejected)
    s16                   statusWidth;     // Pixels of the status line drawn, widened in steps while an answer is pending (0 the whole line)
    s16                   marqueeX;        // Left edge of the animated sprite crossing the top of the screen, in 1/16 pixel from the screen center
    s16                   marqueeSpeed;    // 1/16 pixels that sprite moves left each frame; a typed key raises it and it decays back to one pixel
    s16                   field_1A;        // Cleared when the task starts and never read; role unproven
} _Actor143000KeypadWork;
STATIC_ASSERT_SIZEOF(_Actor143000KeypadWork, 0x1C);

extern TaskDesc            D_actor_143000_80134558;
extern u8                  D_actor_143000_80134570[];
extern ActionPromptHotspot D_actor_143000_80134580[];
extern const char*         D_actor_143000_801345F8[3];

static void _actionPromptResetDefault(Task* task);
static void func_actor_143000_80132A04(Task* arg0);
static void _actor143000ArmKeypadCursor(Task* task);
static void _actor143000OpenKeypadCommands(Task* task);
static void func_actor_143000_801336E8(Task* arg0);
static void func_actor_143000_80133800(Task* arg0);
static void _actor143000ResumeKeypadInput(Task* task);
static void _actor143000TypeKeypadKey(Task* task);
static void _actor143000AutoTypeKeypadCode(Task* task);
static void _actor143000WaitForKeypadFade(Task* task);
static s32  _actor143000HitTestKeypadHotspots(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
static void _actor143000OutlineKeypadHotspots(void);

static void _actor143000KeypadCursorTask(Task* task);
static void _actor143000KeypadTask(Task* task);

extern const char D_actor_143000_80131E54[14];
extern const char D_actor_143000_80131E64[14];
extern const char D_actor_143000_80131E74[14];

TaskDesc D_actor_143000_80134558 = { { { TASK_BODY_NONE, 192 } }, _actor143000KeypadCursorTask, { .value = 0 } };

TaskDesc D_actor_143000_80134564 = { { { TASK_BODY_NONE, 32 } }, _actor143000KeypadTask, { .value = 0 } };

u8 D_actor_143000_80134570[16] = {
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
};

/// Action-cursor hotspots. `_actor143000HitTestKeypadHotspots` tests them; the last
/// entry is the end marker.
ActionPromptHotspot D_actor_143000_80134580[10] = {
    { -128, 32, 208, 48, 5, 0, 0 },
    { -133, 11, 266, 17, 4, 0, 0 },
    { -160, -120, 27, 240, 2, 0, 0 },
    { 133, -120, 27, 240, 2, 0, 0 },
    { -160, -120, 320, 48, 2, 0, 0 },
    { -160, -55, 320, 66, 2, 0, 0 },
    { -160, 84, 320, 36, 2, 0, 0 },
    { -120, -72, 266, 17, 1, 0, 0 },
    { 88, 64, 40, 16, 3, 1, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

const char* D_actor_143000_801345F8[3] = {
    D_actor_143000_80131E74,
    D_actor_143000_80131E64,
    D_actor_143000_80131E54,
};

static AnimationPackedPose _gActor143000Animation02A20Bank1[2] = {
#include "assets/actor_143000_animation_02A20_bank1.inc"
};

static AnimationPackedRotation _gActor143000Animation02A20Bank4[26] = {
#include "assets/actor_143000_animation_02A20_bank4.inc"
};

static AnimationRecord _gActor143000Animation02A20Records[101] = {
#include "assets/actor_143000_animation_02A20_records.inc"
};

static u16 _gActor143000Animation02A20Indices[20] = {
#include "assets/actor_143000_animation_02A20_indices.inc"
};

AnimationSet gActor143000Animation02A20 = {
    _gActor143000Animation02A20Records,
    _gActor143000Animation02A20Indices,
    { NULL, _gActor143000Animation02A20Bank1, NULL, NULL, _gActor143000Animation02A20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143000Animation02CCCBank1[2] = {
#include "assets/actor_143000_animation_02CCC_bank1.inc"
};

static AnimationPackedRotation _gActor143000Animation02CCCBank4[32] = {
#include "assets/actor_143000_animation_02CCC_bank4.inc"
};

static AnimationRecord _gActor143000Animation02CCCRecords[113] = {
#include "assets/actor_143000_animation_02CCC_records.inc"
};

static u16 _gActor143000Animation02CCCIndices[20] = {
#include "assets/actor_143000_animation_02CCC_indices.inc"
};

AnimationSet gActor143000Animation02CCC = {
    _gActor143000Animation02CCCRecords,
    _gActor143000Animation02CCCIndices,
    { NULL, _gActor143000Animation02CCCBank1, NULL, NULL, _gActor143000Animation02CCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143000Animation02EE8Bank1[2] = {
#include "assets/actor_143000_animation_02EE8_bank1.inc"
};

static AnimationPackedRotation _gActor143000Animation02EE8Bank4[25] = {
#include "assets/actor_143000_animation_02EE8_bank4.inc"
};

static AnimationRecord _gActor143000Animation02EE8Records[84] = {
#include "assets/actor_143000_animation_02EE8_records.inc"
};

static u16 _gActor143000Animation02EE8Indices[20] = {
#include "assets/actor_143000_animation_02EE8_indices.inc"
};

AnimationSet gActor143000Animation02EE8 = {
    _gActor143000Animation02EE8Records,
    _gActor143000Animation02EE8Indices,
    { NULL, _gActor143000Animation02EE8Bank1, NULL, NULL, _gActor143000Animation02EE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143000Animation03090Bank1[2] = {
#include "assets/actor_143000_animation_03090_bank1.inc"
};

static AnimationPackedRotation _gActor143000Animation03090Bank4[23] = {
#include "assets/actor_143000_animation_03090_bank4.inc"
};

static AnimationRecord _gActor143000Animation03090Records[57] = {
#include "assets/actor_143000_animation_03090_records.inc"
};

static u16 _gActor143000Animation03090Indices[20] = {
#include "assets/actor_143000_animation_03090_indices.inc"
};

AnimationSet gActor143000Animation03090 = {
    _gActor143000Animation03090Records,
    _gActor143000Animation03090Indices,
    { NULL, _gActor143000Animation03090Bank1, NULL, NULL, _gActor143000Animation03090Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143000Animation03248Bank1[2] = {
#include "assets/actor_143000_animation_03248_bank1.inc"
};

static AnimationPackedRotation _gActor143000Animation03248Bank4[27] = {
#include "assets/actor_143000_animation_03248_bank4.inc"
};

static AnimationRecord _gActor143000Animation03248Records[57] = {
#include "assets/actor_143000_animation_03248_records.inc"
};

static u16 _gActor143000Animation03248Indices[20] = {
#include "assets/actor_143000_animation_03248_indices.inc"
};

AnimationSet gActor143000Animation03248 = {
    _gActor143000Animation03248Records,
    _gActor143000Animation03248Indices,
    { NULL, _gActor143000Animation03248Bank1, NULL, NULL, _gActor143000Animation03248Bank4, NULL, NULL, NULL },
};

Actor143000CaptureArgs D_actor_143000_80135090 = { { 129, 39, 164, 90 }, 10, 0 };

Actor143000CaptureArgs D_actor_143000_801350A0 = { { 38, 138, 250, 75 }, 8, 0 };

static void func_actor_143000_801324C8(Task* arg0);
static void _actor143000HandleKeypadInput(Task* task);
static void _actor143000DrawKeypad(Task* task);

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

static void func_actor_143000_801324C8(Task* arg0)
{
    _Actor143000KeypadWork* work;
    ActionPromptHotspot*    p;
    u8                      temp_a0;

    p    = D_actor_143000_80134580;
    work = memCalloc(sizeof(_Actor143000KeypadWork), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->spawnArg2.pointer                                    = taskSpawnFromTable(&D_actor_143000_80134558, 0, 1, 0);
    arg0->work                                                 = work;
    temp_a0                                                    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xB;
    D_actor_143000_80135C0C                                    = temp_a0;
    arg0->state                                               += 1;
    work->field_4                                              = 0;
    displayAcquireMenuHold();
    // Clear hits left on the table before the prompt scan starts.
    if (p->id != ACTION_PROMPT_HOTSPOT_END) {
        do {
            p->hit = 0;
            p++;
        } while (p->id != ACTION_PROMPT_HOTSPOT_END);
    }
    work->keypadExamined       = 0;
    work->statusLine           = 1;
    work->resultBanner         = ACTOR_143000_KEYPAD_BANNER_NONE;
    work->marqueeX             = 0xA00;
    work->codeAccepted         = 0;
    work->codeLength           = 0;
    work->marqueeSpeed         = 0x10;
    work->field_1A             = 0;
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
}

/// Handles keypad cursor input and queues hovered-key highlights.
///
/// Requires the initialized keypad work block and gameplay's first prompt slot.
/// CAP playback hides and stops the cursor. Confirm either opens the hotspot's
/// command menu or latches a key after the key grid was examined; cancel closes.
static void _actor143000HandleKeypadInput(Task* task)
{
    enum {
        ACTOR_143000_KEYPAD_CONFIRM_SLOT = 0,
        ACTOR_143000_KEYPAD_CANCEL_SLOT  = 1,
    };
    _Actor143000KeypadWork* work;
    u8                      textureU;
    ActionPromptHotspot*    hotspot;
    POLY_FT4*               quad;
    ActionPrompt*           prompt;
    s16                     gridLeft;
    s16                     gridTop;
    s16                     cellX;
    s16                     cellY;
    s16                     width;
    s16                     height;
    u8                      textureV;
    u8                      textureWidth;
    u8                      textureHeight;

    // CAP playback owns input while a keypad description is on screen.
    work                           = task->work;
    gGameSession->hideHud          = 1;
    gGameSession->eventState       = 1;
    hotspot                        = D_actor_143000_80134580;
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
    prompt                         = D_80114D28;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == ACTOR_143000_KEYPAD_DEBUG_DEMO) {
        _actor143000OutlineKeypadHotspots();
    }
    // Confirm latches a hotspot command, or a grid cell after the keypad was examined.
    work->selectedHotspot = ACTOR_143000_KEYPAD_HOTSPOT_NONE;
    if (_actor143000HitTestKeypadHotspots(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[ACTOR_143000_KEYPAD_CONFIRM_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    if (work->keypadExamined != 0 && hotspot->id == ACTOR_143000_KEYPAD_HOTSPOT_KEYS) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_KEYPAD_KEY, 0, 0);
                        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                        work->keyPress.x    = prompt->screen.xy.x;
                        work->keyPress.y    = prompt->screen.xy.y;
                        task->state         = ACTOR_143000_KEYPAD_STATE_TYPE_KEY;
                        return;
                    }
                    prompt->mode          = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed   = ACTION_PROMPT_SPEED_STOPPED;
                    work->selectedHotspot = hotspot->id;
                    work->promptKind      = hotspot->promptKind;
                    task->state           = ACTOR_143000_KEYPAD_STATE_OPEN_COMMANDS;
                    return;
                }
            }
        }
        // Highlight the hovered key or Enter without changing the cursor position.
        for (hotspot = D_actor_143000_80134580; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
            if (hotspot->hit != 0) {
                if (hotspot->id != ACTOR_143000_KEYPAD_HOTSPOT_ENTER) {
                    if (hotspot->id == ACTOR_143000_KEYPAD_HOTSPOT_KEYS) {
                        ACTOR_143000_RESERVE_KEYPAD_QUAD(quad);
                        setShadeTex(quad, 1);
                        cellX    = (s16)(prompt->screen.xy.x - hotspot->x) / ACTOR_143000_KEYPAD_KEY_PIXELS * ACTOR_143000_KEYPAD_KEY_PIXELS;
                        cellY    = (s16)(prompt->screen.xy.y - hotspot->y) / ACTOR_143000_KEYPAD_KEY_PIXELS * ACTOR_143000_KEYPAD_KEY_PIXELS;
                        gridLeft = hotspot->x;
                        gridTop  = hotspot->y;
                        textureU = cellX;
                        textureV = cellY + ACTOR_143000_KEYPAD_GRID_TEXTURE_TOP;
                        cellX   += gridLeft;
                        cellY   += gridTop;
                        setXYWH(quad, cellX, cellY, ACTOR_143000_KEYPAD_KEY_PIXELS, ACTOR_143000_KEYPAD_KEY_PIXELS);
                        setUVWH(quad, textureU, textureV, ACTOR_143000_KEYPAD_KEY_PIXELS, ACTOR_143000_KEYPAD_KEY_PIXELS);
                        quad->tpage = ACTOR_143000_KEYPAD_TEXTURE_PAGE;
                        quad->clut  = ACTOR_143000_KEYPAD_CLUT_HIGHLIGHT;
                        addPrim(&gGpuCurrentOt[ACTOR_143000_KEYPAD_OT_SLOT], quad);
                    }
                } else {
                    ACTOR_143000_RESERVE_KEYPAD_QUAD(quad);
                    setShadeTex(quad, 1);
                    textureU      = ACTOR_143000_KEYPAD_ENTER_TEXTURE_LEFT;
                    cellX         = hotspot->x;
                    width         = hotspot->w;
                    cellY         = hotspot->y;
                    height        = hotspot->h;
                    textureWidth  = hotspot->w;
                    textureHeight = hotspot->h;
                    setXY4(quad, cellX, cellY, cellX + width, cellY, cellX, cellY + height, cellX + width, cellY + height);
                    setUV4(quad, textureU, ACTOR_143000_KEYPAD_ENTER_TEXTURE_TOP, textureWidth + ACTOR_143000_KEYPAD_ENTER_TEXTURE_LEFT, ACTOR_143000_KEYPAD_ENTER_TEXTURE_TOP, textureU, textureHeight + ACTOR_143000_KEYPAD_ENTER_TEXTURE_TOP - 0x100, textureWidth + ACTOR_143000_KEYPAD_ENTER_TEXTURE_LEFT, textureHeight + ACTOR_143000_KEYPAD_ENTER_TEXTURE_TOP - 0x100);
                    quad->tpage = ACTOR_143000_KEYPAD_TEXTURE_PAGE;
                    quad->clut  = ACTOR_143000_KEYPAD_CLUT_HIGHLIGHT;
                    addPrim(&gGpuCurrentOt[ACTOR_143000_KEYPAD_OT_SLOT], quad);
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[ACTOR_143000_KEYPAD_CANCEL_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = ACTOR_143000_KEYPAD_STATE_CLOSE;
    }
}

/// The three rows of the code keypad, bottom row first;
/// `D_actor_143000_801345F8` lists them top row first.
const char D_actor_143000_80131E54[] = "NOPQRSTUVWXYZ";
const char D_actor_143000_80131E64[] = "ABCDEFGHIJKLM";
const char D_actor_143000_80131E74[] = "0123456789-# ";

/// State table of the actor's callback, `_actor143000KeypadTask`, which
/// copies it onto its stack and indexes it with `Task::state`.
static const TaskFuncTable11 D_actor_143000_80131E84 = { {
    func_actor_143000_801324C8,
    _actor143000ArmKeypadCursor,
    _actor143000HandleKeypadInput,
    _actor143000OpenKeypadCommands,
    func_actor_143000_801336E8,
    func_actor_143000_80133800,
    _actor143000ResumeKeypadInput,
    func_actor_143000_80132A04,
    _actor143000TypeKeypadKey,
    _actor143000AutoTypeKeypadCode,
    _actor143000WaitForKeypadFade,
} };

/// The codes `func_actor_143000_80132A04` accepts; the second only while
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene` is non-zero.
static const char D_actor_143000_80131EB0[] = "A3EILM2S2Y";
static const char D_actor_143000_80131EBC[] = "YSD";

static void func_actor_143000_80132A04(Task* arg0)
{
    _Actor143000KeypadWork* work;

    work = arg0->work;
    if (arg0->killCountdown == 0) {
        s32 var_s2 = 0;

        if ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EB0) == 0) || ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EBC) == 0) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0))) {
            var_s2 = 1;
        }
        work->codeAccepted = var_s2;
    }
    if (work->codeAccepted != 0) {
        switch (arg0->killCountdown) {
            case 0:
                work->statusLine = 2;
                break;
            case 0x3C:
                work->statusLine  = 3;
                work->statusWidth = 0x7C;
                break;
            case 0x46:
                work->statusWidth = 0x83;
                break;
            case 0x50:
                work->statusWidth = 0x8A;
                break;
            case 0x5A:
                work->statusWidth = 0;
                break;
            case 0x78:
                sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_KEYPAD_CODE_ACCEPTED, 0, 0);
                work->resultBanner = ACTOR_143000_KEYPAD_BANNER_ACCEPTED;
                break;
            case 0x96:
                work->statusLine   = 4;
                work->resultBanner = ACTOR_143000_KEYPAD_BANNER_NONE;
                break;
            case 0xF0:
                work->statusLine = 5;
                break;
            case 0x14A:
                arg0->state = 0xA;
                // Fade to black, and stay in the closing state for as long as the ramp takes.
                D_actor_143000_80135C08.blend      = SCREEN_FADE_SUBTRACT;
                D_actor_143000_80135C08.phase      = SCREEN_FADE_RUNNING;
                D_actor_143000_80135C08.rampFrames = 0xF;
                arg0->killCountdown                = 0xF;
                taskSpawn(1, 0x31, 0, &D_actor_143000_80135C08);
                break;
        }
    } else {
        switch (arg0->killCountdown) {
            case 0:
                work->statusLine = 2;
                break;
            case 0x3C:
                work->statusLine  = 3;
                work->statusWidth = 0x7C;
                break;
            case 0x46:
                work->statusWidth = 0x83;
                break;
            case 0x50:
                work->statusWidth = 0x8A;
                break;
            case 0x5A:
                work->statusWidth = 0;
                break;
            case 0x78:
                sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_KEYPAD_CODE_REJECTED, 0, 0);
                work->resultBanner = ACTOR_143000_KEYPAD_BANNER_REJECTED;
                break;
            case 0x96:
                work->statusLine   = 6;
                work->statusWidth  = 0x7C;
                work->resultBanner = ACTOR_143000_KEYPAD_BANNER_NONE;
                break;
            case 0xA0:
                work->statusWidth = 0x83;
                break;
            case 0xAA:
                work->statusWidth = 0x8A;
                break;
            case 0xB4:
                work->statusWidth = 0;
                break;
            case 0xD2:
                work->resultBanner = ACTOR_143000_KEYPAD_BANNER_REJECTED;
                break;
            case 0xF0:
                work->statusLine   = 7;
                work->resultBanner = ACTOR_143000_KEYPAD_BANNER_NONE;
                break;
            case 0x14A:
                work->statusLine = 1;
                work->codeLength = 0;
                arg0->state      = 2;
                break;
        }
    }
    arg0->killCountdown = (s16)((u16)arg0->killCountdown + 1);
}

/// Draws the keypad entry, status, result banner and animated dog each frame.
///
/// Requires initialized keypad work with codeLength in [0, ACTOR_143000_KEYPAD_CODE_CAPACITY] and
/// a writable entry buffer including its terminator. Borrows free frame-arena
/// packets and ordering-table slot 1022; the caller must provide capacity.
/// Coordinates are center-origin pixels; the dog's position and speed have
/// four fractional bits. The atlas and its CLUTs must be loaded for view 11.
static void _actor143000DrawKeypad(Task* task)
{
    _Actor143000KeypadWork* work;
    POLY_FT4*               quad;
    s32                     characterIndex;
    s16                     entryY;
    s16                     edgeX;
    s16                     left;
    u8                      textureRow;
    s16                     top;
    s16                     edgeY;
    u8                      textureU;
    u8                      textureV;
    u8                      bottomV;
    u8                      rightU;
    s16                     clut;

    // Keep the entry terminated before another state checks or copies it.
    edgeX                                     = -0x48;
    work                                      = task->work;
    D_actor_143000_80135C20[work->codeLength] = '\0';
    D_actor_143000_80135C00++;
    entryY = 0x10;
    // Draw one mask glyph per character and a blinking insertion caret.
    for (characterIndex = 0; characterIndex < work->codeLength; characterIndex++) {
        edgeY    = entryY + ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS;
        textureU = ACTOR_143000_KEYPAD_ENTRY_TEXTURE_LEFT;
        textureV = ACTOR_143000_KEYPAD_ENTRY_TEXTURE_TOP;
        rightU   = textureU + ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS;
        bottomV  = textureV + ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS;
        ACTOR_143000_RESERVE_KEYPAD_QUAD(quad);
        setXY4(quad, edgeX, entryY, edgeX + ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS, entryY, edgeX, edgeY, edgeX + ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS, edgeY);
        setUV4(quad, textureU, textureV, rightU, textureV, textureU, bottomV, rightU, bottomV);
        quad->tpage = ACTOR_143000_KEYPAD_TEXTURE_PAGE;
        quad->clut  = ACTOR_143000_KEYPAD_CLUT_ENTRY;
        setShadeTex(quad, 1);
        addPrim(&gGpuCurrentOt[ACTOR_143000_KEYPAD_OT_SLOT], quad);
        edgeX += ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS;
    }
    if (work->codeLength != ACTOR_143000_KEYPAD_CODE_CAPACITY && task->state != ACTOR_143000_KEYPAD_STATE_CHECK_CODE) {
        textureU = ACTOR_143000_KEYPAD_CARET_TEXTURE_LEFT;
        textureV = ACTOR_143000_KEYPAD_ENTRY_TEXTURE_TOP;
        ACTOR_143000_RESERVE_KEYPAD_QUAD(quad);
        setXYWH(quad, edgeX, entryY, ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS, ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS);
        setUVWH(quad, textureU, textureV, ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS, ACTOR_143000_KEYPAD_ENTRY_GLYPH_PIXELS);
        quad->tpage = ACTOR_143000_KEYPAD_TEXTURE_PAGE;
        quad->clut  = ACTOR_143000_KEYPAD_CLUT_CARET;
        setShadeTex(quad, 1);
        if (D_actor_143000_80135C00 & ACTOR_143000_KEYPAD_CARET_BLINK_BIT) {
            addPrim(&gGpuCurrentOt[ACTOR_143000_KEYPAD_OT_SLOT], quad);
        }
    }
    // Status rows use the atlas text; a nonzero width reveals its prefix.
    if (work->statusLine != 0) {
        edgeY = ACTOR_143000_KEYPAD_STATUS_WIDTH;
        if (work->statusWidth != 0) {
            edgeY = work->statusWidth;
        }
        left       = -0x78;
        top        = -0x48;
        textureRow = (work->statusLine - 1) * ACTOR_143000_KEYPAD_STATUS_HEIGHT;
        ACTOR_143000_RESERVE_KEYPAD_QUAD(quad);
        setXYWH(quad, left, top, edgeY, ACTOR_143000_KEYPAD_STATUS_HEIGHT);
        setUVWH(quad, 0, textureRow, edgeY, ACTOR_143000_KEYPAD_STATUS_HEIGHT);
        quad->tpage = ACTOR_143000_KEYPAD_TEXTURE_PAGE;
        quad->clut  = ACTOR_143000_KEYPAD_CLUT_STATUS;
        setShadeTex(quad, 1);
        addPrim(&gGpuCurrentOt[ACTOR_143000_KEYPAD_OT_SLOT], quad);
    }
    if (work->resultBanner != ACTOR_143000_KEYPAD_BANNER_NONE) {
        top = left = -0x28;
        edgeX      = 0x18;
        edgeY      = -0x10;
        if (work->resultBanner == ACTOR_143000_KEYPAD_BANNER_ACCEPTED) {
            textureU = ACTOR_143000_KEYPAD_BANNER_ACCEPTED_TEXTURE_LEFT;
            textureV = ACTOR_143000_KEYPAD_BANNER_TEXTURE_TOP;
            clut     = ACTOR_143000_KEYPAD_CLUT_ACCEPTED;
        } else {
            textureU = ACTOR_143000_KEYPAD_BANNER_REJECTED_TEXTURE_LEFT;
            textureV = ACTOR_143000_KEYPAD_BANNER_TEXTURE_TOP;
            clut     = ACTOR_143000_KEYPAD_CLUT_REJECTED;
        }
        ACTOR_143000_RESERVE_KEYPAD_QUAD(quad);
        setXY4(quad, left, top, edgeX, top, left, edgeY, edgeX, edgeY);
        setUVWH(quad, textureU, textureV, ACTOR_143000_KEYPAD_BANNER_WIDTH, ACTOR_143000_KEYPAD_BANNER_HEIGHT);
        quad->tpage = ACTOR_143000_KEYPAD_TEXTURE_PAGE;
        quad->clut  = clut;
        setShadeTex(quad, 1);
        addPrim(&gGpuCurrentOt[ACTOR_143000_KEYPAD_OT_SLOT], quad);
    }
    // Advance the three-frame dog and its 1/16-pixel position.
    top        = -0x60;
    left       = work->marqueeX >> ACTOR_143000_KEYPAD_DOG_FRACTION_BITS;
    edgeY      = top + ACTOR_143000_KEYPAD_DOG_HEIGHT;
    edgeX      = left + ACTOR_143000_KEYPAD_DOG_WIDTH;
    textureRow = D_actor_143000_80134570[(D_actor_143000_80135C04 / ACTOR_143000_KEYPAD_DOG_FRAME_UNITS) % (s32)ARRAY_SIZE(D_actor_143000_80134570)] * ACTOR_143000_KEYPAD_DOG_HEIGHT + ACTOR_143000_KEYPAD_DOG_TEXTURE_TOP - 0x100;
    bottomV    = textureRow + ACTOR_143000_KEYPAD_DOG_HEIGHT;
    clut       = ACTOR_143000_KEYPAD_CLUT_DOG;
    ACTOR_143000_RESERVE_KEYPAD_QUAD(quad);
    setXY4(quad, left, top, edgeX, top, left, edgeY, edgeX, edgeY);
    setUV4(quad, 0, textureRow, ACTOR_143000_KEYPAD_DOG_WIDTH, textureRow, 0, bottomV, ACTOR_143000_KEYPAD_DOG_WIDTH, bottomV);
    quad->tpage = ACTOR_143000_KEYPAD_TEXTURE_PAGE;
    quad->clut  = clut;
    setShadeTex(quad, 1);
    addPrim(&gGpuCurrentOt[ACTOR_143000_KEYPAD_OT_SLOT], quad);
    D_actor_143000_80135C04 += work->marqueeSpeed;
    if (task->state != ACTOR_143000_KEYPAD_STATE_CHECK_CODE && task->state != ACTOR_143000_KEYPAD_STATE_WAIT_FADE) {
        work->marqueeX -= work->marqueeSpeed;
        if (work->marqueeX < ACTOR_143000_KEYPAD_DOG_WRAP_X) {
            work->marqueeX = ACTOR_143000_KEYPAD_DOG_START_X;
        }
    }
    work->marqueeSpeed -= ACTOR_143000_KEYPAD_DOG_SPEED_DECAY;
    if (work->marqueeSpeed < ACTOR_143000_KEYPAD_DOG_IDLE_SPEED) {
        work->marqueeSpeed = ACTOR_143000_KEYPAD_DOG_IDLE_SPEED;
    }
}

#undef ACTOR_143000_RESERVE_KEYPAD_QUAD

#include "../../shared/action_prompt_outline_rect.inc.c"

/// Resets the keypad's action cursors, then updates them on each task tick.
///
/// Task state must be 0 (reset both ports) or 1 (move the selected port).
/// The keypad spawns this companion task with spawnArg1 = 1, selecting port 0.
static void _actor143000KeypadCursorTask(Task* task)
{
    TaskFunc states[] = {
        _actionPromptResetDefault,
        _actionPromptMoveCursorsDefault,
    };

    states[task->state](task);
}

/// Dispatches the laboratory keypad's state and draws its current presentation.
///
/// Task state indexes the eleven-entry keypad table. Initialization allocates
/// the work block and the companion cursor task; closing releases both tasks.
/// Drawing follows the state callback, including on the closing tick.
static void _actor143000KeypadTask(Task* task)
{
    TaskFuncTable11 states;

    states = D_actor_143000_80131E84;
    states.funcs[task->state](task);
    _actor143000DrawKeypad(task);
}

/// Arms the first keypad cursor at the screen center and advances to input.
///
/// Sets the published pixel position; the companion cursor task owns its
/// fixed-point position. Requires the initialized first action-prompt slot.
static void _actor143000ArmKeypadCursor(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Opens the latched keypad hotspot's commands at the cursor position.
///
/// Requires initialized work and promptKind latched by the input state.
/// Hides and stops the cursor, then advances to command-result handling.
static void _actor143000OpenKeypadCommands(Task* task)
{
    ActionPrompt*           prompt = D_80114D28;
    _Actor143000KeypadWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = ACTOR_143000_KEYPAD_STATE_HANDLE_COMMAND;
}

static void func_actor_143000_801336E8(Task* arg0)
{
    _Actor143000KeypadWork* work   = arg0->work;
    ActionPrompt*           prompt = D_80114D28;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        switch ((s16)(work->selectedHotspot - 1)) {
            case 0:
                capRunCommand(8, CAP_PLAYBACK_IN_PLACE);
                break;
            case 1:
                capRunCommand(7, CAP_PLAYBACK_IN_PLACE);
                break;
            case 3:
                capRunCommand(9, CAP_PLAYBACK_IN_PLACE);
                break;
            case 2:
                sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_KEYPAD_ENTER, 0, 0);
                arg0->state         = 7;
                arg0->killCountdown = 0;
                return;
            case 4:
                work->keypadExamined = 1;
                capRunCommand(0xA, CAP_PLAYBACK_IN_PLACE);
                if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) == 1) {
                    arg0->killCountdown = 0xA;
                    arg0->state         = 9;
                    return;
                }
                break;
            default:
                break;
        }
        arg0->state = 2;
    } else if (work->field_4 != 0) {
        arg0->state = 6;
    } else {
        arg0->state = 2;
    }
}

static void func_actor_143000_80133800(Task* arg0)
{
    _Actor143000KeypadWork* work = arg0->work;

    displayReleaseMenuHold();
    gGameSession->cutsceneHold = 0;
    if (work->codeAccepted == 0) {
        D_80114D08                                                 = 0xA;
        gGameSession->eventState                                   = 0;
        gGameSession->hideHud                                      = 0;
        gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_RUNNING;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_actor_143000_80135C0C;
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    } else {
        taskSpawnFromTable(D_actor_143000_801350B0, 1, 0, &D_actor_143000_80135C08);
    }
    taskKill(arg0->spawnArg2.pointer);
    taskRequestKill(arg0, work->codeAccepted);
}

/// Returns the keypad to cursor input after clearing its unused work value.
///
/// The cleared value's wider role is unproven; this package never sets it nonzero.
static void _actor143000ResumeKeypadInput(Task* task)
{
    _Actor143000KeypadWork* work = task->work;

    work->field_4 = 0;
    task->state   = ACTOR_143000_KEYPAD_STATE_INPUT;
}

/// Applies the key at the latched cursor position, then resumes keypad input.
///
/// The screen grid has three 13-key rows of 16-pixel cells. '#' clears the
/// entry and '-' deletes its last character; other keys append up to capacity.
/// A valid key also accelerates the dog. Division truncates toward zero, so
/// callers must latch a position inside the grid rather than use this as a hit test.
static void _actor143000TypeKeypadKey(Task* task)
{
    enum {
        ACTOR_143000_KEYPAD_CLEAR_KEY     = '#',
        ACTOR_143000_KEYPAD_BACKSPACE_KEY = '-',
    };
    _Actor143000KeypadWork* work   = task->work;
    s32                     column = (work->keyPress.x - ACTOR_143000_KEYPAD_GRID_LEFT) / ACTOR_143000_KEYPAD_KEY_PIXELS;
    s32                     row    = (work->keyPress.y - ACTOR_143000_KEYPAD_GRID_TOP) / ACTOR_143000_KEYPAD_KEY_PIXELS;
    const char*             keyCharacter;

    if ((u32)column < ACTOR_143000_KEYPAD_COLUMNS) {
        if (row >= 0) {
            if (row < (s32)ARRAY_SIZE(D_actor_143000_801345F8)) {
                keyCharacter = D_actor_143000_801345F8[row] + column;
                if ((s8)*keyCharacter == ACTOR_143000_KEYPAD_CLEAR_KEY) {
                    work->codeLength = 0;
                } else if ((s8)*keyCharacter == ACTOR_143000_KEYPAD_BACKSPACE_KEY) {
                    if (work->codeLength > 0) {
                        work->codeLength--;
                    }
                } else if (work->codeLength < ACTOR_143000_KEYPAD_CODE_CAPACITY) {
                    D_actor_143000_80135C20[work->codeLength] = *keyCharacter;
                    work->codeLength++;
                }
                work->marqueeSpeed = ACTOR_143000_KEYPAD_DOG_KEY_SPEED;
            }
        }
    }
    task->state = ACTOR_143000_KEYPAD_STATE_INPUT;
}

/// Reveals the accepted keypad code one character at a time after CAP playback.
///
/// Requires a caption-triggered entry with codeLength initially zero and a
/// positive tick countdown. Each character takes another 8..15 task ticks;
/// drawing terminates the copied code at the revealed length. Input resumes
/// when all ten characters have been revealed.
static void _actor143000AutoTypeKeypadCode(Task* task)
{
    enum {
        ACTOR_143000_KEYPAD_AUTO_TYPE_MIN_FRAMES    = 8,
        ACTOR_143000_KEYPAD_AUTO_TYPE_RANDOM_FRAMES = 8,
        ACTOR_143000_RANDOM_FRACTION_BITS           = 15,
    };
    _Actor143000KeypadWork* work = task->work;
    u32                     remainingFrames;
    u32                     revealedLength;

    if (capIsBusy() == 0) {
        remainingFrames     = (u16)task->killCountdown - 1;
        task->killCountdown = remainingFrames;
        if ((s16)remainingFrames <= 0) {
            task->killCountdown = (rand() * ACTOR_143000_KEYPAD_AUTO_TYPE_RANDOM_FRAMES >> ACTOR_143000_RANDOM_FRACTION_BITS) + ACTOR_143000_KEYPAD_AUTO_TYPE_MIN_FRAMES;
            work->codeLength++;
            sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_KEYPAD_KEY, 0, 0);
            // Copy the whole code; the drawer reveals only codeLength characters.
            memcpy(D_actor_143000_80135C20, D_actor_143000_80131EB0, sizeof(D_actor_143000_80131EB0));
            revealedLength = work->codeLength;
            if (revealedLength >= sizeof(D_actor_143000_80131EB0) - 1) {
                task->state = ACTOR_143000_KEYPAD_STATE_INPUT;
            }
        }
    }
}

/// Counts down the keypad's fade-to-black wait, then selects its closing state.
///
/// killCountdown is the remaining task ticks, seeded with the fade's ramp length.
/// Unsigned decrement and signed comparison preserve the stored 16-bit countdown.
static void _actor143000WaitForKeypadFade(Task* task)
{
    u16 remainingFrames = (u16)task->killCountdown - 1;

    task->killCountdown = remainingFrames;
    if ((s16)remainingFrames <= 0) {
        task->state = ACTOR_143000_KEYPAD_STATE_CLOSE;
    }
}

/// Updates every keypad hotspot's hit flag and returns the first hit id, or zero.
///
/// Requires writable entries ending at ACTION_PROMPT_HOTSPOT_END. Cursor and
/// rectangles use signed center-origin pixels; right and bottom edges are
/// excluded and overlapping hits are all marked. Choices must be the keypad's
/// nonzero ids (1..5). The sentinel is untouched.
/// Demo 9 additionally outlines hits in black and misses in red.
static s32 _actor143000HitTestKeypadHotspots(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY)
{
    s32 firstHitId = ACTOR_143000_KEYPAD_HOTSPOT_NONE;

    if (hotspots->id != ACTION_PROMPT_HOTSPOT_END) {
        do {
            if (cursorX >= hotspots->x && cursorX < hotspots->x + hotspots->w && cursorY >= hotspots->y && cursorY < hotspots->y + hotspots->h) {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == ACTOR_143000_KEYPAD_DEBUG_DEMO) {
                    _actionPromptOutlineRectDefault(hotspots, 0, 0, 0);
                }
                hotspots->hit = 1;
                if (firstHitId == ACTOR_143000_KEYPAD_HOTSPOT_NONE) {
                    firstHitId = hotspots->id;
                }
            } else {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == ACTOR_143000_KEYPAD_DEBUG_DEMO) {
                    _actionPromptOutlineRectDefault(hotspots, 0xFF, 0, 0);
                }
                hotspots->hit = 0;
            }
            hotspots++;
        } while (hotspots->id != ACTION_PROMPT_HOTSPOT_END);
    }
    return firstHitId;
}

/// Queues green outlines for every keypad hotspot except the end marker.
///
/// Used by demo 9 before the hit test adds its black/red outlines. Requires
/// the loaded hotspot table and four free LINE_F2 packets per non-sentinel entry.
static void _actor143000OutlineKeypadHotspots(void)
{
    ActionPromptHotspot* hotspot = D_actor_143000_80134580;

    if (hotspot->id != ACTION_PROMPT_HOTSPOT_END) {
        do {
            _actionPromptOutlineRectDefault(hotspot, 0, 0xFF, 0);
            hotspot++;
        } while (hotspot->id != ACTION_PROMPT_HOTSPOT_END);
    }
}

#include "../../shared/action_prompt_reset.inc.c"

/// Captures full-width rows of the stored background into the resident workspace.
///
/// Borrows strip for this call; only its y and h select the source rows.
/// imageByteOffset is a word-aligned byte offset into Fs_ImgBuffers and the
/// complete 320-pixel strip must fit there. The source starts at VRAM (448,256),
/// in RGB555 halfwords. The caller owns workspace reuse and GPU synchronization.
static inline void _actor143000StoreStrip(const RECT* strip, s32 imageByteOffset)
{
    RECT sourceRect = *strip;

    sourceRect.x  = ACTOR_143000_CAPTURE_VRAM_X;
    sourceRect.w  = FILE_SYSTEM_IMAGE_WIDTH;
    sourceRect.y += ACTOR_143000_CAPTURE_VRAM_Y;
    StoreImage(&sourceRect, (u_long*)((u8*)Fs_ImgBuffers + imageByteOffset));
}

void actor143000CaptureStripTask(Task* task)
{
    enum {
        ACTOR_143000_CAPTURE_VIEW             = 14,
        ACTOR_143000_CAPTURE_INTERVAL_FRAMES  = 6,
        ACTOR_143000_CAPTURE_STATE_INITIALIZE = 0,
        ACTOR_143000_CAPTURE_STATE_STORE      = 1,
    };
    Actor143000CaptureArgs* capture = task->spawnArg2.pointer;
    RECT                    strip;
    s32                     stripBottom;
    s32                     imageByteOffset;

    if (gGameSession->location.loc.view != ACTOR_143000_CAPTURE_VIEW) {
        taskKill(task);
        return;
    }
    switch (task->state) {
        case ACTOR_143000_CAPTURE_STATE_INITIALIZE:
            task->killCountdown     = ACTOR_143000_CAPTURE_INTERVAL_FRAMES;
            capture->stripsCaptured = 0;
            task->state++;
            break;
        case ACTOR_143000_CAPTURE_STATE_STORE:
            if (--task->killCountdown > 0) {
                break;
            }
            task->killCountdown = ACTOR_143000_CAPTURE_INTERVAL_FRAMES;
            strip.x             = capture->band.x;
            strip.w             = capture->band.w;
            strip.y             = capture->band.y + capture->band.h * capture->stripsCaptured / capture->stripCount;
            capture->stripsCaptured++;
            stripBottom     = capture->band.y + capture->band.h * capture->stripsCaptured / capture->stripCount;
            imageByteOffset = strip.y * FILE_SYSTEM_IMAGE_ROW_BYTES;
            strip.h         = stripBottom - strip.y;
            // Retained compiler dependency between the row offset and rectangle copy.
            __asm__("" : "+m"(strip) : "r"(imageByteOffset));
            _actor143000StoreStrip(&strip, imageByteOffset);
            if (capture->stripsCaptured >= capture->stripCount) {
                taskKill(task);
            }
            break;
    }
}
