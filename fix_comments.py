import sys
import re

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

lines = text.splitlines()

for i in range(len(lines)):
    # Fix the missing newlines in comments
    if 'auto *loading = new LoadingDialog' in lines[i] and '//' in lines[i]:
        parts = lines[i].split('auto *loading')
        lines[i] = parts[0] + '\n        auto *loading' + parts[1]
    if 'car["updateTime"] = QDateTime' in lines[i] and '//' in lines[i]:
        parts = lines[i].split('car["updateTime"]')
        lines[i] = parts[0] + '\n            car["updateTime"]' + parts[1]
    if 'm_relocBtn->setStyleSheet(kToolBtnPrimary)' in lines[i]:
        # wait, why did it complain about kToolBtnPrimary?
        pass

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'wb') as f:
    f.write('\n'.join(lines).encode('utf-8'))
