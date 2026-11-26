CC = clang
CFLAGS ?= -std=gnu17 -g -O0 -Wall -Wextra -I../include -I/usr/local/include -DHAVE_CONFIG_H -fprofile-arcs -ftest-coverage
LDFLAGS ?= -L/usr/local/lib -lcmocka
COV ?= $(shell if command -v llvm-cov >/dev/null 2>&1; then echo "llvm-cov gcov"; else echo "gcov"; fi)

TEST_TARGETS = test_compat test_array test_term_env test_window test_hook \
               test_ai_profile test_ai_orm test_ai_honcho test_ai_provider

all: ../include/defs.h $(TEST_TARGETS)

../include/defs.h:
	@if [ ! -f ../include/defs.h ]; then cp ../include/defs.h.in ../include/defs.h; fi

test_compat: test_compat.c ../source/compat.c
	$(CC) $(CFLAGS) -DTEST_COMPAT test_compat.c ../source/compat.c $(LDFLAGS) -o $@

test_array: test_array.c
	$(CC) $(CFLAGS) test_array.c $(LDFLAGS) -o $@

test_term_env: test_term_env.c
	$(CC) $(CFLAGS) test_term_env.c $(LDFLAGS) -o $@

test_window: test_window.c
	$(CC) $(CFLAGS) test_window.c $(LDFLAGS) -o $@

test_hook: test_hook.c
	$(CC) $(CFLAGS) test_hook.c $(LDFLAGS) -o $@

test_ai_profile: test_ai_profile.c ../source/ai_profile.c
	$(CC) $(CFLAGS) test_ai_profile.c ../source/ai_profile.c $(LDFLAGS) -o $@

test_ai_orm: test_ai_orm.c ../source/ai_orm.c ../source/ai_profile.c
	$(CC) $(CFLAGS) test_ai_orm.c ../source/ai_orm.c ../source/ai_profile.c $(LDFLAGS) -lsqlite3 -o $@

test_ai_honcho: test_ai_honcho.c ../source/ai_honcho.c
	$(CC) $(CFLAGS) test_ai_honcho.c ../source/ai_honcho.c $(LDFLAGS) -lcurl -o $@

test_ai_provider: test_ai_provider.c ../source/ai_provider.c
	$(CC) $(CFLAGS) test_ai_provider.c ../source/ai_provider.c $(LDFLAGS) -lcurl -o $@

check: all
	@echo "=========================================="
	@echo "       Running All Unit Test Suites       "
	@echo "=========================================="
	./test_compat
	./test_array
	./test_term_env
	./test_window
	./test_hook
	./test_ai_profile
	./test_ai_orm
	./test_ai_honcho
	./test_ai_provider
	@echo "=========================================="
	@echo "       Generating Coverage Reports        "
	@echo "=========================================="
	$(COV) *.gcno || true

clean:
	rm -f $(TEST_TARGETS) *.o *.gcda *.gcno *.gcov test_smoke

.PHONY: all check clean
