#include "serversetup.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>

#include "jellyfin.h"
#include "jellyfinsource.h"
#include "sources.h"

namespace {

const int QuickConnectPollMs = 3000;
// Quick Connect codes expire on the server after a few minutes.
const int MaxQuickConnectPolls = 100;

QString newDeviceId()
{
    // Qt 5.6 has no QUuid::WithoutBraces.
    return QUuid::createUuid().toString().remove(QLatin1Char('{')).remove(QLatin1Char('}'));
}

} // namespace

ServerSetup::ServerSetup(QObject *parent)
    : QObject(parent)
    , m_deviceId(newDeviceId())
{
    m_pollTimer.setInterval(QuickConnectPollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &ServerSetup::pollQuickConnect);
}

ServerSetup::~ServerSetup() = default;

Sources *ServerSetup::sources() const
{
    return m_sources;
}

void ServerSetup::setSources(Sources *sources)
{
    if (m_sources == sources)
        return;
    m_sources = sources;
    emit sourcesChanged();
}

QString ServerSetup::sourceId() const
{
    return m_sourceId;
}

void ServerSetup::setSourceId(const QString &sourceId)
{
    if (m_sourceId == sourceId)
        return;
    m_sourceId = sourceId;
    // Logging in again keeps the source's device id, so the server sees
    // the same device and not a new one.
    const JellyfinSource *source = m_sources ? m_sources->jellyfin(sourceId) : nullptr;
    if (source) {
        m_deviceId = source->config().deviceId;
        m_address = source->address();
        m_serverName = source->name();
        emit serverChanged();
    }
    emit sourceIdChanged();
}

ServerSetup::Step ServerSetup::step() const
{
    return m_step;
}

ServerSetup::Problem ServerSetup::problem() const
{
    return m_problem;
}

QString ServerSetup::serverName() const
{
    return m_serverName;
}

QString ServerSetup::address() const
{
    return m_address;
}

bool ServerSetup::quickConnect() const
{
    return m_quickConnect;
}

QString ServerSetup::quickConnectCode() const
{
    return m_quickConnectCode;
}

void ServerSetup::setStep(Step step)
{
    if (m_step == step)
        return;
    m_step = step;
    emit stepChanged();
}

void ServerSetup::setProblem(Problem problem)
{
    if (m_problem == problem)
        return;
    m_problem = problem;
    emit problemChanged();
}

void ServerSetup::check(const QString &typedAddress)
{
    const QString address = Jellyfin::normalizedAddress(typedAddress);
    if (address.isEmpty()) {
        setProblem(InvalidAddress);
        return;
    }
    setProblem(NoProblem);
    setStep(Checking);
    const Jellyfin::Identity identity{m_deviceId, QString()};
    Jellyfin::request(&m_network, QStringLiteral("GET"),
                      Jellyfin::url(address, QStringLiteral("/System/Info/Public")), identity,
                      QByteArray(), [this, address, identity](Jellyfin::Failure failure, const QJsonObject &info) {
        const QString product = info.value(QStringLiteral("ProductName")).toString();
        const QString name = info.value(QStringLiteral("ServerName")).toString().left(200);
        if (failure == Jellyfin::Failure::Network) {
            setProblem(Unreachable);
            setStep(EnterAddress);
            return;
        }
        if (failure != Jellyfin::Failure::None || !product.contains(QStringLiteral("Jellyfin"))) {
            setProblem(NotJellyfin);
            setStep(EnterAddress);
            return;
        }
        m_address = address;
        m_serverName = name.isEmpty() ? QUrl(address).host() : name;
        // Whether Quick Connect is on decides if the login offers it.
        QNetworkRequest request(Jellyfin::url(address, QStringLiteral("/QuickConnect/Enabled")));
        request.setRawHeader("Authorization", Jellyfin::authorization(identity));
        QNetworkReply *reply = m_network.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            // The answer is a bare "true" or "false", not a JSON object.
            m_quickConnect = reply->error() == QNetworkReply::NoError
                && reply->read(16).trimmed() == "true";
            emit serverChanged();
            setStep(EnterLogin);
        });
    });
}

