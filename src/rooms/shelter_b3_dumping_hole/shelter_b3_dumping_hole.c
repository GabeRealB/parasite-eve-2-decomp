#include "rooms/shelter_b3_dumping_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "shelter_b3_dumping_hole_private.h"

#include "actors/actor_403200.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b3_dumping_hole_8018F4A4[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern u8 D_shelter_b3_dumping_hole_8018F4A4_value __asm__("D_shelter_b3_dumping_hole_8018F4A4");

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(s32, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call2)(s32, s32, s32);
    } handler;
} ShelterB3DumpingHoleMessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB3DumpingHoleMessageEntry, 8);

extern ShelterB3DumpingHoleMessageEntry D_shelter_b3_dumping_hole_80187574[6];

extern TaskDesc D_80164B78;

s32 func_shelter_b3_dumping_hole_8017D758(void);
s32 func_shelter_b3_dumping_hole_8017D760(s32, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b3_dumping_hole_8017D82C(s32, s32, s32);
s32 func_shelter_b3_dumping_hole_8017D868(void);
s32 func_shelter_b3_dumping_hole_8017D870(void);

extern GpAnimSet D_shelter_b3_dumping_hole_80188074;

TmdBone D_shelter_b3_dumping_hole_80186F8C[3] = {
#include "assets/shelter_b3_dumping_hole_model_09F90_skeleton.inc"
};

u32 D_shelter_b3_dumping_hole_80186FF8[3] = {
#include "assets/shelter_b3_dumping_hole_model_09F90_partVerts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_80187004[56] = {
#include "assets/shelter_b3_dumping_hole_model_09F90_verts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_801871C4[6] = {
#include "assets/shelter_b3_dumping_hole_model_09F90_normals.inc"
};

u32 D_shelter_b3_dumping_hole_801871F4[215] = {
#include "assets/shelter_b3_dumping_hole_model_09F90_stream.inc"
};

TmdSource D_shelter_b3_dumping_hole_80187550 = {
    0,
    1768,
    0,
    3,
    D_shelter_b3_dumping_hole_80186FF8,
    D_shelter_b3_dumping_hole_80187004,
    D_shelter_b3_dumping_hole_801871C4,
    D_shelter_b3_dumping_hole_80186F8C,
    D_shelter_b3_dumping_hole_801871F4,
};

ShelterB3DumpingHoleMessageEntry D_shelter_b3_dumping_hole_80187574[6] = {
    { 5102, { .call1 = func_shelter_b3_dumping_hole_8017D760 } },
    { 5105, { .call0 = func_shelter_b3_dumping_hole_8017D758 } },
    { 5103, { .call0 = func_shelter_b3_dumping_hole_8017D868 } },
    { 5104, { .call2 = func_shelter_b3_dumping_hole_8017D82C } },
    { 5108, { .call0 = func_shelter_b3_dumping_hole_8017D870 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TmdBone D_shelter_b3_dumping_hole_801875A4[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A234_skeleton.inc"
};

u32 D_shelter_b3_dumping_hole_801875C8[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A234_partVerts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_801875CC[9] = {
#include "assets/shelter_b3_dumping_hole_model_0A234_verts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_80187614[15] = {
#include "assets/shelter_b3_dumping_hole_model_0A234_normals.inc"
};

u32 D_shelter_b3_dumping_hole_8018768C[90] = {
#include "assets/shelter_b3_dumping_hole_model_0A234_stream.inc"
};

TmdSource D_shelter_b3_dumping_hole_801877F4 = {
    0,
    560,
    0,
    1,
    D_shelter_b3_dumping_hole_801875C8,
    D_shelter_b3_dumping_hole_801875CC,
    D_shelter_b3_dumping_hole_80187614,
    D_shelter_b3_dumping_hole_801875A4,
    D_shelter_b3_dumping_hole_8018768C,
};

TmdBone D_shelter_b3_dumping_hole_80187818[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A4B0_skeleton.inc"
};

u32 D_shelter_b3_dumping_hole_8018783C[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A4B0_partVerts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_80187840[9] = {
#include "assets/shelter_b3_dumping_hole_model_0A4B0_verts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_80187888[16] = {
#include "assets/shelter_b3_dumping_hole_model_0A4B0_normals.inc"
};

u32 D_shelter_b3_dumping_hole_80187908[90] = {
#include "assets/shelter_b3_dumping_hole_model_0A4B0_stream.inc"
};

TmdSource D_shelter_b3_dumping_hole_80187A70 = {
    0,
    560,
    0,
    1,
    D_shelter_b3_dumping_hole_8018783C,
    D_shelter_b3_dumping_hole_80187840,
    D_shelter_b3_dumping_hole_80187888,
    D_shelter_b3_dumping_hole_80187818,
    D_shelter_b3_dumping_hole_80187908,
};

TmdBone D_shelter_b3_dumping_hole_80187A94[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A7B4_skeleton.inc"
};

u32 D_shelter_b3_dumping_hole_80187AB8[1] = {
#include "assets/shelter_b3_dumping_hole_model_0A7B4_partVerts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_80187ABC[11] = {
#include "assets/shelter_b3_dumping_hole_model_0A7B4_verts.inc"
};

SVECTOR D_shelter_b3_dumping_hole_80187B14[19] = {
#include "assets/shelter_b3_dumping_hole_model_0A7B4_normals.inc"
};

u32 D_shelter_b3_dumping_hole_80187BAC[114] = {
#include "assets/shelter_b3_dumping_hole_model_0A7B4_stream.inc"
};

TmdSource D_shelter_b3_dumping_hole_80187D74 = {
    0,
    720,
    0,
    1,
    D_shelter_b3_dumping_hole_80187AB8,
    D_shelter_b3_dumping_hole_80187ABC,
    D_shelter_b3_dumping_hole_80187B14,
    D_shelter_b3_dumping_hole_80187A94,
    D_shelter_b3_dumping_hole_80187BAC,
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[6];
    AnimationPackedRotation        words[18];
} ShelterB3DumpingHolePoseBankA7D8;

ShelterB3DumpingHolePoseBankA7D8 D_shelter_b3_dumping_hole_80187D98 = { .poses = {
#include "assets/shelter_b3_dumping_hole_animation_0AAB4_bank1.inc"
};

AnimationPackedRotation D_shelter_b3_dumping_hole_80187DE0[46] = {
#include "assets/shelter_b3_dumping_hole_animation_0AAB4_bank4.inc"
};

GpAnimRec D_shelter_b3_dumping_hole_80187E98[109] = {
#include "assets/shelter_b3_dumping_hole_animation_0AAB4_records.inc"
};

u16 D_shelter_b3_dumping_hole_8018804C[20] = {
#include "assets/shelter_b3_dumping_hole_animation_0AAB4_indices.inc"
};

GpAnimSet D_shelter_b3_dumping_hole_80188074 = {
    D_shelter_b3_dumping_hole_80187E98,
    D_shelter_b3_dumping_hole_8018804C,
    { NULL, D_shelter_b3_dumping_hole_80187D98, NULL, NULL, D_shelter_b3_dumping_hole_80187DE0, NULL, NULL, NULL },
};

s16 D_shelter_b3_dumping_hole_8018809C = 1;

GpAnimSet* D_shelter_b3_dumping_hole_801880A0[6] = {
    &D_shelter_b3_dumping_hole_80188074,
    &D_actor_403200_8015ED84,
    &D_actor_403200_8015EFF0,
    &D_actor_403200_8015F748,
    &D_actor_403200_8015EC00,
    NULL,
}; /// Message-table handler that accepts every message without acting on it.

static void func_shelter_b3_dumping_hole_8017D8A0(Task* arg0);
static void func_shelter_b3_dumping_hole_8017D998(Task* task);

s32 func_shelter_b3_dumping_hole_8017D758(void)
{
    return 0;
}

s32 func_shelter_b3_dumping_hole_8017D760(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->prefix.packed == 0x28) {
        if (func_shelter_b3_dumping_hole_8017FB70() != 0) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(0x16);
            }
            return 0;
        }
        if (in->field_5 == 0) {
            out->field_3 = (u8)gGameSession->eventRoomIndex + 1;
        }
        return 1;
    }
    return 1;
}

s32 func_shelter_b3_dumping_hole_8017D82C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x12) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x11D) != 0 ? 0x12 : 0x17, 1);
    }
    return 0;
}

