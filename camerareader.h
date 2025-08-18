#ifndef CAMERAREADER_H
#define CAMERAREADER_H

#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "Logger.h"
#include "Timer.h"
#include <functional>
#include <sstream>
#include <boost/lockfree/queue.hpp>
#include <QTime>
#include <QElapsedTimer>
#include <chrono>
// Undefine the Status macro before including OpenCV to prevent conflict with X11
#undef Status
#include <opencv2/opencv.hpp>

class Camerareader {
public:
    Camerareader(const std::string& _camera_pipeline, int _debug=1) :  camera_pipeline(_camera_pipeline) , frameCount(0), debugg(_debug), 
    lastResetTime(QTime::currentTime()), period(33) {
        LOG_INFO("Camerareader Constructor");
    }

    ~Camerareader() {
        stopStreamingThread();
        timer.stop();
        cap.release();
    }
    // Deleted copy operations for thread safety
    Camerareader(const Camerareader&) = delete;
    Camerareader& operator=(const Camerareader&) = delete;

    void update_camera_pipeline(const std::string& _camera_pipeline) {
        camera_pipeline = _camera_pipeline;
        LOG_INFO("update camera_pipeline " + camera_pipeline);
    }
    
    int init() {
        try{
            cap.open(camera_pipeline, cv::CAP_GSTREAMER);
            if (!cap.isOpened()) {
                LOG_ERROR("Error: Could not open the camera.");
                return -1;
            }
            double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
            double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
            double fps = cap.get(cv::CAP_PROP_FPS);
            std::cout << "width " << width << ",height " << height << ", FPS " << fps << std::endl;
            return 0;
        } catch (const std::exception& e) {
            LOG_ERROR("An error occurred in Camerareader init: " + std::string(e.what()));
            return -1;
        }
    }

    void startCapturing(int _period) {
        period =_period;
        timer.start(period, 0, [this]() { CaptureFrame(); });
    }

    void stopCapturing() {
        timer.stop();
    }

    void releasecamera() {
        cap.release();
    }

    int startstream(std::string _stream_pipeline, int _fps, int _width, int _height) {
        try{
            scap.open(_stream_pipeline, 0, _fps, cv::Size(_width, _height), true);
            if (!scap.isOpened()) {
                LOG_ERROR("Error: Could not open the streaming pipline.");
                return -1;
            }
            swidth = _width;
            sheight = _height;
            stream = true;
            startStreamingThread();
            return 0;
        } catch (const std::exception& e) {
            LOG_ERROR("An error occurred in Camerareader startstream: " + std::string(e.what()));
            return -1;
        }
    }  
    
    void stopstream() {
        if (stream) {            
            stream = false;
            stopStreamingThread();
            // cv::Mat* frame;
            // while (streamQueue.pop(frame)) {
            //     if (frame && !frame->empty() && scap.isOpened()) {
            //         scap.write(*frame);
            //     }
            //     delete frame; // Clean up each frame
            // }
            scap.release();
        }
    }

    int startremote(std::string _remote_pipeline) {
        try{
            rcap.open(_remote_pipeline, cv::CAP_GSTREAMER);
            if (!rcap.isOpened()) {
                LOG_ERROR("Error: Could not open the remote pipline.");
                return -1;
            }
            remote = true;
            return 0;
        } catch (const std::exception& e) {
            LOG_ERROR("An error occurred in Camerareader startremote: " + std::string(e.what()));
            return -1;
        }
    }

    void stopremote() {
        if (remote){
            remote = false;
            rcap.release();
        }        
    }

    void setFrameCallback(std::function<void(cv::Mat)> callback) {
        Frame_callback = callback;
    }

    bool takeSnapshotGst(const std::string& pipeline_desc) {
        try{
            GstElement *pipeline = nullptr;
            GstBus *bus = nullptr;
            GstMessage *msg = nullptr;
            GError *error = nullptr;
            bool success = false;
    
            // Initialize GStreamer if needed
            if (!gst_is_initialized())
                gst_init(nullptr, nullptr);

            // Create pipeline from string (e.g. "v4l2src device=/dev/video0 num-buffers=1 ! jpegenc ! filesink location=/tmp/snapshot.jpg")
            pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);
            if (!pipeline) {
                LOG_ERROR("Failed to create pipeline: " + std::string(error ? error->message : "Unknown error"));
                if (error) g_error_free(error);
                return false;
            }

            gst_element_set_state(pipeline, GST_STATE_PLAYING);

            // Wait for EOS or error (should finish after 1 buffer)
            bus = gst_element_get_bus(pipeline);
            msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND,
                    (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
            if (msg != nullptr) {
                if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                    GError *err = nullptr;
                    gchar *debug = nullptr;
                    gst_message_parse_error(msg, &err, &debug);
                    LOG_ERROR("Snapshot pipeline error: " + std::string(err->message));
                    g_error_free(err);
                    g_free(debug);
                    success = false;
                } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
                    success = true;
                }
                gst_message_unref(msg);
            } else {
                LOG_ERROR("Snapshot pipeline timeout");
            }
            gst_object_unref(bus);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(GST_OBJECT(pipeline));
            return success;
        } catch (const std::exception& e) {
            LOG_ERROR("An error occurred in Camerareader startremote: " + std::string(e.what()));
            return false;
        }
    }

    bool takeSnapshot(const std::string& filename) {
        try{
            cv::Mat snapshot;
            if (frame.channels() == 2) {
                cvtColor(frame, snapshot, cv::COLOR_YUV2BGR_YUY2);
                cv::resize(snapshot, snapshot, cv::Size(640,480), 0, 0, cv::INTER_NEAREST);
                cv::imwrite(filename, snapshot);
            }            
            else {
                cv::resize(frame, snapshot, cv::Size(640,480), 0, 0, cv::INTER_NEAREST);
                cv::imwrite(filename, snapshot);
            }
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("An error occurred in Camerareader startremote: " + std::string(e.what()));
            return false;
        }
    }

