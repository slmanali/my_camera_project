#ifndef VIDEOCONTROLLER_H
#define VIDEOCONTROLLER_H

#include <opencv2/opencv.hpp>
#include <gst/gst.h>
#include "Logger.h"
#include "Timer.h"

class Videocontroller {
public:
    Videocontroller(const std::string& _video_path): video_path(_video_path), isStop(true), isPause(true), volume(35), pipeline(nullptr), volumeElement(nullptr) {
        LOG_INFO("Videocontroller Constructor");
    }

    ~Videocontroller(){
        LOG_INFO("Videocontroller Destructor");
        stopPlaying();
        releasevideo();
    }

    void update_video_path(const std::string& _video_path) {
        video_path = _video_path;
        LOG_INFO("update video_path " + video_path);
    }

    int init() {
        try {
            gst_init(nullptr, nullptr);
            // Open video with OpenCV
            cap.open(video_path);
            if (!cap.isOpened()) {
                LOG_ERROR("Error: Could not open video file with OpenCV.");
                return -1;
            }

            // Set frame rate for timer
            double fps = cap.get(cv::CAP_PROP_FPS);
            if (fps <= 0) {
                fps = 25; // Default if FPS retrieval fails
            }
            int interval = static_cast<int>(1000 / fps);
            timer.start(interval, 2, [this]() { PlayFrame(); });            
            isStop = false;
            isPause = false;
            
            // Set up GStreamer pipeline for audio only
            std::string audioPipelineDesc =
                "filesrc location=\"" + video_path + "\" ! decodebin name=d "
                "d. ! queue ! audioconvert ! audioresample ! volume name=vol volume=0.35 ! pulsesink device=alsa_output.platform-sound-wm8904.stereo-fallback";

            pipeline = gst_parse_launch(audioPipelineDesc.c_str(), nullptr);
            if (!pipeline) {
                LOG_ERROR("Failed to create GStreamer audio pipeline.");
                cap.release();
                return -1;
            }
            volumeElement = gst_bin_get_by_name(GST_BIN(pipeline), "vol");        
            // Start audio playback
            gst_element_set_state(pipeline, GST_STATE_PLAYING);    
            return 0;
        } catch (const std::exception& e) {
            LOG_ERROR("Videocontroller init error: " + std::string(e.what()));
            return -1;
        }
    }

    void startPlaying() {
        if (!pipeline || !cap.isOpened())
            return;
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        timer.start(static_cast<int>(1000 / cap.get(cv::CAP_PROP_FPS)), 1, [this]() { PlayFrame(); });
        isStop = false;
        isPause = false;
    }

    void setFrameCallback(std::function<void(cv::Mat)> callback) {
        Frame_callback = callback;
    }

    void stopPlaying() {
        if (!pipeline)
            return;
        gst_element_set_state(pipeline, GST_STATE_NULL);
        timer.stop();
        cap.set(cv::CAP_PROP_POS_FRAMES, 0);
        isStop = true;
        isPause = true;
    }

    void releasevideo() {
        timer.stop();
        isStop = true;
        isPause = true;
        if (cap.isOpened())
            cap.release();
        if (pipeline) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            pipeline = nullptr;
        }
        if (volumeElement) {
            gst_object_unref(volumeElement);
            volumeElement = nullptr;
        }
    }

    void playPause() {
        if (!pipeline || !cap.isOpened())
            return;
        if (!isPause) {
            gst_element_set_state(pipeline, GST_STATE_PAUSED);
            timer.stop();
        } else {
            gst_element_set_state(pipeline, GST_STATE_PLAYING);
            timer.start(static_cast<int>(1000 / cap.get(cv::CAP_PROP_FPS)), 1, [this]() { PlayFrame(); });
        }
        isPause = !isPause;
    }
    
    void seekForward(int _value) {
        if (!pipeline || !cap.isOpened())
            return;
        pauseTimer();
        double pos = cap.get(cv::CAP_PROP_POS_MSEC);
        double new_pos = pos + _value;
        cap.set(cv::CAP_PROP_POS_MSEC, new_pos);
        gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
                                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                static_cast<gint64>(new_pos * GST_MSECOND));
        resumeTimer();
    }
    
    void seekBackward(int _value) {
        if (!pipeline || !cap.isOpened())
            return;
        pauseTimer();
        double pos = cap.get(cv::CAP_PROP_POS_MSEC);
        double new_pos = std::max(pos - _value, 0.0);
        cap.set(cv::CAP_PROP_POS_MSEC, new_pos);
        gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
                                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                static_cast<gint64>(new_pos * GST_MSECOND));
        resumeTimer();
    }
    
    void volumeChanged(int _volume){
        if (volumeElement) {
            g_object_set(volumeElement, "volume", _volume/100.0, nullptr);
            volume = _volume;
        }
    }

    void pauseTimer() {
        timer.stop(); // Stop the timer to prevent frame reading during seek
    }

    void resumeTimer() {
        if (!isStop && !isPause) { // Only restart if playback isn’t stopped or paused
            timer.start(static_cast<int>(1000 / cap.get(cv::CAP_PROP_FPS)), 1, [this]() { PlayFrame(); });
        }
    }
    bool getStop() {
        return isStop;
    }

    bool getPause() {
        return isPause;
    }

    int getVolume() {
        LOG_INFO("getVolume " + std::to_string(volume));
        return volume;
    }

private:
    std::string video_path;
    bool isStop;
    bool isPause;
    int volume;
    GstElement *pipeline;
    GstElement *volumeElement;
    Timer timer;
    int fps;
    cv::VideoCapture cap;    
    cv::Mat frame;
    std::function<void(cv::Mat)> Frame_callback;

    void PlayFrame() {
        if (cap.isOpened()) {
            cv::Mat frame;
            if (cap.read(frame) && !frame.empty()) {
                if (Frame_callback) {
                    Frame_callback(frame);
                }
            } else {
                stopPlaying(); // End of video
            }
        }
    }
};
#endif // VIDEOCONTROLLER_H