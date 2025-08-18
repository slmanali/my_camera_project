#include "camera_viewer.h"
#include <unordered_map>
#include <algorithm>

// Counts frequency of each string in a vector
std::unordered_map<std::string, int> frequency_counter(const std::vector<std::string>& vec) {
    std::unordered_map<std::string, int> freq;
    for (const auto& item : vec) {
        freq[item]++;
    }
    return freq;
}

template <typename T>
T clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

class PdfView : public QGraphicsView {
public:
    PdfView(QGraphicsScene *scene, QWidget *parent = nullptr) : QGraphicsView(scene, parent) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFrameStyle(QFrame::NoFrame);
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QGraphicsView::resizeEvent(event);
        if (scene()) {
            fitInView(scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
        }
    }
};

CameraViewer::CameraViewer(QWidget *parent)
    : QWidget(parent), 
    videoScene(new QGraphicsScene(this)),
    videoScene1(new QGraphicsScene(this)),
    videoScene2(new QGraphicsScene(this)),
    videoPixmapItem(new QGraphicsPixmapItem()),
    videoPixmapItem1(new QGraphicsPixmapItem()),
    videoPixmapItem2(new QGraphicsPixmapItem()),
    videoView(new PdfView(videoScene, this)),
    videoView1(new PdfView(videoScene1, this)),
    videoView2(new PdfView(videoScene2, this)),
    avatarLabel(new QLabel(this)),  
    wifiLabel(new QLabel(this)),  
    batteryLabel(new QLabel(this)),
    legend_label1(new QLabel(this)),
    legend_label2(new QLabel(this)),
    legend_label3(new QLabel(this)),
    status_label(new QLabel(this)),
    user_label(new QLabel(this)),
    qrcode_label(new QLabel(this)),
    task_name(new QLabel(this)),
    task_list(new QLabel(this)),
    message(new QLabel(this)),
    listFiles(new QListWidget(this)),
    taskListWidget(new QListWidget(this)),
    listvideos(new QListWidget(this)),
    listaudios(new QListWidget(this)),    
    batteryLabel_nav(new QLabel(this)), 
    batteryLabel_content(new QLabel(this)),
    batteryLabel_video(new QLabel(this)),
    batteryLabel_audio(new QLabel(this)),
    scene(new QGraphicsScene(this)),
    view(new PdfView(scene, this)),
    stackedWidget(new QStackedWidget(this)),
    firstTab(new QWidget(this)),
    secondTab(new QWidget(this)),
    thirdTab(new QWidget(this)),
    forthTab(new QWidget(this)),
    fifthTab(new QWidget(this)),
    floatingMessage(new FloatingMessage(this)),
    config("/home/x_user/my_camera_project/configuration_ap.json"),
    lang(config.default_language),
    session(" ", config.ssl_cert_path, config.api_key, "offline", config),
    network(config.wireless_interface, config, session),
    pm(config),
    voiceThread(std::make_unique<speechThread>(lang.getVosk(), lang.getGrammar(), config.pipeline_description, 10)),
    cameraThread(std::make_unique<Camerareader>( config._vl_loopback, config.debug)),
    videoThread(std::make_unique<Videocontroller>("")),
    imuThread(std::make_unique<IMUClassifierThread>(config.imu)),
    A_player(config.audio_incoming_pipeline),
    A_streamer(" "),
    gpio_thread("gpiochip2", 25),
    document(nullptr),
    timer(new QTimer(this)),
    standbytimer(new QTimer(this)),
    clicktimer(new QTimer(this)),
    helptimer(new QTimer(this)),
    top_left(337, 57), 
    bottom_right(942, 662) {    
    try {
        // Set some fixed sizes for labels to ensure visibility 
        LOG_INFO("CameraViewer Constructor");           
        gst_init(nullptr, nullptr);
        Display* display = XOpenDisplay(NULL);
        if (display == NULL) {
            LOG_ERROR("Unable to open X display");
        }
        int screen = DefaultScreen(display);
        // Get screen resolution
        Swidth = DisplayWidth(display, screen);
        Sheight = DisplayHeight(display, screen);
        
        // Print the resolution
        LOG_INFO("Screen Resolution: " + std::to_string(Swidth) + "x" + std::to_string(Sheight));
        // Close the connection to the X server
        XCloseDisplay(display);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        stackedWidget->setFrameShape(QFrame::NoFrame);
        stackedWidget->setFrameShadow(QFrame::Plain);
        stackedWidget->setLineWidth(0);
        stackedWidget->setMidLineWidth(0);
        stackedWidget->setStyleSheet("QStackedWidget { border: none }");
        stackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        //camera tab
        videoScene->addItem(videoPixmapItem);
        videoView->setFrameStyle(QFrame::NoFrame);
        videoView->setRenderHint(QPainter::Antialiasing, false);
        videoView->setRenderHint(QPainter::SmoothPixmapTransform, false);
        videoView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);     
        //video tab
        videoScene1->addItem(videoPixmapItem1);
        videoView1->setFrameStyle(QFrame::NoFrame);
        videoView1->setRenderHint(QPainter::Antialiasing, false);
        videoView1->setRenderHint(QPainter::SmoothPixmapTransform, false);
        videoView1->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView1->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);     
        //Content tab (Task + camera + document)
        view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  
        videoScene2->addItem(videoPixmapItem2);
        videoView2->setFrameStyle(QFrame::NoFrame);
        videoView2->setRenderHint(QPainter::Antialiasing, false);
        videoView2->setRenderHint(QPainter::SmoothPixmapTransform, false);
        videoView2->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView2->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView2->setFixedSize(320, 240);
        // videoView2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);     

        stackedWidget->addWidget(createFirstTab());
        stackedWidget->addWidget(createNavigationWidget());
        stackedWidget->addWidget(createContentWidget());
        stackedWidget->addWidget(createVideoTab()); 
        stackedWidget->addWidget(createAudioControlTab()); 
        mainLayout->addWidget(stackedWidget);
        
        // Set the layout for this widget
        setLayout(mainLayout);
        this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        // Apply no border style to the whole widget
        this->setStyleSheet("QWidget { border: none; }");
        LOG_INFO("Finish Interface");
        msgtimer = std::chrono::high_resolution_clock::now();
        
        // Set callback for GPIO thread
        gpio_thread.set_button_pressed_callback([this]() {
            this->button_pressed();
        });
        LOG_INFO("gpio_thread Started");
        // Start the GPIO thread
        gpio_thread.start();
        // Read the WIFI JSON
        readwifijson();
        // Start The WIFI Manager
        network.init();
        network.setConnections(wifi);
        _working_wifi = network.run();
        LOG_INFO("_working_wifi = "+ std::to_string(_working_wifi));

        working_mode();
        connect(timer, &QTimer::timeout, this, &CameraViewer::checkwifi);
        connect(clicktimer, &QTimer::timeout, this, &CameraViewer::report_and_reset_clicks);
        connect(helptimer, &QTimer::timeout, this, &CameraViewer::finish_helping);
        // connect(standbytimer, &QTimer::timeout, this, &CameraViewer::Enter_Low_Power_Mode);
        timer->setInterval(600000);
        clicktimer->setInterval(2000);      
        clicktimer->setSingleShot(true);
        helptimer->setInterval(5000);      
        helptimer->setSingleShot(true);

        session.set_update_status_callback([this](nlohmann::json data, std::string event) {
            QMetaObject::invokeMethod(this, [this, data, event]() {
                FSM(data, event);
            }, Qt::QueuedConnection);
        });

        voiceThread->setCommandCallback([this](const std::string &command) {
            QMetaObject::invokeMethod(this, [this, command]() {
                handle_command_recognize(command);
            });
        });

        cameraThread->setFrameCallback([this](const cv::Mat& _frame) {
            QMetaObject::invokeMethod(this, [this, _frame]() {
                handle_update_frame(_frame);
            });
        });    
        
        videoThread->setFrameCallback([this](const cv::Mat& _frame) {
            QMetaObject::invokeMethod(this, [this, _frame]() {
                handle_update_video(_frame);
            });
        });  
        if (config.testbench == 0) {
            if (imuThread->init() == 0) {
                imuThread->setResultCallback([this](const QString _label) {
                    QMetaObject::invokeMethod(this, [this, _label]() {
                        handleIMUClassification(_label);
                    });
                });  
                imuThread->start_IMU(config.status_update);
            }

            pm.set_battery_status_callback([this](PowerManagement::BatteryStatus status) {
                QMetaObject::invokeMethod(this, [this, status]() {
                    this->batteryiconchange(status);
                }, Qt::QueuedConnection);
            });
            pm.start_updates();

            // std::string chmod_cmd = "chmod +x " + config.script_gps;
            // system(chmod_cmd.c_str());

            // std::string command = "sudo " + config.script_gps;
            // system(command.c_str());
        }
        AudioReset();
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer Constructor : " + std::string(e.what()));
    }
}

CameraViewer::~CameraViewer() {       
    if (videoPixmapItem) {
        delete videoPixmapItem;
        videoPixmapItem = nullptr;
    }
    if (videoPixmapItem1) {
        delete videoPixmapItem1;
        videoPixmapItem1 = nullptr;
    }
    if (videoPixmapItem2) {
        delete videoPixmapItem2;
        videoPixmapItem2 = nullptr;
    }
    if (document)
        delete document;
    if (imuThread && config.testbench == 0) {
        imuThread->stop();
    }
    delete videoView;
    delete videoScene;
    delete videoView1;
    delete videoScene1;
    delete videoView2;
    delete videoScene2;
}

QWidget* CameraViewer::createFirstTab() {
    try {
        LOG_INFO("default tab");
        QGridLayout *gridLayout = new QGridLayout(firstTab);
        // Set grid layout margins
        gridLayout->setContentsMargins(0, 0, 0, 0);
        gridLayout->setSpacing(0);

        // Pixmap scaling
        QPixmap pixmap_battery(QString::fromStdString(config.battery_file_full));
        int default_width = Swidth/10;
        int default_height = Sheight/10;

        QPixmap scaledPixmapBattery = pixmap_battery.scaled(
            default_width/3,
            default_height,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );

        QPixmap pixmap_wifi(QString::fromStdString(config.wifi_icon_nowifi));
        QPixmap scaledPixmapWifi = pixmap_wifi.scaled(
            default_width/3,
            default_height,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );

        QPixmap pixmap_avatar(QString::fromStdString(config.avatar_file));
        QPixmap scaledPixmapAvatar = pixmap_avatar.scaled(
            3*default_width/2,
            default_height,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );

        // QLabel elements setup
        // legend_label1->setFixedSize(default_width+70, default_height/4);
        // legend_label2->setFixedSize(default_width+70, default_height/4);
        // legend_label3->setFixedSize(default_width+70, default_height/4);
        // status_label->setFixedSize(default_width+70, default_height/4);
        task_name->setFixedSize(2*default_width, default_height/2);
        task_list->setFixedSize(4*default_width, 1.5*default_height);
        message->setFixedSize(3*default_width, default_height);
        qrcode_label->setFixedSize(4*default_width, default_height);
        avatarLabel->setFixedSize(default_width, default_height);
        wifiLabel->setFixedSize(default_width/3, default_height);
        batteryLabel->setFixedSize(default_width/3, default_height);

        avatarLabel->setPixmap(scaledPixmapAvatar);
        wifiLabel->setPixmap(scaledPixmapWifi);
        batteryLabel->setPixmap(scaledPixmapBattery);

        wifiLabel->setContentsMargins(0, 0, 0, 0);
        batteryLabel->setContentsMargins(0, 0, 0, 0);

        videoView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        gridLayout->addWidget(videoView, 0, 0, 11, 7);  // Add videoView to layout
        // Set stretch factors
        for (int i = 0; i < 11; ++i) {
            gridLayout->setRowStretch(i, 1);
        }
        for (int j = 0; j < 6; ++j) {
            gridLayout->setColumnStretch(j, 1);
        }
        gridLayout->setColumnStretch(6, 100);
        // Add legend labels in the top-right corner  0
        QVBoxLayout *toprightlayout = new QVBoxLayout();              
        toprightlayout->addWidget(legend_label1);
        toprightlayout->addWidget(legend_label2);
        toprightlayout->addWidget(legend_label3);
        toprightlayout->addWidget(status_label);
        legend_label1->setAlignment(Qt::AlignLeft);
        legend_label2->setAlignment(Qt::AlignLeft);
        legend_label3->setAlignment(Qt::AlignLeft);        
        status_label->setAlignment(Qt::AlignLeft);
        toprightlayout->setSpacing(0);          // Eliminate spacing between widgets
        toprightlayout->setContentsMargins(0, 0, 0, 0);
        gridLayout->addLayout(toprightlayout, 0, 6, Qt::AlignRight | Qt::AlignTop);
        
        // Add task labels in the top-left corner
        QVBoxLayout *topleftlayout = new QVBoxLayout();    
        topleftlayout->addWidget(task_name);
        topleftlayout->addWidget(task_list);
        gridLayout->addLayout(topleftlayout, 0, 0, 1, 4, Qt::AlignLeft | Qt::AlignTop);

        // Add QR code and click labels in the center
        gridLayout->addWidget(qrcode_label, 0, 3, 1, 4, Qt::AlignCenter);

        // Add message, wifi and battery labels in the bottom-left corner
        gridLayout->addWidget(message, 8, 0, 1, 3, Qt::AlignLeft | Qt::AlignTop);
        
        QHBoxLayout *bottomLeftLayout = new QHBoxLayout();
        bottomLeftLayout->addWidget(wifiLabel);
        bottomLeftLayout->addWidget(batteryLabel);
        bottomLeftLayout->setSpacing(0);          // Eliminate spacing between widgets
        bottomLeftLayout->setContentsMargins(0, 0, 0, 0);
        gridLayout->addLayout(bottomLeftLayout, 10, 0, Qt::AlignLeft | Qt::AlignBottom);
        legend_label1->setAlignment(Qt::AlignBottom);
        legend_label2->setAlignment(Qt::AlignBottom);

        // Add avatar and status labels in the bottom-right corner       
        QVBoxLayout *bottomrightlayout = new QVBoxLayout();   
        bottomrightlayout->addWidget(user_label);   
        bottomrightlayout->addWidget(avatarLabel); 
        bottomrightlayout->setSpacing(0);          // Eliminate spacing between widgets
        gridLayout->addLayout(bottomrightlayout, 10, 6, Qt::AlignRight | Qt::AlignBottom);
        avatarLabel->setAlignment(Qt::AlignRight);        
        user_label->setAlignment(Qt::AlignRight);

        // Set placeholder text
        legend_label1->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
        legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","langs")));
        legend_label1->setStyleSheet("QLabel { color : green; font-weight: bold;}");
        legend_label2->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
        legend_label2->setText(QString::fromStdString(lang.getText("defaulttab","standalone")));
        legend_label2->setStyleSheet("QLabel { color : green; font-weight: bold;}");        
        legend_label3->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
        legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
        legend_label3->setStyleSheet("QLabel { color : green; font-weight: bold;}");        
        status_label->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
        status_label->setText(QString::fromStdString(lang.getText("defaulttab","setup")));
        status_label->setStyleSheet("QLabel { color : green; font-weight: bold;}");               
        user_label->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
        user_label->setStyleSheet("QLabel { color : green; font-weight: bold;}");
        user_label->setVisible(false);
        task_name->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
        task_name->setStyleSheet("QLabel { color : green; font-weight: bold;}");
        task_name->setAlignment(Qt::AlignCenter);
        task_name->setVisible(false);
        task_list->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
        task_list->setWordWrap(true);
        task_list->setStyleSheet("QLabel { background-color : lightyellow; color : green; font-weight: bold;}");
        task_list->setVisible(false);
        task_list->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        message->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size + 10));
        message->setWordWrap(true);
        message->setStyleSheet("QLabel { background-color : lightyellow; color : green; font-weight: bold;}"); 
        message->setVisible(false);       
        qrcode_label->setAlignment(Qt::AlignCenter);
        qrcode_label->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
        qrcode_label->setStyleSheet("QLabel { background-color : lightyellow; color : green; font-weight: bold;}");
        qrcode_label->setVisible(false);
        
        firstTab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer createFirstTab: " + std::string(e.what()));
    }  
    return firstTab;
}

QWidget* CameraViewer::createNavigationWidget() {    
    try {
        LOG_INFO("Navigation tab");
        QVBoxLayout *layout = new QVBoxLayout(secondTab);
        layout->setContentsMargins(0, 0, 0, 0); 
        layout->setSpacing(0); 
        
        listFiles->setMaximumWidth(Swidth);
        listFiles->setMaximumHeight(0.95*Sheight);
        listFiles->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));        
        listFiles->setStyleSheet("QListWidget { color : green; font-weight: bold;}");
        
        std::string navJsonStr = lang.getSection("Navigationtab");        
        // Parse the JSON string
        nlohmann::json navJson = nlohmann::json::parse(navJsonStr);        
        // Create a QStringList to hold the items
        QStringList navItems;        
        // Check if the section is an array
        if (navJson.is_array()) {
            for (const auto& item : navJson) {
                navItems << QString::fromStdString(item.get<std::string>());
            }
        }
        // Check if the section is an object (extract values)
        else if (navJson.is_object()) {
            for (auto it = navJson.begin(); it != navJson.end(); ++it) {
                navItems << QString::fromStdString(it.value().get<std::string>());
            }
        } else {
            LOG_ERROR("Navigationtab section is neither an array nor an object");
            navItems << "Error: Invalid Navigationtab format";
        }
        // Add the items to the QListWidget
        listFiles->addItems(navItems);              
        layout->addWidget(listFiles);        
        QPixmap pixmap_battery(QString::fromStdString(config.battery_file_full));
        QPixmap scaledPixmapBattery = pixmap_battery.scaled(
            Swidth/30,
            Sheight*0.05,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );    
        batteryLabel_nav->setFixedSize(Swidth/30, Sheight*0.05);
        batteryLabel_nav->setPixmap(scaledPixmapBattery);
        layout->addWidget(batteryLabel_nav);  
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer createNavigationWidget: " + std::string(e.what()));
    }
    return secondTab;
}

QWidget* CameraViewer::createContentWidget() {
    try {
        LOG_INFO("Content tab");
        QVBoxLayout *layout = new QVBoxLayout(thirdTab);
        layout->setContentsMargins(0, 0, 0, 0);         
        layout->setSpacing(0); 

        QHBoxLayout *Task_videoLayout = new QHBoxLayout();
        taskListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        taskListWidget->setMaximumHeight(Sheight/5);
        taskListWidget->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));           
        taskListWidget->setStyleSheet("QListWidget { color : green; font-weight: bold;}");
        taskListWidget->setWordWrap(true);
        videoView2->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView2->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   
        // videoView2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);            
        videoView2->setFixedSize(320, 240);
        Task_videoLayout->addWidget(taskListWidget);
        Task_videoLayout->addWidget(videoView2);
        view->setMaximumHeight(Sheight*0.75);
        layout->addLayout(Task_videoLayout);        
        layout->addWidget(view);        
        QPixmap pixmap_battery(QString::fromStdString(config.battery_file_full));
        QPixmap scaledPixmapBattery = pixmap_battery.scaled(
            Swidth/30,
            Sheight*0.05,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );    
        batteryLabel_content->setFixedSize(Swidth/30, Sheight*0.05);
        batteryLabel_content->setPixmap(scaledPixmapBattery);
        layout->addWidget(batteryLabel_content);  
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer createContentWidget: " + std::string(e.what()));
    }
    return thirdTab;
}

