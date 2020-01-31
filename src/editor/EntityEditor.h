#pragma once

#include <QWidget>
#include <QGraphicsView>

class QGraphicsView;
class QWheelEvent;
class QMouseEvent;
class QEvent;

namespace sg {

	class EntityEditor : public QWidget {
		Q_OBJECT

		QGraphicsView *const mGraphicsView;
		float mZoomLevel = 250;
		void setZoomLevel(int adjust);
		void setupMatrix();

	public:
		EntityEditor(class Controller& controller, QVariant component_id, QWidget* parent);
		~EntityEditor();
	};
}