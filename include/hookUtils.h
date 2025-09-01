//
// Created by fang on 2024/1/3.
//

#ifndef QBDI_HOOKUTILS_H
#define QBDI_HOOKUTILS_H
#include "frida-gum.h"
#include <dlfcn.h>

#define ANDROID_K 19
#define ANDROID_L 21
#define ANDROID_L2 22
#define ANDROID_M 23
#define ANDROID_N 24
#define ANDROID_N2 25
//Android 8.0
#define ANDROID_O 26
//Android 8.1
#define ANDROID_O2 27
//Android 9.0
#define ANDROID_P 28
//Android 10.0
#define ANDROID_Q 29
//Android 11.0
#define ANDROID_R 30
//Android 12.0
#define ANDROID_S 31

#define HOOK_DEF(ret, func, ...) \
  ret (*orig_##func)(__VA_ARGS__)=nullptr; \
  ret new_##func(__VA_ARGS__)
typedef size_t Size;
struct MapAddresInfo {
    /**
     * 函数的符号
     */
    char *sym = nullptr;
    /**
     * 函数在文件路径
     */
    char *fname = nullptr;

    /**
     * 所在函数的基地址
     */
    size_t sym_base = 0;
    /**
     * 文件基地址
     */
    size_t fbase = 0;

    /**
     * 传入地址,相对于so的偏移
     */
    size_t offset = 0;
};

struct MapItemInfo {
    /**
     * item开始位置
     */
    size_t start;

    /**
     * item结束位置
     */
    size_t end;
};

MapItemInfo getSoBaseAddress(const char *name);
char* getAppName();
char* getPrivatePath();



class hookUtils{
    public:
        static void hookLinker(GumInvocationCallback on_leave);
        static void gum_attach(void *addr,GumInvocationCallback on_enter, GumInvocationCallback on_leave,void* func_data= nullptr);
        static void gum_replace(void* addr,void* replace,void** backup);


private:
        static GumInvocationListener * obtainListener();
        static void empty_onEnter(GumInvocationContext * context,
                                  gpointer user_data);
        static void empty_onLeave(GumInvocationContext * context,
                                  gpointer user_data);

};

#endif //QBDI_HOOKUTILS_H
