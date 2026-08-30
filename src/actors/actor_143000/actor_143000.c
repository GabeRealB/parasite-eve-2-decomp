#include "common.h"

#include "actors/actor_143000.h"
#include "main/sound.h"
#include "main/task.h"
#include "psyq/strings.h"

extern s8               D_8007218B;
extern char             D_actor_143000_80131EB0[];
extern char             D_actor_143000_80131EBC[];
extern Actor143000Spawn D_actor_143000_80135C08;
extern char             D_actor_143000_80135C20[];

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80131F80);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801323E0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801324C8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801325F0);

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131E84);

INCLUDE_RODATA("actors/nonmatchings/actor_143000/actor_143000", D_actor_143000_80131EB0);

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_actor_143000_80131EBC\n"
        "D_actor_143000_80131EBC:\n"
        "\t.asciz \"YSD\"\n"
        "\t.align 2\n"
        "\t.asciz \"\"\n"
        "\t.align 2\n"
        ".section .text\n");
#endif

void func_actor_143000_80132A04(Actor143000* arg0)
{
    s32              var_s2;
    Actor143000Work* temp_s0;

    COMPILER_BARRIER();
    temp_s0 = arg0->field_1C;
    var_s2  = 0;
    if (arg0->field_2A == 0) {
        if ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EB0) == 0) || ((strcmp(D_actor_143000_80135C20, D_actor_143000_80131EBC) == 0) && (D_8007218B != 0))) {
            var_s2 = 1;
        }
        temp_s0->field_C = var_s2;
    }
    if (temp_s0->field_C != 0) {
        switch (arg0->field_2A) {
            case 0:
                temp_s0->field_12 = 2;
                break;
            case 0x3C:
                temp_s0->field_12 = 3;
                temp_s0->field_14 = 0x7C;
                break;
            case 0x46:
                temp_s0->field_14 = 0x83;
                break;
            case 0x50:
                temp_s0->field_14 = 0x8A;
                break;
            case 0x5A:
                temp_s0->field_14 = 0;
                break;
            case 0x78:
                SndEvt_EnqueueType6(0x541F0011, 0, 0);
                temp_s0->field_13 = 1;
                break;
            case 0x96:
                temp_s0->field_12 = 4;
                temp_s0->field_13 = 0;
                break;
            case 0xF0:
                temp_s0->field_12 = 5;
                break;
            case 0x14A:
                arg0->field_30                  = 0xA;
                D_actor_143000_80135C08.field_0 = 0;
                D_actor_143000_80135C08.field_1 = 0;
                D_actor_143000_80135C08.field_2 = 0xF;
                arg0->field_2A                  = 0xF;
                Task_Spawn(1, 0x31, 0, (s32)&D_actor_143000_80135C08);
                break;
        }
    } else {
        switch (arg0->field_2A) {
            case 0:
                temp_s0->field_12 = 2;
                break;
            case 0x3C:
                temp_s0->field_12 = 3;
                temp_s0->field_14 = 0x7C;
                break;
            case 0x46:
                temp_s0->field_14 = 0x83;
                break;
            case 0x50:
                temp_s0->field_14 = 0x8A;
                break;
            case 0x5A:
                temp_s0->field_14 = 0;
                break;
            case 0x78:
                SndEvt_EnqueueType6(0x541F0012, 0, 0);
                temp_s0->field_13 = 2;
                break;
            case 0x96:
                temp_s0->field_12 = 6;
                temp_s0->field_14 = 0x7C;
                temp_s0->field_13 = 0;
                break;
            case 0xA0:
                temp_s0->field_14 = 0x83;
                break;
            case 0xAA:
                temp_s0->field_14 = 0x8A;
                break;
            case 0xB4:
                temp_s0->field_14 = 0;
                break;
            case 0xD2:
                temp_s0->field_13 = 2;
                break;
            case 0xF0:
                temp_s0->field_12 = 7;
                temp_s0->field_13 = 0;
                break;
            case 0x14A:
                temp_s0->field_12 = 1;
                temp_s0->field_10 = 0;
                arg0->field_30    = 2;
                break;
        }
    }
    arg0->field_2A = (s16)((u16)arg0->field_2A + 1);
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80132D10);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133334);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133578);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801335C8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133664);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133698);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801336E8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133800);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801338C8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801338E0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801339CC);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133AC0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133AE8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133C2C);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133C90);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133CF0);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80133EE4);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801342F8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801344A8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_801344D8);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_8013450C);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_8013452C);

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000", func_actor_143000_80134538);
