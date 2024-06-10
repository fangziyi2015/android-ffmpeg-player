#ifndef FMPLAYER_SAFE_QUEUE_H
#define FMPLAYER_SAFE_QUEUE_H

#define SUCCESS 1
#define ERROR 0

#include <pthread.h>
#include <queue>

using namespace std;

template<typename T>
class SafeQueue {
private:
    typedef void (*ReleaseCallback)(T *value);
    typedef void (*SyncCallback)(queue <T> &);

private:
    queue <T> data;
    pthread_mutex_t mutex_t;
    pthread_cond_t cond_t;
    int working = 0;
    ReleaseCallback releaseCallback;
    SyncCallback syncCallback;

public:
    SafeQueue() {
        pthread_mutex_init(&mutex_t, nullptr);
        pthread_cond_init(&cond_t, nullptr);
    }

    ~SafeQueue() {
        pthread_mutex_destroy(&mutex_t);
        pthread_cond_destroy(&cond_t);
    }

    void insertData(T value) {
        pthread_mutex_lock(&mutex_t);

        if (working) {
            data.push(value);
            pthread_cond_signal(&cond_t);
        } else {
            if (releaseCallback) {
                releaseCallback(&value);
            }
        }

        pthread_mutex_unlock(&mutex_t);
    }

    int getData(T &_p) {
        int result = ERROR;

        pthread_mutex_lock(&mutex_t);

        while (working && data.empty()) {
            pthread_cond_wait(&cond_t, &mutex_t);
        }

        if (!data.empty()) {
            _p = data.front();
            data.pop();
            result = SUCCESS;
        }

        pthread_mutex_unlock(&mutex_t);

        return result;
    }

    void setWork(int _work) {
        pthread_mutex_lock(&mutex_t);

        this->working = _work;

        pthread_cond_signal(&cond_t);

        pthread_mutex_unlock(&mutex_t);
    }

    bool empty() {
        return data.empty();
    }

    int size() {
        return data.size();
    }

    void release() {
        pthread_mutex_lock(&mutex_t);
        unsigned int size = data.size();
        for (int i = 0; i < size; ++i) {
            T value = data.front();
            if (releaseCallback) {
                releaseCallback(&value);
            }
            data.pop();
        }
        pthread_mutex_unlock(&mutex_t);
    }

    void setReleaseCallback(ReleaseCallback _releaseCallback) {
        this->releaseCallback = _releaseCallback;
    }

    void setSyncCallback(SyncCallback _syncCallback) {
        this->syncCallback = _syncCallback;
    }

    void sync() {
        pthread_mutex_lock(&mutex_t);
        syncCallback(data);
        pthread_mutex_unlock(&mutex_t);
    }
};

#endif //FMPLAYER_SAFE_QUEUE_H
