#include "overlaywindow.h"

#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QApplication>
#include <QMouseEvent>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QGraphicsDropShadowEffect>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

// Steam-like color palette
static const QColor kBgDim(0, 0, 0, 180);           // Dimming overlay
static const QColor kPopupBg(27, 40, 56);            // Steam dark blue #1b2838
static const QColor kPopupBorder(102, 192, 244, 120); // Steam blue glow border
static const QColor kTitleColor(255, 255, 255);       // White title
static const QColor kBodyColor(198, 213, 226);        // Light grey body text
static const QColor kAccentBlue(102, 192, 244);        // Steam accent blue #66c0f4
static const QColor kButtonBg(42, 71, 94);             // Button background
static const QColor kButtonHover(53, 88, 116);         // Button hover
static const QColor kButtonText(255, 255, 255);        // Button text
static const QColor kGameTagBg(42, 71, 94);            // Game tag background
static const QColor kGameTagText(102, 192, 244);       // Game tag text
static const QColor kSubtitle(124, 142, 158);          // Subtitle grey

static const int kPopupWidth = 480;
static const int kPopupRadius = 8;
static const int kPadding = 28;
static const int kButtonWidth = 120;
static const int kButtonHeight = 36;

OverlayWindow::OverlayWindow(const QString& greeting, const QStringList& games, QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_greeting(greeting)
    , m_games(games)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);

    // Find Steam window and attach to it
    attachToSteamWindow();

    // Calculate popup target position (centered)
    int popupHeight = 200;
    if (!m_games.isEmpty()) popupHeight += 60 + m_games.size() * 32;
    m_popupTargetY = (height() - popupHeight) / 2;
    m_popupY = m_popupTargetY + 40; // Start 40px below target for slide-up

    // Fade-in animation for popup
    m_fadeInAnim = new QPropertyAnimation(this, "popupOpacity", this);
    m_fadeInAnim->setDuration(400);
    m_fadeInAnim->setStartValue(0.0);
    m_fadeInAnim->setEndValue(1.0);
    m_fadeInAnim->setEasingCurve(QEasingCurve::OutCubic);

    // Slide-up animation
    m_slideAnim = new QPropertyAnimation(this, "popupY", this);
    m_slideAnim->setDuration(500);
    m_slideAnim->setStartValue(m_popupY);
    m_slideAnim->setEndValue(m_popupTargetY);
    m_slideAnim->setEasingCurve(QEasingCurve::OutCubic);

    // Auto-close timer (12 seconds)
    m_autoCloseTimer = new QTimer(this);
    m_autoCloseTimer->setSingleShot(true);
    m_autoCloseTimer->setInterval(12000);
    connect(m_autoCloseTimer, &QTimer::timeout, this, &OverlayWindow::startFadeOut);

    // Start animations
    m_fadeInAnim->start();
    m_slideAnim->start();
    m_autoCloseTimer->start();
}

void OverlayWindow::attachToSteamWindow() {
#ifdef Q_OS_WIN
    // Try to find the Steam window
    HWND steamHwnd = nullptr;
    
    // Steam uses different window class names; try common ones
    steamHwnd = FindWindowA("SDL_app", nullptr);
    if (!steamHwnd) steamHwnd = FindWindowA("vguiPopupWindow", nullptr);
    if (!steamHwnd) steamHwnd = FindWindowA("CUIEngineWin32", nullptr);
    
    if (steamHwnd && IsWindowVisible(steamHwnd)) {
        RECT rc;
        GetWindowRect(steamHwnd, &rc);
        setGeometry(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
        
        // Set our window to appear right above Steam
        SetWindowPos((HWND)winId(), HWND_TOPMOST, 
                     rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, 
                     SWP_SHOWWINDOW);
    } else {
        // Fallback: use primary screen
        QScreen* screen = QApplication::primaryScreen();
        if (screen) {
            setGeometry(screen->availableGeometry());
        }
    }
#else
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        setGeometry(screen->availableGeometry());
    }
#endif
}

QRect OverlayWindow::getPopupRect() const {
    int popupHeight = 200;
    if (!m_games.isEmpty()) popupHeight += 60 + m_games.size() * 32;
    int x = (width() - kPopupWidth) / 2;
    return QRect(x, m_popupY, kPopupWidth, popupHeight);
}

void OverlayWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // 1. Draw dim overlay
    QColor dim = kBgDim;
    dim.setAlphaF(0.7 * m_popupOpacity);
    p.fillRect(rect(), dim);

    if (m_popupOpacity <= 0.01) return;

    p.setOpacity(m_popupOpacity);

    QRect popupRect = getPopupRect();

    // 2. Draw popup shadow
    {
        QPainterPath shadowPath;
        QRect shadowRect = popupRect.adjusted(-4, -4, 4, 4);
        shadowPath.addRoundedRect(shadowRect, kPopupRadius + 2, kPopupRadius + 2);
        QColor shadowColor(0, 0, 0, 120);
        p.fillPath(shadowPath, shadowColor);
    }

    // 3. Draw popup background
    {
        QPainterPath bgPath;
        bgPath.addRoundedRect(popupRect, kPopupRadius, kPopupRadius);
        
        // Gradient background
        QLinearGradient grad(popupRect.topLeft(), popupRect.bottomLeft());
        grad.setColorAt(0.0, QColor(27, 40, 56));    // #1b2838
        grad.setColorAt(1.0, QColor(21, 32, 43));    // slightly darker
        p.fillPath(bgPath, grad);

        // Glow border
        QPen borderPen(kPopupBorder, 1.5);
        p.setPen(borderPen);
        p.drawPath(bgPath);
    }

    int yPos = popupRect.top() + kPadding;
    int textLeft = popupRect.left() + kPadding;
    int textWidth = popupRect.width() - kPadding * 2;

    // 4. Draw accent line at top
    {
        QLinearGradient lineGrad(popupRect.left() + kPadding, 0, popupRect.right() - kPadding, 0);
        lineGrad.setColorAt(0.0, kAccentBlue);
        lineGrad.setColorAt(1.0, QColor(102, 192, 244, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(lineGrad);
        p.drawRect(popupRect.left() + kPadding, popupRect.top(), textWidth, 2);
    }

    yPos += 8;

    // 5. Draw wave emoji + title
    {
        QFont titleFont("Segoe UI", 16, QFont::Bold);
        p.setFont(titleFont);
        p.setPen(kTitleColor);

        QString titleText = QString::fromUtf8("\xf0\x9f\x91\x8b") + "  Welcome back!";
        p.drawText(textLeft, yPos, textWidth, 30, Qt::AlignLeft | Qt::AlignVCenter, titleText);
        yPos += 38;
    }

    // 6. Draw body text
    {
        QFont bodyFont("Segoe UI", 10);
        p.setFont(bodyFont);
        p.setPen(kBodyColor);

        QString body = m_greeting;
        QFontMetrics fm(bodyFont);
        QRect bodyRect(textLeft, yPos, textWidth, 80);
        p.drawText(bodyRect, Qt::AlignLeft | Qt::TextWordWrap, body);
        QRect boundingRect = fm.boundingRect(bodyRect, Qt::AlignLeft | Qt::TextWordWrap, body);
        yPos += boundingRect.height() + 16;
    }

    // 7. Draw recently added games section
    if (!m_games.isEmpty()) {
        // Section header
        {
            QFont sectionFont("Segoe UI", 9);
            p.setFont(sectionFont);
            p.setPen(kSubtitle);
            p.drawText(textLeft, yPos, textWidth, 20, Qt::AlignLeft, "RECENTLY ADDED");
            yPos += 24;
        }

        // Game tags
        {
            QFont tagFont("Segoe UI", 9, QFont::DemiBold);
            p.setFont(tagFont);
            QFontMetrics fm(tagFont);

            for (const QString& game : m_games) {
                int tagW = fm.horizontalAdvance(game) + 20;
                int tagH = 26;

                QPainterPath tagPath;
                QRect tagRect(textLeft, yPos, tagW, tagH);
                tagPath.addRoundedRect(tagRect, 4, 4);
                p.fillPath(tagPath, kGameTagBg);

                // Thin left accent
                p.fillRect(textLeft, yPos + 3, 3, tagH - 6, kAccentBlue);

                p.setPen(kGameTagText);
                p.drawText(tagRect.adjusted(14, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, game);

                yPos += tagH + 6;
            }
        }
        yPos += 8;
    }

    // 8. Draw "Got it" button
    {
        QFont btnFont("Segoe UI", 10, QFont::DemiBold);
        p.setFont(btnFont);

        int btnX = popupRect.right() - kPadding - kButtonWidth;
        int btnY = popupRect.bottom() - kPadding - kButtonHeight;
        QRect btnRect(btnX, btnY, kButtonWidth, kButtonHeight);

        // Button gradient
        QLinearGradient btnGrad(btnRect.topLeft(), btnRect.bottomLeft());
        btnGrad.setColorAt(0.0, QColor(52, 103, 142));
        btnGrad.setColorAt(1.0, QColor(42, 85, 118));
        
        QPainterPath btnPath;
        btnPath.addRoundedRect(btnRect, 4, 4);
        p.fillPath(btnPath, btnGrad);
        
        // Button border
        p.setPen(QPen(QColor(102, 192, 244, 80), 1));
        p.drawPath(btnPath);

        p.setPen(kButtonText);
        p.drawText(btnRect, Qt::AlignCenter, "Got it");
    }

    // 9. Draw subtle "Pulse Framework" watermark
    {
        QFont wmFont("Segoe UI", 7);
        p.setFont(wmFont);
        p.setPen(QColor(255, 255, 255, 40));
        p.drawText(popupRect.left() + kPadding, popupRect.bottom() - 14,
                   "Pulse Framework");
    }

    p.setOpacity(1.0);
}

void OverlayWindow::mousePressEvent(QMouseEvent* event) {
    QRect popupRect = getPopupRect();
    
    // Check if "Got it" button was clicked
    int btnX = popupRect.right() - kPadding - kButtonWidth;
    int btnY = popupRect.bottom() - kPadding - kButtonHeight;
    QRect btnRect(btnX, btnY, kButtonWidth, kButtonHeight);

    if (btnRect.contains(event->pos())) {
        onDismiss();
        return;
    }

    // Click outside popup area dismisses
    if (!popupRect.contains(event->pos())) {
        onDismiss();
        return;
    }
}

void OverlayWindow::onDismiss() {
    m_autoCloseTimer->stop();
    startFadeOut();
}

void OverlayWindow::startFadeOut() {
    auto* fadeOut = new QPropertyAnimation(this, "popupOpacity", this);
    fadeOut->setDuration(300);
    fadeOut->setStartValue(m_popupOpacity);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InCubic);
    connect(fadeOut, &QPropertyAnimation::finished, this, &QWidget::close);
    connect(fadeOut, &QPropertyAnimation::finished, qApp, &QApplication::quit);
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}
