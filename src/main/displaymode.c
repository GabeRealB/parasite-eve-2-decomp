#include "common.h"

#include "main/display.h"
#include "main/gamemain.h"
#include "main/unknown_syms.h"

#include <psyq/libgpu.h>

void Display_SetMode(s32 arg0)
{
    DisplayState* ds;
    u32           temp_s5;
    u32           temp_s4;
    s32           temp_s3;
    char          temp_s0;
    register s32  temp_s1 asm("s1");
    DisplayState* new_var;
    s8            var_s7;
    u32           temp_v0_2;

    if (!(arg0 & 0xFFFF)) {
        arg0 = 0x1010;
    }
    var_s7     = (arg0 & 0xF00) != 0;
    ds         = &Display_State;
    temp_s5    = Display_WidthTable[(u32)(arg0 & 0xF0) >> 4];
    temp_s4    = Display_HeightTable[arg0 & 0xF];
    ds->width  = temp_s5;
    ds->height = temp_s4;
    if (D_8007272B != 0) {
        var_s7 = 1;
    }
    temp_s3       = temp_s4 & 0xFFFF;
    ds->interlace = var_s7;
    new_var       = ds;
    if (temp_s3 != 0x1E0) {
        SetDefDrawEnv(&ds->drawEnv[0], 0, 0, (temp_s0 = temp_s5, temp_s5 & 0xFFFF), temp_s3);
        temp_s1 = temp_s3 + 0x20;
        SetDefDispEnv(&ds->dispEnv[0], 0, temp_s1, temp_s5 & 0xFFFF, temp_s3);
        SetDefDrawEnv(&ds->drawEnv[1], 0, temp_s1, temp_s5 & 0xFFFF, temp_s3);
        SetDefDispEnv(&ds->dispEnv[1], 0, 0, temp_s5 & 0xFFFF, temp_s3);
        if (arg0 & 0x2000) {
            ds->dispEnv[1].isrgb24 = 1;
            ds->dispEnv[0].isrgb24 = 1;
        } else {
            ds->dispEnv[1].isrgb24 = 0;
            ds->dispEnv[0].isrgb24 = 0;
        }
        Display_State.drawEnv[1].ofs[0] = temp_s5 >> 1;
        Display_State.drawEnv[0].ofs[0] = temp_s5 >> 1;
        temp_v0_2                       = temp_s4 >> 1;
        Display_State.drawEnv[0].ofs[1] = temp_v0_2;
        Display_State.drawEnv[1].ofs[1] = (temp_s4 + temp_v0_2) + 0x20;
        Display_State.drawEnv[1].clip.y = temp_s4 + 0x20;
        Display_State.drawEnv[1].clip.x = 0;
        Display_State.drawEnv[0].clip.x = 0;
        Display_State.drawEnv[0].clip.y = 0;
        Display_State.drawEnv[1].clip.w = temp_s5;
        Display_State.drawEnv[0].clip.w = temp_s5;
        Display_State.drawEnv[1].clip.h = temp_s4;
        Display_State.drawEnv[0].clip.h = temp_s4;
        Display_State.drawEnv[1].dfe    = 1;
        Display_State.drawEnv[0].dfe    = 1;
    } else {
        SetDefDrawEnv(&ds->drawEnv[0], 0, 0, (temp_s0 = temp_s5, temp_s5 & 0xFFFF), temp_s3);
        SetDefDispEnv(&ds->dispEnv[0], 0, 0, temp_s5 & 0xFFFF, temp_s3);
        SetDefDrawEnv(&new_var->drawEnv[1], 0, 0, temp_s5 & 0xFFFF, temp_s3);
        SetDefDispEnv(&ds->dispEnv[1], 0, 0, temp_s5 & 0xFFFF, temp_s3);
        temp_s0               = (temp_s5 & 0xFFFF) >> 1;
        ds->drawEnv[1].ofs[0] = (temp_s5 & 0xFFFF) >> 1;
        ds->drawEnv[0].ofs[0] = (temp_s5 & 0xFFFF) >> 1;
        ds->drawEnv[1].ofs[1] = 0xF0;
        ds->drawEnv[0].ofs[1] = 0xF0;
        ds->drawEnv[1].clip.x = 0;
        ds->drawEnv[0].clip.x = 0;
        ds->drawEnv[1].clip.y = 0;
        ds->drawEnv[0].clip.y = 0;
        ds->drawEnv[1].clip.w = temp_s5;
        ds->drawEnv[0].clip.w = temp_s5;
        ds->drawEnv[1].clip.h = temp_s4;
        ds->drawEnv[0].clip.h = temp_s4;
        ds->drawEnv[1].dfe    = 0;
        ds->drawEnv[0].dfe    = 0;
    }
    Display_State.drawEnv[1].dtd = 1;
    Display_State.drawEnv[0].dtd = 1;
    if (arg0 & 0x4000) {
        Display_State.drawEnv[1].isbg = 0;
        Display_State.drawEnv[0].isbg = 0;
    } else {
        Display_State.drawEnv[1].isbg = 1;
        Display_State.drawEnv[0].isbg = 1;
        Display_State.drawEnv[1].r0   = 0;
        Display_State.drawEnv[0].r0   = 0;
        Display_State.drawEnv[1].g0   = 0;
        Display_State.drawEnv[0].g0   = 0;
        Display_State.drawEnv[1].b0   = 0;
        Display_State.drawEnv[0].b0   = 0;
    }
    Display_State.dispEnv[1].isinter = var_s7;
    Display_State.dispEnv[0].isinter = var_s7;
    if (!(arg0 & 0x8000)) {
        Gfx_InitCoordinateTrees();
        Gpu_InitDefaultLights();
    }
}

void Display_SetAutoClear(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 < 0) {
        Display_State.drawEnv[1].isbg = 0;
        Display_State.drawEnv[0].isbg = 0;
        return;
    }
    Display_State.drawEnv[1].isbg = 1;
    Display_State.drawEnv[0].isbg = 1;
    Display_State.drawEnv[1].r0   = arg0;
    Display_State.drawEnv[0].r0   = arg0;
    Display_State.drawEnv[1].g0   = arg1;
    Display_State.drawEnv[0].g0   = arg1;
    Display_State.drawEnv[1].b0   = arg2;
    Display_State.drawEnv[0].b0   = arg2;
}

void Display_SetModeDefault(void)
{
    Display_SetMode(0x1010);
}

void Display_ClampField126(s8 arg0)
{
    s8 val;

    val = arg0;
    if (arg0 >= 8) {
        val = 8;
    } else if (arg0 < -7) {
        val = -8;
    }
    Display_State.field_126 = val;
}
