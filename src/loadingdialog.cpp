#include "loadingdialog.h"
#include "config.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <cmath>

LoadingDialog::LoadingDialog(const QString& username, const QString& password, bool isRegister, bool isGuest, QWidget* parent)
    : QDialog(parent)
    , m_inputUsername(username)
    , m_inputPassword(password)
    , m_isRegister(isRegister)
    , m_isGuest(isGuest)
    , m_success(false)
    , m_errorMsg("")
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
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setFixedSize(860, 520); // Matches onboarding dialog size

    m_networkManager = new QNetworkAccessManager(this);

    // Setup animation timer (60 FPS -> 16ms interval)
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &LoadingDialog::onUpdate);
    m_animationTimer->start(16);
    m_elapsedTimer.start();

    // Start DB authentication check if not a guest
    if (!m_isGuest) {
        startAuth();
    } else {
        m_authCompleted = true;
        m_authSuccess = true;
        // For guest, immediately start patching Steam
        m_stage = 1;
        m_statusText = "patching steam";
        startPatching();
    }
}

LoadingDialog::~LoadingDialog() {
    if (m_authReply) {
        m_authReply->abort();
        m_authReply->deleteLater();
    }
}

void LoadingDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        // Prevent closing on escape
        event->accept();
    } else {
        QDialog::keyPressEvent(event);
    }
}

