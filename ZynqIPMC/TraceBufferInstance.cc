#include <drivers/tracebuffer/tracebuffer.h>
#include <FreeRTOS.h>
#include <libs/ThreadingPrimitives.h>

#define TRACEBUFFER_SIZE (1*1024*1024)

static uint8_t tracebuffer_contents[TRACEBUFFER_SIZE];
static TraceBuffer *trace_buffer = NULL;
static uint8_t tracebuffer_object_memory[sizeof(TraceBuffer)];

/* We will want this to run as a very early constructor regardless, so that no
 * matter how early in the boot process we crash, chances are the tracebuffer
 * data is still valid, even if it is empty.
 *
 * 101 is the earliest non-reserved constructor priority value, according to my
 * understanding.
 */
TraceBuffer& get_tracebuffer() __attribute__((constructor(101)));

/**
 * Instantiate (if required) and retrieve the TraceBuffer.
 * @return A functional TraceBuffer by reference.
 */
TraceBuffer& get_tracebuffer() {
	// Hopefully this will all just be done already and ready to use.
	if (trace_buffer)
		return *trace_buffer;

	CriticalGuard critical(true);
	/* Sadly can't get away with doing this in advance, because it'll modify the
	 * static array.  Luckily it's a quick operation.
	 *
	 * Unfortunately we probably also can't call new, because this might even
	 * be happening in an ISR.  Therefore we will use placement new on a
	 * preallocated buffer.
	 */
	if (!trace_buffer)
		trace_buffer = new (tracebuffer_object_memory) TraceBuffer(tracebuffer_contents, TRACEBUFFER_SIZE);
	return *trace_buffer;
}
