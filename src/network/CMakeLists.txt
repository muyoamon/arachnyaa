add_library(network SHARED socket.c http.c cache.c url.c)
target_link_libraries(arachnyaac PUBLIC network)
