#include "common.h"
#include "main/task.h"
#include "main/session.h"
#include "main/gameflag.h"
#include "gameplay/3CD8.h"

typedef struct {
    u8    _pad0[0x24];
    void* field_24;
    u8    _pad28[0x8];
    s32   field_30;
} DumpingHoleState;

extern TaskDesc D_shelter_b3_dumping_hole_80189ADC;
extern u8       D_shelter_b3_dumping_hole_80187574[];
extern u8       D_shelter_b3_dumping_hole_8018B080[];
extern u8       D_shelter_b3_dumping_hole_8018B428[];
extern u8       D_shelter_b3_dumping_hole_8018F4A4;
extern TaskDesc D_80164B78;

void func_shelter_b3_dumping_hole_80183198(s16 arg0, s16 arg1, s16 arg2);

s32 func_shelter_b3_dumping_hole_8017D870(void)
{
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80189ADC, 0, 0, 0);
    return 0;
}

void func_shelter_b3_dumping_hole_8017D8A0(DumpingHoleState* arg0)
{
    arg0->field_24 = D_shelter_b3_dumping_hole_80187574;
    Game_SetPtrSlot(arg0, 7);
    func_shelter_b3_dumping_hole_80183198(0x180, 0, 0);
    if (GameFlag_GetNibble(0x78) == 0) {
        if (Game_Session->field_9 == 1) {
            if (Game_Session->field_8 == 3) {
                func_800E8634((s32)D_shelter_b3_dumping_hole_8018B080, 0,
                              (s32)D_shelter_b3_dumping_hole_8018B428);
            }
            func_800E3FAC(0xA2, 0x21);
            GameFlag_SetNibble(0x78, 1);
        }
    }
    if (Game_Session->field_5 >= 2) {
        Task_SpawnFromTable(&D_80164B78, 0, 0, 0);
    }
    arg0->field_30                    += 1;
    D_shelter_b3_dumping_hole_8018F4A4 = 0;
}
