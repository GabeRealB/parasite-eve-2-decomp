#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgte.h>

#define gte_rtv0_real()   __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real()   __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtir_real()   __asm__ volatile("nop; nop; .word 0x4A49E012")
#define gte_rtirtr_real() __asm__ volatile("nop; nop; .word 0x4A498012")
#define gte_gpf12_real()  __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_gpl12_real()  __asm__ volatile("nop; nop; .word 0x4BA8003E")

#define gte_MulMatrix0_real(r1, r2, r3) \
    {                                   \
        gte_SetRotMatrix(r1);           \
        gte_ldclmv(r2);                 \
        gte_rtir_real();                \
        gte_stclmv(r3);                 \
        gte_ldclmv((char*)(r2) + 2);    \
        gte_rtir_real();                \
        gte_stclmv((char*)(r3) + 2);    \
        gte_ldclmv((char*)(r2) + 4);    \
        gte_rtir_real();                \
        gte_stclmv((char*)(r3) + 4);    \
    }

void func_800C2140(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);

extern s32        Gp_LcgState;
extern GpItemRec* Gp_SelItemRec;

s32 func_800D5B14(GpItemRec* arg0)
{
    WipSysConfig* cfg;
    GameActor*    actor;
    GpItemScan*   scanEquip;
    GpItemScan*   scanQty;
    GpItemScan*   scanRel;
    GpItemScan*   scanFree;
    GpItemScan*   scanId;
    GpItemSlot*   slot;
    GpItemRec*    table;
    GpItemRec*    rec;
    GpItemRec*    found;
    s32           id;
    s32           ret;
    s32           flag;
    s32           i;
    u8            count;
    s32           held;
    s32           prevId;
    s32           relId;
    s32           qty;
    s32           k;
    s32           avail;
    s32           slotNum;
    s32           sel;
    GpItemRec*    hit;

    ret   = 0;
    flag  = 1;
    id    = arg0->field_0;
    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    cfg   = &Wip_SysConfig;

    if (id != 0) {
        if ((u32)(id - 0x80) < 0x20U) {
            if (actor->field_954 != 2) {
                rec       = NULL;
                scanEquip = &Mc_SaveData.field_5BC;
                prevId    = cfg->field_21 + 0x7F;

                cfg->field_21 = id - 0x7F;

                table = Gp_GetItemTable(scanEquip);
                table = &table[scanEquip->field_0];
                count = scanEquip->field_1;
                for (i = 0; i < count; i++) {
                    if (table->field_0 == prevId) {
                        rec = table;
                    }
                    table++;
                }
                if (rec != NULL) {
                    rec->field_1 = arg0->field_1;
                    Gp_RefreshItemRow(arg0);
                }
                Gp_SetItemSeenBit(id, 1);
            }
            ret = 1;
        } else if ((u32)(id - 0xA0) < 0x20U) {
            relId = 0;
            qty   = 0;
            held  = cfg->field_21 + 0x7F;
            slot  = Gp_GetItemSlot(held);
            if (Gp_EquipRelatedBank(0, held, id, 0) == 0) {
                Gp_PendingRelatedId = id;
                D_8010F890          = flag;
                relId               = slot->field_0;
                if (relId != id) {
                    cfg->field_22 = id + 0x61;
                    slot->field_0 = id;
                    slot->field_1 = 0;
                }
                Gp_SetItemSeenBit(id, 1);
                ret = 1;
            } else if (Gp_EquipRelatedBank(1, held, id, 0) == 0) {
                Gp_PendingRelatedId = -id;
                D_8010F890          = flag;
                relId               = slot->field_2;
                if (relId != id) {
                    slot->field_2 = id;
                    slot->field_3 = 0;
                }
                Gp_SetItemSeenBit(id, 1);
                ret = 1;
            }

            if (relId != 0 && relId != 0xFF) {
                scanQty = &Mc_SaveData.field_5BC;
                qty     = Gp_ScanStackQty(scanQty, relId);
                qty    -= Gp_CountEquippedRelated(scanQty, relId);
            }
            if (qty > 0) {
                scanRel = &Mc_SaveData.field_5BC;
                hit     = NULL;
                table   = Gp_GetItemTable(scanRel);
                i       = 0;
                table   = &table[scanRel->field_0];
                count   = scanRel->field_1;
                for (; i < count; i++) {
                    if (table->field_0 == relId) {
                        hit = table;
                    }
                    table++;
                }
                found = hit;
                if (found != NULL && (s8)found->field_1 == 0) {
                    slotNum  = -1;
                    scanFree = &Mc_SaveData.field_5BC;
                    Gp_GetItemTable(scanFree);
                    for (k = 0; k < 3; k++) {
                        avail = 1;
                        table = Gp_GetItemTable(scanFree);
                        i     = 0;
                        table = &table[scanFree->field_0];
                        count = scanFree->field_1;
                        for (; i < count; i++) {
                            if (table->field_0 != 0 && (s8)table->field_1 == k + 1) {
                                avail = 0;
                                break;
                            }
                            table++;
                        }
                        if (avail == 1) {
                            slotNum = k + 1;
                            break;
                        }
                    }

                    if (slotNum == -1) {
                        scanId = &Mc_SaveData.field_5BC;
                        hit    = NULL;
                        table  = Gp_GetItemTable(scanId);
                        i      = 0;
                        table  = &table[scanId->field_0];
                        count  = scanId->field_1;
                        for (; i < count; i++) {
                            if (table->field_0 == id) {
                                hit = table;
                            }
                            table++;
                        }
                        if (hit != NULL) {
                            sel            = (s8)hit->field_1;
                            hit->field_1   = 0;
                            found->field_1 = sel;
                        }
                    } else {
                        found->field_1 = slotNum;
                    }
                }
            }
        } else if ((u32)(id - 0x60) >= 0x20U) {
            D_8010F894 = id;

            if ((u32)(id - 1) < 0x41U) {
                switch (id) {
                    case 1:
                    case 2:
                    case 3:
                        if (cfg->field_18 < cfg->field_1a) {
                            if (id == 1) {
                                cfg->field_18 += 0x2D;
                            } else if (id == 2) {
                                cfg->field_18 += 0x5A;
                            } else {
                                cfg->field_18 += 0x96;
                            }
                            if (cfg->field_18 > cfg->field_1a) {
                                cfg->field_18 = cfg->field_1a;
                            }
                            D_8010F888 = 1;
                            ret        = 1;
                        }
                        break;
                    case 0x3C:
                        if ((u32)Mc_SaveData.field_26 < 0xFAU) {
                            Mc_SaveData.field_26 += 5;
                        }
                        Gp_RecalcMaxHp();
                        D_8010F888    = 1;
                        ret           = 1;
                        cfg->field_18 = cfg->field_1a;
                        break;
                    case 4:
                        func_8010A1B0(1, 0xD0);
                        ret = D_8010F888 = Gp_StateC08.field_16 = 1;
                        break;
                    case 8:
                        func_8010A1B0(1, 7);
                        ret = D_8010F888 = Gp_StateC08.field_17 = 1;
                        break;
                    case 5:
                        if (cfg->field_1c < cfg->field_1e || cfg->field_18 < cfg->field_1a) {
                            cfg->field_1c += 0x50;
                            cfg->field_18 += 0x14;
                            if (cfg->field_1c > cfg->field_1e) {
                                cfg->field_1c = cfg->field_1e;
                            }
                            if (cfg->field_18 > cfg->field_1a) {
                                cfg->field_18 = cfg->field_1a;
                            }
                            D_8010F888 = 1;
                            ret        = 1;
                        }
                        break;
                    case 6:
                    case 7:
                        if (cfg->field_1c < cfg->field_1e) {
                            if (id == 6) {
                                cfg->field_1c += 0x19;
                            } else {
                                cfg->field_1c += 0x64;
                            }
                            if (cfg->field_1c > cfg->field_1e) {
                                cfg->field_1c = cfg->field_1e;
                            }
                            D_8010F888 = 1;
                            ret        = 1;
                        }
                        break;
                    case 0x3A:
                    case 0x3B:
                        func_800A7CB0((u8)((id - 0x3A) * 3 + 0x2E));
                        func_800A7DB8(id - 0x2B);
                        Gp_SelItemRec = arg0;
                        flag          = 0;
                        ret           = 1;
                        break;
                    case 0x41:
                        func_800A7CB0(0x34);
                        func_800A7DB8(0x11);
                        Gp_SelItemRec = arg0;
                        flag          = 0;
                        ret           = 1;
                        break;
                    case 0x3D:
                        if (cfg->field_1c < cfg->field_1e || cfg->field_18 < cfg->field_1a) {
                            cfg->field_1c = cfg->field_1e;
                            D_8010F888    = 1;
                            cfg->field_18 = cfg->field_1a;
                        }
                        D_8010F888 = 1;
                        ret        = 1;
                        break;
                    case 0x3E:
                        D_8010F888 = 1;
                        ret        = 1;
                        break;
                }
            }

            if (ret == 1 && flag != 0) {
                arg0->field_0 = 0;
                arg0->field_2 = 0;
                arg0->field_1 = 0;
                Gp_SetItemSeenBit(id, 1);
            }
        }
    }
    return ret;
}

