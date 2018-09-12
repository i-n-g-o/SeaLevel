/*
 * appcontroller.cpp
 *
 * listens to geo-location changes, requests elevation data and compares the
 * elevation to a threshold.
 *
 * written by Ingo Randolf - 2018
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
*/

#include "appcontroller.h"

#include <qdebug.h>
#include <QGeoCoordinate>
#include <QtNetwork>
#include <QUrl>
#include <QNetworkReply>

typedef enum {
    ELEVATION_SRC_OPENELEVATION,
    ELEVATION_SRC_GPSVISUALIZER,
    ELEVATION_SRC_GOOGLE,
} elevation_src;

appcontroller::appcontroller(int &argc, char **argv)
    : QCoreApplication (argc, argv)
    , gps_source(nullptr)
    , reply(nullptr)
{

    gps_source = QGeoPositionInfoSource::createDefaultSource(this);

    if (gps_source) {
        connect(gps_source, &QGeoPositionInfoSource::positionUpdated,
                this, &appcontroller::onPositionUpdated);

        connect(gps_source, &QGeoPositionInfoSource::updateTimeout,
                this, &appcontroller::onPositionUpdateTimeout);

        connect(gps_source, SIGNAL(error(QGeoPositionInfoSource::Error)),
                this, SLOT(onPositionError(QGeoPositionInfoSource::Error)));

//        gps_source->setUpdateInterval(10000);
        gps_source->startUpdates();
        /* Qt 5.11 doc:
         * On iOS, starting from version 8, Core Location framework requires
         * additional entries in the application's Info.plist with keys
         * NSLocationAlwaysUsageDescription or NSLocationWhenInUseUsageDescription
         * and a string to be displayed in the authorization prompt.
         * The key NSLocationWhenInUseUsageDescription is used when requesting permission
         * to use location services while the app is in the foreground.
         * The key NSLocationAlwaysUsageDescription is used when requesting permission to use
         * location services whenever the app is running (both the foreground and the background).
         * If both entries are defined, NSLocationWhenInUseUsageDescription has a priority in the foreground mode.
        */
    }

    // start requetsting elevation for porto
    requestElevation(41.161758, -8.583933);
}

appcontroller::~appcontroller() {
    gps_source->stopUpdates();
    delete gps_source;
}

void appcontroller::requestElevation(double lat, double lon) {

    // elevation data:

    // interesting links:
    // https://en.wikipedia.org/wiki/Digital_elevation_model
    // https://opentopography.org
    // ESA data access: https://eo-sso-idp.eo.esa.int
    // NASA earth data: https://urs.earthdata.nasa.gov/users/new

    // open-elevation:
    // it is free and can be self-hosted
    // public api is limited to 1req/sec per IP
    // can be quite slow (3 - 30 seconds)
    // https://github.com/Jorl17/open-elevation/blob/master/docs/api.md
    // https://api.open-elevation.com/api/v1/lookup?locations=41.161758,-8.583933

    // gpsvisualizer:
    // we can parse the response data and calculate the elevation
    // might not be SRTM-30m data?
    // http://www.gpsvisualizer.com/elevation_data/elev2018.js?coords=41.161758,-8.583933

    // googles elevation-api:
    // downside: it's google, limited requests per minute/hour
    // https://developers.google.com/maps/documentation/elevation/start

    elevation_src es = ELEVATION_SRC_GPSVISUALIZER;
    QString url_string = "";

    switch (es) {
    case ELEVATION_SRC_OPENELEVATION:
        url_string = "https://api.open-elevation.com/api/v1/lookup?locations=" +
                QString::number(lat) + "," + QString::number(lon);
        break;
    case ELEVATION_SRC_GPSVISUALIZER:
        // this is returns quickly
        // but hackish
        url_string = "http://www.gpsvisualizer.com/elevation_data/elev2018.js?coords=" +
                QString::number(lat) + "," + QString::number(lon);
        break;
    case ELEVATION_SRC_GOOGLE:
        qDebug() << "google-elevation-api not implemented";
        return;
    }

    qDebug() << "requesting elevation for: " << lat << ":" << lon;
    startrequest(QUrl(url_string));
}


