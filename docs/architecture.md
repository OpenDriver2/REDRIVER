# Architecture

How the game is put together, as far as it is translated. Read it before working on a subsystem
instead of rediscovering it from IDA; add to it when a translation reveals how something works.
Translation progress per file: `python tools/progress.py` (see [ida.md](ida.md)).

## Start-up and main flow

```
WinMain (shell.c)
  InitMemDebug, InitScreen, Init_FileSystem, InitWindow     [/WINDOW: framed window, [A]]
  LoadSavedOptions (frontend/config.c)                      config.dat; failure -> "Please run driver configuration"
  InitDisplay -> grSetDevice, Detect3dfx, InitDrawingSystem (Engine/3dfx/gfxinit.c)
  InitTimers (time.c)
  MainInit (glaunch.c): CoInitialize, InitDirectInput, InitStringMng, InitSoundDevices,
                        InitSoundSystem, InitCDAudio
  DoFrontend (frontend/main.c)        normal start (gDoFrontend)
    FadeInScreen x3                   splash BMPs (frontend/frontend.c)
    PlayRender                        FMV intro (not translated)
    InitFrontend, menu loop: HandleMenu + DrawFrontend, choosefuncs open sub-menus
    StartGame (frontend/frontend.c) -> GameInit, GameLoop, GameExit (glaunch.c)
  or DoFlat (flat/flat.c)             the "motel" front end (gDoMotelRoom)
  or StartGame(0)                     straight into the game
  StopTimers, MainExit, ShutdownDrawingSystem, grGlideReallyShutdown, CloseMemDebug
```

`HandleWinMessages` pumps Windows messages; the game calls it from its loops. Waits like
`FadeInScreen` don't pump at all (see `/WINDOW` in [building.md](building.md)).

## Graphics: Glide through a pointer table

The engine (`Engine/3dfx/*`) is written for 3dfx Glide 2.x and calls it only through function
pointers `grX` (`D3DGlide/glide_ptrs.h`, defined in `D3DGlide/system.c`):

- `grSetDevice` picks the device; `UseD3DDevice` (`D3DGlide/glide2x.c`) fills the table with the
  game's own Glide-on-Direct3D 6 implementation `d3dgl_X` / `d3dgu_X`; `UseGlideDevice`
  (`gfxinit.c`) with the real `glide2x.dll`. `device_type` / `using_glide` tell which.
- A few `grX` names without `_` (`grGlideInit`, `grSstWinOpen`, `grSstControl`, `grLfbLock`,
  `grTexDownloadMipMap2`, `grTexDownloadTable2`, …) are engine wrappers in `system.c` that call
  the `gr_X` pointers or `d3d_X` extras of the D3D path.
- The D3D path renders to `lpRenderSurface`; full screen flips, windowed blits into the clipped
  primary surface (`d3dgl_BufferSwap`). Texture memory is emulated: 64 KB slots
  (`Engine/3dfx/tset.c`: `TextureSlots[256]` texture sets, `gTSlot[64]` slots), downloads go
  through `grTexDownloadMipMap2` with the texture set number.
- `GrVertex.oow` is 1 for Glide and 1/64 for D3D (`using_glide`); the D3D code takes texture
  coordinates and depth from it.
- Render state caching in the engine: `last_tset`, `last_state` (`TEXT3` …), `last_trans_on`.

## Frontend drawing (8 bit, CPU)

The PC menus are not drawn with Glide polygons:

- `InitFrontend` loads `DATA\FRONTEND.TSD` (`LoadFrontendTextures`): palettes into `gPalette`,
  256x256 8 bit texture pages copied to a 64 KB aligned buffer (`gRamTextureRawDataDump`), one per
  `TextureSlots[n].data`, and the texture names/rectangles into `tinfo_block`
  (`GetTextureDetails(name)` in `map.c`).
- Everything is drawn into `vscreen` (640x480, 8 bit, `frontend/cpupoly.c`) by the `tsd.c`
  blitters (chroma key index 113, 50% blend table `pallookup_hh`).
- `CPUEndOfFrame` locks the back buffer (`grLfbLock`, pixel format from `d3d_LfbLock`), converts
  `vscreen` through `pallookup` (`COPY8TO16` / `COPY8TO32`), then draws the "hires" polys
  (double size, straight into the frame buffer), and unlocks.
- A screen is a list of `FE_POLY` (sprites / filled rectangles, ends with flag 4) and `FE_TEXT`
  (ends with `text == NULL`), shown with `OpenFrontendScreen`. Menus are `FE_BUTTON` arrays
  (`selfunc`, `unselfunc`, `choosefunc`, neighbours `l r u d`) run by `HandleMenu`; `selfunc(1)` /
  `unselfunc(1)` make the selection flash. `FE_TEXT.justify` is `PrintAt`'s mode (centre, right,
  translucent for locked items).
- Sub-menus all follow one pattern: `SelectX` sets the texts, runs `HandleMenu` until a choosefunc
  returns non-zero (-1 = Escape → `CloseFrontendScreen`, return 0 to the caller); a choosefunc
  that opens a deeper menu calls `CloseFrontendScreen`, `SelectY`, and on 0 reopens its own screen
  with `OpenFrontendScreen`. Choosing a game ends in `StartGameFromFrontend` /
  `StartMissionFromFrontend` and returns 1.
- Option menus (details, soundoptions, controller, keymap, joymap) also run in the motel room
  (`gDoMotelRoom`): there `MotelMenuKeyControl` replaces `HandleMenu`, `DrawMotel` replaces
  `DrawFrontend`, and items call `selfunc(2)` / `unselfunc(2)`, which set the text index(es) to draw
  red instead of switching the font. Sliders set `on_slider`; the `FE_BUTTON` entries after the
  menu's last item (no choosefunc, which ends the menu) are the left / right steps, reached through
  the slider item's `l` / `r`. Key / joystick redefinition flashes the item until `GetKeyInput` /
  `GetJoyKeyInput` (`frontend/gvars.c`) returns a key, and refuses to leave while two actions share one.
