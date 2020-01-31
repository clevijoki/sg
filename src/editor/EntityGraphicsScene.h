#pragma once

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <memory>

class QGraphicsItemGroup;

namespace sg {

	class EntityGraphicsScene : public QGraphicsScene {
		Q_OBJECT

	public:

		EntityGraphicsScene(class Controller& controller, int64_t entity_id, QObject *parent=nullptr);
	};
}
