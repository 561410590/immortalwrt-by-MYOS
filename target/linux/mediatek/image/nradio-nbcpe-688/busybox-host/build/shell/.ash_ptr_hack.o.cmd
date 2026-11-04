cmd_shell/ash_ptr_hack.o := gcc -Wp,-MD,shell/.ash_ptr_hack.o.d  -std=gnu99 -Iinclude -Ilibbb -Iinclude2 -I/vol3/1000/immortalwrt6.6x/immortalwrt-mt798x-6.6/target/linux/mediatek/image/nradio-nbcpe-688/busybox-host/source/busybox-1.36.1/include -I/vol3/1000/immortalwrt6.6x/immortalwrt-mt798x-6.6/target/linux/mediatek/image/nradio-nbcpe-688/busybox-host/source/busybox-1.36.1/libbb -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG  -DBB_VER='"1.36.1"' -I/vol3/1000/immortalwrt6.6x/immortalwrt-mt798x-6.6/target/linux/mediatek/image/nradio-nbcpe-688/busybox-host/source/busybox-1.36.1/shell -Ishell -malign-data=abi -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Oz  -DKBUILD_BASENAME='"ash_ptr_hack"'  -DKBUILD_MODNAME='"ash_ptr_hack"' -c -o shell/ash_ptr_hack.o /vol3/1000/immortalwrt6.6x/immortalwrt-mt798x-6.6/target/linux/mediatek/image/nradio-nbcpe-688/busybox-host/source/busybox-1.36.1/shell/ash_ptr_hack.c

deps_shell/ash_ptr_hack.o := \
  /vol3/1000/immortalwrt6.6x/immortalwrt-mt798x-6.6/target/linux/mediatek/image/nradio-nbcpe-688/busybox-host/source/busybox-1.36.1/shell/ash_ptr_hack.c \
  /usr/include/stdc-predef.h \

shell/ash_ptr_hack.o: $(deps_shell/ash_ptr_hack.o)

$(deps_shell/ash_ptr_hack.o):
