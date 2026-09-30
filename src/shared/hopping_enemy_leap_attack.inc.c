/* Part of the hopping enemy library; see hopping_enemy.h. */

/// The leap: before frame 40 a pending hit request can cut it short. Frames
/// 43-46 pin part 6 at its view position, frame 45 locks the heading toward a
/// player within +/-0x300, frames 45-53 lunge 250 units a frame with the bite
/// body armed, and from 47 it falls under gravity; a player under 0x171 away
/// turns it into the rebound, and reaching the ground requests landing
/// animation 0x12.
void hopperLeapAttack(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    GfxCoord*        root = arg0->extra.tmd->coords;
    MATRIX           local;
    s16              angle;
    s32              soundId;
    s32              pan;
    s16              facing;
    s16              speed;

    if ((s16)++work->field_412 < 40) {
        if ((s16)hopperTakeHitRequest(arg0)) {
            return;
        }
    } else {
        work->field_438 = 1;
    }
    if ((s16)work->field_412 == 43) {
        GfxCoord* coords = arg0->extra.tmd->coords;
        SVECTOR*  v;

        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&gGfxViewCoord);
        coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&coords[6]);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[6].workm, &local);
        v                      = &work->field_98;
        v->vx                  = local.t[0];
        v->vy                  = local.t[1];
        v->vz                  = local.t[2];
        coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->field_412 >= 43 && work->field_412 <= 46) {
        work->field_432 = 1;
    } else {
        work->field_432 = 0;
    }
    if ((s16)work->field_412 == 46) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0005;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_412 == 45) {
        facing = (work->field_444 + 0x800) & 0xFFF;
        if (facing < 0x300) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else if (facing >= 0xD00) {
            work->field_40C = (facing + work->field_7A) & 0xFFF;
        } else {
            work->field_40C = work->field_7A;
        }
    }
    if (work->field_412 >= 45 && work->field_412 <= 53) {
        angle                                 = work->field_40C;
        speed                                 = -250;
        arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->obj_3AC.flags                  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->obj_3AC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->field_412 >= 45 && work->field_412 <= 48 && work->field_43A < 0x171) {
        Actor341700Work* w;

        work->field_428      = 0;
        work->field_42A      = -200;
        w                    = (Actor341700Work*)arg0->work;
        w->field_426         = 2;
        w->field_41C         = 0x10;
        w->field_418         = 0x10;
        w->field_414         = 1;
        work->field_432      = 0;
        work->obj_3AC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_422     += 2;
        return;
    }
    if ((s16)work->field_412 >= 47) {
        root->coord.t[1]     += work->field_42A;
        work->obj_2CC.pos.vy += work->field_42A;
        work->field_428      += 30;
        work->field_42A      += work->field_428;
        if (root->coord.t[1] >= (s16)work->field_92) {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_426         = 2;
            w->field_41C         = 0x10;
            w->field_418         = 0x12;
            w->field_414         = 1;
            root->coord.t[1]     = (s16)work->field_92;
            work->obj_2CC.pos.vy = 0;
            work->field_412      = 0;
            work->field_422++;
        }
    }
}
