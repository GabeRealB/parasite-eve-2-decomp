#include "gameplay/actor_render.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actor_render.h"
#include "gameplay/hud_sprites.h"
#include "model_objects.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/tmd.h"

extern GpCoord* _gGpCurCoord;

/// Brings one coordinate up to date for the current pass, as
/// `_gpUpdateCoordTree` describes: the body that function and the draw passes
/// share, inlined into each. The high-precision translation is composed with
/// `gte_RotTransLV`.
static __inline__ void _gpRefreshCoord(GpCoord* coord, s32 stamp, s32 parity, GpCoord* root);

/// Brings every coordinate the draw passes use up to date for this frame: the
/// 2D displays' single coordinates, then each model's part coordinates, and
/// advances the frame stamp the next pass will compare against.
static __inline__ void _gpRefreshAllCoords(void);

GpCoord* _gGpCurCoord = NULL;

// "Item obtained!"
// "Bonus item!!"

/* r1 = long vector in, r2 = long vector out: r2 = RT * r1 + TR at full
 * 32-bit precision, the input split into three 10/11-bit slices. */
#define gte_RotTransLV(r1, r2) __asm__ volatile( \
    "lw	$14, 0( %0 );"                           \
    "lw	$15, 4( %0 );"                           \
    "addiu	$16, $0, -0x400;"                     \
    "sra	$12, $14, 21;"                          \
    "and	$12, $16, $12;"                         \
    "andi	$13, $14, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "andi	$12, $12, 0xffff;"                     \
    "sra	$13, $15, 21;"                          \
    "and	$13, $16, $13;"                         \
    "andi	$16, $15, 0x3ff;"                      \
    "or	$13, $16, $13;"                          \
    "sll	$13, $13, 16;"                          \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $0;"                              \
    "sra	$14, $14, 10;"                          \
    "sra	$15, $15, 10;"                          \
    "addiu	$16, $0, -0x400;"                     \
    "sra	$12, $14, 21;"                          \
    "and	$12, $16, $12;"                         \
    "andi	$13, $14, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "sra	$13, $15, 21;"                          \
    "and	$13, $16, $13;"                         \
    "andi	$16, $15, 0x3ff;"                      \
    "or	$13, $16, $13;"                          \
    "srl	$16, $15, 31;"                          \
    "addu	$13, $13, $16;"                        \
    "sll	$13, $13, 16;"                          \
    "srl	$16, $14, 31;"                          \
    "addu	$12, $12, $16;"                        \
    "andi	$12, $12, 0xffff;"                     \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $2;"                              \
    "sra	$14, $14, 10;"                          \
    "sra	$15, $15, 10;"                          \
    "andi	$12, $14, 0xffff;"                     \
    "srl	$16, $14, 31;"                          \
    "addu	$12, $16, $12;"                        \
    "andi	$12, $12, 0xffff;"                     \
    "andi	$13, $15, 0xffff;"                     \
    "srl	$16, $15, 31;"                          \
    "addu	$13, $16, $13;"                        \
    "sll	$13, $13, 16;"                          \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $4;"                              \
    "lw	$16, 8( %0 );"                           \
    "addiu	$14, $0, -0x400;"                     \
    "srl	$15, $16, 31;"                          \
    "sra	$12, $16, 21;"                          \
    "and	$12, $14, $12;"                         \
    "andi	$13, $16, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $1;"                              \
    "sra	$16, $16, 10;"                          \
    "sra	$12, $16, 21;"                          \
    "and	$12, $14, $12;"                         \
    "andi	$13, $16, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "addu	$12, $12, $15;"                        \
    "mtc2	$12, $3;"                              \
    "sra	$16, $16, 10;"                          \
    "addu	$12, $16, $15;"                        \
    "mtc2	$12, $5;"                              \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A480012;"                          \
    "mfc2	$14, $25;"                             \
    "mfc2	$15, $26;"                             \
    "mfc2	$16, $27;"                             \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A40E012;"                          \
    "mfc2	$12, $25;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$14, $12, $14;"                        \
    "mfc2	$12, $26;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$15, $12, $15;"                        \
    "mfc2	$12, $27;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$16, $12, $16;"                        \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A416012;"                          \
    "mfc2	$12, $25;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$14, $12, $14;"                        \
    "mfc2	$12, $26;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$15, $12, $15;"                        \
    "mfc2	$12, $27;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$16, $12, $16;"                        \
    "sw	$14, 0( %1 );"                           \
    "sw	$15, 4( %1 );"                           \
    "sw	$16, 8( %1 )"                            \
    :                                            \
    : "r"(r1), "r"(r2)                           \
    : "$12", "$13", "$14", "$15", "$16", "memory")

