#-------------------------------------------------
#
# Project     : IntelligentAGVSchedulingSystem (QT-AGV)
# Target      : QT-AGV
# Framework   : Qt 6 (Widgets, Network, Mqtt, Multimedia, Svg, WebSockets)
# C++ Standard: C++17
# Architecture: Four-Layer Decoupled
#               1. GUILayer (界面视图、自定义控件与主窗口类)
#               2. AGVModel (AGV 实体模型与运动底盘类)
#               3. DataComm (底层数据、网络通信与配置工具类)
#               4. CoreTopology (拓扑路网、任务调度与算法校验类)
#
#-------------------------------------------------

QT += core gui widgets network mqtt multimedia svg websockets

CONFIG += c++17
CONFIG += console
CONFIG -= app_bundle

TARGET = QT-AGV
TEMPLATE = app

# Windows application icon and manifest
win32 {
    RC_FILE = src/resources/app.rc
}

# Qt Resources
RESOURCES += \
    src/resources/resources.qrc

# Include directories (identical search paths for stable #include references)
INCLUDEPATH += \
    $$PWD/src \
    $$PWD/src/app \
    $$PWD/src/network \
    $$PWD/src/core \
    $$PWD/src/utils \
    $$PWD/src/ui/mainwindow \
    $$PWD/src/ui/common \
    $$PWD/src/ui/dialogs \
    $$PWD/src/ui/map \
    $$PWD/src/ui/task \
    $$PWD/src/ui/control \
    $$PWD/src/widgets \
    $$PWD/src/styles

