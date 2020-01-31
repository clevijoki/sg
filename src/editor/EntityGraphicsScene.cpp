#include "EntityGraphicsScene.h"
#include <vector>
#include <unordered_map>

#include <QPainter>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QDebug>
#include <QFontMetrics>
#include <QFont>
#include <QGraphicsItemGroup>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QLineEdit>
#include <QTransform>
#include <QPainterPathStroker>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

#include "Controller.h"
#include "MessageBox.h"

namespace sg {


	const QFont PROP_FONT("Fixed", 12);
	const QFont TITLE_FONT("Fixed", 12, QFont::Bold);

	const float PIN_SIZE = 13;
	const QMargins PIN_BORDER(7,7,7,7);

	class ConnectionItem : public QGraphicsItem {
		class PinItem &mSource;
		class PinItem &mTarget;
	public:
		ConnectionItem(Controller &controller, PinItem &source, PinItem &target) 
		: mSource(source)
		, mTarget(target){
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override;
	};

	class PreviewConnectionItem : public QGraphicsItem {
		class PinItem &mSource;
		QPointF mTarget;
	public:
		PreviewConnectionItem(Controller &controller, PinItem &source, QPointF target) 
		: mSource(source)
		, mTarget(target){
		}

		void setTarget(const QPointF target) {
			mTarget = target;
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override;
	};

	class PinItem : public QGraphicsItem {

		Controller &mController;

		QRectF mBounds;
		QRectF mDrawBounds;
		QBrush mBrush;

		const int64_t mId;
		QString mPinType;

	public:
		PinItem(Controller& controller, float size, int64_t id, QGraphicsItem *parent = nullptr)
		: QGraphicsItem(parent)
		, mController(controller)
		, mId(id) {
			setSize(size);
			setAcceptHoverEvents(true);
			setPanelModality(QGraphicsItem::PanelModal);
			mBrush = Qt::darkGray;
		}

		void setPinType(QString type) {
			mPinType = std::move(type);
		}

		const QString& pinType() const {
			return mPinType;
		}

		void setSize(float size) {
			mBounds = QRect(0,0,size,size);
			mDrawBounds = mBounds.marginsRemoved(PIN_BORDER);
		}

		QRectF boundingRect() const override {
			return mBounds;
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override {
			painter->setBrush(mBrush);
			painter->drawRect(mDrawBounds);
		}

		void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override {
			mBrush = Qt::red;
			update();
		}

		void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override {
			mBrush = Qt::darkGray;
			update();
		}

		void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
			event->accept();
		}

		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
	};

	struct ItemProperty {
		QString name;
		QString type;
		int64_t id = -1;
		QRectF bounds;
		PinItem *pin = nullptr;
		QGraphicsProxyWidget *proxy = nullptr;
		QLineEdit *edit_widget = nullptr;
	};

	class ComponentEntityItem : public QGraphicsItem {
		enum class EState {
			Default,
			Dragging,
		};

		Controller& mController;

		int64_t mComponentTypeId = -1;
		const int64_t mId;

		std::vector<ItemProperty> mProperties;
		QString mComponentTypeTitle;
		QString mSourceName;

		QRectF mBounds;
		QRectF mComponentNameBounds;
		QRectF mNameBounds;
		PinItem *mRefPin;

		QLineEdit *mTitleEdit;
		QGraphicsProxyWidget *mTitleProxy;

		QPointF mAnchorPoint;
		EState mState = EState::Default;

		QPointF mDragStart;

		void setAnchorPoint(const QPointF& pt) {
			mAnchorPoint = pt;
		}

		void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
			mState = EState::Dragging;
			mDragStart = pos();
			QGraphicsItem::mouseMoveEvent(event);			
		}

		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
			if (mState == EState::Dragging) {

				auto apply = [&]() -> Result<> {
					auto t = mController.createTransaction("Drag component instance");

					auto res = t.update("entity_component", {{"graph_pos", pos()}}, {{"graph_pos", mDragStart}}, "id", qlonglong(mId));
					if (res.failed())
						return res.error();

					return t.commit();
				};

				auto res = apply();
				if (res.failed()) {
					MessageBoxCritical("Unable to apply drag", res.errorMessage(), res.errorInfo());

					setPos(mDragStart);
				}
			} 

			mState = EState::Default;

			QGraphicsItem::mouseReleaseEvent(event);
		}

