//
// Created by FANGG3 on 25-7-29.
//

#include "stringCache.h"
#include "logger.h"

StringCache::StringCache() : nextIndex_(1) {
    // 索引0保留为无效索引
    indexToString_.emplace_back("");
}

uint32_t StringCache::addString(const char* string) {
    if (!string) {
        return 0;
    }
    LOGS("%d",getTotalStrings())
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string str(string);
    auto it = stringToIndex_.find(str);
    if (it != stringToIndex_.end()) {
        // 字符串已存在，返回现有索引
        LOGS("FINDED")
        return it->second;
    }
    
    // 新字符串，分配新索引
    uint32_t index = nextIndex_++;
    stringToIndex_[str] = index;
    indexToString_.push_back(str);
    
    return index;
}

char* StringCache::getString(uint32_t index) {
    if (index == 0) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    if (index >= indexToString_.size()) {
        LOGW("Invalid string index: %u", index);
        return nullptr;
    }
    
    return const_cast<char*>(indexToString_[index].c_str());
}

uint32_t StringCache::getTotalStrings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nextIndex_ - 1;
}

void StringCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    stringToIndex_.clear();
    indexToString_.clear();
    indexToString_.push_back(""); // 保留索引0
    nextIndex_ = 1;
    LOGD("String cache cleared");
}