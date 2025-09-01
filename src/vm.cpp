//
// Created by fang on 23-12-19.
//
#include <iostream>
#include <cassert>
#include <sstream>
#include "QBDI.h"
#include "vm.h"
#include "logger.h"
#include "utils.h"
#include "traceRecord.h"
#include "recorderManager.h"
#include "unwind.h"

using namespace std;
using namespace QBDI;


// Compare Diff Registers From Pre Instruction
inline void calcDiffReg(const QBDI::GPRState *now, extends *extend_data) {
    auto record = extend_data->record;
    auto before = extend_data->gprState;
    for (int i = 0; i < sizeof(QBDI::GPR_NAMES) / sizeof(char *) - 1; ++i) {
        string reg_name = QBDI::GPR_NAMES[i];
        auto before_reg = QBDI_GPR_GET(before, i);
        auto now_reg = QBDI_GPR_GET(now, i);
        if (before_reg == now_reg) {
            if (!extend_data->isFirstRecord) {
                continue;
            }
        }
        record->regsSet |= (1UL << i);
        record->regs[i] = now_reg;
        QBDI_GPR_SET(before, i, now_reg);
    }
    record->pc = now->pc;
    record->regsSet |= (1UL << 33);
//    LOGD("%lx",record->regsSet);
    extend_data->isFirstRecord = false;
}


QBDI::VMAction postInstruction(QBDI::VM *vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState, void *data) {
    auto extend_data = (extends *) data;
    auto record = extend_data->record;
    record->type = RecordType::INSTRUCTION_TRACE;

    const QBDI::InstAnalysis *instAnalysis = vm->getInstAnalysis(
            QBDI::ANALYSIS_INSTRUCTION
//            | QBDI::ANALYSIS_SYMBOL
//            | QBDI::ANALYSIS_DISASSEMBLY
//            | QBDI::ANALYSIS_OPERANDS
    );

    calcDiffReg(gprState,extend_data);
    record->address = instAnalysis->address;
    record->instr = *(uint32_t*)instAnalysis->address;

    if (instAnalysis->isCall) {
        // 仅Trace当前函数
//    vm->removeInstrumentedRange(gprState->pc,gprState->pc+8);
//    vm->addInstrumentedModuleFromAddr(gprState->pc);
    }

    // 使用优化的记录方法
    auto& manager = RecorderManager::getInstance();
    
    // 记录当前记录
    manager.record(*record);
    record->clear();


    return QBDI::VMAction::CONTINUE;
}

inline void memReadAnalyze(const vector<QBDI::MemoryAccess> &memAccVector,extends* extends) {
    for (const auto &i: memAccVector) {
        auto info = i;
        // 使用新的内存访问接口
        if (!extends->record->addMemoryAccess(0, info.size, info.accessAddress, info.value)) {
            LOGW("Memory access buffer full, skipping read access at 0x%lx", info.accessAddress);
            break;
        }
        extends->memAccessCount += 1;
    }
}

inline void memWriteAnalyze(const vector<QBDI::MemoryAccess> &memAccVector,extends* extends) {
    for (const auto &i: memAccVector) {
        auto info = i;
        // 使用新的内存访问接口
        if (!extends->record->addMemoryAccess(1, info.size, info.accessAddress, info.value)) {
            LOGW("Memory access buffer full, skipping write access at 0x%lx", info.accessAddress);
            break;
        }
        extends->memAccessCount += 1;
    }
}

QBDI::VMAction memReadAccess(QBDI::VM *vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState, void *data) {
    auto extend_data = (extends *) data;
    vector<QBDI::MemoryAccess> memAccVector = vm->getInstMemoryAccess();
    memReadAnalyze(memAccVector,extend_data);
    return QBDI::VMAction::CONTINUE;

}

QBDI::VMAction memWriteAccess(QBDI::VM *vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState, void *data) {
    auto extend_data = (extends *) data;
    vector<QBDI::MemoryAccess> memAccVector = vm->getInstMemoryAccess();
    memWriteAnalyze(memAccVector,extend_data);
    return QBDI::VMAction::CONTINUE;
}


