#pragma once

#include "BaseVersionList.h"
#include "BaseWonkoEntity.h"
#include <memory>

using WonkoVersionPtr = std::shared_ptr<class WonkoVersion>;
using WonkoVersionListPtr = std::shared_ptr<class WonkoVersionList>;

class MULTIMC_LOGIC_EXPORT WonkoVersionList : public BaseVersionList, public BaseWonkoEntity
{
	Q_OBJECT
	Q_PROPERTY(QString uid READ uid CONSTANT)
	Q_PROPERTY(QString name READ name NOTIFY nameChanged)
public:
	explicit WonkoVersionList(const QString &uid, QObject *parent = nullptr);

	enum Roles
	{
		UidRole = Qt::UserRole + 100,
		TimeRole,
		RequiresRole,
		WonkoVersionPtrRole
	};

	Task *getLoadTask() override;
	bool isLoaded() override;
	const BaseVersionPtr at(int i) const override;
	int count() const override;
	void sortVersions() override;

	BaseVersionPtr getLatestStable() const override;
	BaseVersionPtr getRecommended() const override;

	QVariant data(const QModelIndex &index, int role) const override;
	RoleList providesRoles() const override;
	QHash<int, QByteArray> roleNames() const override;

	std::unique_ptr<Task> remoteUpdateTask() override;
	std::unique_ptr<Task> localUpdateTask() override;

	QString localFilename() const override;
	QJsonObject serialized() const override;

	QString uid() const { return m_uid; }
	QString name() const { return m_name; }
	QString humanReadable() const;

	bool hasVersion(const QString &version) const;
	WonkoVersionPtr version(const QString &version) const;

	QVector<WonkoVersionPtr> versions() const { return m_versions; }

public: // for usage only by parsers
	void setName(const QString &name);
	void setVersions(const QVector<WonkoVersionPtr> &versions);
	void merge(const BaseWonkoEntity::Ptr &other);

signals:
	void nameChanged(const QString &name);

protected slots:
	void updateListData(QList<BaseVersionPtr> versions) override {}

private:
	QVector<WonkoVersionPtr> m_versions;
	QHash<QString, WonkoVersionPtr> m_lookup;
	QString m_uid;
	QString m_name;

	WonkoVersionPtr m_recommended;
	WonkoVersionPtr m_latest;

	void setupAddedVersion(const int row, const WonkoVersionPtr &version);
};

Q_DECLARE_METATYPE(WonkoVersionListPtr)
