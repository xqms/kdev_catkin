// Build manager for catkin projects
// Author: Max Schwarz <max.schwarz@online.de>

#ifndef CMAKEBUILDMANAGER_H
#define CMAKEBUILDMANAGER_H

#include <project/interfaces/iprojectbuilder.h>

class CatkinBuildManager : public KDevelop::IProjectBuilder
{
public:
	virtual KJob* build(KDevelop::ProjectBaseItem *item) override
	{ return 0; }

	virtual KJob* clean(KDevelop::ProjectBaseItem *item) override
	{ return 0; }

	virtual KJob* install(KDevelop::ProjectBaseItem* item, const QUrl &specificPrefix = {}) override
	{ return 0; }
};

#endif

