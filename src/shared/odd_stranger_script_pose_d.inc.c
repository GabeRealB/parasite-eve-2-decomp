/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 5: restarts clip 0xD at rate 0x12 on entry and moves to state 7 at the clip boundary.
void oddStrangerScriptPoseD(Task* arg0)
{
    TmdObject*       obj;
    OddStrangerWork* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x12;
        work->field_89E       = 0xD;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
#if ODD_STRANGER_BODY2_GRID
        work->field_A10.flags = (u16)(work->field_A10.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
#else
        work->field_A10.flags = (u16)(work->field_A10.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
#endif
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    if (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 7;
    }
}
