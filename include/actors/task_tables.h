#ifndef INCLUDE_ACTORS_TASK_TABLES_H
#define INCLUDE_ACTORS_TASK_TABLES_H

#include "main/task_types.h"

/// Public entry index and complete extent of the Glutton entrance spawn table.
enum {
    ACTOR_341900_TASK_EVENT          = 0,
    ACTOR_341900_ENTRANCE_TASK_COUNT = 10,
};

/// Glutton entrance recipes: event 0, fade 1, body 2, five attached parts 3..7, doors 8..9.
///
/// All run at priority 192, without a terminator. The event and fade are
/// bodyless; model entries skip automatic primitive-buffer allocation. The
/// incinerator room starts entry 0 with unused zero spawn arguments. Internally,
/// body argument 1 is part id 0 and argument 2 is the event parent; child parts
/// use ids 1..5 and the body parent. Doors borrow the event parent in argument 2.
/// Fade argument 1 supplies unsigned-halfword intensity units per tick.
/// Keep actor_341900, actor_444000's borrowed model/animation resources and the
/// selected room/CAP scene loaded while their tasks and script callbacks run.
extern TaskDesc gActor341900GluttonEntranceTaskDescs[ACTOR_341900_ENTRANCE_TASK_COUNT];

/// Public entry index and complete extent of the incinerator exit-event spawn table.
enum {
    ACTOR_342000_TASK_EVENT       = 0,
    ACTOR_342000_EVENT_TASK_COUNT = 10,
};

/// Incinerator exit recipes: event 0, fade 1, Glutton body 2, parts 3..7, doors 8..9.
///
/// All run at priority 192, without a terminator. Event and fade are bodyless;
/// model entries skip automatic primitive-buffer allocation. Start entry 0 at
/// state zero with zero spawn arguments and the room's placed encounter actor
/// live. Body argument 1 is part id 0 and argument 2 is the event parent; parts
/// use ids 1..5 (right leg, left leg, then three fixed parts) and the body parent.
/// Doors borrow the event parent in argument 2; nonzero argument 1 adds depth
/// bias for the timed close. Fade argument 1 is unsigned-halfword intensity
/// removed per tick. Keep actor_342000, actor_444000's model/animation resources
/// and the incinerator room/CAP scene loaded through task and script teardown.
extern TaskDesc gActor342000IncineratorEventTaskDescs[ACTOR_342000_EVENT_TASK_COUNT];

/// Night water-hole scene spawn table: unused entry 0, scene controller at entry 1.
///
/// Both entries are bodyless, without a terminator. Entry 1 runs at priority 32
/// with unused spawn arguments and selects the burner-dependent scene before
/// reloading the night driveway. Keep actor_146000 and its room/CAP resources
/// loaded through handoff. The empty room placement never spawns entry 0, whose
/// priority-192 callback retains a two-argument enemy address; do not spawn it.
extern TaskDesc gActor146000WaterHoleSceneTasks[2];

/// Bone Suckler TMD descriptor shared by the three actor-slot builds.
///
/// Priority 96 with automatic model buffers; spawn at index 0. Room imports
/// use the corresponding slot alias and must keep that slot's resources loaded.
extern TaskDesc gActor01200BoneSucklerTaskDesc;

/// Motel scene spawn recipes: 0 empty placement, 1 Kyle Madigan's view-parented body.
///
/// Both run at priority 192 with unused spawn arguments and no terminator.
/// Entry 0 has no body and ends on first dispatch. Entry 1 creates the
/// twenty-part TMD with automatic buffers disabled; choreography allocates
/// them when the double appears. Keep actor_120500 and the motel room/CAP
/// resources loaded through the room wrapper's actor stop-request handshake.
extern TaskDesc gActor120500EmptyPlacementTaskDesc[2];

/// Dumping-hole spawn recipes: 0 encounter/blaze controller, 1 kill placeholder, 2 fade, 3 body fire, 4 fire emitter.
///
/// All run at priority 192; only entry 4 has a coordinate body, the others are
/// bodyless. Indices 0..4 have no terminator. Fade argument 2 borrows the live
/// controller; emitter argument 1 carries its start delay. Keep actor_342100 and
/// the dumping-hole resources loaded through the encounter and burn sequence.
extern TaskDesc gActor342100EncounterBlazeTaskDescriptors[5];

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

/// Slot export of `gActor02400Tasks` from actor_102400, with the same descriptor contract.
extern TaskDesc gActor102400Tasks[2];

/// Spawn recipe for the staged Bone Suckler used by the motel-room event.
///
/// Index 0 creates a six-part TMD body at priority 96 with automatic buffers.
/// Spawn through the enemy API with a live owned `Enemy` in spawn argument 2.
/// Keep actor_123200's callbacks and borrowed geometry loaded until teardown.
extern TaskDesc gActor123200BoneSucklerTaskDesc;

