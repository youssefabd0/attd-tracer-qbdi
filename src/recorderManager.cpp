//
// Created by FANGG3 on 25-7-23.
//

#include "recorderManager.h"
#include "traceRecord.h"
#include "logger.h"
#include "textRecorder.h"
#include "stringCache.h"
#include "symbolResolver.h"

// RecorderManager实现
RecorderManager &RecorderManager::getInstance() {
    static RecorderManager instance;
    return instance;
}

bool RecorderManager::initialize(RecorderType type, const char *name) {
    currentType_ = type;
    recorder_ = createRecorder(type);
    if (!stringCache) {
        stringCache = std::make_unique<StringCache>();
    }
    
    // 初始化符号缓存系统
    auto& symbolResolver = GlobalSymbolResolver::getInstance();
    
    symbolResolver.initialize();
    // 与 RecorderManager 共用字符串缓存，并启用模块符号预缓存

    if (recorder_) {
        return recorder_->open(name);
    }
    return false;
}

void RecorderManager::saveSymbols(TraceRecord &record) const {

    // register symbols - 直接从缓存获取
    for (int i = 0; i < 34; ++i) {
        if ((record.regsSet >> i) & 1UL) {
            auto symbolEntry = GlobalSymbolResolver::getInstance().resolveAddress(record.regs[i]);
            if (symbolEntry.isValid) {
                record.symbols[i].offset = record.regs[i] - symbolEntry.symbolAddress;
                if (symbolEntry.symbolNamePoolIndex == 0) {
                    symbolEntry.symbolNamePoolIndex =  stringCache->addString(symbolEntry.symbolName.c_str());
                }
                record.symbols[i].symbolIndex = symbolEntry.symbolNamePoolIndex;
                LOGS("%s %d",symbolEntry.symbolName.c_str(),record.symbols[i].symbolIndex);

            }
        }
    }

    auto symbolEntry = GlobalSymbolResolver::getInstance().resolveAddress(ADDRESS_REG_INDEX);
    if (symbolEntry.isValid) {
        record.symbols[ADDRESS_REG_INDEX].offset = record.address - symbolEntry.symbolAddress;
        if (symbolEntry.symbolNamePoolIndex == 0) {
            symbolEntry.symbolNamePoolIndex =  stringCache->addString(symbolEntry.symbolName.c_str());
        }
        record.symbols[ADDRESS_REG_INDEX].symbolIndex = symbolEntry.symbolNamePoolIndex;
    }
}

void RecorderManager::record(const TraceRecord &record) {
    if (!recorder_) {
        initialize(currentType_, nullptr);
    }
    if (recorder_) {
        if (record.type == RecordType::INSTRUCTION_TRACE){
            saveSymbols((TraceRecord &) record);
        }
        recorder_->record(record);
    }
}
void RecorderManager::recordProcessInfo(const ProcessRecord &record) {
    if (!recorder_) {
        initialize(currentType_, nullptr);
    }
    if (recorder_) {
        recorder_->recordProcessInfo(record);
    }
}



void RecorderManager::setRecorderType(RecorderType type) {
    if (currentType_ != type) {
        if (recorder_) {
            recorder_->close();
        }
        currentType_ = type;
        recorder_ = createRecorder(type);
    }
}

std::unique_ptr<IRecorder> RecorderManager::createRecorder(RecorderType type) {
    switch (type) {
        case RecorderType::TEXT:
            return std::make_unique<TextRecorder>();
        case RecorderType::BINARY:
            // 预留给将来的二进制格式实现
            LOGD("Binary recorder not implemented yet, fallback to text");
            return std::make_unique<TextRecorder>();
        default:
            return std::make_unique<TextRecorder>();
    }
}





