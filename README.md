# Teidore Armory Desktop

High-fidelity gun configurator desktop app built with Unreal Engine 5. Functions like the Teidore Armory website but with native desktop rendering quality — studio lighting, PBR materials, smooth interaction.

## Setup Instructions

### Prerequisites
- **Unreal Engine 5.5** (install via Epic Games Launcher)
- **Visual Studio 2022** with "Game development with C++" workload

### Step 1: Open the Project
1. Double-click `TeidoreArmory.uproject` — it will open in UE5 Editor
2. If prompted about modules, click **Yes** to rebuild
3. Wait for shaders to compile (first time takes a few minutes)

### Step 2: Create the Input Action Assets
The GunViewerPawn needs 2 Input Action assets and 1 Input Mapping Context. Create them in the Content Browser:

#### Create Input Actions
Right-click in Content Browser → **Input** → **Input Action** for each:

| Asset Name | Value Type | Description |
|---|---|---|
| `IA_Rotate` | **Axis2D (Vector2D)** | Mouse delta for rotation |
| `IA_ToggleLock` | **Digital (bool)** | Toggle rotation lock |

#### Create Input Mapping Context
Right-click → **Input** → **Input Mapping Context** → name it `IMC_GunViewer`

Open `IMC_GunViewer` and add these mappings:

1. **IA_Rotate** → Add key `Mouse 2D` → Add modifier **Negate** on Y axis
   - Add trigger: **Down** (modifier: Left Mouse Button)
   
2. **IA_ToggleLock** → Add key `L` (or whatever key you prefer)

### Step 3: Create the Map
1. **File** → **New Level** → choose **Empty Level**
2. Save as `Content/Maps/GunViewer`

### Step 4: Set Up the Scene
1. **Place your gun meshes** — drag your Glock 17 static meshes into the level, positioned at the world origin (0, 0, 0)
2. Add **Rect Lights** around the gun for studio lighting
3. Set the **Skybox/Background** to solid dark (or add a dark HDRI for subtle reflections)

### Step 5: Configure the GunViewerPawn
1. In Content Browser, right-click `GunViewerPawn` (C++ class) → **Create Blueprint Class Based on This** → name it `BP_GunViewerPawn`
2. Open `BP_GunViewerPawn`:
   - In the Details panel, assign the Input Actions you created:
     - `Default Mapping Context` → `IMC_GunViewer`
     - `Rotate Action` → `IA_Rotate`
     - `Toggle Lock Action` → `IA_ToggleLock`
   - Tweak rotation/framing parameters if needed
3. **Attach gun meshes**: In the World Outliner, drag your gun mesh actors onto the `GunPivot` component of the pawn (making them children)

### Step 6: Set the GameMode
1. **Edit** → **Project Settings** → **Maps & Modes**
2. Set **Default GameMode** to `TeidoreArmoryGameMode`
3. This automatically spawns `GunViewerPawn` when you hit Play

### Step 7: Test
Press **Play** in the editor:
- **Left-click + drag** → rotate the gun (auto-returns to default when you let go)
- **Camera auto-frames** the gun based on its dimensions (no manual zoom needed)
- **L key** (or your chosen key) → toggle rotation lock (gun stays where you leave it)
- Call `RecalculateFraming()` from Blueprint when you add/remove/swap parts — camera smoothly adjusts
- Call `SetRotationLocked(true/false)` from a UI button via Blueprint

### Packaging as .exe
1. **Edit** → **Project Settings** → **Packaging** → set Build Configuration to **Shipping**
2. **Platforms** → **Windows** → **Package Project**
3. Choose an output folder → UE5 builds a standalone `.exe`

## Project Structure
```
TeidoreArmory/
├── TeidoreArmory.uproject          # UE5 project file
├── Config/
│   ├── DefaultEngine.ini           # Renderer & map settings
│   ├── DefaultGame.ini             # Project metadata
│   └── DefaultInput.ini            # Enhanced Input setup
├── Source/
│   ├── TeidoreArmory.Target.cs     # Game build target
│   ├── TeidoreArmoryEditor.Target.cs  # Editor build target
│   └── TeidoreArmory/
│       ├── TeidoreArmory.Build.cs  # Module dependencies
│       ├── TeidoreArmory.h/.cpp    # Module entry point
│       ├── GunViewerPawn.h/.cpp    # Core viewer interaction
│       └── TeidoreArmoryGameMode.h/.cpp  # Sets default pawn
└── Content/                        # Created by UE5 Editor
    └── Maps/
        └── GunViewer.umap
```
