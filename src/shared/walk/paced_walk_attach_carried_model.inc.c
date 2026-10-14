/* Part of the paced walk library; see paced_walk.h. */

/// Links a carried model's root to a walker part and shares the walker's lighting.
///
/// Requires a live writable `carriedModel` with coordinate 0 and a spawned
/// `PacedWalkWork`. The borrowed `attachmentCoord` and work matrices must stay
/// live through the child's updates and drawing; coordinate ancestry must be
/// acyclic. Keeps the root's local transform and invalidates its composition.
/// Clears all model flags, permitting active drawing and automatic buffer
/// allocation without performing either here. Task ownership is caller-managed.
static inline void _pacedWalkAttachCarriedModel(TmdObject* carriedModel, GfxCoord* attachmentCoord,
                                                PacedWalkWork* parentWork)
{
    enum { PACED_WALK_CARRIED_MODEL_NO_FLAGS = 0 };
    GfxCoord* rootCoord = carriedModel->coords;

    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    carriedModel->lightMtx  = &parentWork->light;
    carriedModel->flags     = PACED_WALK_CARRIED_MODEL_NO_FLAGS;
    carriedModel->colorMtx  = &parentWork->color;
    rootCoord->parent       = attachmentCoord;
}
