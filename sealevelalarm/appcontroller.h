/*
 * appcontroller.h
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

#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QCoreApplication>
#include <QTimer>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTime>

class appcontroller : public QCoreApplication
{
    Q_OBJECT
public:
    appcontroller(int &argc, char **argv);
    ~appcontroller();

private slots:
    // gps
    void onPositionUpdated(const QGeoPositionInfo &info);
    void onPositionUpdateTimeout();
    void onPositionError(QGeoPositionInfoSource::Error err);

    // http
    void httpError(QNetworkReply::NetworkError error);
    void httpFinished();
    void httpReadyRead();


private:
    void elevationChanged(double elevation);
    void requestElevation(double lat, double lon);
    void startrequest(const QUrl& requestedUrl);

    QGeoPositionInfoSource *gps_source;

    QNetworkAccessManager network_manager;
    QNetworkReply *reply;
    QUrl url;

    QTime timer; // to measure time until request has finished
};

#endif // APPCONTROLLER_H
