set_project("AutoFlotationSheet")
set_version("0.1.0")
set_languages("c++20")
set_allowedplats("linux", "windows", "mingw")

add_rules("mode.debug", "mode.release")

-- Shared source sets keep partial tests and Qt MOC inputs in sync with the app.
local function add_application_sources()
    add_includedirs("src")
    add_files("src/**.cpp")
    add_files("src/editor/FlowsheetScene.h")
    add_files("src/document/FlowsheetDocument.h")
    add_files("src/ui/TerminalProductDock.h")
    add_files("src/ui/TerminalProductTableModel.h")
    add_files("src/annotations/AnnotationItem.h")
    add_files("src/annotations/ReagentAnnotationItem.h")
    add_files("src/annotations/AnnotationManager.h")
    add_files("src/ui/ObjectPropertiesDock.h")
    add_files("src/ui/CalculationIssuesDock.h")
    add_files("src/ui/FlowTemplateDock.h")
    add_files("src/ui/WorkspaceSidebar.h")
    add_files("src/ui/ResultMetricMenu.h")
    add_files("src/services/ProjectUndoManager.h")
    add_files("src/app/CalculationController.h")
    add_files("src/contracts/TerminalTableSession.h")
end

local function add_canvas_sources()
    add_files("src/editor/FlowsheetScene*.cpp", "src/editor/FlowsheetScene.h")
    add_files("src/editor/CrossingBridgeRenderer.cpp", "src/editor/FlowsheetRoutingCoordinator.cpp")
    add_files("src/graphics/items/FlotationUnitItem*.cpp", "src/graphics/symbols/*.cpp")
    add_files("src/graphics/items/InputLineItem.cpp", "src/graphics/items/ProductLineItem*.cpp")
    add_files("src/graphics/items/MergeJunctionItem*.cpp", "src/graphics/items/FeedJunctionItem*.cpp")
end

target("AutoFlotationSheet")
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtSvg")
    add_headerfiles("src/(**.h)")
    add_application_sources()
    set_targetdir("build/bin")
    if is_plat("linux") then
        -- Allow a staged Linux bundle to resolve libraries placed in ../lib.
        -- This does not interfere with distribution-provided Qt libraries.
        add_rpathdirs("$ORIGIN/../lib")
    end
    if is_mode("release") then
        set_optimize("fastest")
    end

target("connection_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets")
    add_includedirs("src")
    add_files("tests/cpp/ConnectionTest.cpp", "tests/cpp/CanvasHostChecks.cpp")
    add_canvas_sources()
    set_targetdir("build/tests")

target("merge_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtTest")
    add_includedirs("src")
    add_files("tests/cpp/MergeTest.cpp", "tests/cpp/MergeExtensionInteractionChecks.cpp")
    add_canvas_sources()
    set_targetdir("build/tests")

target("topology_test")
    set_default(false)
    add_rules("qt.console")
    add_frameworks("QtCore")
    add_includedirs("src")
    add_files("tests/cpp/TopologyTest.cpp")
    add_files("src/topology/**.cpp")
    set_targetdir("build/tests")

target("terminal_input_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets")
    add_includedirs("src")
    add_files("tests/cpp/TerminalInput*.cpp")
    add_files("src/app/TerminalTableController.cpp", "src/contracts/TerminalTableSession.h")
    add_files("src/adapters/CanvasTopologyBuilder.cpp")
    add_files("src/document/FlowsheetDocument*.cpp", "src/document/FlowsheetDocument.h")
    add_files("src/ui/TerminalProductTableModel*.cpp", "src/ui/TerminalProductTableModel.h")
    add_files("src/ui/TerminalProductDock*.cpp", "src/ui/TerminalProductDock.h")
    add_files("src/ui/TerminalProductViewSupport.cpp")
    add_files("src/ui/TerminalProductStyle.cpp", "src/ui/CalculationExplanationDialog.cpp")
    add_files("src/services/ThemeService.cpp")
    add_files("src/services/FlowsheetCalculationService.cpp")
    add_files("src/services/CalculationInputBuilder.cpp")
    add_files("src/annotations/**.cpp")
    add_files("src/annotations/AnnotationItem.h", "src/annotations/AnnotationManager.h")
    add_files("src/annotations/ReagentAnnotationItem.h")
    add_files("src/topology/**.cpp")
    add_canvas_sources()
    set_targetdir("build/tests")

