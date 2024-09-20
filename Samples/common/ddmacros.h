/***************************************

	Useful macros

***************************************/

#ifndef __DDMACROS_H__
#define __DDMACROS_H__

/***************************************

	Helpful worker macros

***************************************/

#define SAFE_DELETE(p) \
	{ \
		if (p) { \
			delete (p); \
			(p) = NULL; \
		} \
	}

#define SAFE_DELETE_ARRAY(p) \
	{ \
		if (p) { \
			delete[] (p); \
			(p) = NULL; \
		} \
	}

#define SAFE_RELEASE(p) \
	{ \
		if (p) { \
			(p)->Release(); \
			(p) = NULL; \
		} \
	}

#endif
