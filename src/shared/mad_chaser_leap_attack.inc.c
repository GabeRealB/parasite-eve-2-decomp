/* Part of the Mad Chaser library; see mad_chaser.h. */

/// The leap: before frame 40 a pending hit request can cut it short. Frames
/// 43-46 pin part 6 at its view position, frame 45 locks the heading toward a
/// player within +/-0x300, frames 45-53 lunge 250 units a frame with the bite
/// body armed, and from 47 it falls under gravity; a player under 0x171 away
/// turns it into the rebound, and reaching the ground requests landing
/// animation 0x12.
void madChaserLeapAttack(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    GfxCoord*      root = arg0->extra.tmd->coords;
    MATRIX         local;
    s16            angle;
    s32            soundId;
    s32            pan;
    s16            facing;
    s16            speed;

    if ((s16)++work->stateFrames < 40) {
        if ((s16)madChaserTakeHitRequest(arg0)) {
            return;
        }
    } else {
        work->busy = 1;
    }
    if ((s16)work->stateFrames == 43) {
        GfxCoord* coords = arg0->extra.tmd->coords;
        SVECTOR*  v;

        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&coords[6]);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords[6].workm, &local);
        v                      = &work->leapAnchorPos;
        v->vx                  = local.t[0];
        v->vy                  = local.t[1];
        v->vz                  = local.t[2];
        coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->stateFrames >= 43 && work->stateFrames <= 46) {
        work->anchored = 1;
    } else {
        work->anchored = 0;
    }
    if ((s16)work->stateFrames == 46) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0005;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->stateFrames == 45) {
        facing = (work->playerBearing + 0x800) & 0xFFF;
        if (facing < 0x300) {
            work->leapHeading = (facing + work->rotation.vy) & 0xFFF;
        } else if (facing >= 0xD00) {
            work->leapHeading = (facing + work->rotation.vy) & 0xFFF;
        } else {
            work->leapHeading = work->rotation.vy;
        }
    }
    if (work->stateFrames >= 45 && work->stateFrames <= 53) {
        angle                                 = work->leapHeading;
        speed                                 = -250;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->attackBody.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->stateFrames >= 45 && work->stateFrames <= 48 && work->playerDist < 0x171) {
        MadChaserWork* w;

        work->moveAccel         = 0;
        work->moveSpeed         = -200;
        w                       = (MadChaserWork*)arg0->work;
        w->animBlendFrames      = 2;
        w->animRate             = ANIMATION_RATE_ONE;
        w->animId               = 0x10;
        w->animRequest          = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->anchored          = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->subState         += 2;
        return;
    }
    if ((s16)work->stateFrames >= 47) {
        root->coord.t[1]      += work->moveSpeed;
        work->gridBody.pos.vy += work->moveSpeed;
        work->moveAccel       += 30;
        work->moveSpeed       += work->moveAccel;
        if (root->coord.t[1] >= work->moveStartPos.vy) {
            MadChaserWork* w = (MadChaserWork*)arg0->work;

            w->animBlendFrames    = 2;
            w->animRate           = ANIMATION_RATE_ONE;
            w->animId             = 0x12;
            w->animRequest        = MAD_CHASER_ANIM_REQUEST_BLEND;
            root->coord.t[1]      = work->moveStartPos.vy;
            work->gridBody.pos.vy = 0;
            work->stateFrames     = 0;
            work->subState++;
        }
    }
}
