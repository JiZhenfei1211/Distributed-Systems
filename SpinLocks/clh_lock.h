#ifndef DSHW3_CLH_LOCK_H
#define DSHW3_CLH_LOCK_H
#include <atomic>

namespace clh_space {
    struct qnode {
        std::atomic<qnode*> prev{nullptr};
        std::atomic_bool succ_must_wait{false};
    };

    class clh_lock {

        qnode dummy;
        std::atomic<qnode*> tail{&dummy};
        static thread_local qnode* p;

    public:
        ~clh_lock();
        void acquire();
        void release();
    };

    clh_lock::~clh_lock(){
        delete(p);
    }

    void clh_lock::acquire() {
        p->succ_must_wait = true;
        p->prev = tail.exchange(p, std::memory_order_acquire);
        qnode* pred = p->prev;
        while (pred->succ_must_wait.load());
    }

    void clh_lock::release() {
        qnode* pred = p->prev;
        p->succ_must_wait.store(false, std::memory_order_release);
        p = pred;
    }
    thread_local qnode* clh_lock::p = new qnode{};
}


#endif //DSHW3_CLH_LOCK_H
