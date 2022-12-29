#include "rpc.h"

#include "qimage.h"
#include "qjsondocument.h"
#include "qlocalsocket.h"
#include "qnamespace.h"
#include "qpixmap.h"
#include "src/mainwindow.h"
#include "src/settings.h"
#include "src/world.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

#include <iostream>

Level *Rpc::getLevel()
{
    return &this->mw->getMapView()->world->lvs.at(D_REMOTE);
}

const QPixmap *Rpc::maybeGetPixmapForImageId(int imgId) const
{
    for (const auto &img : this->images)
    {
        if (img.id == imgId)
        {
            return &img.img;
        }
    }
    return nullptr;
}

const QPixmap &Rpc::getPixmapForRemoteMarker(VarPos *vp) const
{
    //qDebug() << "asdasd" << vp->p.x << vp->p.z << vp->type;
    static QPixmap DEFAULT_ICON = QPixmap(":/icons/origin.png");
    if (vp == nullptr)
        return DEFAULT_ICON;
    for (const auto &img : this->images)
    {
        if (img.id == vp->type)
        {
            //qDebug() << "found image id " << vp->type << img.img;
            return img.img;
        }
    }
    qDebug() << "image " << vp->type << " not found; have:";
    for (const auto &img : this->images)
        qDebug() << " - " << img.id << img.img;
    return DEFAULT_ICON;
}

void Rpc::listen()
{
    //std::cout << "asdasd\n";
    QObject::connect(this->rpcSock, &QLocalServer::newConnection, this, &Rpc::onConnection);
    std::cout << "xddd\n";
    if (!this->rpcSock->listen("cubiomes-rpc"))
    {
        std::cout << "failed to listen to rpc: " << this->rpcSock->errorString().toStdString() << "\n";
    }
}
void Rpc::cleanup(QLocalSocket *c)
{
    QObject::disconnect(c, nullptr, nullptr, nullptr);
    c->flush();
    c->close();
    c->deleteLater();
}

void Rpc::onData(QLocalSocket *c)
{
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
    while (1)
    {
        auto data = c->readLine();
        if (data.isEmpty())
            break;
        data.chop(1);
        qDebug() << data;

        QJsonParseError err;
        auto document = QJsonDocument::fromJson(data, &err);
        if (document.isNull() || !document.isObject())
        {
            std::cout << err.errorString().toStdString() << "\n";
            this->badCommand(c, "failed to parse JSON");
            this->cleanup(c);
        }
        auto doc = document.object();
        auto cmd = doc["cmd"].toString();
        if (cmd.isNull())
        {
            this->badCommand(c, "failed to interpret JSON");
            this->cleanup(c);
        }

        this->handleCommand(c, cmd, doc);
    }
}
void Rpc::onError(QLocalSocket *c, QLocalSocket::LocalSocketError err)
{
    std::cout << "error while reading from socket " << c->errorString().toStdString();
    this->cleanup(c);
}
void Rpc::chatCommand(QString text)
{
    QJsonDocument out;
    QJsonObject o;
    o["cmd"] = QString("chatcmd");
    o["text"] = text;

    out.setObject(o);
    emit this->writeCommand(out);
}

void Rpc::onConnection()
{
    auto c = this->rpcSock->nextPendingConnection();
    std::cout << "benis!\n";
    QObject::connect(c, &QLocalSocket::readyRead, [this, c]() {
        this->onData(c);
    });
    QObject::connect(c, &QLocalSocket::errorOccurred, [this, c](QLocalSocket::LocalSocketError err) {
        this->onError(c, err);
    });
    QObject::connect(this, &Rpc::writeCommand, [c](QJsonDocument doc) {
        qDebug() << "writing command to peer " << doc;
        auto j = doc.toJson(QJsonDocument::JsonFormat::Compact);
        c->write(j + "\n");
    });

    {
        QJsonDocument out;
        QJsonObject o;
        o["cmd"] = QString("newimg");
        o["id"] = QString::number(this->lastImageId - 1);

        out.setObject(o);
        auto json = out.toJson(QJsonDocument::JsonFormat::Compact);
        // c->write(QByteArray::number(json.length()) + "\n");
        c->write(json + "\n");
    }
    {
        QJsonDocument out;
        QJsonObject o;
        o["cmd"] = QString("newmarker");
        o["id"] = QString::number(this->lastMarkerId - 1);

        out.setObject(o);
        auto json = out.toJson(QJsonDocument::JsonFormat::Compact);
        // c->write(QByteArray::number(json.length()) + "\n");
        c->write(json + "\n");
    }
    c->flush();
}

