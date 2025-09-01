//
// Created by FANGG3 on 25-7-22.
//
//
// Created by FANGG3 on 2025/4/18.
//
#include "utils.h"
#include <cstring>
#include <sstream>
#include <iomanip>

typedef jint (*JNI_GetCreatedJavaVMs_t)(JavaVM** , jsize , jsize* );
JavaVM* vms;
JNIEnv *env;
jsize vmCount = 0;

jint get_java_vm_wrapper(JavaVM** vms, jsize size, jsize* vm_count){

    void *handle = NULL;
    handle  = dlopen("libnativehelper.so", RTLD_NOLOAD);

    if( handle == NULL){
        handle = dlopen("libnativehelper.so", RTLD_GLOBAL| RTLD_LAZY);
    }else{
        LOGD("get_java_vm_wrapper libnativehelper already load");
    }

    if (handle == NULL) {
        LOGE ("get_java_vm_wrapper libnativehelper dlopen error");
        return JNI_ERR;
    }

    auto JNI_GetCreatedJavaVMs_func= (JNI_GetCreatedJavaVMs_t)dlsym(handle, "JNI_GetCreatedJavaVMs");

    if(JNI_GetCreatedJavaVMs_func == NULL){
        LOGE ("JNI_GetCreatedJavaVMs error");
        dlclose(handle);
        return JNI_ERR;
    }

    jint ret = JNI_GetCreatedJavaVMs_func(vms,size,vm_count);

    if (ret != JNI_OK || vm_count == 0 || vms == NULL) {
        LOGE("Failed to get JVM instance");
        dlclose(handle);
        return JNI_ERR;
    }

    dlclose(handle);
    return JNI_OK;
}

JNIEnv *get_jni_env_wrapper(){
    if (env != nullptr) return env;
    get_java_vm_wrapper(&vms,sizeof(JavaVM),&vmCount);
    if (vmCount > 0){
//        vms->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        vms->AttachCurrentThread(&env, nullptr);
        LOGI("%d %p %d",vmCount,env,env->GetVersion());
        return env;
    }
    return nullptr;
}

std::string toHex(uint64_t value) {
    char buff[32];
    sprintf(buff, "%lx", value);
    return buff;
}

inline std::string byteToHex(unsigned char byte) {
    std::stringstream ss;
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint>(byte);
    return ss.str();
}

std::string readMemToHex(const void *ptr, size_t length) {
    std::string hexString;
    const auto *bytePtr = static_cast<const unsigned char *>(ptr);
    for (size_t i = 0; i < length; ++i) {
        hexString += byteToHex(bytePtr[i]);
    }
    return hexString;
}
