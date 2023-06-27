add_executable (wsrp
	main.cc
		service.cpp
		service.h
		logger.h
		redpanda.cpp
		redpanda.h)
target_link_libraries (wsrp
	PRIVATE v::kafka_client)
set_property(TARGET wsrp PROPERTY POSITION_INDEPENDENT_CODE ON)
