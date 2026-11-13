# Credits and licences

## Code
All game code in `Source/SmashKarts` was written for this project.

## Art, effects and sound
So far the game only uses **Unreal Engine content that ships with the engine** (allowed in Unreal Engine games under
the Unreal Engine EULA):

| What | Engine asset |
|---|---|
| Kart body, loot boxes, arena blocks, projectiles, traps, maces | `/Engine/BasicShapes` (Cube, Sphere, Cone, Cylinder) |
| Colours on those shapes | `/Engine/BasicShapes/BasicShapeMaterial` (and material instances of it in `Content/SmashKarts/Arena/Materials`) |
| Arena floor grid | `/Engine/OpenWorldTemplate/LandscapeMaterial/MI_ProcGrid` |
| Ice block, auras, shield, range ring | `/Engine/EngineDebugMaterials/M_SimpleTranslucent`, `M_SimpleUnlitTranslucent` |
| Tesla beams | `/Engine/EngineMaterials/EmissiveMeshMaterial` |
| Explosions and bursts | copies of the Niagara templates `SimpleExplosion` and `RadialBurst` (`Content/SmashKarts/FX`) |
| "?" signs, UI text | engine default font (Roboto) |

No AI-generated assets are used.

## Planned third-party packs (not in the project yet)
When these are added, list each one here with its source URL and licence:

| Pack | Author | Licence | Used for |
|---|---|---|---|
| Racing Kit | Kenney (kenney.nl) | CC0 1.0 | — |
| Prototype Textures | Kenney | CC0 1.0 | — |
| Particle Pack | Kenney | CC0 1.0 | — |
| Impact Sounds, Interface Sounds, Sci-fi Sounds | Kenney | CC0 1.0 | — |
