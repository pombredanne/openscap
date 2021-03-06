include(UseSWIG)
set(PYTHON_COMPILED_FILES openscap.pyc openscap.pyo openscap_api.pyc openscap_api.pyo)

include_directories(${PYTHON_INCLUDE_PATH})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_SWIG_FLAGS "-module" "openscap_py")
swig_add_module(openscap_py python ../openscap.i)
swig_link_libraries(openscap_py ${PYTHON_LIBRARIES} openscap ${LIBXML2_LIBRARIES} ${LIBXSLT_LIBRARIES} ${LIBXSLT_EXSLT_LIBRARIES} ${PCRE_LIBRARIES} ${CURL_LIBRARIES} ${BZIP2_LIBRARIES} ${RPM_LIBRARIES})

add_custom_command(OUTPUT ${PYTHON_COMPILED_FILES}
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/../openscap_api.py ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND python -m compileall ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND python -O -m compileall ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS ${SWIG_MODULE_openscap_py_REAL_NAME}
)
add_custom_target(python_compile ALL DEPENDS ${PYTHON_COMPILED_FILES})

execute_process(COMMAND
	python -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(True, prefix='${CMAKE_INSTALL_PREFIX}'))"
	OUTPUT_VARIABLE PYTHON_SITE_PACKAGES_INSTALL_DIR
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
install(TARGETS ${SWIG_MODULE_openscap_py_REAL_NAME}
	DESTINATION ${PYTHON_SITE_PACKAGES_INSTALL_DIR}
)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/openscap_py.py ${CMAKE_CURRENT_BINARY_DIR}/openscap_api.py
	DESTINATION ${PYTHON_SITE_PACKAGES_INSTALL_DIR}
)
install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/openscap_py.pyc
	${CMAKE_CURRENT_BINARY_DIR}/openscap_py.pyo
	${CMAKE_CURRENT_BINARY_DIR}/openscap_api.pyc
	${CMAKE_CURRENT_BINARY_DIR}/openscap_api.pyo
	DESTINATION ${PYTHON_SITE_PACKAGES_INSTALL_DIR}
)
