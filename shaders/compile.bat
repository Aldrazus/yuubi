glslangvalidator --target-env vulkan1.3 -e main -o mesh.vert.spv mesh.vert
glslangvalidator --target-env vulkan1.3 -e main -o mesh.frag.spv mesh.frag
glslangvalidator --target-env vulkan1.3 -e main -o skybox.vert.spv skybox.vert
glslangvalidator --target-env vulkan1.3 -e main -o skybox.frag.spv skybox.frag
glslangvalidator --target-env vulkan1.3 -e main -o screen_quad.vert.spv screen_quad.vert
glslangvalidator --target-env vulkan1.3 -e main -o screen_quad.frag.spv screen_quad.frag
pause