QWidget* CameraViewer::createVideoTab() {
    try {
        LOG_INFO("Creating video tab");
        // Initialize forthTab if not already done
        if (!forthTab) {
            forthTab = new QWidget();
        }

        QVBoxLayout *layout = new QVBoxLayout(forthTab);
        layout->setContentsMargins(0, 0, 0, 0);      
        layout->setSpacing(0); 
        
        videoView1->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        videoView1->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);        
        videoView1->setMaximumHeight(0.95*Sheight);
        videoView1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(videoView1);
        QHBoxLayout *bottomLeftLayout = new QHBoxLayout();
        
        listvideos->setMaximumWidth(Swidth*0.95);
        listvideos->setMaximumHeight(0.05*Sheight);
        listvideos->setFlow(QListView::LeftToRight);
        listvideos->setWrapping(true);
        listvideos->setResizeMode(QListView::Adjust);
        listvideos->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));        
        listvideos->setStyleSheet("QListWidget { color : green; font-weight: bold;}");        
        QPixmap pixmap_battery(QString::fromStdString(config.battery_file_full));
        QPixmap scaledPixmapBattery = pixmap_battery.scaled(
            Swidth/30,
            Sheight*0.05,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );    
        batteryLabel_video->setFixedSize(Swidth/30, Sheight*0.05);
        batteryLabel_video->setPixmap(scaledPixmapBattery);
        bottomLeftLayout->addWidget(batteryLabel_video); 
        bottomLeftLayout->addWidget(listvideos);            
        layout->addLayout(bottomLeftLayout);
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer createVideoTab: " + std::string(e.what()));

    }
    return forthTab;
}

QWidget* CameraViewer::createAudioControlTab() {
    try {
        LOG_INFO("Creating Audio Control tab");
        // Initialize forthTab if not already done
        if (!fifthTab) {
            fifthTab = new QWidget();
        }
        QHBoxLayout *layout = new QHBoxLayout(fifthTab);
        layout->setContentsMargins(0, 0, 0, 0);      
        layout->setSpacing(0); 
        listaudios->setMaximumWidth(0.25*Swidth);
        listaudios->setMaximumHeight(Sheight*0.9);
        listaudios->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));        
        listaudios->setStyleSheet("QListWidget { color : green; font-weight: bold;}");
        std::string navJsonStr = lang.getSection("audiocomandtab");        
        // Parse the JSON string
        nlohmann::json navJson = nlohmann::json::parse(navJsonStr);        
        QStringList navItems;        
        if (navJson.is_object()) {
            // Define the desired order of keys as they appear in lang.json
            std::vector<std::string> keyOrder = {
                "volumelouder",
                "volumesilent",
                "Capturelouder",
                "Capturesilent",
                "AudioReset",
                "quit"
            };
            // Iterate over the keys in the defined order
            int i = 1;
            for (const auto& key : keyOrder) {
                if (navJson.contains(key)) {
                    navItems << QString::number(i) + " - " + QString::fromStdString(navJson[key].get<std::string>());
                    i += 1;
                } else {
                    LOG_WARN("Key " + key + " not found in audiocomandtab");
                }
            }
        } else {
            LOG_ERROR("audiocomandtab section is not an object");
            navItems << "Error: Invalid audiocomandtab format";
        }
        listaudios->addItems(navItems);  
        QVBoxLayout *bottomLeftLayout = new QVBoxLayout();
        QPixmap pixmap_battery(QString::fromStdString(config.battery_file_full));
        QPixmap scaledPixmapBattery = pixmap_battery.scaled(
            Swidth/30,
            Sheight*0.05,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );    
        batteryLabel_audio->setFixedSize(Swidth/30, Sheight*0.05);
        batteryLabel_audio->setPixmap(scaledPixmapBattery);
        bottomLeftLayout->addWidget(listaudios);
        bottomLeftLayout->addWidget(batteryLabel_audio); 
        layout->addLayout(bottomLeftLayout);
        QVBoxLayout *Vlayout = new QVBoxLayout();
        Vlayout->setContentsMargins(0, 0, 0, 0);         
        Vlayout->setSpacing(0);
        // Headphone
        Vlayout->addLayout(createSliderControl(QString::fromStdString(lang.getText("audiotab","headphonename")), 0, 63, A_control.getVolumeLevel(), headphoneSlider));
        // Capture
        Vlayout->addLayout(createSliderControl(QString::fromStdString(lang.getText("audiotab","Capturename")), 0, 31, A_control.getCaptureInputVolume(), captureSlider));
        // layout Input (Enum)
        Vlayout->addLayout(createComboControl(QString::fromStdString(lang.getText("audiotab","Inputname")), QString::fromStdString(A_control.getCaptureInputType())));
        layout->addLayout(Vlayout);
        
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer createAudioControlTab: " + std::string(e.what()));

    }
    return fifthTab;
}

// Helper to create slider + label + (optional) mute/enabled checkbox
QHBoxLayout* CameraViewer::createSliderControl(const QString &name, int min, int max, int value, QSlider*& slider) {
    QHBoxLayout *layout = new QHBoxLayout;
    QLabel *label = new QLabel(name);
    slider = new QSlider(Qt::Horizontal);
    slider->setRange(min, max);
    slider->setValue(value);
    slider->setTickPosition(QSlider::TicksBelow);    
    label->setMaximumWidth(0.25*Swidth);
    label->setMaximumHeight(Sheight/8);   
    slider->setMaximumWidth(0.5*Swidth);
    slider->setMaximumHeight(Sheight/8);
    layout->addWidget(label);
    layout->addWidget(slider);
    label->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
    label->setWordWrap(true);
    label->setStyleSheet("QLabel { color : blue; font-weight: bold;}"); 
    label->setAlignment(Qt::AlignCenter);
    return layout;
}

// Helper to create combo control
QHBoxLayout* CameraViewer::createComboControl(const QString &name, const QString &item) {
    QHBoxLayout *layout = new QHBoxLayout;
    QLabel *label = new QLabel(name);
    captureInputLabel = new QLabel(item);
    label->setMaximumWidth(0.25*Swidth);
    label->setMaximumHeight(Sheight/8);
    captureInputLabel->setMaximumWidth(0.40*Swidth);
    captureInputLabel->setMaximumHeight(Sheight/8);
    layout->addWidget(label);
    layout->addWidget(captureInputLabel);
    label->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
    label->setWordWrap(true);
    label->setStyleSheet("QLabel { color : blue; font-weight: bold;}"); 
    label->setAlignment(Qt::AlignCenter);
    captureInputLabel->setFont([](int size) { QFont font; font.setPointSize(size); return font; }(config.font_size));
    captureInputLabel->setWordWrap(true);
    captureInputLabel->setStyleSheet("QLabel { background-color : lightyellow; color : blue; font-weight: bold;}");
    captureInputLabel->setAlignment(Qt::AlignCenter);
    return layout;
}

void CameraViewer::readwifijson() {
    try {
        LOG_INFO("Reading WIFI file " + config.wifi_file);
        std::ifstream file(config.wifi_file, std::ifstream::binary);
        if (!file) {
            LOG_ERROR("Error: Could not open wifi file.");
            return ;
        }
        
        Json::CharReaderBuilder readerBuilder;
        std::string errors;
        bool parsingSuccessful = Json::parseFromStream(readerBuilder, file, &wifi, &errors);
        if (!parsingSuccessful) {
            LOG_ERROR("Error parsing JSON: " + errors);
            return;
        }
        if (wifi.isArray() && wifi.size() > 0 && wifi[0].isMember("uri")) {
            std::string uri = wifi[0]["uri"].asString();
            session.update_api_url("https://"+uri);
        } else {
            LOG_ERROR("Error: JSON format is not as expected." );
            return;
        }
        LOG_INFO("FINISH WIFI file");
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer readwifijson: " + std::string(e.what()));
    }
}

void CameraViewer::working_mode() {
    try {
        QString wifi_name;
        int _status = 0;
        if (_working_wifi == 1) {
            _status = session.check_status();
            if (_status == 0) {
                wifi_name = QString::fromStdString(config.wifi_connect);
                session.update_helmet_status(session.get_operator_status()+"_standby");
                session.generate_notify();   
                stackedWidget->setCurrentIndex(0);         
                qrcode_label->setVisible(false);
                legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
            }
            else {
                complete_standalone_transition(false);              
            }
        }
        else if (_working_wifi == 2) {
            _status = session.check_status();
            if (_status == 0) {
                wifi_name = QString::fromStdString(config.wifi_connect);
                session.update_helmet_status(session.get_operator_status() + "_standby");
                session.generate_notify();   
                stackedWidget->setCurrentIndex(0);         
                qrcode_label->setVisible(false);
                legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
            }
            else {
                complete_standalone_transition(false);
            }
        }
        else if (_working_wifi == 3) {
            wifi_name = QString::fromStdString(config.wifi_icon_novpn);
        }
        else if (_working_wifi == 4) {
            wifi_name = QString::fromStdString(config.wifi_icon_novpn);
        }
        else if (_working_wifi == 5) {
            wifi_name = QString::fromStdString(config.wifi_icon_nohttp);
        }
        else if (_working_wifi == 6) {
            wifi_name = QString::fromStdString(config.wifi_icon_nohttp);
        }
        else if (_working_wifi == 7) {
            wifi_name = QString::fromStdString(config.wifi_icon_nowifi);
        }
        else if (_working_wifi == 8) {
            wifi_name = QString::fromStdString(config.wifi_icon_nowifi);
        }
        else if(_working_wifi == 0) {
            if (network.check_wifi(10) == 1) {
                qrcode_label->setVisible(true);
                qrcode_label->setText(QString::fromStdString(lang.getText("error_message","NOTCONNECT")));
                legend_label3->setVisible(false);
            }
            else if (network.check_wifi(10) == 2) {
                qrcode_label->setVisible(true);
                qrcode_label->setText(QString::fromStdString(lang.getText("error_message","NOWIFI")));
                legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","wifi")));
                legend_label3->setVisible(false);
            }
            // else {
            //     complete_standalone_transition(true);
            // }            
            wifi_name = QString::fromStdString(config.wifi_icon_nowifi);
            QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 600000));
        }
        if (voiceThread->getstatus())        
            voiceThread->start();
        int battery_wifi_Width = Swidth/10;
        int default_height = Sheight/10;
        QPixmap pixmap_wifi(wifi_name);
        
        QPixmap scaledPixmapWifi = pixmap_wifi.scaled(
            battery_wifi_Width/3,
            default_height,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );
        wifiLabel->setPixmap(scaledPixmapWifi);
        if (_working_wifi == 1 || _working_wifi == 2) {
            current_mode = session.get_helmet_status();

            if (current_mode.find("offline") == std::string::npos || _status == 0) {
                // Open the video stream using OpenCV and GStreamer
                std::string _loopback = config._vl_loopback;
                _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
                cameraThread->update_camera_pipeline(_loopback);                            
                int _cap = cameraThread->init();
                if (_cap == -1) { 
                    image = QImage(Swidth, Sheight, QImage::Format_RGB888);
                    image.fill(Qt::black);  // Fill the image with black
                    QPainter painter(&image);
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.setPen(QColor(Qt::green));
                    QFont font("Arial", 30);
                    painter.setFont(font);
                    painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                    painter.end();
                    pixmap = QPixmap::fromImage(image);
                    videoPixmapItem->setPixmap(pixmap);
                    videoScene->setSceneRect(videoPixmapItem->boundingRect());
                    videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                    videoView->centerOn(videoPixmapItem);
                    videoView->viewport()->update();  
                    legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","camera")));
                    status_label->setVisible(false);           
                    session.stop_notify();  
                    session.update_helmet_status("nocamera");
                    current_mode = "nocamera";
                    return;
                }    
                camera_rotate = false;
                cameraThread->startCapturing(config.period);
                // standbytimer->setInterval(config.standby_delay);
                // standbytimer->setSingleShot(true);            
                // standbytimer->start();      
                
            }     
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer working_mode: " + std::string(e.what()));
    }   
}

void CameraViewer::FSM(nlohmann::json _data, std::string _event) {
    try {    
        LOG_INFO("current mode " + current_mode + "--" + _event);
        if (entering_standalone && current_mode.find("Standalone") != std::string::npos) {
            entering_standalone = false;
            complete_standalone_transition(false);
        }
        if (current_mode == "Low_power") {
            if (_event == "active")
                Exit_Low_Power_Mode();        
        }
        else if (current_mode.find("standby") != std::string::npos) {
            current_mode = session.get_helmet_status();
            if (_event == "active") {   
                voiceThread->stopThread();                          
                // session.update_helmet_status(session.get_operator_status()+"_active");
                
                last_processed_event = -1;
                id_call = _data["id_call"].get<int>();
                _ipstream = _data["ipv4"].get<std::string>();
                config.updateAudioOutcoming(_ipstream);
                streamstart(_ipstream);
                start_audio_channel();    
                QPixmap pixmap_avatar(QString::fromStdString(_data["avatar"].get<std::string>()));
                QPixmap scaledPixmapAvatr = pixmap_avatar.scaled(
                    3*Swidth/20,
                    Sheight/10,
                    Qt::KeepAspectRatio,
                    Qt::FastTransformation 
                );
                avatarLabel->setPixmap(scaledPixmapAvatr);
                legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","close")));
                legend_label2->setVisible(false);
                legend_label3->setVisible(false);
                status_label->setVisible(false);
                user_label->setText(QString::fromStdString(_data["username"].get<std::string>()));      
                user_label->setVisible(true);     
                send_audio_settings(); 
            }
        }
        else if (current_mode.find("request") != std::string::npos) {
            current_mode = session.get_helmet_status();
            if (_event == "active") {
                voiceThread->stopThread();                 
                // session.update_helmet_status(session.get_operator_status()+"_active");
                last_processed_event = -1;
                id_call = _data["id_call"].get<int>();                
                _ipstream = _data["ipv4"].get<std::string>();
                config.updateAudioOutcoming(_ipstream);
                streamstart(_ipstream); 
                start_audio_channel();
                QPixmap pixmap_avatar(QString::fromStdString(_data["avatar"].get<std::string>()));
                QPixmap scaledPixmapAvatr = pixmap_avatar.scaled(
                    Swidth/10,
                    Sheight/10,
                    Qt::KeepAspectRatio,
                    Qt::FastTransformation 
                );
                avatarLabel->setPixmap(scaledPixmapAvatr);
                legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","close")));
                legend_label2->setVisible(false);
                legend_label3->setVisible(false);
                status_label->setVisible(false);
                user_label->setText(QString::fromStdString(_data["username"].get<std::string>()));
                user_label->setVisible(true);
                send_audio_settings();
                        
            }
        }
        else if (current_mode.find("active") != std::string::npos)  {
            current_mode = session.get_helmet_status();
            if (_event == "active") {
                if (_data.contains("im") && _data["im"].is_array() && _data["im"].size()>0) {
                    auto last_im = _data["im"].back();                
                    if (id_im != last_im["idMessage"].get<int>()) {
                        id_im = last_im["idMessage"].get<int>();
                        display_messages(last_im["text"]);
                        msgtimer = std::chrono::high_resolution_clock::now();
                    } else {
                        auto now = std::chrono::high_resolution_clock::now();
                        auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(now - msgtimer).count();

                        if (elapsed_time >= 15) {
                            message->clear();
                            message->setVisible(false);
                        }
                    }
                    
                } else {
                    message->setVisible(false);
                    message->clear();
                }
                // Display procedure details
                if (_data.contains("procedure") && _data["procedure"].is_object() ) {
                    if (_data["procedure"].contains("tasks") && _data["procedure"]["tasks"].is_array()) {
                        task_sharing = true;
                        if (tasklist.empty()) {
                            for (const auto &task : _data["procedure"]["tasks"]) {
                                std::map<std::string, std::string> taskMap = {
                                    {"order", std::to_string(task["order"].get<int>())},
                                    {"text", task["text"].get<std::string>()},
                                    {"completed", task["completed"] ? "true" : "false"}
                                };
                                procedure["tasks"].push_back(taskMap);
                            }
                            if (_data["procedure"].contains("name")) {
                                std::string procedureName = _data["procedure"]["name"].get<std::string>();
                                procedure["name"] = { {{"name", procedureName}} };
                            }
                            drawTaskList();
                        }
                    } 
                }
                else {       
                    task_name->clear();
                    task_name->setVisible(false);
                    task_list->clear();
                    task_list->setVisible(false);
                    legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","close")));
                    legend_label2->setVisible(false);
                    legend_label3->setVisible(false);
                    legend_label2->clear();
                    tasklist.clear();
                    procedure.clear();
                    task_sharing = false;
                }
                if (_data.contains("events") && _data["events"].is_array()) {      
                    for (const auto& event : _data["events"]) {
                        if (event.is_object()) {  // Ensure each event is a valid object
                            process_event(event);
                        }
                    }
                }
                id_call = _data["id_call"].get<int>();
            }
            else if (_event == "standby") {
                AudioReset();
                // session.update_helmet_status(session.get_operator_status() + "_standby");
                session.terminate_support();
                remoteend();
                streamend();
                close_audio_channel();
                QPixmap pixmap_avatar(QString::fromStdString(config.avatar_file));
                // QPixmap transformedPixmap_avatr = pixmap_avatar.transformed(transform);
                QPixmap scaledPixmapAvatr = pixmap_avatar.scaled(
                    3*Swidth/20,
                    Sheight/10,
                    Qt::KeepAspectRatio,
                    Qt::FastTransformation 
                );
                avatarLabel->setPixmap(scaledPixmapAvatr);
                legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","langs")));
                legend_label2->setVisible(true);
                legend_label2->setText(QString::fromStdString(lang.getText("defaulttab","standalone")));
                legend_label3->setVisible(true);
                legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
                status_label->setText(QString::fromStdString(lang.getText("defaulttab","setup")));
                status_label->setVisible(true);
                task_name->setVisible(false);
                task_name->clear();
                task_list->setVisible(false);
                task_list->clear();
                message->setVisible(false);
                message->clear();
                user_label->setVisible(false);
                voiceThread->start();
            }
        }
        else if (current_mode.find("qrcode") != std::string::npos) {
            if (_event == "stop_scan_positive") {
                if (_working_wifi == 0) {
                    network.enable_wifi();
                    int Wconnected = 2;
                    while(Wconnected != 0){
                        Wconnected = network.check_wifi();
                    }
                }
                readwifijson();
                network.init();
                network.setForceConnection(true);
                network.setConnections(wifi, true);
                _working_wifi = network.run();
                LOG_INFO("_working_wifi = "+ std::to_string(_working_wifi));
                working_mode();
                stop_qrcode();                
                QString title = QString::fromStdString(lang.getText("standalonetab", "Wifititle"));
                if (wifi.isArray() && wifi.size() > 0 && wifi[0].isObject()) {
                    QString ssid = QString::fromStdString(wifi[0]["ssid"].asString());
                    QString uri = QString::fromStdString(wifi[0]["uri"].asString());
                    QString message = title + "\nSSID: " + ssid + "\nURI: " + uri;
                    floatingMessage->showMessage(message, 2);
                } else {
                    QString error_message = title + "\nError: Invalid WiFi data";
                    floatingMessage->showMessage(error_message, 2);
                }
            }
            else if (_event == "stop_scan_negative") {       
                stop_qrcode();               
            }

        } 
        else if (current_mode.find("nocamera") != std::string::npos) {
            if (_event == "standby") {
                session.update_helmet_status(session.get_operator_status() + "_standby");
                current_mode = session.get_helmet_status();
            }
        }       
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer FSM: " + std::string(e.what()));
    }
}

