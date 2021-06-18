C_FILES := $(wildcard ./*.c)
EXEC_FILES := $(notdir $(C_FILES:.c=))
CFLAGS=-pthread

all: $(EXEC_FILES)

clean:
	rm -f $(EXEC_FILES)

%: %.c
	gcc $(CFLAGS) -o $@ $<
