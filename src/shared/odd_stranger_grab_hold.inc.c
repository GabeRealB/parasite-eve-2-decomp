/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 13: clip 6 restart; on entry sends the player animation 2 and the 0x3F9 object pair and starts a 5/0xFF/8 pad lerp. At the clip boundary spawns the 0x1001 effect at coordinate 1 and moves to state 0xE; each frame copies the clip frame to field_894 and pitches coordinates 2 and 3 by -0x80.
void oddStrangerGrabHold(Task* arg0)
{
    OddStrangerWork*      work;
    AnimationPlayRequest* msg;
    Enemy*                enemy;
    Task*                 player;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_8A2  = 0x10;
        work->field_89E  = 6;
        work->field_898  = 2;
        msg              = &gOddStrangerPlayerAnim;
        msg->animationId = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, msg, 0);
        player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair(enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY) {
#if ODD_STRANGER_VARIANT == 2
        work->field_0 = 0xE;
#endif
        work->field_8B8.coord      = arg0->extra.tmd->coords + 1;
        work->field_8B8.spawnArgLo = ODD_STRANGER_PART1_FX_SCALE;
        work->field_8B8.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, arg0->extra.tmd->coords + 5, NULL, &work->field_8B8);
#if ODD_STRANGER_VARIANT == 1
        work->field_0 = 0xE;
#endif
    }
    work->field_894 = work->field_5A & 0x3FF;
    oddStrangerDrive(arg0);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[2].coord, -0x80, 0);
    arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[3]);
    Gfx_RotMatrixX(&arg0->extra.tmd->coords[3].coord, -0x80, 0);
    arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg0->extra.tmd->coords[2]);
}
