# Portal Parabox

A 3D, OpenGL reimagining of the puzzle game **Patrick's Parabox**, fused with **Portal**-style
recursive teleportation. You push boxes and yourself through rooms that contain *other rooms*,
where each "box room" is reached through a real, rendered portal that looks into the space on the
other side. The project is a course / hobby graphics project: gameplay coverage is intentionally
limited (a handful of levels, no level editor, no sound), but the renderer goes well beyond a
typical toy scene.

> Window title and binary are named `Portal Parabox`.

---

## Highlights

The parts of the project we are most proud of, all backed by working code in this repository.

### 1. Recursive, multi-portal "Portal-in-Portal" rendering

Portals are not faked with a static texture or a simple mirror — each one renders a live view of the
space behind its paired portal, and does so **recursively**, so you can look through a portal that
itself contains another portal:

- A **virtual camera** is reflected through each portal pair to render the world on the other side
  into the portal's own framebuffer.
- **Oblique clip planes** trim away anything behind the destination portal, so the nested view never
  bleeds in front of the portal surface.
- Recursion is rendered **deepest-first**, with a shared depth budget across all portals visible in
  a room and dedicated textures per layer, so nested views stay consistent and don't overwrite one
  another.
- Works with **multiple portals per room**, including portals attached to box rooms that move and
  whose world position is interpolated during a move.

### 2. PBR lighting and shadows, recomputed inside every nested portal view

The shading is a real physically based pipeline, and — importantly — it is **re-evaluated for each
recursion layer**, so the world seen *through* a portal is correctly lit and shadowed rather than
showing a flat snapshot:

- A **Cook–Torrance BRDF** (GGX / Smith / Fresnel) drives a directional key light plus several
  attenuated point lights.
- **Per-room shadow mapping** with PCF soft shadows is run independently for each room, so every
  nested portal view regenerates its own shadows for the space it is showing.
- A **lab-style lighting setup** pairs a warm key light with cool ambient and a grid of ceiling
  lights sized to the room, finished with tone mapping, gamma correction, and subtle rim/back-light
  touches.

### 3. Custom procedural vertex animation

The player block is a "soft cube" deformed entirely on the GPU:

- An **idle squash-and-stretch** gives it a gentle, breathing, jelly-like presence at rest.
- A **directional travel deformation** compresses the trailing face and bulges the sides as it
  moves, for a springy "leaning into the motion" feel relative to the current camera.
- The same deformation is applied in the shadow pass — so the animated silhouette casts a matching
  shadow — and it is **portal-aware**, slicing the player cleanly at the portal surface mid-teleport.

Movement is also interpolated frame-to-frame, with late-phase input buffering so chained moves stay
responsive.

---

## How it works (architecture)

The codebase follows a loose **Model / View / ViewModel** split:

| Layer | Location | Responsibility |
| --- | --- | --- |
| **Model** | `model/` | Pure game logic: grid/sokoban rules, box & box-room pushing, portal traversal, win checking (`GamePlay`), and JSON level loading (`LevelLoader`). |
| **ViewModel** | `viewmodel/game_view_model.hpp` | Thin adapter between input/state and the model. |
| **View** | `view/` | All rendering: PBR scene pass, shadow pass, skybox, recursive portal rendering, UI/buttons, and the soft-cube animation. |
| **App** | `cg_project.cpp` | `GameApplication` owns the GLFW window, the game loop, input throttling, and move-animation timing. |

Levels are plain JSON (`model/levels/*.json`) describing one or more rooms as ASCII grids. A room
flagged `is_box: true` is itself pushable inside its parent room and is entered through a portal.

---

## Requirements

- **OS:** Windows (developed and built with Visual Studio 2022).
- **Toolchain:** MSVC `v143` toolset, configured for the **C++23** language standard, building a
  standard desktop application.
- **GPU/Driver:** An OpenGL **3.3 core profile** capable GPU (the project also uses
  `GL_CLIP_DISTANCE`, which is core in 3.3).

### Bundled dependencies

All third-party libraries are vendored under `extern/` (headers in `extern/include`, prebuilt libs
in `extern/lib`), so there is nothing to install separately:

- **GLFW** — windowing and input.
- **GLAD** — OpenGL function loading (`glad.c`).
- **GLM** — vector/matrix math.
- **Assimp** — loading external `.obj` props (e.g. `resource/modern chair 11 obj.obj`).
- **stb_image** — texture loading; **stb_easy_font** — lightweight UI text.
- **nlohmann/json** (`json.hpp`) — level file parsing.

---

## Building & running

### Visual Studio (recommended / primary)

1. Open `cg_project.sln` in Visual Studio 2022.
2. Select a configuration (e.g. **Debug | x64**) and build.
3. Run from the IDE. The working directory must be the project root so that relative asset paths
   resolve — the app loads shaders from `view/shader/`, textures from `view/assest/`, and levels
   from `model/levels/`.

> Note: `build.bat` is **not** a full-game build — it only compiles the headless gameplay-logic
> test harness in `model/` with `g++`. Use the Visual Studio solution to build the actual 3D game.

---

## How to play

Click **Start** on the title screen to enter the level. The goal of each level is to get the player
to its target tile (and, where present, push every box onto a box target).

### Controls

| Input | Action |
| --- | --- |
| `W` `A` `S` `D` or Arrow keys | Move the player one tile (camera-relative). |
| `U` | Rotate the camera 90° (one way). |
| `I` | Rotate the camera 90° (the other way). |
| Mouse move | Orbit / look. |
| Mouse click | Interact with on-screen UI buttons. |
| `Esc` | Quit. |

Movement is grid-based and rate-limited (Sokoban-style), so taps register as single, discrete
moves; the smooth motion you see between tiles is animation, not free-roam.

### Level format & legend

Each room is a grid of single-character cells:

| Symbol | Meaning |
| --- | --- |
| `#` | Wall |
| `.` | Empty floor |
| `p` | Player start |
| `b` | Box |
| `=` | Player target (goal) |
| `_` | Box target |
| `0`–`9` | A box room (entered via a portal; the digit is its room id) |
| `\|` | Portal-bearing wall segment |

A level is won when the player stands on its target and all box targets are covered.

---

## Project layout

```
cg_project/
├─ cg_project.cpp            # GameApplication: window, game loop, input, animation timing
├─ cg_project.sln            # Visual Studio solution (primary build)
├─ glad.c                    # OpenGL loader
├─ model/                    # Game logic (Model)
│  ├─ include/               #   gameplay & level-loader headers
│  ├─ src/                   #   GamePlay rules, LevelLoader
│  ├─ portal/portal.h        #   Portal: FBO, virtual camera, clip plane, frame geometry
│  └─ levels/*.json          #   Levels as ASCII grids
├─ viewmodel/                # ViewModel adapter
├─ view/                     # Rendering (View)
│  ├─ game_view.hpp          #   Scene/shadow/skybox + recursive portal renderer
│  ├─ gamelight.hpp          #   PBR materials & lab lighting system
│  ├─ shader/                #   PBR, soft-cube animation, portal, shadow, skybox shaders
│  └─ assest/                #   Textures & skybox
├─ resource/                 # External .obj props
└─ extern/                   # Vendored dependencies (GLFW, GLAD, GLM, Assimp, stb, json)
```

---

## Known limitations

- Only a small set of hand-authored levels; no in-game level editor.
- No audio.
- Windows / Visual Studio only as configured; no cross-platform build is provided.
- Portal recursion depth is intentionally shallow for performance.