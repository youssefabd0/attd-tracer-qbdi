//
// Created by FANGG3 on 25-7-24.
// 自定义符号解析器实现 - 替代 xDL 依赖的独立符号解析实现
//

#include "symbolResolver.h"
#include "logger.h"
#include <algorithm>
#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// 静态成员初始化
std::unique_ptr<SymbolResolver> GlobalSymbolResolver::instance_ = nullptr;
std::once_flag GlobalSymbolResolver::once_flag_;

SymbolResolver& GlobalSymbolResolver::getInstance() {
    std::call_once(once_flag_, []() {
        instance_ = std::make_unique<SymbolResolver>();
    });
    return *instance_;
}

SymbolResolver::SymbolResolver() : totalResolves_(0), cacheHits_(0) {
    LOGD("SymbolResolver created");
}

SymbolResolver::~SymbolResolver() {
    // 清理 dlopen 句柄和文件映射
    for (auto& pair : modules_) {
        if (pair.second->handle) {
            dlclose(pair.second->handle);
        }
        // 文件映射由 ModuleInfo 析构函数处理
    }
    LOGD("SymbolResolver destroyed");
}

bool SymbolResolver::initialize() {
    LOGD("Initializing symbol resolver...");
    
    // 使用 dl_iterate_phdr 发现所有已加载的模块
    int result = dl_iterate_phdr(moduleDiscoveryCallback, this);
    LOGD("dl_iterate_phdr returned %d, found %zu modules", result, modules_.size());
    
    // 即使 dl_iterate_phdr 返回0，只要找到了模块就继续
    if (modules_.empty()) {
        LOGE("No modules discovered");
        return false;
    }
    
    // 按地址排序模块列表以优化查找
    modulesByAddress_.clear();
    modulesByAddress_.reserve(modules_.size());
    
    for (auto& pair : modules_) {
        modulesByAddress_.push_back(pair.second.get());
    }
    
    std::sort(modulesByAddress_.begin(), modulesByAddress_.end(),
              [](const ModuleInfo* a, const ModuleInfo* b) {
                  return a->baseAddress < b->baseAddress;
              });
    
    LOGD("Symbol resolver initialized with %zu modules", modules_.size());
    return true;
}

int SymbolResolver::moduleDiscoveryCallback(struct dl_phdr_info *info, size_t size, void *data) {
    SymbolResolver* resolver = static_cast<SymbolResolver*>(data);
    
    // 跳过没有名称的模块
    if (!info->dlpi_name || strlen(info->dlpi_name) == 0) {
        return 0;
    }
    
    // 计算模块的地址范围
    uint64_t baseAddr = info->dlpi_addr;
    uint64_t endAddr = baseAddr;
    
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];
        if (phdr->p_type == PT_LOAD) {
            uint64_t segmentEnd = baseAddr + phdr->p_vaddr + phdr->p_memsz;
            if (segmentEnd > endAddr) {
                endAddr = segmentEnd;
            }
        }
    }
    
    // 创建模块信息
    auto moduleInfo = std::make_unique<ModuleInfo>();
    moduleInfo->path = info->dlpi_name;
    moduleInfo->baseAddress = baseAddr;
    moduleInfo->endAddress = endAddr;
    
    // 尝试使用 dlopen 获取句柄（用于符号查找）
    moduleInfo->handle = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD);
    
    // 解析 ELF 符号表
    resolver->parseElfSymbols(moduleInfo.get());
    
    resolver->modules_[info->dlpi_name] = std::move(moduleInfo);
    
    LOGD("Discovered module: %s [0x%lx-0x%lx]", info->dlpi_name, baseAddr, endAddr);
    return 0;
}

