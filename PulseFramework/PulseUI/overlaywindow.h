#ifndef OVERLAYWINDOW_H
#define OVERLAYWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QPropertyAnimation>
#include <QString>
#include <QStringList>

class OverlayWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal popupOpacity READ popupOpacity WRITE setPopupOpacity)
    Q_PROPERTY(int popupY READ popupY WRITE setPopupY)

public:
    explicit OverlayWindow(const QString& greeting, const QStringList& games, QWidget* parent = nullptr);

    qreal popupOpacity() const { return m_popupOpacity; }
    void setPopupOpacity(qreal v) { m_popupOpacity = v; update(); }

    int popupY() const { return m_popupY; }
    void setPopupY(int v) { m_popupY = v; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onDismiss();
    void startFadeOut();

private:
    void attachToSteamWindow();
    QRect getPopupRect() const;

    QString m_greeting;
    QStringList m_games;
    qreal m_popupOpacity = 0.0;
    int m_popupY = 0;
    int m_popupTargetY = 0;
    qreal m_dimOpacity = 0.0;

    QPropertyAnimation* m_fadeInAnim = nullptr;
    QPropertyAnimation* m_slideAnim = nullptr;
    QPropertyAnimation* m_dimAnim = nullptr;
    QTimer* m_autoCloseTimer = nullptr;
};

#endif // OVERLAYWINDOW_H
