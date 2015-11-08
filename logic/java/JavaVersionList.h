/* Copyright 2013-2015 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <QObject>
#include <QAbstractListModel>

#include "BaseVersionList.h"
#include "tasks/Task.h"
#include "java/JavaCheckerJob.h"

#include "multimc_logic_export.h"

class JavaListLoadTask;

struct JavaVersion : public BaseVersion
{
	JavaVersion(){}
	JavaVersion(QString id, QString arch, QString path)
	: id(id), arch(arch), path(path)
	{
	}
	virtual QString descriptor()
	{
		return id;
	}

	virtual QString name()
	{
		return id;
	}

	virtual QString typeString() const
	{
		return arch;
	}

	QString id;
	QString arch;
	QString path;
	bool recommended = false;
};

typedef std::shared_ptr<JavaVersion> JavaVersionPtr;

class MULTIMC_LOGIC_EXPORT JavaVersionList : public BaseVersionList
{
	Q_OBJECT
public:
	explicit JavaVersionList(QObject *parent = 0);

	virtual Task *getLoadTask() override;
	virtual bool isLoaded() override;
	virtual const BaseVersionPtr at(int i) const override;
	virtual int count() const override;
	virtual void sortVersions() override;

	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual RoleList providesRoles() const override;

public slots:
	virtual void updateListData(QList<BaseVersionPtr> versions) override;

protected:
	QList<BaseVersionPtr> m_vlist;

	bool m_loaded = false;
};

class JavaListLoadTask : public Task
{
	Q_OBJECT

public:
	explicit JavaListLoadTask(JavaVersionList *vlist);
	~JavaListLoadTask();

	virtual void executeTask();
public slots:
	void javaCheckerFinished(QList<JavaCheckResult> results);

protected:
	std::shared_ptr<JavaCheckerJob> m_job;
	JavaVersionList *m_list;
	JavaVersion *m_currentRecommended;
};