void Rpc::handleCommand(QLocalSocket *c, QString cmd, QJsonObject doc)
{
    std::cout << "handling command " << cmd.toStdString() << "!\n";

    if (cmd == "goto")
    {
        qreal x = doc["x"].toDouble();
        qreal z = doc["z"].toDouble();
        qreal scale = doc["scale"].toDouble(this->mw->getMapView()->getScale());

        this->mw->mapGoto(x, z, scale);
    }
    else if (cmd == "dimension")
    {
        int dimid = doc["id"].toInt();
        if (dimid != 0 && dimid != 1 && dimid != -1)
        {
            this->badCommand(c, "dimension doesn't match enum");
            return;
        }
        WorldInfo wi;
        this->mw->getSeed(&wi);
        this->mw->setSeed(wi, dimid);
        this->rebuildMarkers();
    }
    else if (cmd == "seed")
    {
        bool ok{0};
        auto seedStr = doc["value"].toString();
        long seed = seedStr.toLong(&ok);
        if (!ok)
        {
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
    else if (cmd == "icon")
    {
        auto len = doc["len"].toInt();
        if (len == 0)
        {
            this->badCommand(c, "fatal: bad length for icon command");
            this->cleanup(c);
            return;
            // not sure if the remote wants to send data now, better ignore this shit
        }
        int leftBytes = len;
        QByteArray data;
        while (leftBytes)
        {
            auto thisTime = c->read(leftBytes);
            data.append(thisTime);
            leftBytes -= thisTime.size();
            if (!leftBytes)
                break;
            if (!c->waitForReadyRead())
            {
                std::cout << "error while waiting for command data, " << leftBytes << "bytes left, xdxd:\n";
                std::cout << data.toStdString() << "\n";
                std::cout << "----\n";
                std::cout << "error state" << c->errorString().toStdString() << "\n";
                this->cleanup(c);
                return;
            }
        }
        auto id = this->lastImageId;
        this->lastImageId += 1;
        auto qi = QPixmap::fromImage(QImage::fromData(data));
        qDebug() << "AAAAAAAAAAA" << qi;
        this->images.emplace_back(qi, id);
        QJsonDocument out;
        QJsonObject o;
        o["cmd"] = QString("newimg");
        o["id"] = QString::number(id);

        out.setObject(o);
        auto json = out.toJson(QJsonDocument::JsonFormat::Compact);
        // c->write(QByteArray::number(json.length()) + "\n");
        c->write(json + "\n");
    }
    else if (cmd == "markernew")
    {
        auto id = this->lastMarkerId;
        this->lastMarkerId += 1;
        auto iconId = doc["icon"].toInt(-1);
        auto pic = this->maybeGetPixmapForImageId(iconId);
        if (!pic)
        {
            this->badCommand(c,
                             QString("icon id (%1) does not exist, max id is %2").arg(iconId).arg(this->lastImageId));
            return;
        }
        int x = doc["x"].toInt();
        int z = doc["z"].toInt();
        int dim = doc["dim"].toInt();
        this->markers.push_back({iconId, id, {{x, z}, 0}, dim});
        qDebug() << "new marker id" << id;

        QJsonDocument out;
        QJsonObject o;
        o["cmd"] = QString("newmarker");
        o["id"] = QString::number(id);

        out.setObject(o);
        auto json = out.toJson(QJsonDocument::JsonFormat::Compact);
        // c->write(QByteArray::number(json.length()) + "\n");
        c->write(json + "\n");
        c->flush();
        this->rebuildMarkers();
    }
    else if (cmd == "markermove")
    {
        auto id = doc["id"].toInt(-1);
        if (id == -1)
        {
            this->badCommand(c, "missing id");
            return;
        }
        bool found = false;
        RemoteMarker *marker = nullptr;
        for (auto it = markers.begin(); it < markers.end(); it++)
        {
            if (it->id == id)
            {
                marker = &*it;
                found = true;
                break;
            }
        }
        if (!found)
        {
            this->badCommand(c, QString("marker %1 does not exist").arg(id));
            return;
        }
        auto iconId = doc["icon"].toInt(-1);
        auto pic = this->maybeGetPixmapForImageId(iconId);
        if (!pic)
        {
            this->badCommand(c,
                             QString("icon id (%1) does not exist, max id is %2").arg(iconId).arg(this->lastImageId));
            return;
        }
        int x = doc["x"].toInt();
        int z = doc["z"].toInt();
        int dim = doc["dim"].toInt();
        marker->vp->p.x = x;
        marker->vp->p.z = z;
        marker->dim = dim;
        marker->icon = iconId;
        qDebug() << "moved marker" << id << " to " << x << z << "dim" << dim;
        this->rebuildMarkers();
    }
    else if (cmd == "markerdel")
    {
        auto id = doc["id"].toInt(-1);
        if (id == -1)
        {
            this->badCommand(c, "missing id");
            return;
        }
        bool found = false;
        for (auto it = markers.begin(); it < markers.end(); it++)
        {
            if (it->id == id)
            {
                markers.erase(it);
                qDebug() << "deleted marker " << id;
                found = true;
                break;
            }
        }
        if (found)
        {
            this->rebuildMarkers();
        }
    }
    else if (cmd == "debug")
    {
        this->badCommand(c, "markers:");
        for (const auto &m : this->markers)
        {
            this->badCommand(c, QString(" - [%1] %2 (%3, %4)").arg(m.id).arg(m.dim).arg(m.vp->p.x).arg(m.vp->p.z));
        }
    }
    else if (cmd == "popup")
    {
        int w = doc["w"].toInt();
        int h = doc["h"].toInt();
        auto fp = this->mw->dock->focusPolicy();
        this->mw->dock->setFocusPolicy(Qt::NoFocus);
        this->mw->dock->setFloating(true);
        this->mw->dock->resize(w, h);
        this->mw->dock->setFocusPolicy(fp);
    }
    else if (cmd == "unpopup")
    {
        this->mw->dock->setFloating(false);
    }
    else if (cmd == "popupmove")
    {
        auto fp = this->mw->dock->focusPolicy();
        this->mw->dock->setFocusPolicy(Qt::NoFocus);
        int x = doc["x"].toInt();
        int y = doc["y"].toInt();

        this->mw->dock->move(x, y);
        this->mw->dock->setFocusPolicy(fp);
    }
}
void Rpc::badCommand(QLocalSocket *c, QString why)
{
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

void Rpc::rebuildMarkers()
{
    //qDebug() << "a";
    this->mw->getMapView()->setShow(D_REMOTE, true);
    //qDebug() << "b";
    auto *lvl = this->getLevel();
    //qDebug() << "c";
    for (const auto *q : lvl->cells)
    {
        if (q)
        {
            delete q;
        }
    }
    lvl->cells.clear();
    auto *q = new Quad(lvl, 0, 0);
    //qDebug() << "d";
    //qDebug() << "e" /*<< positions*/;
    auto *positions = new std::vector<VarPos>();
    positions->clear();
    //qDebug() << "f";
    int dim = this->mw->getDim();
    qDebug() << "refresh dim is " << dim;
    lvl->dim = dim;
    for (const auto &marker : this->markers)
    {
        //qDebug() << "g" << marker.icon << marker.id << marker.vp;
        //qDebug() << "h" << marker.vp->p.x << marker.vp->p.z;
        if (marker.dim == dim)
        {
            positions->push_back(*marker.vp);
        }
        //qDebug() << "i";
    }
    //qDebug() << "j";
    q->spos = positions;
    q->done = true;
    q->img = new QImage(":/icons/origin.png");
    lvl->cells.push_back(q);
    this->mw->getMapView()->update();
    //qDebug() << "k";
}

Rpc *Rpc::instance;