# ==============================================================================
# 1. GUILayer (界面视图、自定义控件与主窗口类)
# ==============================================================================
HEADERS += \
    src/ui/mainwindow/MainWindow.h \
    src/ui/mainwindow/MainWindowPageManager.h \
    src/ui/mainwindow/MainWindowStatusController.h \
    src/styles/MainWindowStyle.h \
    src/ui/common/ButtonClickEffect.h \
    src/ui/common/LoadingDialog.h \
    src/ui/common/SpinBoxWheelFilter.h \
    src/ui/common/StyleLoader.h \
    src/ui/common/ThemeColors.h \
    src/ui/common/Toast.h \
    src/ui/common/ToggleSwitch.h \
    src/ui/common/UiHelper.h \
    src/ui/control/CarWidget.h \
    src/ui/control/CarDialogHelper.h \
    src/ui/control/CarTableHelper.h \
    src/ui/control/ConfigDefaultCompleter.h \
    src/ui/control/ConfigFieldDictionary.h \
    src/ui/control/ConfigMetadataHelper.h \
    src/ui/control/DirectionPadWidget.h \
    src/ui/control/DirectionPadGeometry.h \
    src/ui/control/EditConfigDataLoader.h \
    src/ui/control/EditConfigParamEditor.h \
    src/ui/control/EditConfigRouteSync.h \
    src/ui/control/EditConfigStyle.h \
    src/ui/control/EditConfigTableBuilder.h \
    src/ui/control/EditConfigWidget.h \
    src/ui/dialogs/AudioManagerController.h \
    src/ui/dialogs/AudioManagerDialog.h \
    src/styles/AudioManagerStyle.h \
    src/styles/DeviceManagerStyle.h \
    src/ui/dialogs/BaseDialog.h \
    src/ui/dialogs/DeviceCard.h \
    src/ui/dialogs/DeviceCardWidgets.h \
    src/ui/dialogs/DeviceCardLayout.h \
    src/ui/dialogs/DeviceCardDisplay.h \
    src/ui/dialogs/DeviceCardProbe.h \
    src/ui/dialogs/DeviceManagerAddPanel.h \
    src/ui/dialogs/DeviceManagerConnectionHandler.h \
    src/ui/dialogs/DeviceManagerDialog.h \
    src/ui/dialogs/ManualControlConfigDialog.h \
    src/ui/dialogs/MapCheckResultDialog.h \
    src/ui/dialogs/MapUploadDialog.h \
    src/ui/dialogs/MapUploadHandler.h \
    src/styles/MapUploadStyle.h \
    src/ui/dialogs/MotionDialog.h \
    src/ui/dialogs/MotionDialogController.h \
    src/styles/MotionDialogStyle.h \
    src/ui/dialogs/MotionDialogTabs.h \
    src/ui/dialogs/NetworkConfigApplier.h \
    src/ui/dialogs/NetworkConfigDialog.h \
    src/ui/dialogs/NetworkConfigReader.h \
    src/ui/dialogs/NetworkConfigSshRunner.h \
    src/styles/NetworkConfigStyle.h \
    src/ui/dialogs/RobotStatusBasicInfoPanel.h \
    src/ui/dialogs/RobotStatusDialog.h \
    src/ui/dialogs/RobotStatusNetworkProbe.h \
    src/styles/RobotStatusStyle.h \
    src/ui/dialogs/RobotStatusTelemetrySync.h \
    src/ui/dialogs/RobotStatusUiFactory.h \
    src/ui/dialogs/SmapUploadDialog.h \
    src/ui/map/CreateMapApiHelper.h \
    src/ui/map/CreateMapCanvasWidget.h \
    src/ui/map/CreateMapTeleopPanel.h \
    src/ui/map/CreateMapWorkflowController.h \
    src/ui/map/CreateMapWidget.h \
    src/ui/map/MapAreaPropsPanel.h \
    src/ui/map/MapCanvas.h \
    src/ui/map/MapCanvasRobotTracker.h \
    src/ui/map/MapCurveHandleManager.h \
    src/ui/map/MapDataSerializer.h \
    src/ui/map/MapDragController.h \
    src/ui/map/MapDrawController.h \
    src/ui/map/MapDrawHelper.h \
    src/ui/map/MapEditDataLoader.h \
    src/ui/map/MapEditDataModel.h \
    src/ui/map/MapEditDialogs.h \
    src/ui/map/MapEditHelpDialog.h \
    src/ui/map/MapEditKeyHandler.h \
    src/ui/map/MapEditLayoutHelper.h \
    src/ui/map/MapEditTelemetryController.h \
    src/ui/map/MapEditToolBarHelper.h \
    src/ui/map/MapEditTypes.h \
    src/ui/map/MapEditWidget.h \
    src/styles/MapEditStyle.h \
    src/styles/MapViewStyle.h \
    src/styles/CreateMapStyle.h \
    src/ui/map/MapFeatureFactory.h \
    src/ui/map/MapFeatureTablePanel.h \
    src/ui/map/MapImageEditWidget.h \
    src/ui/map/MapImageEditIcons.h \
    src/ui/map/MapInteractionHandler.h \
    src/ui/map/MapLinePropsPanel.h \
    src/ui/map/MapMotionFormGroup.h \
    src/ui/map/MapPaintCanvas.h \
    src/ui/map/MapPropRowHelper.h \
    src/ui/map/MapPropsManager.h \
    src/ui/map/MapRasterPaintController.h \
    src/ui/map/MapSceneRenderer.h \
    src/ui/map/MapSmapImporter.h \
    src/ui/map/MapSnapHelper.h \
    src/ui/map/MapStationPropsPanel.h \
    src/ui/map/MapViewCanvasHandler.h \
    src/ui/map/MapViewDataLoader.h \
    src/ui/map/MapViewDevicePropsPanel.h \
    src/ui/map/MapViewDialogHelper.h \
    src/ui/map/MapViewIcons.h \
    src/ui/map/MapViewLayoutHelper.h \
    src/ui/map/MapViewMapPropsPanel.h \
    src/ui/map/MapViewMarkHelper.h \
    src/ui/map/MapViewNavInteraction.h \
    src/ui/map/MapViewRelocController.h \
    src/ui/map/MapViewRightPanel.h \
    src/ui/map/MapViewRobotMarker.h \
    src/ui/map/MapViewRoutePropsPanel.h \
    src/ui/map/MapViewSceneRenderer.h \
    src/ui/map/MapViewStationPropsPanel.h \
    src/ui/map/MapViewSwitchMapDialog.h \
    src/ui/map/MapViewTelemetryController.h \
    src/ui/map/MapViewToolbarHelper.h \
    src/ui/map/MapViewWidget.h \
    src/ui/map/MapWidget.h \
    src/ui/map/MapWidgetDialogs.h \
    src/ui/map/RulerWidget.h \
    src/ui/task/TaskChainWidget.h \
    src/ui/task/TaskChainDragWidgets.h \
    src/ui/task/TaskChainActionDefs.h \
    src/ui/task/TaskActionParamDialog.h \
    src/ui/task/TaskChainJsonSerializer.h \
    src/ui/task/TaskChainGroupCardWidget.h \
    src/ui/task/TaskChainRowHelper.h \
    src/ui/task/TaskChainRuntimeController.h \
    src/ui/task/TaskChainPanelsBuilder.h \
    src/ui/task/TaskChainActionExecutor.h \
    src/ui/task/TaskChainClipboardHelper.h \
    src/ui/task/TaskChainDataSync.h \
    src/ui/task/TaskChainListSync.h \
    src/ui/task/TaskChainCrudHandler.h \
    src/ui/task/TaskChainGroupActionHandler.h \
    src/ui/task/TaskLogWidget.h \
    src/styles/TaskChainStyle.h \
    src/styles/TaskLogStyle.h \
    src/styles/EditConfigStyle.h


