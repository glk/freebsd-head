/******************************************************************************/
#ifdef JEMALLOC_H_TYPES

/* Maximum number of malloc_tsd users with cleanup functions. */
#define	MALLOC_TSD_CLEANUPS_MAX	8

typedef struct malloc_tsd_cleanup_s malloc_tsd_cleanup_t;
struct malloc_tsd_cleanup_s {
	bool	(*f)(void *);
	void	*arg;
};

/*
 * TLS/TSD-agnostic macro-based implementation of thread-specific data.  There
 * are four macros that support (at least) three use cases: file-private,
 * library-private, and library-private inlined.  Following is an example
 * library-private tsd variable:
 *
 * In example.h:
 *   typedef struct {
 *           int x;
 *           int y;
 *   } example_t;
 *   #define EX_INITIALIZER JEMALLOC_CONCAT({0, 0})
 *   malloc_tsd_protos(, example, example_t *)
 *   malloc_tsd_externs(example, example_t *)
 * In example.c:
 *   malloc_tsd_data(, example, example_t *, EX_INITIALIZER)
 *   malloc_tsd_funcs(, example, example_t *, EX_INITIALIZER,
 *       example_tsd_cleanup)
 *
 * The result is a set of generated functions, e.g.:
 *
 *   bool example_tsd_boot(void) {...}
 *   example_t **example_tsd_get() {...}
 *   void example_tsd_set(example_t **val) {...}
 *
 * Note that all of the functions deal in terms of (a_type *) rather than
 * (a_type)  so that it is possible to support non-pointer types (unlike
 * pthreads TSD).  example_tsd_cleanup() is passed an (a_type *) pointer that is
 * cast to (void *).  This means that the cleanup function needs to cast *and*
 * dereference the function argument, e.g.:
 *
 *   void
 *   example_tsd_cleanup(void *arg)
 *   {
 *           example_t *example = *(example_t **)arg;
 *
 *           [...]
 *           if ([want the cleanup function to be called again]) {
 *                   example_tsd_set(&example);
 *           }
 *   }
 *
 * If example_tsd_set() is called within example_tsd_cleanup(), it will be
 * called again.  This is similar to how pthreads TSD destruction works, except
 * that pthreads only calls the cleanup function again if the value was set to
 * non-NULL.
 */

/* malloc_tsd_protos(). */
#define	malloc_tsd_protos(a_attr, a_name, a_type)			\
a_attr bool								\
a_name##_tsd_boot(void);						\
a_attr a_type *								\
a_name##_tsd_get(void);							\
a_attr void								\
a_name##_tsd_set(a_type *val);

/* malloc_tsd_externs(). */
#ifdef JEMALLOC_MALLOC_THREAD_CLEANUP
#define	malloc_tsd_externs(a_name, a_type)				\
extern __thread a_type	a_name##_tls;					\
extern __thread bool	a_name##_initialized;				\
extern bool		a_name##_booted;
#elif (defined(JEMALLOC_TLS))
#define	malloc_tsd_externs(a_name, a_type)				\
extern __thread a_type	a_name##_tls;					\
extern pthread_key_t	a_name##_tsd;					\
extern bool		a_name##_booted;
#else
#define	malloc_tsd_externs(a_name, a_type)				\
extern pthread_key_t	a_name##_tsd;					\
extern bool		a_name##_booted;
#endif

/* malloc_tsd_data(). */
#ifdef JEMALLOC_MALLOC_THREAD_CLEANUP
#define	malloc_tsd_data(a_attr, a_name, a_type, a_initializer)		\
a_attr __thread a_type JEMALLOC_TLS_MODEL				\
    a_name##_tls = a_initializer;					\
a_attr __thread bool JEMALLOC_TLS_MODEL					\
    a_name##_initialized = false;					\
a_attr bool		a_name##_booted = false;
#elif (defined(JEMALLOC_TLS))
#define	malloc_tsd_data(a_attr, a_name, a_type, a_initializer)		\
a_attr __thread a_type JEMALLOC_TLS_MODEL				\
    a_name##_tls = a_initializer;					\
a_attr pthread_key_t	a_name##_tsd;					\
a_attr bool		a_name##_booted = false;
#else
#define	malloc_tsd_data(a_attr, a_name, a_type, a_initializer)		\
a_attr pthread_key_t	a_name##_tsd;					\
a_attr bool		a_name##_booted = false;
#endif

/* malloc_tsd_funcs(). */
#ifdef JEMALLOC_MALLOC_THREAD_CLEANUP
#define	malloc_tsd_funcs(a_attr, a_name, a_type, a_initializer,		\
    a_cleanup)								\
