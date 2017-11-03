// Implements the KDevelop::IProject interface for subprojects
// Author: Max Schwarz <max.schwarz@online.de>

#include "catkinsubproject.h"

#include <interfaces/iplugin.h>

#include <project/projectmodel.h>
#include <project/interfaces/iprojectfilemanager.h>
#include <project/interfaces/ibuildsystemmanager.h>

#include <util/path.h>

#include <serialization/indexedstring.h>

#include <QDebug>

#include <KIO/StatJob>
#include <KIO/MkdirJob>
#include <KIO/FileCopyJob>

#include <KConfig>
#include <KConfigGroup>
#include <KMessageBox>

#include <KLocalizedString>

CatkinSubProject::CatkinSubProject(QObject* parent)
 : KDevelop::IProject(parent)
{

}

CatkinSubProject::~CatkinSubProject()
{
}

bool CatkinSubProject::open(
	const KDevelop::Path& path, const KDevelop::Path& buildPath,
	KDevelop::IPlugin* manager)
{
	m_projectFilePath = path;
	m_buildPath = buildPath;

	m_projectPath = path.parent();

	// developerfile == dirname(projectFileUrl) ."/.kdev4/". basename(projectfileUrl)
	m_developerFilePath = m_projectFilePath;
	m_developerFilePath.setLastPathSegment(QStringLiteral(".kdev4"));
	m_developerFilePath.addPath(m_projectFilePath.lastPathSegment());

	KIO::StatJob* statJob = KIO::stat(m_developerFilePath.toUrl(), KIO::HideProgressInfo);
	if(!statJob->exec())
	{
		// the developerfile does not exist yet, check if its folder exists
		// the developerfile itself will get created below
		QUrl dir = m_developerFilePath.parent().toUrl();
		statJob = KIO::stat(dir, KIO::HideProgressInfo);
		if(!statJob->exec())
		{
			KIO::SimpleJob* mkdirJob = KIO::mkdir(dir);
			if(!mkdirJob->exec())
			{
				KMessageBox::sorry(
					nullptr,
					i18n("Unable to create hidden dir (%1) for developer file",
					dir.toDisplayString(QUrl::PreferLocalFile) )
				);
				return false;
			}
		}
	}

	m_projectTempFile.open();
	auto copyJob = KIO::file_copy(
		m_projectFilePath.toUrl(),
		QUrl::fromLocalFile(m_projectTempFile.fileName()),
		-1, KIO::HideProgressInfo | KIO::Overwrite
	);
// 	KJobWidgets::setWindow(copyJob, Core::self()->uiController()->activeMainWindow());
	if(!copyJob->exec())
	{
		KMessageBox::sorry( nullptr,
						i18n("Unable to get project file: %1",
						m_projectFilePath.pathOrUrl() ) );
		return false;
	}

	if(m_developerFilePath.isLocalFile())
	{
		m_developerTempFilePath = m_developerFilePath.toLocalFile();
	}
	else
	{
		QTemporaryFile tmp;
		tmp.open();
		m_developerTempFilePath = tmp.fileName();

		auto job = KIO::file_copy(
			m_developerFilePath.toUrl(),
			QUrl::fromLocalFile(m_developerTempFilePath),
			-1, KIO::HideProgressInfo | KIO::Overwrite
		);
// 		KJobWidgets::setWindow(job, Core::self()->uiController()->activeMainWindow());
		job->exec();
	}

	m_cfg = KSharedConfig::openConfig(m_developerTempFilePath);
	m_cfg->addConfigSources(QStringList() << m_projectTempFile.fileName());
	KConfigGroup projectGroup( m_cfg, "Project" );

	// Initialize CMake to the build directory
	{
		KConfigGroup cmakeGroup(m_cfg, "CMake");

		cmakeGroup.writeEntry("Build Directory Count", "1");
		cmakeGroup.writeEntry("Current Build Directory Index", "0");

		KConfigGroup dirGroup(&cmakeGroup, "CMake Build Directory 0");

		dirGroup.writeEntry("Build Directory Path", buildPath.toLocalFile());
		dirGroup.writeEntry("Build Type", "");

		m_cfg->sync();
	}

	m_name = projectGroup.readEntry("Name", m_projectFilePath.lastPathSegment());

	m_manager = manager;
	KDevelop::IProjectFileManager* fileManager = m_manager->extension<KDevelop::IProjectFileManager>();
	Q_ASSERT(fileManager);

	m_topItem = fileManager->import(this);
	if(!m_topItem)
	{
		KMessageBox::sorry(nullptr, i18n("Could not open project"));
		return false;
	}

	return true;
}

