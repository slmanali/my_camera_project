#ifndef CAMERA_VIEWER_H
#define CAMERA_VIEWER_H

#include <opencv2/opencv.hpp>

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QColor>
#include <QString>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QImage>
#include <QPixmap>
#include <QListWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QFont>
#include <QGraphicsEllipseItem>
#include <QStackedWidget>
#include <poppler-qt5.h>
#include <QGraphicsPixmapItem>
#include <QScrollBar>
#include <QDir>
#include <QPen>
#include <QFontMetrics>
#include <QTextLayout>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QThread>
#include <QtConcurrent>

#include <gst/gst.h>

#include <thread>
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <zbar.h> // For QR code decoding
#include <iomanip>
#include <sstream>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <cstring> // Include this for memset()
#include <X11/Xlib.h>

#include "Configuration.h"
#include "Audio.h"
#include "gpio.h"
#include "HTTPSession.h"
#include "WiFiManager.h"
#include "Logger.h"
#include "camerareader.h"
#include "speechThread.h"
#include "power_management.h"
#include "PDFCreator.h"
#include "videocontroller.h"
#include "LanguageManager.h"
#include "FloatingMessage.h"
#include "imu_classifier_thread.h" 

#include <algorithm> // For std::sort
#include <map>
#include <codecvt>
#include <locale>
#include <ctime>
#include <map>

class QLabel;

