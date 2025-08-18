QT += widgets
CONFIG += c++11

SOURCES += main.cpp

INCLUDEPATH += $$system(pkg-config --cflags-only-I gstreamer-1.0 gstreamer-video-1.0 | sed 's/-I//g')

LIBS += $$system(pkg-config --libs gstreamer-1.0 gstreamer-video-1.0)

