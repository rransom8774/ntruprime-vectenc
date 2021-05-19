
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


TESTPROGS = \
	out/test/generate-test-vectors \


test: build-dirs $(TESTPROGS)


build-dirs:
	mkdir -p out out/lib out/test


out/test/generate-test-vectors: $(OBJS_GEN_TEST_VECS) $(OBJS_LIB)
	$(CC) -o $@ $+ $(LIBS)

out/test/generate-test-vectors.o: src/test/generate-test-vectors.c $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS) $<


out/lib/modulo.o: src/lib/modulo.c $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS) $<

out/lib/vectenc.o: src/lib/vectenc.c $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS) $<

