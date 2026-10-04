/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 0xD of the `behavior` table. State 0 arms the reaction: `hitFromFront`
/// picks animation 0x16 with a 0x42-frame budget or 0x1A with 0x31 frames,
/// `hurtBody`'s offset and radius are parked and its 0x4000 flag raised,
/// `groundBody`'s dropped, and the enemy's `reactionFlags` cleared.
/// State 1 plays the voice cues of the animation `downedPose` selects at its
/// frame marks and, when the `timer` budget runs out, hands the task over
/// to state 2.
void golemPawnRookCollapseState(Task* arg0)
{
    GolemPawnRookWork* work;
    GfxCoord*          self;
    s32                snd;
    s16                state;

    work  = arg0->work;
    self  = arg0->extra.tmd->coords;
    state = work->step;

    switch (state) {
        case 0:
            if (work->hitFromFront == 0) {
                work->anim            = 0x16;
                work->step            = 1;
                work->downedPose      = 1;
                work->timer           = 0x42;
                work->hurtBody.pos.vz = -0xA7;
            } else {
                work->anim            = 0x1A;
                work->step            = 1;
                work->downedPose      = 2;
                work->timer           = 0x31;
                work->hurtBody.pos.vz = 0x109;
            }
            work->hurtBody.radius                            = 0x15E;
            work->forwardSpeed                               = 0;
            work->turnRate                                   = 0;
            work->knockdownStage                             = 1;
            work->hurtBody.flags                            |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->groundBody.flags                          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            ((Enemy*)arg0->spawnArg2.pointer)->reactionFlags = 0;
            work->fallingDown                                = 1;
            break;
        case 1:
            if (work->knockdownStage == 1) {
                work->knockdownStage = 2;
            }
            if (work->downedPose == 1) {
                if (work->animFrame == 0x14) {
                    s32 pan;

                    snd = gGolemPawnRookVoiceCues[work->soundSet + 0xC] |
                          ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    pan = (s8)worldCoordGetOriginAudioPan(self);

                    sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(self));
                }
                if (work->animFrame == 0x2C) {
                    s32 pan;

                    snd = gGolemPawnRookVoiceCues[work->soundSet + 8] |
                          ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    pan = (s8)worldCoordGetOriginAudioPan(self);

                    sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(self));
                }
            } else if (work->animFrame == 0x19) {
                s32 pan;

                snd = gGolemPawnRookVoiceCues[work->soundSet + 8] |
                      ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan = (s8)worldCoordGetOriginAudioPan(self);

                sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(self));
            }
            work->timer--;
            if (work->timer <= 0) {
                arg0->state       = 2;
                work->step        = 0;
                work->fallingDown = 0;
            }
            break;
    }
}
