#include "userprofiledialog.h"
#include "config.h"
#include <QPainter>
#include <QPainterPath>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QGraphicsDropShadowEffect>
#include <QApplication>

UserProfileDialog::UserProfileDialog(const QString& targetUsername, const QString& myUsername, 
                                     QNetworkAccessManager* netMgr, QWidget* parent)
    : QDialog(parent), m_targetUsername(targetUsername), m_myUsername(myUsername), m_netMgr(netMgr)
{
    m_isOwnProfile = (m_targetUsername == m_myUsername);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(500, 800);
    
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(400);
    connect(m_searchDebounce, &QTimer::timeout, this, &UserProfileDialog::doSearch);
    
    setupUI();
}

void UserProfileDialog::setupUI() {
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    
    QWidget* container = new QWidget();
    container->setObjectName("profileContainer");
    container->setStyleSheet(
        "QWidget#profileContainer {"
        "  background: #0D1117;" // Theme black
        "  border-radius: 20px;"
        "  border: 1px solid rgba(255,255,255,0.08);"
        "}"
    );
    
    QVBoxLayout* main = new QVBoxLayout(container);
    main->setContentsMargins(30,25,30,25);
    main->setSpacing(15);
    
    // === TOP BAR: Avatar + Username + Buttons ===
    QHBoxLayout* topBar = new QHBoxLayout();
    
    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(64,64);
    m_avatarLabel->setStyleSheet("background: #4A6FA5; border-radius: 32px;");
    topBar->addWidget(m_avatarLabel);
    topBar->addSpacing(15);
    
    QVBoxLayout* nameCol = new QVBoxLayout();
    m_usernameLabel = new QLabel(m_targetUsername);
    m_usernameLabel->setStyleSheet("color:white; font-size:22px; font-weight:bold; background:transparent; border:none;");
    nameCol->addWidget(m_usernameLabel);
    
    m_levelLabel = new QLabel("Level 1");
    m_levelLabel->setStyleSheet("color:rgba(255,255,255,0.4); font-size:12px; background:transparent; border:none;");
    nameCol->addWidget(m_levelLabel);
    topBar->addLayout(nameCol);
    topBar->addStretch();
    
    if (m_isOwnProfile) {
        m_editBtn = new QPushButton("✏️ Edit Profile");
        m_editBtn->setCursor(Qt::PointingHandCursor);
        m_editBtn->setStyleSheet(
            "QPushButton { background:#3B82F6; color:white; font-weight:bold; border-radius:10px; padding:8px 16px; border:none; }"
            "QPushButton:hover { background:#2563EB; }"
        );
        connect(m_editBtn, &QPushButton::clicked, this, &UserProfileDialog::onEditClicked);
        topBar->addWidget(m_editBtn);
        
        m_saveBtn = new QPushButton("💾 Save");
        m_saveBtn->setCursor(Qt::PointingHandCursor);
        m_saveBtn->setStyleSheet(
            "QPushButton { background:#22C55E; color:white; font-weight:bold; border-radius:10px; padding:8px 16px; border:none; }"
            "QPushButton:hover { background:#16A34A; }"
        );
        m_saveBtn->hide();
        connect(m_saveBtn, &QPushButton::clicked, this, &UserProfileDialog::onSaveClicked);
        topBar->addWidget(m_saveBtn);
        
        m_cancelBtn = new QPushButton("Cancel");
        m_cancelBtn->setCursor(Qt::PointingHandCursor);
        m_cancelBtn->setStyleSheet(
            "QPushButton { background:rgba(255,255,255,0.1); color:white; border-radius:10px; padding:8px 16px; border:none; }"
            "QPushButton:hover { background:rgba(255,255,255,0.15); }"
        );
        m_cancelBtn->hide();
        connect(m_cancelBtn, &QPushButton::clicked, this, &UserProfileDialog::onCancelEditClicked);
        topBar->addWidget(m_cancelBtn);
    }
    
    QPushButton* closeBtn = new QPushButton("✕");
    closeBtn->setFixedSize(32,32);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("background:rgba(255,255,255,0.1); color:white; border-radius:16px; font-weight:bold; border:none;");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    topBar->addWidget(closeBtn);
    
    main->addLayout(topBar);
    
    // === SCROLL AREA FOR GAME WIDGETS ===
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}"
        "QScrollBar:vertical{width:4px;background:transparent;}"
        "QScrollBar::handle:vertical{background:rgba(255,255,255,0.15);border-radius:2px;min-height:30px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}");
    
    QWidget* scrollContent = new QWidget();
    scrollContent->setStyleSheet("background:transparent;");
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(20);
    scrollLayout->setAlignment(Qt::AlignTop);
    
    // --- GAMES I PLAY CARD ---
    QWidget* playedCard = new QWidget();
    playedCard->setStyleSheet("background:rgba(255,255,255,0.03); border:1px solid rgba(255,255,255,0.06); border-radius:16px;");
    QVBoxLayout* pcl = new QVBoxLayout(playedCard);
    pcl->setContentsMargins(20,15,20,15);
    
    QHBoxLayout* pHeader = new QHBoxLayout();
    QLabel* pTitle = new QLabel("🎮 Games I play");
    pTitle->setStyleSheet("color:white; font-weight:bold; font-size:16px; background:transparent; border:none;");
    pHeader->addWidget(pTitle);
    pHeader->addStretch();
    
    m_addPlayedBtn = new QPushButton("+ Add game");
    m_addPlayedBtn->setCursor(Qt::PointingHandCursor);
    m_addPlayedBtn->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.1);color:white;border-radius:10px;padding:6px 14px;font-weight:bold;border:none;}"
        "QPushButton:hover{background:rgba(255,255,255,0.18);}"
    );
    m_addPlayedBtn->hide(); // Only show in edit mode
    connect(m_addPlayedBtn, &QPushButton::clicked, this, &UserProfileDialog::onAddPlayedGame);
    pHeader->addWidget(m_addPlayedBtn);
    pcl->addLayout(pHeader);
    
    m_playedGamesGrid = new QWidget();
    m_playedGamesGrid->setStyleSheet("background:transparent; border:none;");
    // Use QGridLayout to wrap cards
    QGridLayout* pGrid = new QGridLayout(m_playedGamesGrid);
    pGrid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    pGrid->setSpacing(15);
    pGrid->setContentsMargins(0,10,0,10);
    m_playedGamesLayout = pGrid;
    pcl->addWidget(m_playedGamesGrid);
    
    scrollLayout->addWidget(playedCard);
    
    // --- GAMES IN ROTATION CARD ---
    QWidget* rotCard = new QWidget();
    rotCard->setStyleSheet("background:rgba(255,255,255,0.03); border:1px solid rgba(255,255,255,0.06); border-radius:16px;");
    QVBoxLayout* rcl = new QVBoxLayout(rotCard);
    rcl->setContentsMargins(20,15,20,15);
    
    QHBoxLayout* rHeader = new QHBoxLayout();
    QLabel* rTitle = new QLabel("🔄 Games in rotation");
    rTitle->setStyleSheet("color:white; font-weight:bold; font-size:16px; background:transparent; border:none;");
    rHeader->addWidget(rTitle);
    rHeader->addStretch();
    
    m_addRotationBtn = new QPushButton("+ Add game");
    m_addRotationBtn->setCursor(Qt::PointingHandCursor);
    m_addRotationBtn->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.1);color:white;border-radius:10px;padding:6px 14px;font-weight:bold;border:none;}"
        "QPushButton:hover{background:rgba(255,255,255,0.18);}"
    );
    m_addRotationBtn->hide();
    connect(m_addRotationBtn, &QPushButton::clicked, this, &UserProfileDialog::onAddRotationGame);
    rHeader->addWidget(m_addRotationBtn);
    rcl->addLayout(rHeader);
    
    m_rotationGamesGrid = new QWidget();
    m_rotationGamesGrid->setStyleSheet("background:transparent; border:none;");
    QGridLayout* rGrid = new QGridLayout(m_rotationGamesGrid);
    rGrid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    rGrid->setSpacing(15);
    rGrid->setContentsMargins(0,10,0,10);
    m_rotationGamesLayout = rGrid;
    rcl->addWidget(m_rotationGamesGrid);
    
    scrollLayout->addWidget(rotCard);
    scrollLayout->addStretch();
    
    scroll->setWidget(scrollContent);
    main->addWidget(scroll);
    root->addWidget(container);
}

void UserProfileDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect(), 20, 20);
    p.fillPath(path, QColor(0,0,0,80));
    p.setPen(QPen(QColor(255,255,255,20),1));
    p.drawPath(path);
}

void UserProfileDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
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
            if (doc.isObject()) populateData(doc.object());
        }
    });
}

void UserProfileDialog::populateData(const QJsonObject& data) {
    // Avatar
    QString avUrl = data["avatar_url"].toString();
    if (!avUrl.isEmpty()) {
        QPixmap original;
        original.loadFromData(QByteArray::fromBase64(avUrl.toUtf8()));
        if (!original.isNull()) {
            QPixmap rounded(64,64);
            rounded.fill(Qt::transparent);
            QPainter p(&rounded);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addEllipse(0,0,64,64);
            p.setClipPath(path);
            p.drawPixmap(0,0,64,64, original.scaled(64,64,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation));
            m_avatarLabel->setPixmap(rounded);
        }
    }
    
    int level = data["level"].toInt(1);
    m_levelLabel->setText(QString("Level %1").arg(level));
    
    m_playedGames = data["played_games"].toArray();
    m_rotationGames = data["rotation_games"].toArray();
    
    rebuildGamesGrid(m_playedGamesLayout, m_playedGames, "played");
    rebuildGamesGrid(m_rotationGamesLayout, m_rotationGames, "rotation");
}

