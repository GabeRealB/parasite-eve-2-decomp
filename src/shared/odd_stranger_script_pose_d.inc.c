/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 5: restarts clip 0xD at rate 0x12 on entry and moves to state 7 at the clip boundary.
void oddStrangerScriptPoseD(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest      = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate         = 0x12;
        work->animId           = 0xD;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
#if ODD_STRANGER_BODY2_GRID
        work->gridBody.flags = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
#else
        work->gridBody.flags = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
#endif
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
}
