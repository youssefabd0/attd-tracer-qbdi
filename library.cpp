#include <jni.h>
#include <string>
#include "logger.h"
#include "hookUtils.h"
#include "QBDI.h"
#include "vm.h"



void syn_reg_gum(GumCpuContext *cpu, QBDI::GPRState *state, bool F2Q) {

    if (F2Q) {
        for (int i = 0; i < 29; i++) {
            QBDI_GPR_SET(state, i, cpu->x[i]);
        }
        state->lr = cpu->lr;
        state->sp = cpu->sp;
        state->x29 = cpu->fp;
        state->nzcv = cpu->nzcv;

    } else {
        for (int i = 0; i < 29; i++) {
            cpu->x[i] = QBDI_GPR_GET(state, i);
        }
        cpu->fp = state->x29;
        cpu->lr = state->lr;
        cpu->sp = state->sp;
        cpu->nzcv = state->nzcv;
    }

}

bool isTraceAll = false;

// hook
HOOK_DEF(QBDI::rword, gum_handle) {
    LOGS("begin");
    clock_t start,end;
    start = clock();
    auto context = gum_interceptor_get_current_invocation();
    auto interceptor = (GumInterceptor *) gum_invocation_context_get_replacement_data(context);
    gum_interceptor_revert(interceptor, context->function);
    gum_interceptor_flush(interceptor);
    auto vm_ = new vm();
    auto qvm = vm_->init(context->function,isTraceAll);
    auto state = qvm.getGPRState();
    syn_reg_gum(context->cpu_context, state, true);
    uint8_t *fakestack;
    QBDI::allocateVirtualStack(state, STACK_SIZE, &fakestack);
    QBDI::rword ret;
    qvm.switchStackAndCall(&ret, (QBDI::rword) context->function);
    syn_reg_gum(context->cpu_context, state, false);
    end = clock();
    LOGS("time: %f",(double)(end-start)/CLOCKS_PER_SEC);
    LOGS("end");
    return ret;
}


// export
extern "C" void _init(void) {
    gum_init_embedded();
}
extern "C" {
    __attribute__((visibility ("default"))) void attd(void *target_addr) {
        LOGD("hooking %p", target_addr);
        hookUtils::gum_replace(target_addr, (void *) new_gum_handle, (void **) (&orig_gum_handle));
    }

    __attribute__((visibility ("default"))) void attd_trace(void *addr,bool trace_all) {
        isTraceAll = trace_all;
        attd(addr);
    }
    void attd_call(void *target_addr) {
        LOGS("attd_call start %p", target_addr);
        uint8_t *fakestack;
        auto vm_ = new vm();
        auto qvm = vm_->init(target_addr);
        auto state = qvm.getGPRState();
        QBDI::allocateVirtualStack(state, STACK_SIZE, &fakestack);
        QBDI::rword ret;
        qvm.call(&ret, (QBDI::rword) target_addr);
        LOGS("attd_call end %p", target_addr);

    }
}

__unused __attribute__((constructor)) void init_main() {
    LOGD("load attd ok !!");
}