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

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_actor_143000_80135C0C[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_actor_143000_80135C0C_value __asm__("D_actor_143000_80135C0C");

/// Most characters the keypad's entry line holds.
#define ACTOR_143000_KEYPAD_CODE_CAPACITY 20

/// Banner stored in `_Actor143000KeypadWork::resultBanner`.
#define ACTOR_143000_KEYPAD_BANNER_NONE     0
#define ACTOR_143000_KEYPAD_BANNER_ACCEPTED 1
#define ACTOR_143000_KEYPAD_BANNER_REJECTED 2

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

static void func_actor_143000_80132A04(Task* arg0);
static void func_actor_143000_80133664(Task* task);
static void func_actor_143000_80133698(Task* task);
static void func_actor_143000_801336E8(Task* arg0);
static void func_actor_143000_80133800(Task* arg0);
static void func_actor_143000_801338C8(Task* arg0);
static void func_actor_143000_801338E0(Task* arg0);
static void func_actor_143000_801339CC(Task* arg0);
static void func_actor_143000_80133AC0(Task* arg0);
static s32  func_actor_143000_80133AE8(ActionPromptHotspot* p, s16 x, s16 y);
static void func_actor_143000_80133C2C(void);

void func_actor_143000_80133578(Task*);
void func_actor_143000_801335C8(Task*);

extern const char D_actor_143000_80131E54[14];
extern const char D_actor_143000_80131E64[14];
extern const char D_actor_143000_80131E74[14];

TaskDesc D_actor_143000_80134558 = { { { TASK_BODY_NONE, 192 } }, func_actor_143000_80133578, { .value = 0 } };

TaskDesc D_actor_143000_80134564 = { { { TASK_BODY_NONE, 32 } }, func_actor_143000_801335C8, { .value = 0 } };

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

/// Action-cursor hotspots. `func_actor_143000_80133AE8` tests them; the last
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
static void func_actor_143000_801325F0(Task* arg0);
static void func_actor_143000_80132D10(Task* arg0);

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
    D_actor_143000_80135C0C_value                              = temp_a0;
    arg0->state                                               += 1;
    work->field_4                                              = 0;
    Display_AcquireRef();
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
    Gp_MsgPlayerWeapon(0);
    Gp_MsgPlayer3F3(0);
}

