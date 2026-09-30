/* Part of the web spider library; see web_spider.h. */

/// Steps the actor's coordinate, saving the previous translation in
/// `field_35C` first. The horizontal step follows the coordinate's forward
/// axis (`coord.m[*][2]`) scaled by the speed `field_398`, where 0x1000 is one
/// unit; `field_3A8` is added to the height unscaled.
void spiderMoveStep(Task* arg0)
{
    GfxCoord*        coord;
    Actor105500Work* work;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    work->field_35C.vx = coord->coord.t[0];
    work->field_35C.vy = coord->coord.t[1];
    work->field_35C.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_398) >> 12;
    coord->coord.t[1] += work->field_3A8;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_398) >> 12;
}
