#include <libs/SkyRoad.h>

// Static storage locations for SkyRoad static variables.
volatile SemaphoreHandle_t SkyRoad::mutex;
volatile uint32_t SkyRoad::anonymizer_inc;
std::map<std::string, SkyRoad::Hermes*> SkyRoad::phonebook;

/* Instantiate these to check for compile-time errors in the template classes.
 * If unused they will probably be optimized away by the linker.
 */
template class SkyRoad::Messenger<int>;
template class SkyRoad::Envelope::Scroll<int>;

/**
 * Perform initialization of the static SkyRoad registry during pre-startup.
 */
static void init_skyroad(void) __attribute__((constructor(1000)));
static void init_skyroad(void) {
	SkyRoad::init();
}
