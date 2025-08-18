#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <QTimer>

class GstQtWidget : public QWidget {
    Q_OBJECT
public:
    GstQtWidget(QWidget *parent = nullptr) : QWidget(parent), pipeline(nullptr), sink(nullptr) {
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_PaintOnScreen);
    }

protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        if (!pipeline) {
            QTimer::singleShot(100, this, SLOT(startGst()));
        }
    }

private slots:
    void startGst() {
        gst_init(nullptr, nullptr);
        pipeline = gst_parse_launch(
            "v4l2src device=/dev/video3 ! video/x-raw,format=YUY2,width=1920,height=1080, framerate=30/1 ! videoconvert ! waylandsink name=mysink", nullptr);
        sink = gst_bin_get_by_name(GST_BIN(pipeline), "mysink");
        // g_object_set(sink, "fullscreen", TRUE, NULL);
        WId id = winId();
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), id);
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
    }

public:
    ~GstQtWidget() override {
        if (pipeline) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
        }
        if (sink)
            gst_object_unref(sink);
    }

private:
    GstElement *pipeline;
    GstElement *sink;
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    QVBoxLayout layout(&window);
    GstQtWidget *gstWidget = new GstQtWidget;
    layout.addWidget(gstWidget);
    window.resize(800, 600);
    window.showFullScreen();
    return app.exec();
}
// #include "../videocontroller.h"
// #include <iostream>
// #include <thread>
// #include <chrono>


// void displayFrame(cv::Mat frame) {
//     cv::imshow("Video Player", frame);
//     cv::waitKey(1); // Needed to update the window
// }

// int main(int argc, char* argv[]) {
//     if (argc < 2) {
//         std::cerr << "Usage: " << argv[0] << " <video_file_path>" << std::endl;
//         return 1;
//     }

//     std::string videoPath = argv[1];
    
//     // Create video controller
//     Videocontroller controller(videoPath);
//     controller.setFrameCallback(displayFrame);
    
//     if (controller.init() != 0) {
//         std::cerr << "Failed to initialize video controller" << std::endl;
//         return 1;
//     }
    
//     // Start playing
//     controller.startPlaying();
    
//     // Main loop for user interaction
//     bool running = true;
//     while (running) {
//         std::cout << "\nOptions:\n"
//                   << "1. Play/Pause\n"
//                   << "2. Stop\n"
//                   << "3. Seek Forward 5s\n"
//                   << "4. Seek Backward 5s\n"
//                   << "5. Increase Volume\n"
//                   << "6. Decrease Volume\n"
//                   << "7. Exit\n"
//                   << "Enter choice: ";
        
//         int choice;
//         std::cin >> choice;
        
//         switch (choice) {
//             case 1:
//                 controller.playPause();
//                 std::cout << (controller.getPause() ? "Paused" : "Playing") << std::endl;
//                 break;
//             case 2:
//                 controller.stopPlaying();
//                 std::cout << "Stopped" << std::endl;
//                 break;
//             case 3:
//                 controller.seekForward(5000); // 5 seconds
//                 std::cout << "Seeked forward 5 seconds" << std::endl;
//                 break;
//             case 4:
//                 controller.seekBackward(5000); // 5 seconds
//                 std::cout << "Seeked backward 5 seconds" << std::endl;
//                 break;
//             case 5: {
//                 int vol = controller.getVolume() + 10;
//                 if (vol > 100) vol = 100;
//                 controller.volumeChanged(vol);
//                 std::cout << "Volume set to " << vol << std::endl;
//                 break;
//             }
//             case 6: {
//                 int vol = controller.getVolume() - 10;
//                 if (vol < 0) vol = 0;
//                 controller.volumeChanged(vol);
//                 std::cout << "Volume set to " << vol << std::endl;
//                 break;
//             }
//             case 7:
//                 running = false;
//                 break;
//             default:
//                 std::cout << "Invalid choice" << std::endl;
//         }
//     }
    
//     // Clean up
//     cv::destroyAllWindows();
//     controller.releasevideo();
    
//     return 0;
// }