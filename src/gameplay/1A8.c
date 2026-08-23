#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

void func_800AD6BC(void);

extern TaskFuncTable3   D_80093950;
extern GpVoidFuncTable6 D_80093978;
extern GpVoidFuncTable5 D_80093990;
extern u16              D_80114CD0;
extern u16              Gp_DirFlags;
extern u16              D_80114CD4;
extern u16              Gp_DirPhase;
extern u8               Gp_DirByte;
extern u8               Gp_DirNibble;
extern u8               Gp_DirAlt;
extern u8               Gp_DirAltNibble;
extern u8               D_80114CDC;
extern u8               D_80114CDD;
extern u8               D_80114CDE;
extern s16              D_80114CE0;
extern u16              D_80114CF6;
extern u8               D_80114CF8;
extern s16              D_80114D08;
extern u16              D_8017A738[];
extern u16              D_8017A824[];
extern u16              D_8017A9A0[];
extern u16              D_8017AA0C[];
extern u16              D_8017AD88[];

s16 Gp_LookupStageFlag(s32 arg0)
{
    u16* table;
    u16* entry;

    switch (Game_Session->field_7) {
        case 1:
            arg0 = (s16)arg0;
            if (arg0 >= 0xE) {
                goto fail;
            }
            table = D_8017AA0C;
            break;
        case 2:
            arg0 = (s16)arg0;
            if (arg0 >= 0x1D) {
                goto fail;
            }
            table = D_8017A824;
            break;
        case 3:
            arg0 = (s16)arg0;
            if (arg0 >= 0x1E) {
                goto fail;
            }
            if (arg0 == 0x1D) {
                if (GameFlag_GetNibble(0x7F) == 0) {
                    return 0;
                }
                return 0x802;
            }
            table = D_8017A738;
            break;
        case 4:
            if ((s16)arg0 >= 0x1E) {
                goto fail;
            }
            if ((s16)arg0 == 0) {
                if (GameFlag_GetNibble(0x7A) == 6) {
                    return GameFlag_GetNibble(D_8017AD88[0] & 0x7FF) + 0x800;
                }
            }
            table  = D_8017AD88;
            arg0 <<= 16;
            arg0 >>= 15;
            goto lookup;
        case 5:
            arg0 = (s16)arg0;
            if (arg0 >= 9) {
                goto fail;
            }
            table = D_8017A9A0;
            break;
        default:
            goto fail;
    }

    arg0 <<= 1;
lookup:
    arg0 += (s32)table;
    entry = (u16*)arg0;
    return GameFlag_GetNibble(*entry & 0x7FF) + (*entry & 0x800);
fail:
    return -1;
}

void Gp_ClearAreaFlag4(GpAreaKey* arg0)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = Gp_AreaTables[arg0->field_3];
    if (rec != NULL) {
        obj = rec[arg0->field_2].field_4;
        if (obj != NULL) {
            obj->field_1 &= 0xFB;
        }
    }
}

void Gp_InitDirState(Task* arg0)
{
    D_80114CDE       = 0;
    D_80114CDD       = 0;
    Gp_DirFlags      = 0;
    D_80114CD0       = 0;
    D_80114CDC       = 0;
    Gp_DirByte       = 0;
    Gp_DirNibble     = 0;
    Gp_DirPhase      = 0;
    D_80114CF8       = 0;
    D_80114CE0       = 1;
    Gp_AreaIdBits[0] = 0;
    Gp_AreaIdBits[1] = 0;
    D_80114D08       = 0xA;
    arg0->state++;
}

void func_800AEE00(void)
{
    Gp_CommitObj4CSave();
    func_800AD6BC();
}

s32 Gp_YawToPosXZ(Task* arg0, GpPosXZ* arg1)
{
    SVECTOR    vec;
    GpCoordXZ* coord;

    coord  = (GpCoordXZ*)((GameActorExt*)arg0->extra)->field_8;
    vec.vx = arg1->vx - coord->field_18;
    vec.vy = 0;
    vec.vz = arg1->vz - coord->field_20;
    VectorNormalSS(&vec, &vec);
    return ratan2(vec.vx, vec.vz) & 0xFFF;
}

void func_800AEE8C(Task* arg0)
{
    TaskFuncTable3 sp;
    void*          slot;

    slot = Game_GetPtrSlot(3);
    sp   = D_80093950;
    if (slot != NULL) {
        sp.funcs[arg0->state](arg0);
    }
}

u8 Gp_GetViewCountLo(void)
{
    GameSession*    session;
    GpViewCountTbl* tbl;

    session = Game_Session;
    tbl     = Gp_ViewCountTables[session->field_7 - 1];
    return tbl->field_0[session->field_6 - 1][session->field_5 - 1].field_0;
}

