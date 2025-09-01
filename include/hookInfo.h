//
// Created by fang on 2024/1/4.
//

#ifndef QBDI_HOOKINFO_H
#define QBDI_HOOKINFO_H

#include <cstddef>

struct ModuleInfo{
    const char* name;
    size_t base;
    size_t end;
};

class hookInfo {
public:
    // 获取单例实例的静态方法
    static hookInfo& getInstance() {
        static hookInfo instance; // Guaranteed to be destroyed. Instantiated on first use.
        return instance;
    }
    ModuleInfo get_module();
    void set_module(const char* name ,size_t base,size_t size);

private:
    // 隐藏构造函数，防止外部代码创建实例
    hookInfo() {}

    // 删除复制构造函数和赋值运算符
    hookInfo(const hookInfo&) = delete;
    hookInfo& operator=(const hookInfo&) = delete;
};

#endif //QBDI_HOOKINFO_H
