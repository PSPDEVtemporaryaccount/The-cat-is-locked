TARGET = EBOOT
OBJS = src/main.o
INCDIR =
CFLAGS = -O2 -G0 -Wall -DPSP_VERSION=\\\"0.1.1\\\"
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBS = -lpspgum -lpspgu -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspkernel -lm
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = The Cat Is Locked - Beta 0.1.1
# No custom ICON0.PNG yet: don't pass a missing file to pack-pbp.
PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
