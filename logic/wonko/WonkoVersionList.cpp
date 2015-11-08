#include "WonkoVersionList.h"

#include <QDateTime>

#include "WonkoVersion.h"
#include "tasks/BaseWonkoEntityRemoteLoadTask.h"
#include "tasks/BaseWonkoEntityLocalLoadTask.h"
#include "format/WonkoFormat.h"
#include "WonkoReference.h"

WonkoVersionList::WonkoVersionList(const QString &uid, QObject *parent)
	: BaseVersionList(parent), m_uid(uid)
{
}

Task *WonkoVersionList::getLoadTask()
{
	return remoteUpdateTask().release();
}

bool WonkoVersionList::isLoaded()
{
	return isLocalLoaded() && isRemoteLoaded();
}

const BaseVersionPtr WonkoVersionList::at(int i) const
{
	return m_versions.at(i);
}
int WonkoVersionList::count() const
{
	return m_versions.size();
}

void WonkoVersionList::sortVersions()
{
	std::sort(m_versions.begin(), m_versions.end(), [](const WonkoVersionPtr &a, const WonkoVersionPtr &b)
	{
		return *a.get() < *b.get();
	});
}

QVariant WonkoVersionList::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_versions.size() || index.parent().isValid())
	{
		return QVariant();
	}

	WonkoVersionPtr version = m_versions.at(index.row());

	switch (role)
	{
	case VersionPointerRole: return QVariant::fromValue(std::dynamic_pointer_cast<BaseVersion>(version));
	case VersionRole:
	case VersionIdRole:
		return version->version();
	case ParentGameVersionRole:
	{
		const auto it = std::find_if(version->requires().begin(), version->requires().end(),
									 [](const WonkoReference &ref) { return ref.uid() == "net.minecraft"; });
		if (it != version->requires().end())
		{
			return (*it).version();
		}
		return QVariant();
	}
	case TypeRole: return version->type();

	case UidRole: return version->uid();
	case TimeRole: return version->time();
	case RequiresRole: return QVariant::fromValue(version->requires());
	case SortRole: return version->rawTime();
	case WonkoVersionPtrRole: return QVariant::fromValue(version);
	case RecommendedRole: return version == getRecommended();
	case LatestRole: return version == getLatestStable();
	default: return QVariant();
	}
}

BaseVersionList::RoleList WonkoVersionList::providesRoles() const
{
	return {VersionPointerRole, VersionRole, VersionIdRole, ParentGameVersionRole,
				TypeRole, UidRole, TimeRole, RequiresRole, SortRole,
				RecommendedRole, LatestRole};
}

QHash<int, QByteArray> WonkoVersionList::roleNames() const
{
	QHash<int, QByteArray> roles = BaseVersionList::roleNames();
	roles.insert(UidRole, "uid");
	roles.insert(TimeRole, "time");
	roles.insert(SortRole, "sort");
	roles.insert(RequiresRole, "requires");
	return roles;
}

std::unique_ptr<Task> WonkoVersionList::remoteUpdateTask()
{
	return std::make_unique<WonkoVersionListRemoteLoadTask>(this, this);
}
std::unique_ptr<Task> WonkoVersionList::localUpdateTask()
{
	return std::make_unique<WonkoVersionListLocalLoadTask>(this, this);
}

QString WonkoVersionList::localFilename() const
{
	return m_uid + ".json";
}
QJsonObject WonkoVersionList::serialized() const
{
	return WonkoFormat::serializeVersionList(this);
}

QString WonkoVersionList::humanReadable() const
{
	return m_name.isEmpty() ? m_uid : m_name;
}

bool WonkoVersionList::hasVersion(const QString &version) const
{
	return m_lookup.contains(version);
}
WonkoVersionPtr WonkoVersionList::version(const QString &version) const
{
	return m_lookup.value(version);
}

void WonkoVersionList::setName(const QString &name)
{
	m_name = name;
	emit nameChanged(name);
}
void WonkoVersionList::setVersions(const QVector<WonkoVersionPtr> &versions)
{
	beginResetModel();
	m_versions = versions;
	std::sort(m_versions.begin(), m_versions.end(), [](const WonkoVersionPtr &a, const WonkoVersionPtr &b)
	{
		return a->rawTime() > b->rawTime();
	});
	for (int i = 0; i < m_versions.size(); ++i)
	{
		m_lookup.insert(m_versions.at(i)->version(), m_versions.at(i));
		setupAddedVersion(i, m_versions.at(i));
	}

	m_latest = m_versions.isEmpty() ? nullptr : m_versions.first();
	auto recommendedIt = std::find_if(m_versions.constBegin(), m_versions.constEnd(), [](const WonkoVersionPtr &ptr) { return ptr->type() == "release"; });
	m_recommended = recommendedIt == m_versions.constEnd() ? nullptr : *recommendedIt;
	endResetModel();
}

void WonkoVersionList::merge(const BaseWonkoEntity::Ptr &other)
{
	const WonkoVersionListPtr list = std::dynamic_pointer_cast<WonkoVersionList>(other);
	if (m_name != list->m_name)
	{
		setName(list->m_name);
	}

	if (m_versions.isEmpty())
	{
		setVersions(list->m_versions);
	}
	else
	{
		for (const WonkoVersionPtr &version : list->m_versions)
		{
			if (m_lookup.contains(version->version()))
			{
				m_lookup.value(version->version())->merge(version);
			}
			else
			{
				beginInsertRows(QModelIndex(), m_versions.size(), m_versions.size());
				setupAddedVersion(m_versions.size(), version);
				m_versions.append(version);
				m_lookup.insert(version->uid(), version);
				endInsertRows();

				if (!m_latest || version->rawTime() > m_latest->rawTime())
				{
					m_latest = version;
					emit dataChanged(index(0), index(m_versions.size() - 1), QVector<int>() << LatestRole);
				}
				if (!m_recommended || (version->type() == "release" && version->rawTime() > m_recommended->rawTime()))
				{
					m_recommended = version;
					emit dataChanged(index(0), index(m_versions.size() - 1), QVector<int>() << RecommendedRole);
				}
			}
		}
	}
}

void WonkoVersionList::setupAddedVersion(const int row, const WonkoVersionPtr &version)
{
	connect(version.get(), &WonkoVersion::requiresChanged, this, [this, row]() { emit dataChanged(index(row), index(row), QVector<int>() << RequiresRole); });
	connect(version.get(), &WonkoVersion::timeChanged, this, [this, row]() { emit dataChanged(index(row), index(row), QVector<int>() << TimeRole << SortRole); });
	connect(version.get(), &WonkoVersion::typeChanged, this, [this, row]() { emit dataChanged(index(row), index(row), QVector<int>() << TypeRole); });
}

BaseVersionPtr WonkoVersionList::getLatestStable() const
{
	return m_latest;
}

BaseVersionPtr WonkoVersionList::getRecommended() const
{
	return m_recommended;
}