bool SymbolResolver::parseElfSymbols(ModuleInfo* moduleInfo) {
    if (!moduleInfo) return false;
    
    // 尝试打开 ELF 文件
    int fd = open(moduleInfo->path.c_str(), O_RDONLY);
    if (fd < 0) {
        LOGW("Cannot open ELF file: %s", moduleInfo->path.c_str());
        return false;
    }
    
    // 获取文件大小
    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize <= 0) {
        close(fd);
        return false;
    }
    
    // 映射文件到内存
    void* fileData = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (fileData == MAP_FAILED) {
        LOGW("Failed to mmap ELF file: %s", moduleInfo->path.c_str());
        return false;
    }
    
    // 保存文件映射信息供后续使用
    moduleInfo->fileData = fileData;
    moduleInfo->fileSize = fileSize;
    
    // 解析 ELF 头
    const ElfW(Ehdr)* ehdr = static_cast<const ElfW(Ehdr)*>(fileData);
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        munmap(fileData, fileSize);
        return false;
    }
    
    // 查找节头表
    const ElfW(Shdr)* shdrs = reinterpret_cast<const ElfW(Shdr)*>(
        static_cast<const char*>(fileData) + ehdr->e_shoff);
    
    // 查找字符串表节
    const ElfW(Shdr)* strtabShdr = &shdrs[ehdr->e_shstrndx];
    const char* strtab = static_cast<const char*>(fileData) + strtabShdr->sh_offset;
    
    // 查找符号表和字符串表
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        const ElfW(Shdr)* shdr = &shdrs[i];
        const char* sectionName = strtab + shdr->sh_name;
        
        if (shdr->sh_type == SHT_DYNSYM && strcmp(sectionName, ".dynsym") == 0) {
            // 找到动态符号表
            moduleInfo->dynSymbols = reinterpret_cast<const ElfW(Sym)*>(
                static_cast<const char*>(fileData) + shdr->sh_offset);
            moduleInfo->dynSymCount = shdr->sh_size / sizeof(ElfW(Sym));
            
            // 查找对应的字符串表
            if (shdr->sh_link < ehdr->e_shnum) {
                const ElfW(Shdr)* strShdr = &shdrs[shdr->sh_link];
                moduleInfo->dynStrings = static_cast<const char*>(fileData) + strShdr->sh_offset;
            }
        }
        else if (shdr->sh_type == SHT_SYMTAB && strcmp(sectionName, ".symtab") == 0) {
            // 找到静态符号表（调试符号）
            moduleInfo->staticSymbols = reinterpret_cast<const ElfW(Sym)*>(
                static_cast<const char*>(fileData) + shdr->sh_offset);
            moduleInfo->staticSymCount = shdr->sh_size / sizeof(ElfW(Sym));
            
            // 查找对应的静态字符串表
            if (shdr->sh_link < ehdr->e_shnum) {
                const ElfW(Shdr)* strShdr = &shdrs[shdr->sh_link];
                moduleInfo->staticStrings = static_cast<const char*>(fileData) + strShdr->sh_offset;
            }
        }
    }
    
    bool hasDynSymbols = (moduleInfo->dynSymbols != nullptr && moduleInfo->dynStrings != nullptr);
    bool hasStaticSymbols = (moduleInfo->staticSymbols != nullptr && moduleInfo->staticStrings != nullptr);
    
    if (hasDynSymbols || hasStaticSymbols) {
        LOGD("Parsed ELF symbols for %s: %zu dynamic, %zu static symbols", 
             moduleInfo->path.c_str(), moduleInfo->dynSymCount, moduleInfo->staticSymCount);
        return true;
    } else {
        // 没有找到任何符号表，释放文件映射
        munmap(fileData, fileSize);
        moduleInfo->fileData = nullptr;
        moduleInfo->fileSize = 0;
        return false;
    }
}

void SymbolResolver::refreshModules() {
    // 清理现有模块
    for (auto& pair : modules_) {
        if (pair.second->handle) {
            dlclose(pair.second->handle);
        }
    }
    
    modules_.clear();
    modulesByAddress_.clear();
    symbolCache_.clear();
    
    // 重新初始化
    initialize();
}

ModuleInfo* SymbolResolver::findModuleByAddress(uint64_t address) const {
    // 使用二分查找优化性能
    auto it = std::upper_bound(modulesByAddress_.begin(), modulesByAddress_.end(), address,
                               [](uint64_t addr, const ModuleInfo* module) {
                                   return addr < module->baseAddress;
                               });
    
    if (it != modulesByAddress_.begin()) {
        --it;
        ModuleInfo* module = *it;
        if (address >= module->baseAddress && address < module->endAddress) {
            return module;
        }
    }
    
    return nullptr;
}

