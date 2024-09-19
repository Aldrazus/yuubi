glslangvalidator --target-env vulkan1.3 -e main -o shader.vert.spv shader.vert
glslangvalidator --target-env vulkan1.3 -e main -o shader.frag.spv shader.frag

glslangvalidator --target-env vulkan1.3 -e main -o mesh.vert.spv mesh.vert
glslangvalidator --target-env vulkan1.3 -e main -o mesh.frag.spv mesh.frag
pause
