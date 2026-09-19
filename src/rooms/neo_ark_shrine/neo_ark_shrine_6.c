#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"
#include "rooms/room_common.h"

/// Scratch state of the shrine's cap script, stored at `Task::work`
/// (`memCalloc(0x10)` in `func_neo_ark_shrine_8017ECC4`).
typedef struct {
    /* 0x00 */ u8  pad_0[8];
    /* 0x08 */ u16 timer; ///< frames the current script step has run
    /* 0x0A */ u8  pad_A[2];
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s8  field_E;
    /* 0x0F */ s8  field_F;
} NeoArkShrineScript;

/// Scratch state of the two falling-prop tasks, stored at `Task::work`
/// (`memCalloc(0x48)` in `func_neo_ark_shrine_8017F4C8` / `_8017F688`).
/// `color` / `light` are the prop's own matrices, republished onto
/// `TmdObject::lightMtx` / `field_20` by the two spawn handlers; `speed` /
/// `delta` / `ticks` are the fall itself, stepped by `func_neo_ark_shrine_8017F578`.
typedef struct {
    /* 0x00 */ MATRIX color;
    /* 0x20 */ MATRIX light;
    /* 0x40 */ u16    speed; ///< per-frame gravity step
    /* 0x42 */ u16    delta; ///< accumulated fall distance for this frame
    /* 0x44 */ u16    ticks; ///< frames since the fall started
    /* 0x46 */ u8     pad_46[2];
} NeoArkShrineFall;

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2);

/* No parameter list: `func_neo_ark_shrine_8017F320` passes `task` to the
   argument-less helper (the target loads `$a0` before that call) while every
   other caller here passes nothing. */
void func_neo_ark_shrine_8017EAC0();
void func_neo_ark_shrine_8017F86C(Task* task);

extern s8  D_8007216C;
extern u8  D_8007216D;
extern s8  D_80115410;
extern s16 D_80114D08;

/// One slot of the shrine's 16-slot arrangement puzzle, as a pair of 16-bit
/// board coordinates. `D_neo_ark_shrine_8018256C` / `_8018252C` hold the room's
/// initial and target layouts; `_8018688C` / `_801868CC` are the work copies
/// the puzzle animates between.
typedef struct {
    /* 0x00 */ u16 x;
    /* 0x02 */ u16 y;
} NeoArkShrineSlot;

extern s16              D_neo_ark_shrine_80186868;
extern s16              D_neo_ark_shrine_8018686A;
extern TaskDesc         D_neo_ark_shrine_80182508;
extern u16              D_neo_ark_shrine_80182410[16];
extern NeoArkShrineSlot D_neo_ark_shrine_8018256C[16];
extern s16              D_neo_ark_shrine_8018686C[16];
extern NeoArkShrineSlot D_neo_ark_shrine_8018688C[16];

