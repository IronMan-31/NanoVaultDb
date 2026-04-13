#pragma once
#include<bits/stdc++.h>
#include <atomic>
#include <pthread.h>

using namespace std;

template<typename T,size_t Capacity>
class MPMCQueue{

    struct alignas(64) Slot{
        std::atomic<size_t> sequence;
        T data;
    };

    static_assert((Capacity & (Capacity -1)) == 0);
    static constexpr size_t MASK = Capacity - 1;

    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};

    alignas(64) Slot slots_[Capacity];

public:

    MPMCQueue(){
        for(size_t i = 0; i < Capacity; i++){
            slots_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    bool enqueue(T item) noexcept {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        
        for(;;){
            Slot& slot = slots_[pos & MASK];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t) pos;

            if(diff ==0){
                if(enqueue_pos_.compare_exchange_weak(
                    pos,pos+1,std::memory_order_relaxed
                )){
                    slot.data = std::move(item);
                    slot.sequence.store(pos+1,std::memory_order_release);
                    return true;
                }
            }else if(diff < 0){
                return false; 
            }else{
                
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
    }

    bool dequeue(T & item )noexcept {
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for(;;){
            Slot & slot = slots_[pos & MASK];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq  - (intptr_t)(pos+1);

            if(diff == 0){
                if(dequeue_pos_.compare_exchange_weak(
                    pos,pos+1,std::memory_order_relaxed
                )){
                    item = std::move(slot.data);
                    slot.sequence.store(pos + Capacity,std::memory_order_release);
                    return true;
                }
            }else if(diff < 0){
                return false; 
            }else{
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
};