void CameraViewer::handle_update_frame(cv::Mat _frame) {
    try {
        // auto frame_start = std::chrono::high_resolution_clock::now();
        current_mode = session.get_helmet_status();
        // LOG_INFO("current mode " + current_mode);
        if (!camera_rotate && config.rotate == 1) {            
            std::string command = "i2cset -f -y 1 0x3C 0x38 0x20 0x47 i";
            FILE* pipe = popen(command.c_str(), "r");
            if (!pipe) {
                LOG_ERROR("popen() failed!");
                throw std::runtime_error("popen() failed!");
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            command = "i2cset -f -y 1 0x3C 0x38 0x21 0x01 i";
            pipe = popen(command.c_str(), "r");
            if (!pipe) {
                LOG_ERROR("popen() failed!");
                throw std::runtime_error("popen() failed!");
            }
            camera_rotate = true;
        }
        if (!_frame.empty()) {
            if (current_mode.find("qrcode") != std::string::npos) {  
                if (qrcode_counter %50 == 0) {
                    processQRCode(_frame.clone());      
                }                         
                qrcode_counter += 1;
            }
            image = QImage(_frame.data, _frame.cols, _frame.rows, _frame.step, QImage::Format_BGR888);
        } else {
            image = QImage(Swidth, Sheight, QImage::Format_RGB888);
            image.fill(Qt::black);  // Fill the image with black
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QColor(Qt::green));
            QFont font("Arial", 30);
            painter.setFont(font);
            painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOFRAME")));
            painter.end();
        }
        auto frame_start = std::chrono::high_resolution_clock::now();
        pixmap = QPixmap::fromImage(image);//.scaled(videoLabel->size(), Qt::KeepAspectRatio,Qt::FastTransformation);
        if (current_mode.find("Standalone") == std::string::npos) {
            videoPixmapItem->setPixmap(pixmap);
            videoScene->setSceneRect(videoPixmapItem->boundingRect());
            videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
            videoView->centerOn(videoPixmapItem);
            videoView->viewport()->update(); // Trigger redraw
            auto capture_end = std::chrono::high_resolution_clock::now();
            // Timing calculations
            double capture_time = std::chrono::duration<double, std::milli>(
                capture_end - frame_start).count();
        }
        else {
            videoPixmapItem2->setPixmap(pixmap);
            videoScene2->setSceneRect(videoPixmapItem2->boundingRect());
            videoView2->fitInView(videoScene2->sceneRect(), Qt::KeepAspectRatioByExpanding); 
            videoView2->centerOn(videoPixmapItem2);
            videoView2->viewport()->update(); // Trigger redraw
        }

        // std::cout << "capture time: " << capture_time << std::endl;
        // LOG_INFO("capture_time " + std::to_string(capture_time));
        // if (!screenshot) {
        //     screenshot = true;
        //     QPixmap pixmap1 = this->grab();
        //     // Option 2: Capture entire CameraViewer (uncomment to use)
        //     // QPixmap pixmap = this->grab();
        //     pixmap1.save("/home/x_user/my_camera_project/screenshot.png", "PNG");            
        //     LOG_INFO("lastPixmapSize : " + std::to_string(lastPixmapSize.width()) + "x" + std::to_string(lastPixmapSize.height()));
        //     LOG_INFO("videoView size: " + std::to_string(videoView->size().width()) + "x" + std::to_string(videoView->size().height()));
        //     QRectF rect = videoScene->sceneRect();
        //     LOG_INFO("videoScene rect: (" + std::to_string(rect.x()) + ", " + std::to_string(rect.y()) + ", " +
        //             std::to_string(rect.width()) + ", " + std::to_string(rect.height()) + ")");
        //     LOG_INFO("pixmap size: " + std::to_string(pixmap.size().width()) + "x" + std::to_string(pixmap.size().height()));
        //     LOG_INFO("Widget screenshot saved to /home/x_user/my_camera_project/screenshot.png");
        // }
        } catch (const std::exception& e) {
            LOG_ERROR("An error occurred in CameraViewer handle_update_frame: " + std::string(e.what()));
        }
}

void CameraViewer::handle_update_video(cv::Mat _frame) {
    try {
        if (!videoPixmapItem1) {
            LOG_ERROR("videoPixmapItem1 is null, skipping update");
            return; // Exit if the pixmap item isn’t initialized
        }
        if (!_frame.empty()) {
            image = QImage(_frame.data, _frame.cols, _frame.rows, _frame.step, QImage::Format_BGR888);
        } else {
            image = QImage(Swidth, Sheight, QImage::Format_RGB888);
            image.fill(Qt::black);  // Fill the image with black
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QColor(Qt::green));
            QFont font("Arial", 30);
            painter.setFont(font);
            painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOFRAME")));
            painter.end();
        }
        pixmap1 = QPixmap::fromImage(image);
        videoPixmapItem1->setPixmap(pixmap1);
        videoScene1->setSceneRect(videoPixmapItem1->boundingRect());
        videoView1->fitInView(videoScene1->sceneRect(), Qt::IgnoreAspectRatio); 
        videoView1->centerOn(videoPixmapItem1);
        videoView1->viewport()->update(); // Trigger redraw    
        if (!screenshot) {
            screenshot = true;
            QPixmap pixmap2 = this->grab();
            // Option 2: Capture entire CameraViewer (uncomment to use)
            // QPixmap pixmap = this->grab();
            pixmap2.save("/home/x_user/my_camera_project/screenshot11.png", "PNG");            
            LOG_INFO("videoView1 size: " + std::to_string(videoView1->size().width()) + "x" + std::to_string(videoView1->size().height()));
            QRectF rect = videoScene1->sceneRect();
            LOG_INFO("videoScene1 rect: (" + std::to_string(rect.x()) + ", " + std::to_string(rect.y()) + ", " +
                    std::to_string(rect.width()) + ", " + std::to_string(rect.height()) + ")");
            LOG_INFO("pixmap size: " + std::to_string(pixmap2.size().width()) + "x" + std::to_string(pixmap2.size().height()));
            LOG_INFO("Widget screenshot saved to /home/x_user/my_camera_project/screenshot.png");
        }        
        } catch (const std::exception& e) {
            LOG_ERROR("An error occurred in CameraViewer handle_update_video: " + std::string(e.what()));
        }
}

