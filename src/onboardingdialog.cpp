#include "onboardingdialog.h"
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
#include <QKeyEvent>
#include <cmath>

// ============================================================================
// SLEEK MODULAR LOADING WIDGET IMPLEMENTATION
// ============================================================================
LoadingWidget::LoadingWidget(QWidget* parent)
    : QWidget(parent)
    , m_progress(0.0f)
    , m_time(0.0f)
    , m_stage(0)
    , m_textOpacity(1.0f)
    , m_statusText("verifying details")
    , m_authReply(nullptr)
    , m_authCompleted(false)
    , m_authSuccess(false)
    , m_patchWorker(nullptr)
    , m_patchCompleted(false)
    , m_patchSuccess(false)
    , m_errorMsg("")
{
    m_networkManager = new QNetworkAccessManager(this);
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &LoadingWidget::onUpdate);

    // Load tool logo icon
    QString iconPath = QCoreApplication::applicationDirPath() + "/icon.png";
    if (!QFile::exists(iconPath)) {
        iconPath = "icon.png";
    }
    m_logoPixmap.load(iconPath);
}

LoadingWidget::~LoadingWidget() {
    stop();
}

void LoadingWidget::start(const QString& username, const QString& password, bool isRegister, bool isGuest) {
    m_inputUsername = username;
    m_inputPassword = password;
    m_isRegister = isRegister;
    m_isGuest = isGuest;
    m_progress = 0.0f;
    m_time = 0.0f;
    m_stage = 0;
    m_textOpacity = 1.0f;
    m_statusText = "verifying details";
    m_authCompleted = false;
    m_authSuccess = false;
    m_patchCompleted = false;
    m_patchSuccess = false;
    m_errorMsg = "";

    m_animationTimer->start(16);
    m_elapsedTimer.start();

    if (m_isGuest) {
        m_authCompleted = true;
        m_authSuccess = true;
        m_verifiedUsername = "Guest";
        m_stage = 1;
        m_statusText = "patching steam";
        startPatching();
    } else {
        startAuth();
    }
}

void LoadingWidget::stop() {
    m_animationTimer->stop();
    if (m_authReply) {
        m_authReply->abort();
        m_authReply->deleteLater();
        m_authReply = nullptr;
    }
}

