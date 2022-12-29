#ifndef RPC_H
#define RPC_H

#include "cubiomes/layers.h"
#include "qjsondocument.h"
#include "qlocalsocket.h"
#include "qpixmap.h"
#include "src/mainwindow.h"
#include "world.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>

#include <vector>

class QLocalServer;
class QString;
class QJsonObject;
class MainWindow;

class RemoteMarker
{
public:
    int icon;
    int id;
    VarPos *vp;
    int dim;

    RemoteMarker(int icon, int id, VarPos vp, int dim)
        : icon(icon)
        , id(id)
        , vp(new VarPos(vp))
        , dim(dim)
    {
        this->vp->type = icon;
    }
    // yes this leaks memory and no i dont care that much
    /*
    ~RemoteMarker()
    {
        delete this->vp;
    }
    */
};

class ReceivedImage
{
public:
    QPixmap img;
    int id;

    ReceivedImage(QPixmap img, int id)
        : img(img)
        , id(id)
    {
    }
};

class Rpc : public QObject
{
    Q_OBJECT

protected:
    static Rpc *instance;

public:
    static Rpc *get()
    {
        return instance;
    }

    Rpc(MainWindow *mw)
        : rpcSock(new QLocalServer())
        , mw(mw)
    {
        instance = this;
        this->listen();

        this->mw->getMapView()->setShow(D_REMOTE, true);

        auto *lvl = this->getLevel();
        lvl->dim = 0;
        auto q = new Quad(lvl, 0, 0);
        q->done = true;
        q->img.storeRelaxed(new QImage(":/icons/origin.png"));
        auto *positions = new std::vector<VarPos>();
        q->spos.storeRelaxed(positions);
        lvl->cells.push_back(q);

        this->images.push_back(ReceivedImage(QPixmap::fromImage(QImage(":/icons/origin.png")), this->lastImageId++));
        auto id = this->lastMarkerId;
        this->lastMarkerId += 1;
        this->markers.push_back(RemoteMarker(this->lastMarkerId - 1, id, VarPos{{1337, 1337}, 0}, DIM_OVERWORLD));
        this->rebuildMarkers();
    }
    virtual ~Rpc()
    {
        this->rpcSock->close();
        QLocalServer::removeServer("cubiomes-rpc");
        qDebug() << "closing rpc...\n";
    }

    Level *getLevel();
    const QPixmap &getPixmapForRemoteMarker(VarPos *vp) const;

    void chatCommand(QString text);

private:
    QLocalServer *rpcSock;
    MainWindow *mw;
    std::vector<ReceivedImage> images;
    int lastImageId = 0;

    std::vector<RemoteMarker> markers;
    int lastMarkerId = 0;

    void listen();
    void onConnection();
    void onData(QLocalSocket *);
    void onError(QLocalSocket *c, QLocalSocket::LocalSocketError err);
    void handleCommand(QLocalSocket *c, QString cmd, QJsonObject doc);
    void cleanup(QLocalSocket *);
    void badCommand(QLocalSocket *c, QString why);
    void rebuildMarkers();

    const QPixmap *maybeGetPixmapForImageId(int imgId) const;

signals:
    void writeCommand(QJsonDocument doc);
};

#endif