/// Elevator scene spawn table: unused entry 0, vertical screen shake at entry 1.
///
/// Both entries are bodyless at priority 192, with unused spawn arguments and
/// no terminator. Keep actor_142900 loaded while the shake task runs, with at
/// most one live instance. The room loads this table with an empty placement
/// list. Entry 0 retains a two-argument enemy callback; do not spawn it.
extern TaskDesc gActor142900TaskTable[2];

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

/// Slot export of `gActor03700BatTaskDesc` from actor_103700, with the same descriptor contract.
extern TaskDesc gActor103700BatTaskDesc;

/// Slot export of `gActor07000SucklercephTaskDescs` from actor_107000, with the same descriptor contract.
extern TaskDesc gActor107000SucklercephTaskDescs[2];

/// Descriptor indices shared by the camera-parented figure packages.
enum {
    VIEW_FIGURE_TASK_BODY   = 0,
    VIEW_FIGURE_TASK_HELPER = 1,
    VIEW_FIGURE_TASK_COUNT  = 2
};

/// Camera-parented SWAT figure spawn descriptors for actor_110800.
///
/// Slots are `VIEW_FIGURE_TASK_BODY` and `VIEW_FIGURE_TASK_HELPER`, with no
/// terminator. Both allocate TMD bodies with automatic primitive buffers and
/// run at priority 192. Spawn the body through the enemy spawner so its second
/// spawn argument is the task's live `Enemy`; initialization spawns the helper.
/// The helper borrows body part 8, with a -50-unit local X offset, until body
/// teardown kills it. Keep this package's callbacks and geometry loaded while
/// either task lives; the message and animation helpers support one live figure.
extern TaskDesc gActor110800ViewFigureTasks[VIEW_FIGURE_TASK_COUNT];

/// Camera-parented SWAT figure spawn descriptors for actor_110300.
///
/// Slots are `VIEW_FIGURE_TASK_BODY` and `VIEW_FIGURE_TASK_HELPER`, with no
/// terminator. Both allocate TMD bodies with automatic primitive buffers and
/// run at priority 192. Spawn the body through the enemy spawner so its second
/// spawn argument is the task's live `Enemy`; initialization spawns the helper.
/// The helper borrows body part 8 until body teardown kills it. Keep this
/// package's callbacks and geometry loaded while either task lives; the message
/// and animation helpers support one live figure.
extern TaskDesc gActor110300ViewFigureTasks[VIEW_FIGURE_TASK_COUNT];

/// Grinning Stranger spawn recipe for the Acropolis Square departure sequence.
///
/// Priority 192. Spawn at index 0.
/// TMD bodies skip automatic primitive buffers.
/// Waits for the attachment wheel and display request to clear before setup.
/// Keep actor_111800 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor111800GrinningStrangerTaskDesc;

/// Gary Douglas spawn recipes: 0 body, 1 head and hat, 2 carried model.
///
/// All three create TMD bodies at priority 192; there is no terminator. Entry 0
/// skips automatic model buffers and expects a live owned `Enemy` in spawn
/// argument 2. The body spawns entries 1 and 2 with itself in argument 2 and
/// parent coordinate indices 4 and 8 respectively in argument 1. The children
/// borrow its coordinates and lighting. Keep actor_135400 loaded until teardown.
extern TaskDesc gActor135400GaryDouglasTasks[3];

/// Caterpillar body and puff spawn descriptors: indices 0 and 1, no terminator.
///
/// Both run at priority 96; the body allocates an eight-part TMD with
/// automatic model buffers, and the puff has one coordinate. Spawn through
/// the enemy API with a live owning `Enemy` in `Task::spawnArg2.pointer`.
/// The body retains the table for child spawns. Keep the package callbacks
/// and borrowed geometry loaded while either task lives.
extern TaskDesc gActor05500MaggotCaterpillarTasks[2];

/// Slot-1 export of `gActor05500MaggotCaterpillarTasks` from actor_105500.
extern TaskDesc gActor105500MaggotCaterpillarTasks[2];

/// Hurt Rupert Broderick spawn recipe for the M.I.S.T. briefing room.
///
/// Priority 192. Spawn at index 0.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy` for placement and teardown.
/// Keep actor_113000 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor113000RupertBroderickHurtTaskDesc;

/// Kyle Madigan spawn recipes: 0 walker body, 1 left hand, 2 right hand, 3 held model.
///
/// Priority 192. Indices 0..3 have no terminator.
/// The body skips automatic primitive buffers; attached TMD entries use them.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Children borrow the body task
/// in `Task::spawnArg2` and part 8 (left/held) or 12 (right) in `Task::spawnArg1`.
/// They join the body teardown tree and borrow its coordinates and lighting.
/// Keep actor_135600 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor135600KyleMadiganTaskDescriptors[4];

