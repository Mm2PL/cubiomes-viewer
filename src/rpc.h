#pragma once

#include <QLocalSocket>
#include <QObject>

class QLocalServer;
class QString;
class QJsonObject;
class MainWindow;

class Rpc : public QObject {
public:
  Rpc(MainWindow *);
  ~Rpc();

private:
  QLocalServer *rpcSock;
  MainWindow *mw;
  void listen();
  void onConnection();
  void onData(QLocalSocket *);
  void onError(QLocalSocket *c, QLocalSocket::LocalSocketError err);
  void handleCommand(QLocalSocket *c, QString cmd, QJsonObject doc);
  void cleanup(QLocalSocket *);
  void badCommand(QLocalSocket *c, QString why);
};
static Rpc *rpcInstance;
