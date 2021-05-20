
.DELETE_ON_ERROR:

.PHONY: test build-dirs


CFLAGS = -Iinclude/ \
	-g \

LIBS = -g \
	-lXKCP \


OBJS_LIB = \
	out/lib/modulo.o \
	out/lib/vectenc.o \

OBJS_GEN_TEST_VECS = \
	out/test/generate-test-vectors.o \

OBJS_GEN_TEST_VECS_WIN = \
	out/test/generate-test-vectors-no-getopt.o \


TESTPROGS = \
	out/test/generate-test-vectors \
	out/test/generate-test-vectors-no-getopt \


test: build-dirs $(TESTPROGS)


build-dirs:
	mkdir -p out out/lib out/test


out/test/generate-test-vectors: $(OBJS_GEN_TEST_VECS) $(OBJS_LIB)
	$(CC) -o $@ $+ $(LIBS)

out/test/generate-test-vectors.o: src/test/generate-test-vectors.c $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS) $<


out/test/generate-test-vectors-no-getopt: $(OBJS_GEN_TEST_VECS_WIN) $(OBJS_LIB)
	$(CC) -o $@ $+ $(LIBS)

out/test/generate-test-vectors-no-getopt.o: src/test/generate-test-vectors-no-getopt.c src/test/generate-test-vectors.c $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS) $<


out/lib/modulo.o: src/lib/modulo.c $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS) $<

out/lib/vectenc.o: src/lib/vectenc.c $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS) $<

