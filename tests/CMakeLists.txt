cmake_minimum_required(VERSION 3.22)

find_package(Qt6 REQUIRED COMPONENTS Test Sql Core)

# Shared implementation for unit tests (avoids multiple QTEST_MAIN in one binary).
add_library(offline-edm-test-support STATIC
    ../src/security/KeyManager.cpp
    ../src/security/EncryptionService.cpp
    ../src/storage/SqliteDocumentRepository.cpp
    ../src/storage/AcademicRepository.cpp
    ../src/sync/SqliteSyncQueue.cpp
    ../src/sync/RemoteSyncGateway.cpp
    ../src/portal/NovsuHtmlParser.cpp
    ../src/portal/PortalTypes.cpp
    ../src/auth/AuthController.cpp
)

target_include_directories(offline-edm-test-support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../src)

set_target_properties(offline-edm-test-support PROPERTIES
    AUTOMOC ON
    AUTORCC OFF
    AUTOUIC OFF
)

target_link_libraries(offline-edm-test-support PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Network
)

function(offline_edm_add_qtest target_name source_file)
    add_executable(${target_name} ${source_file})
    set_target_properties(${target_name} PROPERTIES AUTOMOC ON)
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    target_link_libraries(${target_name} PRIVATE
        offline-edm-test-support
        Qt6::Core
        Qt6::Test
        Qt6::Sql
    )
    add_test(NAME ${target_name} COMMAND ${target_name})
    
    # Add Qt bin directory to PATH for Windows
    if(WIN32)
        get_target_property(_qt_core_location Qt6::Core LOCATION)
        get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
        set_tests_properties(${target_name} PROPERTIES
            ENVIRONMENT "PATH=${_qt_bin_dir};$ENV{PATH}"
        )
    endif()
endfunction()

offline_edm_add_qtest(offline-edm-test-encryption TestEncryption.cpp)
offline_edm_add_qtest(offline-edm-test-repository TestRepository.cpp)
offline_edm_add_qtest(offline-edm-test-syncqueue TestSyncQueue.cpp)
offline_edm_add_qtest(offline-edm-test-htmlparser TestHtmlParser.cpp)
offline_edm_add_qtest(offline-edm-test-academic TestAcademicRepository.cpp)
offline_edm_add_qtest(offline-edm-test-auth TestAuthentication.cpp)
