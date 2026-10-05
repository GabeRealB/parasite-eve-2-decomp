/* Part of the factory lift library; see factory_lift.h. */

/// Binds the model to the light and colour matrices in the task's work block
/// and rebuilds its lighting.
void factoryLiftBindLighting(Task* task)
{
    GfxCoord*        coord;
    FactoryLiftWork* work;
    TmdObject*       extra;

    work                = task->work;
    extra               = task->extra.tmd;
    coord               = extra->coords;
    extra->lightMtx     = &work->light;
    extra->colorMtx     = &work->color;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    worldCoordSetModelLighting(extra, coord->workm.t, 0, 3);
}
