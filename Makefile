obj-m += hidefs.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


### In case I ever publish the implementation that uses Khook to hook into functions instead of Kprobes (which has a blacklist and WONT let me access the readdir family of functions!)

#obj-m += hidefs.o

# Include the KHOOK Makefile
# include $(PWD)/khook/Makefile.khook
# 
# # Add KHOOK goals and flags
# $(MODNAME)-y += $(KHOOK_GOALS)
# ccflags-y    += $(KHOOK_CCFLAGS)
# ldflags-y    += $(KHOOK_LDFLAGS)

# all:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
# 
# clean:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
# 
