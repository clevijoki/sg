#pragma once

#include <QGraphicsItem>
#include <QGraphicsScene>

namespace sg {

	class EntityGraphicsScene : public QGraphicsScene {
		Q_OBJECT

	public:
		EntityGraphicsScene(class Controller& controller, QVariant entity_id, QObject *parent=nullptr);
	};
}
