import sys
import re

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.h', 'rb') as f:
    text = f.read().decode('utf-8')

text = text.replace('QGraphicsView  *m_mapView', 'MapCanvas      *m_canvas')

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.h', 'wb') as f:
    f.write(text.encode('utf-8'))

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'rb') as f:
    text = f.read().decode('utf-8')

# The QGraphicsView was already m_canvas in HEAD, so we just need to replace the constructor
text = text.replace('m_canvas = new QGraphicsView(this);', 'm_canvas = new MapCanvas(m_scene, this);')

# In MapViewWidget.cpp, m_canvas is ALREADY defined as MapCanvas* in HEAD, and the subclass MapCanvas was embedded.
# So I just need to remove the panning/zooming logic from MapViewWidget.cpp because MapCanvas does it.
# Actually, let's check if MapViewWidget has eventFilter or wheelEvent
text = re.sub(r'\s*if\s*\(\s*event->type\(\)\s*==\s*QEvent::Wheel\s*\)\s*\{[^{}]*\{[^{}]*\}[^{}]*\{[^{}]*\}[^{}]*\}', '', text, flags=re.DOTALL)
text = re.sub(r'\s*// Ctrl \+ Left click: start map rotation drag\s*if\s*\(me->button\(\)\s*==\s*Qt::LeftButton\s*&&\s*\(me->modifiers\(\)\s*&\s*Qt::ControlModifier\)\)\s*\{.*?\n\s*\}', '', text, flags=re.DOTALL)

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'wb') as f:
    f.write(text.encode('utf-8'))
