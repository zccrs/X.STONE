QT       += core gui network xkbcommon_support-private
CONFIG += link_pkgconfig
PKGCONFIG += libinput libudev

HEADERS += \
    input.h \
    output.h

SOURCES += \
    input.cpp \
    main.cpp \
    output.cpp

