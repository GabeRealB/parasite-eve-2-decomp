/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Death while standing: falls by `hitFromFront` as it fades into full view,
/// filing the matching `downedPose` and moving the grid test from `groundBody`
/// to the shifted `hurtBody`, and queues the fall sound on the impact frame.
/// When `timer` ends it moves the task to its dead state (2).
void golemKnightBishopCollapseDeathSeq(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    snd;
    s32                    pan;
    s32                    frames;
    s16                    timer;

    work  = arg0->work;
    state = work->step;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->hitFromFront == 0) {
                work->anim            = 0xD;
                work->step            = 1;
                work->downedPose      = 1;
                work->timer           = 0x42;
                work->hurtBody.pos.vz = -0xA7;
            } else {
                work->anim            = 0x11;
                work->step            = 1;
                work->downedPose      = 2;
                work->timer           = 0x31;
                work->hurtBody.pos.vz = 0x109;
            }
            work->hurtBody.radius        = 0x15E;
            work->knockdownStage         = 1;
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->translucencyFadeFrames = 0x14;
            work->colorBlendFadeFrames   = 0xA;
            work->reactionLock           = 2;
            work->forwardSpeed           = 0;
            work->hurtBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->groundBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case 1:
            if (work->knockdownStage == state) {
                work->knockdownStage = 2;
            }
            frames = 0x19;
            if (work->downedPose == state) {
                frames = 0x2C;
            }
            if (work->animFrame == frames) {
                snd = gGolemKnightBishopAnimCues[work->soundSet + 8] | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0) {
                arg0->state        = 2;
                work->step         = 0;
                work->reactionLock = 0;
            }
            break;
    }
}
