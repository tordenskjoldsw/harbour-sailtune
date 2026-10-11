#ifndef SERVERSETUP_H
#define SERVERSETUP_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>

class Sources;

// Adding a Jellyfin server, or logging in to one again: the address is
// checked, then the user logs in with a password or with Quick Connect.
// Only the token is kept; the password goes to the server and is dropped.
class ServerSetup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Sources *sources READ sources WRITE setSources NOTIFY sourcesChanged)
    // An existing source to log in to again; empty to add a new one.
    Q_PROPERTY(QString sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
    Q_PROPERTY(Step step READ step NOTIFY stepChanged)
    Q_PROPERTY(Problem problem READ problem NOTIFY problemChanged)
    Q_PROPERTY(QString serverName READ serverName NOTIFY serverChanged)
    Q_PROPERTY(QString address READ address NOTIFY serverChanged)
    Q_PROPERTY(bool quickConnect READ quickConnect NOTIFY serverChanged)
    Q_PROPERTY(QString quickConnectCode READ quickConnectCode NOTIFY quickConnectCodeChanged)

public:
    enum Step {
        EnterAddress,
        Checking,
        EnterLogin,
        LoggingIn,
        WaitingForQuickConnect,
        Done
    };
    Q_ENUM(Step)

    enum Problem {
        NoProblem,
        InvalidAddress,
        Unreachable,
        NotJellyfin,
        WrongLogin,
        QuickConnectFailed,
        ServerError
    };
    Q_ENUM(Problem)

    explicit ServerSetup(QObject *parent = nullptr);
    ~ServerSetup() override;

    Sources *sources() const;
    void setSources(Sources *sources);
    QString sourceId() const;
    void setSourceId(const QString &sourceId);
    Step step() const;
    Problem problem() const;
    QString serverName() const;
    QString address() const;
    bool quickConnect() const;
    QString quickConnectCode() const;

    Q_INVOKABLE void check(const QString &typedAddress);
    Q_INVOKABLE void logIn(const QString &userName, const QString &password);
    Q_INVOKABLE void startQuickConnect();
    // Stops waiting for Quick Connect and goes back to the login.
    Q_INVOKABLE void cancelQuickConnect();

signals:
    void sourcesChanged();
    void sourceIdChanged();
    void stepChanged();
    void problemChanged();
    void serverChanged();
    void quickConnectCodeChanged();
    // The source is added or logged in again and active.
    void finished();

private:
    void setStep(Step step);
    void setProblem(Problem problem);
    void pollQuickConnect();
    // Asks the server who the token belongs to, then stores the source.
    void complete(const QString &token, const QString &userId, const QString &userName);

    Sources *m_sources = nullptr;
    QString m_sourceId;
    Step m_step = EnterAddress;
    Problem m_problem = NoProblem;
    QString m_address;
    QString m_serverName;
    bool m_quickConnect = false;
    QString m_deviceId;
    QString m_quickConnectSecret;
    QString m_quickConnectCode;
    int m_quickConnectPolls = 0;
    QTimer m_pollTimer;
    QNetworkAccessManager m_network;
};

#endif // SERVERSETUP_H
