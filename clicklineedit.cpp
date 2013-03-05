#include "clicklineedit.h"

clickLineEdit::clickLineEdit(QWidget *parent) :
    QLineEdit(parent)
{
}

void clickLineEdit::mousePressEvent(QMouseEvent* event)
{
    emit clicked();
}
