import sys
import re

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/TaskEditWidget.h', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

text = text.replace('#include <QGraphicsView>', '#include "MapCanvas.h"')
text = text.replace('QGraphicsView  *m_mapView      = nullptr;', 'MapCanvas      *m_mapView      = nullptr;')

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/TaskEditWidget.h', 'wb') as f:
    f.write(text.encode('utf-8'))

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/TaskEditWidget.cpp', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

text = text.replace('m_mapView = new QGraphicsView(this);', 'm_mapView = new MapCanvas(m_scene, this);')

text = re.sub(r'\s*if\s*\(\s*event->type\(\)\s*==\s*QEvent::Wheel\s*\)\s*\{[^{}]*\{[^{}]*\}[^{}]*\{[^{}]*\}[^{}]*\}', '', text, flags=re.DOTALL)
text = re.sub(r'\s*// Ctrl \+ Left click: start map rotation drag\s*if\s*\(me->button\(\)\s*==\s*Qt::LeftButton\s*&&\s*\(me->modifiers\(\)\s*&\s*Qt::ControlModifier\)\)\s*\{.*?\n\s*\}', '', text, flags=re.DOTALL)

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/TaskEditWidget.cpp', 'wb') as f:
    f.write(text.encode('utf-8'))