/* Initialization/cleanup. */						\
a_attr bool								\
a_name##_tsd_cleanup_wrapper(void *arg)					\
{									\
	bool (*cleanup)(void *) = arg;					\
									\
	if (a_name##_initialized) {					\
		a_name##_initialized = false;				\
		cleanup(&a_name##_tls);					\
	}								\
	return (a_name##_initialized);					\
}									\
a_attr bool								\
a_name##_tsd_boot(void)							\
{									\
									\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		malloc_tsd_cleanup_register(				\
		    &a_name##_tsd_cleanup_wrapper, a_cleanup);		\
	}								\
	a_name##_booted = true;						\
	return (false);							\
}									\
/* Get/set. */								\
a_attr a_type *								\
a_name##_tsd_get(void)							\
{									\
									\
	assert(a_name##_booted);					\
	return (&a_name##_tls);						\
}									\
a_attr void								\
a_name##_tsd_set(a_type *val)						\
{									\
									\
	assert(a_name##_booted);					\
	a_name##_tls = (*val);						\
	if (a_cleanup != malloc_tsd_no_cleanup)				\
		a_name##_initialized = true;				\
}
#elif (defined(JEMALLOC_TLS))
#define	malloc_tsd_funcs(a_attr, a_name, a_type, a_initializer,		\
    a_cleanup)								\
/* Initialization/cleanup. */						\
a_attr bool								\
a_name##_tsd_boot(void)							\
{									\
									\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		if (pthread_key_create(&a_name##_tsd, a_cleanup) != 0)	\
			return (true);					\
	}								\
	a_name##_booted = true;						\
	return (false);							\
}									\
/* Get/set. */								\
a_attr a_type *								\
a_name##_tsd_get(void)							\
{									\
									\
	assert(a_name##_booted);					\
	return (&a_name##_tls);						\
}									\
a_attr void								\
a_name##_tsd_set(a_type *val)						\
{									\
									\
	assert(a_name##_booted);					\
	a_name##_tls = (*val);						\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		if (pthread_setspecific(a_name##_tsd,			\
		    (void *)(&a_name##_tls))) {				\
			malloc_write("<jemalloc>: Error"		\
			    " setting TSD for "#a_name"\n");		\
			if (opt_abort)					\
				abort();				\
		}							\
	}								\
}
#else
#define	malloc_tsd_funcs(a_attr, a_name, a_type, a_initializer,		\
    a_cleanup)								\
/* Data structure. */							\
typedef struct {							\
	bool	isstatic;						\
	bool	initialized;						\
	a_type	val;							\
} a_name##_tsd_wrapper_t;						\
/* Initialization/cleanup. */						\
a_attr void								\
a_name##_tsd_cleanup_wrapper(void *arg)					\
{									\
	a_name##_tsd_wrapper_t *wrapper = (a_name##_tsd_wrapper_t *)arg;\
									\
	if (a_cleanup != malloc_tsd_no_cleanup &&			\
	    wrapper->initialized) {					\
		wrapper->initialized = false;				\
		a_cleanup(&wrapper->val);				\
		if (wrapper->initialized) {				\
			/* Trigger another cleanup round. */		\
			if (pthread_setspecific(a_name##_tsd,		\
			    (void *)wrapper)) {				\
				malloc_write("<jemalloc>: Error"	\
				    " setting TSD for "#a_name"\n");	\
				if (opt_abort)				\
					abort();			\
			}						\
			return;						\
		}							\
	}								\
	if (wrapper->isstatic == false)					\
		malloc_tsd_dalloc(wrapper);				\
}									\
a_attr bool								\
a_name##_tsd_boot(void)							\
{									\
									\
	if (pthread_key_create(&a_name##_tsd,				\
	    a_name##_tsd_cleanup_wrapper) != 0)				\
		return (true);						\
	a_name##_booted = true;						\
	return (false);							\
}									\
/* Get/set. */								\
a_attr a_name##_tsd_wrapper_t *						\
a_name##_tsd_get_wrapper(void)						\
{									\
	a_name##_tsd_wrapper_t *wrapper = (a_name##_tsd_wrapper_t *)	\
	    pthread_getspecific(a_name##_tsd);				\
									\
	if (wrapper == NULL) {						\
		wrapper = (a_name##_tsd_wrapper_t *)			\
		    malloc_tsd_malloc(sizeof(a_name##_tsd_wrapper_t));	\
		if (wrapper == NULL) {					\
			static a_name##_tsd_wrapper_t			\
			    a_name##_tsd_static_data =			\
			    {true, false, a_initializer};		\
			malloc_write("<jemalloc>: Error allocating"	\
			    " TSD for "#a_name"\n");			\
			if (opt_abort)					\
				abort();				\
			wrapper = &a_name##_tsd_static_data;		\
		} else {						\
			static a_type tsd_static_data = a_initializer;	\
			wrapper->isstatic = false;			\
			wrapper->val = tsd_static_data;			\
		}							\
		if (pthread_setspecific(a_name##_tsd,			\
		    (void *)wrapper)) {					\
			malloc_write("<jemalloc>: Error setting"	\
			    " TSD for "#a_name"\n");			\
			if (opt_abort)					\
				abort();				\
		}							\
	}								\
	return (wrapper);						\
}									\
a_attr a_type *								\
a_name##_tsd_get(void)							\
{									\
	a_name##_tsd_wrapper_t *wrapper;				\
									\
	assert(a_name##_booted);					\
	wrapper = a_name##_tsd_get_wrapper();				\
	return (&wrapper->val);						\
}									\
a_attr void								\
a_name##_tsd_set(a_type *val)						\
{									\
	a_name##_tsd_wrapper_t *wrapper;				\
									\
	assert(a_name##_booted);					\
	wrapper = a_name##_tsd_get_wrapper();				\
	wrapper->val = *(val);						\
	if (a_cleanup != malloc_tsd_no_cleanup)				\
		wrapper->initialized = true;				\
}
#endif

#endif /* JEMALLOC_H_TYPES */
/******************************************************************************/
#ifdef JEMALLOC_H_STRUCTS

#endif /* JEMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

void	*malloc_tsd_malloc(size_t size);
void	malloc_tsd_dalloc(void *wrapper);
void	malloc_tsd_no_cleanup(void *);
void	malloc_tsd_cleanup_register(bool (*f)(void *), void *arg);
void	malloc_tsd_boot(void);

#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES

#endif /* JEMALLOC_H_INLINES */
/******************************************************************************/
