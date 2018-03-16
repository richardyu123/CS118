#include <string>

#include "Parameters.h"


void util::exit_on_error(const std::string& msg) {
    fprintf(stderr, "ERROR: %s.\n", msg.c_str());
    std::exit(1);
}