- Motel room (`flat/`): `DoFlat` loops `ControlMotel` (left / right turn the camera to the next of
  five options, Return selects) and `DrawMotel` (the room is the map, drawn with the city's palette;
  menu texts and icons over it without depth). Selecting moves the camera close (`DoMotelCamera`,
  `flat/prescam.c` builds the path along the fly-by spline `FLATn.FBD`) and queues the option's
  menu (`queuefunc`), run when the camera stops: save game, exit, drive (`flat_retcode` -2),
  options (the frontend option menus in motel mode), answerphone (the mission ladder room's
  messages; accept a mission → `flat_retcode` = message, or a car park → `DoMotelCarSelect`).
  Icons are Glide quads in 640 x 480 coordinates (`MyDrawIcon`, scaled by `motel_xscale` /
  `motel_yscale`), faded with `gMotelFadeIntensity`.
- Mission ladder (`mladder/mladder.c`): the story mode. `StartMissionFromFrontend` (new game or a
  loaded save) runs `InitMissionLadder`, which compiles `DATA\MLADDER.DML` (one command per line:
  `SETCITY`, `ROOM` ... `ENDROOM` with `MISSION` / `JOKE` / `CARSELECT` messages, `SUBMISSION`,
  `INTERVIEW`, `RENDER`, `TRIGGER`, `IF` / `IFOK` ... `ELSE` ... `ENDIF`, `QUITONFAIL`, `END`) into
  a command list linked by `next_cmd` / `else_cmd`, and one `MLADDER_ROOM` (4 answerphone messages)
  per room. `RunMissionLadder` walks it: a room runs `DoFlat` and continues at the accepted
  message's commands (the motel's "drive" plays Take a Drive in the current city and stays), a
  mission sets `GameMode` and runs `StartGame`, `IFOK` / `QUITONFAIL` test `mission.objective`
  (0xFF0 = failed), `TRIGGER` / `IF` the bits of `mladder.flags`, `END` plays the credits replay
  and unlocks the cheats. Before each room and mission the position goes into `game_data` (saved
  by the motel's save game). Leaving the motel or quitting a game (`exit_flag`) ends the ladder.
- Credits (`credits.c`): the `CREDITS` replay with `gDoCredits` (the cheats menu's
  `credits_choosefunc`, the ladder's `END`, `/CREDITS`). `GameInit` calls `InitCredits` (the table's
  y positions, the `PERSON1` - 3 texture sets), `GameLoop` `ControlCredits` (one pixel a frame) and
  `DrawCredits`: black over the right half of the screen, the table scrolled up it (logos, texts,
  language strings, photos), the Driver logo on top, gradient fades (`FlatPoly2`). A photo is
  copied into one of the 3 `PERSON` sets when it scrolls in (`GetCreditsTexture`). The replay ends
  by itself or on Escape (`DM_UNKNOWN` in `HandleDirector`).
- Level loading (`map.c`): `FlatInit` (motel; `GameInit` for the game) loads `<level>.D3D` (the
  palettes of each texture set), `FRUSTRUM.DAT` (`view_arc`, the visible cells per camera angle)
  and the LEV file (`ReadLevelFile`): a lump list — texture sets (8 bit parent / child pairs or
  16 bit, `ProcessTextureLump`), models (`ConvertModelToInternalFormat` turns the file format
  into int indices and float colours / uvs, `RelocateModel` turns offsets into pointers; an
  instance shares its parent's vertices), the cell map in packed regions, roads, junctions, PVS,
  lamps, chairs … `DriverFRead` drives the loading bar. Four regions around the player are
  unpacked into a 2 x 2 "barrel" (slot = (region x & 1) + 2 (region z & 1), `UnpackRegion`);
  `DrawMap` walks the cells of `view_arc` that the PVS marks visible and sorts their objects by
  `draw_type` (normal, water, shadow, alpha, transparent, neon).
- Object drawing (`Engine/3dfx/object.c`, `polydraw.c`): `SetupFrustrum` builds six world space
  planes; `FrustrumCheck` / `FrustrumCheckFar` return 0 (outside), 1, 2 (crosses the near plane)
  or 3 (crosses the far plane) and set `do_clipping`. `DrawObject*` (any matrix, rotation about
  y reusing `calculated_matrix`, axis aligned models, cars) back face test once per normal (the
  first polygon of a normal has flag 8) and call the draw function of each polygon type through
  `PolygonDrawFunction` / `ClipNearPolygonDrawFunction` / `ClipFarPolygonDrawFunction`
  (`InitDrawingSystem` points them at the `_D3D` or `_Glide` tables). Vertices are rotated once
  per object (`conv_points`, `current_vertex_computed_value`); a near / far clipped polygon is
  clipped edge by edge into `clip_poly_info` (`ClipTestScale*`) and drawn as a triangle fan.
  `_LIT` types take their light from `light_intensity` (per normal) or `conv_point_normals` (per
  point normal), plus the lamps near the model (`CalcVisibleLamps`). Triangles go through
  `gDrawTriangle` / `gDrawTriangleWithClip` (D3D: `d3dgl_DrawTriangle_REAL3D`, batched per
  texture set). Only the D3D draw functions are translated so far.
- Game loop (`glaunch.c`): `GameInit` compiles the mission (`LoadMission`), loads the level and
  sets up every system; `StartGame` runs `GameLoop` while `quit`: the first pass runs two frames,
  then `Game_Control` as many times as `WaitForSync` says (the logic runs at a fixed rate) and one
  `Game_Draw`. `Game_Control` reads the pad (`FillPad`: KeySet keys → `CAR_PAD_*` bits in
  `controller_bits`), runs camera, map, events, `ControlCars` (each car's pad → `ProcessCarPad`,
  `StepCarPhysics` adds it to `active_car_list`), the AI and cops (not in training), then the
  physics (`GlobalTimeStep`) and the mission script (`MissionHandler`).
- Car physics (`handling.c`, PSX style fixed point: 4096 = 1.0, positions in world units,
  `st.fposition` in 1/16, angles 4096 to a turn): `StepOneCar` adds gravity (`GRAVITY`), pushes the
  body points (`car_cosmetics.cPoints`) out of the ground with an impulse, then per wheel the
  suspension (compression 0 - 60 → force), side friction (front wheels along the steering angle),
  thrust and brakes; `ConvertTorqueToAngularAcceleration` damps the spin. `GlobalTimeStep`
  integrates; cars that may touch (`CheckCarToCarCollisions`, bounding boxes) take 4 sub frames
  of 2 Runge-Kutta steps with the car to car impulses (`CarCarCollision3`), the
  others one Euler step; then it rebuilds the matrix from the quaternion (`RebuildCarMatrix`).
  The ground is `FindSurface` (`MapHeight*` alike): the road map cell (`roadbits.c`,
  `ROADS_GetRoadInfo`: surface model type, height, heading) and, if the type has a surface model
  (`SurfacePtrs`), the plane of its polygon under the point (grass and water lower the friction).
- Car models (`denting.c`, `InitialiseDenting` from `GameInit`): per model type (0 player,
  1 cop, 2 - 11 civilian) the level's clean, damaged and low models (`PLAYERCARnCLEAN`,
  `COPCARDAM`, `CIVCARnLOW` ...) in `gCarCleanModelPtr` / `gCarDamModelPtr` / `gCarLowModelPtr`.
  Each car slot (id + 1) draws its own copy of the clean model (`gTempCarVertDump`,
  `gTempCarNormalDump`, `gTempCarPolyDump`); `ApplyDentingOnCar` moves the vertices of the six
  damage zones (`gDamageZoneVerts`) towards the damaged model and recalculates the normals, the
  player car also gets dent stages blitted into its damage texture page (`BlitTextDent`).
- Collisions: car to car (`bcoll3d.c`, from `CheckCarToCarCollisions`) tests two oriented boxes
  (the car matrix and `car_cosmetics.colBox`) face by face and returns the least penetration;
  `DamageCar3D` (`bcollide.c`) adds the damage to one of six zones. Scenery (`objcoll.c`,
  `DoScenaryCollisions` in `Game_Control` after `ControlCars`, before the physics): for the
  player car, car 65 (mission recording / replay) and wrecked or cop civilian cars, the packets
  (`MODEL.collision_block`: boxes and cylinders) of the models in the car's cell, the event
  models and the helicopter become `BUILDING_BOX`es; the player's axis aligned boxes next to each
  other are merged (`MergeAllBoxes`) so walls have no joins. `CarBuildingCollision` tests box
  and car as rectangles on the ground (`bcollided2d`), finds the time of the hit by halving the
  frame's movement (`bFindCollisionTime`), pushes the car out along the face normal and applies
  the impulse, damage (`DamageCar`), sparks, felony; smashable models are knocked over instead.
- Smashables (`smashable.c`): `damage_object` reports carnage, adds the model's mission time
  bonus (`ModelTimeBonus`), smashes a chair there (`SmashChair`) and moves the cell object out of
  the map (x = -20000000); one of six `damaged_object` slots keeps its real x and a velocity from
  the car's. `MoveSmashable_object` (in `Game_Control`) flies it under gravity until it lands
  (one bounce); `DrawSmashable_sprites` draws it tumbling, opaque and transparent passes, from
  `Game_Draw`. A lamp breaks on its third hit (`damage_lamp`). `EnableModel` hides or shows a
  model on the map (`ModelActive`, mission scripts).
- Carnage (`carnage.c`, the Carnage driving game): `InitCarnage` sets `carnage_flags` and
  `BuildCarnageTable` groups the level's smashable models by kind (cones, bins, boxes … found by
  name, each kind sorted); `ReportCarnage` adds `values[kind]` for a smashed model
  (`GetCarnageType`), or with 0xFF00 100 per 120 of car damage (`new_damage`, from
  `DamageCar` / `DamageCar3D`), up to 999999, into `global_score.items`.
