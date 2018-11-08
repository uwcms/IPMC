#ifndef SRC_COMMON_UW_IPMC_LIBS_SKYROAD_H_
#define SRC_COMMON_UW_IPMC_LIBS_SKYROAD_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <list>
#include <IPMC.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/StatCounter.h>
#include <libs/LogTree.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <typeinfo>

/**
 * Hermes the Messenger, SkyRoad Central Message Bus.
 *
 * Call SkyRoad:request_messenger() to receive a Messenger object for a specific
 * topic.  You may then send messages to this topic with
 * Messenger.send().
 *
 * Messages may be received by either callbacks registered with the Messenger,
 * which will run in the calling thread during Messneger.send(), or by a Temple
 * which has registered to receive interprocess deliveries from that Messenger.
 *
 * After creating a Temple and subscribing to a Messenger with it, you may call
 * Temple.receive(), which will return an Envelope object.  Compare
 * Envelope.messenger to determine which of the messengers your Temple may be
 * subscribed to delivered this message, then call Envelope.open() to receive
 * the message content.
 *
 * This messaging system is strongly typed.  A given Messenger<T> instance
 * carries messages of one specific predetermined type, so you always know
 * what you are getting.  You must specify this same type to Envelope.open<T>()
 * in order to receive your content.  An incorrect type specification will lead
 * to an assert.
 *
 * Timeouts are available for receipt of messages, but message sending has the
 * potential to block and does not offer timeouts.  This happens only when the
 * delivery queues of one or more Temples are full, and only until space is
 * available for delivery.  If a sender blocks for sufficiently long, a deadlock
 * detection mechanism will activate.
 */
class SkyRoad {
public:
	/**
	 * Hermes is the base class of all `Messenger`s.
	 *
	 * This class contains the main functionality of a Messenger except for the
	 * Messenger.send() function.  It cannot be instantiated directly,
	 * instantiate a Messenger instead.
	 */
	class Hermes {
	public:
		const std::string address; ///< The address/name of this messenger.

		/// \name Configuration Constants:
		///@{
		/**
		 * The backoff between successive send() attempts to multiple blocking
		 * queues starts as 1 and is multiplied by this value every pass.
		 */
		const TickType_t send_block_backoff_multiplier = 2;
		/**
		 * The backoff between successive send() attempts to multiple blocking
		 * queues never exceeds this value.
		 */
		const TickType_t send_block_backoff_cap = 20/portTICK_RATE_MS; // 20ms
		/**
		 * If we have waited this many ticks in total to complete a single
		 * send() operation, we will trigger deadlock detection.
		 */
		const TickType_t send_block_deadlock_duration = configTICK_RATE_HZ/10; // 0.1s
		///@}
	protected:
		LogTree &log_root; ///< The logtree root for this messenger.
		const std::string type_name; ///< The type name in human readable format, cached for logging.
		/**
		 * The superclass constructor for all `Messenger`s.
		 * \param address  The address this messenger accepts deliveries for.
		 * \param logtree  The logtree facility for this messenger.
		 * \param type_name  The name of the type of content associated with this messenger.
		 */
		Hermes(const std::string address, LogTree &logtree, const std::string type_name)
	      : address(address), log_root(logtree), type_name(type_name),
			deliveries(stdsprintf("skyroad.[%s].deliveries", address.c_str())),
			blocking_deliveries(stdsprintf("skyroad.[%s].blocking_deliveries", address.c_str())) {
			this->mutex = xSemaphoreCreateMutex();
			configASSERT(this->mutex);
		};
		template <typename T> friend class Messenger;
	public:
		virtual ~Hermes() {
			configASSERT(this->inboxes.size() == 0);
			vSemaphoreDelete(this->mutex);
		};