target("stream_filter_interaction_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtSvg", "QtTest")
    add_files("tests/cpp/StreamFilterInteractionTest.cpp", "tests/cpp/ExcelExportInteractionChecks.cpp")
    add_application_sources()
    remove_files("src/main.cpp")
    set_targetdir("build/tests")

target("clipboard_interaction_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtSvg", "QtTest")
    add_files("tests/cpp/ClipboardInteractionTest.cpp", "tests/cpp/ReagentClipboardChecks.cpp",
              "tests/cpp/CopyMoveInteractionChecks.cpp", "tests/cpp/ReagentMenuInteractionChecks.cpp",
              "tests/cpp/ReagentMenuTargetChecks.cpp", "tests/cpp/ReagentShortcutChecks.cpp",
              "tests/cpp/ScenarioNoteInteractionChecks.cpp", "tests/cpp/GroupProductNameChecks.cpp")
    add_application_sources()
    remove_files("src/main.cpp")
    set_targetdir("build/tests")

target("result_source_interaction_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtSvg", "QtTest")
    add_files("tests/cpp/ResultSourceInteractionTest.cpp", "tests/cpp/AnnotationTextStyleDialogChecks.cpp")
    add_application_sources()
    remove_files("src/main.cpp")
    set_targetdir("build/tests")

target("ball_mill_interaction_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtSvg", "QtTest")
    add_files("tests/cpp/BallMillInteractionTest.cpp", "tests/cpp/ResizeInteractionChecks.cpp",
              "tests/cpp/CrossingBridgeChecks.cpp")
    add_application_sources()
    remove_files("src/main.cpp")
    set_targetdir("build/tests")

target("thickener_interaction_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtSvg", "QtTest")
    add_files("tests/cpp/ThickenerInteractionTest.cpp")
    add_application_sources()
    remove_files("src/main.cpp")
    set_targetdir("build/tests")

target("workspace_tools_interaction_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtSvg", "QtTest")
    add_files("tests/cpp/WorkspaceToolsInteractionTest.cpp",
              "tests/cpp/ObjectPropertyChecks.cpp", "tests/cpp/CalculationDiagnosticsChecks.cpp",
              "tests/cpp/CalculationControllerChecks.cpp",
              "tests/cpp/WorkspaceSidebarChecks.cpp", "tests/cpp/WorkspacePanelSelectorChecks.cpp",
              "tests/cpp/TerminalTableLayoutChecks.cpp",
              "tests/cpp/FlowTemplateChecks.cpp", "tests/cpp/TemplatePreviewChecks.cpp")
    add_application_sources()
    remove_files("src/main.cpp")
    set_targetdir("build/tests")

target("closed_loop_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets")
    add_includedirs("src")
    add_files("tests/cpp/ClosedLoopTest.cpp")
    add_files("src/adapters/CanvasTopologyBuilder.cpp")
    add_files("src/editor/CanvasActions.cpp")
    add_canvas_sources()
    add_files("src/topology/**.cpp")
    set_targetdir("build/tests")

target("canvas_view_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets")
    add_includedirs("src")
    add_files("tests/cpp/CanvasViewTest.cpp")
    add_files("src/editor/CanvasView.cpp")
    set_targetdir("build/tests")

target("export_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtSvg")
    add_includedirs("src")
    add_files("tests/cpp/ExportTest.cpp")
    add_files("src/services/RasterExporter.cpp")
    add_files("src/services/SvgExporter.cpp")
    set_targetdir("build/tests")

target("excel_export_test")
    set_default(false)
    add_rules("qt.console")
    add_frameworks("QtCore", "QtGui")
    add_includedirs("src")
    add_files("tests/cpp/ExcelExportTest.cpp")
    add_files("src/services/ExcelDataExporter.cpp", "src/services/ExcelWorksheetBuilder.cpp", "src/services/XlsxWorkbookWriter.cpp")
    add_files("src/document/FlowsheetDocument*.cpp", "src/document/FlowsheetDocument.h")
    add_files("src/topology/TopologyTypes.cpp", "src/topology/TopologyGraph.cpp")
    set_targetdir("build/tests")