- Street lamps (`lamps.c`): the LEV lamps lump keeps the lamps per map sector (`lamp_data`). At
  night `ProcessLamps` (from `Game_Draw`) adds those of the 13 x 13 sectors around the player
  (21 x 21 with `gUltraLamps`) and the mission lamps within 30000 to the lit lamps (`AddLamp`);
  type 1 flickers. `InvertLamps` mirrors them for the rear view mirror.
- Weather and light (`effects.c`): `DoNightSpecificStuff` adds the night lamps (player
  headlights by front damage, flashing sirens of the player's police car and pursuing cops, the
  interview car park, the pending lamps) and runs the lightning (`DoLightning`: sky and light
  flashes with thunder, not under cover). `DoDawn` / `DoDusk` step the sky colour
  (`ChangeSkyRGB`, the sky polys of models 0 – 3) and move the light source towards the day /
  night one over 500 steps (`SetLightSource`, `ChangeLightLevel`). Rain and snow share
  `gRainList` (100 drops round the camera, a free list of drop numbers; `CheckForUnderCover`
  tests the indoor models' cover boxes, collision packet type 4); rain also draws splashes
  (`DisplaySplashes`) and the reflections of the lights stored this frame on the wet road
  (`StoreLightReflection`, `DisplayLightReflections`, depth sorted). `text3d` / `DrawWord` is
  the 3D countdown (3, 2, 1, GO, CHECKPOINT, RACE OVER models).
- Checkpoint (`checkpoint.c`): the mission's one `Checkpoint` is either the race countdown (type 1,
  from the script's `INITIALISERACE`: 3, 2, 1 at 30 frames each, then GO; it finishes objective 3)
  or a point to reach (type 2, `CHECKPOINT` command: within the radius of the player's car it
  finishes objective 1). `HandleCheckpoint` runs every frame from the mission script; the word
  models are found by language in `InitCheckPointMode` and drawn by `DrawWord`.
- Animated objects (`objanim.c`): each level has a list of `ANIMATED_OBJECT`s (model name, effect
  id, offset, parameters). `InitAnimatingObjects` (from `GameInit`) resolves the names, sets
  `SHAPE_FLAGS_ANIMOBJ` on the models and their instances, fills the `smashable[]` models, and
  colours the lamp glass and billboard polygons for day or night. `DrawCellObject` hands such
  models to `DrawAnimatingObject`, which adds the effect (traffic lights from the junction phase,
  lamp haze, smoking drums and manholes, boats bobbing with `fsintab`) and draws the model.
  `DoNeonColourCycle` rotates ranges of palette 19 (neon signs) while light flares are on.
- Overlay (`overlay.c`): `DisplayOverlays` (from `Game_Draw`) draws the damage / felony /
  opponent `PERCENTAGE_BAR`s (texts from the mission's DAMAGEBAR ... variables), the speedometer
  and the overhead map. `LoadLevelMap` keeps the level's `.MAP` (an 8 bit BMP, 16 bit when 8 bit
  textures are emulated) in `map_data`; `GrabMapGlide` copies the 128x128 pixels around the player
  into the MAP_X texture, which `DisplayOverheadMap` draws turned with the player and clipped,
  flashing when the cops know where the player is, with the player, targets, cops and their view
  cones, a beam to the nearest target and the compass. `DrawFullscreenMap` (the pause menu's map)
  loads the whole map page by page through `TempTPage`. `DrawTargetArrows` puts an arrow (or !,
  watch, stop sign) over the mission targets; `DrawGameSpecificOverlays` writes the lap times and
  records of the driving games (`InitLapTimes`, `UpdateBestLap`).
- Pause menu (`pause.c`): `PauseGame` asks, `CheckPause` (once a frame) opens or closes.
  `OpenPauseMenu` pauses the sounds and picks the `PAUSE_TEXT` menu from the mission state (pause,
  mission passed / failed, game over, demo over, replay); `PauseKeyControl` moves through it and
  runs the entry's function, `RenderInput` draws it (or the full screen map) with the sound and
  draw distance sliders. Leaving through a replay (`yes_func`) keeps `mission` / `global_score`
  in `backup_mission` / `backup_score`, restored when the menu opens after it.
- Tyre tracks and shadows (`shadow.c`): each wheel keeps a ring of `num_tracks` `TYRE_TRACK`
  quads (the player's in `track_buffer`, the recorded car's in `other_track_buffer`);
  `AddTyreTrack` adds one from the wheel's last ground position to the new one (with tyre smoke,
  grass on grass) and chains it to the one before while the car keeps sliding
  (`continuous_track`). `DrawTyreTracks` draws the runs as strips darkening the ground
  (`GR_BLEND_ONE_MINUS_SRC_COLOR`), fading in at the first quad and out at the last.
  `DrawShadow` lays the CARSHAD texture under a car at its `shadowPoints`: one quad, or with
  detailed shadows a grid of quads following `MapHeightShadow` (always for the player).
- Effects (`debris.c`): smoke, flames, sparks and water splashes share the `smoke` pool, debris
  and leaves have their own (`debris`, `leaf`), each with a free list (`AllocateSmoke` ...).
  The `Setup_*` / `Add*` functions start them, `HandleEffects` moves them every other frame.
  The car and street lights are added each frame (`StartOfEffectsFrame` empties the lists):
  brake and indicator lights into `brake_lights`, flares and lamp hazes into `light_flares`.
  `DisplayEffects` draws the leaves, birds and debris, then sorts the smoke and the lights by
  depth into `smoke_table` and draws them additively (`DisplaySortedEffectsList`; black smoke
  and dust subtract). Moving lights at night leave streaks from their screen position two
  frames ago (`carblur_info` per car light, `Known_Lamps` for traffic lights, see
  `find_lamp_streak`). `DrawLensFlare` draws the sun; under Glide it counts the pixels of an
  8 x 8 square at the sun that pass the depth test (`grSstPerfStats`) to dim the flare.
  `DrawCarPools` (headlight pools at night) and `DrawAllPedShadows` draw ground quads through
  the Glide `DrawPOLYFT4`.
- Traffic (`ai.c`, road map in `roadbits.c`): civilian n is `civilian_ai_data[n]` driving
  `car_data[n + 1]` (`civ_ai_data_pt = &car_data[1]`); slots come from `civilian_slots` up to
  `gTrafficDensity` (`AddCivilianCar` / `RemoveCivilianCar`). `ControlAI` (in `Game_Control`) runs
  each car's `CivilianStateFunction` (`CIVSTATE_*`; `CIVSTATE_PURSUIT` is the cop chase in
  `cops.c`), `ControlSpeed` (thrust to the lane speed, braking for the junction and for cars in
  the way, `SlowCarInWayCheck` with `pre_rotated_boxes`) and `StepCarPhysics`; it removes cars more
  than 27 road map cells from the player and refills through `ControlPings` →
  `AddCarRelativeToPlayer`, which places a car (`AllocateAndPlaceCar`) at a random
  `road_ping_points` cell of the ring `BuildJunctionPingInfo` built (23 cells out,
  `GetPingPoints`). Positions are world units, lanes 1500 wide; `currentLane` is the direction
  (0 north, 1 east, 2 south, 3 west), `laneOffset` the lane from the road's centre.
  `ROADS_GetNextJunction` fills `RouteData` with the junction ahead and the distance to it;
  `SelectNextRoad` picks the exit (`RoadLaneInfo`, one way roads `IsOneWay` 0x80 + direction);
  near the junction `GetThrustState` (lights `JunctionLightsNS` / `WE`, give way) decides to go,
  `TurnToTarget` / `CivilianState_TURNING` steer into the new lane. Parking and lane changes
  (`TryParkingCar`, `TryChangingLanes`) set `CVST_*` flags; `DrawCivilianCars` draws the cars in
  range grouped by palette, with `DrawCarExtras` (indicators, brake and cop lights).
  `RequestRoadBlock` / `SetupRoadBlock` place cop cars (`CIVSTATE_BLOCKING`) across the road
  30000 units ahead of the player.
