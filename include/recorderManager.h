//
// Created by FANGG3 on 25-7-23.
//

#ifndef ATTD_RECORDERMANAGER_H
#define ATTD_RECORDERMANAGER_H
#include "traceRecord.h"
#include "stringCache.h"
#include "symbolResolver.h"
#include <memory>
#include <atomic>


// 抽象记录器接口
class IRecorder {
public:
    virtual ~IRecorder() = default;
    virtual bool open(const char* name) = 0;
    virtual void record(const TraceRecord& record) = 0;
    virtual void recordProcessInfo(const ProcessRecord& record) = 0;

    virtual void close() = 0;
};

// 记录器管理类
class RecorderManager {
public:
    std::unique_ptr<StringCache> stringCache;
    static RecorderManager& getInstance();

    bool initialize(RecorderType type, const char* name);
    void record(const TraceRecord& record);
    void recordProcessInfo(const ProcessRecord& record);

    void setRecorderType(RecorderType type);
    void saveSymbols(TraceRecord &record) const;

private:
    std::unique_ptr<IRecorder> recorder_;
    RecorderType currentType_;
    
    // 统计信息
    RecorderManager() : currentType_(RecorderType::TEXT) {}
    std::unique_ptr<IRecorder> createRecorder(RecorderType type);
};



#endif //ATTD_RECORDERMANAGER_H
