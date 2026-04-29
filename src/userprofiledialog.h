#ifndef USERPROFILEDIALOG_H
#define USERPROFILEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QScrollArea>
#include <QTimer>

class UserProfileDialog : public QDialog {
    Q_OBJECT
public:
    explicit UserProfileDialog(const QString& targetUsername, const QString& myUsername, 
                               QNetworkAccessManager* netMgr, QWidget* parent = nullptr);
    ~UserProfileDialog() override = default;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onEditClicked();
    void onSaveClicked();
    void onCancelEditClicked();
    void onAddPlayedGame();
    void onAddRotationGame();
    void onSearchTextChanged(const QString& text);
    void doSearch();

private:
    void setupUI();
    void fetchProfileData();
    void populateData(const QJsonObject& data);
    void rebuildGamesGrid(QLayout* layout, const QJsonArray& games, const QString& category);
    QWidget* createGameTile(const QString& appId, const QString& name, const QString& thumbUrl, const QString& category);
    void removeGameFromList(const QString& appId, const QString& category);
    void showSearchPopup(const QString& category);
    void hideSearchPopup();
    void saveProfile();
    
    // UI Elements
    QLabel* m_avatarLabel = nullptr;
    QLabel* m_usernameLabel = nullptr;
    QLabel* m_levelLabel = nullptr;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    
    // Games I Play section
    QWidget* m_playedGamesGrid = nullptr;
    QGridLayout* m_playedGamesLayout = nullptr;
    QPushButton* m_addPlayedBtn = nullptr;
    
    // Games in Rotation section
    QWidget* m_rotationGamesGrid = nullptr;
    QGridLayout* m_rotationGamesLayout = nullptr;
    QPushButton* m_addRotationBtn = nullptr;
    
    // Search popup
    QWidget* m_searchOverlay = nullptr;
    QLineEdit* m_searchInput = nullptr;
    QVBoxLayout* m_searchResultsLayout = nullptr;
    QScrollArea* m_searchResultsScroll = nullptr;
    QString m_activeSearchCategory; // "played" or "rotation"
    QTimer* m_searchDebounce = nullptr;

    // Data
    QString m_targetUsername;
    QString m_myUsername;
    QNetworkAccessManager* m_netMgr;
    bool m_isOwnProfile;
    bool m_isEditing = false;
    
    QJsonArray m_playedGames;   // [{appId, name, thumbUrl}, ...]
    QJsonArray m_rotationGames;
};

#endif // USERPROFILEDIALOG_H
