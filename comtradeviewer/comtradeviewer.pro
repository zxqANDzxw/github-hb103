QT += core gui widgets
CONFIG += c++11 debug
TARGET = comtradeviewer
TEMPLATE = app

SOURCES += \
    main.cpp \
    comtrade.cpp \
    waveformview.cpp \
    viewerwindow.cpp

HEADERS += \
    comtrade.h \
    waveformview.h \
    viewerwindow.h
