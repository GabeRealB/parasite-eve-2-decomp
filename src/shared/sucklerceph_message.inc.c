/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Message handler of the first enemy. While the task is in state 1, modes 4
/// and 5 start the collapse: the 0x60080 effect is spawned, animation 1 is
/// bound and the reaction state moves to 4 (mode 5 also restarts the spawn
/// count). Mode 1 reveals a dormant or dropping enemy: on maps 0x27 and 0x28
/// the root is placed at the spawn point the command selects (playing the
/// appearance sound on 0x27), the heading is taken from it and folded into
/// -0x800..0x800, the model's buffers are allocated and shown, the bodies are
/// re-armed and the drop begins at the live stage. Mode 3 hides the model,
/// disarms the bodies, resets the root and returns the task to state 3.
s32 sucklercephMessage(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    SucklercephWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    SVECTOR          rot;
    u16              word;
    s16              heading;
    s32              magnitude;
    s32              mode;
    s32              state;
    s32              sound;
    s32              pan;

    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    state = arg0->state;
    work  = arg0->work;
    coord = obj->coords;
    if (state == 1) {
        mode = request->command;
        if (mode == 4) {
            Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, coord, 0x400, &gSucklercephCollapseFxOffset);
            work->animId = SUCKLERCEPH_ANIM_IDLE;
            sucklercephTickAnim(arg0);
            work->animFrames = 0;
            work->state      = SUCKLERCEPH_STATE_PUFFING;
            return 0;
        }
        if (mode == 5) {
            Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, coord, 0x400, &gSucklercephCollapseFxOffset);
            work->animId = SUCKLERCEPH_ANIM_IDLE;
            sucklercephTickAnim(arg0);
            work->animFrames  = 0;
            work->swellFrames = 0;
            work->state       = SUCKLERCEPH_STATE_PUFFING;
            return 0;
        }
    }
    word = request->command & 0xFF;
    if ((word & 0xFF) == 1) {
        if ((u32)(arg0->state - 1) >= 2U) {
            if (gGameSession->location.loc.area == 0x27) {
                rot.vx            = 0;
                rot.vy            = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].x;
                coord->coord.t[1] = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].y;
                coord->coord.t[2] = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].z;
                sound             = (((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54270006);
                pan               = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (gGameSession->location.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].x;
                coord->coord.t[1] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].y;
                coord->coord.t[2] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].z;
            }
            heading       = rot.vy;
            work->heading = heading;
            magnitude     = heading >= 0 ? heading : -heading;
            if (magnitude >= 0x801) {
                if (heading >= 0x801) {
                    work->heading = heading - 0x1000;
                } else if (heading < -0x800) {
                    work->heading = heading + 0x1000;
                }
            }
            Tmd_AllocBuffers(arg0->extra.tmd);
            arg0->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            enemy->node.state.parts.flags = 0;
            work->senseBody.flags        |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->body.flags             |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            RotMatrix(&rot, &coord->coord);
            work->forwardSpeed                    = 0xC8;
            work->dropArmed                       = 1;
            work->fallSpeed                       = 0x64;
            work->dropCollided                    = 0;
            work->state                           = SUCKLERCEPH_STATE_AWAKE;
            work->awakeStage                      = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->senseBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        rot.vz                        = 0;
        rot.vy                        = 0;
        rot.vx                        = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                     = 0;
        coord->coord.t[1]                     = 0;
        coord->coord.t[0]                     = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(arg0->extra.tmd->coords);
        arg0->state      = 3;
        work->dropArmed  = 0;
        work->state      = SUCKLERCEPH_STATE_DORMANT;
        work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_NONE;
    }
    return 0;
}
