#
# Makefile for the Marvell Gigabit Ethernet driver
#

ifeq ($(CONFIG_ARCH_FEROCEON_ORION),y)
	include $(srctree)/arch/arm/mach-feroceon-orion/config/mvRules.mk
endif

ifeq ($(CONFIG_ARCH_FEROCEON_KW),y)
	include $(srctree)/arch/arm/mach-kirkwood/config/mvRules.mk
endif

ifeq ($(CONFIG_ARCH_FEROCEON_MV78XX0),y)
	include $(srctree)/arch/arm/mach-feroceon-mv78xx0/config/mvRules.mk
endif

obj-$(CONFIG_MV_ETHERNET) += mv_netdev.o
obj-$(CONFIG_MV_ETH_PROC) += mv_eth_proc.o
obj-$(CONFIG_MV_GATEWAY) +=  mv_gateway.o
obj-$(CONFIG_MV_GTW_IGMP) += mv_gtw_igmp.o
obj-$(CONFIG_MV_ETH_TOOL) += mv_eth_tool.o

obj-$(CONFIG_MV_XCAT98DX) += mv_xcat_pp/
