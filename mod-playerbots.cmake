# Included by the core's modules/CMakeLists.txt after the `modules` target exists.
#
# The core links the MySQL client library privately into its `database` target,
# so its include directories do not reach module code. PlayerbotsDatabase.cpp
# needs the complete MySQLPreparedStatement type (which pulls in mysql.h) and
# Playerbots.cpp uses ER_BAD_DB_ERROR from mysqld_error.h. Linking the imported
# `mysql` target here propagates those include directories to the module build.
target_link_libraries(modules
  PRIVATE
    mysql)

# The sub-class balance harness (src/Script/SubclassBalanceHarness.cpp) uses the core's path engine and its
# balance policy, which live with the custom scripts.
target_include_directories(modules
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom)
