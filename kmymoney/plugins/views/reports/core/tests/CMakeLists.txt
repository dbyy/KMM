include(ECMAddTests)

set(tests_sources
  chart-test.cpp
  pivotgrid-test.cpp
  pivottable-test.cpp
  querytable-test.cpp
)

ecm_add_tests(${tests_sources}
  NAME_PREFIX
    "reports-"
  LINK_LIBRARIES
    Qt::Core
    Qt::Test
    reports
    kmm_mymoney
    kmm_testutilities
)

add_executable(reports-chart-test chart-test.cpp)
target_link_libraries(reports-chart-test
  reports
  Qt::Core
  Qt::Gui
  Qt::Test
)
