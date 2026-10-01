/* Part of the burster library; see burster.h. */

/// Message handler of the first enemy. While the task is in state 1, modes 4
/// and 5 start the collapse: the 0x60080 effect is spawned, animation 1 is
/// bound and the reaction state moves to 4 (mode 5 also restarts the spawn
/// count). Mode 1 reveals a dormant or dropping enemy: on maps 0x27 and 0x28
/// the root is placed at the spawn point the command selects (playing the
/// appearance sound on 0x27), the heading is taken from it and folded into
/// -0x800..0x800, the model's buffers are allocated and shown, the bodies are
/// re-armed and the drop begins at the live stage. Mode 3 hides the model,
/// disarms the bodies, resets the root and returns the task to state 3.
s32 bursterMessage(Task* arg0, s32 arg1, ActorCommand* request)
{
    Actor104600Work* work;
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
    work  = (Actor104600Work*)arg0->work;
    coord = obj->coords;
    if (state == 1) {
        mode = request->command;
        if (mode == 4) {
            Gp_SpawnEff(0x60080, coord, 0x400, &gBursterCollapseFxOffset);
            work->field_2B8 = 1;
            Actor04600_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2B2 = 4;
            return 0;
        }
        if (mode == 5) {
            Gp_SpawnEff(0x60080, coord, 0x400, &gBursterCollapseFxOffset);
            work->field_2B8 = 1;
            Actor04600_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2D4 = 0;
            work->field_2B2 = 4;
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
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            } else if (gGameSession->location.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].x;
                coord->coord.t[1] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].y;
                coord->coord.t[2] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].z;
            }
            heading         = rot.vy;
            work->field_2B0 = heading;
            magnitude       = heading >= 0 ? heading : -heading;
            if (magnitude >= 0x801) {
                if (heading >= 0x801) {
                    work->field_2B0 = heading - 0x1000;
                } else if (heading < -0x800) {
                    work->field_2B0 = heading + 0x1000;
                }
            }
            Tmd_AllocBuffers(arg0->extra.tmd);
            arg0->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            enemy->node.state.parts.flags = 0;
            work->objFC.flags            |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->obj134.flags           |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            RotMatrix(&rot, &coord->coord);
            work->field_2BE                       = 0xC8;
            work->field_2E2                       = 1;
            work->field_2DE                       = 0x64;
            work->field_2E0                       = 0;
            work->field_2B2                       = 1;
            work->field_2C8                       = 1;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->objFC.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->obj134.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        rot.vz                        = 0;
        rot.vy                        = 0;
        rot.vx                        = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                     = 0;
        coord->coord.t[1]                     = 0;
        coord->coord.t[0]                     = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(arg0->extra.tmd->coords);
        arg0->state     = 3;
        work->field_2E2 = 0;
        work->field_2B2 = 0;
        work->field_2C8 = 0;
    }
    return 0;
}
