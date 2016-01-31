//
// Created by robotbrain on 1/30/16.
//

#include <Env.h>
#include <QtCore/qurlquery.h>
#include <QtNetwork/qhttpmultipart.h>
#include "SkinUpload.h"

SkinUpload::SkinUpload(QFile *skinData, QString username, QByteArray password, SkinUpload::SkinType type) : m_skinData(skinData), m_skinType(type), m_username(username), m_password(password) {
    m_qnam = std::make_shared<QNetworkAccessManager>();
}

SkinUpload::~SkinUpload() {

}

void SkinUpload::executeTask() {
    QNetworkRequest request(QUrl("https://minecraft.net/login"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "MultiMC/5.0 (Uncached)");

    QNetworkReply *rep = m_qnam->get(request);
    m_reply = std::shared_ptr<QNetworkReply>(rep);

    setStatus(tr("Getting authenticity token..."));
    connect(rep, &QNetworkReply::uploadProgress, this, &Task::setProgress);
    connect(rep, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(downloadError(QNetworkReply::NetworkError)));
    connect(rep, SIGNAL(finished()), this, SLOT(downloadFinished()));
}

void SkinUpload::downloadError(QNetworkReply::NetworkError error) {
    // error happened during download.
    qCritical() << "Network error: " << error;
    emitFailed(m_reply->errorString());
}

void SkinUpload::downloadFinished() {
    if (m_reply->error() == QNetworkReply::NetworkError::NoError) {
        switch (m_uploadState) {
            case GET_TOKEN: {
                QByteArray data = m_reply->readAll();
                m_reply.reset();
                QRegExp tokenRegex("<input type=\"hidden\" name=\"authenticityToken\" value=\"(.*?)\">");
                if (tokenRegex.indexIn(data) < 0) {
                    qCritical() << "No authenticity token found!";
                    return;
                }
                m_authToken = tokenRegex.cap(1);

                QUrl url("https://minecraft.net/login");
                QUrlQuery query;
                query.addQueryItem("authenticityToken", m_authToken);
                query.addQueryItem("username", m_username);
                query.addQueryItem("password", m_password);
                query.addQueryItem("redirect", "/profile");
                url.setQuery(query);

                QNetworkRequest request(url);
                request.setHeader(QNetworkRequest::UserAgentHeader, "MultiMC/5.0 (Uncached)");

                m_uploadState = LOGIN;
                QNetworkReply *rep = m_qnam->get(request);
                m_reply = std::shared_ptr<QNetworkReply>(rep);

                setStatus(tr("Getting authenticity token..."));
                connect(rep, &QNetworkReply::uploadProgress, this, &Task::setProgress);
                connect(rep, SIGNAL(error(QNetworkReply::NetworkError)), this,
                        SLOT(downloadError(QNetworkReply::NetworkError)));
                connect(rep, SIGNAL(finished()), this, SLOT(downloadFinished()));
                break;
            }
            case LOGIN: {
                QByteArray data = m_reply->readAll();
                m_reply.reset();
                QRegExp errorRegex("<p class=\"error\">([\\\\w\\\\W]*?)</p>");
                if (errorRegex.indexIn(data) >= 0) {
                    qCritical() << "Login error: " << errorRegex.cap(1);
                    emitFailed(QString("Wrong credentials: %s").arg(errorRegex.cap(1)));
                    return;
                }

                QNetworkRequest request(QUrl("https://minecraft.net/profile/skin"));
                request.setHeader(QNetworkRequest::UserAgentHeader, "MultiMC/5.0 (Uncached)");
                QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

                QHttpPart authTokenPart;
                authTokenPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"authenticityToken\""));
                authTokenPart.setBody(m_authToken.toUtf8());
                multiPart->append(authTokenPart);

                QHttpPart skinTypePart;
                skinTypePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"model\""));
                skinTypePart.setBody(types[m_skinType]);
                multiPart->append(skinTypePart);

                QHttpPart imagePart;
                imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/png"));
                imagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(QString("form-data; name=\"skin\"; filename=\"%s\"").arg(m_skinData->fileName())));
                //The multipart takes ownership of the file object. Be aware.
                imagePart.setBodyDevice(m_skinData);
                m_skinData->setParent(multiPart);

                QNetworkReply *rep = m_qnam->post(request, multiPart);
                m_uploadState = UPLOAD;
                multiPart->setParent(rep);
                m_reply = std::shared_ptr<QNetworkReply>(rep);
                setStatus(tr("Getting authenticity token..."));
                connect(rep, &QNetworkReply::uploadProgress, this, &Task::setProgress);
                connect(rep, SIGNAL(error(QNetworkReply::NetworkError)), this,
                        SLOT(downloadError(QNetworkReply::NetworkError)));
                connect(rep, SIGNAL(finished()), this, SLOT(downloadFinished()));
                break;
            }
            case UPLOAD:
                qDebug() << "Skin uploaded.";
                //If we made it here, it worked
                emitSucceeded();
        }
    }
}
