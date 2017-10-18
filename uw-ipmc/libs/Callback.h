#ifndef _CALLBACK_H
#define _CALLBACK_H

// callback<void> c1 = callback<void>::create<Class, &Class::method>(&object);
// c1 = callback<void>::create<&func>();
template <typename RET> class callback {
	private:
		void *obj;
		RET (*method)(void *, void *);
		template<typename ObjT, RET (ObjT::*M)(void *)> static RET thunk (void *obj, void *data) { return (reinterpret_cast<ObjT*>(obj)->*M)(data); }
		template<RET (*M)(void *)> static RET thunk (void *obj, void *data) { return M(data); }
		callback<RET>(void *obj, RET (*method)(void *, void*))
			: obj(obj), method(method) { };
	public:
		template <typename ObjT, RET (ObjT::*M)(void *)> static callback<RET> create(ObjT *obj) {
			return callback<RET>(obj,&thunk<ObjT, M>);
		}
		template <RET (*M)(void *)> static callback<RET> create() {
			return callback<RET>(NULL,&thunk<M>);
		}
		RET operator()(void *data) {
			return method(obj, data);
		}
};

#endif
