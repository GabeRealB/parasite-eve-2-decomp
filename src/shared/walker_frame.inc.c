/* Actor-render frame state shared by several walker implementations. */

#ifndef ACTOR_RENDER_WALKER_FRAME
#error "Bind ACTOR_RENDER_WALKER_FRAME before including this fragment"
#endif

/* ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW is an object-like binding to a
 * declared static void(Task*) drawer. The call evaluates task once. */
#ifndef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW
#error "Bind ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW before including this fragment"
#endif

// Define the typed helper once when a carrier includes two frame instances.
#ifndef SRC_SHARED_ACTOR_RENDER_LIGHT_WALKER_ROOT
#define SRC_SHARED_ACTOR_RENDER_LIGHT_WALKER_ROOT

/// Samples a walker's three lights from its composed root before movement.
///
/// Borrows a live model with a writable root and lighting matrices. The XYZ
/// sample retains the root's composition frame and subtracts 800 units from Y.
/// Requires the composition and lighting helpers' initialized scratch/GTE state;
/// the sample is consumed synchronously and no pointer to it is retained.
static inline void _actorRenderLightWalkerRoot(const TmdObject* model)
{
    enum {
        ACTOR_RENDER_WALKER_LIGHT_SAMPLE_Y_OFFSET = 800,
        ACTOR_RENDER_WALKER_FIRST_LIGHT_INDEX     = 0,
        ACTOR_RENDER_WALKER_LIGHT_COUNT           = 3
    };
    GfxCoord* rootCoord;
    VECTOR3   lightingSample;

    rootCoord = model->coords;
    // Preserve the pre-movement sample and the root's composition frame.
    actorRenderComposeCoord(rootCoord);
    lightingSample.vx = rootCoord->workm.t[0];
    lightingSample.vy = rootCoord->workm.t[1] - ACTOR_RENDER_WALKER_LIGHT_SAMPLE_Y_OFFSET;
    lightingSample.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, ACTOR_RENDER_WALKER_FIRST_LIGHT_INDEX, ACTOR_RENDER_WALKER_LIGHT_COUNT);
}

#endif

/// Refreshes a walker's lighting, advances its motion and animation, and draws its shadow.
///
/// `task` must have a live TMD root, writable lighting matrices and the initialized
/// work/animation storage required by its bound update. Dispatchers for updates
/// using a published work pointer must publish this task's block before entry.
/// The enemy argument is unused but preserves the `EnemyTaskFunc` signature.
/// Lighting samples the root's full-chain cached translation with 800 signed
/// coordinate units subtracted from Y; no conversion to another frame is made.
/// Lighting runs even for hidden models, before movement. The shadow reads the
/// root cache left by the update; this state does not compose the root again.
/// The task, model, coordinates and animation data must remain live throughout;
/// the lighting, animation and shadow helpers require initialized scratch/GTE state.
///
/// Bind `ACTOR_RENDER_WALKER_FRAME` to a function identifier declared
/// `static void (Enemy* unusedEnemy, Task* task)` in the carrier's prologue.
/// This object-like alias has no arguments, captures no locals and uses no
/// stringification or token pasting. Bind `walkerUpdate` to a declared
/// `void (Task*)` update and `ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW` to its
/// shadow drawer for the same task. Undefine all three after each inclusion;
/// multiple walkers require distinct frame identifiers and matching helpers.
static void ACTOR_RENDER_WALKER_FRAME(Enemy* unusedEnemy, Task* task)
{
    _actorRenderLightWalkerRoot(task->extra.tmd);
    walkerUpdate(task);
    ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW(task);
}