target("project_io_test")
    set_default(false)
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets")
    add_includedirs("src")
    add_files("tests/cpp/ProjectIo*.cpp")
    add_files("src/annotations/AnnotationItem*.cpp", "src/annotations/AnnotationItem.h")
    add_files("src/services/ProjectSerializer.cpp")
    add_files("src/services/ProjectSceneRestorer.cpp")
    add_files("src/services/ProjectJsonWriter.cpp", "src/services/ProjectUnitKindCodec.cpp")
    add_files("src/services/ProjectJsonReader.cpp", "src/services/ProjectJsonReadSupport.cpp")
    add_files("src/services/ProjectGeometryReader.cpp", "src/services/ProjectDisplaySettingsReader.cpp")
    add_files("src/services/ProjectScenarioReader.cpp")
    add_files("src/services/ProjectUndoManager.cpp", "src/services/ProjectUndoManager.h")
    add_files("src/services/FlowGroup*.cpp", "src/services/FlowGroupClipboard.h")
    add_files("src/services/FlowsheetCalculationService.cpp")
    add_files("src/services/CalculationInputBuilder.cpp")
    add_files("src/adapters/CanvasTopologyBuilder.cpp")
    add_files("src/document/FlowsheetDocument*.cpp", "src/document/FlowsheetDocument.h")
    add_canvas_sources()
    add_files("src/topology/**.cpp")
    set_targetdir("build/tests")

-- A calculation must build and run without QtWidgets or any canvas source.
target("calculation_service_test")
    set_default(false)
    add_rules("qt.console")
    add_frameworks("QtCore", "QtGui")
    add_includedirs("src")
    add_files("tests/cpp/CalculationServiceTest.cpp")
    add_files("src/document/FlowsheetDocument*.cpp", "src/document/FlowsheetDocument.h")
    add_files("src/services/CalculationInputBuilder.cpp", "src/services/FlowsheetCalculationService.cpp")
    add_files("src/topology/**.cpp")
    set_targetdir("build/tests")

target("table_session_test")
    set_default(false)
    add_rules("qt.console")
    add_frameworks("QtCore", "QtGui")
    add_includedirs("src")
    add_files("tests/cpp/TableSessionTest.cpp")
    add_files("src/app/TerminalTableController.cpp", "src/contracts/TerminalTableSession.h")
    add_files("src/document/FlowsheetDocument*.cpp", "src/document/FlowsheetDocument.h")
    add_files("src/topology/TopologyTypes.cpp")
    set_targetdir("build/tests")

task("deploy")
    set_menu {
        usage = "xmake deploy",
        description = "Stage the application for the configured platform"
    }
    on_run(function ()
        import("core.project.config")
        import("core.project.project")
        config.load()
        local target = project.target("AutoFlotationSheet")
        assert(target, "AutoFlotationSheet target not found")
        assert(os.isfile(target:targetfile()), "application is not built; run xmake first")

        if is_plat("windows", "mingw") then
            local sdkdir = config.get("qt")
            assert(sdkdir, "Qt SDK is not configured; run xmake f --qt=... first")
            local deployqt = path.join(sdkdir, "bin", "windeployqt.exe")
            assert(os.isfile(deployqt), "windeployqt not found: " .. deployqt)
            os.vrunv(deployqt, {
                "--release",
                "--force",
                "--no-compiler-runtime",
                "--no-translations",
                target:targetfile()
            })
        elseif is_plat("linux") then
            -- Linux distributions normally provide Qt as system packages. Stage
            -- only the executable; packagers may add private libraries to lib/.
            local deploydir = path.join(os.projectdir(), "build", "deploy")
            local bindir = path.join(deploydir, "bin")
            os.mkdir(bindir)
            os.cp(target:targetfile(), bindir)
            cprint("${bright green}staged:${clear} %s", deploydir)
        else
            raise("deploy is not supported on platform: %s", get_config("plat"))
        end
    end)
