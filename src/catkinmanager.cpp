// Catkin project manager
// Author: Max Schwarz <max.schwarz@online.de>

#include "catkinmanager.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QStack>
#include <QDomDocument>

#include <KPluginFactory>
#include <KDirWatch>
#include <KJob>
#include <KConfig>
#include <KConfigGroup>

#include <interfaces/iproject.h>
#include <interfaces/icore.h>
#include <interfaces/iplugincontroller.h>

#include <serialization/indexedstring.h>

#include <project/projectmodel.h>

#include <util/executecompositejob.h>

#include <unistd.h>

using namespace KDevelop;

K_PLUGIN_FACTORY_WITH_JSON(CatkinManagerFactory, "kdevcatkin.json", registerPlugin<CatkinManager>(); )

namespace
{

class SubProjectFile : public KDevelop::ProjectFileItem
{
public:
	SubProjectFile(IProject* project, const Path& path, KDevelop::ProjectFileItem* subProject, ProjectBaseItem* parent = nullptr)
	 : ProjectFileItem(project, path, parent)
	 , m_subProject(subProject)
	{}

	KDevelop::ProjectFileItem* subProjectItem()
	{ return m_subProject; }
private:
	KDevelop::ProjectFileItem* m_subProject;
};

class SubProjectRoot : public KDevelop::ProjectFolderItem
{
public:
	SubProjectRoot(IProject* project, const Path& path, ProjectBaseItem* parent, CatkinSubProject* subProject)
	 : ProjectFolderItem(project, path, parent)
	 , m_subProject(subProject)
	{}

	QString iconName() const override
	{ return "package-x-generic"; }

	CatkinSubProject* subProject()
	{ return m_subProject; }
private:
	CatkinSubProject* m_subProject;
};

}


CatkinManager::CatkinManager(QObject* parent, const QVariantList&)
 : KDevelop::AbstractFileManagerPlugin(QStringLiteral("kdevcatkin"), parent)
{
	m_buildManager.reset(new CatkinBuildManager);
}

CatkinManager::~CatkinManager()
{
}

KDevelop::ProjectFolderItem *CatkinManager::import(KDevelop::IProject *project)
{
	return AbstractFileManagerPlugin::import(project);
}

class ListPackagesJob : public KDevelop::ExecuteCompositeJob
{
Q_OBJECT
public:
	ListPackagesJob(IProject* project, CatkinManager* manager)
	 : ExecuteCompositeJob(manager, {})
	 , project(project)
	 , manager(manager)
	{
	}

	void processPackage(const KDevelop::Path& packageXmlPath)
	{
		QFile packageXml(packageXmlPath.toLocalFile());
		QDomDocument doc;

		QString errorMsg;
		int errorLine, errorColumn;
		if(!doc.setContent(&packageXml, &errorMsg, &errorLine, &errorColumn))
		{
			fprintf(stderr, "Could not parse package XML '%s':%d:%d: %s\n",
				qPrintable(packageXmlPath.toLocalFile()),
				errorLine, errorColumn,
				qPrintable(errorMsg)
			);
			return;
		}

		QDomElement packageElem = doc.namedItem("package").toElement();
		if(packageElem.isNull())
		{
			qWarning()
				<< "No <package> element found for package at"
				<< packageXmlPath.toLocalFile();
			return;
		}

		QDomElement nameElem = packageElem.namedItem("name").toElement();
		if(nameElem.isNull())
		{
			qWarning()
				<< "No <name> element found for package at "
				<< packageXmlPath.toLocalFile();
			return;
		}

		QString name = nameElem.text();

		KDevelop::Path buildPath(project->path(), "../build");
		KDevelop::Path projectBuildPath(buildPath, name);

		if(!QFileInfo(projectBuildPath.toLocalFile()).isDir())
		{
			qWarning() << "No build directory found for package" << name;
			return;
		}

		// Do we already have a project file in place?
		KDevelop::Path projectSourcePath(packageXmlPath.parent());
		KDevelop::Path projectFilePath(projectSourcePath, QString("%1.kdev4").arg(name));

		if(!QFile::exists(projectFilePath.toLocalFile()))
		{
			KSharedConfigPtr cfg = KSharedConfig::openConfig(projectFilePath.toLocalFile(), KConfig::SimpleConfig);
			if(!cfg->isConfigWritable(true))
			{
				qWarning() << "Can't write to config file";
				return;
			}

			KConfigGroup grp = cfg->group("Project");
			grp.writeEntry("Name", name);
			grp.writeEntry("CreatedFrom", "CMakeLists.txt");
			grp.writeEntry("Manager", "KDevCMakeManager");
			cfg->sync();
		}

		auto project = new CatkinSubProject(manager);

		if(!project->open(projectFilePath, projectBuildPath, manager->cmakePlugin()))
		{
			qWarning("Could not open project");
			return;
		}

		manager->addSubproject(project);

		auto job = manager->cmakeManager()->createImportJob(project->projectItem());

		connect(job, &KJob::result, this, [project](){
			qDebug() << "=========================== Subproject import for" << project->name() << "finished ========================";
		});

		addSubjob(job);
	}

