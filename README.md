# CLRayTracer
An full featured ray tracer with minimum amount of code possible. using GPU with OpenCL.
C style non modern C++. with least amount of classes but lots of static variables and functions.
!!Warning if you have Nvidia GPU you should have driver version 530 or above(update your drivers please because half is not supported in old versions:) )
Features:
* mesh rendering, texture mapping, phong shading
* BVH, simd optimizations, cpu ray cast
* obj mesh importer from scratch
* FXAA, tone-mapping, Vignette  
<br/> <b/>Still work in progress so I will add these:
- [ ] shadows
- [ ] refraction
- [ ] transculency
- [ ] sperate thread for gameplay + entity system
- [ ] recompile kernel at runtime if needed
