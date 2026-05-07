#ifndef STEAMPATCHWORKER_H
#define STEAMPATCHWORKER_H

#include <QThread>
#include <QString>

class SteamPatchWorker : public QThread {
    Q_OBJECT

public:
    explicit SteamPatchWorker(QObject* parent = nullptr);

signals:
    void log(QString message, QString level = "INFO");
    void finished(QString message);
    void error(QString errorMessage);

protected:
    void run() override;
};

#endif // STEAMPATCHWORKER_H
