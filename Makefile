GBDKDIR ?= $(firstword $(wildcard $(HOME)/gbdk/ /opt/gbdk/ /usr/local/gbdk/))
GBDKDIR := $(patsubst %/,%,$(GBDKDIR))/
export GBDKDIR
LCC      := $(GBDKDIR)bin/lcc
PYTHON   := $(if $(wildcard $(CURDIR)/.venv/bin/python3),$(CURDIR)/.venv/bin/python3,python3)

TARGET   := build/ramen-time.gbc
SRC      := src/main.c $(wildcard src/gen/*.c)
HDR      := $(wildcard src/gen/*.h)
CFLAGS   := -Wm-yc -Wm-yt0x1B -Wm-yn"RAMENTIME"

.PHONY: all assets playtest clean check

all: $(TARGET)

$(TARGET): $(SRC) $(HDR) | build
	@test -x $(LCC) || { echo "GBDK not found at $(GBDKDIR)"; exit 1; }
	$(LCC) $(CFLAGS) -o $@ $(SRC)
	@echo "built $@"

build:
	@mkdir -p build

assets:
	$(PYTHON) assets/make_art.py
	@$(MAKE) $(TARGET)

playtest: $(TARGET)
	$(PYTHON) tools/playtest.py --rom $(TARGET) \
		--script "wait:8,snap:title,press:start:4,wait:20,snap:play" \
		--out build/playtest-strip.png --frames-dir build/frames --strict

check:
	PATH="$(CURDIR)/.venv/bin:$(PATH)" GBDKDIR=$(GBDKDIR) ./tools/check_env.sh --strict

clean:
	rm -rf build
