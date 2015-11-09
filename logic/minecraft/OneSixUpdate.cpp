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

#include "Env.h"
#include "OneSixUpdate.h"

#include <QtNetwork>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDataStream>
#include <QEventLoop>
#include <JlCompress.h>

#include "BaseInstance.h"
#include "wonko/WonkoVersionList.h"
#include "wonko/WonkoVersion.h"
#include "wonko/WonkoIndex.h"
#include "minecraft/MinecraftProfile.h"
#include "minecraft/OneSixLibrary.h"
#include "minecraft/OneSixInstance.h"
#include "forge/ForgeMirrors.h"
#include "net/URLConstants.h"
#include "minecraft/AssetsUtils.h"
#include "Exception.h"
#include "MMCZip.h"
#include <FileSystem.h>

OneSixUpdate::OneSixUpdate(OneSixInstance *inst, QObject *parent) : Task(parent), m_inst(inst)
{
}

bool OneSixUpdate::run(const std::unique_ptr<Task> &task)
{
	if (task->isFinished())
	{
		return task->successful();
	}
	connect(task.get(), &Task::progress, this, &OneSixUpdate::progress);
	connect(task.get(), &Task::status, this, &OneSixUpdate::setStatus);

	QEventLoop loop;
	connect(task.get(), &Task::finished, &loop, &QEventLoop::quit);
	QMetaObject::invokeMethod(task.get(), "start", Qt::QueuedConnection); // < need to wait until the loop is started
	loop.exec();
	return task->successful();
}

void OneSixUpdate::executeTask()
{
	// Make directories
	QDir mcDir(m_inst->minecraftRoot());
	if (!mcDir.exists() && !mcDir.mkpath("."))
	{
		emitFailed(tr("Failed to create folder for minecraft binaries."));
		return;
	}

	// this is here for backwards compatibility, old instances don't have the minecraft/lwjgl patches
	// included normally, so we need to add them if they aren't already there.
	// it should be possible to just remove the next two lines and the following if after a year or two,
	// and instead have an error message telling people to do something
	const bool hasMinecraft = !!m_inst->getMinecraftProfile()->versionPatch("net.minecraft");
	const bool hasLwjgl = !!m_inst->getMinecraftProfile()->versionPatch("org.lwjgl");
	if (!hasMinecraft || !hasLwjgl)
	{
		if (!ENV.wonkoIndex()->isLocalLoaded())
		{
			run(ENV.wonkoIndex()->localUpdateTask());
		}
		if ((!ENV.wonkoIndex()->hasUid("net.minecraft") || !ENV.wonkoIndex()->hasUid("org.lwjgl")) && !ENV.wonkoIndex()->isRemoteLoaded())
		{
			run(ENV.wonkoIndex()->remoteUpdateTask());
		}

		if (!hasMinecraft)
		{
			WonkoVersionListPtr mcList = ENV.wonkoIndex()->getList("net.minecraft");
			if (!mcList->isLocalLoaded())
			{
				if (!run(mcList->localUpdateTask()) && !run(mcList->remoteUpdateTask()))
				{
					emitFailed(tr("Unable to load versions list for Minecraft"));
					return;
				}
			}
			WonkoVersionPtr version = mcList->getVersion(m_inst->intendedVersionId());
			if (!version->isLocalLoaded())
			{
				if (!run(version->localUpdateTask()) && !run(version->remoteUpdateTask()))
				{
					emitFailed(tr("Unable to load the wanted version of Minecraft"));
				}
			}
			m_inst->installWonkoVersion(version);
		}
		if (!hasLwjgl)
		{
			WonkoVersionListPtr lwjglList = ENV.wonkoIndex()->getList("org.lwjgl");
			if (!lwjglList->isLocalLoaded())
			{
				if (!run(lwjglList->localUpdateTask()) && !run(lwjglList->remoteUpdateTask()))
				{
					emitFailed(tr("Unable to load versions list for LWJGL"));
					return;
				}
			}
			WonkoVersionPtr version = lwjglList->getVersion("2.9.1");
			if (!version->isLocalLoaded())
			{
				if (!run(version->localUpdateTask()) && !run(version->remoteUpdateTask()))
				{
					emitFailed(tr("Unable to load the default version of LWJGL"));
				}
			}
			m_inst->installWonkoVersion(version);
		}

		try
		{
			m_inst->reloadProfile();
		}
		catch (Exception &e)
		{
			emitFailed(e.cause());
			return;
		}
	}

	// step 4: start the actual update process
	jarlibStart();
}