void UserProfileDialog::rebuildGamesGrid(QLayout* layout, const QJsonArray& games, const QString& category) {
    // Clear existing
    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    
    if (games.isEmpty()) {
        QLabel* empty = new QLabel(m_isOwnProfile ? "Click Edit to add games!" : "No games added yet");
        empty->setStyleSheet("color:rgba(255,255,255,0.3); font-size:13px; padding:20px; background:transparent; border:none;");
        layout->addWidget(empty);
        return;
    }
    
    QGridLayout* gridLayout = qobject_cast<QGridLayout*>(layout);
    if (!gridLayout) return;
    
    int colCount = 3; // Max 3 cards per row for 500px width
    int row = 0;
    int col = 0;
    
    for (const QJsonValue& val : games) {
        QJsonObject g = val.toObject();
        QString appId = g["appId"].toString();
        QString name = g["name"].toString();
        // Steam vertical library capsule
        QString thumbUrl = QString("https://steamcdn-a.akamaihd.net/steam/apps/%1/library_600x900.jpg").arg(appId);
        
        QWidget* tile = createGameTile(appId, name, thumbUrl, category);
        gridLayout->addWidget(tile, row, col);
        
        col++;
        if (col >= colCount) {
            col = 0;
            row++;
        }
    }
}

QWidget* UserProfileDialog::createGameTile(const QString& appId, const QString& name, const QString& thumbUrl, const QString& category) {
    QWidget* tile = new QWidget();
    tile->setFixedSize(110, 185); // Vertical card size
    tile->setStyleSheet("background:rgba(255,255,255,0.03); border-radius:12px; border:1px solid rgba(255,255,255,0.06);");
    tile->setToolTip(name);
    
    QVBoxLayout* tl = new QVBoxLayout(tile);
    tl->setContentsMargins(0,0,0,0);
    tl->setSpacing(0);
    
    // Thumbnail (vertical)
    QLabel* thumb = new QLabel();
    thumb->setFixedSize(110, 150);
    thumb->setStyleSheet("border-top-left-radius:12px; border-top-right-radius:12px; background:rgba(0,0,0,0.5); border:none;");
    thumb->setScaledContents(true);
    tl->addWidget(thumb);
    
    // Load thumbnail (with fallback)
    QNetworkReply* imgReply = m_netMgr->get(QNetworkRequest(QUrl(thumbUrl)));
    connect(imgReply, &QNetworkReply::finished, this, [this, thumb, imgReply, appId]() {
        imgReply->deleteLater();
        if (imgReply->error() == QNetworkReply::NoError) {
            QPixmap pix;
            pix.loadFromData(imgReply->readAll());
            if (!pix.isNull() && thumb) {
                thumb->setPixmap(pix.scaled(110,150,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation));
            }
        } else {
            // Fallback to horizontal header if vertical capsule is missing
            if (thumb) {
                QString fallbackUrl = QString("https://cdn.cloudflare.steamstatic.com/steam/apps/%1/header.jpg").arg(appId);
                QNetworkReply* fallbackReply = m_netMgr->get(QNetworkRequest(QUrl(fallbackUrl)));
                connect(fallbackReply, &QNetworkReply::finished, this, [thumb, fallbackReply]() {
                    fallbackReply->deleteLater();
                    if (fallbackReply->error() == QNetworkReply::NoError) {
                        QPixmap pix;
                        pix.loadFromData(fallbackReply->readAll());
                        if (!pix.isNull() && thumb) {
                            thumb->setPixmap(pix.scaled(110,150,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation));
                        }
                    }
                });
            }
        }
    });
    
    // Name label
    QLabel* nameLabel = new QLabel(name);
    nameLabel->setFixedHeight(35);
    nameLabel->setStyleSheet("color:rgba(255,255,255,0.8); font-size:11px; font-weight:bold; padding:0px 8px; background:transparent; border:none;");
    nameLabel->setWordWrap(true);
    nameLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    tl->addWidget(nameLabel);
    
    // Remove button (only in edit mode)
    if (m_isEditing && m_isOwnProfile) {
        QPushButton* removeBtn = new QPushButton("✕");
        removeBtn->setFixedSize(20,20);
        removeBtn->setStyleSheet(
            "QPushButton{background:rgba(231,76,60,0.8);color:white;border-radius:10px;font-size:10px;font-weight:bold;border:none;}"
            "QPushButton:hover{background:#E74C3C;}"
        );
        removeBtn->move(90, 5);
        removeBtn->setParent(tile);
        removeBtn->show();
        removeBtn->raise();
        connect(removeBtn, &QPushButton::clicked, this, [this, appId, category]() {
            removeGameFromList(appId, category);
        });
    }
    
    return tile;
}