void CameraViewer::button_pressed() {
    try {
        if (!btn_click && !standalone_language_transition) {
            clicks += 1;
            QMetaObject::invokeMethod(clicktimer, "start", Qt::QueuedConnection, Q_ARG(int, 2000));
            LOG_INFO("Button pressed!");

            // Marshal GUI operations to the main thread
            QMetaObject::invokeMethod(this, [this]() {
                floatingMessage->showMessage(QString::fromStdString(lang.getText("standalonetab","msgboxbutton")) + QString::number(clicks), 1);   
            }, Qt::QueuedConnection);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer button_pressed: " + std::string(e.what()));
    }
}

void CameraViewer::report_and_reset_clicks() {
    try {
        btn_click = true;
        if (clicks !=0) {
            LOG_INFO("Handling button clicks " + std::to_string(clicks) + " current mode " + current_mode);
            if (current_mode == "Low_power" and clicks> 0)
                Exit_Low_Power_Mode();
            else if (current_mode.find("standby") != std::string::npos) {                
                if (clicks == 1) {
                    if (stackedWidget->currentIndex() == 4) {  
                        A_control.setVolumeLevel(A_control.getVolumeLevel() + 5);
                        if (headphoneSlider) {
                            headphoneSlider->blockSignals(true);
                            headphoneSlider->setValue(A_control.getVolumeLevel());
                            headphoneSlider->blockSignals(false);
                        }
                    }
                    else {
                        if (config.default_language == "English")
                            changeLanguage("Русский");
                        else if (config.default_language == "Русский")
                            changeLanguage("عربي");
                        else if (config.default_language == "عربي")
                            changeLanguage("English");
                    }
                }
                else if (clicks == 2) {
                    if (stackedWidget->currentIndex() == 4) {  
                        A_control.setVolumeLevel(A_control.getVolumeLevel() - 5);
                        if (headphoneSlider) {
                            headphoneSlider->blockSignals(true);
                            headphoneSlider->setValue(A_control.getVolumeLevel());
                            headphoneSlider->blockSignals(false);
                        }
                    }
                    else {
                        session.stop_notify();
                        session.standalone_request();
                        entering_standalone = true; // Set flag
                        current_mode = session.get_helmet_status();
                        if (current_mode.find("Standalone") != std::string::npos) {
                            entering_standalone = false; // Clear flag if already in standalone
                            complete_standalone_transition(false);
                        }
                    }
                }
                else if (clicks == 3) {
                    if (stackedWidget->currentIndex() == 4) {  
                        A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() + 5);
                        if (captureSlider) {
                            captureSlider->blockSignals(true);
                            captureSlider->setValue(A_control.getCaptureInputVolume());
                            captureSlider->blockSignals(false);
                        }
                    }
                    else {
                        session.update_helmet_status(session.get_operator_status() + "_request");
                        session.request_support();
                        current_mode = session.get_helmet_status(); 
                        legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","close")));
                        legend_label2->setVisible(false);
                        legend_label3->setVisible(false);      
                        status_label->setVisible(false);
                        user_label->setText(QString::fromStdString(lang.getText("defaulttab","call_status")));
                        user_label->setVisible(true);
                    }
                }
                else if (clicks == 4) {
                    //Qrcode 
                    if (stackedWidget->currentIndex() == 4) {  
                        int newCaptureInputVolume = A_control.getCaptureInputVolume() - 5;
                        if (newCaptureInputVolume > 20 ) {
                            A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() - 5);
                            if (captureSlider) {
                                captureSlider->blockSignals(true);
                                captureSlider->setValue(newCaptureInputVolume);
                                captureSlider->blockSignals(false);
                            }
                        }
                    }
                    else {
                        session.stop_notify();
                        start_qrcode();
                        current_mode = "qrcode";
                    }
                }
                else if (clicks == 5) {
                    if (stackedWidget->currentIndex() == 4) {  
                        AudioReset();
                    }
                }
                else if (clicks == 6) {
                    if (stackedWidget->currentIndex() == 4) {  
                        stackedWidget->setCurrentIndex(0);
                    }
                }
            }
            else if (current_mode.find("request") != std::string::npos) {
                if (clicks == 3) {
                    session.update_helmet_status(session.get_operator_status() + "_standby");
                    session.terminate_support();
                    current_mode = session.get_helmet_status();
                    legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","langs")));
                    legend_label2->setVisible(true);              
                    legend_label2->setText(QString::fromStdString(lang.getText("defaulttab","standalone")));
                    legend_label3->setVisible(true);
                    legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
                    status_label->setText(QString::fromStdString(lang.getText("defaulttab","setup")));
                    status_label->setVisible(true);
                    task_name->setVisible(false);
                    task_name->clear();
                    task_list->setVisible(false);
                    task_list->clear();
                    message->setVisible(false);
                    message->clear();
                    user_label->setVisible(false);
                    user_label->clear();
                }
                else if (clicks == 6) {
                    //reboot hdmi
                }
            }
            else if (current_mode.find("active") != std::string::npos) {
                if (clicks == 1) {
                    if (task_sharing){
                        session.handle_tasks_next();
                        if (_index < static_cast<int>(tasklist.size()))
                            _index += 1;
                        drawTaskList();
                    }
                }
                else if (clicks == 2) {
                    if (task_sharing){
                        session.handle_tasks_back();
                        if (_index > 0)
                            _index -= 1;
                        drawTaskList();
                    }
                }
                else if (clicks == 3) {
                    session.update_helmet_status(session.get_operator_status() + "_standby");
                    session.terminate_support();
                    current_mode = session.get_helmet_status();
                    remoteend();
                    streamend();
                    close_audio_channel();
                    QPixmap pixmap_avatar(QString::fromStdString(config.avatar_file));
                    QPixmap scaledPixmapAvatr = pixmap_avatar.scaled(
                        3*Swidth/20,
                        Sheight/10,
                        Qt::KeepAspectRatio,
                        Qt::FastTransformation 
                    );
                    avatarLabel->setPixmap(scaledPixmapAvatr);
                    legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","langs")));
                    legend_label2->setVisible(true);                   
                    legend_label2->setText(QString::fromStdString(lang.getText("defaulttab","standalone")));
                    legend_label3->setVisible(true);
                    legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
                    status_label->setText(QString::fromStdString(lang.getText("defaulttab","setup")));
                    status_label->setVisible(true);
                    task_name->setVisible(false);
                    task_name->clear();
                    task_list->setVisible(false);
                    task_list->clear();
                    message->setVisible(false);
                    message->clear();
                    user_label->setVisible(false);
                    user_label->clear();
                }
            }
            else if (current_mode.find("qrcode") != std::string::npos) {
                if (clicks == 1) {
                    //Qrcode
                    stop_qrcode();
                    session.generate_notify();
                    current_mode = session.get_helmet_status();    
                }
            }
            else if (current_mode.find("offline") != std::string::npos) {
                if (clicks == 1) {
                    if (stackedWidget->currentIndex() == 4) {  
                        A_control.setVolumeLevel(A_control.getVolumeLevel() + 5);
                        if (headphoneSlider) {
                            headphoneSlider->blockSignals(true);
                            headphoneSlider->setValue(A_control.getVolumeLevel());
                            headphoneSlider->blockSignals(false);
                        }
                    }
                    else {
                        network.enable_wifi();
                        int Wconnected = 2;
                        while(Wconnected != 0){
                            Wconnected = network.check_wifi();
                        }
                        readwifijson();
                        network.init();
                        network.setConnections(wifi);
                        _working_wifi = network.run();
                        LOG_INFO("_working_wifi = "+ std::to_string(_working_wifi));
                        working_mode();
                    }
                }
                else if (clicks == 2) {
                    if (stackedWidget->currentIndex() == 4) {  
                        A_control.setVolumeLevel(A_control.getVolumeLevel() - 5);
                        if (headphoneSlider) {
                            headphoneSlider->blockSignals(true);
                            headphoneSlider->setValue(A_control.getVolumeLevel());
                            headphoneSlider->blockSignals(false);
                        }
                    }
                    else {
                        complete_standalone_transition(true);
                    }
                }
                else if (clicks == 3) {
                    if (stackedWidget->currentIndex() == 4) {  
                        A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() + 5);
                        if (captureSlider) {
                            captureSlider->blockSignals(true);
                            captureSlider->setValue(A_control.getCaptureInputVolume());
                            captureSlider->blockSignals(false);
                        }
                    }
                }
                else if (clicks == 4) {
                    if (stackedWidget->currentIndex() == 4) {  
                        int newCaptureInputVolume = A_control.getCaptureInputVolume() - 5;
                        if (newCaptureInputVolume > 20 ) {
                            A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() - 5);
                            if (captureSlider) {
                                captureSlider->blockSignals(true);
                                captureSlider->setValue(newCaptureInputVolume);
                                captureSlider->blockSignals(false);
                            }
                        }
                    }
                    else {
                        //Qrcode 
                        std::string _loopback = config._vl_loopback;
                        _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
                        cameraThread->update_camera_pipeline(_loopback);
                        int _cap = cameraThread->init();
                        if (_cap == -1) { 
                            image = QImage(Swidth, Sheight, QImage::Format_RGB888);
                            image.fill(Qt::black);  // Fill the image with black
                            QPainter painter(&image);
                            painter.setRenderHint(QPainter::Antialiasing);
                            painter.setPen(QColor(Qt::green));
                            QFont font("Arial", 30);
                            painter.setFont(font);
                            painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                            painter.end();
                            pixmap = QPixmap::fromImage(image);
                            videoPixmapItem->setPixmap(pixmap);
                            videoScene->setSceneRect(videoPixmapItem->boundingRect());
                            videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                            videoView->centerOn(videoPixmapItem);
                            videoView->viewport()->update();                
                            return;
                        }    
                        camera_rotate = false;
                        cameraThread->startCapturing(config.period);
                        start_qrcode();
                        current_mode = "qrcode";
                    }
                }
                else if (clicks == 5) {
                    if (stackedWidget->currentIndex() == 4) {  
                        AudioReset();
                    }
                }
                else if (clicks == 6) {
                    if (stackedWidget->currentIndex() == 4) {  
                        stackedWidget->setCurrentIndex(0);
                    }
                }                
            }
            else if (current_mode.find("Standalone") != std::string::npos) {
                LOG_INFO("scenaraio  " + std::to_string(scenaraio));
                if (scenaraio == 0) {
                    if (clicks == 1) {
                        scenaraio = 1;
                        showFilesList(config.todo,".pdf");
                    }
                    else if (clicks == 2) {
                        scenaraio = 2;
                        showFilesList(config.todo,".txt");
                    }
                    else if (clicks == 3) {
                        scenaraio = 3;
                        showFilesList(config.todo,".mp4");
                    }
                    else if (clicks == 4) {
                        if (config.default_language == "English")
                            changeLanguage("Русский");
                        else if (config.default_language == "Русский")
                            changeLanguage("عربي");
                        else if (config.default_language == "عربي")
                            changeLanguage("English");
                    }
                    else if (clicks == 5) {
                        stackedWidget->setCurrentIndex(4);
                        scenaraio = 5;
                    }
                    else if (clicks == 6) {
                        cameraThread->releasecamera();    
                        if (pdf.getPageCount() > 2)
                            pdf.saveToFile(lang.getText("pdf_message","name")+getCurrentDateTime()+".pdf");
                        if (config.debug == 0) {
                            network.enable_wifi();
                            int Wconnected = 2;
                            while(Wconnected != 0){
                                Wconnected = network.check_wifi();
                            }
                            network.init();
                            network.setConnections(wifi);
                            _working_wifi = network.run();
                            working_mode();
                        }
                        else {                           
                            std::string _loopback = config._vl_loopback;
                            _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
                            cameraThread->update_camera_pipeline(_loopback);
                            int _cap = cameraThread->init();
                            if (_cap == -1) { 
                                image = QImage(Swidth, Sheight, QImage::Format_RGB888);
                                image.fill(Qt::black);  // Fill the image with black
                                QPainter painter(&image);
                                painter.setRenderHint(QPainter::Antialiasing);
                                painter.setPen(QColor(Qt::green));
                                QFont font("Arial", 30);
                                painter.setFont(font);
                                painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                                painter.end();
                                pixmap = QPixmap::fromImage(image);
                                videoPixmapItem->setPixmap(pixmap);
                                videoScene->setSceneRect(videoPixmapItem->boundingRect());
                                videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                                videoView->centerOn(videoPixmapItem);
                                videoView->viewport()->update();  
                                legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","camera")));
                                status_label->setVisible(false);           
                                session.stop_notify();  
                                session.update_helmet_status("nocamera");
                                current_mode = "nocamera";
                                return;
                            }    
                            camera_rotate = false;
                            cameraThread->startCapturing(config.period);
                            session.update_helmet_status(session.get_operator_status() + "_standby");
                            current_mode = session.get_helmet_status();
                            session.generate_notify();   
                            stackedWidget->setCurrentIndex(0);    
                        }
                    }                
                }
                else if (scenaraio == 1) {
                    if (clicks <= (static_cast<int>(pdfFiles.size()))) {
                        scenaraio = 11;
                        showpdfmode();
                        LoadPDF(pdfFiles[clicks-1]);
                    }
                    else {
                        scenaraio = 0;
                        pdfFiles.clear();
                        txtFiles.clear();
                        mp4Files.clear();
                        showdefaultstandalone();
                    }
                }
                else if (scenaraio == 2) {
                    if (clicks <= (static_cast<int>(txtFiles.size()))) {
                        scenaraio = 22;
                        showtxtmode();
                        loadTXT(txtFiles[clicks-1]);
                    }
                    else {
                        scenaraio = 0;
                        pdfFiles.clear();
                        txtFiles.clear();
                        mp4Files.clear();                        
                        showdefaultstandalone();        
                    } 
                }
                else if (scenaraio == 3) {
                    if (clicks <= (static_cast<int>(mp4Files.size()))) {
                        scenaraio = 33;
                        LoadMP4(mp4Files[clicks-1]);
                        showvideomode();
                    }
                    else {
                        scenaraio = 0;
                        pdfFiles.clear();
                        txtFiles.clear();
                        mp4Files.clear();                        
                        showdefaultstandalone();
                    } 
                }
                else if (scenaraio == 5) {
                    if (clicks == 1) {
                        A_control.setVolumeLevel(A_control.getVolumeLevel() + 5);
                        if (headphoneSlider) {
                            headphoneSlider->blockSignals(true);
                            headphoneSlider->setValue(A_control.getVolumeLevel());
                            headphoneSlider->blockSignals(false);
                        }
                    }
                    else if (clicks == 2) {
                        A_control.setVolumeLevel(A_control.getVolumeLevel() - 5);
                        if (headphoneSlider) {
                            headphoneSlider->blockSignals(true);
                            headphoneSlider->setValue(A_control.getVolumeLevel());
                            headphoneSlider->blockSignals(false);
                        }
                    }
                    else if (clicks == 3) {
                        A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() + 5);
                        if (captureSlider) {
                            captureSlider->blockSignals(true);
                            captureSlider->setValue(A_control.getCaptureInputVolume());
                            captureSlider->blockSignals(false);
                        }
                    }
                    else if (clicks == 4) {
                        int newCaptureInputVolume = A_control.getCaptureInputVolume() - 5;
                        if (newCaptureInputVolume > 20 ) {
                            A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() - 5);
                            if (captureSlider) {
                                captureSlider->blockSignals(true);
                                captureSlider->setValue(newCaptureInputVolume);
                                captureSlider->blockSignals(false);
                            }
                        }
                    }
                    else if (clicks == 5) {
                        AudioReset();
                    }
                    else if (clicks == 6) {
                        stackedWidget->setCurrentIndex(1);
                        scenaraio = 0;
                    }
                }
                else if (scenaraio == 11) {
                    if (clicks == 1)
                        nextPage();
                    else if (clicks == 2)
                        previousPage();
                    else if (clicks == 3)
                        zoomIn();
                    else if (clicks == 4)
                        zoomOut();
                    else if (clicks == 5)
                        scrollUp();
                    else if (clicks == 6)
                        scrollDown();
                    else if (clicks == 7)
                        scrollLeft();
                    else if (clicks == 8)
                        scrollRight();
                    else if (clicks == 9) {
                        scenaraio = 1;
                        currentPage = 0;
                        zoomFactor = 1.5;
                        scene->clear();
                        document = nullptr;
                        pdfFiles.clear();
                        showFilesList(config.todo,".pdf");
                        cameraThread->stopCapturing();
                    }                
                }
                else if (scenaraio == 22) {
                    if (clicks == 1) {
                        nextTask();
                    }
                    else if (clicks == 2) {
                        prevTask();
                    }
                    else if (clicks == 3) {
                        scenaraio = 2;
                        currentTaskIndex = 0;
                        scene->clear();
                        txtFiles.clear();
                        showFilesList(config.todo,".txt");
                        cameraThread->stopCapturing();
                    } 
                }
                else if (scenaraio == 222) {
                    if (clicks == 1) {
                        nextPage();
                        nextTask();
                    }
                    else if (clicks == 2) {
                        previousPage();
                        prevTask();
                    }
                    else if (clicks == 3) {
                        zoomIn();
                    }
                    else if (clicks == 4) {
                        zoomOut();
                    }
                    else if (clicks == 5) {
                        scrollUp();
                    }
                    else if (clicks == 6) {
                        scrollDown();
                    }
                    else if (clicks == 7) {
                        scrollRight();
                    }
                    else if (clicks == 8) {
                        scrollLeft();
                    }
                    else if (clicks == 9) {
                        scenaraio = 2;
                        currentPage = 0;
                        zoomFactor = 1.5;                    
                        currentTaskIndex = 0;
                        pdfFiles.clear();
                        txtFiles.clear();
                        scene->clear();
                        document = nullptr;
                        showFilesList(config.todo,".txt");
                        cameraThread->stopCapturing();
                    }                
                }
                else if (scenaraio == 33) {
                    if (clicks == 1) {         
                        if (!videoThread->getStop()) {
                            videoThread->startPlaying();
                            listFiles->item(0)->setText("1- " + QString::fromStdString(lang.getText("standalonetab","play")));
                            listvideos->item(0)->setText("1- " + QString::fromStdString(lang.getText("standalonetab","play")));
                        }
                        else {
                            videoThread->startPlaying();
                            listFiles->item(0)->setText("1- " + QString::fromStdString(lang.getText("standalonetab","stop")));                            
                            listvideos->item(0)->setText("1- " + QString::fromStdString(lang.getText("standalonetab","stop")));
                        }
                    }
                    else if (clicks == 2) {
                        if (!videoThread->getStop()) {
                            if (!videoThread->getPause()) {
                                listFiles->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","stop")));                           
                                listvideos->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","stop")));
                            }
                            else {
                                listFiles->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","pause")));                           
                                listvideos->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","pause")));
                            }
                            videoThread->playPause();
                        }
                    }
                    else if (clicks == 3) {
                        videoThread->seekBackward(5000);
                    }                  
                    else if (clicks == 4) {
                        videoThread->seekForward(5000);
                    }                
                    else if (clicks == 5) {                       
                        videoThread->volumeChanged(qMin(videoThread->getVolume() + 10, 80));
                    }                
                    else if (clicks == 6) {
                        videoThread->volumeChanged(qMax(videoThread->getVolume() - 10, 5));
                    }
                    else if (clicks == 7) {
                        if (!videoThread->getStop()) {
                            videoThread->stopPlaying();   // Stops the timer
                            videoThread->releasevideo(); // Releases capture
                        }
                        // Wait for video thread to fully stop (optional, if needed)
                        while (!videoThread->getStop()) {
                           std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                        videoScene1->clear();
                        videoPixmapItem1 = nullptr;
                        scenaraio = 3;
                        videoScene1->clear();
                        mp4Files.clear();
                        showFilesList(config.todo,".mp4");
                    } 
                }                
            }
            else if (current_mode.find("nocamera") != std::string::npos) {
                if (clicks == 4) {
                    if (stackedWidget->currentIndex() == 4) {  
                        stackedWidget->setCurrentIndex(0);
                    }                    
                }
                else if (clicks == 6) {
                    //reboot hdmi 
                }
                else if (clicks == 2) {
                    if (stackedWidget->currentIndex() == 4) {  
                        stackedWidget->setCurrentIndex(0);
                    }
                    else {
                        complete_standalone_transition(true);
                    }
                }
                else if (clicks == 1) {
                    if (stackedWidget->currentIndex() == 4) {  
                        AudioReset();
                    }
                    else {
                        if (config.default_language == "English")
                            changeLanguage("Русский");
                        else if (config.default_language == "Русский")
                            changeLanguage("عربي");
                        else if (config.default_language == "عربي")
                            changeLanguage("English");
                    }
                }
                else if (clicks == 3) {
                    std::string _loopback = config._vl_loopback;
                    _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
                    cameraThread->update_camera_pipeline(_loopback);
                    int _cap = cameraThread->init();
                    if (_cap == -1) { 
                        image = QImage(Swidth, Sheight, QImage::Format_RGB888);
                        image.fill(Qt::black);  // Fill the image with black
                        QPainter painter(&image);
                        painter.setRenderHint(QPainter::Antialiasing);
                        painter.setPen(QColor(Qt::green));
                        QFont font("Arial", 30);
                        painter.setFont(font);
                        painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                        painter.end();
                        pixmap = QPixmap::fromImage(image);
                        videoPixmapItem->setPixmap(pixmap);
                        videoScene->setSceneRect(videoPixmapItem->boundingRect());
                        videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                        videoView->centerOn(videoPixmapItem);
                        videoView->viewport()->update();  
                        legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","camera")));
                        status_label->setVisible(false);           
                        session.stop_notify();  
                        session.update_helmet_status("nocamera");
                        current_mode = "nocamera";
                        return;
                    }   
                    else {
                        qrcode_label->setVisible(false);
                        legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
                        status_label->setVisible(true);           
                        session.update_helmet_status(session.get_operator_status()+"_standby");
                        session.generate_notify(); 
                        camera_rotate = false;
                        cameraThread->startCapturing(config.period);
                    }
                }
            }
            else if(current_mode.find("emptyStandalone") != std::string::npos) {                
                if (config.debug == 0) {
                    network.enable_wifi();
                    int Wconnected = 2;
                    while(Wconnected != 0){
                        Wconnected = network.check_wifi();
                    }
                    network.init();
                    network.setConnections(wifi);
                    _working_wifi = network.run();
                    working_mode();
                }
                else {
                    std::string _loopback = config._vl_loopback;
                    _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
                    cameraThread->update_camera_pipeline(_loopback);
                    int _cap = cameraThread->init();
                    if (_cap == -1) { 
                        image = QImage(Swidth, Sheight, QImage::Format_RGB888);
                        image.fill(Qt::black);  // Fill the image with black
                        QPainter painter(&image);
                        painter.setRenderHint(QPainter::Antialiasing);
                        painter.setPen(QColor(Qt::green));
                        QFont font("Arial", 30);
                        painter.setFont(font);
                        painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                        painter.end();
                        pixmap = QPixmap::fromImage(image);
                        videoPixmapItem->setPixmap(pixmap);
                        videoScene->setSceneRect(videoPixmapItem->boundingRect());
                        videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                        videoView->centerOn(videoPixmapItem);
                        videoView->viewport()->update();  
                        legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","camera")));
                        status_label->setVisible(false);           
                        session.stop_notify();  
                        session.update_helmet_status("nocamera");
                        current_mode = "nocamera";
                        return;
                    }    
                    camera_rotate = false;
                    cameraThread->startCapturing(config.period);
                    session.update_helmet_status(session.get_operator_status() + "_standby");
                    current_mode = session.get_helmet_status();
                    session.generate_notify();   
                    stackedWidget->setCurrentIndex(0);    
                }
            }
            clicks = 0;
            btn_click = false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer report_and_reset_clicks: " + std::string(e.what()));
    }
}

void CameraViewer::finish_helping() {    
    stackedWidget->setCurrentIndex(2);
}

void CameraViewer::checkwifi() {    
    if (network.check_wifi(10) == 0) {
        network.init();
        network.setConnections(wifi);
        _working_wifi = network.run();
        working_mode();
    }
}

void CameraViewer::handleIMUClassification(const QString& label) {
    std::string result = label.toStdString();
    // LOG_INFO("IMU Classification: " + result);

    // Store label in the rolling vector
    operator_status_list.push_back(result);
    if (operator_status_list.size() >= 3) {
        auto frequency = frequency_counter(operator_status_list);

        if (frequency["Fall"] >= 2) {    
            session.set_operator_status("Fall");                      
        }
        else if (frequency["Relax"] >= 2) {
            session.set_operator_status("Fall");
        }
        else if (frequency["Work"] > 2) {
            session.set_operator_status("Work");
        }
        operator_status_list.clear();
        if (current_mode.find("Standalone") != std::string::npos) {
            pdf.addText(lang.getText("pdf_message","operator") + session.get_operator_status());
            HTTPSession::GPSData _gps = session.getGPS();
            pdf.addText(lang.getText("pdf_message","gps") + std::to_string(_gps.lat) + "," + std::to_string(_gps.lng));            
            pdf.addText("------------------------------------------------");
        }
    }
}

void CameraViewer::complete_standalone_transition(bool _NOWIFI) {
    try {
        A_control.setCaptureInputVolume(0);
        A_control.setDigitalPlaybackVolume(0);
        A_control.setLineOutputVolume(0);
        A_control.setDigitalCaptureVolume(0);
        cameraThread->stopCapturing();
        stackedWidget->setCurrentIndex(1);
        cameraThread->releasecamera();       
        pdf.reset(); 
        standalone_language_transition = true;
        showdefaultstandalone();
        if (!_NOWIFI) {
            if (session.check_standalone() == 1) {
                floatingMessage->showMessage(QString::fromStdString(config.download_file) , 3);
                QtConcurrent::run([this](){
                    session.Download_standalone_FILES(); // Heavy blocking
                    QMetaObject::invokeMethod(this, [this](){
                        floatingMessage->timer_stop();
                        floatingMessage->showMessage(QString::fromStdString(lang.getText("standalonetab","download")), 1);
                        A_control.setCaptureInputType("ADC");
                        A_control.setCaptureInputVolume(30);
                        A_control.setDigitalPlaybackVolume(95);
                        A_control.setLineOutputVolume(60);
                        A_control.setDigitalCaptureVolume(115);
                        standalone_language_transition = false;
                        LOG_INFO("current mode " + current_mode);
                        cameraThread->update_camera_pipeline(config._vl_loopback_small);
                        int _cap = cameraThread->init();
                        if (_cap == -1) { 
                            image = QImage(320, 240, QImage::Format_RGB888);
                            image.fill(Qt::black);  // Fill the image with black
                            QPainter painter(&image);
                            painter.setRenderHint(QPainter::Antialiasing);
                            painter.setPen(QColor(Qt::green));
                            QFont font("Arial", 10);
                            painter.setFont(font);
                            painter.drawText(5, 80, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                            painter.end();
                            pixmap = QPixmap::fromImage(image);
                            videoPixmapItem2->setPixmap(pixmap);
                            videoScene2->setSceneRect(videoPixmapItem2->boundingRect());
                            videoView2->fitInView(videoScene2->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                            videoView2->centerOn(videoPixmapItem2);
                            videoView2->viewport()->update();  
                            return;
                        }  
                        camera_rotate = false;
                    }, Qt::QueuedConnection);
                });
            }
            else {
                floatingMessage->showMessage(QString::fromStdString(lang.getText("error_message","NOFILES")), 2);
                A_control.setCaptureInputType("ADC");
                A_control.setCaptureInputVolume(30);
                A_control.setDigitalPlaybackVolume(95);
                A_control.setLineOutputVolume(60);
                A_control.setDigitalCaptureVolume(115);
                standalone_language_transition = false;
                showdefaultstandalone(false);
            }
            if (config.debug == 0)
                network.disable_wifi();            
        }
        else{
            current_mode = "Standalone";
            session.update_helmet_status("Standalone");
        }
        
    }catch (const std::exception& e) {
        LOG_ERROR("Error while completing standalone transition: " + std::string(e.what()));
    }
}

std::string CameraViewer::toUpperCase(const std::string& input) {
    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        std::wstring wide_str = converter.from_bytes(input);
        
        for (wchar_t& ch : wide_str) {
            ch = std::towupper(ch);
        }
        
        return converter.to_bytes(wide_str);
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer toUpperCase: " + std::string(e.what()));
        return input;
    }  
}

std::string CameraViewer::getCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();

}

