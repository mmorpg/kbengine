LIB =	cstdkbe

SRCS =				\
	cstdkbe			\
	tasks			\
	timer			\
	timestamp		\
	strutil			\
	kbeversion		\
	md5			\
	memorystream

ifndef KBE_ROOT
export KBE_ROOT := $(subst /kbe/src/lib/$(LIB),,$(CURDIR))
endif

include $(KBE_ROOT)/kbe/src/build/common.mak