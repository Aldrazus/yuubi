#pragma once

#include "core/util.h"
#include "renderer/vma/image.h"
#include "renderer/device.h"
#include "renderer/immediate_command_executor.h"
#include "pch.h"

namespace yuubi {

class ImageManager : NonCopyableOrMovable {
public:
    ImageManager();

    ImageManager(
        std::shared_ptr<Device> device,
        std::shared_ptr<ImmediateCommandExecutor> immCmdExec
    );

    std::shared_ptr<Image> load(const std::string& filename);

private:
    std::shared_ptr<Device> device_;
    std::unordered_map<std::string, std::weak_ptr<Image>> cache_;
};

std::shared_ptr<Image> ImageManager::load(const std::string& filename) {
    if (!cache_.contains(std::string(filename))) {
        Image* pImage = new Image{};

        std::shared_ptr<Image> spImage(pImage, [this](Image* p) { delete p; });
    }

    return cache_.at(filename).lock();
}
}
