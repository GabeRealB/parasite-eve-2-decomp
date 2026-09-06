#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
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