	protected:
		std::list<QueueHandle_t> inboxes; ///< Temples receiving deliveries from this messenger.
		SemaphoreHandle_t mutex;          ///< Mutex to protect the registration lists.
		StatCounter deliveries;           ///< A count of deliveries to this topic.
		StatCounter blocking_deliveries;  ///< A count of blocking deliveries to this topic.
		static StatCounter global_deliveries; ///< A count of deliveries globally.
		static StatCounter global_blocking_deliveries; ///< A count of blocking deliveries globally.

		/**
		 * Helper function used by temples subscribing to this Messenger.
		 *
		 * \param queue  The FreeRTOS queue used as an inbox by the temple.
		 */
		void temple_subscribe(QueueHandle_t queue) {
			xSemaphoreTake(this->mutex, portMAX_DELAY);
			this->inboxes.push_back(queue);
			this->log_root.log(
					stdsprintf(
							"Messenger<%s>(\"%s\") had a Temple subscribe.  Now with %d temples.",
							this->type_name.c_str(), this->address.c_str(), this->inboxes.size()),
					LogTree::LOG_INFO);
			xSemaphoreGive(this->mutex);
		}
		friend class Temple;

		/**
		 * Helper function used by temples unsubscribing from this Messenger.
		 *
		 * \param queue  The FreeRTOS queue used as an inbox by the temple.
		 */
		void temple_unsubscribe(QueueHandle_t queue) {
			xSemaphoreTake(this->mutex, portMAX_DELAY);
			this->inboxes.remove(queue);
			this->log_root.log(
					stdsprintf(
							"Messenger<%s>(\"%s\") had a Temple unsubscribe.  Now with %d temples.",
							this->type_name.c_str(), this->address.c_str(), this->inboxes.size()),
					LogTree::LOG_INFO);
			xSemaphoreGive(this->mutex);
		}
		friend class Temple;

		// Not copyable or assignable.
		Hermes(const Hermes&) = delete;
		Hermes& operator= (const Hermes&) = delete;
	}; // class Hermes