/// Mind Suckler TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor01500MindSucklerTaskDesc;

/// No. 9 GOLEM spawn recipe for the scripted sanctuary figure.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`; scripts select its poses.
/// Keep actor_110700 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor110700No9GolemTaskDesc;

/// No. 9 GOLEM spawn recipes: 0 scripted walker, 1 carried Beam Sword.
///
/// Priority 96. Indices 0..1 have no terminator.
/// The body uses automatic primitive buffers; the sword skips them.
/// Spawn both through the enemy interface. The sword is reparented under
/// the walker and borrows body part 7 and its work-owned lighting.
/// Keep actor_150400 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor150400No9GolemTaskDescriptors[2];

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

/// Garden variant-2 spawn recipe for the sound-enabled nineteen-part scene walker.
///
/// Creates a TMD body from the Eve model at priority 192 with automatic
/// primitive buffers. Spawn at index 0 through the enemy spawner: argument 2
/// must be its live `Enemy`. The task owns its model and work and publishes
/// singleton message state. Keep actor_151000 code, model and clip data loaded
/// until teardown; singleton messages must stop when that task is released.
extern TaskDesc gActor151000WalkerTaskDesc;

/// Spawn recipes for Eve's Dryfield lamp-shattering scene and its children.
///
/// Indices 0..10 are the double, fade in, fade out, debris spawner, five debris
/// models, blackout and texture uploader. There is no terminal descriptor.
/// All run at priority 192; the body and debris defer automatic model buffers.
/// The room loads actor_121300 as resource 132 before spawning the double at 0.
/// Debris arguments select lamp 0..11 and placement 0..14; texture uploads
/// borrow their live double in argument 2. Keep the actor and room resources
/// loaded until every child and borrowed scene callback has finished.
extern TaskDesc gActor121300SceneTasks[11];

/// Beta Generator body and Life Support spawn recipes exported by actor_105300.
///
/// Entry `GENERATOR_BODY_TASK_INDEX` creates the TMD body; entry
/// `GENERATOR_LIFE_SUPPORT_TASK_INDEX` creates its coordinate-only child.
/// Both execute at priority 96 and receive a live `Enemy` in spawn argument 2
/// through `enemySpawnFromTable`. The child requires its body task as parent.
/// The actor overlay and the body's borrowed model data must remain loaded.
extern TaskDesc gActor105300GeneratorTasks[GENERATOR_TASK_DESC_COUNT];

/// Entry-scene flight recipes: 0 TMD controller, 1 coordinate beam, 2 smoke emitter.
///
/// All run at priority 192. Spawn entry 0 with unused arguments to unfold and fly
/// the model. Entry 1 borrows the controller task through spawn argument 2 and
/// joins its teardown tree; entry 2 takes a copied parent-frame translation and
/// emits smoke independently. No terminator. Keep actor_141000 and its resources
/// loaded until every task using these callbacks and model data has ended.
extern TaskDesc gActor141000FlightTasks[3];

/// Scripted nineteen-part Eve walker with manual model-buffer recovery.
///
/// Creates a TMD body at priority 192, initially skipping automatic primitive
/// buffer allocation. Area placement supplies the task-owned `Enemy` in spawn
/// argument 2. Initialization installs animation, placement, visibility, walk,
/// pace and facial-texture messages. Keep actor_141000 loaded until teardown.
extern TaskDesc gActor141000EveWalkerTaskDesc;

/// Scene Eve's nineteen-part TMD walker with optional footstep sounds.
///
/// Spawn at index zero through the enemy spawner with a live owning `Enemy`
/// in argument 2. Priority 192 uses automatic model primitive buffers. The
/// task owns its model and work and publishes singleton message state.
/// Keep actor_535700's code, model and clips loaded until task teardown;
/// singleton messages require that original task and work to remain live.
extern TaskDesc gActor535700EveWalkerTaskDesc;

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

/// Blood Suckler hanging-pool controller, with no body, at priority 96.
///
/// Spawn at index 0 with unused arguments. It borrows hanging actor tasks,
/// keeps two drop slots filled during battle, and ends when candidates run out.
/// Keep the selected actor slot loaded while the controller and pool are live;
/// the night-toilet room imports the actor_104000 instance.
extern TaskDesc gActor04000HangingPoolTaskDesc;

