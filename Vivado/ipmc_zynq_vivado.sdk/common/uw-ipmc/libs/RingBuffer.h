#ifndef UW_IPMC_LIBS_RINGBUFFER_H_
#define UW_IPMC_LIBS_RINGBUFFER_H_

#include <stddef.h>

/**
 * A simple DMA-aware ring buffer.
 *
 * The RingBuffer class implements a basic ring buffer with a read() and write()
 * function.  It also provides functionality to assist in DMA-like input and
 * output (providing a linear buffer to fill or drain).
 *
 * \note It may require multiple DMA-like operations to fill or drain the buffer.
 *
 * \note All functions other than the constructor and destructor are ISR safe.
 */
template <typename T> class RingBuffer {
protected:
	T *buffer;                      ///< The actual buffer space itself.
	size_t buflen;                  ///< The size of the buffer in units of sizeof(T) (maxlength()+1 for bookkeeping.)
	volatile size_t next_read_idx;  ///< The next read position in the ringbuffer.
	volatile size_t next_write_idx; ///< The next write position in the ringbuffer.
public:
	RingBuffer(size_t items);
	size_t write(const T *data, size_t len);
	size_t read(T *data, size_t len);
	size_t length();
	size_t maxlength();
	/**
	 * Determine whether the ring buffer is empty.
	 * \return true if the ring buffer is empty, otherwise false
	 */
	bool empty() { return this->next_read_idx == this->next_write_idx; }
	/**
	 * Determine whether the ring buffer is full.
	 * \return true if the ring buffer is full, otherwise false
	 */
	bool full() { return this->next_read_idx == ((this->next_write_idx+1) % this->buflen); }
	void setup_dma_input(T **data_start, size_t *maxitems);
	void notify_dma_input_occurred(size_t items);
	void setup_dma_output(T **data_start, size_t *maxitems);
	void notify_dma_output_occurred(size_t items);
	virtual ~RingBuffer();
};

#endif /* UW_IPMC_LIBS_RINGBUFFER_H_ */