	void start() override
	{
		KDevelop::Path projectPath = project->path();

		KDevelop::Path srcPath(projectPath);

		// Crawl for packages.
		QStack<KDevelop::Path> fringe;
		fringe.push(srcPath);

		while(!fringe.isEmpty())
		{
			Path path = fringe.pop();
			QDir dir(path.toLocalFile());

			Path ignoreFile(path, "CATKIN_IGNORE");
			if(QFile::exists(ignoreFile.toLocalFile()))
				continue;

			Path packageFile(path, "package.xml");
			if(QFile::exists(packageFile.toLocalFile()))
			{
				processPackage(packageFile);
			}

			QDirIterator it(path.toLocalFile());
			while(it.hasNext())
			{
				it.next();

				auto info = it.fileInfo();
				if(info.fileName().startsWith('.'))
					continue;

				if(info.isDir())
					fringe.push(Path(path, info.fileName()));
			}
		}

		ExecuteCompositeJob::start();
	}

private:
	IProject* const project;
	CatkinManager* const manager;
};

KJob* CatkinManager::createImportJob(KDevelop::ProjectFolderItem* item)
{
	if(!m_cmakeManager)
	{
		m_cmakePlugin = core()->pluginController()->pluginForExtension(QStringLiteral("org.kdevelop.IProjectFileManager"), QStringLiteral("KDevCMakeManager"));
		Q_ASSERT(m_cmakePlugin);
		m_cmakeManager = m_cmakePlugin->extension<KDevelop::IProjectFileManager>();
		Q_ASSERT(m_cmakeManager);
	}

	auto project = item->project();

	auto job = new ListPackagesJob(project, this);
	connect(job, &KJob::result, this, [job](){
		if (job->error() != 0) {
			qWarning() << "Got error from ListPackagesJob";
		}
	});

	const QList<KJob*> jobs = {
		job,
		KDevelop::AbstractFileManagerPlugin::createImportJob(item) // generate the file system listing
	};

	Q_ASSERT(!jobs.contains(nullptr));
	ExecuteCompositeJob* composite = new ExecuteCompositeJob(this, jobs);
	//     even if the cmake call failed, we want to load the project so that the project can be worked on
	composite->setAbortOnError(false);
	return composite;
}


bool CatkinManager::isValid(const Path& path, const bool isFolder, KDevelop::IProject* project) const
{
	if(path.lastPathSegment().endsWith(".kdev4"))
		return false;

	return AbstractFileManagerPlugin::isValid(path, isFolder, project);
}

KDevelop::IProjectBuilder* CatkinManager::builder() const
{
	return m_buildManager.get();
}

KDevelop::ProjectTargetItem * CatkinManager::createTarget(const QString& target, KDevelop::ProjectFolderItem* parent)
{
	return 0;
}

QList<KDevelop::ProjectTargetItem *> CatkinManager::targets(KDevelop::ProjectFolderItem*) const
{
	return {};
}

bool CatkinManager::addFilesToTarget(const QList<KDevelop::ProjectFileItem *>& files, KDevelop::ProjectTargetItem* target)
{
	return false;
}

bool CatkinManager::removeFilesFromTargets(const QList<KDevelop::ProjectFileItem *>& files)
{
	return false;
}

bool CatkinManager::removeTarget(KDevelop::ProjectTargetItem* target)
{
	return false;
}

bool CatkinManager::hasBuildInfo(KDevelop::ProjectBaseItem* item) const
{
	auto fileItem = dynamic_cast<SubProjectFile*>(item);

	if(fileItem)
	{
		auto buildManager = fileItem->subProjectItem()->project()->buildSystemManager();
		if(!buildManager)
			return false;

		bool available = buildManager->hasBuildInfo(fileItem->subProjectItem());

		return available;
	}

	return false;
}