	public:
		ComponentEntityItem(Controller& controller, int64_t id, QGraphicsItem *parent = nullptr) 
		: QGraphicsItem(parent)
		, mController(controller)
		, mId(id)
		, mRefPin(new PinItem(controller, PIN_SIZE, id, this))
		, mTitleProxy(new QGraphicsProxyWidget(this))
		, mTitleEdit(new QLineEdit()) {

			setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
			setAcceptHoverEvents(true);

			mRefPin->setPinType("object_ref");

			mTitleProxy->setWidget(mTitleEdit);
			mTitleEdit->setFont(TITLE_FONT);

			mTitleProxy->connect(mTitleEdit, &QLineEdit::returnPressed, mTitleProxy, [&controller, this](){
				
				auto t = controller.createTransaction("Rename Component Instance");
				t.update("entity_component", {{"name", mTitleEdit->text()}}, {{"name", mSourceName}}, "id", qlonglong(mId));

				auto res = t.commit();
				if (res.failed()) {
					MessageBoxCritical("Unable to rename component instance", res.errorMessage(), res.errorInfo());
				}

			});

		}

		void refresh(QString instance_name, const QString& component_type_name, int64_t component_type_id) {

			mComponentTypeId = component_type_id;
			mComponentTypeTitle = QString("%1:").arg(component_type_name);

			mSourceName = instance_name;
			mTitleEdit->setText(std::move(instance_name));

			QFontMetrics title_fm(TITLE_FONT);
			QFontMetrics prop_fm(PROP_FONT);

			mComponentNameBounds = title_fm.boundingRect(mComponentTypeTitle).marginsAdded(QMargins(1,1,1,10));
			mNameBounds = title_fm.boundingRect(mTitleEdit->text());

			mComponentNameBounds.moveTo(0,0);
			mNameBounds.moveTo(mComponentNameBounds.right(), 0);

			mTitleProxy->setGeometry(mNameBounds);

			float height = std::max(mComponentNameBounds.bottom(), mNameBounds.bottom());

			mComponentNameBounds.setHeight(height);
			mNameBounds.setHeight(height);

			mRefPin->setSize(prop_fm.height());
			mRefPin->setPos(mNameBounds.right(), (height-prop_fm.height())/2);

			float width = mNameBounds.right() + prop_fm.height();
			float pin_label_width = 0;

			QSqlQueryModel m;
			m.setQuery(QString("SELECT name, type, default_value, id FROM component_prop WHERE component_id = %1").arg(ToSqlLiteral(qlonglong(mComponentTypeId))));
			const int row_count = m.rowCount();			

			for (int row = 0; row < row_count; ++row) {

				ItemProperty *ip = nullptr;

				const int64_t prop_id = m.data(m.index(row, 3)).toLongLong();

				// first try and find an existing property that matches this id
				for (ItemProperty& existing : mProperties) {
					if (prop_id == existing.id) {
						ip = &existing;
						break;
					}
				}

				if (!ip) {

					mProperties.push_back({});

					ip = &mProperties.back();

					ip->id = prop_id;
					ip->pin = new PinItem(mController, prop_fm.height(), prop_id, this);

					ip->proxy = new QGraphicsProxyWidget(this);
					ip->edit_widget = new QLineEdit();
					ip->proxy->setWidget(ip->edit_widget);

				}

				ip->name = m.data(m.index(row, 0)).toString();
				ip->type = m.data(m.index(row, 1)).toString();
				ip->edit_widget->setText(m.data(m.index(row, 2)).toString());
		
				ip->bounds = prop_fm.boundingRect(ip->name);
				ip->bounds.moveTo(prop_fm.height(), height);

				ip->pin->setPinType(ip->type);
				ip->pin->setPos(0, height);


				width = std::max<float>(width, ip->bounds.right());
				pin_label_width = std::max<float>(pin_label_width, ip->bounds.right());

				height = ip->bounds.bottom();
			}

			pin_label_width += PIN_SIZE;

			width = std::max(width, pin_label_width+100);

			for (int row = 0; row < row_count; ++row) {

				ItemProperty& ip = mProperties[row];
				ip.edit_widget->setGeometry(QRectF(pin_label_width, ip.bounds.top(), width-pin_label_width, ip.bounds.height()).toRect());
			}

			mBounds = QRectF(0, 0, width, height).marginsAdded(QMargins(4,4,4,4));
		}