s32 Gp_ItemIsUnusable(s32 arg0, GpItemRec* arg1)
{
    WipSysConfig* cfg;
    GpItemScan*   scan;
    s32           ret;
    s32           val;

    ret = 1;
    cfg = &Wip_SysConfig;
    if (arg0 != 0) {
        if ((u32)(arg0 - 0x80) < 0x20U) {
            ret = 0;
        } else if ((u32)(arg0 - 0xA0) < 0x20U) {
            scan = &Mc_SaveData.field_5BC;
            val  = arg1->field_2 - Gp_CountEquippedRelated(scan, arg0);
            if (val > 0) {
                if (Gp_EquipRelatedItem(scan, cfg->field_21 + 0x7F, arg0, 0) == 0) {
                    ret = 0;
                }
            }
        } else if ((u32)(arg0 - 1) < 0x41U) {
            switch (arg0) {
                case 1:
                case 2:
                case 3:
                    if (cfg->field_18 < cfg->field_1a) {
                        ret = 0;
                    }
                    break;
                case 4:
                    if (Gp_StateC08.field_16 == 0) {
                        ret = 0;
                    }
                    break;
                case 8:
                    if ((s8)Gp_StateC08.field_17 == 0) {
                        ret = 0;
                    }
                    break;
                case 5:
                    if (cfg->field_1c < cfg->field_1e) {
                        ret = 0;
                    } else if (cfg->field_18 < cfg->field_1a) {
                        ret = 0;
                    }
                    break;
                case 6:
                case 7:
                    if (cfg->field_1c < cfg->field_1e) {
                        ret = 0;
                    }
                    break;
                case 0x3A:
                case 0x3B:
                case 0x3C:
                case 0x41:
                    ret = 0;
                    break;
                case 0x3D:
                    if (cfg->field_1c < cfg->field_1e) {
                        ret = 0;
                    } else if (cfg->field_18 < cfg->field_1a) {
                        ret = 0;
                    }
                    break;
                case 0x3E:
                    if (func_800B9D80(0x140) == 0) {
                        ret = 0;
                    }
                    break;
            }
        }
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6334);

/* After Armor/Attachments from func_800D6334 so overlay .rodata stays packed. */
const char Gp_StrWeapon[] = {
    'W',
    'e',
    'a',
    'p',
    'o',
    'n',
    '\0',
    0x60,
};

s32 Gp_FlushPendingRelated(s32 arg0, s32 arg1)
{
    s32 val;

    val = Gp_PendingRelatedId;
    if (val <= 0) {
        if (val >= 0) {
            return -1;
        }
        val = -val;
    }
    Gp_PendingRelatedId = 0;
    return Gp_EquipRelatedItem(&Mc_SaveData.field_5BC, arg0, val, -1);
}

GpItemRec* Gp_FindItemById(s32 arg0)
{
    GpItemScan* scan;
    GpItemRec*  table;
    s32         i;
    s32         count;
    GpItemRec*  rec;

    rec   = NULL;
    scan  = &Mc_SaveData.field_5BC;
    table = Gp_GetItemTable(scan);
    i     = 0;
    table = &table[scan->field_0];
    count = scan->field_1;
    for (; i < count; i++) {
        if (table->field_0 == arg0) {
            rec = table;
        }
        table++;
    }
    return rec;
}

GpItemRec* Gp_FindItemByKind(s32 arg0)
{
    GpItemScan* scan;
    GpItemRec*  table;
    s32         i;
    s32         count;
    GpItemRec*  rec;

    rec   = NULL;
    scan  = &Mc_SaveData.field_5BC;
    table = Gp_GetItemTable(scan);
    i     = 0;
    table = &table[scan->field_0];
    count = scan->field_1;
    for (; i < count; i++) {
        if ((s8)table->field_1 == arg0 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    return rec;
}

GpItemRec* Gp_FindItemInScan(s32 arg0, GpItemScan* arg1)
{
    GpItemRec* table;
    s32        i;
    s32        count;
    GpItemRec* rec;

    rec   = NULL;
    table = Gp_GetItemTable(arg1);
    i     = 0;
    table = &table[arg1->field_0];
    count = arg1->field_1;
    for (; i < count; i++) {
        if (table->field_0 == arg0) {
            rec = table;
        }
        table++;
    }
    return rec;
}

void Gp_DrawWeaponLabel(Task* arg0)
{
    UiPanel* panel;
    s32      x;
    s32      y;

    panel            = arg0->spawnArg2;
    panel->field_C.y = 0x1C - Display_State.vramYOffset;
    Ui_InsetLayout(panel, NULL, NULL, 0);
    x = (s16)panel->field_1C;
    y = (s16)panel->field_18;
    func_800C2140(panel, x + 2, y + 0xF, 1);
    Ui_DrawText(panel, Gp_StrWeapon);
}

void Gp_UpdateRoomCoords(Task* arg0)
{
    register Task*           task asm("s7");
    register GpRoomCoordSet* set asm("s5");
    register SVECTOR*        vec asm("s4");
    GsCOORDINATE2*           parent6C;
    register s32             i asm("s1");
    s32                      j;
    u16                      tmp;

    task = arg0;
    {
        register GameSession* gs asm("a0");

        gs  = Game_Session;
        set = (GpRoomCoordSet*)Gp_GetRoomCoordSet((GameSessionFrom4*)&gs->field_4);
    }
    if (set == NULL) {
        Task_Kill(task);
        return;
    }

    {
        register void** scratch asm("v0");
        register u8*    head asm("v1");

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = (u8*)*scratch;
        head    -= 0x1C;
        *scratch = head;
        vec      = (SVECTOR*)head;
        if (task->state == 0) {
            {
                register GpCoord60* p asm("a0");
                register GpCoord60* cur asm("s0");
                GsCOORDINATE2*      parent;

                p = set->arr60;
                if (set->n60 > 0) {
                    i      = 0;
                    parent = &D_80070F10;
                    do {
                        cur            = p;
                        i             += 1;
                        cur->coord.sub = parent;
                        cur->coord.flg = 0;
                        p              = cur + 1;
                    } while (i < set->n60);
                }
            }

            {
                register GpCoord6C*   obj asm("s3");
                register GpCoord6C*   cur asm("s0");
                register Gp6CDirWalk* dirw asm("s2");
                register Gp6CMatWalk* matw asm("s6");

                obj = set->arr6C;
                i   = 0;
                if (set->n6C > 0) {
                    parent6C = &D_80070F10;
                    dirw     = (Gp6CDirWalk*)&obj->dir;
                    matw     = (Gp6CMatWalk*)&obj->coord.coord;
                    do {
                        cur                                                    = obj;
                        ((Gp6CMid*)((u8*)dirw - OFFSET_OF(Gp6CMid, dir)))->sub = parent6C;
                        if (dirw->dir.vy != 0) {
                            goto perp;
                        }
                        if (dirw->dir.vz == 0) {
                            goto along_x;
                        }
                    perp:
                        vec->vx = 0;
                        vec->vy = -(s16)(u16)dirw->dir.vz;
                        vec->vz = (u16)dirw->dir.vy;
                        goto join;
                    along_x:
                        vec->vx = (u16)dirw->dir.vy;
                        tmp     = (u16)dirw->dir.vx;
                        vec->vz = 0;
                        vec->vy = -(s16)tmp;
                    join:
                        Gfx_OrthonormalBasis(&matw->mtx, &dirw->dir, vec);
                        i             += 1;
                        dirw          += 1;
                        matw          += 1;
                        cur->coord.flg = 0;
                        obj           += 1;
                    } while (i < set->n6C);
                }
            }

            j = 0;
            if (set->n58 > 0) {
                register GpCoord58* p asm("a0");
                register GpCoord58* cur asm("s0");
                GsCOORDINATE2*      parent;

                p = set->arr58;
                __asm__ volatile("" : "+r"(j));
                i = 0;
                if (i < set->n58) {
                    parent = &D_80070F10;
                    do {
                        cur            = p;
                        i             += 1;
                        cur->coord.sub = parent;
                        cur->coord.flg = 0;
                        p              = cur + 1;
                    } while (i < set->n58);
                    j = 0;
                }
            }

            {
                GsCOORDINATE2*          parent;
                register GpCoord64View* view asm("a0");
                register GpCoord64*     slot asm("v1");
                register void*          base asm("v0");

                parent = &D_80070F10;
                base   = Gp_RoomCoords;
                view   = (GpCoord64View*)&((GpCoord64*)base)->coord;
                slot   = (GpCoord64*)base;
                do {
                    slot->field_0   = 0;
                    view->coord.sub = parent;
                    view           += 1;
                    j              += 1;
                    slot           += 1;
                } while (j < 8);
            }

            task->state += 1;
        }
    }

    __asm__ volatile("" ::: "memory");
    Gp_UpdateCoord(&D_80070F10);

    {
        register s32        hi asm("v0");
        register GpCoord64* p asm("s1");
        register s32        k asm("s2");

        __asm__ volatile(
            "lui\t%0, %%hi(Gp_RoomCoords)\n\t"
            "addiu\t%1, %0, %%lo(Gp_RoomCoords)"
            : "=r"(hi), "=r"(p));
        k = 0;
        do {
            if (p->field_0 != 0) {
                Gp_UpdateCoordEx(&p->coord, (s32)&D_80070F10);
            }
            k += 1;
            p += 1;
        } while (k < 8);
    }

    {
        register GpCoord60* p asm("a0");
        register GpCoord60* cur asm("s0");

        p = set->arr60;
        __asm__ volatile("" : "+r"(p));
        i = 0;
        if (set->n60 > 0) {
            do {
                cur = p;
                Gp_UpdateCoordEx(&cur->coord, (s32)&D_80070F10);
                i += 1;
                p  = cur + 1;
            } while (i < set->n60);
        }
    }

    {
        register GpCoord6C* obj asm("s3");
        register GpCoord6C* cur asm("s0");

        obj = set->arr6C;
        i   = 0;
        if (set->n6C > 0) {
            do {
                cur = obj;
                Gp_UpdateCoordEx(&cur->coord, (s32)&D_80070F10);
                i  += 1;
                obj = cur + 1;
            } while (i < set->n6C);
        }
    }

    if (set->n58 > 0) {
        register GpCoord58* p asm("s0");

        p = set->arr58;
        for (i = 0; i < set->n58;) {
            Gp_UpdateCoordEx(&p->coord, (s32)&D_80070F10);
            i += 1;
            p += 1;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

s32 Gp_LightPointRoom(GpObj44* arg0, VECTOR3* arg1)
{
    register void**         scratch asm("a1");
    register u8*            head asm("a3");
    register GpAttnScratch* block asm("a2");
    register s32            vx asm("v0");
    register s32            lum asm("v1");
    register GpObj44*       obj asm("t0");
    register GpObj44*       obj2 asm("t2");
    register VECTOR3*       pos asm("t1");
    s32                     result;
    s32                     tooFar;
    s32                     r;
    s32                     g;
    s32                     b;
    s32                     dist;
    s32                     inner;
    u16                     scale;
    u8*                     ptr;
    s16                     room;

    obj  = arg0;
    obj2 = obj;
    room = obj2->field_44;
    pos  = arg1;
    if (room != 0) {
        if ((u8)Game_Session->field_4 != room) {
            return 0;
        }
    }
    scratch = (void**)G_SCRATCH_HEAD;
    vx      = obj2->field_38.vx;
    head    = *scratch;
    vx     -= pos->vx;
    vx    >>= 1;
    {
        register GpAttnScratch* tmp asm("a0");

        tmp                                     = (GpAttnScratch*)(head - 0x20);
        ((GpAttnScratch*)(head - 0x20))->vec.vx = vx;
        block                                   = tmp;
    }
    block->vec.vy  = (obj2->field_38.vy - pos->vy) >> 1;
    block->vec.vz  = (obj2->field_38.vz - pos->vz) >> 1;
    vx             = obj->field_5C;
    block->scale   = 0;
    vx           >>= 1;
    block->outerSq = vx;
    vx             = ((GpAttnScratch*)(head - 0x20))->vec.vx;
    *scratch       = block;
    if (vx < 0) {
        ((GpAttnScratch*)(head - 0x20))->vec.vx = -vx;
    }
    vx = block->vec.vz;
    if (vx < 0) {
        block->vec.vz = -vx;
    }
    vx     = ((GpAttnScratch*)(head - 0x20))->vec.vx;
    tooFar = (u32)block->outerSq < (u32)vx;
    if (!tooFar) {
        tooFar = (u32)block->outerSq < (u32)block->vec.vz;
        if (!tooFar) {
            register s32 sq asm("a0");

            vx  = obj->field_5C;
            lum = vx * vx;
            vx  = lum >> 2;
            asm volatile("" : "+r"(vx));
            block->outerSq = vx;
            vx             = ((GpAttnScratch*)(head - 0x20))->vec.vx;
            lum            = vx * vx;
            vx             = block->vec.vy;
            result         = vx * vx;
            vx             = block->vec.vz;
            sq             = vx * vx;
            vx             = lum + result;
            tooFar         = (u32)block->outerSq < (u32)(vx + sq);
            block->distSq  = vx + sq;
        }
    }
    if (tooFar) {
        result = 0;
    } else {
        block->innerSq = (obj->field_58 * obj->field_58) >> 2;
        r              = obj->field_50;
        g              = obj->field_52;
        b              = obj->field_54;
        block->scale   = 0x1000;
        lum            = (r * 8 + g * 6 + b * 2) >> 8;
        dist           = block->distSq;
        inner          = block->innerSq;
        result         = lum + 0xF00;
        if ((u32)inner < (u32)dist) {
            s32 temp;

            temp = block->outerSq;
            asm volatile("" : "+r"(temp));
            lum = inner;
            asm volatile("" : "+r"(lum));
            block->outerSq = temp - inner;
            block->distSq -= lum;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                lum          = block->scale * result;
                result       = (u32)lum >> 12;
            }
        }
    }
    scale                 = block->scale;
    ptr                   = *(u8**)G_SCRATCH_HEAD;
    obj2->field_4A        = scale;
    *(u8**)G_SCRATCH_HEAD = ptr + 0x20;
    return result;
}

s32 Gp_LightPoint(GpObj44* arg0, VECTOR3* arg1)
{
    register void**         scratch asm("a3");
    register u8*            head;
    register GpAttnScratch* tmp asm("a0");
    register GpAttnScratch* block asm("t0");
    register s32            result asm("a1");
    register s32            lum;
    register GpObj44*       obj asm("t1");
    s32                     tooFar;
    s32                     r;
    s32                     g;
    s32                     b;
    s32                     dist;
    s32                     inner;
    s32                     vx;
    s32                     sq;
    u16                     scale;
    u8*                     ptr;

    obj                                     = arg0;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    vx                                      = obj->field_38.vx;
    head                                    = *scratch;
    vx                                     -= arg1->vx;
    vx                                    >>= 1;
    tmp                                     = (GpAttnScratch*)(head - 0x20);
    ((GpAttnScratch*)(head - 0x20))->vec.vx = vx;
    block                                   = tmp;
    block->vec.vy                           = (obj->field_38.vy - arg1->vy) >> 1;
    block->vec.vz                           = (obj->field_38.vz - arg1->vz) >> 1;
    sq                                      = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->distSq                           = sq;
    sq                                      = obj->field_5C;
    lum                                     = sq * sq;
    sq                                      = lum >> 2;
    lum                                     = block->distSq;
    result                                  = 0;
    block->outerSq                          = sq;
    *scratch                                = block;
    tooFar                                  = (u32)sq < (u32)lum;
    block->scale                            = 0;
    if (!tooFar) {
        block->innerSq = (obj->field_58 * obj->field_58) >> 2;
        r              = obj->field_50;
        g              = obj->field_52;
        b              = obj->field_54;
        block->scale   = 0x1000;
        lum            = (r * 8 + g * 6 + b * 2) >> 8;
        dist           = block->distSq;
        inner          = block->innerSq;
        result         = lum + 0xF00;
        if ((u32)inner < (u32)dist) {
            s32 temp;

            temp = block->outerSq;
            asm volatile("" : "+r"(temp));
            lum = inner;
            asm volatile("" : "+r"(lum));
            block->outerSq = temp - inner;
            block->distSq -= lum;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                lum          = block->scale * result;
                result       = (u32)lum >> 12;
            }
        }
    }
    scale                 = block->scale;
    ptr                   = *(u8**)G_SCRATCH_HEAD;
    obj->field_4A         = scale;
    *(u8**)G_SCRATCH_HEAD = ptr + 0x20;
    return result;
}

s32 Gp_LightCone(GpObj68* arg0, VECTOR3* arg1)
{
    register GpObj68*       obj2 asm("s2");
    register VECTOR3*       pos;
    register GpObj68*       obj asm("s1");
    register s32            result asm("s3");
    register void**         scratch asm("a1");
    register u8*            head asm("a2");
    register u8*            addr asm("a0");
    register GpSpotScratch* block asm("s0");
    register s32            lum;
    s32                     tooFar;
    s32                     r;
    s32                     g;
    s32                     b;
    s32                     dist;
    s32                     inner;
    s32                     vx;
    s32                     sq;
    u16                     scale;
    u8*                     ptr;
    s16                     room;

    obj2 = arg0, pos = arg1, obj = obj2;
    asm volatile("" : "+r"(obj2), "+r"(pos), "+r"(obj));
    room   = obj->field_44;
    result = 0;
    if (room != 0) {
        if ((u8)Game_Session->field_4 != room) {
            return result;
        }
    }
    scratch                                 = (void**)G_SCRATCH_HEAD;
    vx                                      = obj->field_24.t[0];
    head                                    = *scratch;
    vx                                     -= pos->vx;
    vx                                    >>= 1;
    addr                                    = head - 0x2C;
    ((GpSpotScratch*)(head - 0x2C))->vec.vx = vx;
    block                                   = (GpSpotScratch*)addr;
    block->vec.vy                           = (obj->field_24.t[1] - pos->vy) >> 1;
    block->vec.vz                           = (obj->field_24.t[2] - pos->vz) >> 1;
    asm volatile("" : "+r"(addr));
    sq             = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->distSq  = sq;
    sq             = obj2->field_64;
    lum            = sq * sq;
    sq             = lum >> 2;
    lum            = block->distSq;
    block->outerSq = sq;
    *scratch       = block;
    tooFar         = (u32)sq < (u32)lum;
    block->scale   = 0;
    if (tooFar) {
        result = 0;
    } else {
        register VECTOR* light asm("a0");

        block->innerSq = (obj2->field_60 * obj2->field_60) >> 2;
        light          = (VECTOR*)block;
        Gfx_NormalizeLightDir(light, (SVECTOR*)(head - 0x1C));
        {
            register s32 dot asm("v0");

            dot           = block->dir.vx * obj->field_24.m[0][2] + block->dir.vy * obj->field_24.m[1][2] + block->dir.vz * obj->field_24.m[2][2];
            block->cosAng = -dot >> 12;
        }
        if (rcos(obj2->field_68 >> 1) < block->cosAng) {
            r            = obj2->field_50;
            g            = obj2->field_52;
            b            = obj2->field_54;
            block->scale = 0x1000;
            lum          = (r * 8 + g * 6 + b * 2) >> 8;
            dist         = block->distSq;
            inner        = block->innerSq;
            result       = lum + 0xF00;
            if ((u32)inner < (u32)dist) {
                s32 temp;

                temp = block->outerSq;
                asm volatile("" : "+r"(temp));
                lum = inner;
                asm volatile("" : "+r"(lum));
                block->outerSq = temp - inner;
                block->distSq -= lum;
                while ((u32)block->outerSq > 0xFFFF) {
                    block->outerSq = (u32)block->outerSq >> 4;
                    block->distSq  = (u32)block->distSq >> 4;
                }
                if (block->outerSq != 0) {
                    block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                    lum          = block->scale * result;
                    result       = (u32)lum >> 12;
                }
            }
        }
    }
    scale                 = block->scale;
    ptr                   = *(u8**)G_SCRATCH_HEAD;
    obj->field_4A         = scale;
    *(u8**)G_SCRATCH_HEAD = ptr + 0x2C;
    return result;
}

void func_800D759C(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3)
{
    void**                       scratch;
    u8*                          head;
    register GpViewLightScratch* block asm("s0");
    register SVECTOR*            dir asm("s5");
    MATRIX*                      mtx;
    register MATRIX*             dirMtx asm("s6");
    register MATRIX*             colorMtx asm("s7");
    SVECTOR                      tmp;
    SVECTOR*                     tmpp;
    register s32                 val asm("v0");
    register s32                 scale;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (GpViewLightScratch*)(head - 0x3C);
    dir      = (SVECTOR*)(head - 0x2C);
    mtx      = (MATRIX*)(head - 0x24);
    dirMtx   = arg3->field_1C;
    colorMtx = arg3->field_20;

    ((GpViewLightScratch*)(head - 0x3C))->in.vx = -arg1->field_18.vx;
    block->in.vy                                = -arg1->field_18.vy;
    *scratch                                    = block;
    block->in.vz                                = -arg1->field_18.vz;
    Gfx_NormalizeLightDir((VECTOR*)block, dir);

    Gp_UpdateCoord(arg1->field_4C);
    TransposeMatrix(&D_80070F34, mtx);
    gte_MulMatrix0_real(mtx, &arg1->field_4C->workm, mtx);

    tmp = *(SVECTOR*)(head - 0x2C);
    gte_SetRotMatrix(mtx);
    __asm__ volatile("addiu %0, $sp, 0x10" : "=r"(tmpp));
    gte_ldv0(tmpp);
    gte_rtv0_real();
    gte_stsv(dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->field_4A;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->field_50);
    gte_gpf12_real();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    *scratch = (u8*)*scratch + 0x3C;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D78A4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D7A9C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8684);

void Gp_RemapActorColor(GpEnemy* arg0, MATRIX* arg1, s32 arg2)
{
    s32 i;
    s32 val;

    if (arg2 == 1) {
        goto case1;
    } else if (arg2 < 2) {
        goto def;
    } else if (arg2 == 2) {
        goto case2;
    } else if (arg2 == 3) {
        goto case3;
    } else {
        goto def;
    }

case1: {
    s32 t;
    for (i = 0; i < 3; i++) {
        t             = (arg1->m[0][i] * 7 + arg1->m[1][i] * 6 + arg1->m[2][i] * 3) / 33;
        arg1->m[0][i] = t * 4;
        arg1->m[1][i] = t * 2;
        arg1->m[2][i] = t;
    }
}
    return;

case3:
    if ((arg0->field_4E & 0x80) && (arg0->field_4B == 0)) {
        goto flicker;
    }
    arg1->m[0][0] = arg1->m[0][1] = arg1->m[0][2] = 0x180;
    arg1->m[1][0] = arg1->m[1][1] = arg1->m[1][2] = 0x100;
    arg1->m[2][0] = arg1->m[2][1] = arg1->m[2][2] = 0x100;
    return;

case2:
    if ((arg0->field_4E & 0x80) && (arg0->field_4B == 0)) {
        goto flicker;
    }
    arg1->m[0][0] = 0;
    arg1->m[0][1] = 0;
    arg1->m[0][2] = 0;
    arg1->m[1][0] = 0;
    arg1->m[1][1] = 0;
    arg1->m[1][2] = 0;
    arg1->m[2][0] = 0;
    arg1->m[2][1] = 0;
    arg1->m[2][2] = 0;
    return;

def:
    if ((arg0->field_4E & 0x80) && (arg0->field_4B == 0)) {
    flicker:
        val = rsin(Display_State.field_14 << 6) + 0x1800;
        if ((Display_State.field_14 & 1) == 0) {
            val >>= 1;
        }
        arg1->m[0][0] = arg1->m[0][1] = arg1->m[0][2] = 0x200;
        arg1->m[1][0] = arg1->m[1][1] = arg1->m[1][2] = val;
        arg1->m[2][0] = arg1->m[2][1] = arg1->m[2][2] = 0x200;
        arg0->field_4E                               &= 0x7F;
    } else if (arg0->field_4C & 0xC) {
        s32 t;
        for (i = 0; i < 3; i++) {
            t             = (arg1->m[0][i] * 7 + arg1->m[1][i] * 6 + arg1->m[2][i] * 3) / 33;
            arg1->m[0][i] = t * 3;
            arg1->m[1][i] = t;
            arg1->m[2][i] = t * 3;
        }
    }
}

void Gp_UpdateActorColor(GpEnemy* arg0, VECTOR* arg1)
{
    GameActorExt*   extra;
    MATRIX*         colorMtx;
    s32             mode;
    u8*             head;
    GpColorScratch* block;
    SVECTOR*        col0;
    SVECTOR*        col1;
    GpMtxCol*       src;
    GpMtxCol*       dst;
    s32             i;
    s32             w0;
    s32             w1;

    extra    = (GameActorExt*)arg0->task->extra;
    colorMtx = extra->field_20;
    mode     = arg0->field_4E & 3;
    if ((!(extra->field_C & 0x80) && (extra->field_18 != NULL)) || (Game_Session->field_65 != 1)) {
        {
            register void**          scratch asm("v1");
            register GpColorScratch* tmp asm("v0");

            scratch  = (void**)G_SCRATCH_HEAD;
            head     = *scratch;
            tmp      = (GpColorScratch*)(head - 0x30);
            block    = tmp;
            *scratch = tmp;
        }
        func_800D7A9C(extra, arg1, 0, 3);
        if ((s8)arg0->field_4F <= 0) {
            Gp_RemapActorColor(arg0, colorMtx, mode);
        } else {
            block->mtx.m[0][0] = colorMtx->m[0][0];
            block->mtx.m[0][1] = colorMtx->m[0][1];
            block->mtx.m[0][2] = colorMtx->m[0][2];
            block->mtx.m[1][0] = colorMtx->m[1][0];
            block->mtx.m[1][1] = colorMtx->m[1][1];
            block->mtx.m[1][2] = colorMtx->m[1][2];
            block->mtx.m[2][0] = colorMtx->m[2][0];
            block->mtx.m[2][1] = colorMtx->m[2][1];
            block->mtx.m[2][2] = colorMtx->m[2][2];
            Gp_RemapActorColor(arg0, colorMtx, mode);
            Gp_RemapActorColor(arg0, &block->mtx, (arg0->field_4E >> 2) & 3);
            i    = 0;
            col0 = (SVECTOR*)(head - 0x10);
            col1 = (SVECTOR*)(head - 8);
            src  = (GpMtxCol*)colorMtx;
            w0   = (s8)arg0->field_4F << 8;
            dst  = (GpMtxCol*)block;
            w1   = 0x1000 - w0;
            do {
                block->col0.vx = src->x;
                asm volatile("" : "+r"(src));
                block->col0.vy = src->y;
                asm volatile("" : "+r"(src));
                block->col0.vz = src->z;
                block->col1.vx = dst->x;
                asm volatile("" : "+r"(dst));
                block->col1.vy = dst->y;
                asm volatile("" : "+r"(dst));
                block->col1.vz = dst->z;
                gte_lddp(w1);
                gte_ldsv(col0);
                gte_gpf12_real();
                gte_lddp(w0);
                gte_ldsv(col1);
                gte_gpl12_real();
                gte_stsv(col0);
                src->x = block->col0.vx;
                dst    = (GpMtxCol*)&dst->_0;
                src->y = block->col0.vy;
                i++;
                src->z = block->col0.vz;
                src    = (GpMtxCol*)&src->_0;
            } while (i < 3);
            if (D_801153F4 == 0) {
                arg0->field_4F--;
            }
        }
        *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x30;
    }
}

void Gp_LightFalloff(GpObj44* arg0)
{
    register void**         scratch asm("a1");
    register u8*            head asm("a0");
    register GpAttnScratch* tmp asm("v1");
    register GpAttnScratch* block asm("a2");
    register s32            result asm("t0");
    register s32            lum asm("v1");
    s32                     tooFar;
    s32                     r;
    s32                     g;
    s32                     b;
    s32                     dist;
    s32                     inner;
    s32                     vx;
    u16                     scale;
    u8*                     ptr;

    result                                  = 0;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    vx                                      = arg0->field_18.vx;
    head                                    = *scratch;
    vx                                    >>= 1;
    tmp                                     = (GpAttnScratch*)(head - 0x20);
    ((GpAttnScratch*)(head - 0x20))->vec.vx = vx;
    block                                   = tmp;
    block->vec.vy                           = arg0->field_18.vy >> 1;
    block->vec.vz                           = arg0->field_18.vz >> 1;
    block->distSq                           = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->outerSq                          = (arg0->field_5C * arg0->field_5C) >> 2;
    tooFar                                  = (u32)block->outerSq < (u32)block->distSq;
    *scratch                                = block;
    block->scale                            = 0;
    if (!tooFar) {
        block->innerSq = (arg0->field_58 * arg0->field_58) >> 2;
        r              = arg0->field_50;
        g              = arg0->field_52;
        b              = arg0->field_54;
        block->scale   = 0x1000;
        lum            = (r * 8 + g * 6 + b * 2) >> 8;
        dist           = block->distSq;
        inner          = block->innerSq;
        result         = lum + 0xF00;
        if ((u32)inner < (u32)dist) {
            s32 temp;

            temp = block->outerSq;
            asm volatile("" : "+r"(temp));
            lum = inner;
            asm volatile("" : "+r"(lum));
            block->outerSq = temp - inner;
            block->distSq -= lum;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                lum          = block->scale * result;
                result       = (u32)lum >> 12;
            }
        }
    }
    scale                 = block->scale;
    ptr                   = *(u8**)G_SCRATCH_HEAD;
    arg0->field_38.vx     = result;
    arg0->field_4A        = scale;
    *(u8**)G_SCRATCH_HEAD = ptr + 0x20;
}

void Gp_SetLightMode(GpObj4C* arg0, s32 arg1)
{
    u8 val;

    val   = arg0->field_4E;
    arg1 &= 3;
    if ((val & 3) != arg1) {
        arg0->field_4E = (val & 0xF0) | ((val & 3) << 2) | arg1;
        arg0->field_4F = 0x10;
    }
}

s32 Gp_GetObjDepth(GpObj38* arg0)
{
    s32 val;

    val = arg0->field_24.t[2] - Display_State.field_110;
    if (val >= 0x7FFF) {
        val = 0x7FFF;
    }
    if (val < -0x7FFF) {
        val = -0x7FFF;
    }
    return val >> 8;
}

s32 Gp_GetObjPan(GpObj38* arg0)
{
    void**        scratch;
    u8*           head;
    GpPanScratch* block;
    SVECTOR*      vec;
    s32           ret;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (GpPanScratch*)(head - 0x18);
    *scratch = block;
    vec      = &block->vec;
    gte_SetRotMatrix(&arg0->field_24);
    gte_SetTransMatrix(&arg0->field_24);
    block->vec.vz = 0;
    block->vec.vy = 0;
    block->vec.vx = 0;
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpPanScratch*)(head - 0x18))->sx);
    gte_stdp(&((GpPanScratch*)(head - 0x18))->p);
    gte_stflg(&((GpPanScratch*)(head - 0x18))->flag);
    gte_stszotz(&((GpPanScratch*)(head - 0x18))->otz);
    if (block->flag >= 0) {
        if (block->sx >= 0xA0) {
            block->sx = 0x9F;
        }
        if (block->sx < -0x9F) {
            block->sx = -0xA0;
        }
        ret = -block->sx / 10;
    } else {
        ret = 0;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
    return -ret;
}

void Gp_SetOverrideVec(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        Gp_OverrideVecFlag = 0;
        return;
    }
    Gp_OverrideVecFlag = 1;
    Gp_OverrideVec     = *arg0;
}