void ServerSetup::logIn(const QString &userName, const QString &password)
{
    if (m_address.isEmpty() || userName.trimmed().isEmpty())
        return;
    setProblem(NoProblem);
    setStep(LoggingIn);
    QJsonObject body;
    body.insert(QStringLiteral("Username"), userName.trimmed());
    body.insert(QStringLiteral("Pw"), password);
    Jellyfin::request(&m_network, QStringLiteral("POST"),
                      Jellyfin::url(m_address, QStringLiteral("/Users/AuthenticateByName")),
                      {m_deviceId, QString()}, QJsonDocument(body).toJson(QJsonDocument::Compact),
                      [this](Jellyfin::Failure failure, const QJsonObject &answer) {
        const QString token = answer.value(QStringLiteral("AccessToken")).toString();
        const QJsonObject user = answer.value(QStringLiteral("User")).toObject();
        if (failure == Jellyfin::Failure::Unauthorized) {
            setProblem(WrongLogin);
            setStep(EnterLogin);
            return;
        }
        if (failure != Jellyfin::Failure::None || token.isEmpty()) {
            setProblem(failure == Jellyfin::Failure::Network ? Unreachable : ServerError);
            setStep(EnterLogin);
            return;
        }
        complete(token, user.value(QStringLiteral("Id")).toString(),
                 user.value(QStringLiteral("Name")).toString());
    });
}

void ServerSetup::startQuickConnect()
{
    if (m_address.isEmpty())
        return;
    setProblem(NoProblem);
    setStep(WaitingForQuickConnect);
    Jellyfin::request(&m_network, QStringLiteral("POST"),
                      Jellyfin::url(m_address, QStringLiteral("/QuickConnect/Initiate")),
                      {m_deviceId, QString()}, QByteArray(),
                      [this](Jellyfin::Failure failure, const QJsonObject &answer) {
        m_quickConnectSecret = answer.value(QStringLiteral("Secret")).toString();
        m_quickConnectCode = answer.value(QStringLiteral("Code")).toString().left(16);
        if (failure != Jellyfin::Failure::None || m_quickConnectSecret.isEmpty()) {
            setProblem(QuickConnectFailed);
            setStep(EnterLogin);
            return;
        }
        emit quickConnectCodeChanged();
        m_quickConnectPolls = 0;
        m_pollTimer.start();
    });
}

void ServerSetup::cancelQuickConnect()
{
    m_pollTimer.stop();
    m_quickConnectSecret.clear();
    m_quickConnectCode.clear();
    emit quickConnectCodeChanged();
    setStep(EnterLogin);
}

void ServerSetup::pollQuickConnect()
{
    if (++m_quickConnectPolls > MaxQuickConnectPolls) {
        cancelQuickConnect();
        setProblem(QuickConnectFailed);
        return;
    }
    const QString secret = m_quickConnectSecret;
    Jellyfin::request(&m_network, QStringLiteral("GET"),
                      Jellyfin::url(m_address, QStringLiteral("/QuickConnect/Connect"),
                                    {{QStringLiteral("secret"), secret}}),
                      {m_deviceId, QString()}, QByteArray(),
                      [this, secret](Jellyfin::Failure failure, const QJsonObject &answer) {
        if (secret != m_quickConnectSecret || m_step != WaitingForQuickConnect)
            return;
        if (failure != Jellyfin::Failure::None) {
            cancelQuickConnect();
            setProblem(QuickConnectFailed);
            return;
        }
        if (!answer.value(QStringLiteral("Authenticated")).toBool())
            return;
        m_pollTimer.stop();
        QJsonObject body;
        body.insert(QStringLiteral("Secret"), secret);
        Jellyfin::request(&m_network, QStringLiteral("POST"),
                          Jellyfin::url(m_address, QStringLiteral("/Users/AuthenticateWithQuickConnect")),
                          {m_deviceId, QString()}, QJsonDocument(body).toJson(QJsonDocument::Compact),
                          [this](Jellyfin::Failure failure, const QJsonObject &result) {
            const QString token = result.value(QStringLiteral("AccessToken")).toString();
            const QJsonObject user = result.value(QStringLiteral("User")).toObject();
            if (failure != Jellyfin::Failure::None || token.isEmpty()) {
                cancelQuickConnect();
                setProblem(QuickConnectFailed);
                return;
            }
            m_quickConnectSecret.clear();
            complete(token, user.value(QStringLiteral("Id")).toString(),
                     user.value(QStringLiteral("Name")).toString());
        });
    });
}

void ServerSetup::complete(const QString &token, const QString &userId, const QString &userName)
{
    if (!m_sources || userId.isEmpty()) {
        setProblem(ServerError);
        setStep(EnterLogin);
        return;
    }
    JellyfinSource::Config config;
    config.id = m_sourceId;
    config.serverName = m_serverName;
    config.address = m_address;
    config.userName = userName.left(200);
    config.userId = userId;
    config.deviceId = m_deviceId;
    m_sources->storeJellyfin(config, token);
    setStep(Done);
    emit finished();
}
