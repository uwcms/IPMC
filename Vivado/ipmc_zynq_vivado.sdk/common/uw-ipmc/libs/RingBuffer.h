#ifndef UW_IPMC_LIBS_RINGBUFFER_H_
#define UW_IPMC_LIBS_RINGBUFFER_H_

#include <libs/ThreadingPrimitives.h>

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
	const size_t buflen;            ///< The size of the buffer in units of sizeof(T).
	const size_t maxlen;			///< The maximum number of units that can be stored in the buffer.
	volatile size_t next_read_idx;  ///< The next read position in the ringbuffer.
	volatile size_t next_write_idx; ///< The next write position in the ringbuffer.

public:
	/**
	 * Instantiate a new RingBuffer. Space for `items` items will be allocated
	 * on the heap.
	 *
	 * \param items The number of `T` items that will fit in the buffer. Needs to be power of 2.
	 */
	RingBuffer(size_t items) :
			buflen(items), maxlen(items-1), next_read_idx(0), next_write_idx(0) {
		configASSERT((items & maxlen) == 0); // Check if size is power of 2
		this->buffer = static_cast<T*>(malloc(this->buflen * sizeof(T)));
	}

	///! Cleans and resets the ring buffer.
	void reset() {
		if (!IN_INTERRUPT())
			portENTER_CRITICAL();

		next_read_idx = 0;
		next_write_idx = 0;

		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();
	}

	/**
	 * Write items into the ring buffer.
	 *
	 * \param data  A pointer to the data buffer to copy into the ring buffer.
	 * \param len   The number of data items to copy into the ring buffer.
	 * \return      The number of items copied into the ring buffer successfully.
	 */
	size_t write(const T *data, size_t len) {
		size_t i = 0;
		if (!IN_INTERRUPT())
			portENTER_CRITICAL();

		if (this->full()) {
			// Do nothing, buffer is full!
		} else if (this->empty()) {
			// Buffer is empty, so copy everything possible from the start
			const size_t copy_cnt = (len >= this->maxlen)? this->maxlen : len;

			memcpy(this->buffer, data, sizeof(T) * copy_cnt);

			this->next_read_idx = 0;
			this->next_write_idx = copy_cnt;
			i = copy_cnt;
		} else if (this->next_write_idx > this->next_read_idx) {
			// Write index is ahead, buffer can be written to the end and wrap if necessary
			size_t remaining = this->buflen - this->next_write_idx;
			if (this->next_read_idx == 0) remaining -= 1;
			const size_t copy_cnt = (len >= remaining)? remaining : len;

			memcpy(&(this->buffer[this->next_write_idx]), data, sizeof(T) * copy_cnt);

			this->next_write_idx = (this->next_write_idx + copy_cnt) & this->maxlen;
			i = copy_cnt;

			if (len > copy_cnt && this->next_read_idx > 0) {
				// There is still space and data to copy
				i += this->write(&(data[copy_cnt]), len - copy_cnt);
			}
		} else {
			// Read index is ahead, the write operation will probably hit it
			const size_t remaining = this->next_read_idx - this->next_write_idx - 1;
			const size_t copy_cnt = (len >= remaining)? remaining : len;

			memcpy(&(this->buffer[this->next_write_idx]), data, sizeof(T) * copy_cnt);

			this->next_write_idx += copy_cnt; // Will never wrap
			i = copy_cnt;
		}

		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();

		return i;
	}

	/**
	 * Read items out of the ring buffer.
	 *
	 * \param data  A pointer to the data buffer to copy the data into.
	 * \param len   The number of data items to copy out of the ring buffer.
	 * \return      The number of items copied out of the ring buffer successfully.
	 */
	size_t read(T *data, size_t len) {
		size_t i = 0;
		if (!IN_INTERRUPT())
			portENTER_CRITICAL();

		if (this->empty()) {
			// No data available, do nothing.
		} else if (this->next_write_idx > this->next_read_idx) {
			// The current buffer contents are linear in memory.
			const size_t available = this->length();
			const size_t copy_cnt = (len >= available)? available : len;

			memcpy(data, this->buffer, sizeof(T) * copy_cnt);

			this->next_read_idx += copy_cnt; // Won't wrap.
			i = copy_cnt;
		} else {
			// The current buffer contents are nonlinear in memory
			const size_t available = this->buflen - this->next_read_idx;
			const size_t copy_cnt = (len >= available)? available : len;

			memcpy(data, this->buffer, sizeof(T) * copy_cnt);

			this->next_read_idx = (this->next_read_idx + copy_cnt) & this->maxlen;

			if (len > copy_cnt && !this->empty()) {
				// Not everything was read and there is still data, continue then.
				i += this->read(&data[copy_cnt], len - copy_cnt);
			}
		}

		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();

		return i;
	}

	/**
	 * Return the number of items currently stored in the ring buffer.
	 *
	 * \return The number of items currently stored in the ring buffer.
	 */
	inline size_t length() {
		return (this->next_write_idx - this->next_read_idx) & this->maxlen;
	}

	/**
	 * Return the maximum number of items which may be stored in the ring buffer.
	 *
	 * \return The maximum number of items which may be stored in the ring buffer.
	 */
	inline size_t maxlength() {
		return this->maxlen;
	}

	/**
	 * Determine whether the ring buffer is empty.
	 * \return true if the ring buffer is empty, otherwise false
	 */
	inline bool empty() {
		return this->next_read_idx == this->next_write_idx;
	}
	/**
	 * Determine whether the ring buffer is full.
	 * \return true if the ring buffer is full, otherwise false
	 */
	inline bool full() {
		return (this->length() == this->maxlength());
	}

	/**
	 * Set up buffers for DMA-style input.
	 *
	 * You may fill the returned contiguous buffer externally with up to
	 * `maxitems` items.  Additions will be reflected in the state of this
	 * object upon completion of the DMA-style transaction with
	 * #notify_dma_input_occurred().
	 *
	 * \param[out] data_start  Returns a pointer to the start of the input
	 *                         buffer prepared by this function.
	 *
	 * \param[out] maxitems    Indicates the number of items that may be written
	 * 						   to the provided buffer.
	 *
	 * \note It may be necessary to use multiple DMA-like operations to fill
	 *       this ring buffer.
	 *
	 * \warning You MUST NOT overlap DMA-style input and calls to #write().
	 *
	 * \see #notify_dma_input_occurred()
	 */
	void setup_dma_input(T **data_start,
			size_t *maxitems) {
		if (!IN_INTERRUPT())
			portENTER_CRITICAL();

		if (this->full()) {
			*maxitems = 0;
		} else if (this->empty()) {
			*data_start = this->buffer;
			*maxitems = this->maxlength();
			this->next_read_idx = 0;
			this->next_write_idx = 0;
		} else if (this->next_write_idx > this->next_read_idx) {
			// The next write goes into the end of the physical buffer, and free
			// space wraps.
			*data_start = &(this->buffer[this->next_write_idx]);
			*maxitems = this->buflen - this->next_write_idx;

			// If we were to completely fill the physical buffer, next_read_idx
			// would equal next_write_idx and the buffer would be "empty".
			if (this->next_read_idx == 0) *maxitems -= 1;
		} else {
			// The next write goes into the beginning of the physical buffer,
			// and free space does not wrap.
			*data_start = &(this->buffer[this->next_write_idx]);

			// If we were to completely fill the physical buffer, next_read_idx
			// would equal next_write_idx and the buffer would be "empty".
			*maxitems = this->next_read_idx - this->next_write_idx - 1;
		}

		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();
	}

	/**
	 * Notify this object that DMA-style input has occurred.
	 *
	 * \param items  The number of items copied into the ring buffer by this
	 *               DMA-style operation.
	 *
	 * \note You may call this function multiple times.  You do not need to
	 *       re-call setup_dma_input() unless you have run out of buffer.
	 *
	 * \see #setup_dma_input()
	 */
	void notify_dma_input_occurred(
			size_t items) {
		if (!IN_INTERRUPT())
			portENTER_CRITICAL();

		configASSERT(this->length() + items <= this->maxlength());
		this->next_write_idx = (this->next_write_idx + items) & this->maxlen;

		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();
	}

	/**
	 * Set up buffers for DMA-style output.
	 *
	 * You may drain the returned contiguous buffer externally of up to
	 * `maxitems` items.  Removals will be reflected in the state of this object
	 * upon completion of the DMA-style transaction with
	 * #notify_dma_output_occurred().
	 *
	 * \param[out] data_start  Returns a pointer to the start of the output
	 *                         buffer prepared by this function.
	 *
	 * \param[out] maxitems    Indicates the number of items that may be read
	 * 						   from the provided buffer.
	 *
	 * \note It may be necessary to use multiple DMA-like operations to drain
	 *       this ring buffer.
	 *
	 * \warning You MUST NOT overlap DMA-style output and calls to #read().
	 *
	 * \see #notify_dma_output_occurred()
	 */
	void setup_dma_output(T **data_start,
			size_t *maxitems) {
		if (!IN_INTERRUPT())
			portENTER_CRITICAL();

		if (this->empty()) {
			*maxitems = 0;
		} else if (this->next_write_idx > this->next_read_idx) {
			// The current buffer contents are linear in memory.
			*data_start = &(this->buffer[this->next_read_idx]);
			*maxitems = this->next_write_idx - this->next_read_idx;
		} else {
			// The current buffer contents are nonlinear in memory.
			*data_start = &(this->buffer[this->next_read_idx]);
			*maxitems = this->buflen - this->next_read_idx;
		}

		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();
	}

	/**
	 * Notify this object that DMA-style output is complete.
	 *
	 * \param items  The number of items copied out of the ring buffer by this
	 *               DMA-style operation.
	 *
	 * \note You may call this function multiple times.  You do not need to
	 *       re-call setup_dma_output() unless you have run out of buffer.
	 *
	 * \see #setup_dma_output()
	 */
	void notify_dma_output_occurred(
			size_t items) {
		if (!IN_INTERRUPT())
			portENTER_CRITICAL();

		configASSERT(items <= this->length());
		this->next_read_idx = (this->next_read_idx + items) & this->maxlen;

		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();
	}

	/**
	 * Release the allocated buffer.
	 */
	~RingBuffer() {
		free(this->buffer);
	}
};

#endif /* UW_IPMC_LIBS_RINGBUFFER_H_ */