void Gp_SetOverrideVec2(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        Gp_OverrideVec2Flag = 0;
        return;
    }
    Gp_OverrideVec2Flag = 1;
    Gp_OverrideVec2     = *arg0;
}

void Gp_SetObjTrans(GpObj20* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    MATRIX* m;

    m       = arg0->field_20;
    m->t[0] = arg1;
    m->t[1] = arg2;
    m->t[2] = arg3;
}

GpRoomBoundVec* Gp_GetRoomBound(GameSessionFrom4* arg0)
{
    GpRoomCoordRec** mid;
    GpRoomCoordRec*  rec;
    GpRoomBoundVec*  result;
    GpRoomBoundVec*  table;

    mid = Gp_RoomCoordTables[arg0->field_3 - 1];
    rec = NULL;
    if (mid != NULL) {
        rec = mid[arg0->field_2 - 1];
        if (rec != NULL) {
            rec = &rec[arg0->field_1 - 1];
        }
    }
    result = (GpRoomBoundVec*)&Gp_RoomBoundDefault;
    if (rec != NULL) {
        table = rec->field_4;
        if (table != NULL) {
            if (table->field_0 >= arg0->field_0) {
                result = &table[arg0->field_0];
            }
        }
    }
    return result;
}

s32 Gp_CountRoomCoords(void)
{
    s32 count;
    s32 i;

    count = 0;
    for (i = 0; i < 8; i++) {
        if (Gp_RoomCoords[i].field_0 != 0) {
            count++;
        }
    }
    return count;
}

s32 Gp_GetRoomCoordSet(GameSessionFrom4* arg0)
{
    GpRoomCoordRec** mid;
    GpRoomCoordRec*  rec;
    s32              result;

    result = 0;
    mid    = Gp_RoomCoordTables[arg0->field_3 - 1];
    rec    = NULL;
    if (mid != NULL) {
        rec = mid[arg0->field_2 - 1];
        if (rec != NULL) {
            rec = &rec[arg0->field_1 - 1];
        }
    }
    if (rec != NULL) {
        result = rec->field_0;
    }
    return result;
}

void func_800D96C8(Task* arg0)
{
    TaskFunc funcs[2] = { Gp_BindDefaultMtx, func_800D8684 };

    funcs[arg0->state](arg0);
}

s32 Gp_GetObjLuma(GpObj44* arg0)
{
    s16 val;

    val = arg0->field_44;
    if (val != 0 && (u8)Game_Session->field_4 != val) {
        return 0;
    }
    arg0->field_4A = 0x1000;
    return ((arg0->field_50 * 8 + arg0->field_52 * 6 + arg0->field_54 * 2) >> 8) + 0xF00;
}

s32 Gp_GetObjTransX(GpObj38* arg0)
{
    return arg0->field_24.t[0];
}

void func_800D9794(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3)
{
    void**                   scratch;
    u8*                      head;
    register GpLightScratch* block asm("s0");
    SVECTOR*                 dir;
    MATRIX*                  dirMtx;
    MATRIX*                  colorMtx;
    register s32             val asm("v0");
    register s32             scale asm("a2");

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (GpLightScratch*)(head - 0x1C);
    dir      = (SVECTOR*)(head - 0xC);
    *scratch = block;
    dirMtx   = arg3->field_1C;
    colorMtx = arg3->field_20;
    Gfx_NormalizeLightDir((VECTOR*)((GpObj38*)arg1)->field_24.t, dir);

    dirMtx->m[arg0][0] = block->dir.vx;
    dirMtx->m[arg0][1] = block->dir.vy;
    dirMtx->m[arg0][2] = block->dir.vz;

    val          = arg1->field_4A;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->field_50);
    gte_gpf12_real();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    *scratch = (u8*)*scratch + 0x1C;
}

void func_800D98C4(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3)
{
    void**                   scratch;
    u8*                      head;
    register GpLightScratch* block asm("s0");
    SVECTOR*                 dir;
    MATRIX*                  dirMtx;
    MATRIX*                  colorMtx;
    register s32             val asm("v0");
    register s32             scale asm("t0");

    scratch      = (void**)G_SCRATCH_HEAD;
    head         = *scratch;
    block        = (GpLightScratch*)(head - 0x1C);
    dir          = (SVECTOR*)(head - 0xC);
    dirMtx       = arg3->field_1C;
    colorMtx     = arg3->field_20;
    block->in.vx = arg2->vx - ((GpObj38*)arg1)->field_24.t[0];
    block->in.vy = arg2->vy - ((GpObj38*)arg1)->field_24.t[1];
    *scratch     = block;
    block->in.vz = arg2->vz - ((GpObj38*)arg1)->field_24.t[2];
    Gfx_NormalizeLightDir(&block->in, dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->field_4A;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->field_50);
    gte_gpf12_real();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    *scratch = (u8*)*scratch + 0x1C;
}

void func_800D9A30(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3)
{
    void**                   scratch;
    u8*                      head;
    register GpLightScratch* block asm("s0");
    SVECTOR*                 dir;
    MATRIX*                  dirMtx;
    MATRIX*                  colorMtx;
    register s32             val asm("v0");
    register s32             scale asm("t0");

    scratch      = (void**)G_SCRATCH_HEAD;
    head         = *scratch;
    block        = (GpLightScratch*)(head - 0x1C);
    dir          = (SVECTOR*)(head - 0xC);
    dirMtx       = arg3->field_1C;
    colorMtx     = arg3->field_20;
    block->in.vx = arg2->vx - ((GpObj38*)arg1)->field_24.t[0];
    block->in.vy = arg2->vy - ((GpObj38*)arg1)->field_24.t[1];
    *scratch     = block;
    block->in.vz = arg2->vz - ((GpObj38*)arg1)->field_24.t[2];
    Gfx_NormalizeLightDir(&block->in, dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->field_4A;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->field_50);
    gte_gpf12_real();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    *scratch = (u8*)*scratch + 0x1C;
}

void Gp_InsertRankedSlot(GpRec12* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    GpRec12* rec;
    GpRec12* next;

    if (arg1 <= 0) {
        return;
    }

    rec = (GpRec12*)(arg4 * sizeof(*arg0) + (s32)arg0);
    if (rec->field_4 < arg1) {
        if (arg4 < 3) {
            rec[1] = *rec;
        }
        if (arg4 > 0) {
            Gp_InsertRankedSlot(arg0, arg1, arg2, arg3, arg4 - 1);
        } else {
            arg0->field_4 = arg1;
            arg0->field_0 = arg2;
            arg0->field_8 = arg3;
        }
    } else if (arg4 < 3) {
        next           = rec + 1;
        next->field_4  = arg1;
        rec[1].field_0 = arg2;
        next->field_8  = arg3;
    }
}

void Gp_FillSVec3x3(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    arg0->field_0.vx = arg0->field_0.vy = arg0->field_0.vz = arg1;
    arg0->field_6.vx = arg0->field_6.vy = arg0->field_6.vz = arg2;
    arg0->field_C.vx = arg0->field_C.vy = arg0->field_C.vz = arg3;
}

GpRoomCoordRec* Gp_GetRoomCoordRec(GameSessionFrom4* arg0)
{
    GpRoomCoordRec** mid;
    GpRoomCoordRec*  rec;

    mid = Gp_RoomCoordTables[arg0->field_3 - 1];
    rec = NULL;
    if (mid != NULL) {
        rec = mid[arg0->field_2 - 1];
        if (rec != NULL) {
            rec = &rec[arg0->field_1 - 1];
        }
    }
    return rec;
}

void func_800D9CC8(Task* arg0)
{
    Task_CallExit(arg0);
}

void Gp_CopyDefaultBound(GBytes8* arg0)
{
    *arg0 = Gp_RoomBoundDefault;
}

