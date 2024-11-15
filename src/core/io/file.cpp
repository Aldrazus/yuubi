#include "file.h"
#include "pch.h"

namespace yuubi {

    std::vector<char> readFile(std::string_view filename) {
        std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);

        UB_INFO("Opening file: {}", filename);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        const size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

}
