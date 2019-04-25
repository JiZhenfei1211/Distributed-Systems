#ifndef DSHW3_MCS_LOCK_H
#define DSHW3_MCS_LOCK_H

#include <atomic>

namespace mcs_space {

    class MCS_Lock {

        struct mcs_node {
            bool waiting{true};
            mcs_node* next{nullptr};
        };

    public:
        void acquire();
        void release();

    private:
        std::atomic<mcs_node*> tail{nullptr};
        static thread_local mcs_node local_node;

    };

    void MCS_Lock::acquire() {
        auto prior_node = tail.exchange(&local_node, std::memory_order_acquire);
        if (prior_node!= nullptr){
            local_node.waiting = true;
            prior_node->next = &local_node;
            while (local_node.waiting);
        }
    }

    void MCS_Lock::release() {
        if (local_node.next == nullptr) {
            mcs_node *p = &local_node;
            if (tail.compare_exchange_strong(p, nullptr, std::memory_order_release, std::memory_order_relaxed)) {
                return;
            }
            while (local_node.next == nullptr);
        }
        local_node.next->waiting = false;
        local_node.next = nullptr;
    }
    thread_local MCS_Lock::mcs_node MCS_Lock::local_node = MCS_Lock::mcs_node{};

}
#endif //DSHW3_MCS_LOCK_H
