#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QAbstractSocket>

class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);
    ~TcpClient();

    // 连接断开
    bool connectTcp(const QString& ip, quint16 port);
    void disconnectTcp();
    bool isConnected() const;

    // 发原始字节流
    void sendRawData(const QByteArray& data);

signals:
    void sigRecvRawData(const QByteArray&);
    void sigConnected();
    void sigDisconnected();
    void sigError(const QString& errMsg);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);

private:
    QTcpSocket* m_tcpSocket = nullptr;
};

#endif // TCPCLIENT_H
