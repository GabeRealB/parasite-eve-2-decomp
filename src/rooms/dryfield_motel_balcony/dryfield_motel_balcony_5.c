#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_motel_balcony.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32 Gp_LcgState;

extern void Room_Draw02(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u8* arg3);

void func_dryfield_motel_balcony_80181628(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    flag  = Gp_State1C->field_4;
    mem   = arg0->spawnArg2;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->field_22++;
        switch (arg0->state) {
            case 0:
                Gp_SpawnEff(0x60076, coord, 0x400, NULL);
                if (arg0->spawnArg1 != 0) {
                    Gp_SpawnEff(0x60070, coord, 0x80004600, NULL);
                    arg0->state = 1;
                    return;
                }
                Gp_SpawnEff(0x6007C, coord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, coord, 0x100, NULL);
                mem->field_24 = 0x100;
                mem->field_26 = 0xC0;
                arg0->state   = 2;
                return;
            case 1:
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x60070, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                            (SVECTOR*)&mem->field_10);
                break;
            case 2:
                mem->field_26 -= 0x20;
                mem->field_24 += 0x30;
                rgb[0]         = mem->field_26;
                rgb[1]         = (u16)mem->field_26 >> 1;
                rgb[2]         = (u16)mem->field_26 >> 2;
                Room_Draw02(coord, 0x100, 0x100, rgb);
                Room_Draw02(coord, mem->field_24, mem->field_24, rgb);
                break;
            case 3:
                goto kill;
            default:
                return;
        }
        if (mem->field_22 >= 7) {
            arg0->state = 3;
        }
        return;
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}
INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony_5", func_dryfield_motel_balcony_801818B0);