/// Kyle Madigan spawn recipes: 0 walker body, 1 left hand, 2 right hand.
///
/// Priority 192. Indices 0..2 have no terminator.
/// The body skips automatic primitive buffers; attached TMD entries use them.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Hands borrow the body task
/// in `Task::spawnArg2` and part 8 or 12 in `Task::spawnArg1`, then join its teardown tree.
/// Keep actor_120400 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor120400KyleMadiganTaskDescriptors[3];

/// Trailer-coach Gary Douglas spawn recipes: 0 body, 1 head-and-hat, 2 shotgun.
///
/// All are TMD bodies at priority 192, without a terminator. Body placement
/// supplies a live Enemy in spawn argument 2; attachment spawns use zero
/// arguments and borrow the published singleton body. Keep actor_420700 loaded.
extern TaskDesc gActor420700GaryDouglasTaskDescriptors[3];

/// Slot export of `gActor07000SlouchTaskDescs` from actor_107000, with the same descriptor contract.
extern TaskDesc gActor107000SlouchTaskDescs[3];

/// Quiet scene-walker spawn descriptor using the Eve body at priority 192.
///
/// Creates a TMD task with automatic primitive buffers and singleton message
/// handlers. Keep the actor_451100 code, geometry and clip data loaded.
extern TaskDesc gActor451100QuietWalkTaskDesc;

/// Flint spawn recipe for the night garage scene.
///
/// Priority 192. Spawn at index 0.
/// TMD bodies skip automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`; actor-control holds suspend dispatch.
/// Keep actor_135400 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor135400FlintTaskDesc;

/// Gary Douglas night Main Street spawn recipes: body, unused, head, rifle, fade-in, fade-out.
///
/// All six slots have priority 192 and no terminator; never spawn unused slot 1.
/// Slots 0, 2 and 3 create TMD bodies with manually allocated primitive buffers.
/// Head/rifle argument 2 borrows a live parent coordinate; nonzero rifle argument
/// 1 selects its attached pose. Fade slots 4/5 are bodyless and take an unsigned
/// low-halfword intensity increment per tick in argument 1, normally 9.
/// Keep actor_136100 and the night Main Street scene resources loaded while
/// these tasks run; head and rifle initialization require the live body singleton.
extern TaskDesc gActor136100SceneTaskDescriptors[6];

/// First scripted walker TMD recipe, priority 192 with automatic model buffers.
///
/// Spawn at index 0 with its live Enemy owner in spawn argument 2. Keep
/// actor_143900 geometry, clips and callbacks loaded throughout the singleton walker's life.
extern TaskDesc gActor143900ScriptedWalkerTaskDesc;

/// Stinger spawn recipes: 0 enemy TMD body, 1 fireball coordinate body, 2 beam coordinate body.
///
/// All execute at priority 96; indices 0..2 have no terminator. Enemy spawns
/// supply a live Enemy in spawn argument 2. Keep this package loaded until
/// the body and its coordinate children are released.
extern TaskDesc gActor105100TaskDescriptors[3];

/// Pierce Carradine spawn recipes: 0 body, 1 billboard, 2 part-4 model, 3 part-2 model.
///
/// Priority 192. Indices 0..3 have no terminator.
/// The body skips automatic primitive buffers; attached TMD entries use them.
/// Children borrow their body task in `Task::spawnArg2` and parent-part index in `Task::spawnArg1`.
/// The body spawns the billboard on part 8 only in placement variant 2.
/// Child roots and lighting remain borrowed until the body teardown tree exits them.
/// Keep actor_113100 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor113100PierceCarradineTaskDescriptors[4];

/// Slot export of `gActor01600ScavengerTaskDesc` from actor_101600, with the same descriptor contract.
extern TaskDesc gActor101600ScavengerTaskDesc;

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

/// Slot export of `gActor05600PawnGolemTasks` from actor_105600, with the same descriptor contract.
extern TaskDesc gActor105600PawnGolemTasks[3];

/// Boss Stranger task recipe: nineteen-part TMD body, priority 96 and actor-managed buffers.
///
/// The callback and model data belong to actor_110600 and must stay loaded
/// while its tasks live. Area resources select this descriptor at task index 0.
extern TaskDesc gActor110600TaskDesc;

/// Grinning Stranger TMD spawn descriptor shared by actor_101900 and actor_301900.
///
/// Spawn at index 0 with a live owning `Enemy` in `Task::spawnArg2.pointer`.
/// Priority 96 attaches the nineteen-part body and automatic model buffers.
/// Placement entry 19 selects it; keep the package callbacks and borrowed
/// model geometry loaded through enemy teardown.
extern TaskDesc gActor01900GrinningStrangerTaskDesc;

/// Slot-1 export of `gActor01900GrinningStrangerTaskDesc` from actor_101900.
extern TaskDesc gActor101900GrinningStrangerTaskDesc;

