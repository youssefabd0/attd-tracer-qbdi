//
// Created by FANGG3 on 25-7-23.
//

#ifndef ATTD_TEXTRECORDER_H
#define ATTD_TEXTRECORDER_H

#include "recorderManager.h"
#include <memory>
#include <fstream>

// TextRecorder类声明
class TextRecorder : public IRecorder {
public:
    bool open(const char* name) override;
    void record(const TraceRecord& record) override;
    void recordProcessInfo(const ProcessRecord& record) override;
    void close() override;
private:
    std::unique_ptr<std::fstream> fs_;
    std::string filepath_;
};

#endif //ATTD_TEXTRECORDER_H