private:
    std::string camera_pipeline;
    Timer timer;
    cv::VideoCapture cap;
    cv::VideoWriter scap;
    cv::VideoCapture rcap;
    cv::Mat frame;
    cv::Mat rframe;
    std::function<void(cv::Mat)> Frame_callback;
    int frameCount;
    int debugg;
    QTime lastResetTime;    
    int swidth, sheight;
    int period;
    bool stream = false;
    bool remote = false;
    // Streaming thread support
    boost::lockfree::queue<cv::Mat*, boost::lockfree::capacity<20>> streamQueue;
    std::thread streamThread;
    bool streamRunning = false;

    void CaptureFrame() {
        if (cap.isOpened()) {          
            cap.read(frame);
            if (frame.channels() == 2) {
                cvtColor(frame, frame, cv::COLOR_YUV2BGR_YUY2);
            }
            
            if (debugg == 1) {
                frameCount++;    
                std::string text = std::to_string(frameCount);
                cv::Point org(200, 200); // Bottom-left corner of the text
                int fontFace = cv::FONT_HERSHEY_SIMPLEX;
                double fontScale = 5;
                cv::Scalar color(0, 255, 0); // BGR color (Blue, Green, Red) - here, Green
                int thickness = 3;
                int lineType = cv::LINE_AA; // For anti-aliased lines

                // 3. Put Text on the Image
                cv::putText(frame, text, org, fontFace, fontScale, color, thickness, lineType);
            }
            
            if (!frame.empty()) {
                cv::Mat* framePtr = new cv::Mat(frame); // Clone and store as raw pointer
                if (Frame_callback) {
                    if (remote) {
                        CaptureRemoteFrame();
                    } else {
                        Frame_callback(*framePtr);
                    }                  
                }
                if (stream) {
                    cv::Mat* resizedFramePtr = new cv::Mat();
                    cv::Size target_size(swidth, sheight);
                    if (framePtr->size() != target_size) {
                        cv::resize(*framePtr, *resizedFramePtr, target_size, 0, 0, cv::INTER_NEAREST);
                    } else {
                        *resizedFramePtr = *framePtr; // Shallow copy or assignment
                    }
                    if (!streamQueue.push(resizedFramePtr)) {
                        delete resizedFramePtr; // Clean up if push fails
                        LOG_WARN("Stream queue full, dropping frame");
                    }
                    delete framePtr; // Delete the original cloned frame
                } else {
                    delete framePtr; // Clean up if not streaming
                }
            }
            // LOG_INFO("Read time: ");
        }
    }

    void startStreamingThread() {
        streamRunning = true;
        streamThread = std::thread([this]() {
            const int target_fps = 25;
            const auto frame_period = std::chrono::milliseconds(1000 / target_fps);
            auto next_frame_time = std::chrono::steady_clock::now();

            while (streamRunning) {
                cv::Mat* frameToStream = nullptr;
                cv::Mat* lastFrame = nullptr;

                // Always get the latest frame available
                while (streamQueue.pop(frameToStream)) {
                    if (lastFrame) delete lastFrame; // Drop previously stored frame
                    lastFrame = frameToStream;       // Keep newest frame
                }
                if (lastFrame && !lastFrame->empty() && scap.isOpened()) {
                    scap.write(*lastFrame);
                    delete lastFrame;
                }
                // Sleep until next 40ms interval
                next_frame_time += frame_period;
                std::this_thread::sleep_until(next_frame_time);
            }
        });
    }

    void CaptureRemoteFrame() {
        if (rcap.isOpened()) {
            rcap.read(rframe);
            if (!rframe.empty()) {
                Frame_callback(rframe);
            } else {
                Frame_callback(cv::Mat());
            }
        }
    }
    
    void stopStreamingThread() {
        streamRunning = false;
        if (streamThread.joinable()) {
            streamThread.join();
        }
    }
};
#endif // CAMERAREADER_H
