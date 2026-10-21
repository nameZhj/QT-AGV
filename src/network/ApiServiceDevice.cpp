/**
 * @file    ApiServiceDevice.cpp
 * @layer   ProtocolComm
 * @brief   Device management RESTful APIs including car registration, status, linkage, and MQTT dispatch.
 * @deps    ApiService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ApiService.h"

void ApiService::listCarAll(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/car/listAll", query), cb);
}

void ApiService::listCarAll(const Callback &cb)
{
    listCarAll(QJsonObject(), cb);
}

void ApiService::listCar(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/car/list", query), cb);
}

void ApiService::getCar(const QString &carId, const Callback &cb)
{
    get("/robot/car/" + carId, cb);
}

void ApiService::addCar(const QJsonObject &data, const Callback &cb)
{
    post("/robot/car", data, cb);
}

void ApiService::linkCar(const QJsonObject &data, const Callback &cb)
{
    put("/robot/car/link", data, cb);
}

void ApiService::updateCar(const QJsonObject &data, const Callback &cb)
{
    put("/robot/car", data, cb);
}

void ApiService::changeCarStatus(const QString &carId, const QString &mapId, const QString &status, const Callback &cb)
{
    put("/robot/car/changeStatus", QJsonObject{{"carId", carId}, {"mapId", mapId}, {"status", status}}, cb);
}

void ApiService::delCar(const QString &carId, const Callback &cb)
{
    del("/robot/car/" + carId, cb);
}

void ApiService::checkCertification(const QString &ckey, const Callback &cb)
{
    get("/robot/car/checkC/" + ckey, cb);
}

void ApiService::sendMqtt(const QJsonObject &data, const Callback &cb)
{
    post("/robot/mqtt/pub", data, cb);
}

void ApiService::editCarMap(const QString &mapId, const Callback &cb)
{
    const QString url = "/robot/car/edit/map/" + mapId;
    put(url, QJsonObject{}, [this, url, cb](int code, const QJsonObject &data) {
        if (code == 200) {
            cb(code, data);
        } else {
            get(url, [this, url, cb](int code2, const QJsonObject &data2) {
                if (code2 == 200) {
                    cb(code2, data2);
                } else {
                    post(url, QJsonObject{}, cb);
                }
            });
        }
    });
}
