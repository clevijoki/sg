#pragma once

#include <QGraphicsItem>
#include <QGraphicsScene>

namespace sg {

	class EntityItem : public QGraphicsItem {

	public:
		using QGraphicsItem::QGraphicsItem;
		QRectF boundingRect() const override;
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override;
	};
	
	class EntityGraphicsScene : public QGraphicsScene {
		Q_OBJECT

	public:
		EntityGraphicsScene(QVariant entity_id, QWidget *widget=nullptr);
	};
}
