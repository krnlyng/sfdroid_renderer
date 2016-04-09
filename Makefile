OUT         := sfdroid
SRC         := main.cpp uinput.cpp renderer.cpp sfdroid_funcs.cpp sfconnection.cpp utility.cpp sensorconnection.cpp
OBJ         := $(patsubst %.c, %.o, $(filter %.c, $(SRC)))
OBJ         += $(patsubst %.cpp, %.o, $(filter %.cpp, $(SRC)))
DEP         := $(OBJ:.o=.d)

CFLAGS      := -Wall -Werror -std=gnu99
CXXFLAGS    := -Wall -Werror -std=c++11
LDFLAGS     :=
LDLIBS      :=

CFLAGS  += `pkg-config --cflags sdl2`
CFLAGS  += `pkg-config --cflags glesv1_cm`
CFLAGS  += `pkg-config --cflags egl`
CFLAGS  += `pkg-config --cflags wayland-egl`
CFLAGS  += -I/usr/include/android
CFLAGS  += -I/usr/lib/droid-devel/droid-headers
CFLAGS  += `pkg-config --cflags sensord-qt5` `pkg-config --cflags Qt5DBus` `pkg-config --cflags Qt5Network` -D_REENTRANT -fPIE -DQT_NO_DEBUG -DQT_DBUS_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB 

CXXFLAGS    += `pkg-config --cflags sdl2`
CXXFLAGS    += `pkg-config --cflags glesv1_cm`
CXXFLAGS    += `pkg-config --cflags egl`
CXXFLAGS    += `pkg-config --cflags wayland-egl`
CXXFLAGS    += -I/usr/include/android
CXXFLAGS    += -I/usr/lib/droid-devel/droid-headers
CXXFLAGS    += `pkg-config --cflags sensord-qt5` `pkg-config --cflags Qt5DBus` `pkg-config --cflags Qt5Network` -D_REENTRANT -fPIE -DQT_NO_DEBUG -DQT_DBUS_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB 

LDLIBS  += `pkg-config --libs sdl2`
LDLIBS  += `pkg-config --libs glesv1_cm`
LDLIBS  += `pkg-config --libs egl`
LDLIBS  += `pkg-config --libs wayland-egl`
LDLIBS  += -lhardware
LDLIBS  += `pkg-config --libs sensord-qt5` `pkg-config --libs Qt5DBus` `pkg-config --cflags Qt5Network`

DEBUG       ?= 0
VERBOSE     ?= 0

ifeq ($(DEBUG),1)
	CFLAGS += -O0 -g3 -ggdb -pg -DDEBUG=1
	CXXFLAGS += -O0 -g3 -ggdb -pg -DDEBUG=1
	LDFLAGS += -pg
endif

ifeq ($(VERBOSE),1)
	MSG := @true
	CMD :=
else
	MSG := @echo
	CMD := @
endif

.PHONY: release clean

release: CFLAGS += -O3
release: CXXFLAGS += -O3
release: $(OUT)

clean:
	$(MSG) -e "\tCLEAN\t"
	$(CMD)$(RM) $(OBJ) $(DEP) $(OUT)

$(OUT): $(OBJ)
	$(MSG) -e "\tLINK\t$@"
	$(CMD)$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c %.d
	$(MSG) -e "\tCC\t$@"
	$(CMD)$(CC) $(CFLAGS) -c $< -o $@

%.d: %.c
	$(MSG) -e "\tDEP\t$@"
	$(CMD)$(CC) $(CFLAGS) -MF $@ -MM $<

%.o: %.cpp %.d
	$(MSG) -e "\tCXX\t$@"
	$(CMD)$(CXX) $(CXXFLAGS) -c $< -o $@

%.d: %.cpp
	$(MSG) -e "\tDEP\t$@"
	$(CMD)$(CXX) $(CXXFLAGS) -MF $@ -MM $<

ifneq ($(MAKECMDGOALS),clean)
-include $(DEP)
endif