- Cop routes (`pathfind.c`): a 55 x 55 grid of road map cells centred on the cop's target cell
  (offset 1512 = `node_offset`). `GenerateOccupancyGrid` copies the cells from
  `RoadMapInfoRegions` (`CopyRun`: bit 0, bits 1 - 4 from the object bits 26 - 29, 30 = blocked)
  and `AddOtherCarsToOccupancyGrid` marks the quarter cells covered by `close_cars` (bit 5, a
  polygon clipped against each quarter); `CostGrid` (`CostValues`) and `BlockedGrid` follow.
  `GetRouteAStar` is an A* from the cop's cell to the target (`open_queue` binary heap, `nodes`
  with per-search cookies, diagonal steps only past a free orthogonal neighbour); `sub_50E1A7`
  turns the result into `COP.route_entries` (world positions, an extra node round the corner of
  a blocked cell, `direction`), with `num_nodes_to_copy` entries of the old route kept in front
  (`save_path`). `ShowRoute` and `DisplayOccupancyGrid` are debug displays (`OverlayOccupancyGrid`).
- Cops (`cops.c`): a cop is a civilian car with `CIV_STATE.IsCopCar` = 1 - 8, its data in
  `cop_data[IsCopCar - 1]`. `ControlCops` runs each frame: `ControlCopDetection` (any cop that
  `CopCanSeePlayer` updates `last_known_x/z`, `OutOfSightCount` counts frames unseen, speech 0x800A
  when the player is lost), `AddClosestCopCars` turns parked or passing cars into cops up to
  `max_active_cops`, `ControlCopTargetting` and `ControlGridGeneration` / `CalculateRoadRouteToPlayer`
  feed the routes of `pathfind.c`; `CopState_PURSUIT` steers a cop along its route with the speeds
  scaled by `gCopsSpeedScale`. `AddFelonyPoints` / `PlayCopSpeechOnce` are the cop side of the felony.
