import sys
import re

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

replacement = '''void MapViewWidget::onDeleteMap()
{
    if (m_mapId.isEmpty()) return;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认删除");
    msgBox.setText(QString("确认删除地图「%1」？\\n此操作不可撤销").arg(m_mapName));
    QPushButton *yesBtn = msgBox.addButton("确定", QMessageBox::YesRole);
    msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.exec();
    if (msgBox.clickedButton() != yesBtn) return;

    auto *loading = new LoadingDialog("正在删除地图...", this);
    loading->show();
    ApiService::instance().delMap(m_mapId, [this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            m_mapId.clear(); m_mapName.clear();
            m_mapView->clearMap();
            m_mapNameLabel->setText("已删除");
            setStatus("地图已删除，请切换地图");
            Toast::success(this, "地图已删除");
        } else {
            QMessageBox::warning(this, "错误", "删除地图失败");
        }
    });
}'''

text = re.sub(r'void MapViewWidget::onDeleteMap\(\).*', replacement, text, flags=re.DOTALL)

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'wb') as f:
    f.write(text.encode('utf-8'))
