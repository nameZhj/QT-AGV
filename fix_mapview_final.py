import sys
import re

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

# Fix onRenameMap
rename_match = re.search(r'void MapViewWidget::onRenameMap\(\).*?(?=void MapViewWidget::onDeleteMap)', text, flags=re.DOTALL)
if rename_match:
    new_rename = '''void MapViewWidget::onRenameMap()
{
    if (m_mapId.isEmpty()) return;
    bool ok;
    QString name = QInputDialog::getText(this, "重命名地图", "新名称",
                                         QLineEdit::Normal, m_mapName, &ok);
    if (!ok || name.trimmed().isEmpty() || name == m_mapName) return;

    auto *loading = new LoadingDialog("正在重命名...", this);
    loading->show();
    QJsonObject req; req["mapId"] = m_mapId; req["mapName"] = name.trimmed();
    ApiService::instance().updateMap(req, [this, name, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            m_mapName = name.trimmed();
            m_mapNameLabel->setText(m_mapName);
            Toast::success(this, "重命名成功");
        } else {
            QMessageBox::warning(this, "错误", "重命名失败");
        }
    });
}

'''
    text = text[:rename_match.start()] + new_rename + text[rename_match.end():]

# Fix onDeleteMap
delete_match = re.search(r'void MapViewWidget::onDeleteMap\(\).*', text, flags=re.DOTALL)
if delete_match:
    new_delete = '''void MapViewWidget::onDeleteMap()
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
            m_canvas->clearMap();
            m_mapNameLabel->setText("已删除");
            setStatus("地图已删除，请切换地图");
            Toast::success(this, "地图已删除");
        } else {
            QMessageBox::warning(this, "错误", "删除地图失败");
        }
    });
}
'''
    text = text[:delete_match.start()] + new_delete + text[delete_match.end():]

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'wb') as f:
    f.write(text.encode('utf-8'))
