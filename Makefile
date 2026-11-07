TARGET = EBOOT
OBJS = src/main.o src/assets.o
INCDIR =
CFLAGS = -O2 -G0 -Wall -DPSP_VERSION=\"0.7.2\"
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBS = -lpspgum -lpspgu -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspkernel -lpsputility -lm
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = The Cat Is Locked - Beta 0.7.2
PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
