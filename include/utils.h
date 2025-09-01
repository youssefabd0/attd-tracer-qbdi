//
// Created by FANGG3 on 25-7-22.
//

#ifndef NATIVELIB_UTILS_H
#define NATIVELIB_UTILS_H


#include "jni.h"
#include "dlfcn.h"
#include "logger.h"
#include <vector>

using namespace std;
std::string readMemToHex(const void* ptr, size_t length);
std::string toHex(uint64_t value);
jint get_java_vm_wrapper(JavaVM**, jsize , jsize* );
JNIEnv *get_jni_env_wrapper();


#endif //NATIVELIB_UTILS_H
