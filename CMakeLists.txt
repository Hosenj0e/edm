cmake_minimum_required(VERSION 3.22)

project(offline-edm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(OFFLINE_EDM_BUILD_TESTS "Build unit tests" OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Sql)

# Для отладки можно использовать WIN32_EXECUTABLE FALSE чтобы показывать консоль
# Для релиза установите TRUE чтобы скрыть консоль
option(OFFLINE_EDM_SHOW_CONSOLE "Show console window for debugging" ON)

if(WIN32 AND NOT OFFLINE_EDM_SHOW_CONSOLE)
    set(APP_WIN32_OPTION WIN32)
endif()

qt_add_executable(offline-edm ${APP_WIN32_OPTION}
    src/main.cpp
    src/auth/AuthController.cpp
    src/auth/AuthController.h
    src/app/AppController.cpp
    src/app/AppController.h
    src/domain/AcademicTypes.h
    src/storage/AcademicRepository.cpp
    src/storage/AcademicRepository.h
    src/portal/NovsuHtmlParser.cpp
    src/portal/NovsuHtmlParser.h
    src/portal/PortalImportService.cpp
    src/portal/PortalImportService.h
    src/portal/PortalSessionStore.cpp
    src/portal/PortalSessionStore.h
    src/portal/PortalTypes.h
    src/portal/PortalTypes.cpp
    src/ui/model/GroupListModel.cpp
    src/ui/model/GroupListModel.h
    src/ui/model/StudentListModel.cpp
    src/ui/model/StudentListModel.h
    src/ui/model/DisciplineListModel.cpp
    src/ui/model/DisciplineListModel.h
    src/ui/model/GradeSheetListModel.cpp
    src/ui/model/GradeSheetListModel.h
    resources.qrc
)

target_link_libraries(offline-edm PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Sql
)

if (OFFLINE_EDM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
    )
    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET offline-edm POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --qmldir "${CMAKE_SOURCE_DIR}/src/ui/qml"
                "$<TARGET_FILE:offline-edm>"
            COMMENT "Running windeployqt..."
        )
    endif()
endif()