/// Message-table handler that accepts every message without acting on it.
s32 func_shelter_b3_dumping_hole_8017D868(void)
{
    return 0;
}

s32 func_shelter_b3_dumping_hole_8017D870(void)
{
    Task_SpawnFromTable(D_shelter_b3_dumping_hole_80189ADC, 0, 0, 0);
    return 0;
}

static void func_shelter_b3_dumping_hole_8017D8A0(Task* arg0)
{
    arg0->msgTable = D_shelter_b3_dumping_hole_80187574;
    Game_SetPtrSlot(arg0, 7);
    func_shelter_b3_dumping_hole_80183198(0x180, 0, 0);
    if (GameFlag_GetNibble(0x78) == 0) {
        if (gGameSession->at4.loc.place == 1) {
            if (gGameSession->at4.loc.warp == 3) {
                func_800E8634(D_shelter_b3_dumping_hole_8018B080, 0,
                              D_shelter_b3_dumping_hole_8018B428);
            }
            func_800E3FAC(0xA2, 0x21);
            GameFlag_SetNibble(0x78, 1);
        }
    }
    if (gGameSession->at4.loc.room >= 2) {
        Task_SpawnFromTable(&D_80164B78, 0, 0, 0);
    }
    arg0->state                             += 1;
    D_shelter_b3_dumping_hole_8018F4A4_value = 0;
}

/// Empty function; the unused local reserves the 0x10-byte stack frame the
/// original carries.
static void func_shelter_b3_dumping_hole_8017D998(Task* task)
{
    char pad[0x10];
}

/// State handlers of the room's controller task, run by
/// `func_shelter_b3_dumping_hole_8017D9A8`: set-up, an idle state, and the
/// kill.
static const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D5C4 = { {
    func_shelter_b3_dumping_hole_8017D8A0,
    func_shelter_b3_dumping_hole_8017D998,
    taskKill,
} };

void func_shelter_b3_dumping_hole_8017D9A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D5C4;
    sp.funcs[task->state](task);
}
