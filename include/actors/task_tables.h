#ifndef INCLUDE_ACTORS_TASK_TABLES_H
#define INCLUDE_ACTORS_TASK_TABLES_H

#include "main/task_types.h"

/// Bone Suckler TMD descriptor shared by the three actor-slot builds.
///
/// Priority 96 with automatic model buffers; spawn at index 0. Room imports
/// use the corresponding slot alias and must keep that slot's resources loaded.
extern TaskDesc gActor01200BoneSucklerTaskDesc;

// Room placement records address these tables in the currently loaded actor
// slots. Different packages occupy those slots over the lifetime of a stage.

/// Brain Stinger spawn descriptors: 0 enemy body, 1 drain attachment, 2 flying fireball.
///
/// Entries 0 and 1 allocate TMD bodies with automatic buffers; entry 2 allocates
/// one coordinate body. All run at priority 96. Indices are 0..2, without a
/// terminator. Keep the callbacks and borrowed models loaded for the tasks' lives;
/// the drain child additionally borrows its parent enemy's work and lighting.
extern TaskDesc gActor00300Tasks[3];

/// Sucklerceph spawn descriptors: 0 standing, 1 hidden/drop, both with TMD bodies.
///
/// Indices are 0..1 with no terminator, at priority 96. The hidden/drop entry
/// skips automatic model buffers and waits for a room command to reveal it.
/// Enemy spawns supply the owning Enemy in spawnArg2; spawnArg1's low halfword
/// selects the sound variant. A signed high halfword of 1 cancels a hidden spawn.
/// Keep the callbacks and borrowed model geometry loaded while tasks use them.
extern TaskDesc gActor07000SucklercephTaskDescs[2];

/// Slouch spawn descriptors: 0 standing TMD body, 1 coordinate-body projectile, 2 hidden/drop TMD body.
///
/// Indices are 0..2 with no terminator, at priority 96. The hidden entry skips
/// automatic model buffers and waits for a room command. Enemy spawns supply
/// the owning Enemy in spawnArg2; a signed high halfword of 1 in spawnArg1
/// cancels a hidden spawn. Projectile spawnArg1 is its aim yaw, 4096 units per turn.
/// Keep the callbacks and borrowed model geometry loaded while tasks use them.
extern TaskDesc gActor07000SlouchTaskDescs[3];

/// Slot export of `gActor07000SucklercephTaskDescs` from actor_207000, with the same descriptor contract.
extern TaskDesc gActor207000SucklercephTaskDescs[2];

/// Slot export of `gActor07000SlouchTaskDescs` from actor_207000, with the same descriptor contract.
extern TaskDesc gActor207000SlouchTaskDescs[3];

/// Standing Sucklerceph TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor04600SucklercephTaskDesc;

/// Skull Stalker TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor04600SkullStalkerTaskDesc;

/// Black Beetle TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor03800BlackBeetleTaskDesc;

/// Bat TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor03700BatTaskDesc;

/// Amoeba spawn descriptors: 0 TMD body, 1 coordinate-body fireball.
///
/// Spawn indices are 0..1, without a terminator. Keep the callbacks and
/// borrowed model geometry loaded while their tasks are in use.
extern TaskDesc gActor02400Tasks[2];

/// Beam Sword Rook GOLEM spawn descriptors: 0 body, 1 sword, 2 shield; all are TMD bodies.
///
/// Spawn indices are 0..2, without a terminator. Keep the callbacks and
/// borrowed model geometry loaded while their tasks are in use.
extern TaskDesc gActor02300RookGolemTasks[3];

/// Watcher TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor02100WatcherTaskDesc;

/// Mossback spawn descriptors: 0 body, 1 coordinate-body spit glob, 2 coordinate-body spit cloud.
///
/// Spawn indices are 0..2, without a terminator. Keep the callbacks and
/// borrowed model geometry loaded while their tasks are in use.
extern TaskDesc gActor01100Tasks[3];

/// Desert Chaser TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor00100DesertChaserTaskDesc;

/// Laboratory terminal task table: slot 0 ends a placement task, slot 1 runs login/menu/quiz control.
///
/// Slot 1 borrows a persistent `ScreenFade` as its second spawn argument and
/// requests its return on initialization; the actor overlay must remain loaded.
extern TaskDesc gActor143000TerminalTasks[2];

/// Slot export of `gActor02100WatcherTaskDesc` from actor_102100, with the same descriptor contract.
extern TaskDesc gActor102100WatcherTaskDesc;

