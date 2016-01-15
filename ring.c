/******************************************************************************
 * Simple ringbuffer implementation from open-bldc's libgovernor that
 * you can find at:
 * https://github.com/open-bldc/open-bldc/tree/master/source/libgovernor
 *****************************************************************************/

#include "ring.h"

void
ring_init(ring_t *ring, uint8_t *buf, ring_size_t size)
{
    ring->data = buf;
    ring->size = size;
    ring->begin = 0;
    ring->end = 0;
}

int32_t
ring_write_ch(ring_t *ring, uint8_t ch)
{
    if (((ring->end + 1) % ring->size) != ring->begin) {
        ring->data[ring->end++] = ch;
        ring->end %= ring->size;
        return (uint32_t)ch;
    }

    return -1;
}

int32_t
ring_write(ring_t *ring, uint8_t *data, ring_size_t size)
{
    int32_t i;

    for (i = 0; i < size; i++) {
        if (ring_write_ch(ring, data[i]) < 0)
            return -i;
    }

    return i;
}

int32_t
ring_read_ch(ring_t *ring, uint8_t *ch)
{
    int32_t ret = -1;

    if (ring->begin != ring->end) {
        ret = ring->data[ring->begin++];
        ring->begin %= ring->size;
        if (ch)
            *ch = ret;
    }

    return ret;
}

int32_t
ring_read(ring_t *ring, uint8_t *data, ring_size_t size)
{
    int32_t i;

    for (i = 0; i < size; i++) {
        if (ring_read_ch(ring, data + i) < 0)
            return i;
    }

    return -i;
}

int32_t
ring_read_contineous(ring_t *ring, uint8_t **data)
{
    int32_t i;

    *data = &ring->data[ring->begin];

    if (ring->begin == ring->end) {
        return 0;
    } else if (ring->begin > ring->end) {
        i = ring->size - ring->begin;
        ring->begin = 0;
    } else if (ring->begin < ring->end) {
        i = ring->end - ring->begin;
        ring->begin = ring->end;
    }

    return i;
}

uint32_t
ring_mark(ring_t *ring)
{
    return ring->end;
}

uint32_t
ring_marklen(ring_t *ring, uint32_t mark)
{
    return (((ring->end - mark) + ring->size) % ring->size);
}
