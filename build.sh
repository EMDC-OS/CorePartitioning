gcc -pthread -Wall -fPIC -shared -Wl,--no-as-needed -ldl -o CPART_SINGLE.so fdtable.c ipc.c ld_preload.c print.c affinity.c -D__FILEIO__ -DSINGLE
gcc -pthread -Wall -fPIC -shared -Wl,--no-as-needed -ldl -o CPART_CROSS.so fdtable.c ipc.c ld_preload.c print.c affinity.c -D__FILEIO__-DCROSS
gcc -pthread -Wall -fPIC -shared -Wl,--no-as-needed -ldl -o CPART_PER.so fdtable.c ipc.c ld_preload.c print.c affinity.c -D__FILEIO__-DPER
gcc -pthread -Wall -fPIC -shared -Wl,--no-as-needed -ldl -o CPART_APP.so fdtable.c ipc.c ld_preload.c print.c affinity.c -D__FILEIO__-DAPP
# gcc -pthread -Wall -fPIC -shared -Wl,--no-as-needed -ldl -o CPART_SINGLE_DEBUG.so fdtable.c ipc.c ld_preload.c print.c affinity.c -D__FILEIO__ -DSINGLE -D__DEBUG_SYSCALL__ -D__DEBUG_SYSTHREAD__ -D__DEBUG_FDTABLE__ -D__DEBUG_POOL__ -D__DEBUG_AFFINITY__
# gcc -pthread -Wall -fPIC -shared -Wl,--no-as-needed -ldl -o CPART_CROSS_DEBUG.so fdtable.c ipc.c ld_preload.c print.c affinity.c -D__FILEIO__-DCROSS -D__DEBUG_SYSCALL__ -D__DEBUG_SYSTHREAD__ -D__DEBUG_FDTABLE__ -D__DEBUG_POOL__
#gcc -pthread -Wall -fPIC -shared -Wl,--no-as-needed -ldl -o CPART_PER_DEBUG.so fdtable.c ipc.c ld_preload.c print.c affinity.c -D__FILEIO__-DPER -D__DEBUG_SYSCALL__ -D__DEBUG_SYSTHREAD__ -D__DEBUG_FDTABLE__ -D__DEBUG_POOL__
gcc -pthread -Wall -fPIC -shared -Wl,--no-as-needed -ldl -o CPART_SINGLE_affinity.so fdtable.c ipc.c ld_preload.c print.c affinity.c -D__FILEIO__ -D__AFFINITY__ -DSINGLE -D__DEBUG_AFFINITY__
