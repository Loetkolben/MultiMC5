#pragma once

#include <memory>
#include <QList>
#include <QJsonDocument>
#include "JarMod.h"

class MinecraftProfile;

enum ProblemSeverity
{
	PROBLEM_NONE,
	PROBLEM_WARNING,
	PROBLEM_ERROR
};

class PatchProblem
{
public:
	PatchProblem(ProblemSeverity severity, const QString & description)
	{
		m_severity = severity;
		m_description = description;
	}
	const QString & getDescription() const
	{
		return m_description;
	}
	const ProblemSeverity getSeverity() const
	{
		return m_severity;
	}
private:
	ProblemSeverity m_severity;
	QString m_description;
};

class ProfilePatch
{
public:
	virtual ~ProfilePatch(){};
	virtual void applyTo(MinecraftProfile *version) = 0;

	virtual bool isMinecraftVersion() = 0;
	virtual bool hasJarMods() = 0;
	virtual QList<JarmodPtr> getJarMods() = 0;

	virtual bool isMoveable() = 0;
	virtual bool isCustomizable() = 0;
	virtual bool isRevertible() = 0;
	virtual bool isRemovable() = 0;
	virtual bool isCustom() = 0;
	virtual bool isEditable() = 0;
	virtual bool isVersionChangeable() = 0;

	virtual void setOrder(int order) = 0;
	virtual int getOrder() = 0;

	virtual QString getPatchID() = 0;
	virtual QString getPatchName() = 0;
	virtual QString getPatchVersion() = 0;
	virtual QString getPatchFilename() = 0;

	virtual const QList<PatchProblem>& getProblems()
	{
		return m_problems;
	}
	virtual void addProblem(ProblemSeverity severity, const QString &description)
	{
		if(severity > m_problemSeverity)
		{
			m_problemSeverity = severity;
		}
		m_problems.append(PatchProblem(severity, description));
	}
	virtual ProblemSeverity getProblemSeverity()
	{
		return m_problemSeverity;
	}
	virtual bool hasFailed()
	{
		return getProblemSeverity() == PROBLEM_ERROR;
	}

protected:
	QList<PatchProblem> m_problems;
	ProblemSeverity m_problemSeverity = PROBLEM_NONE;
};

typedef std::shared_ptr<ProfilePatch> ProfilePatchPtr;