	/**
	 * A Messenger delivers messages of a specific type and purpose to Temples
	 * that are interested in them.
	 */
	template <typename T> class Messenger : public Hermes {
	protected:
		const std::string delivery_made_log_message; ///< A time/space/beauty tradeoff made in favor of time (on every message delivery).
		/**
		 * Instantiate a new Messenger.
		 *
		 * Internal to SkyRoad::request_messenger().
		 *
		 * \param address  The address this messenger accepts deliveries for.
		 * \param logtree  The logtree facility for this messenger.
		 */
		Messenger(std::string address, LogTree &logtree)
		    : Hermes(address, logtree, cxa_demangle(typeid(T).name())),
			  delivery_made_log_message(stdsprintf("Messenger<%s>(\"%s\") made a delivery.", cxa_demangle(typeid(T).name()).c_str(), address.c_str())),
			  last_cbid(0)
			  { };
		friend class SkyRoad;
		~Messenger() {
			configASSERT(this->callbacks.size() == 0);
		}

	public:
		/**
		 * The type of a callback function which may be registered with this
		 * Messenger to complete processless event triggers.
		 *
		 * \warning These will execute in the sender thread, regardless of its
		 *          priority.  Be fast.  Be careful with blocking.  Know what
		 *          you're doing.  There are zero ordering guarantees.
		 *
		 * \param content A reference to the message content.
		 * \return true if this callback should remain registered, else false
		 */
		typedef std::function<bool(T& content)> callback_t;

		/**
		 * Register an inline callback to receive messages sent through this
		 * Messenger.
		 *
		 * \param callback  The callback to register.
		 * \return          A handle to unregister the callback.
		 */
		uint32_t callback_subscribe(callback_t callback) {
			xSemaphoreTake(this->mutex, portMAX_DELAY);
			while (!this->callbacks.insert(std::make_pair(++this->last_cbid, callback)).second)
				;
			uint32_t cbid = this->last_cbid;
			this->log_root.log(
					stdsprintf(
							"Messenger<%s>(\"%s\") had a callback explicitly subscribe.  Now with %d callbacks and %d temples.",
							this->type_name.c_str(), this->address.c_str(), this->callbacks.size(), this->inboxes.size()),
					LogTree::LOG_INFO);
			xSemaphoreGive(this->mutex);
			return cbid;
		};

		/**
		 * Unregister an inline callback using the handle provided by
		 * #callback_subscribe().
		 *
		 * \param callback_handle  The handle returned by #callback_subscribe().
		 * \return                 true if the callback was removed, else false.
		 */
		bool callback_unsubscribe(uint32_t callback_handle) {
			xSemaphoreTake(this->mutex, portMAX_DELAY);
			size_t n_erased = this->callbacks.erase(callback_handle);
			this->log_root.log(
					stdsprintf(
							"Messenger<%s>(\"%s\") had a callback explicitly unsubscribe.  Now with %d callbacks and %d temples.",
							this->type_name.c_str(), this->address.c_str(), this->callbacks.size(), this->inboxes.size()),
					LogTree::LOG_INFO);
			xSemaphoreGive(this->mutex);
			return (bool)n_erased;
		};

		/**
		 * Deliver a message to all subscribing Temples and callbacks.
		 *
		 * \param content  The message to be delivered.
		 */
		void send(std::shared_ptr<T> content) {
			xSemaphoreTake(this->mutex, portMAX_DELAY);
			std::list<QueueHandle_t> targets(this->inboxes); // Take a copy to work through.

			this->deliveries.increment();
			Hermes::global_deliveries.increment();

			// Step 1: Deliver to all queues with space, so they aren't blocked by callbacks.
			Envelope::Scroll<T> *next_delivery = NULL;
			if (!targets.empty()) {
				// We have at least one delivery to do.  Prepare its Scroll.
				next_delivery = new Envelope::Scroll<T>(this, content);
			}
			for (auto it = targets.begin(); it != targets.end(); ) {
				if (pdTRUE == xQueueSend(*it, next_delivery, 0)) {
					it = targets.erase(it);
					if (!targets.empty()) {
						/* We delivered our Scroll and still have a delivery left to do.
						 * Copy another one.
						 */
						next_delivery = new Envelope::Scroll<T>(this, content);
					}
				}
				else {
					++it; // Skip, but save for later.
				}
			}

			// Step 2: We're not blocking any queue that didn't block itself.  Run callbacks.
			for (auto it = this->callbacks.begin(); it != this->callbacks.end(); ) {
				if (it->second(*content))
					++it; // Remain registered.
				else
					it = this->callbacks.erase(it); // Deregister.
			}

			// Accumulate statistics on blocked queues.
			if (!targets.empty()) {
				uint64_t previous_blocking_deliveries =
						this->blocking_deliveries.increment();
				if (!previous_blocking_deliveries)
					this->log_root.log(
							stdsprintf(
									"Messenger<%s>(\"%s\") blocked on a full Temple queue.",
									this->type_name.c_str(),
									this->address.c_str()),
							LogTree::LOG_WARNING);
				Hermes::global_blocking_deliveries.increment();
			}

			/* Step 3: Deliver to remaining queues.
			 *
			 * Here's where it gets complex.  We can't use a QueueSet because
			 * apparently they're only for receivers, so we need to come up with
			 * a fair-as-possible way of blocking on the remaining queues.
			 *
			 * So here's the plan:
			 *
			 * We're going to block on a queue for 1 tick, then sweep through
			 * all remaining queues.  If any were delivered we'll reset the
			 * backoff, else we'll increase it.
			 *
			 * If there's ever only one queue, we'll block indefinitely.
			 *
			 * We'll always pause to send a deadlock alert if needed.
			 */
			AbsoluteTimeout deadlock_timeout(send_block_deadlock_duration);
			TickType_t backoff = 1;
			bool deadlock_alert_triggered = false;
			while (!targets.empty()) {
				bool skip_backoff_increment = false;
				TickType_t current_wait = backoff;
				if (targets.size() == 1) {
					// Only one left.  Just wait for it.
					current_wait = portMAX_DELAY;
				}
				if (!deadlock_alert_triggered && current_wait > deadlock_timeout.get_timeout()) {
					/* If the deadlock alert hasn't yet been triggered, don't
					 * sleep longer than "until it should be".
					 */
					skip_backoff_increment = true;
					current_wait = deadlock_timeout.get_timeout();
				}
				for (auto it = targets.begin(); it != targets.end(); ) {
					if (pdTRUE == xQueueSend(*it, next_delivery, current_wait)) {
						it = targets.erase(it);
						if (!targets.empty()) {
							/* We delivered our Scroll and still have a delivery left to do.
							 * Copy another one.
							 */
							next_delivery = new Envelope::Scroll<T>(this, content);
						}
						// We made a successful delivery!
						backoff = 1;
						skip_backoff_increment = true;
					}
					else {
						++it; // Skip, but save for later.
					}
					current_wait = 0; // Continue through the full sweep before backing off again.
				}
				if (!skip_backoff_increment)
					backoff *= send_block_backoff_multiplier;
				if (backoff > send_block_backoff_cap)
					backoff = send_block_backoff_cap;
				if (!targets.empty() && deadlock_timeout.get_timeout() == 0) {
					configASSERT(0); // Deadlock Detected.  TODO: Alert the watchdog and allow it to manage this.
					deadlock_alert_triggered = true;
				}
			}
			xSemaphoreGive(this->mutex);

			this->log_root.log(this->delivery_made_log_message.c_str(), LogTree::LOG_TRACE);
		}

	protected:
		uint32_t last_cbid; ///< The next callback id to register.
		std::map<uint32_t, callback_t> callbacks; ///< Callbacks receiving deliveries from this messenger.
	}; // class Messenger

