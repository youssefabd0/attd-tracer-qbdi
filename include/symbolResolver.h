//
// Created by FANGG3 on 25-7-24.
// 自定义符号解析器 - 替代 xDL 依赖的独立符号解析实现
//

#ifndef ATTD_SYMBOLRESOLVER_H
#define ATTD_SYMBOLRESOLVER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <dlfcn.h>
#include <link.h>
#include <elf.h>
#include <sys/mman.h>

// 符号解析结果结构
struct ResolvedSymbol {
    std::string symbolName;    // 符号名称
    uint64_t symbolAddress;    // 符号精确地址  
    uint64_t symbolSize;       // 符号大小
    std::string modulePath;    // 模块路径
    uint64_t moduleBase;       // 模块基地址
    uint64_t symbolNamePoolIndex; // 符号在字符串池中的偏移
    bool isValid;              // 解析是否成功
    bool isDebugSymbol;        // 是否为调试符号
    
    ResolvedSymbol() : symbolAddress(0), symbolSize(0), moduleBase(0), 
                       isValid(false), isDebugSymbol(false),symbolNamePoolIndex(0) {}
};

// ELF 模块信息
struct ModuleInfo {
    std::string path;          // 模块路径
    uint64_t baseAddress;      // 加载基地址
    uint64_t endAddress;       // 结束地址
    void* handle;              // dlopen 句柄
    void* fileData;            // ELF文件映射数据
    size_t fileSize;           // 文件大小
    
    // ELF 符号表信息
    const ElfW(Sym)* dynSymbols;    // 动态符号表
    const char* dynStrings;         // 动态字符串表
    size_t dynSymCount;             // 动态符号数量
    
    // 调试符号表信息
    const ElfW(Sym)* staticSymbols; // 静态符号表
    const char* staticStrings;      // 静态字符串表
    size_t staticSymCount;          // 静态符号数量
    
    ModuleInfo() : baseAddress(0), endAddress(0), handle(nullptr),
                   fileData(nullptr), fileSize(0),
                   dynSymbols(nullptr), dynStrings(nullptr), dynSymCount(0),
                   staticSymbols(nullptr), staticStrings(nullptr), staticSymCount(0) {}
    
    ~ModuleInfo() {
        if (fileData && fileData != MAP_FAILED) {
            munmap(fileData, fileSize);
        }
    }
};

// 自定义符号解析器类
class SymbolResolver {
private:
    // 已加载的模块信息
    std::unordered_map<std::string, std::unique_ptr<ModuleInfo>> modules_;
    std::vector<ModuleInfo*> modulesByAddress_; // 按地址排序的模块列表
    
    // 符号缓存
    std::unordered_map<uint64_t, ResolvedSymbol> symbolCache_;
    
    // 模块发现回调
    static int moduleDiscoveryCallback(struct dl_phdr_info *info, size_t size, void *data);
    
    // ELF 解析方法
    bool parseElfSymbols(ModuleInfo* moduleInfo);
    ModuleInfo* findModuleByAddress(uint64_t address) const;
    ResolvedSymbol resolveInModule(uint64_t address, ModuleInfo* moduleInfo) const;
    ResolvedSymbol searchInSymbolTable(uint64_t address, ModuleInfo* moduleInfo,
                                      const ElfW(Sym)* symbols, const char* strings,
                                      size_t symCount, bool isDebugSymbol) const;
    
    // 地址到符号的查找
    ResolvedSymbol findNearestSymbol(uint64_t address, ModuleInfo* moduleInfo) const;
    
public:
    SymbolResolver();
    ~SymbolResolver();
    
    // 禁用拷贝
    SymbolResolver(const SymbolResolver&) = delete;
    SymbolResolver& operator=(const SymbolResolver&) = delete;
    
    // 初始化 - 发现所有已加载的模块
    bool initialize();
    
    // 刷新模块列表（用于动态加载的库）
    void refreshModules();
    
    // 主要接口 - 解析地址对应的符号
    ResolvedSymbol resolveAddress(uint64_t address);
    
    // 批量符号解析
    std::vector<ResolvedSymbol> resolveAddresses(const std::vector<uint64_t>& addresses);
    
    // 符号查找 - 通过符号名称查找地址
    std::vector<uint64_t> findSymbolAddresses(const std::string& symbolName) const;
    
    // 调试符号特有接口
    std::vector<ResolvedSymbol> findDebugSymbols(const std::string& symbolName) const;
    std::vector<ResolvedSymbol> getAllSymbolsInModule(const std::string& modulePath) const;
    
    // 模块管理
    std::vector<std::string> getAllModules() const;
    ModuleInfo* getModuleInfo(const std::string& modulePath) const;
    
    // 统计信息
    struct ResolverStats {
        size_t moduleCount;
        size_t cachedSymbols;
        size_t totalResolves;
        size_t cacheHits;
        size_t dynamicSymbolCount;  // 动态符号总数
        size_t debugSymbolCount;    // 调试符号总数
        double hitRate() const { 
            return totalResolves > 0 ? (double)cacheHits / totalResolves : 0.0; 
        }
    };
    ResolverStats getStats() const;
    void printStats() const;
    
    // 调试接口
    void dumpModules() const;
    void dumpSymbols(const std::string& modulePath, size_t maxCount = 50) const;
    
private:
    // 统计计数器
    mutable size_t totalResolves_;
    mutable size_t cacheHits_;
};

// 全局符号解析器单例
class GlobalSymbolResolver {
private:
    static std::unique_ptr<SymbolResolver> instance_;
    static std::once_flag once_flag_;
    
public:
    static SymbolResolver& getInstance();
};

#endif //ATTD_SYMBOLRESOLVER_H