static void func_actor_143000_801325F0(Task* arg0)
{
    _Actor143000KeypadWork* work;
    u8                      u;
    ActionPromptHotspot*    p;
    POLY_FT4*               prim;
    ActionPrompt*           prompt;
    s16                     dx;
    s16                     dy;
    s16                     x;
    s16                     y;
    s16                     w;
    s16                     h;
    u8                      v;
    u8                      uw;
    u8                      vh;

    work                           = arg0->work;
    gGameSession->hideHud          = 1;
    gGameSession->eventState       = 1;
    p                              = D_actor_143000_80134580;
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
    prompt                         = D_80114D28;
    if (Gp_CapBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 9) {
        func_actor_143000_80133C2C();
    }
    work->selectedHotspot = 0;
    if (func_actor_143000_80133AE8(p, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; p->id != ACTION_PROMPT_HOTSPOT_END; p++) {
                if (p->hit != 0) {
                    if (work->keypadExamined != 0 && p->id == 5) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_KEYPAD_KEY, 0, 0);
                        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                        work->keyPress.x    = prompt->screen.xy.x;
                        work->keyPress.y    = prompt->screen.xy.y;
                        arg0->state         = 8;
                        return;
                    }
                    prompt->mode          = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed   = ACTION_PROMPT_SPEED_STOPPED;
                    work->selectedHotspot = p->id;
                    work->promptKind      = p->promptKind;
                    arg0->state           = 3;
                    return;
                }
            }
        }
        for (p = D_actor_143000_80134580; p->id != ACTION_PROMPT_HOTSPOT_END; p++) {
            if (p->hit != 0) {
                if (p->id != 3) {
                    if (p->id == 5) {
                        prim           = gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        SetPolyFT4(prim);
                        setShadeTex(prim, 1);
                        x  = (s16)(prompt->screen.xy.x - p->x) / 16 * 16;
                        y  = (s16)(prompt->screen.xy.y - p->y) / 16 * 16;
                        dx = p->x;
                        dy = p->y;
                        u  = x;
                        v  = y + 0x70;
                        x += dx;
                        y += dy;
                        setXYWH(prim, x, y, 16, 16);
                        setUVWH(prim, u, v, 16, 16);
                        prim->tpage = 0x16;
                        prim->clut  = 0x3DC1;
                        addPrim(&gGpuCurrentOt[0x3FE], prim);
                    }
                } else {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    SetPolyFT4(prim);
                    setShadeTex(prim, 1);
                    u  = 0x30;
                    x  = p->x;
                    w  = p->w;
                    y  = p->y;
                    h  = p->h;
                    uw = p->w;
                    vh = p->h;
                    setXY4(prim, x, y, x + w, y, x, y + h, x + w, y + h);
                    setUV4(prim, u, 0xB8, uw + 0x30, 0xB8, u, vh - 0x48, uw + 0x30, vh - 0x48);
                    prim->tpage = 0x16;
                    prim->clut  = 0x3DC1;
                    addPrim(&gGpuCurrentOt[0x3FE], prim);
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        arg0->state = 5;
    }
}

/// The three rows of the code keypad, bottom row first;
/// `D_actor_143000_801345F8` lists them top row first.
const char D_actor_143000_80131E54[] = "NOPQRSTUVWXYZ";
const char D_actor_143000_80131E64[] = "ABCDEFGHIJKLM";
const char D_actor_143000_80131E74[] = "0123456789-# ";

/// State table of the actor's callback, `func_actor_143000_801335C8`, which
/// copies it onto its stack and indexes it with `Task::state`.
static const TaskFuncTable11 D_actor_143000_80131E84 = { {
    func_actor_143000_801324C8,
    func_actor_143000_80133664,
    func_actor_143000_801325F0,
    func_actor_143000_80133698,
    func_actor_143000_801336E8,
    func_actor_143000_80133800,
    func_actor_143000_801338C8,
    func_actor_143000_80132A04,
    func_actor_143000_801338E0,
    func_actor_143000_801339CC,
    func_actor_143000_80133AC0,
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
                Task_Spawn(1, 0x31, 0, &D_actor_143000_80135C08);
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

static void func_actor_143000_80132D10(Task* arg0)
{
    _Actor143000KeypadWork* work;
    POLY_FT4*               prim;
    s32                     i;
    s16                     y;
    s16                     x1;
    s16                     sx;
    u8                      sv;
    s16                     sy;
    s16                     y1;
    u8                      u;
    u8                      v;
    u8                      v1;
    u8                      u1;
    s16                     clut;

    x1                                        = -0x48;
    work                                      = arg0->work;
    D_actor_143000_80135C20[work->codeLength] = 0;
    D_actor_143000_80135C00++;
    y = 0x10;
    for (i = 0; i < work->codeLength; i++) {
        y1             = y + 8;
        u              = 0x58;
        v              = 0xB8;
        u1             = u + 8;
        v1             = v + 8;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        SetPolyFT4(prim);
        setXY4(prim, x1, y, x1 + 8, y, x1, y1, x1 + 8, y1);
        setUV4(prim, u, v, u1, v, u, v1, u1, v1);
        prim->tpage = 0x16;
        prim->clut  = 0x3DC5;
        setShadeTex(prim, 1);
        addPrim(&gGpuCurrentOt[0x3FE], prim);
        x1 += 8;
    }
    if (work->codeLength != ACTOR_143000_KEYPAD_CODE_CAPACITY && arg0->state != 7) {
        u              = 0x60;
        v              = 0xB8;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        SetPolyFT4(prim);
        setXYWH(prim, x1, y, 8, 8);
        setUVWH(prim, u, v, 8, 8);
        prim->tpage = 0x16;
        prim->clut  = 0x3DC6;
        setShadeTex(prim, 1);
        if (D_actor_143000_80135C00 & 0x10) {
            addPrim(&gGpuCurrentOt[0x3FE], prim);
        }
    }
    if (work->statusLine != 0) {
        y1 = 0xFE;
        if (work->statusWidth != 0) {
            y1 = work->statusWidth;
        }
        sx             = -0x78;
        sy             = -0x48;
        sv             = (work->statusLine - 1) * 0x10;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        SetPolyFT4(prim);
        setXYWH(prim, sx, sy, y1, 0x10);
        setUVWH(prim, 0, sv, y1, 0x10);
        prim->tpage = 0x16;
        prim->clut  = 0x3DC0;
        setShadeTex(prim, 1);
        addPrim(&gGpuCurrentOt[0x3FE], prim);
    }
    if (work->resultBanner != ACTOR_143000_KEYPAD_BANNER_NONE) {
        sy = sx = -0x28;
        x1      = 0x18;
        y1      = -0x10;
        if (work->resultBanner == ACTOR_143000_KEYPAD_BANNER_ACCEPTED) {
            u    = 0x70;
            v    = 0xA0;
            clut = 0x3DC3;
        } else {
            u    = 0x30;
            v    = 0xA0;
            clut = 0x3DC4;
        }
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        SetPolyFT4(prim);
        setXY4(prim, sx, sy, x1, sy, sx, y1, x1, y1);
        setUVWH(prim, u, v, 0x40, 0x18);
        prim->tpage = 0x16;
        prim->clut  = clut;
        setShadeTex(prim, 1);
        addPrim(&gGpuCurrentOt[0x3FE], prim);
    }
    sy             = -0x60;
    sx             = work->marqueeX >> 4;
    y1             = sy + 0x18;
    x1             = sx + 0x30;
    sv             = D_actor_143000_80134570[(D_actor_143000_80135C04 / 16) % 16] * 0x18 - 0x60;
    v1             = sv + 0x18;
    clut           = 0x3DC7;
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    SetPolyFT4(prim);
    setXY4(prim, sx, sy, x1, sy, sx, y1, x1, y1);
    setUV4(prim, 0, sv, 0x30, sv, 0, v1, 0x30, v1);
    prim->tpage = 0x16;
    prim->clut  = clut;
    setShadeTex(prim, 1);
    addPrim(&gGpuCurrentOt[0x3FE], prim);
    D_actor_143000_80135C04 += work->marqueeSpeed;
    if (arg0->state != 7 && arg0->state != 0xA) {
        work->marqueeX -= work->marqueeSpeed;
        if (work->marqueeX < -0xD00) {
            work->marqueeX = 0xA00;
        }
    }
    work->marqueeSpeed -= 4;
    if (work->marqueeSpeed < 0x10) {
        work->marqueeSpeed = 0x10;
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

/// Callback of the action-prompt task that `func_actor_143000_801324C8` spawns
/// from `D_actor_143000_80134558`: a two-state dispatcher whose handler table
/// is built on the stack. State 0, `actionPromptReset`, resets both
/// prompt slots; state 1, `actionPromptMoveCursors`, drives the cursor every
/// frame from then on.
void func_actor_143000_80133578(Task* task)
{
    TaskFunc funcs[2] = {
        actionPromptReset,
        actionPromptMoveCursors,
    };

    funcs[task->state](task);
}

void func_actor_143000_801335C8(Task* arg0)
{
    TaskFuncTable11 fns;

    fns = D_actor_143000_80131E84;
    fns.funcs[arg0->state](arg0);
    func_actor_143000_80132D10(arg0);
}

/// State 1 of the actor's callback: arms the first action-prompt slot at
/// `ACTION_PROMPT_SPEED_AIM` with the idle cursor, clears its screen position
/// and steps the task on to state 2.
static void func_actor_143000_80133664(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// State 3 of the actor's callback, entered once a hotspot is picked: clears
/// the prompt's highlight and target, re-spawns the prompt at its current
/// screen position with the picked hotspot's `promptKind`, and moves the task
/// to state 4.
static void func_actor_143000_80133698(Task* task)
{
    ActionPrompt*           prompt = D_80114D28;
    _Actor143000KeypadWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

static void func_actor_143000_801336E8(Task* arg0)
{
    _Actor143000KeypadWork* work   = arg0->work;
    ActionPrompt*           prompt = D_80114D28;
    s32                     cmd;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (func_800D4EC0() != 0) {
        switch ((s16)(work->selectedHotspot - 1)) {
            case 0:
                cmd = 8;
                goto run;
            case 1:
                cmd = 7;
                goto run;
            case 3:
                cmd = 9;
            run:
                Gp_RunCapCmd(cmd, 0);
                arg0->state = 2;
                break;
            case 2:
                sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_KEYPAD_ENTER, 0, 0);
                arg0->state         = 7;
                arg0->killCountdown = 0;
                break;
            case 4:
                work->keypadExamined = 1;
                Gp_RunCapCmd(0xA, 0);
                if (gameFlagGetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) == 1) {
                    arg0->killCountdown = 0xA;
                    arg0->state         = 9;
                } else {
                    arg0->state = 2;
                }
                break;
            default:
                arg0->state = 2;
                break;
        }
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
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_actor_143000_80135C0C_value;
        Gp_MsgPlayer3F3(1);
    } else {
        taskSpawnFromTable(D_actor_143000_801350B0, 1, 0, &D_actor_143000_80135C08);
    }
    taskKill(arg0->spawnArg2.pointer);
    Task_RequestKill(arg0, work->codeAccepted);
}

static void func_actor_143000_801338C8(Task* arg0)
{
    _Actor143000KeypadWork* work = arg0->work;

    work->field_4 = 0;
    arg0->state   = 2;
}

static void func_actor_143000_801338E0(Task* arg0)
{
    _Actor143000KeypadWork* work = arg0->work;
    s32                     col  = (work->keyPress.x + 0x80) / 16;
    s32                     row  = (work->keyPress.y - 0x20) / 16;
    const char*             key;

    if ((u32)col < 13) {
        if (row >= 0) {
            if (row < 3) {
                key = D_actor_143000_801345F8[row] + col;
                if ((s8)*key == '#') {
                    work->codeLength = 0;
                } else if ((s8)*key == '-') {
                    if (work->codeLength > 0) {
                        work->codeLength--;
                    }
                } else if (work->codeLength < ACTOR_143000_KEYPAD_CODE_CAPACITY) {
                    D_actor_143000_80135C20[work->codeLength] = *key;
                    work->codeLength++;
                }
                work->marqueeSpeed = 0x30;
            }
        }
    }
    arg0->state = 2;
}

static void func_actor_143000_801339CC(Task* arg0)
{
    _Actor143000KeypadWork* work = arg0->work;
    u32                     count;

    if (Gp_CapBusy() == 0) {
        count               = (u16)arg0->killCountdown - 1;
        arg0->killCountdown = count;
        if ((s16)count <= 0) {
            arg0->killCountdown = (rand() * 8 >> 15) + 8;
            work->codeLength++;
            sndEvtRequestScriptStart(SOUND_SHELTER_B2_LAB_KEYPAD_KEY, 0, 0);
            memcpy(D_actor_143000_80135C20, D_actor_143000_80131EB0, 11);
            count = work->codeLength;
            if (count >= 0xA) {
                arg0->state = 2;
            }
        }
    }
}

static void func_actor_143000_80133AC0(Task* arg0)
{
    u16 count = (u16)arg0->killCountdown - 1;

    arg0->killCountdown = count;
    if ((s16)count <= 0) {
        arg0->state = 5;
    }
}

/// Hit-tests the cursor against `p`. The far edge is outside the rectangle,
/// where `actionPromptHitTest` includes it. Returns the first hit `id`, or 0.
static s32 func_actor_143000_80133AE8(ActionPromptHotspot* p, s16 x, s16 y)
{
    s32 result = 0;

    if (p->id != ACTION_PROMPT_HOTSPOT_END) {
        do {
            if (x >= p->x && x < p->x + p->w && y >= p->y && y < p->y + p->h) {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 9) {
                    actionPromptOutlineRect((ActionPromptRect*)p, 0, 0, 0);
                }
                p->hit = 1;
                if (result == 0) {
                    result = p->id;
                }
            } else {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 9) {
                    actionPromptOutlineRect((ActionPromptRect*)p, 0xFF, 0, 0);
                }
                p->hit = 0;
            }
            p++;
        } while (p->id != ACTION_PROMPT_HOTSPOT_END);
    }
    return result;
}

static void func_actor_143000_80133C2C(void)
{
    ActionPromptHotspot* p = D_actor_143000_80134580;

    if (p->id != ACTION_PROMPT_HOTSPOT_END) {
        do {
            actionPromptOutlineRect((ActionPromptRect*)p, 0, 0xFF, 0);
            p++;
        } while (p->id != ACTION_PROMPT_HOTSPOT_END);
    }
}

#include "../../shared/action_prompt_reset.inc.c"

void func_actor_143000_80133CF0(Task* arg0)
{
    Actor143000CaptureArgs* args = arg0->spawnArg2.pointer;
    RECT                    r;
    RECT                    r2;
    s32                     n;
    s32                     offset;
    RECT*                   rp;
    s32                     bottom;

    if (gGameSession->location.loc.view != 0xE) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            arg0->killCountdown  = 6;
            args->stripsCaptured = 0;
            arg0->state++;
            break;
        case 1:
            if (--arg0->killCountdown > 0) {
                break;
            }
            arg0->killCountdown = 6;
            r.x                 = args->band.x;
            r.w                 = args->band.w;
            r.y                 = args->band.y + args->band.h * args->stripsCaptured / args->stripCount;
            n                   = args->stripsCaptured + 1;
            rp                  = &r2;
            SOFT_TOUCH_REG_USE(rp, n);
            args->stripsCaptured = n;
            bottom               = args->band.y + args->band.h * n / args->stripCount;
            offset               = r.y * FILE_SYSTEM_IMAGE_ROW_BYTES;
            r.h                  = bottom - r.y;
            SOFT_USE_REG(offset);
            r2    = r;
            r2.x  = 0x1C0;
            rp->w = 0x140;
            r2.y += 0x100;
            StoreImage(rp, (u_long*)((u8*)Fs_ImgBuffers + offset));
            if (args->stripsCaptured >= args->stripCount) {
                taskKill(arg0);
            }
            break;
    }
}
