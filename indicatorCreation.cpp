#include <iostream>
#include <cstdint>

using TickFn = void(*)(void*, double);

struct IndicatorEntry {
    void* ptr;
    TickFn fn;
};

template<int N>
struct SMA {
    double ring[N] = {};
    double sum = 0.0;
    int idx = 0;

    inline void on_tick(double x) noexcept {
        sum -= ring[idx];
        ring[idx] = x;
        sum += x;
        idx = (idx + 1) & (N - 1);

        double value = sum / N;
        std::cout << "SMA<" << N << "> = " << value << "\n";
    }
};

template<int N>
void sma_tick(void* p, double x) {
    static_cast<SMA<N>*>(p)->on_tick(x);
}

struct EMA {
    double alpha;
    double value = 0.0;

    EMA(double a) : alpha(a) {}

    inline void on_tick(double x) noexcept {
        value = alpha * x + (1.0 - alpha) * value;
        std::cout << "EMA(alpha=" << alpha << ") = " << value << "\n";
    }
};

void ema_tick(void* p, double x) {
    static_cast<EMA*>(p)->on_tick(x);
}

SMA<8>  sma8;
SMA<16> sma16;
EMA ema_fast(0.2);
EMA ema_slow(0.05);

IndicatorEntry make_sma(int window) {
    switch (window) {
        case 8:  return {&sma8,  &sma_tick<8>};
        case 16: return {&sma16, &sma_tick<16>};
        default:
            throw std::runtime_error("Unsupported SMA window");
    }
}

IndicatorEntry make_ema(double alpha) {
    if (alpha == 0.2)
        return {&ema_fast, &ema_tick};
    else
        return {&ema_slow, &ema_tick};
}

int main() {

    IndicatorEntry table[10];
    int count = 0;

    table[count++] = make_sma(8);
    table[count++] = make_sma(16);
    table[count++] = make_ema(0.2);

 
    double prices[] = {100, 101, 102, 103, 104, 105};

    for (double price : prices) {
        std::cout << "---- price = " << price << " ----\n";

        for (int i = 0; i < count; i++) {
            table[i].fn(table[i].ptr, price);
        }
    }

    return 0;
}