void OneSixUpdate::assetIndexStart()
{
	setStatus(tr("Updating assets index..."));
	OneSixInstance *inst = (OneSixInstance *)m_inst;
	std::shared_ptr<MinecraftProfile> version = inst->getMinecraftProfile();
	QString assetName = version->assets;
	QUrl indexUrl = "http://" + URLConstants::AWS_DOWNLOAD_INDEXES + assetName + ".json";
	QString localPath = assetName + ".json";
	auto job = new NetJob(tr("Asset index for %1").arg(inst->name()));

	auto metacache = ENV.metacache();
	auto entry = metacache->resolveEntry("asset_indexes", localPath);
	job->addNetAction(CacheDownload::make(indexUrl, entry));
	jarlibDownloadJob.reset(job);

	connect(jarlibDownloadJob.get(), SIGNAL(succeeded()), SLOT(assetIndexFinished()));
	connect(jarlibDownloadJob.get(), &NetJob::failed, this, &OneSixUpdate::assetIndexFailed);
	connect(jarlibDownloadJob.get(), SIGNAL(progress(qint64, qint64)),
			SIGNAL(progress(qint64, qint64)));

	qDebug() << m_inst->name() << ": Starting asset index download";
	jarlibDownloadJob->start();
}

void OneSixUpdate::assetIndexFinished()
{
	AssetsIndex index;
	qDebug() << m_inst->name() << ": Finished asset index download";

	OneSixInstance *inst = (OneSixInstance *)m_inst;
	std::shared_ptr<MinecraftProfile> version = inst->getMinecraftProfile();
	QString assetName = version->assets;

	QString asset_fname = "assets/indexes/" + assetName + ".json";
	if (!AssetsUtils::loadAssetsIndexJson(asset_fname, &index))
	{
		auto metacache = ENV.metacache();
		auto entry = metacache->resolveEntry("asset_indexes", assetName + ".json");
		metacache->evictEntry(entry);
		emitFailed(tr("Failed to read the assets index!"));
	}

	QList<Md5EtagDownloadPtr> dls;
	for (auto object : index.objects.values())
	{
		QString objectName = object.hash.left(2) + "/" + object.hash;
		QFileInfo objectFile("assets/objects/" + objectName);
		if ((!objectFile.isFile()) || (objectFile.size() != object.size))
		{
			auto objectDL = MD5EtagDownload::make(
				QUrl("http://" + URLConstants::RESOURCE_BASE + objectName),
				objectFile.filePath());
			objectDL->m_total_progress = object.size;
			dls.append(objectDL);
		}
	}
	if (dls.size())
	{
		setStatus(tr("Getting the assets files from Mojang..."));
		auto job = new NetJob(tr("Assets for %1").arg(inst->name()));
		for (auto dl : dls)
			job->addNetAction(dl);
		jarlibDownloadJob.reset(job);
		connect(jarlibDownloadJob.get(), SIGNAL(succeeded()), SLOT(assetsFinished()));
		connect(jarlibDownloadJob.get(), &NetJob::failed, this, &OneSixUpdate::assetsFailed);
		connect(jarlibDownloadJob.get(), SIGNAL(progress(qint64, qint64)),
				SIGNAL(progress(qint64, qint64)));
		jarlibDownloadJob->start();
		return;
	}
	assetsFinished();
}

void OneSixUpdate::assetIndexFailed(QString reason)
{
	qDebug() << m_inst->name() << ": Failed asset index download";
	emitFailed(tr("Failed to download the assets index:\n%1").arg(reason));
}

void OneSixUpdate::assetsFinished()
{
	emitSucceeded();
}

void OneSixUpdate::assetsFailed(QString reason)
{
	emitFailed(tr("Failed to download assets:\n%1").arg(reason));
}