	/**
	 * A SkyRoad message envelope.  You should check which Messenger this
	 * Envelope was delivered by, then call the appropriate .open() instantiation
	 * to retrieve the message content.
	 */
	class Envelope {
	public:
		Hermes * const messenger; ///< The messenger that delivered this message.
		virtual ~Envelope() { };

	protected:
		/**
		 * Instantiate a Envelope.  Used only by Scroll<T>.
		 * \param messenger  The Messenger this Envelope is delivered by.
		 */
		Envelope(Hermes *messenger) : messenger(messenger) { };
		template <typename T> friend class Scroll;

		template <typename T> class Scroll;

	public:
		/**
		 * Open this envelope and return the message content.
		 *
		 * \return The message content.
		 */
		template <typename T> std::shared_ptr<T> open() {
			Scroll<T> *msg = dynamic_cast< Scroll<T>* >(this);
			configASSERT(msg);
			return msg->content;
		}
	}; // class Envelope

protected:
	/**
	 * A scroll containing the content of the Envelope.  This class should be
	 * accessed only via Envelope.  It is instantiated by
	 * Messenger.send().
	 */
	template <typename T> class Envelope::Scroll : public Envelope {
	protected:
		std::shared_ptr<T> content; ///< The content of the message.
		friend class Envelope;
		/**
		 * Instantiate a Scroll.  Used only by Messenger.send()
		 *
		 * \param messenger  The Messenger this Scroll is delivered by.
		 * \param content    The content of the message.
		 */
		Scroll(Hermes *messenger, std::shared_ptr<T> content)
		  : Envelope(messenger), content(content) { };
		friend class Messenger<T>;
	public:
		virtual ~Scroll() { };
	}; // class Scroll

public:
	/**
	 * Temple objects serve as delivery destinations.  They can receive messages
	 * from one or many Messengers.
	 *
	 * \warning A Temple may not be destructed unless it has no subscriptions
	 *          and no outstanding messages.  Note that you may be in the
	 *          process of receiving a message when Temple.unsubscribe()
	 *          returns, which has not yet arrived at Temple.receive().
	 *
	 * \see receive()
	 */
	class Temple {
	public:
		/**
		 * Instantiate a Temple object.
		 *
		 * \param queuesize  The size of the receive queue for this Temple.
		 */
		Temple(UBaseType_t queuesize=8) {
			this->mutex = xSemaphoreCreateMutex();
			configASSERT(this->mutex);
			this->queue = xQueueCreate(queuesize, sizeof(Envelope*));
			configASSERT(this->queue);
		}
		~Temple() {
			xSemaphoreTake(this->mutex, portMAX_DELAY);
			/* Unfortunately, any attempt to resolve this ourselves would result
			 * in at least the possibility of a deadlock situation with sender
			 * queues.  Fortunately, we don't really need to do this much.
			 */
			configASSERT(this->subscriptions.empty());
			configASSERT(!uxQueueMessagesWaiting(this->queue));
			xSemaphoreGive(this->mutex);
			vQueueDelete(this->queue);
			vSemaphoreDelete(this->mutex);
		}