		QRectF boundingRect() const override {
			return mBounds.marginsAdded(QMargins(3,3,3,3));
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override {

			painter->setBrush(Qt::white);
			painter->setPen(Qt::NoPen);
			painter->drawRect(mBounds);

			painter->setBrush((option->state & QStyle::State_Selected) ? Qt::yellow : Qt::white);
			painter->setPen(QPen(Qt::black, 0));

			QPainterPath path;
			path.addRect(mBounds.marginsAdded(QMargins(2,2,2,2)));
			path.addRect(mBounds);

			painter->drawPath(path);

			painter->setPen(QPen(Qt::black, 0));
			painter->setBrush(Qt::white);
			painter->drawRect(mBounds);

			painter->setFont(TITLE_FONT);
			painter->drawText(mComponentNameBounds, Qt::AlignVCenter | Qt::AlignLeft, mComponentTypeTitle);

			painter->setFont(PROP_FONT);

			for (const ItemProperty& ip : mProperties) {
				painter->drawText(ip.bounds, Qt::AlignLeft | Qt::AlignVCenter, ip.name);
			}
		}

		int64_t id() const {
			return mId;
		}

		int64_t componentId() const {
			return mComponentTypeId;
		}
	};

	void ConnectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) {
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(Qt::blue, 3));

		painter->drawLine(mSource.pos(), mTarget.pos());
	}

	void PreviewConnectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) {
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(Qt::blue, 3));

		painter->drawLine(mSource.pos(), mTarget);
	}

	void PinItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
		event->accept();

		ComponentEntityItem *source_component = dynamic_cast<ComponentEntityItem*>(parentItem());

		if (QGraphicsItem* item = scene()->itemAt(event->scenePos(), QTransform())) {
			if (PinItem* target_pin = dynamic_cast<PinItem*>(item)) {
				if (ComponentEntityItem* target_component = dynamic_cast<ComponentEntityItem*>(target_pin->parentItem())) {
					if (target_component != source_component) {
						qDebug() << QString("Create new connection from id:%1 component_id:%2 property_id:%3 to id:%4 component_id:%5 property_id:%6")
							.arg(source_component->id())
							.arg(source_component->componentId())
							.arg(mId)
							.arg(target_component->id())
							.arg(target_component->componentId())
							.arg(target_pin->mId);
					}
				}
			}
		}
	}

	EntityGraphicsScene::EntityGraphicsScene(class Controller& controller, int64_t entity_id, QObject *parent)
	: QGraphicsScene(parent) {

		QGraphicsItemGroup *all_nodes = createItemGroup({});
		all_nodes->setHandlesChildEvents(false);

		#if 0
		auto dse = new QGraphicsDropShadowEffect();
		dse->setBlurRadius(32);
		dse->setColor(Qt::lightGray);
		all_nodes->setGraphicsEffect(dse);
		#endif

		auto refresh = [this, entity_id, &controller, all_nodes](const Transaction& t) {

			QSqlQueryModel m;
			m.setQuery(QString("SELECT entity_component.name, component.name, component_id, entity_component.id, entity_component.graph_pos FROM entity_component INNER JOIN component on component.id = component_id WHERE entity_id = %1").arg(entity_id), *t.connection());

			if (m.lastError().isValid())
				qDebug() << m.lastError().text();

			std::unordered_map<int64_t, ComponentEntityItem*> existing_component_items;

			qDebug() << "refreshing " << this;

			for (QGraphicsItem* existing : this->items()) {
				if (ComponentEntityItem* cei = dynamic_cast<ComponentEntityItem*>(existing)) {
					existing_component_items[cei->id()] = cei;
				}
			}

			QSet<int64_t> desired_component_items;

			for (int row = 0; row < m.rowCount(); ++row) {

				int64_t id = m.data(m.index(row, 3)).toLongLong();
				desired_component_items.insert(id);

				ComponentEntityItem *ei = nullptr;

				auto ei_itr = existing_component_items.find(id);

				if (ei_itr != existing_component_items.end()) {
					ei = ei_itr->second;
				} else {
					ei = new ComponentEntityItem(controller, id);
					ei->setGroup(all_nodes);
				}

				ei->refresh(
					m.data(m.index(row, 0)).toString(), // instance name
					m.data(m.index(row, 1)).toString(), // type name
					m.data(m.index(row, 2)).toLongLong() // component id
				);

				QPointF pos = sg::ToQPointF(m.data(m.index(row, 4)));
				ei->setPos(pos);
			}

			// clear all items that were removed
			for (const auto& kv : existing_component_items) {
				if (!desired_component_items.contains(kv.first)) {
					removeItem(kv.second);
					delete kv.second;
				}
			}
		};

		refresh(controller.createTransaction("Initial setup"));

		connect(&controller, &Controller::dataChanged, this, [refresh, &controller](const QSet<QString>& tables_affected){
			if (tables_affected.contains("entity_component") || tables_affected.contains("component")) {
				refresh(controller.createTransaction("Refresh Query"));
			}
		});
	}
}
