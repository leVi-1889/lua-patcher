#ifndef USERPROFILEDIALOG_H
#define USERPROFILEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QScrollArea>

class UserProfileDialog : public QDialog {
    Q_OBJECT
public:
    explicit UserProfileDialog(const QString& targetUsername, const QString& myUsername, QNetworkAccessManager* netMgr, QWidget* parent = nullptr);
    ~UserProfileDialog() override = default;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUI();
    void fetchProfileData();
    void populateData(const QJsonObject& data);
    void setupGamesGrid(QLayout* targetLayout, const QJsonArray& games, bool isRotation);
    
    // UI Elements
    QLabel* m_avatarLabel;
    QLabel* m_usernameLabel;
    QPushButton* m_editBtn;
    
    QWidget* m_playedGamesContainer;
    QLayout* m_playedGamesLayout;
    QPushButton* m_addPlayedGameBtn;
    
    QWidget* m_rotationGamesContainer;
    QLayout* m_rotationGamesLayout;
    QPushButton* m_addRotationGameBtn;

    // Data
    QString m_targetUsername;
    QString m_myUsername;
    QNetworkAccessManager* m_netMgr;
    bool m_isOwnProfile;
};

#endif // USERPROFILEDIALOG_H