/// Launcher Pawn GOLEM spawn descriptors: 0 body, 1 launcher, 2 grenade; all are TMD bodies.
///
/// Indices select live descriptors without a terminator. Keep this package
/// loaded while its callbacks and borrowed model geometry are in use.
extern TaskDesc gActor05600PawnGolemTasks[3];

/// Second scripted walker spawn recipes: 0 body, 1 attachment 1, 2 attachment 2.
///
/// All are TMD bodies at priority 192, without a terminator. Body placement
/// supplies a live Enemy in spawn argument 2. Attachment spawn argument 1
/// names the borrowed body part (1 or 12); both attachment tasks and the
/// singleton body require actor_143900 resources to remain loaded.
extern TaskDesc gActor143900SecondScriptedWalkerTaskDescriptors[3];

/// Entry indices of `gActor450800NurseryActorTasks`; every entry is live.
enum {
    ACTOR_450800_NURSERY_ACTOR_TASK_KYLE        = 0,
    ACTOR_450800_NURSERY_ACTOR_TASK_LEFT_HAND   = 1,
    ACTOR_450800_NURSERY_ACTOR_TASK_RIGHT_HAND  = 2,
    ACTOR_450800_NURSERY_ACTOR_TASK_SECOND_BODY = 3,
    ACTOR_450800_NURSERY_ACTOR_TASK_GUN         = 4,
    ACTOR_450800_NURSERY_ACTOR_TASK_COUNT       = 5,
};

/// Nursery actor recipes: Kyle, left hand, right hand, second body and gun.
///
/// Five indexed TMD entries at priority 192, without a terminator. Only the
/// second body skips automatic primitive-buffer allocation. Body spawns take
/// a live owning `Enemy` in argument 2; the signed high half of argument 1
/// selects initial visibility (1 shown, otherwise hidden). Both bodies create
/// the same hand and gun helpers. Helper argument 1 selects the parent's model
/// coordinate (8 left hand/gun, 12 right hand); set their parent before dispatch.
/// Keep the parent body alive through helper use, and this package's code and
/// borrowed model/animation data loaded through teardown.
extern TaskDesc gActor450800NurseryActorTasks[ACTOR_450800_NURSERY_ACTOR_TASK_COUNT];

/// Coordinate-body recipes: 0 destructible-part controller, 1 child part.
///
/// Both execute at priority 96; the two live entries have no end marker. Enemy
/// spawns supply a live `Enemy` in spawn argument 2. Controller placement modes
/// 1..3 select the tunnel, corridor or training room. The parent controller and
/// its work must outlive each child task. Keep actor_205200 and room resources loaded.
extern TaskDesc gActor205200ControllerPartTasks[2];

/// Slot export of `gActor02100WatcherTaskDesc` from actor_202100, with the same descriptor contract.
extern TaskDesc gActor202100WatcherTaskDesc;

/// Slot export of `gActor02400Tasks` from actor_202400, with the same descriptor contract.
extern TaskDesc gActor202400Tasks[2];

/// Spawn descriptors for the No. 9 GOLEM scene walker and its carried model.
///
/// Index 0 spawns the paired walker; it spawns index 1 as its child and
/// attaches that model to part 8. Both run at priority 96; only the child
/// skips automatic primitive buffers. Keep actor_451100's borrowed assets
/// and callbacks loaded while either task is live.
extern TaskDesc gActor451100PairWalkTaskDescs[2];

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

/// Blood Suckler spawn recipe for the general-store scripted figure.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy` for setup, update and teardown.
/// Keep actor_223600 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor223600BloodSucklerTaskDesc;

/// Slot-2 export of `gActor00700RatTaskDesc`, with the same spawn contract.
extern TaskDesc gActor200700RatTaskDesc;

/// Bone Suckler TMD spawn descriptor exported by actor slot 2.
///
/// Spawn at index 0; priority 96 with automatic model buffers. Keep the
/// slot's callback and borrowed model geometry loaded while its tasks run.
extern TaskDesc gActor201200BoneSucklerTaskDesc;

/// Water Tower Desert Chaser spawn recipe; priority 96, automatic model buffers disabled.
///
/// Spawn at index 0 with the owning Enemy in spawnArg2. The TMD body, animation
/// resources and actor_421600 callbacks must remain loaded through enemy teardown.
extern TaskDesc gActor421600DesertChaserTaskDesc;

/// Ivory Stalker body spawn recipe; priority 96 with automatic model buffers.
///
/// Spawn at index 0 with the owning Enemy in spawnArg2. The body initializes
/// its attached arms; keep actor_405800 callbacks and borrowed geometry loaded.
extern TaskDesc gActor405800IvoryStalkerTaskDesc;

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

