#ifndef SUBJECT_INTERFACE_H_
#define SUBJECT_INTERFACE_H_

#include <functional>
#include <memory>
#include <stdint.h>
#include <vector>

template<typename T>
class Observable
{
public:
    Observable(){};
    ~Observable(){};
    typedef std::function<void(T)> OnMessageFunc;

    void Subscribe(OnMessageFunc func)
    {
        subscribed_func_ = func;
    }

    OnMessageFunc subscribed_func_;
};

template<typename T>
class SubjectInterface
{
public:
    virtual ~SubjectInterface(){};
    virtual void Next(T message) = 0;
    virtual std::shared_ptr<Observable<T>> AsObservable() = 0;
    virtual void UnSubscribe() = 0;

    std::vector<std::shared_ptr<Observable<T>>> observers_;
};

#endif