KDevelop::Path CatkinSubProject::projectFile() const
{
	return m_projectFilePath;
}

KDevelop::Path CatkinSubProject::developerFile() const
{
	return m_developerFilePath;
}

QString CatkinSubProject::developerTempFile() const
{
	return m_developerTempFilePath;
}

KDevelop::Path CatkinSubProject::path() const
{
	return m_projectPath;
}

KSharedConfigPtr CatkinSubProject::projectConfiguration() const
{
	return m_cfg;
}

void CatkinSubProject::reloadModel()
{
	// TODO
}

void CatkinSubProject::addToFileSet(KDevelop::ProjectFileItem* file)
{
	if(m_fileSet.contains(file->indexedPath()))
		return;

	m_fileSet.insert(file->indexedPath());
	emit fileAddedToSet(file);
}

void CatkinSubProject::removeFromFileSet(KDevelop::ProjectFileItem* file)
{
    auto it = m_fileSet.find(file->indexedPath());
    if (it == m_fileSet.end())
        return;

    m_fileSet.erase(it);
    emit fileRemovedFromSet(file);
}

QSet<KDevelop::IndexedString> CatkinSubProject::fileSet() const
{
	return m_fileSet;
}

bool CatkinSubProject::isReady() const
{
    return true; // TODO
}

KDevelop::ProjectFolderItem* CatkinSubProject::projectItem() const
{
	return m_topItem;
}

KDevelop::IProjectFileManager* CatkinSubProject::projectFileManager() const
{
	return m_manager->extension<KDevelop::IProjectFileManager>();
}

KDevelop::IBuildSystemManager * CatkinSubProject::buildSystemManager() const
{
	return m_manager->extension<KDevelop::IBuildSystemManager>();
}

void CatkinSubProject::close()
{
	if(!m_developerFilePath.isLocalFile())
    {
		auto copyJob = KIO::file_copy(
			QUrl::fromLocalFile(m_developerTempFilePath),
			m_developerFilePath.toUrl(),
			-1, KIO::HideProgressInfo
		);
// 		KJobWidgets::setWindow(copyJob, Core::self()->uiController()->activeMainWindow());
		if(!copyJob->exec())
		{
			KMessageBox::sorry(nullptr,
				i18n("Could not store developer specific project configuration.\n"
					"Attention: The project settings you changed will be lost.")
			);
		}
	}
}

void CatkinSubProject::setReloadJob(KJob*)
{
}

QString CatkinSubProject::projectTempFile() const
{
	return m_projectTempFile.fileName();
}

bool CatkinSubProject::inProject(const KDevelop::IndexedString& url) const
{
	if(m_fileSet.contains(url))
		return true;

	return !itemsForPath(url).isEmpty();
}

QList<KDevelop::ProjectFileItem *> CatkinSubProject::filesForPath(const KDevelop::IndexedString& file) const
{
	QList<KDevelop::ProjectFileItem*> items;
	foreach(KDevelop::ProjectBaseItem* item, itemsForPath(file))
	{
		if(item->type() == KDevelop::ProjectBaseItem::File)
			items << static_cast<KDevelop::ProjectFileItem*>(item);
	}
	return items;
}

QList<KDevelop::ProjectFolderItem *> CatkinSubProject::foldersForPath(const KDevelop::IndexedString& folder) const
{
	QList<KDevelop::ProjectFolderItem*> items;
	foreach(KDevelop::ProjectBaseItem* item, itemsForPath(folder))
	{
		if(item->type() == KDevelop::ProjectBaseItem::Folder)
			items << static_cast<KDevelop::ProjectFolderItem*>(item);
	}

	return items;
}

QList<KDevelop::ProjectBaseItem *> CatkinSubProject::itemsForPath(const KDevelop::IndexedString& path) const
{
	if(path.isEmpty())
		return {};

	if(!m_topItem->model())
	{
		// this gets hit when the project has not yet been added to the model
		// i.e. during import phase
		// TODO: should we handle this somehow?
		// possible idea: make the item<->path hash per-project
		return {};
	}

	Q_ASSERT(m_topItem->model());
	QList<KDevelop::ProjectBaseItem*> items = m_topItem->model()->itemsForPath(path);

	auto it = items.begin();
	while(it != items.end())
	{
		if((*it)->project() != static_cast<const KDevelop::IProject*>(this))
			it = items.erase(it);
		else
			++it;
	}

	return items;
}


#include "catkinsubproject.moc"