void LoadingDialog::startAuth() {
    QString endpoint = m_isRegister ? "/api/user/register" : "/api/user/login";
    QNetworkRequest req(QUrl(Config::WEBSERVER_BASE_URL + endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["username"] = m_inputUsername;
    body["password"] = m_inputPassword;

    m_authReply = m_networkManager->post(req, QJsonDocument(body).toJson());
    connect(m_authReply, &QNetworkReply::finished, this, &LoadingDialog::onAuthFinished);
}

void LoadingDialog::onAuthFinished() {
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
        m_success = false;
        reject();
    }
    m_authReply = nullptr;
}

void LoadingDialog::startPatching() {
    m_patchWorker = new SteamPatchWorker(this);
    connect(m_patchWorker, &SteamPatchWorker::log, this, &LoadingDialog::onPatchLog);
    connect(m_patchWorker, &SteamPatchWorker::finished, this, &LoadingDialog::onPatchFinished);
    connect(m_patchWorker, &SteamPatchWorker::error, this, &LoadingDialog::onPatchError);
    connect(m_patchWorker, &QThread::finished, m_patchWorker, &QObject::deleteLater);
    m_patchWorker->start();
}

void LoadingDialog::onPatchLog(const QString& msg, const QString& level) {
    // We can print logs to debug console, but loading screen remains clean
    qDebug() << "[Loader PatchLog]" << level << ":" << msg;
}

void LoadingDialog::onPatchFinished(const QString&) {
    m_patchSuccess = true;
    m_patchCompleted = true;
}

void LoadingDialog::onPatchError(const QString& err) {
    m_patchSuccess = false;
    m_patchCompleted = true;
    m_errorMsg = "Steam patch failed: " + err;
    m_success = false;
    reject();
}

void LoadingDialog::onUpdate() {
    float deltaTime = m_elapsedTimer.restart() / 1000.0f;
    if (deltaTime > 0.1f) deltaTime = 0.1f; // Cap delta to avoid jumps

    m_time += deltaTime;

    // Progress sequencing
    if (m_stage == 0) {
        // Stage 0: Verifying details
        if (m_progress < 0.50f) {
            m_progress += deltaTime * 0.20f; // Rapid up to 50%
            if (m_progress > 0.50f) m_progress = 0.50f;
        }

        // Transition to Stage 1 when authentication succeeds
        if (m_authCompleted && m_authSuccess && m_progress >= 0.50f) {
            m_stage = 1;
        }
    } else {
        // Stage 1: Patching steam
        // Smoothly fade out text and fade in "patching steam"
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

        // Fill remaining progress bar
        if (m_progress < 1.0f) {
            m_progress += deltaTime * 0.15f; // Takes ~3.3 seconds to finish
            if (m_progress > 1.0f) m_progress = 1.0f;
        }

        // Automatically accept once everything is completed and progress is 100%
        if (m_progress >= 1.0f && m_patchCompleted) {
            if (m_patchSuccess) {
                m_success = true;
                accept();
            } else {
                reject();
            }
        }
    }

    update(); // Trigger paintEvent
}

void LoadingDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Apply exact rounded corners window clip matching Onboarding Dialog
    int radius = 20;
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect(), radius, radius);
    p.setClipPath(clipPath);

    // Background: Absolute solid pure black (#000000)
    p.fillRect(rect(), QColor(0, 0, 0));

    QPointF center(width() * 0.5, height() * 0.42);

    // ==========================================
    // 1. DRAW MECHANICAL STEAM LOGO
    // ==========================================
    float outerRadius = 44.0f;
    
    // Outer crisp ring (slightly transparent white)
    QPen ringPen(QColor(255, 255, 255, 240), 2.2f);
    p.setPen(ringPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center, outerRadius, outerRadius);

    // Rotation calculations using sine/cosine time variables
    // Left pivot point
    QPointF pivotLeft(center.x() - 6.0f + std::cos(m_time * 2.5f) * 0.8f, 
                      center.y() - 1.0f + std::sin(m_time * 2.5f) * 0.8f);
    
    // Right gears center
    QPointF gearRight(center.x() + 15.0f, center.y() - 9.0f);
    
    // Angled crankshaft coordinate updates
    QPointF stemEnd(pivotLeft.x() - 13.0f + std::sin(m_time * 4.0f) * 1.2f, 
                    pivotLeft.y() - 28.0f + std::cos(m_time * 4.0f) * 1.2f);

    // Draw crankshaft stem
    QPen stemPen(QColor(255, 255, 255), 5.0f);
    stemPen.setCapStyle(Qt::RoundCap);
    p.setPen(stemPen);
    p.drawLine(pivotLeft, stemEnd);

    // Draw main mechanical linkage bar (flywheel linkage)
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

    float expandFactor = 1.0f + std::sin(m_time * 5.0f) * 0.04f;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255));
    p.drawEllipse(gearRight, 7.0f * expandFactor, 7.0f * expandFactor);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0));
    p.drawEllipse(gearRight, 3.2f, 3.2f);

    // ==========================================
    // 2. DRAW CRISP PROGRESS BAR
    // ==========================================
    float barWidth = 380.0f;
    float barHeight = 3.0f;
    QPointF barMin(center.x() - barWidth * 0.5f, center.y() + 85.0f);
    
    // Background track (rgba(255, 255, 255, 0.1))
    p.fillRect(QRectF(barMin, QSizeF(barWidth, barHeight)), QColor(255, 255, 255, 25));

    // Active progress fill (Solid pure white)
    float fillWidth = barWidth * m_progress;
    if (fillWidth > 0.0f) {
        p.fillRect(QRectF(barMin, QSizeF(fillWidth, barHeight)), QColor(255, 255, 255));
    }

    // ==========================================
    // 3. DRAW SEQUENTIAL TEXT (LOWERCASE SANS-SERIF)
    // ==========================================
    QFont textFont("Segoe UI");
    textFont.setPointSizeF(10.0);
    textFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    textFont.setWeight(QFont::Light);
    p.setFont(textFont);

    // Faded white/gray text color (rgba(255, 255, 255, 0.7 * opacity))
    QColor textColor(255, 255, 255, static_cast<int>(178 * m_textOpacity));
    p.setPen(textColor);

    QRectF textRect(center.x() - 250, barMin.y() + 16.0f, 500, 30);
    p.drawText(textRect, Qt::AlignCenter, m_statusText);
}