void CameraViewer::handle_command_recognize(std::string _command) {
    try {
        if (_command !="") {
            _command = toUpperCase(_command);                   
            if (current_mode.find("Standalone") != std::string::npos) {                
                floatingMessage->showMessage(QString::fromStdString(lang.getText("standalonetab","msgboxcommand") + _command));     
                LOG_INFO("scenaraio  " + std::to_string(scenaraio));
                if (scenaraio == 0) {
                    if (_command == lang.getText("standalonetab","document")) {
                        scenaraio = 1;
                        showFilesList(config.todo,".pdf");
                    }
                    else if (_command == lang.getText("standalonetab","task") ) {
                        scenaraio = 2;
                        showFilesList(config.todo,".txt");
                    }
                    else if (_command == lang.getText("standalonetab","video") ) {
                        scenaraio = 3;
                        showFilesList(config.todo,".mp4");
                    }
                    else if (_command == lang.getText("standalonetab","langs")) {
                        std::string navJsonStr = lang.getSection("languagestab");        
                        // Parse the JSON string
                        nlohmann::json navJson = nlohmann::json::parse(navJsonStr);        
                        // Create a QStringList to hold the items
                        QStringList navItems;        
                        // Check if the section is an array
                        if (navJson.is_object()) {
                            for (const auto& item : navJson.items()) {
                                std::string displayName = item.value().get<std::string>();
                                if (displayName != toUpperCase(config.default_language)) {
                                    navItems << QString::fromStdString(displayName);
                                }
                            }
                        } else {
                            std::cout << "Error: 'languagestab' is not an object" << std::endl;
                        }
                        QString title = QString::fromStdString(lang.getText("standalonetab","languagetitle"));
                        QString message = title + navItems.join("\n ");
                        floatingMessage->showMessage(message, 2);
                    }
                    else if (_command == lang.getText("languagestab","russian")) {

                        if (config.default_language!="Русский")
                            changeLanguage("Русский");
                    }
                    else if (_command == lang.getText("languagestab","english")) {
                        if (config.default_language!="English")
                            changeLanguage("English");
                    }
                    else if (_command == lang.getText("languagestab","arabic")) {
                        if (config.default_language!="عربي")
                            changeLanguage("عربي");
                    }
                    else if (_command == lang.getText("defaulttab","audio_command")) {
                        stackedWidget->setCurrentIndex(4);
                        scenaraio = 5;
                    }
                    else if (_command == lang.getText("standalonetab","close")) {       
                        cameraThread->releasecamera();         
                        if (pdf.getPageCount() > 2)
                            pdf.saveToFile(lang.getText("pdf_message","name")+getCurrentDateTime()+".pdf");
                        if (config.debug == 0) {
                            network.enable_wifi();
                            int Wconnected = 2;
                            while(Wconnected != 0){
                                Wconnected = network.check_wifi();
                            }
                            network.init();
                            network.setConnections(wifi);
                            _working_wifi = network.run();
                            working_mode();
                        }                               
                        else {
                            std::string _loopback = config._vl_loopback;
                            _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
                            cameraThread->update_camera_pipeline(_loopback);
                            int _cap = cameraThread->init();
                            if (_cap == -1) { 
                                image = QImage(Swidth, Sheight, QImage::Format_RGB888);
                                image.fill(Qt::black);  // Fill the image with black
                                QPainter painter(&image);
                                painter.setRenderHint(QPainter::Antialiasing);
                                painter.setPen(QColor(Qt::green));
                                QFont font("Arial", 30);
                                painter.setFont(font);
                                painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                                painter.end();
                                pixmap = QPixmap::fromImage(image);
                                videoPixmapItem->setPixmap(pixmap);
                                videoScene->setSceneRect(videoPixmapItem->boundingRect());
                                videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                                videoView->centerOn(videoPixmapItem);
                                videoView->viewport()->update();  
                                legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","camera")));
                                status_label->setVisible(false);           
                                session.stop_notify();  
                                session.update_helmet_status("nocamera");
                                current_mode = "nocamera";
                                return;
                            }    
                            camera_rotate = false;
                            cameraThread->startCapturing(config.period);
                            session.update_helmet_status(session.get_operator_status()+"_standby");
                            current_mode = session.get_helmet_status();
                            session.generate_notify();   
                            stackedWidget->setCurrentIndex(0);    
                        }
                    }                
                }
                else if (scenaraio == 1) {
                    auto it = config.getNumberMappings().find(_command);
                    if (it != config.getNumberMappings().end()) {
                        LOG_INFO("Found mapping - Word: " + it->first + ", Number: " + std::to_string(it->second));
                        if (it->second <= (static_cast<int>(pdfFiles.size())) && ((static_cast<int>(pdfFiles.size())) > 0)) {
                            scenaraio = 11;
                            showpdfmode();
                            LoadPDF(pdfFiles[(it->second) - 1]);
                        }
                    }
                    else if (_command == lang.getText("standalonetab","langs")) {
                        if (config.default_language == "English")
                            changeLanguage("Русский");
                        else if (config.default_language == "Русский")
                            changeLanguage("عربي");
                        else if (config.default_language == "عربي")
                            changeLanguage("English");
                    }
                    else if (_command == lang.getText("languagestab","russian")) {
                        if (config.default_language!="Русский")
                            changeLanguage("Русский");
                    }
                    else if (_command == lang.getText("languagestab","english")) {
                        if (config.default_language!="English")
                            changeLanguage("English");
                    }
                    else if (_command == lang.getText("languagestab","arabic")) {
                        if (config.default_language!="عربي")
                            changeLanguage("عربي");
                    }
                    else if (_command == lang.getText("standalonetab","quit")) {
                        scenaraio = 0;
                        pdfFiles.clear();
                        txtFiles.clear();
                        mp4Files.clear();   
                        showdefaultstandalone();
                    }
                }
                else if (scenaraio == 2) {
                    auto it = config.getNumberMappings().find(_command);
                    if (it != config.getNumberMappings().end()) {
                        LOG_INFO("Found mapping - Word: " + it->first + ", Number: " + std::to_string(it->second));
                        if (it->second <= (static_cast<int>(txtFiles.size())) && ((static_cast<int>(txtFiles.size())) > 0)){
                            scenaraio = 22;
                            showtxtmode();
                            loadTXT(txtFiles[(it->second) - 1]);
                        }
                    }                    
                    else if (_command == lang.getText("standalonetab","langs")) {
                        if (config.default_language == "English")
                            changeLanguage("Русский");
                        else if (config.default_language == "Русский")
                            changeLanguage("عربي");
                        else if (config.default_language == "عربي")
                            changeLanguage("English");
                    }
                    else if (_command == lang.getText("languagestab","russian")) {
                        if (config.default_language!="Русский")
                            changeLanguage("Русский");
                    }
                    else if (_command == lang.getText("languagestab","english")) {
                        if (config.default_language!="English")
                            changeLanguage("English");
                    }
                    else if (_command == lang.getText("languagestab","arabic")) {
                        if (config.default_language!="عربي")
                            changeLanguage("عربي");
                    }
                    else if (_command == lang.getText("standalonetab","quit")) {
                        scenaraio = 0;
                        pdfFiles.clear();
                        txtFiles.clear();
                        mp4Files.clear();   
                        showdefaultstandalone();
                    }
                }
                else if (scenaraio == 3) {
                    auto it = config.getNumberMappings().find(_command);
                    if (it != config.getNumberMappings().end()) {
                        LOG_INFO("Found mapping - Word: " + it->first + ", Number: " + std::to_string(it->second));
                        if (it->second <= (static_cast<int>(mp4Files.size())) && ((static_cast<int>(mp4Files.size())) > 0)){
                            scenaraio = 33;
                            LoadMP4(mp4Files[(it->second) - 1]);
                            showvideomode();
                        }
                    }
                    else if (_command == lang.getText("standalonetab","langs")) {
                        if (config.default_language == "English")
                            changeLanguage("Русский");
                        else if (config.default_language == "Русский")
                            changeLanguage("عربي");
                        else if (config.default_language == "عربي")
                            changeLanguage("English");
                    }
                    else if (_command == lang.getText("languagestab","russian")) {
                        if (config.default_language!="Русский")
                            changeLanguage("Русский");
                    }
                    else if (_command == lang.getText("languagestab","english")) {
                        if (config.default_language!="English")
                            changeLanguage("English");
                    }
                    else if (_command == lang.getText("languagestab","arabic")) {
                        if (config.default_language!="عربي")
                            changeLanguage("عربي");
                    }
                    else if (_command == lang.getText("standalonetab","quit")) {
                        scenaraio = 0;
                        pdfFiles.clear();
                        txtFiles.clear();
                        mp4Files.clear();   
                        showdefaultstandalone();
                    }
                }
                else if (scenaraio == 5) {
                    if (_command == lang.getText("audiocomandtab","volumelouder")) {
                        A_control.setVolumeLevel(A_control.getVolumeLevel() + 5);
                        if (headphoneSlider) {
                            headphoneSlider->blockSignals(true);
                            headphoneSlider->setValue(A_control.getVolumeLevel());
                            headphoneSlider->blockSignals(false);
                        }
                    }
                    else if (_command == lang.getText("audiocomandtab","volumesilent")) {
                        A_control.setVolumeLevel(A_control.getVolumeLevel() - 5);
                        if (headphoneSlider) {
                            headphoneSlider->blockSignals(true);
                            headphoneSlider->setValue(A_control.getVolumeLevel());
                            headphoneSlider->blockSignals(false);
                        }
                    }
                    else if (_command == lang.getText("audiocomandtab","Capturelouder")) {
                        A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() + 5);
                        if (captureSlider) {
                            captureSlider->blockSignals(true);
                            captureSlider->setValue(A_control.getCaptureInputVolume());
                            captureSlider->blockSignals(false);
                        }
                    }
                    else if (_command == lang.getText("audiocomandtab","Capturesilent")) {
                        int newCaptureInputVolume = A_control.getCaptureInputVolume() - 5;
                        if (newCaptureInputVolume > 20 ) {
                            A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() - 5);
                            if (captureSlider) {
                                captureSlider->blockSignals(true);
                                captureSlider->setValue(newCaptureInputVolume);
                                captureSlider->blockSignals(false);
                            }
                        }
                    }
                    else if (_command == lang.getText("audiocomandtab","AudioReset")) {
                        AudioReset();
                    }
                    else if (_command == lang.getText("audiocomandtab","quit")) {
                        stackedWidget->setCurrentIndex(1);
                        scenaraio = 0;
                    }
                }
                else if (scenaraio == 11) {
                    if (_command == lang.getText("standalonetab","next"))
                        nextPage();
                    else if (_command == lang.getText("standalonetab","previous"))
                        previousPage();
                    else if (_command == lang.getText("standalonetab","zoomin"))
                        zoomIn();
                    else if (_command == lang.getText("standalonetab","zoomout"))
                        zoomOut();
                    else if (_command == lang.getText("standalonetab","up"))
                        scrollUp();
                    else if (_command == lang.getText("standalonetab","down"))
                        scrollDown();
                    else if (_command == lang.getText("standalonetab","left"))
                        scrollLeft();
                    else if (_command == lang.getText("standalonetab","right"))
                        scrollRight();
                    else if (_command == lang.getText("standalonetab","snapshot")) {
                        if (cameraThread->takeSnapshot(config.snapshot_file)) {
                            pdf.addImage("/home/x_user/my_camera_project/snapshot.png");
                            pdf.addText("------------------------------------------------");
                        }
                    }
                    else if (_command == lang.getText("standalonetab","quit")) {
                        scenaraio = 1;
                        currentPage = 0;
                        zoomFactor = 1.5;
                        scene->clear();
                        document = nullptr;
                        pdfFiles.clear();
                        showFilesList(config.todo,".pdf");                        
                        cameraThread->stopCapturing();
                    }                    
                    else if (_command == lang.getText("standalonetab","help")) {
                        QMetaObject::invokeMethod(helptimer, "start", Qt::QueuedConnection, Q_ARG(int, 5000));
                        stackedWidget->setCurrentIndex(1);
                    }            
                }
                else if (scenaraio == 22) {
                    if (_command == lang.getText("standalonetab","next")) {
                        nextTask();
                    }
                    else if (_command == lang.getText("standalonetab","previous")) {
                        prevTask();
                    }
                    else if (_command == lang.getText("standalonetab","snapshot")) {
                        if (cameraThread->takeSnapshot(config.snapshot_file)) {
                            pdf.addImage("/home/x_user/my_camera_project/snapshot.png");
                            pdf.addText("------------------------------------------------");
                        }
                    }
                    else if (_command == lang.getText("standalonetab","quit")) {
                        scenaraio = 2;
                        currentTaskIndex = 0;
                        scene->clear();
                        txtFiles.clear();
                        showFilesList(config.todo,".txt");
                        cameraThread->stopCapturing();
                    } 
                    else if (_command == lang.getText("standalonetab","help")) {
                        QMetaObject::invokeMethod(helptimer, "start", Qt::QueuedConnection, Q_ARG(int, 5000));
                        stackedWidget->setCurrentIndex(1);
                    }
                }
                else if (scenaraio == 222) {
                    if (_command == lang.getText("standalonetab","next")) {
                        nextPage();
                        nextTask();
                    }
                    else if (_command == lang.getText("standalonetab","previous")) {
                        previousPage();
                        prevTask();
                    }
                    else if (_command == lang.getText("standalonetab","zoomin")) {
                        zoomIn();
                    }
                    else if (_command == lang.getText("standalonetab","zoomout")) {
                        zoomOut();
                    }
                    else if (_command == lang.getText("standalonetab","up")) {
                        scrollUp();
                    }
                    else if (_command == lang.getText("standalonetab","down")) {
                        scrollDown();
                    }
                    else if (_command == lang.getText("standalonetab","left")) {
                        scrollLeft();
                    }
                    else if (_command == lang.getText("standalonetab","right")) {
                        scrollRight();
                    }
                    else if (_command == lang.getText("standalonetab","snapshot")) {
                        if (cameraThread->takeSnapshot(config.snapshot_file)) {
                            pdf.addImage("/home/x_user/my_camera_project/snapshot.png");
                            pdf.addText("------------------------------------------------");
                        }
                    }
                    else if (_command == lang.getText("standalonetab","quit")) {
                        scenaraio = 2;
                        currentPage = 0;
                        zoomFactor = 1.5;                    
                        currentTaskIndex = 0;
                        pdfFiles.clear();
                        txtFiles.clear();
                        scene->clear();
                        document = nullptr;
                        showFilesList(config.todo,".txt");
                        cameraThread->stopCapturing();
                    }       
                    else if (_command == lang.getText("standalonetab","help")) {
                        QMetaObject::invokeMethod(helptimer, "start", Qt::QueuedConnection, Q_ARG(int, 5000));
                        stackedWidget->setCurrentIndex(1);
                    }         
                }
                else if (scenaraio == 33) {
                    if (_command == lang.getText("standalonetab","play")) {         
                        if (videoThread->getPause()) {
                            videoThread->playPause();
                            listFiles->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","pause")));
                            listvideos->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","pause")));
                        }
                        else if (videoThread->getStop()) {
                            videoThread->startPlaying();
                            listFiles->item(0)->setText("1- " + QString::fromStdString(lang.getText("standalonetab","stop")));                            
                            listvideos->item(0)->setText("1- " + QString::fromStdString(lang.getText("standalonetab","stop")));
                        }
                    }
                    else if (_command == lang.getText("standalonetab","stop")) {
                        if (!videoThread->getStop()) {
                            videoThread->stopPlaying();
                            listFiles->item(0)->setText("1- " + QString::fromStdString(lang.getText("standalonetab","play")));
                            listvideos->item(0)->setText("1- " + QString::fromStdString(lang.getText("standalonetab","play")));
                        }
                        else if (videoThread->getPause()) {
                            videoThread->stopPlaying();
                            listFiles->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","pause")));
                            listvideos->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","pause")));;
                        }
                    }
                    else if (_command == lang.getText("standalonetab","pause")) {
                        if (!videoThread->getPause()) {
                            listFiles->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","play")));
                            listvideos->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","play")));;
                        }
                        else {
                            listFiles->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","pause")));
                            listvideos->item(1)->setText("2- " + QString::fromStdString(lang.getText("standalonetab","pause")));;
                        }
                        videoThread->playPause();
                    }  
                    else if (_command == lang.getText("standalonetab","previous")) {
                        videoThread->seekBackward(5000);
                    }                  
                    else if (_command == lang.getText("standalonetab","next")) {
                        videoThread->seekForward(5000);
                    }                
                    else if (_command == lang.getText("standalonetab","louder")) {         
                        videoThread->volumeChanged(qMin(videoThread->getVolume() + 10, 80));
                    }                
                    else if (_command == lang.getText("standalonetab","silent")) {
                        videoThread->volumeChanged(qMax(videoThread->getVolume() - 10, 5));
                    }
                    else if (_command == lang.getText("standalonetab","quit")) {
                        if (!videoThread->getStop()) {
                            videoThread->stopPlaying();   // Stops the timer
                            videoThread->releasevideo(); // Releases capture
                        }
                        // Wait for video thread to fully stop (optional, if needed)
                        while (!videoThread->getStop()) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                        videoScene1->clear();
                        videoPixmapItem1 = nullptr;
                        scenaraio = 3;
                        videoScene1->clear();
                        mp4Files.clear();
                        showFilesList(config.todo,".mp4");
                    }
                }
            }
            else if(current_mode.find("emptyStandalone") != std::string::npos) {   
                if (_command == lang.getText("standalonetab","close")) {             
                    if (config.debug == 0) {
                        network.enable_wifi();
                        int Wconnected = 2;
                        while(Wconnected != 0){
                            Wconnected = network.check_wifi();
                        }
                        network.init();
                        network.setConnections(wifi);
                        _working_wifi = network.run();
                        working_mode();
                    }
                    else {
                        std::string _loopback = config._vl_loopback;
                        _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
                        cameraThread->update_camera_pipeline(_loopback);
                        int _cap = cameraThread->init();
                        if (_cap == -1) { 
                            image = QImage(Swidth, Sheight, QImage::Format_RGB888);
                            image.fill(Qt::black);  // Fill the image with black
                            QPainter painter(&image);
                            painter.setRenderHint(QPainter::Antialiasing);
                            painter.setPen(QColor(Qt::green));
                            QFont font("Arial", 30);
                            painter.setFont(font);
                            painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                            painter.end();
                            pixmap = QPixmap::fromImage(image);
                            videoPixmapItem->setPixmap(pixmap);
                            videoScene->setSceneRect(videoPixmapItem->boundingRect());
                            videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                            videoView->centerOn(videoPixmapItem);
                            videoView->viewport()->update();  
                            legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","camera")));
                            status_label->setVisible(false);           
                            session.stop_notify();  
                            session.update_helmet_status("nocamera");
                            current_mode = "nocamera";
                            return;
                        }
                        camera_rotate = false;
                        cameraThread->startCapturing(config.period);
                        session.update_helmet_status(session.get_operator_status() + "_standby");
                        current_mode = session.get_helmet_status();
                        session.generate_notify();   
                        stackedWidget->setCurrentIndex(0);    
                    }
                }
            }
            else {
                if (_command == lang.getText("defaulttab","langs_command")) {
                    if (current_mode.find("request") == std::string::npos || current_mode.find("qrcode") == std::string::npos) {
                        std::string navJsonStr = lang.getSection("languagestab");        
                        // Parse the JSON string
                        nlohmann::json navJson = nlohmann::json::parse(navJsonStr);        
                        // Create a QStringList to hold the items
                        QStringList navItems;        
                        // Check if the section is an array
                        if (navJson.is_object()) {
                            for (const auto& item : navJson.items()) {
                                std::string displayName = item.value().get<std::string>();
                                if (displayName != toUpperCase(config.default_language)) {
                                    navItems << QString::fromStdString(displayName);
                                }
                            }
                        } else {
                            std::cout << "Error: 'languagestab' is not an object" << std::endl;
                        }
                        QString title = QString::fromStdString(lang.getText("standalonetab","languagetitle"));
                        QString message = title + navItems.join("\n ");
                        floatingMessage->showMessage(message, 2);
                    }
                }
                else if (_command == lang.getText("defaulttab","audio_command")) {
                    if (current_mode.find("request") == std::string::npos || current_mode.find("qrcode") == std::string::npos) {
                        if (stackedWidget->currentIndex() == 0) {  
                            stackedWidget->setCurrentIndex(4);
                        }
                    }
                }
                else if (_command == lang.getText("audiocomandtab","quit")) {
                    if (stackedWidget->currentIndex() == 4) {  
                        stackedWidget->setCurrentIndex(0);
                    }
                }
                else if (_command == lang.getText("languagestab","russian")) {
                    if (config.default_language != "Русский")
                        changeLanguage("Русский");
                }
                else if (_command == lang.getText("languagestab","english")) {
                    if (config.default_language != "English")
                        changeLanguage("English");
                }
                else if (_command == lang.getText("languagestab","arabic")) {
                    if (config.default_language != "عربي")
                        changeLanguage("عربي");
                }
                else if (_command == lang.getText("defaulttab","standalone_command")) {
                    if (current_mode.find("request") == std::string::npos || current_mode.find("qrcode") == std::string::npos) {
                        session.stop_notify();
                        session.standalone_request();
                        entering_standalone = true; // Set flag
                        current_mode = session.get_helmet_status();
                        if (current_mode.find("Standalone") != std::string::npos) {
                            entering_standalone = false; // Clear flag if already in standalone
                            complete_standalone_transition(false);
                        }
                    }
                }
                else if (_command == lang.getText("defaulttab","call_command")) {
                    if (current_mode.find("qrcode") == std::string::npos) {
                        session.update_helmet_status(session.get_operator_status() + "_request");
                        session.request_support();
                        current_mode = session.get_helmet_status(); 
                        legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","close")));
                        legend_label2->setVisible(false);
                        legend_label3->setVisible(false); 
                        status_label->setVisible(false);     
                        user_label->setText(QString::fromStdString(lang.getText("defaulttab","call_status")));                    
                        user_label->setVisible(true);
                    }
                }
                else if (_command == lang.getText("defaulttab","setup_command")) {
                    if (current_mode.find("standby") != std::string::npos){
                        session.stop_notify();
                        start_qrcode();
                        current_mode = "qrcode";                    
                    }
                    else if (current_mode.find("offline") != std::string::npos) {
                        std::string _loopback = config._vl_loopback;
                        _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
                        cameraThread->update_camera_pipeline(_loopback);
                        int _cap = cameraThread->init();
                        if (_cap == -1) { 
                            image = QImage(Swidth, Sheight, QImage::Format_RGB888);
                            image.fill(Qt::black);  // Fill the image with black
                            QPainter painter(&image);
                            painter.setRenderHint(QPainter::Antialiasing);
                            painter.setPen(QColor(Qt::green));
                            QFont font("Arial", 30);
                            painter.setFont(font);
                            painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                            painter.end();
                            pixmap = QPixmap::fromImage(image);
                            videoPixmapItem->setPixmap(pixmap);
                            videoScene->setSceneRect(videoPixmapItem->boundingRect());
                            videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                            videoView->centerOn(videoPixmapItem);
                            videoView->viewport()->update();                
                            return;
                        }    
                        camera_rotate = false;
                        cameraThread->startCapturing(config.period);
                        start_qrcode();
                        current_mode = "qrcode";
                    }
                }                
                else if (_command == lang.getText("defaulttab","close_command")) {
                    if (current_mode.find("request") != std::string::npos){
                        session.update_helmet_status(session.get_operator_status() + "_standby");
                        session.terminate_support();
                        current_mode = session.get_helmet_status();
                        legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","langs")));
                        legend_label2->setVisible(true);              
                        legend_label2->setText(QString::fromStdString(lang.getText("defaulttab","standalone")));
                        legend_label3->setVisible(true);
                        legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
                        status_label->setText(QString::fromStdString(lang.getText("defaulttab","setup")));
                        status_label->setVisible(true);
                        task_name->setVisible(false);
                        task_name->clear();
                        task_list->setVisible(false);
                        task_list->clear();
                        message->setVisible(false);
                        message->clear();
                        user_label->setVisible(false);
                        user_label->clear();
                    }
                }
                else if (_command == lang.getText("defaulttab","exit_command")) {
                    if (current_mode.find("qrcode") != std::string::npos) {
                        //Qrcode
                        stop_qrcode();
                        session.generate_notify();
                        current_mode = session.get_helmet_status();  
                    }
                }
                else if (_command == lang.getText("audiocomandtab","volumelouder")) {
                    A_control.setVolumeLevel(A_control.getVolumeLevel() + 5);
                    if (headphoneSlider) {
                        headphoneSlider->blockSignals(true);
                        headphoneSlider->setValue(A_control.getVolumeLevel());
                        headphoneSlider->blockSignals(false);
                    }
                }
                else if (_command == lang.getText("audiocomandtab","volumesilent")) {
                    A_control.setVolumeLevel(A_control.getVolumeLevel() - 5);
                    if (headphoneSlider) {
                        headphoneSlider->blockSignals(true);
                        headphoneSlider->setValue(A_control.getVolumeLevel());
                        headphoneSlider->blockSignals(false);
                    }
                }
                else if (_command == lang.getText("audiocomandtab","Capturelouder")) {
                    A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() + 5);
                    if (captureSlider) {
                        captureSlider->blockSignals(true);
                        captureSlider->setValue(A_control.getCaptureInputVolume());
                        captureSlider->blockSignals(false);
                    }
                }
                else if (_command == lang.getText("audiocomandtab","Capturesilent")) {
                    int newCaptureInputVolume = A_control.getCaptureInputVolume() - 5;
                    if (newCaptureInputVolume > 20 ) {
                        A_control.setCaptureInputVolume(A_control.getCaptureInputVolume() - 5);
                        if (captureSlider) {
                            captureSlider->blockSignals(true);
                            captureSlider->setValue(newCaptureInputVolume);
                            captureSlider->blockSignals(false);
                        }
                    }
                }
                else if (_command == lang.getText("audiocomandtab","AudioReset")) {
                    AudioReset();
                }
                else if (_command == lang.getText("defaulttab","camera_command")) {
                    if (current_mode.find("nocamera") != std::string::npos) {
                        std::string _loopback = config._vl_loopback;
                        _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
                        cameraThread->update_camera_pipeline(_loopback);
                        int _cap = cameraThread->init();
                        if (_cap == -1) { 
                            image = QImage(Swidth, Sheight, QImage::Format_RGB888);
                            image.fill(Qt::black);  // Fill the image with black
                            QPainter painter(&image);
                            painter.setRenderHint(QPainter::Antialiasing);
                            painter.setPen(QColor(Qt::green));
                            QFont font("Arial", 30);
                            painter.setFont(font);
                            painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
                            painter.end();
                            pixmap = QPixmap::fromImage(image);
                            videoPixmapItem->setPixmap(pixmap);
                            videoScene->setSceneRect(videoPixmapItem->boundingRect());
                            videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
                            videoView->centerOn(videoPixmapItem);
                            videoView->viewport()->update();  
                            legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","camera")));
                            status_label->setVisible(false);           
                            session.stop_notify();  
                            session.update_helmet_status("nocamera");
                            current_mode = "nocamera";
                            return;
                        }   
                        else {
                            qrcode_label->setVisible(false);
                            legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
                            status_label->setVisible(true);           
                            session.update_helmet_status(session.get_operator_status()+"_standby");
                            session.generate_notify(); 
                            camera_rotate = false;
                            cameraThread->startCapturing(config.period);
                        }
                    }
                }
                // else if (_command == lang.getText("defaulttab","next_command")) {
                //     if (current_mode.find("active") != std::string::npos) {
                //         if (task_sharing){
                //             session.handle_tasks_next();
                //             if (_index < static_cast<int>(tasklist.size()))
                //                 _index += 1;
                //             drawTaskList();
                //         }
                //     }                    
                // }
                // else if (_command == lang.getText("defaulttab","previous_command")) {
                //     if (current_mode.find("active") != std::string::npos) {
                //         if (task_sharing){
                //             session.handle_tasks_back();
                //             if (_index > 0)
                //                 _index -= 1;
                //             drawTaskList();
                //         }
                //     }                    
                // }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer handle_command_recognize: " + std::string(e.what()));
    }
}