/// Jodie Bouquet spawn recipes: 0 body, 1 held model.
///
/// Priority 192. Indices 0..1 have no terminator.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`. The held model borrows the body
/// in `Task::spawnArg2` and part 8 in `Task::spawnArg1`, then joins its teardown tree.
/// Body setup tears down the actor if its held-model spawn fails.
/// Keep actor_213100 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor213100JodieBouquetTaskDescriptors[2];

/// Slot-2 export of `gActor02600MaggotCaterpillarBodyTask` from actor_202600,
/// with the same body descriptor contract.
extern TaskDesc gActor202600MaggotCaterpillarBodyTask;

/// Slot-2 export of `gActor05500MaggotCaterpillarTasks` from actor_205500.
extern TaskDesc gActor205500MaggotCaterpillarTasks[2];

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

/// Spawn recipes for the Burner model (index 0) and its floor-puff emitter (index 1).
///
/// Both use priority 96. The model recipe owns a TMD task and defers primitive
/// buffer allocation; spawn argument 2 borrows the live Enemy. The emitter
/// owns a single coordinate body. Keep actor_403100 and its model assets loaded
/// throughout either task's lifetime.
extern TaskDesc gActor403100TaskDescriptors[2];

/// Odd Stranger TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor401800OddStrangerTaskDesc;

/// Slot export of `gActor04000BloodSucklerTaskDesc` from actor_204000, with the same descriptor contract.
extern TaskDesc gActor204000BloodSucklerTaskDesc;

/// Stationary masked Eve TMD recipe at priority 96 with automatic model buffers.
///
/// Spawn at index zero with a live `Enemy` in spawn argument 2. Placement mode
/// 0 selects the Shelter B6 corridor, 1 the training room. Keep actor_205200,
/// its borrowed model geometry and the selected room resources loaded while live.
extern TaskDesc gActor205200BodyTaskDesc;

/// Cafeteria woman spawn recipes: 0 animated body, 1 carried model.
///
/// Priority 192. Indices 0..1 have no terminator.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Entry 1 ignores its arguments
/// and follows part 4 of the published singleton body task; that body must
/// remain live while the carried model runs.
/// Keep actor_202900 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor202900WomanTaskDescriptors[2];

/// Eric Baldwin spawn recipes: 0 body, 1/2 held models, 3 three-root attachment, 4 right hand.
///
/// Priority 192. Indices 0..4 have no terminator.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Children borrow the body task
/// in `Task::spawnArg2`. Held models use part 8 and the right hand part 12 from
/// `Task::spawnArg1`; entry 3 attaches its three roots to body parts 9..11.
/// All children join the body teardown tree and borrow its lighting.
/// Keep actor_213000 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor213000EricBaldwinTaskDescriptors[5];

/// Rupert Broderick spawn recipe for the sanctuary scene.
///
/// Priority 192. Spawn at index 0.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy` for placement and teardown.
/// Keep actor_210700 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor210700RupertBroderickTaskDesc;

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

/// Sea Diver spawn recipes: 0 enemy body, 1 flying shot with a coordinate body.
///
/// Priority 96. Indices 0..1 have no terminator.
/// The TMD body uses automatic primitive buffers; the shot has no TMD buffer.
/// Body `Task::spawnArg2` borrows its live `Enemy`. The shot spawner supplies its
/// work and world transform before its first dispatch; descriptor data is unused.
/// Keep actor_206100 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor206100SeaDiverTaskDescriptors[2];

/// Blizzard Chaser spawn recipe for the Mine Cavern encounter.
///
/// Spawn at index 0 with priority 96. The 24-part TMD body skips automatic
/// primitive-buffer allocation. `Task::spawnArg2` borrows a live `Enemy` for
/// setup, update and teardown. Keep actor_403000 code and its borrowed model
/// and animation resources loaded while the spawned actor uses them.
extern TaskDesc gActor403000TaskDesc;

/// Grinning Stranger spawn recipe for the cafeteria scene.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies skip automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy` for setup, update and teardown.
/// Keep actor_210600 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor210600GrinningStrangerTaskDesc;

/// Scavenger TMD spawn descriptor; priority 96, with automatic model buffers disabled.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor01600ScavengerTaskDesc;

/// Slot export of `gActor01100Tasks` from actor_201100, with the same descriptor contract.
extern TaskDesc gActor201100Tasks[3];

/// Dryfield No. 9 GOLEM spawn recipes (0 body, 1 attached gunblade).
///
/// Both are TMD bodies at priority 96 with automatic model buffers; no terminator.
/// Area placement selects the body; its initializer spawns the gunblade with the
/// parent Enemy in spawnArg2. The child borrows the body's work and lighting.
/// Keep actor_521100 and its model/animation resources loaded until both are gone.
extern TaskDesc gActor521100No9GolemTaskDescs[2];

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

