# ATTD-Tracer
该项目是[ATTD-GUI](https://github.com/FANGG3/ATTD-GUI)的Trace采集器,基于QBDI和Frida的Trace.

## trace输出
trace文件输出路径: `/data/data/$packagename/attd/record_$pid_$soBase_$funcAddr.txt`

> 通过`adb log -s FANGG3`打印日志,判断trace状态:
> ```
> 09-01 11:06:59.487  9846  9846 D FANGG3  : load attd ok !!
> 09-01 11:06:59.489  9846  9846 D FANGG3  : hooking 0x5b83a78b54
> 09-01 11:06:59.490  9846  9846 D FANGG3  : begin
> ...
> 09-01 11:07:02.350  9846  9846 D FANGG3  : end
> ```

## 使用说明
- 导入并关闭selinux
```shell
adb push libattd.so /data/local/tmp/
adb shell su -c "setenforce 0" 
```
> 如果你需要在开启selinux的情况下进行trace,可以使用以下两种方式:
> 1. 使用基于ptrace或zgisk的注入器,它们会帮你处理好selinux.🚀
> 2. 使用frida 
>    - 拷贝至应用私有目录`cp /data/local/tmp/libattd.so /data/data/$packagename/`
>    - 修改脚本中的`attd_lib`为实际路径

### Frida使用
```javascript

    function trace(addr,trace_all) {
        let m = Process.findModuleByName("libattd.so")
        if (!m) {
            console.error("load so fail")
            return
        }
        let f = m.findExportByName("attd_trace")
        if (!f) {
            console.error("load so fail, no sym")
            return
        }
        console.log(f)
        let trace_function = new NativeFunction(f, "void", ["pointer","int"])
        trace_function(addr,trace_all)
        console.log("hooked")

    }
     function main() {
         let p_dlopen = Module.findExportByName(null,"dlopen")
         let dlopen = new NativeFunction(p_dlopen, "pointer", ["pointer",  "int"])
         let attd_path = "/data/local/tmp/libattd.so"
         dlopen(Memory.allocUtf8String(attd_path),2)
         let mod = Process.findModuleByName("libtest.so") // find target lib
         // trace(address, isTraceAllCallee)
         trace(mod.base.add(0x193b0),1) 
    }
```


## 编译
```shell
git clone https://github.com/FANGG3/attd-tracer-qbdi
mkdir build
cd build
cmake .. -DANDROID_NDK="$YOUR_NDK_PATH"
make
```
> 💡 release中可以下载已编译好的库