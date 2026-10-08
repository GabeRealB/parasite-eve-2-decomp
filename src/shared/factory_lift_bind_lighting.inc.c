/* Part of the factory lift library; see factory_lift.h. */

/// Binds the lift model to its work-owned matrices and samples three room lights.
///
/// Requires live `FactoryLiftWork`, model/root and initialized coordinate,
/// room-light and scratch/GTE state. The model borrows both matrices until task
/// teardown. Samples the root's complete composition-frame XYZ after marking
/// and composing it, without converting that sample to another frame.
static void _factoryLiftBindLighting(Task* task)
{
    enum { FACTORY_LIFT_LIGHT_COUNT = 3 };
    GfxCoord*        rootCoord;
    FactoryLiftWork* work;
    TmdObject*       model;

    work                    = task->work;
    model                   = task->extra.tmd;
    rootCoord               = model->coords;
    model->lightMtx         = &work->light;
    model->colorMtx         = &work->color;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    worldCoordSetModelLighting(model, rootCoord->workm.t, 0, FACTORY_LIFT_LIGHT_COUNT);
}
