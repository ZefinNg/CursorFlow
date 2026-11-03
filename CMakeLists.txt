cmake_minimum_required(VERSION 3.16)

project(CursorFlow
    VERSION 0.1.0
    DESCRIPTION "CursorFlow - 轻量级鼠标键盘活动统计工具"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源码统一 UTF-8（中文注释），MSVC 默认按系统代码页解析会导致 C4819/C2447
if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.15.2 REQUIRED COMPONENTS Quick QuickControls2 Sql Test Widgets)

# ---- i18n（任务 5.1）：文案变更后运行该目标重新生成 ts + qm（qm 已入 qrc）----
find_package(Qt5 OPTIONAL_COMPONENTS LinguistTools)
if(TARGET Qt5::lupdate)
    add_custom_target(update_translations
        COMMAND Qt5::lupdate src qml -ts
                qml/i18n/CursorFlow_zh_CN.ts qml/i18n/CursorFlow_en.ts
        COMMAND Qt5::lrelease qml/i18n/CursorFlow_zh_CN.ts qml/i18n/CursorFlow_en.ts
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "更新翻译（lupdate + lrelease）"
    )
endif()

# ---- 核心库（UI 无关，主程序与测试共用） ----
add_library(cursorflow_core STATIC
    src/data/Database.h
    src/data/Database.cpp
    src/data/DataManager.h
    src/data/DataManager.cpp
    src/data/InputDelta.h
    src/capture/IPlatformHook.h
    src/capture/InputCaptureWorker.h
    src/capture/InputCaptureWorker.cpp
    src/app/PrivacyGate.h
    src/app/PrivacyGate.cpp
    src/app/StatsController.h
    src/app/StatsController.cpp
    src/app/AppSettings.h
    src/app/AppSettings.cpp
    src/app/HistoryController.h
    src/app/HistoryController.cpp
    src/app/Logger.h
    src/app/Logger.cpp
    src/app/Theme.h
    src/app/Theme.cpp
    src/app/FunFactsController.h
    src/app/FunFactsController.cpp
    src/app/AchievementController.h
    src/app/AchievementController.cpp
    src/export/Exporter.h
    src/export/Exporter.cpp
    src/export/ExporterController.h
    src/export/ExporterController.cpp
)
if(WIN32)
    target_sources(cursorflow_core PRIVATE
        src/capture/platform/WinHook.h
        src/capture/platform/WinHook.cpp
    )
elseif(APPLE)
    # macOS 采集（任务 4.5）：CGEventTap + 辅助功能权限查询
    target_sources(cursorflow_core PRIVATE
        src/capture/platform/MacEventTap.h
        src/capture/platform/MacEventTap.cpp
    )
    target_link_libraries(cursorflow_core PUBLIC
        "-framework CoreGraphics"
        "-framework ApplicationServices"
    )
