import sys

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.h', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

start = text.find('class MapCanvas : public QGraphicsView')
if start != -1:
    end = text.find('};', start) + 2
    text = text[:start] + text[end:]
    if '#include "MapCanvas.h"' not in text:
        text = '#include "MapCanvas.h"\n' + text
    
    with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.h', 'wb') as f:
        f.write(text.encode('utf-8'))
    print('REMOVED MapCanvas from MapViewWidget.h')

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'rb') as f:
    text2 = f.read().decode('utf-8', 'ignore')

start = text2.find('MapCanvas::MapCanvas(')
if start != -1:
    start = text2.rfind('// ── Map canvas', 0, start)
    end = text2.find('// ── Map view widget', start)
    if start != -1 and end != -1:
        text2 = text2[:start] + text2[end:]
        with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/MapViewWidget.cpp', 'wb') as f:
            f.write(text2.encode('utf-8'))
        print('REMOVED MapCanvas from MapViewWidget.cpp')
