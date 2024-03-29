
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kmymoney-sepaorders.json.cmake ${CMAKE_CURRENT_BINARY_DIR}/kmymoney-sepaorders.json @ONLY)

set( sepaOnlineTasks_SRCS
  ui/sepacredittransferedit.cpp
  sepaonlinetasksloader.cpp
)

ki18n_wrap_ui( sepaOnlineTasks_SRCS
  ui/sepacredittransferedit.ui
)

add_library(konlinetasks_sepa_interface INTERFACE)

if (BUILD_STATIC_PLUGINS)
    kcoreaddons_add_plugin(konlinetasks_sepa SOURCES ${sepaOnlineTasks_SRCS} STATIC INSTALL_NAMESPACE "kmymoney_plugins/onlinetasks")
else()
    kcoreaddons_add_plugin(konlinetasks_sepa SOURCES ${sepaOnlineTasks_SRCS} INSTALL_NAMESPACE "kmymoney_plugins/onlinetasks")
    set_target_properties(konlinetasks_sepa PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

# TODO some dependencies can be moved to konlinetasks_sepa
target_link_libraries( konlinetasks_sepa_interface
  INTERFACE
  kmm_mymoney
  kmm_base_widgets
  kmm_plugin
  onlinetask_interfaces
  Qt::Xml
  Qt::Core
  Qt::Gui
)

target_link_libraries(konlinetasks_sepa
  PUBLIC
    konlinetasks_sepa_interface
  PRIVATE
    KF5::I18n
    KF5::Service
    kmm_settings
    kmm_utils_validators
    KF5::ItemModels
)

