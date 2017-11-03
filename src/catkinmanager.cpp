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

#include <interfaces/iproject.h>
#include <interfaces/icore.h>
#include <interfaces/iplugincontroller.h>

#include <project/projectmodel.h>

#include <util/executecompositejob.h>

#include <unistd.h>

using namespace KDevelop;

K_PLUGIN_FACTORY_WITH_JSON(CatkinManagerFactory, "kdevcatkin.json", registerPlugin<CatkinManager>(); )

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
	if(!m_cmakeManager)
	{
		IPlugin* plugin = core()->pluginController()->pluginForExtension(QStringLiteral("org.kdevelop.IProjectManager"), QStringLiteral("KDevCMakeManager"));
		Q_ASSERT(plugin);
		m_cmakeManager = plugin->extension<KDevelop::IProjectFileManager>();
		Q_ASSERT(m_cmakeManager);
	}

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

		KDevelop::Path buildPath(project->path(), "build");
		KDevelop::Path projectBuildPath(buildPath, name);

		if(!QFileInfo(projectBuildPath.toLocalFile()).isDir())
		{
			qWarning() << "No build directory found for package" << name;
			return;
		}

		// Do we already have a project file in place?
		KDevelop::Path projectSourcePath(packageXmlPath.parent());
		KDevelop::Path projectFilePath(projectSourcePath, QString("%1.kdev4").arg(name));
		if(QFile::exists(projectFilePath.toLocalFile()))
		{
			qDebug() << "Already have project file:" << projectFilePath;
		}

		qDebug() << "Build dir:" << projectBuildPath;
	}

	void start() override
	{
		KDevelop::Path projectPath = project->path();

		KDevelop::Path srcPath(projectPath, "src");

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
	}

private:
	IProject* const project;
	CatkinManager* const manager;
};

KJob* CatkinManager::createImportJob(KDevelop::ProjectFolderItem* item)
{
	auto project = item->project();

	auto job = new ListPackagesJob(project, this);
	connect(job, &KJob::result, this, [this, job, project](){
		if (job->error() != 0) {

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
	Path srcPath(project->path(), "src");

	qDebug() << "srcPath: " << srcPath << ", path:" << path;

	if(srcPath != path && !srcPath.isParentOf(path))
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
	qDebug() << "hasBuildInfo";
	return false;
}

KDevelop::Path CatkinManager::buildDirectory(KDevelop::ProjectBaseItem*) const
{
	return {};
}

KDevelop::Path::List CatkinManager::frameworkDirectories(KDevelop::ProjectBaseItem* item) const
{
	return {};
}

KDevelop::Path::List CatkinManager::includeDirectories(KDevelop::ProjectBaseItem* item) const
{
	return {};
}

QHash<QString, QString> CatkinManager::defines(KDevelop::ProjectBaseItem* item) const
{
	return {};
}

#include "catkinmanager.moc"
