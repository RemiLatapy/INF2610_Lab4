#include <QImage>
#include <QDebug>

#include "imagequeue.h"
#include "simpletracer.h"

ImageQueue::ImageQueue(QObject *parent, int capacity) :
    QObject(parent),
    m_capacity(capacity)
{
    m_semIn = CreateSemaphore(
                NULL,           // default security attributes
                capacity,       // initial count
                capacity,       // maximum count
                NULL);          // unnamed semaphore

    m_semOut = CreateSemaphore(
                NULL,           // default security attributes
                0,              // initial count
                capacity,       // maximum count
                NULL);          // unnamed semaphore

    m_queuedImages = QQueue<QImage *>();
}

ImageQueue::~ImageQueue()
{
}

void ImageQueue::enqueue(QImage *item)
{
    int dwWaitResult = WaitForSingleObject(
                m_semIn,               // handle to semaphore
                INFINITE);           // INFINITE time-out interval

    if (dwWaitResult == WAIT_OBJECT_0)
    {
        // The semaphore object was signaled.
        m_queuedImages.enqueue(item);

        // tracer la taille de la file lorsqu'elle change
        SimpleTracer::writeEvent(this, m_queuedImages.size());

        // Release the out semaphore when task is finished
        ReleaseSemaphore(
                    m_semOut,        // handle to semaphore
                    1,            // increase count by one
                    NULL);       // not interested in previous count
    }

}

QImage *ImageQueue::dequeue()
{
    int dwWaitResult = WaitForSingleObject(
                m_semOut,               // handle to semaphore
                INFINITE);              // INFINITE time-out interval

    if (dwWaitResult == WAIT_OBJECT_0)
    {
        QImage *ret = m_queuedImages.dequeue();

        // tracer la taille de la file lorsqu'elle change
        SimpleTracer::writeEvent(this, m_queuedImages.size());

        // The semaphore object was signaled.
        // Release the semaphore in when task is finished
        ReleaseSemaphore(
                    m_semIn,        // handle to semaphore
                    1,            // increase count by one
                    NULL);

       return ret;
    }
    return NULL;
}
