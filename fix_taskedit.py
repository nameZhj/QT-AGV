import sys
import re

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/TaskEditWidget.cpp', 'rb') as f:
    text = f.read().decode('utf-8', 'ignore')

# We know the eventFilter has issues from removing rotation logic. Let's just fix MouseButtonRelease and remove Wheel correctly.

# 1. replace wheel event
text = re.sub(r'\s*if\s*\(\s*event->type\(\)\s*==\s*QEvent::Wheel\s*\)\s*\{[^{}]*\{[^{}]*\}[^{}]*\{[^{}]*\}[^{}]*\}', '', text, flags=re.DOTALL)

# 2. remove m_isRotatingMap handling from MouseMove
text = re.sub(r'\s*\} else if \(m_isRotatingMap\) \{.*?return true;\s*// consume.*?\} else \{', '} else {', text, flags=re.DOTALL)

# 3. remove Ctrl+Left click start map rotation drag
text = re.sub(r'\s*// Ctrl \+ Left click: start map rotation drag\s*if\s*\(me->button\(\)\s*==\s*Qt::LeftButton\s*&&\s*\(me->modifiers\(\)\s*&\s*Qt::ControlModifier\)\)\s*\{.*?\n\s*\}', '', text, flags=re.DOTALL)

# 4. We will replace the entire MouseButtonRelease up to eventFilter end, just like MapEditWidget
release_replacement = '''    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_isDraggingHandle) {
            // Commit to JSON
            auto &lst = m_curveHandles[m_handleCurveId];
            int lastIdx = lst.size() - 1;
            for (int i = 0; i < m_curves.size(); ++i) {
                QJsonObject cv = m_curves[i].toObject();
                if (cv.value("id").toString() != m_handleCurveId) continue;

                QJsonArray controlPoints;
                for (int j = 0; j < lst.size(); ++j) {
                    QPointF w = sceneToWorld(lst[j]->pos().x(), lst[j]->pos().y());
                    QJsonObject pt;
                    pt["x"] = w.x(); pt["y"] = w.y();
                    controlPoints.append(pt);
                }
                cv["controlPoints"] = controlPoints;

                QPointF wStart = sceneToWorld(lst[0]->pos().x(), lst[0]->pos().y());
                QPointF wEnd = sceneToWorld(lst[lastIdx]->pos().x(), lst[lastIdx]->pos().y());
                cv["x1"] = wStart.x(); cv["y1"] = wStart.y();
                cv["x2"] = wEnd.x(); cv["y2"] = wEnd.y();

                if (m_handleIndex == 0 || m_handleIndex == lastIdx) {
                    SnapResult snap = findSnap(lst[m_handleIndex]->pos());
                    const QString &key = (m_handleIndex == 0) ? "startid" : "endid";
                    cv[key] = snap.valid ? snap.id : QString();
                }
                m_curves[i] = cv; break;
            }
            m_isDraggingHandle = false;
            m_activeHandle = nullptr;
            m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);
            m_mapView->viewport()->setCursor(Qt::OpenHandCursor);
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        if (m_drawStatus == Curve && m_curveDrawPoints.size() >= 2) {
            addCurve(m_curveDrawPoints, m_curveDrawPointIds);
        }
        cancelDraw();
        return true;
    }

    return QWidget::eventFilter(obj, event);
}
'''
text = re.sub(r'\s*if\s*\(\s*event->type\(\)\s*==\s*QEvent::MouseButtonRelease\s*\)\s*\{.*$', '\n' + release_replacement, text, flags=re.DOTALL)

with open('E:/code/QT/AGV_QT/QT-AGV/src/widgets/TaskEditWidget.cpp', 'wb') as f:
    f.write(text.encode('utf-8'))