void OneSixUpdate::jarlibStart()
{
	setStatus(tr("Getting the library files from Mojang..."));
	qDebug() << m_inst->name() << ": downloading libraries";
	OneSixInstance *inst = (OneSixInstance *)m_inst;
	try
	{
		inst->reloadProfile();
	}
	catch (Exception &e)
	{
		emitFailed(e.cause());
		return;
	}
	catch (...)
	{
		emitFailed(tr("Failed to load the version description file for reasons unknown."));
		return;
	}

	// Build a list of URLs that will need to be downloaded.
	std::shared_ptr<MinecraftProfile> version = inst->getMinecraftProfile();
	// minecraft.jar for this version
	{
		QString version_id = version->id;
		QString localPath = version_id + "/" + version_id + ".jar";
		QString urlstr = "http://" + URLConstants::AWS_DOWNLOAD_VERSIONS + localPath;

		auto job = new NetJob(tr("Libraries for instance %1").arg(inst->name()));

		auto metacache = ENV.metacache();
		auto entry = metacache->resolveEntry("versions", localPath);
		job->addNetAction(CacheDownload::make(QUrl(urlstr), entry));
		jarHashOnEntry = entry->md5sum;

		jarlibDownloadJob.reset(job);
	}

	// jarmods
	{
		QVector<JarmodPtr> brokenJarmods;

		for (const JarmodPtr &jarmod : version->jarMods)
		{
			if (jarmod->absoluteUrl.isEmpty() && !QDir(inst->jarModsDir()).exists(jarmod->name))
			{
				brokenJarmods.append(jarmod);
			}
			else
			{
				const QUrl url(jarmod->absoluteUrl);
				auto entry = ENV.metacache()->resolveEntry("jarmods", url.toString());
				if (entry->stale)
				{
					jarlibDownloadJob->addNetAction(CacheDownload::make(url, entry));
				}
			}
		}

		if (!brokenJarmods.isEmpty())
		{
			jarlibDownloadJob.reset();
			QStringList failed;
			for (const JarmodPtr &broken : brokenJarmods)
			{
				failed.append(broken->originalName);
			}
			emitFailed(tr("The following jarmods are neither available locally, nor is there any information available on how to fetch them: %1")
					   .arg(failed.join('\n')));
			return;
		}
	}

	// libraries
	{
		auto libs = version->getActiveNativeLibs();
		libs.append(version->getActiveNormalLibs());

		auto metacache = ENV.metacache();
		QList<ForgeXzDownloadPtr> ForgeLibs;
		QList<std::shared_ptr<OneSixLibrary>> brokenLocalLibs;

		for (auto lib : libs)
		{
			if (lib->hint() == "local")
			{
				if (!lib->filesExist(m_inst->librariesPath()))
					brokenLocalLibs.append(lib);
				continue;
			}

			QString raw_storage = lib->storageSuffix();
			QString raw_dl = lib->url();

			auto f = [&](QString storage, QString dl)
			{
				auto entry = metacache->resolveEntry("libraries", storage);
				if (entry->stale)
				{
					if (lib->hint() == "forge-pack-xz")
					{
						ForgeLibs.append(ForgeXzDownload::make(storage, entry));
					}
					else
					{
						jarlibDownloadJob->addNetAction(CacheDownload::make(dl, entry));
					}
				}
			};
			if (raw_storage.contains("${arch}"))
			{
				QString cooked_storage = raw_storage;
				QString cooked_dl = raw_dl;
				f(cooked_storage.replace("${arch}", "32"), cooked_dl.replace("${arch}", "32"));
				cooked_storage = raw_storage;
				cooked_dl = raw_dl;
				f(cooked_storage.replace("${arch}", "64"), cooked_dl.replace("${arch}", "64"));
			}
			else
			{
				f(raw_storage, raw_dl);
			}
		}
		if (!brokenLocalLibs.empty())
		{
			jarlibDownloadJob.reset();
			QStringList failed;
			for (auto brokenLib : brokenLocalLibs)
			{
				failed.append(brokenLib->files());
			}
			QString failed_all = failed.join("\n");
			emitFailed(tr("Some libraries marked as 'local' are missing their jar "
						  "files:\n%1\n\nYou'll have to correct this problem manually. If this is "
						  "an externally tracked instance, make sure to run it at least once "
						  "outside of MultiMC.").arg(failed_all));
			return;
		}
		// TODO: think about how to propagate this from the original json file... or IF AT ALL
		QString forgeMirrorList = "http://files.minecraftforge.net/mirror-brand.list";
		if (!ForgeLibs.empty())
		{
			jarlibDownloadJob->addNetAction(
				ForgeMirrors::make(ForgeLibs, jarlibDownloadJob, forgeMirrorList));
		}
	}

	connect(jarlibDownloadJob.get(), SIGNAL(succeeded()), SLOT(jarlibFinished()));
	connect(jarlibDownloadJob.get(), &NetJob::failed, this, &OneSixUpdate::jarlibFailed);
	connect(jarlibDownloadJob.get(), SIGNAL(progress(qint64, qint64)),
			SIGNAL(progress(qint64, qint64)));

	jarlibDownloadJob->start();
}

