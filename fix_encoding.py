import sys
import re

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

# The text contains some mangled characters because a previous script saved it as utf-8, but it was originally utf-8, wait, no.
# Let's just fix the specific lines by finding them and replacing them.

lines = text.splitlines()

for i in range(len(lines)):
    if 'QInputDialog::getText' in lines[i] and 'this,' in lines[i]:
        lines[i] = '    QString name = QInputDialog::getText(this, "重命名地图", "新名称",'
    if 'msgBox.setText(QString("确认删除地图' in lines[i]:
        lines[i] = '    msgBox.setText(QString("确认删除地图「%1」？\\n此操作不可撤销").arg(m_mapName));'
    if '此操作不可撤销' in lines[i] and not 'msgBox.setText' in lines[i]:
        lines[i] = '' # Remove the wrapped line if any
    
    # Also fix any dangling m_mapBg = m_robotItem = m_routeLayer = m_axesLayer = nullptr;
    if 'm_mapBg =' in lines[i] and 'm_robotItem =' in lines[i]:
        lines[i] = '            m_mapView->clearMap();'

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'wb') as f:
    f.write('\n'.join(lines).encode('utf-8'))
