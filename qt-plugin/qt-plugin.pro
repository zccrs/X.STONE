PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = X.STONE
!equals(TARGET, $$QT_DEFAULT_QPA_PLUGIN): PLUGIN_EXTENDS = -

QT       += core-private gui-private remoteobjects
REPC_REPLICA = ../protocols/kernel.rep

TEMPLATE = lib
CONFIG += plugin

DISTFILES += \
    $$PWD/plugin.json

isEmpty(INSTALL_PATH) {
    target.path = $$[QT_INSTALL_PLUGINS]/platforms
} else {
    target.path = $$INSTALL_PATH
}

INSTALLS += target

SOURCES += \
    backingstore.cpp \
    integration.cpp \
    main.cpp \
    platformwindow.cpp

HEADERS += \
    backingstore.h \
    integration.h \
    platformwindow.h
