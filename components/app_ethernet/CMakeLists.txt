idf_component_register(
	SRCS "ethernet_init.c" "app_eth.c"
	INCLUDE_DIRS "include"
	PRIV_INCLUDE_DIRS
	REQUIRES main
	PRIV_REQUIRES
)