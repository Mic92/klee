add_executable(klee-disassembler
        main.cpp
        )

set(KLEE_LIBS )

target_link_libraries(klee-disassembler ${KLEE_LIBS})

install(TARGETS klee-disassembler RUNTIME DESTINATION bin)