void UserProfileDialog::removeGameFromList(const QString& appId, const QString& category) {
    QJsonArray& arr = (category == "played") ? m_playedGames : m_rotationGames;
    QJsonArray newArr;
    for (const QJsonValue& v : arr) {
        if (v.toObject()["appId"].toString() != appId) newArr.append(v);
    }
    arr = newArr;
    
    QLayout* layout = (category == "played") ? (QLayout*)m_playedGamesLayout : (QLayout*)m_rotationGamesLayout;
    rebuildGamesGrid(layout, arr, category);
}

void UserProfileDialog::onEditClicked() {
    m_isEditing = true;
    if (m_editBtn) m_editBtn->hide();
    if (m_saveBtn) m_saveBtn->show();
    if (m_cancelBtn) m_cancelBtn->show();
    if (m_addPlayedBtn) m_addPlayedBtn->show();
    if (m_addRotationBtn) m_addRotationBtn->show();
    
    rebuildGamesGrid(m_playedGamesLayout, m_playedGames, "played");
    rebuildGamesGrid(m_rotationGamesLayout, m_rotationGames, "rotation");
}

void UserProfileDialog::onSaveClicked() {
    saveProfile();
    m_isEditing = false;
    if (m_editBtn) m_editBtn->show();
    if (m_saveBtn) m_saveBtn->hide();
    if (m_cancelBtn) m_cancelBtn->hide();
    if (m_addPlayedBtn) m_addPlayedBtn->hide();
    if (m_addRotationBtn) m_addRotationBtn->hide();
    
    rebuildGamesGrid(m_playedGamesLayout, m_playedGames, "played");
    rebuildGamesGrid(m_rotationGamesLayout, m_rotationGames, "rotation");
}

void UserProfileDialog::onCancelEditClicked() {
    m_isEditing = false;
    if (m_editBtn) m_editBtn->show();
    if (m_saveBtn) m_saveBtn->hide();
    if (m_cancelBtn) m_cancelBtn->hide();
    if (m_addPlayedBtn) m_addPlayedBtn->hide();
    if (m_addRotationBtn) m_addRotationBtn->hide();
    
    // Re-fetch to discard changes
    fetchProfileData();
}

