#include "pipelinemanager.h"

#include "effects/effects.h"
#include "imageloader.h"
#include "imagesaver.h"
#include "imagequeue.h"
#include "simpletracer.h"
#include "effectstage.h"
#include "effects/effects.h"

#include <QVector>
#include <QProcess>
#include <QDateTime>
#include <QEventLoop>

#if defined(Q_OS_LINUX)
#include "pthread.h"
#elif defined(Q_OS_WIN)
#include "windows.h"
#endif

PipelineManager::PipelineManager(QObject *parent) :
    QObject(parent)
{
}

void PipelineManager::setup(QStringList fx, QDir &input, QDir &output)
{
    // Chargement des images comme première étape du pipeline
    ImageLoader *loader = new ImageLoader(this);
    loader->setName("loader");
    loader->setImageDir(input);
    stageList.append(loader);

    // Préparation des effets
    foreach (QString fxName, fx) {
        if (!effects.hasEffect(fxName)) {
            qDebug() << "unkown effect " << fxName;
            continue;
        }
        EffectStage *fxStage = new EffectStage(this);
        fxStage->setName(fxName);
        fxStage->setEffect(effects.effect(fxName));
        stageList.append(fxStage);
    }

    // Sauvegarde des images comme dernière étape du pipeline
    ImageSaver *saver = new ImageSaver(this);
    saver->setName("saver");
    saver->setOutput(output);
    stageList.append(saver);
}

#if defined(Q_OS_LINUX)
static void *startHelper(void *arg)
{
    PipelineStage *stage = (PipelineStage *) arg;
    qDebug() << "starting " << stage;
    stage->execute();
    return NULL;
}
#elif defined(Q_OS_WIN)
static DWORD WINAPI startHelper( LPVOID lpParam )
{
    PipelineStage *stage = (PipelineStage *) lpParam;
    qDebug() << "starting " << stage;
    stage->execute();
    return 0;
}
#endif

void PipelineManager::launchParallel(int queueSize)
{
    QDateTime now = QDateTime::currentDateTime();
    QString traceDir = QString("trace-%1").arg(now.toString());
    traceDir.replace(":", "_"); // Windows n'aime pas les ":" dans les noms de fichiers
    QDir dir(traceDir);
    dir.mkpath(".");

    // Définir le rank (position de l'étape dans le pipeline)
    for (int i = 0; i < stageList.size(); i++) {
        PipelineStage *stage = stageList.at(i);
        stage->setRank(i);
        SimpleTracer *t = new SimpleTracer(stage);
        QString stageName = QString("%1-0-%2.data")
                .arg(i)
                .arg(stage->name());
        QString path = traceDir + QDir::separator() + stageName;
        t->setPath(path);
        t->init();
    }

    // Connecter les étapes par des objets ImageQueue
    for (int i = 0; i < stageList.size() - 1; i++) {
        PipelineStage *prod = stageList.at(i);
        PipelineStage *cons = stageList.at(i + 1);
        ImageQueue *q = new ImageQueue(this, queueSize);
        qDebug() << "queue " << i << " " << prod << " ---> " << cons;

        // enregistrer les modifications effectuées à cette queue
        SimpleTracer *t = new SimpleTracer(q);
        QString stageName = QString("%1-1-%2-%3.data")
                .arg(i)
                .arg(prod->name())
                .arg(cons->name());
        QString path = traceDir + QDir::separator() + stageName;
        t->setPath(path);
        t->init();

        stageList.at(i)->setProdQueue(q);
        stageList.at(i + 1)->setConsQueue(q);
    }
#if defined(Q_OS_LINUX)
    // Démarrer les fils d'exécutions
    pthread_t *threads = new pthread_t[stageList.size()];
    for (int i = 0; i < stageList.size(); i++) {
        pthread_create(&threads[i], NULL, startHelper, stageList.at(i));
    }

    // Attendre la fin de l'exécution
    for (int i = 0; i < stageList.size(); i++) {
        pthread_join(threads[i], NULL);
    }
    delete[] threads;
#elif defined(Q_OS_WIN)
    /*
     * Démarrer les threads Windows
     * Attention: utilisez seulement l'API Windows (pas l'API de Qt!)
     */
    HANDLE  hThreadArray[stageList.size()];
    DWORD   dwThreadIdArray[stageList.size()];

    for( int i=0; i<stageList.size(); i++ ) {
        hThreadArray[i] = CreateThread(
                    NULL,                   // default security attributes
                    0,                      // use default stack size
                    startHelper,            // thread function name
                    stageList.at(i),        // argument to thread function
                    0,                      // use default creation flags
                    &dwThreadIdArray[i]);   // returns the thread identifier

        // Check the return value for success.
        // If CreateThread fails, terminate execution.
        // This will automatically clean up threads and memory.

        if (hThreadArray[i] == NULL)
        {
            // ErrorHandler(TEXT("CreateThread"));
            ExitProcess(3);
        }
    } // End of main thread creation loop.

    // Wait until all threads have terminated.

    WaitForMultipleObjects(stageList.size(), hThreadArray, TRUE, INFINITE);

    // Close all thread handles and free memory allocations.

    for(int i=0; i<stageList.size(); i++)
    {
        CloseHandle(hThreadArray[i]);
//        if(pDataArray[i] != NULL)
//        {
//            HeapFree(GetProcessHeap(), 0, pDataArray[i]);
//            pDataArray[i] = NULL;    // Ensure address is not reused.
//        }
    }
#endif
}