ResolvedSymbol SymbolResolver::searchInSymbolTable(uint64_t address, ModuleInfo* moduleInfo,
                                                  const ElfW(Sym)* symbols, const char* strings,
                                                  size_t symCount, bool isDebugSymbol) const {
    ResolvedSymbol result;
    
    if (!symbols || !strings || symCount == 0) {
        return result;
    }
    
    // 查找最匹配的符号
    const ElfW(Sym)* bestMatch = nullptr;
    uint64_t bestDistance = UINT64_MAX;
    
    for (size_t i = 0; i < symCount; ++i) {
        const ElfW(Sym)* sym = &symbols[i];
        
        // 跳过无效符号
        if (sym->st_name == 0 || ELF64_ST_TYPE(sym->st_info) == STT_NOTYPE) {
            continue;
        }
        
        // 跳过 TLS 符号（与 xdl_sym_is_match 一致）
        if (ELF64_ST_TYPE(sym->st_info) == STT_TLS) {
            continue;
        }
        
        uint64_t symAddr = moduleInfo->baseAddress + sym->st_value;
        
        // 精确的符号窗口匹配（与 xdl_sym_is_match 一致）
        // 只有当地址严格在符号范围内时才匹配
        if (sym->st_size > 0 && address >= symAddr && address < (symAddr + sym->st_size)) {
            uint64_t distance = address - symAddr;
            if (distance < bestDistance) {
                bestMatch = sym;
                bestDistance = distance;
            }
        }
    }
    
    if (bestMatch) {
        result.symbolName = strings + bestMatch->st_name;
        result.symbolAddress = moduleInfo->baseAddress + bestMatch->st_value;
        result.symbolSize = bestMatch->st_size;
        result.modulePath = moduleInfo->path;
        result.moduleBase = moduleInfo->baseAddress;
        result.isValid = true;
        result.isDebugSymbol = isDebugSymbol;
    }
    
    return result;
}

ResolvedSymbol SymbolResolver::resolveInModule(uint64_t address, ModuleInfo* moduleInfo) const {
    ResolvedSymbol result;
    
    if (!moduleInfo) {
        return result;
    }
    
    // 首先在动态符号表中查找
    ResolvedSymbol dynResult = searchInSymbolTable(
        address, moduleInfo, 
        moduleInfo->dynSymbols, moduleInfo->dynStrings, moduleInfo->dynSymCount, false);
    
    // 如果动态符号表中找到精确匹配，直接返回
    if (dynResult.isValid && dynResult.symbolAddress == address) {
        return dynResult;
    }
    
    // 在调试符号表中查找
    ResolvedSymbol staticResult = searchInSymbolTable(
        address, moduleInfo,
        moduleInfo->staticSymbols, moduleInfo->staticStrings, moduleInfo->staticSymCount, true);
    
    // 选择最优结果：优先调试符号（更详细），其次动态符号
    if (staticResult.isValid) {
        return staticResult;
    }
    return result;
}

ResolvedSymbol SymbolResolver::resolveAddress(uint64_t address) {
    totalResolves_++;
    
    // 检查缓存
    auto cacheIt = symbolCache_.find(address);
    if (cacheIt != symbolCache_.end()) {
        cacheHits_++;
        return cacheIt->second;
    }
    
    // 查找包含此地址的模块
    ModuleInfo* moduleInfo = findModuleByAddress(address);
    if (!moduleInfo) {
        return ResolvedSymbol(); // 返回无效的符号
    }
    
    // 在模块中解析符号
    ResolvedSymbol result = resolveInModule(address, moduleInfo);
    
    // 缓存结果（无论是否成功）
    symbolCache_[address] = result;
    
    return result;
}

std::vector<ResolvedSymbol> SymbolResolver::resolveAddresses(const std::vector<uint64_t>& addresses) {
    std::vector<ResolvedSymbol> results;
    results.reserve(addresses.size());
    
    for (uint64_t address : addresses) {
        results.push_back(resolveAddress(address));
    }
    
    return results;
}

