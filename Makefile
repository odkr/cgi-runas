su-php: su-php.c config.h
	$(CC) -I. -o$@ $<
