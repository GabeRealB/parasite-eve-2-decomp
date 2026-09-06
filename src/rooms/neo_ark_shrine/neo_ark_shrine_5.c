#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Scratch state of the shrine's cap script, stored at `Task::idMap`
/// (`Mem_Calloc(0x10)` in `func_neo_ark_shrine_8017ECC4`).
typedef struct {
    /* 0x00 */ u8  pad_0[8];
    /* 0x08 */ u16 timer; ///< frames the current script step has run
    /* 0x0A */ u8  pad_A[2];
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s8  field_E;
    /* 0x0F */ s8  field_F;
} NeoArkShrineScript;

/// Scratch state of the two falling-prop tasks, stored at `Task::idMap`
/// (`Mem_Calloc(0x48)` in `func_neo_ark_shrine_8017F4C8` / `_8017F688`).
typedef struct {
    /* 0x00 */ u8  pad_0[0x40];
    /* 0x40 */ u16 speed; ///< per-frame gravity step
    /* 0x42 */ u16 delta; ///< accumulated fall distance for this frame
    /* 0x44 */ u16 ticks; ///< frames since the fall started
    /* 0x46 */ u8  pad_46[2];
} NeoArkShrineFall;

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2);

void func_neo_ark_shrine_8017EAC0(void);
void func_neo_ark_shrine_8017F86C(Task* task);

extern s8 D_8007216C;
extern s8 D_80115410;

extern s16      D_neo_ark_shrine_8018686A;
extern TaskDesc D_neo_ark_shrine_80182508;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017EDE0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017EE44);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017EED4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017EF68);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017EFE4);

void func_neo_ark_shrine_8017F094(Task* task)
{
    NeoArkShrineScript* st;

    st                        = (NeoArkShrineScript*)task->idMap;
    D_neo_ark_shrine_8018686A = 1;
    func_neo_ark_shrine_8017EAC0();
    Task_Kill((Task*)task->spawnArg2);
    st->timer = 0;
    task->state++;
}

void func_neo_ark_shrine_8017F0F0(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;

    st = (NeoArkShrineScript*)task->idMap;
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

    st        = (NeoArkShrineScript*)task->idMap;
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

    st        = (NeoArkShrineScript*)task->idMap;
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
    D_80115410             = 2;
    Mc_SaveData.field_5    = 6;
    Game_Session->field_5  = 6;
    Game_Session->field_76 = 1;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    Game_Session->field_1  = 0;
    Game_Session->field_68 = 0;
    Game_Session->field_66 = 0;
    Mc_SaveData.field_4    = 0xA;
    Task_RequestKill(task, 0);
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017F320);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017F398);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017F448);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017F4C8);

void func_neo_ark_shrine_8017F578(Task* task)
{
    NeoArkShrineFall* st;
    GsCOORDINATE2*    coord;
    u16               ticks;
    u16               speed;
    u16               delta;
    s32               y;

    st        = (NeoArkShrineFall*)task->idMap;
    coord     = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
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

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_5", func_neo_ark_shrine_8017F688);

void func_neo_ark_shrine_8017F738(Task* task)
{
    NeoArkShrineFall* st;
    GsCOORDINATE2*    coord;
    u16               ticks;
    u16               speed;
    u16               delta;
    s32               y;

    st        = (NeoArkShrineFall*)task->idMap;
    coord     = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
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
