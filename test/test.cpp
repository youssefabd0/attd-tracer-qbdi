//
// Created by FANGG3 on 25-7-22.
//
#include "attd.h"
#include <cstdio>
#include <iostream>
#include <vector>
#include <memory>
using namespace std::chrono;

__attribute__((noinline)) int fib(int n) {
    auto aaa = (char*)malloc(10);
    memcpy(aaa,"aaaaaaa",3);
    free(aaa);
    if (n < 3) {
        return 1;
    } else {
        return (fib(n - 1) + fib(n - 2));
    }
}

__attribute__((noinline)) int test2(int aa) {
    if (aa > 1000) {
        return aa;
    }
    if (aa < 0) {
        return aa + 100;
    }
    if (aa > 100) {
        return aa - 100;
    }
    return 0;
}

__attribute__((noinline)) int test_printf(int aa) {
    printf("Hello ATTD! %d\n", test2(aa));
    return aa + 100;
}


int main() {
#pragma optimize( "off" )

    auto start = high_resolution_clock::now();
    auto ret = fib(1);
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    printf("%d\n", ret);



    attd_trace((void *) fib);
    auto trace_start = high_resolution_clock::now();
    auto trace_ret = fib(1);
    auto trace_end = high_resolution_clock::now();
    auto trace_duration = duration_cast<microseconds>(trace_end - trace_start);
    printf("%d\n", trace_ret);
    printf("orig:%lld ,trace: %lld \n",duration.count(),trace_duration.count());

    return 0;
}