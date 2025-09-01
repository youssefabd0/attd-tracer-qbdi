//
// Created by fang on 2024/1/3.
//
#include <climits>
#include <cstring>
#include <cstdio>
#include <asm-generic/unistd.h>
#include <unistd.h>
#include <unwind.h>
#include <vector>
#include <string>
#include <sys/system_properties.h>


#include "logger.h"
#include "hookUtils.h"
using namespace std;



MapItemInfo getSoBaseAddress(const char *name) {
    MapItemInfo info{0};
    if (name == nullptr) {
        return info;
    }
    size_t start = 0;
    size_t end = 0;
    bool isFirst = true;
    size_t len = 0;
    char buffer[PATH_MAX];
    memset(buffer, 0, PATH_MAX);


    //找不到用原始文件
    FILE *fp = fopen("/proc/self/maps", "r");

    if (fp == nullptr) {
        LOGD("找不到用原始文件");
        return info;
    }

    char *line = nullptr;
    while (getline(&line, &len, fp) != -1) {
        if (line != nullptr && strstr(line, name)) {
            sscanf(line, "%lx-%lx", &start, &end);
            if (isFirst) {
                info.start = start;
                isFirst = false;
            }
        }
    }
    info.end = end;
    syscall(__NR_close, fp);
    return info;
}


char* appName = nullptr;
char* getAppName(){
    if (appName != NULL){
        LOGD("get appName %s",appName);
        return appName;
    }
    FILE* f = fopen("/proc/self/cmdline","r");
    size_t len;
    char* line = nullptr;
    if(getline(&line,&len,f)==-1){
        perror("can't get app name");
    }
    appName = line;
    LOGD("get appName %s",appName);
    return appName;
}

char privatePath[PATH_MAX];
char* getPrivatePath(){
    if (privatePath[0] != 0 ){
        return privatePath;
    }
    sprintf(privatePath,"%s%s%s","/data/data/",getAppName(),"/");
    return privatePath;
}
void hookUtils::gum_replace(void* addr, void* replace, void** backup){
    GumInterceptor *interceptor;
    interceptor = gum_interceptor_obtain();
    gum_interceptor_begin_transaction(interceptor);
    auto ret =gum_interceptor_replace(interceptor,addr,replace, interceptor,backup);
    gum_interceptor_end_transaction(interceptor);
    gum_interceptor_flush(interceptor);
    LOGI("HOOK REPLACE: %d",ret);
}



void hookUtils::gum_attach(void* addr, GumInvocationCallback on_enter, GumInvocationCallback on_leave, void* func_data ){
    GumInterceptor *interceptor;
    interceptor = gum_interceptor_obtain();
    gum_interceptor_begin_transaction(interceptor);
    if (on_enter == nullptr) on_enter = hookUtils::empty_onEnter;
    if (on_leave == nullptr) on_leave = hookUtils::empty_onLeave;
    void* data = malloc(sizeof(void*));
    auto listener = gum_make_call_listener(on_enter,on_leave, data, nullptr);

    gum_interceptor_attach(interceptor,addr,listener, func_data);
    gum_interceptor_end_transaction(interceptor);

}


static int SDK_INT = -1;

int get_sdk_level() {
    if (SDK_INT > 0) {
        return SDK_INT;
    }
    char sdk[PROP_VALUE_MAX] = {0};
    __system_property_get("ro.build.version.sdk", sdk);
    SDK_INT = atoi(sdk);
    return SDK_INT;
}
char *getLinkerPath() {
    char *linker;

    //get_sdk_level 是dlfc自己实现的方法
    //android_get_device_api_level是系统方法,低版本的NDK没有此方法。
#if defined(__aarch64__)
    if (get_sdk_level() >= ANDROID_R) {
        linker = (char*)"/apex/com.android.runtime/bin/linker64";
    } else if (get_sdk_level() >= ANDROID_Q) {
        linker = (char*)"/apex/com.android.runtime/bin/linker64";
    } else {
        linker = (char*)"/system/bin/linker64";
    }
#else
    if (get_sdk_level() >= ANDROID_R) {
        linker = (char*)"/apex/com.android.runtime/bin/linker";
    } else if (get_sdk_level() >= ANDROID_Q) {
        linker = (char*)"/apex/com.android.runtime/bin/linker";
    } else {
        linker = (char*)"/system/bin/linker";
    }
#endif

    return linker;
}

void listen_dlopen_onEnter(GumInvocationContext * context,
                           gpointer user_data){
    //LOGD("call dlopen %s",(char*)context->cpu_context->x[0]);
    auto d = (void**)user_data;
    *d = (char*)context->cpu_context->x[0];
    LOGD("listen_dlopen_onEnter %p %s",user_data,(char*)context->cpu_context->x[0]);

}

void listen_dlopen_onLeave(GumInvocationContext * context,
                           gpointer user_data){
    auto d = (void**)user_data;
    LOGD("listen_dlopen_onLeave %s",(char*)*d);
}

HOOK_DEF(void,call_array,const char* array_name,
         void* functions,
         size_t count,
         bool reverse,
         const char* realpath){

    LOGD("call init function: %s %s ",array_name,realpath);
    orig_call_array(array_name,functions,count,reverse,realpath);

}
void hookCallArray(){
    auto sym = gum_module_find_symbol_by_name("linker64","__dl__ZL10call_arrayIPFviPPcS1_EEvPKcPT_mbS5_");
    if (sym){
        LOGD("hooking call array %zx",sym);
        hookUtils::gum_replace((void*)sym, (void*)new_call_array, (void**)&orig_call_array);
    }


}
void hookUtils::hookLinker(GumInvocationCallback on_leave) {
    string dlopen_symbols[] = {
            "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv",
            "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv",
            "__dl__ZL10dlopen_extPKciPK17android_dlextinfoPv",
            "__dl__Z20__android_dlopen_extPKciPK17android_dlextinfoPKv",
            "__dl___loader_android_dlopen_ext",
            "__dl__Z9do_dlopenPKciPK17android_dlextinfo",
            "__dl__Z8__dlopenPKciPKv",
            "__dl___loader_dlopen",
            "__dl_dlopen"
    };
    if (on_leave == nullptr){on_leave = listen_dlopen_onLeave;}
//    hookUtils::gum_attach((void*)dlopen,listen_dlopen_onEnter,on_leave);
    GumAddress addr ;
    for (const string& sym:dlopen_symbols) {
         addr = gum_module_find_symbol_by_name("linker64",sym.c_str());
        if (addr != 0) {
            LOGD("find dlopen  %s at %lx",sym.c_str(),addr);
            hookUtils::gum_attach((void*)addr, listen_dlopen_onEnter, on_leave);
        }
    }
    hookCallArray();

}

void hookUtils::empty_onEnter(GumInvocationContext * context,
                              gpointer user_data) {

//    LOGD("call empty_onEnter");

}

void hookUtils::empty_onLeave(GumInvocationContext * context,
                              gpointer user_data) {
//    LOGD("call empty_onLeave");

}
