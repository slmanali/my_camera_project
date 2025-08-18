#ifndef FLOATINGMESSAGE_H
#define FLOATINGMESSAGE_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

class FloatingMessage : public QWidget {
    Q_OBJECT
public:
    explicit FloatingMessage(QWidget *parent = nullptr) 
        : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TranslucentBackground); // Enable transparent background

        label = new QLabel(this);
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignCenter);
        
        // Set larger font size
        QFont font = label->font();
        font.setPointSize(18); // Increased from default (typically 10-12) to 18
        font.setBold(true);
        label->setFont(font);

        // label->setStyleSheet("QLabel { color: green; "
        //                     "background-color: rgba(0, 0, 0, 150); "
        //                     "border-radius: 1px; "
        //                     "padding: 1px; }");
        label->setStyleSheet("QLabel { color: red; background-color: transparent; }");

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(label);
        layout->setContentsMargins(10, 10, 10, 10);
        setLayout(layout);

        timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, &FloatingMessage::hide);
        
        setMinimumSize(600, 200); // Width: 300px, Height: 150px (adjust as needed)
        setStyleSheet("background: transparent; border: none;");
        hide();
    }

    void showMessage(const QString& message, int type =0) {
        if (currenttype == 1){
            if (type < 3) {
                label->setText(message);
                adjustSize();
                if (parentWidget()) {
                    QRect parentGeometry = parentWidget()->geometry();
                    if (type == 0) {            
                        int x = parentGeometry.right() - width();
                        int y = 5;
                        move(x, y);
                    }
                    else {
                        int x = parentGeometry.center().x() - width() / 2;
                        int y = parentGeometry.center().y() - height() / 2;
                        move(x, y);
                    }
                }
                show();
                if (type < 2)
                    timer->start(2000);
                else
                    timer->start(5000);
            }
            else {
                currenttype = 0;
                QPixmap imagefile(message);
                QPixmap scaledimagefile = imagefile.scaled(
                    600,
                    200,
                    Qt::KeepAspectRatio,
                    Qt::FastTransformation
                );
                label->setPixmap(scaledimagefile);
                adjustSize();
                if (parentWidget()) {
                    QRect parentGeometry = parentWidget()->geometry();
                    int x = parentGeometry.center().x() - width() / 2;
                    int y = parentGeometry.center().y() - height() / 2;
                    move(x, y);
                }
                show();
            }
        }
    }

    void timer_stop() {
        if (currenttype == 0){
            timer->start(1);
            currenttype = 1;
        }
    }

private:
    QLabel *label;
    QTimer *timer;
    int currenttype = 1;
};

#endif // FLOATINGMESSAGE_H