void LoadingWidget::startAuth() {
    QString endpoint = m_isRegister ? "/api/user/register" : "/api/user/login";
    QNetworkRequest req(QUrl(Config::WEBSERVER_BASE_URL + endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["username"] = m_inputUsername;
    body["password"] = m_inputPassword;

    m_authReply = m_networkManager->post(req, QJsonDocument(body).toJson());
    connect(m_authReply, &QNetworkReply::finished, this, &LoadingWidget::onAuthFinished);
}

void LoadingWidget::onAuthFinished() {
    if (!m_authReply) return;
    m_authReply->deleteLater();

    QByteArray responseData = m_authReply->readAll();
    QJsonObject obj = QJsonDocument::fromJson(responseData).object();

    if (m_authReply->error() == QNetworkReply::NoError && obj["success"].toBool()) {
        m_userData = obj["user"].toObject();
        m_verifiedUsername = m_userData["username"].toString();
        m_authSuccess = true;
        m_authCompleted = true;
        
        // Start patching steam as soon as authentication finishes successfully
        startPatching();
    } else {
        m_authSuccess = false;
        m_authCompleted = true;

        QString errorMsg = "Auth failed";
        if (!obj["error"].toString().isEmpty()) {
            errorMsg = obj["error"].toString();
        } else if (m_authReply->error() != QNetworkReply::NoError) {
            errorMsg = m_authReply->errorString();
        } else if (m_authReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            errorMsg = "Invalid username or password";
        }
        
        m_errorMsg = errorMsg;
        stop();
        emit failed(errorMsg);
    }
    m_authReply = nullptr;
}

void LoadingWidget::startPatching() {
    m_patchWorker = new SteamPatchWorker(this);
    connect(m_patchWorker, &SteamPatchWorker::log, this, &LoadingWidget::onPatchLog);
    connect(m_patchWorker, &SteamPatchWorker::finished, this, &LoadingWidget::onPatchFinished);
    connect(m_patchWorker, &SteamPatchWorker::error, this, &LoadingWidget::onPatchError);
    connect(m_patchWorker, &QThread::finished, m_patchWorker, &QObject::deleteLater);
    m_patchWorker->start();
}

void LoadingWidget::onPatchLog(const QString& msg, const QString& level) {
    qDebug() << "[Loader Widget]" << level << ":" << msg;
}

void LoadingWidget::onPatchFinished(const QString&) {
    m_patchSuccess = true;
    m_patchCompleted = true;
}

void LoadingWidget::onPatchError(const QString& err) {
    m_patchSuccess = false;
    m_patchCompleted = true;
    m_errorMsg = "Steam patch failed: " + err;
    stop();
    emit failed(m_errorMsg);
}

void LoadingWidget::onUpdate() {
    float deltaTime = m_elapsedTimer.restart() / 1000.0f;
    if (deltaTime > 0.1f) deltaTime = 0.1f;

    m_time += deltaTime;

    // Progress sequencing
    if (m_stage == 0) {
        if (m_progress < 0.50f) {
            m_progress += deltaTime * 0.20f;
            if (m_progress > 0.50f) m_progress = 0.50f;
        }
        if (m_authCompleted && m_authSuccess && m_progress >= 0.50f) {
            m_stage = 1;
        }
    } else {
        if (m_statusText != "patching steam") {
            m_textOpacity -= deltaTime * 6.0f;
            if (m_textOpacity <= 0.0f) {
                m_statusText = "patching steam";
                m_textOpacity = 0.0f;
            }
        } else if (m_textOpacity < 1.0f) {
            m_textOpacity += deltaTime * 5.0f;
            if (m_textOpacity > 1.0f) m_textOpacity = 1.0f;
        }

        if (m_progress < 1.0f) {
            m_progress += deltaTime * 0.15f;
            if (m_progress > 1.0f) m_progress = 1.0f;
        }

        // Complete loading successfully when bar is 100% and patch is done
        if (m_progress >= 1.0f && m_patchCompleted) {
            m_animationTimer->stop();
            if (m_patchSuccess) {
                emit success();
            } else {
                emit failed(m_errorMsg);
            }
        }
    }

    update();
}

void LoadingWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int radius = 20;
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect(), radius, radius);
    p.setClipPath(clipPath);

    // Background: Absolute solid pure black (#000000)
    p.fillRect(rect(), QColor(0, 0, 0));

    QPointF center(width() * 0.5, height() * 0.42);
    float outerRadius = 44.0f;
    
    // Outer crisp ring (slightly transparent white)
    QPen ringPen(QColor(255, 255, 255, 240), 2.2f);
    p.setPen(ringPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center, outerRadius, outerRadius);

    // pulsation factor
    float expandFactor = 1.0f + std::sin(m_time * 5.0f) * 0.04f;

    // Draw tool logo icon.png inside the center ring
    p.save();
    p.translate(center);
    float rotationDegrees = m_time * 40.0f; // continuous spinning rotation
    p.rotate(rotationDegrees);
    p.scale(expandFactor, expandFactor);
    
    if (!m_logoPixmap.isNull()) {
        int iconSize = 54;
        p.drawPixmap(-iconSize/2, -iconSize/2, iconSize, iconSize, m_logoPixmap);
    }
    p.restore();

    // Mechanical linkages
    QPointF pivotLeft(center.x() - 6.0f + std::cos(m_time * 2.5f) * 0.8f, 
                      center.y() - 1.0f + std::sin(m_time * 2.5f) * 0.8f);
    QPointF gearRight(center.x() + 15.0f, center.y() - 9.0f);
    QPointF stemEnd(pivotLeft.x() - 13.0f + std::sin(m_time * 4.0f) * 1.2f, 
                    pivotLeft.y() - 28.0f + std::cos(m_time * 4.0f) * 1.2f);

    // Draw crankshaft stem
    QPen stemPen(QColor(255, 255, 255), 5.0f);
    stemPen.setCapStyle(Qt::RoundCap);
    p.setPen(stemPen);
    p.drawLine(pivotLeft, stemEnd);

    // Draw main mechanical linkage bar
    QPen linkageOuterPen(QColor(255, 255, 255), 7.0f);
    linkageOuterPen.setCapStyle(Qt::RoundCap);
    p.setPen(linkageOuterPen);
    p.drawLine(pivotLeft, gearRight);

    // Black inner groove of linkage bar
    QPen linkageInnerPen(QColor(0, 0, 0), 1.5f);
    linkageInnerPen.setCapStyle(Qt::RoundCap);
    p.setPen(linkageInnerPen);
    p.drawLine(pivotLeft, gearRight);

    // Left pivot disk
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0));
    p.drawEllipse(pivotLeft, 8.0f, 8.0f);

    p.setPen(QPen(QColor(255, 255, 255), 2.5f));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(pivotLeft, 8.0f, 8.0f);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255));
    p.drawEllipse(pivotLeft, 3.0f, 3.0f);

    // Right rotating disk
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0));
    p.drawEllipse(gearRight, 14.0f, 14.0f);

    p.setPen(QPen(QColor(255, 255, 255), 2.5f));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(gearRight, 14.0f, 14.0f);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255));
    p.drawEllipse(gearRight, 7.0f * expandFactor, 7.0f * expandFactor);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0));
    p.drawEllipse(gearRight, 3.2f, 3.2f);
    
    // Draw Progress Bar
    float barWidth = 380.0f;
    float barHeight = 3.0f;
    QPointF barMin(center.x() - barWidth * 0.5f, center.y() + 85.0f);
    p.fillRect(QRectF(barMin, QSizeF(barWidth, barHeight)), QColor(255, 255, 255, 25));

    float fillWidth = barWidth * m_progress;
    if (fillWidth > 0.0f) {
        p.fillRect(QRectF(barMin, QSizeF(fillWidth, barHeight)), QColor(255, 255, 255));
    }

    // Draw Sequential Text
    QFont textFont("Segoe UI");
    textFont.setPointSizeF(10.0);
    textFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    textFont.setWeight(QFont::Light);
    p.setFont(textFont);

    QColor textColor(255, 255, 255, static_cast<int>(178 * m_textOpacity));
    p.setPen(textColor);

    QRectF textRect(center.x() - 250, barMin.y() + 16.0f, 500, 30);
    p.drawText(textRect, Qt::AlignCenter, m_statusText);
}