/// Moth TMD spawn descriptor; priority 96 with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor300700MothTaskDesc;

/// Akropolis No. 9 spawn recipes: body, prop, weapon, chest, grenade, light and blast source.
///
/// Indices 0..6, without a terminator; all run at priority 96. Entries 0..5
/// allocate TMD bodies with automatic buffers, entry 6 a coordinate body.
/// Child entries borrow the body enemy through spawn argument 2; light entry
/// 5 takes index 0..2 in argument 1. Keep actor_510900 and its models loaded
/// throughout the tasks' lifetimes.
extern TaskDesc gActor510900TaskDescriptors[7];

/// Bone Suckler TMD spawn descriptor exported by actor slot 3.
///
/// Spawn at index 0; priority 96 with automatic model buffers. Keep the
/// slot's callback and borrowed model geometry loaded while its tasks run.
extern TaskDesc gActor301200BoneSucklerTaskDesc;

/// Masked Eve Brea reversing-walker spawn recipe for the B6 corridor.
///
/// Nineteen-part TMD body at priority 192, with automatic model buffers disabled.
/// Spawn at index 0 with the owning Enemy in spawnArg2. Keep actor_350500 and
/// its borrowed model/animation resources loaded until the task is destroyed.
extern TaskDesc gActor350500MaskedEveReverseWalkTaskDesc;

/// Stranger spawn recipe for the fire-escape barrier encounter.
///
/// Priority 192. Spawn at index 0.
/// TMD bodies skip automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Its task edits the loaded
/// fire-escape collision grid, which must remain live through the encounter.
/// Keep actor_311500 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor311500StrangerTaskDesc;

/// Rat TMD spawn descriptor exported by actor_300700; priority 96.
///
/// Spawn at index 0 with a live `Enemy` in `Task::spawnArg2.pointer`.
/// Automatic model buffers attach the seven-part rat; placement entry 7
/// selects it through file-group base 30. Keep the package callbacks and
/// borrowed geometry loaded until enemy teardown.
extern TaskDesc gActor300700RatTaskDesc;

/// Masked Eve Brea reversing-walker spawn recipe for the B6 training room.
///
/// Nineteen-part TMD body at priority 192, with automatic model buffers disabled.
/// Spawn at index 0 with the owning Enemy in spawnArg2. Keep actor_350700 and
/// its borrowed model/animation resources loaded until the task is destroyed.
extern TaskDesc gActor350700MaskedEveReverseWalkTaskDesc;

/// Scripted SWAT member 1 spawn recipe for the Akropolis patio.
///
/// TMD body at priority 96 with automatic model buffers disabled. Spawn at index
/// 0 with the owning Enemy in spawnArg2. Keep actor_312200 callbacks and borrowed
/// model/animation resources loaded through teardown.
extern TaskDesc gActor312200SwatMember1TaskDesc;

/// Scripted ANMC-woman spawn recipes: 0 nineteen-part model, 1 periodic splatter emitter.
///
/// Both run at priority 192, with no terminator. Entry 0 skips automatic model
/// buffers and borrows its Enemy through spawn argument 2. Entry 1 has no body;
/// argument 1 selects the woman (0) or player (1), and argument 2 borrows the
/// live woman task. Keep actor_521100 and the selected models loaded through use.
extern TaskDesc gActor521100AnmcWomanTaskDescs[2];

/// Scripted Grinning Stranger spawn recipe for the Akropolis patio.
///
/// TMD body at priority 192 with automatic model buffers disabled. Spawn at index
/// 0 with the owning Enemy in spawnArg2. Keep actor_317000 callbacks and borrowed
/// model/animation resources loaded through the scripted leap and teardown.
extern TaskDesc gActor317000GrinningStrangerTaskDesc;

/// Slot export of `gActor04000BloodSucklerTaskDesc` from actor_304000, with the same descriptor contract.
extern TaskDesc gActor304000BloodSucklerTaskDesc;

/// Gary Douglas's body, attached head-and-hat and gun spawn recipes (indices 0..2).
///
/// The night motel balcony resource selects body 0. Body initialization spawns
/// head 1 on part 4 and gun 2 on part 8, borrowing the body's lighting and
/// lifetime. All run at priority 192; only the body skips automatic model buffers.
/// The actor package's code and model data must remain loaded for all three tasks.
extern TaskDesc gActor335800GaryDouglasTasks[3];

/// SWAT figure spawn recipe for the security-monitor camera.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`. The figure is parented to the
/// view coordinate and shown only on its mapped monitor camera.
/// Keep actor_311900 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor311900SwatTaskDesc;