/// Brings one coordinate up to date for the current pass, as
/// `_gpUpdateCoordTree` describes: the body that function and the draw passes
/// share, inlined into each. The high-precision translation is composed with
/// `gte_RotTransLV`.
static __inline__ void _gpRefreshCoord(GpCoord* coord, s32 stamp, s32 parity, GpCoord* root)
{
    GpCoord* parent;

    parent     = coord->sub;
    coord->flg = (coord->flg << 1) >> 1;
    if (parent == root) {
        if (coord->flg == 0) {
            coord->workm = coord->coord;
            coord->flg   = stamp;
        }
    } else {
        if ((parent->flg == 0) || ((parent->flg >> 31) != parity)) {
            _gpUpdateCoordTree(parent, stamp, parity, root);
        }
        if (coord->flg < (parent->flg & 0x7FFFFFFF)) {
            gte_CompMatrix(&parent->workm, &coord->coord, &coord->workm);
            coord->flg = stamp;
            gte_SetRotMatrix(&parent->workm);
            gte_ldclmv(&coord->coord.m[0][0]);
            gte_rtir();
            gte_stclmv(&coord->workm.m[0][0]);
            gte_ldclmv(&coord->coord.m[0][1]);
            gte_rtir();
            coord->flg = stamp;
            gte_stclmv(&coord->workm.m[0][1]);
            gte_ldclmv(&coord->coord.m[0][2]);
            gte_rtir();
            gte_SetTransVector(parent->workm.t);
            gte_stclmv(&coord->workm.m[0][2]);
            gte_RotTransLV(coord->coord.t, coord->workm.t);
        }
    }
    if (parity != 0) {
        coord->flg |= 0x80000000;
    }
}

/// Brings every coordinate the draw passes use up to date for this frame: the
/// 2D displays' single coordinates, then each model's part coordinates, and
/// advances the frame stamp the next pass will compare against.
static __inline__ void _gpRefreshAllCoords(void)
{
    TmdObject* node;
    GpCoord*   coord;
    s32        stamp;
    s32        parity;
    u32        i;

    stamp  = D_80071210 & 0x7FFFFFFF;
    parity = D_80071210 & 1;
    for (node = PARENT_OF(gTmdDisp2dList.next, TmdObject, link); node != NULL;
         node = PARENT_OF(node->link.next, TmdObject, link)) {
        _gpRefreshCoord(node->coords, stamp, parity, NULL);
    }
    for (node = PARENT_OF(gTmdList.next, TmdObject, link); node != NULL;
         node = PARENT_OF(node->link.next, TmdObject, link)) {
        coord = node->coords;
        for (i = 0; i < node->partCount; i++) {
            _gpRefreshCoord(coord, stamp, parity, NULL);
            coord++;
        }
    }
    D_80071210 += 1;
}

/// Refreshes every coordinate for this frame, then draws the models the
/// flagged pass draws.
void Gp_DrawActorTmdFlagged(GpuOtBuf* arg0)
{
    _gpRefreshAllCoords();
    Tmd_DrawFlaggedNodes(PARENT_OF(gTmdList.next, TmdObject, link));
}

/// Refreshes every coordinate for this frame, then draws the active models.
void Gp_DrawActorTmdActive(GpuOtBuf* arg0)
{
    _gpRefreshAllCoords();
    Tmd_DrawActiveNodes(PARENT_OF(gTmdList.next, TmdObject, link));
}

void Gp_UpdateCoord(GpCoord* arg0)
{
    _gGpCurCoord = arg0;
    _gpUpdateCoordTree(arg0, D_80071210 & 0x7FFFFFFF, D_80071210 & 1, 0);
}

void Gp_UpdateCoordEx(GpCoord* arg0, GpCoord* arg1)
{
    if (arg0->sub == NULL) {
        _gGpCurCoord = arg0;
        _gpUpdateCoordTree(arg0, D_80071210 & 0x7FFFFFFF, D_80071210 & 1, 0);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &arg0->workm, &arg0->coord);
    } else {
        _gpUpdateCoordTree(arg0, D_80071210 & 0x7FFFFFFF, D_80071210 & 1, arg1);
    }
}
