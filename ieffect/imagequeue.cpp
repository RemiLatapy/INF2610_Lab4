#include <QImage>
#include <QDebug>
#include <QQueue>

#include "imagequeue.h"
#include "simpletracer.h"

ImageQueue::ImageQueue(QObject *parent, int capacity) :
    QObject(parent),
    m_capacity(capacity)
{
}

ImageQueue::~ImageQueue()
{
}

void ImageQueue::enqueue(QImage *item)
{
    // tracer la taille de la file lorsqu'elle change
    SimpleTracer::writeEvent(this, 0);
}

QImage *ImageQueue::dequeue()
{
    // tracer la taille de la file lorsqu'elle change
    SimpleTracer::writeEvent(this, 0);
    return NULL;
}