void appcontroller::elevationChanged(double elevation) {

    // where do we sealevel rise data from (dynamically)?
    // we could run something on our own servers to keep the level a dynamic lookup.
    // prognosis might change over time...

    // https://www.ucsusa.org/global_warming/science_and_impacts/impacts/infographic-sea-level-rise-global-warming.html#.W5j05lK-l24
    // until 2050: most likely: +6-16 inch | high: +16-24 inch (0,406 - 0,609 m)
    // until 2100: most likely: +12-48 inch | high: +48-78 (1,219 - 1,981 m)

    // https://www.skepticalscience.com/sea-level-rise-predictions.htm
    // https://coast.noaa.gov/digitalcoast/tools/slr

    // most extrem value found: ~ 2m in 2100

    qDebug() << "elevation changed: " << elevation;

    if (elevation < 2.0) {
        // ALARM
        // TODO: audio-visual alarm in a GUI app
        qDebug() << "sealevel alarm: in 2100 this part of land will be under water!";
    }
}


//
void appcontroller::onPositionUpdated(const QGeoPositionInfo &info) {

    qDebug() << "Position updated:" << info;

    // request elevation
    requestElevation(info.coordinate().latitude(), info.coordinate().longitude());
}

void appcontroller::onPositionUpdateTimeout() {
    qDebug() << "onPositionUpdateTimeout";

    /*
     * If requestUpdate() was called, this signal will be emitted if the current position could not be retrieved within the specified timeout.
     *
     * If startUpdates() has been called, this signal will be emitted if this QGeoPositionInfoSource subclass determines that it will not be able to provide further regular updates. This signal will not be emitted again until after the regular updates resume.
    */
}

void appcontroller::onPositionError(QGeoPositionInfoSource::Error err) {
    qDebug() << "onPositionError: " << err;
}

void appcontroller::startrequest(const QUrl& requestedUrl) {

    qDebug() << "start request for: " << requestedUrl.toString();

    timer.start();

    url = requestedUrl;
    reply = network_manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished,
            this, &appcontroller::httpFinished);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(httpError(QNetworkReply::NetworkError)));

    connect(reply, &QIODevice::readyRead,
            this, &appcontroller::httpReadyRead);
}


void appcontroller::httpError(QNetworkReply::NetworkError error) {
    qDebug() << "http error: " << error << ": " << reply->errorString();
}

void appcontroller::httpFinished() {

    const QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << reply->errorString();
    }

    reply->deleteLater();
    reply = nullptr;

    if (!redirectionTarget.isNull()) {
        const QUrl redirectedUrl = url.resolved(redirectionTarget.toUrl());
        startrequest(redirectedUrl);
        return;
    }

    qDebug() << "http request finished after: " << timer.elapsed() << " [ms]";
//    quit();
}

void appcontroller::httpReadyRead() {

    QByteArray arr = reply->readAll();
    QString data_string(arr);

//    qDebug() << "http data: " << data_string;

    if (data_string.startsWith("LocalElevationCallback(")) {

        bool dbl_ok;

        // parse string
        auto list = data_string.split("(");
        QString num_str = list[1].split(",")[0];
        QString datasrc_str = list[1].split(",")[1].split("'")[1];
        qDebug() << "elevation data source: " << datasrc_str;

        double num = num_str.toDouble(&dbl_ok);

        if (dbl_ok) {
            // calc elevation
            elevationChanged(1.0/num);
        } else {
            qDebug() << "error converting string to double: " << num_str;
        }

    } else if (data_string.startsWith("{")) {

        // json
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(arr, &err);
        if(doc.isObject())
        {
            QJsonValue results_value = doc.object().value("results");
            if (results_value.isArray()) {

                QJsonArray result_array = results_value.toArray();

                // get first result
                if (result_array.size() > 0 && result_array[0].isObject()) {

                    if (result_array[0].toObject().contains("elevation")) {
                        // get the elevation
                        elevationChanged(result_array[0].toObject().value("elevation").toDouble());
                    } else {
                        qDebug() << "no elevation in result array";
                    }

                } else {
                    qDebug() << "no object in result array";
                }
            } else {
                qDebug() << "no result in json";
            }
        }
    } else {
        // received something else
        qDebug() << "received other data: " << data_string;
    }
}