- Felony (`felony.c`): `FelonyRating` 0 - 4095, a pursuit from 682 on. `NoteFelony` adds a
  felony's points (`gFelonyData.value`, placid or angry by `CopSightData`) when a cop sees it and
  its reoccurrence delay has run out; `CheckPlayerMiscFelonies` notes red lights, broken lights,
  reckless driving and speeding. `AdjustFelony` lowers the rating out of a pursuit and raises it
  in one (with the heading speech 0x8004 + `slu`), calling `RequestRoadBlock` past
  `gCopData.roadblockTrigger`. `ManageCopSightData` doubles the sight distances in a pursuit.
- Pedestrians (`pedest.c`): `pedestrians[64]`, the free slots in `pedestrian_list`. A road map
  cell where `ROADS_GetRoadOrJunction` gives -32768 is pavement; `HandlePedestrians` reads the
  cell and its four neighbours into `pedestrian_roads`, `PingInPedestrians` creates them at the
  traffic ping points (sitting on a chair, `GetRandomChair`, else walking) up to `max_pedestrians`,
  and those 40500 units from the player are removed. A walker heads for the next pavement cell
  (`last_dir` 1 / 2 / 4 / 8 = west / east / north / south; `NewTarget` at each cell centre,
  `CorrectPathPosition` / `CorrectTargetPosition` keep it to one side), turning 128 a frame.
  Motion types (`motion_type_2`) 1 walk, 2 run, 3 dive, 4 sit index `MC_Data` / `mcd_frames`
  (`motion_c.c`); `StepPedestrianAnimation` moves the pedestrian by the frames' root motion,
  `DrawPedestrian` draws the skeleton with `WalkMotionObject` (blending into `motion_type_1` with
  `LerpObjects` while `flags_1` 0x80). `BuildCollisionFrustrum` makes four planes in front of the
  player's car (`gPlayerFrustrum`); a pedestrian inside it, or near a car's horn
  (`CheckHornReaction`), dives aside (`SetupCivJump`), a felony for the player.
  `DisplayPedestrians` draws them one palette (`pedestrian_palette_table`) at a time.
- Motion capture (`motion_c.c`): `InitMotionCaptureData` (from `GameInit`) finds the models
  `PED1BASE` - `PED4BASE` / `PED1HEAD` - `PED4HEAD` and loads `WALK.MCD`, `RUN.MCD`, `DIVE.MCD`,
  `SITTING.MCD` (`RUSSJUMP.MCD` for walking with the RailroadCops cheat) into `MC_Data`; without
  them `pedestrian_data_ok` stays 0 and there are no pedestrians. A `.MCD` file: save mode,
  `num_objects`, frame count, with save mode the bone lengths and `hierarchy_tree`, then per
  frame the root position and a quaternion per object (`LoadMCDFile` keeps the root's height and
  z step). `WalkMotionObject` walks the hierarchy multiplying the rotations down the bones and
  draws each leaf (`num_children` 255) with `DrawObject`.
