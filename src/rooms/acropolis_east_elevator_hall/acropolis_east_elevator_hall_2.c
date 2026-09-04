#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_east_elevator_hall.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

extern s32     D_acropolis_east_elevator_hall_8017D5E0;
extern SVECTOR D_acropolis_east_elevator_hall_8017D5E8;
extern s32     D_acropolis_east_elevator_hall_80185C8C;
extern s32     D_acropolis_east_elevator_hall_80185D54;
extern s32     D_acropolis_east_elevator_hall_801860B4;
extern s32     D_acropolis_east_elevator_hall_8018621C;
extern s32     D_acropolis_east_elevator_hall_801862F4;
extern s32     D_acropolis_east_elevator_hall_8018631C;

s32 func_acropolis_east_elevator_hall_8017F378(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 0 && GameFlag_GetNibble(0) == 0 && D_acropolis_east_elevator_hall_8018631C == 0) {
        func_800E8634((s32)&D_acropolis_east_elevator_hall_80185D54, 0, (s32)&D_acropolis_east_elevator_hall_801860B4);
        D_acropolis_east_elevator_hall_8018631C = 1;
        GameFlag_SetNibble(0, 1);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 3);
        GameFlag_SetNibble(8, 2);
        func_800E3FAC(0xA2, 2);
    }
    return 0;
}

s32 func_acropolis_east_elevator_hall_8017F420(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        func_800E8614((s32)&D_acropolis_east_elevator_hall_8018621C, 0);
    }
    return 0;
}

void func_acropolis_east_elevator_hall_8017F450(void)
{
    Gp_StartCapSlot(0x10, 1, 0);
}

void func_acropolis_east_elevator_hall_8017F478(Task* task)
{
    task->field_24 = &D_acropolis_east_elevator_hall_801862F4;
    Game_SetPtrSlot(task, 7);
    Gp_MsgSlot4Chain(0, 1);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_acropolis_east_elevator_hall_80185C8C, 0);
    task->state++;
}

void func_acropolis_east_elevator_hall_8017F4E8(void)
{
    if (Display_State.field_112 != 0) {
        func_807245E4(Game_GetPtrSlot(3));
        if (Display_State.field_112 != 0) {
            func_80724608(Game_GetPtrSlot(3), -0x8C, -0x32, &D_acropolis_east_elevator_hall_8017D5E0);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall_2", func_acropolis_east_elevator_hall_8017F55C);

void func_acropolis_east_elevator_hall_8017F5B4(Task* task)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)task->extra)->field_8;
    switch (task->state) {
        case 0:
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state++;
            /* fallthrough */
        case 1:
            if ((u8)Game_Session->field_4 == 2) {
                SVECTOR vec = D_acropolis_east_elevator_hall_8017D5E8;

                Gp_SpawnEff(0x60022, coord, 0xC03, &vec);
                vec.vx = 0x1600;
                vec.vy = -0x985;
                vec.vz = 0x55;
                Gp_SpawnEff(0x60022, coord, 0xC03, &vec);
                vec.vx = 0x1600;
                vec.vy = -0xA81;
                vec.vz = -0x1CA;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
                vec.vx = 0x1600;
                vec.vy = -0xA93;
                vec.vz = -0x61C;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
                vec.vx = 0x1600;
                vec.vy = -0x460;
                vec.vz = -0x1A1;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
                vec.vx = 0x1600;
                vec.vy = -0x449;
                vec.vz = -0x635;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall_2", func_acropolis_east_elevator_hall_8017F77C);

void func_acropolis_east_elevator_hall_8017FAAC(Task* arg0)
{
    void**                    scratch;
    u8*                       head;
    register AeehMoteScratch* block asm("v1");
    TILE_1*                   prim;
    GsCOORDINATE2*            coord;
    void*                     mem;
    u16                       vz;

    scratch = (void**)G_SCRATCH_HEAD;
    coord   = ((TmdObject*)arg0->extra)->field_8;
    mem     = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    head          = *scratch;
    block         = (AeehMoteScratch*)(head - 0xC);
    block->vec.vx = *(u16*)&coord->workm.t[0];
    block->vec.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AeehMoteScratch*)(head - 0xC))->vec);
    gte_rtps_real();
    prim           = (TILE_1*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setTile1(prim);
    gte_stsxy(&prim->x0);
    gte_stszotz(&block->otz);
    if (((AeehMoteScratch*)(head - 0xC))->otz >= 0x11) {
        setRGB0(prim, 0x80, 0x80, 0x80);
        addPrim((u_long*)(((((u32)((AeehMoteScratch*)(head - 0xC))->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, ((AeehMoteScratch*)(head - 0xC))->otz);
    }
    *scratch = (u8*)*scratch + 0xC;
    Gp_ReleaseState1CMem(mem, arg0);
}
