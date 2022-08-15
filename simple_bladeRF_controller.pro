QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

LIBS += -lbladeRF libm.a

SOURCES += \
    Application.cpp \
    BladeRfDeviceController.cpp \
    #BladeRfDevicesManager.cpp \
    BladeRfStream.cpp \
    Other/conversions.c \
    Other/dc_calibration.c \
    RawDataWriter.cpp \
    Types/MissionConfig.cpp \
    Types/RawData.cpp \
    main.cpp

HEADERS += \
    Application.hpp \
    BladeRfDeviceController.hpp \
    #BladeRfDevicesManager.hpp \
    BladeRfStream.hpp \
    Other/conversions.h \
    Other/dc_calibration.h \
    RawDataWriter.hpp \
    Types/BladeRFDeviceState.hpp \
    Types/JsonConfig.hpp \
    Types/MissionConfig.hpp \
    Types/RawData.hpp