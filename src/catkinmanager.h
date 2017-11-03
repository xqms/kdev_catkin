// Catkin project manager
// Author: Max Schwarz <max.schwarz@online.de>

#ifndef CATKINMANAGER_H
#define CATKINMANAGER_H

#include <project/abstractfilemanagerplugin.h>
#include <project/interfaces/iprojectfilemanager.h>
#include <project/interfaces/ibuildsystemmanager.h>

#include <project/projectmodel.h>

#include "catkinsubproject.h"
#include "catkinbuildmanager.h"

#include <memory>

class CatkinManager
  : public KDevelop::AbstractFileManagerPlugin
  , public virtual KDevelop::IProjectFileManager
  , public virtual KDevelop::IBuildSystemManager
{
Q_OBJECT

Q_INTERFACES(KDevelop::IProjectFileManager)
Q_INTERFACES(KDevelop::IBuildSystemManager)

public:
	explicit CatkinManager(QObject* parent = nullptr, const QVariantList& args = QVariantList());
	~CatkinManager();

	virtual Features features() const override { return Features(Folders | Files ); }
	virtual KDevelop::ProjectFolderItem *import(KDevelop::IProject *project ) override;

	virtual KJob* createImportJob(KDevelop::ProjectFolderItem* item) override;

	virtual KDevelop::IProjectBuilder* builder() const override;

	virtual KDevelop::Path::List includeDirectories(KDevelop::ProjectBaseItem *item) const override;
	virtual KDevelop::Path::List frameworkDirectories(KDevelop::ProjectBaseItem *item) const override;
	virtual QHash<QString, QString> defines(KDevelop::ProjectBaseItem *item) const override;

	virtual KDevelop::ProjectTargetItem* createTarget(const QString& target, KDevelop::ProjectFolderItem *parent) override;

	virtual bool removeTarget(KDevelop::ProjectTargetItem* target) override;

	virtual QList<KDevelop::ProjectTargetItem*> targets(KDevelop::ProjectFolderItem*) const override;

	virtual bool addFilesToTarget(
		const QList<KDevelop::ProjectFileItem*>& files,
		KDevelop::ProjectTargetItem* target
	) override;

	virtual bool removeFilesFromTargets(
		const QList<KDevelop::ProjectFileItem*> &files
	) override;

	virtual bool hasBuildInfo(KDevelop::ProjectBaseItem* item) const override;

	virtual KDevelop::Path buildDirectory(KDevelop::ProjectBaseItem*) const override;

	void addSubproject(CatkinSubProject* project);

	inline KDevelop::IProjectFileManager* cmakeManager()
	{ return m_cmakeManager; }

	inline KDevelop::IPlugin* cmakePlugin()
	{ return m_cmakePlugin; }
protected:
	virtual bool isValid(const KDevelop::Path& path, const bool isFolder, KDevelop::IProject* project) const override;

	virtual KDevelop::ProjectFileItem* createFileItem(
		KDevelop::IProject* project, const KDevelop::Path& path,
		KDevelop::ProjectBaseItem* parent) override;

	virtual KDevelop::ProjectFolderItem* createFolderItem(
		KDevelop::IProject* project, const KDevelop::Path& path,
		KDevelop::ProjectBaseItem* parent) override;
private:
	std::shared_ptr<CatkinBuildManager> m_buildManager;
	KDevelop::IPlugin* m_cmakePlugin = 0;
	KDevelop::IProjectFileManager* m_cmakeManager = 0;

	KDevelop::ProjectModel m_subProjectModel;
	QList<CatkinSubProject*> m_subProjects;
};

#endif
