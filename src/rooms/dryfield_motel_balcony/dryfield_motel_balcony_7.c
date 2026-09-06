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

extern s32 D_80115728;
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
extern void RoomsShared8017e4f8Fade(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);
extern void Room_Draw02(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u8* arg3);
extern void func_dryfield_motel_balcony_801809AC(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);
extern void func_dryfield_motel_balcony_801818B0(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);
extern s32  Gp_LcgState;

void func_dryfield_motel_balcony_801802DC(Task* arg0)
{
    u8                      rgb[3];
    GpEffWork*              mem;
    register GsCOORDINATE2* coord asm("s2");
    s16                     flag;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
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
                mem->field_24 = 0;
                mem->field_26 = 0x80;
                mem->field_2A = 0x100 / arg0->spawnArg1;
                arg0->state   = 1;
                return;
            case 1:
                mem->field_24   += mem->field_2A;
                mem->field_26   += mem->field_2A;
                arg0->spawnArg1 -= 1;
                rgb[0]           = mem->field_24;
                rgb[1]           = (u16)mem->field_24 >> 2;
                rgb[2]           = (u16)mem->field_24 >> 1;
                func_dryfield_motel_balcony_801809AC(coord, mem->field_26, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                func_dryfield_motel_balcony_801809AC(coord, (u16)mem->field_26 * 2, rgb);
                Room_Draw02(coord, 0x300 - (u16)mem->field_26 * 2, 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->field_24 = 0xFF;
                    arg0->state   = 2;
                    rgb[0]        = mem->field_24;
                    rgb[1]        = (u16)mem->field_24 >> 2;
                    rgb[2]        = (u16)mem->field_24 >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                return;
            case 2:
                if (mem->field_24 >= 0x11) {
                    rgb[0] = mem->field_24;
                    rgb[1] = (u16)mem->field_24 >> 2;
                    rgb[2] = (u16)mem->field_24 >> 1;
                    func_dryfield_motel_balcony_801818B0(coord, mem->field_26 * 3, rgb);
                    mem->field_24 -= 0x10;
                    mem->field_26 -= 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}