extern TaskDesc D_actor_142600_80135E24;

/// Slot export of `gActor02400Tasks` from actor_102400, with the same descriptor contract.
extern TaskDesc gActor102400Tasks[2];

extern TaskDesc D_actor_123200_80137234;

extern TaskDesc D_actor_142900_80137600[];

/// Slot export of `gActor04600SucklercephTaskDesc` from actor_104600, with the same descriptor contract.
extern TaskDesc gActor104600SucklercephTaskDesc;

/// Slot export of `gActor02500Tasks` from actor_102500, with the same descriptor contract.
extern TaskDesc gActor102500Tasks[2];

/// Bodyless observatory scene tasks: 0 immediate stop, 1 companion puffs, 2 player head aim.
///
/// Loaded with the actor_450200 package through the observatory's area resources.
/// Direct spawns use entries 1 and 2; entry 0 kills its task on first dispatch.
/// The table has no terminator, and the task callbacks require the scene's actors
/// and room resources to remain loaded while they run.
extern TaskDesc gActor450200SceneTaskDescs[3];

/// Slot export of `gActor03800BlackBeetleTaskDesc` from actor_103800, with the same descriptor contract.
extern TaskDesc gActor103800BlackBeetleTaskDesc;

/// Slot export of `gActor04600SkullStalkerTaskDesc` from actor_104600, with the same descriptor contract.
extern TaskDesc gActor104600SkullStalkerTaskDesc;

/// Rat TMD spawn descriptor shared by actor_100700 and actor_200700; priority 96.
///
/// Spawn at index 0 with a live `Enemy` in `Task::spawnArg2.pointer`.
/// Automatic model buffers attach the seven-part rat; keep its package loaded
/// until enemy teardown. Placement entry 7 selects this descriptor.
extern TaskDesc gActor00700RatTaskDesc;

/// Bone Suckler TMD spawn descriptor exported by actor slot 1.
///
/// Spawn at index 0; priority 96 with automatic model buffers. Keep the
/// slot's callback and borrowed model geometry loaded while its tasks run.
extern TaskDesc gActor101200BoneSucklerTaskDesc;

/// Moth TMD spawn descriptor shared by actor_100700 and actor_200700; priority 96.
///
/// Spawn at index 0 with a live `Enemy` in `Task::spawnArg2.pointer`.
/// Automatic model buffers attach the four-part moth; keep its package loaded
/// until enemy teardown. Placement entry 8 selects this descriptor.
extern TaskDesc gActor00700MothTaskDesc;

extern TaskDesc D_actor_511000_80139924[];

/// Slot export of `gActor03700BatTaskDesc` from actor_103700, with the same descriptor contract.
extern TaskDesc gActor103700BatTaskDesc;

/// Slot export of `gActor07000SucklercephTaskDescs` from actor_107000, with the same descriptor contract.
extern TaskDesc gActor107000SucklercephTaskDescs[2];

extern TaskDesc gActor110800ViewFigureTasks[];

extern TaskDesc D_actor_461800_80139F8C[];

extern TaskDesc gActor110300ViewFigureTasks[];

extern TaskDesc D_actor_111800_8013A468;

extern TaskDesc D_actor_135400_8013A4AC[];

extern TaskDesc gMaggotCaterpillarBodyTask;

extern TaskDesc D_actor_105500_8013A8DC[];

extern TaskDesc D_actor_113000_8013ABB4;

extern TaskDesc D_actor_135600_8013B0C4[];

/// Mind Suckler TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor01500MindSucklerTaskDesc;

extern TaskDesc D_actor_110700_8013BF94;

extern TaskDesc D_actor_150400_8013C8F4[];

/// Descriptor indices shared by the Beta and Proto Generator spawn tables.
///
/// Both tables contain exactly two recipes and have no terminator.
enum {
    GENERATOR_BODY_TASK_INDEX         = 0,
    GENERATOR_LIFE_SUPPORT_TASK_INDEX = 1,
    GENERATOR_TASK_DESC_COUNT         = 2
};

/// Proto Generator body and Life Support spawn recipes exported by actor_105400.
///
/// Entry `GENERATOR_BODY_TASK_INDEX` creates the TMD body; entry
/// `GENERATOR_LIFE_SUPPORT_TASK_INDEX` creates its coordinate-only child.
/// Both execute at priority 96 and receive a live `Enemy` in spawn argument 2
/// through `enemySpawnFromTable`. The child requires its body task as parent.
/// The actor overlay and the body's borrowed model data must remain loaded.
extern TaskDesc gActor105400GeneratorTasks[GENERATOR_TASK_DESC_COUNT];

