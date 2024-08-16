#pragma once

#include "core/util.h"
namespace yuubi {

// TODO: is this copyable or movable?
class Resource {
public:
protected:
    Resource(const std::string& name) : name_(name) {}
private:
    std::string name_;
};
    
}
