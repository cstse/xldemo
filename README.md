编译

gcc libxsdn_channel_capi.so xsdn_channel_capi_demo.c -o xsdn_channel_capi_demo

依赖：

# ldd xsdn_channel_capi_demo
        linux-vdso.so.1 =>  (0x00007fff9c37b000)
        libxsdn_channel_capi.so => ./libxsdn_channel_capi.so (0x00007f02d63ae000)
        libc.so.6 => /lib64/libc.so.6 (0x00007f02d5feb000)
        libdl.so.2 => /lib64/libdl.so.2 (0x00007f02d5de7000)
        libstdc++.so.6 => /lib64/libstdc++.so.6 (0x00007f02d5adf000)
        libm.so.6 => /lib64/libm.so.6 (0x00007f02d57dd000)
        libgcc_s.so.1 => /lib64/libgcc_s.so.1 (0x00007f02d55c7000)
        libpthread.so.0 => /lib64/libpthread.so.0 (0x00007f02d53ab000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f02d711e000)
        
        
xsdn_channel_capi_demo 使用

一端启动服务

./xsdn_channel_capi_demo -s -pid server111

一端启动主动连接

./xsdn_channel_capi_demo -pid client111 -dst server111