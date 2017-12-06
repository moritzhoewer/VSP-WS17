# name of your application
APPLICATION = vslab-riot

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../RIOT

NODES_NUM ?= 8

USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += gnrc_ipv6_default
USEMODULE += gcoap
USEMODULE += gnrc_icmpv6_echo
USEMODULE += gnrc_udp
USEMODULE += gnrc_sock_udp
USEMODULE += fmt
USEMODULE += xtimer

ifeq ($(BOARD),pba-d-01-kw2x)
	USEMODULE += hdc1000
endif

# Set this to 1 to enable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 0
CFLAGS += -DGCOAP_REQ_WAITING_MAX=$(NODES_NUM)
CFLAGS += -DELECT_NODES_NUM=$(NODES_NUM)
# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