void OneSixUpdate::jarlibFinished()
{
	OneSixInstance *inst = (OneSixInstance *)m_inst;
	std::shared_ptr<MinecraftProfile> version = inst->getMinecraftProfile();

	// put jarmods into the instance
	try
	{
		for (const JarmodPtr &jarmod : version->jarMods)
		{
			auto entry = ENV.metacache()->resolveEntry("jarmods", QUrl(jarmod->absoluteUrl).toString());
			const QString dest = QDir(inst->jarModsDir()).absoluteFilePath(jarmod->name);

			if (QFile::exists(dest))
			{
				FS::remove(dest);
			}
			FS::copyFile(entry->getFullPath(), dest);
		}
	}
	catch (FS::FileSystemException &e)
	{
		emitFailed(tr("Unable to copy some jarmods into their final destinations: %1").arg(e.cause()));
	}

	if (version->traits.contains("legacyFML"))
	{
		fmllibsStart();
	}
	else
	{
		assetIndexStart();
	}
}

void OneSixUpdate::jarlibFailed(QString reason)
{
	QStringList failed = jarlibDownloadJob->getFailedFiles();
	QString failed_all = failed.join("\n");
	emitFailed(
		tr("Failed to download the following files:\n%1\n\nReason:%2\nPlease try again.").arg(failed_all, reason));
}

void OneSixUpdate::fmllibsStart()
{
	// Get the mod list
	OneSixInstance *inst = (OneSixInstance *)m_inst;
	std::shared_ptr<MinecraftProfile> fullversion = inst->getMinecraftProfile();
	bool forge_present = false;

	QString version = inst->intendedVersionId();
	auto &fmlLibsMapping = g_VersionFilterData.fmlLibsMapping;
	if (!fmlLibsMapping.contains(version))
	{
		assetIndexStart();
		return;
	}

	auto &libList = fmlLibsMapping[version];

	// determine if we need some libs for FML or forge
	setStatus(tr("Checking for FML libraries..."));
	forge_present = (fullversion->versionPatch("net.minecraftforge") != nullptr);
	// we don't...
	if (!forge_present)
	{
		assetIndexStart();
		return;
	}

	// now check the lib folder inside the instance for files.
	for (auto &lib : libList)
	{
		QFileInfo libInfo(FS::PathCombine(inst->libDir(), lib.filename));
		if (libInfo.exists())
			continue;
		fmlLibsToProcess.append(lib);
	}

	// if everything is in place, there's nothing to do here...
	if (fmlLibsToProcess.isEmpty())
	{
		assetIndexStart();
		return;
	}

	// download missing libs to our place
	setStatus(tr("Dowloading FML libraries..."));
	auto dljob = new NetJob("FML libraries");
	auto metacache = ENV.metacache();
	for (auto &lib : fmlLibsToProcess)
	{
		auto entry = metacache->resolveEntry("fmllibs", lib.filename);
		QString urlString = lib.ours ? URLConstants::FMLLIBS_OUR_BASE_URL + lib.filename
									 : URLConstants::FMLLIBS_FORGE_BASE_URL + lib.filename;
		dljob->addNetAction(CacheDownload::make(QUrl(urlString), entry));
	}

	connect(dljob, SIGNAL(succeeded()), SLOT(fmllibsFinished()));
	connect(dljob, &NetJob::failed, this, &OneSixUpdate::fmllibsFailed);
	connect(dljob, SIGNAL(progress(qint64, qint64)), SIGNAL(progress(qint64, qint64)));
	legacyDownloadJob.reset(dljob);
	legacyDownloadJob->start();
}

void OneSixUpdate::fmllibsFinished()
{
	legacyDownloadJob.reset();
	if (!fmlLibsToProcess.isEmpty())
	{
		setStatus(tr("Copying FML libraries into the instance..."));
		OneSixInstance *inst = (OneSixInstance *)m_inst;
		auto metacache = ENV.metacache();
		int index = 0;
		for (auto &lib : fmlLibsToProcess)
		{
			progress(index, fmlLibsToProcess.size());
			auto entry = metacache->resolveEntry("fmllibs", lib.filename);
			auto path = FS::PathCombine(inst->libDir(), lib.filename);
			if (!FS::ensureFilePathExists(path))
			{
				emitFailed(tr("Failed creating FML library folder inside the instance."));
				return;
			}
			if (!QFile::copy(entry->getFullPath(), FS::PathCombine(inst->libDir(), lib.filename)))
			{
				emitFailed(tr("Failed copying Forge/FML library: %1.").arg(lib.filename));
				return;
			}
			index++;
		}
		progress(index, fmlLibsToProcess.size());
	}
	assetIndexStart();
}

void OneSixUpdate::fmllibsFailed(QString reason)
{
	emitFailed(tr("Game update failed: it was impossible to fetch the required FML libraries.\nReason:\n%1").arg(reason));
	return;
}