void func_800AEF4C(void)
{
    GpVoidFuncTable6 sp;

    sp = D_80093978;
    sp.funcs[*(s16*)&Gp_DirPhase]();
}

void func_800AEFBC(void)
{
    GpVoidFuncTable5 sp;

    sp = D_80093990;
    if (Gp_StateF0.field_0 == 2) {
        if (D_80114CDE == 1) {
            D_80114CDD = D_80114CDE;
        }
    }
    if (D_80114CDD != 0) {
        Gp_StateF0.field_1 = 0x3C;
    }
    sp.funcs[*(s16*)&Gp_DirPhase]();
}

void Gp_ClearDirCursor(void)
{
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    D_80114CF8      = 0;
}

void Gp_PostMsg13EF(void)
{
    GpMsg13EF sp;
    void*     slot;

    if (Game_Session->field_1 == 0) {
        if (Gp_CapBusy() == 0) {
            sp.field_0 = Gp_DirFlags;
            sp.field_2 = Gp_DirByte;
            sp.field_3 = Gp_DirNibble;
            slot       = Game_GetPtrSlot(7);
            Gp_DispatchMsg(slot, 0x13EF, (s32)&sp, 0);
        }
    }
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    D_80114CF8      = 0;
    if (D_80114CDC == 0) {
        Game_Session->field_13A = 0;
    }
}

void Gp_SpawnEvt1IfCapIdle(void)
{
    if (Game_Session->field_1 == 0) {
        if (Gp_CapBusy() == 0) {
            Gp_SpawnEvt1(Gp_DirByte, Gp_DirNibble);
        }
    }
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
}

void Gp_FadeDirAdvance(void)
{
    u8 fade;

    if (*(s16*)&D_80114CF6 != 0) {
        fade = *(u8*)&D_80114CF6;
        Fade_DrawOverlay(fade, fade, fade, 2);
        D_80114CF6 += 0x1E;
        if ((s16)D_80114CF6 >= 0x100) {
            D_80114CF6 = 0xFF;
        }
    }
    Gp_DirPhase++;
}

void Gp_CommitSaveLoc(void)
{
    u8 fade;

    if (*(s16*)&D_80114CF6 != 0) {
        fade = *(u8*)&D_80114CF6;
        Fade_DrawOverlay(fade, fade, fade, 2);
    }
    Mc_SaveData.field_6 = Gp_WarpLoc.field_0;
    Mc_SaveData.field_8 = Gp_WarpLoc.field_2;
    Mc_SaveData.field_5 = Gp_WarpLoc.field_3;
    Task_Spawn(0, 0x11, 0, 0);
    D_80114CF8   = 0;
    Gp_DirNibble = 0;
    Gp_DirByte   = 0;
    Gp_DirFlags  = 0;
}

void Gp_MsgPlayer3EE(void)
{
    GpMsg3EE sp;
    void*    slot;

    slot = Game_GetPtrSlot(3);
    if (Game_Session->field_1 != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
        D_80114CDD      = 0;
    } else {
        sp.field_10 = 0;
        sp.field_14 = 0;
        sp.field_12 = Gp_DirNibble << 4;
        Gp_DispatchMsg(slot, 0x3EE, (s32)&sp, 0);
        Gp_DirPhase++;
    }
}

void Gp_MsgPlayer3F0(void)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        Gp_DirPhase++;
    }
}

void Gp_MsgPlayer3EF(void)
{
    s32   sp[2];
    void* slot;

    slot  = Game_GetPtrSlot(3);
    sp[0] = (Gp_DirFlags >> 8) & 1;
    sp[1] = Gp_DirByte & 0xF;
    Gp_DispatchMsg(slot, 0x3EF, (s32)sp, 0);
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    Gp_DirPhase++;
}

void Gp_SetCurAreaFlag4(void)
{
    GpAreaKey* key;
    GpAreaRec* rec;
    GpAreaObj* obj;

    key = (GpAreaKey*)&Game_Session->field_4;
    rec = Gp_AreaTables[key->field_3];
    if (rec != NULL) {
        obj = rec[key->field_2].field_4;
        if (obj != NULL) {
            obj->field_1 |= 0x4;
        }
    }
}

void Gp_ApplyAreaFlag4List(s16 arg0, GpAreaFlagRec* arg1)
{
    GpAreaRec* rec;
    GpAreaObj* obj;

    rec = Gp_AreaTables[arg0];
    if (rec != NULL) {
        for (; arg1->field_0 != 0xFF; arg1++) {
            if (arg1->field_1 != 0) {
                obj = rec[arg1->field_0].field_4;
                if (obj != NULL) {
                    obj->field_1 |= 0x4;
                }
            }
        }
    }
}
