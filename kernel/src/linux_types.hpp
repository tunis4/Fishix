#pragma once

using ssize_t = long;
using paddr_t = unsigned long long;

struct list_head {
    list_head *next;
    list_head *prev;
};

enum dma_data_direction {
    DMA_BIDIRECTIONAL,
    DMA_TO_DEVICE,
    DMA_FROM_DEVICE
};