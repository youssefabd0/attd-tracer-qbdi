//
// Created by FANGG3 on 25-7-29.
//

#ifndef ATTD_STRINGCACHE_H
#define ATTD_STRINGCACHE_H

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include <mutex>

/**
 * 简单的内存字符串缓存，使用索引映射
 * 替代复杂的文件持久化字符串池
 */
class StringCache {
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, uint32_t> stringToIndex_;
    std::vector<std::string> indexToString_;
    uint32_t nextIndex_;

public:
    StringCache();
    ~StringCache() = default;

    // 添加字符串并返回索引，如果已存在则返回现有索引
    uint32_t addString(const char* string);
    
    // 根据索引获取字符串指针
    char* getString(uint32_t index);
    
    // 获取总字符串数量
    uint32_t getTotalStrings() const;
    
    // 清空缓存
    void clear();
};

#endif //ATTD_STRINGCACHE_H