void UserProfileDialog::saveProfile() {
    QJsonObject payload;
    payload["played_games"] = m_playedGames;
    payload["rotation_games"] = m_rotationGames;
    
    QUrl url(Config::WEBSERVER_BASE_URL + "/api/user/profile");
    url.setQuery("username=" + m_myUsername);
    
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_netMgr->post(req, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [reply]() { reply->deleteLater(); });
}

void UserProfileDialog::onAddPlayedGame() {
    m_activeSearchCategory = "played";
    showSearchPopup("played");
}

void UserProfileDialog::onAddRotationGame() {
    m_activeSearchCategory = "rotation";
    showSearchPopup("rotation");
}

void UserProfileDialog::showSearchPopup(const QString& category) {
    if (m_searchOverlay) { m_searchOverlay->deleteLater(); m_searchOverlay = nullptr; }
    
    m_searchOverlay = new QWidget(this);
    m_searchOverlay->setGeometry(rect());
    m_searchOverlay->setStyleSheet("background: rgba(0,0,0,0.7);");
    m_searchOverlay->show();
    m_searchOverlay->raise();
    
    QVBoxLayout* ol = new QVBoxLayout(m_searchOverlay);
    ol->setAlignment(Qt::AlignCenter);
    
    QWidget* popup = new QWidget();
    popup->setFixedSize(450, 400);
    popup->setStyleSheet("background:rgba(20,25,35,250); border-radius:16px; border:1px solid rgba(255,255,255,0.1);");
    
    QVBoxLayout* pl = new QVBoxLayout(popup);
    pl->setContentsMargins(20,20,20,20);
    pl->setSpacing(12);
    
    QHBoxLayout* header = new QHBoxLayout();
    QString title = (category == "played") ? "Add to Games I Play" : "Add to Games in Rotation";
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("color:white; font-size:16px; font-weight:bold; background:transparent; border:none;");
    header->addWidget(titleLabel);
    header->addStretch();
    
    QPushButton* closePopup = new QPushButton("✕");
    closePopup->setFixedSize(28,28);
    closePopup->setCursor(Qt::PointingHandCursor);
    closePopup->setStyleSheet("background:rgba(255,255,255,0.1);color:white;border-radius:14px;font-weight:bold;border:none;");
    connect(closePopup, &QPushButton::clicked, this, &UserProfileDialog::hideSearchPopup);
    header->addWidget(closePopup);
    pl->addLayout(header);
    
    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("Search for a game...");
    m_searchInput->setStyleSheet(
        "QLineEdit{background:rgba(255,255,255,0.08);color:white;border:1px solid rgba(255,255,255,0.15);"
        "border-radius:10px;padding:10px 14px;font-size:14px;}"
        "QLineEdit:focus{border:1px solid #3B82F6;}"
    );
    connect(m_searchInput, &QLineEdit::textChanged, this, &UserProfileDialog::onSearchTextChanged);
    pl->addWidget(m_searchInput);
    
    m_searchResultsScroll = new QScrollArea();
    m_searchResultsScroll->setWidgetResizable(true);
    m_searchResultsScroll->setStyleSheet("QScrollArea{border:none;background:transparent;}"
        "QScrollBar:vertical{width:4px;background:transparent;}"
        "QScrollBar::handle:vertical{background:rgba(255,255,255,0.15);border-radius:2px;}");
    
    QWidget* resultsWidget = new QWidget();
    resultsWidget->setStyleSheet("background:transparent;");
    m_searchResultsLayout = new QVBoxLayout(resultsWidget);
    m_searchResultsLayout->setSpacing(6);
    m_searchResultsLayout->setAlignment(Qt::AlignTop);
    
    QLabel* hint = new QLabel("Type a game name to search...");
    hint->setStyleSheet("color:rgba(255,255,255,0.3);font-size:13px;padding:30px;background:transparent;border:none;");
    hint->setAlignment(Qt::AlignCenter);
    m_searchResultsLayout->addWidget(hint);
    
    m_searchResultsScroll->setWidget(resultsWidget);
    pl->addWidget(m_searchResultsScroll);
    
    ol->addWidget(popup);
    m_searchInput->setFocus();
}

