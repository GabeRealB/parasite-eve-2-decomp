#include "gameplay/actor_render.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actor_render.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/model_objects.h"
#include "model_objects.h"

#include "main/gfx.h"
#include "main/tmd.h"

static __inline__ void _actorRenderRefreshCoord(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root);

/// Last node explicitly submitted for composition through its full parent chain.
///
/// Initially NULL; updates with an excluded ancestor leave this snapshot alone.
/// No game code reads it. The pointer is borrowed and may outlive its node.
static GfxCoord* _gActorRenderLastFullChainCoord = NULL;

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

/// Visits a coordinate after its ancestors and refreshes its composed matrix when stale.
///
/// The supplied root is excluded. `gte_RotTransLV` preserves the full signed
/// translation range while the rotation is composed with the GTE.
static __inline__ void _actorRenderRefreshCoord(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root)
{
    GfxCoord* parent;

    // Apply GRAPHICS_COORD_STAMP_MASK with shifts to avoid a hoisted mask register.
    parent              = coord->parent;
    coord->composeStamp = (coord->composeStamp << 1) >> 1;
    if (parent == root) {
        if (coord->composeStamp == GRAPHICS_COORD_DIRTY) {
            coord->workm        = coord->coord;
            coord->composeStamp = stamp;
        }
    } else {
        if ((parent->composeStamp == GRAPHICS_COORD_DIRTY) || ((parent->composeStamp >> 31) != parity)) {
            actorRenderComposeCoordChain(parent, stamp, parity, root);
        }
        if (coord->composeStamp < (parent->composeStamp & GRAPHICS_COORD_STAMP_MASK)) {
            gte_CompMatrix(&parent->workm, &coord->coord, &coord->workm);
            coord->composeStamp = stamp;
            gte_SetRotMatrix(&parent->workm);
            gte_ldclmv(&coord->coord.m[0][0]);
            gte_rtir();
            gte_stclmv(&coord->workm.m[0][0]);
            gte_ldclmv(&coord->coord.m[0][1]);
            gte_rtir();
            coord->composeStamp = stamp;
            gte_stclmv(&coord->workm.m[0][1]);
            gte_ldclmv(&coord->coord.m[0][2]);
            gte_rtir();
            gte_SetTransVector(parent->workm.t);
            gte_stclmv(&coord->workm.m[0][2]);
            gte_RotTransLV(coord->coord.t, coord->workm.t);
        }
    }
    if (parity != 0) {
        coord->composeStamp |= GRAPHICS_COORD_VISIT_PARITY_BIT;
    }
}

/// Brings every coordinate the draw passes use up to date for this frame: the
/// coordinate bodies' single transforms, then each model's part coordinates, and
/// advances the frame stamp the next pass will compare against.
static __inline__ void _gpRefreshAllCoords(void)
{
    // The cursors have disjoint lifetimes and reuse one saved register.
    register ModelObjectCoordBody* display asm("s3");
    register TmdObject*            model asm("s3");
    GfxCoord*                      coord;
    s32                            stamp;
    s32                            parity;
    u32                            partIndex;

    stamp  = D_80071210 & GRAPHICS_COORD_STAMP_MASK;
    parity = D_80071210 & 1;
    for (display = PARENT_OF(gModelObjectCoordBodyList.next, ModelObjectCoordBody, link); display != NULL;
         display = PARENT_OF(display->link.next, ModelObjectCoordBody, link)) {
        _actorRenderRefreshCoord(display->coord, stamp, parity, NULL);
    }
    for (model = PARENT_OF(gTmdList.next, TmdObject, link); model != NULL;
         model = PARENT_OF(model->link.next, TmdObject, link)) {
        coord = model->coords;
        for (partIndex = 0; partIndex < model->partCount; partIndex++) {
            _actorRenderRefreshCoord(coord, stamp, parity, NULL);
            coord++;
        }
    }
    D_80071210 += 1;
}

/// Refreshes every coordinate for this frame, then draws the models the
/// flagged pass draws.
void Gp_DrawActorTmdFlagged(GsOT* arg0)
{
    _gpRefreshAllCoords();
    Tmd_DrawFlaggedNodes(PARENT_OF(gTmdList.next, TmdObject, link));
}

/// Refreshes every coordinate for this frame, then draws the active models.
void Gp_DrawActorTmdActive(GsOT* arg0)
{
    _gpRefreshAllCoords();
    Tmd_DrawActiveNodes(PARENT_OF(gTmdList.next, TmdObject, link));
}

void Gp_UpdateCoord(GfxCoord* coord)
{
    _gActorRenderLastFullChainCoord = coord;
    actorRenderComposeCoordChain(coord, D_80071210 & GRAPHICS_COORD_STAMP_MASK, D_80071210 & 1, 0);
}

void Gp_UpdateCoordEx(GfxCoord* coord, GfxCoord* root)
{
    if (coord->parent == NULL) {
        _gActorRenderLastFullChainCoord = coord;
        actorRenderComposeCoordChain(coord, D_80071210 & GRAPHICS_COORD_STAMP_MASK, D_80071210 & 1, 0);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &coord->coord);
    } else {
        actorRenderComposeCoordChain(coord, D_80071210 & GRAPHICS_COORD_STAMP_MASK, D_80071210 & 1, root);
    }
}
