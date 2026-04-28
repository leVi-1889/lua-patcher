#include "friendpopover.h"
#include "materialicons.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QWindow>

FriendPopover::FriendPopover(const QString& friendUsername, const QString& avatarUrl, QWidget* parent)
    : QWidget(parent), m_friendUsername(friendUsername), m_avatarUrl(avatarUrl)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    // Width: 200, Height: 240. The pointer arrow will take 12px on the right.
    setFixedSize(212, 240);
    
    setupUI();

    QGraphicsOpacityEffect* opacity = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(opacity);
    opacity->setOpacity(0.0);
    
    m_opacityAnim = new QPropertyAnimation(opacity, "opacity", this);
    m_opacityAnim->setDuration(150);
    m_opacityAnim->setStartValue(0.0);
    m_opacityAnim->setEndValue(1.0);
    m_opacityAnim->setEasingCurve(QEasingCurve::OutQuad);
    
    // Install event filter on application to detect clicks outside
    qApp->installEventFilter(this);
}

void FriendPopover::setupUI() {
    // The main layout needs margins on the right for the pointer arrow
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 20, 27, 20); // 27 = 15 + 12px arrow
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setSpacing(15);

    // 1. Large Avatar
    QLabel* avatar = new QLabel();
    int avSz = 80;
    avatar->setFixedSize(avSz, avSz);
    
    QPixmap pix(avSz, avSz);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    
    if (!m_avatarUrl.isEmpty()) {
        QPixmap original;
        original.loadFromData(QByteArray::fromBase64(m_avatarUrl.toUtf8()));
        if (!original.isNull()) {
            QPainterPath path;
            path.addEllipse(0, 0, avSz, avSz);
            p.setClipPath(path);
            p.drawPixmap(0, 0, avSz, avSz, original.scaled(avSz, avSz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        }
    } else {
        p.setBrush(QColor("#4A6FA5"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, avSz, avSz);
        p.setPen(Qt::white);
        p.setFont(QFont("Segoe UI", 28, QFont::Bold));
        p.drawText(pix.rect(), Qt::AlignCenter, m_friendUsername.left(1).toUpper());
    }
    p.end();
    avatar->setPixmap(pix);
    mainLayout->addWidget(avatar, 0, Qt::AlignCenter);

    // 2. Username
    QLabel* nameLabel = new QLabel(m_friendUsername);
    nameLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold; background: transparent;");
    nameLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(nameLabel, 0, Qt::AlignCenter);

    // 3. Action Buttons Row
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);
    btnLayout->setAlignment(Qt::AlignCenter);

    QString btnStyle = 
        "QPushButton {"
        "  background: rgba(255, 255, 255, 0.08);"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 8px;"
        "  color: white;"
        "  font-size: 18px;"
        "}"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.15); }"
        "QPushButton:pressed { background: rgba(255, 255, 255, 0.2); }";

    // Message Button
    QPushButton* msgBtn = new QPushButton("💬");
    msgBtn->setFixedSize(40, 40);
    msgBtn->setCursor(Qt::PointingHandCursor);
    msgBtn->setStyleSheet(btnStyle);
    msgBtn->setToolTip("Message");
    connect(msgBtn, &QPushButton::clicked, this, [this]() {
        emit messageClicked(m_friendUsername);
        close();
    });
    btnLayout->addWidget(msgBtn);

    // View Profile Button
    QPushButton* profileBtn = new QPushButton("👤");
    profileBtn->setFixedSize(40, 40);
    profileBtn->setCursor(Qt::PointingHandCursor);
    profileBtn->setStyleSheet(btnStyle);
    profileBtn->setToolTip("View Profile");
    connect(profileBtn, &QPushButton::clicked, this, [this]() {
        emit viewProfileClicked(m_friendUsername);
        close();
    });
    btnLayout->addWidget(profileBtn);

    // Remove Friend Button
    QPushButton* removeBtn = new QPushButton("❌");
    removeBtn->setFixedSize(40, 40);
    removeBtn->setCursor(Qt::PointingHandCursor);
    QString removeStyle = btnStyle;
    removeStyle.replace("QPushButton:hover { background: rgba(255, 255, 255, 0.15); }", 
                        "QPushButton:hover { background: rgba(231, 76, 60, 0.6); border-color: #E74C3C; }");
    removeBtn->setStyleSheet(removeStyle);
    removeBtn->setToolTip("Remove Friend");
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        emit removeFriendClicked(m_friendUsername);
        close();
    });
    btnLayout->addWidget(removeBtn);

    mainLayout->addLayout(btnLayout);
}

void FriendPopover::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Dimensions
    int arrowWidth = 12;
    int arrowHeight = 20;
    int radius = 16;
    
    // The main rounded rect (excluding the arrow space on the right)
    QRectF bodyRect(0, 0, width() - arrowWidth, height());
    
    QPainterPath path;
    path.addRoundedRect(bodyRect, radius, radius);
    
    // Add the triangle pointer on the right side
    QPolygonF arrow;
    qreal midY = height() / 2.0;
    qreal rightX = width() - arrowWidth;
    
    arrow << QPointF(rightX, midY - arrowHeight / 2.0)
          << QPointF(width(), midY)
          << QPointF(rightX, midY + arrowHeight / 2.0);
          
    path.addPolygon(arrow);

    // Draw glassmorphism background
    p.fillPath(path, QColor(15, 20, 30, 240)); // Dark, slightly transparent
    p.setPen(QPen(QColor(255, 255, 255, 30), 1)); // Subtle border
    p.drawPath(path);
}

void FriendPopover::popup(const QPoint& targetPos) {
    // targetPos should be the global position of the center-left edge of the friend button
    // The popup's pointer (right edge, center Y) should point there.
    
    int x = targetPos.x() - width();
    int y = targetPos.y() - (height() / 2);
    
    move(x, y);
    show();
    m_opacityAnim->start();
}

bool FriendPopover::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        // If clicked outside of this widget, close it
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!geometry().contains(mouseEvent->globalPosition().toPoint())) {
            close();
        }
    }
    return QWidget::eventFilter(obj, event);
}
