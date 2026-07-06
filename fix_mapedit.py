import sys
import re

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapEditWidget.cpp', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

# 1. remove Wheel event completely
text = re.sub(r'\s*if\s*\(\s*event->type\(\)\s*==\s*QEvent::Wheel\s*\)\s*\{[^{}]*\{[^{}]*\}[^{}]*\{[^{}]*\}[^{}]*\}', '', text, flags=re.DOTALL)

# 2. remove m_isRotatingMap handling from MouseMove
text = re.sub(r'\s*\} else if \(m_isRotatingMap\) \{.*?return true;\s*// consume.*?\} else \{', '} else {', text, flags=re.DOTALL)

# 3. remove Ctrl+Left click start map rotation drag
text = re.sub(r'\s*// Ctrl \+ Left click: start map rotation drag\s*if\s*\(me->button\(\)\s*==\s*Qt::LeftButton\s*&&\s*\(me->modifiers\(\)\s*&\s*Qt::ControlModifier\)\)\s*\{.*?\n\s*\}', '', text, flags=re.DOTALL)

# 4. remove MouseButtonRelease m_isRotatingMap
text = re.sub(r'\s*if\s*\(me->button\(\)\s*==\s*Qt::LeftButton\s*&&\s*m_isRotatingMap\)\s*\{.*?\n\s*\}', '', text, flags=re.DOTALL)

# 5. fix setWorldInfo missing in MapEditWidget.cpp
# m_mapView->setWorldInfo(m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
# wait, MapCanvas uses setWorldInfo, I just need to make sure MapEditWidget includes MapCanvas and uses m_mapView correctly.
# Oh, it complains because it doesn't know setWorldInfo if m_mapView is QGraphicsView!
# Let me write out the fixed file.
with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapEditWidget.cpp', 'wb') as f:
    f.write(text.encode('utf-8'))
