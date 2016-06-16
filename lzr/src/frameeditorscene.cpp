
#include <QtGui>
#include "frameeditorscene.h"
#include "utils.h"
#include "settings.h"


FrameScene::FrameScene(QWidget *parent) : QGraphicsScene(parent)
{
    //enforce custom coordinate system [-1.0, 1.0]
    //Y is negative to make positive values go upwards
    setSceneRect(-3.0, -3.0, 6.0, 6.0); //bigger than (-1.0, -1.0, 2.0, 2.0) for scrollability
    setBackgroundBrush(Qt::black);

    state = new FrameEditorState;
    state->grid_divisions = 8;
    state->snap = false;
    state->reverse = false;

    addItem(grid = new Grid(state));
}

FrameScene::~FrameScene()
{
    delete state;
}

void FrameScene::setModel(Frame* m, QItemSelectionModel* path_sel)
{
    foreach(Path* path, paths)
    {
        removeItem(path);
        delete path;
    }

    paths.clear();

    model = m;

    connect(model, SIGNAL(rowsInserted(const QModelIndex&, int, int)),
            this, SLOT(path_added(const QModelIndex&, int, int)));
    connect(model, SIGNAL(rowsRemoved(const QModelIndex&, int, int)),
            this, SLOT(path_removed(const QModelIndex&, int, int)));

    //create all of the path objects in the model
    for(int i = 0; i < model->rowCount(); i++)
    {
        paths.append(new_path(model->index(i)));
    }

    //the path selection model
    path_selection = path_sel;
    connect(path_selection, SIGNAL(selectionChanged(const QItemSelection&,
                                                    const QItemSelection&)),
            this, SLOT(path_selection_changed(const QItemSelection&,
                                              const QItemSelection&)));
}

Path* FrameScene::new_path(QModelIndex index)
{
    Path* path = new Path(state, index);
    addItem(path);

    connect(path, SIGNAL(changed(Path*)),
            this, SLOT(path_changed(Path*)));

    return path;
}

void FrameScene::drawForeground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect); //because we always render the entire scene

    if(state->tool == LINE && current_path() && current_path()->size() > 0)
    {
        //lookup the currently selected path
        Path* path = current_path();
        Point* point;

        if(!state->reverse)
            point = path->last();
        else
            point = path->first();

        QPointF pos = constrain_and_snap(mouse,
                                         state->snap,
                                         state->grid_divisions);
        painter->setPen(QPen(Qt::darkGray, 0));
        painter->drawLine(QLineF(point->x(), point->y(), pos.x(), pos.y()));
    }
}

void FrameScene::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    mouse = e->scenePos();

    if(state->tool == LINE && current_path())
    {
        update();
    }

    QGraphicsScene::mouseMoveEvent(e);
}

void FrameScene::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if(state->tool == LINE && current_path())
    {
        Path* path = current_path();
        QPointF pos = constrain_and_snap(e->scenePos(),
                                         state->snap,
                                         state->grid_divisions);
        path->add_point(new Point(state, pos, state->color), state->reverse);
    }

    QGraphicsScene::mousePressEvent(e);
}

void FrameScene::keyPressEvent(QKeyEvent* e)
{
    if(!e->isAutoRepeat())
    {
        switch(e->key())
        {
            case EDITOR_SNAP_KEY:
                state->snap = true;
                update();
                break;
            case EDITOR_REVERSE_KEY:
                state->reverse = true;
                update();
                break;
            case EDITOR_DELETE_KEY:
                foreach(QGraphicsItem* item, selectedItems())
                {
                    ((Point*) item)->remove();
                }
                update();
                break;
        }
    }

    QGraphicsScene::keyPressEvent(e);
}

void FrameScene::keyReleaseEvent(QKeyEvent* e)
{
    if(!e->isAutoRepeat())
    {
        switch(e->key())
        {
            case EDITOR_SNAP_KEY:
                state->snap = false;
                update();
                break;
            case EDITOR_REVERSE_KEY:
                state->reverse = false;
                update();
                break;
        }
    }

    QGraphicsScene::keyReleaseEvent(e);
}

Path* FrameScene::current_path()
{
    if(path_selection->hasSelection())
        return paths[path_selection->currentIndex().row()];
    return NULL;
}


/*
 * Slots
 */

void FrameScene::path_selection_changed(const QItemSelection& selected,
                                        const QItemSelection& deselected)
{
    //deselect paths
    foreach(const QModelIndex& index, deselected.indexes())
    {
        if(index.row() >= paths.size())
        {
            qDebug() << "Error: model refers to path that doesn't exist in the editor";
            continue;
        }

        Path* path = paths[index.row()];
        path->setEnabled(false);
    }

    //select paths
    foreach(const QModelIndex& index, selected.indexes())
    {
        if(index.row() >= paths.size())
        {
            qDebug() << "Error: model refers to path that doesn't exist in the editor";
            continue;
        }

        Path* path = paths[index.row()];
        path->setEnabled(true);
    }
}

void FrameScene::path_changed(Path* path)
{
    //update the model with the new path data
    QVariant v;
    v.setValue(path->to_LZR());
    model->setData(path->get_index(), v);
}

void FrameScene::path_added(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent);

    for(int i = first; i <= last; i++)
    {
        paths.insert(i, new_path(model->index(i)));
    }
}

void FrameScene::path_removed(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent);

    for(int i = first; i <= last; i++)
    {
        delete paths[first];
        paths.removeAt(first);
    }
}

void FrameScene::tool_changed(tool_t t)
{
    state->tool = t;
    update();
}

void FrameScene::color_changed(QColor c)
{
    state->color = c;
}

void FrameScene::grid_changed(int divisions)
{
    state->grid_divisions = divisions;
    update();
}