std::vector<uint64_t> SymbolResolver::findSymbolAddresses(const std::string& symbolName) const {
    std::vector<uint64_t> addresses;
    
    for (const auto& pair : modules_) {
        const ModuleInfo* moduleInfo = pair.second.get();
        
        // 搜索动态符号表
        if (moduleInfo->dynSymbols && moduleInfo->dynStrings) {
            for (size_t i = 0; i < moduleInfo->dynSymCount; ++i) {
                const ElfW(Sym)* sym = &moduleInfo->dynSymbols[i];
                
                if (sym->st_name != 0) {
                    const char* name = moduleInfo->dynStrings + sym->st_name;
                    if (symbolName == name) {
                        uint64_t address = moduleInfo->baseAddress + sym->st_value;
                        addresses.push_back(address);
                    }
                }
            }
        }
        
        // 搜索调试符号表
        if (moduleInfo->staticSymbols && moduleInfo->staticStrings) {
            for (size_t i = 0; i < moduleInfo->staticSymCount; ++i) {
                const ElfW(Sym)* sym = &moduleInfo->staticSymbols[i];
                
                if (sym->st_name != 0) {
                    const char* name = moduleInfo->staticStrings + sym->st_name;
                    if (symbolName == name) {
                        uint64_t address = moduleInfo->baseAddress + sym->st_value;
                        // 避免重复地址
                        if (std::find(addresses.begin(), addresses.end(), address) == addresses.end()) {
                            addresses.push_back(address);
                        }
                    }
                }
            }
        }
    }
    
    return addresses;
}

std::vector<ResolvedSymbol> SymbolResolver::findDebugSymbols(const std::string& symbolName) const {
    std::vector<ResolvedSymbol> symbols;
    
    for (const auto& pair : modules_) {
        const ModuleInfo* moduleInfo = pair.second.get();
        
        // 只搜索调试符号表
        if (moduleInfo->staticSymbols && moduleInfo->staticStrings) {
            for (size_t i = 0; i < moduleInfo->staticSymCount; ++i) {
                const ElfW(Sym)* sym = &moduleInfo->staticSymbols[i];
                
                if (sym->st_name != 0) {
                    const char* name = moduleInfo->staticStrings + sym->st_name;
                    if (symbolName == name) {
                        ResolvedSymbol result;
                        result.symbolName = name;
                        result.symbolAddress = moduleInfo->baseAddress + sym->st_value;
                        result.symbolSize = sym->st_size;
                        result.modulePath = moduleInfo->path;
                        result.moduleBase = moduleInfo->baseAddress;
                        result.isValid = true;
                        result.isDebugSymbol = true;
                        symbols.push_back(result);
                    }
                }
            }
        }
    }
    
    return symbols;
}

std::vector<ResolvedSymbol> SymbolResolver::getAllSymbolsInModule(const std::string& modulePath) const {
    std::vector<ResolvedSymbol> symbols;
    
    auto it = modules_.find(modulePath);
    if (it == modules_.end()) {
        return symbols;
    }
    
    const ModuleInfo* moduleInfo = it->second.get();
    
    // 收集动态符号
    if (moduleInfo->dynSymbols && moduleInfo->dynStrings) {
        for (size_t i = 0; i < moduleInfo->dynSymCount; ++i) {
            const ElfW(Sym)* sym = &moduleInfo->dynSymbols[i];
            
            if (sym->st_name != 0 && ELF64_ST_TYPE(sym->st_info) != STT_NOTYPE) {
                ResolvedSymbol result;
                result.symbolName = moduleInfo->dynStrings + sym->st_name;
                result.symbolAddress = moduleInfo->baseAddress + sym->st_value;
                result.symbolSize = sym->st_size;
                result.modulePath = moduleInfo->path;
                result.moduleBase = moduleInfo->baseAddress;
                result.isValid = true;
                result.isDebugSymbol = false;
                symbols.push_back(result);
            }
        }
    }
    
    // 收集调试符号
    if (moduleInfo->staticSymbols && moduleInfo->staticStrings) {
        for (size_t i = 0; i < moduleInfo->staticSymCount; ++i) {
            const ElfW(Sym)* sym = &moduleInfo->staticSymbols[i];
            
            if (sym->st_name != 0 && ELF64_ST_TYPE(sym->st_info) != STT_NOTYPE) {
                ResolvedSymbol result;
                result.symbolName = moduleInfo->staticStrings + sym->st_name;
                result.symbolAddress = moduleInfo->baseAddress + sym->st_value;
                result.symbolSize = sym->st_size;
                result.modulePath = moduleInfo->path;
                result.moduleBase = moduleInfo->baseAddress;
                result.isValid = true;
                result.isDebugSymbol = true;
                symbols.push_back(result);
            }
        }
    }
    
    return symbols;
}

