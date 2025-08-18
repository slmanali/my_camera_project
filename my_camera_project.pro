QT += core gui widgets concurrent

CONFIG += c++17

# Add this to force debug symbols
# CONFIG += debug
# QMAKE_CXXFLAGS += -g
# QMAKE_LFLAGS += -rdynamic

CONFIG += release
CONFIG += optimize_full

QMAKE_CXXFLAGS += -Wno-psabi

# Keep RTTI enabled (remove -fno-rtti)
CONFIG += exceptions

TARGET = my_camera_project

SOURCES += main.cpp \
           camera_viewer.cpp 

HEADERS += camera_viewer.h \
            Configuration.h \
            Logger.h \
            gpio.h \
            Audio.h \
            HTTPSession.h \
            power_management.h \
            speechThread.h \
            camerareader.h \ 
            PDFCreator.h \
            videocontroller.h \
            LanguageManager.h \
            FloatingMessage.h \
            imu_classifier_thread.h

INCLUDEPATH += /usr/include/opencv4 \
               /usr/include/gstreamer-1.0 \
               /usr/lib/aarch64-linux-gnu/gstreamer-1.0 \
               /usr/include/jsoncpp \
               /usr/include/poppler/qt5 \
               /usr/include/boost \
               /home/x_user/my_camera_project/onnxruntime/include \
               /usr/include/glib-2.0 \
               /usr/lib/aarch64-linux-gnu/glib-2.0/include

LIBS += `pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 glib-2.0`
LIBS += -lopencv_core -lopencv_imgproc -lopencv_videoio -lopencv_imgcodecs
LIBS += -lcurl -ljsoncpp -lgpiod -lpoppler-qt5
LIBS += -lssl -lcrypto -lzbar -lX11 -lzip -lpthread
LIBS += -L/home/x_user/my_camera_project -lvosk
LIBS += -lgstreamer-1.0 -lgstapp-1.0 -lgstvideo-1.0
LIBS += -lhpdf -lpng -lz -lonnxruntime
LIBS += -L/usr/lib/aarch64-linux-gnu -lQt5GLib-2.0

# Modified compiler flags (removed -fno-rtti)
QMAKE_CXXFLAGS_RELEASE += -O3 -march=native -ffast-math
QMAKE_LFLAGS_RELEASE += -O3

QMAKE_LFLAGS += -Wl,-rpath=/usr/lib/aarch64-linux-gnu/gstreamer-1.0

# Disable specific warnings from Poppler
QMAKE_CXXFLAGS += -Wno-deprecated-declarations