extern TaskDesc D_actor_151000_8013D2E0;

extern TaskDesc D_actor_121300_8013D390[];

/// Beta Generator body and Life Support spawn recipes exported by actor_105300.
///
/// Entry `GENERATOR_BODY_TASK_INDEX` creates the TMD body; entry
/// `GENERATOR_LIFE_SUPPORT_TASK_INDEX` creates its coordinate-only child.
/// Both execute at priority 96 and receive a live `Enemy` in spawn argument 2
/// through `enemySpawnFromTable`. The child requires its body task as parent.
/// The actor overlay and the body's borrowed model data must remain loaded.
extern TaskDesc gActor105300GeneratorTasks[GENERATOR_TASK_DESC_COUNT];

extern TaskDesc D_actor_141000_8013D77C;

extern TaskDesc D_actor_535700_8013DADC;

/// Twenty-part model task for the sterilization room's placed scene actor.
///
/// Creates a TMD body at priority 96 and runs the paced walker's spawn/frame
/// states. Scene placement supplies a live owning `Enemy` through `spawnArg2.pointer`.
/// The `actor_160600` model, clips and callbacks must remain loaded until teardown.
extern TaskDesc gActor160600PlacedActorTask;

/// Blood Suckler TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor04000BloodSucklerTaskDesc;

extern TaskDesc D_actor_120400_8013E748[];

extern TaskDesc D_actor_420700_8013EF68[];

/// Slot export of `gActor07000SlouchTaskDescs` from actor_107000, with the same descriptor contract.
extern TaskDesc gActor107000SlouchTaskDescs[3];

extern TaskDesc D_actor_451100_8013F734;

extern TaskDesc D_actor_135400_8013F8D8;

extern TaskDesc D_actor_136100_80140744[];

extern TaskDesc D_actor_143900_801413EC;

extern TaskDesc D_actor_105100_80141464[];

extern TaskDesc D_actor_146300_801427C8[];

extern TaskDesc D_actor_461800_801437EC;

extern TaskDesc D_actor_113100_80144308[];

/// Slot export of `gActor01600ScavengerTaskDesc` from actor_101600, with the same descriptor contract.
extern TaskDesc gActor101600ScavengerTaskDesc;

extern TaskDesc gPairWalkTasks[];

extern TaskDesc D_actor_511000_801472E8[];

/// Slot export of `gActor01100Tasks` from actor_101100, with the same descriptor contract.
extern TaskDesc gActor101100Tasks[3];

/// Slot export of `gActor02300RookGolemTasks` from actor_102300, with the same descriptor contract.
extern TaskDesc gActor102300RookGolemTasks[3];

/// Slot export of `gActor02000PawnGolemTasks` from actor_102000, with the same descriptor contract.
extern TaskDesc gActor102000PawnGolemTasks[2];

/// Beam Sword Pawn GOLEM spawn descriptors: 0 body, 1 attached sword.
///
/// Both are priority-96 TMD tasks with automatic model buffers. Indices 0..1
/// select descriptors without a terminator. Keep this actor package loaded
/// while the callbacks, model geometry and body animation bank are borrowed.
extern TaskDesc gActor02000PawnGolemTasks[2];

/// Slot export of `gActor00300Tasks` from actor_100300, with the same descriptor contract.
extern TaskDesc gActor100300Tasks[3];

extern TaskDesc gActor460200StrideWalkTasks[];

/// Slot export of `gActor05600PawnGolemTasks` from actor_105600, with the same descriptor contract.
extern TaskDesc gActor105600PawnGolemTasks[3];

/// Boss Stranger task recipe: nineteen-part TMD body, priority 96 and actor-managed buffers.
///
/// The callback and model data belong to actor_110600 and must stay loaded
/// while its tasks live. Area resources select this descriptor at task index 0.
extern TaskDesc gActor110600TaskDesc;

extern TaskDesc D_actor_101900_80149120[];

/// Launcher Pawn GOLEM spawn descriptors: 0 body, 1 launcher, 2 grenade; all are TMD bodies.
///
/// Indices select live descriptors without a terminator. Keep this package
/// loaded while its callbacks and borrowed model geometry are in use.
extern TaskDesc gActor05600PawnGolemTasks[3];