/// Rupert Broderick spawn recipe for the security-monitor walking scene.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies use automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`. The view-parented figure starts
/// walking once seen and selects teardown after ninety walking updates.
/// Keep actor_311900 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor311900RupertTaskDesc;

/// Kyle Madigan spawn recipes: 0 walker body, 1 left hand, 2 right hand, 3 held gun.
///
/// Priority 192. Indices 0..3 have no terminator.
/// The body skips automatic primitive buffers; attached TMD entries use them.
/// Body `Task::spawnArg2` borrows its live `Enemy`. Children borrow the body task
/// in `Task::spawnArg2` and part 8 (left/gun) or 12 (right) in `Task::spawnArg1`.
/// They join the body teardown tree and borrow its coordinates and lighting.
/// Keep actor_350700 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor350700KyleMadiganTaskDescriptors[4];

/// Desert Chaser spawn recipe for the breezeway scripted encounter.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies skip automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`; this package carries the
/// cutscene build of the Desert Chaser lifecycle.
/// Keep actor_323400 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor323400DesertChaserTaskDesc;

/// Toilet encounter spawn recipes: 0 woman, 1 Lesser Stranger revealed by her command.
///
/// Priority 192. Indices 0..1 have no terminator.
/// The woman skips automatic primitive buffers; the Stranger uses them.
/// Body entry 0 borrows its live `Enemy` in `Task::spawnArg2`. Entry 1 receives
/// zero spawn arguments; its setup establishes its own placement and work.
/// Keep actor_323300 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor323300WomanStrangerTaskDescriptors[2];

/// Spawn recipe for the night motel balcony's independently placed Flint model.
///
/// The room resource addresses this single nineteen-part TMD recipe directly.
/// It runs at priority 192 and skips automatic model buffers; actor_335800's
/// code and geometry must stay loaded until Flint's task is released.
extern TaskDesc gActor335800FlintTaskDesc;

/// Horned Stranger spawn recipe for Neo Ark's forest zone.
///
/// TMD body at priority 96 with automatic model buffers. Spawn at index 0 with
/// the owning Enemy in spawnArg2. Keep actor_356100 callbacks and borrowed
/// model/animation resources loaded through enemy teardown.
extern TaskDesc gActor356100HornedStrangerTaskDesc;

/// Desert Chaser spawn recipe for the main-street scripted encounter.
///
/// Priority 96. Spawn at index 0.
/// TMD bodies skip automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`; this package carries the
/// cutscene build of the Desert Chaser lifecycle.
/// Keep actor_323000 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor323000DesertChaserTaskDesc;

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

/// Scripted tentacle used by Shelter R48 resource slot 45.
///
/// Creates the nineteen-part actor_361100 model with explicit primitive-buffer
/// control, then handles placement, animation, translation and a burning collapse.
extern TaskDesc gActor503500TentacleTaskDesc;

/// Slot export of `gActor01100Tasks` from actor_301100, with the same descriptor contract.
extern TaskDesc gActor301100Tasks[3];

/// Slot export of `gActor02000PawnGolemTasks` from actor_302000, with the same descriptor contract.
extern TaskDesc gActor302000PawnGolemTasks[2];

/// Slot-3 export of `gActor01900GrinningStrangerTaskDesc` from actor_301900.
extern TaskDesc gActor301900GrinningStrangerTaskDesc;

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

/// Spawn indices of the scripted Pawn GOLEM tables, without a terminator.
enum {
    PAIR_WALK_TASK_BODY          = 0,
    PAIR_WALK_TASK_CARRIED_MODEL = 1,
    PAIR_WALK_TASK_COUNT         = 2,
};

/// Nursery's scripted Pawn GOLEM spawn descriptors: body 0, carried Beam Sword 1.
///
/// Both create TMD bodies at priority 96 with automatic model buffers disabled.
/// The body creates and owns the sword child, which follows part 7 and borrows
/// the body's lighting. Spawn the body through its room placement; keep
/// actor_450800's callbacks, models and animation data loaded until teardown.
extern TaskDesc gActor450800PawnGolemTasks[PAIR_WALK_TASK_COUNT];

/// Dryfield night R08's scripted Pawn GOLEM spawn descriptors: body 0, carried Beam Sword 1.
///
/// Both create TMD bodies at priority 96 with automatic model buffers disabled.
/// The body creates and owns the sword child, which follows part 7 and borrows
/// the body's lighting. Spawn the body through its room placement; keep
/// actor_535700's callbacks, models and animation data loaded until teardown.
extern TaskDesc gActor535700PawnGolemTasks[PAIR_WALK_TASK_COUNT];

#endif // INCLUDE_ACTORS_TASK_TABLES_H
