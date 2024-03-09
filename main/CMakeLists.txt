idf_component_register(SRCS "wifi_tools.c"
                    INCLUDE_DIRS "")
# Flags for overriding the ieee80211_raw_frame_sanity_check, for allowing us to use raw action frames.
target_link_libraries(${COMPONENT_LIB} -Wl,-zmuldefs)