/// Spawns the action prompt for the script's current step: runs the shrine's
/// per-step helper, clears the prompt's highlight state, then re-spawns the
/// prompt at the coordinates the gameplay side left in `D_80114D28` with the
/// display mode this step picked, and advances the task to state 4.
void func_neo_ark_shrine_8017EDE0(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* work   = (NeoArkShrineScript*)task->work;

    func_neo_ark_shrine_8017EAC0();
    /* Without this local-alloc ranks `work` (2 refs over 6 insns) above `task`
       (3 refs over 12), which swaps their `$s1` / `$s2` homes. */
    SOFT_TOUCH_REG(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->field_E);
    task->state = 4;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_6", func_neo_ark_shrine_8017EE44);

void func_neo_ark_shrine_8017EED4(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    D_8007216C                 = 0xA;
    /* Without this the scheduler hoists the `spawnArg2` load above the
       `D_8007216C` byte store, which then fills `taskKill`'s delay slot. */
    SOFT_BARRIER();
    taskKill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

/// Same as `func_neo_ark_shrine_8017F320`, but it latches the script's pad
/// mode on rather than off.
void func_neo_ark_shrine_8017EF68(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    Gp_SpawnPadLerp(0x12, 0x30, 0x90);
    D_neo_ark_shrine_80186868 = 1;
    prompt->mode              = 0;
    prompt->targetId          = 0;
    func_neo_ark_shrine_8017EAC0(task);
    st->timer = 0;
    task->state++;
}

/// The script step that runs while the shrine's pad is idle: it re-clears the
/// prompt, ticks the step's timer, and once the step has run 0x1E frames latches
/// the shrine's mode — 2, or 5 when flag 0xE9 is set — into `D_8007216D` and the
/// session, which makes the room rebuild its objects, and enters state 2.
///
/// The same literal is stored in both arms on purpose: `gGameSession` is read
/// per arm, and jump_optimize's cross-jumping (post-sched2) merges the arms'
/// identical `sb` pairs into the join. Written with one shared `var_v0` the
/// stores are one pair too but the constant's `li` precedes the address, the
/// merge swallows the `gGameSession` load as well, and the function comes out
/// four insns short.
void func_neo_ark_shrine_8017EFE4(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    st->timer        = st->timer + 1;
    func_neo_ark_shrine_8017EAC0(task);
    if (st->timer >= 0x1E) {
        if (GameFlag_GetNibble(0xE9) == 0) {
            D_8007216D                 = 2;
            gGameSession->at4.loc.room = 2;
        } else {
            D_8007216D                 = 5;
            gGameSession->at4.loc.room = 5;
        }
        gGameSession->roomObjsDirty = 1;
        task->state                 = 2;
    }
}

void func_neo_ark_shrine_8017F094(Task* task)
{
    NeoArkShrineScript* st;

    st                        = (NeoArkShrineScript*)task->work;
    D_neo_ark_shrine_8018686A = 1;
    func_neo_ark_shrine_8017EAC0();
    taskKill((Task*)task->spawnArg2);
    st->timer = 0;
    task->state++;
}

void func_neo_ark_shrine_8017F0F0(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;

    st = (NeoArkShrineScript*)task->work;
    func_neo_ark_shrine_8017EAC0();
    timer     = st->timer + 1;
    st->timer = timer;
    if (timer >= 0x1EU) {
        Task_SpawnFromTable(&D_neo_ark_shrine_80182508, 1, 0, 0);
        D_8007216C = 0xE;
        /* Without this the scheduler hoists the `task->state` reload above the
           `D_8007216C` byte store to fill its load-delay slot. */
        SOFT_BARRIER();
        st->timer = 0;
        task->state++;
    }
}

void func_neo_ark_shrine_8017F178(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;
    s32                 next;

    st        = (NeoArkShrineScript*)task->work;
    timer     = st->timer + 1;
    st->timer = timer;
    if (timer >= 0x5AU) {
        st->timer = 0;
        if (GameFlag_GetNibble(0xE9) == 0) {
            Task_SpawnFromTable(&D_neo_ark_shrine_80182508, 2, 0, 0);
            D_8007216C = 0xD;
            GameFlag_SetNibble(0xE9, 1);
            next = task->state + 1;
        } else {
            next = task->state + 2;
        }
        task->state = next;
    }
}

void func_neo_ark_shrine_8017F21C(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;

    st        = (NeoArkShrineScript*)task->work;
    timer     = st->timer + 1;
    st->timer = timer;
    if (timer == 0x1E) {
        D_80115410 = 1;
    }
    if (st->timer >= 0x3CU) {
        task->state++;
    }
}

void func_neo_ark_shrine_8017F274(Task* task)
{
    D_80115410                  = 2;
    Mc_SaveData.at4.loc.room    = 6;
    gGameSession->at4.loc.room  = 6;
    gGameSession->roomObjsDirty = 1;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    Mc_SaveData.at4.loc.view   = 0xA;
    Task_RequestKill(task, 0);
}

/// Runs the shrine's per-step helper and restarts the script's step timer:
/// raises a pad lerp, clears the prompt's highlight state and advances the
/// task to the next state.
void func_neo_ark_shrine_8017F320(Task* task)
{
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    Gp_SpawnPadLerp(0x12, 0x30, 0x90);
    D_neo_ark_shrine_80186868 = 0;
    prompt->mode              = 0;
    prompt->targetId          = 0;
    func_neo_ark_shrine_8017EAC0(task);
    st->timer = 0;
    task->state++;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_6", func_neo_ark_shrine_8017F398);

/// Resets the shrine's 16-slot arrangement puzzle to its starting state: clears
/// the two puzzle flags, reloads the work copy of the slot layout from the
/// room's initial-layout table, and re-seeds the slot arrangement with the
/// room's starting order.
void func_neo_ark_shrine_8017F448(void)
{
    NeoArkShrineSlot* dstSlot;
    NeoArkShrineSlot* srcSlot;
    s16*              dstOrder;
    u16*              srcOrder;
    s32               i;
    u16               y;
    u16               order;

    i                         = 0;
    dstSlot                   = D_neo_ark_shrine_8018688C;
    srcSlot                   = D_neo_ark_shrine_8018256C;
    D_neo_ark_shrine_8018686A = 0;
    D_neo_ark_shrine_80186868 = 0;
    do {
        i++;
        dstSlot->x = srcSlot->x;
        y          = srcSlot->y;
        srcSlot++;
        dstSlot->y = y;
        dstSlot++;
    } while (i < 0x10);

    i        = 0;
    dstOrder = D_neo_ark_shrine_8018686C;
    srcOrder = D_neo_ark_shrine_80182410;
    do {
        order = *srcOrder;
        srcOrder++;
        i++;
        *dstOrder = order;
        dstOrder++;
    } while (i < 0x10);
}

/// Second state of the shrine's first falling prop: allocates its 0x48-byte
/// scratch block, republishes the block's light / colour matrices onto the
/// model's `TmdObject`, parks the prop at its starting position parented to the
/// room's view coordinate system, and advances the task to the falling state.
void func_neo_ark_shrine_8017F4C8(Task* task)
{
    TmdObject*        extra;
    GsCOORDINATE2*    coord;
    NeoArkShrineFall* st;

    extra      = (TmdObject*)task->extra;
    coord      = extra->coords;
    st         = (NeoArkShrineFall*)memCalloc(sizeof(NeoArkShrineFall), 0);
    task->work = (TaskIdMap*)st;
    if (st == NULL) {
        taskKill(task);
        return;
    }
    extra->lightMtx   = &st->light;
    extra->flags      = 0;
    extra->colorMtx   = &st->color;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = 0x1B58;
    coord->coord.t[1] = -0xBB8;
    coord->coord.t[2] = -0x3E8;
    func_neo_ark_shrine_8017F86C(task);
    task->state++;
}

void func_neo_ark_shrine_8017F578(Task* task)
{
    NeoArkShrineFall* st;
    GsCOORDINATE2*    coord;
    u16               ticks;
    u16               speed;
    u16               delta;
    s32               y;

    st        = (NeoArkShrineFall*)task->work;
    coord     = (GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
    ticks     = st->ticks + 1;
    st->ticks = ticks;
    if ((s16)ticks == 4) {
        Gp_SpawnPadLerp(0x18, 0x40, 0xFF);
        SndEvt_EnqueueType6(0x55150009, 0, 0);
    }
    speed             = st->speed + 1;
    delta             = st->delta + speed;
    st->delta         = delta;
    st->speed         = speed;
    y                 = coord->coord.t[1] + (s16)delta;
    coord->coord.t[1] = y;
    if (y > 0) {
        coord->coord.t[1] = 0;
        task->state++;
    }
    func_neo_ark_shrine_8017F86C(task);
}

void func_neo_ark_shrine_8017F640(Task* task)
{
    func_neo_ark_shrine_8017F86C(task);
    if (D_neo_ark_shrine_8018686A == 0) {
        task->state++;
    }
}

/// Second state of the shrine's second falling prop: as `func_neo_ark_shrine_8017F4C8`,
/// but parked at the mirror position on the far side of the shrine.
void func_neo_ark_shrine_8017F688(Task* task)
{
    TmdObject*        extra;
    GsCOORDINATE2*    coord;
    NeoArkShrineFall* st;

    extra      = (TmdObject*)task->extra;
    coord      = extra->coords;
    st         = (NeoArkShrineFall*)memCalloc(sizeof(NeoArkShrineFall), 0);
    task->work = (TaskIdMap*)st;
    if (st == NULL) {
        taskKill(task);
        return;
    }
    extra->lightMtx   = &st->light;
    extra->flags      = 0;
    extra->colorMtx   = &st->color;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = 0x222E;
    coord->coord.t[1] = -0xBB8;
    coord->coord.t[2] = -0x11C6;
    func_neo_ark_shrine_8017F86C(task);
    task->state++;
}

void func_neo_ark_shrine_8017F738(Task* task)
{
    NeoArkShrineFall* st;
    GsCOORDINATE2*    coord;
    u16               ticks;
    u16               speed;
    u16               delta;
    s32               y;

    st        = (NeoArkShrineFall*)task->work;
    coord     = (GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
    ticks     = st->ticks + 1;
    st->ticks = ticks;
    if ((s16)ticks == 2) {
        SndEvt_EnqueueType6(0x5515000B, 0, 0);
    }
    if ((s16)st->ticks == 0x12) {
        Gp_SpawnPadLerp(0xA, 0xA0, 0xFF);
    }
    speed             = st->speed + 2;
    delta             = st->delta + speed;
    st->delta         = delta;
    st->speed         = speed;
    y                 = coord->coord.t[1] + (s16)delta;
    coord->coord.t[1] = y;
    if (y > 0) {
        coord->coord.t[1] = 0;
        task->state++;
    }
    func_neo_ark_shrine_8017F86C(task);
}