- Mission scripts (`MScript.c`): `CompileMissionScript` reads a `.dms` line by line; a token is
  a command of `DmsCommands` or a variable of `DmsVars` (each points at the game global it sets,
  e.g. `MAXPLAYERDAMAGE`). Level set up commands (LOADMAP sets `GameLevel`, NIGHT, PLAYERCARTYPE,
  RANDOMCHASE, TRAINING ...) act while compiling; the rest become `MissionList` lines (type, then
  ints or a string) for `ExecuteMissionScript`. Mission 0 (no script) only sets the level's event
  script (`EVENTSCRIPT`, a `.DES`). Every frame `MissionHandler` runs lines until one waits (WAIT,
  WAITFOROBJECTIVECOMPLETE ... return `DMS_ERROR` and stay), then the checkpoint, the timer
  (`DMSHandleTimer`) and the objective (`HandleMissionObjective`: targets, mission window, tail,
  chase, gang car). `mission.objective` 0x7FFF means the objective is done, 0xFF0 failed
  (`failed_reason`), the 0x8000 bit the mission is complete. Texts come from the mission strings:
  `...MessageId` / `...BarId` copy `gMissionStrings[id]` into the string variables
  (`DAMAGEBAR`, `FELONYBAR` are the bar labels, "Not set" until the script runs).
  Trail blazer courses (`pTrailData`, 100 cones) and their recording (F5 / F6, `TRAIL.DAT`) are
  here too.
- Mini cut scenes (`minicutscene.c`): the mission script sets `MCS_StartPosition` /
  `MCS_EndPosition`, up to four bad guys, `MCS_DropOff` (0 they run to the player car's door
  nearer the start, 1 from the door to the end, 2 start to end) and `StartCutScene`.
  `ControlMiniCutScene` (in `Game_Control`, and again from `Game_Draw` to draw) walks them along
  the walk / run motion capture towards `CarDoorPosition`, drawn with the third / fourth
  pedestrian skeleton (`TrimmedWalkMotionObject`); `PlaceCameraCutScene` (cameraview 7) looks at
  the first one. 50 frames after all have arrived the scene ends and the camera cuts back
  behind the car (`CutToCar`).
- Mission effects (`job_fx.c`): the bomb crate in the boot (`crate`, `ControlCrate`) slides with
  the player car's acceleration against friction and the boot walls; hard knocks add damage
  (`ExBoxDamage`, more smoke from 40) and at 80, or with the car on its side, it goes off
  (`TriggerBomb`, mission failed). The explosion (`ex_time` up to 192, `ex_centre`) is five
  spheres (`DrawSphere`: fire, smoke, two flat shock rings) and a screen flash, a lamp
  (`DoExplosionLamps`) and smashes the smashable objects in the cells round it at 10, 30 and 60
  (`SmashSurroundingObjects`). The helicopter (`chopper`, GameLevel 1) lifts off when the
  mission timer reaches `chopper.start` (`ControlChopper`: move, or draw body and rotor).
- Interview and training (`interview.c`): in the car park (GameLevel 4) `DoTrainingMode` runs
  the stopwatch (`HandleStopwatch`, a lap of `gStopwatchAngle`) and `HandleInterviewTasks`
  ticks off the nine tasks on the paper (`TaskStatus`, `CompleteInterviewTask` with its text and
  speech from BANK0.DSS): burnout, handbrake, slaloms and a lap (the car park zones driven
  through, `VisitedZones`, against `SlalomLeftTask` …), 180, 360, reverse 180, quick stop, brake
  turn; four crashes (`giInterviewDamage`) or the time end it. `DrawTasks` draws the paper
  (`ELPAPEL`, other languages copy `PAPER_FR` … over it) with the crossouts and crash crosses.
  In training (`gDoTraining`, `gTrainingMode` 1) and mission chases (GameMode 201)
  `AddToGhostTrail` lays a fading trail of 40 pieces behind the leader car (car_data[65]),
  drawn by `DrawGhostTrail`; `CloseToTrail` counts the player off it.
