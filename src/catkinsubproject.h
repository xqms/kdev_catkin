// Implements the KDevelop::IProject interface for subprojects
// Author: Max Schwarz <max.schwarz@online.de>

#ifndef CATKINSUBPROJECT_H
#define CATKINSUBPROJECT_H

#include <interfaces/iproject.h>

#include <util/path.h>

#include <KSharedConfig>

#include <QSet>
#include <QTemporaryFile>

class CatkinSubProject
 : public KDevelop::IProject
{
Q_OBJECT
public:
	explicit CatkinSubProject(QObject *parent = nullptr);
	~CatkinSubProject() override;

	bool open(
		const KDevelop::Path& path, const KDevelop::Path& buildPath,
		KDevelop::IPlugin* manager
	);

	QList<KDevelop::ProjectBaseItem*> itemsForPath(const KDevelop::IndexedString& path) const override;
	QList<KDevelop::ProjectFileItem*> filesForPath(const KDevelop::IndexedString& file) const override;
	QList<KDevelop::ProjectFolderItem*> foldersForPath(const KDevelop::IndexedString& folder) const override;

	QString projectTempFile() const;
	QString developerTempFile() const;
	KDevelop::Path developerFile() const;
	void reloadModel() override;
	KDevelop::Path projectFile() const override;
	KSharedConfigPtr projectConfiguration() const override;

	void addToFileSet(KDevelop::ProjectFileItem* file) override;
	void removeFromFileSet(KDevelop::ProjectFileItem* file) override;
	QSet<KDevelop::IndexedString> fileSet() const override;

	bool isReady() const override;

	KDevelop::Path path() const override;

	Q_SCRIPTABLE QString name() const override
	{ return m_name; }
public Q_SLOTS:
	void close() override;

	KDevelop::IProjectFileManager* projectFileManager() const override;
	KDevelop::IBuildSystemManager* buildSystemManager() const override;
	KDevelop::IPlugin* versionControlPlugin() const override
	{ return nullptr; }

	KDevelop::IPlugin* managerPlugin() const override
	{ return m_manager; }

	KDevelop::ProjectFolderItem* projectItem() const override;
	bool inProject(const KDevelop::IndexedString &url) const override;

	void setReloadJob(KJob* job) override;
private:
	KDevelop::Path m_projectFilePath;
	KDevelop::Path m_buildPath;
	KDevelop::Path m_developerFilePath;
	KDevelop::Path m_projectPath;

	KDevelop::IPlugin* m_manager;

	QTemporaryFile m_projectTempFile;
	QString m_developerTempFilePath;

	KSharedConfigPtr m_cfg;

	QSet<KDevelop::IndexedString> m_fileSet;

	KDevelop::ProjectFolderItem* m_topItem;

	QString m_name;
};

#endif
