VALVE_LINUX_LIB_START_EXE=-lm -ldl -lpthread
VALVE_LINUX_LIB_END_EXE=-lstdc++

VALVE_LINUX_LIB_START_SO=
VALVE_LINUX_LIB_END_SO=-lstdc++ -weak_library /usr/lib/libcurl.dylib
