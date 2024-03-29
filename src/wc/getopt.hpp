#ifndef CPPCORO_EXAMPLE_GETOPT_HPP
#define CPPCORO_EXAMPLE_GETOPT_HPP

#ifdef __cplusplus
extern "C" {
#endif

extern char *optarg;
extern int optind;

int getopt(int argc, char *const argv[], const char *optstring);

#ifdef __cplusplus
}
#endif

#endif //CPPCORO_EXAMPLE_GETOPT_HPP
