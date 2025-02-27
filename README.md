# Demo application for real-time procedural resurfacing using GPU mesh shaders

GitHub Project: [Andarael/resurfacing](https://github.com/Andarael/resurfacing)

This project demonstrates the use of GPU mesh shaders to enhance object surfaces in real-time. The application is written in C++ and GLSL, utilizing the Vulkan API.

## Tested Configuration
- **Operating System**: Windows
- **GPU**: NVIDIA GPU (RTX Ampere or Ada architecture)
- **Graphics API**: Vulkan, with shaderc.
- **Build System**: MS-Build and CMake
DISCLAIMER, minimal configuration requires a GPU supporting Mesh Shader.
Trying to run this on a non-Windows machine or a non-Nvidia GPU is at your own risk.

## Compilation
- A `.bat` is provided to generate a visual studio solution.
- Shaders are automatically compiled once at build time.
- Note : <!--Shader compilation and  hot-reloading --> Mesh loading are way faster in the release build. 


## ReShade Integration
Reshade package can be used for post-processing effects such as ambiant occlusion. 
Reshade source code needed to be modified to make it compatible with our Vulkan engine.
As of the first public release of our demo application, no reshade integration is provided yet. 
ReShade support is planned for the next build.

<!--
To customize the post-processing effects:
1. Place your `.fx` effect files in the `reshade-shaders/shaders` folder. (e.g. MXAO https://github.com/martymcmodding/iMMERSE/)
2. Launch the application and use the `HOME` key to open the ReShade UI.
3. Enable the desired effects and adjust the settings as needed.-->

## Usage

<!--
### Loading Base Mesh Models

Users can load models in the `.obj` format. The application includes a few sample models in the `models` folder.
Skinned models are supported with `GLTF` file format and are automatically gathered when loading from a `.obj` file (this is because of a limitation of gltf that only allows triangle meshes).

Multiple models can be loaded simultaneously, click the `remove model_name.obj` button to remove a model.

Base mesh can be rendered alone or in addition to the resurfaced model.
-->

### Simple parametric surfaces

In the resurfacing settings in individual models settings, the user can select the type of element to be used for the resurfacing.
Element parametric properties (such as torus radii) can be controlled, as well as orientation and scaling.
Element's maximum resolution can be set independently on u and v directions.
By default, the mapping function F maps an  element by face and an element by vertex.
Normal1 and Normal2 allows for vertex and face issued elements to be oriented in different ways.

### Control cage based surfaces

<!-- Control cages can be loaded from an `.obj` file. -->
When the element type is set to `B-Spline' and a control cage has been loaded, the elements will use the control cage to sample the parametric surface.
Control cages must be provided in a quad grid format, similar to Blender's "Nurbs Surface" object.
The default control cage is a shaped as a scale.

### Pebbles

The pebble mapping function, maps a single element per face of the base mesh.
Roundness, size and extrusion amount can be controlled.
Procedural surface noise can be enabled on the pebbles.

### LOD and Culling

- **LOD (Level of Detail)**: Dynamically reduces element resolution based on screen-space size. Minimum resolution and LOD factors can be adjusted.
- **Culling**: Removes elements outside the camera frustum. Backface culling uses a normal cone, which can be adjusted using the threshold value.

### Performance metrics

Detailed GPU performance metrics are captured per pipeline, using precise GPU counters. Global application performance is displayed in the top bar.

## License

This project is licensed under the **Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0) License**.  

**You may:**  
- Share — Copy and redistribute the material in any medium or format.  
- Adapt — Remix, transform, and build upon the material.  

**Under the following terms:**  
- **Attribution** — You must give appropriate credit and indicate if changes were made.  
- **NonCommercial** — You may not use the material for commercial purposes.  

Full license details: [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/)  
