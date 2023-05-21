#!/bin/sh
mkdir ./disc
sh-elf-objcopy -R .stack -O binary ./bin/KallistiOS/RSDKv5 ./bin/KallistiOS/RSDKv5.bin
$KOS_BASE/utils/scramble/scramble ./bin/KallistiOS/RSDKv5.bin ./disc/1ST_READ.BIN
$KOS_BASE/utils/makeip/makeip ./disc/IP.BIN
mkisofs -V MANIA -G ./disc/IP.BIN -joliet -rock -l -o ./disc.iso ./disc
cdi4dc disc.iso disc.cdi -d