void UserProfileDialog::hideSearchPopup() {
    if (m_searchOverlay) {
        m_searchOverlay->deleteLater();
        m_searchOverlay = nullptr;
    }
}

void UserProfileDialog::onSearchTextChanged(const QString& text) {
    Q_UNUSED(text);
    m_searchDebounce->start();
}

void UserProfileDialog::doSearch() {
    if (!m_searchInput || m_searchInput->text().trimmed().isEmpty()) return;
    
    QString query = m_searchInput->text().trimmed();
    
    // Use Steam store search API
    QString searchUrl = QString("https://store.steampowered.com/api/storesearch/?term=%1&l=english&cc=US").arg(query);
    
    QNetworkReply* reply = m_netMgr->get(QNetworkRequest(QUrl(searchUrl)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (!m_searchResultsLayout || reply->error() != QNetworkReply::NoError) return;
        
        // Clear old results
        while (m_searchResultsLayout->count() > 0) {
            QLayoutItem* item = m_searchResultsLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray items = doc.object()["items"].toArray();
        
        if (items.isEmpty()) {
            QLabel* noResults = new QLabel("No games found");
            noResults->setStyleSheet("color:rgba(255,255,255,0.3);font-size:13px;padding:20px;background:transparent;border:none;");
            noResults->setAlignment(Qt::AlignCenter);
            m_searchResultsLayout->addWidget(noResults);
            return;
        }
        
        for (int i = 0; i < qMin(items.size(), (qsizetype)10); i++) {
            QJsonObject item = items[i].toObject();
            QString appId = QString::number(item["id"].toInt());
            QString name = item["name"].toString();
            QString thumbUrl = QString("https://cdn.cloudflare.steamstatic.com/steam/apps/%1/header.jpg").arg(appId);
            
            // Check if already added
            bool alreadyAdded = false;
            QJsonArray& targetArr = (m_activeSearchCategory == "played") ? m_playedGames : m_rotationGames;
            for (const QJsonValue& v : targetArr) {
                if (v.toObject()["appId"].toString() == appId) { alreadyAdded = true; break; }
            }
            
            QPushButton* resultBtn = new QPushButton();
            resultBtn->setFixedHeight(50);
            resultBtn->setCursor(Qt::PointingHandCursor);
            resultBtn->setStyleSheet(
                alreadyAdded ?
                "QPushButton{background:rgba(34,197,94,0.15);border:1px solid rgba(34,197,94,0.3);border-radius:10px;text-align:left;padding-left:15px;color:rgba(255,255,255,0.5);font-size:13px;font-weight:bold;}"
                :
                "QPushButton{background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.08);border-radius:10px;text-align:left;padding-left:15px;color:white;font-size:13px;font-weight:bold;}"
                "QPushButton:hover{background:rgba(59,130,246,0.2);border:1px solid rgba(59,130,246,0.4);}"
            );
            resultBtn->setText(alreadyAdded ? name + " ✓" : name);
            resultBtn->setEnabled(!alreadyAdded);
            
            connect(resultBtn, &QPushButton::clicked, this, [this, appId, name]() {
                QJsonObject gameObj;
                gameObj["appId"] = appId;
                gameObj["name"] = name;
                
                QJsonArray& arr = (m_activeSearchCategory == "played") ? m_playedGames : m_rotationGames;
                
                // Check limits
                if (m_activeSearchCategory == "played" && arr.size() >= 20) return;
                if (m_activeSearchCategory == "rotation" && arr.size() >= 5) return;
                
                arr.append(gameObj);
                
                QLayout* layout = (m_activeSearchCategory == "played") ? (QLayout*)m_playedGamesLayout : (QLayout*)m_rotationGamesLayout;
                rebuildGamesGrid(layout, arr, m_activeSearchCategory);
                
                hideSearchPopup();
            });
            
            m_searchResultsLayout->addWidget(resultBtn);
        }
    });
}