elseif(UNIX)
    # Linux 采集（任务 4.6）：XInput2 raw events + Wayland 降级提示
    find_package(X11 REQUIRED)
    if(NOT X11_Xi_FOUND)
        message(FATAL_ERROR "XInput2 开发文件未找到：请安装 libxi-dev / libxi-devel")
    endif()
    target_sources(cursorflow_core PRIVATE
        src/capture/platform/X11InputHook.h
        src/capture/platform/X11InputHook.cpp
    )
    target_include_directories(cursorflow_core PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(cursorflow_core PUBLIC ${X11_X11_LIB} ${X11_Xi_LIB})
endif()
target_include_directories(cursorflow_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 编译期版本注入（任务 6.1，M4）：version.h 与 project(VERSION)/CPack 同源
set(CURSORFLOW_VERSION ${PROJECT_VERSION})
string(TIMESTAMP CURSORFLOW_BUILD_DATE "%Y-%m-%d")
set(CURSORFLOW_BUILD_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
configure_file(src/app/version.h.in version.h @ONLY)
target_include_directories(cursorflow_core PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(cursorflow_core PUBLIC Qt5::Sql Qt5::Widgets)

# ---- 主程序 ----
add_executable(CursorFlow WIN32 MACOSX_BUNDLE
    src/main.cpp
    qml/qml.qrc
)

# Windows 可执行文件图标（M4 增补）：资源管理器/任务栏图标
if(WIN32)
    target_sources(CursorFlow PRIVATE assets/image/app_icon.rc)
endif()

target_link_libraries(CursorFlow PRIVATE
    cursorflow_core
    Qt5::Quick
    Qt5::QuickControls2
    Qt5::Widgets
)

# ---- 单元测试 ----
enable_testing()
add_executable(tst_database tests/tst_Database.cpp)
target_link_libraries(tst_database PRIVATE cursorflow_core Qt5::Test)
add_test(NAME tst_database COMMAND tst_database)

add_executable(tst_datamanager tests/tst_DataManager.cpp)
target_link_libraries(tst_datamanager PRIVATE cursorflow_core Qt5::Test)
add_test(NAME tst_datamanager COMMAND tst_datamanager)

add_executable(tst_inputcapture tests/tst_InputCaptureWorker.cpp)
target_link_libraries(tst_inputcapture PRIVATE cursorflow_core Qt5::Test)
add_test(NAME tst_inputcapture COMMAND tst_inputcapture)

add_executable(tst_privacygate tests/tst_PrivacyGate.cpp)
target_link_libraries(tst_privacygate PRIVATE cursorflow_core Qt5::Test)
add_test(NAME tst_privacygate COMMAND tst_privacygate)

add_executable(tst_statscontroller tests/tst_StatsController.cpp)
target_link_libraries(tst_statscontroller PRIVATE cursorflow_core Qt5::Test)
add_test(NAME tst_statscontroller COMMAND tst_statscontroller)

add_executable(tst_appsettings tests/tst_AppSettings.cpp)
target_link_libraries(tst_appsettings PRIVATE cursorflow_core Qt5::Test)
add_test(NAME tst_appsettings COMMAND tst_appsettings)

add_executable(tst_logger tests/tst_Logger.cpp)
target_link_libraries(tst_logger PRIVATE cursorflow_core Qt5::Test)
add_test(NAME tst_logger COMMAND tst_logger)

add_executable(tst_historycontroller tests/tst_HistoryController.cpp)
target_link_libraries(tst_historycontroller PRIVATE cursorflow_core Qt5::Test)
add_test(NAME tst_historycontroller COMMAND tst_historycontroller)

add_executable(tst_exporter tests/tst_Exporter.cpp)
target_link_libraries(tst_exporter PRIVATE cursorflow_core Qt5::Test)
add_test(NAME tst_exporter COMMAND tst_exporter)

# 5.3 性能专项：图表渲染计时（软件后端，无 GPU 依赖）
add_executable(perf_chartrender tests/perf_ChartRender.cpp)
target_link_libraries(perf_chartrender PRIVATE cursorflow_core Qt5::Quick Qt5::QuickControls2 Qt5::Test)
add_test(NAME perf_chartrender COMMAND perf_chartrender)

if(WIN32)
    add_executable(tst_winhook tests/tst_WinHook.cpp)
    target_link_libraries(tst_winhook PRIVATE cursorflow_core Qt5::Test user32)
    add_test(NAME tst_winhook COMMAND tst_winhook)
endif()

# ---- 安装与打包（05 号文档 §4-5，任务 5.4）----
install(TARGETS CursorFlow
    RUNTIME DESTINATION .
    BUNDLE  DESTINATION .
)
if(WIN32)
    # 安装期调用 windeployqt 收集运行时依赖，再按需剪裁（05 号文档 §4 包体控制）：
    # - 应用未用 Network / VirtualKeyboard / bearer / qmltooling（调试插件）
    # - SQLite 之外的 SQL 驱动、图片编解码器仅保留 svg/ico/jpeg
    install(CODE "
        get_filename_component(QtBinDir \"\${Qt5_DIR}/../../../bin\" ABSOLUTE)
        find_program(WINDEPLOYQT_EXE windeployqt HINTS \"\${QtBinDir}\" REQUIRED)
        execute_process(
            COMMAND \"\${WINDEPLOYQT_EXE}\" --release --no-compiler-runtime
                    --no-opengl-sw --no-system-d3d-compiler --no-translations
                    --qmldir \"${CMAKE_CURRENT_SOURCE_DIR}/qml\"
                    --dir \"\${CMAKE_INSTALL_PREFIX}\"
                    \"\$<TARGET_FILE:CursorFlow>\"
            COMMAND_ECHO STDOUT
        )
        foreach(_dir virtualkeyboard bearer qmltooling QtQml/RemoteObjects)
            file(REMOVE_RECURSE \"\${CMAKE_INSTALL_PREFIX}/\${_dir}\")
        endforeach()
        # windeployqt 会连带部署未使用的 RemoteObjects（dumpbin 确认 exe 无此依赖）
        file(REMOVE \"\${CMAKE_INSTALL_PREFIX}/Qt5Network.dll\"
                    \"\${CMAKE_INSTALL_PREFIX}/Qt5RemoteObjects.dll\")
        file(GLOB _sqlDrivers \"\${CMAKE_INSTALL_PREFIX}/sqldrivers/*.dll\")
        foreach(_drv \${_sqlDrivers})
            if(NOT _drv MATCHES \"qsqlite\")
                file(REMOVE \"\${_drv}\")
            endif()
        endforeach()
        file(GLOB _imgPlugins \"\${CMAKE_INSTALL_PREFIX}/imageformats/*.dll\")
        foreach(_img \${_imgPlugins})
            if(NOT _img MATCHES \"qsvg|qico|qjpeg\")
                file(REMOVE \"\${_img}\")
            endif()
        endforeach()
    ")
endif()

set(CPACK_PACKAGE_NAME "CursorFlow")
set(CPACK_PACKAGE_VENDOR "CursorFlow")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CursorFlow - 轻量级鼠标键盘活动统计工具")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "CursorFlow-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-x64")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/LICENSE.txt")
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "CursorFlow.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/cursorflow/cursorflow")
    # 自启动由应用内设置写入注册表 Run 键，安装器不默认勾选（隐私优先）
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop") # DMG；签名与公证在 CI 后置（05 号文档 §5）
    set(CPACK_DMG_SLA_DIR ${CMAKE_SOURCE_DIR}/packaging/macos/sla)
else()
    # CPack 无原生 AppImage 生成器：Linux 使用 packaging/linux/build-appimage.sh
    # （linuxdeployqt 封装），此处以 TGZ 作为 CI 兜底产物
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
