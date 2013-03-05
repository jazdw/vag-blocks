#ifndef CLICKLINEEDIT_H
#define CLICKLINEEDIT_H

#include <QLineEdit>

class clickLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit clickLineEdit(QWidget *parent = 0);
signals:
    void clicked();
protected:
    void mousePressEvent(QMouseEvent* event);
};

#endif // CLICKLINEEDIT_H