std::vector<std::string> SymbolResolver::getAllModules() const {
    std::vector<std::string> moduleNames;
    moduleNames.reserve(modules_.size());
    
    for (const auto& pair : modules_) {
        moduleNames.push_back(pair.first);
    }
    
    return moduleNames;
}

ModuleInfo* SymbolResolver::getModuleInfo(const std::string& modulePath) const {
    auto it = modules_.find(modulePath);
    return (it != modules_.end()) ? it->second.get() : nullptr;
}

SymbolResolver::ResolverStats SymbolResolver::getStats() const {
    ResolverStats stats;
    stats.moduleCount = modules_.size();
    stats.cachedSymbols = symbolCache_.size();
    stats.totalResolves = totalResolves_;
    stats.cacheHits = cacheHits_;
    
    // 统计符号数量
    stats.dynamicSymbolCount = 0;
    stats.debugSymbolCount = 0;
    
    for (const auto& pair : modules_) {
        const ModuleInfo* moduleInfo = pair.second.get();
        stats.dynamicSymbolCount += moduleInfo->dynSymCount;
        stats.debugSymbolCount += moduleInfo->staticSymCount;
    }
    
    return stats;
}

void SymbolResolver::printStats() const {
    auto stats = getStats();
    
    LOGD("=== Symbol Resolver Statistics ===");
    LOGD("Modules: %zu", stats.moduleCount);
    LOGD("Dynamic Symbols: %zu", stats.dynamicSymbolCount);
    LOGD("Debug Symbols: %zu", stats.debugSymbolCount);
    LOGD("Cached Symbols: %zu", stats.cachedSymbols);
    LOGD("Total Resolves: %zu", stats.totalResolves);
    LOGD("Cache Hits: %zu", stats.cacheHits);
    LOGD("Hit Rate: %.2f%%", stats.hitRate() * 100);
}

void SymbolResolver::dumpModules() const {
    LOGD("=== Loaded Modules ===");
    
    for (const auto& pair : modules_) {
        const ModuleInfo* moduleInfo = pair.second.get();
        LOGD("%s: [0x%lx-0x%lx] dyn: %zu, debug: %zu", 
             moduleInfo->path.c_str(), 
             moduleInfo->baseAddress, 
             moduleInfo->endAddress,
             moduleInfo->dynSymCount,
             moduleInfo->staticSymCount);
    }
}

void SymbolResolver::dumpSymbols(const std::string& modulePath, size_t maxCount) const {
    auto it = modules_.find(modulePath);
    if (it == modules_.end()) {
        LOGW("Module not found: %s", modulePath.c_str());
        return;
    }
    
    const ModuleInfo* moduleInfo = it->second.get();
    
    LOGD("=== Symbols for %s (max %zu) ===", modulePath.c_str(), maxCount);
    
    size_t count = 0;
    
    // 显示动态符号
    if (moduleInfo->dynSymbols && moduleInfo->dynStrings) {
        LOGD("--- Dynamic Symbols ---");
        for (size_t i = 0; i < moduleInfo->dynSymCount && count < maxCount/2; ++i) {
            const ElfW(Sym)* sym = &moduleInfo->dynSymbols[i];
            
            if (sym->st_name != 0 && ELF64_ST_TYPE(sym->st_info) != STT_NOTYPE) {
                const char* name = moduleInfo->dynStrings + sym->st_name;
                uint64_t address = moduleInfo->baseAddress + sym->st_value;
                
                LOGD("0x%lx: %s (size: %llu) [DYN]", address, name, (unsigned long long)sym->st_size);
                count++;
            }
        }
    }
    
    // 显示调试符号
    if (moduleInfo->staticSymbols && moduleInfo->staticStrings && count < maxCount) {
        LOGD("--- Debug Symbols ---");
        for (size_t i = 0; i < moduleInfo->staticSymCount && count < maxCount; ++i) {
            const ElfW(Sym)* sym = &moduleInfo->staticSymbols[i];
            
            if (sym->st_name != 0 && ELF64_ST_TYPE(sym->st_info) != STT_NOTYPE) {
                const char* name = moduleInfo->staticStrings + sym->st_name;
                uint64_t address = moduleInfo->baseAddress + sym->st_value;
                
                LOGD("0x%lx: %s (size: %llu) [DEBUG]", address, name, (unsigned long long)sym->st_size);
                count++;
            }
        }
    }
    
    if (count >= maxCount) {
        LOGD("... and more symbols");
    }
}