glslangvalidator --target-env vulkan1.3 -e main -o mesh.vert.spv mesh.vert
glslangvalidator --target-env vulkan1.3 -e main -o mesh.frag.spv mesh.frag
glslangvalidator --target-env vulkan1.3 -e main -o skybox.vert.spv skybox.vert
glslangvalidator --target-env vulkan1.3 -e main -o skybox.frag.spv skybox.frag
glslangvalidator --target-env vulkan1.3 -e main -o screen_quad.vert.spv screen_quad.vert
glslangvalidator --target-env vulkan1.3 -e main -o screen_quad.frag.spv screen_quad.frag
glslangvalidator --target-env vulkan1.3 -e main -o depth.vert.spv depth.vert
glslangvalidator --target-env vulkan1.3 -e main -o depth.frag.spv depth.frag
glslangvalidator --target-env vulkan1.3 -e main -o ssao.frag.spv ssao.frag
glslangvalidator --target-env vulkan1.3 -e main -o cubemap.vert.spv cubemap.vert
glslangvalidator --target-env vulkan1.3 -e main -o cubemap.frag.spv cubemap.frag
glslangvalidator --target-env vulkan1.3 -e main -o irradiance.frag.spv irradiance.frag
glslangvalidator --target-env vulkan1.3 -e main -o prefilter.frag.spv prefilter.frag
glslangvalidator --target-env vulkan1.3 -e main -o brdflut.frag.spv brdflut.frag

pause
