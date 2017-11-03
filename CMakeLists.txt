
cmake_minimum_required(VERSION 3.0)

project(kdev-catkin)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_PATCH 1)

# KDevplatform dependency version
set(KDEVPLATFORM_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

find_package (ECM "5.14.0" REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMAddTests)
include(ECMQtDeclareLoggingCategory)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(FeatureSummary)

find_package(Qt5 REQUIRED Core Widgets Test)
find_package(KF5 REQUIRED COMPONENTS IconThemes ItemModels ThreadWeaver TextEditor I18n)
find_package(KDevPlatform ${KDEVPLATFORM_VERSION} REQUIRED)

find_package(Boost REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${Boost_INCLUDE_DIRS}
)

#ecm_qt_declare_logging_category(kdevclangtidy_LOG_SRCS
#	HEADER debug.h
#	IDENTIFIER KDEV_CLANGTIDY
#	CATEGORY_NAME "kdevelop.analyzers.clangtidy"
#)

set(kdevcatkin_PART_SRCS
    src/catkinmanager.cpp

#     ${kdevclangtidy_LOG_SRCS}
)

kdevplatform_add_plugin(kdevcatkin
	JSON kdevcatkin.json
	SOURCES ${kdevcatkin_PART_SRCS}
)
target_link_libraries(kdevcatkin
	KDev::Interfaces
	KDev::Project
	KDev::Language
	KDev::OutputView
	KDev::Util
	KDev::Shell
	KF5::ConfigCore
	KF5::IconThemes
	KF5::KIOCore
	KF5::WidgetsAddons
	KF5::TextEditor
	Qt5::Network
)