void CameraViewer::AudioReset() {
    try {
        A_control.setVolumeLevel(55);
        A_control.setLineOutputVolume(60);
        A_control.setCaptureInputVolume(30);
        A_control.setDigitalPlaybackVolume(95);
        A_control.setDigitalCaptureVolume(115);
        // A_control.setDigitalPlaybackBoostVolume(0);
        // A_control.setDigitalSidetoneVolume(0);
        A_control.setCaptureInputType("ADC");
        if (headphoneSlider) {
            headphoneSlider->blockSignals(true);
            headphoneSlider->setValue(A_control.getVolumeLevel());
            headphoneSlider->blockSignals(false);
        }
        if (captureSlider) {
            captureSlider->blockSignals(true);
            captureSlider->setValue(A_control.getCaptureInputVolume());
            captureSlider->blockSignals(false);
        }
        if (captureInputLabel) {
            captureInputLabel->setText("ADC");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer AudioReset: " + std::string(e.what()));
    }
}

void CameraViewer::changeLanguage(std::string _lang) {
    // Change the language and update UI labels
    lang.setLanguage(_lang);
    legend_label1->setText(QString::fromStdString(lang.getText("defaulttab", "langs")));
    legend_label2->setText(QString::fromStdString(lang.getText("defaulttab", "standalone")));
    legend_label3->setText(QString::fromStdString(lang.getText("defaulttab", "call")));
    status_label->setText(QString::fromStdString(lang.getText("defaulttab", "setup")));

    
    if (scenaraio == 1 || scenaraio == 2 || scenaraio == 3){
        listFiles->item(listFiles->count()-1)->setText(
            QString::number(listFiles->count()) + 
            QString::fromStdString(" - " + lang.getText("standalonetab","quit"))
        );        
    }
    else {
        listFiles->clear();
        taskListWidget->clear();
        std::string navJsonStr = lang.getSection("Navigationtab");        
        // Parse the JSON string
        nlohmann::json navJson = nlohmann::json::parse(navJsonStr);        
        // Create a QStringList to hold the items
        QStringList navItems;        
        // Check if the section is an array
        if (navJson.is_array()) {
            for (const auto& item : navJson) {
                navItems << QString::fromStdString(item.get<std::string>());
            }
        }
        // Check if the section is an object (extract values)
        else if (navJson.is_object()) {
            for (auto it = navJson.begin(); it != navJson.end(); ++it) {
                navItems << QString::fromStdString(it.value().get<std::string>());
            }
        } else {
            LOG_ERROR("Navigationtab section is neither an array nor an object");
            navItems << "Error: Invalid Navigationtab format";
        }
        // Add the items to the QListWidget
        listFiles->addItems(navItems); 
        listaudios->clear();
        navJsonStr = lang.getSection("audiocomandtab");        
        // Parse the JSON string
        navJson = nlohmann::json::parse(navJsonStr);    
        navItems.clear();    
        if (navJson.is_object()) {
            // Define the desired order of keys as they appear in lang.json
            std::vector<std::string> keyOrder = {
                "volumelouder",
                "volumesilent",
                "Capturelouder",
                "Capturesilent",
                "AudioReset",
                "quit"
            };
            // Iterate over the keys in the defined order
            int i = 1;
            for (const auto& key : keyOrder) {
                if (navJson.contains(key)) {
                    navItems << QString::number(i) + " - " + QString::fromStdString(navJson[key].get<std::string>());
                    i += 1;
                } else {
                    LOG_WARN("Key " + key + " not found in audiocomandtab");
                }
            }
        } else {
            LOG_ERROR("audiocomandtab section is not an object");
            navItems << "Error: Invalid audiocomandtab format";
        }
        listaudios->addItems(navItems); 
        listvideos->clear();
        navJsonStr = lang.getSection("Videotab");        
        // Parse the JSON string
        navJson = nlohmann::json::parse(navJsonStr);        
        // Create a QStringList to hold the items  
        navItems.clear();      
        // Check if the section is an array
        if (navJson.is_array()) {
            for (const auto& item : navJson) {
                navItems << QString::fromStdString(item.get<std::string>());
            }
        }
        // Check if the section is an object (extract values)
        else if (navJson.is_object()) {
            for (auto it = navJson.begin(); it != navJson.end(); ++it) {
                navItems << QString::fromStdString(it.value().get<std::string>());
            }
        } else {
            LOG_ERROR("Navigationtab section is neither an array nor an object");
            navItems << "Error: Invalid Navigationtab format";
        }
        // Add the items to the QListWidget
        listvideos->addItems(navItems);
    }
    floatingMessage->showMessage(QString::fromStdString(lang.getText("standalonetab", "languagemessage")), 2);
    standalone_language_transition = true;
    // 3. Start the heavy work in a background thread
    QtConcurrent::run([this, _lang]() {
        try {
            voiceThread->stopThread();
            auto newVoiceThread = std::make_unique<speechThread>(
                lang.getVosk(),
                lang.getGrammar(),
                config.pipeline_description,
                5
            );
            newVoiceThread->setCommandCallback([this](const std::string &command) {
                QMetaObject::invokeMethod(this, [this, command]() {
                    handle_command_recognize(command);
                });
            });

            // Instead of passing &newVoiceThread, use std::move capture
            QMetaObject::invokeMethod(this, [this, thread = std::move(newVoiceThread)]() mutable {
                voiceThread = std::move(thread);
                AudioReset();
                voiceThread->start();
                config.updateDefaultLanguage(lang.getDefaultLanguage());
                floatingMessage->showMessage(
                    QString::fromStdString(lang.getText("standalonetab", "languagetitle") + lang.getDefaultLanguage()), 2
                );
                standalone_language_transition = false;
            }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in changeLanguage worker: " + std::string(e.what()));
            QMetaObject::invokeMethod(this, [this, err=std::string(e.what())]() {
                floatingMessage->showMessage(QString::fromStdString("Error changing language: " + err), 2);
            }, Qt::QueuedConnection);
        }
    });  
}

void CameraViewer::Enter_Low_Power_Mode() {
    try {
        LOG_INFO("Enter Low Power Mode");
        setVisors("0");
        setCamera("1");
        standbytimer->stop();
        current_mode = "Low_power";
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer Enter_Low_Power_Mode: " + std::string(e.what()));
    }
}

void CameraViewer::Exit_Low_Power_Mode() {
    try {
        LOG_INFO("Return to Normal Power Mode");
        setVisors("1");
        setCamera("0");
        current_mode = session.get_helmet_status();
        standbytimer->start();
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer Exit_Low_Power_Mode: " + std::string(e.what()));
    }
}

void CameraViewer::setVisors(std::string _value) {
    try {
        const std::string gpioPath = "/sys/class/gpio/gpio120/value";
        std::ofstream gpioFile(gpioPath);
        if (gpioFile.is_open()) {
            gpioFile << _value;
            gpioFile.close();
            if (_value == "1"){
                LOG_INFO("Visors ensabled (HDMI on)");
            }
            else{
                LOG_INFO("Visors disabled (HDMI off)");
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));

        } else {
            LOG_ERROR("Failed to open GPIO file: " + gpioPath);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer setVisors: " + std::string(e.what()));
    }
}

void CameraViewer::setCamera(std::string _value) {
    try {
        const std::string gpioPath = "/sys/class/gpio/gpio125/value";
        const std::string gpioPath1 = "/sys/class/gpio/gpio83/value";

        std::ofstream gpioFile(gpioPath);
        std::ofstream gpioFile1(gpioPath1);

        if (gpioFile.is_open()) {
            gpioFile << _value;
            gpioFile.close();
            if (_value == "0"){
                LOG_INFO("Camera enabled");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (gpioFile1.is_open()) {
                    gpioFile1 << _value;
                    gpioFile1.close();
                }
                else {
                    LOG_ERROR("Failed to open GPIO file: " + gpioPath1);
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (gpioFile1.is_open()) {
                    gpioFile1 << "1";
                    gpioFile1.close();
                }
                else {
                    LOG_ERROR("Failed to open GPIO file: " + gpioPath1);
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            else{
                LOG_INFO("Camera disabled ");
            }
        } else {
            LOG_ERROR("Failed to open GPIO file: " + gpioPath);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer setCamera: " + std::string(e.what()));
    }
}

void CameraViewer::send_audio_settings() {
    try {
        LOG_INFO("send audio settings");
        session.record_event("playbackVolume", std::to_string(A_control.getVolumeLevel()));
        
        std::string status = A_control.getCaptureInputType() == "DMIC" ? "1" : "0";
        session.record_event("digitalMicrophone", status);

        session.record_event("microphoneVolume", std::to_string(A_control.getCaptureInputVolume()));
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer send_audio_settings: " + std::string(e.what()));
    }
}

void CameraViewer::start_audio_channel() {
    try {
        LOG_INFO("Start audio channel");        
        A_player.init();
        A_player.run();
        A_streamer.update_pipline(config.audio_outcoming_pipeline);
        A_streamer.init();
        A_streamer.run();
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer start_audio_channel: " + std::string(e.what()));
    }
}

void CameraViewer::close_audio_channel() {
    try {
        LOG_INFO("Close audio channel");
        A_player.quit();
        A_streamer.quit();
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer close_audio_channel: " + std::string(e.what()));
    }
}

void CameraViewer::process_event(nlohmann::json _data) {
    try {
        if (_data["idCallEvent"].get<int>() > last_processed_event) {
            std::string command = _data["event"]["cmd"];
            if (command == "screen") {
                std::string screen_data = _data["event"]["data"];
                session.update_event(_data["idCallEvent"], "progress", "received");                
                if (screen_data == "local") {
                    remoteend();
                } else if (screen_data == "remote") {
                    remotestart();
                }
            }
            else if (command == "playbackVolume") {
                int volume_level = 50;
                if (_data["event"]["data"].is_string()) 
                    volume_level = clamp(std::stoi(_data["event"]["data"].get<std::string>()), 0, 100);
                else
                    volume_level = clamp(_data["event"]["data"].get<int>(), 0, 100);
                volume_level = volume_level*0.63;
                A_control.setVolumeLevel(volume_level);
            }
            else if (command == "digitalMicrophone") {
                bool is_dmic = false;
                if (_data["event"]["data"].is_string()) 
                    is_dmic = std::stoi(_data["event"]["data"].get<std::string>()) == 1;
                else
                    is_dmic = _data["event"]["data"].get<int>() == 1;
                std::string audio_input = is_dmic ? "DMIC" : "ADC";
                A_control.setCaptureInputType(audio_input);
            }
            else if (command == "microphoneVolume") {
                int mic_level = 50;
                if (_data["event"]["data"].is_string()) 
                    mic_level = std::stoi(_data["event"]["data"].get<std::string>());
                else
                    mic_level = _data["event"]["data"].get<int>();
                mic_level = clamp(mic_level, 0, 200); // Clamping mic level for safety
                mic_level = mic_level*31/200;
                A_control.setCaptureInputVolume(mic_level);
            }
            else if(command == "camera") {
                std::string on_off = _data["event"]["data"].get<std::string>();
                if (on_off == "on") {
                    _pause_stream = false;
                }
                else
                    _pause_stream = true;
            }
            else if(command == "videoSettings") {
                std::string setting = _data["event"]["data"].get<std::string>();
                size_t pos = 0;
                std::string delimiter = ",";
                std::string token;
                int index = 0;
                int values[4];
                while ((pos = setting.find(delimiter)) != std::string::npos) {
                    token = setting.substr(0, pos);
                    values[index++] = std::stoi(token); 
                    setting.erase(0, pos + delimiter.length());
                }
                values[index] = std::stoi(setting); 
                bool areEqual = true;
                if (values[2] == 1280)
                    values[2] = 1024;
                if (values[3] == 720)
                    values[3] = 768;
                // Compare each element
                for (int i = 0; i < 4; ++i) {                    
                    if (old_values[i] != values[i]) {
                        areEqual = false; // Set to false if any element differs
                        break; // Exit the loop early
                    }                    
                }
                memcpy(old_values, values, sizeof(old_values));
                if (!areEqual) {
                    std::cout << "!areEqual : " << std::endl;
                    config.updatestreaming(values[0], values[1], values[2], values[3]);
                    streamend();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    streamstart(_ipstream);                
                }
            }
            // Update last processed event ID
            last_processed_event = _data["idCallEvent"].get<int>();
            LOG_INFO("process_event " + std::to_string(last_processed_event));
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error processing event: " + std::string(e.what()));
    } 
}

void CameraViewer::remotestart() {
    try {
        LOG_INFO("[GST] START REMOTING START");
        int b_remote = cameraThread->startremote(config._vp_remote);
        if (b_remote == -1) {
            LOG_ERROR("Error: Could not open the remote pipline.");
            return;
        }                
        session.update_event(last_processed_event, "progress", "completed");
        LOG_INFO("[GST] START REMOTING END");
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer remotestart: " + std::string(e.what()));
    }
}

void CameraViewer::remoteend() {
    try {
        LOG_INFO("[GST] STOP REMOTING START");
        cameraThread->stopremote();    
        LOG_INFO("[GST] STOP REMOTING END");
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer remoteend: " + std::string(e.what()));
    }
}

void CameraViewer::streamstart(std::string _data) { 
    try {   
        LOG_INFO("[GST] START STREAMING START");  
        cameraThread->stopCapturing();
        cameraThread->releasecamera();    
        std::string _loopback = config._vl_loopback;
        _loopback = config.replacePlaceholder(_loopback, "$FPS", "30");
        cameraThread->update_camera_pipeline(_loopback);  
        cameraThread->stopCapturing();
        cameraThread->releasecamera();  
        int _cap = cameraThread->init();
        if (_cap == -1) { 
            image = QImage(Swidth, Sheight, QImage::Format_RGB888);
            image.fill(Qt::black);  // Fill the image with black
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QColor(Qt::green));
            QFont font("Arial", 30);
            painter.setFont(font);
            painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
            painter.end();
            pixmap = QPixmap::fromImage(image);
            videoPixmapItem->setPixmap(pixmap);
            videoScene->setSceneRect(videoPixmapItem->boundingRect());
            videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
            videoView->centerOn(videoPixmapItem);
            videoView->viewport()->update();  
            legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","camera")));
            status_label->setVisible(false);           
            session.stop_notify();  
            session.update_helmet_status("nocamera");
            current_mode = "nocamera";
            return;
        }          
        camera_rotate = false;
        cameraThread->startCapturing(33);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::string _stream = config._vs_streaming;          
        _stream = config.replacePlaceholder(_stream, "$VPN_ADDR", _data);
        _stream = config.replacePlaceholder(_stream, "$server_port", std::to_string(config.server_port));
        // std::cout << "_stream : " << _stream  << std::endl;
        int b_stream = cameraThread->startstream(_stream, config.speriod, config.swidth, config.sheight);
        if (b_stream == -1) {
            LOG_ERROR("Error: Could not open the streaming pipline.");
            return;
        }
        LOG_INFO("[GST] START STREAMING END");     
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer streamstart: " + std::string(e.what()));
    }
}

void CameraViewer::streamend() {
    try {
        LOG_INFO("[GST] STOP STREAMING START");
        cameraThread->stopCapturing();
        cameraThread->releasecamera();    
        std::string _loopback = config._vl_loopback;
        _loopback = config.replacePlaceholder(_loopback, "$FPS", "15");
        cameraThread->update_camera_pipeline(_loopback);  
        cameraThread->stopCapturing();
        cameraThread->releasecamera();  
        int _cap = cameraThread->init();
        if (_cap == -1) { 
            image = QImage(Swidth, Sheight, QImage::Format_RGB888);
            image.fill(Qt::black);  // Fill the image with black
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QColor(Qt::green));
            QFont font("Arial", 30);
            painter.setFont(font);
            painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOCAMERA")));
            painter.end();
            pixmap = QPixmap::fromImage(image);
            videoPixmapItem->setPixmap(pixmap);
            videoScene->setSceneRect(videoPixmapItem->boundingRect());
            videoView->fitInView(videoScene->sceneRect(), Qt::KeepAspectRatioByExpanding); 
            videoView->centerOn(videoPixmapItem);
            videoView->viewport()->update();  
            legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","camera")));
            status_label->setVisible(false);           
            session.stop_notify();  
            session.update_helmet_status("nocamera");
            current_mode = "nocamera";
            return;
        }  
        camera_rotate = false;
        cameraThread->startCapturing(config.period);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cameraThread->stopstream();   
        LOG_INFO("[GST] STOP STREAMING END");
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer streamend: " + std::string(e.what()));
    }    
}

void CameraViewer::showdefaultstandalone(bool _standalone) {
    try {        
        if(_standalone) {
            pdf.addText(lang.getText("pdf_message","first_page") + getCurrentDateTime());
            stackedWidget->setCurrentIndex(1);
            listFiles->clear();
            taskListWidget->clear();
            std::string navJsonStr = lang.getSection("Navigationtab");        
            // Parse the JSON string
            nlohmann::json navJson = nlohmann::json::parse(navJsonStr);        
            // Create a QStringList to hold the items
            QStringList navItems;        
            // Check if the section is an array
            if (navJson.is_array()) {
                for (const auto& item : navJson) {
                    navItems << QString::fromStdString(item.get<std::string>());
                }
            }
            // Check if the section is an object (extract values)
            else if (navJson.is_object()) {
                for (auto it = navJson.begin(); it != navJson.end(); ++it) {
                    navItems << QString::fromStdString(it.value().get<std::string>());
                }
            } else {
                LOG_ERROR("Navigationtab section is neither an array nor an object");
                navItems << "Error: Invalid Navigationtab format";
            }
            // Add the items to the QListWidget
            listFiles->addItems(navItems);         
            QPixmap pixmap1 = this->grab();
            pixmap1.scaled(640, 480, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            pixmap1.save("/home/x_user/my_camera_project/screenshot.png", "PNG");
            pdf.addImage("/home/x_user/my_camera_project/screenshot.png");
            pdf.addText("------------------------------------------------");
            // if (cameraThread->takeSnapshotGst(config.snapshot_pipeline))
            if (cameraThread->takeSnapshot(config.snapshot_file)) {
                pdf.addImage("/home/x_user/my_camera_project/snapshot.png");
                pdf.addText("------------------------------------------------");
            }
        }
        else {
            listFiles->addItem(QString::fromStdString(lang.getText("error_message","NOFILES")));
            listFiles->addItem(QString::fromStdString(lang.getText("standalonetab","close")));
            current_mode = "emptyStandalone";            
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer showdefaultstandalone: " + std::string(e.what()));
    }
}

void CameraViewer::showpdfmode() {
    listFiles->clear();
    taskListWidget->clear();
    listFiles->addItem("1- " + QString::fromStdString(lang.getText("standalonetab","next")));
    listFiles->addItem("2- " + QString::fromStdString(lang.getText("standalonetab","previous")));
    listFiles->addItem("3- " + QString::fromStdString(lang.getText("standalonetab","zoomin")));
    listFiles->addItem("4- " + QString::fromStdString(lang.getText("standalonetab","zoomout")));
    listFiles->addItem("5- " + QString::fromStdString(lang.getText("standalonetab","up")));
    listFiles->addItem("6- " + QString::fromStdString(lang.getText("standalonetab","down")));
    listFiles->addItem("7- " + QString::fromStdString(lang.getText("standalonetab","left")));
    listFiles->addItem("8- " + QString::fromStdString(lang.getText("standalonetab","right")));
    listFiles->addItem("9- " + QString::fromStdString(lang.getText("standalonetab","quit")));
}

void CameraViewer::showtxtmode() {
    listFiles->clear();
    taskListWidget->clear();
    listFiles->addItem("1- " + QString::fromStdString(lang.getText("standalonetab","next")));
    listFiles->addItem("2-" + QString::fromStdString(lang.getText("standalonetab","previous")));
    listFiles->addItem("3- " + QString::fromStdString(lang.getText("standalonetab","quit")));
}

void CameraViewer::showvideomode() {
    listFiles->clear();
    taskListWidget->clear();
    listvideos->clear();
    std::string navJsonStr = lang.getSection("Videotab");        
    // Parse the JSON string
    nlohmann::json navJson = nlohmann::json::parse(navJsonStr);        
    // Create a QStringList to hold the items
    QStringList navItems;        
    // Check if the section is an array
    if (navJson.is_array()) {
        for (const auto& item : navJson) {
            navItems << QString::fromStdString(item.get<std::string>());
        }
    }
    // Check if the section is an object (extract values)
    else if (navJson.is_object()) {
        for (auto it = navJson.begin(); it != navJson.end(); ++it) {
            navItems << QString::fromStdString(it.value().get<std::string>());
        }
    } else {
        LOG_ERROR("Navigationtab section is neither an array nor an object");
        navItems << "Error: Invalid Navigationtab format";
    }
    // Add the items to the QListWidget
    listFiles->addItems(navItems);
    listvideos->addItems(navItems);
}

void CameraViewer::showFilesList(const std::string &folder_path, const std::string &suffix) {
    try {        
        std::string files_type;
        if (suffix == ".mp4")
            files_type = lang.getText("standalonetab","video");
        else if (suffix == ".pdf")
            files_type = lang.getText("standalonetab","document");
        else
            files_type = lang.getText("standalonetab","task");
        pdf.addText(lang.getText("pdf_message","mode") + files_type + " - " + getCurrentDateTime());
        listFiles->clear();
        taskListWidget->clear();
        QDir dir(QString::fromStdString(folder_path));
        dir.setNameFilters({ "*" + QString::fromStdString(suffix) });
        QStringList files = dir.entryList(QDir::Files);
        int i = 1;          // Counter for item numbers
        for (const QString &file : files) {
            QString fullPath = dir.filePath(file);
            if (suffix == ".pdf") {
                pdfFiles.push_back(fullPath.toStdString());
            } else if (suffix == ".txt") {
                txtFiles.push_back(fullPath.toStdString());
            }
            else if (suffix == ".mp4") {
                mp4Files.push_back(fullPath.toStdString());
            }
            // Add item to the QListWidget
            listFiles->addItem(QString::number(i) + " - " + file);
            i++;
        }
        listFiles->addItem(QString::number(i) + QString::fromStdString(" - " + lang.getText("standalonetab","quit")));  
        stackedWidget->setCurrentIndex(1);
        QPixmap pixmap1 = this->grab();
        pixmap1.scaled(640, 480, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        pixmap1.save("/home/x_user/my_camera_project/screenshot.png", "PNG");
        pdf.addImage("/home/x_user/my_camera_project/screenshot.png");
        pdf.addText("------------------------------------------------");
        // if (cameraThread->takeSnapshotGst(config.snapshot_pipeline))
        if (cameraThread->takeSnapshot(config.snapshot_file)) {
            pdf.addImage("/home/x_user/my_camera_project/snapshot.png");
            pdf.addText("------------------------------------------------");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer showFilesList: " + std::string(e.what()));
    }
}

void CameraViewer::LoadPDF(const std::string &full_path) {
    try {
        // Extract file name using std::filesystem
        std::filesystem::path path_obj(full_path);
        std::string filename = path_obj.filename().string();
        document = Poppler::Document::load(QString::fromStdString(full_path));
        if (!document) {
            LOG_ERROR("Failed to load document!");
            if (scenaraio == 11) {
                scenaraio = 1;
                showFilesList(config.todo,".pdf");
            }
            else {
                scenaraio = 2;
                showFilesList(config.todo,".txt");
            }
            return;
        }        
        cameraThread->startCapturing(config.period);
        pdf.addText(lang.getText("pdf_message","pdf") + filename + " - " + getCurrentDateTime());
        stackedWidget->setCurrentIndex(2);
        document->setRenderHint(Poppler::Document::Antialiasing);
        document->setRenderHint(Poppler::Document::TextAntialiasing);
        showPage(currentPage);
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer LoadPDF: " + std::string(e.what()));
    }
}

void CameraViewer::LoadMP4(const std::string &full_path) {
    try {
        // Extract file name using std::filesystem
        std::filesystem::path path_obj(full_path);
        std::string filename = path_obj.filename().string();
        LOG_INFO("Loading MP4 file: " + filename);
        if (!videoThread->getStop()) {
            videoThread->stopPlaying();
            videoThread->releasevideo();
        }
        videoScene1->clear();
        // Ensure videoPixmapItem1 is null since the scene was cleared
        videoPixmapItem1 = nullptr;

        // Create a new videoPixmapItem1 and add to scene
        videoPixmapItem1 = new QGraphicsPixmapItem();
        videoScene1->addItem(videoPixmapItem1);
        videoThread->update_video_path(full_path);
        int _vid = videoThread->init();
        if (_vid == 0) {
            pdf.addText(lang.getText("pdf_message","mp4")  + filename + " - " + getCurrentDateTime());
            stackedWidget->setCurrentIndex(3);
            listFiles->clear();
            listvideos->clear();
            std::string navJsonStr = lang.getSection("Videotab");        
            // Parse the JSON string
            nlohmann::json navJson = nlohmann::json::parse(navJsonStr);        
            // Create a QStringList to hold the items
            QStringList navItems;        
            // Check if the section is an array
            if (navJson.is_array()) {
                for (const auto& item : navJson) {
                    navItems << QString::fromStdString(item.get<std::string>());
                }
            }
            // Check if the section is an object (extract values)
            else if (navJson.is_object()) {
                for (auto it = navJson.begin(); it != navJson.end(); ++it) {
                    navItems << QString::fromStdString(it.value().get<std::string>());
                }
            } else {
                LOG_ERROR("Navigationtab section is neither an array nor an object");
                navItems << "Error: Invalid Navigationtab format";
            }
            // Add the items to the QListWidget
            listFiles->addItems(navItems);
            listvideos->addItems(navItems);
            QPixmap pixmap1 = this->grab();
            pixmap1.scaled(640, 480, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            pixmap1.save("/home/x_user/my_camera_project/screenshot.png", "PNG");
            pdf.addImage("/home/x_user/my_camera_project/screenshot.png");
            pdf.addText("------------------------------------------------");            
            // if (cameraThread->takeSnapshotGst(config.snapshot_pipeline))
            if (cameraThread->takeSnapshot(config.snapshot_file)) {
                pdf.addImage("/home/x_user/my_camera_project/snapshot.png");
                pdf.addText("------------------------------------------------");
            }
        }
        else {
            floatingMessage->showMessage(QString::fromStdString(lang.getText("standalonetab","NOVIDEO")), 2); 
            image = QImage(Swidth, Sheight, QImage::Format_RGB888);
            image.fill(Qt::black);  // Fill the image with black
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QColor(Qt::green));
            QFont font("Arial", 30);
            painter.setFont(font);
            painter.drawText(Swidth/2 -100, Sheight/2, QString::fromStdString(lang.getText("error_message", "NOVIDEO")));
            painter.end();
            pixmap = QPixmap::fromImage(image);
            videoPixmapItem1->setPixmap(pixmap);
            videoScene1->setSceneRect(videoPixmapItem1->boundingRect());
            videoView1->fitInView(videoScene1->sceneRect(), Qt::KeepAspectRatioByExpanding); 
            videoView1->centerOn(videoPixmapItem1);
            videoView1->viewport()->update();      
                       
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer LoadMP4: " + std::string(e.what()));
    }
}

std::string CameraViewer::loadTasks(const std::string &full_path) {
    try {
        // Extract file name using std::filesystem
        std::filesystem::path path_obj(full_path);
        std::string filename = path_obj.filename().string();
        pdf.addText(lang.getText("pdf_message","txt") + filename + " - " + getCurrentDateTime());
        std::ifstream file(full_path);
        std::string line;
        std::string lastLine;
        tasks.clear();
        // Check if the file is open
        if (!file.is_open()) {
            LOG_ERROR("Error: Could not open file " + full_path);
            scenaraio = 2;
            showFilesList(config.todo,".txt");
            return "";
        }
        // Read each line and update `lastLine` with the non-empty stripped line
        while (std::getline(file, line)) {
            // Add each line to the tasks vector
            tasks.push_back(line);
            // Remove leading and trailing whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);
            // Update lastLine if the line is not empty
            if (!line.empty()) {
                lastLine = line;
            }
        }
        file.close();        
        return lastLine;
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer loadTasks: " + std::string(e.what()));
        return "";
    }
}

void CameraViewer::displayTasks() {
    try {        
        pdf.addText(lang.getText("pdf_message","taskN")  + std::to_string(currentTaskIndex + 1) +  " - " + getCurrentDateTime());        
        taskListWidget->clear();
        // Ensure up to 3 tasks are displayed, including the current one
        int startIndex = std::max(0, currentTaskIndex);
        taskListWidget->setStyleSheet(
            "QListWidget::item { color: black; }"
            "QListWidget::item:selected { color: green; font-weight: bold; }"
        );
        int endIndex = std::min(static_cast<int>(tasks.size()), startIndex + 1);
        for (int i = startIndex; i < endIndex; ++i) {
            QListWidgetItem *item = new QListWidgetItem(QString::fromStdString(tasks[i]));
            item->setFont([](int size) { QFont font; /*font.setBold(true);*/ font.setPointSize(size); return font; }(config.font_size));
            item->setTextAlignment(Qt::AlignCenter);
            taskListWidget->addItem(item);
            // Highlight the current task
            if (i == currentTaskIndex) {
                item->setBackground(QColor("lightyellow"));
                // item->setStyleSheet("QListWidgetItem { color : green; font-weight: bold;}");
                item->setForeground(Qt::green); 
                item->setFont(QFont(item->font().family(), item->font().pointSize(), QFont::Bold));  
            }
        }
        QPixmap pixmap1 = this->grab();
        pixmap1.scaled(640, 480, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        pixmap1.save("/home/x_user/my_camera_project/screenshot.png", "PNG");
        pdf.addImage("/home/x_user/my_camera_project/screenshot.png");
        pdf.addText("------------------------------------------------");        
        // if (cameraThread->takeSnapshotGst(config.snapshot_pipeline))
        if (cameraThread->takeSnapshot(config.snapshot_file)) {
            pdf.addImage("/home/x_user/my_camera_project/snapshot.png");
            pdf.addText("------------------------------------------------");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer displayTasks: " + std::string(e.what()));
    }
}

void CameraViewer::loadTXT(const std::string &full_path) {
    try {
        std::string last_line = loadTasks(full_path);
        if (last_line == "")
            return;
        std::string pdf_name = full_path;
        pdf_name = pdf_name.erase(pdf_name.size()-3);
        pdf_name = pdf_name + "pdf";
        if (last_line.find("[document]") != std::string::npos)
        {
            scenaraio = 222;
            showpdfmode();
            LoadPDF(pdf_name);
        }
        else {
            showtxtmode();   
            stackedWidget->setCurrentIndex(2);
        }
        displayTasks();    
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer loadTXT: " + std::string(e.what()));
    }  
}

void CameraViewer::showPage(int page_num) {
    try {
        if (!document || page_num < 0 || page_num >= document->numPages()) {
            LOG_ERROR("Invalid page number or document not loaded.");
            scenaraio = 1;
            showFilesList(config.todo,".pdf");
            return;
        }
        // Get the page
        Poppler::Page *page = document->page(page_num);
        if (!page) {
            LOG_ERROR("Failed to load page: " + std::to_string(page_num));
            scenaraio = 1;
            showFilesList(config.todo,".pdf");
            return;
        }
        // Render the page to an image
        QImage image = page->renderToImage(zoomFactor * 72.0, zoomFactor * 72.0);
        if (image.isNull()) {
            LOG_ERROR("Failed to render page: " + std::to_string(page_num));
            scenaraio = 1;
            showFilesList(config.todo,".pdf");
            delete page;
            return;
        }
        pdf.addText(lang.getText("pdf_message","pageN") + std::to_string(currentPage + 1) +  " - " + getCurrentDateTime());
        // Convert the image to a pixmap and add it to the scene
        QPixmap pixmap = QPixmap::fromImage(image);
        scene->clear();
        scene->addPixmap(pixmap);
        scene->setSceneRect(pixmap.rect());
        // Clean up
        delete page;
        QPixmap pixmap1 = this->grab();
        pixmap1.scaled(640, 480, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        pixmap1.save("/home/x_user/my_camera_project/screenshot.png", "PNG");
        pdf.addImage("/home/x_user/my_camera_project/screenshot.png");
        pdf.addText("------------------------------------------------");
        // if (cameraThread->takeSnapshotGst(config.snapshot_pipeline))
        if (cameraThread->takeSnapshot(config.snapshot_file)) {
            pdf.addImage("/home/x_user/my_camera_project/snapshot.png");
            pdf.addText("------------------------------------------------");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer showPage: " + std::string(e.what()));
    }   
}

void CameraViewer::nextPage() {
    if (document && currentPage < document->numPages() - 1) {
        ++currentPage;
        showPage(currentPage);        
    }
}

void CameraViewer::previousPage() {
    if (document && currentPage > 0) {        
        --currentPage;
        showPage(currentPage);                
    }
}

void CameraViewer::zoomIn() {
    if (zoomFactor < 6.0)
        zoomFactor += 0.5;
    showPage(currentPage);
}

void CameraViewer::zoomOut() {
    if (zoomFactor > 0.75)
        zoomFactor -= 0.5;
    showPage(currentPage);
}

void CameraViewer::scrollUp() {
    if (view && view->verticalScrollBar()) {
        int visibleHeight = view->viewport()->height();        
        // Calculate 30% of the visible height
        int scrollAmount = static_cast<int>(visibleHeight * 0.3);
        view->verticalScrollBar()->setValue(view->verticalScrollBar()->value() - scrollAmount);
    }
}

void CameraViewer::scrollDown() {
    if (view && view->verticalScrollBar()) {
        int visibleHeight = view->viewport()->height();        
        // Calculate 30% of the visible height
        int scrollAmount = static_cast<int>(visibleHeight * 0.3);
        view->verticalScrollBar()->setValue(view->verticalScrollBar()->value() + scrollAmount);
    }
}

void CameraViewer::scrollLeft() {
    if (view && view->horizontalScrollBar()) {
        int visibleWidth = view->viewport()->width();        
        // Calculate 30% of the visible width
        int scrollAmount = static_cast<int>(visibleWidth * 0.3);
        view->horizontalScrollBar()->setValue(view->horizontalScrollBar()->value() - scrollAmount);
    }
}

void CameraViewer::scrollRight() {
    if (view && view->horizontalScrollBar()) {
        int visibleWidth = view->viewport()->width();        
        // Calculate 30% of the visible width
        int scrollAmount = static_cast<int>(visibleWidth * 0.3);
        view->horizontalScrollBar()->setValue(view->horizontalScrollBar()->value() + scrollAmount);
    }
}

void CameraViewer::nextTask() {
    if (currentTaskIndex < ((int)tasks.size()))    {
        ++currentTaskIndex;
        displayTasks();                
    }
}

void CameraViewer::prevTask() {
    if (currentTaskIndex > 0)    {
        --currentTaskIndex;
        displayTasks();
    }
}

void CameraViewer::display_messages(std::string _msg) {
    try {
        message->setVisible(true);
        QFont font("Arial", config.font_size);
        message->setFont(font);
        if (!message->text().toStdString().empty()){        
            std::string current_text = message->text().toStdString();
            size_t line_count = std::count(current_text.begin(), current_text.end(), '\n');
            // Ensure only the last line is kept if there are already two lines
            if (line_count >= 2) {
                auto pos = current_text.find_last_of('\n');
                if (pos != std::string::npos) {
                    current_text = current_text.substr(pos + 1);
                }
                _msg = current_text + "\n" + _msg;
            }                
        }
        QString formattedText = splitTextIntoLines(QString::fromStdString(_msg), message->width(), font);
        message->setText(formattedText);       
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer display_messages: " + std::string(e.what()));
    }
}

QString CameraViewer::splitTextIntoLines(const QString& text, int maxWidth, const QFont& font) {
    try {
        QFontMetrics fontMetrics(font);
        QString result;
        QString currentLine;
        QStringList words = text.split(' ');
        // If there is only one word and it fits in the maxWidth, just return it
        if (words.size() == 1) {
            QString word = words.first();
            int wordWidth = fontMetrics.horizontalAdvance(word);
            
            if (wordWidth <= maxWidth) {
                return word;  // Single word fits, return as is
            } else {
                // Word doesn't fit, break it into two lines
                int halfLength = word.length() / 2;
                
                // Try to split the word into two parts
                QString part1 = word.left(halfLength);
                QString part2 = word.mid(halfLength);

                // Ensure both parts fit within the width
                if (fontMetrics.horizontalAdvance(part1) <= maxWidth) {
                    result += part1;
                }
                if (fontMetrics.horizontalAdvance(part2) <= maxWidth) {
                    if (!result.isEmpty()) {
                        result += '\n';
                    }
                    result += part2;
                }
                
                return result;
            }
        }
        for (const QString& word : words) {
            QString tempLine = currentLine.isEmpty() ? word : currentLine + " " + word;
            // Check if the current line fits within the maximum width
            if (fontMetrics.horizontalAdvance(tempLine) <= maxWidth) {
                currentLine = tempLine;
            } else {
                // Append the current line to the result and start a new line
                if (!result.isEmpty()) {
                    result += '\n';
                }
                result += currentLine;
                currentLine = word;
            }
        }
        // Add the last line
        if (!currentLine.isEmpty()) {
            if (!result.isEmpty()) {
                result += '\n';
            }
            result += currentLine;
        }
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer splitTextIntoLines: " + std::string(e.what()));
        return "";
    }
}

void CameraViewer::drawTaskList() {
    try {
        if (!procedure.empty()) {
            if (tasklist.empty()) {
                _index = 0;
                tasklist = procedure.at("tasks");      
                task_name->setVisible(true);
                task_list->setVisible(true); 
                task_name->setText(QString::fromStdString(procedure.at("name")[0].at("name")));
                // Check if we need to append "Exit" as the last task
                bool exitExists = std::any_of(tasklist.begin(), tasklist.end(), [](const std::map<std::string, std::string> &task) {
                    return task.at("text") == "Exit";
                });
                if (!exitExists) {
                    tasklist.push_back({{"order", std::to_string(tasklist.size() + 1)}, {"text", lang.getText("defaulttab","exit_task")}, {"completed", "false"}});
                }
            }             
            // Check if on the first task
            if (_index == 0) {
                legend_label1->setVisible(true);
                legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","next")));
                legend_label2->setVisible(false);
                legend_label3->setVisible(false);
            }

            // Check if on the "Exit" task
            else if (_index == static_cast<int>(tasklist.size()) - 1) {
                legend_label1->setVisible(true);          
                legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","exit")));
                legend_label2->setText(QString::fromStdString(lang.getText("defaulttab","previous")));
                legend_label2->setVisible(true);
            }
            // Check if in between tasks (not first, last, or Exit)
            else  {
                legend_label1->setVisible(true);
                legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","next")));
                legend_label2->setText(QString::fromStdString(lang.getText("defaulttab","previous")));
                legend_label2->setVisible(true);
            }

            // Display the current task text
            if (_index < static_cast<int>(tasklist.size())) {
                QString taskText = QString::fromStdString(tasklist[_index].at("text")).replace("\n", ", ");
                task_list->setText(taskText);
            }            
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer drawTaskList: " + std::string(e.what()));
    }
}

void CameraViewer::start_qrcode() {
    try {
        LOG_INFO("start_qrcode");
        legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","exit")));
        legend_label2->setVisible(false);
        legend_label3->setVisible(false);
        status_label->setVisible(false);
        qrcode_label->setVisible(true);
        qrcode_label->setText(QString::fromStdString(lang.getText("defaulttab","qrcode")));
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer start_qrcode: " + std::string(e.what()));
    }
}

void CameraViewer::stop_qrcode() {
    try {
        LOG_INFO("stop_qrcode");
        legend_label1->setText(QString::fromStdString(lang.getText("defaulttab","langs")));
        legend_label2->setText(QString::fromStdString(lang.getText("defaulttab","standalone")));
        legend_label2->setVisible(true);
        legend_label3->setVisible(true);
        legend_label3->setText(QString::fromStdString(lang.getText("defaulttab","call")));
        status_label->setText(QString::fromStdString(lang.getText("defaulttab","setup")));
        status_label->setVisible(true);
        qrcode_label->setVisible(false);
        qrcode_label->clear();
        qrcode_counter = 0;
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer stop_qrcode: " + std::string(e.what()));
    }
}

std::string CameraViewer::base64_decode_openssl(const std::string &encoded) {
    try {
        BIO *bio, *b64;
        char *decoded = new char[encoded.size()];
        memset(decoded, 0, encoded.size());
        
        b64 = BIO_new(BIO_f_base64());
        bio = BIO_new_mem_buf(encoded.c_str(), encoded.size());
        bio = BIO_push(b64, bio);

        // Disable line breaks
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

        // Perform decoding
        int decoded_length = BIO_read(bio, decoded, encoded.size());
        BIO_free_all(bio);

        std::string result(decoded, decoded_length);
        delete[] decoded;
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer stop_qrcode: " + std::string(e.what()));
        return "";
    }
}

std::string CameraViewer::aes_decrypt_ecb(const std::string &cipherText, const std::string &key) {
    try {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create EVP_CIPHER_CTX");
        }

        unsigned char decrypted[1024] = {0}; // Adjust size based on expected output
        int len = 0, plaintext_len = 0;

        // Initialize decryption
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, 
            reinterpret_cast<const unsigned char *>(key.c_str()), NULL)) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize AES decryption");
        }

        // Disable padding
        EVP_CIPHER_CTX_set_padding(ctx, 0);

        // Perform decryption
        if (!EVP_DecryptUpdate(ctx, decrypted, &len, 
            reinterpret_cast<const unsigned char *>(cipherText.c_str()), cipherText.size())) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to decrypt");
        }
        plaintext_len = len;

        if (!EVP_DecryptFinal_ex(ctx, decrypted + len, &len)) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to finalize decryption");
        }
        plaintext_len += len;

        EVP_CIPHER_CTX_free(ctx);

        return std::string(reinterpret_cast<const char *>(decrypted), plaintext_len);
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer aes_decrypt_ecb: " + std::string(e.what()));
        return "";
    }
}

// Helper to trim padding
std::string CameraViewer::removePadding(const std::string &input, const std::string &padding) {
    size_t end = input.find_last_not_of(padding);
    return (end == std::string::npos) ? "" : input.substr(0, end + 1);
}

// QR Code Processing
void CameraViewer::processQRCode(cv::Mat _frame){
    try {
        LOG_INFO("processQRCode");
        cv::Point top_left1(337, 57);
        cv::Point bottom_right1(942, 662);
        cv::resize(_frame, resized_image, cv::Size(490, 490), 0, 0, cv::INTER_LINEAR);
        // Crop the image
        // cropped_image = resized_image(cv::Rect(top_left1.x, top_left1.y, bottom_right1.x - top_left1.x, bottom_right1.y - top_left1.y));

        // // Resize cropped image to 490x490
        // cv::resize(cropped_image, cropped_image_scaled, cv::Size(490, 490));
        // Convert to grayscale
        cv::cvtColor(resized_image, gray_img, cv::COLOR_BGR2GRAY);

        // Decode the QR code using zbar
        scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);
        zbar::Image zbarImage(gray_img.cols, gray_img.rows, "Y800", gray_img.data, gray_img.cols * gray_img.rows);
        nlohmann::json emptyData;

        if (scanner.scan(zbarImage)) {
            for (auto symbol = zbarImage.symbol_begin(); symbol != zbarImage.symbol_end(); ++symbol) {
                try {
                    // Decode the payload
                    std::string payload = symbol->get_data();

                    // Base64 decode
                    std::string encrypted_msg = base64_decode_openssl(payload);

                    // AES decrypt
                    std::string decrypted_msg = aes_decrypt_ecb(encrypted_msg, QR_CODE_KEY);

                    // Remove padding
                    std::string cleaned_str = removePadding(decrypted_msg, QR_CODE_PADDING);
                    LOG_INFO(cleaned_str);
                    QString title_message = QString::fromStdString(lang.getText("standalonetab","Wifititle") + cleaned_str);
                    floatingMessage->showMessage(title_message, 2);
                    // Parse JSON
                    Json::Value root;
                    Json::Reader reader;
                    if (reader.parse(cleaned_str, root)) {
                        if (root.isMember("s") && root.isMember("p") && root.isMember("i")) {
                            Json::Value wifiArray;
                            Json::Value wifiEntry;
                            wifiEntry["ssid"] = root["s"];
                            wifiEntry["password"] = root["p"];
                            wifiEntry["uri"] = root["i"];
                            wifiArray.append(wifiEntry);
                            Json::StreamWriterBuilder writer;
                            std::string _wifi = Json::writeString(writer, wifiArray);
                            // Emit signal (if applicable)
                            // LOG_INFO(_wifi);
                            
                            // Save to WiFi file
                            std::ofstream file(config.wifi_file);
                            if (file.is_open()) {
                                file << _wifi;
                                file.close();
                            } else {
                                LOG_ERROR("Failed to write to WiFi file");
                            }
                            FSM(emptyData, "stop_scan_positive");
                        }
                    }
                } catch (const std::exception &e) {
                    LOG_ERROR("Error processing QR code: " + std::string(e.what()));
                    FSM(emptyData, "stop_scan_negative");
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer processQRCode: " + std::string(e.what()));
    }
}

void CameraViewer::batteryiconchange(PowerManagement::BatteryStatus status) {
    try {
        QString battery_name;
        switch (status) {
            case PowerManagement::BatteryStatus::GREEN:
                if (battery_status != status) {
                    battery_status = status;
                    battery_name = QString::fromStdString(config.battery_file_full);                    
                    floatingMessage->timer_stop();
                }
                // LOG_INFO("GREEN");
                break;
            case PowerManagement::BatteryStatus::YELLOW:
                if (battery_status != status) {
                    battery_status = status;
                    battery_name = QString::fromStdString(config.battery_file_mid);
                    floatingMessage->timer_stop();
                }
                // LOG_INFO("YELLOW");
                break;
            case PowerManagement::BatteryStatus::RED:
                if (battery_status != status) {
                    battery_status = status;
                    battery_name = QString::fromStdString(config.battery_file_low); 
                    floatingMessage->timer_stop();
                }      
                // LOG_INFO("RED");          
                break;
            case PowerManagement::BatteryStatus::CRITICAL:
                if (battery_status != status) {
                    battery_status = status;
                    battery_name = QString::fromStdString(config.battery_file_empty);
                }
                // LOG_INFO("CRITICAL");    
                floatingMessage->showMessage(QString::fromStdString(config.battery_file_empty) , 3);
                break;
        }  
        if (!battery_name.isEmpty()) {
            // Use a mutex to ensure thread-safe access to the QPixmap
            // std::cout << "battery_name: " << battery_name.toStdString() << std::endl;
            
            std::unique_lock<std::mutex> lock(battery_mutex); // Use unique_lock

            // Load the pixmap
            QPixmap pixmap_battery(battery_name);
            battery_name.clear();
            int battery_wifi_Width = Swidth/30;
            int default_height = Sheight/10;
            if (current_mode.find("Standalone") != std::string::npos) {
                battery_wifi_Width = Swidth/30;
                default_height =  Sheight*0.05;
            }
            QPixmap scaledPixmapBattery = pixmap_battery.scaled(
                battery_wifi_Width,
                default_height,
                Qt::IgnoreAspectRatio,
                Qt::FastTransformation
            );
            // Explicitly unlock the mutex after critical section
            lock.unlock(); // Now safe to unlock
            QMetaObject::invokeMethod(this, [this, scaledPixmapBattery]() {
                if (current_mode.find("Standalone") != std::string::npos) {
                    batteryLabel_nav->setPixmap(scaledPixmapBattery);
                    batteryLabel_content->setPixmap(scaledPixmapBattery);
                    batteryLabel_video->setPixmap(scaledPixmapBattery);
                    batteryLabel_audio->setPixmap(scaledPixmapBattery);
                } else {
                    batteryLabel->setPixmap(scaledPixmapBattery);
                    batteryLabel_audio->setPixmap(scaledPixmapBattery);
                }
            });
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An error occurred in CameraViewer batteryiconchange: " + std::string(e.what()));
    }    
}

