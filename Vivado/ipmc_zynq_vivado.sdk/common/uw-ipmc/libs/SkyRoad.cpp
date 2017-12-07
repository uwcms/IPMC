#include <libs/SkyRoad.h>

// Static storage locations for SkyRoad static variables.
volatile SemaphoreHandle_t SkyRoad::mutex;
volatile uint32_t SkyRoad::anonymizer_inc;
std::map<std::string, SkyRoad::Hermes*> * volatile SkyRoad::phonebook = NULL;;

/* Instantiate these to check for compile-time errors in the template classes.
 * If unused they will probably be optimized away by the linker.
 */
template class SkyRoad::Messenger<int>;
template class SkyRoad::Envelope::Scroll<int>;
