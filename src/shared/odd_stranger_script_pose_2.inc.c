/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 1: restarts clip 2 at rate 0x10 on entry and keeps driving it.
void oddStrangerScriptPose2(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->animRequest      = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate         = 0x10;
        work->animId           = 2;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
#if ODD_STRANGER_BODY2_GRID
        work->gridBody.flags = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
#else
        work->gridBody.flags = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
#endif
        oddStrangerDrive(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        oddStrangerDrive(arg0);
    }
}