KDevelop::Path CatkinManager::buildDirectory(KDevelop::ProjectBaseItem*) const
{
	return {};
}

KDevelop::Path::List CatkinManager::frameworkDirectories(KDevelop::ProjectBaseItem* item) const
{
	auto fileItem = dynamic_cast<SubProjectFile*>(item);

	if(fileItem)
	{
		auto buildManager = fileItem->subProjectItem()->project()->buildSystemManager();
		if(!buildManager)
			return {};

		auto directories = buildManager->frameworkDirectories(fileItem->subProjectItem());

		return directories;
	}

	return {};
}

KDevelop::Path::List CatkinManager::includeDirectories(KDevelop::ProjectBaseItem* item) const
{
	auto fileItem = dynamic_cast<SubProjectFile*>(item);

	if(fileItem)
	{
		auto buildManager = fileItem->subProjectItem()->project()->buildSystemManager();
		if(!buildManager)
			return {};

		auto directories = buildManager->includeDirectories(fileItem->subProjectItem());

		return directories;
	}

	return {};
}

QHash<QString, QString> CatkinManager::defines(KDevelop::ProjectBaseItem* item) const
{
	auto fileItem = dynamic_cast<SubProjectFile*>(item);

	if(fileItem)
	{
		auto buildManager = fileItem->subProjectItem()->project()->buildSystemManager();
		if(!buildManager)
			return {};

		return buildManager->defines(fileItem->subProjectItem());
	}

	return {};
}

bool CatkinManager::reload(KDevelop::ProjectFolderItem* item)
{
	auto folderItem = dynamic_cast<SubProjectRoot*>(item);

	if(folderItem)
	{
		qWarning() << "Reloading sub project";
		auto fileManager = folderItem->subProject()->projectFileManager();
		if(!fileManager)
			return false;

		return fileManager->reload(folderItem->subProject()->projectItem());
	}

	return KDevelop::AbstractFileManagerPlugin::reload(item);
}


QString CatkinManager::extraArguments(KDevelop::ProjectBaseItem* item) const
{
	auto fileItem = dynamic_cast<SubProjectFile*>(item);

	if(fileItem)
	{
		auto buildManager = fileItem->subProjectItem()->project()->buildSystemManager();
		if(!buildManager)
			return QString();

		return buildManager->extraArguments(fileItem->subProjectItem());
	}

	return QString();
}

void CatkinManager::addSubproject(CatkinSubProject* project)
{
	m_subProjects << project;
	m_subProjectModel.appendRow(project->projectItem());
}

KDevelop::ProjectFileItem* CatkinManager::createFileItem(KDevelop::IProject* project, const KDevelop::Path& path, KDevelop::ProjectBaseItem* parent)
{
	KDevelop::IndexedString indexedPath(path.toLocalFile());

	auto it = std::find_if(m_subProjects.begin(), m_subProjects.end(), [&](CatkinSubProject* subProj){
		return subProj->inProject(indexedPath);
	});

	if(it != m_subProjects.end())
	{
		auto items = (*it)->itemsForPath(indexedPath);

		if(items.isEmpty())
		{
			qWarning() << "Got empty response from itemsForPath on" << indexedPath;
			return AbstractFileManagerPlugin::createFileItem(project, path, parent);
		}

		auto item = dynamic_cast<KDevelop::ProjectFileItem*>(items[0]);
		if(!item)
		{
			qWarning() << "Got invalid object type from itemsForPath";
			return AbstractFileManagerPlugin::createFileItem(project, path, parent);
		}

		return new SubProjectFile(project, path, item, parent);
	}

	return AbstractFileManagerPlugin::createFileItem(project, path, parent);
}

KDevelop::ProjectFolderItem* CatkinManager::createFolderItem(KDevelop::IProject* project, const KDevelop::Path& path, KDevelop::ProjectBaseItem* parent)
{
	auto it = std::find_if(m_subProjects.begin(), m_subProjects.end(), [&](CatkinSubProject* subProj){
		return subProj->path() == path;
	});

	if(it != m_subProjects.end())
		return new SubProjectRoot(project, path, parent, *it);

	return AbstractFileManagerPlugin::createFolderItem(project, path, parent);
}

KDevelop::Path CatkinManager::compiler(KDevelop::ProjectTargetItem* p) const
{
	return {};
}

#include "catkinmanager.moc"
