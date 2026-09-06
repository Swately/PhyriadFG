# tests/layers/negative_body.cmake — the NEGATIVE twin of the layer generator's body checks (R3).
#
# The generator refuses to emit when a row body breaks its contract (a missing body, a missing entry point, a direct
# `lp.` UBO read instead of the generated alias, a cross-row parameter read without a declared `needs`, a COMPOSE
# body that ignores `c_in` without declaring `overrides`, a wrong signature) — it prints the violation and returns 3.
# During R3 that was verified by hand once. This makes it a standing test: the tree is copied, ONE body is broken in
# the cheapest unambiguous way (a direct `lp.` read), and the generator must exit 3. If it ever exits 0 here, the
# body checks have stopped working and every row body is unguarded.
#
# Invoked by CTest as: cmake -DGEN=<layer_gen> -DSHADERS=<src shaders dir> -DWORK=<scratch> -P this
# Made with my soul - Swately <3

foreach(_v GEN SHADERS WORK)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "negative_body.cmake: -D${_v}=... is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/shaders" "${WORK}/out")
file(COPY "${SHADERS}/" DESTINATION "${WORK}/shaders")

set(_victim "${WORK}/shaders/layers/mv_guided.glsl")
if(NOT EXISTS "${_victim}")
    message(FATAL_ERROR "negative_body.cmake: the victim body ${_victim} is missing — the row set changed; point this test at another Kind::F body")
endif()

# The control: the UNMODIFIED copy must generate cleanly, or the test proves nothing about the corruption.
execute_process(COMMAND "${GEN}" "${WORK}/out" "${WORK}/shaders"
                RESULT_VARIABLE _clean_rc OUTPUT_QUIET ERROR_VARIABLE _clean_err)
if(NOT _clean_rc EQUAL 0)
    message(FATAL_ERROR "negative_body.cmake: the UNMODIFIED tree failed to generate (rc=${_clean_rc}) — the test cannot attribute a later failure to the corruption.\n${_clean_err}")
endif()

# The corruption: a direct UBO read, which the generator must reject in favour of the generated <ROW>_<param> alias.
file(READ "${_victim}" _body)
file(WRITE "${_victim}" "${_body}\nfloat pfg_negative_test_probe() { return lp.injected_by_the_negative_test; }\n")

execute_process(COMMAND "${GEN}" "${WORK}/out" "${WORK}/shaders"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 3)
    message(FATAL_ERROR "negative_body.cmake: the generator accepted a body with a direct `lp.` read (rc=${_rc}, expected 3). The R3 body checks are not guarding the row bodies.\n--- stdout ---\n${_out}\n--- stderr ---\n${_err}")
endif()
if(NOT _err MATCHES "reads the UBO directly")
    message(FATAL_ERROR "negative_body.cmake: the generator exited 3 but not for the injected reason; the message was:\n${_err}")
endif()

file(REMOVE_RECURSE "${WORK}")
message(STATUS "negative_body: the generator refused the corrupted body with rc=3 and named the reason")
