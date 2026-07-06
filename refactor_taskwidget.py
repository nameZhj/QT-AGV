import sys
import re

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/TaskWidget.h', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

# fix syntax error if it exists (missing newline before class TaskWidget)
text = re.sub(r'// .*?class TaskWidget', r'\nclass TaskWidget', text)

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/TaskWidget.h', 'wb') as f:
    f.write(text.encode('utf-8'))
