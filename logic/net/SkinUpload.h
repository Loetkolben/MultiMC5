#pragma once
#include "tasks/Task.h"
#include <QMessageBox>
#include <QNetworkReply>
#include <QBuffer>
#include <memory>
#include <QtCore/qfile.h>

#include "multimc_logic_export.h"

class MULTIMC_LOGIC_EXPORT SkinUpload : public Task
{
Q_OBJECT
public:
    enum SkinType {
        ALEX = 0,
        STEVE
    };
    SkinUpload(QFile *skinData, QString username, QByteArray password, SkinType type = STEVE);
    virtual ~SkinUpload();
protected:
    virtual void executeTask();
private:
    enum Upload_State {
        GET_TOKEN,
        LOGIN,
        UPLOAD;
    } m_uploadState = GET_TOKEN;
    QFile *m_skinData;
    SkinType m_skinType;
    std::shared_ptr<QNetworkReply> m_reply;
    QString m_authToken;
    QString m_username;
    QByteArray m_password;
    std::shared_ptr<QNetworkAccessManager> m_qnam;
    const QByteArrayList types = QByteArrayList() << "3pxarm" << "4pxarm";
public
    slots:
    void downloadError(QNetworkReply::NetworkError);
    void downloadFinished();


};


