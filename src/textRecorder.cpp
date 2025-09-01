//
// Created by fang on 2024/1/4.
//

#include <string>
#include <fstream>
#include <cstdarg>
#include "traceRecord.h"
#include "logger.h"
#include "textRecorder.h"

using namespace std;

#include "QBDI.h"
#include "utils.h"

// TextRecorder类实现
bool TextRecorder::open(const char *name) {
    if (fs_ && fs_->is_open()) {
        return true;
    }
    filepath_ = name;
    fs_ = std::make_unique<std::fstream>(name, std::ios_base::app);
    LOGD("File record path: %s  %p %d", name, fs_.get(), fs_->is_open());
    return fs_->is_open();
}

void TextRecorder::record(const TraceRecord &record) {
    if (!fs_ || !fs_->is_open()) {
        open(nullptr);
    }
    if (fs_ && fs_->is_open()) {
        // register
        for (int i = 0; i < 34; ++i) {
            if ((record.regsSet >> i) & 1UL) {
                *fs_ << QBDI::GPR_NAMES[i]
                     << "=0x" << std::hex << record.regs[i];
                // symbol
                auto regSymbol = getRegSymbol(record, i);
                // 有符号
                if (regSymbol.symbolIndex != 0) {
                    *fs_ << ":" << RecorderManager::getInstance().stringCache->getString(regSymbol.symbolIndex)
                         << "+" << std::hex << regSymbol.offset;;
                }
                *fs_ << ",";
            }
        }

        // memory - 使用新的内存访问接口
        int memAccessCount = record.getMemoryAccessCount();
        for (int i = 0; i < memAccessCount; ++i) {
            const MemAccess *memAcc = record.getMemoryAccess(i);
            if (memAcc) {
                *fs_ << ((memAcc->type == 0) ? "mr" : "mw");
                *fs_ << "=";
                *fs_ << std::hex << memAcc->address;
                *fs_ << ":";
                *fs_ << std::dec << memAcc->value;
                *fs_ << ":";
                *fs_ << memAcc->size;
                *fs_ << ",";
            }
        }

        // instr
        auto s_addr = toHex(record.address);
        *fs_ << "inst="
             << s_addr
             << ":"
             << readMemToHex((void *) record.address, 4);

        auto addrSymbol = getAddressSymbol(record);
        // 有符号
        if (addrSymbol.symbolIndex != 0) {
            *fs_ << ":" << RecorderManager::getInstance().stringCache->getString(addrSymbol.symbolIndex)
                 << "+" << std::hex << addrSymbol.offset;
        }

        *fs_ << std::endl;
    }
}

void TextRecorder::recordProcessInfo(const ProcessRecord &record) {
    *fs_ << "process;";
    *fs_ << "base=" << std::hex << record.base << ";";
    *fs_ << "offset=" << std::hex << record.offset << ";";
    if (record.jni_p != 0){
        *fs_ << "env_functions_p=" << std::hex << record.jni_p << ";";
        *fs_ << "env_functions_data=" << readMemToHex((void*)record.jni_p, sizeof(JNINativeInterface));
    }else {
        LOGE("Can not find JniEnv");
    }
    *fs_ << std::endl;
}

void TextRecorder::close() {
    if (fs_ && fs_->is_open()) {
        fs_->close();
    }
}






