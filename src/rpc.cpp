#include "rpc.h"
#include "qjsondocument.h"
#include "qlocalsocket.h"
#include "src/mainwindow.h"
#include "src/settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

#include <iostream>

Rpc::Rpc(MainWindow *mw) {
  rpcInstance = this;
  this->mw = mw;
  this->rpcSock = new QLocalServer(); //
  this->listen();
}

Rpc::~Rpc() {
  this->rpcSock->close();
  QLocalServer::removeServer("cubiomes-rpc");
  std::cout << "closing rpc...\n";
}

void Rpc::listen() {
  std::cout << "asdasd\n";
  QObject::connect(this->rpcSock, &QLocalServer::newConnection, this,
                   &Rpc::onConnection);
  std::cout << "xddd\n";
  if (!this->rpcSock->listen("cubiomes-rpc")) {
    std::cout << "failed to listen to rpc: "
              << this->rpcSock->errorString().toStdString() << "\n";
  }
}
void Rpc::cleanup(QLocalSocket *c) {
  QObject::disconnect(c, &QLocalSocket::readyRead, nullptr, nullptr);
  c->flush();
  c->close();
  c->deleteLater();
}

void Rpc::onData(QLocalSocket *c) {
  std::cout << "no commands...\n";
  /*
  if (c->error() == QLocalSocket::LocalSocketError::PeerClosedError) {
    std::cout << "connection closed\n";
    this->cleanup(c);
    return;
  }
  if (c->error() != QLocalSocket::LocalSocketError::SocketTimeoutError) {
    std::cout << "error " << c->errorString().toStdString() << "\n";
    this->cleanup(c);
    return;
  }
  */
  // auto line = c->readLine();
  // std::cout << "Read line " << line.toStdString() << "\n";
  // auto len = line.toInt(&ok);
  // if (!ok) {
  //   std::cout << "bad line\n";
  //   this->badCommand(c, "bad line length");
  //   this->cleanup(c);
  //   return;
  // }
  // int leftBytes = len;
  // QByteArray data;
  // while (leftBytes) {
  //   auto thisTime = c->read(leftBytes);
  //   data.append(thisTime);
  //   leftBytes -= thisTime.size();
  //   if (!leftBytes)
  //     break;
  //   if (!c->waitForReadyRead()) {
  //     std::cout << "error while waiting for command data, " << leftBytes
  //               << "bytes left, xdxd:\n";
  //     std::cout << data.toStdString() << "\n";
  //     std::cout << "----\n";
  //     std::cout << "error state" << c->errorString().toStdString() << "\n";
  //     this->cleanup(c);
  //     return;
  //   }
  // }
  auto data = c->readLine();
  data.chop(1);
  qDebug() << data;

  QJsonParseError err;
  auto document = QJsonDocument::fromJson(data, &err);
  if (document.isNull() || !document.isObject()) {
    std::cout << err.errorString().toStdString() << "\n";
    this->badCommand(c, "failed to parse JSON");
    this->cleanup(c);
  }
  auto doc = document.object();
  auto cmd = doc["cmd"].toString();
  if (cmd.isNull()) {
    this->badCommand(c, "failed to interpret JSON");
    this->cleanup(c);
  }

  this->handleCommand(c, cmd, doc);
}
void Rpc::onError(QLocalSocket *c, QLocalSocket::LocalSocketError err) {
  std::cout << "error while reading from socket "
            << c->errorString().toStdString();
  this->cleanup(c);
}

void Rpc::onConnection() {
  auto c = this->rpcSock->nextPendingConnection();
  std::cout << "benis!\n";
  QObject::connect(c, &QLocalSocket::readyRead,
                   [this, c]() { this->onData(c); });
  QObject::connect(
      c, &QLocalSocket::errorOccurred,
      [this, c](QLocalSocket::LocalSocketError err) { this->onError(c, err); });
}

void Rpc::handleCommand(QLocalSocket *c, QString cmd, QJsonObject doc) {
  std::cout << "handling command " << cmd.toStdString() << "!\n";

  if (cmd == "goto") {
    qreal x = doc["x"].toDouble();
    qreal z = doc["z"].toDouble();
    qreal scale = doc["scale"].toDouble(this->mw->getMapView()->getScale());

    this->mw->mapGoto(x, z, scale);
  } else if (cmd == "dimension") {
    int dimid = doc["id"].toInt();
    if (dimid != 0 && dimid != 1 && dimid != -1) {
      this->badCommand(c, "dimension doesn't match enum");
      return;
    }
    WorldInfo wi;
    this->mw->getSeed(&wi);
    this->mw->setSeed(wi, dimid);
  } else if (cmd == "seed") {
    bool ok{0};
    auto seedStr = doc["value"].toString();
    long seed = seedStr.toLong(&ok);
    if (!ok) {
      qDebug() << doc;
      this->badCommand(c, QString("cannot parse seed: %1").arg(seedStr));
      return;
    }
    WorldInfo wi;
    this->mw->getSeed(&wi);
    wi.seed = seed;
    std::cout << "setting seed to " << wi.seed << "\n";
    this->mw->setSeed(wi);
  }
}
void Rpc::badCommand(QLocalSocket *c, QString why) {
  QJsonDocument out;
  QJsonObject o;
  o["cmd"] = QString("error");
  o["message"] = why;

  out.setObject(o);
  auto json = out.toJson(QJsonDocument::JsonFormat::Compact);
  // c->write(QByteArray::number(json.length()) + "\n");
  c->write(json + "\n");
  std::cout << "bad command: " << why.toStdString() << "\n";
}
