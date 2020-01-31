#include "ViewEventFilters.h"
#include <QEvent>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QDebug>

namespace sg {
	bool GraphicsViewMouseEventFilter::eventFilter(QObject *object, QEvent *e) {

		QGraphicsView* view = static_cast<QGraphicsView*>(object);

		switch (e->type()) {
			case QEvent::MouseButtonPress: {
				QMouseEvent *me = static_cast<QMouseEvent*>(e);

				switch (me->button()) {
					case Qt::LeftButton: qDebug() << "rubber band!"; view->setDragMode(QGraphicsView::RubberBandDrag); break;
					case Qt::MiddleButton: qDebug() << "scroll hand!"; view->setDragMode(QGraphicsView::ScrollHandDrag); break;
					default: view->setDragMode(QGraphicsView::NoDrag); break;
				}

			} break;
			case QEvent::MouseButtonRelease: {
				view->setDragMode(QGraphicsView::NoDrag);
			} break;
		}

		return false;
	}
}