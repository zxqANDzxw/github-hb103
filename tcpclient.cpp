#include "tcpclient.h"

TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
{
    m_tcpSocket = new QTcpSocket(this);

    connect(m_tcpSocket, &QTcpSocket::readyRead,
            this, &TcpClient::onReadyRead);
    connect(m_tcpSocket, &QTcpSocket::connected,
            this, &TcpClient::sigConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected,
            this, &TcpClient::sigDisconnected);
    connect(m_tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));
}

TcpClient::~TcpClient()
{
    disconnectTcp();
}

bool TcpClient::connectTcp(const QString &ip, quint16 port)
{
    if(m_tcpSocket->state() == QAbstractSocket::ConnectedState)
    {
        return true;
    }
    m_tcpSocket->connectToHost(ip, port);
    return m_tcpSocket->waitForConnected(3000);
}

void TcpClient::disconnectTcp()
{
    if(m_tcpSocket)
    {
        m_tcpSocket->disconnectFromHost();
    }
}

bool TcpClient::isConnected() const
{
    return m_tcpSocket->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::sendRawData(const QByteArray &data)
{
    if(!isConnected()) return;
    m_tcpSocket->write(data);
}

void TcpClient::onReadyRead()
{
    QByteArray bytes = m_tcpSocket->readAll();
    emit sigRecvRawData(bytes);
}

void TcpClient::onSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err)
    emit sigError(m_tcpSocket->errorString());
}
