idf_component_register(SRCS "tabvia_usb.c" INCLUDE_DIRS "include"
  REQUIRES usb_host_hid hid_usage via_transport hid_console)
