// main helmet start
#include <QApplication>
#include <csignal>
// #undef Status  // Undefine X11's Status macro to avoid conflict with OpenCV
#include "camera_viewer.h"
#include <QSurfaceFormat>
#include <cstdlib>  // For setenvy
#include <gst/gst.h>
void setEnvIfDifferent(const char* var, const char* desired_value) {
    const char* current_value = getenv(var);
    if (!current_value || strcmp(current_value, desired_value) != 0) {
        setenv(var, desired_value, 1);
        std::cout << "Set " << var << " to " << desired_value << std::endl;  // Optional debug output
    } else {
        std::cout << var << " is already set to " << desired_value << std::endl;  // Optional debug output
    }
}

volatile std::sig_atomic_t gSignalStatus = 0;

void signal_handler(int signal) {
  gSignalStatus = signal;
   qApp->quit();  // Terminate the Qt event loop
}

int main(int argc, char *argv[]) {

    try {
        // std::signal(SIGINT, signal_handler);  // Handle Ctrl+C
        gst_init(&argc, &argv);
        freopen("/home/x_user/my_camera_project/Outputs.log", "a", stdout);
        freopen("/home/x_user/my_camera_project/Errors.log", "a", stderr);
        // Set the environment variable for Qt's QPA platform
        setEnvIfDifferent("QT_LOGGING_RULES", "*.debug=false;qt.qpa.*=false");
        setEnvIfDifferent("GST_PLUGIN_PATH", "/usr/lib/aarch64-linux-gnu/gstreamer-1.0/");
        // setEnvIfDifferent("LD_LIBRARY_PATH", "/usr/lib/aarch64-linux-gnu/");
        setEnvIfDifferent("XDG_RUNTIME_DIR", "/run/user/0");
        setEnvIfDifferent("WAYLAND_DISPLAY", "wayland-1");
        setEnvIfDifferent("QT_QPA_PLATFORM", "wayland");
        // setEnvIfDifferent("MESA_LOADER_DRIVER_OVERRIDE", "vivante");
        // setEnvIfDifferent("EGL_PLATFORM", "wayland");
        setEnvIfDifferent("DISPLAY", ":0");
        setEnvIfDifferent("GST_DEBUG", "3");
        setEnvIfDifferent("GST_PLUGIN_FEATURE_RANK", "vaapih264dec:256");
        // setEnvIfDifferent("GST_GL_PLATFORM", "egl");   
        // setEnvIfDifferent("LANG", "ru_RU.UTF-8");   
        // setEnvIfDifferent("LC_ALL", "ru_RU.UTF-8");   
        // setenv("QT_QPA_PLATFORM", "xcb", 1); 
        // setenv("QT_QPA_PLATFORM", "wayland", 1);  // 1 means to overwrite if the variable is already set
        // setenv("WAYLAND_DISPLAY", "/run/wayland-0", 1);
        // setenv("XDG_RUNTIME_DIR", "/run/user/0", 1);
        // setenv("QT_XCB_GL_INTEGRATION", "xcb_egl", 1);  // Force EGL
        // setenv("GST_DEBUG", "3", 1);  // 1 means to overwrite if the variable is already set
        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setProfile(QSurfaceFormat::CoreProfile);
        QSurfaceFormat::setDefaultFormat(format);
        QApplication app(argc, argv);   
        LOG_INFO("=========================================================================================================================");
        LOG_INFO("Smart Helmet Client application logic constructor");
        LOG_INFO("========================================================================================================================="); 

        CameraViewer viewer;
        viewer.showFullScreen();

        return app.exec();
    } catch (const std::exception& e) {
        LOG_ERROR("APPLICATION NOT RUNNING: " + std::string(e.what()));
        return -1;
    }
}

// main helmet end
