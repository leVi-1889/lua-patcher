#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QElapsedTimer>
#include "workers/steampatchworker.h"

class LoadingDialog : public QDialog {
    Q_OBJECT
public:
    LoadingDialog(const QString& username, const QString& password, bool isRegister, bool isGuest, QWidget* parent = nullptr);
    ~LoadingDialog();

    bool isSuccess() const { return m_success; }
    QString errorMsg() const { return m_errorMsg; }
    QString username() const { return m_verifiedUsername; }
    QJsonObject userData() const { return m_userData; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override; // Disable escape closing

private slots:
    void onUpdate();
    void onAuthFinished();
    void onPatchLog(const QString& msg, const QString& level);
    void onPatchFinished(const QString& msg);
    void onPatchError(const QString& err);

private:
    void startAuth();
    void startPatching();

    // Inputs
    QString m_inputUsername;
    QString m_inputPassword;
    bool m_isRegister;
    bool m_isGuest;

    // Results
    bool m_success;
    QString m_errorMsg;
    QString m_verifiedUsername;
    QJsonObject m_userData;

    // Animation & State
    float m_progress;
    float m_time;
    int m_stage; // 0: verifying, 1: patching
    float m_textOpacity;
    QString m_statusText;

    QTimer* m_animationTimer;
    QElapsedTimer m_elapsedTimer;

    // Async Operations
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_authReply;
    bool m_authCompleted;
    bool m_authSuccess;

    SteamPatchWorker* m_patchWorker;
    bool m_patchCompleted;
    bool m_patchSuccess;
};

#endif // LOADINGDIALOG_H
