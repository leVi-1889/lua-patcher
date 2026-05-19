#include "onboardingdialog.h"
#include "loadingdialog.h"
#include "utils/colors.h"
#include "config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGraphicsDropShadowEffect>
#include <QRegularExpression>
#include <QApplication>
#include <QScreen>
#include <QPainterPath>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QCoreApplication>

OnboardingDialog::OnboardingDialog(QWidget* parent)
    : QDialog(parent)
    , m_currentMode(LOGIN)
    , m_isAvailable(false)
    , m_isChecking(false)
    , m_isGuest(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    
    QScreen* screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int w = 860;
    int h = 520;
    setFixedSize(w, h);
    move(screenGeometry.center() - QPoint(w/2, h/2));
    
    // Load background image
    QString imgPath = QCoreApplication::applicationDirPath() + "/login_bg.png";
    if (QFile::exists(imgPath)) {
        m_bgImage.load(imgPath);
    }
    
    // Network
    m_networkManager = new QNetworkAccessManager(this);
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(300);
    connect(m_debounceTimer, &QTimer::timeout, this, &OnboardingDialog::checkAvailability);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &OnboardingDialog::onCheckFinished);
    
    // ── Main horizontal layout ──
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // ── Left Panel (Image) ──
    QWidget* leftPanel = new QWidget(this);
    leftPanel->setFixedWidth(360);
    leftPanel->setStyleSheet("background: transparent;");
    mainLayout->addWidget(leftPanel);
    
    // ── Right Panel (Form) ──
    m_rightPanel = new QWidget(this);
    m_rightPanel->setStyleSheet("background: transparent;");
    mainLayout->addWidget(m_rightPanel);
    
    QVBoxLayout* formLayout = new QVBoxLayout(m_rightPanel);
    formLayout->setContentsMargins(45, 45, 45, 35);
    formLayout->setSpacing(0);
    
    // ── Tab Row: Sign In | Sign Up ──
    QHBoxLayout* tabRow = new QHBoxLayout();
    tabRow->setSpacing(30);
    tabRow->setAlignment(Qt::AlignLeft);
    
    m_tabSignIn = new QLabel("Sign In");
    m_tabSignIn->setCursor(Qt::PointingHandCursor);
    m_tabSignIn->setStyleSheet(
        "font-size: 22px; font-weight: bold; color: #FFFFFF; font-family: 'Segoe UI';"
        "padding-bottom: 6px;"
    );
    
    m_tabSignUp = new QLabel("Sign Up");
    m_tabSignUp->setCursor(Qt::PointingHandCursor);
    m_tabSignUp->setStyleSheet(
        "font-size: 22px; font-weight: normal; color: rgba(255,255,255,100); font-family: 'Segoe UI';"
        "padding-bottom: 6px;"
    );
    
    // Make tabs clickable via event filter
    m_tabSignIn->installEventFilter(this);
    m_tabSignUp->installEventFilter(this);
    
    tabRow->addWidget(m_tabSignIn);
    tabRow->addWidget(m_tabSignUp);
    tabRow->addStretch();
    formLayout->addLayout(tabRow);
    formLayout->addSpacing(40);
    
    // ── Username Input ──
    QLabel* userLabel = new QLabel("Your username");
    userLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,100); font-family: 'Segoe UI'; margin-bottom: 2px;");
    formLayout->addWidget(userLabel);
    
    m_usernameInput = new QLineEdit();
    m_usernameInput->setPlaceholderText("Enter username");
    m_usernameInput->setFixedHeight(38);
    m_usernameInput->setStyleSheet(
        "QLineEdit {"
        "  background: transparent; border: none; border-bottom: 1.5px solid rgba(255,255,255,30);"
        "  color: white; padding: 4px 0px; font-size: 15px; font-family: 'Segoe UI';"
        "}"
        "QLineEdit:focus { border-bottom: 1.5px solid #00D4FF; }"
        "QLineEdit::placeholder { color: rgba(255,255,255,40); }"
    );
    formLayout->addWidget(m_usernameInput);
    formLayout->addSpacing(22);
    
    // ── Password Input ──
    QLabel* passLabel = new QLabel("Your password");
    passLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,100); font-family: 'Segoe UI'; margin-bottom: 2px;");
    formLayout->addWidget(passLabel);
    
    m_passwordInput = new QLineEdit();
    m_passwordInput->setPlaceholderText("Enter password");
    m_passwordInput->setEchoMode(QLineEdit::Password);
    m_passwordInput->setFixedHeight(38);
    m_passwordInput->setStyleSheet(
        "QLineEdit {"
        "  background: transparent; border: none; border-bottom: 1.5px solid rgba(255,255,255,30);"
        "  color: white; padding: 4px 0px; font-size: 15px; font-family: 'Segoe UI';"
        "}"
        "QLineEdit:focus { border-bottom: 1.5px solid #00D4FF; }"
        "QLineEdit::placeholder { color: rgba(255,255,255,40); }"
    );
    formLayout->addWidget(m_passwordInput);
    
    // ── Status Label ──
    m_statusLabel = new QLabel("");
    m_statusLabel->setStyleSheet("font-size: 12px; margin-top: 8px; color: #F2B8B5;");
    m_statusLabel->setWordWrap(true);
    formLayout->addWidget(m_statusLabel);
    
    formLayout->addSpacing(28);
    
    // ── Primary Button (SIGN IN / SIGN UP) ──
    m_continueBtn = new QPushButton("SIGN IN");
    m_continueBtn->setFixedHeight(48);
    m_continueBtn->setCursor(Qt::PointingHandCursor);
    m_continueBtn->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00B4D8, stop:1 #00D4FF);"
        "  color: #FFFFFF; border-radius: 24px; font-size: 14px; font-weight: bold;"
        "  font-family: 'Segoe UI'; border: none; letter-spacing: 1px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00C4E8, stop:1 #00E4FF);"
        "}"
        "QPushButton:disabled {"
        "  background: rgba(255,255,255,0.08); color: rgba(255,255,255,0.3);"
        "}"
    );
    formLayout->addWidget(m_continueBtn);
    
    formLayout->addSpacing(16);
    
    // ── "or" divider ──
    QHBoxLayout* orRow = new QHBoxLayout();
    auto makeLine = [this]() {
        QFrame* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("background: rgba(255,255,255,15); max-height: 1px;");
        return line;
    };
    QLabel* orLabel = new QLabel("or");
    orLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,80); font-family: 'Segoe UI'; padding: 0 12px;");
    orLabel->setAlignment(Qt::AlignCenter);
    orRow->addWidget(makeLine());
    orRow->addWidget(orLabel);
    orRow->addWidget(makeLine());
    formLayout->addLayout(orRow);
    
    formLayout->addSpacing(16);
    
    // ── Guest Button ──
    m_guestBtn = new QPushButton("CONTINUE AS GUEST");
    m_guestBtn->setFixedHeight(48);
    m_guestBtn->setCursor(Qt::PointingHandCursor);
    m_guestBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  color: #00D4FF; border-radius: 24px; font-size: 13px; font-weight: bold;"
        "  font-family: 'Segoe UI'; border: 1.5px solid rgba(0,212,255,0.4); letter-spacing: 1px;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(0,212,255,0.08); border-color: rgba(0,212,255,0.7);"
        "}"
    );
    formLayout->addWidget(m_guestBtn);
    
    formLayout->addStretch();
    
    // ── Footer ──
    QLabel* footer = new QLabel("Privacy · Terms · About");
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet("font-size: 11px; color: rgba(255,255,255,60); font-family: 'Segoe UI';");
    formLayout->addWidget(footer);
    
    // ── Connections ──
    connect(m_usernameInput, &QLineEdit::returnPressed, this, &OnboardingDialog::onPrimaryClicked);
    connect(m_passwordInput, &QLineEdit::returnPressed, this, &OnboardingDialog::onPrimaryClicked);
    connect(m_usernameInput, &QLineEdit::textChanged, this, &OnboardingDialog::onUsernameChanged);
    connect(m_continueBtn, &QPushButton::clicked, this, &OnboardingDialog::onPrimaryClicked);
    connect(m_guestBtn, &QPushButton::clicked, this, &OnboardingDialog::onGuestClicked);
    
    m_usernameInput->setFocus();
}