- Events (`compiler.c`, `event.c`): moving level objects (boats, trains, barriers).
  `CompileEventScript` reads `Scripts\Events\<EVENTSCRIPT>` ("DRIVER EVENT SCRIPT 2.0"):
  `MODEL_NAMES` names event model numbers (`event_model_list`, from the map's event model lump),
  `EVENT_LIST` holds `EVENT_START` … `EVENT_END` blocks and `MULTIEVENT_START n` blocks
  (`MODELS` / `PALETTES` / `VELOCITIES` lists, cloned n - 1 times sharing the commands). The
  trigger lines fill the `EVENT` (`TRIGGER_RADIUS` / `TRIGGER_CELL` reuse its position and node
  fields, see the macros in `event.h`; `TRIGGER_VALUE` = started by id); every other command is
  stored in the current model's command list as its number and parameters (`comm_len` bytes),
  ended by 0. `EventHandler` starts an event when the player enters its radius or cell
  (`NextEvent` resets its models) and runs the started ones (`DoEvent`): each model calls
  `ExecuteEvent[command]` until a handler returns 0 (wait: it leaves `commands` on itself and
  the model resumes there next frame). `DO` … `LOOP` jumps back by a byte offset; `TRIGGER` /
  `TRIGNEXT` start other events. `DisplayEvents` draws them (tilted along the ground with
  `SEND_MESSAGE 1`; the Miami boat smokes). `CarBlockingEvent` slows `MODEL_MOVETO` for a car in
  the model's stop box.
- Replays (`replays.c`): `ReplayMode` 1 records, 2 plays back, 3 is a mission chase (the
  leader, car 1 = `car_data[65]`, plays back its pads). Pads are stored as 5 byte records
  (repeat count, low 16 bits of `controller_bits`, analogue x, y as signed bytes); a pad equal to
  the last one only counts up the repeat (`GetRecordingPad`, `GetRecordedPad`). A user replay is
  `ReplayData\User\<name>.CFG` (`REPLAY_CONFIG`: settings, cheats, the player's start state; the
  car's end position sits in the struct's padding bytes) plus `.PADS` and `.CAMS`
  (`DirectorStorage.c`); recording writes `$TEMP$.*` (`DeInitPadPlayRec`, `SaveReplayConfig`).
  Mission chases keep `.LEADER` / `.PLAYER` (`CAR_DATA` at the start), `.PADS` and `.PINGS`
  (civilians pinged in, `REPLAY_PING`, replayed by `DoControlledPing`) in `ReplayData\Chase`.
  Ghost laps: `StartOfGhostLap` saves the player's state, `EndOfGhostLap` makes a best lap's pads
  the ghost (`car_data[65]`, `PlayGhostLap`).
- Director files (`DirectorStorage.c`): `.CAMS` is a `DRLP` chunk holding `RPDR` (frame count),
  `WYPT` (waypoints) and `RCAM` (cameras, version 2 = `DIRECTOR_CAMERA` as in memory, version 1 =
  an older 60 byte layout); each chunk has a 12 byte id / size / version header. While playing,
  `SaveWaypoints` appends the player's position every 150 frames to `Scripts\Waypoint.tmp`;
  `LoadWaypoints` moves them into `Director` (adding the default chase camera) and deletes the
  file. `InitReplay` fails when neither the waypoint file nor the `.CAMS` file loads.
- Replay director (`Director.c`): `Director.Mode` (`DM_*`) picks what `HandleDirector` does each
  frame: the paused menu (`DM_PLAY`, `DM_DEBOUNCE`), playing the stored cameras (`DM_PLAYPAUSE`,
  `PlayDirectorReplay`), the automatic director (`DM_AUTOCAMERA`, `DM_AUTODIRECTOR`: at each waypoint
  `CheckForNewCamera` stores a random `CreateNewAutoCamera` camera) or editing the work camera
  (`DM_MOVECAMERA`, `DM_ROTATECAMERA`, `DM_EDITCAMERA` moves its start frame). Cameras are a list
  sorted by start frame (`AddDirectorCamera`, `InsertCamera`, `DeleteCamera`); the one in use is the
  last starting at or before the frame. `SetCurrentCamera` / `GetCurrentCamera` copy between a
  `DIRECTOR_CAMERA` and the game camera (`camera.c`; `type` is a `cameraview`). The menu is a
  `DIRECTOR_TEXT` table (`DirectorMainMenu`, `DirectorPauseMenu`): items link left/right through
  `next`, down through `extra`, to their parent through `prev`; keys come from `ReplayDebounce`
  (arrows, Enter/Space, Shift faster, numpad +/- height, End levels, `.` hides the menu).
- Fonts (`frontend/font.c`): glyphs are textures `F1_<code>` … `F4_<code>`; in the frontend
  `DrawFontString` blits into `vscreen`, in game it draws Glide quads. `^^n` in a string switches
  to font n.
- Splash screens (`FadeInScreen`) are different: 24 bit BMPs cut into six 16 bit Glide textures.

## Input, timing, sound

- `input.c`: DirectInput 5. `ReadInput` copies `KeyMap` to `PrevKeyMap` and reads the keyboard
  state (`DIK_*` indices, 0x80 = down); `CheckKey` = pressed this frame. Joystick buttons and stick
  are mapped onto `KeyMap` (in menus onto Return / Escape / arrows).
- `time.c`: up to 4 multimedia timers (`CreateTimer`); `frontend/demo.c` counts seconds with one
  (`demo_timer_count`, the rolling demo starts after 45 s in the main menu).
- `general.c`: `WaitForSync` paces frames with `timeGetTime` and returns the frames elapsed;
  `DebugMalloc` / `DebugFree` wrap every allocation.
- Sound: `sound.c` keeps 20 channels and 80 samples (loaded from `.DSS` lists: wave file +
  `/LOOP /2D /FREQ= /VOL= /PRI=`; sound ids >= 4096 are bank-relative through `SoundIDFixups`).
  Changes are flagged per channel and sent once a frame by `UpdateSound`, which also places the
  listener at the camera. The back end is a `SOUND_CALLBACKS` table filled by
  `eax/directsound.c`: DirectSound 5, each playing channel a duplicate of the sample's static 3D
  buffer, listener / 3D changes deferred and committed in `EndFrame`, EAX 1.0 reverb presets
  through `IKsPropertySet`. `gSoundCompatibility` picks hardware or software buffers and the
  voice manager. Music is CD audio over MCI (`eax/cdaudio.c`, track n + 1 for the game's track n).
- Game sounds (`gsound.c`): `StartGameSounds` locks fixed channels (0 / 1 the player's engine
  idle / under load, 2 horn or siren, 5 / 6 the engines and 7 / 8 the sirens of the two closest
  pursuing cops, 15 speech) and starts them silent; `SoundTasks` (every frame) pitches the
  engines by their revs (`ControlEngineRevs`: four gears in `geard`, `GetEngineRevs`) and plays
  the speech queue. `HandleTaxiSounds` (`taxisnd.c`) watches the player car for the taxi
  passenger: spins, handbrake turns, crashes, damage, hard braking and speed each queue a
  complaint (`ScareTaxi`, at most once per 500 frames each, never twice in a row) and fill
  `OpponentBar`. In game keys are `CheckMiscKeys` (`general.c`: Escape → `PauseGame`, view
  toggle and look keys in `CheckCameraKeys`, - / = viewport size, cheat keys with
  `gAllowCheatKeys`).
- Input uses `DISCL_FOREGROUND`. The original fails `InitDirectInput` when the window is not in
  the foreground at start (RDP, script, attached debugger) and crashes in the first
  `ReadKeyboardInput`; the rebuild ([A]) keeps the devices and re-acquires the keyboard once the
  window gets focus. EAX sound also works through DSOAL (OpenAL Soft's DirectSound).
- Saves (`frontend/config.c`): `SAVES\GAMEn.BIN` (8 slots) = `SAVEDATA_HEADER` (ident, checksum
  from `GetSaveDataHash`) + `GAME_DATA` (name, cities open, mission ladder position, car);
  `SAVES\REPLAYn.BIN` = header + 56 byte name + the `REPLAYDATA\USER\$TEMP$.CFG / .CAMS / .PADS`
  files as chunks (ident, size, data), unpacked back to `$TEMP$` and loaded with `InitReplay`;
  `SAVES\SCORES.HI` is read / written by `scores.c`.

## Stubs

Anything not translated links from the generated `src_rebuild/stubs/stubs.c` (see
[building.md](building.md)). A stub function returns 0 and logs its first call to `stubs.log`;
stub data has the IDA initial bytes. So the first stub in `stubs.log` on a path is the next thing
to translate, and a stub returning 0 can change the flow (e.g. a `d3dgl_SstControl` stub made the
window close itself on `WM_MOVE`).

## Data layout

Each object file's initialised data is one chunk in `.data`; its zero-initialised statics form
per-object blocks too. The big region from about 0xaac000 up (seen up to 0x12fxxxx) is the
linker's common area: uninitialised globals of many files mixed, position proves nothing.
Ownership rules: [translating.md](translating.md), step 2.

Blocks established so far (the `.c` files note them at their data declarations):

| File | Initialised data | Zero / bss blocks |
|---|---|---|
| `frontend/cheats.c` | 0x54bae8 (IDA mark) | 0x57d7ca – 0x57d8c8 (`CurrentJunction` … `CurrentRoad`) |
| `frontend/config.c` | 0x54bcc0 | 0x57d8f8 |
| `frontend/drivinggames.c` | 0x54c970 | 0x5d7a10 |
| `frontend/demo.c` | 0x54c1f4 | 0x5d79f0 |
| `frontend/font.c` | 0x54cad4 | 0x5d7a18 |
| `frontend/frontend.c` | 0x54caf0 | 0x5da0b0 (`tile_buffer`), 0x5fa0b0 |
| `frontend/gvars.c` | 0x54ced8 (key name tables) | |
| `frontend/main.c` | 0x54ecd8 | 0x600bd0 |
| `frontend/newgame.c`, `options.c`, `replay.c`, `scores.c` | 0x54f220, 0x54f2c8, 0x54f3b8, 0x54f4d0 | 0x600c00, 0x600c08, 0x600c10, 0x600c18 |
| `frontend/takearide.c`, `training.c` | 0x5500d8, 0x5502a8 | 0x600c50, 0x600c58 |
| `flat/answerphone.c`, `carselect.c`, `detailoptions.c`, `exit.c` | 0x550370, 0x550440, -, 0x550580 | 0x600c60, 0x600c7c, 0x600c90, 0x600c9c |
| `flat/flat.c`, `frontend.c`, `fudge.c` | 0x550610, 0x55065c, 0x550680 | 0x600ca8, 0x600cc0 |
| `flat/options.c` (gameplayoptions.c, options.c) | 0x550688, 0x5507e0 | 0x600ce4, 0x600cf8 |
| `flat/prescam.c`, `savegame.c` | 0x5508b4, 0x550968 | 0x600d08, 0x6010b0 |
| `mladder/mladder.c` | 0x550b98 (`cur_command`, strings) | 0x601198 – 0x602614 (`missions` … `in_room`) |
| `D3DGlide/glide2x.c` | 0x551068 | 0x602a30 |
| `eax/cdaudio.c` | 0x55335c | 0xb9bb40 (IDA mark) |
| `carnage.c` | 0x5537b0 (IDA mark) | |
| `credits.c` | 0x554a70 (`credits`, strings) | 0xb9f258 – 0xb9f27c (`tsets` … `black`) |
| `checkpoint.c` | 0x553ce0 (word model names) | |
| `Director.c` | 0x561318 (`DirectorPauseMenu`, `DirectorMainMenu`) | 0xd96e10 – 0xd97034 (`bWaypointsStarted` … `bOverlaysDisabled`) |
| `debug.c` | 0x555e8c | |
| `effects.c` | 0x561f90 | |
| `input.c` | 0x564cb0 | |
| `interview.c`, `job_fx.c` | 0x564d50, 0x564f54 | |
| `pres.c` | 0x577d1c | |
| `shell.c` | 0x578240 | 0x57d8c8 (`gScreen` …) |
| `sound.c` | 0x5787dc | 0x5c89e0 |
| `taxisnd.c` | 0x5788d8 | |
| `asmmath.c` | 0x57c330 | |
| `tsd.c` | 0x57c410 | |
| `frontend/cpupoly.c` | | 0x57d9b0 – 0x5c89dd (`vscreen`) |
| `Engine/3dfx/gfxinit.c` | | 0xb9b314 (IDA mark) |
| `Engine/3dfx/tset.c` | | 0xb9b61c (IDA mark) |