		/**
		 * Subscribe to a messenger.
		 * \param messenger
		 */
		template <typename T> void subscribe(Messenger<T> *messenger) {
			configASSERT(messenger);
			xSemaphoreTake(this->mutex, portMAX_DELAY);
			this->subscriptions.push_back(static_cast<Hermes*>(messenger));
			messenger->temple_subscribe(this->queue);
			xSemaphoreGive(this->mutex);
		}

		/**
		 * Unsubscribe from a messenger.
		 * \param messenger
		 */
		template <typename T> void unsubscribe(Messenger<T> *messenger) {
			configASSERT(messenger);
			xSemaphoreTake(this->mutex, portMAX_DELAY);
			messenger->temple_unsubscribe(this->queue);
			this->subscriptions.remove(static_cast<Hermes*>(messenger));
			xSemaphoreGive(this->mutex);
		}

		/**
		 * Receive a message queued for delivery to this Temple.
		 *
		 * This returns a `std::shared_ptr<Envelope>`.  Use it as:
		 * \code
		 * std::shared_ptr<SkyRoad::Envelope> envelope = temple.receive(0);
		 * if (!envelope)
		 *     continue; // Timed out.
		 * if (envelope->messenger == expected_messenger_1) {
		 *     std::shared_ptr<ContentType> content = envelope->open();
		 *     // Do work.
		 * }
		 * \endcode
		 *
		 * @param timeout
		 * @return A `std::shared_ptr<Envelope>` containing your message or empty on timeout.
		 */
		std::shared_ptr<Envelope> receive(TickType_t timeout) {
			Envelope *incoming;
			if (!xQueueReceive(this->queue, &incoming, timeout))
				return std::shared_ptr<Envelope>(); // return NULL

			/* The Envelope (i.e. Scroll<T>) object will now be owned by this
			 * shared_ptr and deleted with `delete` at cleanup time, calling the
			 * virtual destructor on the Scroll<T> via Envelope.
			 */
			return std::shared_ptr<Envelope>(incoming);
		}

		/**
		 * Return a handle to the internal queue, for use in QueueSets
		 *
		 * \warning Do not use this in any way or for any purpose other than
		 *          QueueSets.
		 *
		 * \warning Be reminded that you are only permitted to add empty queues
		 *          to a QueueSet, and may only call receive() on a Temple which
		 *          has been added to a QueueSet when that QueueSet has returned
		 *          it from a select call.
		 *
		 * \return  A handle to the internal queue.
		 */
		QueueHandle_t get_queue() {
			return this->queue;
		}