void Gp_BindDefaultMtx(Task* arg0)
{
    GpActorWork*     slot;
    GameActorExt*    extra;
    GameActor*       actor;
    register s32     result asm("v1");
    s32              i;
    register MATRIX* mtxA asm("a2");
    register MATRIX* mtxB asm("a1");
    register s32     addr asm("v0");

    slot  = Game_GetPtrSlot(3);
    extra = slot->extra;
    if (slot != NULL) {
        result = Gp_GetRoomCoordSet((GameSessionFrom4*)&Game_Session->field_4);
        i      = 0;
        if (result == 0) {
            Task_Kill(arg0);
            return;
        }
        addr = (s32)&Gp_DefaultMtx;
        __asm__ volatile("" : "+r"(addr));
        mtxA = (MATRIX*)addr;
        addr = (s32)&Gp_DefaultMtx2;
        __asm__ volatile("" : "+r"(addr));
        mtxB                = (MATRIX*)addr;
        arg0->spawnArg2     = (void*)result;
        extra->field_1C     = mtxA;
        extra->field_20     = mtxB;
        actor               = slot->actor;
        Gp_OverrideVecFlag  = 0;
        Gp_OverrideVec2Flag = 0;
        D_80114F28          = 0;
        do {
            extra           = (&actor->field_920)[i]->extra;
            extra->field_1C = mtxA;
            extra->field_20 = mtxB;
            i++;
        } while (i < 2);
        arg0->state++;
        func_800D8684(arg0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9DFC);

void* func_800DA2A0(GpActorWork* arg0, VECTOR3* out, s32 flag)
{
    void**             scratch;
    u8*                head;
    GpLockScanScratch* block;
    GameActor*         actor;
    GsCOORDINATE2*     coord;
    GsCOORDINATE2*     nodeCoord;
    GpLinkXform*       node;
    GpLinkXform*       best;
    s32                bestAngle;
    u32                bestDist;
    s32                baseAngle;
    s32                angle;
    u32                dist;
    s32                sub;
    SVECTOR            tmp;
    SVECTOR*           srcp;

    best    = NULL;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* newhead asm("a1");
        newhead = head - 0x38;
        block   = (GpLockScanScratch*)newhead;
    }
    actor         = arg0->actor;
    coord         = (GsCOORDINATE2*)arg0->extra->field_8;
    block->src.vx = *(u16*)&coord->coord.t[0];
    block->src.vy = *(u16*)&coord->coord.t[1] - 1000;
    block->src.vz = *(u16*)&coord->coord.t[2];
    *scratch      = block;
    Gp_UpdateCoord(&D_80070F10);
    srcp = &((GpLockScanScratch*)(head - 0x38))->src;
    gte_SetRotMatrix(&D_80070F34);
    gte_ldv0(srcp);
    gte_rtv0_real();
    gte_stsv(&block->self);
    *(u16*)&block->self.vx = *(u16*)&block->self.vx + *(u16*)&D_80070F10.workm.t[0];
    *(u16*)&block->self.vy = *(u16*)&block->self.vy + *(u16*)&D_80070F10.workm.t[1];
    *(u16*)&block->self.vz = *(u16*)&block->self.vz + *(u16*)&D_80070F10.workm.t[2];

    if (actor->field_90C != NULL && flag != 0) {
        node      = (GpLinkXform*)actor->field_90C;
        baseAngle = ratan2(node->dst.vx, node->dst.vz);
    } else {
        baseAngle = 0;
    }
    bestAngle = 0x3000;
    bestDist  = 0x7FFFFFFF;
    dist      = 0;
    for (node = (GpLinkXform*)Gp_LinkList; node != NULL; node = node->next) {
        if (((GpLinkNode*)node)->field_4 & 1) {
            continue;
        }
        angle = ratan2(node->dst.vx, node->dst.vz);
        if (flag == 0) {
            dist = node->dst.vz * node->dst.vz + node->dst.vx * node->dst.vx + node->dst.vy * node->dst.vy;
            if (angle < 0) {
                angle = -angle;
            }
            if (dist <= 0x300000) {
                sub    = 0x300000 - dist;
                sub  >>= 13;
                sub   *= 3;
                angle -= sub;
                if (angle < 0) {
                    angle = 0;
                }
                dist += sub / 3;
            }
            angle >>= 10;
            if ((GpLinkNode*)node == actor->field_90C) {
                angle += 0x1000;
            }
            if (angle == bestAngle && dist > bestDist) {
                angle = 0x2000;
            }
        } else {
            angle -= baseAngle;
            if (angle < 0) {
                angle += 0x1000;
            }
            if (angle >= 0x1000) {
                angle -= 0x1000;
            }
            if (flag == 1) {
                angle = -angle;
            }
            if ((GpLinkNode*)node == actor->field_90C) {
                angle += 0x1000;
            }
        }
        if (angle > bestAngle) {
            continue;
        }
        Gp_UpdateCoord(node->coord);
        block->node.vx = *(u16*)&node->src.vx;
        block->node.vy = *(u16*)&node->src.vy;
        block->node.vz = *(u16*)&node->src.vz;
        nodeCoord      = node->coord;
        tmp            = block->node;
        gte_SetRotMatrix(&nodeCoord->workm);
        gte_ldv0(&tmp);
        gte_rtv0_real();
        gte_stsv(&block->node);
        block->node.vx += *(u16*)&node->coord->workm.t[0];
        block->node.vy += *(u16*)&node->coord->workm.t[1];
        block->node.vz += *(u16*)&node->coord->workm.t[2];
        if (func_800E0308(&block->node, &block->self) != 1) {
            bestAngle = angle;
            best      = node;
            bestDist  = dist;
        }
    }
    if (best != NULL) {
        Gp_GetLockPos((GpLockPos*)best, out);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
    return best;
}

void func_800DA6E8(void* arg0, s32 arg1)
{
    GpSlot70* found;
    s32       i;
    GpSlot70* p;

    found = NULL;
    i     = 0;
    p     = Gp_LockSlots;
loop:
    if (p->field_0 == arg0) {
        if (arg1 >= 0) {
            if (p->field_4 >= 0) {
                found = p;
                goto done;
            }
            p++;
        } else if (p->field_4 < 0) {
            found = p;
            goto done;
        } else {
            p++;
        }
    } else {
        p++;
    }
    i++;
    if (i < 0x20) {
        goto loop;
    }
done:
    if (found == NULL) {
        i = 0;
        p = Gp_LockSlots;
    loop2:
        if (p->field_0 == NULL) {
            found          = p;
            p->field_0     = arg0;
            found->field_4 = 0;
        } else {
            i++;
            p++;
            if (i < 0x20) {
                goto loop2;
            }
        }
        if (found != NULL) {
            goto update;
        }
    } else {
    update:
        found->field_6  = 0x14;
        found->field_4 += arg1;
    }
}

static __inline__ void project_slot(s32* sxy, GpSlot70* slot)
{
    register u8*             head asm("a0");
    register GpPerspSrc*     src asm("a1");
    register GpPerspScratch* block asm("v1");

    src           = (GpPerspSrc*)slot->field_0;
    head          = *(u8**)G_SCRATCH_HEAD;
    block         = (GpPerspScratch*)(head - 0x14);
    block->vec.vx = src->field_C;
    block->vec.vy = src->field_10;
    block->vec.vz = src->field_14;
    __asm__ volatile("" ::: "memory");
    *(void**)G_SCRATCH_HEAD = block;
    gte_SetRotMatrix(&((GsCOORDINATE2*)src->field_8)->workm);
    gte_SetTransMatrix(&((GsCOORDINATE2*)src->field_8)->workm);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(sxy);
    gte_stdp(&((GpPerspScratch*)(head - 0x14))->p);
    gte_stflg(&((GpPerspScratch*)(head - 0x14))->flag);
    gte_stszotz(&((GpPerspScratch*)(head - 0x14))->otz);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

void func_800DA7B8(void)
{
    RECT          rect;
    u8            buf[16];
    TextDrawReq   req;
    s32           i;
    GpSlot70*     slot;
    u8*           bufp;
    TextDrawReq*  reqp;
    register s16* p6 asm("s4");
    s32           x;
    s32           y;
    s32           val;
    register s32  x14 asm("s3");
    register s32  color asm("v0");
    s32           ot;
    void*         obj;
    GpLinkNode*   node;
    s32           found;

    slot = Gp_LockSlots;
    i    = 0;
    bufp = buf;
    reqp = &req;
    p6   = &slot->field_6;
    do {
        obj = slot->field_0;
        if (obj == NULL) {
            goto empty;
        }
        node  = Gp_LinkList;
        found = 0;
        if (node != NULL) {
            do {
                if (obj == node) {
                    found = 1;
                    goto check_found;
                }
                node = node->next;
            } while (node != NULL);
        }
    check_found:
        if (found != 0) {
            project_slot((s32*)&slot->field_8, slot);
        } else {
            slot->field_0 = (void*)4;
        }

        val = p6[-1];
        if (val >= 0) {
            x = p6[1] + 0xA;
            y = p6[2] + 4;
        } else {
            x = p6[1] - 0xA;
            y = p6[2] - 0x10;
        }
        if (x < -0x88) {
            x = (x & 7) - 0x8F;
        }
        if (x >= 0x89) {
            x = -(x & 7) + 0x8F;
        }
        if (y >= 0x55) {
            y = (y & 7) + 0x4D;
        }
        if (y < -0x64) {
            y = -(y & 7) - 0x5D;
        }

        color = 0x37A78;
        x14   = x + 0xE;
        ot    = -0xA;
        asm volatile("" ::"r"(color), "r"(x14), "r"(ot));
        req.field_8    = color;
        req.glyphTable = 5;
        req.centerMode = 2;
        req.x          = x14;
        req.y          = y;
        req.otIndex    = ot;
        req.field_E    = 1;

        val = p6[-1];
        if (val < 0) {
            req.field_8 = 0x808008;
            val         = -val;
        }
        if (val >= 0x2710) {
            val = 0x270F;
        }

        req.x       = x14;
        req.field_E = 0;
        func_8002E53C(reqp, Text_ItoaSigned(bufp, val));
        req.x       = x14;
        req.field_E = 4;
        func_8002E53C(reqp, Text_ItoaSigned(bufp, val));

        rect.x = x - 0x10;
        rect.y = y - 8;
        rect.w = 0x20;
        rect.h = 0xC;
        if (val >= 0x3E8) {
            rect.x = x - 0x18;
            rect.w = 0x28;
        } else if (val < 0x64) {
            rect.x = x - 8;
            rect.w = 0x18;
        }
        Ui_DrawTextInRect(&rect, -0xA, 2, NULL);

        {
            u16 timer;
            timer = p6[0];
            timer--;
            p6[0] = timer;
            if ((s32)(timer << 16) > 0) {
                goto next;
            }
        }
        p6[-1]        = 0;
        p6[0]         = 0;
        slot->field_0 = NULL;
        goto next;

    empty:
        p6[-1] = 0;
        p6[0]  = 0;
    next:
        i++;
        p6 += 6;
        slot++;
    } while (i < 0x20);
}

void Gp_UnlinkNode(GpLinkNode* node)
{
    s32                    i;
    GpActorWork* volatile* p;
    GpActorWork*           work;
    GameActor*             actor;
    GpLinkNode**           list;

    i = 0;
    p = Gp_ActorSlots;
    do {
        work = *p;
        if (work != NULL) {
            actor = work->actor;
            if (actor->field_90C == node) {
                actor->field_90C = NULL;
            }
        }
        i++;
        p++;
    } while (i < 2);

    if (node->field_6 == 1) {
        list = &Gp_LinkList;
        if (Gp_LinkList != node) {
            do {
                if (*list == NULL) {
                    goto done;
                }
                list = &(*list)->next;
            } while (*list != node);
        }
        if (*list != NULL) {
            *list = node->next;
        }
    done:
        node->field_6 = 0;
        node->field_5 = 0;
    }
}

void Gp_LinkNode(GpLinkNode* node)
{
    GpLinkNode** p;
    register s32 val asm("v0");

    if (node->field_6 == 0) {
        p = &Gp_LinkList;
        if (Gp_LinkList != NULL) {
            do {
                p = &(*p)->next;
            } while (*p != NULL);
        }
        *p            = node;
        val           = node->field_4;
        node->next    = NULL;
        node->field_5 = 0;
        node->field_6 = 1;
    } else {
        val = node->field_4;
    }
    node->field_4 = val & 0xFE;
}

s32 Gp_NodeSlotMask(GpLinkNode* arg0)
{
    s32                    mask;
    s32                    i;
    s32                    one;
    GpActorWork* volatile* p;
    GpActorWork*           work;

    mask = 0;
    i    = mask;
    one  = 1;
    p    = Gp_ActorSlots;
    do {
        work = *p;
        if (work != NULL) {
            if (work->actor->field_90C == arg0) {
                mask |= one << i;
            }
        }
        i++;
        p++;
    } while (i < 2);
    return mask;
}

void Gp_AssignNodeSlot0(GpLinkNode* arg0)
{
    GpActorWork* work;
    GameActor*   actor;
    GpLinkNode*  node;
    u8           val;

    work = Gp_ActorSlots[0];
    if (work != NULL) {
        actor = work->actor;
        node  = actor->field_90C;
        if (node != NULL) {
            node->field_5 = 0;
        }
        actor->field_90C = arg0;
    }
    val           = arg0->field_4;
    arg0->field_5 = 1;
    arg0->field_4 = val & 0xFE;
}

void Gp_ClearNodeSlots(GpLinkNode* arg0)
{
    s32                    i;
    GpActorWork* volatile* p;
    GpActorWork*           work;
    GameActor*             actor;
    u8                     val;

    i = 0;
    p = Gp_ActorSlots;
    do {
        work = *p;
        if (work != NULL) {
            actor = work->actor;
            if (actor->field_90C == arg0) {
                actor->field_90C = NULL;
            }
        }
        i++;
        p++;
    } while (i < 2);
    val           = arg0->field_4;
    arg0->field_5 = 0;
    arg0->field_4 = val | 1;
}

void* Gp_FindLockNode(GpActorWork* arg0)
{
    VECTOR3 pos;

    return func_800DA2A0(arg0, &pos, 0);
}

void* Gp_FindLockNodePad(GpActorWork* arg0)
{
    VECTOR3  pos;
    VECTOR3* p;
    s32      flag;

    p = &pos;
    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return func_800DA2A0(arg0, p, flag);
}

void* Gp_FindLockNodeAt(GpActorWork* arg0, VECTOR3* pos)
{
    s32 flag;

    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return func_800DA2A0(arg0, pos, flag);
}

/* After D_8009745C from func_800D8684 so overlay .rodata stays packed. */
const char Gp_StrGetLockPosNull[] = {
    '#',
    '#',
    '#',
    '#',
    '#',
    '#',
    '#',
    'g',
    'e',
    't',
    '_',
    'l',
    'o',
    'c',
    'k',
    '_',
    'p',
    'o',
    's',
    ' ',
    '-',
    '-',
    '-',
    '>',
    ' ',
    'N',
    'U',
    'L',
    'L',
    '!',
    '!',
    '!',
    '\n',
    '\0',
    0x8C,
    0x16,
};

void Gp_GetLockPos(GpLockPos* arg0, VECTOR3* out)
{
    GsCOORDINATE2* world;
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    MATRIX*        mat;

    if (arg0 == NULL) {
        printf(Gp_StrGetLockPosNull);
        out->vx = 0;
        out->vy = 0;
        out->vz = 0;
        return;
    }

    coord = arg0->coord;
    world = &D_80070F10;
    if (coord == world) {
        out->vx = arg0->pos.vx;
        out->vy = arg0->pos.vy;
        out->vz = arg0->pos.vz;
        return;
    }

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x28;
    Gp_UpdateCoord(coord);
    mat = (MATRIX*)(head - 0x20);
    Gp_WorldToLocal(&world->workm, &coord->workm, mat);
    gte_SetRotMatrix(mat);
    gte_SetTransMatrix(mat);
    gte_ldlvl(&arg0->pos);
    gte_rtirtr_real();
    gte_stlvl(out);
    *scratch = (u8*)*scratch + 0x28;
}

void Gp_ClearLockSlots(void)
{
    s32       i;
    GpSlot70* p;

    p = Gp_LockSlots;
    i = 0;
    do {
        i++;
        p->field_0 = NULL;
        p->field_4 = 0;
        p->field_6 = 0;
        p++;
    } while (i < 0x20);
}

void Gp_ResetLinkState(void)
{
    Gp_LinkList = NULL;
    Gp_ClearLockSlots();
    D_8010F9F0 = 0xFFF00000;
    D_8010F9EC = 0xFFF00000;
}

s32 Gp_ProjectToSxy(GpPerspSrc* arg0, s32* sxy)
{
    void**          scratch;
    u8*             head;
    GpPerspScratch* block;
    s32             ret;

    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (GpPerspScratch*)(head - 0x14);
    block->vec.vx = arg0->field_C;
    block->vec.vy = arg0->field_10;
    *scratch      = block;
    block->vec.vz = arg0->field_14;
    __asm__ volatile("" ::: "memory");
    gte_SetRotMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
    gte_SetTransMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(sxy);
    gte_stdp(&((GpPerspScratch*)(head - 0x14))->p);
    gte_stflg(&((GpPerspScratch*)(head - 0x14))->flag);
    gte_stszotz(&((GpPerspScratch*)(head - 0x14))->otz);
    ret      = block->otz;
    *scratch = (u8*)*scratch + 0x14;
    return ret;
}

void Gp_ClearSlotNodeFlags(void)
{
    s32                    i;
    GpActorWork* volatile* p;
    GpActorWork*           work;
    GpLinkNode*            node;

    i = 0;
    p = Gp_ActorSlots;
    do {
        work = *p;
        if (work != NULL) {
            node = work->actor->field_90C;
            if (node != NULL) {
                node->field_5 = 0;
            }
        }
        i++;
        p++;
    } while (i < 2);
}

s32 Gp_GrantLocationItems(GpItemScan* arg0)
{
    GameSessionFrom4* loc;
    GpGiveRec*        rec;
    s32               key;
    s32               ret;
    s32               i;
    u16               item;
    s8                mode;
    u8                stage;
    u8                area;
    u8                sub;

    ret   = 0;
    loc   = (GameSessionFrom4*)&Game_Session->field_4;
    stage = loc->field_3;
    area  = loc->field_2;
    sub   = loc->field_5;
    key   = (stage << 24) | (area << 16) | (sub << 8);
    mode  = Mc_SaveData.field_F;
    if ((mode == 0) || (mode == 2)) {
        rec = D_8010F9F4[stage];
    } else {
        rec = D_8010FA0C[stage];
    }
    if (rec->field_0 != -1) {
        do {
            if (rec->field_0 == key) {
                for (i = 0; i < 4; i++) {
                    item = rec->items[i];
                    if (item != 0) {
                        if ((i != 3) || (func_800B9D80(0x80000) != 0)) {
                            if (func_800B7420(item) == 0) {
                                ret = 1;
                                if (i == 3) {
                                    ret = 2;
                                }
                                Gp_GiveItem(arg0, item, -1);
                            }
                        }
                    }
                }
                return ret;
            }
            rec++;
        } while (rec->field_0 != -1);
    }
    return ret;
}

s32 Gp_LoadActorImage(GpActorWork* arg0, GpImgRec* arg1, RECT* arg2)
{
    s32        ret;
    TmdObject* extra;
    s32        x;

    extra = (TmdObject*)arg0->extra;
    ret   = 0;
    if (arg1 != NULL) {
        arg1->rect.x = ((s8)extra->field_24 << 6) + (x = (arg2->x + 1) / 2 + 0x180);
        arg1->rect.y = arg2->y + 0x100;
        arg1->rect.w = arg2->w;
        arg1->rect.h = arg2->h;
        Gp_LoadImages(arg1);
    } else {
        ret = 1;
    }
    return ret;
}

void Gp_LoadImages(GpImgRec* arg0)
{
    void**         scratch;
    void*          head;
    register void* temp asm("v0");
    RECT*          dest;
    s32            done;
    register s32   max asm("s4");

    done     = 0;
    scratch  = (void**)G_SCRATCH_HEAD;
    max      = 0xFF;
    head     = *scratch;
    temp     = (u8*)head - 8;
    dest     = temp;
    *scratch = dest;

    do {
        if (arg0->field_0 == 0) {
            dest->x = arg0->rect.x;
            dest->y = arg0->rect.y;
            dest->w = arg0->rect.w;
            dest->h = arg0->rect.h;
            LoadImage(dest, arg0->data);
        } else {
            done = 1;
        }
        arg0++;
    } while (done == 0);

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
    asm("" ::"r"(max));
}

void Gp_InitStateF0(void)
{
    GpStateF0*  p;
    McSaveData* save;
    u8          val;

    p                  = &Gp_StateF0;
    Gp_StateF0.field_0 = 0;
    p->field_1         = 0;
    p->field_2         = 0;
    p->field_3         = 0;
    p->field_4         = 0;
    p->field_5         = 0;
    p->field_6         = 0;
    p->field_8         = 0;
    p->field_C         = 0;
    p->field_10        = 0;
    p->field_14        = 0;
    p->field_18        = 0;
    p->field_19        = 0;
    p->field_1A        = 0;
    p->field_1B        = 0;
    p->field_1C        = 0;
    p->field_1D        = 0;
    p->field_1E        = 0;
    p->field_1F        = 0;
    p->field_20        = 0;
    p->field_21        = 0;
    p->field_22        = 0;
    p->field_23        = 0;
    p->field_24        = 0;
    p->field_25        = 0;
    p->field_26        = 0;
    p->field_27        = 0;
    p->field_28        = 0;
    p->field_29        = 0;
    p->field_2A        = 0;
    if (Gp_IsDebugAttachRoom() == 1) {
        p->field_2B = 0;
    } else {
        save        = &Mc_SaveData;
        val         = (u8)save->field_F;
        p->field_2B = val;
        if (val == 0) {
            if (save->field_E != 0) {
                p->field_2B = 4;
            }
        }
    }
}

void Gp_ArmStateF0(s32 arg0)
{
    if (Gp_StateF0.field_0 == 0) {
        Gp_StateF0.field_0 = 1;
    }
}

void Gp_SetStateF0Bit(s32 arg0)
{
    if (arg0 != 0) {
        Gp_StateF0.field_2 |= 1 << (arg0 - 1);
    }
}

void Gp_SetStateF0Byte3(s32 arg0)
{
    D_801153F3 = arg0;
}

void Gp_IncStateF0Ref(void)
{
    Gp_StateF0.field_6++;
}

void Gp_ReleaseStateF0Add(GpObj20E* arg0)
{
    GpStateF0*  p;
    GpStateF0*  q;
    GpPairSrcE* rec;

    p = &Gp_StateF0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            Gp_StateF0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            if (!(Game_Session->field_69 & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
        rec = arg0->field_20->field_50;
        if (rec != NULL) {
            q            = &Gp_StateF0;
            q->field_8  += rec->field_6;
            q->field_C  += rec->field_8;
            q->field_10 += rec->field_A;
        }
    }
}

void Gp_ReleaseStateF0Clear(void)
{
    GpStateF0* p;

    p = &Gp_StateF0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            Gp_StateF0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            p->field_8         = 0;
            p->field_C         = 0;
            p->field_10        = 0;
            if (!(Game_Session->field_69 & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}

void Gp_ReleaseStateF0(void)
{
    GpStateF0* p;

    p = &Gp_StateF0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            Gp_StateF0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            if (!(Game_Session->field_69 & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}

void func_800DB72C(void)
{
    if (Game_GetPtrSlot(3) != NULL) {
        func_8010154C();
        func_800E0540(D_80115570);
        func_800E0540(D_80115574);
        func_800E0540(D_80115578);
        func_800E0540(D_8011557C);
        func_800E0540(D_80115580);
        func_800E0540(D_8011558C);
        func_800E0540(D_80115590);
        func_800E0414(D_80115570, D_80115578);
        func_800E0414(D_80115570, D_8011557C);
        func_800E0414(D_80115570, D_80115580);
        func_800E0414(D_80115570, D_80115590);
        func_800DB900(D_80115570);
        func_800E0414(D_80115574, D_80115578);
        func_800E0414(D_80115574, D_80115580);
        func_800E0414(D_80115574, D_80115588);
        func_800E0414(D_80115578, D_80115580);
        func_800E0414(D_80115578, D_80115590);
        func_800DB900(D_80115578);
        func_800E0414(D_8011557C, D_80115580);
        func_800E0414(D_80115580, D_80115590);
        if (Gp_PendingObj4CFlag != 0) {
            Gp_ClearPendingObj4C();
        }
        func_800E0608(D_80115570, 0x9007, 0x9004);
        if (Game_Session->field_12C == 0) {
            func_800E06AC(D_80115570, 0xA007, 0xA004);
        }
    }
}

void func_800DB900(GpObj* node)
{
    GpObj*     other;
    GpU16Pair* rec;
    s32        rowOff;
    s32        temp;
    u16        flags;
    u16        handler;
    u16        swap;
    u8         kind;
    u8         otherKind;

    for (; node != NULL; node = node->next) {
        flags = node->flags;
        other = node->next;
        if (flags & 0x8000) {
            kind = (node->flags & 7) - 1;
            if (other != NULL) {
                rowOff = kind << 4;
                for (; other != NULL; other = other->next) {
                    if (other->flags & 0x8000) {
                        otherKind = (other->flags & 7) - 1;
                        temp      = (otherKind << 2) + rowOff;
                        rec       = &D_8010FA4C[0][0] + (temp >> 2);
                        swap      = rec->field_2;
                        handler   = rec->field_0;
                        if (swap == 0) {
                            D_8010FA38[handler](node, other, handler);
                        } else {
                            D_8010FA38[handler](other, node, handler);
                        }
                    }
                }
            }
        }
    }
}

void func_800DBA20(GpObj* arg0, GpObj* arg1, GpSphereScratch* arg2)
{
    register s32      a3v asm("a3");
    register GpRec18* slot asm("t1");
    register GpRec18* otable asm("t0");
    register GpRec18* otherSlot asm("v1");
    unsigned int      recFlags;
    u16               f0;
    s32               key;

    if (arg1->field_18 == 0) {
        return;
    }

    a3v = 0;
    switch (arg0->flags & 7) {
        case 0:
            break;
        case 1:
            a3v = (s32)arg0->field_C;
            break;
        case 2:
            a3v = (s32)((GpObj*)arg0->field_C)->field_C;
            break;
        case 3:
            a3v = (s32)((GpActorD4Rec*)arg0->field_C)->field_14;
            break;
        empty_or: /* between case 3 and 4 so the empty-slot trampoline matches */
        {
            register s32 tmp asm("v0");
            tmp           = a3v & 0xF0;
            slot->field_0 = recFlags | (tmp + 1);
            goto fill;
        }
        case 4:
            a3v = (s32)((GpObj*)arg0->field_C)->field_8;
            break;
    }
    slot = (GpRec18*)a3v;
    if (slot == NULL) {
        return;
    }

    a3v = arg0->flags;
    if (a3v & 0x800) {
        recFlags = slot->field_0;
        if (recFlags & 1) {
            {
                register s32 cmp asm("v0");
                cmp = 0x100000;
                a3v = 0xFFFF0000;
                if ((slot->field_4 & a3v) != cmp) {
                    a3v    = (((s32)slot->field_12 << 16) & a3v) | (u16)slot->field_10;
                    otable = NULL;
                    switch (((GpObj*)a3v)->flags & 7) {
                        case 0:
                            break;
                        case 1:
                            otable = ((GpObj*)a3v)->field_C;
                            break;
                        case 2:
                            otable = ((GpObj*)((GpObj*)a3v)->field_C)->field_C;
                            break;
                        case 3:
                            otable = ((GpActorD4Rec*)((GpObj*)a3v)->field_C)->field_14;
                            break;
                        case 4:
                            otable = (GpRec18*)((GpObj*)((GpObj*)a3v)->field_C)->field_8;
                            break;
                    }
                    otherSlot = otable;
                    if (otherSlot == NULL) {
                        return;
                    }
                    key = arg0->field_18;
                loop:
                    if (otherSlot->field_4 != key) {
                        if (otherSlot->field_0 & 2) {
                            return;
                        }
                        otherSlot++;
                        goto loop;
                    }
                    f0                  = otherSlot->field_0;
                    otherSlot->field_4  = 0;
                    otherSlot->field_2  = 0;
                    otherSlot->field_8  = 0;
                    otherSlot->field_A  = 0;
                    otherSlot->field_C  = 0;
                    otherSlot->field_10 = 0;
                    otherSlot->field_12 = 0;
                    otherSlot->field_14 = 0;
                    otherSlot->field_0  = f0 & 0xFFFE;
                }
            }
            recFlags      = slot->field_0;
            recFlags      = recFlags | ((arg0->flags & 0xF0) + 1);
            slot->field_0 = recFlags;
            goto fill;
        } else {
            goto empty_or;
        }
    } else {
        while (1) {
            recFlags = slot->field_0;
            if (!(recFlags & 1)) {
                break;
            }
            if (recFlags & 2) {
                return;
            }
            slot++;
        }
        slot->field_0 = recFlags | ((arg0->flags & 0xF0) + 1);
    }

fill:
    slot->field_4              = arg1->field_18;
    slot->field_2              = (u16)arg2->rsum;
    *(SVECTOR*)&slot->field_8  = arg2->src;
    *(SVECTOR*)&slot->field_10 = arg2->extra;
}

s32 func_800DBCAC(GpObj* arg0, GpObj* arg1)
{
    void**           scratch;
    u8*              head;
    GpSphereScratch* block;
    register s32     dx asm("v0");
    register s32     a asm("a0");
    register s32     b asm("v1");
    register s32     c asm("a1");
    register s32     ret asm("s5");
    register s32     t0 asm("a2");
    s32              dz;
    s32              rsum;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (GpSphereScratch*)(head - 0x48);
    *scratch = block;
    Gp_ObjWorldPos(arg0, (VECTOR3*)(head - 0x34));
    Gp_ObjWorldPos(arg1, (VECTOR3*)(head - 0x24));

    dx              = block->pos0.vx;
    a               = block->pos1.vx;
    b               = block->pos0.vy;
    c               = block->pos1.vy;
    dx             -= a;
    a               = block->pos0.vz;
    b              -= c;
    block->delta.vy = b;
    b               = block->pos1.vz;
    ret             = 0;
    block->delta.vx = dx;
    __asm__ volatile("" ::: "memory");
    if (dx < 0) {
        dx = -dx;
    }
    dz              = a - b;
    block->delta.vz = dz;
    if ((dx > 0x7FFF) || (ABS(dz) > 0x7FFF)) {
        *scratch = (u8*)*scratch + 0x48;
        return 0;
    }

    __asm__ volatile("" ::: "memory");
    dx            = block->delta.vx;
    t0            = dx * dx;
    dx            = block->delta.vy;
    c             = dx * dx;
    dx            = block->delta.vz;
    a             = dx * dx;
    rsum          = (u16)arg0->field_1C + (u16)arg1->field_1C;
    block->rsum32 = rsum;
    dx            = t0 + c + a;
    if (dx < (b = rsum * rsum)) {
        s32 rad;

        a                             = (s32)arg0;
        c                             = (s32)arg1;
        dx                            = (u16)block->pos1.vx;
        t0                            = (s32)block;
        ((SVECTOR*)(head - 0x48))->vx = dx;
        dx                            = (u16)block->pos1.vy;
        b                             = (u16)block->pos1.vz;
        rad                           = (u16)block->rsum32;
        ret                           = 1;
        block->extra.vx               = 0;
        block->extra.vy               = 0;
        block->extra.vz               = 0;
        block->src.vy                 = dx;
        block->src.vz                 = b;
        block->rsum                   = rad;
        func_800DBA20((GpObj*)a, (GpObj*)c, (GpSphereScratch*)t0);

        ((SVECTOR*)(head - 0x48))->vx = (s16)block->pos0.vx;
        block->src.vy                 = (s16)block->pos0.vy;
        block->src.vz                 = (s16)block->pos0.vz;
        block->extra.vx               = 0;
        block->extra.vy               = 0;
        block->extra.vz               = 0;
        block->rsum                   = (s16)block->rsum32;
        func_800DBA20(arg1, arg0, block);
    }

    *scratch = (u8*)*scratch + 0x48;
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBE7C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DC528);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DCB80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD324);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD940);

void func_800DDC2C(GpObj* arg0)
{
    register GpObj*         obj asm("a0");
    register SVECTOR*       dir asm("a1");
    register void**         scratch asm("a2");
    register u8*            head asm("s0");
    register GpEdgeScratch* block asm("s1");
    GsCOORDINATE2*          coord;
    s32                     prod;
    s32                     x;

    obj     = arg0;
    dir     = (SVECTOR*)obj->field_C;
    prod    = dir->vx * (u16)obj->field_1C;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register s32 tmp asm("v0");

        tmp              = (s32)head - 0x50;
        x                = (u16)obj->field_10;
        block            = (GpEdgeScratch*)tmp;
        block->src[0].vy = 0;
        x               += prod >> 12;
        block->src[0].vx = x;
    }
    block->src[0].vz = (u16)obj->field_14 + ((dir->vz * (u16)obj->field_1C) >> 12);
    prod             = dir->vx * (u16)obj->field_1C;
    x                = (u16)obj->field_10;
    block->src[1].vy = 0;
    x               += (-prod) >> 12;
    block->src[1].vx = x;
    head            -= 0x20;
    block->src[1].vz = (u16)obj->field_14 + ((-(dir->vz * (u16)obj->field_1C)) >> 12);
    coord            = (GsCOORDINATE2*)obj->field_8;
    *scratch         = block;
    Gp_WorldToLocal(&D_80070F34, &coord->workm, (MATRIX*)head);
    gte_SetRotMatrix((MATRIX*)head);
    {
        register VECTOR*       out asm("a2");
        register s32           off asm("a3");
        register s32           i asm("t0");
        register s32           val asm("v0");
        register s32           t asm("v1");
        register GpGridParams* p asm("a1");
        register s32           y asm("a0");
        register s32           hi asm("t1");

        i = 0;
        asm volatile("lui %0, %%hi(Gp_GridParams)" : "=r"(hi) : "r"(i) : "memory");
        out = block->pos;
        off = 0x20;
        do {
            gte_ldv0((SVECTOR*)((u8*)block + off));
            gte_rtv0_real();
            gte_stlvnl(out);
            off += 8;
            i++;
            val = out->vx;
            asm volatile("" : "+r"(val));
            asm("lw %0, %%lo(Gp_GridParams)(%2)\n\tlw %1, 68(%3)" : "=r"(p), "=r"(t) : "r"(hi), "r"(block));
            y       = p->field_14;
            out->vy = 0;
            out->vx = val + t + y;
            y       = p->field_18;
            out->vz = out->vz + block->mat.t[2] + y;
            out++;
        } while (i < 2);
        func_800DE2C0(block->pos, 0);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DDDF8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE150);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE2C0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE7CC);

void func_800DEAFC(SVECTOR* arg0, SVECTOR* arg1)
{
    void**                 scratch;
    u8*                    head;
    GpGridPairScratch*     block;
    VECTOR*                out;
    register GpGridParams* p asm("a2");

    scratch      = (void**)G_SCRATCH_HEAD;
    head         = *scratch;
    block        = (GpGridPairScratch*)(head - 0x40);
    block->in.vx = arg0->vx;
    block->in.vy = arg0->vy;
    block->in.vz = arg0->vz;
    out          = (VECTOR*)(head - 0x30);
    *scratch     = block;
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &block->in, out);
    p              = Gp_GridParams;
    block->pos0.vx = (s16)((u16)block->out.vx + (u16)p->field_14 - (u16)p->field_0->coord.t[0]);
    block->pos0.vy = 0;
    block->pos0.vz = (s16)((u16)block->out.vz + (u16)p->field_18 - (u16)p->field_0->coord.t[2]);
    block->in.vx   = arg1->vx;
    block->in.vy   = arg1->vy;
    block->in.vz   = arg1->vz;
    ApplyTransposeMatrixLV(&p->field_0->workm, &block->in, out);
    p              = Gp_GridParams;
    block->pos1.vx = (s16)((u16)block->out.vx + (u16)p->field_14 - (u16)p->field_0->coord.t[0]);
    block->pos1.vy = 0;
    block->pos1.vz = (s16)((u16)block->out.vz + (u16)p->field_18 - (u16)p->field_0->coord.t[2]);
    func_800DE2C0((VECTOR*)(head - 0x20), 0);
    *scratch = (u8*)*scratch + 0x40;
}

void func_800DEC80(GpObj* arg0, VECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    register GpObj*         obj asm("t2");
    register void**         scratch asm("v1");
    register u8*            head asm("v0");
    register s32            found asm("t1");
    register GpActorD4Rec*  rec asm("t7");
    register GpNormScratch* block asm("a0");
    register VECTOR*        pos asm("t0");
    register SVECTOR*       src asm("a3");
    register SVECTOR*       local asm("t3");
    register s32            temp asm("v0");
    GpRec18*                slot;
    s32                     flags;

    obj     = arg0;
    scratch = (void**)G_SCRATCH_HEAD;
    asm volatile("" : "+r"(obj), "+r"(scratch));
    head     = *scratch;
    found    = 0;
    head    -= 0x18;
    *scratch = head;
    rec      = (GpActorD4Rec*)obj->field_C;
    block    = (GpNormScratch*)head;

    if (arg3 == 0) {
        if (obj->flags & 0x800) {
            temp = (s32)rec;
            asm volatile("" : "+r"(temp));
            slot = ((GpActorD4Rec*)temp)->field_14;
            for (;;) {
                flags = slot->field_0;
                if (flags & 1) {
                    arg1->vx = slot->field_8;
                    arg1->vy = slot->field_A;
                    arg1->vz = slot->field_C;
                    found    = 1;
                    goto done_search;
                }
                if (flags & 2) {
                    goto done_search;
                }
                slot++;
            }
        } else if (obj->flags & 0x400) {
            slot = ((GpActorD4Rec*)obj->field_C)->field_14;
            for (;;) {
                if (slot->field_0 & 1) {
                    if ((slot->field_4 & 0xFFFF0000) == 0x100000) {
                        arg1->vx = slot->field_8;
                        arg1->vy = slot->field_A;
                        arg1->vz = slot->field_C;
                        found    = 1;
                        goto done_search;
                    }
                }
                if (slot->field_0 & 2) {
                    goto done_search;
                }
                slot++;
            }
        }
    } else if (obj->flags & 0x400) {
        slot = ((GpActorD4Rec*)obj->field_C)->field_14;
        for (;;) {
            if (slot->field_0 & 1) {
                if ((slot->field_4 & 0xFFFF0000) == 0x100000) {
                    arg1->vx = slot->field_8;
                    arg1->vy = slot->field_A;
                    arg1->vz = slot->field_C;
                    found    = 1;
                    goto done_search;
                }
            }
            if (slot->field_0 & 2) {
                break;
            }
            slot++;
        }
    }

done_search:
    gte_SetRotMatrix(&((GsCOORDINATE2*)obj->field_8)->workm);
    if (found < 2) {
        local = &block->local;
        temp  = found << 4;
        pos   = (VECTOR*)(temp + (s32)arg1);
        temp  = found << 3;
        src   = (SVECTOR*)(temp + (s32)rec);
        do {
            block->local.vx = (u16)src->vx + (u16)obj->field_10;
            block->local.vy = (u16)src->vy + (u16)obj->field_12;
            block->local.vz = (u16)src->vz + (u16)obj->field_14;
            gte_ldv0(local);
            gte_rtv0_real();
            gte_stlvnl(&block->vec);
            pos->vx = block->vec.vx + ((GsCOORDINATE2*)obj->field_8)->workm.t[0];
            pos->vy = block->vec.vy + ((GsCOORDINATE2*)obj->field_8)->workm.t[1];
            pos->vz = block->vec.vz + ((GsCOORDINATE2*)obj->field_8)->workm.t[2];
            src++;
            found++;
            pos++;
        } while (found < 2);
    }

    block->vec.vx = arg1[0].vx - arg1[1].vx;
    block->vec.vy = arg1[0].vy - arg1[1].vy;
    block->vec.vz = arg1[0].vz - arg1[1].vz;
    VectorNormalS(&block->vec, arg2);

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEF80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DF6AC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DFCCC);

void Gp_ClearObjHeads(void)
{
    D_80115570          = NULL;
    D_80115574          = NULL;
    D_80115578          = NULL;
    D_8011557C          = NULL;
    D_80115580          = NULL;
    D_80115584          = NULL;
    D_80115588          = NULL;
    D_8011558C          = NULL;
    D_80115590          = NULL;
    Gp_GridParams       = 0;
    Gp_PendingObj4C     = NULL;
    Gp_Obj4CList        = NULL;
    D_80115550          = NULL;
    Gp_PendingObj4CFlag = 0;
}

s32 func_800E0308(SVECTOR* arg0, SVECTOR* arg1)
{
    void**           scratch;
    register u8*     head asm("v0");
    register VECTOR* vec asm("s1");
    GpObj3A*         node;
    s32              ret;

    ret                          = 0;
    scratch                      = (void**)G_SCRATCH_HEAD;
    node                         = D_80115550;
    head                         = *scratch;
    ((VECTOR*)(head - 0x10))->vx = arg1->vx - arg0->vx;
    head                         = head - 0x10;
    vec                          = (VECTOR*)head;
    __asm__ volatile("" : "+r"(vec) : "r"(head));
    vec->vy  = arg1->vy - arg0->vy;
    *scratch = vec;
    vec->vz  = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
    return ret;
}

void func_800E0414(GpObj* a, GpObj* b)
{
    GpObj*     other;
    GpU16Pair* rec;
    s32        rowOff;
    s32        temp;
    u16        flags;
    u16        handler;
    u16        swap;
    u8         kind;
    u8         otherKind;

    for (; a != NULL; a = a->next) {
        flags = a->flags;
        if (flags & 0x8000) {
            kind  = (a->flags & 7) - 1;
            other = b;
            if (other != NULL) {
                rowOff = kind << 4;
                for (; other != NULL; other = other->next) {
                    if (other->flags & 0x8000) {
                        otherKind = (other->flags & 7) - 1;
                        temp      = (otherKind << 2) + rowOff;
                        rec       = &D_8010FA4C[0][0] + (temp >> 2);
                        swap      = rec->field_2;
                        handler   = rec->field_0;
                        if (swap == 0) {
                            D_8010FA38[handler](a, other, handler);
                        } else {
                            D_8010FA38[handler](other, a, handler);
                        }
                    }
                }
            }
        }
    }
}

void func_800E0540(GpObj* node)
{
    u16 flags;

    if (Gp_GridParams != 0) {
        for (; node != NULL; node = node->next) {
            flags = node->flags;
            if (flags & 0x4000) {
                switch (flags & 7) {
                    case 0:
                        break;
                    case 1:
                        func_800DC528(node);
                        break;
                    case 2:
                        break;
                    case 3:
                        func_800DDDF8(node);
                        break;
                    case 4:
                        if (node->flags & 0x200) {
                            func_800DD940(node);
                        }
                        func_800DCB80(node);
                        break;
                }
            }
        }
    }
}

void func_800E0608(GpObj* node, s32 mask, s32 match)
{
    GpObj4C* other;

    other = Gp_PendingObj4C;
    for (; node != NULL; node = node->next) {
        if ((node->flags & mask) == (u16)match) {
            for (; other != NULL; other = other->next) {
                if (other->field_4A & 0x40) {
                    func_800DEF80(node, other);
                }
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E06AC);

s32 Gp_PairNop(void)
{
    return 0;
}

void Gp_LocalToGrid(VECTOR3* arg0, SVECTOR3* arg1)
{
    void**        scratch;
    u8*           head;
    VECTOR*       vec;
    GpGridParams* p;
    s32           val;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    vec = *scratch = (VECTOR*)(head - 0x10);
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)arg0, vec);
    p   = Gp_GridParams;
    val = ((VECTOR*)(head - 0x10))->vx + p->field_14 - p->field_0->coord.t[0];
    if (val >= 0) {
        arg1->vx = val / p->field_20;
    } else {
        arg1->vx = -1;
    }
    p        = Gp_GridParams;
    arg1->vy = 0;
    val      = vec->vz + p->field_18 - p->field_0->coord.t[2];
    if (val >= 0) {
        arg1->vz = val / p->field_20;
    } else {
        arg1->vz = -1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void Gp_ObjWorldPos(GpObj* arg0, VECTOR3* arg1)
{
    void**   scratch;
    u8*      head;
    VECTOR3* vec;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (VECTOR3*)(head - 0x30);
    *scratch = vec;
    __asm__ volatile("" ::: "memory");
    gte_SetRotMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
    gte_ldv0(&arg0->field_10);
    gte_rtv0_real();
    gte_stlvnl(vec);
    arg1->vx = ((GsCOORDINATE2*)arg0->field_8)->workm.t[0] + ((VECTOR3*)(head - 0x30))->vx;
    arg1->vy = ((GsCOORDINATE2*)arg0->field_8)->workm.t[1] + vec->vy;
    arg1->vz = ((GsCOORDINATE2*)arg0->field_8)->workm.t[2] + vec->vz;
    *scratch = (u8*)*scratch + 0x30;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0994);

void Gp_ClearPendingObj4C(void)
{
    GpObj4C* node;

    for (node = Gp_PendingObj4C; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            node->field_4B = 0;
        }
    }
}

void Gp_WorldToGrid(VECTOR3* arg0, SVECTOR3* arg1)
{
    s32           val;
    GpGridParams* p;

    p   = Gp_GridParams;
    val = arg0->vx + p->field_14;
    if (val >= 0) {
        arg1->vx = val / p->field_20;
    } else {
        arg1->vx = -1;
    }
    p        = Gp_GridParams;
    arg1->vy = 0;
    val      = arg0->vz + p->field_18;
    if (val >= 0) {
        arg1->vz = val / p->field_20;
    } else {
        arg1->vz = -1;
    }
}

s32 func_800E0C10(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3)
{
    void**          scratch;
    u8*             head;
    GpSlideScratch* s;
    GpRec18*        rec;
    s32             i;
    s32             j;
    s32             count;
    s32             mask;
    s32             ret;

    count = 0;
    ret   = 0;
    mask  = 0;
    /* `list` is a VLA, so its alloca has to be emitted after the three
     * initializations above; the inner block is what pins that order. */
    {
        s16 list[arg2];

        if (arg2 == 0) {
            return count;
        }

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        *scratch = head - 0x34;
        s        = (GpSlideScratch*)(head - 0x34);

        s->acc[0].vx = 0;
        s->acc[0].vy = 0;
        s->acc[0].vz = 0;
        s->acc[1].vx = 0;
        s->acc[1].vy = 0;
        s->acc[1].vz = 0;
        s->count     = 0;

        for (i = 0; i < arg2; i++) {
            rec = &arg0[i];
            if ((rec->field_0 & 1) && (rec->field_4 & 0xFFFF0000) == 0x100000) {
                mask |= 1 << rec->field_4;
                if (Gp_RoomParams[rec->field_4 & 7] == 0) {
                    if (rec->field_12 >= -0xDDA) {
                        s->acc[0].vx += rec->field_10 * rec->field_2;
                        s->acc[0].vy += rec->field_12 * rec->field_2;
                        s->acc[0].vz += rec->field_14 * rec->field_2;
                        list[count++] = i;
                    } else {
                        s->acc[1].vx  = 0;
                        s->acc[1].vy += rec->field_12 * rec->field_2;
                        s->acc[1].vz  = 0;
                        s->count++;
                    }
                }
                ret = 1;
            }
        }

        if (arg3 != NULL) {
            *arg3 = mask;
        }

        for (i = 0; i < count; i++) {
            for (j = 1; j < count; j++) {
                s->acc[2].vx = arg0[list[i]].field_10 * arg0[list[j]].field_10;
                s->acc[2].vz = arg0[list[i]].field_14 * arg0[list[j]].field_14;
                if (s->acc[2].vx < -0x800000 || s->acc[2].vz < -0x800000) {
                    ret = 2;
                }
            }
        }

        arg1->vx.w = s->acc[0].vx << 4;
        arg1->vy.w = s->acc[0].vy << 4;
        arg1->vz.w = s->acc[0].vz << 4;
        if (s->count != 0) {
            arg1->vx.w += (s->acc[1].vx / s->count) << 4;
            arg1->vy.w += (s->acc[1].vy / s->count) << 4;
            arg1->vz.w += (s->acc[1].vz / s->count) << 4;
        }

        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
        return ret;
    }
}

s32 func_800E0FEC(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3)
{
    void**         scratch;
    u8*            head;
    GpPushScratch* s;
    GpRec18*       rec;
    s32            i;
    s32            j;
    s32            count;
    s32            mask;
    s32            ret;
    s32            prev;
    u8             list[0x20];

    ret   = 0;
    count = 0;
    mask  = 0;
    prev  = 0;
    if (arg2 == 0) {
        return ret;
    }

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x40;
    s        = (GpPushScratch*)(head - 0x40);

    for (i = 0; i < 3; i++) {
        s->acc[i].vx = 0;
        s->acc[i].vy = 0;
        s->acc[i].vz = 0;
    }

    for (i = 0; i < arg2; i++) {
        rec = &arg0[i];
        if ((rec->field_0 & 1) && (rec->field_4 & 0xFFFF0000) == 0x100000) {
            mask |= 1 << rec->field_4;
            if (Gp_RoomParams[rec->field_4 & 7] == 0) {
                switch ((u32)(rec->field_4 & 0xF00) >> 8) {
                    case 0:
                        s->acc[0].vx += rec->field_2 * rec->field_10;
                        s->acc[0].vy += rec->field_2 * rec->field_12;
                        s->acc[0].vz += rec->field_2 * rec->field_14;
                        list[count++] = i;
                        break;
                    case 1:
                        s->acc[1].vx = 0;
                        s->acc[1].vy = -(rec->field_2 << 12);
                        s->acc[1].vz = 0;
                        break;
                    case 2:
                        if (rec->field_12 == 0 && ((s16)prev == 0 || rec->field_2 < (s16)prev)) {
                            s->acc[2].vx = rec->field_2 * rec->field_10;
                            s->acc[2].vy = 0;
                            s->acc[2].vz = rec->field_2 * rec->field_14;
                            prev         = (u16)rec->field_2;
                        }
                        break;
                }
            }
            ret = 1;
        }
    }

    for (i = 0; i < count; i++) {
        for (j = 1; j < count; j++) {
            s->acc[3].vx = arg0[list[i]].field_10 * arg0[list[j]].field_10;
            s->acc[3].vz = arg0[list[i]].field_14 * arg0[list[j]].field_14;
            if (s->acc[3].vx < -0x800000 || s->acc[3].vz < -0x800000) {
                ret = 2;
            }
        }
    }

    if (arg3 != NULL) {
        *arg3 = mask;
    }

    if (count != 0) {
        arg1->vx.w = (s->acc[0].vx + s->acc[1].vx) << 4;
        arg1->vy.w = (s->acc[0].vy + s->acc[1].vy) << 4;
        arg1->vz.w = (s->acc[0].vz + s->acc[1].vz) << 4;
    } else {
        arg1->vx.w = (s->acc[1].vx + s->acc[2].vx) << 4;
        arg1->vy.w = (s->acc[1].vy + s->acc[2].vy) << 4;
        arg1->vz.w = (s->acc[1].vz + s->acc[2].vz) << 4;
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x40;
    return ret;
}

s32 Gp_FindNearestSlot(GpObj* arg0, s32 arg1)
{
    register void** scratch asm("v0");
    register s32    hi asm("v0");
    u8*             head;
    GpNearScratch*  block;
    GpActorD4Rec*   rec;
    GpRec18*        slot;
    s32             minDist;
    s32             index;
    s32             best;
    s32             dx;
    s32             dy;
    s32             dz;
    s32             dist;

    minDist = -1;
    index   = 0;
    best    = index;
    asm("lui %0, 0x1F80" : "=r"(hi) : "r"(best));
    asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
    rec      = (GpActorD4Rec*)arg0->field_C;
    head     = *scratch;
    slot     = rec->field_14;
    *scratch = (void*)(head - 0x28);
    __asm__ volatile("" ::: "memory");
    block = (GpNearScratch*)(head - 0x28);
    gte_SetRotMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
    block->local.vx = (u16)rec->field_8 + (u16)arg0->field_10;
    block->local.vy = (u16)rec->field_A + (u16)arg0->field_12;
    block->local.vz = (u16)rec->field_C + (u16)arg0->field_14;
    gte_ldv0((SVECTOR*)(head - 8));
    gte_rtv0_real();
    gte_stlvnl(block);
    block->world.vx = ((VECTOR3*)(head - 0x28))->vx + ((GsCOORDINATE2*)arg0->field_8)->workm.t[0];
    block->world.vy = block->vec.vy + ((GsCOORDINATE2*)arg0->field_8)->workm.t[1];
    block->world.vz = block->vec.vz + ((GsCOORDINATE2*)arg0->field_8)->workm.t[2];

    for (;;) {
        if ((slot->field_0 & 1) && ((slot->field_4 & 0xFFFF0000) == arg1)) {
            dx            = slot->field_8 - block->world.vx;
            block->vec.vx = dx;
            dy            = slot->field_A - block->world.vy;
            block->vec.vy = dy;
            dz            = slot->field_C - block->world.vz;
            block->vec.vz = dz;
            dist          = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
            if ((u32)dist < (u32)minDist) {
                minDist = dist;
                best    = index + 1;
            }
        }
        if (slot->field_0 & 2) {
            break;
        }
        slot++;
        index++;
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
    return best;
}

void Gp_LinkObj(s32 arg0, GpObj* arg1)
{
    u16    flags;
    GpObj* head;
    GpObj* node;
    GpObj* temp;

    head  = Gp_ObjLists[arg0];
    flags = arg1->flags;
    if (!(flags & 0x8)) {
        if ((flags & 0x7) < 5) {
            arg1->flags = flags | 0x8;
            temp        = head->next;
            if (temp != NULL) {
                node = temp;
                while (node->next != NULL) {
                    node = node->next;
                }
                node->next = arg1;
                arg1->prev = node;
            } else {
                head->next = arg1;
                arg1->prev = head;
            }
            arg1->next = NULL;
        }
    }
}

void Gp_UnlinkObj(GpObj* node)
{
    u16    flags;
    GpObj* next;
    GpObj* prev;

    flags = node->flags;
    if (flags & 0x8) {
        next        = node->next;
        node->flags = flags & 0x7;
        prev        = node->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = node->prev;
            node->next = NULL;
        } else {
            prev->next = NULL;
        }
        node->prev = NULL;
    }
}

void Gp_LinkObj4A(s32 arg0, GpObj4A* arg1)
{
    u8       flags;
    GpObj4A* head;
    GpObj4A* node;
    GpObj4A* temp;

    head  = Gp_Obj4ALists[arg0];
    flags = arg1->field_4A;
    if (!(flags & 0x20)) {
        arg1->field_4A = flags | 0x20;
        temp           = head->next;
        if (temp != NULL) {
            node = temp;
            while (node->next != NULL) {
                node = node->next;
            }
            node->next = arg1;
            arg1->prev = node;
        } else {
            head->next = arg1;
            arg1->prev = head;
        }
        arg1->next = NULL;
    }
}

void Gp_UnlinkObj4A(s32 arg0, GpObj4A* arg1)
{
    u8       flags;
    GpObj4A* next;
    GpObj4A* prev;

    flags = arg1->field_4A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_4A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            prev->next = NULL;
        }
        arg1->prev = NULL;
    }
}

void Gp_ClearObj4AList(s32 arg0)
{
    GpObj4A* head;
    GpObj4A* node;
    GpObj4A* next;
    GpObj4A* temp;
    s32      flags;
    s32      mask;

    head = Gp_Obj4ALists[arg0];
    temp = head->next;
    if (temp != NULL) {
        node       = temp;
        head->next = NULL;
        mask       = ~0x78;
    loop:
        flags          = node->field_4A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_4A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

void Gp_LinkObj3A(s32 arg0, GpObj3A* arg1)
{
    u8       flags;
    GpObj3A* head;
    GpObj3A* node;
    GpObj3A* temp;

    head  = Gp_Obj3ALists[arg0];
    flags = arg1->field_3A;
    if (!(flags & 0x20)) {
        arg1->field_3A = flags | 0x20;
        temp           = head->next;
        if (temp != NULL) {
            node = temp;
            while (node->next != NULL) {
                node = node->next;
            }
            node->next = arg1;
            arg1->prev = node;
        } else {
            head->next = arg1;
            arg1->prev = head;
        }
        arg1->next = NULL;
    }
}

void Gp_UnlinkObj3A(s32 arg0, GpObj3A* arg1)
{
    u8       flags;
    GpObj3A* next;
    GpObj3A* prev;

    flags = arg1->field_3A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_3A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            prev->next = NULL;
        }
        arg1->prev = NULL;
    }
}

void Gp_ClearObj3AList(s32 arg0)
{
    GpObj3A* head;
    GpObj3A* node;
    GpObj3A* next;
    GpObj3A* temp;
    s32      flags;
    s32      mask;

    head = Gp_Obj3ALists[arg0];
    temp = head->next;
    if (temp != NULL) {
        node       = temp;
        head->next = NULL;
        mask       = ~0x78;
    loop:
        flags          = node->field_3A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_3A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

void Gp_InitRec18Table(GpRec18* arg0, s32 arg1, s32 arg2)
{
    Mem_Set(arg0, 0, arg1 * 0x18);
    arg0[arg1 - 1].field_0 = 2;
}

void Gp_LoadRoomParams(void)
{
    s32              i;
    GameSession*     session;
    GpRoomParamRec** recs;

    for (i = 7; i >= 0; i--) {
        Gp_RoomParams[i] = 0;
    }

    session = Game_Session;
    recs    = Gp_RoomParamTables[session->field_7 - 1][session->field_6 - 1];
    for (i = 0; i < 8; i++) {
        Gp_RoomParams[i] = recs[i]->field_3;
    }
}

s32 Gp_FindRec18(GpRec18* arg0, s32 arg1)
{
    s32 result;
    s32 index;

    result = 0;
    for (index = 1;; index++) {
        if (arg0->field_0 & 1) {
            if (arg1 == 0) {
                return 1;
            }
            if (arg0->field_4 == arg1) {
                result = index;
            }
        }
        if ((arg0++)->field_0 & 2) {
            break;
        }
    }
    return result;
}

s32 Gp_CountRec18Hi(GpRec18* arg0, s32 arg1)
{
    s32 count;

    count = 0;
    do {
        if ((arg0->field_0 & 1) && ((arg0->field_4 & 0xFFFF0000) == arg1)) {
            count += 1;
        }
    } while (!((arg0++)->field_0 & 2));
    return count;
}

void Gp_ClearRec18Occupied(GpRec18* arg0)
{
    for (;;) {
        if (arg0->field_0 & 1) {
            arg0->field_0 &= 2;
            arg0->field_2  = 0;
            arg0->field_4  = 0;
            arg0->field_8  = 0;
            arg0->field_A  = 0;
            arg0->field_C  = 0;
            arg0->field_10 = 0;
            arg0->field_12 = 0;
            arg0->field_14 = 0;
        }
        if (arg0->field_0 & 2) {
            break;
        }
        arg0++;
    }
}

s32 func_800E1ACC(u8* arg0)
{
    s32 val;
    s32 ret;

    val = *arg0 << 12;
    if (val != 0) {
        ret = cln(val) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

s32 func_800E1B24(s32 arg0)
{
    s32 mask[2];
    s32 val;
    s32 tmp;
    s32 ret;

    val     = 1 << arg0;
    mask[0] = val;
    tmp     = (u8)val << 12;
    if (tmp != 0) {
        ret = cln(tmp) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

void Gp_CommitObj4CSave(void)
{
    GpObj4C* node;

    for (node = Gp_Obj4CList; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            node->field_4B = 0;
            if ((u8)Game_Session->field_4 == node->field_48) {
                Mc_SaveData.field_4 = node->field_49;
            }
        }
    }
}

s32 Gp_TakePendingObj4C(u16* arg0, u8* arg1, u8* arg2)
{
    GpObj4C* node;

    for (node = Gp_PendingObj4C; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            Gp_PendingObj4CFlag = 1;
            *arg0               = node->field_46;
            *arg1               = node->field_48;
            *arg2               = node->field_49;
            return 1;
        }
    }
    return 0;
}

void Gp_ClaimSlot18(GpObj54* arg0, void* arg1)
{
    GpSlot18*  slot;
    GpSlot18*  temp;
    s32        one;
    GpStateF0* p;

    temp = arg0->field_54;
    if (temp != NULL) {
        slot = temp;
        one  = 1;
        while (1) {
            if ((*(s32*)&slot->field_0 & 3) != one) {
                break;
            }
            slot++;
        }
        slot->field_4  = arg1;
        slot->field_2  = 0;
        slot->field_8  = 0;
        slot->field_A  = 0;
        slot->field_C  = 0;
        slot->field_10 = 0;
        slot->field_12 = 0;
        slot->field_14 = 0;
        slot->field_0 |= 1;
        p              = &Gp_StateF0;
        p->field_5++;
    }
}

void Gp_OrientAlong(VECTOR* arg0, MATRIX* arg1, s32 arg2)
{
    void**           scratch;
    u8*              head;
    GpDirMatScratch* block;
    SVECTOR*         vec;
    MATRIX*          mat1;
    MATRIX*          mat2;
    s32              sin_yaw;
    s32              yaw;
    s32              pitch;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (GpDirMatScratch*)(head - 0x4C);
    vec      = (SVECTOR*)block;
    *scratch = block;
    VectorNormalS(arg0, vec);

    mat1       = (MATRIX*)(head - 0x44);
    yaw        = ratan2(((SVECTOR*)(head - 0x4C))->vx, vec->vz) & 0xFFF;
    block->yaw = yaw;
    sin_yaw    = rsin(yaw);
    block->pitch =
        ratan2(vec->vy, (vec->vx * sin_yaw + vec->vz * rcos(block->yaw)) >> 12) & 0xFFF;

    ((SVECTOR*)(head - 0x4C))->vx = 0;
    vec->vz                       = 0;
    vec->vy                       = block->yaw;
    RotMatrix(vec, mat1);

    mat2                          = (MATRIX*)(head - 0x24);
    pitch                         = block->pitch;
    ((SVECTOR*)(head - 0x4C))->vx = -pitch;
    vec->vy                       = 0;
    vec->vz                       = 0;
    RotMatrix(vec, mat2);

    gte_SetRotMatrix(mat1);
    gte_ldclmv(mat2);
    gte_rtir_real();
    gte_stclmv(mat1);
    gte_ldclmv(&mat2->m[0][1]);
    gte_rtir_real();
    gte_stclmv(&mat1->m[0][1]);
    gte_ldclmv(&mat2->m[0][2]);
    gte_rtir_real();
    gte_stclmv(&mat1->m[0][2]);

    ((SVECTOR*)(head - 0x4C))->vx = 0;
    vec->vy                       = 0;
    vec->vz                       = arg2;
    RotMatrix(vec, mat2);

    gte_SetRotMatrix(mat1);
    gte_ldclmv(mat2);
    gte_rtir_real();
    gte_stclmv(arg1);
    gte_ldclmv(&mat2->m[0][1]);
    gte_rtir_real();
    gte_stclmv(&arg1->m[0][1]);
    gte_ldclmv(&mat2->m[0][2]);
    gte_rtir_real();
    gte_stclmv(&arg1->m[0][2]);

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x4C;
}

u32 func_800E1FEC(u32 arg0, u32 arg1, s32 arg2, s32 arg3)
{
    u8  flag;
    u32 dmg;

    flag = 0;
    if ((arg0 & 0xFFFF0000) != 0x20000) {
        return 0;
    }

    if ((arg0 & 0x8000) == 0) {
        u8  lo;
        u32 base;
        u32 raw;
        u32 rand;
        s32 pct;
        u8  col;
        s32 sel;
        s32 val;
        s32 mult;
        u32 tmp;
        s32 extra;

        if ((arg0 & 0x80) == 0) {
            if ((arg0 & 0x7F) < 0x21) {
                flag = 1;
            }
        }
        lo   = arg0 & 0x7F;
        arg0 = (arg0 >> 8) & 0x3F;
        raw  = Gp_IdParamLo[lo].field_0;
        base = raw << 8;
        if (flag != 0) {
            if ((Wip_SysConfig.field_25 & 0x80) != 0) {
                base = base * 150 / 100;
            }
        }

        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        rand        = (u32)Gp_LcgState >> 16;
        pct         = (u16)(rand % 20) + 100;
        base        = base * pct / 100;

        col = arg1 / 1000;
        if (col < 0x10) {
            sel = (u8)D_80113864[col];
        } else {
            sel = 5;
        }
        val = (D_80113568[arg0][sel] << 8) / 100;

        if (arg2 == 0) {
            mult = 0x100;
        } else if (Gp_IdParamLo[lo].field_4 == arg2) {
            mult = arg3;
        } else {
            mult = 0x100;
        }

        tmp = base * val >> 8;
        dmg = tmp * mult >> 16;

        if (flag != 0) {
            extra = (s8)Gp_StateC08.field_D;
            if (extra != 0) {
                dmg = dmg * D_80113D0C[(extra / 16 - 1) * 2 + (s8)(extra % 16)][0] / 100;
            }
            if (func_800B9D80(0x10000) != 0) {
                dmg = dmg * 120 / 100;
            }
        }

        dmg = dmg * D_80113F90[D_8011541B] / 100;
        if (dmg == 0) {
            if (base != 0) {
                dmg = 1;
            }
        }
    } else {
        u32 rnd;
        s32 pc;

        dmg = Gp_IdParamHi[arg0 & 0x7F].field[4];
        if ((arg0 & 0x7F) >= 0x19 && (arg0 & 0x7F) < 0x1C) {
            if (Gp_StateF0.field_5 != 0) {
                dmg = dmg / Gp_StateF0.field_5;
            } else {
                dmg = 0;
            }
        }

        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        rnd         = (u32)Gp_LcgState >> 16;
        pc          = (u16)(rnd % 10) + 100;
        dmg         = dmg * pc / 100;

        if (func_800B9D80(0x20000) != 0) {
            dmg = dmg * 150 / 100;
        }

        dmg = dmg * D_80113F90[D_8011541B] / 100;
    }
    return dmg;
}

s32 Gp_ScaleDamage(s32 arg0, s32 arg1, s32* arg2, s32 arg3)
{
    s32                ret;
    s32                lo;
    s32                extra;
    s8                 rem;
    u16*               scaleTbl;
    register s32       hp asm("v0");
    register GpDmgRow* table asm("a1");
    register u16*      cols asm("a0");
    register s32       val asm("a1");
    register s32       addr asm("v0");
    register s32       scale asm("v0");
    register s32       div asm("v0");
    register u32       mag asm("v0");

    ret = 0;
    if ((arg0 & 0xFFFF0000) != 0x40000) {
        return ret;
    }

    lo = arg0 & 0xFFF;
    if (arg2 != NULL) {
        *arg2 = ((u32)arg0 >> 12) & 0xF;
    }

    if (arg3 == 0) {
        register s32 col asm("v1");
        register s32 row asm("a0");

        hp    = Wip_SysConfig.field_18;
        table = Gp_DmgRows;
        cols  = D_80113F54;
        addr  = (s32)&cols[hp / 10];
        asm("lui %0, %%hi(D_8011541B)" : "=r"(row) : "r"(addr));
        col = *(u16*)addr;
        asm("lbu %0, %%lo(D_8011541B)(%1)" : "=r"(row) : "r"(row), "r"(col));
        col <<= 1;
        asm volatile("");
        col  += row * 20;
        col  += (s32)table;
        extra = Gp_StateC08.field_C;
        col   = ((GpDmgSlot*)col)->field_A;
        val   = col << 8;
        if (extra != 0) {
            scaleTbl = D_80113CFC;
            div      = extra / 16;
            col      = (div - 1) * 2;
            rem      = extra % 16;
            scale    = scaleTbl[col + rem];
            col      = val * scale;
            asm volatile("" : "+r"(col));
            mag = 0x51EB851F;
            asm volatile("multu %0, %1" : : "r"(col), "r"(mag));
            asm volatile("mfhi %0" : "=r"(col));
            val = (u32)col >> 5;
        }
    } else {
        register s32 col asm("v1");
        register s32 row asm("a0");

        hp    = (s16)Mc_SaveData.field_6C8;
        table = Gp_DmgRows;
        cols  = D_80113F54;
        addr  = (s32)&cols[hp / 10];
        asm("lui %0, %%hi(D_8011541B)" : "=r"(row) : "r"(addr));
        col = *(u16*)addr;
        asm("lbu %0, %%lo(D_8011541B)(%1)" : "=r"(row) : "r"(row), "r"(col));
        col <<= 1;
        asm volatile("");
        col  += row * 20;
        col  += (s32)table;
        scale = ((GpDmgSlot*)col)->field_0;
        val   = scale << 8;
    }

    {
        register s32 hi asm("v1");
        mag = 0x51EB851F;
        asm volatile("multu %0, %1" : : "r"(val), "r"(mag));
        asm volatile("mfhi %0" : "=r"(hi));
        val = (u32)hi >> 5;
        hi  = lo * val;
        ret = (u32)hi >> 8;
    }
    if (ret == 0) {
        if (lo != 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 func_800E25F8(GpEnemy* arg0, u32 arg1, s32 arg2)
{
    GpActorWork*   slot;
    GsCOORDINATE2* pcoord;
    void**         scratch;
    u8*            head;
    GpDistScratch* blk;
    s32            dist;
    u16            sel;
    s32            kind;
    s32            col;
    s32            val;
    s32            chance;
    u16            base;
    s32            extra;
    s32            rand;

    slot = Game_GetPtrSlot(3);
    if (slot == NULL) {
        return 0;
    }
    if ((arg1 & 0x8000) != 0) {
        return 0;
    }

    base = (arg0->field_50->field_B << 12) / 100;
    if (base == 0) {
        return 0;
    }

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (GpDistScratch*)(head - 0x20);
    *scratch = blk;
    Gp_UpdateCoord(arg0->field_18);

    ((VECTOR3*)(head - 0x20))->vx = arg0->field_1C.vx;
    blk->local.vy                 = arg0->field_1C.vy;
    blk->local.vz                 = arg0->field_1C.vz;

    gte_SetRotMatrix(&arg0->field_18->workm);
    gte_ldv0(&blk->local);
    gte_rtv0_real();
    gte_stlvnl(head - 0x10);

    blk->world.vx = arg0->field_18->workm.t[0] + blk->world.vx;
    blk->world.vy = arg0->field_18->workm.t[1] + blk->world.vy;
    blk->world.vz = arg0->field_18->workm.t[2] + blk->world.vz;

    pcoord                        = (GsCOORDINATE2*)slot->extra->field_8;
    ((VECTOR3*)(head - 0x20))->vx = blk->world.vx - pcoord->workm.t[0];
    blk->local.vy                 = blk->world.vy - pcoord->workm.t[1];
    blk->local.vz                 = blk->world.vz - pcoord->workm.t[2];

    dist = SquareRoot0(((VECTOR3*)(head - 0x20))->vx * ((VECTOR3*)(head - 0x20))->vx +
                       blk->local.vy * blk->local.vy + blk->local.vz * blk->local.vz);

    sel = dist / 1000;
    sel = sel < 0x10 ? D_80113864[sel] : 5;

    kind = (arg1 >> 8) & 0x3F;
    if (Gp_IdParamLo[arg1 & 0x7F].field_4 == 6) {
        val = (D_80113858[sel] << 12) / 100;
    } else {
        val = (D_80113568[kind][sel] << 12) / 100;
    }

    if ((arg1 & 0x4000) != 0) {
        col = 7;
    } else {
        col = 6;
    }

    chance = (((D_80113568[kind][col] << 12) / 100) * base >> 12) * val >> 12;
    if ((arg0->field_4C & 2) != 0) {
        chance <<= 1;
    }

    extra = (s8)Gp_StateC08.field_D;
    if (extra != 0) {
        chance = chance * D_80113D0C[(extra / 16 - 1) * 2 + (s8)(extra % 16)][1] / 100;
    }
    if (arg2 != 0) {
        chance *= arg2;
    }

    Gp_LcgState             = Gp_LcgState * 5 + 0x71357911;
    rand                    = (u32)Gp_LcgState >> 16 & 0xFFF;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
    return rand < chance;
}

void Gp_ApplyObjKind(GpObj5D* arg0, s32 arg1)
{
    u16 raw;
    s32 kind;
    s32 val;
    s32 limit;
    s32 rand;

    if ((arg1 & 0x8000) == 0) {
        raw = Gp_IdParamLo[arg1 & 0x7F].field_4;
        asm volatile("" : "+r"(raw));
        kind = raw;
    } else {
        raw = Gp_IdParamHi[arg1 & 0x7F].field[5];
        asm volatile("" : "+r"(raw));
        kind = raw;
    }

    switch (kind) {
        case 0:
            break;
        case 1:
            arg0->field_4C |= 1;
            break;
        case 2:
            arg0->field_58  = 0;
            arg0->field_5B  = 0;
            arg0->field_4C |= 2;
            if ((arg1 & 0x8000) == 0) {
                arg0->field_5D = 0;
                return;
            }
            if ((arg1 & 0x3F) == 0x31) {
                arg0->field_5D = 0;
                return;
            }
            arg0->field_5D = Gp_StateC08.field_0 % 10U;
            break;
        case 3:
            val         = arg0->field_50->field_D;
            limit       = (val << 12) / 100;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            rand        = (u32)Gp_LcgState >> 16 & 0xFFF;
            if (rand < limit) {
                arg0->field_5A  = 0;
                arg0->field_4C |= 4;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                arg0->field_59  = ((u32)Gp_LcgState >> 16 & 0xF) + 0x53;
                if ((arg1 & 0x8000) == 0) {
                    arg0->field_5C = 0;
                    return;
                }
                arg0->field_5C = Gp_StateC08.field_0 % 10U;
            }
            break;
    }
}

s32 Gp_PackObjPair(GpObj50* arg0, s32 arg1)
{
    GpU16Pair* pairs;
    s32        ret;

    if (arg0->field_50 == NULL) {
        return 0;
    }
    pairs = arg0->field_50->field_0;
    ret   = pairs[arg1].field_0 & 0xFFF;
    ret  |= (pairs[arg1].field_2 & 0xF) << 12;
    ret  |= 0x40000;
    return ret;
}

s32 Gp_PackPair(GpU16Pair* arg0, s32 arg1)
{
    s32 ret;

    if (arg0 == NULL) {
        return 0;
    }
    ret  = arg0[arg1].field_0 & 0xFFF;
    ret |= (arg0[arg1].field_2 & 0xF) << 12;
    ret |= 0x40000;
    return ret;
}

void func_800E2C78(GpObj40* arg0, s32 arg1, s32 arg2)
{
    s32 val;

    if ((u32)((arg1 & 0x7F) - 0x19) < 3U) {
        val = arg0->field_40;
        if ((u32)val < (u32)arg2) {
            Gp_StateF0.field_14 += val;
            return;
        }
        Gp_StateF0.field_14 += arg2;
    }
}

s32 Gp_LookupIdField(s32 arg0, s32 arg1)
{
    s32 ret;

    ret = 0;
    switch (arg1) {
        case 0:
            ret = Gp_IdField0[(u16)arg0].field_0;
            break;
        case 1:
            ret = Gp_IdField1[(u16)arg0].field_0;
            break;
    }
    return ret;
}

s32 Gp_GetIdParam0(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].field_4;
    } else {
        ret = Gp_IdParamHi[arg0 & 0x7F].field[5];
    }
    return ret;
}

s32 Gp_GetIdParam1(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].field_6;
    } else {
        ret = Gp_IdParamHi[arg0 & 0x7F].field[6];
    }
    return ret;
}

void Gp_SetObjFlag4(GpObj5C* arg0, s32 arg1)
{
    s32 val;
    s32 limit;
    s32 rand;

    val         = arg0->field_50->field_D;
    limit       = (val << 12) / 100;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    rand        = (u32)Gp_LcgState >> 16 & 0xFFF;
    if (rand < limit) {
        arg0->field_5A  = 0;
        arg0->field_4C |= 4;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        arg0->field_59  = ((u32)Gp_LcgState >> 16 & 0xF) + 0x53;
        if ((arg1 & 0x8000) == 0) {
            arg0->field_5C = 0;
            return;
        }
        arg0->field_5C = Gp_StateC08.field_0 % 10U;
    }
}

s32 Gp_TickObjFlag4(GpObj5C* arg0)
{
    s32 ret;
    s32 val;
    s32 scale;

    ret = 0;
    arg0->field_59--;
    if (arg0->field_59 == 0) {
        arg0->field_5A++;
        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
        arg0->field_59 = ((u32)Gp_LcgState >> 16 & 0xF) + 0x53;
        val            = arg0->field_50->field_4;
        scale          = D_80113D38[arg0->field_5C];
        ret            = (val * scale) / 100;
        if (ret == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_ObjFlag4Expired(GpObj5C* arg0)
{
    s32          val;
    s32          ret;
    register s32 scale asm("v1");

    ret = 0;
    val = arg0->field_50->field_E;
    if (!(arg0->field_4C & 4)) {
        return 1;
    }
    if (val == 0) {
        return 0;
    }
    scale = D_80113D28[arg0->field_5C];
    if (arg0->field_5A >= (val * scale) / 100) {
        ret = 1;
    }
    return ret;
}

void Gp_SetObjFlag1(GpObj4C* arg0)
{
    arg0->field_4C |= 1;
}

void Gp_SetObjFlag2(GpObj5D* arg0, s32 arg1)
{
    arg0->field_58  = 0;
    arg0->field_5B  = 0;
    arg0->field_4C |= 2;
    if ((arg1 & 0x8000) == 0) {
        arg0->field_5D = 0;
        return;
    }
    if ((arg1 & 0x3F) == 0x31) {
        arg0->field_5D = 0;
        return;
    }
    arg0->field_5D = Gp_StateC08.field_0 % 10U;
}

s32 Gp_TickObjFlag2(GpObj5D* arg0)
{
    s32 ret;
    s32 limit;
    s32 val;
    s32 scale;

    ret = 0;
    val = arg0->field_50->field_C;
    if (val == 0) {
        return ret;
    }
    scale = D_80113D30[arg0->field_5D];
    limit = (val * scale) / 100;
    if (arg0->field_58 < limit) {
        arg0->field_5B++;
        if (arg0->field_5B >= 0x1F) {
            arg0->field_58++;
            if (arg0->field_58 >= limit) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                arg0->field_5B = (u32)Gp_LcgState >> 16 & 0x3F;
            } else {
                arg0->field_5B = 0;
            }
        }
    } else {
        arg0->field_5B--;
        if (arg0->field_5B == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_GetIdParam2(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].field_8;
    } else {
        ret = Gp_IdParamHi[arg0 & 0x7F].field[7];
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E31E8);

void func_800E337C(Task* arg0)
{
    s32 flags;
    s32 bit0;
    s32 mode;
    s32 flag;

    flag  = 1;
    flags = arg0->spawnArg1;
    switch (arg0->state) {
        case 0:
            bit0 = flags & 1;
            if (bit0 != 0) {
                Gp_MsgPlayerWeapon(0);
                D_801153F4 = flag;
            }
            if (flags & 2) {
                Gp_MsgPlayer3F3(0);
            }
            if (flags & 4) {
                mode = 2;
            } else if (bit0 == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            Gp_RunCapCmd((s32)arg0->spawnArg2, mode);
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (flags & 1) {
                Gp_MsgPlayerWeapon(1);
                D_801153F4 = 0;
            }
            if (flags & 2) {
                Gp_MsgPlayer3F3(1);
            }
            if (D_80115598 != 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F2, (s32)arg0->spawnArg2 + 0x64, 0);
            }
            Task_Kill(arg0);
            break;
    }
}