// ============================================================================
// MAIN ONBOARDING DIALOG IMPLEMENTATION
// ============================================================================
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
    if (!QFile::exists(imgPath)) {
        imgPath = "login_bg.png";
    }
    if (QFile::exists(imgPath)) {
        m_bgImage.load(imgPath);
    }
    
    // Network & Debounce
    m_networkManager = new QNetworkAccessManager(this);
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(300);
    connect(m_debounceTimer, &QTimer::timeout, this, &OnboardingDialog::checkAvailability);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &OnboardingDialog::onCheckFinished);

    // ── Build root layout using QStackedWidget container ──
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_stackedWidget = new QStackedWidget(this);
    rootLayout->addWidget(m_stackedWidget);

    // Page 0: Login Form View
    m_loginPageWidget = new QWidget(this);
    m_stackedWidget->addWidget(m_loginPageWidget);

    // Page 1: Cinematic Steam Loading View
    m_loadingWidget = new LoadingWidget(this);
    m_stackedWidget->addWidget(m_loadingWidget);

    // ── Build standard horizontal layout for Page 0 ──
    QHBoxLayout* mainLayout = new QHBoxLayout(m_loginPageWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Left Panel (Image)
    m_leftPanel = new QWidget(m_loginPageWidget);
    m_leftPanel->setFixedWidth(360);
    m_leftPanel->setStyleSheet("background: transparent;");
    mainLayout->addWidget(m_leftPanel);
    
    // Right Panel (Form)
    m_rightPanel = new QWidget(m_loginPageWidget);
    m_rightPanel->setStyleSheet("background: transparent;");
    mainLayout->addWidget(m_rightPanel);
    
    QVBoxLayout* formLayout = new QVBoxLayout(m_rightPanel);
    formLayout->setContentsMargins(45, 45, 45, 35);
    formLayout->setSpacing(0);
    
    // Tab Row: Sign In | Sign Up
    QHBoxLayout* tabRow = new QHBoxLayout();
    tabRow->setSpacing(30);
    tabRow->setAlignment(Qt::AlignLeft);
    
    m_tabSignIn = new QLabel("Sign In", m_rightPanel);
    m_tabSignIn->setCursor(Qt::PointingHandCursor);
    m_tabSignIn->setStyleSheet(
        "font-size: 22px; font-weight: bold; color: #FFFFFF; font-family: 'Segoe UI';"
        "padding-bottom: 6px;"
    );
    
    m_tabSignUp = new QLabel("Sign Up", m_rightPanel);
    m_tabSignUp->setCursor(Qt::PointingHandCursor);
    m_tabSignUp->setStyleSheet(
        "font-size: 22px; font-weight: normal; color: rgba(255,255,255,100); font-family: 'Segoe UI';"
        "padding-bottom: 6px;"
    );
    
    m_tabSignIn->installEventFilter(this);
    m_tabSignUp->installEventFilter(this);
    
    tabRow->addWidget(m_tabSignIn);
    tabRow->addWidget(m_tabSignUp);
    tabRow->addStretch();
    formLayout->addLayout(tabRow);
    formLayout->addSpacing(40);
    
    // Username Input
    QLabel* userLabel = new QLabel("Your username", m_rightPanel);
    userLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,100); font-family: 'Segoe UI'; margin-bottom: 2px;");
    formLayout->addWidget(userLabel);
    
    m_usernameInput = new QLineEdit(m_rightPanel);
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
    
    // Password Input
    QLabel* passLabel = new QLabel("Your password", m_rightPanel);
    passLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,100); font-family: 'Segoe UI'; margin-bottom: 2px;");
    formLayout->addWidget(passLabel);
    
    m_passwordInput = new QLineEdit(m_rightPanel);
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
    
    // Status Label
    m_statusLabel = new QLabel("", m_rightPanel);
    m_statusLabel->setStyleSheet("font-size: 12px; margin-top: 8px; color: #F2B8B5;");
    m_statusLabel->setWordWrap(true);
    formLayout->addWidget(m_statusLabel);
    
    formLayout->addSpacing(28);
    
    // Primary Button (SIGN IN / SIGN UP)
    m_continueBtn = new QPushButton("SIGN IN", m_rightPanel);
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
    
    // "or" divider
    QHBoxLayout* orRow = new QHBoxLayout();
    auto makeLine = [this]() {
        QFrame* line = new QFrame(m_rightPanel);
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("background: rgba(255,255,255,15); max-height: 1px;");
        return line;
    };
    QLabel* orLabel = new QLabel("or", m_rightPanel);
    orLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,80); font-family: 'Segoe UI'; padding: 0 12px;");
    orLabel->setAlignment(Qt::AlignCenter);
    orRow->addWidget(makeLine());
    orRow->addWidget(orLabel);
    orRow->addWidget(makeLine());
    formLayout->addLayout(orRow);
    
    formLayout->addSpacing(16);
    
    // Guest Button
    m_guestBtn = new QPushButton("CONTINUE AS GUEST", m_rightPanel);
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
    
    // Footer
    QLabel* footer = new QLabel("Privacy · Terms · About", m_rightPanel);
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet("font-size: 11px; color: rgba(255,255,255,60); font-family: 'Segoe UI';");
    formLayout->addWidget(footer);
    
    // ── Stacked Widget Connections ──
    connect(m_loadingWidget, &LoadingWidget::success, this, [this]() {
        m_userData = m_loadingWidget->userData();
        m_username = m_loadingWidget->username();
        m_isGuest = m_loadingWidget->isGuest();
        accept();
    });
    
    connect(m_loadingWidget, &LoadingWidget::failed, this, [this](QString errMsg) {
        m_stackedWidget->setCurrentIndex(0);
        m_statusLabel->setText("✗ " + errMsg);
        m_statusLabel->setStyleSheet("color: #F2B8B5;");
        m_continueBtn->setEnabled(true);
        m_continueBtn->setText(m_currentMode == LOGIN ? "SIGN IN" : "SIGN UP");
    });

    // ── Form Input Connections ──
    connect(m_usernameInput, &QLineEdit::returnPressed, this, &OnboardingDialog::onPrimaryClicked);
    connect(m_passwordInput, &QLineEdit::returnPressed, this, &OnboardingDialog::onPrimaryClicked);
    connect(m_usernameInput, &QLineEdit::textChanged, this, &OnboardingDialog::onUsernameChanged);
    connect(m_continueBtn, &QPushButton::clicked, this, &OnboardingDialog::onPrimaryClicked);
    connect(m_guestBtn, &QPushButton::clicked, this, &OnboardingDialog::onGuestClicked);
    
    m_usernameInput->setFocus();
}