SOURCES += \
    src/main.cpp \
    src/ui/mainwindow/MainWindow.cpp \
    src/ui/mainwindow/MainWindowPageManager.cpp \
    src/ui/mainwindow/MainWindowStatusController.cpp \
    src/ui/common/ButtonClickEffect.cpp \
    src/ui/common/LoadingDialog.cpp \
    src/ui/common/SpinBoxWheelFilter.cpp \
    src/ui/common/StyleLoader.cpp \
    src/ui/common/Toast.cpp \
    src/ui/common/ToggleSwitch.cpp \
    src/ui/common/UiHelper.cpp \
    src/ui/control/CarWidget.cpp \
    src/ui/control/CarDialogHelper.cpp \
    src/ui/control/CarTableHelper.cpp \
    src/ui/control/ConfigDefaultCompleter.cpp \
    src/ui/control/ConfigFieldDictionary.cpp \
    src/ui/control/ConfigMetadataHelper.cpp \
    src/ui/control/DirectionPadWidget.cpp \
    src/ui/control/DirectionPadGeometry.cpp \
    src/ui/control/EditConfigDataLoader.cpp \
    src/ui/control/EditConfigParamEditor.cpp \
    src/ui/control/EditConfigRouteSync.cpp \
    src/ui/control/EditConfigTableBuilder.cpp \
    src/ui/control/EditConfigWidget.cpp \
    src/ui/dialogs/AudioManagerController.cpp \
    src/ui/dialogs/AudioManagerDialog.cpp \
    src/ui/dialogs/BaseDialog.cpp \
    src/ui/dialogs/DeviceCard.cpp \
    src/ui/dialogs/DeviceCardWidgets.cpp \
    src/ui/dialogs/DeviceCardLayout.cpp \
    src/ui/dialogs/DeviceCardDisplay.cpp \
    src/ui/dialogs/DeviceCardProbe.cpp \
    src/ui/dialogs/DeviceManagerAddPanel.cpp \
    src/ui/dialogs/DeviceManagerConnectionHandler.cpp \
    src/ui/dialogs/DeviceManagerDialog.cpp \
    src/ui/dialogs/ManualControlConfigDialog.cpp \
    src/ui/dialogs/MapCheckResultDialog.cpp \
    src/ui/dialogs/MapUploadDialog.cpp \
    src/ui/dialogs/MapUploadHandler.cpp \
    src/ui/dialogs/MotionDialog.cpp \
    src/ui/dialogs/MotionDialogController.cpp \
    src/ui/dialogs/MotionDialogTabs.cpp \
    src/ui/dialogs/NetworkConfigApplier.cpp \
    src/ui/dialogs/NetworkConfigDialog.cpp \
    src/ui/dialogs/NetworkConfigReader.cpp \
    src/ui/dialogs/NetworkConfigSshRunner.cpp \
    src/ui/dialogs/RobotStatusBasicInfoPanel.cpp \
    src/ui/dialogs/RobotStatusDialog.cpp \
    src/ui/dialogs/RobotStatusNetworkProbe.cpp \
    src/ui/dialogs/RobotStatusTelemetrySync.cpp \
    src/ui/dialogs/RobotStatusUiFactory.cpp \
    src/ui/dialogs/SmapUploadDialog.cpp \
    src/ui/map/CreateMapApiHelper.cpp \
    src/ui/map/CreateMapCanvasWidget.cpp \
    src/ui/map/CreateMapTeleopPanel.cpp \
    src/ui/map/CreateMapWorkflowController.cpp \
    src/ui/map/CreateMapWidget.cpp \
    src/ui/map/MapAreaPropsPanel.cpp \
    src/ui/map/MapCanvas.cpp \
    src/ui/map/MapCanvasRobotTracker.cpp \
    src/ui/map/MapCurveHandleManager.cpp \
    src/ui/map/MapDataSerializer.cpp \
    src/ui/map/MapDragController.cpp \
    src/ui/map/MapDrawController.cpp \
    src/ui/map/MapDrawHelper.cpp \
    src/ui/map/MapEditDataLoader.cpp \
    src/ui/map/MapEditDataModel.cpp \
    src/ui/map/MapEditDialogs.cpp \
    src/ui/map/MapEditHelpDialog.cpp \
    src/ui/map/MapEditKeyHandler.cpp \
    src/ui/map/MapEditLayoutHelper.cpp \
    src/ui/map/MapEditTelemetryController.cpp \
    src/ui/map/MapEditToolBarHelper.cpp \
    src/ui/map/MapEditWidget.cpp \
    src/styles/MapEditStyle.cpp \
    src/ui/map/MapFeatureFactory.cpp \
    src/ui/map/MapFeatureTablePanel.cpp \
    src/ui/map/MapImageEditWidget.cpp \
    src/ui/map/MapImageEditIcons.cpp \
    src/ui/map/MapInteractionHandler.cpp \
    src/ui/map/MapLinePropsPanel.cpp \
    src/ui/map/MapMotionFormGroup.cpp \
    src/ui/map/MapPaintCanvas.cpp \
    src/ui/map/MapPropsManager.cpp \
    src/ui/map/MapRasterPaintController.cpp \
    src/ui/map/MapSceneRenderer.cpp \
    src/ui/map/MapSmapImporter.cpp \
    src/ui/map/MapSnapHelper.cpp \
    src/ui/map/MapStationPropsPanel.cpp \
    src/ui/map/MapViewCanvasHandler.cpp \
    src/ui/map/MapViewDataLoader.cpp \
    src/ui/map/MapViewDevicePropsPanel.cpp \
    src/ui/map/MapViewDialogHelper.cpp \
    src/ui/map/MapViewIcons.cpp \
    src/ui/map/MapViewLayoutHelper.cpp \
    src/ui/map/MapViewMapPropsPanel.cpp \
    src/ui/map/MapViewMarkHelper.cpp \
    src/ui/map/MapViewNavInteraction.cpp \
    src/ui/map/MapViewRelocController.cpp \
    src/ui/map/MapViewRightPanel.cpp \
    src/ui/map/MapViewRobotMarker.cpp \
    src/ui/map/MapViewRoutePropsPanel.cpp \
    src/ui/map/MapViewSceneRenderer.cpp \
    src/ui/map/MapViewStationPropsPanel.cpp \
    src/ui/map/MapViewSwitchMapDialog.cpp \
    src/ui/map/MapViewTelemetryController.cpp \
    src/ui/map/MapViewToolbarHelper.cpp \
    src/ui/map/MapViewWidget.cpp \
    src/ui/map/MapWidget.cpp \
    src/ui/map/MapWidgetDialogs.cpp \
    src/ui/map/RulerWidget.cpp \
    src/ui/task/TaskChainWidget.cpp \
    src/ui/task/TaskChainDragWidgets.cpp \
    src/ui/task/TaskChainActionDefs.cpp \
    src/ui/task/TaskActionParamDialog.cpp \
    src/ui/task/TaskChainJsonSerializer.cpp \
    src/ui/task/TaskChainGroupCardWidget.cpp \
    src/ui/task/TaskChainRowHelper.cpp \
    src/ui/task/TaskChainRuntimeController.cpp \
    src/ui/task/TaskChainPanelsBuilder.cpp \
    src/ui/task/TaskChainActionExecutor.cpp \
    src/ui/task/TaskChainClipboardHelper.cpp \
    src/ui/task/TaskChainDataSync.cpp \
    src/ui/task/TaskChainListSync.cpp \
    src/ui/task/TaskChainCrudHandler.cpp \
    src/ui/task/TaskChainGroupActionHandler.cpp \
    src/ui/task/TaskLogWidget.cpp

