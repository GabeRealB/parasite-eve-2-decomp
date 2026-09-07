#include "common.h"

#include "main/unknown_syms.h"
#include "main/fs.h"
#include "main/loadui.h"
#include "main/text.h"
#include "main/wipsys.h"
#include "psyq/libetc.h"

void CdCmd_EnqueueLoadFile(s32 arg0, s32 arg1, s32 arg2)
{
    s8             param2[4];
    u8*            param1;
    void**         scratch;
    void*          head;
    register void* temp asm("v0");
    u8             f74;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    temp     = (u8*)head - 8;
    param1   = temp;
    *scratch = temp;

    param1[2]       = 2;
    param1[3]       = 0;
    ((u8*)head)[-8] = arg1;
    param2[0]       = arg0;

    if ((u8)arg2 < 5) {
        switch ((u8)arg2) {
            case 0:
                param2[1] = 3;
                param2[2] = -8;
                param2[3] = -3;
                break;
            case 1:
                param2[2] = 0;
                param2[1] = 0;
                param2[3] = -2;
                break;
            case 2:
                param2[1] = 3;
                param2[2] = 0;
                param2[3] = -2;
                break;
            case 3:
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                break;
            case 4:
                if (D_800626E8 != 0) {
                    param1[3] = Game_Session->field_7;
                    param1[2] = Game_Session->field_6;
                    param1[0] = Gp_GetViewIndex();
                    f74       = Game_Session->field_74;
                    param2[1] = 1;
                    param2[3] = 0;
                    param2[2] = 0;
                    param2[0] = f74;
                    CdCmd_EnqueueUnlessStream(0x21, param1, (u8*)param2);
                    D_800626E8 = 0;
                }
                *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
                return;
        }
    }

    CdCmd_Enqueue(0x21, param1, (u8*)param2);
    D_800626E8              = 1;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

s32 func_80042500(void)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    switch (D_8007A394) {
        case 0:
            D_8007A393 = Fs_GetStageDiskKind();
            if (D_8007A393 == 0) {
                break;
            }
            SndEvt_EnqueueType2(0, 8);
            SndEvt_EnqueueType7(0x80000000, 0x78);
            SndEvt_EnqueueType7(0x60010001, 0x78);
            Display_State.field_11e = 0xFF;
            Display_State.field_100 = 0;
            if (D_8007A393 == 1) {
                CdCmd_EnqueueLoadFile(1, 0x3C, 3);
                D_8007A392 = 0;
            }
            if (D_8007A393 == 2) {
                CdCmd_EnqueueLoadFile(1, 0x3D, 3);
                D_8007A392 = 1;
            }
            D_8007A390       = 5;
            queue->field_244 = 1;
            D_8007A394++;
            return 0xFF;
        case 1:
            if (CdCmd_IsIdle()) {
                Fs_StopCd();
                Display_State.field_100 = 1;
                D_8007A394++;
            }
            return 0xFF;
        case 2:
            Prim_DrawLoadingSprt();
            D_8007A390--;
            if ((D_8007A390 & 0x7FFF) == 0) {
                if (D_8007A390 & 0x8000) {
                    if (D_8007A393 == 1) {
                        D_8007A392 = 0;
                    } else if (D_8007A393 == 2) {
                        D_8007A392 = 1;
                    }
                    D_8007A390 = 5;
                    return 0xFF;
                } else {
                    D_8007A394++;
                }
            }
            return 0xFF;
        case 3:
            if ((u8)Fs_WaitDiskSwap() == 0xFF) {
                Fs_ClearDiskError();
                D_8007A392 = 2;
                D_8007A390 = 0x8080;
                D_8007A394 = 1;
                return 0xFF;
            } else {
                Fs_ClearDiskError();
                D_8007A394++;
                return 0xFF;
            }
        case 4:
            Fs_ScanIsoDirectory(0);
            if (Wip_SysFlags.field_0 != 0) {
                while (Fs_CdOpStatus != 0xFF) {
                    if (Fs_CdOpStatus == 0x80) {
                        return 0xFF;
                    }
                    VSync(0);
                }
                Fs_ClearDiskError();
            }
            if (Wip_SysFlags.field_0 != D_8007A393) {
                D_8007A392 = 2;
                D_8007A390 = 0x8080;
                D_8007A394 = 1;
                return 0xFF;
            } else {
                D_8007A390 = 5;
                D_8007A394++;
                return 0xFF;
            }
        case 5:
            D_8007A390--;
            if (D_8007A390 == 0) {
                Display_State.field_100 = 0;
                Display_State.field_11e = 0;
                queue->field_244        = 0;
                break;
            }
            return 0xFF;
    }
    return 0;
}

void Prim_DrawLoadingSprt(void)
{
    SPRT*     p;
    DR_TPAGE* dr;
    u8        mode;

    p              = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setlen(p, 4);
    setcode(p, 0x67);
    p->clut = GetClut(0, 0xFF);

    mode = D_8007A392;
    switch (mode) {
        case 0:
            p->w  = 0x9F;
            p->h  = 0x10;
            p->x0 = -0x50;
            p->u0 = 0;
            p->v0 = 0;
            p->y0 = 0x32;
            break;
        case 1:
            p->v0 = 0x10;
            p->w  = 0x9F;
            p->h  = 0x10;
            p->x0 = -0x50;
            p->u0 = 0;
            p->y0 = 0x32;
            break;
        case 2:
            p->v0 = 0x20;
            p->w  = 0x68;
            p->h  = 0x10;
            p->x0 = -0x32;
            p->u0 = 0;
            p->y0 = 0x32;
            break;
    }

    addPrim(Gpu_CurrentOt - 0x10, p);

    dr             = (DR_TPAGE*)Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setDrawTPage(dr, 0, 0, 0xF);
    addPrim(Gpu_CurrentOt - 0x10, dr);
}

void Snd_ApplyVolumeTable(s32 arg0)
{
    GBytes4 sp10;
    u8      temp;

    sp10 = D_80013F18;
    if (Mc_SaveData.field_1a9 == 0) {
        CdVol_SetMixMode(1);
    } else {
        CdVol_SetMixMode(0);
    }
    if ((arg0 & 0xFFFF) != 0) {
        D_8007A396 = arg0;
        if (D_80062737 != 0) {
            SndEvt_EnqueueType5(D_80062737, (u8)D_8007A396);
        } else {
            SndEvt_EnqueueType5(0, (u8)D_8007A396);
        }
    } else {
        temp       = sp10.data[Mc_SaveData.field_1aa];
        D_8007A396 = temp;
        if ((s8)Mc_SaveData.field_1aa == 3) {
            SndEvt_EnqueueType5Pending();
        } else {
            SndEvt_FlushType5Pending();
        }
        if (D_80062737 != 0) {
            SndEvt_EnqueueType5(D_80062737, (u8)D_8007A396);
        } else {
            SndEvt_EnqueueType5(0, sp10.data[Mc_SaveData.field_1aa]);
        }
    }
}
