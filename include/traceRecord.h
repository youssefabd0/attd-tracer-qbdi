//
// Created by fang on 2024/1/4.
//

#ifndef QBDI_FILERECORD_H
#define QBDI_FILERECORD_H
#include "hookUtils.h"
#include <bitset>
#include <string>
#include <memory>
#include <jni.h>
using namespace std;
#define ADDRESS_REG_INDEX 34  // 表示address的regIndex值

// 记录类型枚举
enum class RecordType {
    INSTRUCTION_TRACE,  // 指令跟踪记录
    PROCESS_INFO,       // 进程信息记录  
    CUSTOM             // 自定义记录
};

// 记录器类型枚举
enum class RecorderType {
    TEXT,              // 文本格式
    BINARY            // 二进制格式（预留）
};

// 符号索引条目 - 紧凑存储，包含偏移量信息
struct SymbolEntry {
    uint32_t symbolIndex;    // 字符串池中纯符号名称的索引(1开始，0=无符号)
    uint64_t offset;         // 相对于符号基地址的偏移量
} __attribute__((packed));

struct MemAccess{
    int type; // read 0 ,write 1
    int size;
    uint64_t address;
    uint64_t value;
};



struct ProcessRecord {
    uint64_t base;
    uint64_t offset;
    uint64_t jni_p;
    uint64_t jni_size = sizeof(JNINativeInterface);
};

// 优化的统一记录数据结构 - 内存对齐和缓存友好
// 使用缓存行对齐 (64字节) 提升访问性能
struct alignas(64) TraceRecord {
    // 第一个缓存行：最常访问的基本信息 (64字节)
    uint64_t address;              // 指令地址 (8字节)
    uint64_t pos;                  // 位置信息 (8字节)
    uint64_t regsSet;              // 寄存器掩码 (8字节)
    uint64_t size;                 // 记录大小 (8字节)
    RecordType type;               // 记录类型 (4字节，enum)
    uint32_t instr;                // 指令码 (4字节)
    uint8_t symbolCount;           // 符号数量 (1字节)
    uint8_t padding1[3];           // 填充对齐 (3字节)
    MemAccess memAccess[2];        // 内存访问 - 减少到2个常用的 (2*16=32字节)

    // 第二个缓存行开始：寄存器数据
    union {
        uint64_t regs[34];         // 寄存器数组访问
        struct {
            //按顺序排列
            uint64_t x0, x1, x2, x3, x4, x5, x6, x7;           //
            uint64_t x8, x9, x10, x11, x12, x13, x14, x15;     //
            uint64_t x16, x17, x18;                            //
            uint64_t x19, x20, x21, x22, x23, x24, x25, x26;   //
            uint64_t x27, x28;                                 //
            uint64_t sp, lr, fp;                               //
            uint64_t nzcv;                                     // 标志寄存器
            uint64_t pc;                                       // 程序计数器
        };
    };

    // 符号索引存储 - 稀疏数组，仅存储有符号的寄存器
    SymbolEntry symbols[35];       // 最多35个符号(34寄存器+1地址)
    
    // 额外的内存访问记录 (如果需要超过2个)
    MemAccess extraMemAccess[2];   // 额外的内存访问记录
    
    // 默认构造函数 - 初始化关键字段
    TraceRecord() noexcept 
        : address(0), pos(0), regsSet(0), size(sizeof(TraceRecord))
        , type(RecordType::INSTRUCTION_TRACE), instr(0), symbolCount(0) {
        memset(padding1, 0, sizeof(padding1));
        memset(memAccess, 0, sizeof(memAccess));
        memset(regs, 0, sizeof(regs));
        memset(symbols, 0, sizeof(symbols));
        memset(extraMemAccess, 0, sizeof(extraMemAccess));
    }
    
    // 参数化构造函数
    TraceRecord(RecordType t, uint64_t p, uint64_t addr) noexcept
        : address(addr), pos(p), regsSet(0), size(sizeof(TraceRecord))
        , type(t), instr(0), symbolCount(0) {
        memset(padding1, 0, sizeof(padding1));
        memset(memAccess, 0, sizeof(memAccess));
        memset(regs, 0, sizeof(regs));
        memset(symbols, 0, sizeof(symbols));
        memset(extraMemAccess, 0, sizeof(extraMemAccess));
    }
    
    // 内存访问辅助方法
    bool addMemoryAccess(int type, int size, uint64_t address, uint64_t value) noexcept {
        // 首先尝试主内存访问数组
        for (int i = 0; i < 2; ++i) {
            if (memAccess[i].size == 0) {
                memAccess[i].type = type;
                memAccess[i].size = size;
                memAccess[i].address = address;
                memAccess[i].value = value;
                return true;
            }
        }
        // 然后尝试额外内存访问数组
        for (int i = 0; i < 2; ++i) {
            if (extraMemAccess[i].size == 0) {
                extraMemAccess[i].type = type;
                extraMemAccess[i].size = size;
                extraMemAccess[i].address = address;
                extraMemAccess[i].value = value;
                return true;
            }
        }
        return false; // 已满
    }
    
    // 获取内存访问总数
    int getMemoryAccessCount() const noexcept {
        int count = 0;
        for (int i = 0; i < 2; ++i) {
            if (memAccess[i].size != 0) count++;
        }
        for (int i = 0; i < 2; ++i) {
            if (extraMemAccess[i].size != 0) count++;
        }
        return count;
    }
    
    // 获取指定索引的内存访问
    const MemAccess* getMemoryAccess(int index) const noexcept {
        if (index < 2 && memAccess[index].size != 0) {
            return &memAccess[index];
        } else if (index >= 2 && index < 4 && extraMemAccess[index-2].size != 0) {
            return &extraMemAccess[index-2];
        }
        return nullptr;
    }
    
    // 安全的重置方法 - 清理所有数据但保持对象有效
    void clear() noexcept {
        address = 0;
        pos = 0;
        regsSet = 0;
        size = sizeof(TraceRecord);
        type = RecordType::INSTRUCTION_TRACE;
        instr = 0;
        symbolCount = 0;
        memset(padding1, 0, sizeof(padding1));
        memset(memAccess, 0, sizeof(memAccess));
        memset(regs, 0, sizeof(regs));
        memset(symbols, 0, sizeof(symbols));
        memset(extraMemAccess, 0, sizeof(extraMemAccess));
    }
} __attribute__((packed));  // 确保结构体紧凑排列

// TraceRecord符号管理辅助函数
inline SymbolEntry getAddressSymbol(const TraceRecord &record) {
    return record.symbols[ADDRESS_REG_INDEX];
}

inline SymbolEntry getRegSymbol(const TraceRecord &record, uint8_t regIndex) {
    return  record.symbols[regIndex];
}

inline bool hasSymbol(const TraceRecord &record, uint8_t regIndex) {
    return getRegSymbol(record, regIndex).symbolIndex != 0;
}


// 特殊常数

#endif //QBDI_FILERECORD_H