# ==============================================================================
# 2. AGVModel (AGV 实体模型与运动底盘类)
# ==============================================================================
HEADERS += \
    src/core/RobotInfoManager.h \
    src/core/RobotInfoParser.h \
    src/core/TeleopController.h \
    src/core/TeleopInputHandler.h \
    src/core/MotionConfigManager.h \
    src/core/MotionClampHelper.h \
    src/core/MotionConfigSerializer.h \
    src/core/MotionMetadataParser.h \
    src/core/AlignConfigManager.h

SOURCES += \
    src/core/RobotInfoManager.cpp \
    src/core/RobotInfoParser.cpp \
    src/core/TeleopController.cpp \
    src/core/TeleopInputHandler.cpp \
    src/core/MotionConfigManager.cpp \
    src/core/MotionClampHelper.cpp \
    src/core/MotionConfigSerializer.cpp \
    src/core/MotionMetadataParser.cpp \
    src/core/AlignConfigManager.cpp

# ==============================================================================
# 3. DataComm (底层数据、网络通信与配置工具类)
# ==============================================================================
HEADERS += \
    src/network/ApiService.h \
    src/network/MqttService.h \
    src/network/MqttTopicRouter.h \
    src/network/NetworkConstants.h \
    src/network/RouteGeoJsonCodec.h \
    src/network/RouteNetworkApi.h \
    src/network/RouteSmapConverter.h \
    src/app/AppConfig.h \
    src/app/AppConfigProfileSerializer.h \
    src/app/AppConstants.h \
    src/utils/JsonUtil.h

