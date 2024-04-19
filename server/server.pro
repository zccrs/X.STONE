QT       += core gui remoteobjects core-private fb_support-private
equals(QT_MAJOR_VERSION, 5): QT += xkbcommon_support-private

CONFIG += link_pkgconfig
PKGCONFIG += libinput libudev

REPC_SOURCE = ../protocols/kernel.rep

HEADERS += \
    compositor.h \
    input.h \
    output.h \
    protocol.h

SOURCES += \
    compositor.cpp \
    input.cpp \
    main.cpp \
    output.cpp \
    protocol.cpp

