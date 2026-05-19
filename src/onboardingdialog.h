#ifndef ONBOARDINGDIALOG_H
#define ONBOARDINGDIALOG_H

#include <QDialog>
#include <QEvent>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>

class OnboardingDialog : public QDialog {
  Q_OBJECT
public:
  explicit OnboardingDialog(QWidget *parent = nullptr);
  QString username() const;
  QJsonObject userData() const;
  bool isGuest() const;

protected:
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onUsernameChanged(const QString &text);
  void checkAvailability();
  void onCheckFinished(QNetworkReply *reply);
  void onPrimaryClicked();
  void switchToLogin();
  void switchToRegister();
  void onGuestClicked();

private:
  enum Mode { LOGIN, REGISTER };
  int m_currentMode;

  // UI
  QWidget *m_rightPanel;
  QLabel *m_tabSignIn;
  QLabel *m_tabSignUp;

  QLineEdit *m_usernameInput;
  QLineEdit *m_passwordInput;
  QLabel *m_statusLabel;
  QPushButton *m_continueBtn;
  QPushButton *m_guestBtn;

  // Image
  QPixmap m_bgImage;

  // Network
  QTimer *m_debounceTimer;
  QNetworkAccessManager *m_networkManager;
  QString m_username;
  QJsonObject m_userData;
  bool m_isAvailable;
  bool m_isChecking;
  bool m_isGuest;
};

#endif // ONBOARDINGDIALOG_H
