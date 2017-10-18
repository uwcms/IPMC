#include <libs/RingBuffer.h>
#include <libs/ThreadingPrimitives.h>

/**
 * Instantiate a new RingBuffer. Space for `items` items will be allocated on
 * the heap.
 *
 * \param items The number of `T` items that will fit in the allocated buffer.
 *
 * \note Implementation Detail:  The allocated buffer will be
 *       `sizeof(T)*(items+1)` bytes in length to support an "empty"/"full"
 *       indicator.
 */
template<typename T> RingBuffer<T>::RingBuffer(size_t items) :
		buflen(items + 1), next_read_idx(0), next_write_idx(0) {
	this->buffer = static_cast<T*>(malloc(this->buflen * sizeof(T)));
}

/**
 * Release the allocated buffer.
 */
template<typename T> RingBuffer<T>::~RingBuffer() {
	free(this->buffer);
}

/**
 * Write items into the ring buffer.
 *
 * \param data  A pointer to the data buffer to copy into the ringbuffer.
 * \param len   The number of data items to copy into the ring buffer.
 * \return      The number of items copied into the ring buffer successfully.
 */
template<typename T> size_t RingBuffer<T>::write(const T *data, size_t len) {
	size_t i = 0;
	if (!IN_INTERRUPT())
		portENTER_CRITICAL();
	for (i = 0; i < len; ++i) {
		if (this->full())
			break;
		configASSERT(this->next_write_idx < this->buflen);
		this->buffer[this->next_write_idx] = *(data++);
		this->next_write_idx = (this->next_write_idx + 1) % this->buflen;
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
template<typename T> size_t RingBuffer<T>::read(T *data, size_t len) {
	size_t i = 0;
	if (!IN_INTERRUPT())
		portENTER_CRITICAL();
	for (i = 0; i < len && !this->empty(); ++i) {
		configASSERT(this->next_read_idx < this->buflen);
		*(data++) = this->buffer[this->next_read_idx];
		this->next_read_idx = (this->next_read_idx + 1) % this->buflen;
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
template<typename T> size_t RingBuffer<T>::length() {
	if (!IN_INTERRUPT())
		portENTER_CRITICAL();
	size_t ret = (this->next_write_idx - this->next_read_idx) % this->buflen;
	if (!IN_INTERRUPT())
		portEXIT_CRITICAL();
	return ret;
}

/**
 * Return the maximum number of items which may be stored in the ring buffer.
 *
 * \return The maximum number of items which may be stored in the ring buffer.
 */
template<typename T> size_t RingBuffer<T>::maxlength() {
	return this->buflen - 1;
}

/**
 * Set up buffers for DMA-style input.
 *
 * You may fill the returned contiguous buffer externally with up to `maxitems`
 * items.  Additions will be reflected in the state of this object upon
 * completion of the DMA-style transaction with #notify_dma_input_occurred().
 *
 * \param[out] data_start  Returns a pointer to the start of the input buffer
 *                         prepared by this function.
 *
 * \param[out] maxitems    Indicates the number of items that may be written to
 * 						   the provided buffer.
 *
 * \note It may be necessary to use multiple DMA-like operations to fill this
 *       ring buffer.
 *
 * \warning You MUST NOT overlap DMA-style input and calls to #write().
 *
 * \see #notify_dma_input_occurred()
 */
template<typename T> void RingBuffer<T>::setup_dma_input(T **data_start,
		size_t *maxitems) {
	if (!IN_INTERRUPT())
		portENTER_CRITICAL();
	if (this->full()) {
		*maxitems = 0;
		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();
		return;
	}
	if (this->next_write_idx > this->next_read_idx) {
		// The next write goes into the end of the physical buffer, and free
		// space wraps.
		*data_start = &(this->buffer[this->next_write_idx]);
		*maxitems = this->buflen - this->next_write_idx;

		// If we were to completely fill the physical buffer, next_read_idx
		// would equal next_write_idx and the buffer would be "empty".
		if (this->next_read_idx == 0)
			*maxitems -= 1;
	} else {
		// The next write goes into the beginning of the physical buffer, and
		// free space does not wrap.
		*data_start = &(this->buffer[this->next_write_idx]);

		// If we were to completely fill the physical buffer, next_read_idx
		// would equal next_write_idx and the buffer would be "empty".
		*maxitems = (this->next_read_idx - this->next_write_idx) - 1;
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
 * \note You may call this function multiple times.  You do not need to re-call
 *       setup_dma_input() unless you have run out of buffer.
 *
 * \see #setup_dma_input()
 */
template<typename T> void RingBuffer<T>::notify_dma_input_occurred(
		size_t items) {
	if (!IN_INTERRUPT())
		portENTER_CRITICAL();
	configASSERT(this->length() + items <= this->maxlength());
	this->next_write_idx = (this->next_write_idx + items) % this->buflen;
	if (!IN_INTERRUPT())
		portEXIT_CRITICAL();
}

/**
 * Set up buffers for DMA-style output.
 *
 * You may drain the returned contiguous buffer externally of up to `maxitems`
 * items.  Removals will be reflected in the state of this object upon
 * completion of the DMA-style transaction with #notify_dma_output_occurred().
 *
 * \param[out] data_start  Returns a pointer to the start of the output buffer
 *                         prepared by this function.
 *
 * \param[out] maxitems    Indicates the number of items that may be read from
 * 						   the provided buffer.
 *
 * \note It may be necessary to use multiple DMA-like operations to drain this
 *       ring buffer.
 *
 * \warning You MUST NOT overlap DMA-style output and calls to #read().
 *
 * \see #notify_dma_output_occurred()
 */
template<typename T> void RingBuffer<T>::setup_dma_output(T **data_start,
		size_t *maxitems) {
	if (!IN_INTERRUPT())
		portENTER_CRITICAL();
	if (this->empty()) {
		*maxitems = 0;
		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();
		return;
	}
	if (this->next_write_idx > this->next_read_idx) {
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
 * \note You may call this function multiple times.  You do not need to re-call
 *       setup_dma_output() unless you have run out of buffer.
 *
 * \see #setup_dma_output()
 */
template<typename T> void RingBuffer<T>::notify_dma_output_occurred(
		size_t items) {
	if (!IN_INTERRUPT())
		portENTER_CRITICAL();
	configASSERT(items <= this->length());
	this->next_read_idx = (this->next_read_idx + items) % this->buflen;
	if (!IN_INTERRUPT())
		portEXIT_CRITICAL();
}

// Explicit instantiations.
template class RingBuffer<u8> ;
