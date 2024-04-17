QT       += core gui network core-private xkbcommon_support-private fb_support-private
CONFIG += link_pkgconfig
PKGCONFIG += libinput libudev

HEADERS += \
    compositor.h \
    input.h \
    output.h

SOURCES += \
    compositor.cpp \
    input.cpp \
    main.cpp \
    output.cpp

