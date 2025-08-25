#ifndef TEXTOVERLAYCONTROLLER_H
#define TEXTOVERLAYCONTROLLER_H

#include <QObject>
#include <QString>

class TextOverlayController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString overlayText READ overlayText NOTIFY overlayTextChanged)
    
public:
    explicit TextOverlayController(QObject *parent = nullptr) : QObject(parent), m_overlayText("Default Text") {}
    
    QString overlayText() const { return m_overlayText; }
    
    Q_INVOKABLE void setOverlayText(const QString &text) {
        if (m_overlayText != text) {
            m_overlayText = text;
            emit overlayTextChanged();
        }
    }
    
signals:
    void overlayTextChanged();
    
private:
    QString m_overlayText;
};

#endif // TEXTOVERLAYCONTROLLER_H