extern TaskDesc D_actor_143900_80149664[];

extern TaskDesc D_actor_450800_8014AC88[];

extern TaskDesc D_actor_205200_8014CA60[];

/// Slot export of `gActor02100WatcherTaskDesc` from actor_202100, with the same descriptor contract.
extern TaskDesc gActor202100WatcherTaskDesc;

/// Slot export of `gActor02400Tasks` from actor_202400, with the same descriptor contract.
extern TaskDesc gActor202400Tasks[2];

extern TaskDesc D_actor_451100_8014E6E4[];

/// Skull Stalker TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor207200SkullStalkerTaskDesc;

/// Slot export of `gActor04600SucklercephTaskDesc` from actor_204600, with the same descriptor contract.
extern TaskDesc gActor204600SucklercephTaskDesc;

/// Scorpion spawn descriptors: 0 TMD body, 1 coordinate-body corpse poison.
///
/// Indices select live descriptors without a terminator. Keep this package
/// loaded while its callbacks and borrowed model geometry are in use.
extern TaskDesc gActor02500Tasks[2];

/// Slot export of `gActor03800BlackBeetleTaskDesc` from actor_203800, with the same descriptor contract.
extern TaskDesc gActor203800BlackBeetleTaskDesc;

/// Slot export of `gActor04600SkullStalkerTaskDesc` from actor_204600, with the same descriptor contract.
extern TaskDesc gActor204600SkullStalkerTaskDesc;

extern TaskDesc D_actor_223600_80150B48;

/// Slot-2 export of `gActor00700RatTaskDesc`, with the same spawn contract.
extern TaskDesc gActor200700RatTaskDesc;

/// Bone Suckler TMD spawn descriptor exported by actor slot 2.
///
/// Spawn at index 0; priority 96 with automatic model buffers. Keep the
/// slot's callback and borrowed model geometry loaded while its tasks run.
extern TaskDesc gActor201200BoneSucklerTaskDesc;

extern TaskDesc D_actor_421600_80151254;

extern TaskDesc D_actor_405800_801514CC;

/// Spawn recipe for the Zebra Stalker body task, running at priority 96.
///
/// Requires this actor overlay's model, animation records and code to remain
/// loaded until the body and its arm tasks are released. The callback takes
/// ownership of the Enemy in spawnArg2.pointer. The high nibble of spawnArg1's
/// low byte selects 0 ordinary, 1 water, 2 group drop, 3 Junk Yard, 4 floor drop
/// or 5 delayed group entrance; the low nibble is ignored.
extern TaskDesc gActor400600ZebraStalkerTaskDesc;

/// Slot export of `gActor03700BatTaskDesc` from actor_203700, with the same descriptor contract.
extern TaskDesc gActor203700BatTaskDesc;

extern TaskDesc D_actor_213100_801521A8[];

/// Slot-2 export of `gActor02600MaggotCaterpillarBodyTask` from actor_202600,
/// with the same body descriptor contract.
extern TaskDesc gActor202600MaggotCaterpillarBodyTask;

extern TaskDesc D_actor_205500_801528DC[];

extern TaskDesc gPairWalkTasks[];

/// Gray Stalker body-task spawn recipe exported to Dryfield underpass resources.
///
/// Creates a TMD body at execution priority 96. The `actor_400500` package and
/// its geometry must remain loaded throughout the body and child-arm lifetimes.
extern TaskDesc gActor400500TaskDesc;

/// Creeping Stranger TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor207200CreepingStrangerTaskDesc;

/// GOLEM TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor403900GolemTaskDesc;

/// Knight GOLEM model-task descriptor exported to room resource tables.
///
/// Attaches the nineteen-part GOLEM body at task priority 96; the second spawn
/// argument borrows the live Enemy through spawn, living frames and corpse frames.
extern TaskDesc gActor402200GolemTaskDesc;

/// Odd Stranger spawn recipe exported by actor_401000 to room resources.
///
/// Uses a TMD body at priority 96 and defers primitive-buffer allocation
/// until the chosen opening state needs drawing. Borrows a live `Enemy` as
/// spawn argument 2; the actor overlay and its model data must remain loaded.
extern TaskDesc gActor401000StrangerTaskDesc;

extern TaskDesc D_actor_511000_80155070[];

extern TaskDesc D_actor_403100_8015560C[];

/// Odd Stranger TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor401800OddStrangerTaskDesc;

