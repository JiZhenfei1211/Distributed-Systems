#ifndef DSHW3_K42_MCS_LOCK_H
#define DSHW3_K42_MCS_LOCK_H

#include <atomic>

namespace k42_mcs_space {

    struct mcs_node {
        std::atomic<mcs_node*> tail;
        std::atomic<mcs_node*> next;
        mcs_node(mcs_node* t, mcs_node* n):tail(t), next(n){}
    };

    mcs_node waiting = {nullptr, nullptr};

    class k42_mcs_lock{
    private:
         mcs_node* q = new mcs_node{nullptr, nullptr};
    public:
        ~k42_mcs_lock();
        void acquire();
        void release();
    };

    k42_mcs_lock::~k42_mcs_lock() {
        delete(q);
    }

    void k42_mcs_lock::acquire() {
        while (true){
            mcs_node* prev = q->tail.load();
            if (prev == nullptr){
                mcs_node *nullpt = nullptr;
                if (q->tail.compare_exchange_strong(nullpt, q, std::memory_order_release, std::memory_order_relaxed)) {
                    break;
                }
            } else {
                mcs_node n = {&waiting, nullptr};
                if (q->tail.compare_exchange_strong(prev, &n, std::memory_order_acquire)){
                    prev->next.store(&n);
                    while (n.tail.load() == &waiting);
                    mcs_node* succ = n.next.load();
                    if (succ == nullptr){
                        q->next.store(nullptr);
                        mcs_node* np = &n;
                        if (!q->tail.compare_exchange_strong(np, q, std::memory_order_release, std::memory_order_relaxed)){
                            while (n.next.load() == nullptr);
                            q->next.store(succ);
                        }
                        break;
                    } else {
                        q->next.store(succ);
                        break;
                    }
                }
            }
        }
    }

    void k42_mcs_lock::release() {
        mcs_node* succ = q->next.load(std::memory_order_acquire);
        if (succ == nullptr) {
            if (q->tail.compare_exchange_strong(q, nullptr, std::memory_order_release, std::memory_order_relaxed)) return;
            while (q->next.load() == nullptr);
        }
        succ->tail.store(nullptr);
    }

}

#endif //DSHW3_K42_MCS_LOCK_H