void settingRecordName(void *address) {

    GumThreadId tid = gum_process_get_current_thread_id();
    char *private_path = getPrivatePath();
    std::string path = private_path;
    path.append("attd/");

    // 如果文件夹存在，删除它
    char *record_path = const_cast<char *>(path.c_str());
    if (access(record_path, F_OK) != -1) {
        system((std::string("rm -rf ") + path).c_str());
    }
    g_mkdir_with_parents(path.c_str(), 00777);

    path.append("record_");
    path.append(std::to_string(tid));
    Dl_info info;
    stringstream ss;
    if (dladdr(address, &info)) {
        ss << "_";
        ss << std::hex << (uint64_t) info.dli_fbase; // base
        ss << "_";
        ss << std::hex << (uint64_t) address - (uint64_t) info.dli_fbase; // offset
    }
    path.append(ss.str());
    path.append(".txt");

    RecorderManager::getInstance().initialize(RecorderType::TEXT,path.c_str());
}

void settingProcessInfo(void *address) {
    auto processInfo = new ProcessRecord();
    Dl_info info;
    if (dladdr(address, &info)) {
        processInfo->base = (uint64_t )info.dli_fbase;
        processInfo->offset =(uint64_t) address - (uint64_t) info.dli_fbase;
    }
    auto env = get_jni_env_wrapper();
    if (env != nullptr) {
        processInfo->jni_p = (uint64_t) env->functions;
    }
    RecorderManager::getInstance().recordProcessInfo(*processInfo);
}

QBDI::VM vm::init(void *address, bool trace_all) {
    QBDI::VM qvm{};
    auto *data = new extends();
    LOGD("init %p %p", data, data->gprState);
    settingRecordName(address);
    settingProcessInfo(address);

    qvm.recordMemoryAccess(QBDI::MEMORY_READ_WRITE);
    qvm.addMemAccessCB(MemoryAccessType::MEMORY_READ, memReadAccess, data);
    qvm.addMemAccessCB(MemoryAccessType::MEMORY_WRITE, memWriteAccess, data);
    qvm.addCodeCB(QBDI::POSTINST, postInstruction, data);

    if (trace_all){
        qvm.instrumentAllExecutableMaps();
    }else {
        qvm.addInstrumentedModuleFromAddr(reinterpret_cast<QBDI::rword>(address));
    }


    qvm.removeInstrumentedModule("libartbase.so");
    qvm.removeInstrumentedModule("libc.so");
    qvm.removeInstrumentedModule("libc++.so");
    qvm.removeInstrumentedModule("libart.so");

    qvm.removeInstrumentedModule("libEGL.so");
    qvm.removeInstrumentedModule("libGLESv1_CM.so");


    qvm.removeInstrumentedRange((rword) pthread_mutex_init, (rword) pthread_mutex_init + 8);

    qvm.removeInstrumentedRange((rword) pthread_mutex_lock, (rword) pthread_mutex_lock + 8);
    qvm.removeInstrumentedRange((rword) pthread_mutex_unlock, (rword) pthread_mutex_unlock + 8);
    qvm.removeInstrumentedRange((rword) pthread_getspecific, (rword) pthread_getspecific + 8);
    qvm.removeInstrumentedRange((rword) pthread_atfork, (rword) pthread_atfork + 8);
    qvm.removeInstrumentedRange((rword) pthread_create, (rword) pthread_create + 8);
    qvm.removeInstrumentedRange((rword) free, (rword) free + 8);
    qvm.removeInstrumentedRange((rword) malloc, (rword) malloc + 8);
    qvm.removeInstrumentedRange((rword) realloc, (rword) realloc + 8);
    qvm.removeInstrumentedRange((rword) calloc, (rword) calloc + 8);
    qvm.removeInstrumentedRange((rword) memset, (rword) memset + 8);

//    qvm.instrumentAllExecutableMaps();



    return qvm;
}








