#pragma once

#include <QObject>

namespace sg {

	// sets up mouse so left drags and middle pans
	class GraphicsViewMouseEventFilter : public QObject {
		Q_OBJECT
	public:
		using QObject::QObject;
		
		bool eventFilter(QObject *object, QEvent *e) override;
	};
}