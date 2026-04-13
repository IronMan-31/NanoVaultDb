#pragma once
#include <bits/types/struct_sched_param.h>
#include <pthread.h>
#include <sched.h>
#include <stdexcept>
#include <thread>
#include <string>
#include <cerrno>
#include <cstring>




inline void pin_thread_to_cpu(int cpu_id){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    sched_param param{};
    param.sched_priority = 99;
    sched_setscheduler(0, SCHED_FIFO, &param);
}

inline void pin_thread_to_cpu(std::thread &t,int cpu_id){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
}

inline void pin_thread_to_cpu(std::jthread &t, int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
}

inline void set_realtime_priority(std::thread &t,int prio = 99){
    sched_param param{};
    param.sched_priority = prio;
    pthread_setschedparam(t.native_handle(), SCHED_FIFO, &param);
}



inline int current_numa_node() noexcept {
    int cpu = sched_getcpu();
    if (cpu < 0) return 0;
     return cpu / 16;
}