bool OnboardingDialog::eventFilter(QObject* obj, QEvent* event) {
    if (m_stackedWidget->currentIndex() == 1) return true; // Block during loading
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

void OnboardingDialog::keyPressEvent(QKeyEvent* event) {
    if (m_stackedWidget->currentIndex() == 1 && event->key() == Qt::Key_Escape) {
        event->accept(); // Disable ESC closing during active loading
    } else {
        QDialog::keyPressEvent(event);
    }
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
    m_continueBtn->setEnabled(false);
    m_usernameInput->setFocus();
}

void OnboardingDialog::onUsernameChanged(const QString& text) {
    if (m_currentMode == LOGIN || m_stackedWidget->currentIndex() == 1) return;
    
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
    if (m_stackedWidget->currentIndex() == 1) return;
    QString url = Config::WEBSERVER_BASE_URL + "/api/user/check/" + m_usernameInput->text().trimmed();
    QNetworkReply* reply = m_networkManager->get(QNetworkRequest(QUrl(url)));
    reply->setProperty("type", "check");
}

void OnboardingDialog::onCheckFinished(QNetworkReply* reply) {
    reply->deleteLater();
    if (m_stackedWidget->currentIndex() == 1) return;
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

void OnboardingDialog::onGuestClicked() {
    m_stackedWidget->setCurrentIndex(1);
    m_loadingWidget->start("", "", false, true);
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
    
    // Switch to page 1 and kickstart animation + auth
    m_stackedWidget->setCurrentIndex(1);
    m_loadingWidget->start(user, pass, (m_currentMode == REGISTER), false);
}

void OnboardingDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    int radius = 20;
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect(), radius, radius);
    p.setClipPath(clipPath);
    
    // We only paint the background for the login page widget here (since Page 1 LoadingWidget paints its own black background)
    if (m_stackedWidget->currentIndex() == 0) {
        // Draw full background (dark navy #0a0d36)
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
        
        // Draw right panel background
        QRect rightRect(360, 0, width() - 360, height());
        p.fillRect(rightRect, QColor("#0a0d36"));
    }
    
    // Outer border
    p.setPen(QPen(QColor(255,255,255,20), 1.5));
    QPainterPath borderPath;
    borderPath.addRoundedRect(rect().adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    p.drawPath(borderPath);
}

QString OnboardingDialog::username() const { return m_username; }
QJsonObject OnboardingDialog::userData() const { return m_userData; }
bool OnboardingDialog::isGuest() const { return m_isGuest; }
