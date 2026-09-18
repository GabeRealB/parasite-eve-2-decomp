#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

extern void           func_800E8614(s32 arg0, s32 arg1);
extern void           func_800E8634(s32 arg0, s32 arg1, s32 arg2);
extern s32            func_800E3FCC(s32 arg0);
extern s8             D_80071090;
extern s8             D_8007272D;
extern s32            D_dryfield_night_trailer_coach_8018794C;
extern s32            D_dryfield_night_trailer_coach_801879B8;
extern s32            D_dryfield_night_trailer_coach_80187CEC;
extern s32            D_dryfield_night_trailer_coach_80187D00;
extern s32            D_dryfield_night_trailer_coach_80187F58;
extern s32            D_dryfield_night_trailer_coach_801880A8;
extern s32            D_dryfield_night_trailer_coach_801881F8;
extern s32            D_dryfield_night_trailer_coach_80188348;
extern s32            D_dryfield_night_trailer_coach_80188510;
extern s32            D_dryfield_night_trailer_coach_80188708;
extern s32            D_dryfield_night_trailer_coach_80188858;
extern s32            D_dryfield_night_trailer_coach_801889A8;
extern s32            D_dryfield_night_trailer_coach_80188F00;
extern s32            D_dryfield_night_trailer_coach_80189080;
extern s32            D_dryfield_night_trailer_coach_801892C0;
extern GpAreaApplyRec D_dryfield_night_trailer_coach_8018C208;

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach_2", RoomsShared8017d878Table);

void func_dryfield_night_trailer_coach_8018231C(Task* task)
{
    task->field_24 = &D_dryfield_night_trailer_coach_8018794C;
    Game_SetPtrSlot(task, 7);
    if (Game_GetPtrSlot(0xA) != NULL) {
        Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_night_trailer_coach_801879B8, 0);
        Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_night_trailer_coach_80187CEC, 0);
    }
    if (Mc_SaveData.field_8 == 2) {
        func_800E8634((s32)&D_dryfield_night_trailer_coach_80187D00, 0, (s32)&D_dryfield_night_trailer_coach_80187F58);
    }
    if (Mc_SaveData.field_8 == 3) {
        func_800E8634((s32)&D_dryfield_night_trailer_coach_80189080, 0, (s32)&D_dryfield_night_trailer_coach_801892C0);
        Gp_SetCurBit2Flag(0x22, 1);
    }
    if (func_800E3FCC(0xA2) == 0x25) {
        func_800E3FAC(0xA2, 0x26);
    }
    D_80071090                 = 3;
    *(s32*)((u8*)task + 0x30) += 1;
}

void func_dryfield_night_trailer_coach_8018243C(Task* task)
{
    register s32 cap asm("a0");
    s32          flag;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            if (GameFlag_GetNibble(0x61) == 0) {
                func_800E8614((s32)&D_dryfield_night_trailer_coach_801880A8, 1);
                SOFT_BARRIER();
                goto inc;
            }
            if (GameFlag_GetNibble(0xE0) == 0) {
                if (GameFlag_GetNibble(0x7A) >= 4) {
                    GameFlag_SetNibble(0xE0, 1);
                    func_800E8614((s32)&D_dryfield_night_trailer_coach_80188510, 0);
                    goto kill;
                }
            }
            func_800E8614((s32)&D_dryfield_night_trailer_coach_80188348, 0);
        kill:
            Task_Kill(task);
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0xB) {
                cap  = (s32)&D_dryfield_night_trailer_coach_801881F8;
                flag = 0;
                goto do_8614;
            }
            if (Gp_GetCapEventKey() == 0xC) {
                cap = (s32)&D_dryfield_night_trailer_coach_80188708;
                goto a1_1;
            }
            if (Gp_GetCapEventKey() == 0xD) {
                if (GameFlag_GetNibble(0x5B) == 0) {
                    GameFlag_SetNibble(0x5B, 1);
                    GameFlag_SetNibble(0x4C, 0);
                    Gp_ApplyAreaRecs(&D_dryfield_night_trailer_coach_8018C208);
                    func_800E8634((s32)&D_dryfield_night_trailer_coach_801889A8, 1,
                                  (s32)&D_dryfield_night_trailer_coach_80188F00);
                    func_800E3FAC(0xA2, 0x13);
                    D_8007272D = 2;
                } else {
                    cap = (s32)&D_dryfield_night_trailer_coach_80188858;
                a1_1:
                    flag = 1;
                do_8614:
                    func_800E8614(cap, flag);
                }
            }
            goto inc;
        case 1:
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
        inc:
            task->state = task->state + 1;
            return;
        case 4:
            Task_Kill(task);
            return;
    }
}
