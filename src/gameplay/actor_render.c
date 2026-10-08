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

/// Selects the pass-counter parity stored in bit 31 of each visited coordinate's stamp.
enum { ACTOR_RENDER_COORD_PASS_PARITY_MASK = 1 };

/// Last node explicitly submitted for composition through its full parent chain.
///
/// Initially NULL; updates with an excluded ancestor leave this snapshot alone.
/// No game code reads it. The pointer is borrowed and may outlive its node.
static GfxCoord* _gActorRenderLastFullChainCoord = NULL;

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

/// Composes the coordinate-body and model lists, then advances the composition pass.
///
/// Visits every linked body's coordinate and every model's `partCount` nodes,
/// regardless of draw flags, through their complete parent chains. The lists,
/// their borrowed parent nodes and each model's owned coordinate array must
/// stay live and unchanged during the walk. Empty lists still advance the
/// counter. Explicit node compositions before the next list pass use that
/// next pass's stamp and parity; this is a pass boundary, not a frame clock.
static __inline__ void _actorRenderComposeListedCoords(void)
{
    TmdListNode* link;
    GfxCoord*    partCoord;
    s32          rebuildStamp;
    s32          visitParity;
    u32          partIndex;

    rebuildStamp = D_80071210 & GRAPHICS_COORD_STAMP_MASK;
    visitParity  = D_80071210 & ACTOR_RENDER_COORD_PASS_PARITY_MASK;
    // Both lists share one pass, so common ancestors can reuse their caches.
    for (link = gModelObjectCoordBodyList.next; link != NULL; link = link->next) {
        _actorRenderRefreshCoord(PARENT_OF(link, ModelObjectCoordBody, link)->coord, rebuildStamp, visitParity, NULL);
    }
    for (link = gTmdList.next; link != NULL; link = link->next) {
        partCoord = PARENT_OF(link, TmdObject, link)->coords;
        for (partIndex = 0; partIndex < PARENT_OF(link, TmdObject, link)->partCount; partIndex++) {
            _actorRenderRefreshCoord(partCoord, rebuildStamp, visitParity, NULL);
            partCoord++;
        }
    }
    // Advance only after both lists have used the same rebuild stamp and parity.
    D_80071210 += 1;
}

void actorRenderComposeAndDrawFlaggedModels(GsOT* unusedOt)
{
    // Compose both lists before drawing, including models omitted from this pass.
    _actorRenderComposeListedCoords();
    tmdDrawFlaggedModels(PARENT_OF(gTmdList.next, TmdObject, link));
}

void actorRenderComposeAndDrawActiveModels(GsOT* unusedOt)
{
    // Compose both lists before drawing, including hidden and unbuffered models.
    _actorRenderComposeListedCoords();
    tmdDrawActiveModels(PARENT_OF(gTmdList.next, TmdObject, link));
}

void actorRenderComposeCoord(GfxCoord* coord)
{
    _gActorRenderLastFullChainCoord = coord;
    actorRenderComposeCoordChain(coord, D_80071210 & GRAPHICS_COORD_STAMP_MASK,
                                 D_80071210 & ACTOR_RENDER_COORD_PASS_PARITY_MASK, NULL);
}

void actorRenderComposeCoordRelative(GfxCoord* coord, GfxCoord* excludedAncestor)
{
    if (coord->parent == NULL) {
        // Keep the composed cache while rebasing only the detached node's local matrix.
        _gActorRenderLastFullChainCoord = coord;
        actorRenderComposeCoordChain(coord, D_80071210 & GRAPHICS_COORD_STAMP_MASK,
                                     D_80071210 & ACTOR_RENDER_COORD_PASS_PARITY_MASK, NULL);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &coord->coord);
    } else {
        actorRenderComposeCoordChain(coord, D_80071210 & GRAPHICS_COORD_STAMP_MASK,
                                     D_80071210 & ACTOR_RENDER_COORD_PASS_PARITY_MASK, excludedAncestor);
    }
}