/// Slot export of `gActor04000BloodSucklerTaskDesc` from actor_204000, with the same descriptor contract.
extern TaskDesc gActor204000BloodSucklerTaskDesc;

extern TaskDesc D_actor_205200_801567C4;

extern TaskDesc D_actor_202900_80156E24[];

extern TaskDesc D_actor_213000_80157DE0[];

extern TaskDesc D_actor_210700_801585CC;

/// Spawn recipes for Pierce in shelter room 47 and his attached held model.
///
/// Entry 0 creates the twenty-part body without an automatic primitive buffer;
/// its second spawn argument must point to the live owning `Enemy`. Entry 1
/// creates the held model: first argument is the parent part index (Pierce uses
/// 4), second argument is the live parent `Task*`. Both run at priority 192.
/// The table has two entries and no terminator. Keep `actor_443500` loaded for
/// the tasks' callbacks and borrowed model/animation resources.
extern TaskDesc gActor443500PierceTaskDescs[2];

/// Horned Stranger TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep the callback and borrowed model geometry loaded.
extern TaskDesc gActor401300HornedStrangerTaskDesc;

extern TaskDesc D_actor_206100_80158B0C[];

extern TaskDesc D_actor_403000_80158D58;

extern TaskDesc D_actor_260500_80159DB0;

extern TaskDesc D_actor_210600_8015A4EC;

/// Scavenger TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor01600ScavengerTaskDesc;

/// Slot export of `gActor01100Tasks` from actor_201100, with the same descriptor contract.
extern TaskDesc gActor201100Tasks[3];

extern TaskDesc D_actor_521100_8015F6E4[];

/// Slot export of `gActor02300RookGolemTasks` from actor_202300, with the same descriptor contract.
extern TaskDesc gActor202300RookGolemTasks[3];

/// Slot export of `gActor02000PawnGolemTasks` from actor_202000, with the same descriptor contract.
extern TaskDesc gActor202000PawnGolemTasks[2];

/// Slot export of `gActor00300Tasks` from actor_200300, with the same descriptor contract.
extern TaskDesc gActor200300Tasks[3];

/// Slot export of `gActor05600PawnGolemTasks` from actor_205600, with the same descriptor contract.
extern TaskDesc gActor205600PawnGolemTasks[3];

/// TMD host recipe for the garbage-incinerator Glutton, at execution priority 96.
///
/// The room borrows this descriptor from the loaded actor_444000 overlay.
/// Its model source and callback must remain loaded for the host's lifetime.
extern TaskDesc gActor444000GluttonHostTaskDesc;

extern TaskDesc D_actor_341300_80165208[];

/// Moth TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor300700MothTaskDesc;

extern TaskDesc D_actor_510900_80167A18[];

/// Bone Suckler TMD spawn descriptor exported by actor slot 3.
///
/// Spawn at index 0; priority 96 with automatic model buffers. Keep the
/// slot's callback and borrowed model geometry loaded while its tasks run.
extern TaskDesc gActor301200BoneSucklerTaskDesc;

extern TaskDesc D_actor_350500_80168EA4;

extern TaskDesc D_actor_311500_80169338;

extern TaskDesc D_actor_300700_801693AC;

extern TaskDesc D_actor_350700_80169D10;

extern TaskDesc D_actor_312200_80169F7C;

extern TaskDesc D_actor_521100_8016A388[];

extern TaskDesc gMaggotCaterpillarBodyTask;

extern TaskDesc D_actor_361100_8016BAE4;

extern TaskDesc D_actor_317000_8016CF44;

/// Slot export of `gActor04000BloodSucklerTaskDesc` from actor_304000, with the same descriptor contract.
extern TaskDesc gActor304000BloodSucklerTaskDesc;

/// Gary Douglas's body, attached head-and-hat and gun spawn recipes (indices 0..2).
///
/// The night motel balcony resource selects body 0. Body initialization spawns
/// head 1 on part 4 and gun 2 on part 8, borrowing the body's lighting and
/// lifetime. All run at priority 192; only the body skips automatic model buffers.
/// The actor package's code and model data must remain loaded for all three tasks.
extern TaskDesc gActor335800GaryDouglasTasks[3];

extern TaskDesc D_actor_311900_8016EC00;

extern TaskDesc D_actor_311900_8016EC0C;

extern TaskDesc D_actor_350700_801708DC[];

extern TaskDesc D_actor_323400_8017120C;

extern TaskDesc D_actor_361100_80171BAC;