class CameraViewer : public QWidget {
    Q_OBJECT

public:
    CameraViewer(QWidget *parent = nullptr);
    QWidget* createFirstTab();
    QWidget* createNavigationWidget();
    QWidget* createContentWidget();
    QWidget* createVideoTab(); 
    QWidget* createAudioControlTab(); 
    void button_pressed();
    void readwifijson();
    void working_mode();
    void FSM(nlohmann::json _data, std::string _event);    
    void handle_command_recognize(std::string _command);
    void changeLanguage(std::string _lang);
    void handle_update_frame(cv::Mat _frame);
    void handle_update_video(cv::Mat _frame);
    std::string toUpperCase(const std::string& input);
    std::string getCurrentDateTime();
    void setVisors(std::string _value);
    void setCamera(std::string _value);
    void send_audio_settings();
    void start_audio_channel();
    void close_audio_channel();
    void process_event(nlohmann::json _data);
    void remotestart();
    void remoteend();
    void streamstart(std::string _stream);
    void streamend();
    void showdefaultstandalone(bool _standalone = true);
    void showpdfmode();
    void showvideomode();
    void showtxtmode();
    void showFilesList(const std::string &folder_path, const std::string &suffix);
    void LoadPDF(const std::string &filepath);
    void LoadMP4(const std::string &filepath);
    std::string loadTasks(const std::string &filename);
    void displayTasks();
    void loadTXT(const std::string &filePath);
    void showPage(int pageNum);
    void nextPage();
    void previousPage();
    void zoomIn();
    void zoomOut();
    void scrollUp();
    void scrollDown();
    void scrollLeft();
    void scrollRight();
    void nextTask();
    void prevTask();
    void display_messages(std::string _msg);
    QString splitTextIntoLines(const QString& text, int maxWidth, const QFont& font);
    void setTextWithWrapping(const QString& text);
    void drawTaskList();
    void start_qrcode();
    void stop_qrcode();
    std::string base64_decode_openssl(const std::string &encoded);
    std::string aes_decrypt_ecb(const std::string &cipherText, const std::string &key);
    std::string removePadding(const std::string &input, const std::string &padding);    
    void processQRCode(cv::Mat _frame);
    void batteryiconchange(PowerManagement::BatteryStatus _status);
    void complete_standalone_transition(bool _NOWIFI);
    QHBoxLayout* createSliderControl(const QString &name, int min, int max, int value, QSlider*& slider);
    QHBoxLayout* createComboControl(const QString &name, const QString &items);
    void AudioReset();
    ~CameraViewer();

private slots:
    void Enter_Low_Power_Mode();
    void Exit_Low_Power_Mode();
    void report_and_reset_clicks();
    void finish_helping();
    void checkwifi();
    void handleIMUClassification(const QString& label);

private:
    QGraphicsScene *videoScene, *videoScene1, *videoScene2;
    QGraphicsPixmapItem *videoPixmapItem, *videoPixmapItem1, *videoPixmapItem2;
    QGraphicsView *videoView, *videoView1, *videoView2;
    QLabel *avatarLabel;
    QLabel *wifiLabel;
    QLabel *batteryLabel;
    QLabel *legend_label1;
    QLabel *legend_label2;
    QLabel *legend_label3;
    QLabel *status_label;
    QLabel *user_label;
    QLabel *qrcode_label;
    QLabel *task_name;
    QLabel *task_list;
    QLabel *message;
    QListWidget *listFiles, *taskListWidget, *listvideos, *listaudios;
    QLabel *batteryLabel_nav;   
    QLabel *batteryLabel_content;
    QLabel *batteryLabel_video;  
    QLabel *batteryLabel_audio; 
    QGraphicsScene *scene;
    QGraphicsView *view;
    QStackedWidget *stackedWidget;
    QWidget *firstTab, *secondTab, *thirdTab, *forthTab, *fifthTab;
    FloatingMessage *floatingMessage;
    Configuration config;
    LanguageManager lang;
    HTTPSession session;
    WiFiManager network;
    PowerManagement pm;
    std::unique_ptr<speechThread> voiceThread;
    std::unique_ptr<Camerareader> cameraThread;
    std::unique_ptr<Videocontroller> videoThread;
    std::unique_ptr<IMUClassifierThread> imuThread;
    AudioControl A_control;
    AudioPlayer A_player;
    AudioStreamer A_streamer;
    GPIOThread gpio_thread;
    Json::Value wifi;
    Poppler::Document *document;
    QTimer *timer;
    QTimer *standbytimer;
    QTimer *clicktimer;
    QTimer *helptimer;
    cv::Mat resized_image;
    cv::Mat cropped_image;
    cv::Mat cropped_image_scaled;
    cv::Mat gray_img;
    zbar::ImageScanner scanner;
    QPixmap pixmap, pixmap1;
    QImage image;
    PDFCreator pdf;
    std::vector<std::string> pdfFiles;
    std::vector<std::string> txtFiles;
    std::vector<std::string> mp4Files;
    std::vector<std::string> tasks;
    std::chrono::time_point<std::chrono::high_resolution_clock> msgtimer;
    std::vector<std::map<std::string, std::string>> tasklist;
    std::map<std::string, std::vector<std::map<std::string, std::string>>> procedure;        
    cv::Point top_left;
    cv::Point bottom_right;
    int Swidth, Sheight;
    std::mutex battery_mutex;
    int old_values[4] = {5000,25,1024,768};
    QSlider *headphoneSlider;
    QSlider *captureSlider;
    QLabel *captureInputLabel;
    std::vector<std::string> operator_status_list;
    std::string operator_status = "Unknown";
    const std::string QR_CODE_KEY = "mZq4t7w!z%C*F-Ja";
    const std::string QR_CODE_PADDING = "A";
    PowerManagement::BatteryStatus battery_status = PowerManagement::BatteryStatus::GREEN;
    int _index = 0;
    int id_im = -1;
    bool standalone_language_transition = false;
    bool camera_rotate = false;
    bool task_sharing = false;
    bool screenshot = false;
    int currentTaskIndex = 0;
    bool _pause_stream = false;
    int scenaraio = 0;
    float zoomFactor = 1.5;
    int currentPage = 0;
    int _working_wifi = 0;
    std::string current_mode = "offline";
    int clicks = 0;
    bool btn_click = false;
    int last_processed_event = -1;
    int id_call = -1;
    pid_t gstreamer_pid = -1;
    bool entering_standalone = false;
    std::string _ipstream = "";
    int qrcode_counter = 0;

};

#endif // CAMERA_VIEWER_H