	protected:
		QueueHandle_t queue; ///< The receive queue.
		SemaphoreHandle_t mutex; ///< A mutex protecting the subscription list.
		std::list<Hermes*> subscriptions; ///< A list of messengers we are subscribed to, for cleanup.
	}; // class Temple

public:
	/**
	 * Request a Messenger for a given topic, creating or retrieving the
	 * Messenger associated with that topic.
	 *
	 * \note You should call this function only once, and keep a copy of the
	 *       returned Messenger pointer.  It is not designed for every-access
	 *       efficiency.
	 *
	 * \note Messenger topics should be in a reverse dotted format:
	 *       `mymodule.myinstance.myevent` or similar.
	 *
	 * \note Messenger topics must be unique unless anonymize is set to true.
	 *       Additional .register_topic() calls will return the same Messenger
	 *       instance.  If anonymize is set to true, the messenger address will
	 *       be made unique and it will not be possible to look up the messenger
	 *       through another call to this function.  You should still use a
	 *       descriptive name, as it may be used for debug or trace output.
	 *
	 * \param topic      The topic for this Messenger.
	 * \param anonymize  Whether to return an anyonymous Messenger.
	 * \return The requested Messenger
	 */
	template <typename T> static Messenger<T> *request_messenger(const std::string topic, bool anonymize=false) {
		configASSERT(topic.find("/") == std::string::npos); // Don't allow re-lookup of an anonymized name.

		if (!SkyRoad::mutex) {
			SemaphoreHandle_t new_mutex = xSemaphoreCreateMutex();
			taskENTER_CRITICAL();
			if (!SkyRoad::mutex) {
				SkyRoad::mutex = new_mutex;
				new_mutex = NULL;
			}
			taskEXIT_CRITICAL();
			if (new_mutex)
				vSemaphoreDelete(new_mutex);
		}
		xSemaphoreTake(SkyRoad::mutex, portMAX_DELAY);
		if (!SkyRoad::phonebook)
			SkyRoad::phonebook = new std::map<std::string, Hermes*>();
		if (!SkyRoad::log_root)
			SkyRoad::log_root = &LOG["skyroad"];
		if (!SkyRoad::log_registration)
			SkyRoad::log_registration = &LOG["skyroad"]["registration"];
		if (!SkyRoad::log_messengers)
			SkyRoad::log_messengers = &LOG["skyroad"]["messengers"];

		Messenger<T> *ret = NULL;
		if (!anonymize && 0 < SkyRoad::phonebook->count(topic)) {
			ret = dynamic_cast< Messenger<T>* >(SkyRoad::phonebook->at(topic));
			configASSERT(ret);
		}
		else {
			if (anonymize) {
				std::string anon_name;
				do {
					anon_name = stdsprintf("%s/%u", topic.c_str(), static_cast<unsigned int>(SkyRoad::anonymizer_inc++));
				} while (0 < SkyRoad::phonebook->count(anon_name));
				ret = new Messenger<T>(anon_name, (*SkyRoad::log_messengers)[stdsprintf("[%s]", anon_name.c_str())]);
			}
			else {
				ret = new Messenger<T>(topic, (*SkyRoad::log_messengers)[stdsprintf("[%s]", topic.c_str())]);
			}
			SkyRoad::phonebook->insert(std::pair<std::string, Hermes*>(ret->address, ret));
			SkyRoad::log_registration->log(stdsprintf("Messenger<%s>(\"%s\") was created.", ret->type_name.c_str(), ret->address.c_str()), LogTree::LOG_INFO);
		}
		xSemaphoreGive(SkyRoad::mutex);
		SkyRoad::log_registration->log(stdsprintf("Messenger<%s>(\"%s\") was retrieved.", ret->type_name.c_str(), ret->address.c_str()), LogTree::LOG_DIAGNOSTIC);
		return ret;
	};

protected:
	static volatile SemaphoreHandle_t mutex; ///< A mutex protecting the phonebook.
	static volatile uint32_t anonymizer_inc; ///< An auto-increment for the anonymize=true option to register_topic().
	static std::map<std::string, Hermes*> * volatile phonebook; ///< A mapping of all existing messengers.

	static LogTree *log_root; ///< Root logtree for SkyRoad
	static LogTree *log_registration; ///< Logtree for messenger creation & registration
	static LogTree *log_messengers; ///< Logtree for messenger subtrees
}; // class SkyRoad

#endif /* SRC_COMMON_UW_IPMC_LIBS_SKYROAD_H_ */