// ── Event filter for tab clicks ──
bool OnboardingDialog::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        if (obj == m_tabSignIn) {
            switchToLogin();
            return true;
        } else if (obj == m_tabSignUp) {
            switchToRegister();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void OnboardingDialog::switchToLogin() {
    m_currentMode = LOGIN;
    m_statusLabel->setText("");
    m_usernameInput->clear();
    m_passwordInput->clear();
    
    m_tabSignIn->setStyleSheet(
        "font-size: 22px; font-weight: bold; color: #FFFFFF; font-family: 'Segoe UI'; padding-bottom: 6px;"
    );
    m_tabSignUp->setStyleSheet(
        "font-size: 22px; font-weight: normal; color: rgba(255,255,255,100); font-family: 'Segoe UI'; padding-bottom: 6px;"
    );
    
    m_continueBtn->setText("SIGN IN");
    m_continueBtn->setEnabled(true);
    m_usernameInput->setFocus();
}

void OnboardingDialog::switchToRegister() {
    m_currentMode = REGISTER;
    m_statusLabel->setText("");
    m_usernameInput->clear();
    m_passwordInput->clear();
    
    m_tabSignIn->setStyleSheet(
        "font-size: 22px; font-weight: normal; color: rgba(255,255,255,100); font-family: 'Segoe UI'; padding-bottom: 6px;"
    );
    m_tabSignUp->setStyleSheet(
        "font-size: 22px; font-weight: bold; color: #FFFFFF; font-family: 'Segoe UI'; padding-bottom: 6px;"
    );
    
    m_continueBtn->setText("SIGN UP");
    m_continueBtn->setEnabled(false); // Needs username check
    m_usernameInput->setFocus();
}

void OnboardingDialog::onGuestClicked() {
    LoadingDialog loader("", "", false, true, this);
    if (loader.exec() == QDialog::Accepted) {
        m_isGuest = true;
        m_username = "Guest";
        accept();
    } else {
        m_statusLabel->setText("✗ " + loader.errorMsg());
        m_statusLabel->setStyleSheet("color: #F2B8B5;");
    }
}

void OnboardingDialog::onUsernameChanged(const QString& text) {
    if (m_currentMode == LOGIN) return;
    
    m_continueBtn->setEnabled(false);
    m_isAvailable = false;
    QString trimmed = text.trimmed();
    
    if (trimmed.isEmpty()) { m_statusLabel->setText(""); return; }
    if (trimmed.length() < 3) { m_statusLabel->setText("⚠ Minimum 3 characters"); return; }
    
    m_statusLabel->setStyleSheet("color: rgba(255,255,255,160);");
    m_statusLabel->setText("⏳ Checking...");
    m_debounceTimer->start();
}

void OnboardingDialog::checkAvailability() {
    QString url = Config::WEBSERVER_BASE_URL + "/api/user/check/" + m_usernameInput->text().trimmed();
    QNetworkReply* reply = m_networkManager->get(QNetworkRequest(QUrl(url)));
    reply->setProperty("type", "check");
}

void OnboardingDialog::onCheckFinished(QNetworkReply* reply) {
    reply->deleteLater();
    if (reply->property("type").toString() != "check") return;
    if (reply->error() != QNetworkReply::NoError) {
         m_statusLabel->setText("✗ Connection error");
         return;
    }
    
    QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    m_isAvailable = obj["available"].toBool();
    m_statusLabel->setText(m_isAvailable ? "✓ Username available" : "✗ Already taken");
    m_statusLabel->setStyleSheet(m_isAvailable ? "color: #A8DB8F;" : "color: #F2B8B5;");
    m_continueBtn->setEnabled(m_isAvailable);
}

void OnboardingDialog::onPrimaryClicked() {
    QString user = m_usernameInput->text().trimmed();
    QString pass = m_passwordInput->text();
    
    if (user.isEmpty()) { m_statusLabel->setText("⚠ Username required"); return; }
    if (pass.isEmpty()) { m_statusLabel->setText("⚠ Password required"); return; }
    
    if (m_currentMode == REGISTER && pass.length() < 6) {
        m_statusLabel->setText("⚠ Password must be at least 6 characters");
        return;
    }
    
    m_continueBtn->setEnabled(false);
    m_continueBtn->setText("Processing...");
    
    LoadingDialog loader(user, pass, (m_currentMode == REGISTER), false, this);
    if (loader.exec() == QDialog::Accepted) {
        m_userData = loader.userData();
        m_username = loader.username();
        accept();
    } else {
        m_statusLabel->setText("✗ " + loader.errorMsg());
        m_statusLabel->setStyleSheet("color: #F2B8B5;");
        m_continueBtn->setEnabled(true);
        m_continueBtn->setText(m_currentMode == LOGIN ? "SIGN IN" : "SIGN UP");
    }
}

void OnboardingDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    int radius = 20;
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect(), radius, radius);
    p.setClipPath(clipPath);
    
    // Draw full background (dark navy)
    p.fillRect(rect(), QColor("#0a0d36"));
    
    // Draw left panel image
    if (!m_bgImage.isNull()) {
        QRect leftRect(0, 0, 360, height());
        QPixmap scaled = m_bgImage.scaled(leftRect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int xOff = (scaled.width() - leftRect.width()) / 2;
        int yOff = (scaled.height() - leftRect.height()) / 2;
        p.drawPixmap(leftRect, scaled, QRect(xOff, yOff, leftRect.width(), leftRect.height()));
        
        // Subtle dark gradient overlay on the right edge of the image for blending
        QLinearGradient fadeGrad(leftRect.right() - 60, 0, leftRect.right(), 0);
        fadeGrad.setColorAt(0, Qt::transparent);
        fadeGrad.setColorAt(1, QColor("#0a0d36"));
        p.fillRect(leftRect.right() - 60, 0, 60, height(), fadeGrad);
    }
    
    // Draw right panel background (slightly different shade for depth)
    QRect rightRect(360, 0, width() - 360, height());
    p.fillRect(rightRect, QColor("#0a0d36"));
    
    // Outer border
    p.setPen(QPen(QColor(255,255,255,20), 1.5));
    QPainterPath borderPath;
    borderPath.addRoundedRect(rect().adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    p.drawPath(borderPath);
}

QString OnboardingDialog::username() const { return m_username; }
QJsonObject OnboardingDialog::userData() const { return m_userData; }
bool OnboardingDialog::isGuest() const { return m_isGuest; }
