#ifndef FRIENDPOPOVER_H
#define FRIENDPOPOVER_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPropertyAnimation>

class FriendPopover : public QWidget {
    Q_OBJECT
public:
    explicit FriendPopover(const QString& friendUsername, const QString& avatarUrl, QWidget* parent = nullptr);
    ~FriendPopover() override = default;

    void popup(const QPoint& targetPos);

signals:
    void messageClicked(const QString& username);
    void viewProfileClicked(const QString& username);
    void removeFriendClicked(const QString& username);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUI();
    
    QString m_friendUsername;
    QString m_avatarUrl;
    QPropertyAnimation* m_opacityAnim;
};

#endif // FRIENDPOPOVER_H
