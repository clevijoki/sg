#include "EntityGraphicsScene.h"
#include <QPainter>

namespace sg {
	QRectF EntityItem::boundingRect() const {
		const qreal pen_width = 1;
		return QRectF(
			-10 - pen_width /2, -10 - pen_width/2,
			20 + pen_width, 20 + pen_width
		);
	}

	void EntityItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) {
		painter->drawRoundedRect(-10, -10, 20, 20, 5, 5);
	}

	EntityGraphicsScene::EntityGraphicsScene(QVariant entity_id, QWidget *widget) {
	}
}
