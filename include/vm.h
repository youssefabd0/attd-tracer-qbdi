//
// Created by fang on 23-12-19.
//

#ifndef QBDIRECORDER_VM_H
#define QBDIRECORDER_VM_H

#include "QBDI.h"
#include "traceRecord.h"

using namespace std;

#define STACK_SIZE  0x10000

struct extends {
    int depth;
    bool isFirstRecord;
    QBDI::GPRState *gprState;
    string memRecord;
    string callTarget;
    TraceRecord *record;  // 指向对象池分配的记录
    int memAccessCount;

    extends() {
        depth = 0;
        isFirstRecord = true;
        gprState = new QBDI::GPRState();
        memAccessCount = 0;
        record = new TraceRecord();
    }
    
    // 析构函数
    ~extends() {
        delete gprState;
    }
};


class vm {

public:
    QBDI::VM init(void *address,bool trace_all = false);
private:

};


#endif //QBDIRECORDER_VM_H