extern TaskDesc D_actor_323300_8017255C[];

/// Spawn recipe for the night motel balcony's independently placed Flint model.
///
/// The room resource addresses this single nineteen-part TMD recipe directly.
/// It runs at priority 192 and skips automatic model buffers; actor_335800's
/// code and geometry must stay loaded until Flint's task is released.
extern TaskDesc gActor335800FlintTaskDesc;

extern TaskDesc D_actor_356100_80173294;

extern TaskDesc D_actor_323000_80173A08;

/// Spawn recipe indices for the incinerator encounter controller and its row spawners.
enum {
    ACTOR_342400_ENCOUNTER_CONTROLLER_DESCRIPTOR = 0,
    ACTOR_342400_MAD_CHASER_SPAWNER_DESCRIPTOR   = 1,
    ACTOR_342400_SLOUCH_SPAWNER_DESCRIPTOR       = 2,
    ACTOR_342400_PAIR_SPAWNER_DESCRIPTOR         = 3,
    ACTOR_342400_ENCOUNTER_DESCRIPTOR_COUNT      = 4,
};

/// Bodyless tasks driving the incinerator's mixed-enemy encounter.
///
/// Index 0 starts the controller at priority 32; 1 Mad Chaser, 2 Slouch and
/// 3 Sucklerceph-pair spawners run at priority 97. Spawner spawnArg1 packs a row
/// 0..16 in the high halfword and an appear command in the low halfword.
/// There is no terminator. Keep actor_342400 and the selected enemy/room
/// resources loaded until the controller and spawners finish.
extern TaskDesc gActor342400EncounterTaskDescs[ACTOR_342400_ENCOUNTER_DESCRIPTOR_COUNT];

/// Mad Chaser spawn recipes: 0 ordinary, 1 waits for a scripted emerge command.
///
/// Both create the nine-part TMD at priority 96. Index 1 skips automatic model
/// buffers; emergence allocates them explicitly. Supply a live Enemy as the
/// second spawn argument, normally through enemySpawnFromTable. The low nibble
/// of the first argument selects the spawn kind; scripted bit 16 cancels spawn.
/// Keep actor_342400's code and borrowed model/animation resources loaded while
/// tasks use them. Only indices 0..1 are exposed here; there is no terminator.
extern TaskDesc gActor342400MadChaserTaskDescs[2];

/// Slot export of `gActor01600ScavengerTaskDesc` from actor_301600, with the same descriptor contract.
extern TaskDesc gActor301600ScavengerTaskDesc;

extern TaskDesc D_actor_341700_80176354;

/// Scripted tentacle used by Shelter R48 resource slot 45.
///
/// Creates the nineteen-part actor_361100 model with explicit primitive-buffer
/// control, then handles placement, animation, translation and a burning collapse.
extern TaskDesc gActor503500TentacleTaskDesc;

/// Slot export of `gActor01100Tasks` from actor_301100, with the same descriptor contract.
extern TaskDesc gActor301100Tasks[3];

/// Slot export of `gActor02000PawnGolemTasks` from actor_302000, with the same descriptor contract.
extern TaskDesc gActor302000PawnGolemTasks[2];

extern TaskDesc D_actor_301900_80179120[];

extern TaskDesc D_actor_310600_801796A4[];

/// Launcher Rook GOLEM spawn descriptors: 0 body, 1 launcher, 2 grenade, 3 shield.
///
/// All four recipes create TMD bodies at priority 96 with automatic model
/// buffers. Indices address live entries; there is no terminator. Room
/// placements spawn entry 0, whose work borrows this table for its children.
/// Keep the actor package loaded while any spawned task, including a detached
/// grenade, uses its callbacks or borrowed model geometry.
extern TaskDesc gActor05700RookGolemTasks[4];

/// Maggot body spawn recipe with an eight-part TMD model and execution priority 96.
///
/// Spawn at index 0 with a live `Enemy*` in spawn argument 2. Its placement
/// record supplies the entrance mode; the body dispatches setup, active and
/// dying phases. Model primitive buffers use the normal automatic allocation
/// policy. Keep the actor package and borrowed model geometry loaded while the
/// body or its puff children remain live.
extern TaskDesc gActor02600MaggotCaterpillarBodyTask;

extern TaskDesc gActor450800PairWalkTasks[];

extern TaskDesc gActor535700PairWalkTasks[];

#endif // INCLUDE_ACTORS_TASK_TABLES_H