SOURCES += \
    src/network/ApiServiceCore.cpp \
    src/network/ApiServiceFile.cpp \
    src/network/ApiServiceDevice.cpp \
    src/network/ApiServiceMap.cpp \
    src/network/ApiServiceTask.cpp \
    src/network/ApiServiceControl.cpp \
    src/network/MqttService.cpp \
    src/network/MqttTopicRouter.cpp \
    src/network/RouteGeoJsonGenerator.cpp \
    src/network/RouteGeoJsonParser.cpp \
    src/network/RouteNetworkApi.cpp \
    src/network/RoutePoint.cpp \
    src/network/RouteSegment.cpp \
    src/network/RouteSmapConverter.cpp \
    src/app/AppConfig.cpp \
    src/app/AppConfigProfileSerializer.cpp \
    src/utils/JsonUtil.cpp

# ==============================================================================
# 4. CoreTopology (拓扑路网、任务调度与算法校验类)
# ==============================================================================
HEADERS += \
    src/core/TaskChainManager.h \
    src/core/TaskChainSerializer.h \
    src/core/RouteFeatureSelect.h \
    src/core/RouteGeoJsonBuilder.h \
    src/core/RouteNetworkValidator.h \
    src/core/RouteObstacleSync.h \
    src/utils/MapPgmCodec.h \
    src/utils/MapPgmUtil.h \
    src/utils/MapSmapParser.h \
    src/utils/MapTopologyUtil.h \
    src/utils/ZipUtil.h

SOURCES += \
    src/core/TaskChainManager.cpp \
    src/core/TaskChainSerializer.cpp \
    src/core/RouteFeatureSelect.cpp \
    src/core/RouteGeoJsonBuilder.cpp \
    src/core/RouteNetworkValidator.cpp \
    src/core/RouteObstacleSync.cpp \
    src/utils/MapPgmCodec.cpp \
    src/utils/MapPgmUtil.cpp \
    src/utils/MapSmapParser.cpp \
    src/utils/MapTopologyUtil.cpp \
    src/utils/ZipUtil.cpp
