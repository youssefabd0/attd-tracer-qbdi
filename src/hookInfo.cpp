//
// Created by fang on 2024/1/4.
//

#include "hookInfo.h"

static ModuleInfo moduleInfo;

ModuleInfo hookInfo::get_module() {
    return moduleInfo;
}

void hookInfo::set_module(const char* name, size_t base, size_t end) {
    moduleInfo.name = name;
    moduleInfo.end = end;
    moduleInfo.base = base;
}
