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

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017EDE0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017EE44);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017EED4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017EF68);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017EFE4);

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

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F0F0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F178);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F21C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F274);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F320);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F398);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F448);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F4C8);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F578);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F640);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F688);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_4", func_neo_ark_shrine_8017F738);
