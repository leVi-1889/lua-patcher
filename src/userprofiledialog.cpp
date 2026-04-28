#include "userprofiledialog.h"
#include "config.h"
#include "materialicons.h"
#include <QPainter>
#include <QPainterPath>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>

UserProfileDialog::UserProfileDialog(const QString& targetUsername, const QString& myUsername, QNetworkAccessManager* netMgr, QWidget* parent)
    : QDialog(parent), m_targetUsername(targetUsername), m_myUsername(myUsername), m_netMgr(netMgr)
{
    m_isOwnProfile = (m_targetUsername == m_myUsername);
    
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(1000, 700);

    setupUI();
}

void UserProfileDialog::setupUI() {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ================= LEFT COLUMN (IDENTITY) =================
    QWidget* leftCol = new QWidget();
    leftCol->setFixedWidth(320);
    leftCol->setStyleSheet("background: rgba(20, 25, 35, 0.7); border-top-left-radius: 16px; border-bottom-left-radius: 16px;");
    
    QVBoxLayout* leftLayout = new QVBoxLayout(leftCol);
    leftLayout->setContentsMargins(20, 40, 20, 20);
    leftLayout->setAlignment(Qt::AlignTop);

    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(120, 120);
    m_avatarLabel->setStyleSheet("background: #4A6FA5; border-radius: 60px;");
    leftLayout->addWidget(m_avatarLabel, 0, Qt::AlignCenter);

    m_usernameLabel = new QLabel(m_targetUsername);
    m_usernameLabel->setStyleSheet("color: white; font-size: 24px; font-weight: bold; margin-top: 15px; background: transparent;");
    m_usernameLabel->setAlignment(Qt::AlignCenter);
    leftLayout->addWidget(m_usernameLabel);

    if (m_isOwnProfile) {
        m_editBtn = new QPushButton("Edit Profile");
        m_editBtn->setCursor(Qt::PointingHandCursor);
        m_editBtn->setStyleSheet(
            "QPushButton {"
            "  background: #3B82F6; color: white; font-weight: bold; border-radius: 8px; padding: 10px;"
            "}"
            "QPushButton:hover { background: #2563EB; }"
        );
        leftLayout->addWidget(m_editBtn);
        leftLayout->addSpacing(20);
    }

    mainLayout->addWidget(leftCol);

    // ================= RIGHT COLUMN (CONTENT) =================
    QWidget* rightCol = new QWidget();
    rightCol->setStyleSheet("background: rgba(15, 20, 30, 0.9); border-top-right-radius: 16px; border-bottom-right-radius: 16px;");
    
    QVBoxLayout* rightLayout = new QVBoxLayout(rightCol);
    rightLayout->setContentsMargins(30, 30, 30, 30);
    rightLayout->setSpacing(20);

    // Close button at top right
    QHBoxLayout* topBar = new QHBoxLayout();
    topBar->addStretch();
    QPushButton* closeBtn = new QPushButton("✕");
    closeBtn->setFixedSize(32, 32);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("background: rgba(255,255,255,0.1); color: white; border-radius: 16px; font-weight: bold;");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    topBar->addWidget(closeBtn);
    rightLayout->addLayout(topBar);

    // Scroll Area for widgets
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    
    QWidget* scrollContent = new QWidget();
    scrollContent->setStyleSheet("background: transparent;");
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(25);
    scrollLayout->setAlignment(Qt::AlignTop);

    // --- Games I Play Widget ---
    QWidget* playedGamesCard = new QWidget();
    playedGamesCard->setStyleSheet("background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.05); border-radius: 16px;");
    QVBoxLayout* pgl = new QVBoxLayout(playedGamesCard);
    
    QHBoxLayout* pglHeader = new QHBoxLayout();
    QLabel* pglTitle = new QLabel("Games I play\n<span style='font-size: 11px; color: gray;'>Add up to 20 games</span>");
    pglTitle->setStyleSheet("color: white; font-weight: bold; font-size: 16px; background: transparent; border: none;");
    pglHeader->addWidget(pglTitle);
    
    if (m_isOwnProfile) {
        m_addPlayedGameBtn = new QPushButton("+ Add game");
        m_addPlayedGameBtn->setStyleSheet("background: rgba(255,255,255,0.1); color: white; border-radius: 12px; padding: 6px 12px; font-weight: bold;");
        m_addPlayedGameBtn->setCursor(Qt::PointingHandCursor);
        pglHeader->addWidget(m_addPlayedGameBtn);
    }
    pgl->addLayout(pglHeader);
    
    // Grid layout for games (Placeholder for now)
    QWidget* pglGridContainer = new QWidget();
    m_playedGamesLayout = new QHBoxLayout(pglGridContainer);
    ((QHBoxLayout*)m_playedGamesLayout)->setAlignment(Qt::AlignLeft);
    pgl->addWidget(pglGridContainer);
    
    scrollLayout->addWidget(playedGamesCard);

    // --- Games in Rotation Widget ---
    QWidget* rotationGamesCard = new QWidget();
    rotationGamesCard->setStyleSheet("background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.05); border-radius: 16px;");
    QVBoxLayout* rgl = new QVBoxLayout(rotationGamesCard);
    
    QHBoxLayout* rglHeader = new QHBoxLayout();
    QLabel* rglTitle = new QLabel("Games in rotation\n<span style='font-size: 11px; color: gray;'>Add up to 5 games</span>");
    rglTitle->setStyleSheet("color: white; font-weight: bold; font-size: 16px; background: transparent; border: none;");
    rglHeader->addWidget(rglTitle);
    
    if (m_isOwnProfile) {
        m_addRotationGameBtn = new QPushButton("+ Add game");
        m_addRotationGameBtn->setStyleSheet("background: rgba(255,255,255,0.1); color: white; border-radius: 12px; padding: 6px 12px; font-weight: bold;");
        m_addRotationGameBtn->setCursor(Qt::PointingHandCursor);
        rglHeader->addWidget(m_addRotationGameBtn);
    }
    rgl->addLayout(rglHeader);
    
    QWidget* rglGridContainer = new QWidget();
    m_rotationGamesLayout = new QHBoxLayout(rglGridContainer);
    ((QHBoxLayout*)m_rotationGamesLayout)->setAlignment(Qt::AlignLeft);
    rgl->addWidget(rglGridContainer);
    
    scrollLayout->addWidget(rotationGamesCard);

    scrollArea->setWidget(scrollContent);
    rightLayout->addWidget(scrollArea);

    mainLayout->addWidget(rightCol);
}

void UserProfileDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    QPainterPath path;
    path.addRoundedRect(rect(), 16, 16);
    
    p.fillPath(path, QColor(0, 0, 0, 100)); // Base drop shadow effect
    p.setPen(QPen(QColor(255, 255, 255, 20), 1));
    p.drawPath(path);
}

void UserProfileDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    fetchProfileData();
}

void UserProfileDialog::fetchProfileData() {
    QUrl url(Config::WEBSERVER_BASE_URL + "/api/user/profile");
    url.setQuery("username=" + m_targetUsername);
    
    QNetworkReply* reply = m_netMgr->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (doc.isObject()) {
                populateData(doc.object());
            }
        }
    });
}

void UserProfileDialog::populateData(const QJsonObject& data) {
    // Populate Avatar
    QString avUrl = data["avatar_url"].toString();
    if (!avUrl.isEmpty()) {
        QPixmap original;
        original.loadFromData(QByteArray::fromBase64(avUrl.toUtf8()));
        if (!original.isNull()) {
            QPixmap rounded(120, 120);
            rounded.fill(Qt::transparent);
            QPainter p(&rounded);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addEllipse(0, 0, 120, 120);
            p.setClipPath(path);
            p.drawPixmap(0, 0, 120, 120, original.scaled(120, 120, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            m_avatarLabel->setPixmap(rounded);
        }
    }

    // Process games arrays
    // For now we just add placeholders or labels if arrays exist
    QJsonArray playedGames = data["played_games"].toArray();
    QJsonArray rotationGames = data["rotation_games"].toArray();
    
    // We will populate grid boxes later
}
