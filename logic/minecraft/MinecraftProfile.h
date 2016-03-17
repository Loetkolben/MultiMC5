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

#include <QAbstractListModel>

#include <QString>
#include <QList>
#include <memory>

#include "Library.h"
#include "VersionFile.h"
#include "JarMod.h"

#include "multimc_logic_export.h"

class ProfileStrategy;
class OneSixInstance;


class MULTIMC_LOGIC_EXPORT MinecraftProfile : public QAbstractListModel
{
	Q_OBJECT
	friend class ProfileStrategy;

public:
	explicit MinecraftProfile(ProfileStrategy *strategy);

	void setStrategy(ProfileStrategy * strategy);
	ProfileStrategy *strategy();

	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	virtual int columnCount(const QModelIndex &parent) const override;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

	/// is this version unchanged by the user?
	bool isVanilla();

	/// remove any customizations on top of whatever 'vanilla' means
	bool revertToVanilla();

	/// install more jar mods
	void installJarMods(QStringList selectedFiles);

	/// DEPRECATED, remove ASAP
	int getFreeOrderNumber();

	enum MoveDirection { MoveUp, MoveDown };
	/// move patch file # up or down the list
	void move(const int index, const MoveDirection direction);

	/// remove patch file # - including files/records
	bool remove(const int index);

	/// remove patch file by id - including files/records
	bool remove(const QString id);

	bool customize(int index);

	bool revertToBase(int index);

	void resetOrder();

	/// reload all profile patches from storage, clear the profile and apply the patches
	void reload();

	/// clear the profile
	void clear();

	/// apply the patches. Throws all sorts of errors.
	void reapply();

	/// apply the patches. Catches all the errors and returns true/false for success/failure
	bool reapplySafe();

public:
	void applyMinecraftVersion(const QString& id);
	void applyMainClass(const QString& mainClass);
	void applyAppletClass(const QString& appletClass);
	void applyMinecraftArguments(const QString& minecraftArguments, bool isMinecraft);
	void applyMinecraftVersionType(const QString& type);
	void applyMinecraftAssets(const QString& assets);
	void applyTraits(const QSet<QString> &traits);
	void applyTweakers(const QStringList &tweakers);
	void applyJarMods(const QList<JarmodPtr>&jarMods);
	void applyLibrary(LibraryPtr library, bool isMinecraft);

public:
	/// get all java libraries that belong to the classpath
	QList<LibraryPtr> getActiveNormalLibs() const;

	/// get all native libraries that need to be available to the process
	QList<LibraryPtr> getActiveNativeLibs() const;

	QString getMinecraftVersion() const;
	QString getMainClass() const;
	QString getAppletClass() const;
	QString getMinecraftVersionType() const;
	QString getMinecraftAssets() const;
	QString getMinecraftArguments() const;
	QString getVanillaMinecraftArguments() const;
	const QSet<QString> & getTraits() const;
	const QStringList & getTweakers() const;
	const QList<JarmodPtr> & getJarMods() const;
	const QList<LibraryPtr> & getLibraries() const;
	const QList<LibraryPtr> & getVanillaLibraries() const;
	bool hasTrait(const QString & trait) const;

public:
	/// get file ID of the patch file at #
	QString versionFileId(const int index) const;

	/// get the profile patch by id
	ProfilePatchPtr versionPatch(const QString &id);

	/// get the profile patch by index
	ProfilePatchPtr versionPatch(int index);

	/// save the current patch order
	void saveCurrentOrder() const;

	/// Remove all the patches
	void clearPatches();

	/// Add the patch object to the internal list of patches
	void appendPatch(ProfilePatchPtr patch);

protected: /* data */
	/// the ID - determines which jar to use! ACTUALLY IMPORTANT!
	QString id;

	/// Release type - "release" or "snapshot"
	QString type;

	/// Assets type - "legacy" or a version ID
	QString assets;

	/**
	 * arguments that should be used for launching minecraft
	 *
	 * ex: "--username ${auth_player_name} --session ${auth_session}
	 *      --version ${version_name} --gameDir ${game_directory} --assetsDir ${game_assets}"
	 */
	QString minecraftArguments;

	/// Same as above, but only for vanilla
	QString vanillaMinecraftArguments;

	/// A list of all tweaker classes
	QStringList tweakers;

	/// The main class to load first
	QString mainClass;

	/// The applet class, for some very old minecraft releases
	QString appletClass;

	/// the list of libs - both active and inactive, native and java
	QList<LibraryPtr> libraries;

	/// same, but only vanilla.
	QList<LibraryPtr> vanillaLibraries;

	/// traits, collected from all the version files (version files can only add)
	QSet<QString> traits;

	/// A list of jar mods. version files can add those.
	QList<JarmodPtr> jarMods;

	/*
	FIXME: add support for those rules here? Looks like a pile of quick hacks to me though.

	"rules": [
		{
		"action": "allow"
		},
		{
		"action": "disallow",
		"os": {
			"name": "osx",
			"version": "^10\\.5\\.\\d$"
		}
		}
	],
	"incompatibilityReason": "There is a bug in LWJGL which makes it incompatible with OSX
	10.5.8. Please go to New Profile and use 1.5.2 for now. Sorry!"
	}
	*/
	// QList<Rule> rules;
private:
	QList<ProfilePatchPtr> VersionPatches;
	ProfileStrategy *m_strategy = nullptr;
};
